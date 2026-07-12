#include "cad_core/runtime/recompute.h"

#include "cad_core/base/placement.h"
#include "cad_core/graph/recompute_plan.h"
#include "cad_core/runtime/element_reference_update.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/reference_lifecycle.h"
#include "cad_core/runtime/reference_resolution.h"
#include "cad_core/runtime/topo_naming_state.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/topo/subshape_identity.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::runtime {

namespace {

gp_Trsf objectPlacement(const app::DocumentObject& object)
{
    if (object.typeId == "App::Origin") {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Origin.cpp::Origin::Origin(),
        // "App::Origin is a LCS for which placement is fixed to identity"; parent group
        // placement is still applied by resolveGlobalPlacement().
        return gp_Trsf();
    }
    if (const auto placement = app::readPlacement(object, "Placement")) {
        return base::placementFromComponents(placement->base, placement->rotation);
    }
    return gp_Trsf();
}

gp_Trsf resolveGlobalPlacement(const app::Document& document,
                               const std::string& objectName,
                               std::map<std::string, gp_Trsf>& placements,
                               std::set<std::string>& visiting)
{
    const auto cached = placements.find(objectName);
    if (cached != placements.end()) {
        return cached->second;
    }

    const auto objectIt = document.indexByName.find(objectName);
    if (objectIt == document.indexByName.end() || visiting.count(objectName) != 0U) {
        return gp_Trsf();
    }

    visiting.insert(objectName);
    const auto& object = document.objects.at(objectIt->second);
    gp_Trsf placement = objectPlacement(object);
    const auto parentIt = document.parentGroupByObject.find(objectName);
    if (parentIt != document.parentGroupByObject.end()) {
        placement = resolveGlobalPlacement(document, parentIt->second, placements, visiting) * placement;
    }
    visiting.erase(objectName);

    placements[objectName] = placement;
    return placement;
}

std::map<std::string, gp_Trsf> buildGlobalPlacements(const app::Document& document)
{
    std::map<std::string, gp_Trsf> placements;
    std::set<std::string> visiting;
    for (const auto& object : document.objects) {
        resolveGlobalPlacement(document, object.name, placements, visiting);
    }
    return placements;
}

std::map<std::string, const app::DocumentObject*> buildDocumentObjectMap(const app::Document& document)
{
    std::map<std::string, const app::DocumentObject*> objects;
    for (const auto& object : document.objects) {
        objects[object.name] = &object;
    }
    return objects;
}

std::set<std::string> findTransformationTemplateObjects(const app::Document& document)
{
    std::set<std::string> templates;
    for (const auto& object : document.objects) {
        if (object.typeId != "PartDesign::MultiTransform") {
            continue;
        }
        for (const auto& link : app::readLinks(object, "Transformations")) {
            templates.insert(link.object);
        }
    }
    return templates;
}

std::string standardFailureTypeName(const Standard_Failure& failure)
{
    const Handle(Standard_Type)& type = failure.DynamicType();
    if (!type.IsNull() && type->Name() != nullptr && type->Name()[0] != '\0') {
        return type->Name();
    }
    return "Standard_Failure";
}

std::string standardFailureMessage(const Standard_Failure& failure)
{
    const std::string typeName = standardFailureTypeName(failure);
    const char* message = failure.GetMessageString();
    if (message == nullptr || message[0] == '\0') {
        return typeName;
    }
    return typeName + ": " + message;
}

void markOcctExecutionFailure(const app::DocumentObject& object,
                              ComputeContext& context,
                              const std::string& message)
{
    addDiagnostic(
        context.diagnostics,
        "error",
        "execution_failed",
        message,
        object.name,
        {},
        "runtime"
    );
    context.objects[object.name] = {
        {"status", "error"},
        {"reason", message},
    };
}

void mergeTopoNamingStateElementMap(const std::string& name, ComputeContext& context)
{
    auto namedShapeIt = context.namedShapes.find(name);
    if (namedShapeIt == context.namedShapes.end() || !context.topoNamingState.is_object()) {
        return;
    }
    const auto objectsIt = context.topoNamingState.find("objects");
    if (objectsIt == context.topoNamingState.end() || !objectsIt->is_object()) {
        return;
    }
    const auto objectIt = objectsIt->find(name);
    if (objectIt == objectsIt->end() || !objectIt->is_object()) {
        return;
    }
    const auto documentObjectIt = context.documentObjects.find(name);
    if (documentObjectIt != context.documentObjects.end()
        && documentObjectIt->second != nullptr
        && objectIt->value("objectHash", "")
               != topoNamingStateObjectHash(*documentObjectIt->second)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
        // ::PropertyLinkBase::_updateElementReference() can attempt recovery when persisted
        // metadata is stale, but a mismatched object state must not seed current ElementMap
        // aliases before the object has been recomputed.
        return;
    }
    const auto elementMapIt = objectIt->find("elementMap");
    if (elementMapIt == objectIt->end() || !elementMapIt->is_object()) {
        return;
    }
    const auto entriesIt = elementMapIt->find("entries");
    if (entriesIt == elementMapIt->end() || !entriesIt->is_object()) {
        return;
    }

    part::NamedShape& namedShape = namedShapeIt->second;
    for (const auto& entryItem : entriesIt->items()) {
        const std::string stableSubname = entryItem.key();
        const nlohmann::json& entry = entryItem.value();
        if (stableSubname.empty() || !entry.is_object()) {
            continue;
        }
        const auto targetIt = entry.find("target");
        if (targetIt == entry.end() || !targetIt->is_object()) {
            continue;
        }
        if (targetIt->value("object", "") != name) {
            continue;
        }
        const std::string currentSubname = targetIt->value("subname", "");
        if (currentSubname.empty() || namedShape.elements.count(currentSubname) == 0U) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
        // ::PropertyLinkBase::_updateElementReference(), resolves a persisted stable name
        // through the target object's ElementMap before validating the current subname.
        namedShape.elementMap[stableSubname] = currentSubname;
    }
}

bool isCopyOnChangeCreatedObject(const std::string& name, const ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(name);
    if (objectIt == context.documentObjects.end() || objectIt->second == nullptr) {
        return false;
    }
    const auto sourceIt = objectIt->second->properties.find("_CopyOnChangeSourceObject");
    return sourceIt != objectIt->second->properties.end() && sourceIt->is_object();
}

void mergeCopyOnChangeElementMapAliases(const std::string& name, ComputeContext& context)
{
    auto namedShapeIt = context.namedShapes.find(name);
    if (namedShapeIt == context.namedShapes.end() || !isCopyOnChangeCreatedObject(name, context)) {
        return;
    }

    part::NamedShape& namedShape = namedShapeIt->second;
    for (const auto& [elementName, element] : namedShape.elements) {
        if (!topo::topologicalElementKind(elementName)) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::copyOnChangeCopyObject(), copies linked children while preserving
        // subelement references. cad-core's immutable document update replay needs a request-local
        // ElementMap alias for the newly-created copied object name.
        namedShape.elementMap[name + "." + elementName] = element.name;
    }
}

void registerIndexedNamedShape(const std::string& name, ComputeContext& context)
{
    if (context.namedShapes.count(name) != 0U) {
        mergeTopoNamingStateElementMap(name, context);
        mergeCopyOnChangeElementMapAliases(name, context);
        return;
    }
    const auto shapeIt = context.shapes.find(name);
    if (shapeIt != context.shapes.end()) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, shapeIt->second.shape);
        mergeTopoNamingStateElementMap(name, context);
        mergeCopyOnChangeElementMapAliases(name, context);
        return;
    }
    const auto addSubIt = context.addSubShapes.find(name);
    if (addSubIt == context.addSubShapes.end()) {
        return;
    }
    if (addSubIt->second.addNamedShape) {
        context.namedShapes[name] = *addSubIt->second.addNamedShape;
    }
    else if (addSubIt->second.subNamedShape) {
        context.namedShapes[name] = *addSubIt->second.subNamedShape;
    }
    else if (addSubIt->second.addShape) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, *addSubIt->second.addShape);
    }
    else if (addSubIt->second.subShape) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, *addSubIt->second.subShape);
    }
    mergeTopoNamingStateElementMap(name, context);
    mergeCopyOnChangeElementMapAliases(name, context);
}

std::string displayKind(const nlohmann::json& subshape)
{
    const std::string kind = subshape.value("kind", "");
    if (kind == "face") {
        return "Face";
    }
    if (kind == "edge") {
        return "Edge";
    }
    if (kind == "vertex") {
        return "Vertex";
    }
    return kind.empty() ? "Unknown" : kind;
}

