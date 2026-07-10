#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_TransitionMode.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct SweepInput
{
    std::string objectName;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

struct AdvancedLinkShape
{
    std::string objectName;
    std::string subname;
    TopoDS_Shape shape;
};

struct AdvancedVertexLink
{
    std::string objectName;
    std::string subname;
    TopoDS_Vertex vertex;
};

struct AdvancedSweepOptions
{
    PipeShellOptions pipeOptions;
    nlohmann::json metadata = nlohmann::json::object();
    std::optional<AdvancedLinkShape> auxiliarySpine;
    std::optional<AdvancedLinkShape> spineSupport;
    std::string binormalProperty = "Binormal";
};

struct HelperLifecycleProfile
{
    std::string objectName;
    TopoDS_Shape shape;
};

void addSweepDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {},
    const std::string& subname = {}
)
{
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        code,
        message,
        object.name,
        property,
        "runtime",
        target,
        subname
    );
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_sweep"}};
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

const nlohmann::json& propertyPayload(const nlohmann::json& value)
{
    if (value.is_object() && value.contains("PropertyType") && value.contains("value")) {
        return value.at("value");
    }
    return value;
}

bool isFiniteJsonNumber(const nlohmann::json& value)
{
    return value.is_number() && std::isfinite(value.get<double>());
}

std::optional<bool> readJsonBool(const nlohmann::json& value)
{
    const nlohmann::json& payload = propertyPayload(value);
    if (!payload.is_boolean()) {
        return std::nullopt;
    }
    return payload.get<bool>();
}

int countSubshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

std::string helperShapeTypeName(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "Compound";
        case TopAbs_COMPSOLID:
            return "CompSolid";
        case TopAbs_SOLID:
            return "Solid";
        case TopAbs_SHELL:
            return "Shell";
        case TopAbs_FACE:
            return "Face";
        case TopAbs_WIRE:
            return "Wire";
        case TopAbs_EDGE:
            return "Edge";
        case TopAbs_VERTEX:
            return "Vertex";
        default:
            return "Shape";
    }
}

nlohmann::json helperShapeSummary(const TopoDS_Shape& shape)
{
    return {
        {"is_null", shape.IsNull()},
        {"shape_type", shape.IsNull() ? "Null" : helperShapeTypeName(shape)},
        {"bbox", shape.IsNull() ? nlohmann::json::object() : cad_core::part::objectBBoxForShape(shape)},
        {"topology_counts",
         {
             {"solids", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_SOLID)},
             {"shells", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_SHELL)},
             {"faces", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_FACE)},
             {"wires", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_WIRE)},
             {"edges", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_EDGE)},
             {"vertices", shape.IsNull() ? 0 : countSubshapes(shape, TopAbs_VERTEX)},
         }},
        {"volume", shape.IsNull() ? 0.0 : cad_core::part::volumeForShape(shape)},
    };
}

nlohmann::json helperReturnNone()
{
    return {{"return_kind", "none"}, {"value", nullptr}};
}

nlohmann::json helperReturnBool(bool value)
{
    return {{"return_kind", "bool"}, {"value", value}};
}

nlohmann::json helperReturnInt(int value)
{
    return {{"return_kind", "int"}, {"value", value}};
}

nlohmann::json helperReturnShape(const TopoDS_Shape& shape)
{
    return {{"return_kind", "shape"}, {"shape", helperShapeSummary(shape)}};
}

nlohmann::json helperReturnShapeList(const TopTools_ListOfShape& shapes)
{
    nlohmann::json items = nlohmann::json::array();
    for (TopTools_ListIteratorOfListOfShape it(shapes); it.More(); it.Next()) {
        items.push_back(helperReturnShape(it.Value()));
    }
    return {{"return_kind", "list"}, {"length", items.size()}, {"items", std::move(items)}};
}

void failHelperOperation(
    nlohmann::json& operation,
    nlohmann::json& diagnostics,
    const std::string& type,
    const std::string& message
)
{
    nlohmann::json exception = {{"type", type}, {"message", message}};
    operation["ok"] = false;
    operation["exception"] = exception;
    diagnostics.push_back(std::move(exception));
}

void succeedHelperOperation(nlohmann::json& operation, nlohmann::json result)
{
    operation["ok"] = true;
    operation["return"] = std::move(result);
}

std::string standardFailureMessage(const Standard_Failure& failure)
{
    const char* message = failure.GetMessageString();
    return message != nullptr && *message != '\0' ? std::string(message) : std::string("OCCError");
}

BRepBuilderAPI_TransitionMode helperTransitionMode(int transition)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::TransitionEnums order is "Transformed", "Right corner", "Round corner".
    switch (transition) {
        case 1:
            return BRepBuilderAPI_RightCorner;
        case 2:
            return BRepBuilderAPI_RoundCorner;
        default:
            return BRepBuilderAPI_Transformed;
    }
}

std::optional<TopoDS_Wire> wireFromShapeForHelper(
    const TopoDS_Shape& shape,
    const std::string& label,
    std::string& error
)
{
    if (shape.IsNull()) {
        error = label + " is null";
        return std::nullopt;
    }
    if (shape.ShapeType() == TopAbs_WIRE) {
        return TopoDS::Wire(shape);
    }

    std::optional<TopoDS_Wire> singleWire;
    for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        if (singleWire) {
            error = label + " must resolve to one wire";
            return std::nullopt;
        }
        singleWire = TopoDS::Wire(explorer.Current());
    }
    if (singleWire) {
        return singleWire;
    }

    BRepBuilderAPI_MakeWire wireBuilder;
    bool hasEdge = false;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        wireBuilder.Add(TopoDS::Edge(explorer.Current()));
        hasEdge = true;
    }
    if (!hasEdge) {
        error = label + " cannot form a wire";
        return std::nullopt;
    }
    wireBuilder.Build();
    if (!wireBuilder.IsDone() || wireBuilder.Wire().IsNull()) {
        error = label + " cannot form a wire";
        return std::nullopt;
    }
    TopoDS_Wire wire = wireBuilder.Wire();
    BRepLib::BuildCurves3d(wire);
    BRepLib::SameParameter(wire);
    return wire;
}

TopoDS_Shape profileShapeForHelper(const TopoDS_Shape& shape)
{
    std::string unused;
    if (const auto wire = wireFromShapeForHelper(shape, "profile", unused)) {
        return *wire;
    }
    return shape;
}

