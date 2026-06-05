#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute(),
// reads "Base", "ChamferType", "Size", "Size2", "Angle", "FlipDirection" and "UseAllEdges".
void executeChamfer(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design

namespace cad_core::features {

using part_design::executeChamfer;

}  // namespace cad_core::features
