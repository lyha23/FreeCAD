#include "cad_core/part_design/feature_revolved.h"

#include "cad_core/app/property.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct ProfileSelection {
    app::Link link;
    TopoDS_Shape shape;
    std::optional<gp_Dir> normal;
};

struct AxisSelection {
    gp_Pnt base;
    gp_Dir direction;
};

nlohmann::json pointToJson(const gp_Pnt& point)
{
    return nlohmann::json::array({point.X(), point.Y(), point.Z()});
}

nlohmann::json directionToJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

std::string enumNameFromIndex(double value, const std::vector<std::string>& names)
{
    const auto index = static_cast<std::size_t>(value);
    if (std::abs(value - static_cast<double>(index)) > Precision::Confusion() || index >= names.size()) {
        return {};
    }
    return names[index];
}

std::string readEnumeration(const app::DocumentObject& object,
                            const std::string& property,
                            const std::vector<std::string>& names,
                            const std::string& fallback)
{
    if (const auto value = app::readString(object, property)) {
        return *value;
    }
    if (const auto value = app::readNumber(object, property)) {
        const std::string name = enumNameFromIndex(*value, names);
        return name.empty() ? fallback : name;
    }
    return fallback;
}

std::optional<double> readAngleDegrees(const app::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& property,
                                       const std::string& featureName,
                                       double fallback)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return fallback;
    }
    const auto number = app::readNumber(object, property);
    if (!number) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_property_type",
                               featureName + " " + property + " must be numeric degrees",
                               object.name,
                               property);
        return std::nullopt;
    }
    return *number;
}

bool readBool(const app::DocumentObject& object, const std::string& property, bool fallback)
{
    const auto value = app::readBool(object, property);
    return value.value_or(fallback);
}

bool rejectUnsupportedRevolvedBoundary(const app::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& featureName,
                                       const std::string& method)
{
    if (method == "Angle") {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
    // ::Revolved::tryExecuteRevolved(), "ToFirst/ToFace/ToLast" delegate to BRepFeat_MakeRevol
    // through TopoShape::makeElementRevolution(); this first C4-M2 batch only claims Angle.
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           featureName + " Type=" + method
                               + " requires the deferred BRepFeat_MakeRevol UpTo/TwoAngles path",
                           object.name,
                           "Type");
    return false;
}

std::optional<ProfileSelection> resolveProfile(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const std::string& featureName)
{
    if (app::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    const auto profileLink = app::readLink(object, "Profile");
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               featureName + " Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    if (!profileLink->subnames.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_profile_region",
                               featureName + " Profile subshape selection needs Sketch.InternalShape ElementMap support",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object,
                               profileLink->subnames.front());
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end()
        || (shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Profile)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a profile",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const TopoDS_Shape* profileShape = nullptr;
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch) {
        if (shapeIt->second.profileRequiresSubshapeSelection) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_profile_region",
                                   featureName + " multi-face sketch profile requires explicit InternalFace selection",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink->object);
            return std::nullopt;
        }
        profileShape = shapeIt->second.profileShape ? &*shapeIt->second.profileShape : nullptr;
    }
    else {
        profileShape = &shapeIt->second.shape;
    }

    if (profileShape == nullptr || profileShape->IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               "Profile target " + profileLink->object + " did not produce a closed profile face",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }
    return ProfileSelection{*profileLink, *profileShape, shapeIt->second.profileNormal};
}

bool axisIsParallelToProfileNormal(const AxisSelection& axis, const std::optional<gp_Dir>& normal)
{
    return normal && std::abs(axis.direction.Dot(*normal)) > (1.0 - Precision::Angular());
}

