#include "cad_core/part_design/feature_pipe.h"

#include "cad_core/app/property.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRep_Builder.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

enum class PipeAddSubMode {
    Additive,
    Subtractive,
};

struct PipeInput {
    std::string objectName;
    std::string subname;
    TopoDS_Shape shape;
    TopoDS_Shape sourceShape;
    const part::NamedShape* namedShape = nullptr;
    std::vector<std::string> requestedSubnames;
    std::optional<part::ContinuousEdgeLedger> continuousEdgeLedger;
};

struct PipeLawResolution {
    bool ok = true;
    std::optional<part::PipeScalingLaw> law;
    nlohmann::json metadata;
};

void addPipeDiagnostic(const app::DocumentObject& object,
                       runtime::ComputeContext& context,
                       const std::string& code,
                       const std::string& message,
                       const std::string& property = {},
                       const std::string& target = {},
                       const std::string& subname = {})
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
}

void addHistoryStatus(part::NamedShape& namedShape, const std::string& status)
{
    if (std::find(namedShape.elementHistoryStatus.begin(), namedShape.elementHistoryStatus.end(), status)
        == namedShape.elementHistoryStatus.end()) {
        namedShape.elementHistoryStatus.push_back(status);
    }
}

const part::NamedShape* namedShapeForTarget(const runtime::ComputeContext& context,
                                            const std::string& objectName)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    return namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
}

bool targetIsSketch(const runtime::ComputeContext& context, const std::string& objectName)
{
    const auto objectIt = context.documentObjects.find(objectName);
    return objectIt != context.documentObjects.end() && objectIt->second != nullptr
        && objectIt->second->typeId == "Sketcher::SketchObject";
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

std::optional<TopoDS_Shape> linkedSubshape(const TopoDS_Shape& sourceShape,
                                           const part::NamedShape* namedShape,
                                           const app::Link& link,
                                           std::size_t index)
{
    if (index >= link.subnames.size() || link.subnames.at(index).empty()) {
        return std::nullopt;
    }
    const std::string& subname = link.subnames.at(index);
    const std::string stableSubname
        = index < link.stableSubnames.size() ? link.stableSubnames.at(index) : std::string {};
    if (namedShape != nullptr) {
        if (auto shape = part::subshapeByName(*namedShape, subname, stableSubname)) {
            return shape;
        }
    }
    if (auto shape = part::subshapeByName(sourceShape, subname)) {
        return shape;
    }
    if (!stableSubname.empty() && stableSubname != subname) {
        return part::subshapeByName(sourceShape, stableSubname);
    }
    return std::nullopt;
}

std::optional<int> enumIndex(const app::DocumentObject& object,
                             runtime::ComputeContext& context,
                             const std::string& property,
                             const std::vector<std::string>& labels,
                             int fallback,
                             const std::string& messagePrefix)
{
    int index = fallback;
    if (const auto number = app::readNumber(object, property)) {
        index = static_cast<int>(std::llround(*number));
    }
    else if (const auto label = app::readString(object, property)) {
        const auto labelIt = std::find(labels.begin(), labels.end(), *label);
        if (labelIt == labels.end()) {
            addPipeDiagnostic(object,
                              context,
                              "invalid_property",
                              messagePrefix + " must be one of the supported FreeCAD enum labels",
                              property);
            return std::nullopt;
        }
        index = static_cast<int>(std::distance(labels.begin(), labelIt));
    }

    if (index < 0 || index >= static_cast<int>(labels.size())) {
        addPipeDiagnostic(object,
                          context,
                          "invalid_property",
                          messagePrefix + " enum index is out of range",
                          property);
        return std::nullopt;
    }
    return index;
}

std::string transitionLabel(int transition)
{
    switch (transition) {
        case 1:
            return "Right corner";
        case 2:
            return "Round corner";
        default:
            return "Transformed";
    }
}

std::string modeLabel(int mode)
{
    switch (mode) {
        case 1:
            return "Fixed";
        case 2:
            return "Frenet";
        case 3:
            return "Auxiliary";
        case 4:
            return "Binormal";
        default:
            return "Standard";
    }
}

std::string transformationLabel(int transformation)
{
    switch (transformation) {
        case 1:
            return "Multisection";
        case 2:
            return "Linear";
        case 3:
            return "S-shape";
        case 4:
            return "Interpolation";
        default:
            return "Constant";
    }
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
    // ::Feature::singleSolidRuleMode(), "When the feature is not part of an body" enforces
    // single solid; otherwise it reads Body::AllowCompound before Feature::getSolid().
    return app::readBool(*body, "AllowCompound").value_or(true);
}

int solidCount(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

TopoDS_Shape firstSolid(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        return explorer.Current();
    }
    return shape;
}

std::string firstLinkTarget(const app::DocumentObject& object, const std::string& property)
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

std::string firstLinkSubname(const app::DocumentObject& object, const std::string& property)
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
    return {};
}

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::optional<std::array<double, 3>> firstScalingDataVector(const app::DocumentObject& object)
{
    const nlohmann::json* payload = propertyPayload(object, "ScalingData");
    if (payload == nullptr || !payload->is_array() || payload->empty()) {
        return std::nullopt;
    }
    const nlohmann::json& first = payload->at(0);
    const auto readComponent = [&](const nlohmann::json& value,
                                   const std::string& key,
                                   std::size_t index) -> std::optional<double> {
        const nlohmann::json* component = nullptr;
        if (value.is_object()) {
            const auto it = value.find(key);
            if (it != value.end()) {
                component = &*it;
            }
        }
        else if (value.is_array() && index < value.size()) {
            component = &value.at(index);
        }
        if (component == nullptr || !component->is_number()) {
            return std::nullopt;
        }
        const double number = component->get<double>();
        if (!std::isfinite(number)) {
            return std::nullopt;
        }
        return number;
    };
    const auto x = readComponent(first, "x", 0U);
    const auto y = readComponent(first, "y", 1U);
    const auto z = readComponent(first, "z", 2U);
    if (!x || !y || !z) {
        return std::nullopt;
    }
    return std::array<double, 3>{*x, *y, *z};
}

