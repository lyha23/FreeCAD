#include "cad_core/runtime/recompute.h"

#include "cad_core/graph/recompute_plan.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"

#include <algorithm>

namespace cad_core::runtime {

nlohmann::json recompute(const document::Document& document,
                         std::vector<Diagnostic> diagnostics)
{
    graph::RecomputePlan plan = graph::buildPlan(document, diagnostics);
    FeatureRegistry registry = buildDefaultRegistry();

    ComputeContext context;
    context.diagnostics = std::move(diagnostics);
    context.dependencies = plan.dependencies;

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
    }

    nlohmann::json objects = nlohmann::json::object();
    for (const auto& object : document.objects) {
        auto it = context.objects.find(object.name);
        objects[object.name] = it == context.objects.end() ? nlohmann::json{{"status", "pending"}} : it->second;
    }

    return {
        {"objects", objects},
        {"mesh", context.mesh},
        {"subshapes", context.subshapes},
        {"diagnostics", diagnosticsToJson(context.diagnostics)},
    };
}

}  // namespace cad_core::runtime