std::string configureHelperPipeShell(
    BRepOffsetAPI_MakePipeShell& helper,
    const PipeShellOptions& options
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::setFrenetMode(), setTransitionMode() and
    // setTolerance() expose request-local helper state before add/remove/build calls.
    if (options.tolerance) {
        const PipeShellTolerance& tolerance = *options.tolerance;
        helper.SetTolerance(tolerance.tol3d, tolerance.boundTol, tolerance.tolAngular);
    }
    helper.SetTransitionMode(helperTransitionMode(options.transition));
    if (options.mode == PipeShellMode::Frenet) {
        helper.SetMode(Standard_True);
    }
    else if (options.mode == PipeShellMode::Standard) {
        helper.SetMode(Standard_False);
    }
    else {
        return "Part::Sweep HelperLifecycle currently supports Standard/Frenet helper replay only";
    }
    return {};
}

std::unique_ptr<BRepOffsetAPI_MakePipeShell> makeHelperForLifecycle(
    const TopoDS_Wire& spine,
    const PipeShellOptions& options,
    const std::vector<HelperLifecycleProfile>& activeProfiles,
    std::string& error
)
{
    auto helper = std::make_unique<BRepOffsetAPI_MakePipeShell>(spine);
    error = configureHelperPipeShell(*helper, options);
    if (!error.empty()) {
        return nullptr;
    }
    for (const HelperLifecycleProfile& profile : activeProfiles) {
        helper->Add(profile.shape);
    }
    return helper;
}

std::optional<PipeShellProfilePlacement> readProfilePlacementMode(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const nlohmann::json& value,
    const std::string& property
)
{
    const nlohmann::json& payload = propertyPayload(value);
    if (payload.is_boolean()) {
        return payload.get<bool>()
            ? PipeShellProfilePlacement::AnchorLocationToSpineStartProductContract
            : PipeShellProfilePlacement::OcctLocationOverload;
    }
    if (payload.is_string()) {
        const std::string mode = payload.get<std::string>();
        if (mode == "AnchorLocationToSpineStart" || mode == "AnchorToSpineStart"
            || mode == "cad_core_product_contract") {
            return PipeShellProfilePlacement::AnchorLocationToSpineStartProductContract;
        }
        if (mode == "OcctLocationOverload") {
            return PipeShellProfilePlacement::OcctLocationOverload;
        }
    }

    addSweepDiagnostic(
        object,
        context,
        "invalid_parameter",
        "Part::Sweep SectionOptions ProfilePlacement must be AnchorLocationToSpineStart or false",
        property,
        object.name,
        "ProfilePlacement"
    );
    return std::nullopt;
}

std::string sectionOptionProperty(std::size_t index, const std::string& field = {})
{
    std::string property = "SectionOptions[" + std::to_string(index) + "]";
    if (!field.empty()) {
        property += "." + field;
    }
    return property;
}

std::optional<TopoDS_Shape> linkSubShape(
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape,
    const app::Link& link,
    std::size_t index
)
{
    if (index >= link.subnames.size() || link.subnames[index].empty()) {
        return std::nullopt;
    }
    const std::string current = link.subnames[index];
    const std::string stable = index < link.stableSubnames.size() && !link.stableSubnames[index].empty()
        ? link.stableSubnames[index]
        : current;
    if (namedShape != nullptr) {
        if (auto shape = part::subshapeByName(*namedShape, current, stable)) {
            return shape;
        }
    }
    if (auto shape = part::subshapeByName(sourceShape, current)) {
        return shape;
    }
    if (stable != current) {
        return part::subshapeByName(sourceShape, stable);
    }
    return std::nullopt;
}

std::optional<std::string> jsonStringField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_string()) {
        return std::nullopt;
    }
    return it->get<std::string>();
}

std::optional<bool> jsonBoolField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_boolean()) {
        return std::nullopt;
    }
    return it->get<bool>();
}

std::optional<int> jsonIntField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_number_integer()) {
        return std::nullopt;
    }
    return it->get<int>();
}

std::string helperOperationMethod(const nlohmann::json& operation)
{
    if (const auto value = jsonStringField(operation, "op")) {
        return *value;
    }
    if (const auto value = jsonStringField(operation, "method")) {
        return *value;
    }
    return {};
}

std::string helperOperationLabel(const nlohmann::json& operation, const std::string& method)
{
    if (const auto value = jsonStringField(operation, "label")) {
        return *value;
    }
    return method.empty() ? "unknown()" : method + "()";
}

std::optional<HelperLifecycleProfile> resolveHelperLifecycleProfile(
    const nlohmann::json& operation,
    runtime::ComputeContext& context,
    std::string& error
)
{
    const auto profileIt = operation.find("profile");
    if (profileIt == operation.end()) {
        error = "HelperLifecycle operation requires profile";
        return std::nullopt;
    }

    app::Link link;
    if (profileIt->is_string()) {
        link.object = profileIt->get<std::string>();
    }
    else if (const auto parsed = app::readLink(*profileIt)) {
        link = *parsed;
    }
    else {
        error = "HelperLifecycle profile must be an object name or App::PropertyLink payload";
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        error = "HelperLifecycle profile target " + link.object + " did not produce a shape";
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link.object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;
    TopoDS_Shape profile = shapeIt->second.shape;
    if (!link.subnames.empty()) {
        const auto subshape = linkSubShape(shapeIt->second.shape, namedShape, link, 0U);
        if (!subshape || subshape->IsNull()) {
            error = "HelperLifecycle profile subshape " + link.object + "." + link.subnames.front()
                + " is invalid";
            return std::nullopt;
        }
        profile = *subshape;
    }

    return HelperLifecycleProfile {link.object, profileShapeForHelper(profile)};
}

void removeActiveHelperProfile(
    std::vector<HelperLifecycleProfile>& activeProfiles,
    const TopoDS_Shape& profile
)
{
    for (auto it = activeProfiles.begin(); it != activeProfiles.end(); ++it) {
        if (it->shape.IsSame(profile)) {
            activeProfiles.erase(it);
            return;
        }
    }
}

bool helperCaseIsProductContract(const nlohmann::json& helperCase)
{
    if (const auto nativeParity = jsonBoolField(helperCase, "native_parity"); nativeParity && !*nativeParity) {
        return true;
    }
    return jsonStringField(helperCase, "contract_provenance").has_value();
}

void appendHelperOperationArgs(
    nlohmann::json& operationResult,
    const nlohmann::json& operation,
    const std::optional<HelperLifecycleProfile>& profile,
    const std::optional<int>& count
)
{
    nlohmann::json args = nlohmann::json::object();
    if (profile) {
        args["profile"] = profile->objectName;
    }
    if (count) {
        args["count"] = *count;
    }
    if (!args.empty()) {
        operationResult["args"] = std::move(args);
    }
    if (const auto isolated = jsonBoolField(operation, "isolated"); isolated && *isolated) {
        operationResult["isolated"] = true;
    }
}

void publishShapeOrNullDiagnostic(nlohmann::json& operation, nlohmann::json& diagnostics, const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        failHelperOperation(operation, diagnostics, "FreeCADError", "cannot determine type of null shape");
        return;
    }
    succeedHelperOperation(operation, helperReturnShape(shape));
}