bool rejectExactPipeBlocker(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            const std::string& property,
                            const std::string& message)
{
    addPipeDiagnostic(object,
                      context,
                      "unsupported_property",
                      message,
                      property,
                      firstLinkTarget(object, property),
                      firstLinkSubname(object, property));
    return false;
}

PipeLawResolution resolvePipeLaw(const app::DocumentObject& object,
                                 runtime::ComputeContext& context,
                                 int transformation)
{
    PipeLawResolution resolution;
    if (transformation < 2) {
        return resolution;
    }
    if (transformation == 4) {
        addPipeDiagnostic(
            object,
            context,
            "product_contract_required",
            "PartDesign Pipe Transformation=Interpolation requires a reopened LawSamples product contract; C6-M1 does not map it to Linear or S-shape",
            "Transformation"
        );
        resolution.ok = false;
        resolution.metadata = {
            {"kind", "Interpolation"},
            {"status", "product_contract_required"},
            {"source", "freecad_enum_only"},
            {"contract", "product_contract_required"},
        };
        return resolution;
    }

    const auto scalingData = firstScalingDataVector(object);
    if (!scalingData) {
        if (app::propertyValue(object, "ScalingData") == nullptr) {
            rejectExactPipeBlocker(
                object,
                context,
                "Transformation",
                "PartDesign Pipe scaling-law Transformation is source-blocked until a C6 PipeLaw ScalingData DTO is provided"
            );
        }
        else {
            addPipeDiagnostic(
                object,
                context,
                "invalid_pipe_law_data",
                "PartDesign Pipe ScalingData must provide a finite first vector for the C6 PipeLaw product contract",
                "ScalingData"
            );
        }
        resolution.ok = false;
        return resolution;
    }

    if (transformation == 2) {
        resolution.law = part::PipeScalingLaw {
            part::PipeScalingLawKind::Linear,
            (*scalingData)[0],
            1.0,
            1.0,
        };
        resolution.metadata = {
            {"kind", "Linear"},
            {"source", "freecad_source_commented"},
            {"contract", "cad_core_product_contract"},
            {"domain", {0.0, 1.0}},
            {"parameters", {{"start_scale", 1.0}, {"end_scale", (*scalingData)[0]}}},
        };
        return resolution;
    }

    resolution.law = part::PipeScalingLaw {
        part::PipeScalingLawKind::SShape,
        (*scalingData)[0],
        (*scalingData)[1],
        (*scalingData)[2],
    };
    resolution.metadata = {
        {"kind", "S-shape"},
        {"source", "freecad_source_commented"},
        {"contract", "cad_core_product_contract"},
        {"domain", {0.0, 1.0}},
        {"parameters", {{"x", (*scalingData)[0]}, {"y", (*scalingData)[1]}, {"z", (*scalingData)[2]}}},
    };
    return resolution;
}

