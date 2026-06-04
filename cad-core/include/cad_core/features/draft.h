#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
// reads "Base", "Angle", "NeutralPlane", "PullDirection" and "Reversed", then calls
// TopoShape::makeElementDraft() for selected FaceN subelements.
void executeDraft(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
