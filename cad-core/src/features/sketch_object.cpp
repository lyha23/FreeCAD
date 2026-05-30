#include "cad_core/features/sketch_object.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/face_maker.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/topo/element_map.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepAlgoAPI_Section.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::features {

namespace {

struct SketchSegment {
    std::size_t geometryIndex = 0;
    gp_Pnt start;
    gp_Pnt end;
    bool construction = false;
};

struct SketchPoint {
    std::size_t geometryIndex = 0;
    gp_Pnt point;
    bool construction = false;
};

struct SketchCircle {
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double radius = 0.0;
    bool construction = false;
};

struct SketchEllipse {
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    bool construction = false;
};

struct SketchArc {
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

struct SketchBSpline {
    std::size_t geometryIndex = 0;
    int degree = 0;
    std::vector<gp_Pnt> poles;
    bool construction = false;
};

std::string stableSubnameDiagnosticCode(topo::ElementResolveStatus status)
{
    switch (status) {
        case topo::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case topo::ElementResolveStatus::Split:
            return "split_stable_subname";
        case topo::ElementResolveStatus::Resolved:
        case topo::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& target,
                                           const std::string& stableSubname,
                                           topo::ElementResolveStatus status)
{
    if (status == topo::ElementResolveStatus::Deleted) {
        return "ExternalGeometry target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == topo::ElementResolveStatus::Split) {
        return "ExternalGeometry target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return "ExternalGeometry target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

struct SketchEllipseArc {
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

struct SketchGeometrySet {
    std::vector<SketchSegment> segments;
    std::vector<SketchPoint> points;
    std::vector<SketchCircle> circles;
    std::vector<SketchEllipse> ellipses;
    std::vector<SketchArc> arcs;
    std::vector<SketchEllipseArc> ellipseArcs;
    std::vector<SketchBSpline> bsplines;
};

struct ExternalGeometryResult {
    std::vector<SketchSegment> segments;
    std::vector<gp_Pnt> points;
    std::vector<SketchCircle> circles;
    std::vector<SketchArc> arcs;
    std::vector<SketchEllipse> ellipses;
    std::vector<SketchEllipseArc> ellipseArcs;
};

enum class ExternalGeometryType {
    Projection = 0,
    Intersection = 1,
    Both = 2,
};

enum class SketchConstraintKind {
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

struct EndpointRef {
    std::size_t segmentIndex = 0;
    bool start = true;
};

struct OrientationConstraintRef {
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    std::optional<std::size_t> segmentIndex;
    std::optional<EndpointRef> firstEndpoint;
    std::optional<EndpointRef> secondEndpoint;
};

struct DimensionConstraintRef {
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    int geometryIndex = -1;
    double value = 0.0;
    std::optional<EndpointRef> firstEndpoint;
    std::optional<EndpointRef> secondEndpoint;
    std::optional<EndpointRef> coordinateEndpoint;
};

struct LinePairConstraintRef {
    SketchConstraintKind kind = SketchConstraintKind::Unsupported;
    std::size_t firstSegmentIndex = 0;
    std::size_t secondSegmentIndex = 0;
};

enum class TangentGeometryKind {
    Line,
    Circle,
    Arc,
    Ellipse,
    EllipseArc,
};

struct TangentGeometryRef {
    TangentGeometryKind kind = TangentGeometryKind::Line;
    std::size_t index = 0;
};

struct EqualConstraintRef {
    int firstGeometryIndex = -1;
    int secondGeometryIndex = -1;
};

struct AngleConstraintRef {
    LinePairConstraintRef linePair;
    double value = 0.0;
};

enum class ConstraintPointPosition {
    Start,
    End,
    Mid,
};

enum class ConstraintPointKind {
    SegmentEndpoint,
    PointGeometry,
    CircleCenter,
    EllipseCenter,
    ArcEndpoint,
    EllipseArcEndpoint,
};

struct ConstraintPointRef {
    ConstraintPointKind kind = ConstraintPointKind::SegmentEndpoint;
    std::size_t index = 0;
    bool start = true;
};

struct TangentConstraintRef {
    TangentGeometryRef first;
    TangentGeometryRef second;
    std::optional<ConstraintPointRef> firstPoint;
    std::optional<ConstraintPointRef> secondPoint;
    std::optional<ConstraintPointRef> viaPoint;
};

struct PointwiseAngleConstraintRef {
    TangentConstraintRef geometry;
    double value = 0.0;
};

struct PerpendicularPointLineConstraintRef {
    ConstraintPointRef first;
    ConstraintPointRef second;
    std::size_t axisSegmentIndex = 0;
};

enum class PerpendicularMidpointTargetKind {
    Circle,
    Arc,
};

struct PerpendicularMidpointLineConstraintRef {
    std::size_t lineSegmentIndex = 0;
    PerpendicularMidpointTargetKind targetKind = PerpendicularMidpointTargetKind::Circle;
    std::size_t targetIndex = 0;
};

struct PointOnObjectConstraintRef {
    ConstraintPointRef point;
    int objectGeometryIndex = -1;
};

struct SymmetricConstraintRef {
    ConstraintPointRef first;
    ConstraintPointRef second;
    std::optional<std::size_t> axisSegmentIndex;
    std::optional<ConstraintPointRef> centerPoint;
};

struct BlockConstraintRef {
    int geometryIndex = -1;
};

struct AppliedSketchConstraints {
    std::size_t coincident = 0;
    std::size_t orientation = 0;
    std::size_t dimension = 0;
    std::size_t relation = 0;
    std::size_t block = 0;
};

enum class SketchProfileEdgeKind { Line, ArcOfCircle, ArcOfEllipse, BSpline };

struct SketchProfileEdge {
    SketchProfileEdgeKind kind = SketchProfileEdgeKind::Line;
    gp_Pnt start;
    gp_Pnt end;
    gp_Pnt center;
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    int degree = 0;
    std::vector<gp_Pnt> poles;
};

struct UnionFind {
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

double readNumber2(const nlohmann::json& value, std::size_t index, bool& ok)
{
    if (!value.is_array() || value.size() != 2 || !value.at(index).is_number()) {
        ok = false;
        return 0.0;
    }
    return value.at(index).get<double>();
}

bool samePoint(const gp_Pnt& left, const gp_Pnt& right)
{
    constexpr double eps = 1e-9;
    return std::abs(left.X() - right.X()) < eps && std::abs(left.Y() - right.Y()) < eps;
}

gp_Pnt pointAtAngle(const gp_Pnt& center, double radius, double angle)
{
    return gp_Pnt(center.X() + radius * std::cos(angle), center.Y() + radius * std::sin(angle), center.Z());
}

gp_Ax2 ellipseAxis(const gp_Pnt& center, double angle)
{
    gp_Ax2 axis(center, gp_Dir(0, 0, 1));
    axis.Rotate(gp_Ax1(center, gp_Dir(0, 0, 1)), angle);
    return axis;
}

gp_Pnt pointAtEllipseAngle(const gp_Pnt& center, double majorRadius, double minorRadius, double angleXU, double parameter)
{
    const double cosAxis = std::cos(angleXU);
    const double sinAxis = std::sin(angleXU);
    const double localX = majorRadius * std::cos(parameter);
    const double localY = minorRadius * std::sin(parameter);
    return gp_Pnt(center.X() + localX * cosAxis - localY * sinAxis,
                  center.Y() + localX * sinAxis + localY * cosAxis,
                  center.Z());
}

gp_Pnt pointInSketchLocalPlane(const gp_Pnt& worldPoint, const gp_Trsf& sketchPlacement)
{
    gp_Trsf inverse = sketchPlacement;
    inverse.Invert();
    gp_Pnt local = worldPoint.Transformed(inverse);
    return gp_Pnt(local.X(), local.Y(), 0.0);
}

gp_Dir directionInSketchLocalPlane(gp_Dir worldDirection, const gp_Trsf& sketchPlacement)
{
    gp_Trsf inverse = sketchPlacement;
    inverse.Invert();
    worldDirection.Transform(inverse);
    return worldDirection;
}

gp_Pln sketchPlaneFromPlacement(const gp_Trsf& sketchPlacement)
{
    gp_Pnt origin(0, 0, 0);
    origin.Transform(sketchPlacement);
    gp_Dir normal(0, 0, 1);
    normal.Transform(sketchPlacement);
    return gp_Pln(origin, normal);
}

std::optional<SketchEllipse> projectedEllipseFromAxes(const gp_Pnt& center,
                                                      const gp_Dir& majorDirection,
                                                      double majorRadius,
                                                      const gp_Dir& minorDirection,
                                                      double minorRadius,
                                                      const gp_Trsf& sketchPlacement)
{
    const gp_Dir localMajor = directionInSketchLocalPlane(majorDirection, sketchPlacement);
    const gp_Dir localMinor = directionInSketchLocalPlane(minorDirection, sketchPlacement);

    const double ax = majorRadius * localMajor.X();
    const double ay = majorRadius * localMajor.Y();
    const double bx = minorRadius * localMinor.X();
    const double by = minorRadius * localMinor.Y();

    const double sxx = ax * ax + bx * bx;
    const double syy = ay * ay + by * by;
    const double sxy = ax * ay + bx * by;
    const double trace = sxx + syy;
    const double delta = std::sqrt((sxx - syy) * (sxx - syy) + 4.0 * sxy * sxy);
    const double lambdaMajor = 0.5 * (trace + delta);
    const double lambdaMinor = 0.5 * (trace - delta);
    if (lambdaMajor <= Precision::SquareConfusion()) {
        return std::nullopt;
    }

    double angle = 0.0;
    if (std::abs(sxy) > Precision::Confusion()) {
        angle = std::atan2(lambdaMajor - sxx, sxy);
    }
    else if (syy > sxx) {
        angle = 0.5 * 3.14159265358979323846;
    }

    return SketchEllipse{0U,
                         center,
                         std::sqrt(lambdaMajor),
                         lambdaMinor > Precision::SquareConfusion() ? std::sqrt(lambdaMinor) : 0.0,
                         angle,
                         true};
}

double angleXUInSketchPlane(gp_Dir worldXDirection, gp_Dir worldNormal, const gp_Trsf& sketchPlacement)
{
    const gp_Dir localX = directionInSketchLocalPlane(worldXDirection, sketchPlacement);
    const gp_Dir localNormal = directionInSketchLocalPlane(worldNormal, sketchPlacement);
    return -localX.AngleWithRef(gp_Dir(1, 0, 0), localNormal);
}

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

bool readBoolField(const nlohmann::json& value, const std::string& field, bool fallback)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return fallback;
    }
    if (it->is_boolean()) {
        return it->get<bool>();
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_boolean()) {
        return it->at("value").get<bool>();
    }
    return fallback;
}

std::optional<gp_Pnt> readPoint2Field(const nlohmann::json& value)
{
    bool ok = true;
    const double x = readNumber2(value, 0, ok);
    const double y = readNumber2(value, 1, ok);
    if (!ok) {
        return std::nullopt;
    }
    return gp_Pnt(x, y, 0.0);
}

std::optional<double> readNumberField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return std::nullopt;
    }
    if (it->is_number()) {
        const double number = it->get<double>();
        return std::isfinite(number) ? std::optional<double>{number} : std::nullopt;
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_number()) {
        const double number = it->at("value").get<double>();
        return std::isfinite(number) ? std::optional<double>{number} : std::nullopt;
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

std::optional<ConstraintPointPosition> readPointPosition(const nlohmann::json& constraint, const std::string& field)
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

std::optional<std::size_t> segmentIndexForGeometry(const std::vector<SketchSegment>& segments, int geometryIndex)
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

std::optional<std::size_t> circleIndexForGeometry(const std::vector<SketchCircle>& circles, int geometryIndex)
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

std::optional<std::size_t> ellipseIndexForGeometry(const std::vector<SketchEllipse>& ellipses, int geometryIndex)
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

std::optional<std::size_t> ellipseArcIndexForGeometry(const std::vector<SketchEllipseArc>& arcs, int geometryIndex)
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

std::optional<std::size_t> bsplineIndexForGeometry(const std::vector<SketchBSpline>& bsplines, int geometryIndex)
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

std::optional<EndpointRef> readEndpointRef(const nlohmann::json& constraint,
                                           const std::string& indexField,
                                           const std::string& positionField,
                                           const std::vector<SketchSegment>& segments)
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
    return EndpointRef{*segmentIndex, *start};
}

std::optional<OrientationConstraintRef> readOrientationConstraintRef(const nlohmann::json& constraint,
                                                                     SketchConstraintKind kind,
                                                                     const std::vector<SketchSegment>& segments)
{
    if (constraint.contains("FirstPos") || constraint.contains("Second") || constraint.contains("SecondPos")) {
        const auto first = readEndpointRef(constraint, "First", "FirstPos", segments);
        const auto second = readEndpointRef(constraint, "Second", "SecondPos", segments);
        if (!first || !second) {
            return std::nullopt;
        }
        return OrientationConstraintRef{kind, std::nullopt, first, second};
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex) {
        return std::nullopt;
    }
    const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex);
    if (!segmentIndex) {
        return std::nullopt;
    }
    return OrientationConstraintRef{kind, segmentIndex, std::nullopt, std::nullopt};
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

std::optional<DimensionConstraintRef> readDimensionConstraintRef(const nlohmann::json& constraint,
                                                                 SketchConstraintKind kind,
                                                                 const std::vector<SketchSegment>& segments)
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
        return DimensionConstraintRef{kind, -1, *value, std::nullopt, std::nullopt, endpoint};
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
        return DimensionConstraintRef{kind, -1, *value, first, second};
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex) {
        return std::nullopt;
    }
    return DimensionConstraintRef{kind, *geometryIndex, *value, std::nullopt, std::nullopt};
}

std::optional<LinePairConstraintRef> readLinePairConstraintRef(const nlohmann::json& constraint,
                                                               SketchConstraintKind kind,
                                                               const std::vector<SketchSegment>& segments)
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
    return LinePairConstraintRef{kind, *firstSegment, *secondSegment};
}

std::optional<TangentGeometryRef> tangentGeometryRefForIndex(int geometryIndex,
                                                            const std::vector<SketchSegment>& segments,
                                                            const std::vector<SketchCircle>& circles,
                                                            const std::vector<SketchEllipse>& ellipses,
                                                            const std::vector<SketchArc>& arcs,
                                                            const std::vector<SketchEllipseArc>& ellipseArcs)
{
    if (const auto segment = segmentIndexForGeometry(segments, geometryIndex)) {
        return TangentGeometryRef{TangentGeometryKind::Line, *segment};
    }
    if (const auto circle = circleIndexForGeometry(circles, geometryIndex)) {
        return TangentGeometryRef{TangentGeometryKind::Circle, *circle};
    }
    if (const auto arc = arcIndexForGeometry(arcs, geometryIndex)) {
        return TangentGeometryRef{TangentGeometryKind::Arc, *arc};
    }
    if (const auto ellipse = ellipseIndexForGeometry(ellipses, geometryIndex)) {
        return TangentGeometryRef{TangentGeometryKind::Ellipse, *ellipse};
    }
    if (const auto ellipseArc = ellipseArcIndexForGeometry(ellipseArcs, geometryIndex)) {
        return TangentGeometryRef{TangentGeometryKind::EllipseArc, *ellipseArc};
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

    const bool firstRound = first.kind == TangentGeometryKind::Circle || first.kind == TangentGeometryKind::Arc;
    const bool secondRound = second.kind == TangentGeometryKind::Circle || second.kind == TangentGeometryKind::Arc;
    return firstRound && secondRound;
}

std::optional<ConstraintPointRef> readTangentCurvePointRef(const nlohmann::json& constraint,
                                                           const std::string& geometryField,
                                                           const std::string& positionField,
                                                           const std::vector<SketchSegment>& segments,
                                                           const std::vector<SketchArc>& arcs,
                                                           const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position) {
        return std::nullopt;
    }

    if (*position == ConstraintPointPosition::Start || *position == ConstraintPointPosition::End) {
        if (const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::SegmentEndpoint, *segmentIndex, *position == ConstraintPointPosition::Start};
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::ArcEndpoint, *arcIndex, *position == ConstraintPointPosition::Start};
        }
        if (const auto ellipseArcIndex = ellipseArcIndexForGeometry(ellipseArcs, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::EllipseArcEndpoint, *ellipseArcIndex, *position == ConstraintPointPosition::Start};
        }
    }

