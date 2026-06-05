#include "cad_core/part_design/feature_chamfer.h"
#include "cad_core/part_design/feature_draft.h"
#include "cad_core/part_design/feature_fillet.h"
#include "cad_core/part_design/feature_thickness.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRep_Tool.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_Shape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAPI_IntSS.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Geom_Plane.hxx>
#include <Geom_Surface.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

struct DressUpBase {
    app::Link link;
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};

struct EdgeSelection {
    TopoDS_Edge edge;
    std::string sourceSubname;
    std::string edgeSubname;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
// ::DressUp::getContinuousEdges(), consumes Base subnames, expands Face/Wire selections to
// edges, de-duplicates accepted C0 edges, and keeps AddSub/DressUp history anchored to Base.
struct DressUpSelectionEvidence {
    bool useAllEdges = false;
    std::vector<std::string> requestedSubnames;
    std::vector<std::string> selectedEdgeSubnames;
    std::vector<std::string> selectedEdgeSources;
    int requestedEdgeCount = 0;
    int requestedFaceCount = 0;
    int requestedWireCount = 0;
};

struct DressUpEdgeSelection {
    std::vector<EdgeSelection> edges;
    DressUpSelectionEvidence evidence;
};

struct DraftFaceSelection {
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
    DressUpSelectionEvidence evidence;
};

struct ThicknessFaceSelection {
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
    DressUpSelectionEvidence evidence;
};

struct ThicknessSolidFaces {
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
};

struct ThicknessSolidBuild {
    int solidIndex = 0;
    TopoDS_Shape shape;
    part::NamedShape namedShape;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp
// ::Draft::execute(), if "NeutralPlane.getValue()" is empty, tries to guess a neutral plane from
// the first selected face before choosing the pull direction normal to that plane.
struct DraftNeutralPlane {
    gp_Pln plane;
    std::string source;
};

struct DressUpResult {
    std::string mode;
    std::string sourceBase;
    DressUpBase base;
    TopoDS_Shape shape;
    part::NamedShape namedShape;
    bool supportTransform = false;
    std::string supportTransformSource;
    bool refineApplied = false;
    DressUpSelectionEvidence selection;
    nlohmann::json parameters = nlohmann::json::object();
};

struct ShapeSlotBuild {
    bool ok = true;
    std::optional<TopoDS_Shape> shape;
    std::optional<part::NamedShape> namedShape;
};

enum class AddSubKind {
    Additive,
    Subtractive,
    Unknown,
};

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string readEnumProperty(const app::DocumentObject& object,
                             const std::string& property,
                             const std::vector<std::string>& values,
                             const std::string& fallback)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr) {
        return fallback;
    }
    if (payload->is_string()) {
        return payload->get<std::string>();
    }
    if (payload->is_number_integer()) {
        const int index = payload->get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < values.size()) {
            return values[static_cast<std::size_t>(index)];
        }
    }
    return fallback;
}

bool readBoolProperty(const app::DocumentObject& object, const std::string& property, bool fallback = false)
{
    return app::readBool(object, property).value_or(fallback);
}

double readNumberProperty(const app::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return app::readNumber(object, property).value_or(fallback);
}

bool isDressUpType(const std::string& typeId)
{
    return typeId == "PartDesign::Fillet" || typeId == "PartDesign::Chamfer"
        || typeId == "PartDesign::Draft" || typeId == "PartDesign::Thickness";
}

bool hasSolid(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    TopExp_Explorer explorer(shape, TopAbs_SOLID);
    return explorer.More();
}

int countSolids(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

std::vector<TopoDS_Shape> solidSubshapes(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Shape> solids;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        solids.push_back(explorer.Current());
    }
    return solids;
}

std::optional<part::NamedShape> namedShapeForObject(const std::string& objectName,
                                                    const TopoDS_Shape& shape,
                                                    const runtime::ComputeContext& context)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    if (!shape.IsNull()) {
        return part::indexedNamedShapeForObject(objectName, shape);
    }
    return std::nullopt;
}

part::NamedShapeSource namedShapeSource(const std::string& owner,
                                        const TopoDS_Shape& shape,
                                        const std::optional<part::NamedShape>& namedShape)
{
    return part::NamedShapeSource{namedShape ? namedShape->owner : owner, shape, namedShape ? &*namedShape : nullptr};
}

std::optional<std::string> linkedObjectName(const app::DocumentObject& object, const std::string& property)
{
    if (app::propertyValue(object, property) == nullptr) {
        return std::nullopt;
    }
    const auto link = app::readLink(object, property);
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<TopoDS_Shape> solidShapeForObject(const std::string& objectName,
                                                runtime::ComputeContext& context,
                                                const app::DocumentObject& object,
                                                const std::string& property)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + objectName + " did not produce a solid",
                               object.name,
                               property,
                               "runtime",
                               objectName);
        return std::nullopt;
    }
    return shapeIt->second.shape;
}

std::optional<TopoDS_Shape> baseTopoShapeForFeature(const std::string& featureName,
                                                    runtime::ComputeContext& context,
                                                    const app::DocumentObject& object,
                                                    bool diagnoseMissingTarget)
{
    const auto documentIt = context.documentObjects.find(featureName);
    if (documentIt == context.documentObjects.end()) {
        return std::nullopt;
    }

    const auto baseFeatureName = linkedObjectName(*documentIt->second, "BaseFeature");
    if (!baseFeatureName) {
        return std::nullopt;
    }

    if (!diagnoseMissingTarget) {
        const auto shapeIt = context.shapes.find(*baseFeatureName);
        if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            return std::nullopt;
        }
        return shapeIt->second.shape;
    }
    return solidShapeForObject(*baseFeatureName, context, object, "BaseFeature");
}

