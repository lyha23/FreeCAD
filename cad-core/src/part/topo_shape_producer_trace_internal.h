#pragma once

#include "cad_core/app/element_map_producer_trace.h"
#include "cad_core/app/string_hasher.h"

#include <string>
#include <vector>

namespace cad_core::part::producer_trace_detail
{

// Part-private value seam shared by the real U/L de-duplication paths and the focused probe.
// It carries no NamedShape, OCCT shape, or business pointer into the recorder.
struct DuplicateCandidateValue
{
    std::string parent;
    std::string child;
    std::size_t ordinal = 0;
    std::vector<app::StringId> entryLocalRefs;
};

struct DuplicateCandidateEvidence
{
    std::string rawMappedName;
    DuplicateCandidateValue kept;
    DuplicateCandidateValue suppressed;
};

void publishDuplicateCandidateSuppressed(
    app::ElementMapProducerTrace& trace,
    const std::string& slice,
    const std::string& reason,
    const DuplicateCandidateEvidence& evidence
);

}  // namespace cad_core::part::producer_trace_detail
