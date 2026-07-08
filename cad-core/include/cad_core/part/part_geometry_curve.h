#pragma once

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part
{

void executePartGeometryCurve(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part
