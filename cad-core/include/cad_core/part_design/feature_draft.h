#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
// reads "Base", "Angle", "NeutralPlane", "PullDirection" and "Reversed", then calls
// TopoShape::makeElementDraft() for selected FaceN subelements.
void executeDraft(const app::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::part_design

namespace cad_core::features {

using part_design::executeDraft;

}  // namespace cad_core::features
