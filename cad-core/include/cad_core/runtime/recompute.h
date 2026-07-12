#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::runtime {

// FreeCAD authority:
// /Users/li/Chili3DProject/FreeCAD2/src/App/Document.cpp::Document::recompute() begins the trace
// transaction before topo sort and ::drainElementMapProducerTrace() freezes the independent
// artifact after recompute. These value types carry the same request/response binding without
// inserting trace data into the public response.
struct RecomputeTraceMetadata
{
    std::string document;
    std::string build = "cad-core";
    std::string inputSha256;
};

struct RecomputeArtifacts
{
    nlohmann::json response;
    ComputeContext context;
    nlohmann::json producerTrace;
};

ComputeContext recomputeContext(
    const app::Document& document,
    std::vector<Diagnostic> diagnostics,
    std::shared_ptr<app::ElementMapProducerTrace> producerTrace = {}
);
nlohmann::json recomputeResultJson(const app::Document& document,
                                   const ComputeContext& context);
RecomputeArtifacts recomputeArtifacts(
    const app::Document& document,
    std::vector<Diagnostic> diagnostics,
    RecomputeTraceMetadata metadata = {}
);
nlohmann::json recompute(const app::Document& document,
                         std::vector<Diagnostic> diagnostics);

}  // namespace cad_core::runtime