    return std::nullopt;
}

std::optional<ConstraintPointRef> readTangentPointRef(const nlohmann::json& constraint,
                                                      const std::string& geometryField,
                                                      const std::string& positionField,
                                                      const std::vector<SketchSegment>& segments,
                                                      const std::vector<SketchPoint>& points,
                                                      const std::vector<SketchArc>& arcs,
                                                      const std::vector<SketchEllipseArc>& ellipseArcs)
{
    if (const auto curvePoint = readTangentCurvePointRef(constraint, geometryField, positionField, segments, arcs, ellipseArcs)) {
        return curvePoint;
    }

    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position || *position != ConstraintPointPosition::Start) {
        return std::nullopt;
    }
    if (const auto pointIndex = pointIndexForGeometry(points, *geometryIndex)) {
        return ConstraintPointRef{ConstraintPointKind::PointGeometry, *pointIndex, true};
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
    const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }

    const auto first = tangentGeometryRefForIndex(*firstGeometry, segments, circles, ellipses, arcs, ellipseArcs);
    const auto second = tangentGeometryRefForIndex(*secondGeometry, segments, circles, ellipses, arcs, ellipseArcs);
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
        return TangentConstraintRef{*first, *second};
    }

    if (hasThird) {
        if (!firstNone || !secondNone) {
            return std::nullopt;
        }
        const auto viaPoint =
            readTangentPointRef(constraint, "Third", "ThirdPos", segments, points, arcs, ellipseArcs);
        if (!viaPoint) {
            return std::nullopt;
        }
        return TangentConstraintRef{*first, *second, std::nullopt, std::nullopt, viaPoint};
    }

    const auto firstPoint =
        readTangentCurvePointRef(constraint, "First", "FirstPos", segments, arcs, ellipseArcs);
    if (!firstPoint) {
        return std::nullopt;
    }
    if (secondNone) {
        return TangentConstraintRef{*first, *second, firstPoint};
    }

    const auto secondPoint =
        readTangentCurvePointRef(constraint, "Second", "SecondPos", segments, arcs, ellipseArcs);
    if (!secondPoint) {
        return std::nullopt;
    }
    return TangentConstraintRef{*first, *second, firstPoint, secondPoint};
}

std::optional<PointwiseAngleConstraintRef> readPointwiseAngleConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto value = readConstraintValue(constraint);
    if (!value || !std::isfinite(*value) || *value < 0.0 || *value > 3.14159265358979323846) {
        return std::nullopt;
    }

    const auto geometry = readTangentConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!geometry || (!geometry->firstPoint && !geometry->viaPoint)) {
        return std::nullopt;
    }
    return PointwiseAngleConstraintRef{*geometry, *value};
}

std::optional<ConstraintPointRef> readConstraintPointRef(const nlohmann::json& constraint,
                                                         const std::string& geometryField,
                                                         const std::string& positionField,
                                                         const std::vector<SketchSegment>& segments,
                                                         const std::vector<SketchPoint>& points,
                                                         const std::vector<SketchCircle>& circles,
                                                         const std::vector<SketchEllipse>& ellipses,
                                                         const std::vector<SketchArc>& arcs,
                                                         const std::vector<SketchEllipseArc>& ellipseArcs);

