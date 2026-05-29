#pragma once

#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::topo {

struct SubshapeName {
    TopAbs_ShapeEnum kind;
    int index = 0;
};

nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape);
std::optional<SubshapeName> parseSubshapeName(const std::string& name);
std::optional<TopoDS_Shape> subshapeByName(const TopoDS_Shape& shape, const std::string& name);
std::string subshapeKindName(TopAbs_ShapeEnum kind);

}  // namespace cad_core::topo
