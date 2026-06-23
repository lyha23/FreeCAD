#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

#include <optional>
#include <string>

namespace cad_core::part_design {

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
// ::ProfileBased::getTopoShapeVerifiedFace(), reads "Profile.getSubValues()" before resolving a
// selected subshape; ::ProfileBased::getProfileNormal() returns the profile normal used by
// FeatureExtrude and Revolved. cad-core returns a typed selection so executors consume shared
// ProfileBased profile semantics instead of re-parsing LinkSub/StableSubList/ReferenceShadow.
struct ProfileBasedProfileSelection {
    app::Link link;
    TopoDS_Shape shape;
    std::optional<gp_Dir> normal;
    std::string selectedSubname;
    std::string stableSubname;
    bool usedStableSubname = false;
    bool recoveredFromReferenceShadow = false;
    bool recoveredFromShadowSub = false;
    bool fromBodyCumulativeReplay = false;
};

std::optional<ProfileBasedProfileSelection> resolveProfileBasedProfile(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName,
    std::string profileRequirementMessage = {});

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
