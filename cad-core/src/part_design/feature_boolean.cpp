#include "cad_core/part_design/feature_boolean.h"

#include "cad_core/app/property.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

std::string enumNameFromIndex(double value, const std::vector<std::string>& names)
{
    const auto index = static_cast<std::size_t>(value);
    if (std::abs(value - static_cast<double>(index)) > Precision::Confusion() || index >= names.size()) {
        return {};
    }
    return names[index];
}

std::string readBooleanType(const app::DocumentObject& object)
{
    const std::vector<std::string> types {"Fuse", "Cut", "Common"};
    if (const auto value = app::readString(object, "Type")) {
        return *value;
    }
    if (const auto value = app::readNumber(object, "Type")) {
        const std::string name = enumNameFromIndex(*value, types);
        return name.empty() ? "Fuse" : name;
    }
    return "Fuse";
}

std::optional<part::BooleanOperation> operationFromType(const app::DocumentObject& object,
                                                        runtime::ComputeContext& context,
                                                        const std::string& type)
{
    if (type == "Fuse") {
        return part::BooleanOperation::Fuse;
    }
    if (type == "Cut") {
        return part::BooleanOperation::Cut;
    }
    if (type == "Common") {
        return part::BooleanOperation::Common;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureBoolean.cpp
    // ::Boolean::execute(), only accepts "Fuse", "Cut" and "Common"; LinkStage3-only
    // Compound/Section branches are commented as pending a product decision.
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Unsupported PartDesign Boolean Type=" + type,
                           object.name,
                           "Type");
    return std::nullopt;
}

std::optional<part::NamedShapeSource> sourceForObject(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context,
                                                      const std::string& target,
                                                      const std::string& property)
{
    const auto shapeIt = context.shapes.find(target);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid
        || shapeIt->second.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + target + " did not produce a solid",
                               object.name,
                               property,
                               "runtime",
                               target);
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(target);
    if (namedShapeIt != context.namedShapes.end()) {
        return part::NamedShapeSource{namedShapeIt->second.owner, shapeIt->second.shape, &namedShapeIt->second};
    }
    return part::NamedShapeSource{target, shapeIt->second.shape, nullptr};
}

int solidCount(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

std::string shapeKind(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        case TopAbs_SHAPE:
            break;
    }
    return "occt_shape";
}

}  // namespace

void executeBoolean(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureBoolean.cpp
    // ::Boolean::execute(), "TypeEnums[] = {\"Fuse\", \"Cut\", \"Common\"}", rejects Cut
    // without BaseFeature, treats the last Group item as base when BaseFeature is absent, then
    // calls "result.makeElementBoolean(op, shapes, nullptr, FuzzyTolerance.getValue())".
    if (!runtime::rejectUnsupportedProperties(object, context, {"Type", "Group", "BaseFeature", "Refine", "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::string type = readBooleanType(object);
    const auto operation = operationFromType(object, context, type);
    if (!operation) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Group")) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "PartDesign Boolean Group must list tool solids",
                               object.name,
                               "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<app::Link> tools = app::readLinks(object, "Group");
    std::vector<part::NamedShapeSource> sources;
    if (const auto baseFeature = app::readLink(object, "BaseFeature")) {
        if (!baseFeature->object.empty()) {
            auto source = sourceForObject(object, context, baseFeature->object, "BaseFeature");
            if (!source) {
                context.objects[object.name] = {{"status", "error"}};
                return;
            }
            sources.push_back(*source);
        }
    }

    if (sources.empty()) {
        if (*operation == part::BooleanOperation::Cut) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_target",
                                   "Cannot do boolean cut without BaseFeature",
                                   object.name,
                                   "BaseFeature");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        if (tools.empty()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_target",
                                   "PartDesign Boolean needs a BaseFeature or Group base solid",
                                   object.name,
                                   "Group");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        const app::Link baseTool = tools.back();
        tools.pop_back();
        auto source = sourceForObject(object, context, baseTool.object, "Group");
        if (!source) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        sources.push_back(*source);
    }

    if (tools.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_target",
                               "PartDesign Boolean Group must contain at least one tool solid",
                               object.name,
                               "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<std::string> toolNames;
    for (const auto& tool : tools) {
        auto source = sourceForObject(object, context, tool.object, "Group");
        if (!source) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        toolNames.push_back(tool.object);
        sources.push_back(*source);
    }

    const auto build = part::makeElementBooleanFromSources(object.name, sources, *operation);
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build.error.empty() ? "Boolean operation failed" : build.error,
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto refined = runtime::applyPartDesignFeatureRefineProperty(object, context, build.shape, build.namedShape);
    if (!refined) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const int solids = solidCount(refined->shape);
    if (solids != 1) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "PartDesign Boolean result must be a single solid",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, refined->shape};
    context.namedShapes[object.name] = refined->namedShape
        ? *refined->namedShape
        : part::indexedNamedShapeForObject(object.name, refined->shape);
    context.mesh[object.name] = cad_core::part::meshForShape(refined->shape);
    context.subshapes[object.name] = part::subshapeMapForShape(refined->shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", shapeKind(refined->shape)},
        {"body_mode", "replace"},
        {"boolean_type", type},
        {"tools", toolNames},
        {"bbox", cad_core::part::bboxForShape(refined->shape)},
        {"volume", cad_core::part::volumeForShape(refined->shape)},
        {"topo_naming_history", "maker_history:boolean"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (refined->applied) {
        context.objects[object.name]["refine"] = "applied";
    }
}

}  // namespace cad_core::part_design
