#include "cad_core/features/part_boolean.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Iterator.hxx>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::features
{

namespace
{

struct BooleanInput
{
    std::string objectName;
    TopoDS_Shape shape;
    const topo::NamedShape* namedShape = nullptr;
};

std::optional<BooleanInput> resolveBooleanInput(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (document::propertyValue(object, property) == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " link is not set",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const auto link = document::readLink(object, property);
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " must be an App::PropertyLink",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link->object + " did not produce a shape",
            object.name,
            property,
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link->object);
    return BooleanInput {
        link->object,
        shapeIt->second.shape,
        namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
    };
}

topo::NamedShapeSource sourceForInput(const BooleanInput& input)
{
    return topo::NamedShapeSource {
        input.namedShape != nullptr ? input.namedShape->owner : input.objectName,
        input.shape,
        input.namedShape
    };
}

std::vector<topo::NamedShapeSource> sourcesForInputs(const std::vector<BooleanInput>& inputs)
{
    std::vector<topo::NamedShapeSource> sources;
    sources.reserve(inputs.size());
    for (const auto& input : inputs) {
        sources.push_back(sourceForInput(input));
    }
    return sources;
}

std::vector<BooleanInput> expandSingleCompoundInput(const std::vector<BooleanInput>& inputs)
{
    if (inputs.size() != 1U || inputs.front().shape.ShapeType() != TopAbs_COMPOUND) {
        return inputs;
    }

    std::vector<BooleanInput> children;
    int childIndex = 1;
    for (TopoDS_Iterator it(inputs.front().shape); it.More(); it.Next(), ++childIndex) {
        children.push_back(
            BooleanInput {
                inputs.front().objectName + ".Child" + std::to_string(childIndex),
                it.Value(),
                nullptr,
            }
        );
    }
    return children.empty() ? inputs : children;
}

std::string shapeLabel(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SHELL:
            return "occt_shell";
        default:
            return "occt_shape";
    }
}

std::optional<std::vector<BooleanInput>> resolveShapeList(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (document::propertyValue(object, property) == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " links are not set",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    const std::vector<document::Link> links = document::readLinks(object, property);
    if (links.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " must contain at least two shape links",
            object.name,
            property,
            "runtime"
        );
        return std::nullopt;
    }

    std::vector<BooleanInput> inputs;
    inputs.reserve(links.size());
    for (const auto& link : links) {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_link_target",
                property + " target " + link.object + " did not produce a shape",
                object.name,
                property,
                "runtime",
                link.object
            );
            return std::nullopt;
        }
        const auto namedShapeIt = context.namedShapes.find(link.object);
        inputs.push_back(
            BooleanInput {
                link.object,
                shapeIt->second.shape,
                namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
            }
        );
    }
    return inputs;
}

