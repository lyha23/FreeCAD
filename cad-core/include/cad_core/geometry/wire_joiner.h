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
    struct WireInfo {
        TopoDS_Wire wire;
        enum class FragmentOwnership {
            Open,
            ConsumedByBoundedFace,
            RetainedResultFragment,
        };
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges() replaces source EdgeInfo entries with split fragments before
        // build() exports openWireCompound from final EdgeInfo states. cad-core keeps the current
        // compatibility subset as explicit fragment ownership instead of parallel output flags.
        struct EdgeFragment {
            TopoDS_Edge edge;
            bool splitFromInputEdge = false;
            FragmentOwnership ownership = FragmentOwnership::Open;
        };
        std::vector<EdgeFragment> fragments;
    };
    std::vector<WireInfo> openWires_;
    std::vector<TopoDS_Edge> sourceEdges_;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::build(), copies result-wire EdgeInfo states into openWireCompound before
// SketchObject::buildInternals() compounds them with FaceMakerBuildFace output. This is the
// current result-wire graph subset until the full EdgeInfo/WireInfo history ledger is migrated.
std::optional<TopoDS_Shape> copiedResultWireGraphForSketchInternals(const TopoDS_Shape& boundedFaceShape,
                                                                    const std::vector<TopoDS_Edge>& openEdges,
                                                                    std::size_t closedWireCount,
                                                                    std::size_t boundedFaceCount,
                                                                    bool splitProducedBoundedFaces,
                                                                    bool hasOpenWireOutput);

}  // namespace cad_core::geometry
