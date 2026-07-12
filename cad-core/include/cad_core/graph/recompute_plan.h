#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/diagnostics.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cad_core::graph {

struct RecomputePlan {
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Document.cpp
    // ::Document::recompute() iterates getDependencyList(..., DepSort | options). `order` preserves
    // that dependency-first Boost DFS finish order and is the request's auditable effective target
    // sequence; callers must not re-sort it by Object.ID or object name.
    std::vector<std::string> order;
    std::map<std::string, std::vector<std::string>> dependencies;
    std::set<std::string> blockedObjects;
};

RecomputePlan buildPlan(
    const app::Document& document,
    std::vector<runtime::Diagnostic>& diagnostics,
    const std::set<std::string>& producerMissingReferenceAdmissionTypeIds = {}
);

}  // namespace cad_core::graph