std::string localElementName(const std::string& name)
{
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::optional<std::string> topologicalElementKind(const std::string& name)
{
    std::string local = localElementName(name);
    constexpr const char* internalPrefix = "Internal";
    if (local.rfind(internalPrefix, 0) == 0) {
        local = local.substr(std::string(internalPrefix).size());
    }
    const auto hasPrefixAndDigits = [&local](const std::string& prefix) {
        if (local.rfind(prefix, 0) != 0 || local.size() == prefix.size()) {
            return false;
        }
        return std::all_of(local.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                           local.end(),
                           [](unsigned char value) { return std::isdigit(value) != 0; });
    };
    if (hasPrefixAndDigits("Face")) {
        return "Face";
    }
    if (hasPrefixAndDigits("Edge")) {
        return "Edge";
    }
    if (hasPrefixAndDigits("Vertex")) {
        return "Vertex";
    }
    return std::nullopt;
}

bool stableSubnameKindMatchesIndexed(const std::string& indexed, const std::string& stableSubname)
{
    const auto indexedKind = topologicalElementKind(indexed);
    const auto stableKind = topologicalElementKind(stableSubname);
    if (!indexedKind || !stableKind) {
        return true;
    }
    return *indexedKind == *stableKind;
}

bool namedShapeHasCurrentElementEvidence(const std::string& indexed,
                                         const part::NamedShape* namedShape)
{
    if (namedShape == nullptr) {
        return false;
    }
    return std::any_of(namedShape->elementMap.begin(),
                       namedShape->elementMap.end(),
                       [&](const auto& item) {
                           return item.second == indexed
                               && topo::hasStableElementMapEvidence(namedShape, indexed, item.first);
                       });
}

int stableSubnamePriority(const std::string& indexed, const std::string& stableSubname)
{
    if (stableSubname == indexed) {
        return 0;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap(), first preserves source subelement names through
    // "mapSubElement(shapes)" and then consumes maker history into ElementMap. When both a
    // display-path alias such as Pad.Face5 and a source alias such as Sketch.Face1 target the
    // same current element, the source alias is the stable reference identity.
    return localElementName(stableSubname) == indexed ? 1 : 2;
}

std::string stableSubnameFor(const std::string& indexed,
                             const part::NamedShape* namedShape)
{
    const bool internalIndexed = indexed.rfind("InternalFace", 0) == 0
        || indexed.rfind("InternalEdge", 0) == 0
        || indexed.rfind("InternalVertex", 0) == 0;
    const std::string stableSubname = topo::stableSubnameFromNamedShape(indexed, namedShape);
    if (!stableSubname.empty()) {
        return internalIndexed && stableSubname == indexed ? std::string{} : stableSubname;
    }
    // Sketch Internal* names are request-local until the sketch InternalShape has a real
    // NamedShape/ElementMap. Do not synthesize a stable name from the current indexed name.
    return {};
}

std::string internalElementStableSubnameFor(const std::string& objectName,
                                            const std::string& indexed,
                                            const ComputeContext& context)
{
    if (indexed.rfind("InternalEdge", 0) != 0 && indexed.rfind("InternalVertex", 0) != 0) {
        return {};
    }

    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return {};
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return {};
    }
    const auto mappedIt = mapIt->find(indexed);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return {};
    }

    const std::string stableSubname = mappedIt->get<std::string>();
    if (stableSubname.rfind("Internal", 0) == 0) {
        return {};
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::getInternalElementMap() only maps InternalVertex/InternalEdge to raw Vertex/Edge names
    // with findSubShapesWithSharedVertex(..., CheckGeometry | SingleResult). InternalFace still
    // waits for FaceMaker/WireJoiner history before cad-core can publish a stable name.
    return stableSubname;
}

bool isRawSketchGeometryStableSubname(const std::string& value)
{
    if (value.size() < 2U || value.front() != 'g') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char item) {
        return std::isdigit(item) != 0;
    });
}

std::optional<std::string> rawSketchStableSubnameForCurrentEdge(const std::string& objectName,
                                                                const std::string& indexed,
                                                                const ComputeContext& context)
{
    const auto parsed = part::parseSubshapeName(localElementName(indexed));
    if (!parsed || parsed->kind != TopAbs_EDGE) {
        return std::nullopt;
    }

    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto identityIt = objectIt->second.find("raw_edge_identity");
    if (identityIt == objectIt->second.end() || !identityIt->is_object()) {
        return std::nullopt;
    }
    const auto byIndexedIt = identityIt->find("byIndexed");
    if (byIndexedIt == identityIt->end() || !byIndexedIt->is_object()) {
        return std::nullopt;
    }
    const auto indexedIt = byIndexedIt->find(localElementName(indexed));
    if (indexedIt == byIndexedIt->end() || !indexedIt->is_object()) {
        return std::nullopt;
    }
    const auto stableIt = indexedIt->find("sourceStableSubname");
    if (stableIt == indexedIt->end() || !stableIt->is_string()) {
        return std::nullopt;
    }
    const std::string stableSubname = stableIt->get<std::string>();
    if (!isRawSketchGeometryStableSubname(stableSubname)) {
        return std::nullopt;
    }
    return stableSubname;
}

std::optional<std::string> rawSketchEdgeForInternalEdge(const std::string& objectName,
                                                        const std::string& indexed,
                                                        const ComputeContext& context)
{
    if (indexed.rfind("InternalEdge", 0) != 0) {
        return std::nullopt;
    }

    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return std::nullopt;
    }
    const auto mappedIt = mapIt->find(indexed);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return std::nullopt;
    }
    const std::string rawEdge = mappedIt->get<std::string>();
    const auto parsed = part::parseSubshapeName(rawEdge);
    if (!parsed || parsed->kind != TopAbs_EDGE) {
        return std::nullopt;
    }
    return rawEdge;
}

std::string normalizedInternalEdgeStableSubname(const std::string& objectName,
                                                const std::string& indexed,
                                                const std::string& candidateStableSubname,
                                                const ComputeContext& context)
{
    if (indexed.rfind("InternalEdge", 0) != 0) {
        return candidateStableSubname;
    }
    if (isRawSketchGeometryStableSubname(candidateStableSubname)) {
        return candidateStableSubname;
    }

    std::optional<std::string> rawEdge;
    const auto candidate = part::parseSubshapeName(localElementName(candidateStableSubname));
    if (candidate && candidate->kind == TopAbs_EDGE) {
        rawEdge = localElementName(candidateStableSubname);
    }
    if (!rawEdge) {
        rawEdge = rawSketchEdgeForInternalEdge(objectName, indexed, context);
    }
    if (!rawEdge) {
        return {};
    }
    return rawSketchStableSubnameForCurrentEdge(objectName, *rawEdge, context).value_or(std::string {});
}

std::string currentSubnameForStable(const std::string& indexed,
                                    const std::string& stableSubname)
{
    const std::size_t dot = stableSubname.rfind('.');
    if (dot == std::string::npos) {
        return indexed;
    }
    return stableSubname.substr(0, dot + 1) + indexed;
}

std::string displaySubnameFor(const std::string& indexed, const part::NamedShape* namedShape)
{
    if (namedShape == nullptr) {
        return {};
    }
    for (const auto& [stableSubname, currentSubname] : namedShape->elementMap) {
        if (currentSubname != indexed) {
            continue;
        }
        if (!stableSubnameKindMatchesIndexed(indexed, stableSubname)) {
            continue;
        }
        if (stableSubname != indexed && localElementName(stableSubname) == indexed) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
            // ::Body::execute(), reads the Tip shape and writes "Shape.setValue(tipShape)".
            // cad-core keeps Body display subnames in the current Tip namespace, while
            // stableSubname may point further back to the profile source ElementMap alias.
            return stableSubname;
        }
    }
    return {};
}

bool isPlainTopologicalElementName(const std::string& name)
{
    return localElementName(name) == name && topologicalElementKind(name).has_value();
}

struct BodyTipSubshapeResponseContext {
    std::string owner;
    bool stablePrefix = false;
};

struct TopologicalElementToken {
    std::string prefix;
    std::string kind;
    int index = 0;
};

struct BodyDisplayCompoundResponseContext {
    const part::NamedShape* namedShape = nullptr;
    std::set<std::string> childFeatures;
    std::map<std::string, bool> childHasNamedShape;
};

struct ResponseMappedNamePublication {
    std::string rawMappedName;
    std::string canonicalMappedName;
    std::string resolvedIndexed;
};

std::optional<std::string> qualifiedStableSubnameOwner(const std::string& stableSubname)
{
    const std::size_t dot = stableSubname.find('.');
    if (dot == std::string::npos || dot == 0U) {
        return std::nullopt;
    }
    if (!topologicalElementKind(stableSubname)) {
        return std::nullopt;
    }
    return stableSubname.substr(0, dot);
}

bool bodyTipStableSubnameShouldUseCurrentOwner(
    const std::string& stableSubname,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext)
{
    if (!tipContext) {
        return false;
    }
    const auto owner = qualifiedStableSubnameOwner(stableSubname);
    return owner && *owner != tipContext->owner;
}

bool namedShapeMapsStableToCurrent(const part::NamedShape* namedShape,
                                   const std::string& stableSubname,
                                   const std::string& indexed)
{
    if (namedShape == nullptr || stableSubname.empty()) {
        return false;
    }
    const auto mappedIt = namedShape->elementMap.find(stableSubname);
    return mappedIt != namedShape->elementMap.end() && mappedIt->second == indexed;
}

bool bodyTipStableSubnameCanUseCurrentOwner(
    const std::string& indexed,
    const std::string& stableSubname,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* stableSource)
{
    if (!bodyTipStableSubnameShouldUseCurrentOwner(stableSubname, tipContext)) {
        return false;
    }
    return namedShapeMapsStableToCurrent(stableSource, tipContext->owner + "." + indexed, indexed);
}

std::string bodyTipBodyLocalStableSubnameFor(
    const std::string& indexed,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* stableSource)
{
    if (!tipContext || !isPlainTopologicalElementName(indexed)) {
        return {};
    }
    return topo::hasStableElementMapEvidence(stableSource, indexed, indexed) ? indexed : std::string {};
}

std::optional<TopologicalElementToken> parseTopologicalElementToken(const std::string& indexed)
{
    const std::string local = localElementName(indexed);
    const auto parseWithPrefix = [&local](const std::string& prefix,
                                          const std::string& kind) -> std::optional<TopologicalElementToken> {
        if (local.rfind(prefix, 0) != 0 || local.size() == prefix.size()) {
            return std::nullopt;
        }
        int index = 0;
        for (auto it = local.begin() + static_cast<std::ptrdiff_t>(prefix.size()); it != local.end(); ++it) {
            if (std::isdigit(static_cast<unsigned char>(*it)) == 0) {
                return std::nullopt;
            }
            index = index * 10 + (*it - '0');
        }
        if (index <= 0) {
            return std::nullopt;
        }
        return TopologicalElementToken{prefix, kind, index};
    };
    if (const auto token = parseWithPrefix("Face", "face")) {
        return token;
    }
    if (const auto token = parseWithPrefix("Edge", "edge")) {
        return token;
    }
    return parseWithPrefix("Vertex", "vertex");
}

