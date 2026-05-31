#pragma once

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
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(const std::string& historyPrefix) const;

private:
    bool tightBound_ = false;
    bool mergeEdges_ = false;
    struct WireInfo {
        TopoDS_Wire wire;
        bool consumedByBoundedFace = false;
    };
    std::vector<WireInfo> openWires_;
};

}  // namespace cad_core::geometry
