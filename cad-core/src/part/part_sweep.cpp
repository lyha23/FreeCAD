#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <cmath>
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

void addSweepDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {}
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
        target
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

bool rejectDeferredSweepAdvancedProperties(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
    // ::makeSweepSurface(), exposes a helper tolerance/fillMode wrapper; PartFeatures.cpp
    // ::Sweep::execute() only publishes Sections/Spine/Solid/Frenet/Transition/Linearize.
    static const std::vector<std::string> deferred {
        "AuxiliarySpine",
        "SupportMode",
        "BiNormal",
        "LocationMode",
        "Tolerance",
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
             "SupportMode",
             "BiNormal",
             "LocationMode",
             "Tolerance"}
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
    const auto build = makeElementPipeShellFromSources(
        object.name,
        sweepSources(*spine, *sections),
        solid,
        frenet,
        *transition,
        linearize
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addSweepDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::Sweep failed" : build.error,
            "Spine"
        );
        return;
    }

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        {
            {"feature", "part_sweep"},
            {"spine", spine->objectName},
            {"sections", sectionNamesJson(*sections)},
            {"solid", solid},
            {"frenet", frenet},
            {"transition", transitionLabel(*transition)},
            {"linearize", linearize},
            {"topo_naming_history", "maker_history:pipeshell"},
        },
        build.namedShape
    );
}

}  // namespace cad_core::part
