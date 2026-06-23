#pragma once

#include "cad_core/part/face_maker.h"
#include "cad_core/part/wire_joiner.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <cstddef>
#include <optional>
#include <vector>

namespace cad_core::sketcher
{

struct SketchInternalBuildInput
{
    std::vector<TopoDS_Wire> faceWires;
    std::vector<TopoDS_Wire> openWires;
    std::vector<TopoDS_Edge> openEdges;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() copies sourceEdgeArray into sourceEdges before ::splitEdges() writes
    // "aHistory->AddModified(split.intersectShape, newInfo.edge)". Keep each wire edge's
    // sourceEdgeArray slot as explicit request-local ledger input instead of recovering it from
    // copied edge geometry in WireJoiner.
    std::vector<std::vector<std::size_t>> faceWireSourceEdgeIndices;
    std::vector<std::vector<std::size_t>> openWireSourceEdgeIndices;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), builds a source compound from sourceEdgeArray before
    // noOriginal filtering; cad-core keeps the same source-edge set for open-wire filtering.
    std::vector<TopoDS_Edge> sourceEdges;
};

struct SketchInternalBuildResult
{
    std::optional<TopoDS_Shape> profileShape;
    std::optional<TopoDS_Shape> internalShape;
    bool faceMakerFailed = false;
    bool splitProducedBoundedFaces = false;
    bool requiresSubshapeSelection = false;
    std::optional<part::FaceMakerHistorySummary> faceMakerHistory;
    std::optional<part::WireJoinerBuildResult> wireJoinerResult;
};

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals(), calls "Part::FaceMakerBuildFace", then
// WireJoiner::getOpenWires(), then makeElementCompound({result, openWires}).
SketchInternalBuildResult buildSketchInternals(const SketchInternalBuildInput& input);

}  // namespace cad_core::sketcher
