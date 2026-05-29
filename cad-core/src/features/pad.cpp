#include "cad_core/features/pad.h"

#include "cad_core/features/feature_extrude.h"
#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

namespace cad_core::features {

void executePad(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/FeaturePad.cpp Pad::execute()
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
                                      "AlongSketchNormal"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Additive, "Pad");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const TopoDS_Shape solid = extrusion->toolShape;
    const nlohmann::json mesh = geometry::meshForShape(solid);
    const nlohmann::json subshapeMap = topo::subshapeMapForShape(solid);

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt};
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
        {"kernel", geometry::kernelVersion()},
    };
    if (extrusion->topoNamingKnownGap) {
        result["topo_naming"] = "known_gap:taper_history";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::features
