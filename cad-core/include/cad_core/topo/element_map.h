#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::topo {

struct InternalElementHistory
{
    std::string source;
    std::vector<std::string> targets;
};

struct InternalGeneratedElementHistory
{
    std::string target;
    std::vector<std::string> sources;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap(),
// iterates InternalShape vertices/edges and calls Shape.findSubShapesWithSharedVertex(...,
// CheckGeometry | SingleResult) to create bidirectional Internal* <-> raw element names.
nlohmann::json internalElementMapForSketch(const TopoDS_Shape& rawShape, const TopoDS_Shape& internalShape);

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::build(), writes split EdgeInfo states into openWireCompound, while
// SketchObject::getInternalElementMap() only maps one concrete Internal* name when
// findSubShapesWithSharedVertex(..., SingleResult) can prove it. This helper records the
// unresolved one-source-to-many InternalEdge relation as terminal split history.
std::vector<InternalElementHistory> internalSplitElementHistoryForSketch(const TopoDS_Shape& rawShape,
                                                                         const TopoDS_Shape& internalShape);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
// ::FaceMaker::postBuild(), "name the face using the edges of its outer wire". Sketch internals
// still cannot map raw FaceN because SketchObject::getInternalElementMap() iterates only vertices
// and edges, so this records InternalFaceN as generated from raw boundary EdgeN without adding a
// stable FaceN alias. Only the outer wire participates here; inner wires stay separate sources.
std::vector<InternalGeneratedElementHistory> internalGeneratedFaceHistoryForSketch(
    const TopoDS_Shape& rawShape,
    const TopoDS_Shape& internalShape
);
// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::getOpenWires(noOriginal=true), removes source open-wire edges that do not
// survive into InternalShape. This helper records raw EdgeN/VertexN entries with no exact or
// split Internal* target as terminal deleted history.
std::vector<std::string> internalDeletedElementHistoryForSketch(const TopoDS_Shape& rawShape,
                                                                const TopoDS_Shape& internalShape);

}  // namespace cad_core::topo
