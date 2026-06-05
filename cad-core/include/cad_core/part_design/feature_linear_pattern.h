#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
// ::LinearPattern::getTransformations(), builds translations from "Direction", "Length"/"Offset" and "Occurrences".
void executeLinearPattern(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design

namespace cad_core::features {

using part_design::executeLinearPattern;

}  // namespace cad_core::features
