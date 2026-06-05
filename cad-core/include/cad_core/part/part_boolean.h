#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part
{

void executePartFuse(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartCut(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartCommon(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartSection(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartMultiFuse(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartMultiCommon(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartXor(const app::DocumentObject& object, runtime::ComputeContext& context);
void executePartBooleanFragments(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);

}  // namespace cad_core::part

namespace cad_core::features {

using part::executePartBooleanFragments;
using part::executePartCommon;
using part::executePartCut;
using part::executePartFuse;
using part::executePartMultiCommon;
using part::executePartMultiFuse;
using part::executePartSection;
using part::executePartXor;

}  // namespace cad_core::features
