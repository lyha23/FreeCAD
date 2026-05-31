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
    // ::WireJoinerP::findClosedWires() and getOpenWires() use final EdgeInfo/WireInfo ownership
    // to omit edges consumed by bounded faces and keep leftover open fragments.
    // Temporary subset: until EdgeInfo/WireInfo is migrated, a wire is treated as consumed when
    // removing it lowers the bounded-face count. Delete this classifier when final ownership
    // directly drives getOpenWires().
    void classifyBoundedFaceOwnership(const std::vector<TopoDS_Wire>& faceWires, std::size_t fullFaceCount);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), when noOriginal=true, builds a source compound from
    // sourceEdgeArray and removes open-wire edges that still match original source edges.
    void addSourceEdge(const TopoDS_Edge& edge);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(const std::string& historyPrefix, bool noOriginal = true) const;

private:
    bool tightBound_ = false;
    bool mergeEdges_ = false;
    struct WireInfo {
        TopoDS_Wire wire;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges() replaces source EdgeInfo entries with split fragments before
        // build() exports openWireCompound from final EdgeInfo states.
        std::vector<TopoDS_Edge> edges;
        std::vector<bool> consumedByBoundedFace;
    };
    std::vector<WireInfo> openWires_;
    std::vector<TopoDS_Edge> sourceEdges_;
};

}  // namespace cad_core::geometry
