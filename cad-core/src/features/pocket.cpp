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
    if (!rejectUnsupportedProperties(object, context, {"Profile", "Type", "Length", "Reversed", "SideType"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto extrusion = buildLengthExtrusion(object, context, AddSubMode::Subtractive, "Pocket");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const TopoDS_Shape tool = extrusion->toolShape;
    context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, tool};
    context.mesh[object.name] = geometry::meshForShape(tool);
    context.subshapes[object.name] = topo::subshapeMapForShape(tool);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"add_sub", "sub"},
        {"source_profile", extrusion->profile.object},
        {"bbox", extrusion->bbox},
        {"volume", extrusion->volume},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
