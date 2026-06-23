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
    const std::optional<part::WireJoinerHistorySummary>& wireJoinerHistory
)
{
    if (!faceMakerHistory && !wireJoinerHistory) {
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

    if (wireJoinerHistory) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(), calls makeShapeWithElementMap(...,
        // MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op). This passes
        // only WireJoiner-produced history summary into topo; topo must not infer WireJoiner
        // split/generated/deleted history from raw/internal geometry.
        context.wireJoinerSourceEdgeCount = wireJoinerHistory->sourceEdgeCount;
        context.wireJoinerSplitResultEdgeCount = wireJoinerHistory->splitResultEdgeCount;
        for (const part::WireJoinerHistoryEvent& event : wireJoinerHistory->historyEvents) {
            part::SketchInternalWireJoinerHistoryEvent topoEvent;
            topoEvent.eventIndex = event.eventIndex;
            topoEvent.openExportIndex = event.openExportIndex;
            topoEvent.edgeInfoIndex = event.edgeInfoIndex;
            topoEvent.openWireCompoundChildWireInfoIndex = event.openWireCompoundChildWireInfoIndex;
            topoEvent.relation = part::wireJoinerHistoryRelationName(event.relation);
            topoEvent.relationFromChildWireLedger = event.relationFromChildWireLedger;
            topoEvent.sourceEdgeIndices = event.sourceEdgeIndices;
            topoEvent.sourceLineageFromSplitterHistory = event.sourceLineageFromSplitterHistory;
            topoEvent.noOriginalPurgedByLedger = event.noOriginalPurgedByLedger;
            topoEvent.splitFragmentFromModifiedHistory = event.splitFragmentFromModifiedHistory;
            topoEvent.splitFragmentFromGeneratedHistory = event.splitFragmentFromGeneratedHistory;
            context.wireJoinerHistoryEvents.push_back(std::move(topoEvent));
        }
        for (const part::WireJoinerOpenExportHistoryEntry& entry :
             wireJoinerHistory->openExportEntries) {
            part::SketchInternalWireJoinerOpenExportHistoryEntry topoEntry;
            topoEntry.openExportIndex = entry.openExportIndex;
            topoEntry.edgeInfoIndex = entry.edgeInfoIndex;
            topoEntry.openExportWire = entry.openExportWire;
            topoEntry.openExportEdge = entry.openExportEdge;
            topoEntry.wireJoinerHistoryRelation = entry.historyRelationFromChildWireLedger
                ? part::wireJoinerHistoryRelationName(entry.historyRelation)
                : std::string();
            topoEntry.wireJoinerHistoryRelationFromChildWireLedger
                = entry.historyRelationFromChildWireLedger;
            topoEntry.wireJoinerHistoryEventIndex = entry.wireJoinerHistoryEventIndex;
            topoEntry.wireJoinerHistoryEventFromChildWireLedger
                = entry.wireJoinerHistoryEventFromChildWireLedger;
            topoEntry.resultWireProducerKind = part::resultWireProducerKindName(
                entry.resultWireProducer.kind
            );
            topoEntry.resultWireProducerState = part::resultWireProducerStateName(
                entry.resultWireProducer.state
            );
            topoEntry.resultWireProducerBlocker = part::resultWireBlockerName(
                entry.resultWireProducer.blocker
            );
            topoEntry.resultWireProducerSourceEdgeInfoIndex
                = entry.resultWireProducer.sourceEdgeInfoIndex;
            topoEntry.resultWireProducerRootEdgeInfoIndex = entry.resultWireProducer.rootEdgeInfoIndex;
            topoEntry.resultWireProducerCurrentMemberEdgeInfoIndex
                = entry.resultWireProducer.currentMemberEdgeInfoIndex;
            topoEntry.resultWireProducerChildWireInfoIndex = entry.resultWireProducer.childWireInfoIndex;
            topoEntry.openWireCompoundChildWireInfoIndex = entry.openWireCompoundChildWireInfoIndex;
            topoEntry.openWireCompoundExportSource = part::openWireCompoundExportSourceName(
                entry.openWireCompoundExportSource
            );
            topoEntry.openWireCompoundEdgeInfoIteration = entry.openWireCompoundEdgeInfoIteration;
            topoEntry.openWireCompoundEdgeInfoIteration2 = entry.openWireCompoundEdgeInfoIteration2;
            topoEntry.openWireCompoundOwnerWireInfo = entry.openWireCompoundOwnerWireInfo;
            topoEntry.openWireCompoundOwnerWireInfo2 = entry.openWireCompoundOwnerWireInfo2;
            topoEntry.openWireCompoundOpenLeafExport = entry.openWireCompoundOpenLeafExport;
            topoEntry.openWireCompoundUnownedOpenEdgeExport = entry.openWireCompoundUnownedOpenEdgeExport;
            topoEntry.openWireCompoundRootCurrentMemberChildProducer
                = entry.openWireCompoundRootCurrentMemberChildProducer;
            topoEntry.openWireCompoundChildShapeIdentityRecorded
                = entry.openWireCompoundChildShapeIdentityRecorded;
            topoEntry.openWireCompoundChildWireEdgeCount = entry.openWireCompoundChildWireEdgeCount;
            topoEntry.openWireCompoundChildWireVertexCount = entry.openWireCompoundChildWireVertexCount;
            topoEntry.openWireCompoundSourceEdgeIndices = entry.openWireCompoundSourceEdgeIndices;
            topoEntry.openWireCompoundSourceLineageFromSplitterHistory
                = entry.openWireCompoundSourceLineageFromSplitterHistory;
            topoEntry.openWireCompoundNoOriginalPurgeMatch = entry.openWireCompoundNoOriginalPurgeMatch;
            topoEntry.openWireCompoundNoOriginalPurgedByLedger
                = entry.openWireCompoundNoOriginalPurgedByLedger;
            topoEntry.openWireCompoundNoOriginalSharedSourceLedgerRecorded
                = entry.openWireCompoundNoOriginalSharedSourceLedgerRecorded;
            topoEntry.openWireCompoundNoOriginalSharedSourceEdgeCount
                = entry.openWireCompoundNoOriginalSharedSourceEdgeCount;
            topoEntry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount
                = entry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount;
            topoEntry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount
                = entry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount;
            topoEntry.openWireCompoundProducerLedgerWireBuilt
                = entry.openWireCompoundProducerLedgerWireBuilt;
            topoEntry.openWireCompoundProducerLedgerWireFromSourceVmap
                = entry.openWireCompoundProducerLedgerWireFromSourceVmap;
            topoEntry.openWireCompoundSourceVmapEndpointLedgerRecorded
                = entry.openWireCompoundSourceVmapEndpointLedgerRecorded;
            topoEntry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount
                = entry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount;
            topoEntry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount
                = entry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount;
            topoEntry.openWireCompoundEndpointProvenanceRecorded
                = entry.openWireCompoundEndpointProvenanceRecorded;
            topoEntry.openWireCompoundEndpointProvenanceOutputVertexCount
                = entry.openWireCompoundEndpointProvenanceOutputVertexCount;
            topoEntry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount
                = entry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount;
            topoEntry.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount
                = entry.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount;
            topoEntry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount
                = entry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount;
            topoEntry.openWireCompoundEndpointProvenanceUnmatchedVertexCount
                = entry.openWireCompoundEndpointProvenanceUnmatchedVertexCount;
            topoEntry.openWireCompoundVmapReplacementEventCount
                = entry.openWireCompoundVmapReplacementEventCount;
            for (const part::WireJoinerVmapReplacementEvent& event :
                 entry.openWireCompoundVmapReplacementEvents) {
                part::SketchInternalWireJoinerVmapReplacementEvent topoEvent;
                topoEvent.eventIndex = event.eventIndex;
                topoEvent.affectedSourceEdgeIndex = event.affectedSourceEdgeIndex;
                topoEvent.affectedChildWireEdgeInfoIndex = event.affectedChildWireEdgeInfoIndex;
                topoEvent.affectedEndpoint = event.affectedEndpoint;
                topoEvent.affectedSourceEndpoint = event.affectedSourceEndpoint;
                topoEvent.affectedChildWireEndpoint = event.affectedChildWireEndpoint;
                topoEvent.replacementSourceEdgeIndex = event.replacementSourceEdgeIndex;
                topoEvent.replacementSourceEndpoint = event.replacementSourceEndpoint;
                topoEvent.replacementFromMutableSourceEdgeLedger
                    = event.replacementFromMutableSourceEdgeLedger;
                topoEvent.replacementFromSplitFragmentLedger = event.replacementFromSplitFragmentLedger;
                topoEntry.openWireCompoundVmapReplacementEvents.push_back(std::move(topoEvent));
            }
            topoEntry.openWireCompoundCurrentMemberProducerOutput
                = entry.openWireCompoundCurrentMemberProducerOutput;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate
                = entry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded
                = entry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount
                = entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount;
            topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount
                = entry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount;
            for (const part::WireJoinerEndpointIdentityDebt& debt :
                 entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt) {
                part::SketchInternalWireJoinerEndpointIdentityDebt topoDebt;
                topoDebt.outputVertexIndex = debt.outputVertexIndex;
                topoDebt.matchedMemberSplitLedger = debt.matchedMemberSplitLedger;
                topoDebt.matchedCandidateLedger = debt.matchedCandidateLedger;
                topoDebt.currentChildWireOutputVertexMatchesOtherOutput
                    = debt.currentChildWireOutputVertexMatchesOtherOutput;
                topoDebt.candidateWireVertexMatchesOtherOutput
                    = debt.candidateWireVertexMatchesOtherOutput;
                topoDebt.explanation = debt.explanation;
                topoDebt.currentChildWireOutputVertexIdentity = debt.currentChildWireOutputVertexIdentity;
                topoDebt.memberSplitLedgerVertexIdentity = debt.memberSplitLedgerVertexIdentity;
                topoDebt.candidateWireVertexIdentity = debt.candidateWireVertexIdentity;
                topoDebt.mismatchReason = debt.mismatchReason;
                topoEntry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt.push_back(
                    std::move(topoDebt)
                );
            }
            topoEntry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked
                = entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked;
            topoEntry.missingOpenWireCompoundChildWire = entry.missingOpenWireCompoundChildWire;
            topoEntry.sourceEdgeIndices = entry.sourceEdgeIndices;
            topoEntry.sourceLineageFromSplitterHistory = entry.sourceLineageFromSplitterHistory;
            topoEntry.splitFragmentSourceEdgeIndices = entry.splitFragmentSourceEdgeIndices;
            topoEntry.splitFragmentModifiedSourceEdgeIndices
                = entry.splitFragmentModifiedSourceEdgeIndices;
            topoEntry.splitFragmentGeneratedSourceEdgeIndices
                = entry.splitFragmentGeneratedSourceEdgeIndices;
            topoEntry.splitFragmentFromModifiedHistory = entry.splitFragmentFromModifiedHistory;
            topoEntry.splitFragmentFromGeneratedHistory = entry.splitFragmentFromGeneratedHistory;
            topoEntry.splitFragmentSourceLineageFromIdentityFallback
                = entry.splitFragmentSourceLineageFromIdentityFallback;
            topoEntry.splitFragmentSourceLineageFromSourceIdentityFallback
                = entry.splitFragmentSourceLineageFromSourceIdentityFallback;
            topoEntry.splitFragmentHistoryShapeGeometryBridge
                = entry.splitFragmentHistoryShapeGeometryBridge;
            topoEntry.sourceVertexIdentity = entry.sourceVertexIdentity;
            topoEntry.sourceVertexReplacementSourceEdgeIndices
                = entry.sourceVertexReplacementSourceEdgeIndices;
            topoEntry.sourceVertexReplacementEndpoints = entry.sourceVertexReplacementEndpoints;
            topoEntry.sourceVertexReplacementIdentity = entry.sourceVertexReplacementIdentity;
            context.wireJoinerOpenExportHistoryEntries.push_back(std::move(topoEntry));
        }
        context.wireJoinerModifiedSourceEdgeCount = wireJoinerHistory->modifiedSourceEdgeCount;
        context.wireJoinerModifiedHistoryCount = wireJoinerHistory->modifiedHistoryCount;
        context.wireJoinerGeneratedHistoryCount = wireJoinerHistory->generatedHistoryCount;
        context.wireJoinerDeletedHistoryCount = wireJoinerHistory->deletedHistoryCount;
        context.wireJoinerSplitterHistory = wireJoinerHistory->splitterHistory;
    }

    return context;
}

nlohmann::json resultWireProducerLedgerEntriesJson(
    const std::vector<part::ResultWireProducerLedgerEntry>& entries
)
{
    nlohmann::json result = nlohmann::json::array();
    for (const part::ResultWireProducerLedgerEntry& entry : entries) {
        result.push_back({
            {"open_export_index", entry.openExportIndex},
            {"source_edge_info_index", entry.sourceEdgeInfoIndex},
            {"root_edge_info_index", entry.rootEdgeInfoIndex},
            {"current_member_edge_info_index", entry.currentMemberEdgeInfoIndex},
            {"child_wire_info_index", entry.childWireInfoIndex},
            {"kind", part::resultWireProducerKindName(entry.kind)},
            {"state", part::resultWireProducerStateName(entry.state)},
            {"blocker", part::resultWireBlockerName(entry.blocker)},
            {"open_wire_compound_export_source",
             part::openWireCompoundExportSourceName(entry.openWireCompoundExportSource)},
            {"open_wire_compound_edge_info_iteration", entry.openWireCompoundEdgeInfoIteration},
            {"open_wire_compound_edge_info_iteration2", entry.openWireCompoundEdgeInfoIteration2},
            {"open_wire_compound_owner_wire_info", entry.openWireCompoundOwnerWireInfo},
            {"open_wire_compound_owner_wire_info2", entry.openWireCompoundOwnerWireInfo2},
            {"open_wire_compound_open_leaf_export", entry.openWireCompoundOpenLeafExport},
            {"open_wire_compound_unowned_open_edge_export",
             entry.openWireCompoundUnownedOpenEdgeExport},
            {"open_wire_compound_root_current_member_child_producer",
             entry.openWireCompoundRootCurrentMemberChildProducer},
            {"wire_joiner_history_event_index", entry.wireJoinerHistoryEventIndex},
            {"child_shape_identity_recorded", entry.childShapeIdentityRecorded},
            {"child_wire_edge_count", entry.childWireEdgeCount},
            {"child_wire_vertex_count", entry.childWireVertexCount},
            {"source_edge_indices", entry.sourceEdgeIndices},
        });
    }
    return result;
}

nlohmann::json wireJoinerLedgerToJson(const part::WireJoinerLedgerSummary& ledger)
{
    return {
        {"edge_info_count", ledger.edgeInfoCount},
        {"split_edge_info_count", ledger.splitEdgeInfoCount},
        {"primary_owned_edge_info_count", ledger.primaryOwnedEdgeInfoCount},
        {"secondary_owned_edge_info_count", ledger.secondaryOwnedEdgeInfoCount},
        {"closed_wire_assigned_edge_info_count", ledger.closedWireAssignedEdgeInfoCount},
        {"closed_wire_info_count", ledger.closedWireInfoCount},
        {"closed_wire_vertex_count", ledger.closedWireVertexCount},
        {"closed_wire_search_stack_frame_count", ledger.closedWireSearchStackFrameCount},
        {"closed_wire_search_vertex_stack_count", ledger.closedWireSearchVertexStackCount},
        {"closed_wire_search_edge_set_visit_count", ledger.closedWireSearchEdgeSetVisitCount},
        {"closed_wire_search_backtrack_count", ledger.closedWireSearchBacktrackCount},
        {"closed_wire_search_intersect_skip_count", ledger.closedWireSearchIntersectSkipCount},
        {"tight_bound_done_wire_info_count", ledger.tightBoundDoneWireInfoCount},
        {"tight_bound_split_wire_info_count", ledger.tightBoundSplitWireInfoCount},
        {"tight_bound_new_wire_candidate_count", ledger.tightBoundNewWireCandidateCount},
        {"tight_bound_new_wire_vertex_count", ledger.tightBoundNewWireVertexCount},
        {"tight_bound_owner_transfer_candidate_edge_info_count",
         ledger.tightBoundOwnerTransferCandidateEdgeInfoCount},
        {"tight_bound_transfer_wire_info_count", ledger.tightBoundTransferWireInfoCount},
        {"tight_bound_transfer_wire_vertex_count", ledger.tightBoundTransferWireVertexCount},
        {"tight_bound_transferred_owner_edge_info_count",
         ledger.tightBoundTransferredOwnerEdgeInfoCount},
        {"tight_bound_split_owner_wire_info_count", ledger.tightBoundSplitOwnerWireInfoCount},
        {"tight_bound_split_owner_vertex_count", ledger.tightBoundSplitOwnerVertexCount},
        {"tight_bound_split_owner_built_wire_count", ledger.tightBoundSplitOwnerBuiltWireCount},
        {"tight_bound_split_wire_vertex_count", ledger.tightBoundSplitWireVertexCount},
        {"tight_bound_split_wire_built_count", ledger.tightBoundSplitWireBuiltCount},
        {"tight_bound_existing_wire_search_count", ledger.tightBoundExistingWireSearchCount},
        {"tight_bound_existing_wire_hit_count", ledger.tightBoundExistingWireHitCount},
        {"tight_bound_existing_wire_reverse_hit_count", ledger.tightBoundExistingWireReverseHitCount},
        {"tight_bound_existing_wire_purge_count", ledger.tightBoundExistingWirePurgeCount},
        {"tight_bound_purged_wire_info_count", ledger.tightBoundPurgedWireInfoCount},
        {"tight_bound_exhaust_visited_wire_info_count", ledger.tightBoundExhaustVisitedWireInfoCount},
        {"tight_bound_exhaust_done_wire_info_count", ledger.tightBoundExhaustDoneWireInfoCount},
        {"tight_bound_exhaust_discarded_purged_wire_info_count",
         ledger.tightBoundExhaustDiscardedPurgedWireInfoCount},
        {"tight_bound_exhaust_primary_reset_edge_info_count",
         ledger.tightBoundExhaustPrimaryResetEdgeInfoCount},
        {"tight_bound_full_wire_set_insert_count", ledger.tightBoundFullWireSetInsertCount},
        {"tight_bound_full_wire_set_erase_count", ledger.tightBoundFullWireSetEraseCount},
        {"tight_bound_full_wire_set_abort_count", ledger.tightBoundFullWireSetAbortCount},
        {"tight_bound_full_wire_set_purge_candidate_count",
         ledger.tightBoundFullWireSetPurgeCandidateCount},
        {"tight_bound_full_wire_set_blocked_transfer_count",
         ledger.tightBoundFullWireSetBlockedTransferCount},
        {"tight_bound_full_wire_set_abort_search_count", ledger.tightBoundFullWireSetAbortSearchCount},
        {"tight_bound_full_wire_set_abort_resolved_by_hit_count",
         ledger.tightBoundFullWireSetAbortResolvedByHitCount},
        {"tight_bound_full_wire_set_abort_blocked_search_count",
         ledger.tightBoundFullWireSetAbortBlockedSearchCount},
        {"tight_bound_existing_wire_multi_round_wire_info_count",
         ledger.tightBoundExistingWireMultiRoundWireInfoCount},
        {"tight_bound_existing_wire_multi_round_search_count",
         ledger.tightBoundExistingWireMultiRoundSearchCount},
        {"exhaust_adjacent_search_count", ledger.exhaustAdjacentSearchCount},
        {"exhaust_adjacent_search_hit_count", ledger.exhaustAdjacentSearchHitCount},
        {"exhaust_adjacent_search_miss_count", ledger.exhaustAdjacentSearchMissCount},
        {"exhaust_adjacent_search_stack_frame_count", ledger.exhaustAdjacentSearchStackFrameCount},
        {"exhaust_adjacent_search_vertex_stack_count", ledger.exhaustAdjacentSearchVertexStackCount},
        {"exhaust_adjacent_search_edge_set_visit_count",
         ledger.exhaustAdjacentSearchEdgeSetVisitCount},
        {"exhaust_adjacent_search_backtrack_count", ledger.exhaustAdjacentSearchBacktrackCount},
        {"exhaust_adjacent_wire_set_insert_count", ledger.exhaustAdjacentWireSetInsertCount},
        {"exhaust_adjacent_wire_set_erase_count", ledger.exhaustAdjacentWireSetEraseCount},
        {"exhaust_adjacent_wire_set_abort_count", ledger.exhaustAdjacentWireSetAbortCount},
        {"exhaust_adjacent_wire_info2_abort_count", ledger.exhaustAdjacentWireInfo2AbortCount},
        {"repeated_split_exhaust_cycle_count", ledger.repeatedSplitExhaustCycleCount},
        {"repeated_split_exhaust_removed_edge_info_count",
         ledger.repeatedSplitExhaustRemovedEdgeInfoCount},
        {"repeated_split_exhaust_removed_unowned_edge_info_count",
         ledger.repeatedSplitExhaustRemovedUnownedEdgeInfoCount},
        {"repeated_split_exhaust_removed_secondary_edge_info_count",
         ledger.repeatedSplitExhaustRemovedSecondaryEdgeInfoCount},
        {"repeated_split_exhaust_removed_primary_edge_info_count",
         ledger.repeatedSplitExhaustRemovedPrimaryEdgeInfoCount},
        {"repeated_split_exhaust_rerun_active_edge_info_count",
         ledger.repeatedSplitExhaustRerunActiveEdgeInfoCount},
        {"repeated_split_exhaust_rerun_owned_active_edge_info_count",
         ledger.repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount},
        {"repeated_split_exhaust_rerun_reset_primary_edge_info_count",
         ledger.repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount},
        {"repeated_split_exhaust_rerun_reset_secondary_edge_info_count",
         ledger.repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount},
        {"repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count",
         ledger.repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount},
        {"repeated_split_exhaust_rerun_no_active_search_count",
         ledger.repeatedSplitExhaustRerunNoActiveSearchCount},
        {"repeated_split_exhaust_rerun_closed_wire_search_count",
         ledger.repeatedSplitExhaustRerunClosedWireSearchCount},
        {"repeated_split_exhaust_rerun_closed_wire_miss_count",
         ledger.repeatedSplitExhaustRerunClosedWireMissCount},
        {"repeated_split_exhaust_rerun_miss_live_reset_edge_info_count",
         ledger.repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount},
        {"repeated_split_exhaust_rerun_closed_wire_info_count",
         ledger.repeatedSplitExhaustRerunClosedWireInfoCount},
        {"repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count",
         ledger.repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount},
        {"repeated_split_exhaust_rerun_closed_wire_vertex_count",
         ledger.repeatedSplitExhaustRerunClosedWireVertexCount},
        {"repeated_split_exhaust_rerun_resettable_closed_wire_info_count",
         ledger.repeatedSplitExhaustRerunResettableClosedWireInfoCount},
        {"repeated_split_exhaust_rerun_resettable_assigned_edge_info_count",
         ledger.repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount},
        {"repeated_split_exhaust_rerun_live_reset_primary_edge_info_count",
         ledger.repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount},
        {"repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count",
         ledger.repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount},
        {"repeated_split_exhaust_rerun_live_closed_wire_info_count",
         ledger.repeatedSplitExhaustRerunLiveClosedWireInfoCount},
        {"repeated_split_exhaust_rerun_live_assigned_edge_info_count",
         ledger.repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount},
        {"repeated_split_exhaust_rerun_live_closed_wire_vertex_count",
         ledger.repeatedSplitExhaustRerunLiveClosedWireVertexCount},
        {"repeated_split_exhaust_rerun_live_branch_search_candidate_count",
         ledger.repeatedSplitExhaustRerunLiveBranchSearchCandidateCount},
        {"repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count",
         ledger.repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount},
        {"repeated_split_exhaust_rerun_live_done_wire_info_count",
         ledger.repeatedSplitExhaustRerunLiveDoneWireInfoCount},
        {"repeated_split_exhaust_rerun_removal_scan_count",
         ledger.repeatedSplitExhaustRerunRemovalScanCount},
        {"repeated_split_exhaust_rerun_loop_exit_no_removal_count",
         ledger.repeatedSplitExhaustRerunLoopExitNoRemovalCount},
        {"repeated_split_exhaust_rerun_branch_search_candidate_count",
         ledger.repeatedSplitExhaustRerunBranchSearchCandidateCount},
        {"repeated_split_exhaust_rerun_branch_search_inside_candidate_count",
         ledger.repeatedSplitExhaustRerunBranchSearchInsideCandidateCount},
        {"repeated_split_exhaust_rerun_new_wire_seed_candidate_count",
         ledger.repeatedSplitExhaustRerunNewWireSeedCandidateCount},
        {"repeated_split_exhaust_generated_identity_blocked_edge_info_count",
         ledger.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount},
        {"tight_bound_existing_wire_search_stack_frame_count",
         ledger.tightBoundExistingWireSearchStackFrameCount},
        {"tight_bound_existing_wire_search_vertex_stack_count",
         ledger.tightBoundExistingWireSearchVertexStackCount},
        {"tight_bound_existing_wire_search_edge_set_visit_count",
         ledger.tightBoundExistingWireSearchEdgeSetVisitCount},
        {"tight_bound_existing_wire_search_backtrack_count",
         ledger.tightBoundExistingWireSearchBacktrackCount},
        {"tight_bound_existing_wire_search_idx_vertex_count",
         ledger.tightBoundExistingWireSearchIdxVertexCount},
        {"tight_bound_existing_wire_search_stack_pos_count",
         ledger.tightBoundExistingWireSearchStackPosCount},
        {"tight_bound_existing_wire_search_path_vertex_count",
         ledger.tightBoundExistingWireSearchPathVertexCount},
        {"tight_bound_existing_wire_selected_hit_count",
         ledger.tightBoundExistingWireSelectedHitCount},
        {"tight_bound_existing_wire_search_only_hit_count",
         ledger.tightBoundExistingWireSearchOnlyHitCount},
        {"tight_bound_existing_wire_search_only_idx_vertex_count",
         ledger.tightBoundExistingWireSearchOnlyIdxVertexCount},
        {"tight_bound_existing_wire_search_only_stack_pos_count",
         ledger.tightBoundExistingWireSearchOnlyStackPosCount},
        {"tight_bound_existing_wire_search_only_path_blocked_count",
         ledger.tightBoundExistingWireSearchOnlyPathBlockedCount},
        {"tight_bound_existing_wire_search_only_order_blocked_count",
         ledger.tightBoundExistingWireSearchOnlyOrderBlockedCount},
        {"tight_bound_existing_wire_idx_vertex_count", ledger.tightBoundExistingWireIdxVertexCount},
        {"tight_bound_existing_wire_stack_pos_count", ledger.tightBoundExistingWireStackPosCount},
        {"result_wire_producer_ledger_entries",
         resultWireProducerLedgerEntriesJson(ledger.resultWireProducerLedgerEntries)},
        {"source_identity_shared_vertex_edge_info_count",
         ledger.sourceIdentitySharedVertexEdgeInfoCount},
        {"source_identity_only_source_vertices_edge_info_count",
         ledger.sourceIdentityOnlySourceVerticesEdgeInfoCount},
        {"source_identity_open_export_shared_vertex_edge_info_count",
         ledger.sourceIdentityOpenExportSharedVertexEdgeInfoCount},
        {"source_identity_open_export_only_source_vertices_edge_info_count",
         ledger.sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount},
        {"source_lineage_edge_info_count", ledger.sourceLineageEdgeInfoCount},
        {"source_lineage_split_edge_info_count", ledger.sourceLineageSplitEdgeInfoCount},
        {"source_lineage_open_export_edge_info_count", ledger.sourceLineageOpenExportEdgeInfoCount},
        {"source_lineage_missing_open_export_edge_info_count",
         ledger.sourceLineageMissingOpenExportEdgeInfoCount},
        {"source_lineage_multi_source_edge_info_count", ledger.sourceLineageMultiSourceEdgeInfoCount},
        {"split_fragment_source_lineage_edge_info_count",
         ledger.splitFragmentSourceLineageEdgeInfoCount},
        {"split_fragment_modified_history_edge_info_count",
         ledger.splitFragmentModifiedHistoryEdgeInfoCount},
        {"split_fragment_generated_history_edge_info_count",
         ledger.splitFragmentGeneratedHistoryEdgeInfoCount},
        {"split_fragment_identity_fallback_edge_info_count",
         ledger.splitFragmentIdentityFallbackEdgeInfoCount},
        {"split_fragment_source_identity_fallback_edge_info_count",
         ledger.splitFragmentSourceIdentityFallbackEdgeInfoCount},
        {"split_fragment_history_shape_geometry_bridge_edge_info_count",
         ledger.splitFragmentHistoryShapeGeometryBridgeEdgeInfoCount},
        {"closed_wire_cycle_split_ledger_source_edge_count",
         ledger.closedWireCycleSplitLedgerSourceEdgeCount},
        {"closed_wire_cycle_split_ledger_open_export_decision_count",
         ledger.closedWireCycleSplitLedgerOpenExportDecisionCount},
        {"super_edge_candidate_count", ledger.superEdgeCandidateCount},
        {"super_edge_candidate_edge_info_count", ledger.superEdgeCandidateEdgeInfoCount},
        {"super_edge_root_edge_info_count", ledger.superEdgeRootEdgeInfoCount},
        {"super_edge_closed_candidate_count", ledger.superEdgeClosedCandidateCount},
        {"super_edge_open_candidate_count", ledger.superEdgeOpenCandidateCount},
        {"super_edge_materialized_root_edge_info_count",
         ledger.superEdgeMaterializedRootEdgeInfoCount},
        {"super_edge_materialized_edge_info_count", ledger.superEdgeMaterializedEdgeInfoCount},
        {"super_edge_shadowed_member_edge_info_count", ledger.superEdgeShadowedMemberEdgeInfoCount},
        {"super_edge_lifecycle_member_minus_one_edge_info_count",
         ledger.superEdgeLifecycleMemberMinusOneEdgeInfoCount},
        {"super_edge_lifecycle_open_root_edge_info_count",
         ledger.superEdgeLifecycleOpenRootEdgeInfoCount},
        {"super_edge_lifecycle_closed_root_edge_info_count",
         ledger.superEdgeLifecycleClosedRootEdgeInfoCount},
        {"super_edge_lifecycle_adjacent_range_rewrite_count",
         ledger.superEdgeLifecycleAdjacentRangeRewriteCount},
        {"super_edge_lifecycle_endpoint_rewrite_count", ledger.superEdgeLifecycleEndpointRewriteCount},
        {"super_edge_lifecycle_adjacent_range_source_edge_info_count",
         ledger.superEdgeLifecycleAdjacentRangeSourceEdgeInfoCount},
        {"super_edge_lifecycle_adjacent_range_vertex_count",
         ledger.superEdgeLifecycleAdjacentRangeVertexCount},
        {"open_export_edge_info_count", ledger.openExportEdgeInfoCount},
        {"open_wire_compound_wire_info_count", ledger.openWireCompoundWireInfoCount},
        {"open_wire_compound_built_wire_info_count", ledger.openWireCompoundBuiltWireInfoCount},
        {"open_wire_compound_edge_info_count", ledger.openWireCompoundEdgeInfoCount},
        {"open_wire_compound_super_edge_wire_info_count",
         ledger.openWireCompoundSuperEdgeWireInfoCount},
        {"open_wire_compound_source_lineage_wire_info_count",
         ledger.openWireCompoundSourceLineageWireInfoCount},
        {"open_wire_compound_splitter_lineage_wire_info_count",
         ledger.openWireCompoundSplitterLineageWireInfoCount},
        {"open_wire_compound_no_original_purge_match_wire_info_count",
         ledger.openWireCompoundNoOriginalPurgeMatchWireInfoCount},
        {"open_wire_compound_no_original_shared_source_ledger_wire_info_count",
         ledger.openWireCompoundNoOriginalSharedSourceLedgerWireInfoCount},
        {"open_wire_compound_no_original_shared_source_edge_count",
         ledger.openWireCompoundNoOriginalSharedSourceEdgeCount},
        {"open_wire_compound_no_original_shared_source_matched_edge_count",
         ledger.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount},
        {"open_wire_compound_no_original_shared_source_unmatched_edge_count",
         ledger.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount},
        {"open_wire_compound_producer_ledger_wire_built_wire_info_count",
         ledger.openWireCompoundProducerLedgerWireBuiltWireInfoCount},
        {"open_wire_compound_producer_ledger_wire_from_source_vmap_wire_info_count",
         ledger.openWireCompoundProducerLedgerWireFromSourceVmapWireInfoCount},
        {"open_wire_compound_source_vmap_endpoint_ledger_wire_info_count",
         ledger.openWireCompoundSourceVmapEndpointLedgerWireInfoCount},
        {"open_wire_compound_source_vmap_endpoint_ledger_output_vertex_count",
         ledger.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount},
        {"open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
         ledger.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount},
        {"open_wire_compound_endpoint_provenance_wire_info_count",
         ledger.openWireCompoundEndpointProvenanceWireInfoCount},
        {"open_wire_compound_endpoint_provenance_output_vertex_count",
         ledger.openWireCompoundEndpointProvenanceOutputVertexCount},
        {"open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
         ledger.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount},
        {"open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
         ledger.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount},
        {"open_wire_compound_endpoint_provenance_candidate_matched_vertex_count",
         ledger.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount},
        {"open_wire_compound_endpoint_provenance_unmatched_vertex_count",
         ledger.openWireCompoundEndpointProvenanceUnmatchedVertexCount},
        {"open_wire_compound_vmap_replacement_event_wire_info_count",
         ledger.openWireCompoundVmapReplacementEventWireInfoCount},
        {"open_wire_compound_vmap_replacement_event_count",
         ledger.openWireCompoundVmapReplacementEventCount},
        {"open_wire_compound_current_member_split_ledger_vertex_candidate_wire_info_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerVertexCandidateWireInfoCount},
        {"open_wire_compound_current_member_split_ledger_vertex_debt_wire_info_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerVertexDebtWireInfoCount},
        {"open_wire_compound_current_member_split_ledger_member_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount},
        {"open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount},
        {"open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount},
        {"open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount},
        {"open_wire_compound_current_member_split_ledger_output_distinct_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount},
        {"open_wire_compound_current_member_split_ledger_candidate_distinct_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount},
        {"open_wire_compound_current_member_split_ledger_candidate_vertex_multiplicity_loss_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerCandidateVertexMultiplicityLossCount},
        {"open_wire_compound_current_member_split_ledger_output_other_output_matched_vertex_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerOutputOtherOutputMatchedVertexCount},
        {"open_wire_compound_current_member_split_ledger_candidate_other_output_matched_vertex_"
         "count",
         ledger.openWireCompoundCurrentMemberSplitLedgerCandidateOtherOutputMatchedVertexCount},
        {"open_wire_compound_current_member_split_ledger_candidate_vertex_reuse_risk_count",
         ledger.openWireCompoundCurrentMemberSplitLedgerCandidateVertexReuseRiskCount},
        {"open_wire_compound_missing_child_wire_history_edge_info_count",
         ledger.openWireCompoundMissingChildWireHistoryEdgeInfoCount},
        {"open_wire_compound_root_current_member_producer_output_wire_info_count",
         ledger.openWireCompoundRootCurrentMemberProducerOutputWireInfoCount},
        {"open_wire_compound_source_shared_vertex_wire_info_count",
         ledger.openWireCompoundSourceSharedVertexWireInfoCount},
        {"ordered_wire_info_count", ledger.orderedWireInfoCount},
        {"ordered_vertex_count", ledger.orderedVertexCount},
        {"iteration2_marked_edge_info_count", ledger.iteration2MarkedEdgeInfoCount},
        {"branch_search_candidate_count", ledger.branchSearchCandidateCount},
        {"branch_search_seed_wire_info_count", ledger.branchSearchSeedWireInfoCount},
        {"branch_search_inside_candidate_count", ledger.branchSearchInsideCandidateCount},
        {"new_wire_seed_candidate_count", ledger.newWireSeedCandidateCount},
        {"new_wire_seed_wire_info_count", ledger.newWireSeedWireInfoCount},
        {"split_wire_candidate_count", ledger.splitWireCandidateCount},
        {"split_wire_edge_info_count", ledger.splitWireEdgeInfoCount},
        {"done_wire_info_count", ledger.doneWireInfoCount},
        {"done_owned_edge_info_count", ledger.doneOwnedEdgeInfoCount},
        {"owner_propagation_candidate_count", ledger.ownerPropagationCandidateCount},
        {"owner_propagation_other_wire_candidate_count",
         ledger.ownerPropagationOtherWireCandidateCount},
        {"owner_propagation_other_wire_live_edge_info_count",
         ledger.ownerPropagationOtherWireLiveEdgeInfoCount},
        {"exhaust_seed_edge_info_count", ledger.exhaustSeedEdgeInfoCount},
        {"exhaust_shared_owner_edge_info_count", ledger.exhaustSharedOwnerEdgeInfoCount},
        {"exhaust_done_secondary_edge_info_count", ledger.exhaustDoneSecondaryEdgeInfoCount},
        {"exhaust_search_candidate_edge_info_count", ledger.exhaustSearchCandidateEdgeInfoCount},
        {"exhaust_secondary_owner_edge_info_count", ledger.exhaustSecondaryOwnerEdgeInfoCount},
    };
}

nlohmann::json wireJoinerHistoryDetailToJson(const part::WireJoinerHistorySummary& history)
{
    nlohmann::json wireJoinerHistoryEvents = nlohmann::json::array();
    for (const part::WireJoinerHistoryEvent& event : history.historyEvents) {
        wireJoinerHistoryEvents.push_back({
            {"event_index", event.eventIndex},
            {"open_export_index", event.openExportIndex},
            {"edge_info_index", event.edgeInfoIndex},
            {"open_wire_compound_child_wire_info_index", event.openWireCompoundChildWireInfoIndex},
            {"relation", part::wireJoinerHistoryRelationName(event.relation)},
            {"relation_from_child_wire_ledger", event.relationFromChildWireLedger},
            {"source_edge_indices", event.sourceEdgeIndices},
            {"source_lineage_from_splitter_history", event.sourceLineageFromSplitterHistory},
            {"no_original_purged_by_ledger", event.noOriginalPurgedByLedger},
            {"split_fragment_from_modified_history", event.splitFragmentFromModifiedHistory},
            {"split_fragment_from_generated_history", event.splitFragmentFromGeneratedHistory},
        });
    }

    nlohmann::json openExportHistoryEntries = nlohmann::json::array();
    for (const part::WireJoinerOpenExportHistoryEntry& entry : history.openExportEntries) {
        nlohmann::json currentMemberSplitOutputVertexDebt = nlohmann::json::array();
        for (const part::WireJoinerEndpointIdentityDebt& debt :
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
                {"member_split_ledger_vertex_identity", debt.memberSplitLedgerVertexIdentity},
                {"candidate_wire_vertex_identity", debt.candidateWireVertexIdentity},
                {"mismatch_reason", debt.mismatchReason},
            });
        }

        nlohmann::json vmapReplacementEvents = nlohmann::json::array();
        for (const part::WireJoinerVmapReplacementEvent& event :
             entry.openWireCompoundVmapReplacementEvents) {
            vmapReplacementEvents.push_back({
                {"event_index", event.eventIndex},
                {"affected_source_edge_index", event.affectedSourceEdgeIndex},
                {"affected_child_wire_edge_info_index", event.affectedChildWireEdgeInfoIndex},
                {"affected_endpoint", event.affectedEndpoint},
                {"affected_source_endpoint", event.affectedSourceEndpoint},
                {"affected_child_wire_endpoint", event.affectedChildWireEndpoint},
                {"replacement_source_edge_index", event.replacementSourceEdgeIndex},
                {"replacement_source_endpoint", event.replacementSourceEndpoint},
                {"replacement_from_mutable_source_edge_ledger",
                 event.replacementFromMutableSourceEdgeLedger},
                {"replacement_from_split_fragment_ledger", event.replacementFromSplitFragmentLedger},
            });
        }

        openExportHistoryEntries.push_back({
            {"open_export_index", entry.openExportIndex},
            {"edge_info_index", entry.edgeInfoIndex},
            {"open_wire_compound_export_source",
             part::openWireCompoundExportSourceName(entry.openWireCompoundExportSource)},
            {"open_wire_compound_edge_info_iteration", entry.openWireCompoundEdgeInfoIteration},
            {"open_wire_compound_edge_info_iteration2", entry.openWireCompoundEdgeInfoIteration2},
            {"open_wire_compound_owner_wire_info", entry.openWireCompoundOwnerWireInfo},
            {"open_wire_compound_owner_wire_info2", entry.openWireCompoundOwnerWireInfo2},
            {"open_wire_compound_open_leaf_export", entry.openWireCompoundOpenLeafExport},
            {"open_wire_compound_unowned_open_edge_export",
             entry.openWireCompoundUnownedOpenEdgeExport},
            {"open_wire_compound_root_current_member_child_producer",
             entry.openWireCompoundRootCurrentMemberChildProducer},
            {"open_wire_compound_child_shape_identity_recorded",
             entry.openWireCompoundChildShapeIdentityRecorded},
            {"open_wire_compound_child_wire_edge_count", entry.openWireCompoundChildWireEdgeCount},
            {"open_wire_compound_child_wire_vertex_count", entry.openWireCompoundChildWireVertexCount},
            {"wire_joiner_history_relation",
             entry.historyRelationFromChildWireLedger
                 ? part::wireJoinerHistoryRelationName(entry.historyRelation)
                 : ""},
            {"wire_joiner_history_relation_from_child_wire_ledger",
             entry.historyRelationFromChildWireLedger},
            {"wire_joiner_history_event_index", entry.wireJoinerHistoryEventIndex},
            {"wire_joiner_history_event_from_child_wire_ledger",
             entry.wireJoinerHistoryEventFromChildWireLedger},
            {"result_wire_producer_kind",
             part::resultWireProducerKindName(entry.resultWireProducer.kind)},
            {"result_wire_producer_state",
             part::resultWireProducerStateName(entry.resultWireProducer.state)},
            {"result_wire_producer_blocker",
             part::resultWireBlockerName(entry.resultWireProducer.blocker)},
            {"result_wire_producer_source_edge_info_index",
             entry.resultWireProducer.sourceEdgeInfoIndex},
            {"result_wire_producer_root_edge_info_index", entry.resultWireProducer.rootEdgeInfoIndex},
            {"result_wire_producer_current_member_edge_info_index",
             entry.resultWireProducer.currentMemberEdgeInfoIndex},
            {"result_wire_producer_child_wire_info_index", entry.resultWireProducer.childWireInfoIndex},
            {"open_wire_compound_child_wire_info_index", entry.openWireCompoundChildWireInfoIndex},
            {"open_wire_compound_source_edge_indices", entry.openWireCompoundSourceEdgeIndices},
            {"open_wire_compound_source_lineage_from_splitter_history",
             entry.openWireCompoundSourceLineageFromSplitterHistory},
            {"open_wire_compound_no_original_purge_match", entry.openWireCompoundNoOriginalPurgeMatch},
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
            {"open_wire_compound_vmap_replacement_events", std::move(vmapReplacementEvents)},
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
             std::move(currentMemberSplitOutputVertexDebt)},
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
            {"source_vertex_identity_any",
             entry.sourceVertexIdentity[0] || entry.sourceVertexIdentity[1]},
            {"source_vertex_identity_all",
             entry.sourceVertexIdentity[0] && entry.sourceVertexIdentity[1]},
            {"source_vertex_replacement_source_edge_indices",
             entry.sourceVertexReplacementSourceEdgeIndices},
            {"source_vertex_replacement_endpoints", entry.sourceVertexReplacementEndpoints},
            {"source_vertex_replacement_identity", entry.sourceVertexReplacementIdentity},
        });
    }

    return {
        {"source_edge_count", history.sourceEdgeCount},
        {"split_result_edge_count", history.splitResultEdgeCount},
        {"wire_joiner_history_event_count", history.historyEvents.size()},
        {"wire_joiner_history_event_from_child_wire_ledger_count",
         history.historyEventFromChildWireLedgerCount},
        {"wire_joiner_history_events", std::move(wireJoinerHistoryEvents)},
        {"open_export_history_entries", std::move(openExportHistoryEntries)},
        {"modified_source_edge_count", history.modifiedSourceEdgeCount},
        {"modified_history_count", history.modifiedHistoryCount},
        {"generated_history_count", history.generatedHistoryCount},
        {"deleted_history_count", history.deletedHistoryCount},
        {"splitter_history", history.splitterHistory},
    };
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
            sketchInternalHistoryContext(input.faceMakerHistory, input.wireJoinerHistory)
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
    if (input.wireJoinerLedger) {
        result.objectFields["wire_joiner_ledger"] = wireJoinerLedgerToJson(*input.wireJoinerLedger);
        result.objectFields["wire_joiner_history"]
            = "history_partial:edge_info_wire_info_split_done_exhaust";
    }
    if (input.wireJoinerHistory) {
        result.objectFields["wire_joiner_history_detail"] = wireJoinerHistoryDetailToJson(
            *input.wireJoinerHistory
        );
    }
    if (input.faceMakerHistory) {
        result.objectFields["facemaker_history"] = faceMakerHistoryToJson(*input.faceMakerHistory);
        result.objectFields["facemaker_history_status"] = "history_evidence:facemaker_buildface";
    }

    return result;
}

}  // namespace cad_core::sketcher
