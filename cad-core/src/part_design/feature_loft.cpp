#include "cad_core/part_design/feature_loft.h"

#include "cad_core/app/property.h"
#include "cad_core/part/face_maker.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRep_Builder.hxx>
#include <Precision.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

enum class LoftAddSubMode {
    Additive,
    Subtractive,
};

struct SectionShape {
    std::string objectName;
    std::string subname;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

struct SolidifiedLoft {
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    int shellCount = 0;
};

void addLoftDiagnostic(const app::DocumentObject& object,
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

bool targetIsSketch(const runtime::ComputeContext& context, const std::string& objectName)
{
    const auto objectIt = context.documentObjects.find(objectName);
    return objectIt != context.documentObjects.end() && objectIt->second != nullptr
        && objectIt->second->typeId == "Sketcher::SketchObject";
}

const part::NamedShape* namedShapeForTarget(const runtime::ComputeContext& context,
                                            const std::string& objectName)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    return namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
}

std::optional<TopoDS_Wire> wireFromEdges(const TopoDS_Shape& shape)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    int edgeCount = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        wireBuilder.Add(TopoDS::Edge(explorer.Current()));
        ++edgeCount;
    }
    if (edgeCount == 0 || !wireBuilder.IsDone()) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::vector<TopoDS_Shape> sectionElementsFromShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> wires;
    for (TopExp_Explorer explorer(shape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        wires.push_back(explorer.Current());
    }
    if (!wires.empty()) {
        return wires;
    }

    if (const auto wire = wireFromEdges(shape)) {
        return {TopoDS_Shape(*wire)};
    }

    std::vector<TopoDS_Shape> vertices;
    for (TopExp_Explorer explorer(shape, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        vertices.push_back(TopoDS::Vertex(explorer.Current()));
    }
    return vertices;
}

std::optional<std::vector<SectionShape>> getSectionShape(const app::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const char* name,
                                                         const app::Link& link,
                                                         std::size_t expectedSize = 0U)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLoft.cpp
    // ::Loft::getSectionShape(), when a Part2DObject sub-selection is not Vertex*, consumes
    // the entire sketch; otherwise it resolves each selected sub-element before extracting wires
    // or vertices and enforcing equal wire/vertex counts.
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addLoftDiagnostic(object,
                          context,
                          "missing_link_target",
                          std::string("Failed to get shape of ") + name,
                          name,
                          link.object);
        return std::nullopt;
    }

    std::vector<SectionShape> rawShapes;
    const bool useEntireSketch = targetIsSketch(context, link.object)
        && (link.subnames.empty() || link.subnames.front().rfind("Vertex", 0U) != 0U);
    if (link.subnames.empty() || useEntireSketch) {
        rawShapes.push_back(SectionShape{link.object, {}, shapeIt->second.shape, namedShapeForTarget(context, link.object)});
    }
    else {
        const part::NamedShape* namedShape = namedShapeForTarget(context, link.object);
        for (std::size_t index = 0; index < link.subnames.size(); ++index) {
            const std::string& subname = link.subnames.at(index);
            const std::string stableSubname
                = index < link.stableSubnames.size() ? link.stableSubnames.at(index) : std::string {};
            std::optional<TopoDS_Shape> subshape;
            if (namedShape != nullptr) {
                subshape = part::subshapeByName(*namedShape, subname, stableSubname);
            }
            if (!subshape) {
                subshape = part::subshapeByName(shapeIt->second.shape, subname);
            }
            if (!subshape) {
                addLoftDiagnostic(object,
                                  context,
                                  "invalid_subshape",
                                  std::string("Failed to get shape of ") + name,
                                  name,
                                  link.object,
                                  subname);
                return std::nullopt;
            }
            rawShapes.push_back(SectionShape{link.object, subname, *subshape, namedShape});
        }
    }

    std::vector<SectionShape> result;
    for (const SectionShape& raw : rawShapes) {
        for (const TopoDS_Shape& element : sectionElementsFromShape(raw.shape)) {
            result.push_back(SectionShape{raw.objectName, raw.subname, element, raw.namedShape});
        }
    }

    if (result.empty()) {
        addLoftDiagnostic(object,
                          context,
                          "invalid_profile",
                          std::string("Invalid ") + name + " shape, expecting either wires or vertices",
                          name,
                          link.object);
        return std::nullopt;
    }
    if (expectedSize != 0U && expectedSize != result.size()) {
        addLoftDiagnostic(
            object,
            context,
            "invalid_sections",
            "Sections need to have the same amount of wires or vertices as the base section",
            name,
            link.object
        );
        return std::nullopt;
    }
    return result;
}

std::vector<part::NamedShapeSource> sourcesForSectionShapes(const std::vector<SectionShape>& shapes)
{
    std::vector<part::NamedShapeSource> sources;
    sources.reserve(shapes.size());
    for (const SectionShape& shape : shapes) {
        sources.push_back(part::NamedShapeSource{shape.objectName, shape.shape, shape.namedShape});
    }
    return sources;
}

