#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

struct BodyTopoShapeOptions {
    bool emitDocumentUpdates = true;
    bool applyBodyPlacement = true;
};

struct BodyTopoShapeResult {
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    bool usesPreciseBoundingBox = false;
    std::string stopFeature;
    std::optional<std::string> origin;
    std::vector<std::string> groupNames;
    std::vector<std::string> appliedAdditiveFeatures;
    std::vector<std::string> appliedSubtractiveFeatures;
    std::vector<std::string> appliedReplacementFeatures;
    std::vector<std::string> refinedFeatures;
    std::optional<std::string> directTipSubshapeOwner;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
// reads the Tip feature's "Shape" and writes "Shape.setValue(tipShape)".
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
// ::Revolved::setResult(), writes AddSubShape for the tool and "this->Shape.setValue(result)"
// after fusing/cutting with the BaseFeature. FreeCAD persists that cumulative feature Shape during
// document recompute; cad-core is stateless, so it reconstructs the same Body-at-feature state from
// Body.Group and FeatureAddSub outputs inside one request.
std::optional<BodyTopoShapeResult> getBodyTopoShapeAtFeature(const app::DocumentObject& body,
                                                             runtime::ComputeContext& context,
                                                             const std::string& featureName,
                                                             BodyTopoShapeOptions options = {});

}  // namespace cad_core::part_design
