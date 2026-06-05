#include "sketch_object_geometry.h"

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/compute_context.h"

#include <cmath>
#include <optional>
#include <utility>

namespace cad_core::sketcher
{

namespace
{

double readNumber2(const nlohmann::json& value, std::size_t index, bool& ok)
{
    if (!value.is_array() || value.size() != 2 || !value.at(index).is_number()) {
        ok = false;
        return 0.0;
    }
    const double number = value.at(index).get<double>();
    if (!std::isfinite(number)) {
        ok = false;
        return 0.0;
    }
    return number;
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
        return std::isfinite(number) ? std::optional<double> {number} : std::nullopt;
    }
    if (it->is_object() && it->contains("value") && it->at("value").is_number()) {
        const double number = it->at("value").get<double>();
        return std::isfinite(number) ? std::optional<double> {number} : std::nullopt;
    }
    return std::nullopt;
}

} // namespace

gp_Pnt pointAtAngle(const gp_Pnt& center, double radius, double angle)
{
    return gp_Pnt(
        center.X() + radius * std::cos(angle),
        center.Y() + radius * std::sin(angle),
        center.Z()
    );
}

gp_Pnt pointAtEllipseAngle(
    const gp_Pnt& center,
    double majorRadius,
    double minorRadius,
    double angleXU,
    double parameter
)
{
    const double cosAxis = std::cos(angleXU);
    const double sinAxis = std::sin(angleXU);
    const double localX = majorRadius * std::cos(parameter);
    const double localY = minorRadius * std::sin(parameter);
    return gp_Pnt(
        center.X() + localX * cosAxis - localY * sinAxis,
        center.Y() + localX * sinAxis + localY * cosAxis,
        center.Z()
    );
}

bool parseSketchGeometry(
    const nlohmann::json& geometry,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    SketchGeometrySet& parsed,
    const std::string& propertyName
)
{
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        const auto& item = geometry.at(index);
        if (!item.is_object() || !item.contains("kind") || !item.at("kind").is_string()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_geometry",
                "Sketch Geometry item must declare a supported kind",
                object.name,
                propertyName
            );
            return false;
        }

        const std::string kind = item.at("kind").get<std::string>();
        if (kind == "Point" || kind == "GeomPoint") {
            bool ok = true;
            const double px = readNumber2(item.at("point"), 0, ok);
            const double py = readNumber2(item.at("point"), 1, ok);
            if (!ok) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "Point must provide a two-number point",
                    object.name,
                    propertyName
                );
                return false;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::SketchObject::buildShape(), for "Part::GeomPoint" calls geo->toShape()
            // and exposes it as a Vertex in the Sketch Shape compound.
            parsed.points.push_back(
                SketchPoint {index, gp_Pnt(px, py, 0.0), readBoolField(item, "construction", false)}
            );
            continue;
        }

        if (kind == "LineSegment") {
            bool ok = true;
            const double sx = readNumber2(item.at("start"), 0, ok);
            const double sy = readNumber2(item.at("start"), 1, ok);
            const double ex = readNumber2(item.at("end"), 0, ok);
            const double ey = readNumber2(item.at("end"), 1, ok);
            if (!ok) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "LineSegment start/end must be two numbers",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.segments.push_back(
                SketchSegment {
                    index,
                    gp_Pnt(sx, sy, 0.0),
                    gp_Pnt(ex, ey, 0.0),
                    readBoolField(item, "construction", false)
                }
            );
            continue;
        }

        if (kind == "Circle") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto radius = readNumberField(item, "radius");
            if (!ok || !radius || *radius <= 0.0) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "Circle center must be two numbers and radius must be positive",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.circles.push_back(
                SketchCircle {index, gp_Pnt(cx, cy, 0.0), *radius, readBoolField(item, "construction", false)}
            );
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
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "Ellipse center, majorRadius and minorRadius are required",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.ellipses.push_back(
                SketchEllipse {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *majorRadius,
                    *minorRadius,
                    angle,
                    readBoolField(item, "construction", false)
                }
            );
            continue;
        }

        if (kind == "ArcOfCircle") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto radius = readNumberField(item, "radius");
            const auto startAngle = readNumberField(item, "startAngle");
            const auto endAngle = readNumberField(item, "endAngle");
            if (!ok || !radius || !startAngle || !endAngle || *radius <= 0.0
                || *startAngle == *endAngle) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ArcOfCircle center, positive radius, startAngle and endAngle are required",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.arcs.push_back(
                SketchArc {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *radius,
                    *startAngle,
                    *endAngle,
                    readBoolField(item, "construction", false)
                }
            );
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
            if (!ok || !majorRadius || !minorRadius || !startAngle || !endAngle
                || *majorRadius <= 0.0 || *minorRadius <= 0.0 || *majorRadius < *minorRadius
                || *startAngle == *endAngle || !std::isfinite(angle)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ArcOfEllipse center, radii, startAngle and endAngle are required",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.ellipseArcs.push_back(
                SketchEllipseArc {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *majorRadius,
                    *minorRadius,
                    angle,
                    *startAngle,
                    *endAngle,
                    readBoolField(item, "construction", false)
                }
            );
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
            const std::size_t rawPoleCount = polesIt != item.end() && polesIt->is_array()
                ? polesIt->size()
                : 0U;
            if (!degree || *degree < 1 || poles.size() != rawPoleCount
                || poles.size() < static_cast<std::size_t>(*degree + 1)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "BSpline requires a positive degree and at least degree + 1 two-number poles",
                    object.name,
                    propertyName
                );
                return false;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
            // ::SketchBSplineCurve::getPoint(), returns "bsp->getStartPoint()" and
            // "bsp->getEndPoint()" for profile connectivity.
            parsed.bsplines.push_back(
                SketchBSpline {index, *degree, std::move(poles), readBoolField(item, "construction", false)}
            );
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // registers geometry families independently. cad-core keeps unsupported families explicit
        // until their profile and internal-shape paths are migrated.
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_geometry",
            "Sketch Geometry kind " + kind + " is not supported in the current P5 subset",
            object.name,
            propertyName
        );
        return false;
    }
    return true;
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

} // namespace cad_core::sketcher
