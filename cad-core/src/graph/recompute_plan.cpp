#include "cad_core/graph/recompute_plan.h"

#include "cad_core/runtime/reference_lifecycle.h"

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace cad_core::graph {

using runtime::addDiagnostic;

namespace {

std::map<std::string, const app::DocumentObject*> buildDocumentObjectMap(const app::Document& document)
{
    std::map<std::string, const app::DocumentObject*> objects;
    for (const auto& object : document.objects) {
        objects[object.name] = &object;
    }
    return objects;
}

bool isPartDesignPipe(const app::DocumentObject& object)
{
    return object.typeId == "PartDesign::AdditivePipe" || object.typeId == "PartDesign::SubtractivePipe";
}

bool isPartDesignBodyFeature(const app::DocumentObject& object)
{
    return object.typeId.rfind("PartDesign::", 0U) == 0U && object.typeId != "PartDesign::Body";
}

std::optional<std::string> previousPartDesignBodyFeature(const std::string& name,
                                                         const app::Document& document)
{
    const auto parentIt = document.parentGroupByObject.find(name);
    if (parentIt == document.parentGroupByObject.end()) {
        return std::nullopt;
    }
    const auto bodyIt = document.indexByName.find(parentIt->second);
    if (bodyIt == document.indexByName.end()) {
        return std::nullopt;
    }
    const app::DocumentObject& body = document.objects.at(bodyIt->second);
    if (body.typeId != "PartDesign::Body") {
        return std::nullopt;
    }
    const std::vector<app::Link> groupLinks = app::readLinks(body, "Group");
    const auto groupIt = std::find_if(groupLinks.begin(), groupLinks.end(), [&](const app::Link& link) {
        return link.object == name;
    });
    if (groupIt == groupLinks.end() || groupIt == groupLinks.begin()) {
        return std::nullopt;
    }
    for (auto it = groupIt; it != groupLinks.begin();) {
        --it;
        const auto objectIt = document.indexByName.find(it->object);
        if (objectIt == document.indexByName.end()) {
            continue;
        }
        const app::DocumentObject& candidate = document.objects.at(objectIt->second);
        if (isPartDesignBodyFeature(candidate)) {
            return candidate.name;
        }
    }
    return std::nullopt;
}

void visitObject(const std::string& name,
                 const app::Document& document,
                 const runtime::ReferenceLifecycleView& lifecycleView,
                 RecomputePlan& plan,
                 std::vector<runtime::Diagnostic>& diagnostics,
                 std::vector<std::string>& visiting,
                 std::set<std::string>& visited)
{
    if (visited.count(name) != 0U) {
        return;
    }

    const auto cycleIt = std::find(visiting.begin(), visiting.end(), name);
    if (cycleIt != visiting.end()) {
        std::vector<std::string> cycle(cycleIt, visiting.end());
        cycle.push_back(name);
        std::ostringstream message;
        message << "Cycle dependency:";
        for (const auto& item : cycle) {
            message << ' ' << item;
            plan.blockedObjects.insert(item);
        }
        addDiagnostic(diagnostics, "error", "cycle_dependency", message.str(), name, {}, "graph", name);
        return;
    }

    visiting.push_back(name);
    const auto& object = document.objects.at(document.indexByName.at(name));
    if (!object.invalidProperties.empty()) {
        plan.blockedObjects.insert(name);
        visiting.pop_back();
        visited.insert(name);
        if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
            plan.order.push_back(name);
        }
        return;
    }

    std::set<std::string> seenDependencies;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase
    // is the dependency-bearing property base for PropertyLink, PropertyLinkList, PropertyLinkSub
    // and PropertyLinkSubList. cad-core graph consumes document-normalized links only.
    for (const auto& link : object.dependencyLinks) {
        if (seenDependencies.count(link.object) != 0U) {
            continue;
        }
        seenDependencies.insert(link.object);
        app::PropertyValue fallbackProperty;
        fallbackProperty.name = link.property;
        const auto propertyIt = object.propertyValues.find(link.property);
        const auto& propertyValue = propertyIt == object.propertyValues.end()
            ? fallbackProperty
            : propertyIt->second;
        const auto lifecycle =
            runtime::classifyReferenceLifecycle(object, propertyValue, link, lifecycleView);
        if (lifecycle.action == runtime::ReferenceLifecycleAction::BlockRecompute) {
            if (lifecycle.diagnostic) {
                diagnostics.push_back(*lifecycle.diagnostic);
            }
            plan.blockedObjects.insert(name);
            continue;
        }
        if (!lifecycle.requiresGraphDependency) {
            continue;
        }
        plan.dependencies[name].push_back(link.object);
        visitObject(link.object, document, lifecycleView, plan, diagnostics, visiting, visited);
    }

    if (isPartDesignPipe(object)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
        // ::Pipe::execute(), "getBaseTopoShape()" supplies the same-Body base before
        // "Part::OpCodes::Fuse/Cut"; cad-core's stateless graph must therefore execute the
        // previous Body.Group solid feature before publishing the Pipe feature Shape.
        if (const auto previousFeature = previousPartDesignBodyFeature(name, document);
            previousFeature && seenDependencies.count(*previousFeature) == 0U) {
            seenDependencies.insert(*previousFeature);
            plan.dependencies[name].push_back(*previousFeature);
            visitObject(*previousFeature, document, lifecycleView, plan, diagnostics, visiting, visited);
        }
    }

    visiting.pop_back();
    visited.insert(name);
    if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
        plan.order.push_back(name);
    }
}

}  // namespace

RecomputePlan buildPlan(const app::Document& document, std::vector<runtime::Diagnostic>& diagnostics)
{
    RecomputePlan plan;
    std::set<std::string> visited;
    std::vector<std::string> visiting;
    const auto documentObjects = buildDocumentObjectMap(document);
    const runtime::ReferenceLifecycleView lifecycleView {documentObjects, &document};

    for (const auto& target : document.targets) {
        if (document.indexByName.count(target) == 0U) {
            addDiagnostic(diagnostics,
                          "error",
                          "missing_object",
                          "Recompute target " + target + " does not exist",
                          target,
                          {},
                          "graph",
                          target);
            continue;
        }
        visitObject(target, document, lifecycleView, plan, diagnostics, visiting, visited);
    }

    return plan;
}

}  // namespace cad_core::graph
