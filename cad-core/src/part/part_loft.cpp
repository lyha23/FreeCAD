#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <Geom_BSplineSurface.hxx>
#include <Standard_Failure.hxx>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct LoftSection
{
    std::string objectName;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

void addLoftDiagnostic(
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
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_loft"}};
}

std::optional<int> readLoftMaxDegree(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Loft::Degrees initializes MaxDegree constraints as "{2, Geom_BSplineSurface::MaxDegree(), 1}".
    const int maxDegree = static_cast<int>(
        std::llround(app::readNumber(object, "MaxDegree").value_or(5.0))
    );
    if (maxDegree < 2 || maxDegree > Geom_BSplineSurface::MaxDegree()) {
        addLoftDiagnostic(
            object,
            context,
            "invalid_property",
            "Part::Loft MaxDegree must be within FreeCAD's B-spline surface degree constraint",
            "MaxDegree"
        );
        return std::nullopt;
    }
    return maxDegree;
}

std::optional<std::vector<LoftSection>> resolveLoftSections(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Loft::execute(), reads "Sections" App::PropertyLinkList and resolves every linked object
    // through getTopoShape(..., "ResolveLink | Transform") before calling makeElementLoft().
    const std::vector<app::Link> links = app::readLinks(object, "Sections");
    if (links.empty()) {
        addLoftDiagnostic(object, context, "missing_property", "No sections linked.", "Sections");
        return std::nullopt;
    }

    std::vector<LoftSection> sections;
    sections.reserve(links.size());
    for (const auto& link : links) {
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
            addLoftDiagnostic(
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
            LoftSection {
                link.object,
                shapeIt->second.shape,
                namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
            }
        );
    }
    return sections;
}

nlohmann::json sectionNamesJson(const std::vector<LoftSection>& sections)
{
    nlohmann::json names = nlohmann::json::array();
    for (const auto& section : sections) {
        names.push_back(section.objectName);
    }
    return names;
}

std::vector<NamedShapeSource> loftSourcesForSections(const std::vector<LoftSection>& sections)
{
    std::vector<NamedShapeSource> sources;
    sources.reserve(sections.size());
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

void executePartLoft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Loft::execute(), consumes Sections/Solid/Ruled/Closed/MaxDegree, calls
    // "result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)", then applies
    // Linearize only as a post-processing step.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Sections", "Solid", "Ruled", "Closed", "Linearize", "MaxDegree"}
        )) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_loft"}};
        return;
    }

    const bool linearize = app::readBool(object, "Linearize").value_or(false);
    const auto sections = resolveLoftSections(object, context);
    if (!sections) {
        return;
    }
    const auto maxDegree = readLoftMaxDegree(object, context);
    if (!maxDegree) {
        return;
    }

    const bool solid = app::readBool(object, "Solid").value_or(true);
    const bool ruled = app::readBool(object, "Ruled").value_or(false);
    const bool closed = app::readBool(object, "Closed").value_or(false);
    const auto build = makeElementLoftFromSources(
        object.name,
        loftSourcesForSections(*sections),
        solid,
        ruled,
        closed,
        *maxDegree,
        linearize
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addLoftDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::Loft failed" : build.error,
            "Sections"
        );
        return;
    }

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        {
            {"feature", "part_loft"},
            {"sections", sectionNamesJson(*sections)},
            {"solid", solid},
            {"ruled", ruled},
            {"closed", closed},
            {"linearize", linearize},
            {"max_degree", *maxDegree},
            {"topo_naming_history", "maker_history:loft_thru_sections"},
        },
        build.namedShape
    );
}

}  // namespace cad_core::part