nlohmann::json executeHelperLifecycleCase(
    const nlohmann::json& helperCase,
    const TopoDS_Wire& spine,
    const PipeShellOptions& options,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp exposes "add/remove/isReady/getStatus/build/shape/
    // firstShape/lastShape/generated/simulate/makeSolid"; C12-M14 replays those calls only when
    // the request supplies the CAD Core HelperLifecycle DTO.
    nlohmann::json result = {
        {"case_id", jsonStringField(helperCase, "case_id").value_or("helper_lifecycle")},
        {"operations", nlohmann::json::array()},
        {"operation_order", nlohmann::json::array()},
        {"diagnostics", nlohmann::json::array()},
    };
    if (const auto oracle = jsonStringField(helperCase, "oracle_id")) {
        result["oracle_id"] = *oracle;
    }
    const bool productContract = helperCaseIsProductContract(helperCase);
    if (productContract) {
        result["native_parity"] = false;
        result["contract_provenance"] = jsonStringField(helperCase, "contract_provenance")
            .value_or("cad_core_product_contract_non_parity");
        result["freecad_native_instability"] = {
            {"error", "NCollection_Sequence::ChangeValue"},
            {"source_artifact", "docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json"},
        };
    }
    else {
        result["native_parity"] = true;
    }

    const auto operationsIt = helperCase.find("operations");
    if (operationsIt == helperCase.end() || !operationsIt->is_array()) {
        result["status"] = "stable_native_diagnostic";
        result["diagnostics"].push_back(
            {{"type", "ValueError"}, {"message", "HelperLifecycle case requires operations array"}}
        );
        return result;
    }

    std::vector<HelperLifecycleProfile> activeProfiles;
    std::string helperError;
    auto helper = makeHelperForLifecycle(spine, options, activeProfiles, helperError);
    if (!helper) {
        result["status"] = "stable_native_diagnostic";
        result["diagnostics"].push_back({{"type", "ValueError"}, {"message", helperError}});
        return result;
    }

    for (const auto& operation : *operationsIt) {
        const std::string method = helperOperationMethod(operation);
        nlohmann::json operationResult = {
            {"index", result["operations"].size()},
            {"op", method},
            {"label", helperOperationLabel(operation, method)},
        };
        result["operation_order"].push_back(operationResult["label"]);

        std::optional<HelperLifecycleProfile> profile;
        if (method == "add" || method == "remove" || method == "generated") {
            std::string profileError;
            profile = resolveHelperLifecycleProfile(operation, context, profileError);
            if (!profile) {
                appendHelperOperationArgs(operationResult, operation, std::nullopt, std::nullopt);
                failHelperOperation(operationResult, result["diagnostics"], "ValueError", profileError);
                result["operations"].push_back(std::move(operationResult));
                continue;
            }
        }
        const std::optional<int> count = method == "simulate" ? jsonIntField(operation, "count")
                                                              : std::nullopt;
        appendHelperOperationArgs(operationResult, operation, profile, count);

        try {
            if (method == "add") {
                helper->Add(profile->shape);
                activeProfiles.push_back(*profile);
                succeedHelperOperation(operationResult, helperReturnNone());
            }
            else if (method == "remove") {
                helper->Delete(profile->shape);
                removeActiveHelperProfile(activeProfiles, profile->shape);
                succeedHelperOperation(operationResult, helperReturnNone());
            }
            else if (method == "isReady") {
                succeedHelperOperation(operationResult, helperReturnBool(helper->IsReady() == Standard_True));
            }
            else if (method == "getStatus") {
                succeedHelperOperation(operationResult, helperReturnInt(static_cast<int>(helper->GetStatus())));
            }
            else if (method == "build") {
                helper->Build();
                succeedHelperOperation(operationResult, helperReturnNone());
            }
            else if (method == "shape") {
                publishShapeOrNullDiagnostic(operationResult, result["diagnostics"], helper->Shape());
            }
            else if (method == "firstShape") {
                publishShapeOrNullDiagnostic(operationResult, result["diagnostics"], helper->FirstShape());
            }
            else if (method == "lastShape") {
                publishShapeOrNullDiagnostic(operationResult, result["diagnostics"], helper->LastShape());
            }
            else if (method == "generated") {
                const TopTools_ListOfShape& shapes = helper->Generated(profile->shape);
                succeedHelperOperation(operationResult, helperReturnShapeList(shapes));
            }
            else if (method == "simulate") {
                if (!count) {
                    failHelperOperation(
                        operationResult,
                        result["diagnostics"],
                        "ValueError",
                        "HelperLifecycle simulate operation requires integer count"
                    );
                }
                else {
                    TopTools_ListOfShape shapes;
                    const bool isolated = jsonBoolField(operation, "isolated").value_or(false);
                    if (isolated) {
                        std::string isolatedError;
                        auto isolatedHelper = makeHelperForLifecycle(
                            spine,
                            options,
                            activeProfiles,
                            isolatedError
                        );
                        if (!isolatedHelper) {
                            failHelperOperation(
                                operationResult,
                                result["diagnostics"],
                                "ValueError",
                                isolatedError
                            );
                        }
                        else {
                            isolatedHelper->Simulate(*count, shapes);
                            operationResult["isolated"] = true;
                            succeedHelperOperation(operationResult, helperReturnShapeList(shapes));
                        }
                    }
                    else {
                        helper->Simulate(*count, shapes);
                        succeedHelperOperation(operationResult, helperReturnShapeList(shapes));
                    }
                }
            }
            else if (method == "makeSolid") {
                succeedHelperOperation(operationResult, helperReturnBool(helper->MakeSolid() == Standard_True));
            }
            else {
                failHelperOperation(
                    operationResult,
                    result["diagnostics"],
                    "ValueError",
                    "Unsupported HelperLifecycle operation: " + method
                );
            }
        }
        catch (const Standard_Failure& failure) {
            failHelperOperation(
                operationResult,
                result["diagnostics"],
                "OCCError",
                standardFailureMessage(failure)
            );
        }
        catch (const std::exception& exception) {
            failHelperOperation(operationResult, result["diagnostics"], "RuntimeError", exception.what());
        }

        result["operations"].push_back(std::move(operationResult));
    }

    if (productContract) {
        result["status"] = "cad_core_product_contract";
        result["classification"] = "product_contract_only";
    }
    else {
        result["status"] = result["diagnostics"].empty() ? "stable_native_payload"
                                                         : "stable_native_diagnostic";
        result["classification"] = result["status"];
    }
    return result;
}

