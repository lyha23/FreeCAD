#include "cad_core/part_design/feature_pad.h"

#include "cad_core/part_design/feature_extrude.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"

namespace cad_core::part_design {

void executePad(const app::DocumentObject& object, runtime::ComputeContext& context)
{
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
                                      "TaperAngle",
                                      "TaperAngle2",
                                      "UseCustomVector",
                                      "Direction",
                                      "ReferenceAxis",
                                      "AlongSketchNormal",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Additive, "Pad");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
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

    const TopoDS_Shape solid = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    const nlohmann::json mesh = cad_core::part::meshForShape(solid);
    const nlohmann::json subshapeMap = part::subshapeMapForShape(solid);

    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt, namedShape, std::nullopt};
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
