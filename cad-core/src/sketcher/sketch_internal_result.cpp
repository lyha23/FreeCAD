#include "cad_core/sketcher/sketch_internal_result.h"

#include "cad_core/app/element_map.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include "sketch_object_operations.h"

#include <utility>

namespace cad_core::sketcher
{

namespace
{

std::optional<part::SketchInternalHistoryContext> sketchInternalHistoryContext(
    const std::optional<part::FaceMakerHistorySummary>& faceMakerHistory,
    const std::optional<part::WireJoinerBuildResult>& wireJoinerResult
)
{
    if (!faceMakerHistory && !wireJoinerResult) {
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
    // ::FaceMaker::postBuild(), consumes "MapperHistory(myPreSplitHistory)" and
    // "MapperMaker(mySplitter)" before SketchObject::getInternalElementMap() exposes the
    // request-local InternalShape. Store the FaceMakerBuildFace summary next to the InternalShape
    // NamedShape so topo consumers can see which maker-history stages backed generated/split/
    // deleted element history.
    part::SketchInternalHistoryContext context;
    if (faceMakerHistory) {
        context.sourceEdgeCount = faceMakerHistory->sourceEdgeCount;
        context.preSplitEdgeCount = faceMakerHistory->preSplitEdgeCount;
        context.splitterEdgeCount = faceMakerHistory->splitterEdgeCount;
        context.boundedFaceCount = faceMakerHistory->boundedFaceCount;
        context.preSplitHistory = faceMakerHistory->preSplitHistory;
        context.splitterHistory = faceMakerHistory->splitterHistory;
        for (const part::FaceMakerEdgeHistoryEvidence& entry : faceMakerHistory->edgeEvidence) {
            part::SketchInternalFaceMakerEdgeEvidence topoEntry;
            topoEntry.makerStage = entry.makerStage;
            topoEntry.relation = entry.relation;
            topoEntry.sourceEdgeIndex = entry.sourceEdgeIndex;
            topoEntry.targetEdgeIndex = entry.targetEdgeIndex;
            topoEntry.targetEdge = entry.targetEdge;
            topoEntry.preSplitHistory = entry.preSplitHistory;
            topoEntry.splitterHistory = entry.splitterHistory;
            context.faceMakerEdgeEvidence.push_back(std::move(topoEntry));
        }
        for (const part::FaceMakerBoundedFaceHistoryEvidence& entry :
             faceMakerHistory->boundedFaceEvidence) {
            part::SketchInternalFaceMakerBoundedFaceEvidence topoEntry;
            topoEntry.boundedFaceIndex = entry.boundedFaceIndex;
            topoEntry.face = entry.face;
            topoEntry.sourceEdgeIndices = entry.sourceEdgeIndices;
            topoEntry.outerBoundaryTargetEdgeIndices = entry.outerBoundaryTargetEdgeIndices;
            for (const part::FaceMakerBoundedFaceBoundaryEvidence& boundary : entry.outerBoundary) {
                part::SketchInternalFaceMakerBoundedFaceBoundaryEvidence topoBoundary;
                topoBoundary.sourceEdgeIndex = boundary.sourceEdgeIndex;
                topoBoundary.targetEdgeIndex = boundary.targetEdgeIndex;
                topoBoundary.makerStage = boundary.makerStage;
                topoBoundary.relation = boundary.relation;
                topoBoundary.targetEdge = boundary.targetEdge;
                topoEntry.outerBoundary.push_back(std::move(topoBoundary));
            }
            context.faceMakerBoundedFaceEvidence.push_back(std::move(topoEntry));
        }
    }

    if (wireJoinerResult) {
        const part::SketchInternalHistoryContext& wireJoinerHistory =
            wireJoinerResult->historyEvidence;
        context.wireJoinerSourceEdgeCount = wireJoinerHistory.wireJoinerSourceEdgeCount;
        context.wireJoinerSplitResultEdgeCount =
            wireJoinerHistory.wireJoinerSplitResultEdgeCount;
        context.wireJoinerHistoryEvents = wireJoinerHistory.wireJoinerHistoryEvents;
        context.wireJoinerOpenExportHistoryEntries =
            wireJoinerHistory.wireJoinerOpenExportHistoryEntries;
        context.wireJoinerModifiedSourceEdgeCount =
            wireJoinerHistory.wireJoinerModifiedSourceEdgeCount;
        context.wireJoinerModifiedHistoryCount = wireJoinerHistory.wireJoinerModifiedHistoryCount;
        context.wireJoinerGeneratedHistoryCount = wireJoinerHistory.wireJoinerGeneratedHistoryCount;
        context.wireJoinerDeletedHistoryCount = wireJoinerHistory.wireJoinerDeletedHistoryCount;
        context.wireJoinerSplitterHistory = wireJoinerHistory.wireJoinerSplitterHistory;
    }

    return context;
}


nlohmann::json faceMakerHistoryToJson(const part::FaceMakerHistorySummary& history)
{
    nlohmann::json edgeEvidence = nlohmann::json::array();
    for (const part::FaceMakerEdgeHistoryEvidence& entry : history.edgeEvidence) {
        edgeEvidence.push_back({
            {"maker_stage", entry.makerStage},
            {"relation", entry.relation},
            {"source_edge_index", entry.sourceEdgeIndex},
            {"target_edge_index", entry.targetEdgeIndex},
            {"pre_split_history", entry.preSplitHistory},
            {"splitter_history", entry.splitterHistory},
        });
    }

    nlohmann::json boundedFaceEvidence = nlohmann::json::array();
    for (const part::FaceMakerBoundedFaceHistoryEvidence& entry : history.boundedFaceEvidence) {
        nlohmann::json boundary = nlohmann::json::array();
        for (const part::FaceMakerBoundedFaceBoundaryEvidence& boundaryEntry : entry.outerBoundary) {
            boundary.push_back({
                {"source_edge_index", boundaryEntry.sourceEdgeIndex},
                {"target_edge_index", boundaryEntry.targetEdgeIndex},
                {"maker_stage", boundaryEntry.makerStage},
                {"relation", boundaryEntry.relation},
            });
        }
        boundedFaceEvidence.push_back({
            {"bounded_face_index", entry.boundedFaceIndex},
            {"source_edge_indices", entry.sourceEdgeIndices},
            {"outer_boundary_target_edge_indices", entry.outerBoundaryTargetEdgeIndices},
            {"outer_boundary", std::move(boundary)},
        });
    }

    return {
        {"source_edge_count", history.sourceEdgeCount},
        {"pre_split_edge_count", history.preSplitEdgeCount},
        {"splitter_edge_count", history.splitterEdgeCount},
        {"bounded_face_count", history.boundedFaceCount},
        {"pre_split_history", history.preSplitHistory},
        {"splitter_history", history.splitterHistory},
        {"profile_result_source", faceMakerRuntimeSourceName(history.profileResultSource)},
        {"internal_result_source", faceMakerRuntimeSourceName(history.internalResultSource)},
        {"topology_switch_used", history.topologySwitchUsed},
        {"edge_evidence", std::move(edgeEvidence)},
        {"bounded_face_evidence", std::move(boundedFaceEvidence)},
    };
}

}  // namespace

SketchInternalResult buildSketchInternalResult(const SketchInternalResultInput& input)
{
    SketchInternalResult result {
        runtime::ShapeValue {runtime::ShapeValue::Kind::Sketch, input.rawShape}
    };
    result.shapeValue.profileShape = input.profileShape;
    result.shapeValue.profileNormal = input.profileNormal;
    result.shapeValue.internalShape = input.internalShape;
    result.shapeValue.profileRequiresSubshapeSelection = input.profileRequiresSubshapeSelection;

    const bool hasNonEmptyInternalShape = input.internalShape && !input.internalShape->IsNull();
    if (hasNonEmptyInternalShape) {
        result.shapeValue.internalNamedShape = part::namedShapeForSketchInternalShape(
            input.objectName,
            input.rawShape,
            *input.internalShape,
            sketchInternalHistoryContext(input.faceMakerHistory, input.wireJoinerResult)
        );
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), writes auxiliary "InternalShape"; the web response
        // renders that request-local shape with InternalFace ids matching subshapes.
        result.mesh
            = part::meshForShape(*input.internalShape, "InternalFace", "InternalEdge", "InternalVertex");
    }

