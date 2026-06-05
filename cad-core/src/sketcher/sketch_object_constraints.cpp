#include "sketch_object_constraints.h"

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/compute_context.h"

#include <Precision.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace cad_core::sketcher
{

namespace
{

std::optional<bool> readEndpointPosition(const nlohmann::json& constraint, const std::string& field);

std::optional<double> readNumberField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return std::nullopt;
    }
    if (it->is_number()) {
        const double number = it->get<double>();
        return std::isfinite(number) ? std::optional<double> {number} : std::nullopt;
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_number()) {
        const double number = it->at("value").get<double>();
        return std::isfinite(number) ? std::optional<double> {number} : std::nullopt;
    }
    return std::nullopt;
}

std::string normalizedConstraintValue(double value)
{
    return std::to_string(static_cast<long long>(std::llround(value * 100000000.0)));
}

bool sameConstraintValue(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= 1e-7;
}

std::optional<std::string> endpointTargetKey(
    const nlohmann::json& constraint,
    const std::string& indexField,
    const std::string& positionField
)
{
    const auto geometryIndex = readIntField(constraint, indexField);
    const auto start = readEndpointPosition(constraint, positionField);
    if (!geometryIndex || !start) {
        return std::nullopt;
    }
    return std::to_string(*geometryIndex) + (*start ? ":start" : ":end");
}

std::optional<std::string> endpointPairTargetKey(const nlohmann::json& constraint)
{
    const auto first = endpointTargetKey(constraint, "First", "FirstPos");
    const auto second = endpointTargetKey(constraint, "Second", "SecondPos");
    if (!first || !second) {
        return std::nullopt;
    }
    return *first < *second ? "points:" + *first + "|" + *second
                            : "points:" + *second + "|" + *first;
}

std::optional<std::string> wholeGeometryTargetKey(const nlohmann::json& constraint)
{
    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex) {
        return std::nullopt;
    }
    return "geometry:" + std::to_string(*geometryIndex);
}

std::optional<std::string> orientationSolverTargetKey(const nlohmann::json& constraint)
{
    if (constraint.contains("FirstPos") || constraint.contains("Second")
        || constraint.contains("SecondPos")) {
        return endpointPairTargetKey(constraint);
    }
    return wholeGeometryTargetKey(constraint);
}

std::optional<std::string> dimensionSolverTargetKey(
    const nlohmann::json& constraint,
    SketchConstraintKind kind
)
{
    const bool hasSecondEndpoint = constraint.contains("Second") || constraint.contains("SecondPos");
    if ((kind == SketchConstraintKind::DistanceX || kind == SketchConstraintKind::DistanceY)
        && constraint.contains("FirstPos") && !hasSecondEndpoint) {
        const auto endpoint = endpointTargetKey(constraint, "First", "FirstPos");
        if (!endpoint) {
            return std::nullopt;
        }
        return "coordinate:" + *endpoint;
    }
    if (kind != SketchConstraintKind::Radius && kind != SketchConstraintKind::Diameter
        && (constraint.contains("FirstPos") || hasSecondEndpoint)) {
        return endpointPairTargetKey(constraint);
    }
    return wholeGeometryTargetKey(constraint);
}

std::optional<std::string> angleSolverTargetKey(const nlohmann::json& constraint)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos")
        || constraint.contains("Third") || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }
    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }
    const std::string first = std::to_string(*firstGeometry);
    const std::string second = std::to_string(*secondGeometry);
    return first < second ? "geometry_pair:" + first + "|" + second
                          : "geometry_pair:" + second + "|" + first;
}


struct UnionFind
{
    std::vector<std::size_t> parent;

    explicit UnionFind(std::size_t size)
        : parent(size)
    {
        for (std::size_t index = 0; index < size; ++index) {
            parent[index] = index;
        }
    }

    std::size_t find(std::size_t value)
    {
        if (parent[value] != value) {
            parent[value] = find(parent[value]);
        }
        return parent[value];
    }

    void unite(std::size_t left, std::size_t right)
    {
        const std::size_t leftRoot = find(left);
        const std::size_t rightRoot = find(right);
        if (leftRoot != rightRoot) {
            parent[rightRoot] = leftRoot;
        }
    }
};

