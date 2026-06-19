#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLoft.cpp
// ::AdditiveLoft::AdditiveLoft() sets "addSubType = Additive"; Loft::execute() builds
// AddSubShape from Profile/Sections before Body applies the additive fuse replay.
void executeAdditiveLoft(const app::DocumentObject& object, runtime::ComputeContext& context);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLoft.cpp
// ::SubtractiveLoft::SubtractiveLoft() sets "addSubType = Subtractive"; Loft::execute()
// stores the same solidified loft tool in AddSubShape for Body cut replay.
void executeSubtractiveLoft(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