std::optional<PerpendicularPointLineConstraintRef> readPerpendicularPointLineConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs)
{
    if (!readPointPositionIsNone(constraint, "ThirdPos", true)) {
        return std::nullopt;
    }

    const auto first = readConstraintPointRef(
        constraint, "First", "FirstPos", segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second = readConstraintPointRef(
        constraint, "Second", "SecondPos", segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto thirdGeometry = readIntField(constraint, "Third");
    if (!first || !second || !thirdGeometry) {
        return std::nullopt;
    }

    const auto axisSegment = segmentIndexForGeometry(segments, *thirdGeometry);
    if (!axisSegment) {
        return std::nullopt;
    }
    return PerpendicularPointLineConstraintRef{*first, *second, *axisSegment};
}

std::optional<PerpendicularMidpointLineConstraintRef> readPerpendicularMidpointLineConstraintRef(
    const nlohmann::json& constraint,
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchArc>& arcs)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos") || constraint.contains("Third")
        || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto firstGeometry = readIntField(constraint, "First");
    const auto secondGeometry = readIntField(constraint, "Second");
    if (!firstGeometry || !secondGeometry) {
        return std::nullopt;
    }

    const auto firstSegment = segmentIndexForGeometry(segments, *firstGeometry);
    const auto secondSegment = segmentIndexForGeometry(segments, *secondGeometry);
    const auto readTarget = [&](int geometryIndex) -> std::optional<PerpendicularMidpointLineConstraintRef> {
        if (const auto circleIndex = circleIndexForGeometry(circles, geometryIndex)) {
            return PerpendicularMidpointLineConstraintRef{0, PerpendicularMidpointTargetKind::Circle, *circleIndex};
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, geometryIndex)) {
            return PerpendicularMidpointLineConstraintRef{0, PerpendicularMidpointTargetKind::Arc, *arcIndex};
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
    return EqualConstraintRef{*firstGeometry, *secondGeometry};
}

std::optional<AngleConstraintRef> readAngleConstraintRef(const nlohmann::json& constraint,
                                                         const std::vector<SketchSegment>& segments)
{
    if (constraint.contains("FirstPos") || constraint.contains("SecondPos") || constraint.contains("Third")
        || constraint.contains("ThirdPos")) {
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
    return AngleConstraintRef{*linePair, *value};
}

std::optional<ConstraintPointRef> readConstraintPointRef(const nlohmann::json& constraint,
                                                         const std::string& geometryField,
                                                         const std::string& positionField,
                                                         const std::vector<SketchSegment>& segments,
                                                         const std::vector<SketchPoint>& points,
                                                         const std::vector<SketchCircle>& circles,
                                                         const std::vector<SketchEllipse>& ellipses,
                                                         const std::vector<SketchArc>& arcs,
                                                         const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto geometryIndex = readIntField(constraint, geometryField);
    const auto position = readPointPosition(constraint, positionField);
    if (!geometryIndex || !position) {
        return std::nullopt;
    }

    if (*position == ConstraintPointPosition::Start || *position == ConstraintPointPosition::End) {
        if (const auto segmentIndex = segmentIndexForGeometry(segments, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::SegmentEndpoint, *segmentIndex, *position == ConstraintPointPosition::Start};
        }
        if (*position == ConstraintPointPosition::Start) {
            if (const auto pointIndex = pointIndexForGeometry(points, *geometryIndex)) {
                return ConstraintPointRef{ConstraintPointKind::PointGeometry, *pointIndex, true};
            }
        }
        if (const auto arcIndex = arcIndexForGeometry(arcs, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::ArcEndpoint, *arcIndex, *position == ConstraintPointPosition::Start};
        }
        if (const auto ellipseArcIndex = ellipseArcIndexForGeometry(ellipseArcs, *geometryIndex)) {
            return ConstraintPointRef{
                ConstraintPointKind::EllipseArcEndpoint, *ellipseArcIndex, *position == ConstraintPointPosition::Start};
        }
    }

    if (*position == ConstraintPointPosition::Mid) {
        if (const auto circleIndex = circleIndexForGeometry(circles, *geometryIndex)) {
            return ConstraintPointRef{ConstraintPointKind::CircleCenter, *circleIndex, true};
        }
        if (const auto ellipseIndex = ellipseIndexForGeometry(ellipses, *geometryIndex)) {
            return ConstraintPointRef{ConstraintPointKind::EllipseCenter, *ellipseIndex, true};
        }
    }

    return std::nullopt;
}

std::optional<ConstraintPointRef> readConstraintPointRef(const nlohmann::json& constraint,
                                                         const std::vector<SketchSegment>& segments,
                                                         const std::vector<SketchPoint>& points,
                                                         const std::vector<SketchCircle>& circles,
                                                         const std::vector<SketchEllipse>& ellipses,
                                                         const std::vector<SketchArc>& arcs,
                                                         const std::vector<SketchEllipseArc>& ellipseArcs)
{
    return readConstraintPointRef(
        constraint, "First", "FirstPos", segments, points, circles, ellipses, arcs, ellipseArcs);
}

bool hasPointOnObjectTarget(int geometryIndex,
                            const std::vector<SketchSegment>& segments,
                            const std::vector<SketchCircle>& circles,
                            const std::vector<SketchEllipse>& ellipses,
                            const std::vector<SketchArc>& arcs,
                            const std::vector<SketchEllipseArc>& ellipseArcs)
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
    const std::vector<SketchEllipseArc>& ellipseArcs)
{
    if (constraint.contains("SecondPos") || constraint.contains("Third") || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto targetGeometry = readIntField(constraint, "Second");
    if (!targetGeometry || !hasPointOnObjectTarget(*targetGeometry, segments, circles, ellipses, arcs, ellipseArcs)) {
        return std::nullopt;
    }

    const auto point = readConstraintPointRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!point) {
        return std::nullopt;
    }
    return PointOnObjectConstraintRef{*point, *targetGeometry};
}

std::optional<SymmetricConstraintRef> readSymmetricConstraintRef(const nlohmann::json& constraint,
                                                                 const std::vector<SketchSegment>& segments,
                                                                 const std::vector<SketchPoint>& points,
                                                                 const std::vector<SketchCircle>& circles,
                                                                 const std::vector<SketchEllipse>& ellipses,
                                                                 const std::vector<SketchArc>& arcs,
                                                                 const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto first = readConstraintPointRef(
        constraint, "First", "FirstPos", segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second = readConstraintPointRef(
        constraint, "Second", "SecondPos", segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto thirdGeometry = readIntField(constraint, "Third");
    if (!first || !second || !thirdGeometry) {
        return std::nullopt;
    }

    if (readPointPositionIsNone(constraint, "ThirdPos", true)) {
        const auto axisSegment = segmentIndexForGeometry(segments, *thirdGeometry);
        if (!axisSegment) {
            return std::nullopt;
        }
        return SymmetricConstraintRef{*first, *second, axisSegment, std::nullopt};
    }

    const auto center = readConstraintPointRef(
        constraint, "Third", "ThirdPos", segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!center) {
        return std::nullopt;
    }
    return SymmetricConstraintRef{*first, *second, std::nullopt, center};
}

bool hasBlockTarget(int geometryIndex,
                    const std::vector<SketchSegment>& segments,
                    const std::vector<SketchPoint>& points,
                    const std::vector<SketchCircle>& circles,
                    const std::vector<SketchEllipse>& ellipses,
                    const std::vector<SketchArc>& arcs,
                    const std::vector<SketchEllipseArc>& ellipseArcs,
                    const std::vector<SketchBSpline>& bsplines)
{
    return segmentIndexForGeometry(segments, geometryIndex).has_value()
        || pointIndexForGeometry(points, geometryIndex).has_value()
        || circleIndexForGeometry(circles, geometryIndex).has_value()
        || ellipseIndexForGeometry(ellipses, geometryIndex).has_value()
        || arcIndexForGeometry(arcs, geometryIndex).has_value()
        || ellipseArcIndexForGeometry(ellipseArcs, geometryIndex).has_value()
        || bsplineIndexForGeometry(bsplines, geometryIndex).has_value();
}

std::optional<BlockConstraintRef> readBlockConstraintRef(const nlohmann::json& constraint,
                                                         const std::vector<SketchSegment>& segments,
                                                         const std::vector<SketchPoint>& points,
                                                         const std::vector<SketchCircle>& circles,
                                                         const std::vector<SketchEllipse>& ellipses,
                                                         const std::vector<SketchArc>& arcs,
                                                         const std::vector<SketchEllipseArc>& ellipseArcs,
                                                         const std::vector<SketchBSpline>& bsplines)
{
    if ((constraint.contains("FirstPos") && !readPointPositionIsNone(constraint, "FirstPos", false))
        || constraint.contains("Second") || constraint.contains("SecondPos") || constraint.contains("Third")
        || constraint.contains("ThirdPos")) {
        return std::nullopt;
    }

    const auto geometryIndex = readIntField(constraint, "First");
    if (!geometryIndex
        || !hasBlockTarget(*geometryIndex, segments, points, circles, ellipses, arcs, ellipseArcs, bsplines)) {
        return std::nullopt;
    }
    return BlockConstraintRef{*geometryIndex};
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

bool parseSketchGeometry(const nlohmann::json& geometry,
                         const document::DocumentObject& object,
                         runtime::ComputeContext& context,
                         SketchGeometrySet& parsed)
{
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        const auto& item = geometry.at(index);
        if (!item.is_object() || !item.contains("kind") || !item.at("kind").is_string()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_geometry",
                                   "Sketch Geometry item must declare a supported kind",
                                   object.name,
                                   "Geometry");
            return false;
        }

        const std::string kind = item.at("kind").get<std::string>();
        if (kind == "Point" || kind == "GeomPoint") {
            bool ok = true;
            const double px = readNumber2(item.at("point"), 0, ok);
            const double py = readNumber2(item.at("point"), 1, ok);
            if (!ok) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "Point must provide a two-number point",
                                       object.name,
                                       "Geometry");
                return false;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::SketchObject::buildShape(), for "Part::GeomPoint" calls geo->toShape()
            // and exposes it as a Vertex in the Sketch Shape compound.
            parsed.points.push_back(SketchPoint{index,
                                                gp_Pnt(px, py, 0.0),
                                                readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "LineSegment") {
            bool ok = true;
            const double sx = readNumber2(item.at("start"), 0, ok);
            const double sy = readNumber2(item.at("start"), 1, ok);
            const double ex = readNumber2(item.at("end"), 0, ok);
            const double ey = readNumber2(item.at("end"), 1, ok);
            if (!ok) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "LineSegment start/end must be two numbers",
                                       object.name,
                                       "Geometry");
                return false;
            }

            parsed.segments.push_back(SketchSegment{index,
                                                    gp_Pnt(sx, sy, 0.0),
                                                    gp_Pnt(ex, ey, 0.0),
                                                    readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "Circle") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto radius = readNumberField(item, "radius");
            if (!ok || !radius || *radius <= 0.0) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "Circle center must be two numbers and radius must be positive",
                                       object.name,
                                       "Geometry");
                return false;
            }

            parsed.circles.push_back(SketchCircle{index,
                                                  gp_Pnt(cx, cy, 0.0),
                                                  *radius,
                                                  readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "Ellipse") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto majorRadius = readNumberField(item, "majorRadius");
            const auto minorRadius = readNumberField(item, "minorRadius");
            const auto angle = readNumberField(item, "angle").value_or(0.0);
            if (!ok || !majorRadius || !minorRadius || *majorRadius <= 0.0 || *minorRadius <= 0.0
                || *majorRadius < *minorRadius || !std::isfinite(angle)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "Ellipse center, majorRadius and minorRadius are required",
                                       object.name,
                                       "Geometry");
                return false;
            }

            parsed.ellipses.push_back(SketchEllipse{index,
                                                    gp_Pnt(cx, cy, 0.0),
                                                    *majorRadius,
                                                    *minorRadius,
                                                    angle,
                                                    readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "ArcOfCircle") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto radius = readNumberField(item, "radius");
            const auto startAngle = readNumberField(item, "startAngle");
            const auto endAngle = readNumberField(item, "endAngle");
            if (!ok || !radius || !startAngle || !endAngle || *radius <= 0.0 || *startAngle == *endAngle) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "ArcOfCircle center, positive radius, startAngle and endAngle are required",
                                       object.name,
                                       "Geometry");
                return false;
            }

            parsed.arcs.push_back(SketchArc{index,
                                            gp_Pnt(cx, cy, 0.0),
                                            *radius,
                                            *startAngle,
                                            *endAngle,
                                            readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "ArcOfEllipse") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto majorRadius = readNumberField(item, "majorRadius");
            const auto minorRadius = readNumberField(item, "minorRadius");
            const auto angle = readNumberField(item, "angle").value_or(0.0);
            const auto startAngle = readNumberField(item, "startAngle");
            const auto endAngle = readNumberField(item, "endAngle");
            if (!ok || !majorRadius || !minorRadius || !startAngle || !endAngle || *majorRadius <= 0.0
                || *minorRadius <= 0.0 || *majorRadius < *minorRadius || *startAngle == *endAngle
                || !std::isfinite(angle)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "ArcOfEllipse center, radii, startAngle and endAngle are required",
                                       object.name,
                                       "Geometry");
                return false;
            }

            parsed.ellipseArcs.push_back(SketchEllipseArc{index,
                                                          gp_Pnt(cx, cy, 0.0),
                                                          *majorRadius,
                                                          *minorRadius,
                                                          angle,
                                                          *startAngle,
                                                          *endAngle,
                                                          readBoolField(item, "construction", false)});
            continue;
        }

        if (kind == "BSpline" || kind == "BSplineCurve" || kind == "GeomBSplineCurve") {
            const auto degree = readIntField(item, "degree");
            const auto polesIt = item.find("poles");
            std::vector<gp_Pnt> poles;
            if (polesIt != item.end() && polesIt->is_array()) {
                poles.reserve(polesIt->size());
                for (const auto& pole : *polesIt) {
                    const auto point = readPoint2Field(pole);
                    if (!point) {
                        break;
                    }
                    poles.push_back(*point);
                }
            }
            const std::size_t rawPoleCount = polesIt != item.end() && polesIt->is_array() ? polesIt->size() : 0U;
            if (!degree || *degree < 1 || poles.size() != rawPoleCount
                || poles.size() < static_cast<std::size_t>(*degree + 1)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "BSpline requires a positive degree and at least degree + 1 two-number poles",
                                       object.name,
                                       "Geometry");
                return false;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
            // ::SketchBSplineCurve::getPoint(), returns "bsp->getStartPoint()" and
            // "bsp->getEndPoint()" for profile connectivity.
            parsed.bsplines.push_back(
                SketchBSpline{index, *degree, std::move(poles), readBoolField(item, "construction", false)});
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // registers geometry families independently. cad-core keeps unsupported families explicit
        // until their profile and internal-shape paths are migrated.
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_geometry",
                               "Sketch Geometry kind " + kind + " is not supported in the current P5 subset",
                               object.name,
                               "Geometry");
        return false;
    }
    return true;
}

std::vector<SketchCircle> profileCircles(const std::vector<SketchCircle>& circles)
{
    std::vector<SketchCircle> profile;
    for (const auto& circle : circles) {
        if (!circle.construction) {
            profile.push_back(circle);
        }
    }
    return profile;
}

std::vector<SketchEllipse> profileEllipses(const std::vector<SketchEllipse>& ellipses)
{
    std::vector<SketchEllipse> profile;
    for (const auto& ellipse : ellipses) {
        if (!ellipse.construction) {
            profile.push_back(ellipse);
        }
    }
    return profile;
}

bool addCircleWire(const SketchCircle& circle,
                   BRepBuilderAPI_MakeWire& wireBuilder)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
    // registers Part::GeomCircle as a Sketcher geometry type with a center point but no start/end
    // endpoint; a single non-construction circle is already a closed profile.
    BRepBuilderAPI_MakeEdge edgeBuilder(gp_Circ(gp_Ax2(circle.center, gp_Dir(0, 0, 1)), circle.radius));
    if (!edgeBuilder.IsDone()) {
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return wireBuilder.IsDone();
}

bool addEllipseWire(const SketchEllipse& ellipse,
                    BRepBuilderAPI_MakeWire& wireBuilder)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
    // GeomEllipse::Restore() rebuilds from "MajorRadius", "MinorRadius" and "AngleXU".
    const gp_Ax2 axis = ellipseAxis(ellipse.center, ellipse.angle);
    BRepBuilderAPI_MakeEdge edgeBuilder(gp_Elips(axis, ellipse.majorRadius, ellipse.minorRadius));
    if (!edgeBuilder.IsDone()) {
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return wireBuilder.IsDone();
}

bool orientationConstraintSatisfied(const OrientationConstraintRef& constraint,
                                    const std::vector<SketchSegment>& segments)
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

std::optional<double> dimensionConstraintValue(const DimensionConstraintRef& constraint,
                                               const std::vector<SketchSegment>& segments,
                                               const std::vector<SketchCircle>& circles,
                                               const std::vector<SketchArc>& arcs)
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

    if (constraint.kind == SketchConstraintKind::Radius || constraint.kind == SketchConstraintKind::Diameter) {
        for (const SketchCircle& circle : circles) {
            if (circle.geometryIndex == static_cast<std::size_t>(constraint.geometryIndex)) {
                return constraint.kind == SketchConstraintKind::Diameter ? 2.0 * circle.radius : circle.radius;
            }
        }
        for (const SketchArc& arc : arcs) {
            if (arc.geometryIndex == static_cast<std::size_t>(constraint.geometryIndex)) {
                return constraint.kind == SketchConstraintKind::Diameter ? 2.0 * arc.radius : arc.radius;
            }
        }
    }
    return std::nullopt;
}

bool dimensionConstraintSatisfied(const DimensionConstraintRef& constraint,
                                  const std::vector<SketchSegment>& segments,
                                  const std::vector<SketchCircle>& circles,
                                  const std::vector<SketchArc>& arcs)
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
    return std::pair<double, double>{dx / length, dy / length};
}

double pointToLineDistance2d(const gp_Pnt& point, const SketchSegment& segment);
std::optional<gp_Pnt> constraintPointValue(const ConstraintPointRef& point,
                                           const std::vector<SketchSegment>& segments,
                                           const std::vector<SketchPoint>& points,
                                           const std::vector<SketchCircle>& circles,
                                           const std::vector<SketchEllipse>& ellipses,
                                           const std::vector<SketchArc>& arcs,
                                           const std::vector<SketchEllipseArc>& ellipseArcs);

bool linePairConstraintSatisfied(const LinePairConstraintRef& constraint,
                                 const std::vector<SketchSegment>& segments)
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

std::optional<double> linePairAngleValue(const LinePairConstraintRef& constraint,
                                         const std::vector<SketchSegment>& segments)
{
    const auto first = segmentDirection2d(segments.at(constraint.firstSegmentIndex));
    const auto second = segmentDirection2d(segments.at(constraint.secondSegmentIndex));
    if (!first || !second) {
        return std::nullopt;
    }

    const double dot = first->first * second->first + first->second * second->second;
    return std::acos(std::clamp(std::abs(dot), -1.0, 1.0));
}

struct RoundTangentGeometry {
    gp_Pnt center;
    double radius = 0.0;
};

struct EllipseTangentGeometry {
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
};

std::optional<RoundTangentGeometry> roundTangentGeometry(const TangentGeometryRef& geometry,
                                                        const std::vector<SketchCircle>& circles,
                                                        const std::vector<SketchArc>& arcs)
{
    if (geometry.kind == TangentGeometryKind::Circle) {
        const SketchCircle& circle = circles.at(geometry.index);
        return RoundTangentGeometry{circle.center, circle.radius};
    }
    if (geometry.kind == TangentGeometryKind::Arc) {
        const SketchArc& arc = arcs.at(geometry.index);
        return RoundTangentGeometry{arc.center, arc.radius};
    }
    return std::nullopt;
}