std::optional<std::string> baseLinkObjectName(const app::DocumentObject& object)
{
    const auto link = app::readLink(object, "Base");
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<std::string> resolveSupportTransformFeature(const DressUpBase& base,
                                                          runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getAddSubShape(), SupportTransform finds "the previous support feature
    // (which must be of type FeatureAddSub), and skipping any consecutive DressUp in-between".
    std::string currentName = base.link.object;
    std::set<std::string> visited;
    while (visited.insert(currentName).second) {
        const auto documentIt = context.documentObjects.find(currentName);
        if (documentIt == context.documentObjects.end() || !isDressUpType(documentIt->second->typeId)) {
            return currentName;
        }

        if (const auto baseFeatureName = linkedObjectName(*documentIt->second, "BaseFeature")) {
            currentName = *baseFeatureName;
            continue;
        }
        if (const auto sourceBaseIt = context.objects.find(currentName);
            sourceBaseIt != context.objects.end() && sourceBaseIt->second.contains("source_base")
            && sourceBaseIt->second.at("source_base").is_string()) {
            currentName = sourceBaseIt->second.at("source_base").get<std::string>();
            continue;
        }
        if (const auto linkedBase = baseLinkObjectName(*documentIt->second)) {
            currentName = *linkedBase;
            continue;
        }
        break;
    }
    return std::nullopt;
}

AddSubKind addSubKindForFeature(const std::string& featureName, runtime::ComputeContext& context)
{
    const auto addSubIt = context.addSubShapes.find(featureName);
    if (addSubIt != context.addSubShapes.end()) {
        if (addSubIt->second.addShape) {
            return AddSubKind::Additive;
        }
        if (addSubIt->second.subShape) {
            return AddSubKind::Subtractive;
        }
    }

    const auto documentIt = context.documentObjects.find(featureName);
    if (documentIt == context.documentObjects.end()) {
        return AddSubKind::Unknown;
    }
    if (documentIt->second->typeId == "PartDesign::Pad" || documentIt->second->typeId == "PartDesign::FeatureBase") {
        return AddSubKind::Additive;
    }
    if (documentIt->second->typeId == "PartDesign::Pocket" || documentIt->second->typeId == "PartDesign::Hole") {
        return AddSubKind::Subtractive;
    }
    return AddSubKind::Unknown;
}

ShapeSlotBuild cutSlot(const app::DocumentObject& object,
                       runtime::ComputeContext& context,
                       const std::string& owner,
                       const std::string& leftOwner,
                       const TopoDS_Shape& left,
                       const std::optional<part::NamedShape>& leftNamedShape,
                       const std::string& rightOwner,
                       const TopoDS_Shape& right,
                       const std::optional<part::NamedShape>& rightNamedShape,
                       const std::string& property)
{
    const auto build = part::makeElementBooleanFromSources(owner,
                                                           {namedShapeSource(leftOwner, left, leftNamedShape),
                                                            namedShapeSource(rightOwner, right, rightNamedShape)},
                                                           part::BooleanOperation::Cut);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Dress-up AddSubShape cache could not cut boolean sources: " + build.error,
                               object.name,
                               property);
        return ShapeSlotBuild{false, std::nullopt, std::nullopt};
    }
    if (!hasSolid(build.shape)) {
        return ShapeSlotBuild{true, std::nullopt, std::nullopt};
    }
    return ShapeSlotBuild{true,
                          build.shape,
                          build.namedShape ? std::optional<part::NamedShape>{*build.namedShape}
                                           : std::optional<part::NamedShape>{
                                               part::indexedNamedShapeForObject(owner, build.shape)}};
}

std::string stableSubnameDiagnosticCode(part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case part::ElementResolveStatus::Split:
            return "split_stable_subname";
        case part::ElementResolveStatus::Resolved:
        case part::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& target,
                                           const std::string& stableSubname,
                                           part::ElementResolveStatus status)
{
    if (status == part::ElementResolveStatus::Deleted) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == part::ElementResolveStatus::Split) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return "Base target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

std::optional<DressUpBase> resolveDressUpBase(const app::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.h::DressUp,
    // declares "App::PropertyLinkSub Base" for the feature and its selected subelements.
    if (app::propertyValue(object, "Base") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               object.typeId + " Base must link to a solid and selected subelements",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    const auto link = app::readLink(object, "Base");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               object.typeId + " Base must be an App::PropertyLinkSub",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Base target " + link->object + " did not produce a solid",
                               object.name,
                               "Base",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    std::optional<part::NamedShape> namedShape;
    const auto namedShapeIt = context.namedShapes.find(link->object);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = namedShapeIt->second;
    }

    return DressUpBase{*link, shapeIt->second.shape, namedShape};
}

std::optional<TopoDS_Shape> resolveDressUpSubshape(const DressUpBase& base,
                                                   const app::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   std::size_t subnameIndex)
{
    const std::string& requestedSubname = base.link.subnames[subnameIndex];
    const std::string stableSubname =
        subnameIndex < base.link.stableSubnames.size() ? base.link.stableSubnames[subnameIndex] : std::string{};

    std::string currentSubname = requestedSubname;
    if (base.namedShape) {
        const auto resolved = part::resolveElementReference(*base.namedShape, requestedSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != requestedSubname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(base.link.object, stableSubname, resolved.status),
                                   object.name,
                                   "Base",
                                   "runtime",
                                   base.link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid dress-up Base subshape " + currentSubname,
                               object.name,
                               "Base",
                               "runtime",
                               base.link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_FACE && parsed->kind != TopAbs_WIRE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               "Dress-up Base requires EdgeN or FaceN, not " + part::subshapeKindName(parsed->kind),
                               object.name,
                               "Base",
                               "runtime",
                               base.link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (base.namedShape) {
        subshape = part::subshapeByName(*base.namedShape, currentSubname);
    }
    else {
        subshape = part::subshapeByName(base.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Base target " + base.link.object + " has no subshape " + currentSubname,
                               object.name,
                               "Base",
                               "runtime",
                               base.link.object,
                               currentSubname);
        return std::nullopt;
    }

    return subshape;
}

bool isC0ContinuousEdge(const TopoDS_Shape& baseShape, const TopoDS_Edge& edge)
{
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    if (!edgeFaces.Contains(edge)) {
        return false;
    }
    const TopTools_ListOfShape& faces = edgeFaces.FindFromKey(edge);
    if (faces.Extent() != 2) {
        return false;
    }

    TopTools_ListIteratorOfListOfShape it(faces);
    const TopoDS_Face face1 = TopoDS::Face(it.Value());
    it.Next();
    const TopoDS_Face face2 = TopoDS::Face(it.Value());
    return BRep_Tool::Continuity(edge, face1, face2) == GeomAbs_C0;
}

std::optional<std::string> edgeSubnameInBase(const TopoDS_Shape& baseShape, const TopoDS_Edge& edge)
{
    int index = 1;
    for (TopExp_Explorer explorer(baseShape, TopAbs_EDGE); explorer.More(); explorer.Next(), ++index) {
        if (TopoDS::Edge(explorer.Current()).IsSame(edge)) {
            return "Edge" + std::to_string(index);
        }
    }
    return std::nullopt;
}

void addSelectedEdge(const TopoDS_Shape& baseShape,
                     const TopoDS_Edge& edge,
                     const std::string& sourceSubname,
                     DressUpEdgeSelection& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getContinuousEdges(), keeps selected edges only when they have exactly two
    // adjacent faces and "BRep_Tool::Continuity(...) == GeomAbs_C0".
    if (!isC0ContinuousEdge(baseShape, edge)) {
        return;
    }
    for (const auto& existing : result.edges) {
        if (existing.edge.IsSame(edge)) {
            return;
        }
    }

    const std::string edgeSubname = edgeSubnameInBase(baseShape, edge).value_or(sourceSubname);
    result.edges.push_back(EdgeSelection{edge, sourceSubname, edgeSubname});
    result.evidence.selectedEdgeSubnames.push_back(edgeSubname);
    result.evidence.selectedEdgeSources.push_back(sourceSubname);
}

DressUpEdgeSelection allEdges(const TopoDS_Shape& baseShape)
{
    DressUpEdgeSelection result;
    result.evidence.useAllEdges = true;
    int index = 1;
    for (TopExp_Explorer explorer(baseShape, TopAbs_EDGE); explorer.More(); explorer.Next(), ++index) {
        const std::string subname = "Edge" + std::to_string(index);
        result.edges.push_back(EdgeSelection{TopoDS::Edge(explorer.Current()), subname, subname});
        result.evidence.selectedEdgeSubnames.push_back(subname);
        result.evidence.selectedEdgeSources.push_back(subname);
    }
    return result;
}

std::optional<DressUpEdgeSelection> selectedDressUpEdges(const DressUpBase& base,
                                                         const app::DocumentObject& object,
                                                         runtime::ComputeContext& context)
{
    if (readBoolProperty(object, "UseAllEdges")) {
        return allEdges(base.shape);
    }
    if (base.link.subnames.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Dress-up Base must select at least one EdgeN or FaceN unless UseAllEdges=true",
                               object.name,
                               "Base",
                               "runtime",
                               base.link.object);
        return std::nullopt;
    }

    DressUpEdgeSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        const std::string& subname = base.link.subnames[index];
        if (subshape->ShapeType() == TopAbs_EDGE) {
            ++selection.evidence.requestedEdgeCount;
            addSelectedEdge(base.shape, TopoDS::Edge(*subshape), subname, selection);
            continue;
        }
        if (subshape->ShapeType() == TopAbs_FACE || subshape->ShapeType() == TopAbs_WIRE) {
            if (subshape->ShapeType() == TopAbs_FACE) {
                ++selection.evidence.requestedFaceCount;
            }
            else {
                ++selection.evidence.requestedWireCount;
            }
            for (TopExp_Explorer explorer(*subshape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
                addSelectedEdge(base.shape, TopoDS::Edge(explorer.Current()), subname, selection);
            }
        }
    }

    if (selection.edges.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Dress-up operation is not possible on selected shapes",
                               object.name,
                               "Base");
        return std::nullopt;
    }
    return selection;
}