std::optional<BodyTipSubshapeResponseContext> bodyTipSubshapeResponseContext(
    const std::string& objectName,
    const ComputeContext& context)
{
    const auto documentIt = context.documentObjects.find(objectName);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr
        || documentIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto ownerIt = objectIt->second.find("direct_tip_subshape_owner");
    if (ownerIt == objectIt->second.end() || !ownerIt->is_string() || ownerIt->get<std::string>().empty()) {
        return std::nullopt;
    }
    return BodyTipSubshapeResponseContext {
        ownerIt->get<std::string>(),
        objectIt->second.value("direct_tip_subshape_stable_prefix", false),
    };
}

std::optional<BodyDisplayCompoundResponseContext> bodyDisplayCompoundResponseContext(
    const std::string& objectName,
    const ComputeContext& context,
    const part::NamedShape* namedShape)
{
    const auto documentIt = context.documentObjects.find(objectName);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr
        || documentIt->second->typeId != "PartDesign::Body" || namedShape == nullptr) {
        return std::nullopt;
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()
        || !objectIt->second.value("body_adopted_display_only_compound", false)) {
        return std::nullopt;
    }

    BodyDisplayCompoundResponseContext result;
    result.namedShape = namedShape;
    const auto childrenIt = objectIt->second.find("display_only_children");
    if (childrenIt != objectIt->second.end() && childrenIt->is_array()) {
        for (const auto& child : *childrenIt) {
            if (!child.is_object()) {
                continue;
            }
            const auto featureIt = child.find("feature");
            if (featureIt != child.end() && featureIt->is_string() && !featureIt->get<std::string>().empty()) {
                const std::string feature = featureIt->get<std::string>();
                result.childFeatures.insert(feature);
                result.childHasNamedShape[feature] = child.value("has_named_shape", false);
            }
        }
    }
    return result;
}

std::optional<std::string> bodyDisplayCompoundSubnameFor(
    const std::string& indexed,
    const std::optional<BodyDisplayCompoundResponseContext>& displayContext)
{
    if (!displayContext || displayContext->namedShape == nullptr) {
        return std::nullopt;
    }
    const auto token = parseTopologicalElementToken(indexed);
    if (!token) {
        return std::nullopt;
    }
    for (const part::NamedShapeChildMap& childMap : displayContext->namedShape->childElementMaps) {
        if (childMap.kind != token->kind || childMap.count <= 0 || childMap.sourceOwner.empty()) {
            continue;
        }
        if (!displayContext->childFeatures.empty()
            && displayContext->childFeatures.count(childMap.sourceOwner) == 0U) {
            continue;
        }
        if (token->index <= childMap.offset || token->index > childMap.offset + childMap.count) {
            continue;
        }
        const int localIndex = token->index - childMap.offset;
        return childMap.sourceOwner + "." + token->prefix + std::to_string(localIndex);
    }
    return std::nullopt;
}

std::optional<std::string> bodyDisplayCompoundOwnerFor(
    const std::string& indexed,
    const std::optional<BodyDisplayCompoundResponseContext>& displayContext)
{
    if (!displayContext || displayContext->namedShape == nullptr) {
        return std::nullopt;
    }
    const auto token = parseTopologicalElementToken(indexed);
    if (!token) {
        return std::nullopt;
    }
    for (const part::NamedShapeChildMap& childMap : displayContext->namedShape->childElementMaps) {
        if (childMap.kind != token->kind || childMap.count <= 0 || childMap.sourceOwner.empty()) {
            continue;
        }
        if (!displayContext->childFeatures.empty()
            && displayContext->childFeatures.count(childMap.sourceOwner) == 0U) {
            continue;
        }
        if (token->index <= childMap.offset || token->index > childMap.offset + childMap.count) {
            continue;
        }
        return childMap.sourceOwner;
    }
    return std::nullopt;
}

std::string bodyDisplayCompoundQualifiedStableSubname(
    const std::string& indexed,
    const std::string& stableSubname,
    const std::optional<BodyDisplayCompoundResponseContext>& displayContext)
{
    if (!displayContext || stableSubname.empty() || stableSubname.find('.') != std::string::npos
        || isPlainTopologicalElementName(stableSubname)) {
        return stableSubname;
    }
    const auto owner = bodyDisplayCompoundOwnerFor(indexed, displayContext);
    if (!owner) {
        return stableSubname;
    }
    const auto childIt = displayContext->childHasNamedShape.find(*owner);
    if (childIt != displayContext->childHasNamedShape.end() && !childIt->second) {
        return {};
    }
    // FreeCAD:
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // reads the Tip/display child shape while App::ElementMap child maps keep the child source
    // owner. When a Body display-only compound preserves a child-local stable alias such as
    // "g100001", publish it as "Pad.g100001" so downstream PropertyLinkSub can peel off the
    // selected child owner instead of treating the Body result as an ownerless stable name.
    return *owner + "." + stableSubname;
}

bool bodyDisplayCompoundStableSubnameHasChildEvidence(
    const std::string& stableSubname,
    const std::optional<BodyDisplayCompoundResponseContext>& displayContext)
{
    if (!displayContext || stableSubname.empty()) {
        return true;
    }
    const std::size_t dot = stableSubname.rfind('.');
    if (dot == std::string::npos || dot == 0U) {
        return true;
    }
    const std::string owner = stableSubname.substr(0, dot);
    const auto childIt = displayContext->childHasNamedShape.find(owner);
    return childIt == displayContext->childHasNamedShape.end() || childIt->second;
}

bool hasSourceBackedMappedName(const part::MappedNameProvenance& provenance)
{
    return provenance.status == part::MappedNameProvenanceStatus::SourceBacked
        && !provenance.rawMappedName.empty()
        && !provenance.canonicalMappedName.empty();
}

bool hasFreeCadEncodedElementToken(const std::string& rawMappedName)
{
    const std::size_t postfix = rawMappedName.find(';');
    const std::string data = rawMappedName.substr(0, postfix);
    return data.find('#') != std::string::npos;
}

bool isProducerOnlyMappedElement(const std::string& indexed,
                                 const part::NamedShape* namedShape)
{
    if (namedShape == nullptr) {
        return false;
    }
    for (const auto& [stableName, currentName] : namedShape->elementMap) {
        if (currentName != indexed) {
            continue;
        }
        const auto provenanceIt = namedShape->mappedNameProvenance.find(stableName);
        if (provenanceIt != namedShape->mappedNameProvenance.end()
            && provenanceIt->second.publicationScope
                == part::MappedNamePublicationScope::ProducerOnly) {
            return true;
        }
    }
    return false;
}

std::string prefixedMappedName(const std::string& prefix,
                               const std::string& mappedName)
{
    if (prefix.empty() || mappedName.rfind(prefix, 0) == 0) {
        return mappedName;
    }
    return prefix + mappedName;
}

std::optional<ResponseMappedNamePublication> publicationFromProvenance(
    const std::string& prefix,
    const std::string& indexed,
    const part::MappedNameProvenance& provenance)
{
    const std::size_t postfix = provenance.rawMappedName.find(';');
    const bool producerLocalMappedName = provenance.sourceElement.find('.') == std::string::npos
        && postfix != std::string::npos
        && provenance.rawMappedName.substr(0, postfix) == provenance.sourceElement
        && provenance.operationPostfix.rfind(";:M;", 0U) == 0U;
    const bool sketchProducerMappedName = provenance.rawMappedName.find(";SKT;") != std::string::npos;
    if (!hasSourceBackedMappedName(provenance)
        || provenance.publicationScope
            == part::MappedNamePublicationScope::ProducerOnly
        || (!hasFreeCadEncodedElementToken(provenance.rawMappedName)
            && !producerLocalMappedName
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::SketchObject::buildShape() calls makeElementWires(..., Part::OpCodes::Sketch)
            // after convertSubName() produces g<ID>/g<ID>v<point>.  This admits the already
            // source-backed g...;SKT; producer record; it never synthesizes a response token.
            && !sketchProducerMappedName)) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::encodeElementName() may encode an operation directly after a producer-local
    // IndexedName (for example Edge1;:M;SEC;:H...), without a leading child-map '#'. Require the
    // raw base to be the unqualified producer source and the postfix to be maker-backed, so
    // owner-qualified lookup aliases do not replace an existing child-map identity.
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::execute(),
    // "Shape.setValue(tipShape)" publishes the Tip shape as Body display geometry, while
    // /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::addChildElements() keeps the Tip
    // mapped-name ledger as child-local evidence. Body response subshapes therefore expose the
    // current Body indexed name but carry the Tip-prefixed FreeCAD MappedName identity.
    return ResponseMappedNamePublication {
        prefixedMappedName(prefix, provenance.rawMappedName),
        prefixedMappedName(prefix, provenance.canonicalMappedName),
        indexed,
    };
}