    const nlohmann::json internalSubshapes = hasNonEmptyInternalShape
        ? part::subshapeMapForShape(*input.internalShape, "Internal")
        : nlohmann::json::object();
    if (!input.rawShape.IsNull()) {
        result.subshapes = part::subshapeMapForShape(input.rawShape);
        if (hasNonEmptyInternalShape) {
            for (const auto& item : internalSubshapes.items()) {
                result.subshapes[item.key()] = item.value();
            }
        }
    }

    const nlohmann::json internalElementMap = hasNonEmptyInternalShape
        ? app::internalElementMapForSketch(input.rawShape, *input.internalShape)
        : nlohmann::json::object();

    result.objectFields = {
        {"profile", profileShapeLabel(input.profileShape)},
        {"profile_ready", input.profileShape.has_value()},
        {"internal_shape",
         input.internalShape ? (input.internalShape->IsNull() ? "empty" : "occt_internal_shape")
                             : "none"},
        {"internal_face_count", countSubshapesOfKind(internalSubshapes, "face")},
        {"internal_edge_count", countSubshapesOfKind(internalSubshapes, "edge")},
        {"internal_vertex_count", countSubshapesOfKind(internalSubshapes, "vertex")},
        {"internal_element_map", internalElementMap},
    };
    if (input.wireJoinerResult) {
        result.objectFields["wire_joiner_diagnostics"] = input.wireJoinerResult->diagnostics;
        result.objectFields["wire_joiner_ledger"] = input.wireJoinerResult->compatibilityLedger;
        result.objectFields["wire_joiner_history"]
            = "history_partial:edge_info_wire_info_split_done_exhaust";
        result.objectFields["wire_joiner_history_detail"] =
            input.wireJoinerResult->compatibilityHistoryDetail;
    }
    if (input.faceMakerHistory) {
        result.objectFields["facemaker_history"] = faceMakerHistoryToJson(*input.faceMakerHistory);
        result.objectFields["facemaker_history_status"] = "history_evidence:facemaker_buildface";
    }

    return result;
}

}  // namespace cad_core::sketcher
