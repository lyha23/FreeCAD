#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features
{

void executePartFuse(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartCut(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartCommon(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartSection(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartMultiFuse(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartMultiCommon(const document::DocumentObject& object, runtime::ComputeContext& context);
void executePartXor(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