std::optional<DraftFaceSelection> selectedDraftFaces(const DressUpBase& base,
                                                     const app::DocumentObject& object,
                                                     runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
    // calls Base.getSubValuesStartsWith("Face") and copies the previous feature when no FaceN
    // subelement is selected.
    DraftFaceSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const std::string& subname = base.link.subnames[index];
        const auto parsed = part::parseSubshapeName(subname);
        if (!parsed || parsed->kind != TopAbs_FACE) {
            continue;
        }

        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        if (subshape->ShapeType() != TopAbs_FACE) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_subshape_kind",
                                   "Draft Base requires FaceN, not " + part::subshapeKindName(subshape->ShapeType()),
                                   object.name,
                                   "Base",
                                   "runtime",
                                   base.link.object,
                                   subname);
            return std::nullopt;
        }

        ++selection.evidence.requestedFaceCount;
        selection.faces.push_back(TopoDS::Face(*subshape));
        selection.selectedFaceSubnames.push_back(subname);
    }
    return selection;
}

std::optional<ThicknessFaceSelection> selectedThicknessFaces(const DressUpBase& base,
                                                            const app::DocumentObject& object,
                                                            runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), reads "Base.getSubValues(true)" and returns the unchanged TopShape
    // when no sub elements are selected; selected entries are then consumed as close faces.
    ThicknessFaceSelection selection;
    selection.evidence.requestedSubnames = base.link.subnames;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const std::string& subname = base.link.subnames[index];
        const auto parsed = part::parseSubshapeName(subname);
        if (!parsed) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   "Invalid Thickness Base subshape " + subname,
                                   object.name,
                                   "Base",
                                   "runtime",
                                   base.link.object,
                                   subname);
            return std::nullopt;
        }
        if (parsed->kind != TopAbs_FACE) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_subshape_kind",
                                   "Thickness Base currently requires FaceN, not "
                                       + part::subshapeKindName(parsed->kind),
                                   object.name,
                                   "Base",
                                   "runtime",
                                   base.link.object,
                                   subname);
            return std::nullopt;
        }

        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        if (subshape->ShapeType() != TopAbs_FACE) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_subshape_kind",
                                   "Thickness Base requires FaceN, not "
                                       + part::subshapeKindName(subshape->ShapeType()),
                                   object.name,
                                   "Base",
                                   "runtime",
                                   base.link.object,
                                   subname);
            return std::nullopt;
        }

        ++selection.evidence.requestedFaceCount;
        selection.faces.push_back(TopoDS::Face(*subshape));
        selection.selectedFaceSubnames.push_back(subname);
    }
    return selection;
}

std::optional<short> thicknessModeIndex(const app::DocumentObject& object,
                                        runtime::ComputeContext& context)
{
    const std::string mode = readEnumProperty(object, "Mode", {"Skin", "Pipe", "RectoVerso"}, "Skin");
    if (mode == "Skin") {
        return 0;
    }
    if (mode == "Pipe") {
        return 1;
    }
    if (mode == "RectoVerso") {
        return 2;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Thickness Mode must be Skin, Pipe or RectoVerso",
                           object.name,
                           "Mode");
    return std::nullopt;
}

std::optional<short> thicknessJoinIndex(const app::DocumentObject& object,
                                        runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), PartDesign exposes only "Arc" and "Intersection"; enum value 1 is
    // remapped to OCCT/Part JoinType::intersection because "we do not offer tangent join type".
    const std::string join = readEnumProperty(object, "Join", {"Arc", "Intersection"}, "Arc");
    if (join == "Arc") {
        return 0;
    }
    if (join == "Intersection") {
        return 2;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Thickness Join must be Arc or Intersection",
                           object.name,
                           "Join");
    return std::nullopt;
}

