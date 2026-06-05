#pragma once

// Sketcher-layer SketchObject executor aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject*.cpp.
#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::sketcher {

void executeSketchObject(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::sketcher
