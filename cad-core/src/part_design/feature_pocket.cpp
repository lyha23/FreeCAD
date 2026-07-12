#include "cad_core/part_design/feature_pocket.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/producer_trace_scope.h"
#include "cad_core/runtime/reference_resolution.h"
#include "cad_core/part_design/feature_extrude.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/element_map_producer_trace_snapshot.h"

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
    runtime::ProducerTraceScope producerTrace(
        context,
        object,
        "partdesign.extrude",
        "FeaturePocket::execute",
        {{"mode", "subtractive"},
         {"reversed", app::readBool(object, "Reversed").value_or(false)},
         {"type", app::readString(object, "Type").value_or("Length")}},
        {{"options", "11"}}
    );
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
    if (extrusion && context.producerTrace) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
        // FeatureExtrude.cpp::FeatureExtrude::execute() assigns `rawShape = prism` after
        // buildExtrusion(), before makeElementRefine() consumes the mapped tool.
        part::NamedShape rawShape;
        rawShape.owner = object.name + ".rawShape";
        rawShape.shape = extrusion->toolShape;
        rawShape.producerTag = 0;
        rawShape.stringHasher = context.stringHasher;
        context.producerTrace->record({
            "toposhape.set_shape",
            "begin",
            "reset_requested",
            {{"incomingNull", extrusion->toolShape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", "0"}},
        });
        part::checkpointNamedShapeLedger(
            rawShape, object.name + ":raw-prism", "toposhape.set_shape_checkpoint"
        );
        context.producerTrace->record({
            "shape_slot.assign",
            "assigned",
            "extrude_raw_prism",
            {{"property", "rawShape"}},
        });
    }
    if (!extrusion) {
        producerTrace.event(
            "rejected", "feature_extrusion_failed", {{"profile", ""}, {"hasRawShape", false}}
        );
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

    const auto featureShape = finalizeFeatureExtrusion(object, context, AddSubMode::Subtractive, *extrusion);
    if (!featureShape) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    std::optional<part::NamedShape> namedShape = featureShape->namedShape;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // refines the subtractive feature before publishing its Shape.  Preserve that producer-side
    // ElementMap/MapperHistory for the Body Tip and any later DressUp consumer.
    if (context.producerTrace) {
        part::NamedShape rawShape;
        rawShape.owner = object.name + ".Shape.raw";
        rawShape.shape = featureShape->shape;
        rawShape.producerTag = 0L;
        rawShape.stringHasher = context.stringHasher;
        context.producerTrace->record({
            "toposhape.set_shape",
            "begin",
            "reset_requested",
            {{"incomingNull", featureShape->shape.IsNull() ? "true" : "false"},
             {"resetElementMap", "true"},
             {"tag", "0"}},
        });
        part::checkpointNamedShapeLedger(
            rawShape, object.name + ":shape-raw", "toposhape.set_shape_checkpoint"
        );
    }
    const auto refined = runtime::applyPartDesignFeatureRefineProperty(
        object,
        context,
        featureShape->shape,
        namedShape
    );
    if (!refined) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    runtime::RefineShapeResult shapeResult = *refined;

    const TopoDS_Shape tool = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (namedShape) {
        if (!featureShape->combinedWithBase && context.producerTrace) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::execute(), the no-base Shape handoff follows the final Refine;
            // combined CUT results already received that setShape handoff in the post-Boolean
            // Refine and must not publish it twice before PropertyPartShape::setValue().
            part::NamedShape reset;
            reset.owner = object.name + ".Shape.result";
            reset.shape = tool;
            reset.producerTag = 0L;
            reset.stringHasher = context.stringHasher;
            context.producerTrace->record({
                "toposhape.set_shape", "begin", "reset_requested",
                {{"incomingNull", tool.IsNull() ? "true" : "false"},
                 {"resetElementMap", "true"}, {"tag", "0"}},
            });
            part::checkpointNamedShapeLedger(
                reset, object.name + ":shape-result", "toposhape.set_shape_checkpoint"
            );
        }
        // The final Pocket Shape, not its local pre-Boolean prism/AddSub cache, crosses the
        // same PropertyPartShape::setValue() boundary that Body and the next feature consume.
        *namedShape = part::namedShapeForPropertyShapeValue(
            object.name,
            tool,
            *namedShape,
            static_cast<long>(object.id),
            true,
            false,
            runtime::downstreamElementReferenceSubnames(object.name, context)
        );
    }
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::FeatureExtrude::execute() writes the cumulative Boolean result through
    // `this->Shape.setValue(getSolid(solRes))`; AddSubShape remains the tool cache only.
    // Body subsequently copies this feature-owned Shape as its Tip.
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, tool};
    context.addSubShapes[object.name] = runtime::AddSubShape{
        std::nullopt,
        featureShape->addSubShape,
        std::nullopt,
        featureShape->addSubNamedShape,
    };
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
    if (context.producerTrace) {
        context.producerTrace->record({
            "shape_slot.assign",
            "assigned",
            featureShape->combinedWithBase ? "extrude_boolean_result" : "extrude_prism_result",
            {{"property", "Shape"}},
        });
        const std::string snapshot = context.producerTrace->currentSnapshotId();
        context.producerTrace->record({
            "partdesign.extrude.final_checkpoint",
            "published",
            "",
            {{"snapshot", snapshot}},
            snapshot,
            snapshot,
        });
    }
}

}  // namespace cad_core::part_design