std::string thicknessModeName(short mode)
{
    switch (mode) {
        case 0:
            return "Skin";
        case 1:
            return "Pipe";
        case 2:
            return "RectoVerso";
        default:
            return "Skin";
    }
}

std::string thicknessJoinName(short join)
{
    return join == 2 ? "Intersection" : "Arc";
}

std::optional<int> ancestorSolidIndexForFace(const TopoDS_Shape& baseShape,
                                             const std::vector<TopoDS_Shape>& solids,
                                             const TopoDS_Face& face)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), after resolving each selected face, calls
    // "TopShape.findAncestor(face, TopAbs_SOLID)" and groups close faces by that solid index.
    TopTools_IndexedDataMapOfShapeListOfShape faceSolids;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_FACE, TopAbs_SOLID, faceSolids);
    if (!faceSolids.Contains(face)) {
        return std::nullopt;
    }

    const TopTools_ListOfShape& ancestors = faceSolids.FindFromKey(face);
    for (TopTools_ListIteratorOfListOfShape it(ancestors); it.More(); it.Next()) {
        for (std::size_t index = 0; index < solids.size(); ++index) {
            if (solids[index].IsSame(it.Value())) {
                return static_cast<int>(index + 1U);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::map<int, ThicknessSolidFaces>> thicknessFacesBySolid(
    const DressUpBase& base,
    const ThicknessFaceSelection& selection,
    const std::vector<TopoDS_Shape>& solids,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    std::map<int, ThicknessSolidFaces> result;
    for (std::size_t index = 0; index < selection.faces.size(); ++index) {
        const auto solidIndex = ancestorSolidIndexForFace(base.shape, solids, selection.faces[index]);
        if (!solidIndex) {
            // FreeCAD logs and ignores non-solid faces. If all selected faces are ignored the
            // subsequent maker path fails; cad-core exposes that as a structured diagnostic below.
            continue;
        }
        result[*solidIndex].faces.push_back(selection.faces[index]);
        result[*solidIndex].selectedFaceSubnames.push_back(selection.selectedFaceSubnames[index]);
    }
    if (result.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Thickness selected faces do not belong to any solid",
                               object.name,
                               "Base");
        return std::nullopt;
    }
    return result;
}

std::optional<TopoDS_Shape> resolveReferenceSubshape(const app::Link& link,
                                                     const app::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const std::string& property,
                                                     TopAbs_ShapeEnum expectedKind)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference exactly one " + part::subshapeKindName(expectedKind) + " subshape",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " did not produce a shape",
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               link.subnames.front());
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front() : std::string{};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved = part::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid " + property + " subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != expectedKind) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " requires " + part::subshapeKindName(expectedKind) + " subshape, not "
                                   + part::subshapeKindName(parsed->kind),
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + link.object + " has no subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    return subshape;
}

std::optional<TopoDS_Edge> firstEdge(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        return TopoDS::Edge(explorer.Current());
    }
    return std::nullopt;
}

std::optional<TopoDS_Face> firstFace(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return TopoDS::Face(explorer.Current());
    }
    return std::nullopt;
}

std::optional<gp_Dir> lineDirectionFromEdge(const TopoDS_Edge& edge,
                                            const app::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const std::string& property)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " reference edge must be linear",
                               object.name,
                               property);
        return std::nullopt;
    }
    return curve.Line().Direction();
}

std::optional<gp_Dir> resolveDraftPullDirection(const app::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const gp_Pln& neutralPlane)
{
    const auto* property = app::propertyValue(object, "PullDirection");
    if (property == nullptr || property->raw.is_null()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
        // when no PullDirection reference is set, "Choose pull direction normal to neutral plane".
        return neutralPlane.Axis().Direction();
    }

    const auto link = app::readLink(object, "PullDirection");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "PullDirection must link to a datum line or linear EdgeN",
                               object.name,
                               "PullDirection");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "PullDirection target " + link->object + " did not produce a shape",
                               object.name,
                               "PullDirection",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        const auto edge = firstEdge(shapeIt->second.shape);
        if (!edge) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "PullDirection DatumLine has no edge shape",
                                   object.name,
                                   "PullDirection",
                                   "runtime",
                                   link->object);
            return std::nullopt;
        }
        return lineDirectionFromEdge(*edge, object, context, "PullDirection");
    }

    TopoDS_Edge edge;
    if (link->subnames.empty() && shapeIt->second.kind == runtime::ShapeValue::Kind::PartPrimitive) {
        const auto directEdge = firstEdge(shapeIt->second.shape);
        if (!directEdge) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_type",
                                   "PullDirection primitive must be a line or select a linear EdgeN",
                                   object.name,
                                   "PullDirection",
                                   "runtime",
                                   link->object);
            return std::nullopt;
        }
        edge = *directEdge;
    }
    else {
        const auto subshape = resolveReferenceSubshape(*link, object, context, "PullDirection", TopAbs_EDGE);
        if (!subshape) {
            return std::nullopt;
        }
        edge = TopoDS::Edge(*subshape);
    }
    return lineDirectionFromEdge(edge, object, context, "PullDirection");
}

std::optional<gp_Pln> planeFromFace(const TopoDS_Face& face,
                                    const app::DocumentObject& object,
                                    runtime::ComputeContext& context,
                                    const std::string& property)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " reference face must be planar",
                               object.name,
                               property);
        return std::nullopt;
    }
    return surface.Plane();
}

std::optional<DraftNeutralPlane> guessDraftNeutralPlaneFromFace(const TopoDS_Face& face,
                                                                const app::DocumentObject& object,
                                                                runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
    // with no NeutralPlane, maps edges from the first selected face; circular edges use
    // "gp_Pln(p1, c.Circle().Axis().Direction())", while linear edges intersect an auxiliary
    // plane with the face surface and use the resulting Geom_Line direction.
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(face, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        try {
            BRepAdaptor_Curve curve(TopoDS::Edge(edges(index)));
            const gp_Pnt p1 = curve.Value(curve.FirstParameter());
            const gp_Pnt p2 = curve.Value(curve.LastParameter());

            if (curve.IsClosed()) {
                if (curve.GetType() == GeomAbs_Circle) {
                    return DraftNeutralPlane{gp_Pln(p1, curve.Circle().Axis().Direction()),
                                             "guessed_from_circular_edge"};
                }
                continue;
            }

            if (p1.Distance(p2) <= Precision::Confusion()) {
                continue;
            }
            const gp_Pnt midpoint = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2.0);
            Handle(Geom_Plane) auxiliaryPlane =
                new Geom_Plane(midpoint, gp_Dir(p2.X() - p1.X(), p2.Y() - p1.Y(), p2.Z() - p1.Z()));
            BRepAdaptor_Surface surface(face, Standard_False);
            Handle(Geom_Surface) rawSurface = surface.Surface().Surface();
            GeomAPI_IntSS intersector(auxiliaryPlane, rawSurface, Precision::Confusion());
            if (!intersector.IsDone() || intersector.NbLines() < 1) {
                continue;
            }
            const Handle(Geom_Curve)& intersection = intersector.Line(1);
            if (!intersection->IsKind(STANDARD_TYPE(Geom_Line))) {
                continue;
            }
            Handle(Geom_Line) line = Handle(Geom_Line)::DownCast(intersection);
            return DraftNeutralPlane{gp_Pln(midpoint, line->Lin().Direction()), "guessed_from_linear_edge"};
        }
        catch (Standard_Failure&) {
            continue;
        }
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "execution_failed",
                           "No neutral plane specified and none can be guessed",
                           object.name,
                           "NeutralPlane");
    return std::nullopt;
}

