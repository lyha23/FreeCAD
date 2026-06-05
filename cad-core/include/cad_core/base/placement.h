#pragma once

// Base-layer placement helper aligned with FreeCAD Base/App Placement value
// semantics.
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <array>

namespace cad_core::base {

gp_Trsf placementFromComponents(const std::array<double, 3>& base, const std::array<double, 4>& rotation);
TopoDS_Shape transformShape(const TopoDS_Shape& shape, const gp_Trsf& transform);

}  // namespace cad_core::base
