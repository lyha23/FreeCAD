#include "cad_core/part/internal_shape_history_ledger.h"

#include "internal_shape_history_ledger_detail.h"

#include <utility>

namespace cad_core::part
{

namespace
{

const char* faceMakerRuntimeSourceName(FaceMakerBuildFaceRuntimeSource source)
{
    switch (source) {
        case FaceMakerBuildFaceRuntimeSource::BuilderFace:
            return "builder_face";
        case FaceMakerBuildFaceRuntimeSource::FaceWithHolesProfile:
            return "face_with_holes_profile";
        case FaceMakerBuildFaceRuntimeSource::None:
            break;
    }
    return "none";
}

InternalShapeHistoryRelation relationFromName(const std::string& relation)
{
    if (relation == "preserved") {
        return InternalShapeHistoryRelation::Preserved;
    }
    if (relation == "generated") {
        return InternalShapeHistoryRelation::Generated;
    }
    if (relation == "modified") {
        return InternalShapeHistoryRelation::Modified;
    }
    if (relation == "deleted") {
        return InternalShapeHistoryRelation::Deleted;
    }
    if (relation == "split") {
        return InternalShapeHistoryRelation::Split;
    }
    return InternalShapeHistoryRelation::DiagnosticOnly;
}

nlohmann::json faceMakerHistoryToJson(const FaceMakerHistorySummary& history)
{
    nlohmann::json edgeEvidence = nlohmann::json::array();
    for (const FaceMakerEdgeHistoryEvidence& entry : history.edgeEvidence) {
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
    for (const FaceMakerBoundedFaceHistoryEvidence& entry : history.boundedFaceEvidence) {
        nlohmann::json boundary = nlohmann::json::array();
        for (const FaceMakerBoundedFaceBoundaryEvidence& boundaryEntry : entry.outerBoundary) {
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

nlohmann::json sketchInternalHistoryContextToJson(const SketchInternalHistoryContext& history)
{
    nlohmann::json faceMakerEdgeEvidence = nlohmann::json::array();
    for (const SketchInternalFaceMakerEdgeEvidence& entry : history.faceMakerEdgeEvidence) {
        faceMakerEdgeEvidence.push_back({
            {"maker_stage", entry.makerStage},
            {"relation", entry.relation},
            {"source_edge_index", entry.sourceEdgeIndex},
            {"target_edge_index", entry.targetEdgeIndex},
            {"pre_split_history", entry.preSplitHistory},
            {"splitter_history", entry.splitterHistory},
        });
    }
    nlohmann::json faceMakerBoundedFaceEvidence = nlohmann::json::array();
    for (const SketchInternalFaceMakerBoundedFaceEvidence& entry :
         history.faceMakerBoundedFaceEvidence) {
        nlohmann::json boundary = nlohmann::json::array();
        for (const SketchInternalFaceMakerBoundedFaceBoundaryEvidence& boundaryEntry :
             entry.outerBoundary) {
            boundary.push_back({
                {"source_edge_index", boundaryEntry.sourceEdgeIndex},
                {"target_edge_index", boundaryEntry.targetEdgeIndex},
                {"maker_stage", boundaryEntry.makerStage},
                {"relation", boundaryEntry.relation},
            });
        }
        faceMakerBoundedFaceEvidence.push_back({
            {"bounded_face_index", entry.boundedFaceIndex},
            {"source_edge_indices", entry.sourceEdgeIndices},
            {"outer_boundary_target_edge_indices", entry.outerBoundaryTargetEdgeIndices},
            {"outer_boundary", std::move(boundary)},
        });
    }
    nlohmann::json wireJoinerHistoryEvents = nlohmann::json::array();
    std::size_t wireJoinerHistoryEventFromChildWireLedgerCount = 0;
    for (const SketchInternalWireJoinerHistoryEvent& event : history.wireJoinerHistoryEvents) {
        if (event.relationFromChildWireLedger) {
            ++wireJoinerHistoryEventFromChildWireLedgerCount;
        }
        wireJoinerHistoryEvents.push_back({
            {"event_index", event.eventIndex},
            {"open_export_index", event.openExportIndex},
            {"edge_info_index", event.edgeInfoIndex},
            {"open_wire_compound_child_wire_info_index",
             event.openWireCompoundChildWireInfoIndex},
            {"relation", event.relation},
            {"relation_from_child_wire_ledger", event.relationFromChildWireLedger},
            {"source_edge_indices", event.sourceEdgeIndices},
            {"source_lineage_from_splitter_history", event.sourceLineageFromSplitterHistory},
            {"no_original_purged_by_ledger", event.noOriginalPurgedByLedger},
            {"split_fragment_from_modified_history", event.splitFragmentFromModifiedHistory},
            {"split_fragment_from_generated_history", event.splitFragmentFromGeneratedHistory},
        });
    }
    nlohmann::json wireJoinerOpenExportEntries = nlohmann::json::array();
    for (const SketchInternalWireJoinerOpenExportHistoryEntry& entry :
         history.wireJoinerOpenExportHistoryEntries) {
        nlohmann::json currentMemberSplitOutputVertexDebt = nlohmann::json::array();
        for (const SketchInternalWireJoinerEndpointIdentityDebt& debt :
             entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt) {
            currentMemberSplitOutputVertexDebt.push_back({
                {"output_vertex_index", debt.outputVertexIndex},
                {"matched_member_split_ledger", debt.matchedMemberSplitLedger},
                {"matched_candidate_ledger", debt.matchedCandidateLedger},
                {"current_child_wire_output_vertex_matches_other_output",
                 debt.currentChildWireOutputVertexMatchesOtherOutput},
                {"candidate_wire_vertex_matches_other_output",
                 debt.candidateWireVertexMatchesOtherOutput},
                {"explanation", debt.explanation},
                {"current_child_wire_output_vertex_identity",
                 debt.currentChildWireOutputVertexIdentity},
                {"member_split_ledger_vertex_identity",
                 debt.memberSplitLedgerVertexIdentity},
                {"candidate_wire_vertex_identity", debt.candidateWireVertexIdentity},
                {"mismatch_reason", debt.mismatchReason},
            });
        }
        nlohmann::json vmapReplacementEvents = nlohmann::json::array();
        for (const SketchInternalWireJoinerVmapReplacementEvent& replacementEvent :
             entry.openWireCompoundVmapReplacementEvents) {
            vmapReplacementEvents.push_back({
                {"event_index", replacementEvent.eventIndex},
                {"affected_source_edge_index", replacementEvent.affectedSourceEdgeIndex},
                {"affected_child_wire_edge_info_index",
                 replacementEvent.affectedChildWireEdgeInfoIndex},
                {"affected_endpoint", replacementEvent.affectedEndpoint},
                {"affected_source_endpoint", replacementEvent.affectedSourceEndpoint},
                {"affected_child_wire_endpoint", replacementEvent.affectedChildWireEndpoint},
                {"replacement_source_edge_index", replacementEvent.replacementSourceEdgeIndex},
                {"replacement_source_endpoint", replacementEvent.replacementSourceEndpoint},
                {"replacement_from_mutable_source_edge_ledger",
                 replacementEvent.replacementFromMutableSourceEdgeLedger},
                {"replacement_from_split_fragment_ledger",
                 replacementEvent.replacementFromSplitFragmentLedger},
            });
        }
        wireJoinerOpenExportEntries.push_back({
            {"open_export_index", entry.openExportIndex},
            {"edge_info_index", entry.edgeInfoIndex},
            {"open_wire_compound_export_source", entry.openWireCompoundExportSource},
            {"open_wire_compound_edge_info_iteration",
             entry.openWireCompoundEdgeInfoIteration},
            {"open_wire_compound_edge_info_iteration2",
             entry.openWireCompoundEdgeInfoIteration2},
            {"open_wire_compound_owner_wire_info", entry.openWireCompoundOwnerWireInfo},
            {"open_wire_compound_owner_wire_info2", entry.openWireCompoundOwnerWireInfo2},
            {"open_wire_compound_open_leaf_export", entry.openWireCompoundOpenLeafExport},
            {"open_wire_compound_unowned_open_edge_export",
             entry.openWireCompoundUnownedOpenEdgeExport},
            {"open_wire_compound_root_current_member_child_producer",
             entry.openWireCompoundRootCurrentMemberChildProducer},
            {"open_wire_compound_child_shape_identity_recorded",
             entry.openWireCompoundChildShapeIdentityRecorded},
            {"open_wire_compound_child_wire_edge_count",
             entry.openWireCompoundChildWireEdgeCount},
            {"open_wire_compound_child_wire_vertex_count",
             entry.openWireCompoundChildWireVertexCount},
            {"wire_joiner_history_relation", entry.wireJoinerHistoryRelation},
            {"wire_joiner_history_relation_from_child_wire_ledger",
             entry.wireJoinerHistoryRelationFromChildWireLedger},
            {"wire_joiner_history_event_index", entry.wireJoinerHistoryEventIndex},
            {"wire_joiner_history_event_from_child_wire_ledger",
             entry.wireJoinerHistoryEventFromChildWireLedger},
            {"result_wire_producer_kind", entry.resultWireProducerKind},
            {"result_wire_producer_state", entry.resultWireProducerState},
            {"result_wire_producer_blocker", entry.resultWireProducerBlocker},
            {"result_wire_producer_source_edge_info_index",
             entry.resultWireProducerSourceEdgeInfoIndex},
            {"result_wire_producer_root_edge_info_index", entry.resultWireProducerRootEdgeInfoIndex},
            {"result_wire_producer_current_member_edge_info_index",
             entry.resultWireProducerCurrentMemberEdgeInfoIndex},
            {"result_wire_producer_child_wire_info_index", entry.resultWireProducerChildWireInfoIndex},
            {"open_wire_compound_child_wire_info_index", entry.openWireCompoundChildWireInfoIndex},
            {"open_wire_compound_source_edge_indices", entry.openWireCompoundSourceEdgeIndices},
            {"open_wire_compound_source_lineage_from_splitter_history",
             entry.openWireCompoundSourceLineageFromSplitterHistory},
            {"open_wire_compound_no_original_purge_match",
             entry.openWireCompoundNoOriginalPurgeMatch},
            {"open_wire_compound_no_original_purged_by_ledger",
             entry.openWireCompoundNoOriginalPurgedByLedger},
            {"open_wire_compound_no_original_shared_source_ledger_recorded",
             entry.openWireCompoundNoOriginalSharedSourceLedgerRecorded},
            {"open_wire_compound_no_original_shared_source_edge_count",
             entry.openWireCompoundNoOriginalSharedSourceEdgeCount},
            {"open_wire_compound_no_original_shared_source_matched_edge_count",
             entry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount},
            {"open_wire_compound_no_original_shared_source_unmatched_edge_count",
             entry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount},
            {"open_wire_compound_producer_ledger_wire_built",
             entry.openWireCompoundProducerLedgerWireBuilt},
            {"open_wire_compound_producer_ledger_wire_from_source_vmap",
             entry.openWireCompoundProducerLedgerWireFromSourceVmap},
            {"open_wire_compound_source_vmap_endpoint_ledger_recorded",
             entry.openWireCompoundSourceVmapEndpointLedgerRecorded},
            {"open_wire_compound_source_vmap_endpoint_ledger_output_vertex_count",
             entry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount},
            {"open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
             entry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_recorded",
             entry.openWireCompoundEndpointProvenanceRecorded},
            {"open_wire_compound_endpoint_provenance_output_vertex_count",
             entry.openWireCompoundEndpointProvenanceOutputVertexCount},
            {"open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
             entry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
             entry.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_candidate_matched_vertex_count",
             entry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount},
            {"open_wire_compound_endpoint_provenance_unmatched_vertex_count",
             entry.openWireCompoundEndpointProvenanceUnmatchedVertexCount},
            {"open_wire_compound_vmap_replacement_event_count",
             entry.openWireCompoundVmapReplacementEventCount},
            {"open_wire_compound_vmap_replacement_events", vmapReplacementEvents},
            {"open_wire_compound_current_member_producer_output",
             entry.openWireCompoundCurrentMemberProducerOutput},
            {"open_wire_compound_current_member_split_ledger_vertex_candidate",
             entry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate},
            {"open_wire_compound_current_member_split_ledger_vertex_debt_recorded",
             entry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded},
            {"open_wire_compound_current_member_split_ledger_member_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount},
            {"open_wire_compound_current_member_split_ledger_candidate_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
             entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount},
            {"open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count",
             entry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount},
            {"open_wire_compound_current_member_split_ledger_output_vertex_debt",
             currentMemberSplitOutputVertexDebt},
            {"open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked",
             entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked},
            {"missing_open_wire_compound_child_wire", entry.missingOpenWireCompoundChildWire},
            {"source_edge_indices", entry.sourceEdgeIndices},
            {"source_lineage_from_splitter_history", entry.sourceLineageFromSplitterHistory},
            {"split_fragment_source_edge_indices", entry.splitFragmentSourceEdgeIndices},
            {"split_fragment_modified_source_edge_indices",
             entry.splitFragmentModifiedSourceEdgeIndices},
            {"split_fragment_generated_source_edge_indices",
             entry.splitFragmentGeneratedSourceEdgeIndices},
            {"split_fragment_from_modified_history", entry.splitFragmentFromModifiedHistory},
            {"split_fragment_from_generated_history", entry.splitFragmentFromGeneratedHistory},
            {"split_fragment_source_lineage_from_identity_fallback",
             entry.splitFragmentSourceLineageFromIdentityFallback},
            {"split_fragment_source_lineage_from_source_identity_fallback",
             entry.splitFragmentSourceLineageFromSourceIdentityFallback},
            {"split_fragment_history_shape_geometry_bridge",
             entry.splitFragmentHistoryShapeGeometryBridge},
            {"source_vertex_identity", entry.sourceVertexIdentity},
            {"source_vertex_identity_any", entry.sourceVertexIdentity[0] || entry.sourceVertexIdentity[1]},
            {"source_vertex_identity_all", entry.sourceVertexIdentity[0] && entry.sourceVertexIdentity[1]},
            {"source_vertex_replacement_source_edge_indices",
             entry.sourceVertexReplacementSourceEdgeIndices},
            {"source_vertex_replacement_endpoints", entry.sourceVertexReplacementEndpoints},
            {"source_vertex_replacement_identity", entry.sourceVertexReplacementIdentity},
        });
    }
    return {
        {"source_edge_count", history.sourceEdgeCount},
        {"pre_split_edge_count", history.preSplitEdgeCount},
        {"splitter_edge_count", history.splitterEdgeCount},
        {"bounded_face_count", history.boundedFaceCount},
        {"pre_split_history", history.preSplitHistory},
        {"splitter_history", history.splitterHistory},
        {"facemaker_edge_evidence", std::move(faceMakerEdgeEvidence)},
        {"facemaker_bounded_face_evidence", std::move(faceMakerBoundedFaceEvidence)},
        {"wire_joiner_source_edge_count", history.wireJoinerSourceEdgeCount},
        {"wire_joiner_split_result_edge_count", history.wireJoinerSplitResultEdgeCount},
        {"wire_joiner_history_event_count", history.wireJoinerHistoryEvents.size()},
        {"wire_joiner_history_event_from_child_wire_ledger_count",
         wireJoinerHistoryEventFromChildWireLedgerCount},
        {"wire_joiner_history_events", std::move(wireJoinerHistoryEvents)},
        {"wire_joiner_open_export_history_entries", std::move(wireJoinerOpenExportEntries)},
        {"wire_joiner_modified_source_edge_count", history.wireJoinerModifiedSourceEdgeCount},
        {"wire_joiner_modified_history_count", history.wireJoinerModifiedHistoryCount},
        {"wire_joiner_generated_history_count", history.wireJoinerGeneratedHistoryCount},
        {"wire_joiner_deleted_history_count", history.wireJoinerDeletedHistoryCount},
        {"wire_joiner_splitter_history", history.wireJoinerSplitterHistory},
    };
}

nlohmann::json eventToJson(const InternalShapeHistoryEvent& event)
{
    nlohmann::json sourceEdges = nlohmann::json::array();
    for (const std::size_t sourceEdgeIndex : event.sourceEdgeIndices) {
        sourceEdges.push_back("Edge" + std::to_string(sourceEdgeIndex));
    }
    return {
        {"relation", internalShapeHistoryRelationName(event.relation)},
        {"producer", internalShapeHistoryProducerName(event.producer)},
        {"target_kind", internalShapeHistoryTargetKindName(event.targetKind)},
        {"stage", event.stage},
        {"diagnostic_code", event.diagnosticCode},
        {"source_edge_indices", event.sourceEdgeIndices},
        {"source_edges", std::move(sourceEdges)},
        {"target_shape_available", !event.targetShape.IsNull()},
    };
}

}  // namespace

InternalShapeHistoryLedger::InternalShapeHistoryLedger()
    : data_(std::make_unique<InternalShapeHistoryLedgerData>())
{
}

InternalShapeHistoryLedger::~InternalShapeHistoryLedger() = default;

InternalShapeHistoryLedger::InternalShapeHistoryLedger(const InternalShapeHistoryLedger& other)
    : data_(std::make_unique<InternalShapeHistoryLedgerData>(*other.data_))
{
}

InternalShapeHistoryLedger& InternalShapeHistoryLedger::operator=(
    const InternalShapeHistoryLedger& other
)
{
    if (this != &other) {
        data_ = std::make_unique<InternalShapeHistoryLedgerData>(*other.data_);
    }
    return *this;
}

InternalShapeHistoryLedger::InternalShapeHistoryLedger(InternalShapeHistoryLedger&&) noexcept =
    default;

InternalShapeHistoryLedger& InternalShapeHistoryLedger::operator=(
    InternalShapeHistoryLedger&&
) noexcept = default;

void addFaceMakerEvidenceToLedger(
    InternalShapeHistoryLedger& ledger,
    const FaceMakerHistorySummary& history
)
{
    InternalShapeHistoryLedgerData& data = mutableInternalShapeHistoryLedgerData(ledger);
    data.hasFaceMakerEvidence = true;
    data.faceMakerCompatibility = faceMakerHistoryToJson(history);

    SketchInternalHistoryContext& context = data.compatibilityHistory;
    context.sourceEdgeCount = history.sourceEdgeCount;
    context.preSplitEdgeCount = history.preSplitEdgeCount;
    context.splitterEdgeCount = history.splitterEdgeCount;
    context.boundedFaceCount = history.boundedFaceCount;
    context.preSplitHistory = history.preSplitHistory;
    context.splitterHistory = history.splitterHistory;
    context.faceMakerEdgeEvidence.clear();
    context.faceMakerBoundedFaceEvidence.clear();

    for (const FaceMakerEdgeHistoryEvidence& entry : history.edgeEvidence) {
        SketchInternalFaceMakerEdgeEvidence topoEntry;
        topoEntry.makerStage = entry.makerStage;
        topoEntry.relation = entry.relation;
        topoEntry.sourceEdgeIndex = entry.sourceEdgeIndex;
        topoEntry.targetEdgeIndex = entry.targetEdgeIndex;
        topoEntry.targetEdge = entry.targetEdge;
        topoEntry.preSplitHistory = entry.preSplitHistory;
        topoEntry.splitterHistory = entry.splitterHistory;
        context.faceMakerEdgeEvidence.push_back(std::move(topoEntry));

        data.events.push_back(InternalShapeHistoryEvent {
            relationFromName(entry.relation),
            InternalShapeHistoryProducer::FaceMakerBuildFace,
            InternalShapeHistoryTargetKind::Edge,
            entry.makerStage,
            {},
            {entry.sourceEdgeIndex},
            entry.targetEdge,
        });
    }
    for (const FaceMakerBoundedFaceHistoryEvidence& entry : history.boundedFaceEvidence) {
        SketchInternalFaceMakerBoundedFaceEvidence topoEntry;
        topoEntry.boundedFaceIndex = entry.boundedFaceIndex;
        topoEntry.face = entry.face;
        topoEntry.sourceEdgeIndices = entry.sourceEdgeIndices;
        topoEntry.outerBoundaryTargetEdgeIndices = entry.outerBoundaryTargetEdgeIndices;
        for (const FaceMakerBoundedFaceBoundaryEvidence& boundary : entry.outerBoundary) {
            SketchInternalFaceMakerBoundedFaceBoundaryEvidence topoBoundary;
            topoBoundary.sourceEdgeIndex = boundary.sourceEdgeIndex;
            topoBoundary.targetEdgeIndex = boundary.targetEdgeIndex;
            topoBoundary.makerStage = boundary.makerStage;
            topoBoundary.relation = boundary.relation;
            topoBoundary.targetEdge = boundary.targetEdge;
            topoEntry.outerBoundary.push_back(std::move(topoBoundary));
        }
        context.faceMakerBoundedFaceEvidence.push_back(std::move(topoEntry));

        data.events.push_back(InternalShapeHistoryEvent {
            InternalShapeHistoryRelation::Generated,
            InternalShapeHistoryProducer::FaceMakerBuildFace,
            InternalShapeHistoryTargetKind::Face,
            "facemaker:outer_boundary",
            {},
            entry.sourceEdgeIndices,
            entry.face,
        });
    }
}

void InternalShapeHistoryLedger::merge(const InternalShapeHistoryLedger& other)
{
    const InternalShapeHistoryLedgerData& otherData = *other.data_;
    if (otherData.hasFaceMakerEvidence) {
        data_->hasFaceMakerEvidence = true;
        data_->faceMakerCompatibility = otherData.faceMakerCompatibility;
        data_->compatibilityHistory.sourceEdgeCount =
            otherData.compatibilityHistory.sourceEdgeCount;
        data_->compatibilityHistory.preSplitEdgeCount =
            otherData.compatibilityHistory.preSplitEdgeCount;
        data_->compatibilityHistory.splitterEdgeCount =
            otherData.compatibilityHistory.splitterEdgeCount;
        data_->compatibilityHistory.boundedFaceCount =
            otherData.compatibilityHistory.boundedFaceCount;
        data_->compatibilityHistory.preSplitHistory =
            otherData.compatibilityHistory.preSplitHistory;
        data_->compatibilityHistory.splitterHistory = otherData.compatibilityHistory.splitterHistory;
        data_->compatibilityHistory.faceMakerEdgeEvidence =
            otherData.compatibilityHistory.faceMakerEdgeEvidence;
        data_->compatibilityHistory.faceMakerBoundedFaceEvidence =
            otherData.compatibilityHistory.faceMakerBoundedFaceEvidence;
    }
    if (otherData.hasWireJoinerEvidence) {
        data_->hasWireJoinerEvidence = true;
        data_->wireJoinerDiagnostics = otherData.wireJoinerDiagnostics;
        data_->wireJoinerCompatibilityLedger = otherData.wireJoinerCompatibilityLedger;
        data_->wireJoinerCompatibilityHistoryDetail =
            otherData.wireJoinerCompatibilityHistoryDetail;
        data_->compatibilityHistory.wireJoinerSourceEdgeCount =
            otherData.compatibilityHistory.wireJoinerSourceEdgeCount;
        data_->compatibilityHistory.wireJoinerSplitResultEdgeCount =
            otherData.compatibilityHistory.wireJoinerSplitResultEdgeCount;
        data_->compatibilityHistory.wireJoinerHistoryEvents =
            otherData.compatibilityHistory.wireJoinerHistoryEvents;
        data_->compatibilityHistory.wireJoinerOpenExportHistoryEntries =
            otherData.compatibilityHistory.wireJoinerOpenExportHistoryEntries;
        data_->compatibilityHistory.wireJoinerModifiedSourceEdgeCount =
            otherData.compatibilityHistory.wireJoinerModifiedSourceEdgeCount;
        data_->compatibilityHistory.wireJoinerModifiedHistoryCount =
            otherData.compatibilityHistory.wireJoinerModifiedHistoryCount;
        data_->compatibilityHistory.wireJoinerGeneratedHistoryCount =
            otherData.compatibilityHistory.wireJoinerGeneratedHistoryCount;
        data_->compatibilityHistory.wireJoinerDeletedHistoryCount =
            otherData.compatibilityHistory.wireJoinerDeletedHistoryCount;
        data_->compatibilityHistory.wireJoinerSplitterHistory =
            otherData.compatibilityHistory.wireJoinerSplitterHistory;
    }
    data_->events.insert(
        data_->events.end(),
        otherData.events.begin(),
        otherData.events.end()
    );
}

bool InternalShapeHistoryLedger::empty() const
{
    return !data_->hasFaceMakerEvidence && !data_->hasWireJoinerEvidence
        && data_->events.empty();
}

const std::vector<InternalShapeHistoryEvent>& InternalShapeHistoryLedger::historyEvents() const
{
    return data_->events;
}

nlohmann::json InternalShapeHistoryLedger::internalShapeHistoryJson() const
{
    nlohmann::json events = nlohmann::json::array();
    nlohmann::json relationSummary = nlohmann::json::object();
    nlohmann::json producerTags = nlohmann::json::array();
    nlohmann::json diagnosticCodes = nlohmann::json::array();
    bool hasFaceMaker = false;
    bool hasWireJoiner = false;
    for (const InternalShapeHistoryEvent& event : data_->events) {
        const std::string relation = internalShapeHistoryRelationName(event.relation);
        relationSummary[relation] = relationSummary.value(relation, 0U) + 1U;
        if (event.producer == InternalShapeHistoryProducer::FaceMakerBuildFace) {
            hasFaceMaker = true;
        }
        if (event.producer == InternalShapeHistoryProducer::WireJoinerOpenWires) {
            hasWireJoiner = true;
        }
        if (!event.diagnosticCode.empty()) {
            diagnosticCodes.push_back(event.diagnosticCode);
        }
        events.push_back(eventToJson(event));
    }
    if (hasFaceMaker) {
        producerTags.push_back("FaceMakerBuildFace");
    }
    if (hasWireJoiner) {
        producerTags.push_back("WireJoinerOpenWires");
    }
    return {
        {"event_count", data_->events.size()},
        {"producer_tags", std::move(producerTags)},
        {"relation_summary", std::move(relationSummary)},
        {"diagnostic_codes", std::move(diagnosticCodes)},
        {"events", std::move(events)},
    };
}

nlohmann::json InternalShapeHistoryLedger::compatibilityObjectFields() const
{
    nlohmann::json fields = {
        {"internal_shape_history", internalShapeHistoryJson()},
    };
    if (data_->hasWireJoinerEvidence) {
        fields["wire_joiner_diagnostics"] = data_->wireJoinerDiagnostics;
        fields["wire_joiner_ledger"] = data_->wireJoinerCompatibilityLedger;
        fields["wire_joiner_history"] = "history_partial:edge_info_wire_info_split_done_exhaust";
        fields["wire_joiner_history_detail"] = data_->wireJoinerCompatibilityHistoryDetail;
    }
    if (data_->hasFaceMakerEvidence) {
        fields["facemaker_history"] = data_->faceMakerCompatibility;
        fields["facemaker_history_status"] = "history_evidence:facemaker_buildface";
    }
    return fields;
}

nlohmann::json InternalShapeHistoryLedger::sketchInternalHistoryCompatibilityJson() const
{
    return sketchInternalHistoryContextToJson(data_->compatibilityHistory);
}

std::optional<std::string> InternalShapeHistoryLedger::sketchInternalHistoryStatus() const
{
    if (empty()) {
        return std::nullopt;
    }
    return "history_evidence:facemaker_wirejoiner";
}

const char* internalShapeHistoryRelationName(InternalShapeHistoryRelation relation)
{
    switch (relation) {
        case InternalShapeHistoryRelation::Preserved:
            return "preserved";
        case InternalShapeHistoryRelation::Generated:
            return "generated";
        case InternalShapeHistoryRelation::Modified:
            return "modified";
        case InternalShapeHistoryRelation::Deleted:
            return "deleted";
        case InternalShapeHistoryRelation::Split:
            return "split";
        case InternalShapeHistoryRelation::DiagnosticOnly:
            break;
    }
    return "diagnostic_only";
}

const char* internalShapeHistoryProducerName(InternalShapeHistoryProducer producer)
{
    switch (producer) {
        case InternalShapeHistoryProducer::FaceMakerBuildFace:
            return "FaceMakerBuildFace";
        case InternalShapeHistoryProducer::WireJoinerOpenWires:
            return "WireJoinerOpenWires";
    }
    return "Unknown";
}

const char* internalShapeHistoryTargetKindName(InternalShapeHistoryTargetKind targetKind)
{
    switch (targetKind) {
        case InternalShapeHistoryTargetKind::Shape:
            return "shape";
        case InternalShapeHistoryTargetKind::Face:
            return "face";
        case InternalShapeHistoryTargetKind::Edge:
            return "edge";
        case InternalShapeHistoryTargetKind::Vertex:
            return "vertex";
        case InternalShapeHistoryTargetKind::Wire:
            return "wire";
    }
    return "shape";
}

InternalShapeHistoryLedgerData& mutableInternalShapeHistoryLedgerData(
    InternalShapeHistoryLedger& ledger
)
{
    return *ledger.data_;
}

const InternalShapeHistoryLedgerData& internalShapeHistoryLedgerData(
    const InternalShapeHistoryLedger& ledger
)
{
    return *ledger.data_;
}

}  // namespace cad_core::part
