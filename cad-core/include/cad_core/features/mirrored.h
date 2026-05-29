#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp::Mirrored::getTransformations(),
// reads "MirrorPlane" and returns identity plus one gp_Trsf plane mirror transformation.
void executeMirrored(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