std::optional<ResponseMappedNamePublication> bodyTipMappedNamePublicationFromOwner(
    const std::string& indexed,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* namedShape)
{
    if (!tipContext || namedShape == nullptr || tipContext->owner.empty()) {
        return std::nullopt;
    }
    const std::string ownerPrefix = tipContext->owner + ".";
    for (const auto& [stableName, currentName] : namedShape->elementMap) {
        if (currentName != indexed || stableName.rfind(ownerPrefix, 0) != 0) {
            continue;
        }
        const auto provenanceIt = namedShape->mappedNameProvenance.find(stableName);
        if (provenanceIt == namedShape->mappedNameProvenance.end()) {
            continue;
        }
        if (const auto publication =
                publicationFromProvenance(ownerPrefix, indexed, provenanceIt->second)) {
            return publication;
        }
    }
    return std::nullopt;
}

std::optional<ResponseMappedNamePublication> mappedNamePublicationFromNamedShape(
    const std::string& indexed,
    const part::NamedShape* namedShape)
{
    if (namedShape == nullptr) {
        return std::nullopt;
    }
    std::optional<ResponseMappedNamePublication> selected;
    int selectedPriority = 2;
    for (const auto& [stableName, currentName] : namedShape->elementMap) {
        if (currentName != indexed) {
            continue;
        }
        const auto provenanceIt = namedShape->mappedNameProvenance.find(stableName);
        if (provenanceIt == namedShape->mappedNameProvenance.end()) {
            continue;
        }
        if (const auto publication =
                publicationFromProvenance({}, indexed, provenanceIt->second)) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap() encodes the incoming
            // producer-local IndexedName. Owner-qualified names are resolution aliases, so a
            // source-backed local mapped name wins when both resolve to the same current shape.
            const int priority = stableName.find('.') == std::string::npos ? 0 : 1;
            if (!selected || priority < selectedPriority) {
                selected = *publication;
                selectedPriority = priority;
            }
        }
    }
    return selected;
}

std::optional<ResponseMappedNamePublication> bodyTipMappedNamePublicationFromChildMap(
    const std::string& indexed,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* namedShape)
{
    if (!tipContext || namedShape == nullptr || tipContext->owner.empty()) {
        return std::nullopt;
    }
    const auto token = parseTopologicalElementToken(indexed);
    if (!token) {
        return std::nullopt;
    }
    for (const part::NamedShapeChildMap& childMap : namedShape->childElementMaps) {
        if (childMap.sourceOwner != tipContext->owner
            || childMap.kind != token->kind
            || childMap.count <= 0
            || childMap.sourceNamedShape == nullptr) {
            continue;
        }
        if (token->index <= childMap.offset || token->index > childMap.offset + childMap.count) {
            continue;
        }
        const std::string childIndexed =
            token->prefix + std::to_string(token->index - childMap.offset);
        for (const auto& [stableName, currentName] : childMap.sourceNamedShape->elementMap) {
            if (currentName != childIndexed) {
                continue;
            }
            const auto provenanceIt = childMap.sourceNamedShape->mappedNameProvenance.find(stableName);
            if (provenanceIt == childMap.sourceNamedShape->mappedNameProvenance.end()) {
                continue;
            }
            if (const auto publication =
                    publicationFromProvenance(childMap.sourceOwner + ".", indexed, provenanceIt->second)) {
                return publication;
            }
        }
    }
    return std::nullopt;
}

std::optional<ResponseMappedNamePublication> bodyTipMappedNamePublicationFor(
    const std::string& indexed,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* namedShape)
{
    if (const auto publication =
            bodyTipMappedNamePublicationFromOwner(indexed, tipContext, namedShape)) {
        return publication;
    }
    return bodyTipMappedNamePublicationFromChildMap(indexed, tipContext, namedShape);
}

std::optional<ResponseMappedNamePublication> mappedNamePublicationFor(
    const std::string& indexed,
    const std::optional<BodyTipSubshapeResponseContext>& tipContext,
    const part::NamedShape* namedShape)
{
    if (const auto publication = bodyTipMappedNamePublicationFor(indexed, tipContext, namedShape)) {
        return publication;
    }
    return mappedNamePublicationFromNamedShape(indexed, namedShape);
}

std::string bodyTipQualifiedStableSubname(const std::string& objectName,
                                          const std::string& indexed,
                                          const std::string& stableSubname,
                                          const std::optional<BodyTipSubshapeResponseContext>& tipContext,
                                          const part::NamedShape* stableSource)
{
    if (!tipContext) {
        return stableSubname;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // "Shape.setValue(tipShape)" publishes the Tip feature shape as the Body display shape.
    // The response stable name may use the current Tip child path only when ElementMap evidence
    // proves the Tip owns that local element. Body-only display elements stay Body-local.
    const std::string ownerPrefix = tipContext->owner + ".";
    if (stableSubname.empty()) {
        return {};
    }
    if (bodyTipStableSubnameCanUseCurrentOwner(indexed, stableSubname, tipContext, stableSource)) {
        return ownerPrefix + indexed;
    }
    const std::string bodyPrefix = objectName + ".";
    if (stableSubname.rfind(ownerPrefix, 0) == 0) {
        return namedShapeMapsStableToCurrent(stableSource, stableSubname, indexed) ? stableSubname : std::string {};
    }
    if (stableSubname.rfind(bodyPrefix, 0) == 0) {
        const std::string ownerStableSubname = ownerPrefix + indexed;
        return namedShapeMapsStableToCurrent(stableSource, ownerStableSubname, indexed)
            ? ownerStableSubname
            : stableSubname;
    }
    if (stableSubname == indexed && isPlainTopologicalElementName(stableSubname)) {
        const std::string ownerStableSubname = ownerPrefix + stableSubname;
        return namedShapeMapsStableToCurrent(stableSource, ownerStableSubname, indexed)
            ? ownerStableSubname
            : stableSubname;
    }
    if (tipContext->stablePrefix) {
        const std::string ownerStableSubname = ownerPrefix + stableSubname;
        return namedShapeMapsStableToCurrent(stableSource, ownerStableSubname, indexed)
            ? ownerStableSubname
            : stableSubname;
    }
    return stableSubname;
}

bool bodyTipStableSubnameHasPublicationEvidence(const std::string& indexed,
                                                const std::string& stableSubname,
                                                const part::NamedShape* stableSource,
                                                const std::string& rawIdentityStatus)
{
    if (stableSubname.empty()) {
        return false;
    }
    if (rawIdentityStatus == "stable" || rawIdentityStatus == "stable_split_fragment") {
        return true;
    }
    return namedShapeHasCurrentElementEvidence(indexed, stableSource);
}

std::string responseSubnameFor(const std::string& indexed,
                               const std::string& stableSubname,
                               const part::NamedShape* namedShape,
                               const std::optional<BodyTipSubshapeResponseContext>& tipContext,
                               const std::optional<BodyDisplayCompoundResponseContext>& displayContext)
{
    if (tipContext) {
        const std::string ownerSubname = tipContext->owner + "." + indexed;
        return namedShapeMapsStableToCurrent(namedShape, ownerSubname, indexed) ? ownerSubname : indexed;
    }
    if (const auto displaySubname = bodyDisplayCompoundSubnameFor(indexed, displayContext)) {
        return *displaySubname;
    }
    const std::string displaySubname = displaySubnameFor(indexed, namedShape);
    if (!displaySubname.empty()) {
        return displaySubname;
    }
    return currentSubnameForStable(indexed, stableSubname);
}

std::string responseFullSubnameFor(const std::string& objectName,
                                   const std::string& subname)
{
    if (subname.empty()) {
        return {};
    }
    if (subname.find('.') == std::string::npos) {
        return objectName + "." + subname;
    }
    const std::string objectPrefix = objectName + ".";
    if (subname.rfind(objectPrefix, 0U) == 0U) {
        return subname;
    }
    return objectPrefix + subname;
}

std::string responseFullSubnameFor(const std::string& objectName,
                                   const std::string& indexed,
                                   const std::string& subname,
                                   const std::optional<BodyTipSubshapeResponseContext>& tipContext)
{
    if (tipContext && !indexed.empty()) {
        return objectName + "." + tipContext->owner + "." + indexed;
    }
    return responseFullSubnameFor(objectName, subname);
}

void copyStringField(nlohmann::json& target,
                     const nlohmann::json& source,
                     const std::string& field)
{
    const auto fieldIt = source.find(field);
    if (fieldIt != source.end() && fieldIt->is_string()) {
        target[field] = fieldIt->get<std::string>();
    }
}

void copyIdentityFields(nlohmann::json& target, const nlohmann::json& source)
{
    for (const std::string& field :
         {"stableSubname",
          "sourceStableSubname",
          "fragmentStableSubname",
          "sourceGeometryKind",
          "identityStatus"}) {
        copyStringField(target, source, field);
    }
    const auto sourceGeometryIdIt = source.find("sourceGeometryId");
    if (sourceGeometryIdIt != source.end() && sourceGeometryIdIt->is_number_integer()) {
        target["sourceGeometryId"] = sourceGeometryIdIt->get<long long>();
    }
    const auto sourceGeometryIndexIt = source.find("sourceGeometryIndex");
    if (sourceGeometryIndexIt != source.end() && sourceGeometryIndexIt->is_number_unsigned()) {
        target["sourceGeometryIndex"] = sourceGeometryIndexIt->get<std::size_t>();
    }
}

nlohmann::json responseMesh(const std::string& objectName,
                            const nlohmann::json& mesh,
                            const nlohmann::json& responseSubshapes)
{
    if (!mesh.is_object()) {
        return nullptr;
    }

    nlohmann::json indices = nlohmann::json::array();
    const auto trianglesIt = mesh.find("triangles");
    if (trianglesIt != mesh.end() && trianglesIt->is_array()) {
        for (const auto& triangle : *trianglesIt) {
            if (!triangle.is_array()) {
                continue;
            }
            for (const auto& index : triangle) {
                if (index.is_number_integer()) {
                    indices.push_back(index.get<int>());
                }
            }
        }
    }

    nlohmann::json faceIds = nlohmann::json::array();
    const auto faceIdsIt = mesh.find("faceIds");
    if (faceIdsIt != mesh.end() && faceIdsIt->is_array()) {
        for (const auto& faceId : *faceIdsIt) {
            if (faceId.is_string()) {
                faceIds.push_back(objectName + ":" + faceId.get<std::string>());
            }
        }
    }

    std::map<std::string, const nlohmann::json*> edgeSubshapeById;
    if (responseSubshapes.is_array()) {
        for (const auto& subshape : responseSubshapes) {
            if (!subshape.is_object() || subshape.value("kind", "") != "Edge") {
                continue;
            }
            const std::string id = subshape.value("id", "");
            if (!id.empty()) {
                edgeSubshapeById[id] = &subshape;
            }
        }
    }

    nlohmann::json edgeSegments = nlohmann::json::array();
    const auto edgeSegmentsIt = mesh.find("edgeSegments");
    if (edgeSegmentsIt != mesh.end() && edgeSegmentsIt->is_array()) {
        for (const auto& segment : *edgeSegmentsIt) {
            if (!segment.is_object()) {
                continue;
            }
            const std::string id = segment.value("id", "");
            const std::string indexed = segment.value("indexed", id);
            const auto pointsIt = segment.find("points");
            if (id.empty() || indexed.empty() || pointsIt == segment.end()
                || !pointsIt->is_array()) {
                continue;
            }
            nlohmann::json responseSegment {
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"points", *pointsIt},
            };
            copyIdentityFields(responseSegment, segment);
            const auto subshapeIt = edgeSubshapeById.find(responseSegment["id"].get<std::string>());
            if (subshapeIt != edgeSubshapeById.end()) {
                copyIdentityFields(responseSegment, *subshapeIt->second);
            }
            edgeSegments.push_back(std::move(responseSegment));
        }
    }

    nlohmann::json vertexPoints = nlohmann::json::array();
    const auto vertexPointsIt = mesh.find("vertexPoints");
    if (vertexPointsIt != mesh.end() && vertexPointsIt->is_array()) {
        for (const auto& point : *vertexPointsIt) {
            if (!point.is_object()) {
                continue;
            }
            const std::string id = point.value("id", "");
            const std::string indexed = point.value("indexed", id);
            const auto pointIt = point.find("point");
            if (id.empty() || indexed.empty() || pointIt == point.end() || !pointIt->is_array()) {
                continue;
            }
            vertexPoints.push_back({
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"point", *pointIt},
            });
        }
    }

    return {
        {"vertices", mesh.value("vertices", nlohmann::json::array())},
        {"normals", mesh.value("normals", nlohmann::json::array())},
        {"indices", indices},
        {"faceIds", faceIds},
        {"edgeSegments", edgeSegments},
        {"vertexPoints", vertexPoints},
    };
}

