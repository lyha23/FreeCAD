#include "cad_core/part_design/feature_pipe.h"

#include "cad_core/app/property.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
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
    const part::NamedShape* namedShape = nullptr;
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

bool rejectDeferredPipeBranch(const app::DocumentObject& object,
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

bool rejectDeferredPipeBranches(const app::DocumentObject& object,
                                runtime::ComputeContext& context,
                                int mode,
                                int transformation,
                                int transition)
{
    bool ok = true;
    if (mode != 0) {
        ok = rejectDeferredPipeBranch(object,
                                      context,
                                      "Mode",
                                      "PartDesign Pipe first slice supports Mode=Standard only") && ok;
    }
    if (transformation != 0) {
        ok = rejectDeferredPipeBranch(
                 object,
                 context,
                 "Transformation",
                 "PartDesign Pipe first slice supports Transformation=Constant only"
            )
            && ok;
    }
    if (transition != 0) {
        ok = rejectDeferredPipeBranch(
                 object,
                 context,
                 "Transition",
                 "PartDesign Pipe first slice supports Transition=Transformed only"
             )
            && ok;
    }
    if (!app::readLinks(object, "Sections").empty()) {
        ok = rejectDeferredPipeBranch(
                 object,
                 context,
                 "Sections",
                 "PartDesign Pipe multisection profiles are deferred until Transformation=Multisection is modeled"
             )
            && ok;
    }
    for (const std::string& property :
         {"AuxiliarySpine", "AuxiliarySpineTangent", "AuxiliaryCurvilinear", "Binormal"}) {
        if (app::propertyValue(object, property) != nullptr) {
            ok = rejectDeferredPipeBranch(
                     object,
                     context,
                     property,
                     "PartDesign Pipe Auxiliary/Binormal orientation branch is deferred"
                 )
                && ok;
        }
    }
    if (app::readBool(object, "SpineTangent").value_or(false)) {
        ok = rejectDeferredPipeBranch(
                 object,
                 context,
                 "SpineTangent",
                 "PartDesign Pipe tangent spine expansion is deferred"
             )
            && ok;
    }
    return ok;
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
        return PipeInput{link->object, {}, shapeIt->second.shape, namedShape};
    }

    if (link->subnames.size() > 1U) {
        addPipeDiagnostic(object,
                          context,
                          "unsupported_property",
                          "PartDesign Pipe explicit multi-subelement Profile selection is deferred",
                          "Profile",
                          link->object,
                          link->subnames.front());
        return std::nullopt;
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
    return PipeInput{link->object, link->subnames.front(), *subshape, namedShape};
}

std::optional<PipeInput> resolveSpine(const app::DocumentObject& object,
                                      runtime::ComputeContext& context)
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
    if (!link->subnames.empty()) {
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

    return PipeInput{link->object, {}, spine, namedShape};
}

std::vector<part::NamedShapeSource> pipeSources(const PipeInput& spine,
                                                const PipeInput& profile)
{
    return {
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
    if (!rejectDeferredPipeBranches(object, context, *mode, *transformation, *transition)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto profile = resolveProfile(object, context);
    if (!profile) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto spine = resolveSpine(object, context);
    if (!spine) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto build = part::makeElementPipeShellFromSources(
        object.name,
        pipeSources(*spine, *profile),
        true,
        false,
        *transition,
        false
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

    const TopoDS_Shape solid = shapeResult.shape;
    namedShape = shapeResult.namedShape;
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
        {"mode", "Standard"},
        {"transformation", "Constant"},
        {"transition", transitionLabel(*transition)},
        {"bbox", cad_core::part::bboxForShape(solid)},
        {"volume", cad_core::part::volumeForShape(solid)},
        {"topo_naming_history", "maker_history:partdesign_pipe"},
        {"kernel", cad_core::part::kernelVersion()},
    };
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
