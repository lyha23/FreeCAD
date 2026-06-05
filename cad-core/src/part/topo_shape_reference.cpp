#include "cad_core/part/topo_shape_reference.h"

#include "cad_core/app/document.h"
#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/shape_exporter.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::part {

namespace {

std::string shapeTypeName(TopAbs_ShapeEnum shapeType)
{
    switch (shapeType) {
        case TopAbs_VERTEX:
            return "Vertex";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_FACE:
            return "Face";
        default:
            return "Unsupported";
    }
}

std::optional<TopAbs_ShapeEnum> shapeTypeFromName(const std::string& shapeType)
{
    if (shapeType == "Face") {
        return TopAbs_FACE;
    }
    if (shapeType == "Edge") {
        return TopAbs_EDGE;
    }
    if (shapeType == "Vertex") {
        return TopAbs_VERTEX;
    }
    return std::nullopt;
}

nlohmann::json pointJson(const gp_Pnt& point)
{
    auto clean = [](double value) {
        constexpr double eps = 1e-6;
        if (std::abs(value) < eps) {
            return 0.0;
        }
        const double rounded = std::round(value);
        return std::abs(value - rounded) < eps ? rounded : value;
    };
    return {clean(point.X()), clean(point.Y()), clean(point.Z())};
}

std::size_t countSubshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, kind, map);
    return static_cast<std::size_t>(map.Extent());
}

std::optional<gp_Pnt> centroidForShape(const TopoDS_Shape& shape)
{
    if (shape.ShapeType() == TopAbs_VERTEX) {
        return BRep_Tool::Pnt(TopoDS::Vertex(shape));
    }

    GProp_GProps props;
    if (shape.ShapeType() == TopAbs_FACE) {
        BRepGProp::SurfaceProperties(shape, props);
    }
    else if (shape.ShapeType() == TopAbs_EDGE) {
        BRepGProp::LinearProperties(shape, props);
    }
    else {
        return std::nullopt;
    }
    if (props.Mass() <= 0.0) {
        return std::nullopt;
    }
    return props.CentreOfMass();
}

double measureForShape(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    if (shape.ShapeType() == TopAbs_FACE) {
        BRepGProp::SurfaceProperties(shape, props);
        return props.Mass();
    }
    if (shape.ShapeType() == TopAbs_EDGE) {
        BRepGProp::LinearProperties(shape, props);
        return props.Mass();
    }
    return 0.0;
}

double surfaceAreaForShape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return 0.0;
    }
    if (shape.ShapeType() == TopAbs_FACE) {
        return measureForShape(shape);
    }

    double area = 0.0;
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    for (int index = 1; index <= faces.Extent(); ++index) {
        area += measureForShape(faces(index));
    }
    return area;
}

double commonFaceArea(const TopoDS_Shape& left, const TopoDS_Shape& right)
{
    try {
        BRepAlgoAPI_Common common(left, right);
        common.Build();
        if (!common.IsDone() || common.Shape().IsNull()) {
            return 0.0;
        }
        return surfaceAreaForShape(common.Shape());
    }
    catch (const Standard_Failure&) {
        return 0.0;
    }
}

std::vector<TopoDS_Shape> subshapesOfKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, kind, map);
    std::vector<TopoDS_Shape> result;
    result.reserve(static_cast<std::size_t>(map.Extent()));
    for (int index = 1; index <= map.Extent(); ++index) {
        result.push_back(map(index));
    }
    return result;
}

bool samePoint(const gp_Pnt& lhs, const gp_Pnt& rhs, double tolerance)
{
    return lhs.SquareDistance(rhs) <= tolerance * tolerance;
}

std::vector<gp_Pnt> vertexPointsForShape(const TopoDS_Shape& shape)
{
    std::vector<gp_Pnt> points;
    for (const auto& vertex : subshapesOfKind(shape, TopAbs_VERTEX)) {
        points.push_back(BRep_Tool::Pnt(TopoDS::Vertex(vertex)));
    }
    return points;
}

