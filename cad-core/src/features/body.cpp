#include "cad_core/features/body.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <optional>

namespace cad_core::features {

namespace {

std::optional<std::vector<std::string>> readGroupNames(const nlohmann::json& value)
{
    std::vector<document::Link> links;
    if (value.is_array()) {
        for (const auto& item : value) {
            auto itemLinks = document::readLinks(item);
            links.insert(links.end(), itemLinks.begin(), itemLinks.end());
        }
    }
    else {
        links = document::readLinks(value);
    }

    if (links.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> names;
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

std::optional<TopoDS_Shape> fuseShapes(const TopoDS_Shape& base,
                                       const TopoDS_Shape& tool,
                                       const document::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& feature)
{
    BRepAlgoAPI_Fuse fuse(base, tool);
    fuse.Build();
    if (!fuse.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body could not fuse additive feature " + feature, object.name);
        return std::nullopt;
    }
    return fuse.Shape();
}

std::optional<TopoDS_Shape> cutShapes(const TopoDS_Shape& base,
                                      const TopoDS_Shape& tool,
                                      const document::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      const std::string& feature)
{
    BRepAlgoAPI_Cut cut(base, tool);
    cut.Build();
    if (!cut.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body could not cut subtractive feature " + feature, object.name);
        return std::nullopt;
    }
    return cut.Shape();
}

}  // namespace

void executeBody(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/Body.cpp Body::execute()
    // src/Mod/PartDesign/App/FeatureAddSub.cpp FeatureAddSub::getAddSubShape()
    if (!rejectUnsupportedProperties(object, context, {"Group", "Tip", "BaseFeature"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Group")) {
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

    auto group = readGroupNames(object.properties.at("Group"));
    if (!group) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Group item must be an object link", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const std::vector<std::string>& groupNames = *group;

    if (std::find(groupNames.begin(), groupNames.end(), tip->object) == groupNames.end()) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Tip is not present in Group", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<TopoDS_Shape> bodyShape;
    if (object.properties.contains("BaseFeature")) {
        const auto baseLink = document::readLink(object.properties.at("BaseFeature"));
        if (!baseLink) {
            runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body BaseFeature must link to a solid feature", object.name, "BaseFeature");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const auto baseIt = context.shapes.find(baseLink->object);
        if (baseIt == context.shapes.end() || baseIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "Body BaseFeature target " + baseLink->object + " did not produce a solid",
                                   object.name,
                                   "BaseFeature");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        bodyShape = baseIt->second.shape;
    }

    for (const auto& feature : groupNames) {
        const auto addSubIt = context.addSubShapes.find(feature);
        if (addSubIt == context.addSubShapes.end()) {
            const auto shapeIt = context.shapes.find(feature);
            if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid && !bodyShape) {
                bodyShape = shapeIt->second.shape;
            }
            continue;
        }

        const runtime::AddSubShape& addSubShape = addSubIt->second;
        if (addSubShape.addShape) {
            if (!bodyShape) {
                bodyShape = *addSubShape.addShape;
            }
            else {
                bodyShape = fuseShapes(*bodyShape, *addSubShape.addShape, object, context, feature);
            }
        }
        else if (addSubShape.subShape) {
            if (!bodyShape) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Body cannot apply subtractive feature " + feature + " without a base solid",
                                       object.name,
                                       "Group");
                context.objects[object.name] = {{"status", "error"}};
                return;
            }
            bodyShape = cutShapes(*bodyShape, *addSubShape.subShape, object, context, feature);
        }

        if (!bodyShape) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        if (feature == tip->object) {
            break;
        }
    }

    if (!bodyShape) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body Tip did not produce a shape", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, *bodyShape};
    context.mesh[object.name] = geometry::meshForShape(*bodyShape);
    context.subshapes[object.name] = topo::subshapeMapForShape(*bodyShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"tip", tip->object},
        {"group", groupNames},
        {"shape", "occt_solid"},
        {"bbox", geometry::bboxForShape(*bodyShape)},
        {"volume", geometry::volumeForShape(*bodyShape)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
