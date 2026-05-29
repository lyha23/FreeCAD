#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <array>

namespace cad_core::geometry {

gp_Trsf placementFromComponents(const std::array<double, 3>& base, const std::array<double, 4>& rotation);
TopoDS_Shape transformShape(const TopoDS_Shape& shape, const gp_Trsf& transform);

}  // namespace cad_core::geometry
