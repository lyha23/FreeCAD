#include "cad_core/topo/element_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace cad_core::topo {

namespace {

bool samePoint3d(const gp_Pnt& left, const gp_Pnt& right)
{
    return left.SquareDistance(right) < Precision::SquareConfusion();
}

void addElementMapPair(nlohmann::json& result,
                       const std::string& internalName,
                       const std::string& rawName)
{
    result[internalName] = rawName;
    result[rawName] = internalName;
}

void addDistinctString(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::optional<int> matchingVertexIndex(const TopTools_IndexedMapOfShape& rawVertices,
                                       const TopoDS_Vertex& internalVertex)
{
    const gp_Pnt internalPoint = BRep_Tool::Pnt(internalVertex);
    for (int index = 1; index <= rawVertices.Extent(); ++index) {
        if (samePoint3d(BRep_Tool::Pnt(TopoDS::Vertex(rawVertices(index))), internalPoint)) {
            return index;
        }
    }
    return std::nullopt;
}

std::pair<gp_Pnt, gp_Pnt> edgeEndpoints(const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    return {curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter())};
}

bool sameEdgeEndpoints(const std::pair<gp_Pnt, gp_Pnt>& left,
                       const std::pair<gp_Pnt, gp_Pnt>& right)
{
    return (samePoint3d(left.first, right.first) && samePoint3d(left.second, right.second))
        || (samePoint3d(left.first, right.second) && samePoint3d(left.second, right.first));
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

bool samplesLieOnEdge(const TopoDS_Edge& source, const TopoDS_Edge& target)
{
    try {
        BRepAdaptor_Curve curve(source);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last)) {
            return false;
        }

        constexpr int sampleCount = 7;
        for (int sample = 0; sample < sampleCount; ++sample) {
            const double ratio = static_cast<double>(sample) / (sampleCount - 1);
            const gp_Pnt point = curve.Value(first + (last - first) * ratio);
            if (distanceFromPointToEdge(point, target) > Precision::Confusion()) {
                return false;
            }
        }
    }
    catch (const Standard_Failure&) {
        return false;
    }
    return true;
}

