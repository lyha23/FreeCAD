#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp
// ::PolarPattern::getTransformations(), rotates source features around "Axis" by "Angle"/"Offset" and "Occurrences".
void executePolarPattern(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
