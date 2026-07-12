#include "cad_core/part_design/feature_pad.h"

#include "cad_core/part_design/feature_extrude.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/producer_trace_scope.h"
#include "cad_core/runtime/reference_resolution.h"
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

void executePad(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    runtime::ProducerTraceScope producerTrace(
        context,
        object,
        "partdesign.extrude",
        "FeaturePad::execute",
        {{"mode", "additive"},
         {"reversed", app::readBool(object, "Reversed").value_or(false)},
         {"type", app::readString(object, "Type").value_or("Length")}},
        {{"options", "3"}}
    );
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/FeaturePad.cpp Pad::execute()
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
                                      "StartOffset",
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
    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Additive, "Pad");
    if (extrusion && context.producerTrace) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
        // FeatureExtrude.cpp::FeatureExtrude::execute() assigns `rawShape = prism` after
        // buildExtrusion(). TopoShape::setShape(prism, false) publishes the shape-only value
        // before makeElementRefine() consumes the producer-mapped prism.
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

    const auto featureShape = finalizeFeatureExtrusion(object, context, AddSubMode::Additive, *extrusion);
    if (!featureShape) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    std::optional<part::NamedShape> namedShape = featureShape->namedShape;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
    // ::ProfileBased::execute() refines the feature result before `Shape.setValue(...)`.
    // Body::execute() subsequently reads that already-published Tip Shape; it must not be the
    // first producer of Pad's refine mapper history.
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

    const TopoDS_Shape solid = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (namedShape) {
        if (!featureShape->combinedWithBase && context.producerTrace) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp
            // ::FeatureExtrude::execute(), the no-base branch performs the final
            // `refineShapeIfActive(prism)` handoff before `this->Shape.setValue(...)`. A Boolean
            // result already received that setShape handoff when its post-CUT Refine completed.
            part::NamedShape reset;
            reset.owner = object.name + ".Shape.result";
            reset.shape = solid;
            reset.producerTag = 0L;
            reset.stringHasher = context.stringHasher;
            context.producerTrace->record({
                "toposhape.set_shape", "begin", "reset_requested",
                {{"incomingNull", solid.IsNull() ? "true" : "false"},
                 {"resetElementMap", "true"}, {"tag", "0"}},
            });
            part::checkpointNamedShapeLedger(
                reset, object.name + ":shape-result", "toposhape.set_shape_checkpoint"
            );
        }
        // FreeCAD: src/Mod/Part/App/PropertyTopoShape.cpp::PropertyPartShape::setValue()
        // persists the completed Pad Shape and assigns its owning DocumentObject Tag.
        *namedShape = part::namedShapeForPropertyShapeValue(
            object.name,
            solid,
            *namedShape,
            static_cast<long>(object.id),
            true,
            false,
            runtime::downstreamElementReferenceSubnames(object.name, context)
        );
    }
    const nlohmann::json mesh = cad_core::part::meshForShape(solid);
    const nlohmann::json subshapeMap = part::subshapeMapForShape(solid);

    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.addSubShapes[object.name] = runtime::AddSubShape{
        featureShape->addSubShape,
        std::nullopt,
        featureShape->addSubNamedShape,
        std::nullopt,
    };
    context.mesh[object.name] = mesh;
    context.subshapes[object.name] = subshapeMap;
    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "add"},
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
