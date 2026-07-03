#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// Chili/Onshape-style Thicken: selected face/surface Profile + thickness -> additive solid.
// This is intentionally separate from FreeCAD PartDesign::Thickness, which removes selected
// faces and builds a hollow shell.
void executeThicken(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
