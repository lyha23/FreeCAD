#pragma once

// Part-layer WireJoiner state machine aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp.
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

inline constexpr std::size_t resultWireProducerNpos = static_cast<std::size_t>(-1);

enum class ResultWireProducerKind
{
    None,
    ExistingSourceEdge,
    PartialSharedClosedWire,
    LiveResetOpenEdge,
    SuperEdgeRoot,
    CurrentMemberChildWire,
};

enum class ResultWireProducerState
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() publishes only concrete "info.wire()" open children; an empty
    // result-wire producer identity is unpublished diagnostic state, not a transitional result-slot.
    Unpublished,
    ProducerLocated,
    AHistoryEvidenceReady,
    ChildWireReady,
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() publishes concrete open children with
    // "builder.Add(openWireCompound, info.wire())"; this state means the producer ledger has a
    // materialized open-export edge, not that identity owns a source-shape sidecar field.
    ProducerLedgerReady,
    ExportedWithoutTransitionalSlot,
};

enum class WireJoinerHistoryRelation
{
    Preserved,
    Split,
    Generated,
    Deleted,
};

enum class OpenWireCompoundExportSource
{
    None,
    OpenLeafIterationMinus3,
    UnownedOpenEdge,
    AHistoryProducerChildWire,
    RootCurrentMemberChildProducer,
};

struct WireJoinerHistoryEvent
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::splitEdges() records "aHistory->AddModified(split.intersectShape,
    // newInfo.edge)", ::WireJoinerP::buildClosedWire() records "aHistory->Remove(info.edge)",
    // and ::WireJoinerP::getOpenWires() consumes "MapperHistory(aHistory)". This request-local
    // event is the part-layer history record that topo/sketch consumers forward instead of
    // deriving open-export relation from output geometry.
    std::size_t eventIndex = 0;
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = resultWireProducerNpos;
    std::size_t openWireCompoundChildWireInfoIndex = resultWireProducerNpos;
    WireJoinerHistoryRelation relation = WireJoinerHistoryRelation::Preserved;
    bool relationFromChildWireLedger = false;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    bool noOriginalPurgedByLedger = false;
    bool splitFragmentFromModifiedHistory = false;
    bool splitFragmentFromGeneratedHistory = false;
};

struct WireJoinerVmapReplacementEvent
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
    // TopoDS_Vertex", rebuilds an added edge against the existing vmap/sourceEdges vertex. This
    // event is the part-layer identity ledger for that replacement; sketch/topo consumers may
    // forward it, but must not infer replacement from output geometry.
    std::size_t eventIndex = resultWireProducerNpos;
    TopoDS_Vertex oldVertex;
    TopoDS_Vertex newSharedVertex;
    std::size_t affectedSourceEdgeIndex = resultWireProducerNpos;
    std::size_t affectedChildWireEdgeInfoIndex = resultWireProducerNpos;
    int affectedEndpoint = -1;
    int affectedSourceEndpoint = -1;
    int affectedChildWireEndpoint = -1;
    std::size_t replacementSourceEdgeIndex = resultWireProducerNpos;
    int replacementSourceEndpoint = -1;
    bool replacementFromMutableSourceEdgeLedger = false;
    bool replacementFromSplitFragmentLedger = false;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::buildClosedWire() marks removed targets with "vertex.edgeInfo()->iteration = -1"
// but records the producer source separately through "aHistory->Remove(info.edge)".
enum class ResultWireBlocker
{
    None,
    MissingSourceLineage,
    MissingAHistoryRemoveSource,
    ForeignAHistorySourceLineage,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records the actual producer with
    // "aHistory->Remove(info.edge)"; if that foreign source is already producer-ledger ready
    // (legacy SourceShapeReady diagnostic), the remaining blocker is lineage mismatch.
    ForeignAHistorySourceShapeReadyLineageMismatch,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records the actual foreign producer with
    // "aHistory->Remove(info.edge)". If that producer has a matching EdgeInfo but no
    // producer-ledger result-wire output, the gap is producer readiness, not missing lineage evidence.
    ForeignAHistorySourceShapeIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() stores the result-wire producer as
    // "aHistory->Remove(info.edge)". If that foreign producer curve cannot represent the result-wire
    // candidate edge, the blocker is geometry ownership, not missing source-shape identity.
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
    // still needs source-shape identity before it can replace the result-wire candidate child.
    SameSourceSidecarSourceShapeIdentityNotReady,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() stores producer evidence with
    // "aHistory->Remove(info.edge)". If a same-lineage strict sidecar already has producer-ledger
    // output but its edge curve does not match or contain this result edge, the blocker is geometry
    // ownership, not missing source-shape identity.
    SameSourceSidecarGeometryMismatch,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports final "info.wire()" children; current-member replacement
    // must not introduce vertices that are absent from the request-local openWireCompound ledger.
    SourceShapeMemberVertexIdentityNotReady,
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() builds current-member output from member shapes,
    // and ::getOpenWires() consumes MapperHistory(aHistory). If cad-core's split/member candidate
    // would change vertex multiplicity, the result-slot child remains diagnostic instead of exported.
    CurrentMemberVertexMultiplicityBlocked,
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets member edges to "iteration = -1" while the
    // open root can still satisfy ::build()'s openWireCompound gate. A member result-wire candidate slot cannot
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
    // strict sidecars may still record "aHistory->Remove(info.edge)". If such a producer-ledger
    // sidecar cannot represent the member result edge, the remaining blocker is geometry ownership,
    // not missing child-wire evidence.
    CurrentMemberSidecarGeometryMismatch,
    UnknownInvariant,
};

const char* resultWireProducerKindName(ResultWireProducerKind kind);
const char* resultWireProducerStateName(ResultWireProducerState state);
const char* resultWireBlockerName(ResultWireBlocker blocker);
const char* wireJoinerHistoryRelationName(WireJoinerHistoryRelation relation);
const char* openWireCompoundExportSourceName(OpenWireCompoundExportSource source);

struct WireJoinerEndpointIdentityDebt
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
    // TopoDS_Vertex"; ::WireJoinerP::build(), key "builder.Add(openWireCompound, info.wire())".
    // These fields compare output/candidate endpoint identity without geometry guessing.
    std::size_t outputVertexIndex = 0;
    bool matchedMemberSplitLedger = false;
    bool matchedCandidateLedger = false;
    bool currentChildWireOutputVertexMatchesOtherOutput = false;
    bool candidateWireVertexMatchesOtherOutput = false;
    std::string explanation;
    std::string currentChildWireOutputVertexIdentity;
    std::string memberSplitLedgerVertexIdentity;
    std::string candidateWireVertexIdentity;
    std::string mismatchReason;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::buildClosedWire() records producer evidence with "aHistory->Remove(info.edge)"
