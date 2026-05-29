#include "cad_core/graph/recompute_plan.h"

#include <algorithm>
#include <sstream>

namespace cad_core::graph {

using runtime::addDiagnostic;

namespace {

void visitObject(const std::string& name,
                 const document::Document& document,
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
        if (document.indexByName.count(link.object) == 0U) {
            const std::string subname = link.subnames.empty() ? std::string{} : link.subnames.front();
            addDiagnostic(diagnostics,
                          "error",
                          "missing_link_target",
                          "Object links to missing object " + link.object,
                          name,
                          link.property,
                          "graph",
                          link.object,
                          subname);
            plan.blockedObjects.insert(name);
            continue;
        }
        plan.dependencies[name].push_back(link.object);
        visitObject(link.object, document, plan, diagnostics, visiting, visited);
    }

    visiting.pop_back();
    visited.insert(name);
    if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
        plan.order.push_back(name);
    }
}

}  // namespace

RecomputePlan buildPlan(const document::Document& document, std::vector<runtime::Diagnostic>& diagnostics)
{
    RecomputePlan plan;
    std::set<std::string> visited;
    std::vector<std::string> visiting;

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
        visitObject(target, document, plan, diagnostics, visiting, visited);
    }

    return plan;
}

}  // namespace cad_core::graph
