#include "cad_core/runtime/recompute.h"

#include "cad_core/base/placement.h"
#include "cad_core/graph/recompute_plan.h"
#include "cad_core/runtime/element_reference_update.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/reference_resolution.h"
#include "cad_core/part/topo_shape.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>

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

void setIfNotEmpty(nlohmann::json& value, const std::string& field, const std::string& item)
{
    if (!item.empty()) {
        value[field] = item;
    }
}

nlohmann::json documentReferenceToJson(const app::LinkDocumentRef& ref)
{
    nlohmann::json value = {
        {"method", "PropertyXLinkContainer.DocMap"},
    };
    setIfNotEmpty(value, "file", ref.file);
    setIfNotEmpty(value, "oldName", ref.name);
    setIfNotEmpty(value, "newName", ref.currentName);
    setIfNotEmpty(value, "oldLabel", ref.label);
    setIfNotEmpty(value, "newLabel", ref.currentLabel);
    setIfNotEmpty(value, "oldStamp", ref.stamp);
    setIfNotEmpty(value, "currentStamp", ref.currentStamp);
    setIfNotEmpty(value, "status", ref.status);
    setIfNotEmpty(value, "currentStatus", ref.currentStatus);
    if (ref.allowPartialExplicit) {
        value["allowPartial"] = ref.allowPartial;
    }
    return value;
}

bool documentReferenceRenameChanged(const app::LinkDocumentRef& ref)
{
    return (!ref.name.empty() && !ref.currentName.empty() && ref.name != ref.currentName)
        || (!ref.label.empty() && !ref.currentLabel.empty() && ref.label != ref.currentLabel);
}

bool documentReferenceStampChanged(const app::LinkDocumentRef& ref)
{
    return !ref.stamp.empty() && !ref.currentStamp.empty() && ref.stamp != ref.currentStamp;
}

bool hasStandaloneLabelReferenceRename(const app::Link& link)
{
    return !link.labelReferenceRenames.empty() && link.referenceShadows.empty();
}

bool hasStandaloneDocumentReferenceRename(const app::Link& link)
{
    return link.referenceShadows.empty() && link.documentRef
        && documentReferenceRenameChanged(*link.documentRef);
}

bool hasStandaloneReferenceMetadataUpdate(const app::Link& link)
{
    return hasStandaloneLabelReferenceRename(link) || hasStandaloneDocumentReferenceRename(link);
}

nlohmann::json referenceMetadataLinkUpdateJson(const app::Link& link)
{
    nlohmann::json item = {
        {"value", link.object},
        {"SubList", link.subnames},
    };
    if (!link.labelReferenceRenames.empty()) {
        item["labelReferenceRename"] = labelReferenceRenamesToJson(link.labelReferenceRenames);
    }
    if (link.documentRef && documentReferenceRenameChanged(*link.documentRef)) {
        item["documentReference"] = documentReferenceToJson(*link.documentRef);
    }
    if (link.stableSubnamesExplicit) {
        item["StableSubList"] = link.stableSubnames;
    }
    if (link.fullSubnamesExplicit) {
        item["FullSubList"] = link.fullSubnames;
    }
    if (!link.externalGeometryFlags.empty()) {
        item["ExternalFlags"] = externalGeometryFlagsToJson(link.externalGeometryFlags);
    }
    if (!link.shadowSubs.empty()) {
        item["ShadowSub"] = shadowSubsToJson(link.shadowSubs);
    }
    return item;
}

void appendReferenceMetadataUpdates(const app::DocumentObject& object,
                                    ComputeContext& context)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        if (propertyValue.kind == app::PropertyKind::LinkSub) {
            for (const auto& link : propertyValue.links) {
                if (!hasStandaloneReferenceMetadataUpdate(link)) {
                    continue;
                }
                nlohmann::json update = referenceMetadataLinkUpdateJson(link);
                update["object"] = object.name;
                update["property"] = propertyName;
                update["PropertyType"] = propertyValue.propertyType;
                context.elementReferenceUpdates.push_back(std::move(update));
            }
            continue;
        }
        if (propertyValue.kind != app::PropertyKind::LinkSubList) {
            continue;
        }

        bool changed = false;
        nlohmann::json subSet = nlohmann::json::array();
        for (const auto& link : propertyValue.links) {
            if (hasStandaloneReferenceMetadataUpdate(link)) {
                changed = true;
            }
            subSet.push_back(referenceMetadataLinkUpdateJson(link));
        }
        if (changed) {
            context.elementReferenceUpdates.push_back({
                {"object", object.name},
                {"property", propertyName},
                {"PropertyType", propertyValue.propertyType},
                {"SubSet", std::move(subSet)},
            });
        }
    }
}