std::optional<DraftNeutralPlane> resolveDraftNeutralPlane(const app::DocumentObject& object,
                                                          runtime::ComputeContext& context,
                                                          const TopoDS_Face& firstSelectedFace)
{
    const auto* property = app::propertyValue(object, "NeutralPlane");
    if (property == nullptr || property->raw.is_null()) {
        return guessDraftNeutralPlaneFromFace(firstSelectedFace, object, context);
    }

    const auto link = app::readLink(object, "NeutralPlane");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "NeutralPlane must link to a datum plane or planar FaceN",
                               object.name,
                               "NeutralPlane");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "NeutralPlane target " + link->object + " did not produce a shape",
                               object.name,
                               "NeutralPlane",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    TopoDS_Face face;
    if (link->subnames.empty()
        && (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPlane
            || shapeIt->second.kind == runtime::ShapeValue::Kind::PartPrimitive)) {
        const auto directFace = firstFace(shapeIt->second.shape);
        if (!directFace) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_type",
                                   "NeutralPlane target must be a datum plane or select a planar FaceN",
                                   object.name,
                                   "NeutralPlane",
                                   "runtime",
                                   link->object);
            return std::nullopt;
        }
        face = *directFace;
    }
    else {
        const auto subshape = resolveReferenceSubshape(*link, object, context, "NeutralPlane", TopAbs_FACE);
        if (!subshape) {
            return std::nullopt;
        }
        face = TopoDS::Face(*subshape);
    }

    const auto plane = planeFromFace(face, object, context, "NeutralPlane");
    if (!plane) {
        return std::nullopt;
    }
    return DraftNeutralPlane{*plane, "explicit_reference"};
}

nlohmann::json directionJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

std::optional<TopoDS_Face> ancestorFaceForChamfer(const TopoDS_Shape& baseShape,
                                                  const TopoDS_Edge& edge,
                                                  bool flipDirection,
                                                  const app::DocumentObject& object,
                                                  runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementChamfer(), uses findAncestorShape(edge, TopAbs_FACE), or the
    // last ancestor face when "FlipDirection" is enabled.
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    if (!edgeFaces.Contains(edge)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Chamfer edge has no adjacent face",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    const TopTools_ListOfShape& faces = edgeFaces.FindFromKey(edge);
    if (faces.IsEmpty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Chamfer edge has no adjacent face",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    return TopoDS::Face(flipDirection ? faces.Last() : faces.First());
}

std::optional<DressUpResult> buildFillet(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto edges = selectedDressUpEdges(*base, object, context);
    if (!edges) {
        return std::nullopt;
    }

    const double radius = readNumberProperty(object, "Radius", 1.0);
    if (radius <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Fillet radius must be greater than zero",
                               object.name,
                               "Radius");
        return std::nullopt;
    }

    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementFillet(), creates BRepFilletAPI_MakeFillet and calls
        // "mkFillet.Add(radius1, radius2, TopoDS::Edge(edge))" for every selected edge.
        BRepFilletAPI_MakeFillet maker(base->shape);
        for (const auto& edge : edges->edges) {
            maker.Add(radius, radius, edge.edge);
        }
        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Fillet operation failed",
                                   object.name,
                                   "Base");
            return std::nullopt;
        }

        TopoDS_Shape result = maker.Shape();
        part::NamedShape namedShape = part::namedShapeForMakerHistory(object.name,
                                                                      result,
                                                                      {part::NamedShapeSource{base->link.object,
                                                                                              base->shape,
                                                                                              base->namedShape
                                                                                                  ? &*base->namedShape
                                                                                                  : nullptr}},
                                                                      maker);
        DressUpResult dressUpResult{"fillet",
                                    base->link.object,
                                    *base,
                                    result,
                                    namedShape,
                                    readBoolProperty(object, "SupportTransform")};
        dressUpResult.selection = edges->evidence;
        return dressUpResult;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               failure.GetMessageString(),
                               object.name,
                               "Base");
        return std::nullopt;
    }
}

