#include "cad_core/features/pocket.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/features/feature_extrude.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

namespace cad_core::features {

void executePocket(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/FeaturePocket.cpp Pocket::execute()
    // src/Mod/PartDesign/App/FeatureAddSub.cpp FeatureAddSub::getAddSubShape()
    if (!rejectUnsupportedProperties(object,
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
    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Subtractive, "Pocket");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<topo::NamedShape> namedShape = extrusion->namedShape;
    RefineShapeResult shapeResult{extrusion->toolShape, namedShape, false};
    if (!isFeatureGroupedByBody(object, context)) {
        const auto refined = applyRefineProperty(object, context, extrusion->toolShape, namedShape);
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
    context.mesh[object.name] = geometry::meshForShape(tool);
    context.subshapes[object.name] = topo::subshapeMapForShape(tool);
    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "sub"},
        {"method", extrusion->method},
        {"source_profile", extrusion->profile.object},
        {"bbox", extrusion->bbox},
        {"volume", extrusion->volume},
        {"kernel", geometry::kernelVersion()},
    };
    if (extrusion->topoNamingKnownGap) {
        result["topo_naming"] = "known_gap:taper_history";
        if (namedShape) {
            result["topo_naming_history"] = "history_partial:taper";
        }
    }
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::features
