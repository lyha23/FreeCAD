#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::geometry {

inline constexpr std::size_t resultWireProducerNpos = static_cast<std::size_t>(-1);

enum class ResultWireProducerKind {
    None,
    ExistingSourceEdge,
    PartialSharedClosedWire,
    LiveResetOpenEdge,
    SuperEdgeRoot,
    CurrentMemberChildWire,
};

enum class ResultWireProducerState {
    LegacyHelperCandidate,
    ProducerLocated,
    AHistoryEvidenceReady,
    ChildWireReady,
    SourceShapeReady,
    ExportedWithoutHelper,
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::buildClosedWire() marks removed targets with "vertex.edgeInfo()->iteration = -1"
// but records the producer source separately through "aHistory->Remove(info.edge)".
enum class ResultWireBlocker {
    None,
    MissingSourceLineage,
    MissingAHistoryRemoveSource,
    ForeignAHistorySourceLineage,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records the actual producer with
    // "aHistory->Remove(info.edge)"; if that foreign source is already source-shape ready, the
    // remaining blocker is lineage mismatch, not missing producer geometry.
    ForeignAHistorySourceShapeReadyLineageMismatch,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records the actual foreign producer with
    // "aHistory->Remove(info.edge)". If that producer has a matching EdgeInfo but no source-shaped
    // result-wire output, the gap is source-shape identity readiness, not missing lineage evidence.
    ForeignAHistorySourceShapeIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() stores the result-wire producer as
    // "aHistory->Remove(info.edge)". If that foreign producer curve cannot represent the helper
    // result edge, the blocker is geometry ownership, not missing source-shape identity.
    ForeignAHistorySourceGeometryMismatch,
    MissingRemovedTargetEvidence,
    MissingFullAHistoryProducerEvidence,
    FinalGateBlockedByIteration,
    FinalGateBlockedByWireInfo,
    RootRemovedByUnownedBranch,
    RootRemovedByPrimaryBranch,
    RootRemovedBySecondaryBranch,
    MultiMemberRootPendingSuppression,
    SourceShapeIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires() with noOriginal erases wires whose edges all match
    // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))".
    SourceShapeWouldPurgeOriginal,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() can export live open edges through the final openWireCompound gate;
    // ::getOpenWires(noOriginal=true) then removes children matching original source edges.
    LiveResetSourceShapeWouldPurgeOriginal,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() marks current members with "iteration = -1",
    // then ::build() exports the open root/member child wire before noOriginal purge is applied.
    CurrentMemberSourceShapeWouldPurgeOriginal,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() writes the true producer through
    // "aHistory->Remove(info.edge)"; a foreign Remove source plus a same-lineage strict sidecar
    // still needs source-shape identity before it can replace the helper child.
    SameSourceSidecarSourceShapeIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() stores producer evidence with
    // "aHistory->Remove(info.edge)". If a same-lineage strict sidecar is already source-shaped but
    // its edge curve does not match or contain this result edge, the blocker is geometry ownership,
    // not missing source-shape identity.
    SameSourceSidecarGeometryMismatch,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports final "info.wire()" children; current-member replacement
    // must not introduce vertices that are absent from the request-local openWireCompound ledger.
    SourceShapeMemberVertexIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets member edges to "iteration = -1" while the
    // open root can still satisfy ::build()'s openWireCompound gate. A member helper slot cannot
    // switch output until it is represented by the child-wire producer ledger, not just by the
    // edge-level root-open classifier.
    CurrentMemberChildWireIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() supplies strict producer sidecars only through
    // "aHistory->Remove(info.edge)". Keep this as a fallback for no-sidecar cases that cannot be
    // classified as FreeCAD's open-root "first->superEdge" producer path.
    CurrentMemberMissingSidecarEvidence,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores an open root with "first->superEdge";
    // ::build() then exports that root through openWireCompound without an aHistory sidecar. Until
    // cad-core has a formal root-open current-member producer ledger, this is not a missing sidecar.
    CurrentMemberRootOpenProducerNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() marks current members with "iteration = -1" while
    // strict sidecars may still record "aHistory->Remove(info.edge)". If such a source-shaped
    // sidecar cannot represent the member result edge, the remaining blocker is geometry ownership,
    // not missing child-wire evidence.
    CurrentMemberSidecarGeometryMismatch,
    LegacyHelperShapeStillUsed,
    UnknownInvariant,
};

const char* resultWireProducerKindName(ResultWireProducerKind kind);
const char* resultWireProducerStateName(ResultWireProducerState state);
const char* resultWireBlockerName(ResultWireBlocker blocker);

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::buildClosedWire() records producer evidence with "aHistory->Remove(info.edge)"
// and ::WireJoinerP::build() exports only final "info.wire()" states into openWireCompound.
// This identity keeps result-wire producer state finite while cad-core replaces helper output.
struct ResultWireProducerIdentity {
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::LegacyHelperCandidate;
    ResultWireBlocker blocker = ResultWireBlocker::None;
    std::size_t sourceEdgeInfoIndex = resultWireProducerNpos;
    std::size_t rootEdgeInfoIndex = resultWireProducerNpos;
    std::size_t currentMemberEdgeInfoIndex = resultWireProducerNpos;
    std::size_t childWireInfoIndex = resultWireProducerNpos;
    bool hasSourceLineage = false;
    bool hasStrictRemoveSource = false;
    bool hasRemovedTarget = false;
    bool hasSameSourceRemoveLineage = false;
    bool hasFullAHistoryEvidence = false;
    bool finalGateEligible = false;
    bool childWireBuilt = false;
    bool sourceShapeReady = false;
};

struct ResultWireProducerLedgerEntry {
    std::size_t openExportIndex = 0;
    std::size_t sourceEdgeInfoIndex = resultWireProducerNpos;
    std::size_t rootEdgeInfoIndex = resultWireProducerNpos;
    std::size_t currentMemberEdgeInfoIndex = resultWireProducerNpos;
    std::size_t childWireInfoIndex = resultWireProducerNpos;
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::LegacyHelperCandidate;
    ResultWireBlocker blocker = ResultWireBlocker::None;
};

struct WireJoinerLedgerSummary {
    std::size_t edgeInfoCount = 0;
    std::size_t splitEdgeInfoCount = 0;
    std::size_t primaryOwnedEdgeInfoCount = 0;
    std::size_t secondaryOwnedEdgeInfoCount = 0;
    std::size_t closedWireAssignedEdgeInfoCount = 0;
    std::size_t closedWireInfoCount = 0;
    std::size_t closedWireVertexCount = 0;
    std::size_t closedWireSearchStackFrameCount = 0;
    std::size_t closedWireSearchVertexStackCount = 0;
    std::size_t closedWireSearchEdgeSetVisitCount = 0;
    std::size_t closedWireSearchBacktrackCount = 0;
    std::size_t closedWireSearchIntersectSkipCount = 0;
    std::size_t tightBoundDoneWireInfoCount = 0;
    std::size_t tightBoundSplitWireInfoCount = 0;
    std::size_t tightBoundNewWireCandidateCount = 0;
    std::size_t tightBoundNewWireVertexCount = 0;
    std::size_t tightBoundOwnerTransferCandidateEdgeInfoCount = 0;
    std::size_t tightBoundTransferWireInfoCount = 0;
    std::size_t tightBoundTransferWireVertexCount = 0;
    std::size_t tightBoundTransferredOwnerEdgeInfoCount = 0;
    std::size_t tightBoundSplitOwnerWireInfoCount = 0;
    std::size_t tightBoundSplitOwnerVertexCount = 0;
    std::size_t tightBoundSplitOwnerBuiltWireCount = 0;
    std::size_t tightBoundLiveSplitWireInfoCount = 0;
    std::size_t tightBoundLiveSplitWireEdgeInfoCount = 0;
    std::size_t tightBoundSplitWireVertexCount = 0;
    std::size_t tightBoundSplitWireBuiltCount = 0;
    std::size_t tightBoundExistingWireSearchCount = 0;
    std::size_t tightBoundExistingWireHitCount = 0;
    std::size_t tightBoundExistingWireReverseHitCount = 0;
    std::size_t tightBoundExistingWirePurgeCount = 0;
    std::size_t tightBoundPurgedWireInfoCount = 0;
    std::size_t tightBoundExhaustVisitedWireInfoCount = 0;
    std::size_t tightBoundExhaustDoneWireInfoCount = 0;
    std::size_t tightBoundExhaustDiscardedPurgedWireInfoCount = 0;
    std::size_t tightBoundExhaustPrimaryResetEdgeInfoCount = 0;
    std::size_t tightBoundExhaustPrimaryResetBlockedEdgeInfoCount = 0;
    std::size_t tightBoundFullWireSetInsertCount = 0;
    std::size_t tightBoundFullWireSetEraseCount = 0;
    std::size_t tightBoundFullWireSetAbortCount = 0;
    std::size_t tightBoundFullWireSetPurgeCandidateCount = 0;
    std::size_t tightBoundFullWireSetBlockedTransferCount = 0;
    std::size_t tightBoundFullWireSetAbortSearchCount = 0;
    std::size_t tightBoundFullWireSetAbortResolvedByHitCount = 0;
    std::size_t tightBoundFullWireSetAbortBlockedSearchCount = 0;
    std::size_t tightBoundExistingWireMultiRoundWireInfoCount = 0;
    std::size_t tightBoundExistingWireMultiRoundSearchCount = 0;
    std::size_t repeatedSplitExhaustCycleCount = 0;
    std::size_t repeatedSplitExhaustRemovedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRemovedUnownedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRemovedSecondaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRemovedPrimaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunActiveEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunNoActiveSearchCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireSearchCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireMissCount = 0;
    std::size_t repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireVertexCount = 0;
    std::size_t repeatedSplitExhaustRerunResettableClosedWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveClosedWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveClosedWireVertexCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() reruns "findClosedWires(true); findTightBound()" after
    // consumed-edge removal. These fields track the live rerun owners that continue into the
    // repeated findTightBound branch/transfer lifecycle.
    std::size_t repeatedSplitExhaustRerunLiveBranchSearchCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveBranchSearchOutsideCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveTransferWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveTransferredOwnerEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLiveDoneWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalScanCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalUnownedEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalSecondaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalPrimaryEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunLoopExitNoRemovalCount = 0;
    std::size_t repeatedSplitExhaustRerunBranchSearchCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunBranchSearchInsideCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunBranchSearchOutsideCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunNewWireSeedCandidateCount = 0;
    std::size_t repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount = 0;
    std::size_t tightBoundExistingWireSearchStackFrameCount = 0;
    std::size_t tightBoundExistingWireSearchVertexStackCount = 0;
    std::size_t tightBoundExistingWireSearchEdgeSetVisitCount = 0;
    std::size_t tightBoundExistingWireSearchBacktrackCount = 0;
    std::size_t tightBoundExistingWireSearchIntersectSkipCount = 0;
    std::size_t tightBoundExistingWireSearchIdxVertexCount = 0;
    std::size_t tightBoundExistingWireSearchStackPosCount = 0;
    std::size_t tightBoundExistingWireSearchPathVertexCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWiresWithExisting() writes "idxVertex" / "stackPos" for
    // existing-wire hits; ::findTightBoundWithSplit() later consumes only the selected transfer path.
    std::size_t tightBoundExistingWireSelectedHitCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyHitCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyIdxVertexCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyStackPosCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyPathBlockedCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyOwnerVertexBlockedCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyOrderBlockedCount = 0;
    std::size_t tightBoundExistingWireSearchOnlyWireBuildBlockedCount = 0;
    std::size_t tightBoundExistingWireIdxVertexCount = 0;
    std::size_t tightBoundExistingWireStackPosCount = 0;
    std::size_t sourceIdentitySharedVertexEdgeInfoCount = 0;
    std::size_t sourceIdentityOnlySourceVerticesEdgeInfoCount = 0;
    std::size_t sourceIdentityOpenExportSharedVertexEdgeInfoCount = 0;
    std::size_t sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount = 0;
    std::size_t sourceIdentityPurgeBridgeEdgeInfoCount = 0;
    std::size_t sourceLineageEdgeInfoCount = 0;
    std::size_t sourceLineageSplitEdgeInfoCount = 0;
    std::size_t sourceLineageOpenExportEdgeInfoCount = 0;
    std::size_t sourceLineageMissingOpenExportEdgeInfoCount = 0;
    std::size_t sourceLineageMultiSourceEdgeInfoCount = 0;
    std::size_t superEdgeCandidateCount = 0;
    std::size_t superEdgeCandidateEdgeInfoCount = 0;
    std::size_t superEdgeRootEdgeInfoCount = 0;
    std::size_t superEdgeClosedCandidateCount = 0;
    std::size_t superEdgeOpenCandidateCount = 0;
    std::size_t superEdgeMaterializedRootEdgeInfoCount = 0;
    std::size_t superEdgeMaterializedEdgeInfoCount = 0;
    std::size_t superEdgeShadowedMemberEdgeInfoCount = 0;
    std::size_t superEdgeLifecycleMemberMinusOneEdgeInfoCount = 0;
    std::size_t superEdgeLifecycleOpenRootEdgeInfoCount = 0;
    std::size_t superEdgeLifecycleClosedRootEdgeInfoCount = 0;
    std::size_t superEdgeLifecycleAdjacentRangeRewriteCount = 0;
    std::size_t superEdgeLifecycleEndpointRewriteCount = 0;
    std::size_t superEdgeLifecycleAdjacentRangeSourceEdgeInfoCount = 0;
    std::size_t superEdgeLifecycleAdjacentRangeVertexCount = 0;
    std::size_t openExportEdgeInfoCount = 0;
    std::size_t openWireCompoundWireInfoCount = 0;
    std::size_t openWireCompoundBuiltWireInfoCount = 0;
    std::size_t openWireCompoundEdgeInfoCount = 0;
    std::size_t openWireCompoundSuperEdgeWireInfoCount = 0;
    std::size_t openWireCompoundPurgeBridgeWireInfoCount = 0;
    std::size_t openWireCompoundSourceSharedVertexWireInfoCount = 0;
    std::size_t openWireCompoundPurgeBridgeSourceSharedVertexWireInfoCount = 0;
    std::size_t openWireCompoundPurgeBridgeUnmatchedWireInfoCount = 0;
    std::size_t orderedWireInfoCount = 0;
    std::size_t orderedVertexCount = 0;
    std::size_t iteration2MarkedEdgeInfoCount = 0;
    std::size_t branchSearchCandidateCount = 0;
    std::size_t branchSearchSeedWireInfoCount = 0;
    std::size_t branchSearchInsideCandidateCount = 0;
    std::size_t branchSearchOutsideCandidateCount = 0;
    std::size_t newWireSeedCandidateCount = 0;
    std::size_t newWireSeedWireInfoCount = 0;
    std::size_t splitWireCandidateCount = 0;
    std::size_t splitWireEdgeInfoCount = 0;
    std::size_t doneWireInfoCount = 0;
    std::size_t doneOwnedEdgeInfoCount = 0;
    std::size_t ownerPropagationCandidateCount = 0;
    std::size_t ownerPropagationUnassignedCandidateCount = 0;
    std::size_t ownerPropagationOtherWireCandidateCount = 0;
    std::size_t ownerPropagationOtherWireLiveEdgeInfoCount = 0;
    std::size_t exhaustSeedEdgeInfoCount = 0;
    std::size_t exhaustSharedOwnerEdgeInfoCount = 0;
    std::size_t exhaustDoneSecondaryEdgeInfoCount = 0;
    std::size_t exhaustSearchCandidateEdgeInfoCount = 0;
    std::size_t exhaustSecondaryOwnerEdgeInfoCount = 0;
    std::size_t exhaustAdjacentSearchCount = 0;
    std::size_t exhaustAdjacentSearchHitCount = 0;
    std::size_t exhaustAdjacentSearchMissCount = 0;
    std::size_t exhaustAdjacentSearchStackFrameCount = 0;
    std::size_t exhaustAdjacentSearchVertexStackCount = 0;
    std::size_t exhaustAdjacentSearchEdgeSetVisitCount = 0;
    std::size_t exhaustAdjacentSearchBacktrackCount = 0;
    std::size_t exhaustAdjacentWireSetInsertCount = 0;
    std::size_t exhaustAdjacentWireSetEraseCount = 0;
    std::size_t exhaustAdjacentWireSetAbortCount = 0;
    std::size_t exhaustAdjacentWireInfo2AbortCount = 0;
    std::size_t resultWireProducerBlockerMissingSourceLineageCount = 0;
    std::size_t resultWireProducerBlockerMissingAHistoryRemoveSourceCount = 0;
    std::size_t resultWireProducerBlockerForeignAHistorySourceLineageCount = 0;
    std::size_t resultWireProducerBlockerForeignAHistorySourceShapeReadyLineageMismatchCount = 0;
    std::size_t resultWireProducerBlockerForeignAHistorySourceShapeIdentityNotReadyCount = 0;
    std::size_t resultWireProducerBlockerForeignAHistorySourceGeometryMismatchCount = 0;
    std::size_t resultWireProducerBlockerMissingRemovedTargetEvidenceCount = 0;
    std::size_t resultWireProducerBlockerMissingFullAHistoryProducerEvidenceCount = 0;
    std::size_t resultWireProducerBlockerFinalGateBlockedByIterationCount = 0;
    std::size_t resultWireProducerBlockerFinalGateBlockedByWireInfoCount = 0;
    std::size_t resultWireProducerBlockerRootRemovedByUnownedBranchCount = 0;
    std::size_t resultWireProducerBlockerRootRemovedByPrimaryBranchCount = 0;
    std::size_t resultWireProducerBlockerRootRemovedBySecondaryBranchCount = 0;
    std::size_t resultWireProducerBlockerMultiMemberRootPendingSuppressionCount = 0;
    std::size_t resultWireProducerBlockerSourceShapeIdentityNotReadyCount = 0;
    std::size_t resultWireProducerBlockerSourceShapeWouldPurgeOriginalCount = 0;
    std::size_t resultWireProducerBlockerLiveResetSourceShapeWouldPurgeOriginalCount = 0;
    std::size_t resultWireProducerBlockerCurrentMemberSourceShapeWouldPurgeOriginalCount = 0;
    std::size_t resultWireProducerBlockerSameSourceSidecarSourceShapeIdentityNotReadyCount = 0;
    std::size_t resultWireProducerBlockerSameSourceSidecarGeometryMismatchCount = 0;
    std::size_t resultWireProducerBlockerSourceShapeMemberVertexIdentityNotReadyCount = 0;
    std::size_t resultWireProducerBlockerCurrentMemberChildWireIdentityNotReadyCount = 0;
    std::size_t resultWireProducerBlockerCurrentMemberMissingSidecarEvidenceCount = 0;
    std::size_t resultWireProducerBlockerCurrentMemberRootOpenProducerNotReadyCount = 0;
    std::size_t resultWireProducerBlockerCurrentMemberSidecarGeometryMismatchCount = 0;
    std::size_t resultWireProducerUnknownInvariantCount = 0;
    std::size_t unownedRemovalReadySlotCount = 0;
    std::size_t unownedRemovalCurrentMemberProducerOutputCount = 0;
    std::size_t multiMemberRootDirectOutputCount = 0;
    std::vector<ResultWireProducerLedgerEntry> resultWireProducerLedgerEntries;
};

struct WireJoinerOpenExportHistoryEntry {
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() adds each final "info.wire()" to "openWireCompound", then
    // ::getOpenWires() consumes "MapperHistory(aHistory)" with sourceEdges. Keep per-child
    // source lineage next to the summary so topo can later consume WireJoiner-produced history
    // instead of deriving it from raw/internal geometry.
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = 0;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    bool helperOpenExportOverride = false;
    std::string helperOpenExportOverrideReason;
    bool purgeBridge = false;
    ResultWireProducerIdentity resultWireProducer;
};

struct WireJoinerHistorySummary {
    std::size_t sourceEdgeCount = 0;
    std::size_t splitResultEdgeCount = 0;
    std::size_t openExportEdgeCount = 0;
    std::size_t openExportSourceLineageEdgeCount = 0;
    std::size_t openExportMissingSourceLineageEdgeCount = 0;
    std::size_t openExportHelperOverrideEdgeCount = 0;
    std::size_t openExportHelperOverrideMissingSourceLineageEdgeCount = 0;
    std::size_t openExportPurgeBridgeEdgeCount = 0;
    std::vector<WireJoinerOpenExportHistoryEntry> openExportEntries;
    std::size_t modifiedSourceEdgeCount = 0;
    std::size_t modifiedHistoryCount = 0;
    std::size_t generatedHistoryCount = 0;
    std::size_t deletedHistoryCount = 0;
    bool splitterHistory = false;
    bool finalExportHistory = false;
};

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
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), calls splitEdges(), buildClosedWire(), findTightBound() and
    // exhaustTightBound() before exporting openWireCompound from final EdgeInfo ownership.
    // cad-core's current main path assigns primary owners from the closed path search; unsupported
    // owner gaps must be fixed by migrating FreeCAD's VertexInfo stack search, not graph fallback.
    void buildFinalEdgeOwnership(const TopoDS_Shape* boundedFaceShape = nullptr,
                                 const std::vector<TopoDS_Wire>* closedWires = nullptr,
                                 const std::vector<TopoDS_Edge>* openEdges = nullptr,
                                 bool splitProducedBoundedFaces = false);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), when noOriginal=true, builds a source compound from
    // sourceEdgeArray and removes open-wire edges whose vertices are still shared with source.
    void addSourceEdge(const TopoDS_Edge& edge);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(const std::string& historyPrefix, bool noOriginal = true) const;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls findTightBound()/exhaustTightBound(); this summary
    // exposes only the request-local EdgeInfo/WireInfo slot coverage used by tests and diagnostics,
    // not a frontend protocol contract or persisted graph state.
    WireJoinerLedgerSummary ledgerSummary() const;
    WireJoinerHistorySummary historySummary() const;

