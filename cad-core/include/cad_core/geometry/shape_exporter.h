#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <string>

namespace cad_core::geometry {

nlohmann::json bboxForShape(const TopoDS_Shape& shape);
double volumeForShape(const TopoDS_Shape& shape);
nlohmann::json meshForShape(const TopoDS_Shape& shape);
std::string kernelVersion();

}  // namespace cad_core::geometry