std::optional<EllipseTangentGeometry> ellipseTangentGeometry(const TangentGeometryRef& geometry,
                                                            const std::vector<SketchEllipse>& ellipses,
                                                            const std::vector<SketchEllipseArc>& ellipseArcs)
{
    if (geometry.kind == TangentGeometryKind::Ellipse) {
        const SketchEllipse& ellipse = ellipses.at(geometry.index);
        return EllipseTangentGeometry{ellipse.center, ellipse.majorRadius, ellipse.minorRadius, ellipse.angle};
    }
    if (geometry.kind == TangentGeometryKind::EllipseArc) {
        const SketchEllipseArc& arc = ellipseArcs.at(geometry.index);
        return EllipseTangentGeometry{arc.center, arc.majorRadius, arc.minorRadius, arc.angle};
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
    const double support = std::sqrt(ellipse.majorRadius * ellipse.majorRadius * normalMajor * normalMajor
                                     + ellipse.minorRadius * ellipse.minorRadius * normalMinor * normalMinor);
    return std::abs(pointToLineDistance2d(ellipse.center, line) - support) <= 1e-7;
}

bool roundRoundTangentSatisfied(const RoundTangentGeometry& first, const RoundTangentGeometry& second)
{
    const double distance = first.center.Distance(second.center);
    const double external = first.radius + second.radius;
    const double internal = std::abs(first.radius - second.radius);
    return std::abs(distance - external) <= 1e-7 || (internal > 1e-7 && std::abs(distance - internal) <= 1e-7);
}

std::optional<std::pair<double, double>> normalizeDirection2d(double dx, double dy)
{
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= Precision::Confusion()) {
        return std::nullopt;
    }
    return std::pair<double, double>{dx / length, dy / length};
}

std::optional<std::pair<double, double>> roundTangentDirectionAtPoint(const RoundTangentGeometry& round,
                                                                      const gp_Pnt& point)
{
    const double rx = point.X() - round.center.X();
    const double ry = point.Y() - round.center.Y();
    const double radius = std::sqrt(rx * rx + ry * ry);
    if (std::abs(radius - round.radius) > 1e-7) {
        return std::nullopt;
    }
    return normalizeDirection2d(-ry, rx);
}

std::optional<std::pair<double, double>> ellipseTangentDirectionAtPoint(const EllipseTangentGeometry& ellipse,
                                                                        const gp_Pnt& point)
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
    return normalizeDirection2d(localTangentX * cosAxis - localTangentY * sinAxis,
                                localTangentX * sinAxis + localTangentY * cosAxis);
}

std::optional<std::pair<double, double>> tangentDirectionAtPoint(const TangentGeometryRef& geometry,
                                                                 const gp_Pnt& point,
                                                                 const std::vector<SketchSegment>& segments,
                                                                 const std::vector<SketchCircle>& circles,
                                                                 const std::vector<SketchEllipse>& ellipses,
                                                                 const std::vector<SketchArc>& arcs,
                                                                 const std::vector<SketchEllipseArc>& ellipseArcs)
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

bool tangentDirectionsParallel(const TangentGeometryRef& first,
                               const gp_Pnt& firstPoint,
                               const TangentGeometryRef& second,
                               const gp_Pnt& secondPoint,
                               const std::vector<SketchSegment>& segments,
                               const std::vector<SketchCircle>& circles,
                               const std::vector<SketchEllipse>& ellipses,
                               const std::vector<SketchArc>& arcs,
                               const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto firstDirection = tangentDirectionAtPoint(first, firstPoint, segments, circles, ellipses, arcs, ellipseArcs);
    const auto secondDirection =
        tangentDirectionAtPoint(second, secondPoint, segments, circles, ellipses, arcs, ellipseArcs);
    if (!firstDirection || !secondDirection) {
        return false;
    }

    const double cross = firstDirection->first * secondDirection->second
        - firstDirection->second * secondDirection->first;
    return std::abs(cross) <= 1e-7;
}

bool tangentDirectionsPerpendicular(const TangentGeometryRef& first,
                                    const gp_Pnt& firstPoint,
                                    const TangentGeometryRef& second,
                                    const gp_Pnt& secondPoint,
                                    const std::vector<SketchSegment>& segments,
                                    const std::vector<SketchCircle>& circles,
                                    const std::vector<SketchEllipse>& ellipses,
                                    const std::vector<SketchArc>& arcs,
                                    const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto firstDirection = tangentDirectionAtPoint(first, firstPoint, segments, circles, ellipses, arcs, ellipseArcs);
    const auto secondDirection =
        tangentDirectionAtPoint(second, secondPoint, segments, circles, ellipses, arcs, ellipseArcs);
    if (!firstDirection || !secondDirection) {
        return false;
    }

    const double dot = firstDirection->first * secondDirection->first
        + firstDirection->second * secondDirection->second;
    return std::abs(dot) <= 1e-7;
}

std::optional<double> pointwiseCurveAngleValue(const TangentConstraintRef& constraint,
                                               const std::vector<SketchSegment>& segments,
                                               const std::vector<SketchPoint>& points,
                                               const std::vector<SketchCircle>& circles,
                                               const std::vector<SketchEllipse>& ellipses,
                                               const std::vector<SketchArc>& arcs,
                                               const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto angleAt = [&](const gp_Pnt& firstPoint, const gp_Pnt& secondPoint) -> std::optional<double> {
        const auto firstDirection =
            tangentDirectionAtPoint(constraint.first, firstPoint, segments, circles, ellipses, arcs, ellipseArcs);
        const auto secondDirection =
            tangentDirectionAtPoint(constraint.second, secondPoint, segments, circles, ellipses, arcs, ellipseArcs);
        if (!firstDirection || !secondDirection) {
            return std::nullopt;
        }

        const double dot = firstDirection->first * secondDirection->first
            + firstDirection->second * secondDirection->second;
        return std::acos(std::clamp(std::abs(dot), -1.0, 1.0));
    };

    if (constraint.viaPoint) {
        const auto point =
            constraintPointValue(*constraint.viaPoint, segments, points, circles, ellipses, arcs, ellipseArcs);
        if (!point) {
            return std::nullopt;
        }
        return angleAt(*point, *point);
    }

    if (!constraint.firstPoint) {
        return std::nullopt;
    }

    const auto firstPoint =
        constraintPointValue(*constraint.firstPoint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!firstPoint) {
        return std::nullopt;
    }

    if (constraint.secondPoint) {
        const auto secondPoint =
            constraintPointValue(*constraint.secondPoint, segments, points, circles, ellipses, arcs, ellipseArcs);
        if (!secondPoint || firstPoint->Distance(*secondPoint) > 1e-7) {
            return std::nullopt;
        }
        return angleAt(*firstPoint, *secondPoint);
    }

    return angleAt(*firstPoint, *firstPoint);
}

bool pointwiseCurveAngleConstraintSatisfied(const TangentConstraintRef& constraint,
                                            bool tangent,
                                            const std::vector<SketchSegment>& segments,
                                            const std::vector<SketchPoint>& points,
                                            const std::vector<SketchCircle>& circles,
                                            const std::vector<SketchEllipse>& ellipses,
                                            const std::vector<SketchArc>& arcs,
                                            const std::vector<SketchEllipseArc>& ellipseArcs)
{
    const auto actual = pointwiseCurveAngleValue(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!actual) {
        return false;
    }

    const double expected = tangent ? 0.0 : 3.14159265358979323846 / 2.0;
    return std::abs(*actual - expected) <= 1e-7;
}

bool pointwiseAngleConstraintSatisfied(const PointwiseAngleConstraintRef& constraint,
                                       const std::vector<SketchSegment>& segments,
                                       const std::vector<SketchPoint>& points,
                                       const std::vector<SketchCircle>& circles,
                                       const std::vector<SketchEllipse>& ellipses,
                                       const std::vector<SketchArc>& arcs,
                                       const std::vector<SketchEllipseArc>& ellipseArcs)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addAngleAtPointConstraint() accepts Angle together with Tangent and
    // Perpendicular. For Angle it keeps the user-provided value and adds an angle-via-point
    // GCS constraint. cad-core verifies the already-satisfied point-wise datum and does not move
    // geometry.
    const auto actual = pointwiseCurveAngleValue(
        constraint.geometry, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!actual) {
        return false;
    }

    const double expected = std::min(constraint.value, 3.14159265358979323846 - constraint.value);
    return std::abs(*actual - expected) <= 1e-7;
}

bool tangentConstraintSatisfied(const TangentConstraintRef& constraint,
                                const std::vector<SketchSegment>& segments,
                                const std::vector<SketchPoint>& points,
                                const std::vector<SketchCircle>& circles,
                                const std::vector<SketchEllipse>& ellipses,
                                const std::vector<SketchArc>& arcs,
                                const std::vector<SketchEllipseArc>& ellipseArcs)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Tangent routes whole-geometry constraints to
    // "addTangentConstraint(...)" and routes endpoint-to-curve, endpoint-to-endpoint and
    // tangent-via-point forms to "addAngleAtPointConstraint(..., Tangent, ...)". cad-core
    // verifies already-satisfied direct and point-wise tangency without invoking the solver.
    if (constraint.viaPoint || constraint.firstPoint) {
        return pointwiseCurveAngleConstraintSatisfied(
            constraint, true, segments, points, circles, ellipses, arcs, ellipseArcs);
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
        return tangentConstraintSatisfied(TangentConstraintRef{constraint.second, constraint.first},
                                          segments,
                                          points,
                                          circles,
                                          ellipses,
                                          arcs,
                                          ellipseArcs);
    }

    const auto firstRound = roundTangentGeometry(constraint.first, circles, arcs);
    const auto secondRound = roundTangentGeometry(constraint.second, circles, arcs);
    if (firstRound && secondRound) {
        return roundRoundTangentSatisfied(*firstRound, *secondRound);
    }
    return false;
}

std::optional<double> equalConstraintMeasure(int geometryIndex,
                                             const std::vector<SketchSegment>& segments,
                                             const std::vector<SketchCircle>& circles,
                                             const std::vector<SketchArc>& arcs)
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

bool equalConstraintSatisfied(const EqualConstraintRef& constraint,
                              const std::vector<SketchSegment>& segments,
                              const std::vector<SketchCircle>& circles,
                              const std::vector<SketchArc>& arcs)
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

bool angleConstraintSatisfied(const AngleConstraintRef& constraint,
                              const std::vector<SketchSegment>& segments)
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

std::optional<gp_Pnt> constraintPointValue(const ConstraintPointRef& point,
                                           const std::vector<SketchSegment>& segments,
                                           const std::vector<SketchPoint>& points,
                                           const std::vector<SketchCircle>& circles,
                                           const std::vector<SketchEllipse>& ellipses,
                                           const std::vector<SketchArc>& arcs,
                                           const std::vector<SketchEllipseArc>& ellipseArcs)
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
            return pointAtEllipseAngle(arc.center,
                                       arc.majorRadius,
                                       arc.minorRadius,
                                       arc.angle,
                                       point.start ? arc.startAngle : arc.endAngle);
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

double ellipseEquationError(const gp_Pnt& point,
                            const gp_Pnt& center,
                            double majorRadius,
                            double minorRadius,
                            double angle)
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

bool pointOnObjectConstraintSatisfied(const PointOnObjectConstraintRef& constraint,
                                      const std::vector<SketchSegment>& segments,
                                      const std::vector<SketchPoint>& points,
                                      const std::vector<SketchCircle>& circles,
                                      const std::vector<SketchEllipse>& ellipses,
                                      const std::vector<SketchArc>& arcs,
                                      const std::vector<SketchEllipseArc>& ellipseArcs)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case PointOnObject routes to
    // "addPointOnObjectConstraint(constraint->First, constraint->FirstPos, constraint->Second)".
    // That overload accepts Line, Arc, Circle, Ellipse and ArcOfEllipse targets. cad-core only
    // verifies already-satisfied point-on-object constraints; it does not run the solver or move
    // the constrained point.
    const auto point = constraintPointValue(constraint.point, segments, points, circles, ellipses, arcs, ellipseArcs);
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
        return ellipseEquationError(*point, ellipse.center, ellipse.majorRadius, ellipse.minorRadius, ellipse.angle)
            <= 1e-7;
    }
    if (const auto ellipseArcIndex = ellipseArcIndexForGeometry(ellipseArcs, constraint.objectGeometryIndex)) {
        const SketchEllipseArc& arc = ellipseArcs.at(*ellipseArcIndex);
        return ellipseEquationError(*point, arc.center, arc.majorRadius, arc.minorRadius, arc.angle) <= 1e-7;
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
    return pointToLineDistance2d(midpoint, axis) <= 1e-7 && std::abs(vx * dx + vy * dy) / length <= 1e-7;
}

