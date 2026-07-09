#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::runtime {

// CAD Core protocol fixture executor:
// /Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py
// ::topo_state_mapper_history_probe_response() defines CadCore::TopoNamingStateProbe as a
// mapperHistory DTO probe because native FreeCAD Python does not expose these producer-history
// events directly.
void executeTopoNamingStateProbe(const app::DocumentObject& object, ComputeContext& context);

}  // namespace cad_core::runtime