bool rejectSourceBlockedPipeBranches(const app::DocumentObject& object,
                                     runtime::ComputeContext& context,
                                     int transformation)
{
    bool ok = true;
    if (transformation == 1 && app::readLinks(object, "Sections").empty()) {
        ok = rejectExactPipeBlocker(
                 object,
                 context,
                 "Sections",
                 "PartDesign Pipe Transformation=Multisection requires Sections"
             )
            && ok;
    }
    return ok;
}

part::PipeShellMode pipeShellMode(int mode)
{
    switch (mode) {
        case 1:
            return part::PipeShellMode::Fixed;
        case 2:
            return part::PipeShellMode::Frenet;
        case 3:
            return part::PipeShellMode::Auxiliary;
        case 4:
            return part::PipeShellMode::Binormal;
        default:
            return part::PipeShellMode::Standard;
    }
}

nlohmann::json continuousEdgeLedgerJson(const part::ContinuousEdgeLedger& ledger)
{
    nlohmann::json evidence = nlohmann::json::array();
    for (const part::ContinuousEdgeAdjacencyEvidence& item : ledger.adjacencyEvidence) {
        evidence.push_back({
            {"source_subname", item.sourceSubname},
            {"candidate_subname", item.candidateSubname},
            {"shared_vertex", item.sharedVertex},
            {"continuity", item.continuity},
            {"decision", item.decision},
        });
    }
    return {
        {"source_object", ledger.sourceObject},
        {"requested_subnames", ledger.requestedSubnames},
        {"expanded_subnames", ledger.expandedSubnames},
        {"continuity_rule", ledger.continuityRule},
        {"adjacency_evidence", evidence},
        {"rejection_reason", ledger.rejectionReason},
    };
}

std::optional<PipeInput> resolveSectionLink(const app::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const app::Link& link,
                                            const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // Pipe::execute() local getSectionShape(), for Part2DObject and non-Vertex sub-selection,
    // takes the whole sketch Shape; otherwise Part::Feature requires a concrete sub-element.
    if (link.object.empty()) {
        addPipeDiagnostic(object, context, "missing_link_target", "Pipe: Could not obtain section shape", property);
        return std::nullopt;
    }
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "missing_link_target",
                          "Pipe: Could not obtain section shape",
                          property,
                          link.object);
        return std::nullopt;
    }
    const part::NamedShape* namedShape = namedShapeForTarget(context, link.object);
    const bool useEntireSketch = targetIsSketch(context, link.object)
        && (link.subnames.empty() || link.subnames.front().rfind("Vertex", 0U) != 0U);
    if (link.subnames.empty() || useEntireSketch) {
        return PipeInput{link.object, {}, shapeIt->second.shape, shapeIt->second.shape, namedShape};
    }
    const auto subshape = linkedSubshape(shapeIt->second.shape, namedShape, link, 0U);
    if (!subshape || subshape->IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "invalid_subshape",
                          "Pipe: Could not obtain section shape",
                          property,
                          link.object,
                          link.subnames.front());
        return std::nullopt;
    }
    return PipeInput{link.object, link.subnames.front(), *subshape, shapeIt->second.shape, namedShape};
}

