#include "cad_core/features/chamfer.h"
#include "cad_core/features/fillet.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_Shape.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::features {

namespace {

struct DressUpBase {
    document::Link link;
    TopoDS_Shape shape;
    std::optional<topo::NamedShape> namedShape;
};

struct EdgeSelection {
    TopoDS_Edge edge;
    std::string subname;
};

struct DressUpResult {
    std::string mode;
    std::string sourceBase;
    DressUpBase base;
    TopoDS_Shape shape;
    topo::NamedShape namedShape;
    bool supportTransform = false;
    bool refineApplied = false;
};

struct ShapeSlotBuild {
    bool ok = true;
    std::optional<TopoDS_Shape> shape;
};

enum class AddSubKind {
    Additive,
    Subtractive,
    Unknown,
};

const nlohmann::json* propertyPayload(const document::DocumentObject& object, const std::string& property)
{
    const auto* value = document::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string readEnumProperty(const document::DocumentObject& object,
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

bool readBoolProperty(const document::DocumentObject& object, const std::string& property, bool fallback = false)
{
    return document::readBool(object, property).value_or(fallback);
}

double readNumberProperty(const document::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool isDressUpType(const std::string& typeId)
{
    return typeId == "PartDesign::Fillet" || typeId == "PartDesign::Chamfer";
}

bool hasSolid(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    TopExp_Explorer explorer(shape, TopAbs_SOLID);
    return explorer.More();
}

std::optional<topo::NamedShape> namedShapeForObject(const std::string& objectName,
                                                    const TopoDS_Shape& shape,
                                                    const runtime::ComputeContext& context)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    if (!shape.IsNull()) {
        return topo::indexedNamedShapeForObject(objectName, shape);
    }
    return std::nullopt;
}

topo::NamedShapeSource namedShapeSource(const std::string& owner,
                                        const TopoDS_Shape& shape,
                                        const std::optional<topo::NamedShape>& namedShape)
{
    return topo::NamedShapeSource{namedShape ? namedShape->owner : owner, shape, namedShape ? &*namedShape : nullptr};
}

std::optional<std::string> linkedObjectName(const document::DocumentObject& object, const std::string& property)
{
    if (document::propertyValue(object, property) == nullptr) {
        return std::nullopt;
    }
    const auto link = document::readLink(object, property);
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<TopoDS_Shape> solidShapeForObject(const std::string& objectName,
                                                runtime::ComputeContext& context,
                                                const document::DocumentObject& object,
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
                                                    const document::DocumentObject& object,
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

std::optional<std::string> baseLinkObjectName(const document::DocumentObject& object)
{
    const auto link = document::readLink(object, "Base");
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

ShapeSlotBuild cutSlot(const document::DocumentObject& object,
                       runtime::ComputeContext& context,
                       const std::string& owner,
                       const std::string& leftOwner,
                       const TopoDS_Shape& left,
                       const std::optional<topo::NamedShape>& leftNamedShape,
                       const std::string& rightOwner,
                       const TopoDS_Shape& right,
                       const std::optional<topo::NamedShape>& rightNamedShape,
                       const std::string& property)
{
    const auto build = topo::makeElementBooleanFromSources(owner,
                                                           {namedShapeSource(leftOwner, left, leftNamedShape),
                                                            namedShapeSource(rightOwner, right, rightNamedShape)},
                                                           topo::BooleanOperation::Cut);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Dress-up AddSubShape cache could not cut boolean sources: " + build.error,
                               object.name,
                               property);
        return ShapeSlotBuild{false, std::nullopt};
    }
    if (!hasSolid(build.shape)) {
        return ShapeSlotBuild{true, std::nullopt};
    }
    return ShapeSlotBuild{true, build.shape};
}

std::string stableSubnameDiagnosticCode(topo::ElementResolveStatus status)
{
    switch (status) {
        case topo::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case topo::ElementResolveStatus::Split:
            return "split_stable_subname";
        case topo::ElementResolveStatus::Resolved:
        case topo::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& target,
                                           const std::string& stableSubname,
                                           topo::ElementResolveStatus status)
{
    if (status == topo::ElementResolveStatus::Deleted) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == topo::ElementResolveStatus::Split) {
        return "Base target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return "Base target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

std::optional<DressUpBase> resolveDressUpBase(const document::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.h::DressUp,
    // declares "App::PropertyLinkSub Base" for the feature and its selected subelements.
    if (document::propertyValue(object, "Base") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               object.typeId + " Base must link to a solid and selected subelements",
                               object.name,
                               "Base");
        return std::nullopt;
    }

    const auto link = document::readLink(object, "Base");
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

    std::optional<topo::NamedShape> namedShape;
    const auto namedShapeIt = context.namedShapes.find(link->object);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = namedShapeIt->second;
    }

    return DressUpBase{*link, shapeIt->second.shape, namedShape};
}

std::optional<TopoDS_Shape> resolveDressUpSubshape(const DressUpBase& base,
                                                   const document::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   std::size_t subnameIndex)
{
    const std::string& requestedSubname = base.link.subnames[subnameIndex];
    const std::string stableSubname =
        subnameIndex < base.link.stableSubnames.size() ? base.link.stableSubnames[subnameIndex] : std::string{};

    std::string currentSubname = requestedSubname;
    if (base.namedShape) {
        const auto resolved = topo::resolveElementReference(*base.namedShape, requestedSubname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
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

    const auto parsed = topo::parseSubshapeName(currentSubname);
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
                               "Dress-up Base requires EdgeN or FaceN, not " + topo::subshapeKindName(parsed->kind),
                               object.name,
                               "Base",
                               "runtime",
                               base.link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (base.namedShape) {
        subshape = topo::subshapeByName(*base.namedShape, currentSubname);
    }
    else {
        subshape = topo::subshapeByName(base.shape, currentSubname);
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

void addSelectedEdge(const TopoDS_Shape& baseShape,
                     const TopoDS_Edge& edge,
                     const std::string& subname,
                     std::vector<EdgeSelection>& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
    // ::DressUp::getContinuousEdges(), keeps selected edges only when they have exactly two
    // adjacent faces and "BRep_Tool::Continuity(...) == GeomAbs_C0".
    if (!isC0ContinuousEdge(baseShape, edge)) {
        return;
    }
    for (const auto& existing : result) {
        if (existing.edge.IsSame(edge)) {
            return;
        }
    }
    result.push_back(EdgeSelection{edge, subname});
}

std::vector<EdgeSelection> allEdges(const TopoDS_Shape& baseShape)
{
    std::vector<EdgeSelection> result;
    int index = 1;
    for (TopExp_Explorer explorer(baseShape, TopAbs_EDGE); explorer.More(); explorer.Next(), ++index) {
        result.push_back(EdgeSelection{TopoDS::Edge(explorer.Current()), "Edge" + std::to_string(index)});
    }
    return result;
}

std::optional<std::vector<EdgeSelection>> selectedDressUpEdges(const DressUpBase& base,
                                                               const document::DocumentObject& object,
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

    std::vector<EdgeSelection> edges;
    for (std::size_t index = 0; index < base.link.subnames.size(); ++index) {
        const auto subshape = resolveDressUpSubshape(base, object, context, index);
        if (!subshape) {
            return std::nullopt;
        }
        const std::string& subname = base.link.subnames[index];
        if (subshape->ShapeType() == TopAbs_EDGE) {
            addSelectedEdge(base.shape, TopoDS::Edge(*subshape), subname, edges);
            continue;
        }
        if (subshape->ShapeType() == TopAbs_FACE || subshape->ShapeType() == TopAbs_WIRE) {
            for (TopExp_Explorer explorer(*subshape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
                addSelectedEdge(base.shape, TopoDS::Edge(explorer.Current()), subname, edges);
            }
        }
    }

    if (edges.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Dress-up operation is not possible on selected shapes",
                               object.name,
                               "Base");
        return std::nullopt;
    }
    return edges;
}

std::optional<TopoDS_Face> ancestorFaceForChamfer(const TopoDS_Shape& baseShape,
                                                  const TopoDS_Edge& edge,
                                                  bool flipDirection,
                                                  const document::DocumentObject& object,
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

std::optional<DressUpResult> buildFillet(const document::DocumentObject& object, runtime::ComputeContext& context)
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
        for (const auto& edge : *edges) {
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
        topo::NamedShape namedShape = topo::namedShapeForMakerHistory(object.name,
                                                                      result,
                                                                      {topo::NamedShapeSource{base->link.object,
                                                                                              base->shape,
                                                                                              base->namedShape
                                                                                                  ? &*base->namedShape
                                                                                                  : nullptr}},
                                                                      maker);
        return DressUpResult{"fillet",
                             base->link.object,
                             *base,
                             result,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
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

std::optional<DressUpResult> buildChamfer(const document::DocumentObject& object, runtime::ComputeContext& context)
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
        for (const auto& edge : *edges) {
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
        topo::NamedShape namedShape = topo::namedShapeForMakerHistory(object.name,
                                                                      result,
                                                                      {topo::NamedShapeSource{base->link.object,
                                                                                              base->shape,
                                                                                              base->namedShape
                                                                                                  ? &*base->namedShape
                                                                                                  : nullptr}},
                                                                      maker);
        return DressUpResult{"chamfer",
                             base->link.object,
                             *base,
                             result,
                             namedShape,
                             readBoolProperty(object, "SupportTransform")};
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

bool cacheDressUpAddSubShape(const document::DocumentObject& object,
                             runtime::ComputeContext& context,
                             const DressUpResult& result)
{
    runtime::AddSubShape cache;
    const auto resultNamedShape = std::optional<topo::NamedShape>{result.namedShape};

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
            }
            else {
                cache.addShape = result.shape;
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
            }
            else {
                cache.subShape = result.shape;
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
    cache.subShape = sub.shape;
    if (cache.addShape || cache.subShape) {
        context.addSubShapes[object.name] = cache;
    }
    return true;
}

bool applyDressUpRefine(const document::DocumentObject& object,
                        runtime::ComputeContext& context,
                        DressUpResult& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp
    // ::Fillet::execute() and FeatureChamfer.cpp::Chamfer::execute(), both store rawShape and
    // then call "shape = refineShapeIfActive(shape)" before publishing Shape.
    const auto refined = applyRefineProperty(object, context, result.shape, result.namedShape);
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

void publishDressUpResult(const document::DocumentObject& object,
                          runtime::ComputeContext& context,
                          const DressUpResult& result)
{
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, result.shape};
    context.namedShapes[object.name] = result.namedShape;
    context.mesh[object.name] = geometry::meshForShape(result.shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(result.shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"body_mode", "replace"},
        {"dress_up", result.mode},
        {"support_transform", result.supportTransform},
        {"add_sub_cache",
         result.supportTransform ? "support_transform"
                                 : (context.addSubShapes.count(object.name) != 0U ? "delta" : "empty")},
        {"source_base", result.sourceBase},
        {"bbox", geometry::bboxForShape(result.shape)},
        {"volume", geometry::volumeForShape(result.shape)},
        {"kernel", geometry::kernelVersion()},
    };
    if (result.refineApplied) {
        context.objects[object.name]["refine"] = "applied";
    }
}

}  // namespace

void executeFillet(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()
    if (!rejectUnsupportedProperties(object,
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

void executeChamfer(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()
    if (!rejectUnsupportedProperties(object,
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

}  // namespace cad_core::features
