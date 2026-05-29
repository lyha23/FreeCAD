#include "cad_core/features/part.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

namespace cad_core::features {

void executePart(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // initializes GroupExtension, while GeoFeatureGroupExtension provides placement/group
    // semantics. cad-core keeps App::Part as a container and exposes a single child solid
    // only as the frontend display result for the current CAD Core adapter.
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Group", "Type", "Id", "Uid", "Material", "Meta", "License", "LicenseURL", "Color"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<document::Link> links = document::readLinks(object, "Group");
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
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
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
    context.mesh[object.name] = geometry::meshForShape(displayShape->shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(displayShape->shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"container", "geo_feature_group"},
        {"display_object", displayObject},
        {"group", group},
        {"shape", "occt_solid"},
        {"bbox", geometry::bboxForShape(displayShape->shape)},
        {"volume", geometry::volumeForShape(displayShape->shape)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
