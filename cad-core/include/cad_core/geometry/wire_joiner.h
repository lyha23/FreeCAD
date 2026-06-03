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
    std::size_t graphFallbackAssignedEdgeInfoCount = 0;
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
    std::size_t temporaryResultWireEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSourceEdgeInfoConsumedCount = 0;
    std::size_t helperOpenExportOverrideOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSourceEdgeExportShapeEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports final-gate EdgeInfo wires with
    // "builder.Add(openWireCompound, info.wire())". This counter records helper-selected EdgeInfo
    // that already satisfy that gate, but still cannot safely export EdgeInfo::edge because M2
    // child-wire/source-vertex identity is incomplete.
    std::size_t helperOpenExportOverrideOpenWireCompoundEligibleWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideFullAHistoryProducerEvidenceWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls "aHistory->Remove(info.edge)", while ::build()
    // exports only "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    // Split complete producer evidence that is still forced from same-source safe evidence that
    // does not yet have the full Remove-source/removed-target/source-lineage trio.
    std::size_t helperOpenExportOverrideFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSafeAHistoryProducerEvidenceWithoutFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets member edges with "current->iteration = -1",
    // stores the root "first->superEdge = makeCleanWire(false)", then assigns closed roots
    // "first->iteration = -2" or open roots "first->iteration = iteration". These counters keep
    // helper-forced exports split by that producer lifecycle without promoting helper geometry.
    std::size_t helperOpenExportOverrideSuperEdgeMemberEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberWithRootEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootOpenLifecycleEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootClosedLifecycleEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootOpenWireCompoundEligibleAndSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootOpenWireCompoundEligibleMissingSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() removes consumed result-wire members with
    // "info.iteration = -1" and records "aHistory->Remove(info.edge)" before the next
    // findClosedWires()/findTightBound() pass. These counters isolate open super-edge roots that
    // are blocked specifically by that iteration lifecycle while already carrying producer evidence.
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootMissingSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() has three removal paths before final openWireCompound:
    // direct unowned/unfinished removal, secondary owner vertex counting, and primary owner
    // vertex counting. Split blocked roots by that producer branch before changing output.
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootIterationBlockedUnownedRemovalEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootIterationBlockedPrimaryRemovalEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootIterationBlockedSecondaryRemovalEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootIterationBlockedMissingRemovalBranchEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores the open root "first->superEdge =
    // makeCleanWire(false)", then ::buildClosedWire() may remove that root before final
    // openWireCompound export. These counters mark roots that already have a materialized result-wire
    // producer candidate, without exporting it yet.
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalChildWireProducerReadyEdgeInfoCount =
            0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidatePrimaryRemovalEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateSecondaryRemovalEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranchEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() materializes one root "superEdge" from the
    // complete member chain. Keep member coverage visible here so M3 can avoid output-count
    // inference while migrating the final child-wire producer.
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCoveredMemberEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerCurrentMemberEdgeInfoCount =
            0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberRootResultWireProducerNonCurrentMemberEdgeInfoCount =
            0;
    std::size_t helperOpenExportOverrideSuperEdgeMemberForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        helperOpenExportOverrideSuperEdgeMemberMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    std::size_t helperOpenExportOverrideExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideBindingCandidateEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideWithOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideRemovedSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingRemovedSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideRemovedTargetEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingRemovedTargetEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideAHistoryRemoveSameSourceLineageEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideAHistoryRemoveForeignSourceLineageEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls "aHistory->Remove(info.edge)", while
    // ::build() exports only "iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    // These counters split remaining helper-forced exports by whether they already have safe
    // same-source aHistory producer evidence.
    std::size_t helperOpenExportOverrideSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideSourceLineageRemovedSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideMissingSourceLineageRemovedSourceEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideConsumedOpenCutterGraphEdgeInfoCount = 0;
    std::size_t helperOpenExportOverridePartialJunctionOpenCutterEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideClosedWireCycleEdgeInfoCount = 0;
    std::size_t helperOpenExportOverridePartialSharedClosedWireEdgeInfoCount = 0;
    std::size_t helperOpenExportOverrideCandidateEdgeCount = 0;
    std::size_t helperOpenExportOverrideUnboundEdgeCount = 0;
    std::size_t helperOpenExportOverrideDuplicateSourceEdgeInfoCount = 0;
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
    std::size_t openWireCompoundHelperOpenExportOverrideWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideSourceEdgeInfoWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideSourceEdgeInfoConsumedWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideSourceEdgeExportShapeWireInfoCount = 0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSourceEdgeProducerOutputWireInfoCount =
            0;
    std::size_t openWireCompoundHelperOpenExportOverrideHelperShapeWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideOpenWireCompoundEligibleWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideForcedOpenWireCompoundWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideConsumedOpenCutterGraphWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverridePartialJunctionOpenCutterWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverrideClosedWireCycleWireInfoCount = 0;
    std::size_t openWireCompoundHelperOpenExportOverridePartialSharedClosedWireWireInfoCount = 0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReadyWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerWireBuiltWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReadyWireBuiltWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerOutputWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionCoveredMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionCurrentMemberWireInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst(), "current->iteration = -1" for every member and
    // "first->superEdge = makeCleanWire(false)" for the root. These root-group counters keep the
    // child ownership/suppression gap explicit before the root superEdge can replace helper output.
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootUniqueCoveredMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootUniqueCurrentMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() suppresses super-edge members with
    // "current->iteration = -1"; ::WireJoinerP::buildClosedWire() can then prove the remaining
    // non-current member is already consumed by unowned removal and full "aHistory->Remove(info.edge)"
    // producer evidence. These counters separate formally suppressed non-current members from still
    // pending child ownership.
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootSuppressedPendingMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootSuppressedPendingMemberFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootSuppressedPendingMemberUnownedRemovalEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire(), "vertex.edgeInfo()->iteration = -1" on the removed
    // member and "aHistory->Remove(info.edge)" on the source EdgeInfo. These counters classify
    // the pending members that still block child-ownership-complete result-wire output.
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberMissingFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberPrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootPendingMemberMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootCompleteChildOwnershipRootEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionRootIncompleteChildOwnershipRootEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedWireBuiltWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputCandidateWireInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores the full result producer on the root
    // "superEdge", while ::build() exports child wires through final EdgeInfo slots. This counter
    // records the output-neutral point where root producer evidence is attached to the current
    // member child-wire identity before the source-shape gate is allowed to switch output.
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberChildWireProducerReadyWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberChildWireProducerFullAHistoryEvidenceWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedByPendingMemberWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeFullAHistoryProducerEvidenceWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeMissingFullAHistoryProducerEvidenceWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeOpenWireCompoundEligibleWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeForcedOpenWireCompoundWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeRootProducerReadyWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeCurrentMemberChildWireProducerReadyWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeCurrentMemberChildWireProducerFullAHistoryEvidenceWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputBlockedBySourceShapeCurrentMemberChildWireProducerMissingFullAHistoryEvidenceWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerMemberSuppressedOutputWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerUnownedRemovalChildWireProducerReadyOutputWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerRequiresMemberSuppressionNonCurrentMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerOutputBlockedByMultiMemberSuperEdgeWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberWireInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoCount =
            0;
    std::size_t
        openWireCompoundHelperOpenExportOverrideSuperEdgeRootResultWireProducerOutputBlockedNonCurrentMemberEdgeInfoCount =
            0;
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
    std::size_t graphSecondaryOwnerEdgeInfoCount = 0;
    std::size_t resultWireProducerLedgerEntryCount = 0;
    std::size_t migratedLegacyHelperSlotCount = 0;
    std::size_t resultWireProducerNoneCount = 0;
    std::size_t resultWireProducerNoneWithoutBlockerCount = 0;
    std::size_t resultWireProducerExistingSourceEdgeCount = 0;
    std::size_t resultWireProducerPartialSharedClosedWireCount = 0;
    std::size_t resultWireProducerLiveResetOpenEdgeCount = 0;
    std::size_t resultWireProducerSuperEdgeRootCount = 0;
    std::size_t resultWireProducerCurrentMemberChildWireCount = 0;
    std::size_t resultWireProducerLegacyHelperCandidateCount = 0;
    std::size_t resultWireProducerLocatedCount = 0;
    std::size_t resultWireProducerAHistoryEvidenceReadyCount = 0;
    std::size_t resultWireProducerChildWireReadyCount = 0;
    std::size_t resultWireProducerSourceShapeReadyCount = 0;
    std::size_t resultWireProducerSourceShapeNotReadyCount = 0;
    std::size_t resultWireProducerExportedWithoutHelperWireInfoCount = 0;
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
    std::size_t resultWireProducerBlockerLegacyHelperShapeStillUsedCount = 0;
    std::size_t resultWireProducerUnknownInvariantCount = 0;
    std::size_t sourceShapeIdentityUnknownCount = 0;
    std::size_t openWireCompoundLegacyHelperShapeWireInfoCount = 0;
    std::size_t unownedRemovalReadySlotCount = 0;
    std::size_t unownedRemovalReadyLegacyHelperShapeOutputCount = 0;
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
    bool helperOpenExportOverrideSuperEdgeRootClosedLifecycleEdgeInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedByUnowned = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedByPrimaryOwner = false;
    bool helperOpenExportOverrideSuperEdgeRootRemovedBySecondaryOwner = false;
    bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidence = false;
    int helperOpenExportOverrideSuperEdgeRootSelectedIteration = 0;
    std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo = 0;
    std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo2 = 0;
    bool helperOpenExportOverrideSuperEdgeRootExportBlockedByIteration = false;
    bool helperOpenExportOverrideSuperEdgeRootExportBlockedByWireInfo = false;
    bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootMissingSafeAHistoryProducerEvidenceIterationBlocked = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedPrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootIterationBlockedMissingRemovalBranch = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidate = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateFullAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidence = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReady = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidatePrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingRemovalBranch = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemoval = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranch = false;
    bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberEdgeInfo = false;
    std::vector<std::size_t>
        helperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoIndices;
    std::vector<std::size_t>
        helperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoIndices;
    int helperOpenExportOverrideSelectedIteration = 0;
    std::size_t helperOpenExportOverrideSelectedWireInfo = 0;
    std::size_t helperOpenExportOverrideSelectedWireInfo2 = 0;
    bool helperOpenExportOverrideExportBlockedByIteration = false;
    bool helperOpenExportOverrideExportBlockedByWireInfo = false;
    std::vector<std::size_t> helperOpenExportOverrideCandidateEdgeInfoIndices;
    std::vector<std::size_t> helperOpenExportOverrideOpenWireCompoundEligibleCandidateEdgeInfoIndices;
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
    ResultWireProducerIdentity resultWireProducer;
};