// and ::WireJoinerP::build() exports only final "info.wire()" states into openWireCompound.
// This identity keeps result-wire producer state finite while cad-core replaces result-wire candidate output.
struct ResultWireProducerIdentity
{
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::Unpublished;
    ResultWireBlocker blocker = ResultWireBlocker::None;
    std::size_t sourceEdgeInfoIndex = resultWireProducerNpos;
    std::size_t rootEdgeInfoIndex = resultWireProducerNpos;
    std::size_t currentMemberEdgeInfoIndex = resultWireProducerNpos;
    std::size_t childWireInfoIndex = resultWireProducerNpos;
    bool childWireBuilt = false;
};

struct ResultWireProducerLedgerEntry
{
    std::size_t openExportIndex = 0;
    std::size_t sourceEdgeInfoIndex = resultWireProducerNpos;
    std::size_t rootEdgeInfoIndex = resultWireProducerNpos;
    std::size_t currentMemberEdgeInfoIndex = resultWireProducerNpos;
    std::size_t childWireInfoIndex = resultWireProducerNpos;
    ResultWireProducerKind kind = ResultWireProducerKind::None;
    ResultWireProducerState state = ResultWireProducerState::Unpublished;
    ResultWireBlocker blocker = ResultWireBlocker::None;
    OpenWireCompoundExportSource openWireCompoundExportSource =
        OpenWireCompoundExportSource::None;
    int openWireCompoundEdgeInfoIteration = 0;
    int openWireCompoundEdgeInfoIteration2 = 0;
    std::size_t openWireCompoundOwnerWireInfo = 0;
    std::size_t openWireCompoundOwnerWireInfo2 = 0;
    bool openWireCompoundOpenLeafExport = false;
    bool openWireCompoundUnownedOpenEdgeExport = false;
    bool openWireCompoundRootCurrentMemberChildProducer = false;
    std::size_t wireJoinerHistoryEventIndex = resultWireProducerNpos;
    bool childShapeIdentityRecorded = false;
    std::size_t childWireEdgeCount = 0;
    std::size_t childWireVertexCount = 0;
    std::vector<std::size_t> sourceEdgeIndices;
};

struct WireJoinerLedgerSummary
{
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
    std::size_t repeatedSplitExhaustRerunLiveDoneWireInfoCount = 0;
    std::size_t repeatedSplitExhaustRerunRemovalScanCount = 0;
    std::size_t repeatedSplitExhaustRerunLoopExitNoRemovalCount = 0;
    std::size_t repeatedSplitExhaustRerunBranchSearchCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunBranchSearchInsideCandidateCount = 0;
    std::size_t repeatedSplitExhaustRerunNewWireSeedCandidateCount = 0;
    std::size_t repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount = 0;
    std::size_t tightBoundExistingWireSearchStackFrameCount = 0;
    std::size_t tightBoundExistingWireSearchVertexStackCount = 0;
    std::size_t tightBoundExistingWireSearchEdgeSetVisitCount = 0;
    std::size_t tightBoundExistingWireSearchBacktrackCount = 0;
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
    std::size_t tightBoundExistingWireSearchOnlyOrderBlockedCount = 0;
    std::size_t tightBoundExistingWireIdxVertexCount = 0;
    std::size_t tightBoundExistingWireStackPosCount = 0;
    std::size_t sourceIdentitySharedVertexEdgeInfoCount = 0;
    std::size_t sourceIdentityOnlySourceVerticesEdgeInfoCount = 0;
    std::size_t sourceIdentityOpenExportSharedVertexEdgeInfoCount = 0;
    std::size_t sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(noOriginal=true) filters after openWireCompound child wires are
    // built. Keep the legacy JSON field name stable, but count child-wire ledger candidates rather
    // than recomputing the verdict from EdgeInfo after the child-wire boundary.
    std::size_t sourceLineageEdgeInfoCount = 0;
    std::size_t sourceLineageSplitEdgeInfoCount = 0;
    std::size_t sourceLineageOpenExportEdgeInfoCount = 0;
    std::size_t sourceLineageMissingOpenExportEdgeInfoCount = 0;
    std::size_t sourceLineageMultiSourceEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::splitEdges() records "aHistory->AddModified(split.intersectShape,
    // newInfo.edge)" after sourceEdgeArray is copied into sourceEdges. These counters expose how much
    // of the fragment-to-source ledger is backed by splitter history versus remaining request-local
    // bridges. The aggregate identity fallback remains compatibility debt; the split counters below
    // distinguish source-edge identity recovery from history-shape-to-result geometry binding.
    std::size_t splitFragmentSourceLineageEdgeInfoCount = 0;
    std::size_t splitFragmentModifiedHistoryEdgeInfoCount = 0;
    std::size_t splitFragmentGeneratedHistoryEdgeInfoCount = 0;
    std::size_t splitFragmentIdentityFallbackEdgeInfoCount = 0;
    std::size_t splitFragmentSourceIdentityFallbackEdgeInfoCount = 0;
    std::size_t splitFragmentHistoryShapeGeometryBridgeEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::splitEdges() records "aHistory->AddModified(split.intersectShape,
    // newInfo.edge)" before ::build() exports openWireCompound. These counters expose the
    // closed-cycle generated-open-export precheck that now reads split fragment history instead of
    // resampling bounded result edges.
    std::size_t closedWireCycleSplitLedgerSourceEdgeCount = 0;
    std::size_t closedWireCycleSplitLedgerOpenExportDecisionCount = 0;
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
    std::size_t openWireCompoundSourceLineageWireInfoCount = 0;
    std::size_t openWireCompoundSplitterLineageWireInfoCount = 0;
    std::size_t openWireCompoundNoOriginalPurgeMatchWireInfoCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(noOriginal=true), key
    // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1)).empty()". These counters expose the
    // per-child-wire edge ledger used by the noOriginal purge verdict.
    std::size_t openWireCompoundNoOriginalSharedSourceLedgerWireInfoCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceMatchedEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() stores the emitted "info.wire()" in openWireCompound. This counter
    // keeps the request-local child-wire materialized producer wire visible so history consumers use
    // the child-wire ledger instead of any EdgeInfo output re-export helper.
    std::size_t openWireCompoundProducerLedgerWireBuiltWireInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same TopoDS_Vertex",
    // drives endpoint identity through the mutable vmap/sourceEdges ledger before ::build() emits
    // openWireCompound children. This counter tracks producer wires materialized from that ledger.
    std::size_t openWireCompoundProducerLedgerWireFromSourceVmapWireInfoCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same TopoDS_Vertex".
    // These counters expose how many materialized openWireCompound child vertices already match the
    // request-local source/vmap ledger before any endpoint materialization fallback is considered.
    std::size_t openWireCompoundSourceVmapEndpointLedgerWireInfoCount = 0;
    std::size_t openWireCompoundSourceVmapEndpointLedgerOutputVertexCount = 0;
    std::size_t openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
    // TopoDS_Vertex"; ::build() then exports "info.wire()" children into openWireCompound. These
    // counters expose per-output endpoint provenance on the child-wire ledger.
    std::size_t openWireCompoundEndpointProvenanceWireInfoCount = 0;
    std::size_t openWireCompoundEndpointProvenanceOutputVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceCandidateMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceUnmatchedVertexCount = 0;
    std::size_t openWireCompoundVmapReplacementEventWireInfoCount = 0;
    std::size_t openWireCompoundVmapReplacementEventCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() gathers member "shape(...)" entries into
    // wireData before "first->superEdge = makeCleanWire(false)"; ::splitEdges() records
    // "aHistory->AddModified(split.intersectShape, newInfo.edge)". This counter tracks
    // current-member children whose split/member vertex ledger can form a producer candidate, but
    // whose output still waits for MapperHistory/ElementMap vertex multiplicity parity.
    std::size_t openWireCompoundCurrentMemberSplitLedgerVertexCandidateWireInfoCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores member shapes before ::build() exports
    // "info.wire()". This counts child-wire entries with recorded per-output vertex debt records.
    std::size_t openWireCompoundCurrentMemberSplitLedgerVertexDebtWireInfoCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerMemberVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() builds a current-member candidate from member
    // shapes before ::build() emits "info.wire()". This aggregate counts emitted child vertices that
    // already reuse candidate-wire TopoDS_Vertex identity, the next deletion gate before replacing
    // result-slot endpoint materialization.
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
    // TopoDS_Vertex"; ::build() emits child wires after that lifecycle. These counts compare the
    // blocked current-member output endpoints against candidate endpoint identities by TopoDS_Vertex
    // identity only, exposing why a direct candidate switch would merge vertex multiplicity.
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateVertexMultiplicityLossCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputOtherOutputMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateOtherOutputMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateVertexReuseRiskCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateMissingSharedOutputIdentityCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires() consumes "MapperHistory(aHistory)" after build() has emitted
    // openWireCompound. A non-zero count means the split/member candidate exists, but the currently
    // emitted child still carries vertex identities that the member/split ledger cannot preserve yet.
    std::size_t openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlockedWireInfoCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() writes current-member shapes into wireData before
    // "first->superEdge = makeCleanWire(false)", and ::getOpenWires() maps the final child through
    // MapperHistory(aHistory). This aggregate is the total number of emitted child vertices not yet
    // covered by the member/split ledger, used as the hard deletion gate for result-slot endpoints.
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() is the open-wire export boundary: it calls
    // "builder.Add(openWireCompound, info.wire())" for exportable final EdgeInfo states. A non-zero
    // value means cad-core found an exportable EdgeInfo without a matching child-wire ledger and must
    // report that invariant instead of rebuilding output through an EdgeInfo wire helper.
    std::size_t openWireCompoundMissingChildWireHistoryEdgeInfoCount = 0;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores open member output on the root
    // "first->superEdge" after marking members with "current->iteration = -1". This counter fixes
    // the child-wire path that consumes root/current-member ledger output instead of a result-slot
    // locator edge.
    std::size_t openWireCompoundRootCurrentMemberProducerOutputWireInfoCount = 0;
    std::size_t openWireCompoundSourceSharedVertexWireInfoCount = 0;
    std::size_t orderedWireInfoCount = 0;
    std::size_t orderedVertexCount = 0;
    std::size_t iteration2MarkedEdgeInfoCount = 0;
    std::size_t branchSearchCandidateCount = 0;
    std::size_t branchSearchSeedWireInfoCount = 0;
    std::size_t branchSearchInsideCandidateCount = 0;
    std::size_t newWireSeedCandidateCount = 0;
    std::size_t newWireSeedWireInfoCount = 0;
    std::size_t splitWireCandidateCount = 0;
    std::size_t splitWireEdgeInfoCount = 0;
    std::size_t doneWireInfoCount = 0;
    std::size_t doneOwnedEdgeInfoCount = 0;
    std::size_t ownerPropagationCandidateCount = 0;
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
    std::vector<ResultWireProducerLedgerEntry> resultWireProducerLedgerEntries;
};

