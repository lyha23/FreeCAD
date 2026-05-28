#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

#include <set>
#include <string>

namespace cad_core::features {

using ExecuteFn = void (*)(const document::DocumentObject&, runtime::ComputeContext&);

bool rejectUnsupportedProperties(const document::DocumentObject& object,
                                 runtime::ComputeContext& context,
                                 const std::set<std::string>& allowed);

}  // namespace cad_core::features

