#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace cad_core::topo {

nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape);

}  // namespace cad_core::topo

