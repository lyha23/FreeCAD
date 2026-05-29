#include "cad_core/features/sketch_object.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/face_maker.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/topo/element_map.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
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
#include <optional>
#include <string>
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

struct EndpointRef {
    std::size_t segmentIndex = 0;
    bool start = true;
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

bool isCoincidentConstraint(const nlohmann::json& constraint)
{
    if (const auto type = readStringField(constraint, "Type")) {
        return *type == "Coincident";
    }
    if (const auto type = readIntField(constraint, "Type")) {
        return *type == 1;
    }
    return false;
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

std::optional<std::size_t> applyCoincidentConstraints(const nlohmann::json& constraints,
                                                      const document::DocumentObject& object,
                                                      runtime::ComputeContext& context,
                                                      std::vector<SketchSegment>& segments)
{
    if (constraints.is_null()) {
        return 0U;
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
    std::size_t applied = 0;
    for (const auto& constraint : constraints) {
        if (!constraint.is_object() || !isCoincidentConstraint(constraint)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Only Sketcher Coincident constraints are applied in the current P5 subset",
                                   object.name,
                                   "Constraints");
            return std::nullopt;
        }

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
        ++applied;
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

struct ExternalSubshape {
    TopAbs_ShapeEnum kind = TopAbs_SHAPE;
    TopoDS_Shape shape;
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
    return ExternalSubshape{parsed->kind, *subshape};
}

std::optional<ExternalSubshape> resolveExternalGeometryLink(const document::Link& link,
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
            return internal;
        }
        if (topo::parseInternalSubshapeName(subname)) {
            return std::nullopt;
        }
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link.subnames.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumLine and builds an edge from its shape.
        return ExternalSubshape{TopAbs_EDGE, shapeIt->second.shape};
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPoint && link.subnames.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumPoint and builds a vertex from its shape.
        return ExternalSubshape{TopAbs_VERTEX, shapeIt->second.shape};
    }

    if (link.subnames.size() != 1U || subname.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "ExternalGeometry must reference exactly one EdgeN/VertexN subshape or a DatumLine/DatumPoint",
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
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ExternalGeometry projection currently supports EdgeN and VertexN subshapes",
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
    return ExternalSubshape{parsed->kind, *subshape};
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
    for (const auto& link : links) {
        const auto external = resolveExternalGeometryLink(link, object, context);
        if (!external) {
            return std::nullopt;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() reads "ExternalGeometry" links and fills transient
        // "ExternalGeo" with projected construction geometry before Constraints.acceptGeometry().
        if (external->kind == TopAbs_VERTEX) {
            result.points.push_back(pointInSketchLocalPlane(BRep_Tool::Pnt(TopoDS::Vertex(external->shape)), sketchPlacement));
            continue;
        }

        const TopoDS_Edge edge = TopoDS::Edge(external->shape);
        const auto projected = projectExternalLineEdge(edge, sketchPlacement);
        if (projected) {
            result.segments.push_back(*projected);
            continue;
        }
        if (!projectExternalCurveEdge(edge, sketchPlacement, result)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_geometry",
                                   "ExternalGeometry currently projects line, circle and ellipse edges only",
                                   object.name,
                                   "ExternalGeometry",
                                   "runtime",
                                   link.object,
                                   link.subnames.empty() ? std::string{} : link.subnames.front());
            return std::nullopt;
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
    const auto appliedConstraints = applyCoincidentConstraints(constraints, object, context, parsed.segments);
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
        {"coincident_constraints_applied", *appliedConstraints},
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