std::optional<std::vector<PipeInput>> resolveSections(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    std::vector<PipeInput> sections;
    for (const app::Link& sectionLink : app::readLinks(object, "Sections")) {
        const auto section = resolveSectionLink(object, context, sectionLink, "Sections");
        if (!section) {
            return std::nullopt;
        }
        sections.push_back(*section);
    }
    return sections;
}

std::optional<PipeInput> resolveProfile(const app::DocumentObject& object,
                                        runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // Pipe::execute() calls getSectionShape(Profile.getValue(), Profile.getSubValues()); for a
    // Part2DObject whose selected subname is not "Vertex*" it takes the whole sketch Shape.
    if (app::propertyValue(object, "Profile") == nullptr) {
        addPipeDiagnostic(object,
                          context,
                          "missing_property",
                          "Pipe Profile must link to a Sketch object",
                          "Profile");
        return std::nullopt;
    }
    const auto link = app::readLink(object, "Profile");
    if (!link || link->object.empty()) {
        addPipeDiagnostic(object,
                          context,
                          "missing_property",
                          "Pipe Profile must link to a Sketch object",
                          "Profile");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "missing_link_target",
                          "Pipe: Could not obtain profile shape",
                          "Profile",
                          link->object);
        return std::nullopt;
    }

    const part::NamedShape* namedShape = namedShapeForTarget(context, link->object);
    const bool useEntireSketch = targetIsSketch(context, link->object)
        && (link->subnames.empty() || link->subnames.front().rfind("Vertex", 0U) != 0U);
    if (link->subnames.empty() || useEntireSketch) {
        return PipeInput{link->object, {}, shapeIt->second.shape, shapeIt->second.shape, namedShape};
    }

    const auto subshape = linkedSubshape(shapeIt->second.shape, namedShape, *link, 0U);
    if (!subshape || subshape->IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "invalid_subshape",
                          "Pipe: Could not obtain profile shape",
                          "Profile",
                          link->object,
                          link->subnames.front());
        return std::nullopt;
    }
    return PipeInput{link->object, link->subnames.front(), *subshape, shapeIt->second.shape, namedShape};
}

std::optional<PipeInput> resolveSpine(const app::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      bool tangentExpansion)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // Pipe::execute() reads "Spine", passes Spine.getSubValues() to buildPipePath(), and
    // buildPipePath() accepts selected edges, a direct Edge/Wire, or a Compound of edges/wires.
    if (app::propertyValue(object, "Spine") == nullptr) {
        addPipeDiagnostic(object, context, "missing_property", "No spine linked", "Spine");
        return std::nullopt;
    }
    const auto link = app::readLink(object, "Spine");
    if (!link || link->object.empty()) {
        addPipeDiagnostic(object, context, "missing_property", "No spine linked", "Spine");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "missing_link_target",
                          "Invalid spine",
                          "Spine",
                          link->object);
        return std::nullopt;
    }
    const part::NamedShape* namedShape = namedShapeForTarget(context, link->object);

    TopoDS_Shape spine = shapeIt->second.shape;
    std::optional<part::ContinuousEdgeLedger> ledger;
    if (!link->subnames.empty()) {
        if (tangentExpansion) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
            // FeaturePipe.cpp::buildPipePath(), commented call "getContinuousEdges(shape,
            // subedge)" only applies to selected spine edges, so CAD Core keeps this
            // request-local and does not expand whole-wire/compound inputs.
            ledger = part::expandContinuousEdgesForPipePath(link->object, shapeIt->second.shape, link->subnames);
            if (!ledger->rejectionReason.empty()) {
                addPipeDiagnostic(object,
                                  context,
                                  ledger->rejectionReason,
                                  "PartDesign Pipe SpineTangent continuous-edge expansion rejected the selected spine path",
                                  "SpineTangent",
                                  link->object,
                                  link->subnames.empty() ? std::string {} : link->subnames.front());
                return std::nullopt;
            }
            if (!ledger->expandedShape.IsNull()) {
                spine = ledger->expandedShape;
            }
        }
        else {
            std::vector<TopoDS_Shape> selected;
            selected.reserve(link->subnames.size());
            for (std::size_t index = 0; index < link->subnames.size(); ++index) {
                const auto subshape = linkedSubshape(shapeIt->second.shape, namedShape, *link, index);
                if (!subshape || subshape->IsNull()) {
                    const std::string subname = index < link->subnames.size() ? link->subnames.at(index) : std::string {};
                    addPipeDiagnostic(object,
                                      context,
                                      "invalid_subshape",
                                      "Invalid spine",
                                      "Spine",
                                      link->object,
                                      subname);
                    return std::nullopt;
                }
                selected.push_back(*subshape);
            }
            spine = compoundOf(selected);
        }
    }

    PipeInput input{link->object, {}, spine, shapeIt->second.shape, namedShape};
    input.requestedSubnames = link->subnames;
    input.continuousEdgeLedger = ledger;
    return input;
}