std::optional<nlohmann::json> executeHelperLifecycle(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const SweepInput& spineInput,
    const PipeShellOptions& options
)
{
    const auto* value = app::propertyValue(object, "HelperLifecycle");
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!payload.is_object()) {
        return nlohmann::json {
            {"status", "invalid"},
            {"diagnostics",
             {{{"type", "ValueError"}, {"message", "HelperLifecycle must be an object"}}}},
        };
    }

    std::string spineError;
    const auto helperSpine = wireFromShapeForHelper(spineInput.shape, "HelperLifecycle spine", spineError);
    if (!helperSpine) {
        return nlohmann::json {
            {"status", "invalid"},
            {"diagnostics", {{{"type", "ValueError"}, {"message", spineError}}}},
        };
    }

    nlohmann::json helperLifecycle = {
        {"schema_version", "cad-core.part-sweep-helper-lifecycle.v1"},
        {"dto", "PartSweepHelperLifecycleDTO"},
        {"helper", "Part.BRepOffsetAPI_MakePipeShell"},
        {"runtime_helper", "Part.BRepOffsetAPI.MakePipeShell"},
        {"source_authority",
         "src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::"
         "add/remove/isReady/getStatus/build/shape/firstShape/lastShape/generated/simulate/makeSolid"},
        {"source_artifact",
         jsonStringField(payload, "source_artifact")
             .value_or("docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json")},
        {"cases", nlohmann::json::array()},
    };

    const auto casesIt = payload.find("cases");
    if (casesIt != payload.end() && casesIt->is_array()) {
        for (const auto& helperCase : *casesIt) {
            helperLifecycle["cases"].push_back(
                executeHelperLifecycleCase(helperCase, *helperSpine, options, context)
            );
        }
    }
    else if (const auto operationsIt = payload.find("operations");
             operationsIt != payload.end() && operationsIt->is_array()) {
        helperLifecycle["cases"].push_back(
            executeHelperLifecycleCase(
                nlohmann::json {{"case_id", "helper_lifecycle"}, {"operations", *operationsIt}},
                *helperSpine,
                options,
                context
            )
        );
    }
    else {
        helperLifecycle["status"] = "invalid";
        helperLifecycle["diagnostics"] = {
            {{"type", "ValueError"}, {"message", "HelperLifecycle requires cases or operations"}}
        };
        return helperLifecycle;
    }

    nlohmann::json caseIds = nlohmann::json::array();
    bool hasDiagnostics = false;
    for (const auto& helperCase : helperLifecycle["cases"]) {
        caseIds.push_back(helperCase.value("case_id", ""));
        hasDiagnostics = hasDiagnostics || !helperCase.value("diagnostics", nlohmann::json::array()).empty();
    }
    helperLifecycle["case_ids"] = std::move(caseIds);
    helperLifecycle["status"] = hasDiagnostics ? "completed_with_diagnostics" : "completed";
    return helperLifecycle;
}

std::optional<SweepInput> resolveSweepSpine(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::execute(), checks "if (!Spine.getValue())" then resolves the object with
    // "ResolveLink | Transform"; when "Spine.getSubValues()" is non-empty, every subshape is
    // read via "spine.getSubTopoShape(sub.c_str())" and reassembled as a compound.
    if (app::propertyValue(object, "Spine") == nullptr) {
        addSweepDiagnostic(object, context, "missing_property", "No spine", "Spine");
        return std::nullopt;
    }
    const auto link = app::readLink(object, "Spine");
    if (!link || link->object.empty()) {
        addSweepDiagnostic(object, context, "missing_property", "No spine", "Spine");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addSweepDiagnostic(object, context, "missing_link_target", "Invalid spine", "Spine", link->object);
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(link->object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;

    TopoDS_Shape spine = shapeIt->second.shape;
    if (!link->subnames.empty()) {
        std::vector<TopoDS_Shape> selected;
        selected.reserve(link->subnames.size());
        for (std::size_t index = 0; index < link->subnames.size(); ++index) {
            const auto subshape = linkSubShape(shapeIt->second.shape, namedShape, *link, index);
            if (!subshape || subshape->IsNull()) {
                addSweepDiagnostic(object, context, "invalid_subshape", "Invalid spine", "Spine", link->object);
                return std::nullopt;
            }
            selected.push_back(*subshape);
        }
        spine = compoundOf(selected);
    }

    return SweepInput {link->object, spine, namedShape};
}

std::optional<std::vector<SweepInput>> resolveSweepSections(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::execute(), reads Sections as App::PropertyLinkList and reports
    // "No sections linked." or "Invalid section link" before makeElementPipeShell().
    const std::vector<app::Link> links = app::readLinks(object, "Sections");
    if (links.empty()) {
        addSweepDiagnostic(object, context, "missing_property", "No sections linked.", "Sections");
        return std::nullopt;
    }

    std::vector<SweepInput> sections;
    sections.reserve(links.size());
    for (const auto& link : links) {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            addSweepDiagnostic(
                object,
                context,
                "missing_link_target",
                "Invalid section link",
                "Sections",
                link.object
            );
            return std::nullopt;
        }
        const auto namedShapeIt = context.namedShapes.find(link.object);
        sections.push_back(
            SweepInput {
                link.object,
                shapeIt->second.shape,
                namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
            }
        );
    }
    return sections;
}

std::string transitionLabel(int transition)
{
    switch (transition) {
        case 0:
            return "Transformed";
        case 2:
            return "Round corner";
        default:
            return "Right corner";
    }
}

std::optional<int> readSweepTransition(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::TransitionEnums = {"Transformed", "Right corner", "Round corner"} and the
    // constructor initializes "Transition" to long(1).
    int transition = 1;
    if (const auto number = app::readNumber(object, "Transition")) {
        transition = static_cast<int>(std::llround(*number));
    }
    else if (const auto label = app::readString(object, "Transition")) {
        if (*label == "Transformed") {
            transition = 0;
        }
        else if (*label == "Right corner") {
            transition = 1;
        }
        else if (*label == "Round corner") {
            transition = 2;
        }
        else {
            addSweepDiagnostic(
                object,
                context,
                "invalid_property",
                "Part::Sweep Transition must be Transformed, Right corner or Round corner",
                "Transition"
            );
            return std::nullopt;
        }
    }

    if (transition < 0 || transition > 2) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_property",
            "Part::Sweep Transition must be Transformed, Right corner or Round corner",
            "Transition"
        );
        return std::nullopt;
    }
    return transition;
}

std::string firstAdvancedTarget(const app::DocumentObject& object, const std::string& property)
{
    if (const auto link = app::readLink(object, property)) {
        if (!link->object.empty()) {
            return link->object;
        }
    }
    const std::vector<app::Link> links = app::readLinks(object, property);
    if (!links.empty() && !links.front().object.empty()) {
        return links.front().object;
    }
    return object.name;
}

