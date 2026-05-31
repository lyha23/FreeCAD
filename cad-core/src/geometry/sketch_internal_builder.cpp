#include "cad_core/geometry/sketch_internal_builder.h"

#include "cad_core/geometry/face_maker.h"
#include "cad_core/geometry/wire_joiner.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

namespace cad_core::geometry {

namespace {

TopoDS_Shape compoundShape(const TopoDS_Shape& first, const TopoDS_Shape& second)
{
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    return compound;
}

}  // namespace

SketchInternalBuildResult buildSketchInternals(const SketchInternalBuildInput& input)
{
    SketchInternalBuildResult result;
    if (input.faceWires.empty()) {
        return result;
    }

    const FaceMakerBuildFaceResult faceResult =
        makeFacesFromClosedWiresAndSplitEdgesDetailed(input.faceWires, input.openEdges);
    if (!faceResult.shape || faceResult.shape->IsNull()) {
        result.faceMakerFailed = !input.openEdges.empty();
        return result;
    }

    result.profileShape = faceResult.shape;
    result.internalShape = faceResult.shape;
    result.splitProducedBoundedFaces = faceResult.splitProducedBoundedFaces;
    result.requiresSubshapeSelection = faceResult.splitProducedBoundedFaces;

    if (!input.openWires.empty()) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), "Append open wires (edges not part of any closed face)"
        // after FaceMakerBuildFace. The profile face used by Pad remains the bounded face result.
        WireJoiner joiner;
        joiner.setTightBound(true);
        joiner.setMergeEdges(true);
        for (const TopoDS_Edge& edge : input.sourceEdges) {
            joiner.addSourceEdge(edge);
        }
        for (const TopoDS_Wire& wire : input.openWires) {
            joiner.addOpenWire(wire);
        }
        if (faceResult.splitProducedBoundedFaces && faceResult.shape) {
            joiner.classifyBoundedFaceOwnership(*faceResult.shape);
        }
        const auto openShape = joiner.getOpenWires("SKF", true);
        if (openShape && !openShape->IsNull()) {
            result.internalShape = compoundShape(*faceResult.shape, *openShape);
        }
    }

    return result;
}

}  // namespace cad_core::geometry