std::optional<PipeInput> resolveAuxiliarySpine(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               bool tangentExpansion)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::execute(), when "Mode.getValue() == 3", reads AuxiliarySpine.getSubValues(),
    // calls buildPipePath(auxshape, auxsubedge, auxpath), then setupAlgorithm() passes auxpath to
    // "SetMode(TopoDS::Wire(auxshape), AuxiliaryCurvilinear.getValue())".
    if (app::propertyValue(object, "AuxiliarySpine") == nullptr) {
        addPipeDiagnostic(
            object,
            context,
            "missing_property",
            "No auxiliary spine linked.",
            "AuxiliarySpine"
        );
        return std::nullopt;
    }
    const auto link = app::readLink(object, "AuxiliarySpine");
    if (!link || link->object.empty()) {
        addPipeDiagnostic(
            object,
            context,
            "missing_property",
            "No auxiliary spine linked.",
            "AuxiliarySpine"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "missing_link_target",
                          "Invalid auxiliary spine",
                          "AuxiliarySpine",
                          link->object);
        return std::nullopt;
    }
    const part::NamedShape* namedShape = namedShapeForTarget(context, link->object);

    TopoDS_Shape auxiliary = shapeIt->second.shape;
    std::optional<part::ContinuousEdgeLedger> ledger;
    if (!link->subnames.empty()) {
        if (tangentExpansion) {
            ledger = part::expandContinuousEdgesForPipePath(link->object, shapeIt->second.shape, link->subnames);
            if (!ledger->rejectionReason.empty()) {
                addPipeDiagnostic(object,
                                  context,
                                  ledger->rejectionReason,
                                  "PartDesign Pipe AuxiliarySpineTangent continuous-edge expansion rejected the selected auxiliary spine path",
                                  "AuxiliarySpineTangent",
                                  link->object,
                                  link->subnames.empty() ? std::string {} : link->subnames.front());
                return std::nullopt;
            }
            if (!ledger->expandedShape.IsNull()) {
                auxiliary = ledger->expandedShape;
            }
        }
        else {
            std::vector<TopoDS_Shape> selected;
            selected.reserve(link->subnames.size());
            for (std::size_t index = 0; index < link->subnames.size(); ++index) {
                const auto subshape = linkedSubshape(shapeIt->second.shape, namedShape, *link, index);
                if (!subshape || subshape->IsNull()) {
                    const std::string subname = index < link->subnames.size() ? link->subnames.at(index) : std::string {};
                    addPipeDiagnostic(object,
                                      context,
                                      "invalid_subshape",
                                      "Invalid auxiliary spine",
                                      "AuxiliarySpine",
                                      link->object,
                                      subname);
                    return std::nullopt;
                }
                selected.push_back(*subshape);
            }
            auxiliary = compoundOf(selected);
        }
    }

    PipeInput input{link->object, {}, auxiliary, shapeIt->second.shape, namedShape};
    input.requestedSubnames = link->subnames;
    input.continuousEdgeLedger = ledger;
    return input;
}