struct WireJoinerOpenExportHistoryEntry
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() adds each final "info.wire()" to "openWireCompound", then
    // ::getOpenWires() consumes "MapperHistory(aHistory)" with sourceEdges. Keep per-child
    // source lineage next to the summary so topo can later consume WireJoiner-produced history
    // instead of deriving it from raw/internal geometry.
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = 0;
    TopoDS_Wire openExportWire;
    TopoDS_Edge openExportEdge;
    OpenWireCompoundExportSource openWireCompoundExportSource =
        OpenWireCompoundExportSource::None;
    int openWireCompoundEdgeInfoIteration = 0;
    int openWireCompoundEdgeInfoIteration2 = 0;
    std::size_t openWireCompoundOwnerWireInfo = 0;
    std::size_t openWireCompoundOwnerWireInfo2 = 0;
    bool openWireCompoundOpenLeafExport = false;
    bool openWireCompoundUnownedOpenEdgeExport = false;
    bool openWireCompoundRootCurrentMemberChildProducer = false;
    bool openWireCompoundChildShapeIdentityRecorded = false;
    std::size_t openWireCompoundChildWireEdgeCount = 0;
    std::size_t openWireCompoundChildWireVertexCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), calls
    // "shape.makeShapeWithElementMap(comp, MapperHistory(aHistory), {sourceEdges.begin(),
    // sourceEdges.end()}, op)". Keep the request-local mapper relation on the WireJoiner entry so
    // topo consumers do not infer split/generated/deleted ownership from output geometry.
    WireJoinerHistoryRelation historyRelation = WireJoinerHistoryRelation::Preserved;
    bool historyRelationFromChildWireLedger = false;
    std::size_t openWireCompoundChildWireInfoIndex = resultWireProducerNpos;
    std::vector<std::size_t> openWireCompoundSourceEdgeIndices;
    bool openWireCompoundSourceLineageFromSplitterHistory = false;
    bool openWireCompoundNoOriginalPurgeMatch = false;
    bool openWireCompoundNoOriginalPurgedByLedger = false;
    bool openWireCompoundNoOriginalSharedSourceLedgerRecorded = false;
    std::size_t openWireCompoundNoOriginalSharedSourceEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceMatchedEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount = 0;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    bool openWireCompoundProducerLedgerWireBuilt = false;
    bool openWireCompoundProducerLedgerWireFromSourceVmap = false;
    bool openWireCompoundSourceVmapEndpointLedgerRecorded = false;
    std::size_t openWireCompoundSourceVmapEndpointLedgerOutputVertexCount = 0;
    std::size_t openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount = 0;
    bool openWireCompoundEndpointProvenanceRecorded = false;
    std::size_t openWireCompoundEndpointProvenanceOutputVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceCandidateMatchedVertexCount = 0;
    std::size_t openWireCompoundEndpointProvenanceUnmatchedVertexCount = 0;
    std::vector<WireJoinerVmapReplacementEvent> openWireCompoundVmapReplacementEvents;
    std::size_t openWireCompoundVmapReplacementEventCount = 0;
    bool openWireCompoundCurrentMemberProducerOutput = false;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() uses "wireData->Add(current->shape(...))"; the
    // split ledger comes from ::splitEdges() "aHistory->AddModified(split.intersectShape,
    // newInfo.edge)". This candidate remains diagnostic until ElementMap preserves the same vertex
    // multiplicity as FreeCAD.
    bool openWireCompoundCurrentMemberSplitLedgerVertexCandidate = false;
    bool openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded = false;
    std::size_t openWireCompoundCurrentMemberSplitLedgerMemberVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount = 0;
    std::vector<WireJoinerEndpointIdentityDebt>
        openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt;
    bool openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked = false;
    bool missingOpenWireCompoundChildWire = false;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::splitEdges(), after "add(split.edge, ...)", records
    // "aHistory->AddModified(split.intersectShape, newInfo.edge)". Keep this public history detail
    // separate from sourceVertexIdentity/noOriginal purge evidence so topo can consume actual
    // fragment lineage instead of endpoint geometry guesses.
    std::size_t wireJoinerHistoryEventIndex = resultWireProducerNpos;
    bool wireJoinerHistoryEventFromChildWireLedger = false;
    std::vector<std::size_t> splitFragmentSourceEdgeIndices;
    std::vector<std::size_t> splitFragmentModifiedSourceEdgeIndices;
    std::vector<std::size_t> splitFragmentGeneratedSourceEdgeIndices;
    bool splitFragmentFromModifiedHistory = false;
    bool splitFragmentFromGeneratedHistory = false;
    bool splitFragmentSourceLineageFromIdentityFallback = false;
    bool splitFragmentSourceLineageFromSourceIdentityFallback = false;
    bool splitFragmentHistoryShapeGeometryBridge = false;
    std::array<bool, 2> sourceVertexIdentity {{false, false}};
    std::array<int, 2> sourceVertexReplacementSourceEdgeIndices {{-1, -1}};
    std::array<int, 2> sourceVertexReplacementEndpoints {{-1, -1}};
    std::array<bool, 2> sourceVertexReplacementIdentity {{false, false}};
    ResultWireProducerIdentity resultWireProducer;
};