bool perpendicularPointLineConstraintSatisfied(const PerpendicularPointLineConstraintRef& constraint,
                                               const std::vector<SketchSegment>& segments,
                                               const std::vector<SketchPoint>& points,
                                               const std::vector<SketchCircle>& circles,
                                               const std::vector<SketchEllipse>& ellipses,
                                               const std::vector<SketchArc>& arcs,
                                               const std::vector<SketchEllipseArc>& ellipseArcs)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Perpendicular routes "point point line perpendicularity" to
    // addPerpendicularConstraint(geoId1, pos1, geoId2, pos2, geoId3). That overload calls
    // "GCSsys.addConstraintPerpendicular(p1, p2, l, tag)" and requires geoId3 to be a Line.
    const auto first = constraintPointValue(constraint.first, segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second =
        constraintPointValue(constraint.second, segments, points, circles, ellipses, arcs, ellipseArcs);
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

bool perpendicularMidpointLineConstraintSatisfied(const PerpendicularMidpointLineConstraintRef& constraint,
                                                  const std::vector<SketchSegment>& segments,
                                                  const std::vector<SketchCircle>& circles,
                                                  const std::vector<SketchArc>& arcs)
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

bool symmetricConstraintSatisfied(const SymmetricConstraintRef& constraint,
                                  const std::vector<SketchSegment>& segments,
                                  const std::vector<SketchPoint>& points,
                                  const std::vector<SketchCircle>& circles,
                                  const std::vector<SketchEllipse>& ellipses,
                                  const std::vector<SketchArc>& arcs,
                                  const std::vector<SketchEllipseArc>& ellipseArcs)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
    // ::Sketch::addConstraint(), case Symmetric dispatches either to
    // "addSymmetricConstraint(..., constraint->Third)" for point-point symmetry about a line, or
    // "addSymmetricConstraint(..., constraint->Third, constraint->ThirdPos)" for symmetry about a
    // point. cad-core only verifies already-satisfied point symmetry; it does not run the solver
    // or move geometry.
    const auto first = constraintPointValue(constraint.first, segments, points, circles, ellipses, arcs, ellipseArcs);
    const auto second =
        constraintPointValue(constraint.second, segments, points, circles, ellipses, arcs, ellipseArcs);
    if (!first || !second) {
        return false;
    }

    if (constraint.axisSegmentIndex) {
        return symmetricAroundLineSatisfied(*first, *second, segments.at(*constraint.axisSegmentIndex));
    }
    if (constraint.centerPoint) {
        const auto center =
            constraintPointValue(*constraint.centerPoint, segments, points, circles, ellipses, arcs, ellipseArcs);
        if (!center) {
            return false;
        }
        const gp_Pnt midpoint((first->X() + second->X()) * 0.5, (first->Y() + second->Y()) * 0.5, 0.0);
        return midpoint.Distance(*center) <= 1e-7;
    }
    return false;
}

