#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

#include <set>
#include <string>

namespace cad_core::app {

// Shared FreeCAD App::LinkBaseExtension-style execution used by App::Link-derived
// request-local adapters such as Assembly::AssemblyLink.
void executeAppLinkBaseLike(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            const std::set<std::string>& allowedProperties,
                            const std::string& kind);

}  // namespace cad_core::app
