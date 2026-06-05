#include "cad_core/sketcher/sketch_internal_builder.h"

#include "cad_core/part/face_maker.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <utility>

namespace cad_core::sketcher
{

namespace
{

TopoDS_Shape compoundShape(const TopoDS_Shape& first, const TopoDS_Shape& second)
{
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    return compound;
}

} // namespace

SketchInternalBuildResult buildSketchInternals(const SketchInternalBuildInput& input)
{
    SketchInternalBuildResult result;
    if (input.faceWires.empty()) {
        if (!input.openWires.empty() || !input.openEdges.empty()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::SketchObject::buildInternals(), catches "Part::FaceMaker: result shape is null."
            // for open-only sketches and returns an empty TopoShape as InternalShape.
            result.internalShape = TopoDS_Shape();
        }
        return result;
    }

    const part::FaceMakerBuildFaceResult faceResult =
        part::makeFacesFromClosedWiresAndSplitEdgesDetailed(input.faceWires, input.openEdges);
    if (!faceResult.shape || faceResult.shape->IsNull()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), catches FaceMakerBuildFace failures and leaves
        // InternalShape as an empty TopoShape while the raw Sketch Shape remains available.
        result.internalShape = TopoDS_Shape();
        result.faceMakerFailed = !input.openEdges.empty();
        return result;
    }

    result.profileShape = faceResult.shape;
    result.internalShape = faceResult.internalShape ? faceResult.internalShape : faceResult.shape;
    result.splitProducedBoundedFaces = faceResult.splitProducedBoundedFaces;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::buildInternals() publishes split InternalFace regions, but PartDesign can still use the
    // profile face directly when no open splitter creates multiple selectable bounded regions.
    result.requiresSubshapeSelection = !input.openEdges.empty() && faceResult.faceCount > 1U;
    result.faceMakerHistory = faceResult.historySummary;

    if (!input.faceWires.empty() || !input.openWires.empty()) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), "Append open wires (edges not part of any closed face)"
        // after FaceMakerBuildFace. The profile face used by Pad remains the bounded face result.
        part::WireJoiner joiner;
        joiner.setTightBound(true);
        joiner.setMergeEdges(true);
        for (const TopoDS_Edge& edge : input.sourceEdges) {
            joiner.addSourceEdge(edge);
        }
        for (const TopoDS_Wire& wire : input.faceWires) {
            joiner.addOpenWire(wire);
        }
        for (const TopoDS_Wire& wire : input.openWires) {
            joiner.addOpenWire(wire);
        }
        const std::optional<TopoDS_Shape>& wireJoinerFaceResult =
            faceResult.internalShape ? faceResult.internalShape : faceResult.shape;
        joiner.buildFinalEdgeOwnership(wireJoinerFaceResult ? &*wireJoinerFaceResult : nullptr,
                                       &input.faceWires,
                                       &input.openEdges,
                                       result.splitProducedBoundedFaces);
        result.wireJoinerLedger = joiner.ledgerSummary();
        result.wireJoinerHistory = joiner.historySummary();
        const auto openShape = joiner.getOpenWires("SKF", true);
        if (openShape && !openShape->IsNull()) {
            result.internalShape = compoundShape(*result.internalShape, *openShape);
        }
    }

    return result;
}

} // namespace cad_core::sketcher
