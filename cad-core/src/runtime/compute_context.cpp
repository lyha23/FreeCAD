#include "cad_core/runtime/compute_context.h"

namespace cad_core::runtime {

ComputeContext::ComputeContext(std::shared_ptr<app::ElementMapProducerTrace> trace)
    : producerTrace(trace ? std::move(trace) : std::make_shared<app::ElementMapProducerTrace>())
    , stringHasher(std::make_shared<app::StringHasher>())
{
    stringHasher->attachProducerTrace(producerTrace.get());
}

bool hasFailed(const ComputeContext& context, const std::string& object)
{
    const auto it = context.objects.find(object);
    if (it == context.objects.end()) {
        return false;
    }
    const std::string status = it->second.value("status", "");
    return status == "error" || status == "skipped";
}

}  // namespace cad_core::runtime