bool sameVertexSet(const TopoDS_Shape& oldShape, const TopoDS_Shape& candidate, double tolerance)
{
    const std::vector<gp_Pnt> oldVertices = vertexPointsForShape(oldShape);
    const std::vector<gp_Pnt> candidateVertices = vertexPointsForShape(candidate);
    if (oldVertices.size() != candidateVertices.size()) {
        return false;
    }

    std::vector<bool> used(candidateVertices.size(), false);
    for (const auto& oldVertex : oldVertices) {
        bool found = false;
        for (std::size_t index = 0; index < candidateVertices.size(); ++index) {
            if (used.at(index) || !samePoint(oldVertex, candidateVertices.at(index), tolerance)) {
                continue;
            }
            used[index] = true;
            found = true;
            break;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

ReferenceMatchResult classifyFaceSplitOrDeleted(const TopoDS_Shape& currentShape,
                                                const std::string& subnamePrefix,
                                                const TopoDS_Shape& oldFace)
{
    const double oldArea = surfaceAreaForShape(oldFace);
    if (oldArea <= 0.0) {
        return {};
    }

    struct OverlapCandidate {
        int index = 0;
        TopoDS_Shape shape;
        double area = 0.0;
    };
    std::vector<OverlapCandidate> overlaps;

    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(currentShape, TopAbs_FACE, faces);
    const double areaTolerance = std::max(1e-7, oldArea * 1e-6);
    for (int index = 1; index <= faces.Extent(); ++index) {
        const TopoDS_Shape candidate = faces(index);
        const double overlapArea = commonFaceArea(oldFace, candidate);
        if (overlapArea > areaTolerance) {
            overlaps.push_back(OverlapCandidate {index, candidate, overlapArea});
        }
    }

    if (overlaps.empty()) {
        return ReferenceMatchResult {ReferenceMatchStatus::Deleted, {}, std::nullopt};
    }
    if (overlaps.size() == 1U) {
        const double candidateArea = surfaceAreaForShape(overlaps.front().shape);
        if (std::abs(overlaps.front().area - oldArea) <= areaTolerance
            && std::abs(candidateArea - oldArea) <= areaTolerance) {
            return ReferenceMatchResult {
                ReferenceMatchStatus::Unique,
                subnamePrefix + "Face" + std::to_string(overlaps.front().index),
                overlaps.front().shape,
            };
        }
        return {};
    }

    return ReferenceMatchResult {ReferenceMatchStatus::Split, {}, std::nullopt};
}

std::optional<std::pair<gp_Pnt, gp_Pnt>> lineEdgeEndpoints(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        return std::nullopt;
    }
    return std::make_pair(curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter()));
}

bool lineEndpointsMatch(const std::pair<gp_Pnt, gp_Pnt>& lhs,
                        const std::pair<gp_Pnt, gp_Pnt>& rhs,
                        double tolerance)
{
    return (samePoint(lhs.first, rhs.first, tolerance) && samePoint(lhs.second, rhs.second, tolerance))
        || (samePoint(lhs.first, rhs.second, tolerance) && samePoint(lhs.second, rhs.first, tolerance));
}

double pointLineDistance(const gp_Pnt& point, const gp_Pnt& lineStart, const gp_Vec& direction)
{
    gp_Vec offset(lineStart, point);
    return offset.Crossed(direction).Magnitude();
}

ReferenceMatchResult classifyLineEdgeSplitOrDeleted(const TopoDS_Shape& currentShape,
                                                    const TopoDS_Shape& oldEdge)
{
    const auto oldEndpoints = lineEdgeEndpoints(TopoDS::Edge(oldEdge));
    if (!oldEndpoints) {
        return {};
    }

    const gp_Pnt& oldStart = oldEndpoints->first;
    const gp_Pnt& oldEnd = oldEndpoints->second;
    gp_Vec oldVector(oldStart, oldEnd);
    const double oldLength = oldVector.Magnitude();
    if (oldLength <= 0.0) {
        return {};
    }
    oldVector.Normalize();

    const double distanceTolerance = std::max(1e-7, oldLength * 1e-7);
    const double intervalTolerance = std::max(1e-7, oldLength * 1e-7);
    int overlappingEdges = 0;
    double coveredLength = 0.0;

    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(currentShape, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        const auto candidateEndpoints = lineEdgeEndpoints(TopoDS::Edge(edges(index)));
        if (!candidateEndpoints) {
            continue;
        }

        const gp_Pnt& candidateStart = candidateEndpoints->first;
        const gp_Pnt& candidateEnd = candidateEndpoints->second;
        if (pointLineDistance(candidateStart, oldStart, oldVector) > distanceTolerance
            || pointLineDistance(candidateEnd, oldStart, oldVector) > distanceTolerance) {
            continue;
        }

        gp_Vec startOffset(oldStart, candidateStart);
        gp_Vec endOffset(oldStart, candidateEnd);
        double candidateMin = startOffset.Dot(oldVector);
        double candidateMax = endOffset.Dot(oldVector);
        if (candidateMin > candidateMax) {
            std::swap(candidateMin, candidateMax);
        }

        const double overlapMin = std::max(0.0, candidateMin);
        const double overlapMax = std::min(oldLength, candidateMax);
        if (overlapMax - overlapMin > intervalTolerance) {
            ++overlappingEdges;
            coveredLength += overlapMax - overlapMin;
        }
    }

    if (overlappingEdges == 0) {
        return ReferenceMatchResult {ReferenceMatchStatus::Deleted, {}, std::nullopt};
    }
    if (overlappingEdges > 1 || coveredLength < oldLength - intervalTolerance) {
        return ReferenceMatchResult {ReferenceMatchStatus::Split, {}, std::nullopt};
    }
    return {};
}

double distanceFromPointToEdge(const gp_Pnt& point, const TopoDS_Edge& edge)
{
    try {
        const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point);
        BRepExtrema_DistShapeShape distance(vertex, edge);
        distance.Perform();
        if (distance.IsDone() && distance.NbSolution() > 0) {
            return distance.Value();
        }
    }
    catch (const Standard_Failure&) {
    }
    return std::numeric_limits<double>::infinity();
}