private:
    bool tightBound_ = false;
    bool mergeEdges_ = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo stores "iteration", "iteration2", "superEdge",
    // "wireInfo" and "wireInfo2" while splitEdges()/findTightBound()/exhaustTightBound()
    // move edges between WireInfo owners. The final openWireCompound uses edges with
    // "iteration == -3 || (!info.wireInfo && info.iteration >= 0)"; cad-core keeps that state
    // shape so later tight-bound migration can extend the same ledger.
    struct EdgeInfo {
        TopoDS_Edge edge;
        TopoDS_Wire superEdge;
        mutable TopoDS_Shape edgeReversed;
        mutable TopoDS_Shape superEdgeReversed;
        gp_Pnt p1;
        gp_Pnt p2;
        gp_Pnt mid;
        std::array<int, 2> iStart{{-1, -1}};
        std::array<int, 2> iEnd{{-1, -1}};
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) compares openWireCompound edges with
        // sourceEdgeArray by shared source vertices. Keep the endpoint identity ledger next to
        // EdgeInfo so purge compatibility can be removed only after source/split identity is complete.
        std::array<bool, 2> sourceVertexIdentity{{false, false}};
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() copies sourceEdgeArray into sourceEdges, then splitEdges() records
        // modified EdgeInfo shapes in aHistory before getOpenWires() consumes MapperHistory(aHistory).
        // This request-local lineage records which sourceEdgeArray entries produced this EdgeInfo.
        std::vector<std::size_t> sourceEdgeIndices;
        bool sourceLineageFromSplitterHistory = false;
        bool helperOpenExportOverride = false;
        std::string helperOpenExportOverrideReason;
        bool helperOpenExportOverrideSourceEdgeInfo = false;
        std::size_t helperOpenExportOverrideSourceEdgeInfoIndex = 0;
        bool helperOpenExportOverrideSourceEdgeInfoConsumed = false;
        bool helperOpenExportOverrideOpenWireCompoundEligibleEdgeInfo = false;
        bool helperOpenExportOverrideForcedOpenWireCompoundEdgeInfo = false;
        bool helperOpenExportOverrideSourceEdgeExportShape = false;
        bool helperOpenExportOverrideSourceEdgeProducerOutput = false;
        bool helperOpenExportOverrideFullAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeMemberEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootEdgeInfo = false;
        std::size_t helperOpenExportOverrideSuperEdgeRootEdgeInfoIndex = 0;
        bool helperOpenExportOverrideSuperEdgeRootOpenWireCompoundEligibleEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootOpenLifecycleEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootExportBlockedByIteration = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedUnownedRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedPrimaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedSecondaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidate = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateFullAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReady = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidatePrimaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateSecondaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberEdgeInfo = false;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoIndices;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoIndices;
        bool helperOpenExportOverrideExportBlockedByIteration = false;
        bool helperOpenExportOverrideExportBlockedByWireInfo = false;
        bool helperOpenExportOverrideRemovedSourceEdgeInfo = false;
        bool helperOpenExportOverrideRemovedTargetEdgeInfo = false;
        bool helperOpenExportOverrideAHistoryRemoveSourceEdgeInfo = false;
        std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeInfoIndices;
        std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeIndices;
        bool helperOpenExportOverrideAHistoryRemoveSourceLineage = false;
        bool helperOpenExportOverrideAHistoryRemoveSameSourceLineage = false;
        bool helperOpenExportOverrideAHistoryRemoveForeignSourceLineage = false;
        bool helperOpenExportOverrideSafeAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfo = false;
        std::vector<std::size_t> helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfoIndices;
        bool buildClosedWireRemoved = false;
        bool buildClosedWireRemovedByUnowned = false;
        bool buildClosedWireRemovedByPrimaryOwner = false;
        bool buildClosedWireRemovedBySecondaryOwner = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), counter removal sets "vertex.edgeInfo()->iteration = -1"
        // but calls "aHistory->Remove(info.edge)" with the outer EdgeInfo source.
        bool buildClosedWireAHistoryRemoved = false;
        std::vector<std::size_t> buildClosedWireAHistoryRemoveSourceEdgeInfoIndices;
        std::vector<std::size_t> buildClosedWireAHistoryRemoveSourceEdgeIndices;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports final EdgeInfo identity into "openWireCompound" by calling
        // "builder.Add(openWireCompound, info.wire())". While M3 is replacing the helper-generated
        // result edge source, keep the owner lifecycle on "edge" and override only the export shape
        // recorded in OpenWireCompoundWireInfo.
        std::optional<TopoDS_Edge> openExportOverride;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports final "info.wire()" children. Keep transitional
        // result-slot topology out of openExportOverride and use it only as request-local
        // child-wire vertex evidence for root/current-member producers.
        std::optional<TopoDS_Edge> resultSlotVertexEvidenceEdge;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdges(), "Join edges (let's call it super edge) that are connected
        // to only one other edges". The regular child-wire ledger can consume final superEdge wires;
        // helper root producer wires remain evidence until their M3 output boundary is switched.
        std::size_t superEdgeInfo = 0;
        std::size_t superEdgeMemberCount = 0;
        bool superEdgeRoot = false;
        bool superEdgeClosed = false;
        bool superEdgeMaterialized = false;
        bool superEdgeShadowedMember = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() assigns member edges "iteration = -1",
        // closed root super edges "iteration = -2", open root super edges the current iteration,
        // and rewrites the first endpoint adjacent range from the last endpoint. These fields keep
        // that lifecycle shadowed until cad-core can switch the live EdgeInfo/openWireCompound path.
        int superEdgeLifecycleIteration = 0;
        bool superEdgeLifecycleMemberMinusOne = false;
        bool superEdgeLifecycleOpenRoot = false;
        bool superEdgeLifecycleClosedRoot = false;
        bool superEdgeAdjacentRangeRewritten = false;
        bool superEdgeEndpointRewritten = false;
        int superEdgeEndpointRewriteIndex = -1;
        gp_Pnt superEdgeEndpointRewritePoint;
        std::size_t superEdgeAdjacentRangeSourceEdgeInfo = 0;
        int superEdgeAdjacentRangeSourceEndpoint = -1;
        int superEdgeAdjacentRangeStart = -1;
        int superEdgeAdjacentRangeEnd = -1;
        ResultWireProducerIdentity resultWireProducer;
        const TopoDS_Shape& shape(bool forward = true) const;
        TopoDS_Wire wire(bool forward = true) const;
        TopoDS_Wire openExportWire() const;
        const TopoDS_Edge& openExportEdge() const;
        bool hasOpenExportOverride() const;
        int iteration = 0;
        int iteration2 = 0;
        std::size_t wireInfo = 0;
        std::size_t wireInfo2 = 0;
        bool closedWireOwner = false;
        bool splitFromInputEdge = false;
        bool purgeAsOriginalOpenEdge = false;
        std::size_t branchCandidateCount = 0;
        std::size_t branchInsideCandidateCount = 0;
        std::size_t branchOutsideCandidateCount = 0;
        std::size_t newWireSeedCandidateCount = 0;
        std::size_t splitWireCandidateCount = 0;
        std::size_t ownerPropagationCandidateCount = 0;
        bool tightBoundOwnerTransferCandidate = false;
        bool tightBoundTransferredOwner = false;
        bool exhaustSeed = false;
        bool exhaustSharedOwner = false;
        bool exhaustDoneSecondary = false;
        bool exhaustSearchCandidate = false;
        bool exhaustSecondaryOwner = false;
    };
    struct WireVertex {
        std::size_t edgeIndex = 0;
        bool start = true;
        std::size_t branchCandidateCount = 0;
    };
    struct SuperEdgeInfo {
        std::size_t id = 0;
        TopoDS_Wire wire;
        std::vector<WireVertex> vertices;
        bool closed = false;
        bool materialized = false;
    };
    struct TightBoundBranchCandidate {
        WireVertex ownerVertex;
        WireVertex adjacentVertex;
        bool inside = false;
        bool transfersOwnerEdge = false;
    };
    struct TightBoundTransferWire {
        std::size_t id = 0;
        std::vector<WireVertex> vertices;
        std::vector<WireVertex> splitWireVertices;
        std::size_t transferredOwnerEdgeCount = 0;
        bool existingWireHit = false;
        int existingWireIdxVertex = -1;
        int existingWireStackPos = -1;
        bool wireBuilt = false;
        bool splitWireBuilt = false;
        bool done = false;
    };
    struct TightBoundTransferPath {
        std::vector<WireVertex> transferVertices;
        std::vector<WireVertex> splitOwnerVertices;
        std::vector<WireVertex> splitWireVertices;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::_findClosedWiresWithExisting() writes "idxVertex" and
        // "stackPos" for the existing-wire hit consumed by findTightBoundByVertices().
        bool existingWireHit = false;
        int existingWireIdxVertex = -1;
        int existingWireStackPos = -1;
    };
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices(), after "_findClosedWires(..., &idxEnd,
    // ..., &stackPos)", normalizes "idxEnd == 0" and enforces "ENSURE(idxV <= idxEnd)".
    enum class TightBoundExistingWirePathBlockReason
    {
        None,
        OwnerVertexMissing,
        OrderBlocked,
        WireBuildBlocked,
    };
    struct TightBoundExistingWireSearchTrace {
        bool hit = false;
        bool reverseHit = false;
        bool purge = false;
        int idxVertex = -1;
        int stackPos = -1;
        std::vector<WireVertex> hitPath;
        std::size_t stackFrameCount = 0;
        std::size_t vertexStackCount = 0;
        std::size_t edgeSetVisitCount = 0;
        std::size_t backtrackCount = 0;
        std::size_t intersectSkipCount = 0;
        std::size_t fullWireSetInsertCount = 0;
        std::size_t fullWireSetEraseCount = 0;
        std::size_t fullWireSetAbortCount = 0;
        std::size_t fullWireSetPurgeCandidateCount = 0;
    };
    struct OpenWireCompoundWireInfo {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build(), for each exportable EdgeInfo calls
        // "builder.Add(openWireCompound, info.wire())".
        std::size_t edgeIndex = 0;
        TopoDS_Wire wire;
        bool wireBuilt = false;
        bool superEdgeWire = false;
        bool helperOpenExportOverride = false;
        std::string helperOpenExportOverrideReason;
        bool helperOpenExportOverrideSourceEdgeInfo = false;
        std::size_t helperOpenExportOverrideSourceEdgeInfoIndex = 0;
        bool helperOpenExportOverrideSourceEdgeInfoConsumed = false;
        bool helperOpenExportOverrideOpenWireCompoundEligibleEdgeInfo = false;
        bool helperOpenExportOverrideForcedOpenWireCompoundEdgeInfo = false;
        bool helperOpenExportOverrideSourceEdgeExportShape = false;
        bool helperOpenExportOverrideSourceEdgeProducerOutput = false;
        bool helperOpenExportOverrideFullAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeMemberEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootEdgeInfo = false;
        std::size_t helperOpenExportOverrideSuperEdgeRootEdgeInfoIndex = 0;
        bool helperOpenExportOverrideSuperEdgeRootOpenWireCompoundEligibleEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidate = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReady = false;
        TopoDS_Wire helperOpenExportOverrideSuperEdgeRootResultWireProducerWire;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerWireBuilt = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerOutput = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerUnownedRemovalChildWireProducerReadyOutput = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppression = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerOutputBlockedByMultiMemberSuperEdge = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberEdgeInfo = false;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoIndices;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoIndices;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() stores one root "superEdge" after setting
        // each member "current->iteration = -1". This candidate keeps only the current child
        // member wire so M3 can suppress sibling members without exporting the full root superEdge.
        TopoDS_Wire helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedWire;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedWireBuilt = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputCandidate = false;
        bool
            helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedByPendingMember =
                false;
        bool
            helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShape =
                false;
        bool
            helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberChildWireProducerReady =
                false;
        bool
            helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberChildWireProducerFullAHistoryEvidence =
                false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutput = false;
        bool helperOpenExportOverrideRemovedSourceEdgeInfo = false;
        bool helperOpenExportOverrideRemovedTargetEdgeInfo = false;
        bool helperOpenExportOverrideAHistoryRemoveSourceEdgeInfo = false;
        std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeInfoIndices;
        std::vector<std::size_t> helperOpenExportOverrideAHistoryRemoveSourceEdgeIndices;
        bool helperOpenExportOverrideAHistoryRemoveSourceLineage = false;
        bool helperOpenExportOverrideAHistoryRemoveSameSourceLineage = false;
        bool helperOpenExportOverrideAHistoryRemoveForeignSourceLineage = false;
        bool helperOpenExportOverrideSafeAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfo = false;
        std::vector<std::size_t> helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfoIndices;
        bool purgeBridge = false;
        bool sourceSharedVertexPurgeMatch = false;
        ResultWireProducerIdentity resultWireProducer;
    };
    struct HelperOpenExportOverrideBinding {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports result-wire identity from final EdgeInfo states before
        // ::getOpenWires() consumes "MapperHistory(aHistory)". This binding records the legacy
        // result-slot edge used to locate the pre-existing EdgeInfo it mirrors; producer identity
        // must come from EdgeInfo/WireInfo/aHistory, not from this locator edge.
        TopoDS_Edge resultSlotEdge;
        std::string reason;
        std::vector<std::size_t> sourceEdgeInfoCandidateIndices;
        std::vector<std::size_t> openWireCompoundEligibleCandidateIndices;
    };
    struct HelperOpenExportOverridePlan {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() reruns findClosedWires(true)/findTightBound() before final
        // openWireCompound export. The rerun gate must know whether result-wire identity is still a
        // transitional helper override before it mutates live EdgeInfo owners.
        bool needed = false;
        std::size_t candidateEdgeCount = 0;
        std::size_t unboundEdgeCount = 0;
        std::vector<HelperOpenExportOverrideBinding> bindings;
    };
    struct OwnerWireInfo {
        std::size_t id = 0;
        std::size_t splitWireId = 0;
        TopoDS_Wire wire;
        std::vector<WireVertex> vertices;
        std::vector<WireVertex> splitOwnerVertices;
        std::vector<TightBoundBranchCandidate> branchCandidates;
        std::vector<TightBoundTransferWire> transferWires;
        bool hasNewWireSeed = false;
        bool hasSplitWireCandidate = false;
        bool splitOwnerWireBuilt = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::_findClosedWiresWithExisting() and ::_findClosedWiresUpdateStack()
        // mark "wireInfo->purge = true"; ::exhaustTightBoundUpdateEdge() later discards that
        // WireInfo instead of marking it done. cad-core records the lifecycle bit as diagnostics
        // only until the full repeated split/exhaust loop is migrated.
        bool purge = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::exhaustTightBoundUpdateEdge(), "if (wireInfo->purge) ... wireInfo.reset();
        // else wireInfo->done = true". This sidecar records the FreeCAD-equivalent exhaust
        // transition while the legacy "done" bit still feeds the current output-neutral bridge.
        bool exhaustVisited = false;
        bool exhaustDone = false;
        bool exhaustDiscardedByPurge = false;
        bool done = false;
        std::size_t splitWireCandidateCount = 0;
        std::size_t branchSearchCandidateCount = 0;
        std::size_t branchSearchInsideCandidateCount = 0;
        std::size_t branchSearchOutsideCandidateCount = 0;
        std::size_t closedWireSearchStackFrameCount = 0;
        std::size_t closedWireSearchVertexStackCount = 0;
        std::size_t closedWireSearchEdgeSetVisitCount = 0;
        std::size_t closedWireSearchBacktrackCount = 0;
        std::size_t closedWireSearchIntersectSkipCount = 0;
        std::size_t tightBoundExistingWireSearchCount = 0;
        std::size_t tightBoundExistingWireHitCount = 0;
        std::size_t tightBoundExistingWireReverseHitCount = 0;
        std::size_t tightBoundExistingWirePurgeCount = 0;
        std::size_t tightBoundExistingWireSearchStackFrameCount = 0;
        std::size_t tightBoundExistingWireSearchVertexStackCount = 0;
        std::size_t tightBoundExistingWireSearchEdgeSetVisitCount = 0;
        std::size_t tightBoundExistingWireSearchBacktrackCount = 0;
        std::size_t tightBoundExistingWireSearchIntersectSkipCount = 0;
        std::size_t tightBoundExistingWireSearchIdxVertexCount = 0;
        std::size_t tightBoundExistingWireSearchStackPosCount = 0;
        std::size_t tightBoundExistingWireSearchPathVertexCount = 0;
        std::size_t tightBoundExistingWireSearchOnlyPathBlockedCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::_findClosedWiresWithExisting() writes idxVertex/stackPos; a hit is only
        // selectable by ::findTightBoundWithSplit() if it maps to an ordered owner vertex range
        // that can build the transfer wire.
        std::size_t tightBoundExistingWireSearchOnlyOwnerVertexBlockedCount = 0;
        std::size_t tightBoundExistingWireSearchOnlyOrderBlockedCount = 0;
        std::size_t tightBoundExistingWireSearchOnlyWireBuildBlockedCount = 0;
        std::size_t tightBoundFullWireSetInsertCount = 0;
        std::size_t tightBoundFullWireSetEraseCount = 0;
        std::size_t tightBoundFullWireSetAbortCount = 0;
        std::size_t tightBoundFullWireSetPurgeCandidateCount = 0;
        std::size_t tightBoundFullWireSetBlockedTransferCount = 0;
        std::size_t tightBoundFullWireSetAbortSearchCount = 0;
        std::size_t tightBoundFullWireSetAbortResolvedByHitCount = 0;
        std::size_t tightBoundFullWireSetAbortBlockedSearchCount = 0;
    };
    struct ClosedWireSearchFrame {
        std::size_t start = 0;
        std::size_t current = 0;
        std::size_t end = 0;
    };
    struct ClosedWireSearchResult {
        std::vector<WireVertex> vertices;
        std::size_t stackFrameCount = 0;
        std::size_t vertexStackCount = 0;
        std::size_t edgeSetVisitCount = 0;
        std::size_t backtrackCount = 0;
        std::size_t intersectSkipCount = 0;
    };
    struct ExhaustAdjacentSearchTrace {
        bool hit = false;
        std::size_t stackFrameCount = 0;
        std::size_t vertexStackCount = 0;
        std::size_t edgeSetVisitCount = 0;
        std::size_t backtrackCount = 0;
        std::size_t wireSetInsertCount = 0;
        std::size_t wireSetEraseCount = 0;
        std::size_t wireSetAbortCount = 0;
        std::size_t wireInfo2AbortCount = 0;
    };
    struct WireInfo {
        std::size_t id = 0;
        TopoDS_Wire wire;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::WireInfo owns ordered EdgeInfo vertices for tight-bound wires.
        // cad-core keeps the ordered request-local vertex ledger alongside edge states so
        // findTightBound()/exhaustTightBound() can migrate onto the same shape of state.
        std::vector<EdgeInfo> edges;
        std::vector<WireVertex> orderedVertices;
        std::vector<WireVertex> adjacentVertices;
        std::vector<SuperEdgeInfo> superEdges;
        std::vector<OwnerWireInfo> ownerWires;
        std::vector<OpenWireCompoundWireInfo> openWireCompoundWires;
        bool hasNewWireSeed = false;
        bool hasSplitWireCandidate = false;
        bool done = false;
        std::size_t splitWireCandidateCount = 0;
        std::size_t ownerPropagationCandidateCount = 0;
        std::size_t ownerPropagationUnassignedCandidateCount = 0;
        std::size_t ownerPropagationOtherWireCandidateCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findTightBoundUpdateVertices(), "info->wireInfo = beginInfo.wireInfo"
        // when an unfinished "otherWire" is overtaken by the just-completed tight-bound owner.
        std::size_t ownerPropagationOtherWireLiveEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::exhaustTightBoundWithAdjacent() seeds "wireSet" from the adjacent done
        // owner and searches through adjacentList before writing "wireInfo2".
        std::size_t exhaustAdjacentSearchCount = 0;
        std::size_t exhaustAdjacentSearchHitCount = 0;
        std::size_t exhaustAdjacentSearchMissCount = 0;
        std::size_t exhaustAdjacentSearchStackFrameCount = 0;
        std::size_t exhaustAdjacentSearchVertexStackCount = 0;
        std::size_t exhaustAdjacentSearchEdgeSetVisitCount = 0;
        std::size_t exhaustAdjacentSearchBacktrackCount = 0;
        std::size_t exhaustAdjacentWireSetInsertCount = 0;
        std::size_t exhaustAdjacentWireSetEraseCount = 0;
        std::size_t exhaustAdjacentWireSetAbortCount = 0;
        std::size_t exhaustAdjacentWireInfo2AbortCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() removes consumed EdgeInfo entries, then loops back
        // through "findClosedWires(true); findTightBound();" until no more removal happens.
        // cad-core records the repeat-cycle need without re-entering output mutation yet.
        std::size_t repeatedSplitExhaustCycleCount = 0;
        std::size_t repeatedSplitExhaustRemovedEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRemovedUnownedEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRemovedSecondaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRemovedPrimaryEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() repeats "findClosedWires(true); findTightBound()"
        // after consumed-edge removal. These fields record the output-neutral rerun search on a
        // temporary WireInfo copy until M3 result-wire identity lets the live loop take over.
        std::size_t repeatedSplitExhaustRerunActiveEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findClosedWires(true), at each buildClosedWire() rerun, first clears
        // every "info.wireInfo" and "info.wireInfo2" before rebuilding tight-bound closed owners.
        std::size_t repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() reruns "findClosedWires(true)"; after consumed-edge
        // removal there may be no EdgeInfo with active "iteration >= 0" left to seed that search.
        std::size_t repeatedSplitExhaustRerunNoActiveSearchCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireSearchCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireMissCount = 0;
    std::size_t repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunClosedWireInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunClosedWireVertexCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findClosedWires(true) clears old "info.wireInfo" before rebuilding.
        // These fields classify rerun owners whose live writeback is currently blocked only by
        // primary owners that the rerun reset would remove.
        std::size_t repeatedSplitExhaustRerunResettableClosedWireInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findClosedWires(true) clears "info.wireInfo" / "info.wireInfo2" before
        // assigning the rerun closed owner. These fields track the safe live-reset subset.
        std::size_t repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveClosedWireInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveClosedWireVertexCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveBranchSearchCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveBranchSearchOutsideCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveTransferWireInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveTransferredOwnerEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLiveDoneWireInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), after rerun "findClosedWires(true); findTightBound()",
        // starts the next while pass by scanning for consumed EdgeInfo removal. These fields record
        // that next-pass removal boundary from live rerun owner state.
        std::size_t repeatedSplitExhaustRerunRemovalScanCount = 0;
        std::size_t repeatedSplitExhaustRerunRemovalEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunRemovalUnownedEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunRemovalSecondaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunRemovalPrimaryEdgeInfoCount = 0;
        std::size_t repeatedSplitExhaustRerunLoopExitNoRemovalCount = 0;
        std::size_t repeatedSplitExhaustRerunBranchSearchCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunBranchSearchInsideCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunBranchSearchOutsideCandidateCount = 0;
        std::size_t repeatedSplitExhaustRerunNewWireSeedCandidateCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() reruns owner search before ::build() adds openWireCompound.
        // This field records live rerun owners rejected because their result-wire identity still comes
        // from the generated open-export transition instead of real EdgeInfo/WireInfo/aHistory state.
        std::size_t repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::exhaustTightBoundUpdateEdge() resets purged "wireInfo"; ::buildClosedWire()
        // then removes still-active unowned edges. cad-core clears the live primary owner only when
        // that same consumed-edge removal is applied, so reset cannot leak an open export.
        std::size_t tightBoundExhaustPrimaryResetEdgeInfoCount = 0;
    };
    std::size_t nextWireInfoId_ = 1;
    std::size_t nextSuperEdgeId_ = 1;
    int nextIteration2_ = 1;
    std::vector<WireInfo> openWires_;
    std::vector<TopoDS_Edge> sourceEdges_;
    WireJoinerHistorySummary historySummary_;
    void initializeEdgeInfo(EdgeInfo& edgeInfo, const TopoDS_Edge& edge) const;
    TopoDS_Wire wireFromVertices(const WireInfo& info, const std::vector<WireVertex>& vertices) const;
    void rebuildAdjacentList(WireInfo& info);
    std::optional<WireVertex> soleActiveAdjacentEdge(const WireInfo& info,
                                                     std::size_t edgeIndex,
                                                     int endpointIndex) const;
    void extendSuperEdgeCandidate(const WireInfo& info,
                                  std::deque<WireVertex>& vertices,
                                  std::vector<bool>& used,
                                  bool appendBack,
                                  bool& closed) const;
    void recordSuperEdgeCandidates(WireInfo& info);
    bool markOpenLeafEdges(WireInfo& info);
    void rebuildOrderedVertices(WireInfo& info);
    std::optional<ClosedWireSearchResult> findClosedWirePath(const WireInfo& info,
                                                             std::size_t beginEdgeIndex) const;
    std::size_t assignClosedWireOwners(WireInfo& info, bool assignOwners);
    gp_Pnt vertexPoint(const WireInfo& info, const WireVertex& vertex) const;
    gp_Pnt vertexOtherPoint(const WireInfo& info, const WireVertex& vertex) const;
    bool findTightBoundBranchPathToPoint(const WireInfo& info,
                                         const OwnerWireInfo& owner,
                                         const gp_Pnt& current,
                                         const gp_Pnt& target,
                                         std::vector<bool>& usedEdges,
                                         std::vector<WireVertex>& path) const;
    bool findBranchPathToPointSkippingOwner(const WireInfo& info,
                                            std::size_t skipOwnerId,
                                            const gp_Pnt& current,
                                            const gp_Pnt& target,
                                            std::vector<bool>& usedEdges,
                                            std::vector<WireVertex>& path) const;
    std::optional<TightBoundTransferPath> tightBoundTransferPathForCandidate(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate) const;
    std::optional<TightBoundTransferPath> tightBoundTransferPathForExistingWireHit(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate,
        const TightBoundExistingWireSearchTrace& trace,
        TightBoundExistingWirePathBlockReason* blockReason = nullptr) const;
    std::optional<std::size_t> ownerVertexIndex(const OwnerWireInfo& owner, const WireVertex& vertex) const;
    TightBoundExistingWireSearchTrace traceExistingWireSearchForCandidate(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate) const;
    bool isDoneOwner(const WireInfo& info, std::size_t ownerId) const;
    void recordExhaustOwnerVertex(WireInfo& info, const WireVertex& vertex, std::size_t ownerId);
    ExhaustAdjacentSearchTrace traceExhaustAdjacentSearch(const WireInfo& info,
                                                          const WireVertex& beginVertex,
                                                          const WireVertex& adjacentVertex,
                                                          std::size_t seedOwnerId) const;
    void recordExhaustAdjacentSecondaryOwners(WireInfo& info);
    void recordBuildClosedWireRemovalLifecycle(WireInfo& info);
    void recordRepeatedSplitExhaustRerunLifecycle(WireInfo& info,
                                                  const std::vector<TopoDS_Face>& boundedFaces,
                                                  const HelperOpenExportOverridePlan& helperPlan);
    HelperOpenExportOverridePlan computeHelperOpenExportOverridePlan(
        const WireInfo& info,
        const TopoDS_Shape& boundedFaceShape,
        const std::vector<TopoDS_Wire>& closedWires,
        const std::vector<TopoDS_Edge>& openEdges,
        bool splitProducedBoundedFaces,
        bool hasOpenWireOutput) const;
    bool helperOpenExportOverridePlanHasUnsafeProducer(const WireInfo& info,
                                                       const HelperOpenExportOverridePlan& helperPlan) const;
    bool edgeInfoExportsOpenWireCompound(const EdgeInfo& edgeInfo) const;
    bool helperOpenExportOverrideCandidateHasFullAHistoryProducerEvidence(const EdgeInfo& edgeInfo) const;
    bool helperOpenExportOverrideRootResultWireProducerHasFullAHistoryProducerEvidence(
        const EdgeInfo& edgeInfo) const;
    bool helperOpenExportOverrideRootResultWireProducerCanSuppressPendingMember(
        const EdgeInfo& edgeInfo) const;
    bool helperOpenExportOverrideCandidateHasSafeAHistoryProducerEvidence(const EdgeInfo& edgeInfo) const;
    std::optional<std::size_t> superEdgeRootIndexForMember(const WireInfo& info,
                                                           const EdgeInfo& edgeInfo) const;
    std::vector<std::size_t> strictRemovedSourceEdgeInfoIndicesForSourceLineage(
        const WireInfo& info,
        const EdgeInfo& edgeInfo) const;
    std::optional<std::size_t> sourceShapeReadyAHistoryRemoveProducerIndex(
        const WireInfo& info,
        const EdgeInfo& edgeInfo,
        const TopoDS_Edge* resultEdge = nullptr) const;
    std::optional<std::size_t> sourceShapeReadySameSourceSidecarProducerIndex(
        const WireInfo& info,
        const EdgeInfo& edgeInfo,
        const TopoDS_Edge* resultEdge = nullptr) const;
    ResultWireProducerIdentity classifyResultWireProducerSlot(const WireInfo& info,
                                                              std::size_t edgeInfoIndex) const;
    void attachResultWireProducerLedger(WireInfo& info);
    ResultWireProducerIdentity childWireResultWireProducerIdentity(
        const WireInfo& info,
        const OpenWireCompoundWireInfo& childWire,
        std::size_t childWireIndex) const;
    bool memberSuppressedCurrentMemberSourceShapeReady(
        const WireInfo& info,
        const OpenWireCompoundWireInfo& childWire) const;
    ResultWireProducerLedgerEntry resultWireProducerLedgerEntryForChildWire(
        const OpenWireCompoundWireInfo& childWire,
        std::size_t childWireIndex) const;
    void applyHelperOpenExportOverridePlan(WireInfo& info,
                                           const HelperOpenExportOverridePlan& helperPlan);
    void recordBranchSearchCandidatesForOwner(WireInfo& info,
                                              OwnerWireInfo& owner,
                                              const std::vector<TopoDS_Face>& boundedFaces);
    void recordBranchSearchCandidates(WireInfo& info, const std::vector<TopoDS_Face>& boundedFaces);
    bool recordTightBoundTransferWire(WireInfo& info, OwnerWireInfo& owner);
    void recordTightBoundLifecycle(WireInfo& info);
    void recordExhaustTightBoundLifecycle(WireInfo& info);
    void recordOpenWireCompoundLedger(WireInfo& info);
};

}  // namespace cad_core::geometry
