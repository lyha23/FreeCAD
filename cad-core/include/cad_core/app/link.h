#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::app {

void executeAppLink(const app::DocumentObject& object, runtime::ComputeContext& context);
void executeAppLinkElement(const app::DocumentObject& object, runtime::ComputeContext& context);
void executeAppLinkGroup(const app::DocumentObject& object, runtime::ComputeContext& context);
void executeDocumentObjectGroup(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::app
