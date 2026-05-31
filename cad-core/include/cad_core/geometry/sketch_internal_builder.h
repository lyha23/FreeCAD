#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <optional>
#include <vector>

namespace cad_core::geometry {

struct SketchInternalBuildInput {
    std::vector<TopoDS_Wire> faceWires;
    std::vector<TopoDS_Wire> openWires;
    std::vector<TopoDS_Edge> openEdges;
};

struct SketchInternalBuildResult {
    std::optional<TopoDS_Shape> profileShape;
    std::optional<TopoDS_Shape> internalShape;
    bool faceMakerFailed = false;
    bool splitProducedBoundedFaces = false;
    bool requiresSubshapeSelection = false;
};

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals(), calls "Part::FaceMakerBuildFace", then
// WireJoiner::getOpenWires(), then makeElementCompound({result, openWires}).
SketchInternalBuildResult buildSketchInternals(const SketchInternalBuildInput& input);

}  // namespace cad_core::geometry