std::optional<TopoDS_Shape> profileFace(const app::DocumentObject& object,
                                        runtime::ComputeContext& context,
                                        const app::Link& profileLink)
{
    const auto shapeIt = context.shapes.find(profileLink.object);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch) {
        return shapeIt->second.profileShape;
    }
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Profile) {
        return shapeIt->second.shape;
    }
    addLoftDiagnostic(object,
                      context,
                      "missing_link_target",
                      "Profile target " + profileLink.object + " did not produce a profile",
                      "Profile",
                      profileLink.object);
    return std::nullopt;
}

std::optional<TopoDS_Shape> backFaceForSectionWires(const std::vector<std::vector<SectionShape>>& wireSections)
{
    std::vector<TopoDS_Wire> backWires;
    backWires.reserve(wireSections.size());
    for (const auto& section : wireSections) {
        if (section.empty() || section.back().shape.ShapeType() == TopAbs_VERTEX) {
            return std::nullopt;
        }
        const auto elements = sectionElementsFromShape(section.back().shape);
        if (elements.size() != 1U || elements.front().ShapeType() != TopAbs_WIRE) {
            return std::nullopt;
        }
        backWires.push_back(TopoDS::Wire(elements.front()));
    }
    return part::makeFaceWithHolesFromClosedWires(backWires);
}

std::optional<SolidifiedLoft> buildSolidifiedLoftTool(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context,
                                                      const app::Link& profileLink,
                                                      const std::vector<std::vector<SectionShape>>& wireSections,
                                                      bool ruled,
                                                      bool closed)
{
    std::vector<TopoDS_Shape> shells;
    std::vector<part::NamedShape> shellNamedShapes;
    shells.reserve(wireSections.size());
    shellNamedShapes.reserve(wireSections.size());

    for (const std::vector<SectionShape>& section : wireSections) {
        const auto build = part::makeElementLoftFromSources(
            object.name,
            sourcesForSectionShapes(section),
            false,
            ruled,
            closed,
            5,
            false
        );
        if (!build.error.empty() || build.shape.IsNull()) {
            addLoftDiagnostic(object,
                              context,
                              "execution_failed",
                              build.error.empty() ? "Loft: Failed to create shell" : build.error,
                              "Sections");
            return std::nullopt;
        }
        shells.push_back(build.shape);
        if (build.namedShape) {
            shellNamedShapes.push_back(*build.namedShape);
        }
    }

    TopoDS_Shape front;
    if (!wireSections.empty() && !wireSections.front().empty()
        && wireSections.front().front().shape.ShapeType() != TopAbs_VERTEX) {
        const auto face = profileFace(object, context, profileLink);
        if (!face || face->IsNull()) {
            addLoftDiagnostic(object,
                              context,
                              "open_profile",
                              "Loft: Creating a face from sketch failed",
                              "Profile",
                              profileLink.object);
            return std::nullopt;
        }
        front = *face;
    }

    TopoDS_Shape back;
    if (!wireSections.empty() && !wireSections.front().empty()
        && wireSections.front().back().shape.ShapeType() != TopAbs_VERTEX) {
        const auto face = backFaceForSectionWires(wireSections);
        if (!face || face->IsNull()) {
            addLoftDiagnostic(object,
                              context,
                              "execution_failed",
                              "Loft: Creating a face from section failed",
                              "Sections");
            return std::nullopt;
        }
        back = *face;
    }

    BRepBuilderAPI_Sewing sewing;
    sewing.SetTolerance(Precision::Confusion());
    if (!front.IsNull()) {
        sewing.Add(front);
    }
    if (!back.IsNull()) {
        sewing.Add(back);
    }
    for (const TopoDS_Shape& shell : shells) {
        sewing.Add(shell);
    }
    sewing.Perform();
    TopoDS_Shape sewed = sewing.SewedShape();
    if (sewed.IsNull()) {
        addLoftDiagnostic(object, context, "execution_failed", "Loft: Failed to create shell", "Sections");
        return std::nullopt;
    }

    std::vector<part::NamedShapeSource> sewingSources;
    sewingSources.reserve(shells.size() + 2U);
    const part::NamedShape* profileNamedShape = namedShapeForTarget(context, profileLink.object);
    if (!front.IsNull()) {
        sewingSources.push_back(part::NamedShapeSource{profileLink.object, front, profileNamedShape});
    }
    for (std::size_t index = 0; index < shells.size(); ++index) {
        const part::NamedShape* shellNamedShape = index < shellNamedShapes.size() ? &shellNamedShapes.at(index) : nullptr;
        sewingSources.push_back(part::NamedShapeSource{object.name, shells.at(index), shellNamedShape});
    }
    if (!back.IsNull()) {
        sewingSources.push_back(part::NamedShapeSource{object.name + ".SectionFace", back, nullptr});
    }
    auto sewedNamedShape = part::namedShapeForPreservedSources(object.name, sewed, sewingSources);
    addHistoryStatus(sewedNamedShape, "part_design_loft:sewing");

    part::NamedShapeSource solidSource{object.name, sewed, &sewedNamedShape};
    auto solidBuild = part::makeElementSolidFromSource(object.name, solidSource);
    if (!solidBuild.error.empty() || solidBuild.shape.IsNull()) {
        addLoftDiagnostic(object,
                          context,
                          "execution_failed",
                          solidBuild.error.empty() ? "Loft: Failed to build solid" : solidBuild.error,
                          "Sections");
        return std::nullopt;
    }

    TopoDS_Shape solid = solidBuild.shape;
    BRepClass3d_SolidClassifier classifier(solid);
    classifier.PerformInfinitePoint(Precision::Confusion());
    if (classifier.State() == TopAbs_IN) {
        solid = solid.Reversed();
    }
    std::optional<part::NamedShape> namedShape = solidBuild.namedShape;
    if (namedShape) {
        namedShape->owner = object.name;
        namedShape->shape = solid;
        addHistoryStatus(*namedShape, "part_design_loft:solidification");
        addHistoryStatus(*namedShape, "part_loft:thru_sections_history");
    }

    return SolidifiedLoft{solid, namedShape, static_cast<int>(shells.size())};
}