std::optional<bool> readEndpointPosition(const nlohmann::json& constraint, const std::string& field)
{
    if (const auto position = readStringField(constraint, field)) {
        if (*position == "start" || *position == "Start") {
            return true;
        }
        if (*position == "end" || *position == "End") {
            return false;
        }
        return std::nullopt;
    }
    if (const auto position = readIntField(constraint, field)) {
        if (*position == 1) {
            return true;
        }
        if (*position == 2) {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<ConstraintPointPosition> readPointPosition(
    const nlohmann::json& constraint,
    const std::string& field
)
{
    if (const auto position = readStringField(constraint, field)) {
        if (*position == "start" || *position == "Start") {
            return ConstraintPointPosition::Start;
        }
        if (*position == "end" || *position == "End") {
            return ConstraintPointPosition::End;
        }
        if (*position == "mid" || *position == "Mid") {
            return ConstraintPointPosition::Mid;
        }
        return std::nullopt;
    }
    if (const auto position = readIntField(constraint, field)) {
        if (*position == 1) {
            return ConstraintPointPosition::Start;
        }
        if (*position == 2) {
            return ConstraintPointPosition::End;
        }
        if (*position == 3) {
            return ConstraintPointPosition::Mid;
        }
    }
    return std::nullopt;
}

bool readPointPositionIsNone(const nlohmann::json& constraint, const std::string& field, bool absentIsNone)
{
    if (!constraint.contains(field)) {
        return absentIsNone;
    }
    if (const auto position = readStringField(constraint, field)) {
        return *position == "none" || *position == "None";
    }
    if (const auto position = readIntField(constraint, field)) {
        return *position == 0;
    }
    return false;
}

std::optional<std::size_t> segmentIndexForGeometry(
    const std::vector<SketchSegment>& segments,
    int geometryIndex
)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < segments.size(); ++index) {
        if (segments[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> pointIndexForGeometry(const std::vector<SketchPoint>& points, int geometryIndex)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (points[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> circleIndexForGeometry(
    const std::vector<SketchCircle>& circles,
    int geometryIndex
)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < circles.size(); ++index) {
        if (circles[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ellipseIndexForGeometry(
    const std::vector<SketchEllipse>& ellipses,
    int geometryIndex
)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < ellipses.size(); ++index) {
        if (ellipses[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> arcIndexForGeometry(const std::vector<SketchArc>& arcs, int geometryIndex)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < arcs.size(); ++index) {
        if (arcs[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ellipseArcIndexForGeometry(
    const std::vector<SketchEllipseArc>& arcs,
    int geometryIndex
)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < arcs.size(); ++index) {
        if (arcs[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> bsplineIndexForGeometry(
    const std::vector<SketchBSpline>& bsplines,
    int geometryIndex
)
{
    if (geometryIndex < 0) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < bsplines.size(); ++index) {
        if (bsplines[index].geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<EndpointRef> readEndpointRef(
    const nlohmann::json& constraint,
    const std::string& indexField,
    const std::string& positionField,
    const std::vector<SketchSegment>& segments
)
{
    const auto geometryIndex = readIntField(constraint, indexField);
    const auto start = readEndpointPosition(constraint, positionField);
    if (!geometryIndex || !start) {
        return std::nullopt;
    }
    const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex);
    if (!segmentIndex) {
        return std::nullopt;
    }
    return EndpointRef {*segmentIndex, *start};
}

std::optional<OrientationConstraintRef> readOrientationConstraintRef(
    const nlohmann::json& constraint,
    SketchConstraintKind kind,
    const std::vector<SketchSegment>& segments
)
{
    if (constraint.contains("FirstPos") || constraint.contains("Second")
        || constraint.contains("SecondPos")) {
        const auto first = readEndpointRef(constraint, "First", "FirstPos", segments);
        const auto second = readEndpointRef(constraint, "Second", "SecondPos", segments);
        if (!first || !second) {
            return std::nullopt;
        }
        return OrientationConstraintRef {kind, std::nullopt, first, second};
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex) {
        return std::nullopt;
    }
    const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex);
    if (!segmentIndex) {
        return std::nullopt;
    }
    return OrientationConstraintRef {kind, segmentIndex, std::nullopt, std::nullopt};
}

std::optional<DimensionConstraintRef> readDimensionConstraintRef(
    const nlohmann::json& constraint,
    SketchConstraintKind kind,
    const std::vector<SketchSegment>& segments
)
{
    const auto value = readConstraintValue(constraint);
    if (!value || !std::isfinite(*value)) {
        return std::nullopt;
    }

    const bool hasSecondEndpoint = constraint.contains("Second") || constraint.contains("SecondPos");
    if ((kind == SketchConstraintKind::DistanceX || kind == SketchConstraintKind::DistanceY)
        && constraint.contains("FirstPos") && !hasSecondEndpoint) {
        const auto endpoint = readEndpointRef(constraint, "First", "FirstPos", segments);
        if (!endpoint) {
            return std::nullopt;
        }
        return DimensionConstraintRef {kind, -1, *value, std::nullopt, std::nullopt, endpoint};
    }

    if (*value <= 0.0) {
        return std::nullopt;
    }

    if (kind != SketchConstraintKind::Radius && kind != SketchConstraintKind::Diameter
        && (constraint.contains("FirstPos") || hasSecondEndpoint)) {
        const auto first = readEndpointRef(constraint, "First", "FirstPos", segments);
        const auto second = readEndpointRef(constraint, "Second", "SecondPos", segments);
        if (!first || !second) {
            return std::nullopt;
        }
        return DimensionConstraintRef {kind, -1, *value, first, second};
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex) {
        return std::nullopt;
    }
    return DimensionConstraintRef {kind, *geometryIndex, *value, std::nullopt, std::nullopt};
}

std::optional<double> dimensionConstraintValue(
    const DimensionConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
);

SketchSolverSummary analyzeMalformedSketchConstraints(
    const nlohmann::json& constraints,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    SketchSolverSummary summary;
    if (!constraints.is_array()) {
        return summary;
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/PropertyConstraintList.cpp
    // ::PropertyConstraintList::checkConstraintIndices() rejects invalid constraint geometry
    // indices; /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/
    // SketchObjectConstraints.cpp::SketchObject::retrieveSolverDiagnostics() records
    // "lastHasMalformedConstraints = solvedSketch.hasMalformedConstraints()";
    // SketchObject.cpp::SketchObject::execute() reports "Sketch with malformed constraints".
    for (std::size_t offset = 0; offset < constraints.size(); ++offset) {
        const int constraintIndex = static_cast<int>(offset + 1U);
        const auto& constraint = constraints[offset];
        if (!constraint.is_object()) {
            addUniqueConstraintIndex(summary.malformedConstraints, constraintIndex);
            continue;
        }

        const SketchConstraintKind kind = readSketchConstraintKind(constraint);
        if (kind == SketchConstraintKind::Horizontal || kind == SketchConstraintKind::Vertical) {
            if (!readOrientationConstraintRef(constraint, kind, segments)) {
                addUniqueConstraintIndex(summary.malformedConstraints, constraintIndex);
            }
            continue;
        }

        if (isDimensionSolverConstraint(kind)) {
            const auto dimension = readDimensionConstraintRef(constraint, kind, segments);
            if (!dimension || !dimensionConstraintValue(*dimension, segments, circles, arcs)) {
                addUniqueConstraintIndex(summary.malformedConstraints, constraintIndex);
            }
        }
    }

    if (!summary.malformedConstraints.empty()) {
        summary.state = SketchSolverState::Malformed;
    }
    return summary;
}

std::optional<LinePairConstraintRef> readLinePairConstraintRef(
    const nlohmann::json& constraint,
    SketchConstraintKind kind,
    const std::vector<SketchSegment>& segments
)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos")) {
        return std::nullopt;
    }

    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }
    const auto firstSegment = segmentIndexForGeometry(segments, *firstGeometry);
    const auto secondSegment = segmentIndexForGeometry(segments, *secondGeometry);
    if (!firstSegment || !secondSegment) {
        return std::nullopt;
    }
    return LinePairConstraintRef {kind, *firstSegment, *secondSegment};
}

std::optional<TangentGeometryRef> tangentGeometryRefForIndex(
    int geometryIndex,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (const auto segment = segmentIndexForGeometry(segments, geometryIndex)) {
        return TangentGeometryRef {TangentGeometryKind::Line, *segment};
    }
    if (const auto circle = circleIndexForGeometry(circles, geometryIndex)) {
        return TangentGeometryRef {TangentGeometryKind::Circle, *circle};
    }
    if (const auto arc = arcIndexForGeometry(arcs, geometryIndex)) {
        return TangentGeometryRef {TangentGeometryKind::Arc, *arc};
    }
    if (const auto ellipse = ellipseIndexForGeometry(ellipses, geometryIndex)) {
        return TangentGeometryRef {TangentGeometryKind::Ellipse, *ellipse};
    }
    if (const auto ellipseArc = ellipseArcIndexForGeometry(ellipseArcs, geometryIndex)) {
        return TangentGeometryRef {TangentGeometryKind::EllipseArc, *ellipseArc};
    }
    return std::nullopt;
}

bool directTangentPairSupported(const TangentGeometryRef& first, const TangentGeometryRef& second)
{
    if (first.kind == TangentGeometryKind::Line) {
        return true;
    }
    if (second.kind == TangentGeometryKind::Line) {
        return true;
    }

    const bool firstRound = first.kind == TangentGeometryKind::Circle
        || first.kind == TangentGeometryKind::Arc;
    const bool secondRound = second.kind == TangentGeometryKind::Circle
        || second.kind == TangentGeometryKind::Arc;
    return firstRound && secondRound;
}

std::optional<ConstraintPointRef> readTangentCurvePointRef(
    const nlohmann::json& constraint,
    const std::string& geometryField,
    const std::string& positionField,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position) {
        return std::nullopt;
    }

    if (*position == ConstraintPointPosition::Start || *position == ConstraintPointPosition::End) {
        if (const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::SegmentEndpoint,
                *segmentIndex,
                *position == ConstraintPointPosition::Start
            };
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::ArcEndpoint,
                *arcIndex,
                *position == ConstraintPointPosition::Start
            };
        }
        if (const auto ellipseArcIndex = ellipseArcIndexForGeometry(ellipseArcs, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::EllipseArcEndpoint,
                *ellipseArcIndex,
                *position == ConstraintPointPosition::Start
            };
        }
    }

    return std::nullopt;
}

std::optional<ConstraintPointRef> readTangentPointRef(
    const nlohmann::json& constraint,
    const std::string& geometryField,
    const std::string& positionField,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (
        const auto curvePoint
        = readTangentCurvePointRef(constraint, geometryField, positionField, segments, arcs, ellipseArcs)
    ) {
        return curvePoint;
    }

    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position || *position != ConstraintPointPosition::Start) {
        return std::nullopt;
    }
    if (const auto pointIndex = pointIndexForGeometry(points, *geometryIndex)) {
        return ConstraintPointRef {ConstraintPointKind::PointGeometry, *pointIndex, true};
    }
    return std::nullopt;
}

std::optional<TangentConstraintRef> readTangentConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }

    const auto first
        = tangentGeometryRefForIndex(*firstGeometry, segments, circles, ellipses, arcs, ellipseArcs);
    const auto second
        = tangentGeometryRefForIndex(*secondGeometry, segments, circles, ellipses, arcs, ellipseArcs);
    if (!first || !second) {
        return std::nullopt;
    }

    const bool firstNone = readPointPositionIsNone(constraint, "FirstPos", true);
    const bool secondNone = readPointPositionIsNone(constraint, "SecondPos", true);
    const bool hasThird = constraint.contains("Third");
    if (firstNone && secondNone && !hasThird) {
        if (!directTangentPairSupported(*first, *second)) {
            return std::nullopt;
        }
        return TangentConstraintRef {*first, *second};
    }

    if (hasThird) {
        if (!firstNone || !secondNone) {
            return std::nullopt;
        }
        const auto viaPoint
            = readTangentPointRef(constraint, "Third", "ThirdPos", segments, points, arcs, ellipseArcs);
        if (!viaPoint) {
            return std::nullopt;
        }
        return TangentConstraintRef {*first, *second, std::nullopt, std::nullopt, viaPoint};
    }

    const auto firstPoint
        = readTangentCurvePointRef(constraint, "First", "FirstPos", segments, arcs, ellipseArcs);
    if (!firstPoint) {
        return std::nullopt;
    }
    if (secondNone) {
        return TangentConstraintRef {*first, *second, firstPoint};
    }

    const auto secondPoint
        = readTangentCurvePointRef(constraint, "Second", "SecondPos", segments, arcs, ellipseArcs);
    if (!secondPoint) {
        return std::nullopt;
    }
    return TangentConstraintRef {*first, *second, firstPoint, secondPoint};
}

std::optional<PointwiseAngleConstraintRef> readPointwiseAngleConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto value = readConstraintValue(constraint);
    if (!value || !std::isfinite(*value) || *value < 0.0 || *value > 3.14159265358979323846) {
        return std::nullopt;
    }

    const auto geometry
        = readTangentConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!geometry || (!geometry->firstPoint && !geometry->viaPoint)) {
        return std::nullopt;
    }
    return PointwiseAngleConstraintRef {*geometry, *value};
}

std::optional<ConstraintPointRef> readConstraintPointRef(
    const nlohmann::json& constraint,
    const std::string& geometryField,
    const std::string& positionField,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
);

std::optional<PerpendicularPointLineConstraintRef> readPerpendicularPointLineConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (!readPointPositionIsNone(constraint, "ThirdPos", true)) {
        return std::nullopt;
    }

    const auto first = readConstraintPointRef(
        constraint,
        "First",
        "FirstPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    const auto second = readConstraintPointRef(
        constraint,
        "Second",
        "SecondPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    const auto thirdGeometry = readIntField(constraint, "Third");
    if (!first || !second || !thirdGeometry) {
        return std::nullopt;
    }

    const auto axisSegment = segmentIndexForGeometry(segments, *thirdGeometry);
    if (!axisSegment) {
        return std::nullopt;
    }
    return PerpendicularPointLineConstraintRef {*first, *second, *axisSegment};
}

std::optional<PerpendicularMidpointLineConstraintRef> readPerpendicularMidpointLineConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos")
        || constraint.contains("Third") || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }

    const auto firstSegment = segmentIndexForGeometry(segments, *firstGeometry);
    const auto secondSegment = segmentIndexForGeometry(segments, *secondGeometry);
    const auto readTarget =
        [&](int geometryIndex) -> std::optional<PerpendicularMidpointLineConstraintRef> {
        if (const auto circleIndex = circleIndexForGeometry(circles, geometryIndex)) {
            return PerpendicularMidpointLineConstraintRef {
                0,
                PerpendicularMidpointTargetKind::Circle,
                *circleIndex
            };
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, geometryIndex)) {
            return PerpendicularMidpointLineConstraintRef {
                0,
                PerpendicularMidpointTargetKind::Arc,
                *arcIndex
            };
        }
        return std::nullopt;
    };

    if (firstSegment && !secondSegment) {
        if (auto target = readTarget(*secondGeometry)) {
            target->lineSegmentIndex = *firstSegment;
            return target;
        }
    }
    if (secondSegment && !firstSegment) {
        if (auto target = readTarget(*firstGeometry)) {
            target->lineSegmentIndex = *secondSegment;
            return target;
        }
    }
    return std::nullopt;
}

std::optional<EqualConstraintRef> readEqualConstraintRef(const nlohmann::json& constraint)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos")) {
        return std::nullopt;
    }
    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }
    return EqualConstraintRef {*firstGeometry, *secondGeometry};
}

