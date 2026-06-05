#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::assembly {

void executeAssemblyJointGroup(const app::DocumentObject& object, runtime::ComputeContext& context);
void executeAssemblyFeaturePython(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::assembly

namespace cad_core::features {

using assembly::executeAssemblyFeaturePython;
using assembly::executeAssemblyJointGroup;

}  // namespace cad_core::features
