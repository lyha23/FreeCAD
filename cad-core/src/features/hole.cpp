#include "cad_core/features/hole.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::features {

namespace {

constexpr int baseProfileOnPoints = 1 << 0;
constexpr int baseProfileOnCircles = 1 << 1;
constexpr int baseProfileOnArcs = 1 << 2;
constexpr int baseProfileOnCirclesArcs = baseProfileOnCircles | baseProfileOnArcs;

struct HoleBuild {
    document::Link profile;
    std::string method;
    double diameter = 0.0;
    double depth = 0.0;
    TopoDS_Shape toolShape;
};

const nlohmann::json* propertyPayload(const document::DocumentObject& object, const std::string& property)
{
    const auto* value = document::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::optional<int> readIntegerProperty(const document::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr || !payload->is_number_integer()) {
        return std::nullopt;
    }
    return payload->get<int>();
}

std::string readEnumProperty(const document::DocumentObject& object,
                             const std::string& property,
                             const std::vector<std::string>& values,
                             const std::string& fallback)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr) {
        return fallback;
    }
    if (payload->is_string()) {
        return payload->get<std::string>();
    }
    if (payload->is_number_integer()) {
        const int index = payload->get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < values.size()) {
            return values[static_cast<std::size_t>(index)];
        }
    }
    return fallback;
}

double readNumberProperty(const document::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool readBoolProperty(const document::DocumentObject& object, const std::string& property, bool fallback = false)
{
    return document::readBool(object, property).value_or(fallback);
}

std::optional<TopoDS_Shape> previousSolidShape(const runtime::ComputeContext& context)
{
    for (auto it = context.executionOrder.rbegin(); it != context.executionOrder.rend(); ++it) {
        const auto shapeIt = context.shapes.find(*it);
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
            return shapeIt->second.shape;
        }
    }
    return std::nullopt;
}

double throughAllLength(const TopoDS_Shape& base, const TopoDS_Shape& profile)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getThroughAllLength(), returns "2.02 * sqrt(box.SquareExtent())".
    Bnd_Box box;
    BRepBndLib::Add(base, box);
    if (!profile.IsNull()) {
        BRepBndLib::Add(profile, box);
    }
    box.SetGap(0.0);
    return 2.02 * std::sqrt(box.SquareExtent());
}

std::optional<gp_Dir> planarProfileNormal(const TopoDS_Shape& profileShape)
{
    for (TopExp_Explorer explorer(profileShape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Plane) {
            return surface.Plane().Axis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> circularProfileNormal(const TopoDS_Shape& profileShape)
{
    for (TopExp_Explorer explorer(profileShape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Circle) {
            return curve.Circle().Axis().Direction();
        }
    }
    return std::nullopt;
}

std::optional<gp_Dir> guessHoleDirection(const TopoDS_Shape& rawProfile,
                                         const std::optional<TopoDS_Shape>& profileFace)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::guessNormalDirection(), uses a cylindrical profile axis first, otherwise "getProfileNormal()".
    if (const auto circleNormal = circularProfileNormal(rawProfile)) {
        return circleNormal;
    }
    if (profileFace) {
        return planarProfileNormal(*profileFace);
    }
    return std::nullopt;
}

int readBaseProfileType(const document::DocumentObject& object)
{
    if (const auto value = readIntegerProperty(object, "BaseProfileType")) {
        return *value;
    }
    const auto text = document::readString(object, "BaseProfileType");
    if (!text) {
        return baseProfileOnCirclesArcs;
    }
    if (*text == "Points") {
        return baseProfileOnPoints;
    }
    if (*text == "Circles") {
        return baseProfileOnCircles;
    }
    if (*text == "Arcs") {
        return baseProfileOnArcs;
    }
    if (*text == "PointsCirclesArcs" || *text == "OnPointsCirclesArcs") {
        return baseProfileOnPoints | baseProfileOnCircles | baseProfileOnArcs;
    }
    return baseProfileOnCirclesArcs;
}

std::vector<gp_Pnt> holeCentersFromCircularProfile(const TopoDS_Shape& rawProfile, int baseProfileType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::findHoles(), iterates profile edges, keeps GeomAbs_Circle, then filters
    // "adaptor.IsClosed()" by BaseProfileTypeOptions::OnCircles / OnArcs and uses
    // "circle.Axis().Location()" as the hole center.
    std::vector<gp_Pnt> centers;
    if ((baseProfileType & baseProfileOnCircles) == 0 && (baseProfileType & baseProfileOnArcs) == 0) {
        return centers;
    }

    for (TopExp_Explorer explorer(rawProfile, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        if ((baseProfileType & baseProfileOnCircles) == 0 && curve.IsClosed()) {
            continue;
        }
        if ((baseProfileType & baseProfileOnArcs) == 0 && !curve.IsClosed()) {
            continue;
        }
        centers.push_back(curve.Circle().Axis().Location());
    }

    return centers;
}

std::optional<TopoDS_Shape> buildCylinderTool(const std::vector<gp_Pnt>& centers,
                                              const gp_Dir& direction,
                                              double radius,
                                              double depth,
                                              const document::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    if (centers.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole profile did not provide circle or arc centers",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    std::vector<TopoDS_Shape> holes;
    for (const gp_Pnt& center : centers) {
        BRepPrimAPI_MakeCylinder builder(gp_Ax2(center, direction), radius, depth);
        builder.Build();
        if (!builder.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not build Hole cylinder",
                                   object.name,
                                   "Profile");
            return std::nullopt;
        }
        holes.push_back(builder.Shape());
    }

    if (holes.size() == 1U) {
        return holes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& hole : holes) {
        builder.Add(compound, hole);
    }
    return compound;
}

bool rejectActiveHoleGaps(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), after base cylinder setup, branches into "Threaded", "ModelThread",
    // "HoleCutType", "Tapered" and "DrillPoint". cad-core only accepts the plain Flat cylinder path here.
    struct Gap {
        std::string property;
        std::string message;
    };
    std::vector<Gap> gaps;
    if (readBoolProperty(object, "Threaded")) {
        gaps.push_back({"Threaded", "Hole threaded geometry requires FreeCAD Hole::makeThread migration"});
    }
    if (readBoolProperty(object, "ModelThread")) {
        gaps.push_back({"ModelThread", "Hole model thread geometry requires FreeCAD Hole::makeThread migration"});
    }
    if (readBoolProperty(object, "Tapered")) {
        gaps.push_back({"Tapered", "Hole tapered geometry requires FreeCAD taper profile migration"});
    }

    const std::string holeCutType = readEnumProperty(object,
                                                     "HoleCutType",
                                                     {"None", "Counterbore", "Countersink", "Counterdrill"},
                                                     "None");
    if (holeCutType != "None") {
        gaps.push_back({"HoleCutType", "Hole counterbore/countersink/counterdrill profiles are not migrated yet"});
    }

    const std::string drillPoint = readEnumProperty(object, "DrillPoint", {"Flat", "Angled"}, "Angled");
    if (drillPoint != "Flat") {
        gaps.push_back({"DrillPoint", "Hole angled drill point profile is not migrated yet"});
    }

    for (const auto& gap : gaps) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               gap.message,
                               object.name,
                               gap.property);
    }
    return gaps.empty();
}