std::optional<AngleConstraintRef> readAngleConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments
)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos")
        || constraint.contains("Third") || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto value = readConstraintValue(constraint);
    if (!value || !std::isfinite(*value) || *value < 0.0 || *value > 3.14159265358979323846) {
        return std::nullopt;
    }

    const auto linePair = readLinePairConstraintRef(constraint, SketchConstraintKind::Angle, segments);
    if (!linePair) {
        return std::nullopt;
    }
    return AngleConstraintRef {*linePair, *value};
}

std::optional<ConstraintPointRef> readConstraintPointRef(
    const nlohmann::json& constraint,
    const std::string& geometryField,
    const std::string& positionField,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position) {
        return std::nullopt;
    }

    if (*position == ConstraintPointPosition::Start || *position == ConstraintPointPosition::End) {
        if (const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::SegmentEndpoint,
                *segmentIndex,
                *position == ConstraintPointPosition::Start
            };
        }
        if (*position == ConstraintPointPosition::Start) {
            if (const auto pointIndex = pointIndexForGeometry(points, *geometryIndex)) {
                return ConstraintPointRef {ConstraintPointKind::PointGeometry, *pointIndex, true};
            }
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::ArcEndpoint,
                *arcIndex,
                *position == ConstraintPointPosition::Start
            };
        }
        if (const auto ellipseArcIndex = ellipseArcIndexForGeometry(ellipseArcs, *geometryIndex)) {
            return ConstraintPointRef {
                ConstraintPointKind::EllipseArcEndpoint,
                *ellipseArcIndex,
                *position == ConstraintPointPosition::Start
            };
        }
    }

    if (*position == ConstraintPointPosition::Mid) {
        if (const auto circleIndex = circleIndexForGeometry(circles, *geometryIndex)) {
            return ConstraintPointRef {ConstraintPointKind::CircleCenter, *circleIndex, true};
        }
        if (const auto ellipseIndex = ellipseIndexForGeometry(ellipses, *geometryIndex)) {
            return ConstraintPointRef {ConstraintPointKind::EllipseCenter, *ellipseIndex, true};
        }
    }

    return std::nullopt;
}

std::optional<ConstraintPointRef> readConstraintPointRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    return readConstraintPointRef(
        constraint,
        "First",
        "FirstPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
}

bool hasPointOnObjectTarget(
    int geometryIndex,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    return segmentIndexForGeometry(segments, geometryIndex).has_value()
        || circleIndexForGeometry(circles, geometryIndex).has_value()
        || ellipseIndexForGeometry(ellipses, geometryIndex).has_value()
        || arcIndexForGeometry(arcs, geometryIndex).has_value()
        || ellipseArcIndexForGeometry(ellipseArcs, geometryIndex).has_value();
}

std::optional<PointOnObjectConstraintRef> readPointOnObjectConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (constraint.contains("SecondPos") || constraint.contains("Third")
        || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto targetGeometry = readIntField(constraint, "Second");
    if (!targetGeometry
        || !hasPointOnObjectTarget(*targetGeometry, segments, circles, ellipses, arcs, ellipseArcs)) {
        return std::nullopt;
    }

    const auto point
        = readConstraintPointRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!point) {
        return std::nullopt;
    }
    return PointOnObjectConstraintRef {*point, *targetGeometry};
}

std::optional<SymmetricConstraintRef> readSymmetricConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto first = readConstraintPointRef(
        constraint,
        "First",
        "FirstPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    const auto second = readConstraintPointRef(
        constraint,
        "Second",
        "SecondPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    const auto thirdGeometry = readIntField(constraint, "Third");
    if (!first || !second || !thirdGeometry) {
        return std::nullopt;
    }

    if (readPointPositionIsNone(constraint, "ThirdPos", true)) {
        const auto axisSegment = segmentIndexForGeometry(segments, *thirdGeometry);
        if (!axisSegment) {
            return std::nullopt;
        }
        return SymmetricConstraintRef {*first, *second, axisSegment, std::nullopt};
    }

    const auto center = readConstraintPointRef(
        constraint,
        "Third",
        "ThirdPos",
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    if (!center) {
        return std::nullopt;
    }
    return SymmetricConstraintRef {*first, *second, std::nullopt, center};
}

bool hasBlockTarget(
    int geometryIndex,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs,
    const std::vector<SketchBSpline>& bsplines
)
{
    return segmentIndexForGeometry(segments, geometryIndex).has_value()
        || pointIndexForGeometry(points, geometryIndex).has_value()
        || circleIndexForGeometry(circles, geometryIndex).has_value()
        || ellipseIndexForGeometry(ellipses, geometryIndex).has_value()
        || arcIndexForGeometry(arcs, geometryIndex).has_value()
        || ellipseArcIndexForGeometry(ellipseArcs, geometryIndex).has_value()
        || bsplineIndexForGeometry(bsplines, geometryIndex).has_value();
}

std::optional<BlockConstraintRef> readBlockConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs,
    const std::vector<SketchBSpline>& bsplines
)
{
    if ((constraint.contains("FirstPos") && !readPointPositionIsNone(constraint, "FirstPos", false))
        || constraint.contains("Second") || constraint.contains("SecondPos")
        || constraint.contains("Third") || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (
        !geometryIndex
        || !hasBlockTarget(*geometryIndex, segments, points, circles, ellipses, arcs, ellipseArcs, bsplines)
    ) {
        return std::nullopt;
    }
    return BlockConstraintRef {*geometryIndex};
}

std::size_t endpointId(const EndpointRef& endpoint)
{
    return endpoint.segmentIndex * 2U + (endpoint.start ? 0U : 1U);
}

const gp_Pnt& endpointPoint(const SketchSegment& segment, bool start)
{
    return start ? segment.start : segment.end;
}

gp_Pnt& endpointPoint(SketchSegment& segment, bool start)
{
    return start ? segment.start : segment.end;
}

gp_Pnt endpointPoint(const std::vector<SketchSegment>& segments, const EndpointRef& endpoint)
{
    return endpointPoint(segments.at(endpoint.segmentIndex), endpoint.start);
}

bool orientationConstraintSatisfied(
    const OrientationConstraintRef& constraint,
    const std::vector<SketchSegment>& segments
)
{
    std::optional<gp_Pnt> first;
    std::optional<gp_Pnt> second;
    if (constraint.segmentIndex) {
        const SketchSegment& segment = segments.at(*constraint.segmentIndex);
        first = segment.start;
        second = segment.end;
    }
    else if (constraint.firstEndpoint && constraint.secondEndpoint) {
        first = endpointPoint(segments, *constraint.firstEndpoint);
        second = endpointPoint(segments, *constraint.secondEndpoint);
    }
    if (!first || !second) {
        return false;
    }

    if (constraint.kind == SketchConstraintKind::Horizontal) {
        return std::abs(first->Y() - second->Y()) <= Precision::Confusion();
    }
    if (constraint.kind == SketchConstraintKind::Vertical) {
        return std::abs(first->X() - second->X()) <= Precision::Confusion();
    }
    return false;
}

std::optional<double> dimensionConstraintValue(
    const DimensionConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    if (constraint.kind == SketchConstraintKind::Distance
        || constraint.kind == SketchConstraintKind::DistanceX
        || constraint.kind == SketchConstraintKind::DistanceY) {
        std::optional<gp_Pnt> first;
        std::optional<gp_Pnt> second;
        if (constraint.coordinateEndpoint) {
            const gp_Pnt point = endpointPoint(segments, *constraint.coordinateEndpoint);
            if (constraint.kind == SketchConstraintKind::DistanceX) {
                return point.X();
            }
            if (constraint.kind == SketchConstraintKind::DistanceY) {
                return point.Y();
            }
            return std::nullopt;
        }
        if (constraint.firstEndpoint && constraint.secondEndpoint) {
            first = endpointPoint(segments, *constraint.firstEndpoint);
            second = endpointPoint(segments, *constraint.secondEndpoint);
        }
        else {
            const auto segmentIndex = segmentIndexForGeometry(segments, constraint.geometryIndex);
            if (!segmentIndex) {
                return std::nullopt;
            }
            const SketchSegment& segment = segments.at(*segmentIndex);
            first = segment.start;
            second = segment.end;
        }
        if (!first || !second) {
            return std::nullopt;
        }
        if (constraint.kind == SketchConstraintKind::DistanceX) {
            return std::abs(second->X() - first->X());
        }
        if (constraint.kind == SketchConstraintKind::DistanceY) {
            return std::abs(second->Y() - first->Y());
        }
        return first->Distance(*second);
    }

    if (constraint.kind == SketchConstraintKind::Radius
        || constraint.kind == SketchConstraintKind::Diameter) {
        for (const SketchCircle& circle : circles) {
            if (circle.geometryIndex == static_cast<std::size_t>(constraint.geometryIndex)) {
                return constraint.kind == SketchConstraintKind::Diameter ? 2.0 * circle.radius
                                                                         : circle.radius;
            }
        }
        for (const SketchArc& arc : arcs) {
            if (arc.geometryIndex == static_cast<std::size_t>(constraint.geometryIndex)) {
                return constraint.kind == SketchConstraintKind::Diameter ? 2.0 * arc.radius
                                                                         : arc.radius;
            }
        }
    }
    return std::nullopt;
}

bool dimensionConstraintSatisfied(
    const DimensionConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    const auto actual = dimensionConstraintValue(constraint, segments, circles, arcs);
    if (!actual) {
        return false;
    }
    return std::abs(*actual - constraint.value) <= 1e-7;
}

std::optional<std::pair<double, double>> segmentDirection2d(const SketchSegment& segment)
{
    const double dx = segment.end.X() - segment.start.X();
    const double dy = segment.end.Y() - segment.start.Y();
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= Precision::Confusion()) {
        return std::nullopt;
    }
    return std::pair<double, double> {dx / length, dy / length};
}