std::optional<AxisSelection> sketchAxis(const std::string& subname,
                                        const std::string& objectName,
                                        const runtime::ComputeContext& context)
{
    AxisSelection axis;
    axis.base = gp_Pnt(0.0, 0.0, 0.0);
    if (subname == "H_Axis") {
        axis.direction = gp_Dir(1.0, 0.0, 0.0);
    }
    else if (subname == "V_Axis") {
        axis.direction = gp_Dir(0.0, 1.0, 0.0);
    }
    else if (subname == "N_Axis") {
        axis.direction = gp_Dir(0.0, 0.0, 1.0);
    }
    else {
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(objectName);
    if (placementIt != context.globalPlacements.end()) {
        axis.base.Transform(placementIt->second);
        axis.direction.Transform(placementIt->second);
    }
    return axis;
}

std::optional<AxisSelection> axisFromEdge(const TopoDS_Edge& edge,
                                          const app::DocumentObject& object,
                                          runtime::ComputeContext& context,
                                          const std::string& target,
                                          const std::string& subname)
{
    try {
        BRepAdaptor_Curve curve(edge);
        AxisSelection axis;
        if (curve.GetType() == GeomAbs_Line) {
            axis.base = curve.Line().Location();
            axis.direction = curve.Line().Direction();
        }
        else if (curve.GetType() == GeomAbs_Circle) {
            axis.base = curve.Circle().Location();
            axis.direction = curve.Circle().Axis().Direction();
        }
        else {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_axis",
                                   "ReferenceAxis edge must be a straight line, circle or arc of circle",
                                   object.name,
                                   "ReferenceAxis",
                                   "runtime",
                                   target,
                                   subname);
            return std::nullopt;
        }
        return axis;
    }
    catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               std::string("ReferenceAxis edge could not be read: ")
                                   + (message != nullptr ? message : "unknown OCCT error"),
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               target,
                               subname);
        return std::nullopt;
    }
}

std::optional<AxisSelection> resolveReferenceAxis(const app::DocumentObject& object,
                                                  runtime::ComputeContext& context,
                                                  const ProfileSelection& profile,
                                                  const std::string& featureName)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getAxis(), accepts sketch axes "H_Axis"/"V_Axis"/"N_Axis" and
    // support edges where curve type is "GeomAbs_Line" or "GeomAbs_Circle".
    const auto referenceAxis = app::readLink(object, "ReferenceAxis");
    if (!referenceAxis || referenceAxis->object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               featureName + " ReferenceAxis must link to a sketch axis or edge",
                               object.name,
                               "ReferenceAxis");
        return std::nullopt;
    }
    const std::string subname = referenceAxis->subnames.empty() ? std::string {} : referenceAxis->subnames.front();

    if (referenceAxis->object == profile.link.object && !subname.empty()) {
        auto axis = sketchAxis(subname, referenceAxis->object, context);
        if (axis) {
            if (axisIsParallelToProfileNormal(*axis, profile.normal)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_axis",
                                       "Axis must not be perpendicular to the sketch plane",
                                       object.name,
                                       "ReferenceAxis",
                                       "runtime",
                                       referenceAxis->object,
                                       subname);
                return std::nullopt;
            }
            return axis;
        }
    }

    const auto shapeIt = context.shapes.find(referenceAxis->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "ReferenceAxis target " + referenceAxis->object + " did not produce a shape",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object);
        return std::nullopt;
    }

    TopoDS_Shape axisShape;
    if (subname.empty()) {
        axisShape = shapeIt->second.shape;
    }
    else if (const auto subshape = part::subshapeByName(shapeIt->second.shape, subname)) {
        axisShape = *subshape;
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "ReferenceAxis subshape " + subname + " is not available on " + referenceAxis->object,
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }

    if (axisShape.IsNull() || axisShape.ShapeType() != TopAbs_EDGE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "ReferenceAxis must resolve to an edge",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }

    auto axis = axisFromEdge(TopoDS::Edge(axisShape), object, context, referenceAxis->object, subname);
    if (axis && axisIsParallelToProfileNormal(*axis, profile.normal)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_axis",
                               "Axis must not be perpendicular to the sketch plane",
                               object.name,
                               "ReferenceAxis",
                               "runtime",
                               referenceAxis->object,
                               subname);
        return std::nullopt;
    }
    return axis;
}

TopoDS_Shape rotatedShape(const TopoDS_Shape& shape, const gp_Ax1& axis, double angle)
{
    gp_Trsf transform;
    transform.SetRotation(axis, angle);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

std::optional<part::NamedShapeBuild> buildRevolvedTool(const app::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const ProfileSelection& profile,
                                                       const AxisSelection& axisSelection,
                                                       double angleRadians,
                                                       bool reversed,
                                                       bool midplane)
{
    gp_Ax1 axis(axisSelection.base, axisSelection.direction);
    if (reversed) {
        axis.Reverse();
    }

    TopoDS_Shape sourceShape = profile.shape;
    if (midplane) {
        sourceShape = rotatedShape(sourceShape, axis, -angleRadians / 2.0);
    }

    part::NamedShapeSource source{profile.link.object, sourceShape};
    const auto namedShapeIt = context.namedShapes.find(profile.link.object);
    if (namedShapeIt != context.namedShapes.end() && !midplane) {
        source.namedShape = &namedShapeIt->second;
    }
    return part::makeElementRevolveFromSource(object.name, source, axis, angleRadians);
}

}  // namespace

