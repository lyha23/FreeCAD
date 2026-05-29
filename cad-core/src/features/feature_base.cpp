#include "cad_core/features/feature_base.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

namespace cad_core::features {

void executeFeatureBase(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/PartDesign/App/FeatureBase.cpp FeatureBase::execute().
    if (!rejectUnsupportedProperties(object, context, {"BaseFeature"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("BaseFeature")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "BaseFeature link is not set", object.name, "BaseFeature");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto baseLink = document::readLink(object, "BaseFeature");
    if (!baseLink) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "BaseFeature must link to a solid feature", object.name, "BaseFeature");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto shapeIt = context.shapes.find(baseLink->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "BaseFeature target " + baseLink->object + " did not produce a solid",
                               object.name,
                               "BaseFeature",
                               "runtime",
                               baseLink->object);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape solid = shapeIt->second.shape;
    if (const auto placement = document::readPlacement(object, "Placement")) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureBase.cpp::FeatureBase::execute(),
        // fetches the linked base with ShapeOption::Transform; cad-core applies the document-normalized
        // FeatureBase Placement here so the base solid enters Body in the same coordinate frame.
        solid = geometry::transformShape(solid, geometry::placementFromComponents(placement->base, placement->rotation));
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.mesh[object.name] = geometry::meshForShape(solid);
    context.subshapes[object.name] = topo::subshapeMapForShape(solid);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"base_feature", baseLink->object},
        {"bbox", geometry::bboxForShape(solid)},
        {"volume", geometry::volumeForShape(solid)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
