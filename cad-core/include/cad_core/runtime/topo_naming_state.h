#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>

namespace cad_core::runtime {

// FreeCAD:
// /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp, "addName(mappedName, element, ...)"
// stores durable mapped names; /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
// TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap(), calls "mapSubElement(shapes)"
// before mapper history; /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
// ::PropertyLinkBase::_updateElementReference(), resolves old references through ElementMap.
// cad-core publishes the request-local NamedShape/ElementMap ledger as a stateless response
// snapshot for the next recompute request.
nlohmann::json topoNamingStateJson(
    const app::Document& document,
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject
);

}  // namespace cad_core::runtime