struct WireJoinerHistorySummary {
    std::size_t sourceEdgeCount = 0;
    std::size_t splitResultEdgeCount = 0;
    std::size_t openExportEdgeCount = 0;
    std::size_t openExportSourceLineageEdgeCount = 0;
    std::size_t openExportMissingSourceLineageEdgeCount = 0;
    std::size_t openExportHelperOverrideEdgeCount = 0;
    std::size_t openExportHelperOverrideSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSourceEdgeInfoConsumedCount = 0;
    std::size_t openExportHelperOverrideOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSourceEdgeExportShapeEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSourceEdgeProducerOutputEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() can export natural final-gate EdgeInfo wires before
    // ::getOpenWires() consumes "MapperHistory(aHistory)"; keep the still-helper-shaped subset
    // visible to topo consumers as an M2/M3 identity boundary.
    std::size_t openExportHelperOverrideOpenWireCompoundEligibleWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideFullAHistoryProducerEvidenceWithoutSourceEdgeExportShapeEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls "aHistory->Remove(info.edge)", while ::build()
    // exports only "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    // Preserve the producer/final-gate split for open-export history consumers.
    std::size_t openExportHelperOverrideFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSafeAHistoryProducerEvidenceWithoutFullAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() records super-edge members with
    // "current->iteration = -1" and one root with "first->superEdge = makeCleanWire(false)".
    // Preserve that member/root/export-gate split for open-export history consumers.
    std::size_t openExportHelperOverrideSuperEdgeMemberEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberWithRootEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootOpenLifecycleEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootClosedLifecycleEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleAndSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootOpenWireCompoundEligibleMissingSafeAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceWithoutOpenWireCompoundEligibleEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() removes a consumed edge via the "iteration < 0"
    // lifecycle and preserves the Remove producer in MapperHistory(aHistory). Keep that
    // intersection in history summary so topo does not infer it from output geometry.
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootFullAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootMissingSafeAHistoryProducerEvidenceIterationBlockedEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() removes consumed roots through unowned, primary-owner or
    // secondary-owner branches. Expose that branch split to history consumers as producer evidence.
    std::size_t openExportHelperOverrideSuperEdgeMemberRootIterationBlockedUnownedRemovalEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootIterationBlockedPrimaryRemovalEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootIterationBlockedSecondaryRemovalEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootIterationBlockedMissingRemovalBranchEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() can only export final EdgeInfo wires, while open super-edge roots are
    // materialized by "makeCleanWire(false)" before later removal. Preserve this candidate split for
    // M3 producer migration without changing getOpenWires().
    std::size_t openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceEdgeInfoCount =
            0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateUnownedRemovalChildWireProducerReadyEdgeInfoCount =
            0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidatePrimaryRemovalEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateSecondaryRemovalEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingRemovalBranchEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemovalEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemovalEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemovalEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranchEdgeInfoCount =
            0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() suppresses super-edge members by lifecycle and
    // stores the materialized result on the root. History keeps that member coverage explicit so
    // M4/topo consumers do not infer it from exported geometry.
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCoveredMemberEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerCurrentMemberEdgeInfoCount =
            0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberRootResultWireProducerNonCurrentMemberEdgeInfoCount =
            0;
    std::size_t openExportHelperOverrideSuperEdgeMemberForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t
        openExportHelperOverrideSuperEdgeMemberMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount =
            0;
    std::size_t openExportHelperOverrideExportBlockedByIterationEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideExportBlockedByWireInfoEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideBindingCandidateEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideWithOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingOpenWireCompoundEligibleCandidateEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideRemovedSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingRemovedSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideRemovedTargetEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingRemovedTargetEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingAHistoryRemoveSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingAHistoryRemoveSourceLineageEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideAHistoryRemoveSameSourceLineageEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideAHistoryRemoveForeignSourceLineageEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingSafeAHistoryProducerEvidenceEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records removal through "aHistory->Remove(info.edge)";
    // ::build() later adds only final-gate EdgeInfo wires to "openWireCompound". Preserve that
    // producer/export split for topo consumers of open-export history.
    std::size_t openExportHelperOverrideSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingSafeAHistoryProducerEvidenceForcedOpenWireCompoundEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideSourceLineageRemovedSourceEdgeInfoCount = 0;
    std::size_t openExportHelperOverrideMissingSourceLineageRemovedSourceEdgeInfoCount = 0;
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
        bool helperOpenExportOverrideSuperEdgeRootClosedLifecycleEdgeInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootRemovedByUnowned = false;
        bool helperOpenExportOverrideSuperEdgeRootRemovedByPrimaryOwner = false;
        bool helperOpenExportOverrideSuperEdgeRootRemovedBySecondaryOwner = false;
        bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidence = false;
        int helperOpenExportOverrideSuperEdgeRootSelectedIteration = 0;
        std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo = 0;
        std::size_t helperOpenExportOverrideSuperEdgeRootSelectedWireInfo2 = 0;
        bool helperOpenExportOverrideSuperEdgeRootExportBlockedByIteration = false;
        bool helperOpenExportOverrideSuperEdgeRootExportBlockedByWireInfo = false;
        bool helperOpenExportOverrideSuperEdgeRootSafeAHistoryProducerEvidenceIterationBlocked = false;
        bool helperOpenExportOverrideSuperEdgeRootFullAHistoryProducerEvidenceIterationBlocked = false;
        bool helperOpenExportOverrideSuperEdgeRootMissingSafeAHistoryProducerEvidenceIterationBlocked = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedUnownedRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedPrimaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedSecondaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootIterationBlockedMissingRemovalBranch = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidate = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateFullAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidence = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateUnownedRemovalChildWireProducerReady = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidatePrimaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateSecondaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingRemovalBranch = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceUnownedRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidencePrimaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceSecondaryRemoval = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCandidateMissingFullAHistoryProducerEvidenceMissingRemovalBranch = false;
        bool helperOpenExportOverrideSuperEdgeRootResultWireProducerCurrentMemberEdgeInfo = false;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerCoveredMemberEdgeInfoIndices;
        std::vector<std::size_t>
            helperOpenExportOverrideSuperEdgeRootResultWireProducerNonCurrentMemberEdgeInfoIndices;
        int helperOpenExportOverrideSelectedIteration = 0;
        std::size_t helperOpenExportOverrideSelectedWireInfo = 0;
        std::size_t helperOpenExportOverrideSelectedWireInfo2 = 0;
        bool helperOpenExportOverrideExportBlockedByIteration = false;
        bool helperOpenExportOverrideExportBlockedByWireInfo = false;
        std::vector<std::size_t> helperOpenExportOverrideCandidateEdgeInfoIndices;
        std::vector<std::size_t> helperOpenExportOverrideOpenWireCompoundEligibleCandidateEdgeInfoIndices;
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
        std::size_t helperOpenExportOverrideCandidateEdgeCount = 0;
        std::size_t helperOpenExportOverrideUnboundEdgeCount = 0;
        std::size_t helperOpenExportOverrideDuplicateSourceEdgeInfoCount = 0;
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
