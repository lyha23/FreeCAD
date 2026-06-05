#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp
// ::MultiTransform::getTransformations(), combines child Transformed features by multiplication
// and uses a diagonal composition rule for Scaled children.
void executeMultiTransform(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