std::string firstAdvancedSubname(const app::DocumentObject& object, const std::string& property)
{
    if (const auto link = app::readLink(object, property)) {
        if (!link->stableSubnames.empty() && !link->stableSubnames.front().empty()) {
            return link->stableSubnames.front();
        }
        if (!link->subnames.empty()) {
            return link->subnames.front();
        }
    }
    const std::vector<app::Link> links = app::readLinks(object, property);
    if (!links.empty()) {
        if (!links.front().stableSubnames.empty() && !links.front().stableSubnames.front().empty()) {
            return links.front().stableSubnames.front();
        }
        if (!links.front().subnames.empty()) {
            return links.front().subnames.front();
        }
    }
    return property;
}

std::optional<AdvancedLinkShape> resolveAdvancedLinkShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& missingMessage,
    const std::string& invalidMessage
)
{
    const auto link = app::readLink(object, property);
    if (!link || link->object.empty()) {
        addSweepDiagnostic(
            object,
            context,
            "missing_link_target",
            missingMessage,
            property,
            object.name,
            property
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addSweepDiagnostic(
            object,
            context,
            "missing_link_target",
            missingMessage,
            property,
            link->object,
            firstAdvancedSubname(object, property)
        );
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(link->object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;

    TopoDS_Shape resolved = shapeIt->second.shape;
    std::string firstSubname;
    if (!link->subnames.empty()) {
        std::vector<TopoDS_Shape> selected;
        selected.reserve(link->subnames.size());
        for (std::size_t index = 0; index < link->subnames.size(); ++index) {
            const auto subshape = linkSubShape(shapeIt->second.shape, namedShape, *link, index);
            const std::string subname = index < link->subnames.size() ? link->subnames.at(index)
                                                                      : std::string {};
            if (!subshape || subshape->IsNull()) {
                addSweepDiagnostic(
                    object,
                    context,
                    "invalid_subshape",
                    invalidMessage,
                    property,
                    link->object,
                    subname
                );
                return std::nullopt;
            }
            if (firstSubname.empty()) {
                firstSubname = index < link->stableSubnames.size() && !link->stableSubnames.at(index).empty()
                    ? link->stableSubnames.at(index)
                    : subname;
            }
            selected.push_back(*subshape);
        }
        resolved = compoundOf(selected);
    }

    return AdvancedLinkShape {link->object, firstSubname, resolved};
}

std::optional<AdvancedVertexLink> resolveSectionLocation(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const nlohmann::json& rawLocation,
    const std::string& property
)
{
    const auto link = app::readLink(rawLocation);
    if (!link || link->object.empty()) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep SectionOptions Location must be an App::PropertyLinkSub vertex",
            property,
            object.name,
            "Location"
        );
        return std::nullopt;
    }

    const std::string requestedSubname = !link->stableSubnames.empty() && !link->stableSubnames.front().empty()
        ? link->stableSubnames.front()
        : (!link->subnames.empty() ? link->subnames.front() : std::string {"Location"});

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addSweepDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::Sweep SectionOptions Location target is missing",
            property,
            link->object,
            requestedSubname
        );
        return std::nullopt;
    }
    if (link->subnames.size() > 1U) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_location_subname_count",
            "Part::Sweep SectionOptions Location must reference exactly one vertex",
            property,
            link->object,
            requestedSubname
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link->object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;

    TopoDS_Shape resolved = shapeIt->second.shape;
    if (!link->subnames.empty()) {
        const auto subshape = linkSubShape(shapeIt->second.shape, namedShape, *link, 0U);
        if (!subshape || subshape->IsNull()) {
            addSweepDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Invalid SectionOptions Location",
                property,
                link->object,
                requestedSubname
            );
            return std::nullopt;
        }
        resolved = *subshape;
    }

    if (resolved.IsNull() || resolved.ShapeType() != TopAbs_VERTEX) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::Sweep SectionOptions Location must resolve to a vertex",
            property,
            link->object,
            requestedSubname
        );
        return std::nullopt;
    }

    return AdvancedVertexLink {link->object, requestedSubname, TopoDS::Vertex(resolved)};
}

bool rejectDeferredSweepAdvancedProperties(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeSweepSurface(), exposes a helper tolerance/fillMode wrapper; PartFeatures.cpp
    // ::Sweep::execute() only publishes Sections/Spine/Solid/Frenet/Transition/Linearize.
    static const std::vector<std::string> deferred {
        "LocationMode",
    };
    bool ok = true;
    for (const std::string& property : deferred) {
        if (app::propertyValue(object, property) == nullptr) {
            continue;
        }
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Part::Sweep advanced PipeShell wrapper property " + property
                + " is deferred; this executor follows Sweep::execute() only",
            object.name,
            property,
            "runtime",
            firstAdvancedTarget(object, property),
            firstAdvancedSubname(object, property)
        );
        ok = false;
    }
    if (!ok) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_sweep"}};
    }
    return ok;
}

std::optional<bool> readStrictBool(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (app::propertyValue(object, property) == nullptr) {
        return std::nullopt;
    }
    const auto value = app::readBool(object, property);
    if (!value) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep advanced PipeShell property " + property + " must be a boolean",
            property,
            object.name,
            property
        );
        return std::nullopt;
    }
    return *value;
}

std::optional<bool> readSurfaceNormalSupportMode(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    if (app::propertyValue(object, "SupportMode") == nullptr) {
        return false;
    }
    if (const auto label = app::readString(object, "SupportMode")) {
        if (*label == "None") {
            return false;
        }
        if (*label == "SurfaceNormal") {
            return true;
        }
    }
    if (const auto number = app::readNumber(object, "SupportMode")) {
        const int value = static_cast<int>(std::llround(*number));
        if (value == 0) {
            return false;
        }
        if (value == 1) {
            return true;
        }
    }
    addSweepDiagnostic(
        object,
        context,
        "invalid_parameter",
        "Part::Sweep SupportMode must be None or SurfaceNormal",
        "SupportMode",
        object.name,
        "SupportMode"
    );
    return std::nullopt;
}

std::optional<std::array<double, 3>> readBinormalVector(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    std::string& property
)
{
    const bool hasCanonical = app::propertyValue(object, "Binormal") != nullptr;
    const bool hasLegacy = app::propertyValue(object, "BiNormal") != nullptr;
    if (!hasCanonical && !hasLegacy) {
        return std::nullopt;
    }
    if (hasCanonical && hasLegacy) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep advanced PipeShell accepts either Binormal or BiNormal, not both",
            "Binormal",
            object.name,
            "Binormal"
        );
        return std::nullopt;
    }

    property = hasCanonical ? "Binormal" : "BiNormal";
    const auto vector = app::readVector3(object, property);
    if (!vector) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep Binormal must be a finite [x, y, z] vector",
            property,
            object.name,
            property
        );
        return std::nullopt;
    }

    const double magnitudeSquared = (*vector)[0] * (*vector)[0] + (*vector)[1] * (*vector)[1]
        + (*vector)[2] * (*vector)[2];
    if (magnitudeSquared <= 1.0e-24) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep Binormal must be non-zero",
            property,
            object.name,
            property
        );
        return std::nullopt;
    }
    return *vector;
}