std::optional<AppliedSketchConstraints> applySketchConstraints(const nlohmann::json& constraints,
                                                              const document::DocumentObject& object,
                                                              runtime::ComputeContext& context,
                                                              std::vector<SketchSegment>& segments,
                                                              const std::vector<SketchPoint>& points,
                                                              const std::vector<SketchCircle>& circles,
                                                              const std::vector<SketchEllipse>& ellipses,
                                                              const std::vector<SketchArc>& arcs,
                                                              const std::vector<SketchEllipseArc>& ellipseArcs,
                                                              const std::vector<SketchBSpline>& bsplines)
{
    if (constraints.is_null()) {
        return AppliedSketchConstraints{};
    }
    if (!constraints.is_array()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Sketch Constraints must be a list",
                               object.name,
                               "Constraints");
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
    for (const auto& constraint : constraints) {
        if (!constraint.is_object()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Sketch Constraints must be objects",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }

        const SketchConstraintKind kind = readSketchConstraintKind(constraint);
        if (kind == SketchConstraintKind::Coincident) {
            const auto first = readEndpointRef(constraint, "First", "FirstPos", segments);
            const auto second = readEndpointRef(constraint, "Second", "SecondPos", segments);
            if (!first || !second) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Coincident constraint must reference two line endpoints",
                                       object.name,
                                       "Constraints");
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
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Horizontal/Vertical constraints currently support whole line references",
                                       object.name,
                                       "Constraints");
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
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Parallel constraints currently support whole line pairs only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            linePairConstraints.push_back(*relation);
            continue;
        }

        if (kind == SketchConstraintKind::Perpendicular) {
            if (constraint.contains("Third") && !readPointPositionIsNone(constraint, "FirstPos", true)
                && !readPointPositionIsNone(constraint, "SecondPos", true)) {
                const auto perpendicularPointLine =
                    readPerpendicularPointLineConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
                if (!perpendicularPointLine) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_property",
                                           "Perpendicular point-point-line constraints require two point references and a line reference",
                                           object.name,
                                           "Constraints");
                    return std::nullopt;
                }
                perpendicularPointLineConstraints.push_back(*perpendicularPointLine);
                continue;
            }

            if (constraint.contains("FirstPos") || constraint.contains("SecondPos") || constraint.contains("Third")
                || constraint.contains("ThirdPos")) {
                const auto perpendicular =
                    readTangentConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
                if (!perpendicular) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_property",
                                           "Perpendicular constraints currently support whole line pairs and point-wise curve references only",
                                           object.name,
                                           "Constraints");
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

            const auto midpoint = readPerpendicularMidpointLineConstraintRef(constraint, segments, circles, arcs);
            if (!midpoint) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Perpendicular constraints currently support whole line pairs, line-circle/arc midpoint pairs, and point-wise curve references only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            perpendicularMidpointLineConstraints.push_back(*midpoint);
            continue;
        }

        if (kind == SketchConstraintKind::Tangent) {
            const auto tangent =
                readTangentConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
            if (!tangent) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Tangent constraints currently support direct whole-geometry and point-wise tangency only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            tangentConstraints.push_back(*tangent);
            continue;
        }

        if (kind == SketchConstraintKind::PointOnObject) {
            const auto pointOnObject =
                readPointOnObjectConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
            if (!pointOnObject) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "PointOnObject constraints currently support line/arc/point references on line, circle, arc, ellipse and ellipse-arc targets only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            pointOnObjectConstraints.push_back(*pointOnObject);
            continue;
        }

        if (kind == SketchConstraintKind::Symmetric) {
            const auto symmetric =
                readSymmetricConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
            if (!symmetric) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Symmetric constraints currently support two point references about a line or point reference only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            symmetricConstraints.push_back(*symmetric);
            continue;
        }

        if (kind == SketchConstraintKind::Block) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp
            // ::SketchObject::getBlockedState(), for "cstr->Type == Block" sets
            // "blockedstate = true"; /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp
            // ::Sketch::addConstraint() handles "Sketcher::Block" separately while adding
            // geometry. cad-core has no solver parameters to freeze, so it accepts valid
            // whole-geometry Block declarations without changing the current shape.
            const auto block = readBlockConstraintRef(
                constraint, segments, points, circles, ellipses, arcs, ellipseArcs, bsplines);
            if (!block) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Block constraints currently support whole sketch geometry references only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            blockConstraints.push_back(*block);
            continue;
        }

        if (kind == SketchConstraintKind::Equal) {
            const auto equal = readEqualConstraintRef(constraint);
            if (!equal) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Equal constraints currently support whole line, circle and arc geometry pairs only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            equalConstraints.push_back(*equal);
            continue;
        }

        if (kind == SketchConstraintKind::Angle) {
            if (constraint.contains("FirstPos") || constraint.contains("SecondPos") || constraint.contains("Third")
                || constraint.contains("ThirdPos")) {
                const auto angle =
                    readPointwiseAngleConstraintRef(constraint, segments, points, circles, ellipses, arcs, ellipseArcs);
                if (!angle) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_property",
                                           "Angle constraints currently require a Value/Datum and support whole line pairs or point-wise curve references only",
                                           object.name,
                                           "Constraints");
                    return std::nullopt;
                }
                pointwiseAngleConstraints.push_back(*angle);
                continue;
            }

            const auto angle = readAngleConstraintRef(constraint, segments);
            if (!angle) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Angle constraints currently require a Value/Datum and support whole line pairs or point-wise curve references only",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            angleConstraints.push_back(*angle);
            continue;
        }

        if (kind == SketchConstraintKind::Distance || kind == SketchConstraintKind::DistanceX
            || kind == SketchConstraintKind::DistanceY || kind == SketchConstraintKind::Radius
            || kind == SketchConstraintKind::Diameter) {
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
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Distance/Radius/Diameter constraints currently require a Value/Datum and a whole geometry, line-end pair, or fixed line-end coordinate reference",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            dimensionConstraints.push_back(*dimension);
            continue;
        }

        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Only Sketcher Coincident and already-satisfied Horizontal/Vertical/Parallel/Tangent/Perpendicular/PointOnObject/Symmetric/Block/Angle/Distance/Radius/Diameter/Equal constraints are applied in the current P5 subset",
                               object.name,
                               "Constraints");
        return std::nullopt;
    }

    applied.block = blockConstraints.size();

    if (endpoints.parent.empty() && !orientationConstraints.empty()) {
        for (const auto& orientation : orientationConstraints) {
            if (!orientationConstraintSatisfied(orientation, segments)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Horizontal/Vertical constraint requires solver movement in the current P5 subset",
                                       object.name,
                                       "Constraints");
                return std::nullopt;
            }
            ++applied.orientation;
        }
        return applied;
    }

    std::vector<std::optional<gp_Pnt>> mergedPoints(segments.size() * 2U);
    for (std::size_t index = 0; index < segments.size(); ++index) {
        for (bool start : {true, false}) {
            const EndpointRef endpoint{index, start};
            const std::size_t root = endpoints.find(endpointId(endpoint));
            if (!mergedPoints[root]) {
                mergedPoints[root] = endpointPoint(segments[index], start);
            }
        }
    }
    for (std::size_t index = 0; index < segments.size(); ++index) {
        for (bool start : {true, false}) {
            const EndpointRef endpoint{index, start};
            const std::size_t root = endpoints.find(endpointId(endpoint));
            if (mergedPoints[root]) {
                endpointPoint(segments[index], start) = *mergedPoints[root];
            }
        }
    }

    for (const auto& orientation : orientationConstraints) {
        if (!orientationConstraintSatisfied(orientation, segments)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Horizontal/Vertical constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.orientation;
    }

    for (const auto& dimension : dimensionConstraints) {
        if (!dimensionConstraintSatisfied(dimension, segments, circles, arcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Distance/Radius/Diameter constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.dimension;
    }

    for (const auto& relation : linePairConstraints) {
        if (!linePairConstraintSatisfied(relation, segments)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Parallel/Perpendicular constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& tangent : tangentConstraints) {
        if (!tangentConstraintSatisfied(tangent, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Tangent constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
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
                perpendicular, false, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Perpendicular constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& perpendicular : perpendicularPointLineConstraints) {
        if (!perpendicularPointLineConstraintSatisfied(
                perpendicular, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Perpendicular point-point-line constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& perpendicular : perpendicularMidpointLineConstraints) {
        if (!perpendicularMidpointLineConstraintSatisfied(perpendicular, segments, circles, arcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Perpendicular line-circle/arc midpoint constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& pointOnObject : pointOnObjectConstraints) {
        if (!pointOnObjectConstraintSatisfied(pointOnObject, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "PointOnObject constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& symmetric : symmetricConstraints) {
        if (!symmetricConstraintSatisfied(symmetric, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Symmetric constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& angle : angleConstraints) {
        if (!angleConstraintSatisfied(angle, segments)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Angle constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& angle : pointwiseAngleConstraints) {
        if (!pointwiseAngleConstraintSatisfied(angle, segments, points, circles, ellipses, arcs, ellipseArcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Angle constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    for (const auto& equal : equalConstraints) {
        if (!equalConstraintSatisfied(equal, segments, circles, arcs)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Equal constraint requires solver movement in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }
        ++applied.relation;
    }

    return applied;
}

std::vector<SketchSegment> profileSegments(const std::vector<SketchSegment>& segments)
{
    std::vector<SketchSegment> profile;
    for (const auto& segment : segments) {
        if (!segment.construction) {
            profile.push_back(segment);
        }
    }
    return profile;
}

std::vector<SketchPoint> profilePoints(const std::vector<SketchPoint>& points)
{
    std::vector<SketchPoint> profile;
    for (const auto& point : points) {
        if (!point.construction) {
            profile.push_back(point);
        }
    }
    return profile;
}

std::vector<SketchArc> profileArcs(const std::vector<SketchArc>& arcs)
{
    std::vector<SketchArc> profile;
    for (const auto& arc : arcs) {
        if (!arc.construction) {
            profile.push_back(arc);
        }
    }
    return profile;
}

std::vector<SketchEllipseArc> profileEllipseArcs(const std::vector<SketchEllipseArc>& arcs)
{
    std::vector<SketchEllipseArc> profile;
    for (const auto& arc : arcs) {
        if (!arc.construction) {
            profile.push_back(arc);
        }
    }
    return profile;
}

std::vector<SketchBSpline> profileBSplines(const std::vector<SketchBSpline>& bsplines)
{
    std::vector<SketchBSpline> profile;
    for (const auto& bspline : bsplines) {
        if (!bspline.construction) {
            profile.push_back(bspline);
        }
    }
    return profile;
}

std::vector<SketchProfileEdge> profileEdges(const std::vector<SketchSegment>& segments,
                                            const std::vector<SketchArc>& arcs,
                                            const std::vector<SketchEllipseArc>& ellipseArcs,
                                            const std::vector<SketchBSpline>& bsplines)
{
    std::vector<SketchProfileEdge> edges;
    for (const auto& segment : segments) {
        edges.push_back(SketchProfileEdge{SketchProfileEdgeKind::Line, segment.start, segment.end});
    }
    for (const auto& arc : arcs) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // SketchArcOfCircle::getPoint() exposes start/end/mid; cad-core keeps the same endpoint
        // semantics for profile connectivity while deferring full solver support.
        edges.push_back(SketchProfileEdge{SketchProfileEdgeKind::ArcOfCircle,
                                          pointAtAngle(arc.center, arc.radius, arc.startAngle),
                                          pointAtAngle(arc.center, arc.radius, arc.endAngle),
                                          arc.center,
                                          arc.radius,
                                          0.0,
                                          0.0,
                                          0.0,
                                          arc.startAngle,
                                          arc.endAngle});
    }
    for (const auto& arc : ellipseArcs) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // SketchArcOfEllipse::getPoint() exposes start/end/mid with emulateCCW=true.
        // cad-core uses the same start/end parameters for profile connectivity.
        edges.push_back(SketchProfileEdge{SketchProfileEdgeKind::ArcOfEllipse,
                                          pointAtEllipseAngle(arc.center, arc.majorRadius, arc.minorRadius, arc.angle, arc.startAngle),
                                          pointAtEllipseAngle(arc.center, arc.majorRadius, arc.minorRadius, arc.angle, arc.endAngle),
                                          arc.center,
                                          0.0,
                                          arc.majorRadius,
                                          arc.minorRadius,
                                          arc.angle,
                                          arc.startAngle,
                                          arc.endAngle});
    }
    for (const auto& bspline : bsplines) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // ::SketchBSplineCurve::getPoint() exposes start/end for the same wire connectivity
        // role as line and arc profile geometry.
        if (bspline.poles.size() < 2U) {
            continue;
        }
        edges.push_back(SketchProfileEdge{SketchProfileEdgeKind::BSpline,
                                          bspline.poles.front(),
                                          bspline.poles.back(),
                                          gp_Pnt{},
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          bspline.degree,
                                          bspline.poles});
    }
    return edges;
}

std::optional<Handle(Geom_BSplineCurve)> makeBSplineCurve(int degree, const std::vector<gp_Pnt>& poles)
{
    if (degree < 1 || poles.size() < static_cast<std::size_t>(degree + 1)) {
        return std::nullopt;
    }

    const int poleCount = static_cast<int>(poles.size());
    const int knotCount = poleCount - degree + 1;
    if (knotCount < 2) {
        return std::nullopt;
    }

    TColgp_Array1OfPnt poleArray(1, poleCount);
    for (int index = 1; index <= poleCount; ++index) {
        poleArray.SetValue(index, poles[static_cast<std::size_t>(index - 1)]);
    }

    TColStd_Array1OfReal knotArray(1, knotCount);
    TColStd_Array1OfInteger multiplicities(1, knotCount);
    for (int index = 1; index <= knotCount; ++index) {
        const double parameter = knotCount == 1 ? 0.0
                                                : static_cast<double>(index - 1) / static_cast<double>(knotCount - 1);
        knotArray.SetValue(index, parameter);
        multiplicities.SetValue(index, 1);
    }
    multiplicities.SetValue(1, degree + 1);
    multiplicities.SetValue(knotCount, degree + 1);

    try {
        return Handle(Geom_BSplineCurve)(new Geom_BSplineCurve(poleArray,
                                                               knotArray,
                                                               multiplicities,
                                                               degree,
                                                               Standard_False));
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::optional<TopoDS_Edge> makeProfileEdge(const SketchProfileEdge& edge, bool reversed)
{
    BRepBuilderAPI_MakeEdge edgeBuilder;
    if (edge.kind == SketchProfileEdgeKind::Line) {
        edgeBuilder = BRepBuilderAPI_MakeEdge(edge.start, edge.end);
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfCircle) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomArcOfCircle::Restore() rebuilds from "Radius", "StartAngle" and "EndAngle".
        edgeBuilder = BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(edge.center, gp_Dir(0, 0, 1)), edge.radius),
                                              edge.startAngle,
                                              edge.endAngle);
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfEllipse) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomArcOfEllipse::Restore() rebuilds from "MajorRadius", "MinorRadius",
        // "AngleXU", "StartAngle" and "EndAngle".
        edgeBuilder = BRepBuilderAPI_MakeEdge(gp_Elips(ellipseAxis(edge.center, edge.angle), edge.majorRadius, edge.minorRadius),
                                              edge.startAngle,
                                              edge.endAngle);
    }
    else {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomBSplineCurve stores "Poles", "Knots", "Multiplicity" and "Degree"; this P5
        // subset rebuilds a non-periodic clamped curve from fixture poles and degree.
        const auto curve = makeBSplineCurve(edge.degree, edge.poles);
        if (!curve) {
            return std::nullopt;
        }
        edgeBuilder = BRepBuilderAPI_MakeEdge(*curve);
    }
    if (!edgeBuilder.IsDone()) {
        return std::nullopt;
    }
    const TopoDS_Edge built = edgeBuilder.Edge();
    return reversed ? TopoDS::Edge(built.Reversed()) : built;
}

bool addConnectedWire(const std::vector<SketchProfileEdge>& edges,
                      BRepBuilderAPI_MakeWire& wireBuilder,
                      std::optional<gp_Pnt>& firstStart,
                      std::optional<gp_Pnt>& lastEnd,
                      bool requireClosed)
{
    if (edges.empty()) {
        return false;
    }

    std::vector<bool> used(edges.size(), false);
    firstStart = edges.front().start;
    gp_Pnt currentEnd = edges.front().end;
    const auto firstEdge = makeProfileEdge(edges.front(), false);
    if (!firstEdge) {
        return false;
    }
    wireBuilder.Add(*firstEdge);
    used[0] = true;

    for (std::size_t usedCount = 1; usedCount < edges.size(); ++usedCount) {
        bool found = false;
        for (std::size_t index = 1; index < edges.size(); ++index) {
            if (used[index]) {
                continue;
            }
            if (samePoint(edges[index].start, currentEnd)) {
                const auto nextEdge = makeProfileEdge(edges[index], false);
                if (!nextEdge) {
                    return false;
                }
                wireBuilder.Add(*nextEdge);
                currentEnd = edges[index].end;
                used[index] = true;
                found = true;
                break;
            }
            if (samePoint(edges[index].end, currentEnd)) {
                const auto nextEdge = makeProfileEdge(edges[index], true);
                if (!nextEdge) {
                    return false;
                }
                wireBuilder.Add(*nextEdge);
                currentEnd = edges[index].start;
                used[index] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    lastEnd = currentEnd;
    return wireBuilder.IsDone() && (!requireClosed || samePoint(*firstStart, *lastEnd));
}

std::optional<std::vector<TopoDS_Wire>> makeClosedWiresFromEdges(const std::vector<SketchProfileEdge>& edges)
{
    std::vector<TopoDS_Wire> wires;
    std::vector<bool> used(edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < edges.size(); ++startIndex) {
        if (used[startIndex]) {
            continue;
        }

        BRepBuilderAPI_MakeWire wireBuilder;
        const gp_Pnt firstStart = edges[startIndex].start;
        gp_Pnt currentEnd = edges[startIndex].end;
        const auto firstEdge = makeProfileEdge(edges[startIndex], false);
        if (!firstEdge) {
            return std::nullopt;
        }
        wireBuilder.Add(*firstEdge);
        used[startIndex] = true;

        while (!samePoint(firstStart, currentEnd)) {
            bool found = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (used[index]) {
                    continue;
                }
                if (samePoint(edges[index].start, currentEnd)) {
                    const auto nextEdge = makeProfileEdge(edges[index], false);
                    if (!nextEdge) {
                        return std::nullopt;
                    }
                    wireBuilder.Add(*nextEdge);
                    currentEnd = edges[index].end;
                    used[index] = true;
                    found = true;
                    break;
                }
                if (samePoint(edges[index].end, currentEnd)) {
                    const auto nextEdge = makeProfileEdge(edges[index], true);
                    if (!nextEdge) {
                        return std::nullopt;
                    }
                    wireBuilder.Add(*nextEdge);
                    currentEnd = edges[index].start;
                    used[index] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return std::nullopt;
            }
        }

        if (!wireBuilder.IsDone()) {
            return std::nullopt;
        }
        wires.push_back(wireBuilder.Wire());
    }

    return wires;
}

TopoDS_Shape compoundOrSingleShape(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.empty()) {
        return TopoDS_Shape{};
    }
    if (shapes.size() == 1U) {
        return shapes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

std::optional<TopoDS_Wire> makeWireFromCircle(const SketchCircle& circle)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    if (!addCircleWire(circle, wireBuilder)) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<TopoDS_Wire> makeWireFromEllipse(const SketchEllipse& ellipse)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    if (!addEllipseWire(ellipse, wireBuilder)) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<TopoDS_Shape> buildRawSketchShape(const document::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const std::vector<SketchProfileEdge>& edges,
                                                const std::vector<SketchPoint>& points,
                                                const std::vector<SketchCircle>& circles,
                                                const std::vector<SketchEllipse>& ellipses)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::buildShape(),
    // "GeometryFacade::getConstruction(geo)" is skipped, then raw edges are collected into
    // makeElementWires() before PartDesign later asks ProfileBased to make a face.
    std::vector<TopoDS_Shape> shapes;
    if (!edges.empty()) {
        if (const auto closedWires = makeClosedWiresFromEdges(edges)) {
            shapes.insert(shapes.end(), closedWires->begin(), closedWires->end());
        }
        else {
            BRepBuilderAPI_MakeWire wireBuilder;
            std::optional<gp_Pnt> firstStart;
            std::optional<gp_Pnt> lastEnd;
            if (!addConnectedWire(edges, wireBuilder, firstStart, lastEnd, false)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "OCCT could not build raw Sketch Shape wire",
                                       object.name,
                                       "Geometry");
                return std::nullopt;
            }
            shapes.push_back(wireBuilder.Wire());
        }
    }
    for (const auto& point : points) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomPoint::toShape(), returns "BRepBuilderAPI_MakeVertex(myPoint->Pnt())".
        BRepBuilderAPI_MakeVertex vertexBuilder(point.point);
        if (!vertexBuilder.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build raw Sketch Shape point vertex",
                                   object.name,
                                   "Geometry");
            return std::nullopt;
        }
        shapes.push_back(vertexBuilder.Vertex());
    }
    for (const auto& circle : circles) {
        const auto wire = makeWireFromCircle(circle);
        if (!wire) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build raw Sketch Shape circle wire",
                                   object.name,
                                   "Geometry");
            return std::nullopt;
        }
        shapes.push_back(*wire);
    }
    for (const auto& ellipse : ellipses) {
        const auto wire = makeWireFromEllipse(ellipse);
        if (!wire) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build raw Sketch Shape ellipse wire",
                                   object.name,
                                   "Geometry");
            return std::nullopt;
        }
        shapes.push_back(*wire);
    }

    return compoundOrSingleShape(shapes);
}

std::optional<TopoDS_Shape> buildOptionalProfileFace(const std::vector<SketchProfileEdge>& edges,
                                                     const std::vector<SketchCircle>& circles,
                                                     const std::vector<SketchEllipse>& ellipses)
{
    std::vector<TopoDS_Wire> wires;
    if (!edges.empty()) {
        auto edgeWires = makeClosedWiresFromEdges(edges);
        if (!edgeWires) {
            return std::nullopt;
        }
        wires.insert(wires.end(), edgeWires->begin(), edgeWires->end());
    }
    for (const auto& circle : circles) {
        const auto wire = makeWireFromCircle(circle);
        if (!wire) {
            return std::nullopt;
        }
        wires.push_back(*wire);
    }
    for (const auto& ellipse : ellipses) {
        const auto wire = makeWireFromEllipse(ellipse);
        if (!wire) {
            return std::nullopt;
        }
        wires.push_back(*wire);
    }

    return geometry::makeFaceWithHolesFromClosedWires(wires);
}

std::size_t countSubshapesOfKind(const nlohmann::json& subshapes, const std::string& kind)
{
    std::size_t count = 0;
    for (const auto& item : subshapes.items()) {
        const nlohmann::json& value = item.value();
        if (value.is_object() && value.value("kind", std::string{}) == kind) {
            ++count;
        }
    }
    return count;
}

std::string profileShapeLabel(const std::optional<TopoDS_Shape>& profileShape)
{
    if (!profileShape) {
        return "none";
    }
    if (profileShape->ShapeType() == TopAbs_FACE) {
        return "occt_face";
    }
    if (profileShape->ShapeType() == TopAbs_COMPOUND) {
        return "occt_compound";
    }
    return "occt_profile_shape";
}

std::optional<SketchSegment> projectExternalLineEdge(const TopoDS_Edge& edge,
                                                     const gp_Trsf& sketchPlacement)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        return std::nullopt;
    }

    double first = curve.FirstParameter();
    if (std::abs(first) > 1E99) {
        first = -10000.0;
    }
    double last = curve.LastParameter();
    if (std::abs(last) > 1E99) {
        last = 10000.0;
    }

    const gp_Pnt start = pointInSketchLocalPlane(curve.Value(first), sketchPlacement);
    const gp_Pnt end = pointInSketchLocalPlane(curve.Value(last), sketchPlacement);
    if (start.SquareDistance(end) < Precision::SquareConfusion()) {
        return std::nullopt;
    }
    return SketchSegment{0U, start, end, true};
}

bool isCollapsedProjectedLineEdge(const TopoDS_Edge& edge, const gp_Trsf& sketchPlacement)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        return false;
    }

    double first = curve.FirstParameter();
    if (std::abs(first) > 1E99) {
        first = -10000.0;
    }
    double last = curve.LastParameter();
    if (std::abs(last) > 1E99) {
        last = 10000.0;
    }

    const gp_Pnt start = pointInSketchLocalPlane(curve.Value(first), sketchPlacement);
    const gp_Pnt end = pointInSketchLocalPlane(curve.Value(last), sketchPlacement);
    return start.SquareDistance(end) < Precision::SquareConfusion();
}

bool isFullPeriodicEdge(const BRepAdaptor_Curve& curve)
{
    constexpr double twoPi = 6.28318530717958647692;
    return std::abs(curve.LastParameter() - curve.FirstParameter() - twoPi) < Precision::PConfusion()
        || curve.Value(curve.FirstParameter()).SquareDistance(curve.Value(curve.LastParameter())) < Precision::SquareConfusion();
}

bool projectExternalCurveEdge(const TopoDS_Edge& edge,
                              const gp_Trsf& sketchPlacement,
                              ExternalGeometryResult& result)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Circle) {
        const gp_Circ circle = curve.Circle();
        const gp_Dir localNormal = directionInSketchLocalPlane(circle.Axis().Direction(), sketchPlacement);
        const gp_Pnt center = pointInSketchLocalPlane(circle.Location(), sketchPlacement);
        if (!localNormal.IsParallel(gp_Dir(0, 0, 1), Precision::Angular())) {
            gp_Vec majorVector(gp_Dir(0, 0, 1));
            majorVector.Cross(gp_Vec(localNormal));
            if (majorVector.Magnitude() <= Precision::Confusion()) {
                return false;
            }
            const gp_Dir majorDirection(majorVector);
            const gp_Pnt start(center.X() - circle.Radius() * majorDirection.X(),
                               center.Y() - circle.Radius() * majorDirection.Y(),
                               0.0);
            const gp_Pnt end(center.X() + circle.Radius() * majorDirection.X(),
                             center.Y() + circle.Radius() * majorDirection.Y(),
                             0.0);

            if (localNormal.IsNormal(gp_Dir(0, 0, 1), Precision::Angular())) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                // processEdge(), for a circle with normal vector in the sketch plane,
                // says "projection is a line".
                result.segments.push_back(SketchSegment{0U, start, end, true});
                return true;
            }

            if (!isFullPeriodicEdge(curve)) {
                return false;
            }
            const double angle = std::atan2(majorDirection.Y(), majorDirection.X());
            const double minorRadius = circle.Radius() * std::abs(localNormal.Dot(gp_Dir(0, 0, 1)));
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge(), for the general non-parallel circle case, projects a full circle
            // to a construction Part::GeomEllipse.
            result.ellipses.push_back(SketchEllipse{0U, center, circle.Radius(), minorRadius, angle, true});
            return true;
        }

        if (isFullPeriodicEdge(curve)) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge() projects a full circle edge parallel to the sketch plane as
            // construction Part::GeomCircle in ExternalGeo.
            result.circles.push_back(SketchCircle{0U, center, circle.Radius(), true});
            return true;
        }

        result.arcs.push_back(SketchArc{0U, center, circle.Radius(), curve.FirstParameter(), curve.LastParameter(), true});
        return true;
    }

    if (curve.GetType() == GeomAbs_Ellipse) {
        const gp_Elips ellipse = curve.Ellipse();
        const gp_Dir localNormal = directionInSketchLocalPlane(ellipse.Axis().Direction(), sketchPlacement);
        if (!localNormal.IsParallel(gp_Dir(0, 0, 1), Precision::Angular())) {
            if (!isFullPeriodicEdge(curve)) {
                return false;
            }

            const gp_Pnt center = pointInSketchLocalPlane(ellipse.Location(), sketchPlacement);
            const auto projected = projectedEllipseFromAxes(center,
                                                            ellipse.XAxis().Direction(),
                                                            ellipse.MajorRadius(),
                                                            ellipse.YAxis().Direction(),
                                                            ellipse.MinorRadius(),
                                                            sketchPlacement);
            if (!projected) {
                return false;
            }
            if (projected->minorRadius <= Precision::Confusion()) {
                const gp_Pnt start(center.X() - projected->majorRadius * std::cos(projected->angle),
                                   center.Y() - projected->majorRadius * std::sin(projected->angle),
                                   0.0);
                const gp_Pnt end(center.X() + projected->majorRadius * std::cos(projected->angle),
                                 center.Y() + projected->majorRadius * std::sin(projected->angle),
                                 0.0);
                result.segments.push_back(SketchSegment{0U, start, end, true});
                return true;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge(), GeomAbs_Ellipse uses a general projected-ellipse construction
            // algorithm from the original major/minor axes.
            result.ellipses.push_back(*projected);
            return true;
        }

        const gp_Pnt center = pointInSketchLocalPlane(ellipse.Location(), sketchPlacement);
        const double angle = angleXUInSketchPlane(ellipse.XAxis().Direction(), ellipse.Axis().Direction(), sketchPlacement);
        if (isFullPeriodicEdge(curve)) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge2() keeps a full ellipse edge as construction Part::GeomEllipse.
            result.ellipses.push_back(SketchEllipse{0U, center, ellipse.MajorRadius(), ellipse.MinorRadius(), angle, true});
            return true;
        }

        result.ellipseArcs.push_back(SketchEllipseArc{0U,
                                                      center,
                                                      ellipse.MajorRadius(),
                                                      ellipse.MinorRadius(),
                                                      angle,
                                                      curve.FirstParameter(),
                                                      curve.LastParameter(),
                                                      true});
        return true;
    }

    return false;
}

