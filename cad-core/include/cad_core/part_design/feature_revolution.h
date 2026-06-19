#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp
// ::Revolution::execute(), calls "executeRevolved(Part::RevolMode::FuseWithBase)".
void executeRevolution(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