bool readSectionOptions(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<SweepInput>& sections,
    AdvancedSweepOptions& advanced
)
{
    const auto* value = app::propertyValue(object, "SectionOptions");
    if (value == nullptr) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::add(), profile options are applied per Add()
    // call, so cad-core matches SectionOptions entries to Sections by index.
    const nlohmann::json& options = propertyPayload(value->raw);
    if (!options.is_array()) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep SectionOptions must be an array matched to Sections order",
            "SectionOptions",
            object.name,
            "SectionOptions"
        );
        return false;
    }
    if (options.size() > sections.size()) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep SectionOptions cannot have more entries than Sections",
            "SectionOptions",
            object.name,
            "SectionOptions"
        );
        return false;
    }

    advanced.pipeOptions.sectionOptions.assign(sections.size(), PipeShellSectionOption {});
    nlohmann::json sectionMetadata = nlohmann::json::array();
    for (std::size_t index = 0; index < sections.size(); ++index) {
        nlohmann::json itemMetadata = {
            {"profile", sections.at(index).objectName},
            {"with_contact", false},
            {"with_correction", false},
        };

        if (index >= options.size() || options.at(index).is_null()) {
            sectionMetadata.push_back(std::move(itemMetadata));
            continue;
        }

        const nlohmann::json& item = options.at(index);
        if (!item.is_object()) {
            addSweepDiagnostic(
                object,
                context,
                "invalid_parameter",
                "Part::Sweep SectionOptions entries must be objects",
                sectionOptionProperty(index),
                object.name,
                "SectionOptions"
            );
            return false;
        }

        PipeShellSectionOption& sectionOption = advanced.pipeOptions.sectionOptions.at(index);
        if (const auto locationIt = item.find("Location"); locationIt != item.end()) {
            const std::string property = sectionOptionProperty(index, "Location");
            const auto location = resolveSectionLocation(object, context, *locationIt, property);
            if (!location) {
                return false;
            }
            sectionOption.location = location->vertex;
            sectionOption.hasLocation = true;
            itemMetadata["location"] = {
                {"target", location->objectName},
                {"subname", location->subname},
            };
        }

        if (const auto profilePlacementIt = item.find("ProfilePlacement");
            profilePlacementIt != item.end()) {
            // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // BRepOffsetAPI_MakePipeShellPyImp.cpp::add(), exposes the Location overload as
            // "add(Profile, Location, WithContact, WithCorrection)". C6-M4 requires an
            // explicit CAD Core product-contract selector so existing c5m10 known_gap
            // fixtures keep the native overload blocker.
            const std::string property = sectionOptionProperty(index, "ProfilePlacement");
            const auto profilePlacement = readProfilePlacementMode(
                object,
                context,
                *profilePlacementIt,
                property
            );
            if (!profilePlacement) {
                return false;
            }
            if (*profilePlacement
                == PipeShellProfilePlacement::AnchorLocationToSpineStartProductContract) {
                if (!sectionOption.hasLocation) {
                    addSweepDiagnostic(
                        object,
                        context,
                        "invalid_parameter",
                        "Part::Sweep SectionOptions ProfilePlacement requires Location",
                        property,
                        object.name,
                        "ProfilePlacement"
                    );
                    return false;
                }
                sectionOption.profilePlacement = *profilePlacement;
                itemMetadata["profile_placement"] = {
                    {"contract", "cad_core_product_contract"},
                    {"source", "cad_core_product_contract"},
                    {"strategy", "anchor_location_vertex_to_spine_start"},
                    {"location_overload", "cad_core_product_contract_non_parity"},
                    {"freecadcmd_location_overload_status", "notCollected"},
                };
            }
        }

        if (const auto contactIt = item.find("WithContact"); contactIt != item.end()) {
            const auto withContact = readJsonBool(*contactIt);
            if (!withContact) {
                addSweepDiagnostic(
                    object,
                    context,
                    "invalid_parameter",
                    "Part::Sweep SectionOptions WithContact must be a boolean",
                    sectionOptionProperty(index, "WithContact"),
                    object.name,
                    "WithContact"
                );
                return false;
            }
            sectionOption.withContact = *withContact;
            itemMetadata["with_contact"] = *withContact;
        }

        if (const auto correctionIt = item.find("WithCorrection"); correctionIt != item.end()) {
            const auto withCorrection = readJsonBool(*correctionIt);
            if (!withCorrection) {
                addSweepDiagnostic(
                    object,
                    context,
                    "invalid_parameter",
                    "Part::Sweep SectionOptions WithCorrection must be a boolean",
                    sectionOptionProperty(index, "WithCorrection"),
                    object.name,
                    "WithCorrection"
                );
                return false;
            }
            sectionOption.withCorrection = *withCorrection;
            itemMetadata["with_correction"] = *withCorrection;
        }

        sectionMetadata.push_back(std::move(itemMetadata));
    }

    advanced.metadata["sections"] = std::move(sectionMetadata);
    return true;
}

bool hasSectionLocation(const PipeShellOptions& options)
{
    for (const PipeShellSectionOption& sectionOption : options.sectionOptions) {
        if (sectionOption.hasLocation) {
            return true;
        }
    }
    return false;
}

bool hasProductContractSectionLocation(const PipeShellOptions& options)
{
    for (const PipeShellSectionOption& sectionOption : options.sectionOptions) {
        if (sectionOption.hasLocation
            && sectionOption.profilePlacement
                == PipeShellProfilePlacement::AnchorLocationToSpineStartProductContract) {
            return true;
        }
    }
    return false;
}

std::string locationOverloadKnownGapKind(const PipeShellOptions& options)
{
    if (options.mode == PipeShellMode::Auxiliary && options.tolerance.has_value()) {
        return "part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker";
    }
    return "part_sweep_located_profile_freecadcmd_wrapper_build_blocker";
}

void publishLocationOverloadKnownGap(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    nlohmann::json metadata,
    const PipeShellOptions& options,
    const std::string& error
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::add(), exposes
    // "add(Profile, Location, WithContact, WithCorrection)". C5-M13 S2 FreeCADCmd probes
    // show this overload reaches "is_ready_before_build=true" and then fails at
    // "builder.build()" with "OCCError: NCollection_Array1::Value"; keep request metadata
    // stable without fabricating a FreeCAD shape_summary from cad-core output.
    metadata["status"] = "known_gap";
    metadata["known_gap"] = {
        {"kind", locationOverloadKnownGapKind(options)},
        {"freecadcmd_evidence",
         {
             {"helper", "Part.BRepOffsetAPI_MakePipeShell"},
             {"runtime_helper", "Part.BRepOffsetAPI.MakePipeShell"},
             {"call", "add(Profile, Location, WithContact, WithCorrection)"},
             {"error", "OCCError: NCollection_Array1::Value"},
             {"failed_stage", "build"},
         }},
        {"cad_core_status", "request_metadata_only"},
        {"cad_core_error", error.empty() ? "NCollection_Array1::Value" : error},
        {"delete_condition",
         "Delete only after the FreeCADCmd Location overload returns stable shape_summary."},
    };
    context.objects[object.name] = std::move(metadata);
}

