#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute(),
// creates a subtractive PartDesign feature from "Profile", "Diameter", "DepthType" and "Depth".
void executeHole(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
