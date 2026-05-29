#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
// ::LinearPattern::getTransformations(), builds translations from "Direction", "Length"/"Offset" and "Occurrences".
void executeLinearPattern(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
