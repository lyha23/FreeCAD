#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::geometry {

class WireJoiner {
public:
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setTightBound(), SketchObject::buildInternals() enables tight bounds before
    // getOpenWires().
    void setTightBound(bool enabled);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setMergeEdges(), SketchObject::buildInternals() enables merge before
    // getOpenWires().
    void setMergeEdges(bool enabled);
    void addOpenWire(const TopoDS_Wire& wire);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports openWireCompound only from edges with no final WireInfo
    // ownership. This subset uses the already-built bounded face boundary as the ownership
    // evidence: fragments matching bounded-face edges are consumed; leftover fragments stay open.
    void classifyBoundedFaceOwnership(const TopoDS_Shape& boundedFaceShape);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), when noOriginal=true, builds a source compound from
    // sourceEdgeArray and removes open-wire edges whose vertices are still shared with source.
    void addSourceEdge(const TopoDS_Edge& edge);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(const std::string& historyPrefix, bool noOriginal = true) const;

private:
    bool tightBound_ = false;
    bool mergeEdges_ = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo stores "iteration", "iteration2", "superEdge",
    // "wireInfo" and "wireInfo2" while splitEdges()/findTightBound()/exhaustTightBound()
    // move edges between WireInfo owners. The final openWireCompound uses edges with
    // "iteration == -3 || (!info.wireInfo && info.iteration >= 0)"; cad-core keeps that state
    // shape so later tight-bound migration can extend the same ledger.
    struct EdgeInfo {
        TopoDS_Edge edge;
        TopoDS_Wire superEdge;
        int iteration = 0;
        int iteration2 = 0;
        std::size_t wireInfo = 0;
        std::size_t wireInfo2 = 0;
        bool splitFromInputEdge = false;
    };
    struct WireInfo {
        std::size_t id = 0;
        TopoDS_Wire wire;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::WireInfo owns ordered EdgeInfo vertices for tight-bound wires.
        // cad-core still stores edge-level states rather than vertex pairs, but the owner id now
        // mirrors EdgeInfo::wireInfo / wireInfo2 instead of a separate output-side enum.
        std::vector<EdgeInfo> edges;
    };
    std::size_t nextWireInfoId_ = 1;
    std::vector<WireInfo> openWires_;
    std::vector<TopoDS_Edge> sourceEdges_;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::build(), copies result-wire EdgeInfo states into openWireCompound before
// SketchObject::buildInternals() compounds them with FaceMakerBuildFace output. This is the
// current result-wire graph subset until the full EdgeInfo/WireInfo history ledger is migrated.
std::optional<TopoDS_Shape> copiedResultWireGraphForSketchInternals(const TopoDS_Shape& boundedFaceShape,
                                                                    const std::vector<TopoDS_Edge>& openEdges,
                                                                    const std::vector<TopoDS_Wire>& closedWires,
                                                                    bool splitProducedBoundedFaces,
                                                                    bool hasOpenWireOutput);

}  // namespace cad_core::geometry
