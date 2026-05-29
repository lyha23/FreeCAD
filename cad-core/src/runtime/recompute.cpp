#include "cad_core/runtime/recompute.h"

#include "cad_core/graph/recompute_plan.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/topo/named_shape.h"

#include <algorithm>
#include <set>

namespace cad_core::runtime {

namespace {

gp_Trsf objectPlacement(const document::DocumentObject& object)
{
    if (object.typeId == "App::Origin") {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Origin.cpp::Origin::Origin(),
        // "App::Origin is a LCS for which placement is fixed to identity"; parent group
        // placement is still applied by resolveGlobalPlacement().
        return gp_Trsf();
    }
    if (const auto placement = document::readPlacement(object, "Placement")) {
        return geometry::placementFromComponents(placement->base, placement->rotation);
    }
    return gp_Trsf();
}

gp_Trsf resolveGlobalPlacement(const document::Document& document,
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

std::map<std::string, gp_Trsf> buildGlobalPlacements(const document::Document& document)
{
    std::map<std::string, gp_Trsf> placements;
    std::set<std::string> visiting;
    for (const auto& object : document.objects) {
        resolveGlobalPlacement(document, object.name, placements, visiting);
    }
    return placements;
}

std::map<std::string, const document::DocumentObject*> buildDocumentObjectMap(const document::Document& document)
{
    std::map<std::string, const document::DocumentObject*> objects;
    for (const auto& object : document.objects) {
        objects[object.name] = &object;
    }
    return objects;
}

std::set<std::string> findTransformationTemplateObjects(const document::Document& document)
{
    std::set<std::string> templates;
    for (const auto& object : document.objects) {
        if (object.typeId != "PartDesign::MultiTransform") {
            continue;
        }
        for (const auto& link : document::readLinks(object, "Transformations")) {
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
        context.namedShapes[name] = topo::indexedNamedShapeForObject(name, shapeIt->second.shape);
        return;
    }
    const auto addSubIt = context.addSubShapes.find(name);
    if (addSubIt == context.addSubShapes.end()) {
        return;
    }
    if (addSubIt->second.addShape) {
        context.namedShapes[name] = topo::indexedNamedShapeForObject(name, *addSubIt->second.addShape);
    }
    else if (addSubIt->second.subShape) {
        context.namedShapes[name] = topo::indexedNamedShapeForObject(name, *addSubIt->second.subShape);
    }
}

}  // namespace

ComputeContext recomputeContext(const document::Document& document,
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

nlohmann::json recomputeResultJson(const document::Document& document,
                                   const ComputeContext& context)
{
    nlohmann::json objects = nlohmann::json::object();
    for (const auto& object : document.objects) {
        auto it = context.objects.find(object.name);
        objects[object.name] = it == context.objects.end() ? nlohmann::json{{"status", "pending"}} : it->second;
    }

    return {
        {"objects", objects},
        {"mesh", context.mesh},
        {"subshapes", context.subshapes},
        {"named_shapes", topo::namedShapesToJson(context.namedShapes)},
        {"diagnostics", diagnosticsToJson(context.diagnostics)},
    };
}

nlohmann::json recompute(const document::Document& document,
                         std::vector<Diagnostic> diagnostics)
{
    const ComputeContext context = recomputeContext(document, std::move(diagnostics));
    return recomputeResultJson(document, context);
}

}  // namespace cad_core::runtime
