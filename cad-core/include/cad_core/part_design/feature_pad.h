#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

void executePad(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