bool edgeSamplesLieOnOldEdge(const TopoDS_Edge& oldEdge,
                             const TopoDS_Edge& candidate,
                             double tolerance)
{
    try {
        BRepAdaptor_Curve oldCurve(oldEdge);
        BRepAdaptor_Curve candidateCurve(candidate);
        if (oldCurve.GetType() != candidateCurve.GetType()) {
            return false;
        }

        const double first = candidateCurve.FirstParameter();
        const double last = candidateCurve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last)) {
            return false;
        }

        constexpr int sampleCount = 7;
        for (int sample = 0; sample < sampleCount; ++sample) {
            const double ratio = sampleCount == 1 ? 0.0 : static_cast<double>(sample) / (sampleCount - 1);
            const double parameter = first + (last - first) * ratio;
            const gp_Pnt point = candidateCurve.Value(parameter);
            if (distanceFromPointToEdge(point, oldEdge) > tolerance) {
                return false;
            }
        }
        return true;
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

bool edgeGeometryMatches(const TopoDS_Edge& oldEdge, const TopoDS_Edge& candidate, double tolerance)
{
    try {
        const auto oldLine = lineEdgeEndpoints(oldEdge);
        const auto candidateLine = lineEdgeEndpoints(candidate);
        if (oldLine || candidateLine) {
            return oldLine && candidateLine && lineEndpointsMatch(*oldLine, *candidateLine, tolerance);
        }

        BRepAdaptor_Curve oldCurve(oldEdge);
        BRepAdaptor_Curve candidateCurve(candidate);
        if (oldCurve.GetType() != candidateCurve.GetType()) {
            return false;
        }

        const double oldLength = measureForShape(oldEdge);
        const double candidateLength = measureForShape(candidate);
        const double lengthTolerance = std::max(tolerance, std::max(oldLength, candidateLength) * 1e-6);
        if (std::abs(oldLength - candidateLength) > lengthTolerance) {
            return false;
        }

        return edgeSamplesLieOnOldEdge(oldEdge, candidate, tolerance)
            && edgeSamplesLieOnOldEdge(candidate, oldEdge, tolerance);
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

bool faceSurfaceCompatible(const TopoDS_Shape& oldFace, const TopoDS_Shape& candidate)
{
    try {
        BRepAdaptor_Surface oldSurface(TopoDS::Face(oldFace));
        BRepAdaptor_Surface candidateSurface(TopoDS::Face(candidate));
        if (oldSurface.GetType() == GeomAbs_Plane) {
            return candidateSurface.GetType() == GeomAbs_Plane;
        }
        return oldSurface.GetType() == candidateSurface.GetType();
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

bool faceEdgesMatch(const TopoDS_Shape& oldFace, const TopoDS_Shape& candidate, double tolerance)
{
    const std::vector<TopoDS_Shape> oldEdges = subshapesOfKind(oldFace, TopAbs_EDGE);
    const std::vector<TopoDS_Shape> candidateEdges = subshapesOfKind(candidate, TopAbs_EDGE);
    if (oldEdges.size() != candidateEdges.size()) {
        return false;
    }

    std::vector<bool> used(candidateEdges.size(), false);
    for (const auto& oldEdge : oldEdges) {
        bool found = false;
        for (std::size_t index = 0; index < candidateEdges.size(); ++index) {
            if (used.at(index)) {
                continue;
            }
            if (!edgeGeometryMatches(TopoDS::Edge(oldEdge), TopoDS::Edge(candidateEdges.at(index)), tolerance)) {
                continue;
            }
            used[index] = true;
            found = true;
            break;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool sharedVertexGeometryMatches(const TopoDS_Shape& oldShape,
                                 const TopoDS_Shape& candidate,
                                 TopAbs_ShapeEnum shapeType,
                                 double tolerance)
{
    if (oldShape.IsNull() || candidate.IsNull() || candidate.ShapeType() != shapeType) {
        return false;
    }
    switch (shapeType) {
        case TopAbs_VERTEX:
            return samePoint(BRep_Tool::Pnt(TopoDS::Vertex(oldShape)),
                             BRep_Tool::Pnt(TopoDS::Vertex(candidate)),
                             tolerance);
        case TopAbs_EDGE:
            return sameVertexSet(oldShape, candidate, tolerance)
                && edgeGeometryMatches(TopoDS::Edge(oldShape), TopoDS::Edge(candidate), tolerance);
        case TopAbs_FACE:
            return sameVertexSet(oldShape, candidate, tolerance) && faceSurfaceCompatible(oldShape, candidate)
                && faceEdgesMatch(oldShape, candidate, tolerance);
        default:
            return false;
    }
}

ReferenceMatchResult findSubshapeBySharedVertexGeometry(const TopoDS_Shape& currentShape,
                                                        const std::string& subnamePrefix,
                                                        const TopoDS_Shape& oldShape,
                                                        TopAbs_ShapeEnum shapeType)
{
    ReferenceMatchResult result;
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(currentShape, shapeType, subshapes);
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        const TopoDS_Shape candidate = subshapes(index);
        if (!sharedVertexGeometryMatches(oldShape, candidate, shapeType, 1e-7)) {
            continue;
        }
        if (result.status == ReferenceMatchStatus::Unique) {
            return ReferenceMatchResult {ReferenceMatchStatus::Ambiguous, {}, std::nullopt};
        }
        result.status = ReferenceMatchStatus::Unique;
        result.subname = subnamePrefix + shapeTypeName(shapeType) + std::to_string(index);
        result.shape = candidate;
    }
    return result;
}

ReferenceMatchResult classifyNonLineEdgeSplitOrDeleted(const TopoDS_Shape& currentShape,
                                                       const std::string& subnamePrefix,
                                                       const TopoDS_Shape& oldEdge)
{
    if (lineEdgeEndpoints(TopoDS::Edge(oldEdge))) {
        return {};
    }

    const double oldLength = measureForShape(oldEdge);
    if (oldLength <= 0.0) {
        return {};
    }

    struct OverlapCandidate {
        int index = 0;
        TopoDS_Shape shape;
        double length = 0.0;
    };
    std::vector<OverlapCandidate> overlaps;

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::findSubShapesWithSharedVertex(), Edge search first follows old vertices to
    // current ancestors, then "Perform geometry comparison of the ancestor and input shape".
    // This stateless BREP slice has no cached ancestor map yet, so it conservatively classifies
    // non-line edge snapshots by checking whether current edge samples still lie on the old edge
    // geometry. It keeps FreeCAD's line-only loose endpoint rule separate.
    const double distanceTolerance = std::max(1e-7, oldLength * 1e-6);
    const double lengthTolerance = std::max(1e-7, oldLength * 1e-5);

    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(currentShape, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        const TopoDS_Shape candidateShape = edges(index);
        const double candidateLength = measureForShape(candidateShape);
        if (candidateLength <= lengthTolerance) {
            continue;
        }
        if (!edgeSamplesLieOnOldEdge(TopoDS::Edge(oldEdge), TopoDS::Edge(candidateShape), distanceTolerance)) {
            continue;
        }
        overlaps.push_back(OverlapCandidate {index, candidateShape, candidateLength});
    }

    if (overlaps.empty()) {
        return ReferenceMatchResult {ReferenceMatchStatus::Deleted, {}, std::nullopt};
    }

    double coveredLength = 0.0;
    for (const auto& overlap : overlaps) {
        coveredLength += overlap.length;
    }

    if (overlaps.size() == 1U && std::abs(coveredLength - oldLength) <= lengthTolerance) {
        return ReferenceMatchResult {
            ReferenceMatchStatus::Unique,
            subnamePrefix + "Edge" + std::to_string(overlaps.front().index),
            overlaps.front().shape,
        };
    }

    return ReferenceMatchResult {ReferenceMatchStatus::Split, {}, std::nullopt};
}

std::optional<std::array<double, 3>> readVector3(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() != 3U) {
        return std::nullopt;
    }
    std::array<double, 3> result{};
    for (std::size_t index = 0; index < 3U; ++index) {
        if (!value.at(index).is_number()) {
            return std::nullopt;
        }
        result[index] = value.at(index).get<double>();
    }
    return result;
}

std::optional<std::array<double, 3>> bboxVector(const nlohmann::json& fingerprint, const std::string& key)
{
    const auto it = fingerprint.find(key);
    if (it == fingerprint.end()) {
        return std::nullopt;
    }
    return readVector3(*it);
}

double distance(const std::array<double, 3>& lhs, const std::array<double, 3>& rhs)
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double bboxScale(const nlohmann::json& fingerprint)
{
    const auto min = bboxVector(fingerprint, "bboxMin");
    const auto max = bboxVector(fingerprint, "bboxMax");
    if (!min || !max) {
        return 1.0;
    }
    return std::max(1.0, distance(*min, *max));
}

double modelTolerance(const nlohmann::json& fingerprint)
{
    return std::max(1e-6, bboxScale(fingerprint) * 1e-5);
}

std::optional<std::array<double, 3>> fingerprintVector(const nlohmann::json& fingerprint,
                                                       const std::string& key)
{
    const auto it = fingerprint.find(key);
    if (it == fingerprint.end()) {
        return std::nullopt;
    }
    return readVector3(*it);
}

std::optional<double> fingerprintNumber(const nlohmann::json& fingerprint, const std::string& key)
{
    const auto it = fingerprint.find(key);
    if (it == fingerprint.end()) {
        return std::nullopt;
    }
    if (!it->is_number()) {
        return std::nullopt;
    }
    return it->get<double>();
}

std::optional<int> fingerprintInteger(const nlohmann::json& fingerprint, const std::string& key)
{
    const auto it = fingerprint.find(key);
    if (it == fingerprint.end()) {
        return std::nullopt;
    }
    if (!it->is_number_integer()) {
        return std::nullopt;
    }
    return it->get<int>();
}

std::optional<std::array<double, 3>> jsonVector(const nlohmann::json& value)
{
    return readVector3(value);
}

std::optional<std::string> compareVector(const nlohmann::json& current,
                                         const nlohmann::json& expected,
                                         const std::string& key,
                                         double tolerance)
{
    const auto expectedVector = fingerprintVector(expected, key);
    if (!expectedVector) {
        return std::nullopt;
    }
    const auto currentIt = current.find(key);
    if (currentIt == current.end()) {
        return std::nullopt;
    }
    const auto currentVector = jsonVector(*currentIt);
    if (!currentVector) {
        return std::nullopt;
    }
    if (distance(*currentVector, *expectedVector) > tolerance) {
        return key + " changed";
    }
    return std::nullopt;
}

std::optional<std::string> compareNumber(const nlohmann::json& current,
                                         const nlohmann::json& expected,
                                         const std::string& key)
{
    const auto expectedNumber = fingerprintNumber(expected, key);
    if (!expectedNumber) {
        return std::nullopt;
    }
    const auto currentNumber = fingerprintNumber(current, key);
    if (!currentNumber) {
        return std::nullopt;
    }
    const double tolerance = std::max(1e-6, std::abs(*expectedNumber) * 1e-5);
    if (std::abs(*currentNumber - *expectedNumber) > tolerance) {
        return key + " changed";
    }
    return std::nullopt;
}

std::optional<std::string> compareInteger(const nlohmann::json& current,
                                          const nlohmann::json& expected,
                                          const std::string& key)
{
    const auto expectedNumber = fingerprintInteger(expected, key);
    if (!expectedNumber) {
        return std::nullopt;
    }
    const auto currentNumber = fingerprintInteger(current, key);
    if (!currentNumber) {
        return std::nullopt;
    }
    if (*currentNumber != *expectedNumber) {
        return key + " changed";
    }
    return std::nullopt;
}

}  // namespace

nlohmann::json referenceFingerprintForShape(const TopoDS_Shape& shape)
{
    nlohmann::json fingerprint = {
        {"shapeType", shapeTypeName(shape.ShapeType())},
        {"bboxMin", cad_core::part::bboxForShape(shape).value("min", nlohmann::json::array())},
        {"bboxMax", cad_core::part::bboxForShape(shape).value("max", nlohmann::json::array())},
        {"edgeCount", static_cast<int>(countSubshapes(shape, TopAbs_EDGE))},
        {"vertexCount", static_cast<int>(countSubshapes(shape, TopAbs_VERTEX))},
    };

    if (shape.ShapeType() == TopAbs_FACE) {
        fingerprint["area"] = measureForShape(shape);
    }
    else if (shape.ShapeType() == TopAbs_EDGE) {
        fingerprint["length"] = measureForShape(shape);
    }
    if (const auto centroid = centroidForShape(shape)) {
        fingerprint["centroid"] = pointJson(*centroid);
    }
    return fingerprint;
}

std::optional<std::string> referenceFingerprintDriftReason(const TopoDS_Shape& currentShape,
                                                           const nlohmann::json& expectedFingerprint,
                                                           const std::string& expectedShapeType)
{
    if (!expectedFingerprint.is_object() || expectedFingerprint.empty()) {
        return std::nullopt;
    }

    const nlohmann::json current = referenceFingerprintForShape(currentShape);
    const std::string currentShapeType = current.value("shapeType", "Unsupported");
    if (!expectedShapeType.empty() && currentShapeType != expectedShapeType) {
        return "shape type changed from " + expectedShapeType + " to " + currentShapeType;
    }

    const double tolerance = modelTolerance(expectedFingerprint);
    if (const auto reason = compareNumber(current, expectedFingerprint, "area")) {
        return reason;
    }
    if (const auto reason = compareNumber(current, expectedFingerprint, "length")) {
        return reason;
    }
    if (const auto reason = compareVector(current, expectedFingerprint, "centroid", tolerance)) {
        return reason;
    }
    if (const auto reason = compareVector(current, expectedFingerprint, "bboxMin", tolerance)) {
        return reason;
    }
    if (const auto reason = compareVector(current, expectedFingerprint, "bboxMax", tolerance)) {
        return reason;
    }
    if (const auto reason = compareInteger(current, expectedFingerprint, "edgeCount")) {
        return reason;
    }
    if (const auto reason = compareInteger(current, expectedFingerprint, "vertexCount")) {
        return reason;
    }
    return std::nullopt;
}

ReferenceMatchResult findUniqueSubshapeByReferenceFingerprint(const TopoDS_Shape& currentShape,
                                                              const std::string& subnamePrefix,
                                                              const nlohmann::json& expectedFingerprint,
                                                              const std::string& expectedShapeType)
{
    if (!expectedFingerprint.is_object() || expectedFingerprint.empty()) {
        return {};
    }
    const auto shapeType = shapeTypeFromName(expectedShapeType);
    if (!shapeType) {
        return {};
    }

    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(currentShape, *shapeType, subshapes);

    ReferenceMatchResult result;
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        const TopoDS_Shape candidate = subshapes(index);
        if (referenceFingerprintDriftReason(candidate, expectedFingerprint, expectedShapeType)) {
            continue;
        }
        if (result.status == ReferenceMatchStatus::Unique) {
            return ReferenceMatchResult {ReferenceMatchStatus::Ambiguous, {}, std::nullopt};
        }
        result.status = ReferenceMatchStatus::Unique;
        result.subname = subnamePrefix + expectedShapeType + std::to_string(index);
        result.shape = candidate;
    }
    return result;
}

ReferenceMatchResult findUniqueSubshapeByReferenceBrepSnapshot(const TopoDS_Shape& currentShape,
                                                               const std::string& subnamePrefix,
                                                               const std::string& format,
                                                               const std::string& data,
                                                               long long byteLength,
                                                               const std::string& sha256,
                                                               const std::string& expectedShapeType,
                                                               std::string& error)
{
    const auto oldShape = cad_core::part::readBrepSnapshot(format, data, byteLength, sha256, error);
    if (!oldShape) {
        return {};
    }

    const auto shapeType = shapeTypeFromName(expectedShapeType);
    if (!shapeType) {
        error = "ReferenceShadow shapeType is not supported";
        return {};
    }
    if (oldShape->ShapeType() != *shapeType) {
        error = "ReferenceShadow.brep shape type does not match shapeType";
        return {};
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::findSubShapesWithSharedVertex(), "Search the first vertex", "Compare each
    // vertex of the ancestor shape and the input shape", then "Perform geometry comparison of
    // the ancestor and input shape." BREP snapshots are old single-subshape evidence, so recovery
    // must use that shared-vertex + geometry path instead of bbox/centroid fingerprints that can
    // treat crossing line edges as equivalent.
    const ReferenceMatchResult sharedVertex =
        findSubshapeBySharedVertexGeometry(currentShape, subnamePrefix, *oldShape, *shapeType);
    if (sharedVertex.status != ReferenceMatchStatus::Missing) {
        return sharedVertex;
    }

    if (countSubshapes(*oldShape, TopAbs_VERTEX) == 0U) {
        const ReferenceMatchResult strict = findUniqueSubshapeByReferenceFingerprint(currentShape,
                                                                                     subnamePrefix,
                                                                                     referenceFingerprintForShape(*oldShape),
                                                                                     expectedShapeType);
        if (strict.status != ReferenceMatchStatus::Missing) {
            return strict;
        }
    }
    if (*shapeType == TopAbs_FACE) {
        return classifyFaceSplitOrDeleted(currentShape, subnamePrefix, *oldShape);
    }
    if (*shapeType == TopAbs_EDGE) {
        const auto lineResult = classifyLineEdgeSplitOrDeleted(currentShape, *oldShape);
        if (lineResult.status != ReferenceMatchStatus::Missing) {
            return lineResult;
        }
        return classifyNonLineEdgeSplitOrDeleted(currentShape, subnamePrefix, *oldShape);
    }
    if (*shapeType == TopAbs_VERTEX) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::findSubShapesWithSharedVertex(), Vertex search compares BRep_Tool::Pnt() with
        // tolerance. If shared-vertex matching found no unique current vertex, the old point is no
        // longer represented by current topology.
        return ReferenceMatchResult {ReferenceMatchStatus::Deleted, {}, std::nullopt};
    }
    return {};
}

ReferenceMatchResult findUniqueSubshapeByReferenceBrepText(const TopoDS_Shape& currentShape,
                                                           const std::string& subnamePrefix,
                                                           const std::string& brepText,
                                                           long long byteLength,
                                                           const std::string& sha256,
                                                           const std::string& expectedShapeType,
                                                           std::string& error)
{
    return findUniqueSubshapeByReferenceBrepSnapshot(currentShape,
                                                     subnamePrefix,
                                                     "brep-text",
                                                     brepText,
                                                     byteLength,
                                                     sha256,
                                                     expectedShapeType,
                                                     error);
}

namespace {

std::optional<std::string> unsupportedReferenceShadowBrepReason(const app::ReferenceShadow& shadow)
{
    if (!shadow.brep || shadow.brep->format == "brep-text" || shadow.brep->format == "brep-bin-zstd-base64") {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp
    // ::Feature::onBeforeChange() keeps old subshape geometry in ElementCache in-process.
    // cad-core's stateless recovery decoder only accepts approved snapshot transports; unknown
    // formats must not silently fall back to fingerprint recovery.
    return "ReferenceShadow.brep format " + shadow.brep->format + " is not supported by runtime recovery";
}

std::string brepRecoveryReason(ReferenceMatchStatus status, const std::string& brepError)
{
    if (!brepError.empty()) {
        return "does not decode ReferenceShadow.brep: " + brepError;
    }
    if (status == ReferenceMatchStatus::Ambiguous) {
        return "matches multiple ReferenceShadow.brep candidates";
    }
    if (status == ReferenceMatchStatus::Split) {
        return "is split into multiple current ReferenceShadow.brep candidates";
    }
    if (status == ReferenceMatchStatus::Deleted) {
        return "is deleted from current ReferenceShadow.brep candidates";
    }
    return "does not match a current ReferenceShadow.brep candidate";
}

std::string fingerprintRecoveryReason(ReferenceMatchStatus status)
{
    if (status == ReferenceMatchStatus::Ambiguous) {
        return "matches multiple ReferenceShadow fingerprint candidates";
    }
    if (status == ReferenceMatchStatus::Split) {
        return "is split into multiple current ReferenceShadow fingerprint candidates";
    }
    if (status == ReferenceMatchStatus::Deleted) {
        return "is deleted from current ReferenceShadow fingerprint candidates";
    }
    return "does not match a current ReferenceShadow fingerprint candidate";
}

}  // namespace

ReferenceShadowRecoveryResult recoverReferenceShadowSubshape(const TopoDS_Shape& currentShape,
                                                             const std::string& subnamePrefix,
                                                             const app::ReferenceShadow& shadow)
{
    if (currentShape.IsNull()) {
        return ReferenceShadowRecoveryResult {
            ReferenceMatchStatus::Missing,
            {},
            std::nullopt,
            "does not have a current shape for ReferenceShadow recovery",
            {},
            false,
        };
    }
    if (const auto unsupportedBrepReason = unsupportedReferenceShadowBrepReason(shadow)) {
        return ReferenceShadowRecoveryResult {
            ReferenceMatchStatus::Missing,
            {},
            std::nullopt,
            *unsupportedBrepReason,
            "unsupported_reference_shadow_brep",
            true,
        };
    }

    if (shadow.brep) {
        std::string brepError;
        const auto match = findUniqueSubshapeByReferenceBrepSnapshot(currentShape,
                                                                     subnamePrefix,
                                                                     shadow.brep->format,
                                                                     shadow.brep->data,
                                                                     shadow.brep->byteLength,
                                                                     shadow.brep->sha256,
                                                                     shadow.shapeType,
                                                                     brepError);
        if (match.status == ReferenceMatchStatus::Unique && match.shape && !match.shape->IsNull()) {
            return ReferenceShadowRecoveryResult {
                match.status,
                match.subname,
                match.shape,
                {},
                {},
                true,
            };
        }
        return ReferenceShadowRecoveryResult {
            match.status,
            {},
            std::nullopt,
            brepRecoveryReason(match.status, brepError),
            {},
            true,
        };
    }

    const auto match = findUniqueSubshapeByReferenceFingerprint(currentShape,
                                                                subnamePrefix,
                                                                shadow.fingerprint,
                                                                shadow.shapeType);
    if (match.status == ReferenceMatchStatus::Unique && match.shape && !match.shape->IsNull()) {
        return ReferenceShadowRecoveryResult {
            match.status,
            match.subname,
            match.shape,
            {},
            {},
            false,
        };
    }
    return ReferenceShadowRecoveryResult {
        match.status,
        {},
        std::nullopt,
        fingerprintRecoveryReason(match.status),
        {},
        false,
    };
}

bool referenceShadowMatchesCurrentSubshape(const TopoDS_Shape& currentShape,
                                           const std::string& subnamePrefix,
                                           const std::string& currentSubname,
                                           const TopoDS_Shape& currentSubshape,
                                           const app::ReferenceShadow& shadow)
{
    if (shadow.fingerprint.is_object() && !shadow.fingerprint.empty()
        && !referenceFingerprintDriftReason(currentSubshape, shadow.fingerprint, shadow.shapeType)) {
        return true;
    }
    if (!shadow.brep || currentShape.IsNull()) {
        return false;
    }

    const auto recovery = recoverReferenceShadowSubshape(currentShape, subnamePrefix, shadow);
    return recovery.status == ReferenceMatchStatus::Unique && recovery.subname == currentSubname;
}

}  // namespace cad_core::part
