#include "cad_core/part/part_feature.h"

#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <string>
#include <vector>

namespace cad_core::part
{

void executePart(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // initializes GroupExtension, while GeoFeatureGroupExtension provides placement/group
    // semantics. cad-core keeps App::Part as a container and exposes a single child solid
    // only as the frontend display result for the current CAD Core adapter.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Group", "Type", "Id", "Uid", "Material", "Meta", "License", "LicenseURL", "Color"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<app::Link> links = app::readLinks(object, "Group");
    if (links.empty()) {
        context.objects[object.name] = {
            {"status", "ok"},
            {"container", "geo_feature_group"},
            {"group", nlohmann::json::array()},
        };
        return;
    }

    nlohmann::json group = nlohmann::json::array();
    const runtime::ShapeValue* displayShape = nullptr;
    std::string displayObject;
    for (const auto& link : links) {
        group.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt != context.shapes.end()
            && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
            displayShape = &shapeIt->second;
            displayObject = link.object;
        }
    }

    if (displayShape == nullptr) {
        context.objects[object.name] = {
            {"status", "ok"},
            {"container", "geo_feature_group"},
            {"group", group},
        };
        return;
    }

    context.shapes[object.name] = *displayShape;
    context.mesh[object.name] = cad_core::part::meshForShape(displayShape->shape);
    context.subshapes[object.name] = part::subshapeMapForShape(displayShape->shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"container", "geo_feature_group"},
        {"display_object", displayObject},
        {"group", group},
        {"shape", "occt_solid"},
        {"bbox", cad_core::part::bboxForShape(displayShape->shape)},
        {"volume", cad_core::part::volumeForShape(displayShape->shape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
}

}  // namespace cad_core::part
