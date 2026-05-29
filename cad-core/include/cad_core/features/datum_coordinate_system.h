#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

void executeDatumCoordinateSystem(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