bool sameEdgeGeometry(const TopoDS_Edge& rawEdge, const TopoDS_Edge& internalEdge)
{
    try {
        BRepAdaptor_Curve rawCurve(rawEdge);
        BRepAdaptor_Curve internalCurve(internalEdge);
        const GeomAbs_CurveType rawType = rawCurve.GetType();
        const GeomAbs_CurveType internalType = internalCurve.GetType();
        if (rawType != internalType) {
            return false;
        }
        if (rawType == GeomAbs_Line) {
            return true;
        }
        return samplesLieOnEdge(rawEdge, internalEdge) && samplesLieOnEdge(internalEdge, rawEdge);
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

struct InternalEdgeHistoryMatch
{
    bool exactTarget = false;
    std::vector<std::string> splitTargets;
};

InternalEdgeHistoryMatch internalEdgeHistoryMatch(const TopoDS_Edge& rawEdge,
                                                  const TopTools_IndexedMapOfShape& internalEdges)
{
    InternalEdgeHistoryMatch match;
    const auto rawEndpoints = edgeEndpoints(rawEdge);

    for (int internalIndex = 1; internalIndex <= internalEdges.Extent(); ++internalIndex) {
        const TopoDS_Edge internalEdge = TopoDS::Edge(internalEdges(internalIndex));
        if (sameEdgeEndpoints(rawEndpoints, edgeEndpoints(internalEdge))
            && sameEdgeGeometry(rawEdge, internalEdge)) {
            match.exactTarget = true;
            match.splitTargets.clear();
            break;
        }
        if (samplesLieOnEdge(internalEdge, rawEdge)) {
            match.splitTargets.push_back("InternalEdge" + std::to_string(internalIndex));
        }
    }

    return match;
}

std::vector<std::string> rawBoundarySourcesForInternalFace(
    const TopoDS_Face& internalFace,
    const TopTools_IndexedMapOfShape& rawEdges
)
{
    std::vector<std::string> sources;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::splitWires(), returns "The outer wire"; FaceMaker::postBuild() uses that
    // return value before collecting edge names for the generated face.
    const TopoDS_Wire outerWire = BRepTools::OuterWire(internalFace);
    if (outerWire.IsNull()) {
        return sources;
    }

    for (TopExp_Explorer faceEdgeExplorer(outerWire, TopAbs_EDGE);
         faceEdgeExplorer.More();
         faceEdgeExplorer.Next()) {
        const TopoDS_Edge faceEdge = TopoDS::Edge(faceEdgeExplorer.Current());
        for (int rawIndex = 1; rawIndex <= rawEdges.Extent(); ++rawIndex) {
            const TopoDS_Edge rawEdge = TopoDS::Edge(rawEdges(rawIndex));
            if (samplesLieOnEdge(faceEdge, rawEdge)) {
                addDistinctString(sources, "Edge" + std::to_string(rawIndex));
            }
        }
    }

    return sources;
}

std::optional<int> matchingEdgeIndex(const TopTools_IndexedMapOfShape& rawEdges,
                                     const TopoDS_Edge& internalEdge)
{
    const auto internalEndpoints = edgeEndpoints(internalEdge);
    for (int index = 1; index <= rawEdges.Extent(); ++index) {
        const TopoDS_Edge rawEdge = TopoDS::Edge(rawEdges(index));
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::getInternalElementMap() calls Shape.findSubShapesWithSharedVertex(...,
        // CheckGeometry | SingleResult). Matching only edge endpoints can map a request-local
        // InternalEdge arc to a raw line sharing the same vertices, so cad-core mirrors the
        // geometry check here. The loose endpoint-only exception remains line-only, matching
        // TopoShapeExpansion.cpp::findSubShapesWithSharedVertex().
        if (sameEdgeEndpoints(edgeEndpoints(rawEdge), internalEndpoints)
            && sameEdgeGeometry(rawEdge, internalEdge)) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace

nlohmann::json internalElementMapForSketch(const TopoDS_Shape& rawShape,
                                           const TopoDS_Shape& internalShape)
{
    nlohmann::json result = nlohmann::json::object();

    TopTools_IndexedMapOfShape rawVertices;
    TopTools_IndexedMapOfShape internalVertices;
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    TopExp::MapShapes(internalShape, TopAbs_VERTEX, internalVertices);
    for (int index = 1; index <= internalVertices.Extent(); ++index) {
        const auto rawIndex = matchingVertexIndex(rawVertices, TopoDS::Vertex(internalVertices(index)));
        if (rawIndex) {
            addElementMapPair(result,
                              "InternalVertex" + std::to_string(index),
                              "Vertex" + std::to_string(*rawIndex));
        }
    }

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);
    for (int index = 1; index <= internalEdges.Extent(); ++index) {
        const auto rawIndex = matchingEdgeIndex(rawEdges, TopoDS::Edge(internalEdges(index)));
        if (rawIndex) {
            addElementMapPair(result,
                              "InternalEdge" + std::to_string(index),
                              "Edge" + std::to_string(*rawIndex));
        }
    }

    return result;
}

std::vector<InternalElementHistory> internalSplitElementHistoryForSketch(const TopoDS_Shape& rawShape,
                                                                         const TopoDS_Shape& internalShape)
{
    std::vector<InternalElementHistory> result;

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);

    for (int rawIndex = 1; rawIndex <= rawEdges.Extent(); ++rawIndex) {
        const TopoDS_Edge rawEdge = TopoDS::Edge(rawEdges(rawIndex));
        InternalEdgeHistoryMatch match = internalEdgeHistoryMatch(rawEdge, internalEdges);
        if (!match.exactTarget && match.splitTargets.size() > 1U) {
            result.push_back(InternalElementHistory {"Edge" + std::to_string(rawIndex), std::move(match.splitTargets)});
        }
    }

    return result;
}

std::vector<InternalGeneratedElementHistory> internalGeneratedFaceHistoryForSketch(
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape
)
{
    std::vector<InternalGeneratedElementHistory> result;

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape internalFaces;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(internalShape, TopAbs_FACE, internalFaces);

    for (int faceIndex = 1; faceIndex <= internalFaces.Extent(); ++faceIndex) {
        std::vector<std::string> sources =
            rawBoundarySourcesForInternalFace(TopoDS::Face(internalFaces(faceIndex)), rawEdges);
        if (sources.empty()) {
            continue;
        }
        result.push_back(InternalGeneratedElementHistory {
            "InternalFace" + std::to_string(faceIndex),
            std::move(sources),
        });
    }

    return result;
}

std::vector<std::string> internalDeletedElementHistoryForSketch(const TopoDS_Shape& rawShape,
                                                                const TopoDS_Shape& internalShape)
{
    std::vector<std::string> result;

    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape internalEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(internalShape, TopAbs_EDGE, internalEdges);

    TopTools_IndexedMapOfShape rawVertices;
    TopTools_IndexedMapOfShape internalVertices;
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    TopExp::MapShapes(internalShape, TopAbs_VERTEX, internalVertices);

    for (int rawIndex = 1; rawIndex <= rawEdges.Extent(); ++rawIndex) {
        InternalEdgeHistoryMatch match =
            internalEdgeHistoryMatch(TopoDS::Edge(rawEdges(rawIndex)), internalEdges);
        if (!match.exactTarget && match.splitTargets.empty()) {
            result.push_back("Edge" + std::to_string(rawIndex));
        }
    }

    for (int rawIndex = 1; rawIndex <= rawVertices.Extent(); ++rawIndex) {
        const TopoDS_Vertex rawVertex = TopoDS::Vertex(rawVertices(rawIndex));
        if (!matchingVertexIndex(internalVertices, rawVertex)) {
            result.push_back("Vertex" + std::to_string(rawIndex));
        }
    }

    return result;
}

}  // namespace cad_core::topo