void appendDocumentReferenceDiagnostics(const app::DocumentObject& object,
                                        ComputeContext& context)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        for (const auto& link : propertyValue.links) {
            if (!link.documentRef) {
                continue;
            }
            if (documentReferenceStampChanged(*link.documentRef)) {
                addDiagnostic(context.diagnostics,
                              "warning",
                              "document_hash_mismatch",
                              propertyName + " target " + link.object
                                  + " linked document stamp changed",
                              object.name,
                              propertyName,
                              "runtime",
                              link.object);
            }
        }
    }
}

void registerIndexedNamedShape(const std::string& name, ComputeContext& context)
{
    if (context.namedShapes.count(name) != 0U) {
        return;
    }
    const auto shapeIt = context.shapes.find(name);
    if (shapeIt != context.shapes.end()) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, shapeIt->second.shape);
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

std::string stableSubnameFor(const std::string& indexed,
                             const part::NamedShape* namedShape)
{
    const bool internalIndexed = indexed.rfind("InternalFace", 0) == 0
        || indexed.rfind("InternalEdge", 0) == 0
        || indexed.rfind("InternalVertex", 0) == 0;
    if (namedShape == nullptr) {
        return internalIndexed ? std::string{} : indexed;
    }

    std::string fallback;
    for (const auto& [stableSubname, currentSubname] : namedShape->elementMap) {
        if (currentSubname != indexed) {
            continue;
        }
        if (!stableSubnameKindMatchesIndexed(indexed, stableSubname)) {
            continue;
        }
        if (stableSubname != indexed) {
            return stableSubname;
        }
        fallback = stableSubname;
    }
    if (!fallback.empty()) {
        if (internalIndexed && fallback == indexed) {
            return {};
        }
        return fallback;
    }
    // Sketch Internal* names are request-local until the sketch InternalShape has a real
    // NamedShape/ElementMap. Do not synthesize a stable name from the current indexed name.
    return internalIndexed ? std::string{} : indexed;
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

std::string currentSubnameForStable(const std::string& indexed,
                                    const std::string& stableSubname)
{
    const std::size_t dot = stableSubname.rfind('.');
    if (dot == std::string::npos) {
        return indexed;
    }
    return stableSubname.substr(0, dot + 1) + indexed;
}

bool isPlainTopologicalElementName(const std::string& name)
{
    return localElementName(name) == name && topologicalElementKind(name).has_value();
}

struct BodyTipSubshapeResponseContext {
    std::string owner;
    bool stablePrefix = false;
};

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

std::string bodyTipQualifiedStableSubname(const std::string& objectName,
                                          const std::string& indexed,
                                          const std::string& stableSubname,
                                          const std::optional<BodyTipSubshapeResponseContext>& tipContext)
{
    if (!tipContext || stableSubname.empty()) {
        return stableSubname;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // "Shape.setValue(tipShape)" publishes the Tip feature shape as the Body display shape.
    // The response subname is always the current Tip child path. Stable names from replacement
    // Tips also need that Tip path so downstream PropertyLinkSub can safely peel it off.
    const std::string ownerPrefix = tipContext->owner + ".";
    const std::string bodyPrefix = objectName + ".";
    if (stableSubname.rfind(ownerPrefix, 0) == 0) {
        return stableSubname;
    }
    if (stableSubname.rfind(bodyPrefix, 0) == 0) {
        return ownerPrefix + indexed;
    }
    if (stableSubname == indexed && isPlainTopologicalElementName(stableSubname)) {
        return ownerPrefix + stableSubname;
    }
    if (tipContext->stablePrefix) {
        return ownerPrefix + stableSubname;
    }
    return stableSubname;
}

std::string responseSubnameFor(const std::string& indexed,
                               const std::string& stableSubname,
                               const std::optional<BodyTipSubshapeResponseContext>& tipContext)
{
    if (tipContext) {
        return tipContext->owner + "." + indexed;
    }
    return currentSubnameForStable(indexed, stableSubname);
}

nlohmann::json responseMesh(const std::string& objectName, const nlohmann::json& mesh)
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
            edgeSegments.push_back({
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"points", *pointsIt},
            });
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
        std::string stableSubname = stableSubnameFor(indexed, stableSource);
        if (stableSubname.empty()) {
            stableSubname = internalElementStableSubnameFor(objectName, indexed, context);
        }
        stableSubname = bodyTipQualifiedStableSubname(objectName, indexed, stableSubname, tipContext);
        const std::string subname = responseSubnameFor(indexed, stableSubname, tipContext);
        subshapes.push_back({
            {"id", objectName + ":" + indexed},
            {"kind", displayKind(subshape)},
            {"indexed", indexed},
            {"subname", subname},
            {"stableSubname", stableSubname},
        });
    }
    return subshapes;
}

}  // namespace

