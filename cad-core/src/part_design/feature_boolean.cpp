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

enum class ProductBooleanOperation
{
    Fuse,
    Cut,
    Common,
    Compound,
    Section,
};

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
    const std::vector<std::string> types {"Fuse", "Cut", "Common", "Compound", "Section"};
    if (const auto value = app::readString(object, "Type")) {
        return *value;
    }
    if (const auto value = app::readNumber(object, "Type")) {
        const std::string name = enumNameFromIndex(*value, types);
        return name.empty() ? "Fuse" : name;
    }
    return "Fuse";
}

std::optional<ProductBooleanOperation> operationFromType(const app::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const std::string& type)
{
    if (type == "Fuse") {
        return ProductBooleanOperation::Fuse;
    }
    if (type == "Cut") {
        return ProductBooleanOperation::Cut;
    }
    if (type == "Common") {
        return ProductBooleanOperation::Common;
    }
    if (type == "Compound") {
        return ProductBooleanOperation::Compound;
    }
    if (type == "Section") {
        return ProductBooleanOperation::Section;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureBoolean.cpp
    // ::Boolean::execute(), only exposes "TypeEnums[] = {\"Fuse\", \"Cut\", \"Common\"}";
    // C5.1 productizes only the commented LinkStage3 "Compound" / "Section" paths through
    // src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementBoolean().
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Unsupported PartDesign Boolean Type=" + type,
                           object.name,
                           "Type");
    return std::nullopt;
}

std::optional<part::BooleanOperation> partBooleanOperation(ProductBooleanOperation operation)
{
    switch (operation) {
        case ProductBooleanOperation::Fuse:
            return part::BooleanOperation::Fuse;
        case ProductBooleanOperation::Cut:
            return part::BooleanOperation::Cut;
        case ProductBooleanOperation::Common:
            return part::BooleanOperation::Common;
        case ProductBooleanOperation::Compound:
        case ProductBooleanOperation::Section:
            return std::nullopt;
    }
    return std::nullopt;
}

const app::DocumentObject* owningBody(const app::DocumentObject& object,
                                      const runtime::ComputeContext& context)
{
    const auto parentIt = context.parentGroupByObject.find(object.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return nullptr;
    }
    const auto bodyIt = context.documentObjects.find(parentIt->second);
    if (bodyIt == context.documentObjects.end() || bodyIt->second == nullptr
        || bodyIt->second->typeId != "PartDesign::Body") {
        return nullptr;
    }
    return bodyIt->second;
}

bool owningBodyAllowsCompound(const app::DocumentObject& object,
                              const runtime::ComputeContext& context)
{
    const app::DocumentObject* body = owningBody(object, context);
    if (body == nullptr) {
        return false;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::Feature::singleSolidRuleMode(), "When the feature is not part of an body" it
    // enforces the single-solid rule; otherwise it reads body->AllowCompound.
    return app::readBool(*body, "AllowCompound").value_or(true);
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

bool containsSubshape(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next()) {
        return true;
    }
    return false;
}

TopoDS_Shape firstSolid(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        return explorer.Current();
    }
    return shape;
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

bool appendToolSource(const app::DocumentObject& object,
                      runtime::ComputeContext& context,
                      const app::Link& tool,
                      std::vector<part::NamedShapeSource>& sources,
                      std::vector<std::string>& toolNames,
                      bool isBaseFallback)
{
    if (tool.object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_link_value",
                               isBaseFallback ? "PartDesign Boolean Group base link is null"
                                              : "PartDesign Boolean Group contains a null tool link",
                               object.name,
                               "Group",
                               "runtime");
        return false;
    }
    auto source = sourceForObject(object, context, tool.object, "Group");
    if (!source) {
        return false;
    }
    if (!isBaseFallback) {
        toolNames.push_back(tool.object);
    }
    sources.push_back(*source);
    return true;
}

part::NamedShapeBuild buildProductBoolean(const std::string& owner,
                                          const std::vector<part::NamedShapeSource>& sources,
                                          ProductBooleanOperation operation)
{
    if (const auto partOperation = partBooleanOperation(operation)) {
        return part::makeElementBooleanFromSources(owner, sources, *partOperation);
    }
    if (operation == ProductBooleanOperation::Compound) {
        return part::makeElementCompoundFromSources(owner, sources);
    }
    return part::makeElementSectionFromSources(owner, sources, false);
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
    std::vector<std::string> toolNames;
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
        if (*operation == ProductBooleanOperation::Cut) {
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
        if (!appendToolSource(object, context, baseTool, sources, toolNames, true)) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
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

    for (const auto& tool : tools) {
        if (!appendToolSource(object, context, tool, sources, toolNames, false)) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
    }

    const auto build = buildProductBoolean(object.name, sources, *operation);
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build.error.empty() ? "Boolean operation failed" : build.error,
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (*operation == ProductBooleanOperation::Section && !containsSubshape(build.shape, TopAbs_EDGE)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "no_intersection",
                               "PartDesign Boolean Section produced no section edges",
                               object.name,
                               "Group",
                               "part_design.boolean_section");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto refined = runtime::applyPartDesignFeatureRefineProperty(object, context, build.shape, build.namedShape);
    if (!refined) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const int solids = solidCount(refined->shape);
    const bool allowCompound = owningBodyAllowsCompound(object, context);
    if (*operation != ProductBooleanOperation::Section && !allowCompound && solids != 1) {
        const app::DocumentObject* body = owningBody(object, context);
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "multiple_solids_disallowed",
                               "Result has multiple solids: enable 'Allow Compound' in the active body.",
                               object.name,
                               "AllowCompound",
                               "part_design.single_solid_rule",
                               body == nullptr ? std::string {} : body->name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape resultShape = refined->shape;
    std::optional<part::NamedShape> resultNamedShape = refined->namedShape;
    if (*operation != ProductBooleanOperation::Section && !allowCompound && solids == 1) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
        // ::Feature::getSolid(), when the single-solid rule is enforced and one solid exists,
        // returns "shape.getSubTopoShape(TopAbs_SOLID, 1)" instead of the boolean compound.
        resultShape = firstSolid(refined->shape);
        if (resultNamedShape) {
            resultNamedShape->shape = resultShape;
        }
    }

    const auto resultKind = *operation == ProductBooleanOperation::Section
        ? runtime::ShapeValue::Kind::PartPrimitive
        : runtime::ShapeValue::Kind::Solid;
    context.shapes[object.name] = runtime::ShapeValue{resultKind, resultShape};
    context.namedShapes[object.name] = resultNamedShape
        ? *resultNamedShape
        : part::indexedNamedShapeForObject(object.name, resultShape);
    context.mesh[object.name] = cad_core::part::meshForShape(resultShape);
    context.subshapes[object.name] = part::subshapeMapForShape(resultShape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", shapeKind(resultShape)},
        {"body_mode", *operation == ProductBooleanOperation::Section ? "section_non_solid" : "replace"},
        {"boolean_type", type},
        {"tools", toolNames},
        {"allow_compound", allowCompound},
        {"solid_count", solids},
        {"bbox", cad_core::part::bboxForShape(resultShape)},
        {"volume", cad_core::part::volumeForShape(resultShape)},
        {"topo_naming_history", "maker_history:boolean"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (refined->applied) {
        context.objects[object.name]["refine"] = "applied";
    }
}

}  // namespace cad_core::part_design
