#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
// ::ProfileBased::getProfileShape(), calls Part::Feature::getTopoShape(profile, subShapeOptions,
// sub.c_str()). FreeCAD cumulative PartDesign feature Shapes are already persisted by document
// recompute; cad-core reconstructs that Body-local state only when the request-local direct feature
// shape does not own the referenced FaceN.
std::optional<TopoDS_Shape> resolveLinkedFaceProfile(const app::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const app::Link& profileLink,
                                                     const runtime::ShapeValue& shapeValue,
                                                     const std::string& featureName);

}  // namespace cad_core::part_design
