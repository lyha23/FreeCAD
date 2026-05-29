#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

void executeAppLink(const document::DocumentObject& object, runtime::ComputeContext& context);
void executeAppLinkElement(const document::DocumentObject& object, runtime::ComputeContext& context);
void executeAppLinkGroup(const document::DocumentObject& object, runtime::ComputeContext& context);
void executeAssemblyObject(const document::DocumentObject& object, runtime::ComputeContext& context);
void executeAssemblyLink(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