std::optional<DressUpResult> buildChamfer(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto edges = selectedDressUpEdges(*base, object, context);
    if (!edges) {
        return std::nullopt;
    }

    const std::string chamferType = readEnumProperty(object,
                                                     "ChamferType",
                                                     {"Equal distance", "Two distances", "Distance and Angle"},
                                                     "Equal distance");
    const double size = readNumberProperty(object, "Size", 1.0);
    double size2 = readNumberProperty(object, "Size2", 1.0);
    const double angle = readNumberProperty(object, "Angle", 45.0);
    if (size <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Chamfer Size must be greater than zero",
                               object.name,
                               "Size");
        return std::nullopt;
    }
    if (chamferType == "Two distances" && size2 <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Chamfer Size2 must be greater than zero",
                               object.name,
                               "Size2");
        return std::nullopt;
    }
    if (chamferType == "Distance and Angle" && (angle <= Precision::Confusion() || angle >= 180.0)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_angle",
                               "Chamfer Angle must be greater than 0 and less than 180",
                               object.name,
                               "Angle");
        return std::nullopt;
    }

    const bool flipDirection = readBoolProperty(object, "FlipDirection");
    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementChamfer(), creates BRepFilletAPI_MakeChamfer and adds
        // selected edges with Equal distance, Two distances, or Distance and Angle parameters.
        BRepFilletAPI_MakeChamfer maker(base->shape);
        for (const auto& edge : edges->edges) {
            if (BRep_Tool::Degenerated(edge.edge)) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Chamfer edge is degenerated",
                                       object.name,
                                       "Base");
                return std::nullopt;
            }
            const auto face = ancestorFaceForChamfer(base->shape, edge.edge, flipDirection, object, context);
            if (!face) {
                return std::nullopt;
            }
            if (chamferType == "Equal distance") {
                maker.Add(size, size, edge.edge, *face);
            }
            else if (chamferType == "Two distances") {
                maker.Add(size, size2, edge.edge, *face);
            }
            else if (chamferType == "Distance and Angle") {
                size2 = angle;
                maker.AddDA(size, angle * M_PI / 180.0, edge.edge, *face);
            }
            else {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_property",
                                       "Unsupported ChamferType " + chamferType,
                                       object.name,
                                       "ChamferType");
                return std::nullopt;
            }
        }
        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Chamfer operation failed",
                                   object.name,
                                   "Base");
            return std::nullopt;
        }

        TopoDS_Shape result = maker.Shape();
        part::NamedShape namedShape = part::namedShapeForMakerHistory(object.name,
                                                                      result,
                                                                      {part::NamedShapeSource{base->link.object,
                                                                                              base->shape,
                                                                                              base->namedShape
                                                                                                  ? &*base->namedShape
                                                                                                  : nullptr}},
                                                                      maker);
        DressUpResult dressUpResult{"chamfer",
                                    base->link.object,
                                    *base,
                                    result,
                                    namedShape,
                                    readBoolProperty(object, "SupportTransform")};
        dressUpResult.selection = edges->evidence;
        dressUpResult.parameters = {
            {"chamfer_type", chamferType},
            {"size", size},
            {"flip_direction", flipDirection},
        };
        if (chamferType == "Two distances") {
            dressUpResult.parameters["size2"] = size2;
        }
        if (chamferType == "Distance and Angle") {
            dressUpResult.parameters["angle"] = angle;
        }
        return dressUpResult;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               failure.GetMessageString(),
                               object.name,
                               "Base");
        return std::nullopt;
    }
}

std::optional<DressUpResult> buildDraft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto selection = selectedDraftFaces(*base, object, context);
    if (!selection) {
        return std::nullopt;
    }

    const double angleDegrees = readNumberProperty(object, "Angle", 1.5);
    bool reversed = readBoolProperty(object, "Reversed");

    part::NamedShapeSource baseSource{base->link.object,
                                      base->shape,
                                      base->namedShape ? &*base->namedShape : nullptr};
    if (selection->faces.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
        // when Base.getSubValuesStartsWith("Face") is empty, calls positionByBaseFeature()
        // and stores the unchanged TopShape instead of running the draft maker.
        part::NamedShape namedShape = part::namedShapeForPreservedSources(object.name, base->shape, {baseSource});
        DressUpResult result{"draft",
                             base->link.object,
                             *base,
                             base->shape,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
        result.selection = selection->evidence;
        result.parameters = {
            {"angle", angleDegrees},
            {"reversed", reversed},
            {"selected_faces", nlohmann::json::array()},
            {"mode", "copy_no_face_selection"},
        };
        return result;
    }

    const auto neutralPlane = resolveDraftNeutralPlane(object, context, selection->faces.front());
    if (!neutralPlane) {
        return std::nullopt;
    }
    const auto pullDirection = resolveDraftPullDirection(object, context, neutralPlane->plane);
    if (!pullDirection) {
        return std::nullopt;
    }

    double angle = angleDegrees * std::acos(-1.0) / 180.0;
    if (reversed) {
        angle *= -1.0;
    }

    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementDraft(), initializes BRepOffsetAPI_DraftAngle and calls
        // "mkDraft.Add(TopoDS::Face(...), pullDirection, angle, neutralPlane)" for every FaceN.
        std::vector<TopoDS_Face> faces = selection->faces;
        BRepOffsetAPI_DraftAngle maker;
        bool done = true;
        const bool retry = reversed;
        do {
            if (faces.empty()) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Draft operation has no usable faces",
                                       object.name,
                                       "Base");
                return std::nullopt;
            }

            maker.Init(base->shape);
            done = true;
            for (auto it = faces.begin(); it != faces.end(); ++it) {
                maker.Add(*it, *pullDirection, angle, neutralPlane->plane);
                if (!maker.AddDone()) {
                    if (!retry) {
                        runtime::addDiagnostic(context.diagnostics,
                                               "error",
                                               "execution_failed",
                                               "Draft operation could not add selected face",
                                               object.name,
                                               "Base");
                        return std::nullopt;
                    }
                    done = false;
                    faces.erase(it);
                    break;
                }
            }
        } while (retry && !done);

        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Draft operation failed",
                                   object.name,
                                   "Base");
            return std::nullopt;
        }

        TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "Draft operation produced a null shape",
                                   object.name,
                                   "Base");
            return std::nullopt;
        }

        part::NamedShape namedShape = part::namedShapeForMakerHistory(object.name,
                                                                      resultShape,
                                                                      {baseSource},
                                                                      maker);
        DressUpResult result{"draft",
                             base->link.object,
                             *base,
                             resultShape,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
        result.selection = selection->evidence;
        result.parameters = {
            {"angle", angleDegrees},
            {"reversed", reversed},
            {"selected_faces", selection->selectedFaceSubnames},
            {"pull_direction", directionJson(*pullDirection)},
            {"neutral_plane_normal", directionJson(neutralPlane->plane.Axis().Direction())},
            {"neutral_plane_source", neutralPlane->source},
            {"mode", "draft_angle"},
        };
        return result;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               failure.GetMessageString(),
                               object.name,
                               "Base");
        return std::nullopt;
    }
}

