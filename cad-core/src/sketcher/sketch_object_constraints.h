#pragma once

#include "sketch_object_geometry.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::sketcher
{

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp
// ::SketchObject::retrieveSolverDiagnostics(), fields "lastConflicting",
// "lastRedundant", and "lastMalformedConstraints" carry solver-facing constraint state.
enum class SketchConstraintKind
{
    Coincident,
    Horizontal,
    Vertical,
    Parallel,
    Tangent,
    Perpendicular,
    PointOnObject,
    Symmetric,
    Block,
    Angle,
    Distance,
    DistanceX,
    DistanceY,
    Radius,
    Diameter,
    Equal,
    Unsupported,
};

struct EndpointRef
{
    std::size_t segmentIndex = 0;
    bool start = true;
};

struct OrientationConstraintRef
{
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    std::optional<std::size_t> segmentIndex;
    std::optional<EndpointRef> firstEndpoint;
    std::optional<EndpointRef> secondEndpoint;
};

struct DimensionConstraintRef
{
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    int geometryIndex = -1;
    double value = 0.0;
    std::optional<EndpointRef> firstEndpoint;
    std::optional<EndpointRef> secondEndpoint;
    std::optional<EndpointRef> coordinateEndpoint;
};

struct LinePairConstraintRef
{
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    std::size_t firstSegmentIndex = 0;
    std::size_t secondSegmentIndex = 0;
};

enum class TangentGeometryKind
{
    Line,
    Circle,
    Arc,
    Ellipse,
    EllipseArc,
};

struct TangentGeometryRef
{
    TangentGeometryKind kind = TangentGeometryKind::Line;
    std::size_t index = 0;
};

struct EqualConstraintRef
{
    int firstGeometryIndex = -1;
    int secondGeometryIndex = -1;
};

struct AngleConstraintRef
{
    LinePairConstraintRef linePair;
    double value = 0.0;
};

enum class ConstraintPointPosition
{
    Start,
    End,
    Mid,
};

enum class ConstraintPointKind
{
    SegmentEndpoint,
    PointGeometry,
    CircleCenter,
    EllipseCenter,
    ArcEndpoint,
    EllipseArcEndpoint,
};

struct ConstraintPointRef
{
    ConstraintPointKind kind = ConstraintPointKind::SegmentEndpoint;
    std::size_t index = 0;
    bool start = true;
};

struct TangentConstraintRef
{
    TangentGeometryRef first;
    TangentGeometryRef second;
    std::optional<ConstraintPointRef> firstPoint;
    std::optional<ConstraintPointRef> secondPoint;
    std::optional<ConstraintPointRef> viaPoint;
};

struct PointwiseAngleConstraintRef
{
    TangentConstraintRef geometry;
    double value = 0.0;
};

struct PerpendicularPointLineConstraintRef
{
    ConstraintPointRef first;
    ConstraintPointRef second;
    std::size_t axisSegmentIndex = 0;
};

enum class PerpendicularMidpointTargetKind
{
    Circle,
    Arc,
};

struct PerpendicularMidpointLineConstraintRef
{
    std::size_t lineSegmentIndex = 0;
    PerpendicularMidpointTargetKind targetKind = PerpendicularMidpointTargetKind::Circle;
    std::size_t targetIndex = 0;
};

struct PointOnObjectConstraintRef
{
    ConstraintPointRef point;
    int objectGeometryIndex = -1;
};

struct SymmetricConstraintRef
{
    ConstraintPointRef first;
    ConstraintPointRef second;
    std::optional<std::size_t> axisSegmentIndex;
    std::optional<ConstraintPointRef> centerPoint;
};

struct BlockConstraintRef
{
    int geometryIndex = -1;
};

enum class SketchSolverState
{
    Accepted,
    Malformed,
    Redundant,
    Conflict,
};

struct SketchSolverSummary
{
    SketchSolverState state = SketchSolverState::Accepted;
    std::vector<int> malformedConstraints;
    std::vector<int> conflictingConstraints;
    std::vector<int> redundantConstraints;
};

struct AppliedSketchConstraints
{
    std::size_t coincident = 0;
    std::size_t orientation = 0;
    std::size_t dimension = 0;
    std::size_t relation = 0;
    std::size_t block = 0;
    SketchSolverSummary solver;
};

std::optional<std::string> readStringField(const nlohmann::json& value, const std::string& field);
std::optional<int> readIntField(const nlohmann::json& value, const std::string& field);
SketchConstraintKind readSketchConstraintKind(const nlohmann::json& constraint);
std::optional<double> readConstraintValue(const nlohmann::json& constraint);
void addUniqueConstraintIndex(std::vector<int>& indexes, int index);
std::string constraintIndexTarget(const std::vector<int>& indexes);
bool isDimensionSolverConstraint(SketchConstraintKind kind);
SketchSolverSummary analyzeSketchSolverDiagnostics(const nlohmann::json& constraints);
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
);
std::string solverStateName(SketchSolverState state);
nlohmann::json constraintIndexArray(const std::vector<int>& indexes);
bool solverStateBlocksProfile(SketchSolverState state);
nlohmann::json sketchSolverFailureObject(const AppliedSketchConstraints& applied);

} // namespace cad_core::sketcher