bool publishesStableEdgeIdentityForResponseContract(const std::string& objectName,
                                                    const ComputeContext& context)
{
    const auto documentIt = context.documentObjects.find(objectName);
    return documentIt != context.documentObjects.end() && documentIt->second != nullptr;
}

bool hasStableEdgeIdentityEvidence(const std::string& indexed,
                                   const std::string& stableSubname,
                                   const std::string& subname,
                                   const part::NamedShape* stableSource)
{
    if (!stableSubname.empty()) {
        return true;
    }
    return namedShapeHasCurrentElementEvidence(indexed, stableSource);
}

struct StableSubnamePublicationConflict {
    std::string kind;
    std::string stableSubname;
    std::vector<std::string> indexed;
};

void addDistinctIndexed(std::vector<std::string>& indexed, const std::string& name)
{
    if (name.empty() || std::find(indexed.begin(), indexed.end(), name) != indexed.end()) {
        return;
    }
    indexed.push_back(name);
}

std::string joinIndexedNames(const std::vector<std::string>& indexed)
{
    std::string result;
    for (const std::string& name : indexed) {
        if (!result.empty()) {
            result += ", ";
        }
        result += name;
    }
    return result;
}

std::vector<StableSubnamePublicationConflict> stableSubnamePublicationConflicts(
    const nlohmann::json& subshapes
)
{
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> byStable;
    if (!subshapes.is_array()) {
        return {};
    }
    for (const auto& subshape : subshapes) {
        if (!subshape.is_object()) {
            continue;
        }
        const std::string kind = subshape.value("kind", "");
        const std::string stableSubname = subshape.value("stableSubname", "");
        const std::string indexed = subshape.value("indexed", "");
        if (kind.empty() || stableSubname.empty() || indexed.empty()) {
            continue;
        }
        addDistinctIndexed(byStable[{kind, stableSubname}], indexed);
    }

    std::vector<StableSubnamePublicationConflict> conflicts;
    for (const auto& [key, indexed] : byStable) {
        if (indexed.size() <= 1U) {
            continue;
        }
        conflicts.push_back({key.first, key.second, indexed});
    }
    return conflicts;
}

bool appendStableSubnamePublicationDiagnostics(const std::string& objectName,
                                               const nlohmann::json& subshapes,
                                               std::vector<Diagnostic>& diagnostics)
{
    const std::vector<StableSubnamePublicationConflict> conflicts =
        stableSubnamePublicationConflicts(subshapes);
    for (const StableSubnamePublicationConflict& conflict : conflicts) {
        const std::string indexed = joinIndexedNames(conflict.indexed);
        addDiagnostic(
            diagnostics,
            "error",
            "duplicate_stable_subname",
            objectName + " publishes duplicate " + conflict.kind + " stableSubname "
                + conflict.stableSubname + " for " + indexed,
            objectName,
            {},
            "response",
            conflict.stableSubname,
            indexed
        );
    }
    return !conflicts.empty();
}

bool requiresStableSubnamePublicationDiagnostics(const std::string& objectName,
                                                 const ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(objectName);
    if (objectIt == context.documentObjects.end()
        || objectIt->second == nullptr
        || objectIt->second->typeId != "PartDesign::Body") {
        return false;
    }
    const auto tip = app::readLink(*objectIt->second, "Tip");
    if (!tip || tip->object.empty()) {
        return false;
    }
    const auto tipIt = context.documentObjects.find(tip->object);
    return tipIt != context.documentObjects.end()
        && tipIt->second != nullptr
        && tipIt->second->typeId == "PartDesign::Revolution";
}

