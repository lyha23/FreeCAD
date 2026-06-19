#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
// ::AdditivePipe::AdditivePipe() sets "addSubType = Additive"; Pipe::execute() builds
// a PipeShell tool from "Profile" and "Spine", writes AddSubShape, then Body applies fuse replay.
void executeAdditivePipe(const app::DocumentObject& object, runtime::ComputeContext& context);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
// ::SubtractivePipe::SubtractivePipe() sets "addSubType = Subtractive"; Pipe::execute()
// stores the same solidified PipeShell tool in AddSubShape for Body cut replay.
void executeSubtractivePipe(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