void publishBoolean(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<topo::NamedShape>& namedShape,
    const nlohmann::json& metadata,
    runtime::ShapeValue::Kind kind = runtime::ShapeValue::Kind::Solid
)
{
    context.shapes[object.name] = runtime::ShapeValue {kind, shape};
    context.mesh[object.name] = geometry::meshForShape(shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(shape);
    context.namedShapes[object.name] = namedShape
        ? *namedShape
        : topo::indexedNamedShapeForObject(object.name, shape);

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabel(shape);
    result["bbox"] = geometry::bboxForShape(shape);
    result["volume"] = geometry::volumeForShape(shape);
    result["kernel"] = geometry::kernelVersion();
    context.objects[object.name] = result;
}

std::optional<RefineShapeResult> refineBooleanBuild(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const topo::NamedShapeBuild& build,
    const std::string& failureMessage
);

void executeBinaryPartBoolean(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    topo::BooleanOperation operation,
    const std::string& operationName
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartBoolean.cpp
    // ::Boolean::execute(), reads "Base" and "Tool" with ShapeOption::ResolveLink |
    // ShapeOption::Transform, calls makeOperation(BaseShape, ToolShape), routes the maker through
    // "res.makeElementShape(*mkBool, shapes, opCode())", then applies "res.makeElementRefine()"
    // when Refine is true.
    if (!rejectUnsupportedProperties(object, context, {"Base", "Tool", "History", "Refine"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto base = resolveBooleanInput(object, context, "Base");
    const auto tool = resolveBooleanInput(object, context, "Tool");
    if (!base || !tool) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        topo::NamedShapeBuild build = topo::makeElementBooleanFromSources(
            object.name,
            {sourceForInput(*base), sourceForInput(*tool)},
            operation
        );
        if (!build.error.empty() || build.shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                build.error.empty() ? "Boolean operation failed" : build.error,
                object.name,
                {},
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        const auto refined = applyRefineProperty(object, context, build.shape, build.namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", operationName},
             {"base", base->objectName},
             {"tool", tool->objectName},
             {"refine", document::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Boolean operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executeSection(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartSection.cpp
    // ::Section::makeOperation(), reads inherited Boolean "Base" / "Tool", configures
    // FCBRepAlgoAPI_Section with "Approximation", calls setAutoFuzzy(), Build(), and returns
    // opCode() Part::OpCodes::Section for Part::Boolean::execute() to consume via
    // "res.makeElementShape(*mkBool, shapes, opCode())".
    if (!rejectUnsupportedProperties(
            object,
            context,
            {"Base", "Tool", "History", "Refine", "Approximation"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto base = resolveBooleanInput(object, context, "Base");
    const auto tool = resolveBooleanInput(object, context, "Tool");
    if (!base || !tool) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const bool approximation = document::readBool(object, "Approximation").value_or(false);
    const auto build = topo::makeElementSectionFromSources(
        object.name,
        {sourceForInput(*base), sourceForInput(*tool)},
        approximation
    );
    const auto refined = refineBooleanBuild(object, context, build, "Section failed");
    if (!refined) {
        return;
    }

    publishBoolean(
        object,
        context,
        refined->shape,
        refined->namedShape,
        {{"boolean", "section"},
         {"base", base->objectName},
         {"tool", tool->objectName},
         {"approximation", approximation},
         {"refine", document::readBool(object, "Refine").value_or(false)},
         {"refined", refined->applied}},
        runtime::ShapeValue::Kind::PartPrimitive
    );
}

std::optional<RefineShapeResult> refineBooleanBuild(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const topo::NamedShapeBuild& build,
    const std::string& failureMessage
)
{
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            build.error.empty() ? failureMessage : build.error,
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }
    const auto refined = applyRefineProperty(object, context, build.shape, build.namedShape);
    if (!refined) {
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }
    return refined;
}

void executeMultiFuse(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartFuse.cpp
    // ::MultiFuse::execute(), reads "Shapes", expands a single compound argument to children,
    // requires at least two shapes, runs FCBRepAlgoAPI_Fuse with first shape as Arguments and
    // remaining shapes as Tools, then calls "makeShapeWithElementMap(mkFuse.Shape(),
    // MapperMaker(mkFuse), shapes, OpCodes::Fuse)".
    if (!rejectUnsupportedProperties(object, context, {"Shapes", "History", "Refine"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Shapes");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    *inputs = expandSingleCompoundInput(*inputs);
    if (inputs->size() < 2U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Not enough shape objects linked",
            object.name,
            "Shapes",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = topo::makeElementBooleanFromSources(
            object.name,
            sourcesForInputs(*inputs),
            topo::BooleanOperation::Fuse
        );
        const auto refined = refineBooleanBuild(object, context, build, "MultiFusion failed");
        if (!refined) {
            return;
        }

        nlohmann::json shapes = nlohmann::json::array();
        for (const auto& input : *inputs) {
            shapes.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", "multi_fuse"},
             {"shapes", shapes},
             {"refine", document::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString() : "MultiFusion failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

std::string readMultiCommonBehavior(const document::DocumentObject& object)
{
    if (const auto text = document::readString(object, "Behavior")) {
        return *text == "CommonOfFirstAndRest" ? "CommonOfFirstAndRest" : "CommonOfAllShapes";
    }

    const auto* value = document::propertyValue(object, "Behavior");
    if (value == nullptr) {
        return "CommonOfAllShapes";
    }
    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (payload->is_number_integer() && payload->get<int>() == 1) {
        return "CommonOfFirstAndRest";
    }
    return "CommonOfAllShapes";
}

topo::NamedShapeBuild makeCommonOfAllShapes(
    const std::string& owner,
    const std::vector<BooleanInput>& inputs
)
{
    if (inputs.empty()) {
        return topo::NamedShapeBuild {TopoDS_Shape {}, std::nullopt, "Input shape is null"};
    }
    if (inputs.size() == 1U) {
        const auto source = sourceForInput(inputs.front());
        return topo::NamedShapeBuild {
            inputs.front().shape,
            source.namedShape != nullptr
                ? std::optional<topo::NamedShape>(*source.namedShape)
                : std::optional<topo::NamedShape>(
                      topo::indexedNamedShapeForObject(owner, inputs.front().shape)
                  ),
            {},
        };
    }

    TopoDS_Shape currentShape = inputs.front().shape;
    std::optional<topo::NamedShape> currentNamedShape;
    if (inputs.front().namedShape != nullptr) {
        currentNamedShape = *inputs.front().namedShape;
    }
    std::string currentOwner = inputs.front().objectName;

    for (std::size_t index = 1; index < inputs.size(); ++index) {
        const auto currentSource = topo::NamedShapeSource {
            currentNamedShape ? currentNamedShape->owner : currentOwner,
            currentShape,
            currentNamedShape ? &*currentNamedShape : nullptr,
        };
        const auto step = topo::makeElementBooleanFromSources(
            owner,
            {currentSource, sourceForInput(inputs.at(index))},
            topo::BooleanOperation::Common
        );
        if (!step.error.empty() || step.shape.IsNull()) {
            return step;
        }
        currentShape = step.shape;
        currentNamedShape = step.namedShape;
        currentOwner = owner + ".CommonStep" + std::to_string(index);
    }

    return topo::NamedShapeBuild {currentShape, std::move(currentNamedShape), {}};
}

void executeMultiCommon(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCommon.cpp
    // ::MultiCommon::execute(), reads "Shapes"; default Behavior is "CommonOfAllShapes",
    // which intersects one shape at a time, while "CommonOfFirstAndRest" calls one
    // makeElementBoolean(OpCodes::Common, shapes) for FreeCAD 1.0 compatibility.
    if (!rejectUnsupportedProperties(object, context, {"Shapes", "History", "Refine", "Behavior"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Shapes");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::string behavior = readMultiCommonBehavior(object);
    if (behavior == "CommonOfAllShapes") {
        *inputs = expandSingleCompoundInput(*inputs);
    }
    if (inputs->empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Input shape is null",
            object.name,
            "Shapes",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = behavior == "CommonOfFirstAndRest"
            ? topo::makeElementBooleanFromSources(
                  object.name,
                  sourcesForInputs(*inputs),
                  topo::BooleanOperation::Common
              )
            : makeCommonOfAllShapes(object.name, *inputs);
        const auto refined = refineBooleanBuild(object, context, build, "Resulting shape is null");
        if (!refined) {
            return;
        }

        nlohmann::json shapes = nlohmann::json::array();
        for (const auto& input : *inputs) {
            shapes.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            refined->shape,
            refined->namedShape,
            {{"boolean", "multi_common"},
             {"behavior", behavior},
             {"shapes", shapes},
             {"refine", document::readBool(object, "Refine").value_or(false)},
             {"refined", refined->applied}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Common operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executeXor(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools/SplitFeatures.py
    // ::FeatureXOR.execute(), reads "Objects", expands a single compound via childShapes(),
    // requires at least two shapes, then calls SplitAPI.xor(shapes, Tolerance). SplitAPI.xor()
    // keeps pieces whose source count is odd; the C++ topo equivalent is
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementXor(), which performs Fuse, Common, then Cut.
    //
    // FreeCAD stores this command as Part::FeaturePython with Proxy.Type "FeatureXOR".
    // cad-core exposes Part::XOR / Part::FeatureXOR as typed aliases until Python proxy
    // dispatch is migrated into document/runtime.
    if (!rejectUnsupportedProperties(object, context, {"Objects", "Tolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double tolerance = document::readNumber(object, "Tolerance").value_or(0.0);
    if (tolerance != 0.0) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Part::XOR Tolerance is not supported yet",
            object.name,
            "Tolerance",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto inputs = resolveShapeList(object, context, "Objects");
    if (!inputs) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    *inputs = expandSingleCompoundInput(*inputs);
    if (inputs->size() < 2U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "At least two shapes are needed for computing XOR",
            object.name,
            "Objects",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        const auto build = topo::makeElementXorFromSources(object.name, sourcesForInputs(*inputs));
        const auto result = refineBooleanBuild(object, context, build, "XOR operation failed");
        if (!result) {
            return;
        }

        nlohmann::json objects = nlohmann::json::array();
        for (const auto& input : *inputs) {
            objects.push_back(input.objectName);
        }
        publishBoolean(
            object,
            context,
            result->shape,
            result->namedShape,
            {{"boolean", "xor"}, {"objects", objects}, {"tolerance", tolerance}}
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "XOR operation failed",
            object.name,
            {},
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace

void executePartFuse(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartFuse.cpp
    // ::Fuse::makeOperation(), returns "new FCBRepAlgoAPI_Fuse(base, tool)" and opCode() is
    // Part::OpCodes::Fuse.
    executeBinaryPartBoolean(object, context, topo::BooleanOperation::Fuse, "fuse");
}

void executePartCut(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCut.cpp
    // ::Cut::makeOperation(), returns "new FCBRepAlgoAPI_Cut(base, tool)" and opCode() is
    // Part::OpCodes::Cut.
    executeBinaryPartBoolean(object, context, topo::BooleanOperation::Cut, "cut");
}

void executePartCommon(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartCommon.cpp
    // ::Common::makeOperation(), returns "new FCBRepAlgoAPI_Common(base, tool)" and opCode() is
    // Part::OpCodes::Common.
    executeBinaryPartBoolean(object, context, topo::BooleanOperation::Common, "common");
}

void executePartSection(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    executeSection(object, context);
}

void executePartMultiFuse(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    executeMultiFuse(object, context);
}

void executePartMultiCommon(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    executeMultiCommon(object, context);
}

void executePartXor(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    executeXor(object, context);
}

}  // namespace cad_core::features
