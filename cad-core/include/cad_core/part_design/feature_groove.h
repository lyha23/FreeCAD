#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureGroove.cpp
// ::Groove::execute(), calls "executeRevolved(Part::RevolMode::CutFromBase)".
void executeGroove(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