bool readTolerance(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    AdvancedSweepOptions& advanced
)
{
    const auto* value = app::propertyValue(object, "Tolerance");
    if (value == nullptr) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::setTolerance(), parses exactly three doubles:
    // "tol3d, boundTol, tolAngular". The old scalar CAD Core placeholder remains a deferred
    // compatibility diagnostic and is not promoted to the wrapper contract.
    const nlohmann::json& tolerance = propertyPayload(value->raw);
    if (tolerance.is_number()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Part::Sweep scalar Tolerance is a deferred compatibility placeholder; use "
            "Tolerance.tol3d/boundTol/tolAngular",
            object.name,
            "Tolerance",
            "runtime",
            object.name,
            "Tolerance"
        );
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_sweep"}};
        return false;
    }
    if (!tolerance.is_object()) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep Tolerance must be an object with tol3d, boundTol and tolAngular",
            "Tolerance",
            object.name,
            "Tolerance"
        );
        return false;
    }

    const auto readField = [&](const std::string& field) -> std::optional<double> {
        const auto it = tolerance.find(field);
        if (it == tolerance.end() || !isFiniteJsonNumber(*it)) {
            addSweepDiagnostic(
                object,
                context,
                "invalid_parameter",
                "Part::Sweep Tolerance." + field + " must be a finite number",
                "Tolerance." + field,
                object.name,
                field
            );
            return std::nullopt;
        }
        return it->get<double>();
    };

    const auto tol3d = readField("tol3d");
    if (!tol3d) {
        return false;
    }
    const auto boundTol = readField("boundTol");
    if (!boundTol) {
        return false;
    }
    const auto tolAngular = readField("tolAngular");
    if (!tolAngular) {
        return false;
    }

    advanced.pipeOptions.tolerance = PipeShellTolerance {*tol3d, *boundTol, *tolAngular};
    advanced.metadata["tolerance"] = {
        {"tol3d", *tol3d},
        {"boundTol", *boundTol},
        {"tolAngular", *tolAngular},
    };
    return true;
}

std::optional<AdvancedSweepOptions> readAdvancedSweepOptions(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<SweepInput>& sections,
    bool solid,
    bool frenet,
    int transition,
    bool linearize
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine/setSpineSupport/
    // setBiNormalMode call BRepOffsetAPI_MakePipeShell::SetMode(...); PartDesign source
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::setupAlgorithm() maps Mode=Auxiliary/Binormal through the same SetMode overloads.
    AdvancedSweepOptions advanced;
    advanced.pipeOptions.solid = solid;
    advanced.pipeOptions.mode = frenet ? PipeShellMode::Frenet : PipeShellMode::Standard;
    advanced.pipeOptions.transition = transition;
    advanced.pipeOptions.linearizeFaces = linearize;

    const bool hasAuxiliarySpine = app::propertyValue(object, "AuxiliarySpine") != nullptr;
    const bool hasAuxiliaryCurvilinear = app::propertyValue(object, "AuxiliaryCurvilinear") != nullptr;
    std::optional<bool> auxiliaryCurvilinear = true;
    if (hasAuxiliaryCurvilinear) {
        auxiliaryCurvilinear = readStrictBool(object, context, "AuxiliaryCurvilinear");
        if (!auxiliaryCurvilinear) {
            return std::nullopt;
        }
    }

    const auto surfaceNormalSupport = readSurfaceNormalSupportMode(object, context);
    if (!surfaceNormalSupport) {
        return std::nullopt;
    }

    std::string binormalProperty = "Binormal";
    const auto binormal = readBinormalVector(object, context, binormalProperty);
    const bool requestedBinormal = app::propertyValue(object, "Binormal") != nullptr
        || app::propertyValue(object, "BiNormal") != nullptr;
    if (requestedBinormal && !binormal) {
        return std::nullopt;
    }

    int selectedModes = 0;
    if (hasAuxiliarySpine || hasAuxiliaryCurvilinear) {
        ++selectedModes;
    }
    if (*surfaceNormalSupport) {
        ++selectedModes;
    }
    if (binormal) {
        ++selectedModes;
    }
    if (selectedModes > 1) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep advanced PipeShell accepts one builder mode per request",
            "SupportMode",
            object.name,
            "SupportMode"
        );
        return std::nullopt;
    }

    if (hasAuxiliarySpine || hasAuxiliaryCurvilinear) {
        auto auxiliary = resolveAdvancedLinkShape(
            object,
            context,
            "AuxiliarySpine",
            "Part::Sweep Auxiliary mode requires AuxiliarySpine",
            "Invalid AuxiliarySpine"
        );
        if (!auxiliary) {
            return std::nullopt;
        }
        advanced.auxiliarySpine = auxiliary;
        advanced.pipeOptions.mode = PipeShellMode::Auxiliary;
        advanced.pipeOptions.auxiliarySpine = auxiliary->shape;
        advanced.pipeOptions.auxiliaryCurvilinear = auxiliaryCurvilinear.value_or(true);
        advanced.metadata["mode"] = "Auxiliary";
        advanced.metadata["auxiliary_spine"] = {
            {"target", auxiliary->objectName},
            {"curvilinear", advanced.pipeOptions.auxiliaryCurvilinear},
            {"contact", "NoContact"},
        };
        if (!auxiliary->subname.empty()) {
            advanced.metadata["auxiliary_spine"]["subname"] = auxiliary->subname;
        }
    }

    if (*surfaceNormalSupport) {
        auto support = resolveAdvancedLinkShape(
            object,
            context,
            "SpineSupport",
            "Part::Sweep SupportMode=SurfaceNormal requires SpineSupport",
            "Invalid SpineSupport"
        );
        if (!support) {
            return std::nullopt;
        }
        advanced.spineSupport = support;
        advanced.pipeOptions.useSpineSupport = true;
        advanced.pipeOptions.spineSupport = support->shape;
        advanced.metadata["mode"] = "SurfaceNormal";
        advanced.metadata["support_mode"] = "SurfaceNormal";
        advanced.metadata["spine_support"] = {
            {"target", support->objectName},
            {"set_mode_ok", true},
        };
        if (!support->subname.empty()) {
            advanced.metadata["spine_support"]["subname"] = support->subname;
        }
    }
    else if (app::propertyValue(object, "SpineSupport") != nullptr) {
        addSweepDiagnostic(
            object,
            context,
            "invalid_parameter",
            "Part::Sweep SpineSupport requires SupportMode=SurfaceNormal",
            "SupportMode",
            object.name,
            "SupportMode"
        );
        return std::nullopt;
    }

    if (binormal) {
        advanced.binormalProperty = binormalProperty;
        advanced.pipeOptions.mode = PipeShellMode::Binormal;
        advanced.pipeOptions.binormal = *binormal;
        advanced.metadata["mode"] = "Binormal";
        advanced.metadata["binormal"] = {(*binormal)[0], (*binormal)[1], (*binormal)[2]};
        advanced.metadata["binormal_property"] = binormalProperty;
    }

    if (!readSectionOptions(object, context, sections, advanced)) {
        return std::nullopt;
    }
    if (!readTolerance(object, context, advanced)) {
        return std::nullopt;
    }

    return advanced;
}