nlohmann::json responseSubshapes(const std::string& objectName,
                                 const ComputeContext& context)
{
    nlohmann::json subshapes = nlohmann::json::array();
    const auto subshapeIt = context.subshapes.find(objectName);
    if (subshapeIt == context.subshapes.end() || !subshapeIt->second.is_object()) {
        return subshapes;
    }

    const part::NamedShape* namedShape = nullptr;
    const auto namedShapeIt = context.namedShapes.find(objectName);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = &namedShapeIt->second;
    }
    const ShapeValue* shapeValue = nullptr;
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt != context.shapes.end()) {
        shapeValue = &shapeIt->second;
    }
    const auto tipContext = bodyTipSubshapeResponseContext(objectName, context);
    const auto displayContext = bodyDisplayCompoundResponseContext(objectName, context, namedShape);
    const bool stableEdgeIdentityContract =
        publishesStableEdgeIdentityForResponseContract(objectName, context);

    for (const auto& [indexed, subshape] : subshapeIt->second.items()) {
        const bool internalIndexed = indexed.rfind("InternalFace", 0) == 0
            || indexed.rfind("InternalEdge", 0) == 0
            || indexed.rfind("InternalVertex", 0) == 0;
        const part::NamedShape* stableSource = namedShape;
        if (internalIndexed && shapeValue != nullptr && shapeValue->internalNamedShape) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::getInternalElementMap() maps InternalEdge/InternalVertex through InternalShape,
            // while the public Sketch Shape keeps its own EdgeN/VertexN namespace.
            stableSource = &*shapeValue->internalNamedShape;
        }
        if (!internalIndexed && isProducerOnlyMappedElement(indexed, namedShape)) {
            // The producer-owned scope distinguishes raw Sketch ledger evidence from a public
            // shape identity.  No runtime string or object-type heuristic is involved.
            stableSource = nullptr;
        }
        std::string stableSubname = stableSubnameFor(indexed, stableSource);
        if (indexed.rfind("InternalEdge", 0) == 0) {
            stableSubname = normalizedInternalEdgeStableSubname(objectName, indexed, stableSubname, context);
        }
        else if (stableSubname.empty()) {
            stableSubname = internalElementStableSubnameFor(objectName, indexed, context);
        }
        const std::string rawIdentityStatus = subshape.value("identityStatus", "");
        if (rawIdentityStatus == "stable") {
            const std::string sourceStableSubname = subshape.value("sourceStableSubname", "");
            if (!sourceStableSubname.empty()) {
                stableSubname = sourceStableSubname;
            }
        }
        else if (rawIdentityStatus == "stable_split_fragment") {
            stableSubname = subshape.value("stableSubname", "");
        }
        if (stableSubname.empty()) {
            stableSubname = bodyTipBodyLocalStableSubnameFor(indexed, tipContext, stableSource);
        }
        const std::string downgradedSourceStableSubname =
            bodyTipStableSubnameCanUseCurrentOwner(indexed, stableSubname, tipContext, stableSource)
            ? stableSubname
            : std::string {};
        bool bodyTipDisplayOnlySubname = false;
        if (tipContext
            && !bodyTipStableSubnameHasPublicationEvidence(indexed,
                                                           stableSubname,
                                                           stableSource,
                                                           rawIdentityStatus)) {
            stableSubname.clear();
            bodyTipDisplayOnlySubname = true;
        }
        stableSubname = bodyTipQualifiedStableSubname(objectName, indexed, stableSubname, tipContext, stableSource);
        if (displayContext && stableSubname == indexed && isPlainTopologicalElementName(stableSubname)) {
            stableSubname.clear();
        }
        if (!bodyDisplayCompoundStableSubnameHasChildEvidence(stableSubname, displayContext)) {
            stableSubname.clear();
        }
        stableSubname = bodyDisplayCompoundQualifiedStableSubname(indexed, stableSubname, displayContext);
        // The Part producer declares whether a source-backed mapped name is direct-response
        // evidence or only a request-local producer ledger.  This is a projection of that
        // scope, not a runtime decision based on a Sketch object name or `;SKT;` bytes.
        const auto mappedNamePublication = mappedNamePublicationFor(indexed, tipContext, namedShape);
        if (mappedNamePublication) {
            stableSubname = mappedNamePublication->canonicalMappedName;
        }
        const std::string subname = mappedNamePublication
            ? mappedNamePublication->resolvedIndexed
            : responseSubnameFor(indexed, stableSubname, namedShape, tipContext, displayContext);
        const std::string fullSubname = mappedNamePublication && !tipContext
            ? objectName + "." + indexed
            : responseFullSubnameFor(objectName, indexed, subname, tipContext);
        topo::SubshapeIdentityDecision identityDecision = topo::decideDisplayPublication({
            objectName,
            indexed,
            subname,
            fullSubname,
            stableSubname,
            rawIdentityStatus,
            stableSource,
            bodyTipDisplayOnlySubname,
        });
        stableSubname = identityDecision.stableSubname;
        std::string responseIdentityStatus = topo::identityStatusName(identityDecision.status);
        if (displayKind(subshape) == "Edge"
            && stableEdgeIdentityContract
            && !bodyTipDisplayOnlySubname
            && hasStableEdgeIdentityEvidence(indexed, stableSubname, subname, stableSource)) {
            responseIdentityStatus = "stable";
        }
        nlohmann::json responseSubshape {
            {"id", objectName + ":" + indexed},
            {"kind", displayKind(subshape)},
            {"indexed", indexed},
            {"subname", subname},
            {"stableSubname", stableSubname},
            {"ShadowSub", nlohmann::json::array()},
            {"ReferenceShadow", nlohmann::json::array()},
        };
        if (!fullSubname.empty()) {
            responseSubshape["fullSubname"] = fullSubname;
        }
        if (mappedNamePublication) {
            responseSubshape["rawFreecadMappedName"] = mappedNamePublication->rawMappedName;
            responseSubshape["canonicalFreecadMappedName"] = mappedNamePublication->canonicalMappedName;
            responseSubshape["resolvedIndexed"] = mappedNamePublication->resolvedIndexed;
        }
        for (const std::string& field :
             {"sourceStableSubname",
              "fragmentStableSubname",
              "sourceGeometryKind"}) {
            if (mappedNamePublication && field == "sourceStableSubname") {
                continue;
            }
            const auto fieldIt = subshape.find(field);
            if (fieldIt != subshape.end() && fieldIt->is_string()) {
                responseSubshape[field] = fieldIt->get<std::string>();
            }
        }
        if (!mappedNamePublication
            && !downgradedSourceStableSubname.empty()
            && responseSubshape.find("sourceStableSubname") == responseSubshape.end()) {
            responseSubshape["sourceStableSubname"] = downgradedSourceStableSubname;
        }
        if (!responseIdentityStatus.empty()) {
            responseSubshape["identityStatus"] = responseIdentityStatus;
        }
        const auto sourceGeometryIdIt = subshape.find("sourceGeometryId");
        if (sourceGeometryIdIt != subshape.end() && sourceGeometryIdIt->is_number_integer()) {
            responseSubshape["sourceGeometryId"] = sourceGeometryIdIt->get<long long>();
        }
        const auto sourceGeometryIndexIt = subshape.find("sourceGeometryIndex");
        if (sourceGeometryIndexIt != subshape.end() && sourceGeometryIndexIt->is_number_unsigned()) {
            responseSubshape["sourceGeometryIndex"] = sourceGeometryIndexIt->get<std::size_t>();
        }
        subshapes.push_back(std::move(responseSubshape));
    }
    return subshapes;
}

void appendDatumFrameResultFields(nlohmann::json& result,
                                  const std::string& objectName,
                                  const ComputeContext& context)
{
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return;
    }

    const nlohmann::json& objectResult = objectIt->second;
    const auto datumIt = objectResult.find("datum");
    if (datumIt == objectResult.end() || !datumIt->is_string()) {
        return;
    }

    result["datum"] = *datumIt;
    nlohmann::json frame = nlohmann::json::object();
    for (const std::string& field : {"origin", "x_axis", "normal"}) {
        const auto fieldIt = objectResult.find(field);
        if (fieldIt != objectResult.end()) {
            frame[field] = *fieldIt;
        }
    }
    if (!frame.empty()) {
        result["frame"] = std::move(frame);
    }
}

nlohmann::json topologyCountsForResponseSubshapes(const nlohmann::json& subshapes)
{
    nlohmann::json counts = {
        {"faces", 0},
        {"edges", 0},
        {"vertices", 0},
    };
    if (!subshapes.is_array()) {
        return counts;
    }
    for (const auto& subshape : subshapes) {
        if (!subshape.is_object()) {
            continue;
        }
        const std::string kind = subshape.value("kind", "");
        if (kind == "Face") {
            counts["faces"] = counts["faces"].get<int>() + 1;
        }
        else if (kind == "Edge") {
            counts["edges"] = counts["edges"].get<int>() + 1;
        }
        else if (kind == "Vertex") {
            counts["vertices"] = counts["vertices"].get<int>() + 1;
        }
    }
    return counts;
}

int objectResultInt(const nlohmann::json& objectResult, const std::string& field)
{
    const auto fieldIt = objectResult.find(field);
    if (fieldIt != objectResult.end() && fieldIt->is_number_integer()) {
        return fieldIt->get<int>();
    }
    if (fieldIt != objectResult.end() && fieldIt->is_number_unsigned()) {
        return static_cast<int>(fieldIt->get<std::size_t>());
    }
    return 0;
}

nlohmann::json sketchExternalFlagCounts(const nlohmann::json& objectResult)
{
    static const std::vector<std::pair<std::string, std::string>> kFlagFields {
        {"Defining", "defining"},
        {"Detached", "detached"},
        {"Frozen", "frozen"},
        {"Missing", "missing"},
        {"Sync", "sync"},
    };

    const auto stateCountsIt = objectResult.find("external_geometry_state_counts");
    const nlohmann::json* stateCounts =
        stateCountsIt != objectResult.end() && stateCountsIt->is_object() ? &*stateCountsIt
                                                                          : nullptr;
    nlohmann::json counts = nlohmann::json::object();
    for (const auto& [publicName, internalName] : kFlagFields) {
        int count = 0;
        if (stateCounts != nullptr) {
            const auto countIt = stateCounts->find(internalName);
            if (countIt != stateCounts->end() && countIt->is_number_integer()) {
                count = countIt->get<int>();
            }
            else if (countIt != stateCounts->end() && countIt->is_number_unsigned()) {
                count = static_cast<int>(countIt->get<std::size_t>());
            }
        }
        counts[publicName] = count;
    }
    return counts;
}

nlohmann::json emptySketchExternalFlags(const nlohmann::json& objectResult)
{
    const int externalGeometryCount = objectResultInt(objectResult, "external_geometry_count");
    nlohmann::json flags = nlohmann::json::array();
    for (int index = 0; index < externalGeometryCount; ++index) {
        flags.push_back(nlohmann::json::array());
    }
    return flags;
}

void appendSketchSummaryResultFields(nlohmann::json& result, const nlohmann::json& objectResult)
{
    if (objectResult.value("shape", "") != "occt_sketch_shape") {
        return;
    }

    const int rawEdgeCount = objectResultInt(objectResult, "raw_edge_count");
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::getExternalGeometryCount(), "::getExternalGeometry()" and
    // "InternalShape" expose the public Sketch summary collected by native expected.
    result["object_fields"] = {
        {"external_curve_count", objectResultInt(objectResult, "external_curve_count")},
        {"external_geometry_count", objectResultInt(objectResult, "external_geometry_count")},
        {"external_point_count", objectResultInt(objectResult, "external_point_count")},
        {"raw_edge_count", rawEdgeCount},
        {"shape", "occt_sketch_shape"},
        {"status", objectResult.value("status", "ok")},
    };
    result["sketch_external"] = {
        {"construction_count", objectResultInt(objectResult, "external_construction_count")},
        {"flag_counts", sketchExternalFlagCounts(objectResult)},
        {"flags", emptySketchExternalFlags(objectResult)},
    };
    const std::string internalShape = objectResult.value("internal_shape", "none");
    result["sketch_internal"] = {
        {"profile_ready", objectResult.value("profile_ready", false)},
        {"raw_edge_count", rawEdgeCount},
        {"shape", internalShape},
    };
    if (internalShape == "occt_internal_shape") {
        result["sketch_internal"]["internal_counts"] = {
            {"edges", objectResultInt(objectResult, "internal_edge_count")},
            {"faces", objectResultInt(objectResult, "internal_face_count")},
            {"vertices", objectResultInt(objectResult, "internal_vertex_count")},
        };
    }
}

