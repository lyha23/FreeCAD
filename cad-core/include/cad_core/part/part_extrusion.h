#pragma once

#include "cad_core/part/topo_shape.h"

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

#include <optional>
#include <string>

namespace cad_core::part
{

struct PartLinearExtrusionOptions
{
    gp_Dir direction {0.0, 0.0, 1.0};
    double lengthFwd = 0.0;
    double lengthRev = 0.0;
    double taperAngleFwdRadians = 0.0;
    double taperAngleRevRadians = 0.0;
    bool solid = false;
};

struct PartLinearExtrusionResult
{
    TopoDS_Shape shape;
    bool topoNamingKnownGap = false;
    bool taperHistory = false;
    std::optional<part::NamedShape> namedShape;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp
// ::Extrusion::extrudeShape(), when "Solid" is false, extrudes the linked wire/edge shape
// directly with makeElementPrism(); Pad/Pocket open-profile product semantics reuse that
// geometry helper without turning the user feature into a hidden Part::Extrusion object.
std::optional<PartLinearExtrusionResult> buildLinearExtrusionFromProfile(
    const std::string& owner,
    const std::string& sourceOwner,
    const TopoDS_Shape& profile,
    const PartLinearExtrusionOptions& options,
    const part::NamedShape* sourceNamedShape,
    std::string& error);

}  // namespace cad_core::part
