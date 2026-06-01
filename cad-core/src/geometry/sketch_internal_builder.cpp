#include "cad_core/geometry/sketch_internal_builder.h"

#include "cad_core/geometry/face_maker.h"
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <utility>

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
        if (!input.openWires.empty() || !input.openEdges.empty()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::SketchObject::buildInternals(), catches "Part::FaceMaker: result shape is null."
            // for open-only sketches and returns an empty TopoShape as InternalShape.
            result.internalShape = TopoDS_Shape();
        }
        return result;
    }

    const FaceMakerBuildFaceResult faceResult =
        makeFacesFromClosedWiresAndSplitEdgesDetailed(input.faceWires, input.openEdges);
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
    result.requiresSubshapeSelection = faceResult.splitProducedBoundedFaces;
    result.faceMakerHistory = faceResult.historySummary;

    bool hasOpenWireOutput = false;
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
        if (faceResult.shape) {
            joiner.classifyBoundedFaceOwnership(*faceResult.shape);
        }
        result.wireJoinerLedger = joiner.ledgerSummary();
        const auto openShape = joiner.getOpenWires("SKF", true);
        if (openShape && !openShape->IsNull()) {
            result.internalShape = compoundShape(*result.internalShape, *openShape);
            hasOpenWireOutput = true;
        }
    }

    if (const auto resultEdges = copiedResultWireGraphForSketchInternals(*faceResult.shape,
                                                                         input.openEdges,
                                                                         input.faceWires,
                                                                         faceResult.splitProducedBoundedFaces,
                                                                         hasOpenWireOutput)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports copied result-wire EdgeInfo states into openWireCompound;
        // SketchObject::buildInternals() then compounds them with FaceMakerBuildFace output. This
        // subset materializes that result-edge graph for fully consumed open cutters and closed-wire
        // split cycles until the full EdgeInfo/WireInfo history ledger is migrated.
        result.internalShape = compoundShape(*result.internalShape, *resultEdges);
    }

    return result;
}

}  // namespace cad_core::geometry
