#include "cad_core/part_design/feature_base.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"

namespace cad_core::part_design {

void executeFeatureBase(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/PartDesign/App/FeatureBase.cpp FeatureBase::execute().
    if (!runtime::rejectUnsupportedProperties(object, context, {"BaseFeature"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("BaseFeature")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "BaseFeature link is not set", object.name, "BaseFeature");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto baseLink = app::readLink(object, "BaseFeature");
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
    if (const auto placement = app::readPlacement(object, "Placement")) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureBase.cpp::FeatureBase::execute(),
        // fetches the linked base with ShapeOption::Transform; cad-core applies the document-normalized
        // FeatureBase Placement here so the base solid enters Body in the same coordinate frame.
        solid = base::transformShape(solid, base::placementFromComponents(placement->base, placement->rotation));
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.mesh[object.name] = cad_core::part::meshForShape(solid);
    context.subshapes[object.name] = part::subshapeMapForShape(solid);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"base_feature", baseLink->object},
        {"bbox", cad_core::part::objectBBoxForShape(solid)},
        {"volume", cad_core::part::volumeForShape(solid)},
        {"kernel", cad_core::part::kernelVersion()},
    };
}

}  // namespace cad_core::part_design
