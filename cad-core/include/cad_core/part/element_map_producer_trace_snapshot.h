#pragma once

#include "cad_core/app/element_map_producer_trace.h"
#include "cad_core/part/topo_shape.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::part
{

// FreeCAD authority:
// /Users/li/Chili3DProject/FreeCAD2/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeShapeWithElementMap() records "maker.begin" before setShape()/map consumption
// and calls checkpointTopoShape(..., "maker.final_checkpoint") before return. These CAD Core
// inspections mirror that value boundary with local traversal maps and never write the observed
// NamedShape, StringHasher, mapper history, OCCT shape, or cache.
nlohmann::json inspectShapeInventory(const TopoDS_Shape& shape);
nlohmann::json inspectNamedShapeLedger(const NamedShape& namedShape,
                                       const std::string& role = {});
nlohmann::json inspectMapperHistory(const std::vector<MapperHistoryEvent>& history);

std::string checkpointNamedShapeLedger(const NamedShape& namedShape,
                                       const std::string& role,
                                       const std::string& label = "maker.final_checkpoint",
                                       const std::string& relation = "create",
                                       const std::string& relatedIdentity = {});

}  // namespace cad_core::part