std::optional<DressUpResult> buildThickness(const app::DocumentObject& object,
                                            runtime::ComputeContext& context)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto selection = selectedThicknessFaces(*base, object, context);
    if (!selection) {
        return std::nullopt;
    }

    const double value = readNumberProperty(object, "Value", 1.0);
    const bool reversed = readBoolProperty(object, "Reversed", true);
    const bool intersection = readBoolProperty(object, "Intersection");
    const double thickness = (reversed ? -1.0 : 1.0) * value;
    const auto modeIndex = thicknessModeIndex(object, context);
    const auto joinIndex = thicknessJoinIndex(object, context);
    if (!modeIndex || !joinIndex) {
        return std::nullopt;
    }

    part::NamedShapeSource baseSource{base->link.object,
                                      base->shape,
                                      base->namedShape ? &*base->namedShape : nullptr};
    if (selection->faces.empty() || std::abs(thickness) <= 2.0 * Precision::Confusion()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
        // ::Thickness::execute(), if Base has no selected subelements, calls positionByBaseFeature()
        // and stores the unchanged TopShape; if fabs(thickness) <= 2*tol, no thick-solid maker runs.
        part::NamedShape namedShape = part::namedShapeForPreservedSources(object.name, base->shape, {baseSource});
        DressUpResult result{"thickness",
                             base->link.object,
                             *base,
                             base->shape,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
        result.selection = selection->evidence;
        result.parameters = {
            {"value", value},
            {"thickness", thickness},
            {"mode", thicknessModeName(*modeIndex)},
            {"join", thicknessJoinName(*joinIndex)},
            {"reversed", reversed},
            {"intersection", intersection},
            {"selected_faces", selection->selectedFaceSubnames},
            {"build_mode", selection->faces.empty() ? "copy_no_face_selection" : "copy_zero_thickness"},
        };
        return result;
    }

    const std::vector<TopoDS_Shape> solids = solidSubshapes(base->shape);
    if (solids.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Thickness Base has no solid",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    const auto closeFaces = thicknessFacesBySolid(*base, *selection, solids, object, context);
    if (!closeFaces) {
        return std::nullopt;
    }

    try {
        std::vector<ThicknessSolidBuild> solidBuilds;
        solidBuilds.reserve(solids.size());
        std::vector<int> processedSolids;
        nlohmann::json selectedFacesBySolid = nlohmann::json::object();
        for (std::size_t solidOffset = 0; solidOffset < solids.size(); ++solidOffset) {
            const int solidIndex = static_cast<int>(solidOffset + 1U);
            const auto facesIt = closeFaces->find(solidIndex);
            if (facesIt == closeFaces->end() || facesIt->second.faces.empty()) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
                // ::Thickness::execute(), passes an empty face list for solids without selected
                // close faces; TopoShape::makeElementThickSolid() then throws "Null input shape".
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Thickness multi-solid Base requires selected close faces for every solid",
                                       object.name,
                                       "Base");
                return std::nullopt;
            }

            TopTools_ListOfShape removeFaces;
            for (const auto& face : facesIt->second.faces) {
                removeFaces.Append(face);
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::TopoShape::makeElementThickSolid(), calls "mkThick.MakeThickSolidByJoin(shape,
            // remFace, offset, tol, BRepOffset_Mode(offsetMode), intersection, selfInter,
            // GeomAbs_JoinType(join))" and then makeElementShape(mkThick, shape, op).
            BRepOffsetAPI_MakeThickSolid maker;
            maker.MakeThickSolidByJoin(solids[solidOffset],
                                       removeFaces,
                                       thickness,
                                       Precision::Confusion(),
                                       static_cast<BRepOffset_Mode>(*modeIndex),
                                       intersection ? Standard_True : Standard_False,
                                       Standard_False,
                                       static_cast<GeomAbs_JoinType>(*joinIndex));
            TopoDS_Shape solidResultShape = maker.Shape();
            if (solidResultShape.IsNull()) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Thickness operation produced a null shape",
                                       object.name,
                                       "Base");
                return std::nullopt;
            }

            part::NamedShape solidNamedShape =
                part::namedShapeForMakerHistory(object.name + ".Solid" + std::to_string(solidIndex),
                                                solidResultShape,
                                                {baseSource},
                                                maker);
            solidBuilds.push_back(ThicknessSolidBuild{solidIndex, solidResultShape, solidNamedShape});
            processedSolids.push_back(solidIndex);
            selectedFacesBySolid[std::to_string(solidIndex)] = facesIt->second.selectedFaceSubnames;
        }

        TopoDS_Shape resultShape;
        part::NamedShape namedShape;
        if (solidBuilds.size() == 1U) {
            resultShape = solidBuilds.front().shape;
            namedShape = solidBuilds.front().namedShape;
            namedShape.owner = object.name;
            namedShape.shape = resultShape;
        }
        else {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
            // ::Thickness::execute(), after per-solid makeElementThickSolid(), calls
            // "result.makeElementFuse(shapes)" when more than one result shape was produced.
            std::vector<part::NamedShapeSource> fuseSources;
            fuseSources.reserve(solidBuilds.size());
            for (const auto& solidBuild : solidBuilds) {
                fuseSources.push_back(part::NamedShapeSource{
                    object.name + ".Solid" + std::to_string(solidBuild.solidIndex),
                    solidBuild.shape,
                    &solidBuild.namedShape});
            }
            const auto fuseBuild =
                part::makeElementBooleanFromSources(object.name, fuseSources, part::BooleanOperation::Fuse);
            if (!fuseBuild.error.empty() || fuseBuild.shape.IsNull() || !fuseBuild.namedShape) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       fuseBuild.error.empty() ? "Thickness multi-solid fuse failed" : fuseBuild.error,
                                       object.name,
                                       "Base");
                return std::nullopt;
            }
            resultShape = fuseBuild.shape;
            namedShape = *fuseBuild.namedShape;
        }

        DressUpResult result{"thickness",
                             base->link.object,
                             *base,
                             resultShape,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
        result.selection = selection->evidence;
        result.parameters = {
            {"value", value},
            {"thickness", thickness},
            {"mode", thicknessModeName(*modeIndex)},
            {"join", thicknessJoinName(*joinIndex)},
            {"reversed", reversed},
            {"intersection", intersection},
            {"selected_faces", selection->selectedFaceSubnames},
            {"selected_faces_by_solid", selectedFacesBySolid},
            {"solid_count", static_cast<int>(solids.size())},
            {"processed_solids", processedSolids},
            {"build_mode", solidBuilds.size() > 1U ? "thick_solid_multi_fuse" : "thick_solid"},
        };
        return result;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               failure.GetMessageString(),
                               object.name,
                               "Base");
        return std::nullopt;
    }
}

