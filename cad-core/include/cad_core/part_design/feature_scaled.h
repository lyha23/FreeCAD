#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp
// ::Scaled::getTransformations(), scales original AddSubShape copies around the first original's centre of mass.
void executeScaled(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
