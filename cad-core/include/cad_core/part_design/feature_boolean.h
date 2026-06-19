#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureBoolean.cpp
// ::Boolean::execute(), reads "Type", "Group" and "BaseFeature" before dispatching
// Fuse/Cut/Common through TopoShape::makeElementBoolean().
void executeBoolean(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design