void appendObjectSummaryResultFields(nlohmann::json& result,
                                     const std::string& objectName,
                                     const ComputeContext& context,
                                     const nlohmann::json& subshapes)
{
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return;
    }

    const nlohmann::json& objectResult = objectIt->second;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
    // ::PropertyPartShape::setValue() stores the current TopoShape on the object; the
    // native expected collector publishes its bbox, Volume and subelement counts.
    const auto bboxIt = objectResult.find("bbox");
    if (bboxIt != objectResult.end()) {
        result["bbox"] = *bboxIt;
    }
    const auto volumeIt = objectResult.find("volume");
    if (volumeIt != objectResult.end()) {
        result["volume"] = *volumeIt;
    }
    if (objectResult.value("shape", "") != "occt_sketch_shape" && !subshapes.empty()) {
        result["topology_counts"] = topologyCountsForResponseSubshapes(subshapes);
    }
    appendSketchSummaryResultFields(result, objectResult);
}

void appendProducerPublicResultFields(nlohmann::json& result,
                                      const std::string& objectName,
                                      const ComputeContext& context)
{
    const auto fieldsIt = context.publicResultFields.find(objectName);
    if (fieldsIt == context.publicResultFields.end()) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
    // ::PropertyPartShape::setValue(const TopoShape&) publishes the producer's Shape while
    // PartFeatures.cpp::Loft::execute() / ::Sweep::execute() and helper wrappers own their
    // adjacent public fields. Runtime only merges that producer-owned carrier into the result.
    if (fieldsIt->second.objectFields) {
        result["object_fields"] = *fieldsIt->second.objectFields;
    }
    if (fieldsIt->second.shapeSummary) {
        result["shape_summary"] = *fieldsIt->second.shapeSummary;
    }
    if (fieldsIt->second.nativeError) {
        result["native_error"] = *fieldsIt->second.nativeError;
    }
    if (fieldsIt->second.nativeErrorCode) {
        result["native_error_code"] = *fieldsIt->second.nativeErrorCode;
    }
}

std::vector<std::string> responseTargets(const app::Document& document,
                                         const ComputeContext& context)
{
    std::vector<std::string> targets;
    std::set<std::string> seen;
    const auto append = [&](const std::string& name) {
        if (document.indexByName.count(name) != 0U && seen.insert(name).second) {
            targets.push_back(name);
        }
    };

    for (const std::string& requested : document.targets) {
        const auto objectIt = document.indexByName.find(requested);
        if (objectIt == document.indexByName.end()) {
            continue;
        }
        const app::DocumentObject& object = document.objects.at(objectIt->second);
        if (object.typeId == "PartDesign::Body") {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
            // FeatureDressUp.cpp::DressUp::execute() and FeatureTransformed.cpp::
            // Transformed::execute() each store their replacement Shape before
            // Body::execute() copies Tip.Shape into Body.Shape.  The public native response
            // therefore carries those already-computed replacement producers as well as the
            // requested Body.  Select only Group members whose own producer declared
            // body_mode=replace; this does not infer a shape or manufacture any topology.
            for (const app::Link& member : app::readLinks(object, "Group")) {
                const auto resultIt = context.objects.find(member.object);
                if (resultIt == context.objects.end() || !resultIt->second.is_object()
                    || resultIt->second.value("body_mode", "") != "replace") {
                    continue;
                }
                append(member.object);
            }
        }
        append(requested);
    }
    return targets;
}

std::vector<app::ElementMapProducerTrace::SidRef> traceSidRefs(const nlohmann::json& state)
{
    std::vector<app::ElementMapProducerTrace::SidRef> refs;
    const auto entries = state.find("entries");
    if (entries == state.end() || !entries->is_array()) {
        return refs;
    }
    for (const auto& entry : *entries) {
        const auto related = entry.find("related");
        if (related == entry.end() || !related->is_array()) {
            continue;
        }
        for (const auto& ref : *related) {
            refs.push_back({ref.value("value", 0L), ref.value("index", 0)});
        }
    }
    return refs;
}

std::vector<long> traceDefinedSids(const nlohmann::json& state)
{
    std::vector<long> values;
    const auto entries = state.find("entries");
    if (entries == state.end() || !entries->is_array()) {
        return values;
    }
    for (const auto& entry : *entries) {
        const long value = entry.value("value", 0L);
        if (value > 0) {
            values.push_back(value);
        }
    }
    return values;
}

std::string checkpointStringTable(ComputeContext& context, const std::string& label)
{
    const nlohmann::json state = context.stringHasher->inspectProducerTraceState();
    return context.producerTrace->checkpoint(
        {"stringTable", state, traceSidRefs(state), traceDefinedSids(state), {}, label}
    );
}

std::vector<app::ElementMapProducerTrace::ObjectInfo> traceObjects(
    const app::Document& document
)
{
    std::vector<app::ElementMapProducerTrace::ObjectInfo> objects;
    objects.reserve(document.objects.size());
    for (const app::DocumentObject& object : document.objects) {
        objects.push_back(
            {static_cast<long>(object.id), object.name, object.typeId}
        );
    }
    return objects;
}

nlohmann::json traceObjectSlots(const ComputeContext& context, const std::string& object)
{
    return {
        {"Shape", context.shapes.count(object) != 0U},
        {"NamedShape", context.namedShapes.count(object) != 0U},
        {"AddSubShape", context.addSubShapes.count(object) != 0U},
        {"mesh", context.mesh.count(object) != 0U},
        {"subshapes", context.subshapes.count(object) != 0U},
        {"objectResult", context.objects.count(object) != 0U},
    };
}

nlohmann::json traceInputSlots(const ComputeContext& context, const std::string& object)
{
    nlohmann::json inputs = nlohmann::json::array();
    const auto dependencies = context.dependencies.find(object);
    if (dependencies == context.dependencies.end()) {
        return inputs;
    }
    for (const std::string& dependency : dependencies->second) {
        inputs.push_back({
            {"object", dependency},
            {"slots", traceObjectSlots(context, dependency)},
        });
    }
    return inputs;
}

}  // namespace