ComputeContext recomputeContext(const app::Document& document,
                                std::vector<Diagnostic> diagnostics)
{
    graph::RecomputePlan plan = graph::buildPlan(document, diagnostics);
    FeatureRegistry registry = buildDefaultRegistry();

    ComputeContext context;
    context.diagnostics = std::move(diagnostics);
    context.dependencies = plan.dependencies;
    context.documentObjects = buildDocumentObjectMap(document);
    context.parentGroupByObject = document.parentGroupByObject;
    context.transformationTemplateObjects = findTransformationTemplateObjects(document);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
    // ::GeoFeature::getGlobalPlacement() returns parent GeoFeatureGroup::globalGroupPlacement()
    // multiplied by the object's own Placement.
    context.globalPlacements = buildGlobalPlacements(document);

    for (const auto& name : plan.order) {
        const auto& object = document.objects.at(document.indexByName.at(name));
        if (plan.blockedObjects.count(name) != 0U) {
            context.objects[name] = {{"status", "error"}};
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
                continue;
            }
        }

        ReferenceResolutionView referenceView {
            context.shapes,
            context.objects,
            context.documentObjects,
            context.namedShapes,
        };
        auto referenceValidation = validateObjectReferences(object, referenceView);
        context.diagnostics.insert(context.diagnostics.end(),
                                   referenceValidation.diagnostics.begin(),
                                   referenceValidation.diagnostics.end());
        if (!referenceValidation.valid) {
            context.objects[object.name] = {{"status", "error"}};
            continue;
        }
        for (auto& update : referenceValidation.elementReferenceUpdates) {
            context.elementReferenceUpdates.push_back(std::move(update));
        }
        appendReferenceMetadataUpdates(object, context);
        appendDocumentReferenceDiagnostics(object, context);

        auto executor = registry.executorFor(object.typeId);
        if (executor == nullptr) {
            addDiagnostic(context.diagnostics, "error", "unsupported_type", "Unsupported TypeId " + object.typeId, object.name);
            context.objects[object.name] = {{"status", "error"}};
            continue;
        }
        executor(object, context);
        registerIndexedNamedShape(name, context);
        context.executionOrder.push_back(name);
    }

    return context;
}

nlohmann::json recomputeResultJson(const app::Document& document,
                                   const ComputeContext& context)
{
    nlohmann::json results = nlohmann::json::array();
    for (const std::string& target : document.targets) {
        if (document.indexByName.count(target) == 0U) {
            continue;
        }
        const auto meshIt = context.mesh.find(target);
        results.push_back({
            {"object", target},
            {"mesh", meshIt == context.mesh.end() ? nlohmann::json(nullptr) : responseMesh(target, meshIt->second)},
            {"subshapes", responseSubshapes(target, context)},
        });
    }

    return {
        {"results", results},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", diagnosticsToJson(context.diagnostics)},
        {"binaryPayloads", nlohmann::json::array()},
    };
}

nlohmann::json recompute(const app::Document& document,
                         std::vector<Diagnostic> diagnostics)
{
    const ComputeContext context = recomputeContext(document, std::move(diagnostics));
    return recomputeResultJson(document, context);
}

}  // namespace cad_core::runtime