bool projectExternalEdgeIntoResult(const TopoDS_Edge& edge,
                                   const gp_Trsf& sketchPlacement,
                                   ExternalGeometryResult& result)
{
    const auto projected = projectExternalLineEdge(edge, sketchPlacement);
    if (projected) {
        result.segments.push_back(*projected);
        return true;
    }
    return projectExternalCurveEdge(edge, sketchPlacement, result);
}

void appendExternalGeometry(ExternalGeometryResult& result, const ExternalGeometryResult& source)
{
    result.segments.insert(result.segments.end(), source.segments.begin(), source.segments.end());
    result.points.insert(result.points.end(), source.points.begin(), source.points.end());
    result.circles.insert(result.circles.end(), source.circles.begin(), source.circles.end());
    result.arcs.insert(result.arcs.end(), source.arcs.begin(), source.arcs.end());
    result.ellipses.insert(result.ellipses.end(), source.ellipses.begin(), source.ellipses.end());
    result.ellipseArcs.insert(result.ellipseArcs.end(), source.ellipseArcs.begin(), source.ellipseArcs.end());
}

bool appendUnifiedNormalFaceLine(const ExternalGeometryResult& boundary, ExternalGeometryResult& result)
{
    if (!boundary.points.empty() || !boundary.circles.empty() || !boundary.arcs.empty()
        || !boundary.ellipses.empty() || !boundary.ellipseArcs.empty() || boundary.segments.empty()) {
        return false;
    }

    gp_Pnt start = boundary.segments.front().start;
    gp_Pnt end = boundary.segments.front().end;
    auto updateExtremes = [&](const gp_Pnt& point) {
        const double currentLength = start.SquareDistance(end);
        if (point.SquareDistance(start) < point.SquareDistance(end)) {
            if (point.SquareDistance(end) > currentLength) {
                start = point;
            }
            return;
        }
        if (point.SquareDistance(start) > currentLength) {
            end = point;
        }
    };

    for (const SketchSegment& segment : boundary.segments) {
        updateExtremes(segment.start);
        updateExtremes(segment.end);
    }
    if (start.SquareDistance(end) < Precision::SquareConfusion()) {
        return false;
    }
    result.segments.push_back(SketchSegment{0U, start, end, true});
    return true;
}

bool projectExternalFaceBoundary(const TopoDS_Face& face,
                                 const gp_Trsf& sketchPlacement,
                                 ExternalGeometryResult& result)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        return false;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::processFace(), for a planar face, says "Extract all edges from the face" and then
    // "Process each edge" through processEdge(). This is the planar-boundary subset; the HLR
    // projection path for non-planar faces remains a later Face/ExternalGeometry task.
    ExternalGeometryResult boundary;
    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        if (!projectExternalEdgeIntoResult(edge, sketchPlacement, boundary)) {
            if (isCollapsedProjectedLineEdge(edge, sketchPlacement)) {
                continue;
            }
            return false;
        }
    }
    if (boundary.segments.empty() && boundary.points.empty() && boundary.circles.empty()
        && boundary.arcs.empty() && boundary.ellipses.empty() && boundary.ellipseArcs.empty()) {
        return false;
    }

    const gp_Dir localFaceNormal = directionInSketchLocalPlane(surface.Plane().Axis().Direction(), sketchPlacement);
    if (localFaceNormal.IsNormal(gp_Dir(0, 0, 1), Precision::Angular())) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // ::processFace(), when "The face is normal to the sketch plane", discards the separate
        // edge projections and keeps "a single line that goes from min to max of all the projections".
        return appendUnifiedNormalFaceLine(boundary, result);
    }

    appendExternalGeometry(result, boundary);
    return true;
}

std::vector<ExternalGeometryType> readExternalGeometryTypes(const document::DocumentObject& object,
                                                            std::size_t count)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry(), reads "ExternalTypes.getValues()" and then
    // "Types.resize(Objects.size(), static_cast<long>(ExtType::Projection))".
    std::vector<ExternalGeometryType> types(count, ExternalGeometryType::Projection);
    const auto* value = document::propertyValue(object, "ExternalTypes");
    if (value == nullptr) {
        return types;
    }

    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (!payload->is_array()) {
        return types;
    }

    const std::size_t limit = std::min(count, payload->size());
    for (std::size_t index = 0; index < limit; ++index) {
        if (!payload->at(index).is_number_integer()) {
            continue;
        }
        switch (payload->at(index).get<int>()) {
            case 1:
                types.at(index) = ExternalGeometryType::Intersection;
                break;
            case 2:
                types.at(index) = ExternalGeometryType::Both;
                break;
            default:
                types.at(index) = ExternalGeometryType::Projection;
                break;
        }
    }
    return types;
}

struct ExternalSubshape {
    TopAbs_ShapeEnum kind = TopAbs_SHAPE;
    TopoDS_Shape shape;
    std::string subname;
};

std::optional<ExternalSubshape> resolveSketchInternalSubshape(const document::Link& link,
                                                              const document::DocumentObject& object,
                                                              const runtime::ShapeValue& shapeValue,
                                                              runtime::ComputeContext& context,
                                                              const std::string& subname)
{
    const auto parsed = topo::parseInternalSubshapeName(subname);
    if (!parsed) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getSubObject(),
    // convertInternalName("InternalEdge") resolves the subshape from InternalShape, not Shape.
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ExternalGeometry target " + link.object + " has no InternalShape for " + subname,
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ExternalGeometry projection currently supports InternalEdgeN and InternalVertexN subshapes",
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }

    const auto subshape = topo::subshapeByName(*shapeValue.internalShape, *parsed);
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ExternalGeometry target " + link.object + " has no subshape " + subname,
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }
    return ExternalSubshape{parsed->kind, *subshape, subname};
}

std::vector<ExternalSubshape> wholeShapeExternalSubshapes(const runtime::ShapeValue& shapeValue)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::addExternal(), when the selected shape is not a face but "hasSubShape(TopAbs_FACE)",
    // expands the external reference into each FaceN; otherwise it expands into EdgeN when edges
    // exist. cad-core keeps this as request-local expansion and does not mutate ExternalGeometry.
    std::vector<ExternalSubshape> result;
    if (shapeValue.shape.IsNull()) {
        return result;
    }

    TopAbs_ShapeEnum kind = TopAbs_SHAPE;
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shapeValue.shape, TopAbs_FACE, subshapes);
    if (subshapes.Extent() > 0) {
        kind = TopAbs_FACE;
    }
    else {
        TopExp::MapShapes(shapeValue.shape, TopAbs_EDGE, subshapes);
        if (subshapes.Extent() > 0) {
            kind = TopAbs_EDGE;
        }
    }
    if (kind != TopAbs_FACE && kind != TopAbs_EDGE) {
        return result;
    }

    const std::string prefix = kind == TopAbs_FACE ? "Face" : "Edge";
    result.reserve(static_cast<std::size_t>(subshapes.Extent()));
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        result.push_back(ExternalSubshape{kind, subshapes(index), prefix + std::to_string(index)});
    }
    return result;
}

