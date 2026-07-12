#include "cad_core/graph/recompute_plan.h"

#include "cad_core/runtime/reference_lifecycle.h"

#include <algorithm>
#include <deque>
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

bool isPartDesignBodyResultFeature(const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::Feature::Feature() says "ADD_PROPERTY(BaseFeature, (nullptr))", and
    // ::Feature::getBaseObject() reads "BaseFeature.getValue()" for the preceding result shape.
    // The complete PartDesign datum provider family instead declares
    // "PROPERTY_SOURCE(..., Part::Datum)" in the following absolute source authorities:
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp::Line,
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPlane.cpp::Plane,
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPoint.cpp::Point, and
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumCS.cpp::CoordinateSystem.
    // They are reference providers, not members of the BaseFeature solid-result chain.
    if (object.typeId.rfind("PartDesign::", 0U) != 0U || object.typeId == "PartDesign::Body") {
        return false;
    }
    return object.typeId != "PartDesign::Line" && object.typeId != "PartDesign::Plane"
        && object.typeId != "PartDesign::Point"
        && object.typeId != "PartDesign::CoordinateSystem";
}

void applyFreeCadTopologicalOrder(RecomputePlan& plan, const app::Document& document)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp
    // ::buildDependencyList() pushes each requested root, drains a deque, appends every OutList in
    // its existing order, and adds edges as "add_edge(objectMap[key], objectMap[obj], depList)".
    // ::Document::getDependencyList(..., DepSort) then calls
    // "boost::topological_sort(depList, std::front_inserter(make_order))" and publishes the reverse
    // iteration "make_order.rbegin()". The observable result is Boost DFS finish order: dependency
    // before dependent, with dependency-ready peers ordered by graph traversal rather than Object.ID.
    std::set<std::string> included(plan.order.begin(), plan.order.end());
    std::vector<std::string> vertices;
    std::map<std::string, std::size_t> vertexByName;
    std::deque<std::string> pending;
    const auto appendRoot = [&](const std::string& root) {
        pending.push_back(root);
        while (!pending.empty()) {
            std::string name = std::move(pending.front());
            pending.pop_front();
            if (included.count(name) == 0U || vertexByName.count(name) != 0U) {
                continue;
            }
            vertexByName[name] = vertices.size();
            vertices.push_back(name);
            const auto dependencies = plan.dependencies.find(name);
            if (dependencies != plan.dependencies.end()) {
                pending.insert(pending.end(), dependencies->second.begin(), dependencies->second.end());
            }
        }
    };

    for (const std::string& target : document.targets) {
        appendRoot(target);
    }
    // Transient helper plans can deliberately admit additional document objects after the request
    // targets. FreeCAD's full-document objectArray is in document order, so use that same root
    // order for any admitted object not reached from an explicit target.
    for (const app::DocumentObject& object : document.objects) {
        appendRoot(object.name);
    }
    for (const std::string& name : plan.order) {
        appendRoot(name);
    }

    std::vector<std::vector<std::size_t>> outEdges(vertices.size());
    for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
        const auto dependencies = plan.dependencies.find(vertices[vertex]);
        if (dependencies == plan.dependencies.end()) {
            continue;
        }
        for (const std::string& dependency : dependencies->second) {
            const auto target = vertexByName.find(dependency);
            if (target != vertexByName.end()) {
                outEdges[vertex].push_back(target->second);
            }
        }
    }

    std::vector<unsigned char> colors(vertices.size(), 0U);
    std::vector<std::string> ordered;
    ordered.reserve(plan.order.size());
    const auto finish = [&](const auto& self, std::size_t vertex) -> void {
        if (colors[vertex] == 2U) {
            return;
        }
        if (colors[vertex] == 1U) {
            // visitObject() already owns cycle diagnostics and blocked-object admission. Avoid
            // changing that contract here; finish the remaining vertices deterministically.
            return;
        }
        colors[vertex] = 1U;
        for (const std::size_t dependency : outEdges[vertex]) {
            self(self, dependency);
        }
        colors[vertex] = 2U;
        ordered.push_back(vertices[vertex]);
    };
    for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
        finish(finish, vertex);
    }

    // A back-edge can finish a member before the original recursive frame. Keep every admitted
    // object exactly once, preserving the existing visit order as the cycle-only fallback.
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
        if (isPartDesignBodyResultFeature(candidate)) {
            return candidate.name;
        }
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::Feature::getBaseObject() reads "BaseFeature.getValue()". For the first result feature in a
    // Body.Group, the stateless request carries that predecessor on Body.BaseFeature rather than on
    // the feature object itself, so it is the previous-feature dependency after the Group scan.
    if (const auto baseFeature = app::readLink(body, "BaseFeature");
        baseFeature && document.indexByName.count(baseFeature->object) != 0U) {
        return baseFeature->object;
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
    for (const app::Link* linkPtr : app::dependencyLinksInFreeCadOrder(object)) {
        const app::Link& link = *linkPtr;
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

    if (isPartDesignBodyResultFeature(object)) {
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

    applyFreeCadTopologicalOrder(plan, document);

    return plan;
}

}  // namespace cad_core::graph