struct WireJoinerHistorySummary
{
    std::size_t sourceEdgeCount = 0;
    std::size_t splitResultEdgeCount = 0;
    std::vector<WireJoinerOpenExportHistoryEntry> openExportEntries;
    std::vector<WireJoinerHistoryEvent> historyEvents;
    std::size_t historyEventFromChildWireLedgerCount = 0;
    std::size_t modifiedSourceEdgeCount = 0;
    std::size_t modifiedHistoryCount = 0;
    std::size_t generatedHistoryCount = 0;
    std::size_t deletedHistoryCount = 0;
    bool splitterHistory = false;
};

class WireJoiner
{
public:
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setTightBound(), SketchObject::buildInternals() enables tight bounds before
    // getOpenWires().
    void setTightBound(bool enabled);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setMergeEdges(), SketchObject::buildInternals() enables merge before
    // getOpenWires().
    void setMergeEdges(bool enabled);
    void addOpenWire(
        const TopoDS_Wire& wire,
        const std::vector<std::size_t>& sourceEdgeIndices = {}
    );
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), calls splitEdges(), buildClosedWire(), findTightBound() and
    // exhaustTightBound() before exporting openWireCompound from final EdgeInfo ownership.
    // cad-core's current main path assigns primary owners from the closed path search; unsupported
    // owner gaps must be fixed by migrating FreeCAD's VertexInfo stack search, not graph fallback.
    void buildFinalEdgeOwnership(
        const TopoDS_Shape* boundedFaceShape = nullptr,
        const std::vector<TopoDS_Wire>* closedWires = nullptr,
        const std::vector<TopoDS_Edge>* openEdges = nullptr,
        bool splitProducedBoundedFaces = false
    );
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), when noOriginal=true, builds a source compound from
    // sourceEdgeArray and removes open-wire edges whose vertices are still shared with source.
    void addSourceEdge(const TopoDS_Edge& edge);
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(
        const std::string& historyPrefix,
        bool noOriginal = true
    ) const;
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
    struct EdgeInfo
    {
        TopoDS_Edge edge;
        TopoDS_Wire superEdge;
        mutable TopoDS_Shape edgeReversed;
        mutable TopoDS_Shape superEdgeReversed;
        gp_Pnt p1;
        gp_Pnt p2;
        gp_Pnt mid;
        std::array<int, 2> iStart {{-1, -1}};
        std::array<int, 2> iEnd {{-1, -1}};
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) compares openWireCompound edges with
        // sourceEdgeArray by shared source vertices. Keep the endpoint identity ledger next to
        // EdgeInfo so purge compatibility can be removed only after source/split identity is complete.
        std::array<bool, 2> sourceVertexIdentity {{false, false}};
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same
        // TopoDS_Vertex", uses a temporary BRepBuilderAPI_MakeWire to replace an added edge's
        // endpoint with an existing vmap vertex. This diagnostic ledger records the sourceEdgeArray
        // endpoint that would supply that replacement; it does not drive export or purge decisions.
        std::array<int, 2> sourceVertexReplacementSourceEdgeIndices {{-1, -1}};
        std::array<int, 2> sourceVertexReplacementEndpoints {{-1, -1}};
        std::array<bool, 2> sourceVertexReplacementIdentity {{false, false}};
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() copies sourceEdgeArray into sourceEdges, then splitEdges() records
        // modified EdgeInfo shapes in aHistory before getOpenWires() consumes
        // MapperHistory(aHistory). This request-local lineage records which sourceEdgeArray entries
        // produced this EdgeInfo.
        std::vector<std::size_t> sourceEdgeIndices;
        bool sourceLineageFromSplitterHistory = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges(), after "add(split.edge, ...)", records
        // "aHistory->AddModified(split.intersectShape, newInfo.edge)". These fields keep the
        // fragment-to-source ledger separate from sourceVertexIdentity/noOriginal purge evidence.
        std::vector<std::size_t> splitFragmentSourceEdgeIndices;
        std::vector<std::size_t> splitFragmentModifiedSourceEdgeIndices;
        std::vector<std::size_t> splitFragmentGeneratedSourceEdgeIndices;
        bool splitFragmentFromModifiedHistory = false;
        bool splitFragmentFromGeneratedHistory = false;
        bool splitFragmentSourceLineageFromIdentityFallback = false;
        bool splitFragmentSourceLineageFromSourceIdentityFallback = false;
        bool splitFragmentHistoryShapeGeometryBridge = false;
        bool buildClosedWireRemoved = false;
        bool buildClosedWireRemovedByUnowned = false;
        bool buildClosedWireRemovedByPrimaryOwner = false;
        bool buildClosedWireRemovedBySecondaryOwner = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), counter removal sets "vertex.edgeInfo()->iteration =
        // -1" but calls "aHistory->Remove(info.edge)" with the outer EdgeInfo source.
        bool buildClosedWireAHistoryRemoved = false;
        std::vector<std::size_t> buildClosedWireAHistoryRemoveSourceEdgeInfoIndices;
        std::vector<std::size_t> buildClosedWireAHistoryRemoveSourceEdgeIndices;
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
        const TopoDS_Shape& shape(bool forward = true) const;
        TopoDS_Wire wire(bool forward = true) const;
        int iteration = 0;
        int iteration2 = 0;
        std::size_t wireInfo = 0;
        std::size_t wireInfo2 = 0;
        bool closedWireOwner = false;
        bool splitFromInputEdge = false;
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
    struct WireVertex
    {
        std::size_t edgeIndex = 0;
        bool start = true;
        std::size_t branchCandidateCount = 0;
    };
    struct SuperEdgeInfo
    {
        std::size_t id = 0;
        TopoDS_Wire wire;
        std::vector<WireVertex> vertices;
        bool closed = false;
        bool materialized = false;
    };
    struct TightBoundBranchCandidate
    {
        WireVertex ownerVertex;
        WireVertex adjacentVertex;
        bool inside = false;
        bool transfersOwnerEdge = false;
    };
    struct TightBoundTransferWire
    {
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
    struct TightBoundTransferPath
    {
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
    struct TightBoundExistingWireSearchTrace
    {
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
    struct OpenWireCompoundWireInfo
    {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build(), for each exportable EdgeInfo calls
        // "builder.Add(openWireCompound, info.wire())".
        std::size_t edgeIndex = 0;
        TopoDS_Wire wire;
        bool wireBuilt = false;
        bool superEdgeWire = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports when
        // "iteration == -3 || (!wireInfo && iteration >= 0)", while
        // ::getOpenWires() consumes MapperHistory(aHistory). Keep the final child slot source here so
        // public producer entries read the child-wire ledger instead of a pre-child materialization entry.
        OpenWireCompoundExportSource openExportSource =
            OpenWireCompoundExportSource::None;
        int edgeInfoIteration = 0;
        int edgeInfoIteration2 = 0;
        std::size_t ownerWireInfo = 0;
        std::size_t ownerWireInfo2 = 0;
        bool openLeafIterationMinus3 = false;
        bool unownedOpenEdge = false;
        bool rootCurrentMemberChildProducer = false;
        bool childShapeIdentityRecorded = false;
        std::size_t childWireEdgeCount = 0;
        std::size_t childWireVertexCount = 0;
        std::size_t wireJoinerHistoryEventIndex = resultWireProducerNpos;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() materializes "openWireCompound" child wires before
        // ::getOpenWires(noOriginal=true) filters that compound. Carry source lineage on the
        // child-wire slot itself so downstream history can consume openWireCompound ownership
        // without re-deriving it from EdgeInfo/result-slot geometry.
        std::vector<std::size_t> sourceEdgeIndices;
        bool sourceLineageFromSplitterHistory = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() materializes openWireCompound children from final EdgeInfo states;
        // ::getOpenWires() then consumes MapperHistory(aHistory). Carry split fragment provenance
        // on the child-wire ledger so topo does not infer split ownership from output geometry.
        std::vector<std::size_t> splitFragmentSourceEdgeIndices;
        std::vector<std::size_t> splitFragmentModifiedSourceEdgeIndices;
        std::vector<std::size_t> splitFragmentGeneratedSourceEdgeIndices;
        bool splitFragmentFromModifiedHistory = false;
        bool splitFragmentFromGeneratedHistory = false;
        bool splitFragmentSourceLineageFromIdentityFallback = false;
        bool splitFragmentSourceLineageFromSourceIdentityFallback = false;
        bool splitFragmentHistoryShapeGeometryBridge = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() emits openWireCompound children before
        // ::getOpenWires(noOriginal) compares those children with the original "sourceEdgeArray".
        // Keep source-vertex and split-fragment exclusion evidence on the child-wire slot so the
        // purge candidate verdict is not recomputed through an EdgeInfo helper after materialization.
        bool splitFromInputEdge = false;
        std::array<bool, 2> sourceVertexIdentity {{false, false}};
        std::array<int, 2> sourceVertexReplacementSourceEdgeIndices {{-1, -1}};
        std::array<int, 2> sourceVertexReplacementEndpoints {{-1, -1}};
        std::array<bool, 2> sourceVertexReplacementIdentity {{false, false}};
        bool noOriginalPurgeMatch = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) first reads
        // "TopoShape(openWireCompound, -1).getSubTopoShapes(TopAbs_WIRE)" and erases whole wires.
        // This is the final child-wire deletion verdict after cad-core has regrouped materialized
        // child edges to the same wire granularity.
        bool noOriginalPurgedByLedger = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) purges a child only when every child edge has
        // at least one sourceEdgeArray shared vertex. Keep the edge counts on the child-wire ledger so
        // noOriginal decisions are auditable without rebuilding them from output geometry later.
        bool noOriginalSharedSourceEdgeLedgerRecorded = false;
        std::size_t noOriginalSharedSourceEdgeCount = 0;
        std::size_t noOriginalSharedSourceMatchedEdgeCount = 0;
        std::size_t noOriginalSharedSourceUnmatchedEdgeCount = 0;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() emits final children with
        // "builder.Add(openWireCompound, info.wire())", then ::getOpenWires() consumes that child with
        // MapperHistory(aHistory). The public result-wire producer entry is therefore a child-wire slot
        // decision. EdgeInfo still discovers transitional candidate slots, but downstream purge/history
        // gates read this child-wire ledger bit instead of an EdgeInfo candidate copy.
        bool resultWireProducerLedgerEntry = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() emits "info.wire()" into "openWireCompound"; the emitted child is
        // later consumed by ::getOpenWires() with MapperHistory(aHistory). Store only the materialized
        // producer wire on the child-wire slot so topo evidence does not recover output shape through
        // an EdgeInfo output helper after the openWireCompound boundary.
        TopoDS_Wire producerLedgerWire;
        bool producerLedgerWireBuilt = false;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same
        // TopoDS_Vertex"; true when this child-wire producer was materialized from the mutable
        // vmap/sourceEdges vertex ledger.
        bool producerLedgerWireFromSourceVmap = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
        // TopoDS_Vertex". Keep this as child-wire vertex ledger evidence so later deletion of
        // endpoint materialization can be gated by TopoDS_Vertex identity, not geometry matching.
        bool sourceVmapEndpointLedgerRecorded = false;
        std::size_t sourceVmapEndpointLedgerOutputVertexCount = 0;
        std::size_t sourceVmapEndpointLedgerMatchedVertexCount = 0;
        struct EndpointProvenance
        {
            // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::add() shares coincident endpoints through vmap/sourceEdges before
            // ::build() exports openWireCompound. Record exact TopoDS_Vertex identity coverage for
            // every emitted child endpoint; this is ledger evidence, not an output rewrite.
            TopoDS_Vertex outputVertex;
            bool matchedSourceVmapLedger = false;
            bool matchedVmapReplacementLedger = false;
            bool matchedCurrentMemberCandidateLedger = false;
            std::size_t vmapReplacementEventIndex = resultWireProducerNpos;
        };
        std::vector<EndpointProvenance> endpointProvenance;
        bool endpointProvenanceRecorded = false;
        std::size_t endpointProvenanceOutputVertexCount = 0;
        std::size_t endpointProvenanceSourceVmapMatchedVertexCount = 0;
        std::size_t endpointProvenanceVmapReplacementMatchedVertexCount = 0;
        std::size_t endpointProvenanceCandidateMatchedVertexCount = 0;
        std::size_t endpointProvenanceUnmatchedVertexCount = 0;
        std::vector<WireJoinerVmapReplacementEvent> vmapReplacementEvents;
        std::size_t vmapReplacementEventCount = 0;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() stores "first->superEdge" on the root
        // EdgeInfo, while member EdgeInfos are marked "current->iteration = -1". These fields are
        // the child-wire ledger for that root/member producer lifecycle; EdgeInfo still carries the
        // transitional result-wire classifier used to discover the candidate.
        std::size_t superEdgeRootEdgeInfoIndex = 0;
        bool superEdgeRootOpenWireCompoundEligible = false;
        bool rootResultWireProducerCandidate = false;
        bool rootResultWireProducerUnownedRemovalReady = false;
        TopoDS_Wire rootResultWireProducerWire;
        bool rootResultWireProducerRequiresMemberSuppression = false;
        bool currentMemberEdgeInfo = false;
        std::vector<std::size_t> rootResultWireProducerCoveredMemberEdgeInfoIndices;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() stores one root "superEdge" after setting
        // each member "current->iteration = -1". This candidate keeps only the current child
        // member wire so M3 can suppress sibling members without exporting the full root superEdge.
        TopoDS_Wire currentMemberProducerWire;
        bool currentMemberProducerWireBuilt = false;
        bool currentMemberProducerBlockedByPendingMember = false;
        bool currentMemberProducerBlockedBySourceShape = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires() consumes MapperHistory(aHistory) after openWireCompound
        // export. This blocks current-member export while cad-core's member/split candidate would
        // collapse or otherwise change the vertex multiplicity.
        bool currentMemberProducerBlockedByVertexMultiplicity = false;
        bool currentMemberChildWireProducerReady = false;
        bool currentMemberChildWireProducerFullAHistoryEvidence = false;
        bool currentMemberProducerOutput = false;
        struct CurrentMemberSplitLedgerVertexDebt
        {
            // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::findSuperEdgesUpdateFirst(), key "wireData->Add(current->shape(...))";
            // ::splitEdges(), key "aHistory->AddModified(split.intersectShape, newInfo.edge)".
            // This records each emitted child vertex against the member/split vertex ledger so
            // result-slot endpoint debt is represented as child-wire ledger state, not output repair.
            std::size_t outputVertexIndex = 0;
            TopoDS_Vertex outputVertex;
            bool matchedMemberSplitLedger = false;
            bool matchedCandidateLedger = false;
            bool currentChildWireOutputVertexMatchesOtherOutput = false;
            bool candidateWireVertexMatchesOtherOutput = false;
            std::string explanation;
            std::string currentChildWireOutputVertexIdentity;
            std::string memberSplitLedgerVertexIdentity;
            std::string candidateWireVertexIdentity;
            std::string mismatchReason;
        };
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst(), key "wireData->Add(current->shape(...))",
        // builds root/current-member output from member shapes; ::splitEdges() records
        // "aHistory->AddModified(split.intersectShape, newInfo.edge)". True means the current
        // member split ledger can construct a producer candidate, but cad-core still keeps the
        // result-slot child until MapperHistory/ElementMap vertex multiplicity matches FreeCAD.
        bool currentMemberSplitLedgerVertexCandidate = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports "info.wire()" after the EdgeInfo/WireInfo ledger has
        // stabilized. Keep the current-member split vertex inventory on the child-wire ledger so
        // diagnostics and topo evidence consume ledger records instead of recomputing from geometry.
        std::vector<TopoDS_Vertex> currentMemberSplitLedgerMemberVertices;
        std::vector<TopoDS_Vertex> currentMemberSplitLedgerCandidateVertices;
        std::vector<CurrentMemberSplitLedgerVertexDebt> currentMemberSplitLedgerOutputVertexDebt;
        std::size_t currentMemberSplitLedgerCandidateVertexCount = 0;
        std::size_t currentMemberSplitLedgerOutputVertexCount = 0;
        std::size_t currentMemberSplitLedgerOutputMatchedVertexCount = 0;
        std::size_t currentMemberSplitLedgerOutputCandidateMatchedVertexCount = 0;
        std::size_t currentMemberSplitLedgerOutputUnmatchedVertexCount = 0;
        bool currentMemberSplitLedgerVertexMultiplicityBlocked = false;
        bool sourceSharedVertexPurgeMatch = false;
        ResultWireProducerIdentity resultWireProducer;
    };
    struct WireJoinerHistoryMaterializationBinding
    {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports result-wire identity from final EdgeInfo states before
        // ::getOpenWires() consumes "MapperHistory(aHistory)". This binding records the legacy
        // result-slot edge used as request-local endpoint evidence; producer identity is bound to
        // the final EdgeInfo row, not to a rediscovered equivalent source candidate.
        TopoDS_Edge resultSlotEdge;
        bool partialSharedClosedWireProducer = false;
        std::size_t edgeInfoIndex = resultWireProducerNpos;
    };
    struct WireJoinerHistoryMaterializationEdgeEntry
    {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports from final EdgeInfo rows, while producer/source evidence is
        // scoped to the request-local aHistory materialization. This is a temporary candidate bit
        // until MapperHistory(aHistory) / ElementMap can provide the producer child without a staged
        // per-edge materialization entry; it is not a public child-wire source.
        bool historyProducerChildWireCandidate = false;
        bool superEdgeMember = false;
        bool superEdgeRoot = false;
        std::size_t superEdgeRootIndex = 0;
        bool superEdgeRootOpenWireCompoundEligible = false;
        bool superEdgeRootOpenLifecycle = false;
        bool superEdgeRootExportBlockedByIteration = false;
        bool superEdgeRootIterationBlockedUnownedRemoval = false;
        bool superEdgeRootIterationBlockedPrimaryRemoval = false;
        bool superEdgeRootIterationBlockedSecondaryRemoval = false;
        bool superEdgeRootProducerCandidate = false;
        bool superEdgeRootProducerFullAHistoryEvidence = false;
        bool superEdgeRootProducerUnownedRemovalChildWireReady = false;
        bool superEdgeRootProducerPrimaryRemoval = false;
        bool superEdgeRootProducerSecondaryRemoval = false;
        bool superEdgeRootCurrentMember = false;
        std::vector<std::size_t> superEdgeRootCoveredMemberIndices;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() records producer evidence with
        // "aHistory->Remove(info.edge)", then ::build() exports final children with
        // "builder.Add(openWireCompound, info.wire())". Keep the scoped source/aHistory producer
        // materialization on the same per-edge history entry until MapperHistory/ElementMap can
        // provide the child-wire producer without a staged edge.
        std::optional<TopoDS_Edge> openExportProducerEdge;
        ResultWireProducerIdentity resultWireProducer;
    };
    struct WireJoinerHistoryMaterializationLedger
    {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() reruns findClosedWires(true)/findTightBound() before
        // final openWireCompound export. The rerun gate must know whether generated open-export
        // identity is still a transitional materialization candidate before it mutates live EdgeInfo
        // owners, so keep the candidate statistics on the WireJoiner history ledger itself.
        bool needed = false;
        std::size_t candidateEdgeCount = 0;
        std::size_t unboundEdgeCount = 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build(), sourceEdgeArray -> sourceEdges -> splitEdges() -> buildClosedWire().
        // A closed-cycle generated export is detected from the EdgeInfo split ledger, not from
        // bounded-face result-edge sampling.
        std::size_t closedWireCycleSplitLedgerSourceEdgeCount = 0;
        bool closedWireCycleSplitLedgerOpenExport = false;
        std::vector<WireJoinerHistoryMaterializationBinding> bindings;
        std::vector<WireJoinerHistoryMaterializationEdgeEntry> edgeEntries;
    };
    struct OwnerWireInfo
    {
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
    struct ClosedWireSearchFrame
    {
        std::size_t start = 0;
        std::size_t current = 0;
        std::size_t end = 0;
    };
    struct ClosedWireSearchResult
    {
        std::vector<WireVertex> vertices;
        std::size_t stackFrameCount = 0;
        std::size_t vertexStackCount = 0;
        std::size_t edgeSetVisitCount = 0;
        std::size_t backtrackCount = 0;
        std::size_t intersectSkipCount = 0;
    };
    struct ExhaustAdjacentSearchTrace
    {
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
    struct WireInfo
    {
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
        // ::WireJoinerP::splitEdges() replaces source EdgeInfo rows with multiple Modified
        // fragments before ::buildClosedWire(); this records the request-local closed-cycle precheck.
        std::size_t closedWireCycleSplitLedgerSourceEdgeCount = 0;
        bool closedWireCycleSplitLedgerOpenExport = false;
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
        // ::WireJoinerP::buildClosedWire() reruns owner search before ::build() adds
        // openWireCompound. This field records live rerun owners rejected because their result-wire
        // identity still comes from the generated open-export transition instead of real
        // EdgeInfo/WireInfo/aHistory state.
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
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), "sourceEdges.insert(sourceEdgeArray.begin(),
    // sourceEdgeArray.end())", then ::WireJoinerP::add() may replace a just-added source edge's
    // coincident endpoint using "BRepBuilderAPI_MakeWire mkWire(eOther)". Keep sourceEdges_ as the
    // sourceEdgeArray equivalent for getOpenWires(noOriginal) purge, and keep this separate ledger
    // as FreeCAD's mutable internal sourceEdges equivalent for split/history vertex identity.
    std::vector<TopoDS_Edge> sourceEdgeLedgerEdges_;
    std::vector<WireJoinerVmapReplacementEvent> sourceEdgeLedgerReplacementEvents_;
    std::vector<TopoDS_Edge> sourceEdges_;
    WireJoinerHistorySummary historySummary_;
    void initializeEdgeInfo(EdgeInfo& edgeInfo, const TopoDS_Edge& edge) const;
    TopoDS_Wire wireFromVertices(const WireInfo& info, const std::vector<WireVertex>& vertices) const;
    void rebuildAdjacentList(WireInfo& info);
    std::optional<WireVertex> soleActiveAdjacentEdge(
        const WireInfo& info,
        std::size_t edgeIndex,
        int endpointIndex
    ) const;
    void extendSuperEdgeCandidate(
        const WireInfo& info,
        std::deque<WireVertex>& vertices,
        std::vector<bool>& used,
        bool appendBack,
        bool& closed
    ) const;
    void recordSuperEdgeCandidates(WireInfo& info);
    bool markOpenLeafEdges(WireInfo& info);
    void rebuildOrderedVertices(WireInfo& info);
    std::optional<ClosedWireSearchResult> findClosedWirePath(
        const WireInfo& info,
        std::size_t beginEdgeIndex
    ) const;
    std::size_t assignClosedWireOwners(WireInfo& info, bool assignOwners);
    gp_Pnt vertexPoint(const WireInfo& info, const WireVertex& vertex) const;
    gp_Pnt vertexOtherPoint(const WireInfo& info, const WireVertex& vertex) const;
    bool findTightBoundBranchPathToPoint(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const gp_Pnt& current,
        const gp_Pnt& target,
        std::vector<bool>& usedEdges,
        std::vector<WireVertex>& path
    ) const;
    bool findBranchPathToPointSkippingOwner(
        const WireInfo& info,
        std::size_t skipOwnerId,
        const gp_Pnt& current,
        const gp_Pnt& target,
        std::vector<bool>& usedEdges,
        std::vector<WireVertex>& path
    ) const;
    std::optional<TightBoundTransferPath> tightBoundTransferPathForCandidate(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate
    ) const;
    std::optional<TightBoundTransferPath> tightBoundTransferPathForExistingWireHit(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate,
        const TightBoundExistingWireSearchTrace& trace,
        TightBoundExistingWirePathBlockReason* blockReason = nullptr
    ) const;
    std::optional<std::size_t> ownerVertexIndex(
        const OwnerWireInfo& owner,
        const WireVertex& vertex
    ) const;
    TightBoundExistingWireSearchTrace traceExistingWireSearchForCandidate(
        const WireInfo& info,
        const OwnerWireInfo& owner,
        const TightBoundBranchCandidate& candidate
    ) const;
    bool isDoneOwner(const WireInfo& info, std::size_t ownerId) const;
    void recordExhaustOwnerVertex(WireInfo& info, const WireVertex& vertex, std::size_t ownerId);
    ExhaustAdjacentSearchTrace traceExhaustAdjacentSearch(
        const WireInfo& info,
        const WireVertex& beginVertex,
        const WireVertex& adjacentVertex,
        std::size_t seedOwnerId
    ) const;
    void recordExhaustAdjacentSecondaryOwners(WireInfo& info);
    void recordBuildClosedWireRemovalLifecycle(WireInfo& info);
    void recordRepeatedSplitExhaustRerunLifecycle(
        WireInfo& info,
        const std::vector<TopoDS_Face>& boundedFaces,
        const WireJoinerHistoryMaterializationLedger& materializationLedger
    );
    WireJoinerHistoryMaterializationLedger computeWireJoinerHistoryMaterializationLedger(
        const WireInfo& info,
        const TopoDS_Shape& boundedFaceShape,
        const std::vector<TopoDS_Wire>& closedWires,
        const std::vector<TopoDS_Edge>& openEdges,
        bool splitProducedBoundedFaces,
        bool hasOpenWireOutput
    ) const;
    std::size_t closedWireCycleSplitLedgerSourceEdgeCount(
        const WireInfo& info,
        const std::vector<TopoDS_Wire>& closedWires
    ) const;
    bool wireJoinerHistoryMaterializationLedgerHasUnsafeProducer(
        const WireInfo& info,
        const WireJoinerHistoryMaterializationLedger& materializationLedger
    ) const;
    bool edgeInfoExportsOpenWireCompound(const EdgeInfo& edgeInfo) const;
    bool edgeInfoHasOpenWireCompoundLedgerSlot(
        const EdgeInfo& edgeInfo,
        bool materializedChildSlot = false
    ) const;
    bool resultWireProducerSlotHasFullAHistoryEvidence(
        const EdgeInfo& edgeInfo
    ) const;
    bool resultWireProducerRootHasFullAHistoryEvidence(
        const EdgeInfo& edgeInfo
    ) const;
    bool resultWireProducerRootCanSuppressPendingMember(
        const EdgeInfo& edgeInfo
    ) const;
    bool resultWireProducerSlotHasSafeAHistoryEvidence(
        const EdgeInfo& edgeInfo
    ) const;
    void updateOpenWireCompoundNoOriginalPurgeVerdict(
        OpenWireCompoundWireInfo& childWire
    ) const;
    void updateOpenWireCompoundNoOriginalGroupPurgeVerdicts(
        WireInfo& info
    ) const;
    bool openWireCompoundChildWirePurgedByNoOriginal(
        const OpenWireCompoundWireInfo& childWire,
        bool noOriginal
    ) const;
    std::optional<std::size_t> superEdgeRootIndexForMember(
        const WireInfo& info,
        const EdgeInfo& edgeInfo
    ) const;
    std::vector<std::size_t> strictRemovedSourceEdgeInfoIndicesForSourceLineage(
        const WireInfo& info,
        const EdgeInfo& edgeInfo
    ) const;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records source producer evidence with
    // "aHistory->Remove(info.edge)", and ::build() publishes only concrete children through
    // "builder.Add(openWireCompound, info.wire())". A cad-core producer-ledger-ready slot must carry
    // a non-null scoped producer export edge before consumers can treat it as materialized output.
    bool wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    const TopoDS_Edge& resultWireProducerOpenExportEdge(
        const EdgeInfo& edgeInfo,
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    std::optional<std::size_t> producerLedgerReadyAHistoryRemoveProducerIndex(
        const WireInfo& info,
        const EdgeInfo& edgeInfo,
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        const TopoDS_Edge* resultEdge = nullptr
    ) const;
    std::optional<std::size_t> producerLedgerReadySameSourceSidecarProducerIndex(
        const WireInfo& info,
        const EdgeInfo& edgeInfo,
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        const TopoDS_Edge* resultEdge = nullptr
    ) const;
    ResultWireProducerIdentity classifyResultWireProducerSlot(
        const WireInfo& info,
        std::size_t edgeInfoIndex,
        const WireJoinerHistoryMaterializationLedger& materializationLedger
    ) const;
    void attachResultWireProducerLedger(
        WireInfo& info,
        WireJoinerHistoryMaterializationLedger& materializationLedger
    );
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() emits final children through
    // "builder.Add(openWireCompound, info.wire())", then ::getOpenWires() consumes
    // MapperHistory(aHistory). Publish a child-wire producer entry only after the
    // EdgeInfo candidate has been classified into a finite producer identity or blocker; the raw
    // candidate bit stays an internal diagnostic.
    bool resultWireProducerIdentityPublishesChildWireLedgerEntry(
        const ResultWireProducerIdentity& identity
    ) const;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() emits child wires with
    // "builder.Add(openWireCompound, info.wire())"; ::getOpenWires() then consumes
    // MapperHistory(aHistory). A child source-edge producer output is proven by the child-wire
    // producer edge ledger, not by a copied EdgeInfo/result-slot sidecar flag.
    bool openWireCompoundChildWireHasSourceEdgeProducerOutput(
        const OpenWireCompoundWireInfo& childWire
    ) const;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() emits final child wires with
    // "builder.Add(openWireCompound, info.wire())"; ::getOpenWires() consumes that child compound with
    // MapperHistory(aHistory). The public open-export source is therefore derived from the finalized
    // child-wire ledger, not copied from a history-materialization edge entry.
    OpenWireCompoundExportSource childWireFinalOpenExportSource(
        const OpenWireCompoundWireInfo& childWire
    ) const;
    ResultWireProducerIdentity childWireResultWireProducerIdentity(
        const WireInfo& info,
        const OpenWireCompoundWireInfo& childWire,
        std::size_t childWireIndex,
        const WireJoinerHistoryMaterializationLedger& materializationLedger
    ) const;
    bool memberSuppressedCurrentMemberProducerLedgerReady(
        const WireInfo& info,
        const OpenWireCompoundWireInfo& childWire,
        std::size_t childWireIndex
    ) const;
    ResultWireProducerLedgerEntry resultWireProducerLedgerEntryForChildWire(
        const OpenWireCompoundWireInfo& childWire,
        std::size_t childWireIndex
    ) const;
    void applyWireJoinerHistoryMaterialization(
        WireInfo& info,
        WireJoinerHistoryMaterializationLedger& materializationLedger
    );
    void recordBranchSearchCandidatesForOwner(
        WireInfo& info,
        OwnerWireInfo& owner,
        const std::vector<TopoDS_Face>& boundedFaces
    );
    void recordBranchSearchCandidates(WireInfo& info, const std::vector<TopoDS_Face>& boundedFaces);
    bool recordTightBoundTransferWire(WireInfo& info, OwnerWireInfo& owner);
    void recordTightBoundLifecycle(WireInfo& info);
    void recordExhaustTightBoundLifecycle(WireInfo& info);
    void recordOpenWireCompoundLedger(
        WireInfo& info,
        WireJoinerHistoryMaterializationLedger& materializationLedger
    );
};

}  // namespace cad_core::part
