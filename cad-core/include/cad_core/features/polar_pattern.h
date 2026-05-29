#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp
// ::PolarPattern::getTransformations(), rotates source features around "Axis" by "Angle"/"Offset" and "Occurrences".
void executePolarPattern(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
