#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <optional>
#include <vector>

namespace cad_core::geometry {

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBullseye.cpp
// ::FaceDriller::addHole(), adds inner wires to the selected outer face; this is the current
// closed-wire subset and not the full FaceMakerBuildFace / WireJoiner split ledger.
std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires);
// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals() calls makeElementFace(..., "Part::FaceMakerBuildFace", ...)
// before WireJoiner open-wire handling. This helper covers the bounded-face split subset for
// closed profile wires plus on-face open splitter edges.
std::optional<TopoDS_Shape> makeFacesFromClosedWiresAndSplitEdges(const std::vector<TopoDS_Wire>& wires,
                                                                  const std::vector<TopoDS_Edge>& splitEdges);

}  // namespace cad_core::geometry
