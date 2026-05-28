#include "cad_core/features/body.h"

#include "cad_core/features/feature_executor.h"

#include <algorithm>

namespace cad_core::features {

void executeBody(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic source: src/Mod/PartDesign/App/Body.cpp Body::execute().
    if (!rejectUnsupportedProperties(object, context, {"Group", "Tip"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Group") || !object.properties.at("Group").is_array()) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Group must be a list of object links", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Tip")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto tip = document::readLink(object.properties.at("Tip"));
    if (!tip) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<std::string> groupNames;
    for (const auto& item : object.properties.at("Group")) {
        auto link = document::readLink(item);
        if (!link) {
            runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Group item must be an object link", object.name, "Group");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        groupNames.push_back(link->object);
    }

    if (std::find(groupNames.begin(), groupNames.end(), tip->object) == groupNames.end()) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Tip is not present in Group", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto shapeIt = context.shapes.find(tip->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body Tip did not produce a shape", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.shapes[object.name] = shapeIt->second;
    context.objects[object.name] = {
        {"status", "ok"},
        {"tip", tip->object},
        {"group", groupNames},
    };
}

}  // namespace cad_core::features

