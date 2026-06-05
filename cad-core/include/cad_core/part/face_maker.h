#pragma once

// Part-layer FaceMaker implementation aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker*.cpp.
// The namespace remains cad_core::geometry during the compatibility phase.
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

enum class FaceMakerBuildFaceRuntimeSource
{
    None,
    BuilderFace,
    FaceWithHolesProfile,
};

struct FaceMakerEdgeHistoryEvidence
{
    std::string makerStage;
    std::string relation;
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    TopoDS_Edge targetEdge;
    bool preSplitHistory = false;
    bool splitterHistory = false;
};

struct FaceMakerBoundedFaceBoundaryEvidence
{
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    std::string makerStage;
    std::string relation;
    TopoDS_Edge targetEdge;
};

struct FaceMakerBoundedFaceHistoryEvidence
{
    std::size_t boundedFaceIndex = 0;
    TopoDS_Face face;
    std::vector<std::size_t> sourceEdgeIndices;
    std::vector<std::size_t> outerBoundaryTargetEdgeIndices;
    std::vector<FaceMakerBoundedFaceBoundaryEvidence> outerBoundary;
};

struct FaceMakerHistorySummary
{
    std::size_t sourceEdgeCount = 0;
    std::size_t preSplitEdgeCount = 0;
    std::size_t splitterEdgeCount = 0;
    std::size_t boundedFaceCount = 0;
    bool preSplitHistory = false;
    bool splitterHistory = false;
    FaceMakerBuildFaceRuntimeSource profileResultSource = FaceMakerBuildFaceRuntimeSource::None;
    FaceMakerBuildFaceRuntimeSource internalResultSource = FaceMakerBuildFaceRuntimeSource::None;
    bool topologySwitchUsed = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
    // ::FaceMaker::postBuild(), chains "MapperHistory(myPreSplitHistory)" and
    // "MapperMaker(mySplitter)" before naming the generated face from its outer wire edges.
    std::vector<FaceMakerEdgeHistoryEvidence> edgeEvidence;
    std::vector<FaceMakerBoundedFaceHistoryEvidence> boundedFaceEvidence;
};

struct FaceMakerBuildFaceResult
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), stores the FaceMakerBuildFace result as auxiliary
    // "InternalShape" while PartDesign Pad still builds its profile face from Sketch.Shape.
    // Closed-wire holes therefore keep a profile face with holes, but InternalShape can publish
    // the bounded face network returned by FaceMakerBuildFace.
    std::optional<TopoDS_Shape> shape;
    std::optional<TopoDS_Shape> internalShape;
    std::size_t faceCount = 0;
    bool splitProducedBoundedFaces = false;
    std::optional<FaceMakerHistorySummary> historySummary;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBullseye.cpp
// ::FaceDriller::addHole(), adds inner wires to the selected outer face; this is the current
// closed-wire subset and not the full FaceMakerBuildFace / WireJoiner split ledger.
std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
// ::FaceMakerSimple::Build_Essence(), "make plane faces from all closed wires" independently,
// with "No support for holes".
std::optional<TopoDS_Shape> makeSeparateFacesFromClosedWires(const std::vector<TopoDS_Wire>& wires);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerCheese.cpp
// ::FaceMakerCheese::makeFace(), supports planar "faces with holes, but no islands inside holes";
// Part::FaceMakerExtrusion delegates to this Cheese face builder after extracting wires.
std::optional<TopoDS_Shape> makeCheeseFaceFromClosedWires(const std::vector<TopoDS_Wire>& wires);
// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals() calls makeElementFace(..., "Part::FaceMakerBuildFace", ...)
// before WireJoiner open-wire handling. This helper covers the bounded-face split subset for
// closed profile wires plus on-face open splitter edges.
FaceMakerBuildFaceResult makeFacesFromClosedWiresAndSplitEdgesDetailed(
    const std::vector<TopoDS_Wire>& wires,
    const std::vector<TopoDS_Edge>& splitEdges
);
std::optional<TopoDS_Shape> makeFacesFromClosedWiresAndSplitEdges(
    const std::vector<TopoDS_Wire>& wires,
    const std::vector<TopoDS_Edge>& splitEdges
);

}  // namespace cad_core::part

namespace cad_core::geometry {

using namespace cad_core::part;

}  // namespace cad_core::geometry
