#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

void executeDatumCoordinateSystem(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design

namespace cad_core::features {

using part_design::executeDatumCoordinateSystem;

}  // namespace cad_core::features