std::vector<part::NamedShapeSource> pipeSources(const PipeInput& spine,
                                                const PipeInput& profile,
                                                const std::vector<PipeInput>& sections)
{
    std::vector<part::NamedShapeSource> sources {
        part::NamedShapeSource{
            spine.namedShape != nullptr ? spine.namedShape->owner : spine.objectName,
            spine.shape,
            spine.namedShape,
        },
        part::NamedShapeSource{
            profile.namedShape != nullptr ? profile.namedShape->owner : profile.objectName,
            profile.shape,
            profile.namedShape,
        },
    };
    for (const PipeInput& section : sections) {
        sources.push_back(part::NamedShapeSource{
            section.namedShape != nullptr ? section.namedShape->owner : section.objectName,
            section.shape,
            section.namedShape,
        });
    }
    return sources;
}

void executePipeFeature(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        PipeAddSubMode addSubMode)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // Pipe::execute() resolves "Profile", builds a "Spine" path with buildPipePath(), configures
    // BRepOffsetAPI_MakePipeShell in setupAlgorithm(), writes AddSubShape, then Body fuses/cuts
    // the cached tool for AdditivePipe/SubtractivePipe addSubType.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Profile",
             "Spine",
             "SpineTangent",
             "Sections",
             "Mode",
             "Transition",
             "Transformation",
             "AuxiliarySpine",
             "AuxiliarySpineTangent",
             "AuxiliaryCurvilinear",
             "Binormal",
             "ScalingData",
             "LawSamples",
             "BaseFeature",
             "Refine",
             "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto mode = enumIndex(
        object,
        context,
        "Mode",
        {"Standard", "Fixed", "Frenet", "Auxiliary", "Binormal"},
        0,
        "PartDesign Pipe Mode"
    );
    const auto transformation = enumIndex(
        object,
        context,
        "Transformation",
        {"Constant", "Multisection", "Linear", "S-shape", "Interpolation"},
        0,
        "PartDesign Pipe Transformation"
    );
    const auto transition = enumIndex(
        object,
        context,
        "Transition",
        {"Transformed", "Right corner", "Round corner"},
        0,
        "PartDesign Pipe Transition"
    );
    if (!mode || !transformation || !transition) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!rejectSourceBlockedPipeBranches(object, context, *transformation)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const PipeLawResolution pipeLaw = resolvePipeLaw(object, context, *transformation);
    if (!pipeLaw.ok) {
        context.objects[object.name] = {
            {"status", "error"},
            {"feature", "partdesign_pipe"},
            {"transformation", transformationLabel(*transformation)},
        };
        if (!pipeLaw.metadata.is_null()) {
            context.objects[object.name]["pipe_law"] = pipeLaw.metadata;
        }
        return;
    }

    const auto profile = resolveProfile(object, context);
    if (!profile) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const bool spineTangent = app::readBool(object, "SpineTangent").value_or(false);
    const auto spine = resolveSpine(object, context, spineTangent);
    if (!spine) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    std::vector<PipeInput> sections;
    if (*transformation == 1) {
        const auto resolvedSections = resolveSections(object, context);
        if (!resolvedSections) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        sections = *resolvedSections;
    }

    part::PipeShellOptions pipeOptions;
    pipeOptions.solid = true;
    pipeOptions.mode = pipeShellMode(*mode);
    pipeOptions.transition = *transition;
    pipeOptions.sewCaps = true;
    pipeOptions.scalingLaw = pipeLaw.law;

    std::optional<PipeInput> auxiliarySpine;
    if (*mode == 3) {
        const bool auxiliaryTangent = app::readBool(object, "AuxiliarySpineTangent").value_or(false);
        auxiliarySpine = resolveAuxiliarySpine(object, context, auxiliaryTangent);
        if (!auxiliarySpine) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        pipeOptions.auxiliarySpine = auxiliarySpine->shape;
        pipeOptions.auxiliaryCurvilinear = app::readBool(object, "AuxiliaryCurvilinear").value_or(true);
    }
    if (*mode == 4) {
        const auto binormal = app::readVector3(object, "Binormal");
        if (!binormal) {
            addPipeDiagnostic(object,
                              context,
                              "missing_property",
                              "PartDesign Pipe Mode=Binormal requires Binormal vector",
                              "Binormal");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const double magnitudeSquared = (*binormal)[0] * (*binormal)[0] + (*binormal)[1] * (*binormal)[1]
            + (*binormal)[2] * (*binormal)[2];
        if (magnitudeSquared <= 1.0e-24) {
            addPipeDiagnostic(object,
                              context,
                              "invalid_property",
                              "PartDesign Pipe Mode=Binormal requires non-zero Binormal vector",
                              "Binormal");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        pipeOptions.binormal = *binormal;
    }

    const auto build = part::makeElementPipeShellFromSources(
        object.name,
        pipeSources(*spine, *profile, sections),
        pipeOptions
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addPipeDiagnostic(object,
                          context,
                          "execution_failed",
                          build.error.empty() ? "Pipe could not be built" : build.error,
                          "Spine",
                          spine->objectName);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<part::NamedShape> namedShape = build.namedShape;
    if (namedShape) {
        namedShape->owner = object.name;
        namedShape->shape = build.shape;
        addHistoryStatus(*namedShape, "part_design_pipe:pipeshell_history");
        addHistoryStatus(*namedShape, "part_design_pipe:solidification");
        addHistoryStatus(*namedShape, "part_design_pipe:sewing");
    }

    runtime::RefineShapeResult shapeResult{build.shape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, build.shape, namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        shapeResult = *refined;
    }

    const bool allowCompound = owningBodyAllowsCompound(object, context);
    const int solids = solidCount(shapeResult.shape);
    if (!allowCompound && solids != 1) {
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

    TopoDS_Shape solid = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (!allowCompound && solids == 1) {
        solid = firstSolid(shapeResult.shape);
        if (namedShape) {
            namedShape->shape = solid;
        }
    }
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.mesh[object.name] = cad_core::part::meshForShape(solid);
    context.subshapes[object.name] = part::subshapeMapForShape(solid);

    const bool additive = addSubMode == PipeAddSubMode::Additive;
    if (additive) {
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
        context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt, namedShape, std::nullopt};
    }
    else {
        context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, solid, std::nullopt, namedShape};
    }

    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"feature", "partdesign_pipe"},
        {"add_sub", additive ? "add" : "sub"},
        {"source_profile", profile->objectName},
        {"spine", spine->objectName},
        {"mode", modeLabel(*mode)},
        {"transformation", transformationLabel(*transformation)},
        {"transition", transitionLabel(*transition)},
        {"sections", nlohmann::json::array()},
        {"cap_sewing", "mapper_history:part_design_pipe"},
        {"solid_count", solids},
        {"bbox", cad_core::part::objectBBoxForShape(solid)},
        {"volume", cad_core::part::volumeForShape(solid)},
        {"topo_naming_history", "maker_history:partdesign_pipe"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (auxiliarySpine) {
        result["auxiliary_spine"] = auxiliarySpine->objectName;
        result["auxiliary_curvilinear"] = pipeOptions.auxiliaryCurvilinear;
    }
    if (!pipeLaw.metadata.is_null()) {
        result["pipe_law"] = pipeLaw.metadata;
    }
    if (spine->continuousEdgeLedger) {
        result["continuous_edge_ledger"] = continuousEdgeLedgerJson(*spine->continuousEdgeLedger);
    }
    if (auxiliarySpine && auxiliarySpine->continuousEdgeLedger) {
        result["auxiliary_continuous_edge_ledger"]
            = continuousEdgeLedgerJson(*auxiliarySpine->continuousEdgeLedger);
    }
    if (*mode == 4) {
        result["binormal"] = pipeOptions.binormal;
    }
    for (const PipeInput& section : sections) {
        result["sections"].push_back(section.objectName);
    }
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace

void executeAdditivePipe(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executePipeFeature(object, context, PipeAddSubMode::Additive);
}

void executeSubtractivePipe(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executePipeFeature(object, context, PipeAddSubMode::Subtractive);
}

}  // namespace cad_core::part_design