nlohmann::json sectionNamesJson(const std::vector<SweepInput>& sections)
{
    nlohmann::json names = nlohmann::json::array();
    for (const auto& section : sections) {
        names.push_back(section.objectName);
    }
    return names;
}

std::vector<NamedShapeSource> sweepSources(
    const SweepInput& spine,
    const std::vector<SweepInput>& sections
)
{
    std::vector<NamedShapeSource> sources;
    sources.reserve(sections.size() + 1U);
    sources.push_back(
        NamedShapeSource {
            spine.namedShape != nullptr ? spine.namedShape->owner : spine.objectName,
            spine.shape,
            spine.namedShape,
        }
    );
    for (const auto& section : sections) {
        sources.push_back(
            NamedShapeSource {
                section.namedShape != nullptr ? section.namedShape->owner : section.objectName,
                section.shape,
                section.namedShape,
            }
        );
    }
    return sources;
}

}  // namespace

void executePartSweep(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::execute(), consumes Sections/Spine/Solid/Frenet/Transition/Linearize and calls
    // "result.makeElementPipeShell(shapes, isSolid, isFrenet, transMode, Part::OpCodes::Sweep)".
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Sections",
             "Spine",
             "Solid",
             "Frenet",
             "Linearize",
             "Transition",
             "AuxiliarySpine",
             "AuxiliaryCurvilinear",
             "SpineSupport",
             "SupportMode",
             "Binormal",
             "BiNormal",
             "LocationMode",
             "SectionOptions",
             "Tolerance",
             "HelperLifecycle"}
        )) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_sweep"}};
        return;
    }

    if (!rejectDeferredSweepAdvancedProperties(object, context)) {
        return;
    }

    const bool linearize = app::readBool(object, "Linearize").value_or(false);
    const auto transition = readSweepTransition(object, context);
    if (!transition) {
        return;
    }
    const auto sections = resolveSweepSections(object, context);
    if (!sections) {
        return;
    }
    const auto spine = resolveSweepSpine(object, context);
    if (!spine) {
        return;
    }

    const bool solid = app::readBool(object, "Solid").value_or(true);
    const bool frenet = app::readBool(object, "Frenet").value_or(true);
    const auto advanced
        = readAdvancedSweepOptions(object, context, *sections, solid, frenet, *transition, linearize);
    if (!advanced) {
        return;
    }
    nlohmann::json metadata = {
        {"feature", "part_sweep"},
        {"spine", spine->objectName},
        {"sections", sectionNamesJson(*sections)},
        {"solid", solid},
        {"frenet", frenet},
        {"transition", transitionLabel(*transition)},
        {"linearize", linearize},
        {"topo_naming_history", "maker_history:pipeshell"},
    };
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Sweep::execute() reads Sections/Spine/Solid/Frenet/Transition, delegates to
    // TopoShape::makeElementPipeShell(), then publishes those feature properties with
    // Shape.setValue(). Product-only advanced metadata stays out of this native public subset.
    nlohmann::json publicObjectFields = metadata;
    publicObjectFields["status"] = "ok";
    if (!advanced->metadata.empty()) {
        metadata["advanced"] = advanced->metadata;
    }
    if (const auto helperLifecycle
        = executeHelperLifecycle(object, context, *spine, advanced->pipeOptions)) {
        metadata["helper_lifecycle"] = *helperLifecycle;
    }
    const bool productContractLocation = hasProductContractSectionLocation(advanced->pipeOptions);
    if (productContractLocation) {
        metadata["contract"] = "cad_core_product_contract";
        metadata["contract_provenance"] = "cad_core_product_contract_non_parity";
        metadata["freecadcmd_location_overload_status"] = "notCollected";
        metadata["location_overload"] = {
            {"source_authority",
             "BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)"},
            {"product_strategy", "anchor_location_vertex_to_spine_start"},
            {"freecadcmd_error", "OCCError: NCollection_Array1::Value"},
            {"native_parity", false},
        };
    }
    const auto build = makeElementPipeShellFromSources(
        object.name,
        sweepSources(*spine, *sections),
        advanced->pipeOptions
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        if (advanced->pipeOptions.mode == PipeShellMode::Auxiliary && advanced->auxiliarySpine
            && build.error.rfind("Auxiliary spine", 0U) == 0U) {
            addSweepDiagnostic(
                object,
                context,
                "invalid_subshape",
                build.error,
                "AuxiliarySpine",
                advanced->auxiliarySpine->objectName,
                advanced->auxiliarySpine->subname.empty() ? "AuxiliarySpine"
                                                          : advanced->auxiliarySpine->subname
            );
            return;
        }
        if (advanced->pipeOptions.useSpineSupport && advanced->spineSupport
            && build.error.rfind("SpineSupport", 0U) == 0U) {
            addSweepDiagnostic(
                object,
                context,
                "execution_failed",
                build.error,
                "SpineSupport",
                advanced->spineSupport->objectName,
                advanced->spineSupport->subname.empty() ? "SpineSupport"
                                                        : advanced->spineSupport->subname
            );
            return;
        }
        if (hasSectionLocation(advanced->pipeOptions) && !productContractLocation
            && (build.error.find("NCollection_Array1::Value") != std::string::npos)) {
            publishLocationOverloadKnownGap(
                object,
                context,
                std::move(metadata),
                advanced->pipeOptions,
                build.error
            );
            return;
        }
        addSweepDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::Sweep failed" : build.error,
            "Spine"
        );
        return;
    }
    publicObjectFields["shape"] = part_feature_detail::shapeLabelForPartShape(build.shape);

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        metadata,
        build.namedShape,
        part_feature_detail::PartPublicResultFields {std::move(publicObjectFields), false}
    );
}

}  // namespace cad_core::part
