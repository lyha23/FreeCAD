#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace cad_core::app {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap(),
// iterates InternalShape vertices/edges and calls Shape.findSubShapesWithSharedVertex(...,
// CheckGeometry | SingleResult) to create bidirectional Internal* <-> raw element names.
nlohmann::json internalElementMapForSketch(const TopoDS_Shape& rawShape, const TopoDS_Shape& internalShape);

}  // namespace cad_core::app
