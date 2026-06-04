#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

namespace cad_core::features {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp::Thickness::execute(),
// reads "Base", "Value", "Mode", "Join", "Reversed" and "Intersection", then calls
// TopoShape::makeElementThickSolid() for selected face subelements.
void executeThickness(const document::DocumentObject& object, runtime::ComputeContext& context);

}  // namespace cad_core::features
