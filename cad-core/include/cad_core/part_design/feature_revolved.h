#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

#include <string>

namespace cad_core::part_design {

enum class RevolvedAddSubMode {
    Additive,
    Subtractive,
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
// ::Revolved::tryExecuteRevolved(), validates "Type", "Angle", "ReferenceAxis",
// "Midplane" and "Reversed" before calling TopoShape revolve/revolution helpers.
void executeRevolvedFeature(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            RevolvedAddSubMode mode,
                            const std::string& featureName);

}  // namespace cad_core::part_design
