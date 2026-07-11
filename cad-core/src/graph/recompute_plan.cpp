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

bool isTransientPartHelper(const app::DocumentObject& object)
{
    return object.typeId == "Part::FilledFace" || object.typeId == "Part::GeomPlateSurface";
}

bool isPartDesignBodyFeature(const app::DocumentObject& object)
{
    return object.typeId.rfind("PartDesign::", 0U) == 0U && object.typeId != "PartDesign::Body";
}

bool documentOrderLess(const app::Document& document,
                       const std::string& left,
                       const std::string& right)
{
    const auto leftIt = document.indexByName.find(left);
    const auto rightIt = document.indexByName.find(right);
    if (leftIt == document.indexByName.end() || rightIt == document.indexByName.end()) {
        return left < right;
    }
    const app::DocumentObject& leftObject = document.objects.at(leftIt->second);
    const app::DocumentObject& rightObject = document.objects.at(rightIt->second);
    if (leftObject.id != rightObject.id) {
        return leftObject.id < rightObject.id;
    }
    return leftIt->second < rightIt->second;
}

void stabilizePlanOrder(RecomputePlan& plan, const app::Document& document)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp::Document::recompute()
    // executes the dependency-ready document objects in document order. The document StringHasher
    // is shared, so a depth-first walk of Body.Group must not let an unrelated PocketSketch consume
    // StringIDs before an earlier-ID Tip branch that is already dependency-ready.
    std::set<std::string> included(plan.order.begin(), plan.order.end());
    std::map<std::string, std::size_t> pendingDependencies;
    std::map<std::string, std::vector<std::string>> dependents;
    for (const std::string& name : plan.order) {
        pendingDependencies[name] = 0U;
    }
    for (const auto& [name, dependencies] : plan.dependencies) {
        if (included.count(name) == 0U) {
            continue;
        }
        for (const std::string& dependency : dependencies) {
            if (included.count(dependency) == 0U) {
                continue;
            }
            ++pendingDependencies[name];
            dependents[dependency].push_back(name);
        }
    }

    std::vector<std::string> ready;
    for (const auto& [name, count] : pendingDependencies) {
        if (count == 0U) {
            ready.push_back(name);
        }
    }
    std::vector<std::string> ordered;
    ordered.reserve(plan.order.size());
    while (!ready.empty()) {
        const auto next = std::min_element(
            ready.begin(), ready.end(), [&](const std::string& left, const std::string& right) {
                return documentOrderLess(document, left, right);
            }
        );
        const std::string name = *next;
        ready.erase(next);
        ordered.push_back(name);
        for (const std::string& dependent : dependents[name]) {
            auto pending = pendingDependencies.find(dependent);
            if (pending == pendingDependencies.end() || pending->second == 0U) {
                continue;
            }
            --pending->second;
            if (pending->second == 0U) {
                ready.push_back(dependent);
            }
        }
    }

    // visitObject() already reports dependency cycles. Keep their members executable in stable
    // document order so diagnostics remain deterministic instead of silently dropping them.
    for (const std::string& name : plan.order) {
        if (std::find(ordered.begin(), ordered.end(), name) == ordered.end()) {
            ordered.push_back(name);
        }
    }
    plan.order = std::move(ordered);
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
                 const std::set<std::string>& producerMissingReferenceAdmissionTypeIds,
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
        if (lifecycle.state == runtime::ReferenceLifecycleState::MissingTarget
            && producerMissingReferenceAdmissionTypeIds.count(object.typeId) != 0U) {
            // Transient helpers registered with producer-owned reference admission must see the
            // unresolved link themselves so their native helper envelope is produced at the Part
            // seam. Graph records neither a dependency nor a generic DocumentObject diagnostic.
            continue;
        }
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
        visitObject(link.object,
                    document,
                    lifecycleView,
                    producerMissingReferenceAdmissionTypeIds,
                    plan,
                    diagnostics,
                    visiting,
                    visited);
    }

    if (isPartDesignBodyFeature(object)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp
        // ::FeatureAddSub::getBaseTopoShape() walks the preceding same-Body feature chain before
        // Pad/Pocket/Pipe/DressUp executes its own producer. A stateless recompute graph must
        // therefore depend on the preceding Body.Group PartDesign feature for every such
        // producer; Body::execute() itself only consumes its declared Tip Shape.
        if (const auto previousFeature = previousPartDesignBodyFeature(name, document);
            previousFeature && seenDependencies.count(*previousFeature) == 0U) {
            seenDependencies.insert(*previousFeature);
            plan.dependencies[name].push_back(*previousFeature);
            visitObject(*previousFeature,
                        document,
                        lifecycleView,
                        producerMissingReferenceAdmissionTypeIds,
                        plan,
                        diagnostics,
                        visiting,
                        visited);
        }
    }

    visiting.pop_back();
    visited.insert(name);
    if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
        plan.order.push_back(name);
    }
}

}  // namespace

RecomputePlan buildPlan(
    const app::Document& document,
    std::vector<runtime::Diagnostic>& diagnostics,
    const std::set<std::string>& producerMissingReferenceAdmissionTypeIds
)
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
        visitObject(target,
                    document,
                    lifecycleView,
                    producerMissingReferenceAdmissionTypeIds,
                    plan,
                    diagnostics,
                    visiting,
                    visited);
    }

    const bool collectsTransientPartHelpers = std::any_of(
        document.targets.begin(), document.targets.end(), [&](const std::string& target) {
            const auto it = document.indexByName.find(target);
            return it != document.indexByName.end()
                && isTransientPartHelper(document.objects.at(it->second));
        });
    if (collectsTransientPartHelpers) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp
        // ::Document::recompute() obtains the dependency-sorted document list and calls
        // _recomputeFeature() for every pending object. AppPartPy.cpp::makeFilledFace() and
        // GeomPlate/BuildPlateSurfacePyImp.cpp build transient result helpers only after that
        // source-document recompute. For this helper family, visit unreferenced non-helper source
        // objects as well, so their NamedShape/ElementMap enters the document topo snapshot without
        // changing the public result target set used by ordinary fixture requests.
        for (const auto& object : document.objects) {
            if (!isTransientPartHelper(object)) {
                visitObject(object.name,
                            document,
                            lifecycleView,
                            producerMissingReferenceAdmissionTypeIds,
                            plan,
                            diagnostics,
                            visiting,
                            visited);
            }
        }
    }

    stabilizePlanOrder(plan, document);

    return plan;
}

}  // namespace cad_core::graph