ComputeContext recomputeContext(
    const app::Document& document,
    std::vector<Diagnostic> diagnostics,
    std::shared_ptr<app::ElementMapProducerTrace> producerTrace
)
{
    ComputeContext context(std::move(producerTrace));
    FeatureRegistry registry = buildDefaultRegistry();
    graph::RecomputePlan plan = graph::buildPlan(
        document,
        diagnostics,
        registry.producerMissingReferenceAdmissionTypeIds()
    );
    auto transaction = context.producerTrace->beginTransaction(
        {document.targets,
         {{"objectCount", document.objects.size()},
          {"hasTopoNamingState", document.hasTopoNamingState}},
         plan.order}
    );
    context.producerTrace->firstSeenIdentity(
        "hasher",
        "request.StringHasher",
        "create"
    );
    auto documentScope = context.producerTrace->scope(
        {"recompute", "", 0, "runtime::recompute", {{"targets", document.targets}}}
    );
    checkpointStringTable(context, "table_checkpoint");
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp::recompute()
    // starts the transaction with an empty document-state checkpoint before the first object's
    // producer begins; later producers naturally inherit the preceding ledger checkpoint.
    context.producerTrace->checkpoint(
        {"state", nlohmann::json::object(), {}, {}, {}, "transaction.initial_state"}
    );

    context.diagnostics = std::move(diagnostics);
    context.dependencies = plan.dependencies;
    context.documentObjects = buildDocumentObjectMap(document);
    context.parentGroupByObject = document.parentGroupByObject;
    context.targetObjects = std::set<std::string>(document.targets.begin(), document.targets.end());
    context.displayMeshDeflection = document.displayMeshDeflection.value_or(context.displayMeshDeflection);
    context.topoNamingState = document.topoNamingState;
    context.transformationTemplateObjects = findTransformationTemplateObjects(document);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
    // ::GeoFeature::getGlobalPlacement() returns parent GeoFeatureGroup::globalGroupPlacement()
    // multiplied by the object's own Placement.
    context.globalPlacements = buildGlobalPlacements(document);
    context.producerTrace->record({
        "document.recompute.plan",
        "planned",
        "dependency_plan_built",
        {{"order", plan.order},
         {"blockedObjects", plan.blockedObjects},
         {"targets", document.targets}},
    });

    bool transactionFailed = false;

    for (const auto& name : plan.order) {
        const auto& object = document.objects.at(document.indexByName.at(name));
        auto objectScope = context.producerTrace->scope(
            {"object.execute",
             object.name,
             static_cast<long>(object.id),
             object.typeId,
             {{"typeId", object.typeId}, {"objectId", object.id}}}
        );
        const long lastIdBefore = context.stringHasher->lastId();
        context.producerTrace->record({
            "document.object.execute.begin",
            "begin",
            "object_execute_started",
            {{"object", object.name},
             {"objectId", object.id},
             {"typeId", object.typeId},
             {"lastIdBefore", lastIdBefore},
             {"inputSlots", traceInputSlots(context, object.name)}},
        });
        if (plan.blockedObjects.count(name) != 0U) {
            context.objects[name] = {{"status", "error"}};
            context.producerTrace->record({
                "document.object.execute.end",
                "blocked",
                "dependency_plan_blocked_object",
                {{"lastIdBefore", lastIdBefore},
                 {"lastIdAfter", context.stringHasher->lastId()},
                 {"outputSlots", traceObjectSlots(context, object.name)}},
            });
            objectScope.abort("dependency_plan_blocked_object");
            transactionFailed = true;
            continue;
        }

        auto depIt = context.dependencies.find(name);
        if (depIt != context.dependencies.end()) {
            const auto failedIt = std::find_if(depIt->second.begin(), depIt->second.end(), [&](const std::string& dependency) {
                return hasFailed(context, dependency);
            });
            if (failedIt != depIt->second.end()) {
                context.objects[name] = {
                    {"status", "skipped"},
                    {"reason", "dependency " + *failedIt + " failed"},
                };
                context.producerTrace->record({
                    "document.object.execute.end",
                    "skipped",
                    "dependency_failed",
                    {{"dependency", *failedIt},
                     {"lastIdBefore", lastIdBefore},
                     {"lastIdAfter", context.stringHasher->lastId()},
                     {"outputSlots", traceObjectSlots(context, object.name)}},
                });
                objectScope.abort("dependency_failed");
                transactionFailed = true;
                continue;
            }
        }

        ReferenceResolutionView referenceView {
            context.shapes,
            context.objects,
            context.documentObjects,
            context.namedShapes,
            context.producerTrace.get(),
        };
        const ReferenceLifecycleView lifecycleView {context.documentObjects, &document};
        auto referenceValidation = validateObjectReferences(object, referenceView, lifecycleView);
        context.producerTrace->record({
            "reference.resolve",
            referenceValidation.valid ? "resolved" : "rejected",
            referenceValidation.valid ? "object_references_valid" : "object_reference_invalid",
            {{"propertyLocal", true},
             {"updateCount", referenceValidation.elementReferenceUpdates.size()},
             {"diagnosticCount", referenceValidation.diagnostics.size()}},
        });
        context.diagnostics.insert(context.diagnostics.end(),
                                   referenceValidation.diagnostics.begin(),
                                   referenceValidation.diagnostics.end());
        if (!referenceValidation.valid) {
            context.objects[object.name] = {{"status", "error"}};
            context.producerTrace->record({
                "document.object.execute.end",
                "rejected",
                "object_reference_invalid",
                {{"lastIdBefore", lastIdBefore},
                 {"lastIdAfter", context.stringHasher->lastId()},
                 {"outputSlots", traceObjectSlots(context, object.name)}},
            });
            objectScope.abort("object_reference_invalid");
            transactionFailed = true;
            continue;
        }
        for (auto& update : referenceValidation.elementReferenceUpdates) {
            context.elementReferenceUpdates.push_back(std::move(update));
        }
        appendReferenceMetadataUpdates(object, lifecycleView, context.elementReferenceUpdates);
        appendDocumentReferenceDiagnostics(object, lifecycleView, context.diagnostics);

        auto executor = registry.executorFor(object.typeId);
        if (executor == nullptr) {
            addDiagnostic(context.diagnostics, "error", "unsupported_type", "Unsupported TypeId " + object.typeId, object.name);
            context.objects[object.name] = {{"status", "error"}};
            context.producerTrace->record({
                "document.object.execute.end",
                "rejected",
                "unsupported_type",
                {{"typeId", object.typeId},
                 {"lastIdBefore", lastIdBefore},
                 {"lastIdAfter", context.stringHasher->lastId()},
                 {"outputSlots", traceObjectSlots(context, object.name)}},
            });
            objectScope.abort("unsupported_type");
            transactionFailed = true;
            continue;
        }
        bool executorException = false;
        try {
            executor(object, context);
        }
        catch (const Standard_Failure& failure) {
            executorException = true;
            markOcctExecutionFailure(object, context, standardFailureMessage(failure));
            context.producerTrace->record({
                "failure",
                "exception",
                "occt_standard_failure",
                {{"category", standardFailureTypeName(failure)},
                 {"message", standardFailureMessage(failure)},
                 {"partialWrite", true}},
            });
            objectScope.exception(standardFailureMessage(failure));
        }
        catch (const std::exception& failure) {
            executorException = true;
            const std::string message = std::string("std::exception: ") + failure.what();
            markOcctExecutionFailure(object, context, message);
            context.producerTrace->record({
                "failure",
                "exception",
                "executor_std_exception",
                {{"category", "std::exception"},
                 {"message", message},
                 {"partialWrite", true}},
            });
            objectScope.exception(message);
        }
        catch (...) {
            executorException = true;
            markOcctExecutionFailure(object, context, "unknown executor exception");
            context.producerTrace->record({
                "failure",
                "exception",
                "executor_unknown_exception",
                {{"category", "unknown"},
                 {"message", "unknown executor exception"},
                 {"partialWrite", true}},
            });
            objectScope.exception("unknown executor exception");
        }
        if (hasFailed(context, name)) {
            context.producerTrace->record({
                "document.object.execute.end",
                "failed",
                "executor_reported_failure",
                {{"lastIdBefore", lastIdBefore},
                 {"lastIdAfter", context.stringHasher->lastId()},
                 {"outputSlots", traceObjectSlots(context, object.name)}},
            });
            if (!executorException) {
                objectScope.abort("executor_reported_failure");
            }
            transactionFailed = true;
            continue;
        }
        registerIndexedNamedShape(name, context);
        context.executionOrder.push_back(name);
        checkpointStringTable(context, "table_checkpoint");
        context.producerTrace->record({
            "document.object.execute.end",
            "success",
            "object_execute_finished",
            {{"lastIdBefore", lastIdBefore},
             {"lastIdAfter", context.stringHasher->lastId()},
             {"hasNamedShape", context.namedShapes.count(name) != 0U},
             {"outputSlots", traceObjectSlots(context, object.name)}},
        });
    }

    checkpointStringTable(context, "maker.final_checkpoint");
    context.producerTrace->record({
        "document.recompute.checkpoint",
        transactionFailed ? "partial" : "complete",
        transactionFailed ? "request_completed_with_failures" : "request_completed",
        {{"executionOrder", context.executionOrder},
         {"diagnosticCount", context.diagnostics.size()},
         {"partialWrite", transactionFailed}},
    });
    if (transactionFailed) {
        documentScope.abort("request_completed_with_failures");
        transaction.abort("request_completed_with_failures");
    }
    return context;
}

nlohmann::json recomputeResultJson(const app::Document& document,
                                   const ComputeContext& context)
{
    nlohmann::json results = nlohmann::json::array();
    std::map<std::string, nlohmann::json> responseSubshapesByObject;
    std::vector<Diagnostic> diagnostics = context.diagnostics;
    for (const std::string& target : responseTargets(document, context)) {
        if (document.indexByName.count(target) == 0U) {
            continue;
        }
        const auto meshIt = context.mesh.find(target);
        nlohmann::json subshapes = responseSubshapes(target, context);
        responseSubshapesByObject[target] = subshapes;
        if (requiresStableSubnamePublicationDiagnostics(target, context)
            && appendStableSubnamePublicationDiagnostics(target, subshapes, diagnostics)) {
            continue;
        }
        nlohmann::json result = {
            {"object", target},
            {"mesh",
             meshIt == context.mesh.end()
                 ? nlohmann::json(nullptr)
                 : responseMesh(target, meshIt->second, subshapes)},
            {"subshapes", subshapes},
        };
        appendDatumFrameResultFields(result, target, context);
        appendObjectSummaryResultFields(result, target, context, subshapes);
        appendProducerPublicResultFields(result, target, context);
        results.push_back(std::move(result));
    }

    nlohmann::json payload = {
        {"results", results},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", diagnosticsToJson(diagnostics)},
        {"binaryPayloads", nlohmann::json::array()},
        {"topoNamingState", topoNamingStateJson(document, context, responseSubshapesByObject)},
    };
    return payload;
}

RecomputeArtifacts recomputeArtifacts(const app::Document& document,
                                      std::vector<Diagnostic> diagnostics,
                                      RecomputeTraceMetadata metadata)
{
    auto recorder = std::make_shared<app::ElementMapProducerTrace>();
    ComputeContext context(recorder);
    nlohmann::json response;
    if (auto failure = topoNamingStateRequestFailureJson(document, diagnostics)) {
        response = *failure;
        context.diagnostics = diagnostics;
        {
            auto transaction = recorder->beginTransaction(
                {document.targets,
                 {{"objectCount", document.objects.size()},
                  {"hasTopoNamingState", document.hasTopoNamingState},
                  {"preflightRejected", true}}}
            );
            recorder->firstSeenIdentity("hasher", "request.StringHasher", "create");
            auto scope = recorder->scope(
                {"topo_state.preflight", "", 0, "runtime::recompute", nlohmann::json::object()}
            );
            checkpointStringTable(context, "table_checkpoint");
            recorder->record({
                "failure",
                "rejected",
                "topo_state_preflight_rejected",
                {{"stage", "topo_state.preflight"},
                 {"partialWrite", false},
                 {"diagnostics", response.value("diagnostics", nlohmann::json::array())}},
            });
            recorder->checkpoint(
                {"state",
                 {{"stage", "topo_state.preflight"},
                  {"outcome", "rejected"},
                  {"partialWrite", false}},
                 {},
                 {},
                 {},
                 "maker.final_checkpoint"}
            );
            scope.abort("topo_state_preflight_rejected");
            transaction.abort("topo_state_preflight_rejected");
        }
    }
    else {
        context = recomputeContext(document, std::move(diagnostics), recorder);
        response = recomputeResultJson(document, context);
    }

    app::ElementMapProducerTrace::ProducerMetadata producerMetadata;
    producerMetadata.name = "CADCore";
    producerMetadata.document = metadata.document;
    producerMetadata.build = metadata.build;
    producerMetadata.inputSha256 = metadata.inputSha256;
    producerMetadata.responseSha256 = app::ElementMapProducerTrace::canonicalSha256(response);
    nlohmann::json producerTrace = recorder->drain(producerMetadata, traceObjects(document));
    return {std::move(response), std::move(context), std::move(producerTrace)};
}

nlohmann::json recompute(const app::Document& document,
                         std::vector<Diagnostic> diagnostics)
{
    return recomputeArtifacts(document, std::move(diagnostics)).response;
}

}  // namespace cad_core::runtime