std::optional<std::vector<ExternalSubshape>> resolveExternalGeometryLink(const document::Link& link,
                                                                         const document::DocumentObject& object,
                                                                         runtime::ComputeContext& context)
{
    const std::string subname = link.subnames.empty() ? std::string{} : link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front() : std::string{};
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "ExternalGeometry target " + link.object + " did not produce a shape",
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }

    std::string currentSubname = subname;
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved = topo::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != subname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                                   object.name,
                                   "ExternalGeometry",
                                   "runtime",
                                   link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    if (!subname.empty()) {
        if (auto internal = resolveSketchInternalSubshape(link, object, shapeIt->second, context, subname)) {
            return std::vector<ExternalSubshape>{*internal};
        }
        if (topo::parseInternalSubshapeName(subname)) {
            return std::nullopt;
        }
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link.subnames.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumLine and builds an edge from its shape.
        return std::vector<ExternalSubshape>{ExternalSubshape{TopAbs_EDGE, shapeIt->second.shape, {}}};
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPoint && link.subnames.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumPoint and builds a vertex from its shape.
        return std::vector<ExternalSubshape>{ExternalSubshape{TopAbs_VERTEX, shapeIt->second.shape, {}}};
    }

    if (link.subnames.empty()) {
        std::vector<ExternalSubshape> expanded = wholeShapeExternalSubshapes(shapeIt->second);
        if (!expanded.empty()) {
            return expanded;
        }
    }

    if (link.subnames.size() != 1U || subname.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ExternalGeometry must reference exactly one FaceN/EdgeN/VertexN subshape or a DatumLine/DatumPoint",
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               subname);
        return std::nullopt;
    }

    const auto parsed = topo::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid ExternalGeometry subshape name " + currentSubname,
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_FACE && parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ExternalGeometry projection currently supports FaceN, EdgeN and VertexN subshapes",
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = topo::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = topo::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ExternalGeometry target " + link.object + " has no subshape " + currentSubname,
                               object.name,
                               "ExternalGeometry",
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    return std::vector<ExternalSubshape>{ExternalSubshape{parsed->kind, *subshape, currentSubname}};
}

bool addExternalGeometryIntersection(const ExternalSubshape& external,
                                     const gp_Trsf& sketchPlacement,
                                     ExternalGeometryResult& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry(), for "intersection", runs
    // "FCBRepAlgoAPI_Section maker(refSubShape, sketchPlane)" and then processes section edges
    // through processEdge(); standalone section vertices are imported as points.
    try {
        BRepAlgoAPI_Section maker(external.shape, sketchPlaneFromPlacement(sketchPlacement), Standard_False);
        maker.Approximation(Standard_True);
        maker.Build();
        if (!maker.IsDone()) {
            return false;
        }
        const TopoDS_Shape sectionShape = maker.Shape();
        if (sectionShape.IsNull()) {
            return false;
        }

        bool added = false;
        TopTools_IndexedMapOfShape edgeVertices;
        for (TopExp_Explorer explorer(sectionShape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            if (!projectExternalEdgeIntoResult(edge, sketchPlacement, result)) {
                return false;
            }
            TopExp::MapShapes(edge, TopAbs_VERTEX, edgeVertices);
            added = true;
        }

        for (TopExp_Explorer explorer(sectionShape, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
            const TopoDS_Vertex vertex = TopoDS::Vertex(explorer.Current());
            if (edgeVertices.Contains(vertex)) {
                continue;
            }
            result.points.push_back(pointInSketchLocalPlane(BRep_Tool::Pnt(vertex), sketchPlacement));
            added = true;
        }
        return added;
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

std::optional<ExternalGeometryResult> rebuildExternalGeometry(const document::DocumentObject& object,
                                                              runtime::ComputeContext& context,
                                                              const gp_Trsf& sketchPlacement)
{
    const auto* externalProperty = document::propertyValue(object, "ExternalGeometry");
    if (externalProperty == nullptr) {
        return ExternalGeometryResult{};
    }
    if (externalProperty->propertyType != "App::PropertyLinkSubList") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "ExternalGeometry must be an App::PropertyLinkSubList",
                               object.name,
                               "ExternalGeometry",
                               "runtime");
        return std::nullopt;
    }

    ExternalGeometryResult result;
    const std::vector<document::Link> links = document::readLinks(object, "ExternalGeometry");
    const std::vector<ExternalGeometryType> externalTypes = readExternalGeometryTypes(object, links.size());
    for (std::size_t index = 0; index < links.size(); ++index) {
        const auto& link = links.at(index);
        const ExternalGeometryType externalType = externalTypes.at(index);
        const bool projection =
            externalType == ExternalGeometryType::Projection || externalType == ExternalGeometryType::Both;
        const bool intersection =
            externalType == ExternalGeometryType::Intersection || externalType == ExternalGeometryType::Both;
        const auto externals = resolveExternalGeometryLink(link, object, context);
        if (!externals) {
            return std::nullopt;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() reads "ExternalGeometry" links and fills transient
        // "ExternalGeo" with projected construction geometry before Constraints.acceptGeometry().
        for (const ExternalSubshape& external : *externals) {
            if (projection && external.kind == TopAbs_VERTEX) {
                result.points.push_back(pointInSketchLocalPlane(BRep_Tool::Pnt(TopoDS::Vertex(external.shape)), sketchPlacement));
            }

            if (projection && external.kind == TopAbs_FACE) {
                if (!projectExternalFaceBoundary(TopoDS::Face(external.shape), sketchPlacement, result)) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_geometry",
                                           "ExternalGeometry currently projects planar face boundary edges, line edges, circle edges and ellipse edges",
                                           object.name,
                                           "ExternalGeometry",
                                           "runtime",
                                           link.object,
                                           external.subname);
                    return std::nullopt;
                }
            }

            if (projection && external.kind == TopAbs_EDGE
                && !projectExternalEdgeIntoResult(TopoDS::Edge(external.shape), sketchPlacement, result)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "ExternalGeometry currently projects planar face boundary edges, line edges, circle edges and ellipse edges",
                                       object.name,
                                       "ExternalGeometry",
                                       "runtime",
                                       link.object,
                                       external.subname);
                return std::nullopt;
            }

            if (intersection && !addExternalGeometryIntersection(external, sketchPlacement, result)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_geometry",
                                       "ExternalGeometry could not intersect target with the sketch plane",
                                       object.name,
                                       "ExternalGeometry",
                                       "runtime",
                                       link.object,
                                       external.subname);
                return std::nullopt;
            }
        }
    }
    return result;
}

std::optional<document::Link> readSupportLink(const document::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Part2DObject.h
    // says a 2D object "has a link to a supporting Face"; the property is provided by
    // Part::AttachExtension as AttachmentSupport. cad-core also accepts "Support" as
    // the fixture-facing alias used by ShapeBinder and older exported graph payloads.
    auto support = document::readLink(object, "AttachmentSupport");
    if (support) {
        return support;
    }
    return document::readLink(object, "Support");
}

std::optional<gp_Trsf> supportPlacement(const document::DocumentObject& object,
                                        runtime::ComputeContext& context)
{
    const auto support = readSupportLink(object);
    if (!support) {
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(support->object);
    if (placementIt == context.globalPlacements.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Sketch support " + support->object + " did not produce a placement",
                               object.name,
                               support->property.empty() ? "Support" : support->property,
                               "runtime",
                               support->object);
        return std::nullopt;
    }
    return placementIt->second;
}

}  // namespace

void executeSketchObject(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/Sketcher/App/SketchObject.cpp
    if (!rejectUnsupportedProperties(
            object, context, {"Geometry", "Constraints", "Support", "AttachmentSupport", "MapMode", "ExternalGeometry", "ExternalTypes"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Geometry") || !object.properties.at("Geometry").is_array()) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Sketch Geometry must be a list", object.name, "Geometry");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto& geometry = object.properties.at("Geometry");
    SketchGeometrySet parsed;
    if (!parseSketchGeometry(geometry, object, context, parsed)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto constraintsIt = object.properties.find("Constraints");
    const nlohmann::json emptyConstraints;
    const nlohmann::json& constraints = constraintsIt == object.properties.end() ? emptyConstraints : constraintsIt.value();
    const auto appliedConstraints =
        applySketchConstraints(constraints,
                               object,
                               context,
                               parsed.segments,
                               parsed.points,
                               parsed.circles,
                               parsed.ellipses,
                               parsed.arcs,
                               parsed.ellipseArcs,
                               parsed.bsplines);
    if (!appliedConstraints) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    gp_Trsf placement;
    bool hasPlacement = false;
    if (const auto support = supportPlacement(object, context)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::ProfileBased::positionByPrevious() falls back to sketch->AttachmentSupport Placement
        // when there is no previous base feature.
        placement = *support;
        hasPlacement = true;
    }
    if (document::propertyValue(object, "Placement") != nullptr) {
        const auto localPlacement = document::readPlacement(object, "Placement");
        if (!localPlacement) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_placement",
                                   "Sketch Placement must be an App::PropertyPlacement",
                                   object.name,
                                   "Placement",
                                   "runtime");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const gp_Trsf localTransform = geometry::placementFromComponents(localPlacement->base, localPlacement->rotation);
        placement = hasPlacement ? placement * localTransform : localTransform;
        hasPlacement = true;
    }

    const auto externalGeometry = rebuildExternalGeometry(object, context, hasPlacement ? placement : gp_Trsf{});
    if (!externalGeometry) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<SketchSegment> profile = profileSegments(parsed.segments);
    const std::vector<SketchPoint> points = profilePoints(parsed.points);
    const std::vector<SketchArc> arcs = profileArcs(parsed.arcs);
    const std::vector<SketchEllipseArc> ellipseArcs = profileEllipseArcs(parsed.ellipseArcs);
    const std::vector<SketchBSpline> bsplines = profileBSplines(parsed.bsplines);
    const std::vector<SketchProfileEdge> edges = profileEdges(profile, arcs, ellipseArcs, bsplines);
    const std::vector<SketchCircle> circles = profileCircles(parsed.circles);
    const std::vector<SketchEllipse> ellipses = profileEllipses(parsed.ellipses);
    auto rawShape = buildRawSketchShape(object, context, edges, points, circles, ellipses);
    if (!rawShape) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<TopoDS_Shape> profileShape;
    std::optional<TopoDS_Shape> internalShape;
    if (auto profileFace = buildOptionalProfileFace(edges, circles, ellipses)) {
        profileShape = *profileFace;
        // This is only the first P5 InternalShape baseline. FreeCAD's complete path is
        // SketchObject::buildInternals() -> FaceMakerBuildFace + WireJoiner::getOpenWires().
        // Open wires and InternalEdge/InternalVertex mapping stay explicit P5/P6 work.
        internalShape = *profileFace;
    }

    if (hasPlacement) {
        if (!rawShape->IsNull()) {
            rawShape = geometry::transformShape(*rawShape, placement);
        }
        if (profileShape) {
            profileShape = geometry::transformShape(*profileShape, placement);
        }
        if (internalShape) {
            internalShape = geometry::transformShape(*internalShape, placement);
        }
    }

    runtime::ShapeValue shapeValue{runtime::ShapeValue::Kind::Sketch, *rawShape};
    shapeValue.profileShape = profileShape;
    shapeValue.internalShape = internalShape;
    context.shapes[object.name] = shapeValue;
    if (!rawShape->IsNull()) {
        nlohmann::json subshapes = topo::subshapeMapForShape(*rawShape);
        if (internalShape && !internalShape->IsNull()) {
            const nlohmann::json internalSubshapes = topo::subshapeMapForShape(*internalShape, "Internal");
            for (const auto& item : internalSubshapes.items()) {
                subshapes[item.key()] = item.value();
            }
        }
        context.subshapes[object.name] = subshapes;
    }

    const std::size_t rawEdgeCount = edges.size() + circles.size() + ellipses.size();
    const std::size_t profileEdgeCount = rawEdgeCount;
    const std::size_t rawPointCount = points.size();
    const nlohmann::json internalSubshapes = internalShape ? topo::subshapeMapForShape(*internalShape, "Internal") : nlohmann::json::object();
    const std::size_t internalFaceCount = countSubshapesOfKind(internalSubshapes, "face");
    const std::size_t internalEdgeCount = countSubshapesOfKind(internalSubshapes, "edge");
    const std::size_t internalVertexCount = countSubshapesOfKind(internalSubshapes, "vertex");
    const nlohmann::json internalElementMap =
        internalShape ? topo::internalElementMapForSketch(*rawShape, *internalShape) : nlohmann::json::object();
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", rawShape->IsNull() ? "empty" : "occt_sketch_shape"},
        {"profile", profileShapeLabel(profileShape)},
        {"profile_ready", profileShape.has_value()},
        {"edge_count", profileEdgeCount},
        {"raw_edge_count", rawEdgeCount},
        {"raw_point_count", rawPointCount},
        {"internal_shape", internalShape ? "occt_internal_shape" : "none"},
        {"internal_face_count", internalFaceCount},
        {"internal_edge_count", internalEdgeCount},
        {"internal_vertex_count", internalVertexCount},
        {"internal_element_map", internalElementMap},
        {"coincident_constraints_applied", appliedConstraints->coincident},
        {"orientation_constraints_applied", appliedConstraints->orientation},
        {"dimension_constraints_applied", appliedConstraints->dimension},
        {"relation_constraints_applied", appliedConstraints->relation},
        {"block_constraints_applied", appliedConstraints->block},
        {"external_geometry_count",
         externalGeometry->segments.size() + externalGeometry->points.size() + externalGeometry->circles.size()
             + externalGeometry->arcs.size() + externalGeometry->ellipses.size() + externalGeometry->ellipseArcs.size()},
        {"external_point_count", externalGeometry->points.size()},
        {"external_curve_count",
         externalGeometry->circles.size() + externalGeometry->arcs.size() + externalGeometry->ellipses.size()
             + externalGeometry->ellipseArcs.size()},
    };
}

}  // namespace cad_core::features