bool cacheDressUpAddSubShape(const app::DocumentObject& object,
                             runtime::ComputeContext& context,
                             DressUpResult& result)
{
    runtime::AddSubShape cache;
    const auto resultNamedShape = std::optional<part::NamedShape>{result.namedShape};

    if (result.supportTransform) {
        const auto supportFeature = resolveSupportTransformFeature(result.base, context);
        if (!supportFeature) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "Cannot find additive or subtractive support for " + object.name,
                                   object.name,
                                   "SupportTransform");
            return false;
        }
        result.supportTransformSource = *supportFeature;

        const auto supportKind = addSubKindForFeature(*supportFeature, context);
        const auto priorBaseShape = baseTopoShapeForFeature(*supportFeature, context, object, true);
        if (supportKind == AddSubKind::Additive) {
            if (priorBaseShape && hasSolid(*priorBaseShape)) {
                const auto priorBaseNamedShape =
                    namedShapeForObject(*supportFeature + ".Base", *priorBaseShape, context);
                const auto add = cutSlot(object,
                                         context,
                                         object.name,
                                         object.name,
                                         result.shape,
                                         resultNamedShape,
                                         *supportFeature + ".Base",
                                         *priorBaseShape,
                                         priorBaseNamedShape,
                                         "SupportTransform");
                if (!add.ok) {
                    return false;
                }
                cache.addShape = add.shape;
                cache.addNamedShape = add.namedShape;
            }
            else {
                cache.addShape = result.shape;
                cache.addNamedShape = result.namedShape;
            }
        }
        else if (supportKind == AddSubKind::Subtractive) {
            if (priorBaseShape && hasSolid(*priorBaseShape)) {
                const auto priorBaseNamedShape =
                    namedShapeForObject(*supportFeature + ".Base", *priorBaseShape, context);
                const auto sub = cutSlot(object,
                                         context,
                                         object.name,
                                         *supportFeature + ".Base",
                                         *priorBaseShape,
                                         priorBaseNamedShape,
                                         object.name,
                                         result.shape,
                                         resultNamedShape,
                                         "SupportTransform");
                if (!sub.ok) {
                    return false;
                }
                cache.subShape = sub.shape;
                cache.subNamedShape = sub.namedShape;
            }
            else {
                cache.subShape = result.shape;
                cache.subNamedShape = result.namedShape;
            }
        }
        else {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_type",
                                   "SupportTransform support " + *supportFeature
                                       + " is not an additive or subtractive FeatureAddSub",
                                   object.name,
                                   "SupportTransform",
                                   "runtime",
                                   *supportFeature);
            return false;
        }

        if (cache.addShape || cache.subShape) {
            context.addSubShapes[object.name] = cache;
        }
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getAddSubShape(), without SupportTransform builds two slots:
    // "shape.makeElementCut(baseShape)" and "baseShape.makeElementCut(shape)" so transformed
    // features can fuse added dressing material and cut removed dressing material independently.
    const auto baseFeatureShape = baseTopoShapeForFeature(object.name, context, object, false);
    const TopoDS_Shape baseShape =
        baseFeatureShape && hasSolid(*baseFeatureShape) ? *baseFeatureShape : result.base.shape;
    const auto baseNamedShape = namedShapeForObject(result.base.link.object, baseShape, context);

    const auto add = cutSlot(object,
                             context,
                             object.name,
                             object.name,
                             result.shape,
                             resultNamedShape,
                             result.base.link.object,
                             baseShape,
                             baseNamedShape,
                             "Base");
    if (!add.ok) {
        return false;
    }
    const auto sub = cutSlot(object,
                             context,
                             object.name,
                             result.base.link.object,
                             baseShape,
                             baseNamedShape,
                             object.name,
                             result.shape,
                             resultNamedShape,
                             "Base");
    if (!sub.ok) {
        return false;
    }
    cache.addShape = add.shape;
    cache.addNamedShape = add.namedShape;
    cache.subShape = sub.shape;
    cache.subNamedShape = sub.namedShape;
    if (cache.addShape || cache.subShape) {
        context.addSubShapes[object.name] = cache;
    }
    return true;
}

bool applyDressUpRefine(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        DressUpResult& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp
    // ::Fillet::execute() and FeatureChamfer.cpp::Chamfer::execute(), both store rawShape and
    // then call "shape = refineShapeIfActive(shape)" before publishing Shape.
    const auto refined = runtime::applyRefineProperty(object, context, result.shape, result.namedShape);
    if (!refined) {
        return false;
    }
    result.shape = refined->shape;
    if (refined->namedShape) {
        result.namedShape = *refined->namedShape;
    }
    result.refineApplied = refined->applied;
    return true;
}

nlohmann::json selectionEvidenceJson(const DressUpSelectionEvidence& evidence)
{
    return {
        {"use_all_edges", evidence.useAllEdges},
        {"requested_subnames", evidence.requestedSubnames},
        {"requested_edge_count", evidence.requestedEdgeCount},
        {"requested_face_count", evidence.requestedFaceCount},
        {"requested_wire_count", evidence.requestedWireCount},
        {"selected_edge_count", static_cast<int>(evidence.selectedEdgeSubnames.size())},
        {"selected_edge_subnames", evidence.selectedEdgeSubnames},
        {"selected_edge_sources", evidence.selectedEdgeSources},
    };
}

void publishDressUpResult(const app::DocumentObject& object,
                          runtime::ComputeContext& context,
                          const DressUpResult& result)
{
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, result.shape};
    context.namedShapes[object.name] = result.namedShape;
    context.mesh[object.name] = cad_core::part::meshForShape(result.shape);
    context.subshapes[object.name] = part::subshapeMapForShape(result.shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"body_mode", "replace"},
        {"dress_up", result.mode},
        {"support_transform", result.supportTransform},
        {"support_transform_source", result.supportTransformSource},
        {"add_sub_cache",
         result.supportTransform ? "support_transform"
                                 : (context.addSubShapes.count(object.name) != 0U ? "delta" : "empty")},
        {"source_base", result.sourceBase},
        {"base_selection", selectionEvidenceJson(result.selection)},
        {"bbox", cad_core::part::bboxForShape(result.shape)},
        {"volume", cad_core::part::volumeForShape(result.shape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (result.refineApplied) {
        context.objects[object.name]["refine"] = "applied";
    }
    if (!result.parameters.empty()) {
        context.objects[object.name]["parameters"] = result.parameters;
    }
}

}  // namespace

void executeFillet(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Base",
                                      "BaseFeature",
                                      "SupportTransform",
                                      "Radius",
                                      "UseAllEdges",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildFillet(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

void executeChamfer(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Base",
                                      "BaseFeature",
                                      "SupportTransform",
                                      "ChamferType",
                                      "Size",
                                      "Size2",
                                      "Angle",
                                      "FlipDirection",
                                      "UseAllEdges",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildChamfer(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

void executeDraft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementDraft()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Base",
                                      "BaseFeature",
                                      "SupportTransform",
                                      "Angle",
                                      "NeutralPlane",
                                      "PullDirection",
                                      "Reversed",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildDraft(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

void executeThickness(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp::Thickness::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementThickSolid()
    if (!runtime::rejectUnsupportedProperties(object,
                                     context,
                                     {"Base",
                                      "BaseFeature",
                                      "SupportTransform",
                                      "Value",
                                      "Mode",
                                      "Join",
                                      "Reversed",
                                      "Intersection",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildThickness(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

}  // namespace cad_core::part_design
