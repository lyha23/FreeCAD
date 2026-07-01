#include "cad_core/runtime/recompute.h"

#include "cad_core/base/placement.h"
#include "cad_core/graph/recompute_plan.h"
#include "cad_core/runtime/element_reference_update.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/reference_lifecycle.h"
#include "cad_core/runtime/reference_resolution.h"
#include "cad_core/part/topo_shape.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

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
    if (namedShape == nullptr) {
        return internalIndexed ? std::string{} : indexed;
    }

    std::string bestStableSubname;
    int bestPriority = -1;
    for (const auto& [stableSubname, currentSubname] : namedShape->elementMap) {
        if (currentSubname != indexed) {
            continue;
        }
        if (!stableSubnameKindMatchesIndexed(indexed, stableSubname)) {
            continue;
        }
        const int priority = stableSubnamePriority(indexed, stableSubname);
        if (priority > bestPriority) {
            bestStableSubname = stableSubname;
            bestPriority = priority;
        }
    }
    if (!bestStableSubname.empty()) {
        if (bestPriority == 0 || (internalIndexed && bestStableSubname == indexed)) {
            return {};
        }
        return bestStableSubname;
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
    if (!tipContext) {
        return stableSubname;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // "Shape.setValue(tipShape)" publishes the Tip feature shape as the Body display shape.
    // The response subname is always the current Tip child path. Stable names from replacement
    // Tips also need that Tip path so downstream PropertyLinkSub can safely peel it off.
    const std::string ownerPrefix = tipContext->owner + ".";
    if (stableSubname.empty()) {
        return ownerPrefix + indexed;
    }
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
                               const part::NamedShape* namedShape,
                               const std::optional<BodyTipSubshapeResponseContext>& tipContext)
{
    if (tipContext) {
        return tipContext->owner + "." + indexed;
    }
    const std::string displaySubname = displaySubnameFor(indexed, namedShape);
    if (!displaySubname.empty()) {
        return displaySubname;
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
            nlohmann::json responseSegment {
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"points", *pointsIt},
            };
            for (const std::string& field :
                 {"stableSubname", "sourceStableSubname", "sourceGeometryKind", "identityStatus"}) {
                const auto fieldIt = segment.find(field);
                if (fieldIt != segment.end() && fieldIt->is_string()) {
                    responseSegment[field] = fieldIt->get<std::string>();
                }
            }
            const auto sourceGeometryIdIt = segment.find("sourceGeometryId");
            if (sourceGeometryIdIt != segment.end() && sourceGeometryIdIt->is_number_integer()) {
                responseSegment["sourceGeometryId"] = sourceGeometryIdIt->get<long long>();
            }
            const auto sourceGeometryIndexIt = segment.find("sourceGeometryIndex");
            if (sourceGeometryIndexIt != segment.end() && sourceGeometryIndexIt->is_number_unsigned()) {
                responseSegment["sourceGeometryIndex"] = sourceGeometryIndexIt->get<std::size_t>();
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
        const std::string identityStatus = subshape.value("identityStatus", "");
        if (identityStatus == "stable") {
            const std::string sourceStableSubname = subshape.value("sourceStableSubname", "");
            if (!sourceStableSubname.empty()) {
                stableSubname = sourceStableSubname;
            }
        }
        else if (identityStatus == "index_fallback") {
            stableSubname.clear();
        }
        stableSubname = bodyTipQualifiedStableSubname(objectName, indexed, stableSubname, tipContext);
        const std::string subname = responseSubnameFor(indexed, stableSubname, namedShape, tipContext);
        nlohmann::json responseSubshape {
            {"id", objectName + ":" + indexed},
            {"kind", displayKind(subshape)},
            {"indexed", indexed},
            {"subname", subname},
            {"stableSubname", stableSubname},
        };
        for (const std::string& field :
             {"sourceStableSubname", "sourceGeometryKind", "identityStatus"}) {
            const auto fieldIt = subshape.find(field);
            if (fieldIt != subshape.end() && fieldIt->is_string()) {
                responseSubshape[field] = fieldIt->get<std::string>();
            }
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
        const ReferenceLifecycleView lifecycleView {context.documentObjects, &document};
        auto referenceValidation = validateObjectReferences(object, referenceView, lifecycleView);
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
        appendReferenceMetadataUpdates(object, lifecycleView, context.elementReferenceUpdates);
        appendDocumentReferenceDiagnostics(object, lifecycleView, context.diagnostics);

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
