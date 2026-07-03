#include "cad_core/part_design/feature_pocket.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part_design/feature_extrude.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"

#include <TopAbs_ShapeEnum.hxx>

namespace cad_core::part_design {

namespace {

std::string shapeKind(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        case TopAbs_SHAPE:
            break;
    }
    return "occt_shape";
}

std::string profileKindName(ProfileKind kind)
{
    switch (kind) {
        case ProfileKind::ClosedFace:
            return "closed_face";
        case ProfileKind::OpenWire:
            return "open_wire";
        case ProfileKind::EdgeCompound:
            return "edge_compound";
    }
    return "closed_face";
}

std::string openProfileModeNameForResult(OpenProfileMode mode)
{
    switch (mode) {
        case OpenProfileMode::Auto:
            return "Auto";
        case OpenProfileMode::Reject:
            return "Reject";
        case OpenProfileMode::SurfaceExtrusion:
            return "SurfaceExtrusion";
        case OpenProfileMode::ThinSolid:
            return "ThinSolid";
        case OpenProfileMode::ThinCut:
            return "ThinCut";
        case OpenProfileMode::SurfaceSplitCut:
            return "SurfaceSplitCut";
    }
    return "Auto";
}

void appendOpenProfileResultFields(nlohmann::json& result, const ExtrudeResult& extrusion)
{
    if (extrusion.profileKind == ProfileKind::ClosedFace) {
        return;
    }
    result["profileKind"] = profileKindName(extrusion.profileKind);
    result["openProfileMode"] = openProfileModeNameForResult(extrusion.openProfileMode);
    if (extrusion.resolvedOpenProfileMode) {
        result["resolvedOpenProfileMode"] = openProfileModeNameForResult(*extrusion.resolvedOpenProfileMode);
    }
    result["bodyParticipation"] = extrusion.bodyParticipation;
    result["sourceProfile"] = {
        {"object", extrusion.profile.object},
        {"stableSubnames", extrusion.sourceProfileStableSubnames},
    };
}

void appendProfileResolveFields(nlohmann::json& result, const ExtrudeResult& extrusion)
{
    if (!extrusion.profileResolveMode.empty()) {
        result["profileResolveMode"] = extrusion.profileResolveMode;
    }
    if (!extrusion.profileOwner.empty()) {
        result["profileOwner"] = extrusion.profileOwner;
    }
    if (!extrusion.requestedProfileSubname.empty()) {
        result["requestedSubname"] = extrusion.requestedProfileSubname;
    }
    if (!extrusion.currentProfileSubname.empty()) {
        result["currentSubname"] = extrusion.currentProfileSubname;
    }
}

}  // namespace

void executePocket(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/FeaturePocket.cpp Pocket::execute()
    // src/Mod/PartDesign/App/FeatureAddSub.cpp FeatureAddSub::getAddSubShape()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Profile",
                                      "Type",
                                      "Type2",
                                      "Length",
                                      "Length2",
                                      "Reversed",
                                      "SideType",
                                      "UpToFace",
                                      "UpToFace2",
                                      "UpToShape",
                                      "UpToShape2",
                                      // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Feature.h
                                      // ::PartDesign::Feature carries "App::PropertyLink BaseFeature".
                                      "BaseFeature",
                                      "Offset",
                                      "Offset2",
                                      "TaperAngle",
                                      "TaperAngle2",
                                      "UseCustomVector",
                                      "Direction",
                                      "ReferenceAxis",
                                      "AlongSketchNormal",
                                      "OpenProfileMode",
                                      "OpenProfileThickness",
                                      "OpenProfileSide",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Subtractive, "Pocket");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (extrusion->bodyParticipation == "display_only") {
        const TopoDS_Shape surface = extrusion->toolShape;
        if (extrusion->namedShape) {
            context.namedShapes[object.name] = *extrusion->namedShape;
        }
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::PartPrimitive, surface};
        context.mesh[object.name] = cad_core::part::meshForShape(surface);
        context.subshapes[object.name] = part::subshapeMapForShape(surface);
        nlohmann::json result = {
            {"status", "ok"},
            {"shape", shapeKind(surface)},
            {"add_sub", "display"},
            {"method", extrusion->method},
            {"source_profile", extrusion->profile.object},
            {"bbox", extrusion->bbox},
            {"volume", extrusion->volume},
            {"kernel", cad_core::part::kernelVersion()},
        };
        appendOpenProfileResultFields(result, *extrusion);
        appendProfileResolveFields(result, *extrusion);
        result["topo_naming_history"] = "mapper_history:open_profile_surface";
        context.objects[object.name] = result;
        return;
    }

    std::optional<part::NamedShape> namedShape = extrusion->namedShape;
    runtime::RefineShapeResult shapeResult{extrusion->toolShape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, extrusion->toolShape, namedShape);
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
    context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, tool, std::nullopt, namedShape};
    context.mesh[object.name] = cad_core::part::meshForShape(tool);
    context.subshapes[object.name] = part::subshapeMapForShape(tool);
    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "sub"},
        {"method", extrusion->method},
        {"source_profile", extrusion->profile.object},
        {"bbox", extrusion->bbox},
        {"volume", extrusion->volume},
        {"kernel", cad_core::part::kernelVersion()},
    };
    appendOpenProfileResultFields(result, *extrusion);
    appendProfileResolveFields(result, *extrusion);
    if (extrusion->profileKind != ProfileKind::ClosedFace) {
        result["topo_naming_history"] = "mapper_history:open_profile_thin";
    }
    if (extrusion->taperHistory) {
        result["topo_naming_history"] = "maker_history:taper_thru_sections";
    }
    else if (extrusion->topoNamingKnownGap) {
        result["topo_naming_history"] = "history_partial:taper_thru_sections";
    }
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::part_design
