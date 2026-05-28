#include "cad_core/runtime/compute_context.h"

namespace cad_core::runtime {

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