double pointToLineDistance2d(const gp_Pnt& point, const SketchSegment& segment);
std::optional<gp_Pnt> constraintPointValue(
    const ConstraintPointRef& point,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
);

bool linePairConstraintSatisfied(
    const LinePairConstraintRef& constraint,
    const std::vector<SketchSegment>& segments
)
{
    const auto first = segmentDirection2d(segments.at(constraint.firstSegmentIndex));
    const auto second = segmentDirection2d(segments.at(constraint.secondSegmentIndex));
    if (!first || !second) {
        return false;
    }

    const double cross = first->first * second->second - first->second * second->first;
    const double dot = first->first * second->first + first->second * second->second;
    if (constraint.kind == SketchConstraintKind::Parallel) {
        return std::abs(cross) <= 1e-9;
    }
    if (constraint.kind == SketchConstraintKind::Perpendicular) {
        return std::abs(dot) <= 1e-9;
    }
    return false;
}

std::optional<double> linePairAngleValue(
    const LinePairConstraintRef& constraint,
    const std::vector<SketchSegment>& segments
)
{
    const auto first = segmentDirection2d(segments.at(constraint.firstSegmentIndex));
    const auto second = segmentDirection2d(segments.at(constraint.secondSegmentIndex));
    if (!first || !second) {
        return std::nullopt;
    }

    const double dot = first->first * second->first + first->second * second->second;
    return std::acos(std::clamp(std::abs(dot), -1.0, 1.0));
}

struct RoundTangentGeometry
{
    gp_Pnt center;
    double radius = 0.0;
};

struct EllipseTangentGeometry
{
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
};

std::optional<RoundTangentGeometry> roundTangentGeometry(
    const TangentGeometryRef& geometry,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    if (geometry.kind == TangentGeometryKind::Circle) {
        const SketchCircle& circle = circles.at(geometry.index);
        return RoundTangentGeometry {circle.center, circle.radius};
    }
    if (geometry.kind == TangentGeometryKind::Arc) {
        const SketchArc& arc = arcs.at(geometry.index);
        return RoundTangentGeometry {arc.center, arc.radius};
    }
    return std::nullopt;
}

std::optional<EllipseTangentGeometry> ellipseTangentGeometry(
    const TangentGeometryRef& geometry,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (geometry.kind == TangentGeometryKind::Ellipse) {
        const SketchEllipse& ellipse = ellipses.at(geometry.index);
        return EllipseTangentGeometry {
            ellipse.center,
            ellipse.majorRadius,
            ellipse.minorRadius,
            ellipse.angle
        };
    }
    if (geometry.kind == TangentGeometryKind::EllipseArc) {
        const SketchEllipseArc& arc = ellipseArcs.at(geometry.index);
        return EllipseTangentGeometry {arc.center, arc.majorRadius, arc.minorRadius, arc.angle};
    }
    return std::nullopt;
}

bool lineLineTangentSatisfied(const SketchSegment& first, const SketchSegment& second)
{
    const auto firstDirection = segmentDirection2d(first);
    const auto secondDirection = segmentDirection2d(second);
    if (!firstDirection || !secondDirection) {
        return false;
    }

    const double cross = firstDirection->first * secondDirection->second
        - firstDirection->second * secondDirection->first;
    return std::abs(cross) <= 1e-9 && pointToLineDistance2d(second.start, first) <= 1e-7
        && pointToLineDistance2d(second.end, first) <= 1e-7;
}

bool lineRoundTangentSatisfied(const SketchSegment& line, const RoundTangentGeometry& round)
{
    return std::abs(pointToLineDistance2d(round.center, line) - round.radius) <= 1e-7;
}

bool lineEllipseTangentSatisfied(const SketchSegment& line, const EllipseTangentGeometry& ellipse)
{
    const auto direction = segmentDirection2d(line);
    if (!direction) {
        return false;
    }

    const double nx = -direction->second;
    const double ny = direction->first;
    const double cosAxis = std::cos(ellipse.angle);
    const double sinAxis = std::sin(ellipse.angle);
    const double normalMajor = nx * cosAxis + ny * sinAxis;
    const double normalMinor = -nx * sinAxis + ny * cosAxis;
    const double support = std::sqrt(
        ellipse.majorRadius * ellipse.majorRadius * normalMajor * normalMajor
        + ellipse.minorRadius * ellipse.minorRadius * normalMinor * normalMinor
    );
    return std::abs(pointToLineDistance2d(ellipse.center, line) - support) <= 1e-7;
}

bool roundRoundTangentSatisfied(const RoundTangentGeometry& first, const RoundTangentGeometry& second)
{
    const double distance = first.center.Distance(second.center);
    const double external = first.radius + second.radius;
    const double internal = std::abs(first.radius - second.radius);
    return std::abs(distance - external) <= 1e-7
        || (internal > 1e-7 && std::abs(distance - internal) <= 1e-7);
}

std::optional<std::pair<double, double>> normalizeDirection2d(double dx, double dy)
{
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= Precision::Confusion()) {
        return std::nullopt;
    }
    return std::pair<double, double> {dx / length, dy / length};
}

std::optional<std::pair<double, double>> roundTangentDirectionAtPoint(
    const RoundTangentGeometry& round,
    const gp_Pnt& point
)
{
    const double rx = point.X() - round.center.X();
    const double ry = point.Y() - round.center.Y();
    const double radius = std::sqrt(rx * rx + ry * ry);
    if (std::abs(radius - round.radius) > 1e-7) {
        return std::nullopt;
    }
    return normalizeDirection2d(-ry, rx);
}

std::optional<std::pair<double, double>> ellipseTangentDirectionAtPoint(
    const EllipseTangentGeometry& ellipse,
    const gp_Pnt& point
)
{
    const double dx = point.X() - ellipse.center.X();
    const double dy = point.Y() - ellipse.center.Y();
    const double cosAxis = std::cos(ellipse.angle);
    const double sinAxis = std::sin(ellipse.angle);
    const double localX = dx * cosAxis + dy * sinAxis;
    const double localY = -dx * sinAxis + dy * cosAxis;
    const double value = (localX * localX) / (ellipse.majorRadius * ellipse.majorRadius)
        + (localY * localY) / (ellipse.minorRadius * ellipse.minorRadius);
    if (std::abs(value - 1.0) > 1e-7) {
        return std::nullopt;
    }

    const double localTangentX = -ellipse.majorRadius * localY / ellipse.minorRadius;
    const double localTangentY = ellipse.minorRadius * localX / ellipse.majorRadius;
    return normalizeDirection2d(
        localTangentX * cosAxis - localTangentY * sinAxis,
        localTangentX * sinAxis + localTangentY * cosAxis
    );
}

std::optional<std::pair<double, double>> tangentDirectionAtPoint(
    const TangentGeometryRef& geometry,
    const gp_Pnt& point,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    if (geometry.kind == TangentGeometryKind::Line) {
        const SketchSegment& line = segments.at(geometry.index);
        if (pointToLineDistance2d(point, line) > 1e-7) {
            return std::nullopt;
        }
        return segmentDirection2d(line);
    }
    if (const auto round = roundTangentGeometry(geometry, circles, arcs)) {
        return roundTangentDirectionAtPoint(*round, point);
    }
    if (const auto ellipse = ellipseTangentGeometry(geometry, ellipses, ellipseArcs)) {
        return ellipseTangentDirectionAtPoint(*ellipse, point);
    }
    return std::nullopt;
}

bool tangentDirectionsParallel(
    const TangentGeometryRef& first,
    const gp_Pnt& firstPoint,
    const TangentGeometryRef& second,
    const gp_Pnt& secondPoint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto firstDirection
        = tangentDirectionAtPoint(first, firstPoint, segments, circles, ellipses, arcs, ellipseArcs);
    const auto secondDirection
        = tangentDirectionAtPoint(second, secondPoint, segments, circles, ellipses, arcs, ellipseArcs);
    if (!firstDirection || !secondDirection) {
        return false;
    }

    const double cross = firstDirection->first * secondDirection->second
        - firstDirection->second * secondDirection->first;
    return std::abs(cross) <= 1e-7;
}

bool tangentDirectionsPerpendicular(
    const TangentGeometryRef& first,
    const gp_Pnt& firstPoint,
    const TangentGeometryRef& second,
    const gp_Pnt& secondPoint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto firstDirection
        = tangentDirectionAtPoint(first, firstPoint, segments, circles, ellipses, arcs, ellipseArcs);
    const auto secondDirection
        = tangentDirectionAtPoint(second, secondPoint, segments, circles, ellipses, arcs, ellipseArcs);
    if (!firstDirection || !secondDirection) {
        return false;
    }

    const double dot = firstDirection->first * secondDirection->first
        + firstDirection->second * secondDirection->second;
    return std::abs(dot) <= 1e-7;
}