void executeRevolvedFeature(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            RevolvedAddSubMode mode,
                            const std::string& featureName)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp
    // ::Revolution::execute() calls executeRevolved(Part::RevolMode::FuseWithBase);
    // FeatureGroove.cpp::Groove::execute() calls executeRevolved(Part::RevolMode::CutFromBase).
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"Profile",
                                               "Type",
                                               "Angle",
                                               "Angle2",
                                               "ReferenceAxis",
                                               "Base",
                                               "Axis",
                                               "Reversed",
                                               "Midplane",
                                               "UpToFace",
                                               "UpToFace2",
                                               "BaseFeature",
                                               "Refine",
                                               "FuzzyTolerance",
                                               "FuseOrder"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<std::string> revolutionTypes {"Angle", "UpToLast", "UpToFirst", "UpToFace", "TwoAngles"};
    const std::vector<std::string> grooveTypes {"Angle", "ThroughAll", "UpToFirst", "UpToFace", "TwoAngles"};
    const std::string method = readEnumeration(
        object,
        "Type",
        featureName == "Groove" ? grooveTypes : revolutionTypes,
        "Angle"
    );
    if (!rejectUnsupportedRevolvedBoundary(object, context, featureName, method)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (featureName == "Revolution") {
        const std::string fuseOrder = readEnumeration(object, "FuseOrder", {"BaseFirst", "FeatureFirst"}, "BaseFirst");
        if (fuseOrder == "FeatureFirst") {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Revolution FuseOrder=FeatureFirst is a FreeCAD 1.0 compatibility path deferred from C4-S4",
                                   object.name,
                                   "FuseOrder");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
    }

    const auto angleDegrees = readAngleDegrees(object, context, "Angle", featureName, 360.0);
    if (!angleDegrees) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (*angleDegrees > 360.0) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Angle of revolution too large",
                               object.name,
                               "Angle");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (std::abs(*angleDegrees) <= Precision::Angular()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Angle of revolution too small",
                               object.name,
                               "Angle");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto profile = resolveProfile(object, context, featureName);
    if (!profile) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto axis = resolveReferenceAxis(object, context, *profile, featureName);
    if (!axis) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const bool reversed = readBool(object, "Reversed", false);
    const bool midplane = readBool(object, "Midplane", false);
    const double angleRadians = *angleDegrees * kPi / 180.0;
    const auto build = buildRevolvedTool(object, context, *profile, *axis, angleRadians, reversed, midplane);
    if (!build || !build->error.empty() || build->shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build && !build->error.empty() ? build->error : "Could not revolve the sketch",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<part::NamedShape> namedShape = build->namedShape;
    runtime::RefineShapeResult shapeResult{build->shape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, build->shape, namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        shapeResult = *refined;
    }

    const TopoDS_Shape tool = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.mesh[object.name] = cad_core::part::meshForShape(tool);
    context.subshapes[object.name] = part::subshapeMapForShape(tool);

    if (mode == RevolvedAddSubMode::Additive) {
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, tool};
        context.addSubShapes[object.name] = runtime::AddSubShape{tool, std::nullopt, namedShape, std::nullopt};
    }
    else {
        context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, tool, std::nullopt, namedShape};
    }

    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", mode == RevolvedAddSubMode::Additive ? "add" : "sub"},
        {"method", method},
        {"source_profile", profile->link.object},
        {"angle", *angleDegrees},
        {"axis_base", pointToJson(axis->base)},
        {"axis_direction", directionToJson(axis->direction)},
        {"bbox", cad_core::part::bboxForShape(tool)},
        {"volume", cad_core::part::volumeForShape(tool)},
        {"topo_naming_history", "maker_history:revolve"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (reversed) {
        result["reversed"] = true;
    }
    if (midplane) {
        result["midplane"] = true;
    }
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::part_design