std::optional<HoleBuild> buildHole(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), reads "Profile", "Diameter", "DepthType", "Depth", "DrillPoint",
    // computes "SketchVector = guessNormalDirection(profileshape)", then calls findHoles().
    if (document::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Hole Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto profileLink = document::readLink(object, "Profile");
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Hole Profile must link to a Sketch object",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end()
        || (shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Profile)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a sketch profile",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const TopoDS_Shape rawProfile = shapeIt->second.shape;
    if (rawProfile.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole Profile target " + profileLink->object + " did not produce profile geometry",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink->object);
        return std::nullopt;
    }

    const auto base = previousSolidShape(context);
    if (!base) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Hole requires a previous base solid",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const double diameter = readNumberProperty(object, "Diameter", 6.0);
    if (diameter <= 10.0 * Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole Diameter must be positive",
                               object.name,
                               "Diameter");
        return std::nullopt;
    }

    const std::string method = readEnumProperty(object, "DepthType", {"Dimension", "ThroughAll"}, "Dimension");
    double depth = 0.0;
    if (method == "Dimension") {
        depth = readNumberProperty(object, "Depth", 25.0);
    }
    else if (method == "ThroughAll") {
        depth = throughAllLength(*base, rawProfile);
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported Hole DepthType " + method,
                               object.name,
                               "DepthType");
        return std::nullopt;
    }

    if (depth <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Hole Depth must be positive",
                               object.name,
                               method == "Dimension" ? "Depth" : "DepthType");
        return std::nullopt;
    }

    auto direction = guessHoleDirection(rawProfile, shapeIt->second.profileShape);
    if (!direction) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_direction",
                               "Hole could not infer the profile normal direction",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    if (readBoolProperty(object, "Reversed")) {
        direction->Reverse();
    }

    const int baseProfileType = readBaseProfileType(object);
    const std::vector<gp_Pnt> centers = holeCentersFromCircularProfile(rawProfile, baseProfileType);
    const auto toolShape = buildCylinderTool(centers, *direction, diameter / 2.0, depth, object, context);
    if (!toolShape) {
        return std::nullopt;
    }

    return HoleBuild{*profileLink, method, diameter, depth, *toolShape};
}

}  // namespace

void executeHole(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::findHoles()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Profile",
                                      "Threaded",
                                      "ModelThread",
                                      "CosmeticThread",
                                      "ThreadType",
                                      "ThreadSize",
                                      "ThreadClass",
                                      "ThreadFit",
                                      "ThreadDirection",
                                      "ThreadPitch",
                                      "ThreadDiameter",
                                      "ThreadDepthType",
                                      "ThreadDepth",
                                      "Diameter",
                                      "HoleCutType",
                                      "HoleCutCustomValues",
                                      "HoleCutDiameter",
                                      "HoleCutDepth",
                                      "HoleCutCountersinkAngle",
                                      "DepthType",
                                      "Depth",
                                      "DrillPoint",
                                      "DrillPointAngle",
                                      "DrillForDepth",
                                      "Tapered",
                                      "TaperedAngle",
                                      "UseCustomThreadClearance",
                                      "CustomThreadClearance",
                                      "BaseProfileType",
                                      "Reversed",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!rejectActiveRefineProperty(object, context)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!rejectActiveHoleGaps(object, context)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto hole = buildHole(object, context);
    if (!hole) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, hole->toolShape};
    context.mesh[object.name] = geometry::meshForShape(hole->toolShape);
    context.subshapes[object.name] = topo::subshapeMapForShape(hole->toolShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "sub"},
        {"method", hole->method},
        {"source_profile", hole->profile.object},
        {"diameter", hole->diameter},
        {"depth", hole->depth},
        {"drill_point", "Flat"},
        {"bbox", geometry::bboxForShape(hole->toolShape)},
        {"volume", geometry::volumeForShape(hole->toolShape)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