void executeLoftFeature(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        LoftAddSubMode mode)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLoft.cpp
    // ::Loft::execute(), reads "Profile", "Sections", "Ruled" and "Closed"; it builds one
    // shell per profile wire with makeElementLoft(... IsSolid::notSolid ...), sews front/back
    // faces, converts shells through makeElementSolid(), writes AddSubShape, then Body fuses or
    // cuts the cached tool according to AdditiveLoft/SubtractiveLoft addSubType.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Profile", "Sections", "Ruled", "Closed", "BaseFeature", "Refine", "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (app::propertyValue(object, "Profile") == nullptr) {
        addLoftDiagnostic(object, context, "missing_property", "Loft Profile must link to a Sketch object", "Profile");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto profileLink = app::readLink(object, "Profile");
    if (!profileLink) {
        addLoftDiagnostic(object, context, "missing_property", "Loft Profile must link to a Sketch object", "Profile");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto profileSections = getSectionShape(object, context, "Profile", *profileLink);
    if (!profileSections) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::vector<app::Link> sectionLinks = app::readLinks(object, "Sections");
    if (sectionLinks.empty()) {
        addLoftDiagnostic(object, context, "missing_property", "Loft: At least one section is needed", "Sections");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::vector<std::vector<SectionShape>> wireSections;
    wireSections.reserve(profileSections->size());
    for (const SectionShape& profileSection : *profileSections) {
        wireSections.push_back({profileSection});
    }

    for (const app::Link& sectionLink : sectionLinks) {
        const auto sectionShapes = getSectionShape(object, context, "Sections", sectionLink, wireSections.size());
        if (!sectionShapes) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        for (std::size_t index = 0; index < sectionShapes->size(); ++index) {
            wireSections.at(index).push_back(sectionShapes->at(index));
        }
    }

    bool closed = app::readBool(object, "Closed").value_or(false);
    if (sectionLinks.size() < 2U) {
        closed = false;
    }
    const bool ruled = app::readBool(object, "Ruled").value_or(false);
    const auto tool = buildSolidifiedLoftTool(object, context, *profileLink, wireSections, ruled, closed);
    if (!tool) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<part::NamedShape> namedShape = tool->namedShape;
    runtime::RefineShapeResult shapeResult{tool->shape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, tool->shape, namedShape);
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

    const bool additive = mode == LoftAddSubMode::Additive;
    if (additive) {
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
        context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt, namedShape, std::nullopt};
    }
    else {
        context.addSubShapes[object.name] = runtime::AddSubShape{std::nullopt, solid, std::nullopt, namedShape};
    }

    nlohmann::json sectionNames = nlohmann::json::array();
    for (const app::Link& sectionLink : sectionLinks) {
        sectionNames.push_back(sectionLink.object);
    }
    nlohmann::json result = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"feature", "partdesign_loft"},
        {"add_sub", additive ? "add" : "sub"},
        {"source_profile", profileLink->object},
        {"sections", sectionNames},
        {"ruled", ruled},
        {"closed", closed},
        {"shell_count", tool->shellCount},
        {"bbox", cad_core::part::bboxForShape(solid)},
        {"volume", cad_core::part::volumeForShape(solid)},
        {"topo_naming_history", "maker_history:partdesign_loft"},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace

void executeAdditiveLoft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeLoftFeature(object, context, LoftAddSubMode::Additive);
}

void executeSubtractiveLoft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeLoftFeature(object, context, LoftAddSubMode::Subtractive);
}

}  // namespace cad_core::part_design
