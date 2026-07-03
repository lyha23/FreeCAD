#include "sketch_object_geometry.h"

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/compute_context.h"

#include <cmath>
#include <limits>
#include <optional>
#include <set>
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

struct GeometryIdRead
{
    bool present = false;
    std::optional<long> value;
    std::string field;
};

GeometryIdRead readGeometryIdField(const nlohmann::json& value)
{
    for (const std::string& field : {"id", "Id", "geometryId"}) {
        const auto it = value.find(field);
        if (it == value.end()) {
            continue;
        }
        GeometryIdRead result;
        result.present = true;
        result.field = field;
        const nlohmann::json* source = &*it;
        if (it->is_object() && it->contains("value")) {
            source = &it->at("value");
        }
        if (!source->is_number_integer()) {
            return result;
        }
        const long long raw = source->get<long long>();
        if (raw <= 0 || raw > std::numeric_limits<long>::max()) {
            return result;
        }
        result.value = static_cast<long>(raw);
        return result;
    }
    return {};
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

std::optional<gp_Vec> readVector2Field(const nlohmann::json& value)
{
    bool ok = true;
    const double x = readNumber2(value, 0, ok);
    const double y = readNumber2(value, 1, ok);
    if (!ok) {
        return std::nullopt;
    }
    return gp_Vec(x, y, 0.0);
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
    std::set<long> seenGeometryIds;
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
        const GeometryIdRead geometryId = readGeometryIdField(item);
        if (geometryId.present && !geometryId.value) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_geometry_id",
                "Sketch Geometry " + geometryId.field + " must be a positive integer",
                object.name,
                propertyName
            );
            return false;
        }
        if (geometryId.value && seenGeometryIds.count(*geometryId.value) != 0U) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "duplicate_geometry_id",
                "Sketch Geometry id " + std::to_string(*geometryId.value) + " is duplicated",
                object.name,
                propertyName
            );
            return false;
        }
        if (geometryId.value) {
            seenGeometryIds.insert(*geometryId.value);
        }

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
                SketchPoint {
                    index,
                    gp_Pnt(px, py, 0.0),
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
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
                    readBoolField(item, "construction", false),
                    geometryId.value,
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
                SketchCircle {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *radius,
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
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
                    readBoolField(item, "construction", false),
                    geometryId.value,
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
                    readBoolField(item, "construction", false),
                    geometryId.value,
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
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
            );
            continue;
        }

        if (kind == "ArcOfHyperbola" || kind == "Part::GeomArcOfHyperbola") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto majorRadius = readNumberField(item, "majorRadius");
            const auto minorRadius = readNumberField(item, "minorRadius");
            const auto angle = readNumberField(item, "angle").value_or(0.0);
            const auto startAngle = readNumberField(item, "startAngle");
            const auto endAngle = readNumberField(item, "endAngle");
            if (!ok || !majorRadius || !minorRadius || !startAngle || !endAngle
                || *majorRadius <= 0.0 || *minorRadius <= 0.0 || *startAngle == *endAngle
                || !std::isfinite(angle)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ArcOfHyperbola center, radii, startAngle and endAngle are required",
                    object.name,
                    propertyName
                );
                return false;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
            // ::GeomArcOfHyperbola::Restore(), reads
            // "MajorRadius", "MinorRadius", "AngleXU", "StartAngle" and "EndAngle".
            parsed.hyperbolaArcs.push_back(
                SketchHyperbolaArc {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *majorRadius,
                    *minorRadius,
                    angle,
                    *startAngle,
                    *endAngle,
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
            );
            continue;
        }

        if (kind == "ArcOfParabola" || kind == "Part::GeomArcOfParabola") {
            bool ok = true;
            const double cx = readNumber2(item.at("center"), 0, ok);
            const double cy = readNumber2(item.at("center"), 1, ok);
            const auto focal = readNumberField(item, "focal");
            const auto angle = readNumberField(item, "angle").value_or(0.0);
            const auto startAngle = readNumberField(item, "startAngle");
            const auto endAngle = readNumberField(item, "endAngle");
            if (!ok || !focal || !startAngle || !endAngle || *focal <= 0.0
                || *startAngle == *endAngle || !std::isfinite(angle)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ArcOfParabola center, focal, startAngle and endAngle are required",
                    object.name,
                    propertyName
                );
                return false;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
            // ::GeomArcOfParabola::Restore(), reads
            // "Focal", "AngleXU", "StartAngle" and "EndAngle".
            parsed.parabolaArcs.push_back(
                SketchParabolaArc {
                    index,
                    gp_Pnt(cx, cy, 0.0),
                    *focal,
                    angle,
                    *startAngle,
                    *endAngle,
                    readBoolField(item, "construction", false),
                    geometryId.value,
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
                SketchBSpline {
                    index,
                    *degree,
                    std::move(poles),
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
            );
            continue;
        }

        if (kind == "InterpolatedSpline") {
            const auto pointsIt = item.find("points");
            std::vector<gp_Pnt> points;
            if (pointsIt != item.end() && pointsIt->is_array()) {
                points.reserve(pointsIt->size());
                for (const auto& pointValue : *pointsIt) {
                    const auto point = readPoint2Field(pointValue);
                    if (!point) {
                        break;
                    }
                    points.push_back(*point);
                }
            }
            const std::size_t rawPointCount = pointsIt != item.end() && pointsIt->is_array()
                ? pointsIt->size()
                : 0U;
            const bool periodic = readBoolField(item, "periodic", false);
            if (points.size() != rawPointCount || points.size() < 2U
                || (periodic && points.size() < 3U)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "InterpolatedSpline requires at least two two-number points, or three when periodic",
                    object.name,
                    propertyName
                );
                return false;
            }

            std::optional<gp_Vec> startTangent;
            const auto startTangentIt = item.find("startTangent");
            if (startTangentIt != item.end()) {
                startTangent = readVector2Field(*startTangentIt);
                if (!startTangent) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_geometry",
                        "InterpolatedSpline startTangent must be two numbers",
                        object.name,
                        propertyName
                    );
                    return false;
                }
            }

            std::optional<gp_Vec> endTangent;
            const auto endTangentIt = item.find("endTangent");
            if (endTangentIt != item.end()) {
                endTangent = readVector2Field(*endTangentIt);
                if (!endTangent) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_geometry",
                        "InterpolatedSpline endTangent must be two numbers",
                        object.name,
                        propertyName
                    );
                    return false;
                }
            }
            if (startTangent.has_value() != endTangent.has_value()) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "InterpolatedSpline startTangent and endTangent must be provided together",
                    object.name,
                    propertyName
                );
                return false;
            }

            parsed.interpolatedSplines.push_back(
                SketchInterpolatedSpline {
                    index,
                    std::move(points),
                    startTangent,
                    endTangent,
                    periodic,
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
            );
            continue;
        }

        if (kind == "Bezier" || kind == "BezierCurve" || kind == "GeomBezierCurve") {
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
            std::vector<double> weights;
            auto weightsIt = item.find("weights");
            if (weightsIt == item.end()) {
                weightsIt = item.find("Weights");
            }
            if (weightsIt != item.end()) {
                if (!weightsIt->is_array()) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_geometry",
                        "Bezier weights must be an array when provided",
                        object.name,
                        propertyName
                    );
                    return false;
                }
                weights.reserve(weightsIt->size());
                for (const auto& weight : *weightsIt) {
                    if (!weight.is_number()) {
                        break;
                    }
                    const double value = weight.get<double>();
                    if (!std::isfinite(value) || value <= 0.0) {
                        break;
                    }
                    weights.push_back(value);
                }
            }
            if (poles.size() != rawPoleCount || poles.size() < 2U
                || (!weights.empty() && weights.size() != poles.size())) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "Bezier requires at least two two-number poles and optional positive weights",
                    object.name,
                    propertyName
                );
                return false;
            }

            // FreeCAD:
            // /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
            // ::GeomBezierCurve::Restore(), reads "PolesCount" and each Pole's
            // "X/Y/Z/Weight" fields.
            parsed.beziers.push_back(
                SketchBezier {
                    index,
                    std::move(poles),
                    std::move(weights),
                    readBoolField(item, "construction", false),
                    geometryId.value,
                }
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

std::vector<SketchHyperbolaArc> profileHyperbolaArcs(const std::vector<SketchHyperbolaArc>& arcs)
{
    std::vector<SketchHyperbolaArc> profile;
    for (const auto& arc : arcs) {
        if (!arc.construction) {
            profile.push_back(arc);
        }
    }
    return profile;
}

std::vector<SketchParabolaArc> profileParabolaArcs(const std::vector<SketchParabolaArc>& arcs)
{
    std::vector<SketchParabolaArc> profile;
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

std::vector<SketchInterpolatedSpline> profileInterpolatedSplines(
    const std::vector<SketchInterpolatedSpline>& splines
)
{
    std::vector<SketchInterpolatedSpline> profile;
    for (const auto& spline : splines) {
        if (!spline.construction) {
            profile.push_back(spline);
        }
    }
    return profile;
}

std::vector<SketchBezier> profileBeziers(const std::vector<SketchBezier>& beziers)
{
    std::vector<SketchBezier> profile;
    for (const auto& bezier : beziers) {
        if (!bezier.construction) {
            profile.push_back(bezier);
        }
    }
    return profile;
}

} // namespace cad_core::sketcher