std::optional<double> pointwiseCurveAngleValue(
    const TangentConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto angleAt = [&](const gp_Pnt& firstPoint,
                             const gp_Pnt& secondPoint) -> std::optional<double> {
        const auto firstDirection = tangentDirectionAtPoint(
            constraint.first,
            firstPoint,
            segments,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
        const auto secondDirection = tangentDirectionAtPoint(
            constraint.second,
            secondPoint,
            segments,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
        if (!firstDirection || !secondDirection) {
            return std::nullopt;
        }

        const double dot = firstDirection->first * secondDirection->first
            + firstDirection->second * secondDirection->second;
        return std::acos(std::clamp(std::abs(dot), -1.0, 1.0));
    };

    if (constraint.viaPoint) {
        const auto point = constraintPointValue(
            *constraint.viaPoint,
            segments,
            points,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
        if (!point) {
            return std::nullopt;
        }
        return angleAt(*point, *point);
    }

    if (!constraint.firstPoint) {
        return std::nullopt;
    }

    const auto firstPoint = constraintPointValue(
        *constraint.firstPoint,
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    if (!firstPoint) {
        return std::nullopt;
    }

    if (constraint.secondPoint) {
        const auto secondPoint = constraintPointValue(
            *constraint.secondPoint,
            segments,
            points,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
        if (!secondPoint || firstPoint->Distance(*secondPoint) > 1e-7) {
            return std::nullopt;
        }
        return angleAt(*firstPoint, *secondPoint);
    }

    return angleAt(*firstPoint, *firstPoint);
}

bool pointwiseCurveAngleConstraintSatisfied(
    const TangentConstraintRef& constraint,
    bool tangent,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    const auto actual
        = pointwiseCurveAngleValue(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!actual) {
        return false;
    }

    const double expected = tangent ? 0.0 : 3.14159265358979323846 / 2.0;
    return std::abs(*actual - expected) <= 1e-7;
}

bool pointwiseAngleConstraintSatisfied(
    const PointwiseAngleConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addAngleAtPointConstraint() accepts Angle together with Tangent and
    // Perpendicular. For Angle it keeps the user-provided value and adds an angle-via-point
    // GCS constraint. cad-core verifies the already-satisfied point-wise datum and does not move
    // geometry.
    const auto actual = pointwiseCurveAngleValue(
        constraint.geometry,
        segments,
        points,
        circles,
        ellipses,
        arcs,
        ellipseArcs
    );
    if (!actual) {
        return false;
    }

    const double expected = std::min(constraint.value, 3.14159265358979323846 - constraint.value);
    return std::abs(*actual - expected) <= 1e-7;
}

bool tangentConstraintSatisfied(
    const TangentConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Tangent routes whole-geometry constraints to
    // "addTangentConstraint(...)" and routes endpoint-to-curve, endpoint-to-endpoint and
    // tangent-via-point forms to "addAngleAtPointConstraint(..., Tangent, ...)". cad-core
    // verifies already-satisfied direct and point-wise tangency without invoking the solver.
    if (constraint.viaPoint || constraint.firstPoint) {
        return pointwiseCurveAngleConstraintSatisfied(
            constraint,
            true,
            segments,
            points,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
    }

    if (constraint.first.kind == TangentGeometryKind::Line) {
        const SketchSegment& line = segments.at(constraint.first.index);
        if (constraint.second.kind == TangentGeometryKind::Line) {
            return lineLineTangentSatisfied(line, segments.at(constraint.second.index));
        }
        if (const auto round = roundTangentGeometry(constraint.second, circles, arcs)) {
            return lineRoundTangentSatisfied(line, *round);
        }
        if (const auto ellipse = ellipseTangentGeometry(constraint.second, ellipses, ellipseArcs)) {
            return lineEllipseTangentSatisfied(line, *ellipse);
        }
        return false;
    }

    if (constraint.second.kind == TangentGeometryKind::Line) {
        return tangentConstraintSatisfied(
            TangentConstraintRef {constraint.second, constraint.first},
            segments,
            points,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
    }

    const auto firstRound = roundTangentGeometry(constraint.first, circles, arcs);
    const auto secondRound = roundTangentGeometry(constraint.second, circles, arcs);
    if (firstRound && secondRound) {
        return roundRoundTangentSatisfied(*firstRound, *secondRound);
    }
    return false;
}

std::optional<double> equalConstraintMeasure(
    int geometryIndex,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    if (const auto segmentIndex = segmentIndexForGeometry(segments, geometryIndex)) {
        const SketchSegment& segment = segments.at(*segmentIndex);
        return segment.start.Distance(segment.end);
    }
    for (const SketchCircle& circle : circles) {
        if (circle.geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return circle.radius;
        }
    }
    for (const SketchArc& arc : arcs) {
        if (arc.geometryIndex == static_cast<std::size_t>(geometryIndex)) {
            return arc.radius;
        }
    }
    return std::nullopt;
}

bool equalConstraintSatisfied(
    const EqualConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addEqualConstraint(), for two lines calls "addConstraintEqualLength";
    // for circles/arcs calls equal-radius constraints. cad-core verifies only already
    // satisfied line-length and circle/arc-radius equality, without invoking the solver.
    const auto first = equalConstraintMeasure(constraint.firstGeometryIndex, segments, circles, arcs);
    const auto second = equalConstraintMeasure(constraint.secondGeometryIndex, segments, circles, arcs);
    if (!first || !second) {
        return false;
    }
    return std::abs(*first - *second) <= 1e-7;
}

bool angleConstraintSatisfied(
    const AngleConstraintRef& constraint,
    const std::vector<SketchSegment>& segments
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Angle routes whole-line pairs to
    // "addAngleConstraint(constraint->First, constraint->Second, c.value, c.driving)";
    // that overload calls planegcs "addConstraintL2LAngle(l1, l2, value, tag, driving)".
    // cad-core only verifies already-satisfied whole-line pair datums; it does not run the solver
    // or move geometry to satisfy the angle.
    const auto actual = linePairAngleValue(constraint.linePair, segments);
    if (!actual) {
        return false;
    }
    const double expected = std::min(constraint.value, 3.14159265358979323846 - constraint.value);
    return std::abs(*actual - expected) <= 1e-7;
}

std::optional<gp_Pnt> constraintPointValue(
    const ConstraintPointRef& point,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    switch (point.kind) {
        case ConstraintPointKind::SegmentEndpoint:
            return endpointPoint(segments.at(point.index), point.start);
        case ConstraintPointKind::PointGeometry:
            return points.at(point.index).point;
        case ConstraintPointKind::CircleCenter:
            return circles.at(point.index).center;
        case ConstraintPointKind::EllipseCenter:
            return ellipses.at(point.index).center;
        case ConstraintPointKind::ArcEndpoint: {
            const SketchArc& arc = arcs.at(point.index);
            return pointAtAngle(arc.center, arc.radius, point.start ? arc.startAngle : arc.endAngle);
        }
        case ConstraintPointKind::EllipseArcEndpoint: {
            const SketchEllipseArc& arc = ellipseArcs.at(point.index);
            return pointAtEllipseAngle(
                arc.center,
                arc.majorRadius,
                arc.minorRadius,
                arc.angle,
                point.start ? arc.startAngle : arc.endAngle
            );
        }
    }
    return std::nullopt;
}

double pointToLineDistance2d(const gp_Pnt& point, const SketchSegment& segment)
{
    const double dx = segment.end.X() - segment.start.X();
    const double dy = segment.end.Y() - segment.start.Y();
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= Precision::Confusion()) {
        return std::numeric_limits<double>::infinity();
    }
    const double area = -point.X() * dy + point.Y() * dx + segment.start.X() * segment.end.Y()
        - segment.end.X() * segment.start.Y();
    return std::abs(area) / length;
}

double ellipseEquationError(
    const gp_Pnt& point,
    const gp_Pnt& center,
    double majorRadius,
    double minorRadius,
    double angle
)
{
    const double dx = point.X() - center.X();
    const double dy = point.Y() - center.Y();
    const double cosAxis = std::cos(angle);
    const double sinAxis = std::sin(angle);
    const double localX = dx * cosAxis + dy * sinAxis;
    const double localY = -dx * sinAxis + dy * cosAxis;
    const double value = (localX * localX) / (majorRadius * majorRadius)
        + (localY * localY) / (minorRadius * minorRadius);
    return std::abs(value - 1.0);
}

bool pointOnObjectConstraintSatisfied(
    const PointOnObjectConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case PointOnObject routes to
    // "addPointOnObjectConstraint(constraint->First, constraint->FirstPos, constraint->Second)".
    // That overload accepts Line, Arc, Circle, Ellipse and ArcOfEllipse targets. cad-core only
    // verifies already-satisfied point-on-object constraints; it does not run the solver or move
    // the constrained point.
    const auto point
        = constraintPointValue(constraint.point, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!point) {
        return false;
    }

    if (const auto segmentIndex = segmentIndexForGeometry(segments, constraint.objectGeometryIndex)) {
        return pointToLineDistance2d(*point, segments.at(*segmentIndex)) <= 1e-7;
    }
    if (const auto circleIndex = circleIndexForGeometry(circles, constraint.objectGeometryIndex)) {
        const SketchCircle& circle = circles.at(*circleIndex);
        return std::abs(point->Distance(circle.center) - circle.radius) <= 1e-7;
    }
    if (const auto arcIndex = arcIndexForGeometry(arcs, constraint.objectGeometryIndex)) {
        const SketchArc& arc = arcs.at(*arcIndex);
        return std::abs(point->Distance(arc.center) - arc.radius) <= 1e-7;
    }
    if (const auto ellipseIndex = ellipseIndexForGeometry(ellipses, constraint.objectGeometryIndex)) {
        const SketchEllipse& ellipse = ellipses.at(*ellipseIndex);
        return ellipseEquationError(
                   *point,
                   ellipse.center,
                   ellipse.majorRadius,
                   ellipse.minorRadius,
                   ellipse.angle
               )
            <= 1e-7;
    }
    if (const auto ellipseArcIndex
        = ellipseArcIndexForGeometry(ellipseArcs, constraint.objectGeometryIndex)) {
        const SketchEllipseArc& arc = ellipseArcs.at(*ellipseArcIndex);
        return ellipseEquationError(*point, arc.center, arc.majorRadius, arc.minorRadius, arc.angle)
            <= 1e-7;
    }
    return false;
}

bool symmetricAroundLineSatisfied(const gp_Pnt& first, const gp_Pnt& second, const SketchSegment& axis)
{
    const double dx = axis.end.X() - axis.start.X();
    const double dy = axis.end.Y() - axis.start.Y();
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= Precision::Confusion()) {
        return false;
    }

    const gp_Pnt midpoint((first.X() + second.X()) * 0.5, (first.Y() + second.Y()) * 0.5, 0.0);
    const double vx = second.X() - first.X();
    const double vy = second.Y() - first.Y();
    return pointToLineDistance2d(midpoint, axis) <= 1e-7
        && std::abs(vx * dx + vy * dy) / length <= 1e-7;
}

bool perpendicularPointLineConstraintSatisfied(
    const PerpendicularPointLineConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Perpendicular routes "point point line perpendicularity" to
    // addPerpendicularConstraint(geoId1, pos1, geoId2, pos2, geoId3). That overload calls
    // "GCSsys.addConstraintPerpendicular(p1, p2, l, tag)" and requires geoId3 to be a Line.
    const auto first
        = constraintPointValue(constraint.first, segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second
        = constraintPointValue(constraint.second, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!first || !second) {
        return false;
    }

    const SketchSegment& axis = segments.at(constraint.axisSegmentIndex);
    const double axisDx = axis.end.X() - axis.start.X();
    const double axisDy = axis.end.Y() - axis.start.Y();
    const double pointDx = second->X() - first->X();
    const double pointDy = second->Y() - first->Y();
    const double axisLength = std::sqrt(axisDx * axisDx + axisDy * axisDy);
    const double pointLength = std::sqrt(pointDx * pointDx + pointDy * pointDy);
    if (axisLength <= Precision::Confusion() || pointLength <= Precision::Confusion()) {
        return false;
    }
    return std::abs(axisDx * pointDx + axisDy * pointDy) / (axisLength * pointLength) <= 1e-7;
}

bool perpendicularMidpointLineConstraintSatisfied(
    const PerpendicularMidpointLineConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addPerpendicularConstraint(int geoId1, int geoId2), for Line + Circle/Arc,
    // takes "Points[Geoms[geoId2].midPointId]" and adds "GCSsys.addConstraintPointOnLine".
    const SketchSegment& line = segments.at(constraint.lineSegmentIndex);
    const gp_Pnt center = constraint.targetKind == PerpendicularMidpointTargetKind::Circle
        ? circles.at(constraint.targetIndex).center
        : arcs.at(constraint.targetIndex).center;
    return pointToLineDistance2d(center, line) <= 1e-7;
}

bool symmetricConstraintSatisfied(
    const SymmetricConstraintRef& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Symmetric dispatches either to
    // "addSymmetricConstraint(..., constraint->Third)" for point-point symmetry about a line, or
    // "addSymmetricConstraint(..., constraint->Third, constraint->ThirdPos)" for symmetry about a
    // point. cad-core only verifies already-satisfied point symmetry; it does not run the solver
    // or move geometry.
    const auto first
        = constraintPointValue(constraint.first, segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second
        = constraintPointValue(constraint.second, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!first || !second) {
        return false;
    }

    if (constraint.axisSegmentIndex) {
        return symmetricAroundLineSatisfied(*first, *second, segments.at(*constraint.axisSegmentIndex));
    }
    if (constraint.centerPoint) {
        const auto center = constraintPointValue(
            *constraint.centerPoint,
            segments,
            points,
            circles,
            ellipses,
            arcs,
            ellipseArcs
        );
        if (!center) {
            return false;
        }
        const gp_Pnt midpoint((first->X() + second->X()) * 0.5, (first->Y() + second->Y()) * 0.5, 0.0);
        return midpoint.Distance(*center) <= 1e-7;
    }
    return false;
}


} // namespace

std::optional<std::string> readStringField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return std::nullopt;
    }
    if (it->is_string()) {
        return it->get<std::string>();
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_string()) {
        return it->at("value").get<std::string>();
    }
    return std::nullopt;
}

std::optional<int> readIntField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return std::nullopt;
    }
    if (it->is_number_integer()) {
        return it->get<int>();
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_number_integer()) {
        return it->at("value").get<int>();
    }
    return std::nullopt;
}

SketchConstraintKind readSketchConstraintKind(const nlohmann::json& constraint)
{
    if (const auto type = readStringField(constraint, "Type")) {
        if (*type == "Coincident") {
            return SketchConstraintKind::Coincident;
        }
        if (*type == "Horizontal") {
            return SketchConstraintKind::Horizontal;
        }
        if (*type == "Vertical") {
            return SketchConstraintKind::Vertical;
        }
        if (*type == "Parallel") {
            return SketchConstraintKind::Parallel;
        }
        if (*type == "Tangent") {
            return SketchConstraintKind::Tangent;
        }
        if (*type == "Perpendicular") {
            return SketchConstraintKind::Perpendicular;
        }
        if (*type == "PointOnObject") {
            return SketchConstraintKind::PointOnObject;
        }
        if (*type == "Symmetric") {
            return SketchConstraintKind::Symmetric;
        }
        if (*type == "Block") {
            return SketchConstraintKind::Block;
        }
        if (*type == "Distance") {
            return SketchConstraintKind::Distance;
        }
        if (*type == "DistanceX") {
            return SketchConstraintKind::DistanceX;
        }
        if (*type == "DistanceY") {
            return SketchConstraintKind::DistanceY;
        }
        if (*type == "Angle") {
            return SketchConstraintKind::Angle;
        }
        if (*type == "Radius") {
            return SketchConstraintKind::Radius;
        }
        if (*type == "Diameter") {
            return SketchConstraintKind::Diameter;
        }
        if (*type == "Equal") {
            return SketchConstraintKind::Equal;
        }
        return SketchConstraintKind::Unsupported;
    }
    if (const auto type = readIntField(constraint, "Type")) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Constraint.h,
        // enum ConstraintType keeps "Coincident = 1", "Horizontal = 2", "Vertical = 3",
        // "Parallel = 4", "Tangent = 5", "Distance = 6", "DistanceX = 7", "DistanceY = 8",
        // "Angle = 9", "Perpendicular = 10", "Radius = 11", "Equal = 12",
        // "PointOnObject = 13", "Symmetric = 14", "Block = 17",
        // "Diameter = 18".
        if (*type == 1) {
            return SketchConstraintKind::Coincident;
        }
        if (*type == 2) {
            return SketchConstraintKind::Horizontal;
        }
        if (*type == 3) {
            return SketchConstraintKind::Vertical;
        }
        if (*type == 4) {
            return SketchConstraintKind::Parallel;
        }
        if (*type == 5) {
            return SketchConstraintKind::Tangent;
        }
        if (*type == 6) {
            return SketchConstraintKind::Distance;
        }
        if (*type == 7) {
            return SketchConstraintKind::DistanceX;
        }
        if (*type == 8) {
            return SketchConstraintKind::DistanceY;
        }
        if (*type == 9) {
            return SketchConstraintKind::Angle;
        }
        if (*type == 10) {
            return SketchConstraintKind::Perpendicular;
        }
        if (*type == 11) {
            return SketchConstraintKind::Radius;
        }
        if (*type == 12) {
            return SketchConstraintKind::Equal;
        }
        if (*type == 13) {
            return SketchConstraintKind::PointOnObject;
        }
        if (*type == 14) {
            return SketchConstraintKind::Symmetric;
        }
        if (*type == 17) {
            return SketchConstraintKind::Block;
        }
        if (*type == 18) {
            return SketchConstraintKind::Diameter;
        }
    }
    return SketchConstraintKind::Unsupported;
}

std::optional<double> readConstraintValue(const nlohmann::json& constraint)
{
    for (const std::string& field : {"Value", "Datum", "value"}) {
        if (const auto value = readNumberField(constraint, field)) {
            return *value;
        }
    }
    return std::nullopt;
}

void addUniqueConstraintIndex(std::vector<int>& indexes, int index)
{
    if (std::find(indexes.begin(), indexes.end(), index) == indexes.end()) {
        indexes.push_back(index);
    }
}

std::string constraintIndexTarget(const std::vector<int>& indexes)
{
    std::string target = "Constraints[";
    for (std::size_t index = 0; index < indexes.size(); ++index) {
        if (index != 0) {
            target += ",";
        }
        target += std::to_string(indexes[index]);
    }
    target += "]";
    return target;
}

bool isDimensionSolverConstraint(SketchConstraintKind kind)
{
    return kind == SketchConstraintKind::Distance || kind == SketchConstraintKind::DistanceX
        || kind == SketchConstraintKind::DistanceY || kind == SketchConstraintKind::Radius
        || kind == SketchConstraintKind::Diameter;
}

SketchSolverSummary analyzeSketchSolverDiagnostics(const nlohmann::json& constraints)
{
    SketchSolverSummary summary;
    if (!constraints.is_array()) {
        return summary;
    }

    std::map<std::string, int> exactConstraints;
    std::map<std::string, int> horizontalByTarget;
    std::map<std::string, int> verticalByTarget;
    std::map<std::string, std::pair<int, double>> datumByTarget;
    std::map<std::string, std::pair<int, double>> angleByTarget;

    for (std::size_t offset = 0; offset < constraints.size(); ++offset) {
        const auto& constraint = constraints[offset];
        if (!constraint.is_object()) {
            continue;
        }
        const int constraintIndex = static_cast<int>(offset + 1U);
        const SketchConstraintKind kind = readSketchConstraintKind(constraint);

        if (kind == SketchConstraintKind::Horizontal || kind == SketchConstraintKind::Vertical) {
            const auto target = orientationSolverTargetKey(constraint);
            if (!target) {
                continue;
            }
            const std::string exactKey
                = (kind == SketchConstraintKind::Horizontal ? "H:" : "V:") + *target;
            if (const auto exact = exactConstraints.find(exactKey); exact != exactConstraints.end()) {
                addUniqueConstraintIndex(summary.redundantConstraints, exact->second);
                addUniqueConstraintIndex(summary.redundantConstraints, constraintIndex);
            }
            else {
                exactConstraints[exactKey] = constraintIndex;
            }

            std::map<std::string, int>& sameKindMap
                = kind == SketchConstraintKind::Horizontal ? horizontalByTarget : verticalByTarget;
            std::map<std::string, int>& oppositeKindMap
                = kind == SketchConstraintKind::Horizontal ? verticalByTarget : horizontalByTarget;
            if (const auto opposite = oppositeKindMap.find(*target); opposite != oppositeKindMap.end()) {
                addUniqueConstraintIndex(summary.conflictingConstraints, opposite->second);
                addUniqueConstraintIndex(summary.conflictingConstraints, constraintIndex);
            }
            sameKindMap.emplace(*target, constraintIndex);
            continue;
        }

        if (kind == SketchConstraintKind::Distance || kind == SketchConstraintKind::DistanceX
            || kind == SketchConstraintKind::DistanceY || kind == SketchConstraintKind::Radius
            || kind == SketchConstraintKind::Diameter) {
            const auto target = dimensionSolverTargetKey(constraint, kind);
            const auto value = readConstraintValue(constraint);
            if (!target || !value) {
                continue;
            }
            const std::string familyKey
                = "datum:" + std::to_string(static_cast<int>(kind)) + ":" + *target;
            if (const auto previous = datumByTarget.find(familyKey); previous != datumByTarget.end()) {
                if (!sameConstraintValue(previous->second.second, *value)) {
                    addUniqueConstraintIndex(summary.conflictingConstraints, previous->second.first);
                    addUniqueConstraintIndex(summary.conflictingConstraints, constraintIndex);
                }
            }
            else {
                datumByTarget[familyKey] = {constraintIndex, *value};
            }
            const std::string exactKey = familyKey + ":" + normalizedConstraintValue(*value);
            if (const auto exact = exactConstraints.find(exactKey); exact != exactConstraints.end()) {
                addUniqueConstraintIndex(summary.redundantConstraints, exact->second);
                addUniqueConstraintIndex(summary.redundantConstraints, constraintIndex);
            }
            else {
                exactConstraints[exactKey] = constraintIndex;
            }
            continue;
        }

        if (kind == SketchConstraintKind::Angle) {
            const auto target = angleSolverTargetKey(constraint);
            const auto value = readConstraintValue(constraint);
            if (!target || !value) {
                continue;
            }
            const std::string familyKey = "angle:" + *target;
            if (const auto previous = angleByTarget.find(familyKey); previous != angleByTarget.end()) {
                if (!sameConstraintValue(previous->second.second, *value)) {
                    addUniqueConstraintIndex(summary.conflictingConstraints, previous->second.first);
                    addUniqueConstraintIndex(summary.conflictingConstraints, constraintIndex);
                }
            }
            else {
                angleByTarget[familyKey] = {constraintIndex, *value};
            }
            const std::string exactKey = familyKey + ":" + normalizedConstraintValue(*value);
            if (const auto exact = exactConstraints.find(exactKey); exact != exactConstraints.end()) {
                addUniqueConstraintIndex(summary.redundantConstraints, exact->second);
                addUniqueConstraintIndex(summary.redundantConstraints, constraintIndex);
            }
            else {
                exactConstraints[exactKey] = constraintIndex;
            }
        }
    }

    if (!summary.conflictingConstraints.empty()) {
        summary.state = SketchSolverState::Conflict;
    }
    else if (!summary.redundantConstraints.empty()) {
        summary.state = SketchSolverState::Redundant;
    }
    return summary;
}

std::optional<AppliedSketchConstraints> applySketchConstraints(
    const nlohmann::json& constraints,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs,
    const std::vector<SketchBSpline>& bsplines
)
{
    if (constraints.is_null()) {
        return AppliedSketchConstraints {};
    }
    if (!constraints.is_array()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Sketch Constraints must be a list",
            object.name,
            "Constraints"
        );
        return std::nullopt;
    }

    UnionFind endpoints(segments.size() * 2U);
    AppliedSketchConstraints applied;
    std::vector<OrientationConstraintRef> orientationConstraints;
    std::vector<DimensionConstraintRef> dimensionConstraints;
    std::vector<LinePairConstraintRef> linePairConstraints;
    std::vector<TangentConstraintRef> tangentConstraints;
    std::vector<TangentConstraintRef> perpendicularPointConstraints;
    std::vector<PerpendicularPointLineConstraintRef> perpendicularPointLineConstraints;
    std::vector<PerpendicularMidpointLineConstraintRef> perpendicularMidpointLineConstraints;
    std::vector<EqualConstraintRef> equalConstraints;
    std::vector<AngleConstraintRef> angleConstraints;
    std::vector<PointwiseAngleConstraintRef> pointwiseAngleConstraints;
    std::vector<PointOnObjectConstraintRef> pointOnObjectConstraints;
    std::vector<SymmetricConstraintRef> symmetricConstraints;
    std::vector<BlockConstraintRef> blockConstraints;
    applied.solver = analyzeMalformedSketchConstraints(constraints, segments, circles, arcs);
    if (applied.solver.state == SketchSolverState::Malformed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "sketch_solver_malformed_constraint",
            "Sketch has malformed constraints",
            object.name,
            "Constraints",
            "solver",
            constraintIndexTarget(applied.solver.malformedConstraints)
        );
        return applied;
    }

    for (const auto& constraint : constraints) {
        if (!constraint.is_object()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Sketch Constraints must be objects",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }

        const SketchConstraintKind kind = readSketchConstraintKind(constraint);
        if (kind == SketchConstraintKind::Coincident) {
            const auto first = readEndpointRef(constraint, "First", "FirstPos", segments);
            const auto second = readEndpointRef(constraint, "Second", "SecondPos", segments);
            if (!first || !second) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Coincident constraint must reference two line endpoints",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }

            endpoints.unite(endpointId(*first), endpointId(*second));
            ++applied.coincident;
            continue;
        }

        if (kind == SketchConstraintKind::Horizontal || kind == SketchConstraintKind::Vertical) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.h,
            // addHorizontalConstraint(int geoId) / addVerticalConstraint(int geoId) and their
            // two-point overloads constrain a whole line or two line ends. cad-core only accepts
            // this solver-facing subset when it is already satisfied; it does not move geometry.
            const auto orientation = readOrientationConstraintRef(constraint, kind, segments);
            if (!orientation) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Horizontal/Vertical constraints currently support whole line references",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            orientationConstraints.push_back(*orientation);
            continue;
        }

        if (kind == SketchConstraintKind::Parallel) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.h,
            // addParallelConstraint(int geoId1, int geoId2) constrains whole line geometries.
            // cad-core verifies only the already-satisfied whole-line subset and does not run the
            // sketch solver.
            const auto relation = readLinePairConstraintRef(constraint, kind, segments);
            if (!relation) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Parallel constraints currently support whole line pairs only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            linePairConstraints.push_back(*relation);
            continue;
        }

        if (kind == SketchConstraintKind::Perpendicular) {
            if (constraint.contains("Third") && !readPointPositionIsNone(constraint, "FirstPos", true)
                && !readPointPositionIsNone(constraint, "SecondPos", true)) {
                const auto perpendicularPointLine = readPerpendicularPointLineConstraintRef(
                    constraint,
                    segments,
                    points,
                    circles,
                    ellipses,
                    arcs,
                    ellipseArcs
                );
                if (!perpendicularPointLine) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_property",
                        "Perpendicular point-point-line constraints require two point references "
                        "and a line reference",
                        object.name,
                        "Constraints"
                    );
                    return std::nullopt;
                }
                perpendicularPointLineConstraints.push_back(*perpendicularPointLine);
                continue;
            }

            if (constraint.contains("FirstPos") || constraint.contains("SecondPos")
                || constraint.contains("Third") || constraint.contains("ThirdPos")) {
                const auto perpendicular = readTangentConstraintRef(
                    constraint,
                    segments,
                    points,
                    circles,
                    ellipses,
                    arcs,
                    ellipseArcs
                );
                if (!perpendicular) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_property",
                        "Perpendicular constraints currently support whole line pairs and "
                        "point-wise curve references only",
                        object.name,
                        "Constraints"
                    );
                    return std::nullopt;
                }
                perpendicularPointConstraints.push_back(*perpendicular);
                continue;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.h,
            // addPerpendicularConstraint(int geoId1, int geoId2) constrains whole line
            // geometries and line + circle/arc midpoint pairs; point-wise forms are handled
            // separately through addAngleAtPointConstraint().
            const auto relation = readLinePairConstraintRef(constraint, kind, segments);
            if (relation) {
                linePairConstraints.push_back(*relation);
                continue;
            }

            const auto midpoint
                = readPerpendicularMidpointLineConstraintRef(constraint, segments, circles, arcs);
            if (!midpoint) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Perpendicular constraints currently support whole line pairs, line-circle/arc "
                    "midpoint pairs, and point-wise curve references only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            perpendicularMidpointLineConstraints.push_back(*midpoint);
            continue;
        }

        if (kind == SketchConstraintKind::Tangent) {
            const auto tangent = readTangentConstraintRef(
                constraint,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs
            );
            if (!tangent) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Tangent constraints currently support direct whole-geometry and point-wise "
                    "tangency only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            tangentConstraints.push_back(*tangent);
            continue;
        }

        if (kind == SketchConstraintKind::PointOnObject) {
            const auto pointOnObject = readPointOnObjectConstraintRef(
                constraint,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs
            );
            if (!pointOnObject) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "PointOnObject constraints currently support line/arc/point references on "
                    "line, circle, arc, ellipse and ellipse-arc targets only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            pointOnObjectConstraints.push_back(*pointOnObject);
            continue;
        }

        if (kind == SketchConstraintKind::Symmetric) {
            const auto symmetric = readSymmetricConstraintRef(
                constraint,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs
            );
            if (!symmetric) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Symmetric constraints currently support two point references about a line or "
                    "point reference only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            symmetricConstraints.push_back(*symmetric);
            continue;
        }

        if (kind == SketchConstraintKind::Block) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp
            // ::SketchObject::getBlockedState(), for "cstr->Type == Block" sets
            // "blockedstate = true";
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
            // ::Sketch::addConstraint() handles "Sketcher::Block" separately while adding
            // geometry. cad-core has no solver parameters to freeze, so it accepts valid
            // whole-geometry Block declarations without changing the current shape.
            const auto block = readBlockConstraintRef(
                constraint,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs,
                bsplines
            );
            if (!block) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Block constraints currently support whole sketch geometry references only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            blockConstraints.push_back(*block);
            continue;
        }

        if (kind == SketchConstraintKind::Equal) {
            const auto equal = readEqualConstraintRef(constraint);
            if (!equal) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Equal constraints currently support whole line, circle and arc geometry pairs "
                    "only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            equalConstraints.push_back(*equal);
            continue;
        }

        if (kind == SketchConstraintKind::Angle) {
            if (constraint.contains("FirstPos") || constraint.contains("SecondPos")
                || constraint.contains("Third") || constraint.contains("ThirdPos")) {
                const auto angle = readPointwiseAngleConstraintRef(
                    constraint,
                    segments,
                    points,
                    circles,
                    ellipses,
                    arcs,
                    ellipseArcs
                );
                if (!angle) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_property",
                        "Angle constraints currently require a Value/Datum and support whole line "
                        "pairs or point-wise curve references only",
                        object.name,
                        "Constraints"
                    );
                    return std::nullopt;
                }
                pointwiseAngleConstraints.push_back(*angle);
                continue;
            }

            const auto angle = readAngleConstraintRef(constraint, segments);
            if (!angle) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Angle constraints currently require a Value/Datum and support whole line "
                    "pairs or point-wise curve references only",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            angleConstraints.push_back(*angle);
            continue;
        }

        if (isDimensionSolverConstraint(kind)) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.h,
            // addDistance*Constraint(..., double* value) and addRadiusConstraint(..., double*
            // value) / addDiameterConstraint(..., double* value) attach a datum to the solver
            // constraint. /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
            // ::Sketch::addDiameterConstraint() calls "addConstraintCircleDiameter" /
            // "addConstraintArcDiameter". This P5 subset verifies datums that are already
            // satisfied after Coincident endpoint merging; it does not run the solver or move
            // geometry to satisfy a datum.
            const auto dimension = readDimensionConstraintRef(constraint, kind, segments);
            if (!dimension) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Distance/Radius/Diameter constraints currently require a Value/Datum and a "
                    "whole geometry, line-end pair, or fixed line-end coordinate reference",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            dimensionConstraints.push_back(*dimension);
            continue;
        }

        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Only Sketcher Coincident and already-satisfied "
            "Horizontal/Vertical/Parallel/Tangent/Perpendicular/PointOnObject/Symmetric/Block/"
            "Angle/Distance/Radius/Diameter/Equal constraints are applied in the current P5 subset",
            object.name,
            "Constraints"
        );
        return std::nullopt;
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp
    // ::SketchObject::solve(), after setUpSketch(), says "At this point we have the solver
    // information about conflicting/redundant/over-constrained"; redundancy is recorded first,
    // then "lastDoF < 0" and "lastHasConflict" can override the solve error.
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::execute() returns "Sketch with conflicting constraints" or "Sketch with
    // redundant constraints" before building normal recompute output. cad-core mirrors that
    // diagnostic gate for the migrated, non-solving constraint subset instead of producing a
    // profile after an impossible solver state.
    applied.solver = analyzeSketchSolverDiagnostics(constraints);
    if (applied.solver.state == SketchSolverState::Conflict) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "sketch_solver_conflict",
            "Sketch has conflicting constraints",
            object.name,
            "Constraints",
            "solver",
            constraintIndexTarget(applied.solver.conflictingConstraints)
        );
        return applied;
    }
    if (applied.solver.state == SketchSolverState::Redundant) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "sketch_solver_redundant",
            "Sketch has redundant constraints",
            object.name,
            "Constraints",
            "solver",
            constraintIndexTarget(applied.solver.redundantConstraints)
        );
        return applied;
    }

    applied.block = blockConstraints.size();

    if (endpoints.parent.empty() && !orientationConstraints.empty()) {
        for (const auto& orientation : orientationConstraints) {
            if (!orientationConstraintSatisfied(orientation, segments)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Horizontal/Vertical constraint requires solver movement in the current P5 "
                    "subset",
                    object.name,
                    "Constraints"
                );
                return std::nullopt;
            }
            ++applied.orientation;
        }
        return applied;
    }

    std::vector<std::optional<gp_Pnt>> mergedPoints(segments.size() * 2U);
    for (std::size_t index = 0; index < segments.size(); ++index) {
        for (bool start : {true, false}) {
            const EndpointRef endpoint {index, start};
            const std::size_t root = endpoints.find(endpointId(endpoint));
            if (!mergedPoints[root]) {
                mergedPoints[root] = endpointPoint(segments[index], start);
            }
        }
    }
    for (std::size_t index = 0; index < segments.size(); ++index) {
        for (bool start : {true, false}) {
            const EndpointRef endpoint {index, start};
            const std::size_t root = endpoints.find(endpointId(endpoint));
            if (mergedPoints[root]) {
                endpointPoint(segments[index], start) = *mergedPoints[root];
            }
        }
    }

    for (const auto& orientation : orientationConstraints) {
        if (!orientationConstraintSatisfied(orientation, segments)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Horizontal/Vertical constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.orientation;
    }

    for (const auto& dimension : dimensionConstraints) {
        if (!dimensionConstraintSatisfied(dimension, segments, circles, arcs)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Distance/Radius/Diameter constraint requires solver movement in the current P5 "
                "subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.dimension;
    }

    for (const auto& relation : linePairConstraints) {
        if (!linePairConstraintSatisfied(relation, segments)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Parallel/Perpendicular constraint requires solver movement in the current P5 "
                "subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& tangent : tangentConstraints) {
        if (
            !tangentConstraintSatisfied(tangent, segments, points, circles, ellipses, arcs, ellipseArcs)
        ) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Tangent constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& perpendicular : perpendicularPointConstraints) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
        // ::Sketch::addAngleAtPointConstraint() handles Perpendicular together with Tangent:
        // "if (!(cTyp == Angle || cTyp == Tangent || cTyp == Perpendicular)) return -1".
        // cad-core verifies the already-satisfied point-wise curve-angle subset without moving
        // geometry or invoking the solver.
        if (!pointwiseCurveAngleConstraintSatisfied(
                perpendicular,
                false,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs
            )) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Perpendicular constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& perpendicular : perpendicularPointLineConstraints) {
        if (!perpendicularPointLineConstraintSatisfied(
                perpendicular,
                segments,
                points,
                circles,
                ellipses,
                arcs,
                ellipseArcs
            )) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Perpendicular point-point-line constraint requires solver movement in the current "
                "P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& perpendicular : perpendicularMidpointLineConstraints) {
        if (!perpendicularMidpointLineConstraintSatisfied(perpendicular, segments, circles, arcs)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Perpendicular line-circle/arc midpoint constraint requires solver movement in the "
                "current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& pointOnObject : pointOnObjectConstraints) {
        if (
            !pointOnObjectConstraintSatisfied(pointOnObject, segments, points, circles, ellipses, arcs, ellipseArcs)
        ) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "PointOnObject constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& symmetric : symmetricConstraints) {
        if (
            !symmetricConstraintSatisfied(symmetric, segments, points, circles, ellipses, arcs, ellipseArcs)
        ) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Symmetric constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& angle : angleConstraints) {
        if (!angleConstraintSatisfied(angle, segments)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Angle constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& angle : pointwiseAngleConstraints) {
        if (
            !pointwiseAngleConstraintSatisfied(angle, segments, points, circles, ellipses, arcs, ellipseArcs)
        ) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Angle constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& equal : equalConstraints) {
        if (!equalConstraintSatisfied(equal, segments, circles, arcs)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Equal constraint requires solver movement in the current P5 subset",
                object.name,
                "Constraints"
            );
            return std::nullopt;
        }
        ++applied.relation;
    }

    return applied;
}


std::string solverStateName(SketchSolverState state)
{
    if (state == SketchSolverState::Malformed) {
        return "malformed";
    }
    if (state == SketchSolverState::Conflict) {
        return "conflict";
    }
    if (state == SketchSolverState::Redundant) {
        return "redundant";
    }
    return "accepted";
}

nlohmann::json constraintIndexArray(const std::vector<int>& indexes)
{
    nlohmann::json result = nlohmann::json::array();
    for (const int index : indexes) {
        result.push_back(index);
    }
    return result;
}

bool solverStateBlocksProfile(SketchSolverState state)
{
    return state == SketchSolverState::Malformed || state == SketchSolverState::Conflict
        || state == SketchSolverState::Redundant;
}

nlohmann::json sketchSolverFailureObject(const AppliedSketchConstraints& applied)
{
    return {
        {"status", "error"},
        {"shape", "empty"},
        {"profile", "none"},
        {"profile_ready", false},
        {"solver_state", solverStateName(applied.solver.state)},
        {"solver_malformed_constraints", constraintIndexArray(applied.solver.malformedConstraints)},
        {"solver_conflicting_constraints", constraintIndexArray(applied.solver.conflictingConstraints)},
        {"solver_redundant_constraints", constraintIndexArray(applied.solver.redundantConstraints)},
        {"coincident_constraints_applied", applied.coincident},
        {"orientation_constraints_applied", applied.orientation},
        {"dimension_constraints_applied", applied.dimension},
        {"relation_constraints_applied", applied.relation},
        {"block_constraints_applied", applied.block},
    };
}

} // namespace cad_core::sketcher
