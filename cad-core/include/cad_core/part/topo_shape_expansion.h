#pragma once

// Part-layer TopoShapeExpansion import element-map facade.
#include "cad_core/part/topo_shape.h"

#include <string>

namespace cad_core::part
{

struct ImportElementMapSource
{
    std::string format;
    std::string fileName;
};

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp::TopoShape::read(),
// dispatches to "importStep", "importIges" and "importBrep"; ImportStep::execute() then stores
// the resulting TopoShape in PropertyPartShape "Shape". cad-core keeps the same request-local
// imported TopoDS_Shape and records object-qualified element aliases so LinkSub references can
// survive a recompute without persisting BREP.
NamedShape namedShapeForImportedShape(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const ImportElementMapSource& source
);

}  // namespace cad_core::part

namespace cad_core::topo {

using cad_core::part::ImportElementMapSource;
using cad_core::part::namedShapeForImportedShape;

}  // namespace cad_core::topo
