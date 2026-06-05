#pragma once

// Part-layer PropertyTopoShape/subshape map helper aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp.
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::part {

struct SubshapeName {
    TopAbs_ShapeEnum kind;
    int index = 0;
};

nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getElementTypes(),
// exposes "InternalEdge", "InternalFace" and "InternalVertex" as Sketch element types.
nlohmann::json subshapeMapForShape(const TopoDS_Shape& shape, const std::string& prefix);
std::optional<SubshapeName> parseSubshapeName(const std::string& name);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::convertInternalName(),
// strips the "Internal" prefix before resolving against InternalShape.
std::optional<SubshapeName> parseInternalSubshapeName(const std::string& name);
std::optional<TopoDS_Shape> subshapeByName(const TopoDS_Shape& shape, const SubshapeName& name);
std::optional<TopoDS_Shape> subshapeByName(const TopoDS_Shape& shape, const std::string& name);
std::string subshapeKindName(TopAbs_ShapeEnum kind);

}  // namespace cad_core::part

namespace cad_core::topo {

using cad_core::part::SubshapeName;
using cad_core::part::parseInternalSubshapeName;
using cad_core::part::parseSubshapeName;
using cad_core::part::subshapeByName;
using cad_core::part::subshapeKindName;
using cad_core::part::subshapeMapForShape;

}  // namespace cad_core::topo
