#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRep_Builder.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>

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
    std::vector<std::string> subnames;
    std::vector<std::string> stableSubnames;
    std::string shapeLabel;
};

void addLoftDiagnostic(
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
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_loft"}};
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.size() == 1U) {
        return shapes.front();
    }

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

int subshapeCount(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shape, kind, subshapes);
    return subshapes.Extent();
}

bool isLoftProfileCandidate(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    switch (shape.ShapeType()) {
        case TopAbs_VERTEX:
        case TopAbs_EDGE:
        case TopAbs_WIRE:
        case TopAbs_FACE:
            return true;
        default:
            break;
    }
    return subshapeCount(shape, TopAbs_FACE) == 1 || subshapeCount(shape, TopAbs_WIRE) == 1
        || subshapeCount(shape, TopAbs_VERTEX) == 1 || subshapeCount(shape, TopAbs_EDGE) > 0;
}

std::string linkSubname(const app::Link& link, std::size_t index)
{
    return index < link.subnames.size() ? link.subnames.at(index) : std::string {};
}

std::string linkStableSubname(const app::Link& link, std::size_t index)
{
    const std::string current = linkSubname(link, index);
    return index < link.stableSubnames.size() && !link.stableSubnames.at(index).empty()
        ? link.stableSubnames.at(index)
        : current;
}

std::optional<TopoDS_Shape> linkSubShape(
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape,
    const app::Link& link,
    std::size_t index
)
{
    const std::string current = linkSubname(link, index);
    if (current.empty()) {
        return std::nullopt;
    }

    const std::string stable = linkStableSubname(link, index);
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

std::optional<TopoDS_Shape> selectedLoftSectionShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& link,
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape
)
{
    // FreeCAD native boundary:
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Loft::execute()
    // consumes "Sections.getValues()" from App::PropertyLinkList. The SubList path below is
    // a C6-M7 request-local CAD Core product contract, not a FreeCAD native expected path.
    std::vector<TopoDS_Shape> selected;
    selected.reserve(link.subnames.size());
    for (std::size_t index = 0; index < link.subnames.size(); ++index) {
        const auto subshape = linkSubShape(sourceShape, namedShape, link, index);
        const std::string subname = linkSubname(link, index);
        if (!subshape || subshape->IsNull()) {
            addLoftDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Invalid Loft section subshape",
                "Sections",
                link.object,
                subname
            );
            return std::nullopt;
        }
        selected.push_back(*subshape);
    }

    if (selected.empty()) {
        addLoftDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Invalid Loft section subshape",
            "Sections",
            link.object
        );
        return std::nullopt;
    }

    TopoDS_Shape shape = compoundOf(selected);
    if (!isLoftProfileCandidate(shape)) {
        addLoftDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Selected Loft section subshape is not a supported profile",
            "Sections",
            link.object,
            linkSubname(link, 0U)
        );
        return std::nullopt;
    }
    return shape;
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
        const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
            ? &namedShapeIt->second
            : nullptr;
        TopoDS_Shape sectionShape = shapeIt->second.shape;
        if (!link.subnames.empty()) {
            const auto selectedShape = selectedLoftSectionShape(
                object,
                context,
                link,
                shapeIt->second.shape,
                namedShape
            );
            if (!selectedShape) {
                return std::nullopt;
            }
            sectionShape = *selectedShape;
        }
        sections.push_back(
            LoftSection {
                link.object,
                sectionShape,
                namedShape,
                link.subnames,
                link.stableSubnames,
                part_feature_detail::shapeLabelForPartShape(sectionShape),
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

bool hasSelectedSections(const std::vector<LoftSection>& sections)
{
    for (const auto& section : sections) {
        if (!section.subnames.empty()) {
            return true;
        }
    }
    return false;
}

nlohmann::json selectedSectionEntryJson(const LoftSection& section)
{
    nlohmann::json entry = {
        {"object", section.objectName},
        {"shape", section.shapeLabel},
        {"selected", !section.subnames.empty()},
    };
    if (!section.subnames.empty()) {
        entry["subnames"] = section.subnames;
        entry["stable_subnames"] = section.stableSubnames;
        entry["contract"] = "cad_core_product_contract";
        entry["contract_provenance"] = "cad_core_product_contract_non_parity";
        if (section.subnames.size() == 1U) {
            entry["subname"] = section.subnames.front();
            entry["stable_subname"] = section.stableSubnames.empty()
                ? section.subnames.front()
                : section.stableSubnames.front();
        }
    }
    return entry;
}

nlohmann::json sectionEntriesJson(const std::vector<LoftSection>& sections)
{
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& section : sections) {
        entries.push_back(selectedSectionEntryJson(section));
    }
    return entries;
}

nlohmann::json selectedSectionsJson(const std::vector<LoftSection>& sections)
{
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& section : sections) {
        if (!section.subnames.empty()) {
            entries.push_back(selectedSectionEntryJson(section));
        }
    }
    return entries;
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

    nlohmann::json metadata = {
        {"feature", "part_loft"},
        {"sections", sectionNamesJson(*sections)},
        {"solid", solid},
        {"ruled", ruled},
        {"closed", closed},
        {"linearize", linearize},
        {"max_degree", *maxDegree},
        {"topo_naming_history", "maker_history:loft_thru_sections"},
    };
    if (hasSelectedSections(*sections)) {
        metadata["contract"] = "cad_core_product_contract";
        metadata["contract_provenance"] = "cad_core_product_contract_non_parity";
        metadata["freecad_native_expected"] = false;
        metadata["sections_contract"] = {
            {"property", "Sections"},
            {"source", "request_local_values_sublist"},
            {"freecad_native_property", "App::PropertyLinkList"},
            {"native_parity", false},
        };
        metadata["section_entries"] = sectionEntriesJson(*sections);
        metadata["selected_sections"] = selectedSectionsJson(*sections);
    }

    part_feature_detail::publishPartShape(
        object,
        context,
        build.shape,
        metadata,
        build.namedShape
    );
}

}  // namespace cad_core::part
