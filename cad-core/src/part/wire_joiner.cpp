#include "cad_core/part/wire_joiner.h"

#include "cad_core/app/element_map_producer_trace.h"

#include "internal_shape_history_ledger_detail.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Precision.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <utility>

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

struct WireJoiner::Impl
{
    void setTightBound(bool enabled);
    void setMergeEdges(bool enabled);
    void addOpenWire(
        const TopoDS_Wire& wire,
        const std::vector<std::size_t>& sourceEdgeIndices = {}
    );
    void buildFinalEdgeOwnership(
        const TopoDS_Shape* boundedFaceShape = nullptr,
        const std::vector<TopoDS_Wire>* closedWires = nullptr,
        const std::vector<TopoDS_Edge>* openEdges = nullptr,
        bool splitProducedBoundedFaces = false
    );
    void addSourceEdge(const TopoDS_Edge& edge);
    std::optional<TopoDS_Shape> getOpenWires(
        const std::string& historyPrefix,
        bool noOriginal = true
    ) const;
    WireJoinerBuildResult buildResult(
        const std::string& historyPrefix,
        bool noOriginal = true
    ) const;

    bool tightBound_ = false;
    bool mergeEdges_ = false;
    app::ElementMapProducerTrace* producerTrace_ = nullptr;
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
    struct WireJoinerMapperHistoryProducerEvidence
    {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() records producer history with
        // "aHistory->Remove(info.edge)", and ::getOpenWires() later consumes
        // "MapperHistory(aHistory)". This request-local evidence is the MapperHistory input for a
        // final EdgeInfo child-wire producer; the per-edge materialization entry only carries
        // EdgeInfo/WireInfo lifecycle state.
        std::size_t edgeInfoIndex = resultWireProducerNpos;
        TopoDS_Edge producerShape;
    };
    struct WireJoinerHistoryMaterializationEdgeEntry
    {
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
        std::vector<WireJoinerMapperHistoryProducerEvidence> mapperHistoryProducerEvidence;
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
    // "builder.Add(openWireCompound, info.wire())". A cad-core producer-ledger-ready slot is proven
    // by MapperHistory producer evidence plus the materialized openWireCompound child lifecycle.
    bool wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    const TopoDS_Edge* wireJoinerMapperHistoryProducerEvidenceEdge(
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    bool wireJoinerMapperHistoryProducerEvidenceReady(
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    void recordWireJoinerMapperHistoryProducerEvidence(
        WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex,
        const TopoDS_Edge& producerShape
    ) const;
    bool wireJoinerMapperHistoryProducerEvidenceHasChildWire(
        const WireJoinerHistoryMaterializationLedger& materializationLedger,
        std::size_t edgeInfoIndex
    ) const;
    const TopoDS_Edge& resultWireProducerMapperHistoryInputEdge(
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
    // producer wire ledger, not by a copied EdgeInfo/result-slot sidecar flag.
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
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls findTightBound()/exhaustTightBound(); this summary is
    // module-local evidence for buildResult() diagnostics, not a caller contract.
    WireJoinerLedgerSummary ledgerSummary() const;
    WireJoinerHistorySummary historySummary() const;
};

const char* resultWireProducerKindName(ResultWireProducerKind kind)
{
    switch (kind) {
        case ResultWireProducerKind::None:
            return "None";
        case ResultWireProducerKind::ExistingSourceEdge:
            return "ExistingSourceEdge";
        case ResultWireProducerKind::PartialSharedClosedWire:
            return "PartialSharedClosedWire";
        case ResultWireProducerKind::LiveResetOpenEdge:
            return "LiveResetOpenEdge";
        case ResultWireProducerKind::SuperEdgeRoot:
            return "SuperEdgeRoot";
        case ResultWireProducerKind::CurrentMemberChildWire:
            return "CurrentMemberChildWire";
    }
    return "None";
}

const char* resultWireProducerStateName(ResultWireProducerState state)
{
    switch (state) {
        case ResultWireProducerState::Unpublished:
            return "Unpublished";
        case ResultWireProducerState::ProducerLocated:
            return "ProducerLocated";
        case ResultWireProducerState::AHistoryEvidenceReady:
            return "AHistoryEvidenceReady";
        case ResultWireProducerState::ChildWireReady:
            return "ChildWireReady";
        case ResultWireProducerState::ProducerLedgerReady:
            return "ProducerLedgerReady";
        case ResultWireProducerState::ExportedWithoutTransitionalSlot:
            return "ExportedWithoutTransitionalSlot";
    }
    return "Unpublished";
}

const char* resultWireBlockerName(ResultWireBlocker blocker)
{
    switch (blocker) {
        case ResultWireBlocker::None:
            return "None";
        case ResultWireBlocker::MissingSourceLineage:
            return "MissingSourceLineage";
        case ResultWireBlocker::MissingAHistoryRemoveSource:
            return "MissingAHistoryRemoveSource";
        case ResultWireBlocker::ForeignAHistorySourceLineage:
            return "ForeignAHistorySourceLineage";
        case ResultWireBlocker::ForeignAHistorySourceShapeReadyLineageMismatch:
            return "ForeignAHistorySourceShapeReadyLineageMismatch";
        case ResultWireBlocker::ForeignAHistorySourceShapeIdentityNotReady:
            return "ForeignAHistorySourceShapeIdentityNotReady";
        case ResultWireBlocker::ForeignAHistorySourceGeometryMismatch:
            return "ForeignAHistorySourceGeometryMismatch";
        case ResultWireBlocker::MissingRemovedTargetEvidence:
            return "MissingRemovedTargetEvidence";
        case ResultWireBlocker::MissingFullAHistoryProducerEvidence:
            return "MissingFullAHistoryProducerEvidence";
        case ResultWireBlocker::FinalGateBlockedByIteration:
            return "FinalGateBlockedByIteration";
        case ResultWireBlocker::FinalGateBlockedByWireInfo:
            return "FinalGateBlockedByWireInfo";
        case ResultWireBlocker::RootRemovedByUnownedBranch:
            return "RootRemovedByUnownedBranch";
        case ResultWireBlocker::RootRemovedByPrimaryBranch:
            return "RootRemovedByPrimaryBranch";
        case ResultWireBlocker::RootRemovedBySecondaryBranch:
            return "RootRemovedBySecondaryBranch";
        case ResultWireBlocker::MultiMemberRootPendingSuppression:
            return "MultiMemberRootPendingSuppression";
        case ResultWireBlocker::SourceShapeIdentityNotReady:
            return "SourceShapeIdentityNotReady";
        case ResultWireBlocker::SourceShapeWouldPurgeOriginal:
            return "SourceShapeWouldPurgeOriginal";
        case ResultWireBlocker::LiveResetSourceShapeWouldPurgeOriginal:
            return "LiveResetSourceShapeWouldPurgeOriginal";
        case ResultWireBlocker::CurrentMemberSourceShapeWouldPurgeOriginal:
            return "CurrentMemberSourceShapeWouldPurgeOriginal";
        case ResultWireBlocker::SameSourceSidecarSourceShapeIdentityNotReady:
            return "SameSourceSidecarSourceShapeIdentityNotReady";
        case ResultWireBlocker::SameSourceSidecarGeometryMismatch:
            return "SameSourceSidecarGeometryMismatch";
        case ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady:
            return "SourceShapeMemberVertexIdentityNotReady";
        case ResultWireBlocker::CurrentMemberVertexMultiplicityBlocked:
            return "CurrentMemberVertexMultiplicityBlocked";
        case ResultWireBlocker::CurrentMemberChildWireIdentityNotReady:
            return "CurrentMemberChildWireIdentityNotReady";
        case ResultWireBlocker::CurrentMemberMissingSidecarEvidence:
            return "CurrentMemberMissingSidecarEvidence";
        case ResultWireBlocker::CurrentMemberRootOpenProducerNotReady:
            return "CurrentMemberRootOpenProducerNotReady";
        case ResultWireBlocker::CurrentMemberSidecarGeometryMismatch:
            return "CurrentMemberSidecarGeometryMismatch";
        case ResultWireBlocker::UnknownInvariant:
            return "UnknownInvariant";
    }
    return "UnknownInvariant";
}

const char* wireJoinerHistoryRelationName(WireJoinerHistoryRelation relation)
{
    switch (relation) {
        case WireJoinerHistoryRelation::Preserved:
            return "preserved";
        case WireJoinerHistoryRelation::Split:
            return "split";
        case WireJoinerHistoryRelation::Generated:
            return "generated";
        case WireJoinerHistoryRelation::Deleted:
            return "deleted";
    }
    return "preserved";
}

const char* openWireCompoundExportSourceName(OpenWireCompoundExportSource source)
{
    switch (source) {
        case OpenWireCompoundExportSource::None:
            return "None";
        case OpenWireCompoundExportSource::OpenLeafIterationMinus3:
            return "OpenLeafIterationMinus3";
        case OpenWireCompoundExportSource::UnownedOpenEdge:
            return "UnownedOpenEdge";
        case OpenWireCompoundExportSource::AHistoryProducerChildWire:
            return "AHistoryProducerChildWire";
        case OpenWireCompoundExportSource::RootCurrentMemberChildProducer:
            return "RootCurrentMemberChildProducer";
    }
    return "None";
}

namespace
{

int resultWireProducerStateRank(ResultWireProducerState state)
{
    switch (state) {
        case ResultWireProducerState::Unpublished:
            return 0;
        case ResultWireProducerState::ProducerLocated:
            return 1;
        case ResultWireProducerState::AHistoryEvidenceReady:
            return 2;
        case ResultWireProducerState::ChildWireReady:
            return 3;
        case ResultWireProducerState::ProducerLedgerReady:
            return 4;
        case ResultWireProducerState::ExportedWithoutTransitionalSlot:
            return 5;
    }
    return 0;
}

bool resultWireProducerStateAtLeast(ResultWireProducerState state, ResultWireProducerState threshold)
{
    return resultWireProducerStateRank(state) >= resultWireProducerStateRank(threshold);
}

std::vector<TopoDS_Edge> wireEdges(const TopoDS_Wire& wire)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

std::vector<TopoDS_Vertex> wireVertices(const TopoDS_Wire& wire)
{
    std::vector<TopoDS_Vertex> vertices;
    for (TopExp_Explorer explorer(wire, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        vertices.push_back(TopoDS::Vertex(explorer.Current()));
    }
    return vertices;
}

bool samePoint(const gp_Pnt& lhs, const gp_Pnt& rhs)
{
    return lhs.SquareDistance(rhs) <= Precision::SquareConfusion();
}

std::pair<gp_Pnt, gp_Pnt> edgeEndpoints(const TopoDS_Edge& edge)
{
    return {BRep_Tool::Pnt(TopExp::FirstVertex(edge)), BRep_Tool::Pnt(TopExp::LastVertex(edge))};
}

bool edgeMatchesSourceVertices(const TopoDS_Edge& edge, const TopoDS_Edge& source)
{
    const auto [first, last] = edgeEndpoints(edge);
    const auto [sourceFirst, sourceLast] = edgeEndpoints(source);
    return (samePoint(first, sourceFirst) && samePoint(last, sourceLast))
        || (samePoint(first, sourceLast) && samePoint(last, sourceFirst));
}

std::vector<TopoDS_Edge> boundaryEdges(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

std::vector<TopoDS_Face> facesForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        faces.push_back(TopoDS::Face(explorer.Current()));
    }
    return faces;
}

std::vector<TopoDS_Wire> wiresFromEdges(const std::vector<TopoDS_Edge>& edges)
{
    std::vector<TopoDS_Wire> wires;
    std::vector<bool> used(edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < edges.size(); ++startIndex) {
        if (used[startIndex] || edges[startIndex].IsNull()) {
            continue;
        }

        BRepBuilderAPI_MakeWire builder;
        std::deque<TopoDS_Edge> ordered;
        ordered.push_back(edges[startIndex]);
        used[startIndex] = true;
        auto [currentStart, currentEnd] = edgeEndpoints(edges[startIndex]);

        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (used[index] || edges[index].IsNull()) {
                    continue;
                }
                const auto [edgeStart, edgeEnd] = edgeEndpoints(edges[index]);
                if (samePoint(edgeStart, currentEnd)) {
                    ordered.push_back(edges[index]);
                    currentEnd = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentEnd)) {
                    ordered.push_back(TopoDS::Edge(edges[index].Reversed()));
                    currentEnd = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentStart)) {
                    ordered.push_front(edges[index]);
                    currentStart = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeStart, currentStart)) {
                    ordered.push_front(TopoDS::Edge(edges[index].Reversed()));
                    currentStart = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
            }
        }

        for (const TopoDS_Edge& edge : ordered) {
            builder.Add(edge);
        }
        if (builder.IsDone()) {
            wires.push_back(builder.Wire());
        }
    }

    return wires;
}

struct SplitEdgeRecord
{
    TopoDS_Edge edge;
    std::vector<std::size_t> sourceEdgeIndices;
    std::vector<std::size_t> modifiedSourceEdgeIndices;
    std::vector<std::size_t> generatedSourceEdgeIndices;
    bool fromSplitterHistory = false;
    bool fromModifiedHistory = false;
    bool fromGeneratedHistory = false;
    bool sourceLineageFromIdentityFallback = false;
    bool sourceLineageFromSourceIdentityFallback = false;
    bool sourceLineageFromHistoryShapeGeometryBridge = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::splitEdges() only removes/re-adds an EdgeInfo when "splits.size() > 1"; a
    // singleton Modified result remains an original sourceEdgeArray open edge for
    // ::getOpenWires(noOriginal=true).
    bool fromSingleModifiedSourceEdge = false;
};

struct SplitEdgesResult
{
    std::vector<SplitEdgeRecord> records;
    WireJoinerHistorySummary history;
};

bool edgeEquivalentByGeometryAndEndpoints(const TopoDS_Edge& left, const TopoDS_Edge& right);

void appendUniqueSourceIndex(std::vector<std::size_t>& indices, std::size_t sourceIndex)
{
    if (std::find(indices.begin(), indices.end(), sourceIndex) == indices.end()) {
        indices.push_back(sourceIndex);
    }
}

void appendUniqueSourceIndices(std::vector<std::size_t>& target, const std::vector<std::size_t>& source)
{
    for (const std::size_t index : source) {
        appendUniqueSourceIndex(target, index);
    }
}

bool sourceEdgeIndicesIntersect(
    const std::vector<std::size_t>& left,
    const std::vector<std::size_t>& right
)
{
    return std::any_of(left.begin(), left.end(), [&](std::size_t leftIndex) {
        return std::find(right.begin(), right.end(), leftIndex) != right.end();
    });
}

std::vector<std::size_t> sourceEdgeIndicesByIdentity(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sourceEdges,
    bool* matchedByGeometryFallback = nullptr
)
{
    if (matchedByGeometryFallback != nullptr) {
        *matchedByGeometryFallback = false;
    }
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < sourceEdges.size(); ++index) {
        if (!edge.IsNull() && !sourceEdges[index].IsNull() && edge.IsSame(sourceEdges[index])) {
            indices.push_back(index);
        }
    }
    if (!indices.empty()) {
        return indices;
    }
    for (std::size_t index = 0; index < sourceEdges.size(); ++index) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build(), "sourceEdges.insert(sourceEdgeArray.begin(),
        // sourceEdgeArray.end())", then splitEdges() records
        // "aHistory->AddModified(split.intersectShape, newInfo.edge)". cad-core can receive copied
        // face/open-wire edges before split; when exact sourceEdgeArray identity is already lost,
        // recover only the request-local source index for the copied EdgeInfo, not result-wire candidate output
        // geometry or result-wire ownership.
        if (!edge.IsNull() && !sourceEdges[index].IsNull()
            && edgeEquivalentByGeometryAndEndpoints(edge, sourceEdges[index])) {
            indices.push_back(index);
            if (matchedByGeometryFallback != nullptr) {
                *matchedByGeometryFallback = true;
            }
        }
    }
    return indices;
}

std::vector<std::size_t> inputSourceEdgeIndicesByIdentity(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& inputEdges,
    const std::vector<std::vector<std::size_t>>& inputSourceEdgeIndices
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() feeds sourceEdgeArray into add(), then ::splitEdges() mutates EdgeInfo
    // rows while preserving their aHistory source. For unchanged splitter records, prefer the
    // request-local input EdgeInfo sidecar over rediscovering sourceEdgeArray membership by geometry.
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < inputEdges.size() && index < inputSourceEdgeIndices.size();
         ++index) {
        if (!edge.IsNull() && !inputEdges[index].IsNull() && edge.IsSame(inputEdges[index])) {
            appendUniqueSourceIndices(indices, inputSourceEdgeIndices[index]);
        }
    }
    return indices;
}

std::vector<TopoDS_Edge> shapeEdgesForLineage(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    if (shape.IsNull()) {
        return edges;
    }
    if (shape.ShapeType() == TopAbs_EDGE) {
        edges.push_back(TopoDS::Edge(shape));
        return edges;
    }
    return boundaryEdges(shape);
}

void appendLineageForHistoryShape(
    std::vector<SplitEdgeRecord>& records,
    const TopoDS_Shape& historyShape,
    const std::vector<std::size_t>& sourceIndices,
    bool modifiedHistory,
    bool generatedHistory,
    bool sourceLineageFromSourceIdentityFallback
)
{
    if (sourceIndices.empty()) {
        return;
    }
    for (const TopoDS_Edge& historyEdge : shapeEdgesForLineage(historyShape)) {
        bool matchedByIdentity = false;
        for (SplitEdgeRecord& record : records) {
            if (!record.edge.IsNull() && record.edge.IsSame(historyEdge)) {
                appendUniqueSourceIndices(record.sourceEdgeIndices, sourceIndices);
                if (modifiedHistory) {
                    appendUniqueSourceIndices(record.modifiedSourceEdgeIndices, sourceIndices);
                    record.fromModifiedHistory = true;
                }
                if (generatedHistory) {
                    appendUniqueSourceIndices(record.generatedSourceEdgeIndices, sourceIndices);
                    record.fromGeneratedHistory = true;
                }
                record.fromSplitterHistory = true;
                if (sourceLineageFromSourceIdentityFallback) {
                    record.sourceLineageFromIdentityFallback = true;
                    record.sourceLineageFromSourceIdentityFallback = true;
                }
                matchedByIdentity = true;
            }
        }
        if (matchedByIdentity) {
            continue;
        }
        for (SplitEdgeRecord& record : records) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::splitEdges(), after add(split.edge, ...), records
            // "aHistory->AddModified(split.intersectShape, newInfo.edge)". When OCCT gives the
            // history edge as a copied shape rather than the same TShape held in splitter.Shape(),
            // bind lineage to the existing result record instead of creating a result-wire candidate output edge.
            if (!record.edge.IsNull()
                && edgeEquivalentByGeometryAndEndpoints(record.edge, historyEdge)) {
                appendUniqueSourceIndices(record.sourceEdgeIndices, sourceIndices);
                if (modifiedHistory) {
                    appendUniqueSourceIndices(record.modifiedSourceEdgeIndices, sourceIndices);
                    record.fromModifiedHistory = true;
                }
                if (generatedHistory) {
                    appendUniqueSourceIndices(record.generatedSourceEdgeIndices, sourceIndices);
                    record.fromGeneratedHistory = true;
                }
                record.fromSplitterHistory = true;
                record.sourceLineageFromIdentityFallback = true;
                record.sourceLineageFromSourceIdentityFallback =
                    record.sourceLineageFromSourceIdentityFallback || sourceLineageFromSourceIdentityFallback;
                record.sourceLineageFromHistoryShapeGeometryBridge = true;
            }
        }
    }
}

void appendInputEdgeRecord(
    SplitEdgesResult& result,
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sourceEdges,
    const std::vector<std::size_t>& sourceEdgeIndices = {}
)
{
    SplitEdgeRecord record;
    record.edge = edge;
    if (!sourceEdgeIndices.empty()) {
        record.sourceEdgeIndices = sourceEdgeIndices;
    }
    else {
        bool matchedByGeometryFallback = false;
        record.sourceEdgeIndices = sourceEdgeIndicesByIdentity(
            edge,
            sourceEdges,
            &matchedByGeometryFallback
        );
        record.sourceLineageFromIdentityFallback = matchedByGeometryFallback;
        record.sourceLineageFromSourceIdentityFallback = matchedByGeometryFallback;
    }
    result.records.push_back(std::move(record));
}

SplitEdgesResult splitEdgesAtIntersections(
    const std::vector<TopoDS_Edge>& edges,
    const std::vector<TopoDS_Edge>& sourceEdges,
    const std::vector<std::vector<std::size_t>>& inputSourceEdgeIndices = {}
)
{
    SplitEdgesResult result;
    const std::vector<TopoDS_Edge>& lineageSources = sourceEdges.empty() ? edges : sourceEdges;
    result.history.sourceEdgeCount = lineageSources.size();
    if (edges.size() <= 1U) {
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const std::vector<std::size_t> emptySourceIndices;
            const std::vector<std::size_t>& sourceIndices =
                edgeIndex < inputSourceEdgeIndices.size()
                ? inputSourceEdgeIndices[edgeIndex]
                : emptySourceIndices;
            appendInputEdgeRecord(result, edges[edgeIndex], lineageSources, sourceIndices);
        }
        return result;
    }

    TopTools_ListOfShape arguments;
    for (const TopoDS_Edge& edge : edges) {
        if (!edge.IsNull()) {
            arguments.Append(edge);
        }
    }
    if (arguments.Size() <= 1) {
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const std::vector<std::size_t> emptySourceIndices;
            const std::vector<std::size_t>& sourceIndices =
                edgeIndex < inputSourceEdgeIndices.size()
                ? inputSourceEdgeIndices[edgeIndex]
                : emptySourceIndices;
            appendInputEdgeRecord(result, edges[edgeIndex], lineageSources, sourceIndices);
        }
        return result;
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(arguments);
    splitter.SetToFillHistory(Standard_True);
    splitter.SetRunParallel(Standard_True);
    splitter.SetNonDestructive(Standard_True);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const std::vector<std::size_t> emptySourceIndices;
            const std::vector<std::size_t>& sourceIndices =
                edgeIndex < inputSourceEdgeIndices.size()
                ? inputSourceEdgeIndices[edgeIndex]
                : emptySourceIndices;
            appendInputEdgeRecord(result, edges[edgeIndex], lineageSources, sourceIndices);
        }
        return result;
    }

    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_EDGE); explorer.More(); explorer.Next()) {
        SplitEdgeRecord record;
        record.edge = TopoDS::Edge(explorer.Current());
        result.records.push_back(std::move(record));
    }

    for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        const TopoDS_Edge& edge = edges[edgeIndex];
        if (edge.IsNull()) {
            continue;
        }
        const std::vector<std::size_t> emptySourceIndices;
        const std::vector<std::size_t>& explicitSourceIndices =
            edgeIndex < inputSourceEdgeIndices.size()
            ? inputSourceEdgeIndices[edgeIndex]
            : emptySourceIndices;
        const TopTools_ListOfShape& modified = splitter.Modified(edge);
        if (!modified.IsEmpty()) {
            ++result.history.modifiedSourceEdgeCount;
            bool sourceMatchedByGeometryFallback = false;
            const std::vector<std::size_t> sourceIndices = !explicitSourceIndices.empty()
                ? explicitSourceIndices
                : sourceEdgeIndicesByIdentity(
                      edge,
                      lineageSources,
                      &sourceMatchedByGeometryFallback
                  );
            for (TopTools_ListIteratorOfListOfShape it(modified); it.More(); it.Next()) {
                ++result.history.modifiedHistoryCount;
                appendLineageForHistoryShape(
                    result.records,
                    it.Value(),
                    sourceIndices,
                    true,
                    false,
                    sourceMatchedByGeometryFallback
                );
            }
        }
        const TopTools_ListOfShape& generated = splitter.Generated(edge);
        bool generatedSourceMatchedByGeometryFallback = false;
        const std::vector<std::size_t> sourceIndices = !explicitSourceIndices.empty()
            ? explicitSourceIndices
            : sourceEdgeIndicesByIdentity(
                  edge,
                  lineageSources,
                  &generatedSourceMatchedByGeometryFallback
              );
        for (TopTools_ListIteratorOfListOfShape it(generated); it.More(); it.Next()) {
            ++result.history.generatedHistoryCount;
            appendLineageForHistoryShape(
                result.records,
                it.Value(),
                sourceIndices,
                false,
                true,
                generatedSourceMatchedByGeometryFallback
            );
        }
        if (splitter.IsDeleted(edge)) {
            ++result.history.deletedHistoryCount;
        }
    }

    if (result.records.empty()) {
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const std::vector<std::size_t> emptySourceIndices;
            const std::vector<std::size_t>& sourceIndices =
                edgeIndex < inputSourceEdgeIndices.size()
                ? inputSourceEdgeIndices[edgeIndex]
                : emptySourceIndices;
            appendInputEdgeRecord(result, edges[edgeIndex], lineageSources, sourceIndices);
        }
    }
    for (SplitEdgeRecord& record : result.records) {
        if (record.sourceEdgeIndices.empty()) {
            record.sourceEdgeIndices = inputSourceEdgeIndicesByIdentity(
                record.edge,
                edges,
                inputSourceEdgeIndices
            );
        }
        if (record.sourceEdgeIndices.empty()) {
            bool matchedByGeometryFallback = false;
            record.sourceEdgeIndices = sourceEdgeIndicesByIdentity(
                record.edge,
                lineageSources,
                &matchedByGeometryFallback
            );
            record.sourceLineageFromIdentityFallback = matchedByGeometryFallback;
            record.sourceLineageFromSourceIdentityFallback = matchedByGeometryFallback;
        }
    }
    std::map<std::size_t, std::size_t> modifiedFragmentCountBySourceEdge;
    for (const SplitEdgeRecord& record : result.records) {
        if (!record.fromModifiedHistory) {
            continue;
        }
        for (const std::size_t sourceIndex : record.modifiedSourceEdgeIndices) {
            ++modifiedFragmentCountBySourceEdge[sourceIndex];
        }
    }
    for (SplitEdgeRecord& record : result.records) {
        if (!record.fromModifiedHistory || record.modifiedSourceEdgeIndices.empty()) {
            continue;
        }
        record.fromSingleModifiedSourceEdge = std::any_of(
            record.modifiedSourceEdgeIndices.begin(),
            record.modifiedSourceEdgeIndices.end(),
            [&](std::size_t sourceIndex) {
                const auto countIt = modifiedFragmentCountBySourceEdge.find(sourceIndex);
                return countIt != modifiedFragmentCountBySourceEdge.end() && countIt->second == 1U;
            }
        );
    }
    result.history.splitResultEdgeCount = result.records.size();
    result.history.splitterHistory = result.history.modifiedHistoryCount > 0U
        || result.history.generatedHistoryCount > 0U || result.history.deletedHistoryCount > 0U;
    return result;
}

std::vector<TopoDS_Vertex> edgeVertices(const TopoDS_Edge& edge)
{
    std::vector<TopoDS_Vertex> vertices;
    for (TopExp_Explorer explorer(edge, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        vertices.push_back(TopoDS::Vertex(explorer.Current()));
    }
    return vertices;
}

bool vertexTouchesBoundary(const TopoDS_Vertex& vertex, const TopoDS_Edge& boundary)
{
    if (vertex.IsNull() || boundary.IsNull()) {
        return false;
    }
    BRepExtrema_DistShapeShape distance(vertex, boundary);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= Precision::Confusion();
}

gp_Pnt edgeMidpoint(const TopoDS_Edge& edge)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (!curve.IsNull()) {
        return curve->Value((first + last) * 0.5);
    }
    const auto [start, end] = edgeEndpoints(edge);
    return gp_Pnt((start.X() + end.X()) * 0.5, (start.Y() + end.Y()) * 0.5, (start.Z() + end.Z()) * 0.5);
}

bool pointInsideOrOnAnyFace(const gp_Pnt& point, const std::vector<TopoDS_Face>& faces)
{
    for (const TopoDS_Face& face : faces) {
        if (face.IsNull()) {
            continue;
        }
        BRepClass_FaceClassifier classifier(face, point, Precision::Confusion());
        if (classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON) {
            return true;
        }
    }
    return false;
}

bool vertexIsOriginalSourceByIdentity(
    const TopoDS_Vertex& vertex,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    if (vertex.IsNull()) {
        return false;
    }
    for (const TopoDS_Edge& sourceEdge : sourceEdges) {
        for (const TopoDS_Vertex& sourceVertex : edgeVertices(sourceEdge)) {
            if (vertex.IsSame(sourceVertex)) {
                return true;
            }
        }
    }
    return false;
}

bool edgeEndpointsBackedByLedgerIdentity(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& ledgerEdges
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same
    // TopoDS_Vertex"; ::WireJoinerP::splitEdges() then re-adds fragments with "add(split.edge,
    // false, split.bbox, it)". A producer child can bypass result-slot endpoint evidence only when
    // both endpoints are already represented by the mutable source/split ledger identity.
    if (edge.IsNull() || ledgerEdges.empty()) {
        return false;
    }

    const std::array<TopoDS_Vertex, 2> endpoints {
        TopExp::FirstVertex(edge),
        TopExp::LastVertex(edge),
    };
    return std::all_of(endpoints.begin(), endpoints.end(), [&](const TopoDS_Vertex& endpoint) {
        if (endpoint.IsNull()) {
            return false;
        }
        return std::any_of(ledgerEdges.begin(), ledgerEdges.end(), [&](const TopoDS_Edge& ledgerEdge) {
            if (ledgerEdge.IsNull()) {
                return false;
            }
            const std::array<TopoDS_Vertex, 2> ledgerVertices {
                TopExp::FirstVertex(ledgerEdge),
                TopExp::LastVertex(ledgerEdge),
            };
            return std::any_of(
                ledgerVertices.begin(),
                ledgerVertices.end(),
                [&](const TopoDS_Vertex& ledgerVertex) {
                    return !ledgerVertex.IsNull() && endpoint.IsSame(ledgerVertex);
                }
            );
        });
    });
}

void recordSourceVertexReplacementEvidence(
    const TopoDS_Vertex& vertex,
    const std::vector<TopoDS_Edge>& sourceEdges,
    int& sourceEdgeIndex,
    int& sourceEndpoint,
    bool& sourceIdentity
)
{
    sourceEdgeIndex = -1;
    sourceEndpoint = -1;
    sourceIdentity = false;
    if (vertex.IsNull()) {
        return;
    }

    const gp_Pnt point = BRep_Tool::Pnt(vertex);
    double bestSquareDistance = std::numeric_limits<double>::infinity();
    double bestTolerance = Precision::Confusion();
    for (std::size_t index = 0; index < sourceEdges.size(); ++index) {
        if (sourceEdges[index].IsNull()) {
            continue;
        }
        const std::array<TopoDS_Vertex, 2> sourceVertices {
            TopExp::FirstVertex(sourceEdges[index]),
            TopExp::LastVertex(sourceEdges[index]),
        };
        for (std::size_t endpoint = 0; endpoint < sourceVertices.size(); ++endpoint) {
            const TopoDS_Vertex& sourceVertex = sourceVertices[endpoint];
            if (sourceVertex.IsNull()) {
                continue;
            }
            const bool identity = vertex.IsSame(sourceVertex);
            const double squareDistance = identity
                ? 0.0
                : point.SquareDistance(BRep_Tool::Pnt(sourceVertex));
            if (squareDistance >= bestSquareDistance && !identity) {
                continue;
            }
            bestSquareDistance = squareDistance;
            bestTolerance = std::max(
                {Precision::Confusion(), BRep_Tool::Tolerance(vertex), BRep_Tool::Tolerance(sourceVertex)}
            );
            sourceEdgeIndex = static_cast<int>(index);
            sourceEndpoint = static_cast<int>(endpoint);
            sourceIdentity = identity;
            if (identity) {
                return;
            }
        }
    }

    if (sourceEdgeIndex >= 0 && bestSquareDistance > bestTolerance * bestTolerance) {
        sourceEdgeIndex = -1;
        sourceEndpoint = -1;
        sourceIdentity = false;
    }
}

struct LedgerVertexReplacementCandidate
{
    TopoDS_Edge edge;
    TopoDS_Vertex vertex;
    std::size_t sourceEdgeIndex = resultWireProducerNpos;
    int sourceEndpoint = -1;
};

std::optional<LedgerVertexReplacementCandidate> ledgerVertexReplacementCandidate(
    const TopoDS_Vertex& vertex,
    const std::vector<TopoDS_Edge>& ledgerEdges
)
{
    if (vertex.IsNull()) {
        return std::nullopt;
    }

    const gp_Pnt point = BRep_Tool::Pnt(vertex);
    std::optional<LedgerVertexReplacementCandidate> bestCandidate;
    double bestSquareDistance = std::numeric_limits<double>::infinity();
    for (std::size_t edgeIndex = 0; edgeIndex < ledgerEdges.size(); ++edgeIndex) {
        const TopoDS_Edge& ledgerEdge = ledgerEdges[edgeIndex];
        if (ledgerEdge.IsNull()) {
            continue;
        }
        const std::array<TopoDS_Vertex, 2> ledgerVertices {
            TopExp::FirstVertex(ledgerEdge),
            TopExp::LastVertex(ledgerEdge),
        };
        for (std::size_t endpoint = 0; endpoint < ledgerVertices.size(); ++endpoint) {
            const TopoDS_Vertex& ledgerVertex = ledgerVertices[endpoint];
            if (ledgerVertex.IsNull()) {
                continue;
            }
            if (vertex.IsSame(ledgerVertex)) {
                return LedgerVertexReplacementCandidate {
                    ledgerEdge,
                    ledgerVertex,
                    edgeIndex,
                    static_cast<int>(endpoint),
                };
            }
            const double squareDistance = point.SquareDistance(BRep_Tool::Pnt(ledgerVertex));
            const double tolerance = std::max(
                {Precision::Confusion(), BRep_Tool::Tolerance(vertex), BRep_Tool::Tolerance(ledgerVertex)}
            );
            if (squareDistance > tolerance * tolerance || squareDistance >= bestSquareDistance) {
                continue;
            }
            bestSquareDistance = squareDistance;
            bestCandidate = LedgerVertexReplacementCandidate {
                ledgerEdge,
                ledgerVertex,
                edgeIndex,
                static_cast<int>(endpoint),
            };
        }
    }
    return bestCandidate;
}

TopoDS_Edge edgeWithLedgerVertexReplacements(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& ledgerEdges,
    std::vector<WireJoinerVmapReplacementEvent>* replacementEvents = nullptr,
    std::size_t affectedSourceEdgeIndex = resultWireProducerNpos,
    std::size_t affectedChildWireEdgeInfoIndex = resultWireProducerNpos,
    bool replacementFromMutableSourceEdgeLedger = true,
    bool replacementFromSplitFragmentLedger = false
)
{
    if (edge.IsNull() || ledgerEdges.empty()) {
        return edge;
    }

    const TopoDS_Vertex originalFirst = TopExp::FirstVertex(edge);
    const TopoDS_Vertex originalLast = TopExp::LastVertex(edge);
    TopoDS_Vertex firstVertex = originalFirst;
    TopoDS_Vertex lastVertex = originalLast;
    bool firstReplaced = false;
    bool lastReplaced = false;
    std::array<std::optional<LedgerVertexReplacementCandidate>, 2> replacementCandidates;
    auto replacementVertex =
        [&](const TopoDS_Vertex& endpoint,
            std::size_t endpointIndex) -> std::optional<TopoDS_Vertex> {
        if (endpoint.IsNull()) {
            return std::nullopt;
        }
        const std::optional<LedgerVertexReplacementCandidate> candidate =
            ledgerVertexReplacementCandidate(endpoint, ledgerEdges);
        if (!candidate || candidate->vertex.IsNull() || candidate->edge.IsNull()
            || endpoint.IsSame(candidate->vertex)) {
            return std::nullopt;
        }

        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same
        // TopoDS_Vertex", adjusts vertex tolerance, then uses the mutable vmap/sourceEdges ledger to
        // keep coincident endpoints as shared TopoDS_Vertex instances. Rebuild the producer curve
        // with the selected ledger vertices in one step so both endpoints are validated together.
        const double tolerance = std::max(
            BRep_Tool::Pnt(endpoint).Distance(BRep_Tool::Pnt(candidate->vertex)),
            BRep_Tool::Tolerance(candidate->vertex)
        );
        if (tolerance >= BRep_Tool::Tolerance(endpoint)) {
            ShapeFix_ShapeTolerance fix;
            fix.SetTolerance(endpoint, std::max(tolerance * 0.5, Precision::Confusion()), TopAbs_VERTEX);
        }
        replacementCandidates[endpointIndex] = candidate;
        return candidate->vertex;
    };

    if (const std::optional<TopoDS_Vertex> replacement = replacementVertex(originalFirst, 0U)) {
        firstVertex = *replacement;
        firstReplaced = true;
    }
    if (const std::optional<TopoDS_Vertex> replacement = replacementVertex(originalLast, 1U)) {
        lastVertex = *replacement;
        lastReplaced = true;
    }
    if (!firstReplaced && !lastReplaced) {
        return edge;
    }

    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    TopoDS_Edge result;
    if (!curve.IsNull()) {
        BRepBuilderAPI_MakeEdge builder(curve, firstVertex, lastVertex, first, last);
        if (builder.IsDone() && !builder.Edge().IsNull()) {
            result = builder.Edge();
        }
    }
    if (result.IsNull()) {
        BRepBuilderAPI_MakeEdge fallback(firstVertex, lastVertex);
        if (fallback.IsDone() && !fallback.Edge().IsNull()) {
            result = fallback.Edge();
        }
    }
    if (result.IsNull()) {
        return edge;
    }
    const auto resultContainsVertex = [&](const TopoDS_Vertex& vertex) {
        const TopoDS_Vertex resultFirst = TopExp::FirstVertex(result);
        const TopoDS_Vertex resultLast = TopExp::LastVertex(result);
        return (!resultFirst.IsNull() && resultFirst.IsSame(vertex))
            || (!resultLast.IsNull() && resultLast.IsSame(vertex));
    };
    if ((firstReplaced && !resultContainsVertex(firstVertex))
        || (lastReplaced && !resultContainsVertex(lastVertex))) {
        return edge;
    }
    if (replacementEvents != nullptr) {
        const std::array<TopoDS_Vertex, 2> originalVertices {originalFirst, originalLast};
        for (std::size_t endpoint = 0; endpoint < replacementCandidates.size(); ++endpoint) {
            if (!replacementCandidates[endpoint]) {
                continue;
            }
            WireJoinerVmapReplacementEvent event;
            event.oldVertex = originalVertices[endpoint];
            event.newSharedVertex = replacementCandidates[endpoint]->vertex;
            event.affectedSourceEdgeIndex = affectedSourceEdgeIndex;
            event.affectedChildWireEdgeInfoIndex = affectedChildWireEdgeInfoIndex;
            event.affectedEndpoint = static_cast<int>(endpoint);
            event.affectedSourceEndpoint = affectedSourceEdgeIndex == resultWireProducerNpos
                ? -1
                : static_cast<int>(endpoint);
            event.affectedChildWireEndpoint =
                affectedChildWireEdgeInfoIndex == resultWireProducerNpos
                ? -1
                : static_cast<int>(endpoint);
            event.replacementSourceEdgeIndex =
                replacementCandidates[endpoint]->sourceEdgeIndex;
            event.replacementSourceEndpoint =
                replacementCandidates[endpoint]->sourceEndpoint;
            event.replacementFromMutableSourceEdgeLedger =
                replacementFromMutableSourceEdgeLedger;
            event.replacementFromSplitFragmentLedger = replacementFromSplitFragmentLedger;
            replacementEvents->push_back(std::move(event));
        }
    }
    return result;
}

bool edgeSharesOriginalSourceVertexByIdentity(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }

    return std::any_of(vertices.begin(), vertices.end(), [&](const TopoDS_Vertex& vertex) {
        return vertexIsOriginalSourceByIdentity(vertex, sourceEdges);
    });
}

bool edgeUsesOnlyOriginalSourceVerticesByIdentity(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }

    return std::all_of(vertices.begin(), vertices.end(), [&](const TopoDS_Vertex& vertex) {
        return vertexIsOriginalSourceByIdentity(vertex, sourceEdges);
    });
}

bool allEdgesShareOriginalSourceVertexByIdentity(
    const TopoDS_Wire& wire,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    if (sourceEdges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& edge : wireEdges(wire)) {
        if (!edgeUsesOnlyOriginalSourceVerticesByIdentity(edge, sourceEdges)) {
            return false;
        }
    }
    return true;
}

std::vector<TopoDS_Edge> uniqueEdgesForShape(const TopoDS_Shape& shape)
{
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);

    std::vector<TopoDS_Edge> result;
    result.reserve(static_cast<std::size_t>(edges.Extent()));
    for (int index = 1; index <= edges.Extent(); ++index) {
        result.push_back(TopoDS::Edge(edges(index)));
    }
    return result;
}

bool pointOnOpenEdge(const gp_Pnt& point, const std::vector<TopoDS_Edge>& openEdges)
{
    if (openEdges.empty()) {
        return false;
    }
    const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    for (const TopoDS_Edge& edge : openEdges) {
        BRepExtrema_DistShapeShape distance(vertex, edge);
        distance.Perform();
        if (distance.IsDone() && distance.Value() <= Precision::Confusion()) {
            return true;
        }
    }
    return false;
}

TopoDS_Vertex cachedCopiedVertex(
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices,
    const gp_Pnt& point
)
{
    for (const auto& [existingPoint, vertex] : copiedVertices) {
        if (samePoint(existingPoint, point)) {
            return vertex;
        }
    }
    TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    copiedVertices.emplace_back(point, vertex);
    return vertex;
}

bool pointMatchesAny(const gp_Pnt& point, const std::vector<gp_Pnt>& candidates)
{
    return std::any_of(candidates.begin(), candidates.end(), [&](const gp_Pnt& candidate) {
        return samePoint(point, candidate);
    });
}

std::vector<gp_Pnt> wireVertexPoints(const std::vector<TopoDS_Wire>& wires)
{
    std::vector<gp_Pnt> points;
    for (const TopoDS_Wire& wire : wires) {
        for (TopExp_Explorer explorer(wire, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
            const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
            if (!pointMatchesAny(point, points)) {
                points.push_back(point);
            }
        }
    }
    return points;
}

bool closedWireEdgesAreLinear(const std::vector<TopoDS_Wire>& wires)
{
    for (const TopoDS_Wire& wire : wires) {
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
            if (curve.GetType() != GeomAbs_Line) {
                return false;
            }
        }
    }
    return !wires.empty();
}

TopoDS_Vertex resultWireVertex(
    const TopoDS_Vertex& sourceVertex,
    const std::vector<TopoDS_Edge>& openEdges,
    bool copyAllVertices,
    const std::vector<gp_Pnt>& reusableVertexPoints,
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices
)
{
    const gp_Pnt point = BRep_Tool::Pnt(sourceVertex);
    if (pointMatchesAny(point, reusableVertexPoints)) {
        return sourceVertex;
    }
    if (copyAllVertices || pointOnOpenEdge(point, openEdges)) {
        return cachedCopiedVertex(copiedVertices, point);
    }
    return sourceVertex;
}

TopoDS_Edge copyEdgeWithResultWireVertices(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& openEdges,
    bool copyAllVertices,
    const std::vector<gp_Pnt>& reusableVertexPoints,
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices
)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    const TopoDS_Vertex start = resultWireVertex(
        TopExp::FirstVertex(edge),
        openEdges,
        copyAllVertices,
        reusableVertexPoints,
        copiedVertices
    );
    const TopoDS_Vertex end = resultWireVertex(
        TopExp::LastVertex(edge),
        openEdges,
        copyAllVertices,
        reusableVertexPoints,
        copiedVertices
    );
    if (!curve.IsNull()) {
        const Handle(Geom_Curve) copiedCurve = Handle(Geom_Curve)::DownCast(curve->Copy());
        BRepBuilderAPI_MakeEdge
            builder(copiedCurve.IsNull() ? curve : copiedCurve, start, end, first, last);
        if (builder.IsDone() && !builder.Edge().IsNull()) {
            return builder.Edge();
        }
    }
    BRepBuilderAPI_MakeEdge fallback(start, end);
    return fallback.Edge();
}

bool allOpenEdgeEndpointsTouchBoundary(
    const std::vector<TopoDS_Edge>& openEdges,
    const TopoDS_Shape& faceShape
)
{
    if (openEdges.empty() || faceShape.IsNull()) {
        return false;
    }
    const std::vector<TopoDS_Edge> faceBoundaryEdges = boundaryEdges(faceShape);
    if (faceBoundaryEdges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& edge : openEdges) {
        const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
        if (vertices.empty()) {
            return false;
        }
        for (const TopoDS_Vertex& vertex : vertices) {
            bool touches = false;
            for (const TopoDS_Edge& boundary : faceBoundaryEdges) {
                if (vertexTouchesBoundary(vertex, boundary)) {
                    touches = true;
                    break;
                }
            }
            if (!touches) {
                return false;
            }
        }
    }
    return true;
}

bool allOpenEdgeEndpointsTouchClosedWireBoundary(
    const std::vector<TopoDS_Edge>& openEdges,
    const std::vector<TopoDS_Wire>& closedWires
)
{
    if (openEdges.empty() || closedWires.empty()) {
        return false;
    }

    std::vector<TopoDS_Edge> boundaryEdges;
    for (const TopoDS_Wire& wire : closedWires) {
        const std::vector<TopoDS_Edge> edges = wireEdges(wire);
        boundaryEdges.insert(boundaryEdges.end(), edges.begin(), edges.end());
    }
    if (boundaryEdges.empty()) {
        return false;
    }

    for (const TopoDS_Edge& edge : openEdges) {
        const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
        if (vertices.empty()) {
            return false;
        }
        for (const TopoDS_Vertex& vertex : vertices) {
            bool touches = false;
            for (const TopoDS_Edge& boundary : boundaryEdges) {
                if (vertexTouchesBoundary(vertex, boundary)) {
                    touches = true;
                    break;
                }
            }
            if (!touches) {
                return false;
            }
        }
    }
    return true;
}

bool edgeEndpointsTouchBoundary(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& boundaryEdges)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }
    for (const TopoDS_Vertex& vertex : vertices) {
        bool touches = false;
        for (const TopoDS_Edge& boundary : boundaryEdges) {
            if (vertexTouchesBoundary(vertex, boundary)) {
                touches = true;
                break;
            }
        }
        if (!touches) {
            return false;
        }
    }
    return true;
}

bool pointOnEdge(const gp_Pnt& point, const TopoDS_Edge& edge)
{
    if (edge.IsNull()) {
        return false;
    }
    const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    BRepExtrema_DistShapeShape distance(vertex, edge);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= Precision::Confusion();
}

bool edgeSamplesLieOnEdge(const TopoDS_Edge& edge, const TopoDS_Edge& source)
{
    if (edge.IsNull() || source.IsNull()) {
        return false;
    }
    const auto [start, end] = edgeEndpoints(edge);
    return pointOnEdge(start, source) && pointOnEdge(edgeMidpoint(edge), source)
        && pointOnEdge(end, source);
}

bool edgeLiesOnAnyEdge(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& sources)
{
    for (const TopoDS_Edge& source : sources) {
        if (edgeSamplesLieOnEdge(edge, source)) {
            return true;
        }
    }
    return false;
}

std::vector<TopoDS_Edge> closedWireBoundaryEdges(const std::vector<TopoDS_Wire>& closedWires)
{
    std::vector<TopoDS_Edge> edges;
    for (const TopoDS_Wire& wire : closedWires) {
        const std::vector<TopoDS_Edge> wireBoundary = wireEdges(wire);
        edges.insert(edges.end(), wireBoundary.begin(), wireBoundary.end());
    }
    return edges;
}

std::vector<TopoDS_Edge> edgesContainingEdge(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sources
)
{
    std::vector<TopoDS_Edge> matches;
    for (const TopoDS_Edge& source : sources) {
        if (edgeSamplesLieOnEdge(edge, source)) {
            matches.push_back(source);
        }
    }
    return matches;
}

bool edgeEquivalentByGeometryAndEndpoints(const TopoDS_Edge& left, const TopoDS_Edge& right)
{
    return edgeMatchesSourceVertices(left, right) && edgeSamplesLieOnEdge(left, right)
        && edgeSamplesLieOnEdge(right, left);
}

std::optional<TopoDS_Vertex> vertexAtPoint(const std::vector<TopoDS_Vertex>& vertices, const gp_Pnt& point)
{
    const auto it = std::find_if(vertices.begin(), vertices.end(), [&](const TopoDS_Vertex& vertex) {
        return !vertex.IsNull() && samePoint(BRep_Tool::Pnt(vertex), point);
    });
    if (it == vertices.end()) {
        return std::nullopt;
    }
    return *it;
}

TopoDS_Edge edgeWithReusedVertices(
    const TopoDS_Edge& edge,
    const TopoDS_Vertex& firstVertex,
    const TopoDS_Vertex& lastVertex
)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (!curve.IsNull()) {
        BRepBuilderAPI_MakeEdge builder(curve, firstVertex, lastVertex, first, last);
        if (builder.IsDone() && !builder.Edge().IsNull()) {
            return builder.Edge();
        }
    }
    BRepBuilderAPI_MakeEdge fallback(firstVertex, lastVertex);
    return fallback.Edge();
}

std::optional<TopoDS_Edge> edgeWithEquivalentResultVertices(
    const TopoDS_Edge& producerShape,
    const TopoDS_Edge& resultEdge
)
{
    if (producerShape.IsNull() || resultEdge.IsNull()
        || !edgeEquivalentByGeometryAndEndpoints(producerShape, resultEdge)) {
        return std::nullopt;
    }

    const std::vector<TopoDS_Vertex> resultVertices = edgeVertices(resultEdge);
    const auto [firstPoint, lastPoint] = edgeEndpoints(producerShape);
    const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(resultVertices, firstPoint);
    const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(resultVertices, lastPoint);
    if (!firstVertex || !lastVertex) {
        return std::nullopt;
    }

    TopoDS_Edge outputEdge = edgeWithReusedVertices(producerShape, *firstVertex, *lastVertex);
    if (outputEdge.IsNull()) {
        return std::nullopt;
    }
    return outputEdge;
}

std::optional<TopoDS_Edge> edgeSubsegmentWithReusedVertices(
    const TopoDS_Edge& producerShape,
    const TopoDS_Vertex& firstVertex,
    const TopoDS_Vertex& lastVertex
)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(producerShape, first, last);
    if (curve.IsNull()) {
        return std::nullopt;
    }

    GeomAPI_ProjectPointOnCurve firstProjection(BRep_Tool::Pnt(firstVertex), curve, first, last);
    GeomAPI_ProjectPointOnCurve lastProjection(BRep_Tool::Pnt(lastVertex), curve, first, last);
    if (firstProjection.NbPoints() == 0 || lastProjection.NbPoints() == 0) {
        return std::nullopt;
    }
    const Standard_Real firstParameter = firstProjection.LowerDistanceParameter();
    const Standard_Real lastParameter = lastProjection.LowerDistanceParameter();
    if (std::abs(firstParameter - lastParameter) <= Precision::PConfusion()) {
        return std::nullopt;
    }

    BRepBuilderAPI_MakeEdge builder(curve, firstVertex, lastVertex, firstParameter, lastParameter);
    if (builder.IsDone() && !builder.Edge().IsNull()) {
        return builder.Edge();
    }
    return std::nullopt;
}

std::optional<TopoDS_Edge> edgeWithProducerCurveAndResultVertices(
    const TopoDS_Edge& producerShape,
    const TopoDS_Edge& resultEdge
)
{
    if (const std::optional<TopoDS_Edge> equivalent
        = edgeWithEquivalentResultVertices(producerShape, resultEdge)) {
        return equivalent;
    }
    if (producerShape.IsNull() || resultEdge.IsNull()
        || !edgeSamplesLieOnEdge(resultEdge, producerShape)) {
        return std::nullopt;
    }
    const std::vector<TopoDS_Vertex> resultVertices = edgeVertices(resultEdge);
    const auto [firstPoint, lastPoint] = edgeEndpoints(resultEdge);
    const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(resultVertices, firstPoint);
    const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(resultVertices, lastPoint);
    if (!firstVertex || !lastVertex) {
        return std::nullopt;
    }
    return edgeSubsegmentWithReusedVertices(producerShape, *firstVertex, *lastVertex);
}

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same TopoDS_Vertex";
// ::build() exports "info.wire()" after that vertex replacement. Rebuild a producer subsegment
// only from child-wire endpoint vertex identity, not from an output-side result edge shape.
std::optional<TopoDS_Edge> edgeWithProducerCurveAndResultVertices(
    const TopoDS_Edge& producerShape,
    const std::vector<TopoDS_Vertex>& resultVertices
)
{
    if (producerShape.IsNull() || resultVertices.size() < 2U) {
        return std::nullopt;
    }
    const TopoDS_Vertex& firstVertex = resultVertices.front();
    const TopoDS_Vertex& lastVertex = resultVertices.back();
    if (firstVertex.IsNull() || lastVertex.IsNull()) {
        return std::nullopt;
    }
    if (!pointOnEdge(BRep_Tool::Pnt(firstVertex), producerShape)
        || !pointOnEdge(BRep_Tool::Pnt(lastVertex), producerShape)) {
        return std::nullopt;
    }
    return edgeSubsegmentWithReusedVertices(producerShape, firstVertex, lastVertex);
}

TopoDS_Wire currentMemberWireFromRootSuperEdge(
    const TopoDS_Wire& rootWire,
    const TopoDS_Wire& targetWire,
    const TopoDS_Edge& fallbackMemberEdge,
    const std::vector<TopoDS_Vertex>& ledgerVertices
)
{
    if (rootWire.IsNull()) {
        return TopoDS_Wire();
    }

    std::vector<TopoDS_Edge> targetEdges = wireEdges(targetWire);
    if (!fallbackMemberEdge.IsNull()) {
        const bool fallbackAlreadyCovered
            = std::any_of(targetEdges.begin(), targetEdges.end(), [&](const TopoDS_Edge& targetEdge) {
                  return edgeEquivalentByGeometryAndEndpoints(targetEdge, fallbackMemberEdge);
              });
        if (!fallbackAlreadyCovered) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::findSuperEdgesUpdateFirst() stores the real member edge in the root
            // "superEdge", while cad-core's legacy locator may carry a result-slot edge.
            // Try the real member edge as a producer-match candidate, but still require the final
            // output edge to be rebuilt from request-local child-wire ledger vertices below.
            targetEdges.push_back(fallbackMemberEdge);
        }
    }
    if (targetEdges.empty()) {
        return TopoDS_Wire();
    }

    for (const TopoDS_Edge& rootEdge : wireEdges(rootWire)) {
        for (const TopoDS_Edge& targetEdge : targetEdges) {
            if (edgeEquivalentByGeometryAndEndpoints(rootEdge, targetEdge)) {
                const auto [firstPoint, lastPoint] = edgeEndpoints(rootEdge);
                const std::optional<TopoDS_Vertex> firstVertex
                    = vertexAtPoint(ledgerVertices, firstPoint);
                const std::optional<TopoDS_Vertex> lastVertex
                    = vertexAtPoint(ledgerVertices, lastPoint);
                if (!firstVertex || !lastVertex) {
                    continue;
                }
                const TopoDS_Edge outputEdge
                    = edgeWithReusedVertices(rootEdge, *firstVertex, *lastVertex);
                BRepBuilderAPI_MakeWire builder(outputEdge);
                if (builder.IsDone()) {
                    return builder.Wire();
                }
                continue;
            }
            if (!edgeSamplesLieOnEdge(targetEdge, rootEdge)) {
                continue;
            }
            const auto [firstPoint, lastPoint] = edgeEndpoints(targetEdge);
            const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(ledgerVertices, firstPoint);
            const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(ledgerVertices, lastPoint);
            if (!firstVertex || !lastVertex) {
                continue;
            }
            const std::optional<TopoDS_Edge> outputEdge
                = edgeSubsegmentWithReusedVertices(rootEdge, *firstVertex, *lastVertex);
            if (!outputEdge || outputEdge->IsNull()) {
                continue;
            }
            BRepBuilderAPI_MakeWire builder(*outputEdge);
            if (builder.IsDone()) {
                return builder.Wire();
            }
        }
    }
    if (!fallbackMemberEdge.IsNull()) {
        for (const TopoDS_Edge& targetEdge : targetEdges) {
            if (!edgeEquivalentByGeometryAndEndpoints(fallbackMemberEdge, targetEdge)
                && !edgeSamplesLieOnEdge(targetEdge, fallbackMemberEdge)) {
                continue;
            }
            const auto [firstPoint, lastPoint] = edgeEndpoints(targetEdge);
            const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(ledgerVertices, firstPoint);
            const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(ledgerVertices, lastPoint);
            if (!firstVertex || !lastVertex) {
                continue;
            }
            const std::optional<TopoDS_Edge> outputEdge
                = edgeEquivalentByGeometryAndEndpoints(fallbackMemberEdge, targetEdge)
                ? std::optional<TopoDS_Edge>(
                      edgeWithReusedVertices(fallbackMemberEdge, *firstVertex, *lastVertex)
                  )
                : edgeSubsegmentWithReusedVertices(fallbackMemberEdge, *firstVertex, *lastVertex);
            if (!outputEdge || outputEdge->IsNull()) {
                continue;
            }
            BRepBuilderAPI_MakeWire builder(*outputEdge);
            if (builder.IsDone()) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() feeds each current member through
                // "wireData->Add(current->shape(...))" before ShapeFix_Wire creates the root
                // "first->superEdge". If cad-core cannot match the clean root edge back to this
                // child target, the member EdgeInfo::edge is still the request-local producer, but
                // it may only replace the result-wire candidate child after both target endpoints resolve to
                // ledger vertices.
                return builder.Wire();
            }
        }
    }
    return TopoDS_Wire();
}

bool edgeMatchesSourceSharedVertexSearch(
    const TopoDS_Edge& edge,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    for (const TopoDS_Edge& sourceEdge : sourceEdges) {
        if (edgeEquivalentByGeometryAndEndpoints(edge, sourceEdge)) {
            return true;
        }
    }
    return false;
}

bool allEdgesMatchSourceSharedVertexSearch(
    const TopoDS_Wire& wire,
    const std::vector<TopoDS_Edge>& sourceEdges
)
{
    if (sourceEdges.empty()) {
        return false;
    }
    const std::vector<TopoDS_Edge> edges = wireEdges(wire);
    if (edges.empty()) {
        return false;
    }
    return std::all_of(edges.begin(), edges.end(), [&](const TopoDS_Edge& edge) {
        return edgeMatchesSourceSharedVertexSearch(edge, sourceEdges);
    });
}

bool edgesShareEndpointByPoint(const TopoDS_Edge& lhs, const TopoDS_Edge& rhs)
{
    if (lhs.IsNull() || rhs.IsNull()) {
        return false;
    }
    const auto [lhsFirst, lhsLast] = edgeEndpoints(lhs);
    const auto [rhsFirst, rhsLast] = edgeEndpoints(rhs);
    return samePoint(lhsFirst, rhsFirst) || samePoint(lhsFirst, rhsLast)
        || samePoint(lhsLast, rhsFirst) || samePoint(lhsLast, rhsLast);
}

bool vertexMatchesAnyByIdentity(const TopoDS_Vertex& vertex, const std::vector<TopoDS_Vertex>& candidates)
{
    return std::any_of(candidates.begin(), candidates.end(), [&](const TopoDS_Vertex& candidate) {
        return vertex.IsSame(candidate);
    });
}

void appendUniqueVertexByIdentity(std::vector<TopoDS_Vertex>& vertices, const TopoDS_Vertex& vertex)
{
    if (vertex.IsNull()) {
        return;
    }
    if (!vertexMatchesAnyByIdentity(vertex, vertices)) {
        vertices.push_back(vertex);
    }
}

std::vector<TopoDS_Vertex> uniqueVerticesByIdentity(const std::vector<TopoDS_Vertex>& vertices)
{
    std::vector<TopoDS_Vertex> uniqueVertices;
    uniqueVertices.reserve(vertices.size());
    for (const TopoDS_Vertex& vertex : vertices) {
        appendUniqueVertexByIdentity(uniqueVertices, vertex);
    }
    return uniqueVertices;
}

std::vector<TopoDS_Vertex> edgeEndpointVertices(const std::vector<TopoDS_Edge>& edges)
{
    std::vector<TopoDS_Vertex> vertices;
    vertices.reserve(edges.size() * 2U);
    for (const TopoDS_Edge& edge : edges) {
        if (edge.IsNull()) {
            continue;
        }
        const TopoDS_Vertex first = TopExp::FirstVertex(edge);
        const TopoDS_Vertex last = TopExp::LastVertex(edge);
        if (!first.IsNull()) {
            vertices.push_back(first);
        }
        if (!last.IsNull()) {
            vertices.push_back(last);
        }
    }
    return vertices;
}

int countEquivalentEdges(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& candidates)
{
    int count = 0;
    for (const TopoDS_Edge& candidate : candidates) {
        if (edgeEquivalentByGeometryAndEndpoints(edge, candidate)) {
            ++count;
        }
    }
    return count;
}

std::vector<TopoDS_Edge> openEdgesWithInteriorEndpoint(
    const std::vector<TopoDS_Edge>& openEdges,
    const std::vector<TopoDS_Edge>& closedBoundaryEdges
)
{
    std::vector<TopoDS_Edge> edges;
    for (const TopoDS_Edge& edge : openEdges) {
        if (!edgeEndpointsTouchBoundary(edge, closedBoundaryEdges)) {
            edges.push_back(edge);
        }
    }
    return edges;
}


InternalShapeHistoryRelation internalShapeHistoryRelationForWireJoiner(
    WireJoinerHistoryRelation relation
)
{
    switch (relation) {
        case WireJoinerHistoryRelation::Preserved:
            return InternalShapeHistoryRelation::Preserved;
        case WireJoinerHistoryRelation::Split:
            return InternalShapeHistoryRelation::Split;
        case WireJoinerHistoryRelation::Generated:
            return InternalShapeHistoryRelation::Generated;
        case WireJoinerHistoryRelation::Deleted:
            return InternalShapeHistoryRelation::Deleted;
    }
    return InternalShapeHistoryRelation::DiagnosticOnly;
}

std::vector<std::size_t> oneBasedSourceEdgeIndices(const std::vector<std::size_t>& zeroBased)
{
    std::vector<std::size_t> result;
    result.reserve(zeroBased.size());
    for (const std::size_t index : zeroBased) {
        result.push_back(index + 1U);
    }
    return result;
}

InternalShapeHistoryLedger wireJoinerHistoryEvidenceLedger(const WireJoinerHistorySummary& history)
{
    InternalShapeHistoryLedger ledger;
    InternalShapeHistoryLedgerData& data = mutableInternalShapeHistoryLedgerData(ledger);
    data.hasWireJoinerEvidence = true;
    SketchInternalHistoryContext& context = data.compatibilityHistory;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), calls makeShapeWithElementMap(...,
    // MapperHistory(aHistory), {sourceEdges.begin(), sourceEdges.end()}, op). This passes
    // only WireJoiner-produced history summary into topo; topo must not infer WireJoiner
    // split/generated/deleted history from raw/internal geometry.
    context.wireJoinerSourceEdgeCount = history.sourceEdgeCount;
    context.wireJoinerSplitResultEdgeCount = history.splitResultEdgeCount;
    for (const WireJoinerHistoryEvent& event : history.historyEvents) {
        SketchInternalWireJoinerHistoryEvent topoEvent;
        topoEvent.eventIndex = event.eventIndex;
        topoEvent.openExportIndex = event.openExportIndex;
        topoEvent.edgeInfoIndex = event.edgeInfoIndex;
        topoEvent.openWireCompoundChildWireInfoIndex = event.openWireCompoundChildWireInfoIndex;
        topoEvent.relation = wireJoinerHistoryRelationName(event.relation);
        topoEvent.relationFromChildWireLedger = event.relationFromChildWireLedger;
        topoEvent.sourceEdgeIndices = event.sourceEdgeIndices;
        topoEvent.sourceLineageFromSplitterHistory = event.sourceLineageFromSplitterHistory;
        topoEvent.noOriginalPurgedByLedger = event.noOriginalPurgedByLedger;
        topoEvent.splitFragmentFromModifiedHistory = event.splitFragmentFromModifiedHistory;
        topoEvent.splitFragmentFromGeneratedHistory = event.splitFragmentFromGeneratedHistory;
        context.wireJoinerHistoryEvents.push_back(std::move(topoEvent));
    }
    for (const WireJoinerOpenExportHistoryEntry& entry :
         history.openExportEntries) {
        SketchInternalWireJoinerOpenExportHistoryEntry topoEntry;
        topoEntry.openExportIndex = entry.openExportIndex;
        topoEntry.edgeInfoIndex = entry.edgeInfoIndex;
        topoEntry.openExportWire = entry.openExportWire;
        topoEntry.openExportEdge = entry.openExportEdge;
        topoEntry.wireJoinerHistoryRelation = entry.historyRelationFromChildWireLedger
            ? wireJoinerHistoryRelationName(entry.historyRelation)
            : std::string();
        topoEntry.wireJoinerHistoryRelationFromChildWireLedger
            = entry.historyRelationFromChildWireLedger;
        topoEntry.wireJoinerHistoryEventIndex = entry.wireJoinerHistoryEventIndex;
        topoEntry.wireJoinerHistoryEventFromChildWireLedger
            = entry.wireJoinerHistoryEventFromChildWireLedger;
        topoEntry.resultWireProducerKind = resultWireProducerKindName(
            entry.resultWireProducer.kind
        );
        topoEntry.resultWireProducerState = resultWireProducerStateName(
            entry.resultWireProducer.state
        );
        topoEntry.resultWireProducerBlocker = resultWireBlockerName(
            entry.resultWireProducer.blocker
        );
        topoEntry.resultWireProducerSourceEdgeInfoIndex
            = entry.resultWireProducer.sourceEdgeInfoIndex;
        topoEntry.resultWireProducerRootEdgeInfoIndex = entry.resultWireProducer.rootEdgeInfoIndex;
        topoEntry.resultWireProducerCurrentMemberEdgeInfoIndex
            = entry.resultWireProducer.currentMemberEdgeInfoIndex;
        topoEntry.resultWireProducerChildWireInfoIndex = entry.resultWireProducer.childWireInfoIndex;
        topoEntry.openWireCompoundChildWireInfoIndex = entry.openWireCompoundChildWireInfoIndex;
        topoEntry.openWireCompoundExportSource = openWireCompoundExportSourceName(
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
        for (const WireJoinerVmapReplacementEvent& event :
             entry.openWireCompoundVmapReplacementEvents) {
            SketchInternalWireJoinerVmapReplacementEvent topoEvent;
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
        for (const WireJoinerEndpointIdentityDebt& debt :
             entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt) {
            SketchInternalWireJoinerEndpointIdentityDebt topoDebt;
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
        const std::vector<std::size_t> stableSourceEdgeIndices = oneBasedSourceEdgeIndices(
            !entry.openWireCompoundSourceEdgeIndices.empty()
                ? entry.openWireCompoundSourceEdgeIndices
                : entry.sourceEdgeIndices
        );
        std::string diagnosticCode;
        if (entry.openWireCompoundNoOriginalPurgedByLedger) {
            diagnosticCode = "no_original_purged";
        }
        else if (entry.missingOpenWireCompoundChildWire) {
            diagnosticCode = "missing_child_wire_invariant";
        }
        else if (entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked) {
            diagnosticCode = "vertex_multiplicity_blocked";
        }
        data.events.push_back(InternalShapeHistoryEvent {
            entry.historyRelationFromChildWireLedger
                ? internalShapeHistoryRelationForWireJoiner(entry.historyRelation)
                : InternalShapeHistoryRelation::DiagnosticOnly,
            InternalShapeHistoryProducer::WireJoinerOpenWires,
            InternalShapeHistoryTargetKind::Edge,
            "wire_joiner:open_export",
            std::move(diagnosticCode),
            stableSourceEdgeIndices,
            entry.openExportEdge,
        });
        context.wireJoinerOpenExportHistoryEntries.push_back(std::move(topoEntry));
    }
    context.wireJoinerModifiedSourceEdgeCount = history.modifiedSourceEdgeCount;
    context.wireJoinerModifiedHistoryCount = history.modifiedHistoryCount;
    context.wireJoinerGeneratedHistoryCount = history.generatedHistoryCount;
    context.wireJoinerDeletedHistoryCount = history.deletedHistoryCount;
    context.wireJoinerSplitterHistory = history.splitterHistory;
    return ledger;
}

nlohmann::json resultWireProducerLedgerEntriesJson(
    const std::vector<ResultWireProducerLedgerEntry>& entries
)
{
    nlohmann::json result = nlohmann::json::array();
    for (const ResultWireProducerLedgerEntry& entry : entries) {
        result.push_back({
            {"open_export_index", entry.openExportIndex},
            {"source_edge_info_index", entry.sourceEdgeInfoIndex},
            {"root_edge_info_index", entry.rootEdgeInfoIndex},
            {"current_member_edge_info_index", entry.currentMemberEdgeInfoIndex},
            {"child_wire_info_index", entry.childWireInfoIndex},
            {"kind", resultWireProducerKindName(entry.kind)},
            {"state", resultWireProducerStateName(entry.state)},
            {"blocker", resultWireBlockerName(entry.blocker)},
            {"open_wire_compound_export_source",
             openWireCompoundExportSourceName(entry.openWireCompoundExportSource)},
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

nlohmann::json wireJoinerLedgerToJson(const WireJoinerLedgerSummary& ledger)
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

nlohmann::json wireJoinerHistoryDetailToJson(const WireJoinerHistorySummary& history)
{
    nlohmann::json wireJoinerHistoryEvents = nlohmann::json::array();
    for (const WireJoinerHistoryEvent& event : history.historyEvents) {
        wireJoinerHistoryEvents.push_back({
            {"event_index", event.eventIndex},
            {"open_export_index", event.openExportIndex},
            {"edge_info_index", event.edgeInfoIndex},
            {"open_wire_compound_child_wire_info_index", event.openWireCompoundChildWireInfoIndex},
            {"relation", wireJoinerHistoryRelationName(event.relation)},
            {"relation_from_child_wire_ledger", event.relationFromChildWireLedger},
            {"source_edge_indices", event.sourceEdgeIndices},
            {"source_lineage_from_splitter_history", event.sourceLineageFromSplitterHistory},
            {"no_original_purged_by_ledger", event.noOriginalPurgedByLedger},
            {"split_fragment_from_modified_history", event.splitFragmentFromModifiedHistory},
            {"split_fragment_from_generated_history", event.splitFragmentFromGeneratedHistory},
        });
    }

    nlohmann::json openExportHistoryEntries = nlohmann::json::array();
    for (const WireJoinerOpenExportHistoryEntry& entry : history.openExportEntries) {
        nlohmann::json currentMemberSplitOutputVertexDebt = nlohmann::json::array();
        for (const WireJoinerEndpointIdentityDebt& debt :
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
        for (const WireJoinerVmapReplacementEvent& event :
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
             openWireCompoundExportSourceName(entry.openWireCompoundExportSource)},
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
                 ? wireJoinerHistoryRelationName(entry.historyRelation)
                 : ""},
            {"wire_joiner_history_relation_from_child_wire_ledger",
             entry.historyRelationFromChildWireLedger},
            {"wire_joiner_history_event_index", entry.wireJoinerHistoryEventIndex},
            {"wire_joiner_history_event_from_child_wire_ledger",
             entry.wireJoinerHistoryEventFromChildWireLedger},
            {"result_wire_producer_kind",
             resultWireProducerKindName(entry.resultWireProducer.kind)},
            {"result_wire_producer_state",
             resultWireProducerStateName(entry.resultWireProducer.state)},
            {"result_wire_producer_blocker",
             resultWireBlockerName(entry.resultWireProducer.blocker)},
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

}  // namespace

void WireJoiner::Impl::setTightBound(bool enabled)
{
    tightBound_ = enabled;
}

void WireJoiner::Impl::setMergeEdges(bool enabled)
{
    mergeEdges_ = enabled;
}

void WireJoiner::Impl::addOpenWire(
    const TopoDS_Wire& wire,
    const std::vector<std::size_t>& sourceEdgeIndices
)
{
    if (!wire.IsNull()) {
        WireInfo info;
        info.id = nextWireInfoId_++;
        info.wire = wire;
        const std::vector<TopoDS_Edge> edges = wireEdges(wire);
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const TopoDS_Edge& edge = edges[edgeIndex];
            EdgeInfo edgeInfo;
            initializeEdgeInfo(edgeInfo, edge);
            if (edgeIndex < sourceEdgeIndices.size()) {
                // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build(), "sourceEdges.insert(sourceEdgeArray.begin(),
                // sourceEdgeArray.end())", then ::splitEdges() records
                // "aHistory->AddModified(split.intersectShape, newInfo.edge)". Sketcher already knows
                // the sourceEdgeArray slot for each wire edge, so carry it into EdgeInfo before
                // splitting instead of rediscovering it from copied edge geometry.
                edgeInfo.sourceEdgeIndices.push_back(sourceEdgeIndices[edgeIndex]);
            }
            info.edges.push_back(edgeInfo);
        }
        openWires_.push_back(std::move(info));
    }
}

void WireJoiner::Impl::addSourceEdge(const TopoDS_Edge& edge)
{
    if (!edge.IsNull()) {
        const std::size_t sourceEdgeIndex = sourceEdges_.size();
        std::vector<WireJoinerVmapReplacementEvent> replacementEvents;
        sourceEdges_.push_back(edge);
        TopoDS_Edge ledgerEdge = edgeWithLedgerVertexReplacements(
            edge,
            sourceEdgeLedgerEdges_,
            &replacementEvents,
            sourceEdgeIndex
        );
        if (ledgerEdge.IsNull()) {
            ledgerEdge = edge;
        }
        for (WireJoinerVmapReplacementEvent& event : replacementEvents) {
            event.eventIndex = sourceEdgeLedgerReplacementEvents_.size();
            sourceEdgeLedgerReplacementEvents_.push_back(std::move(event));
        }
        sourceEdgeLedgerEdges_.push_back(ledgerEdge);
    }
}

std::size_t WireJoiner::Impl::closedWireCycleSplitLedgerSourceEdgeCount(
    const WireInfo& info,
    const std::vector<TopoDS_Wire>& closedWires
) const
{
    if (closedWires.size() < 3U) {
        return 0;
    }

    std::map<std::size_t, std::size_t> modifiedFragmentCountBySourceEdge;
    for (const EdgeInfo& edgeInfo : info.edges) {
        if (!edgeInfo.splitFragmentFromModifiedHistory) {
            continue;
        }
        for (const std::size_t sourceIndex : edgeInfo.splitFragmentModifiedSourceEdgeIndices) {
            ++modifiedFragmentCountBySourceEdge[sourceIndex];
        }
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), "sourceEdges.insert(sourceEdgeArray.begin(), sourceEdgeArray.end())",
    // then ::splitEdges() records "aHistory->AddModified(split.intersectShape, newInfo.edge)"
    // before ::buildClosedWire(). This mirrors the old closed-cycle generated-export precheck from
    // the EdgeInfo split ledger instead of sampling bounded-face result edges.
    return static_cast<std::size_t>(std::count_if(
        modifiedFragmentCountBySourceEdge.begin(),
        modifiedFragmentCountBySourceEdge.end(),
        [](const auto& item) { return item.second >= 2U; }
    ));
}

WireJoiner::Impl::WireJoinerHistoryMaterializationLedger WireJoiner::Impl::computeWireJoinerHistoryMaterializationLedger(
    const WireInfo& info,
    const TopoDS_Shape& boundedFaceShape,
    const std::vector<TopoDS_Wire>& closedWires,
    const std::vector<TopoDS_Edge>& openEdges,
    bool splitProducedBoundedFaces,
    bool hasOpenWireOutput
) const
{
    WireJoinerHistoryMaterializationLedger materializationLedger;
    materializationLedger.closedWireCycleSplitLedgerSourceEdgeCount =
        closedWireCycleSplitLedgerSourceEdgeCount(info, closedWires);
    if (openEdges.empty() && closedWires.size() >= 2U) {
        const std::vector<TopoDS_Edge> closedBoundaryEdges = closedWireBoundaryEdges(closedWires);
        std::vector<TopoDS_Edge> finalEdges;
        finalEdges.reserve(info.edges.size());
        for (const EdgeInfo& edgeInfo : info.edges) {
            if (!edgeInfo.edge.IsNull()) {
                finalEdges.push_back(edgeInfo.edge);
            }
        }
        if (!closedBoundaryEdges.empty() && !finalEdges.empty()) {
            for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
                const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
                if (edgeInfo.edge.IsNull() || edgeInfo.sourceEdgeIndices.size() < 2U) {
                    continue;
                }
                const std::vector<TopoDS_Edge> containingSources
                    = edgesContainingEdge(edgeInfo.edge, closedBoundaryEdges);
                if (containingSources.size() < 2U) {
                    continue;
                }
                const bool partialOverlap = std::any_of(
                    containingSources.begin(),
                    containingSources.end(),
                    [&](const TopoDS_Edge& source) {
                        return !edgeEquivalentByGeometryAndEndpoints(edgeInfo.edge, source);
                    }
                );
                if (!partialOverlap || countEquivalentEdges(edgeInfo.edge, finalEdges) > 1) {
                    continue;
                }

                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::buildClosedWire() removes partial shared closed-wire result edges
                // through the real EdgeInfo/aHistory lifecycle; the P6 path binds these slots from
                // EdgeInfo source lineage instead of asking the legacy bounded-face locator to
                // rediscover the same result edge. Bridge deletion condition: WireInfo/wireInfo2
                // exhaust lifecycle plus myShapesToReturn must identify the surviving child wire
                // without a materialized child-slot bridge or result-slot endpoint materialization evidence.
                ++materializationLedger.candidateEdgeCount;
                WireJoinerHistoryMaterializationBinding binding;
                binding.resultSlotEdge = edgeInfo.edge;
                binding.partialSharedClosedWireProducer = true;
                binding.edgeInfoIndex = edgeInfoIndex;
                materializationLedger.bindings.push_back(std::move(binding));
            }
        }
        if (materializationLedger.candidateEdgeCount > 0U) {
            materializationLedger.needed = true;
            return materializationLedger;
        }
    }

    const bool consumedOpenCutterGraph = splitProducedBoundedFaces && !hasOpenWireOutput
        && openEdges.size() >= 2U && allOpenEdgeEndpointsTouchBoundary(openEdges, boundedFaceShape);
    if (consumedOpenCutterGraph) {
        const bool partialJunctionOpenCutter
            = !allOpenEdgeEndpointsTouchClosedWireBoundary(openEdges, closedWires);
        const std::vector<TopoDS_Edge> boundedEdges = uniqueEdgesForShape(boundedFaceShape);
        const std::vector<TopoDS_Edge> closedBoundaryEdges = partialJunctionOpenCutter
            ? closedWireBoundaryEdges(closedWires)
            : std::vector<TopoDS_Edge> {};
        const std::vector<TopoDS_Edge> partialOpenEdges = partialJunctionOpenCutter
            ? openEdgesWithInteriorEndpoint(openEdges, closedBoundaryEdges)
            : std::vector<TopoDS_Edge> {};
        std::vector<TopoDS_Edge> resultSlotSeeds;
        std::vector<std::pair<gp_Pnt, TopoDS_Vertex>> copiedVertices;
        for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
            const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
            if (edgeInfo.edge.IsNull() || countEquivalentEdges(edgeInfo.edge, boundedEdges) == 0) {
                continue;
            }
            if (partialJunctionOpenCutter
                && (!edgeLiesOnAnyEdge(edgeInfo.edge, closedBoundaryEdges)
                    && !edgeLiesOnAnyEdge(edgeInfo.edge, partialOpenEdges))) {
                continue;
            }
            if (countEquivalentEdges(edgeInfo.edge, resultSlotSeeds) > 0) {
                continue;
            }

            TopoDS_Edge resultSlotEdge
                = copyEdgeWithResultWireVertices(edgeInfo.edge, openEdges, false, {}, copiedVertices);
            if (resultSlotEdge.IsNull()) {
                continue;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports consumed-open and junction result wires from final
            // EdgeInfo states in openWireCompound. This P6 binding identifies the legacy slot from
            // those final EdgeInfo rows; resultSlotEdge is only request-local vertex evidence for
            // root/current-member producers, not the producer identity source.
            ++materializationLedger.candidateEdgeCount;
            resultSlotSeeds.push_back(edgeInfo.edge);
            WireJoinerHistoryMaterializationBinding binding;
            binding.resultSlotEdge = resultSlotEdge;
            binding.edgeInfoIndex = edgeInfoIndex;
            materializationLedger.bindings.push_back(std::move(binding));
        }
        materializationLedger.needed = materializationLedger.candidateEdgeCount > 0U;
        return materializationLedger;
    }

    const bool closedWireCycleExport = openEdges.empty() && !boundedFaceShape.IsNull()
        && materializationLedger.closedWireCycleSplitLedgerSourceEdgeCount >= 3U;
    materializationLedger.closedWireCycleSplitLedgerOpenExport = closedWireCycleExport;
    if (closedWireCycleExport) {
        const std::vector<TopoDS_Edge> boundedEdges = uniqueEdgesForShape(boundedFaceShape);
        const std::vector<gp_Pnt> reusableVertexPoints = closedWireEdgesAreLinear(closedWires)
            ? wireVertexPoints(closedWires)
            : std::vector<gp_Pnt> {};
        std::vector<TopoDS_Edge> resultSlotSeeds;
        std::vector<std::pair<gp_Pnt, TopoDS_Vertex>> copiedVertices;
        for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
            const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
            if (edgeInfo.edge.IsNull() || countEquivalentEdges(edgeInfo.edge, boundedEdges) == 0) {
                continue;
            }
            if (countEquivalentEdges(edgeInfo.edge, resultSlotSeeds) > 0) {
                continue;
            }

            TopoDS_Edge resultSlotEdge = copyEdgeWithResultWireVertices(
                edgeInfo.edge,
                openEdges,
                true,
                reusableVertexPoints,
                copiedVertices
            );
            if (resultSlotEdge.IsNull()) {
                continue;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::splitEdges() records source-to-fragment history before
            // ::buildClosedWire() and ::build() export closed-cycle result wires from final
            // EdgeInfo states. This P6 path binds closed-cycle slots from those EdgeInfo rows
            // instead of the legacy bounded-face result-slot finder. Bridge deletion condition:
            // aHistory plus openWireCompound child-wire ownership must produce the result edge
            // identity without a materialized child-slot bridge or result-slot endpoint materialization evidence.
            ++materializationLedger.candidateEdgeCount;
            resultSlotSeeds.push_back(edgeInfo.edge);
            WireJoinerHistoryMaterializationBinding binding;
            binding.resultSlotEdge = resultSlotEdge;
            binding.edgeInfoIndex = edgeInfoIndex;
            materializationLedger.bindings.push_back(std::move(binding));
        }
    }

    materializationLedger.needed = materializationLedger.candidateEdgeCount > 0U;
    return materializationLedger;
}

bool WireJoiner::Impl::wireJoinerHistoryMaterializationLedgerHasUnsafeProducer(
    const WireInfo& info,
    const WireJoinerHistoryMaterializationLedger& materializationLedger
) const
{
    if (!materializationLedger.needed) {
        return false;
    }
    if (materializationLedger.unboundEdgeCount > 0U) {
        return true;
    }

    for (const WireJoinerHistoryMaterializationBinding& binding : materializationLedger.bindings) {
        bool hasAHistoryProducerCandidate = false;
        if (binding.edgeInfoIndex < info.edges.size()) {
            const EdgeInfo& candidate = info.edges[binding.edgeInfoIndex];
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::splitEdges() records "aHistory->AddModified(..., newInfo.edge)" and
            // ::buildClosedWire() later calls "aHistory->Remove(info.edge)". The final EdgeInfo row
            // bound by this materialization entry is safe enough for the rerun gate only when it
            // carries request-local source lineage plus Remove-source evidence. A removed target can
            // also be safe when it records the actual outer EdgeInfo passed to Remove() and that
            // source carries splitter/source lineage.
            if (resultWireProducerSlotHasSafeAHistoryEvidence(candidate)) {
                hasAHistoryProducerCandidate = true;
            }
        }
        if (!hasAHistoryProducerCandidate) {
            return true;
        }
    }
    return false;
}

bool WireJoiner::Impl::edgeInfoExportsOpenWireCompound(const EdgeInfo& edgeInfo) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), exports an EdgeInfo into "openWireCompound" only when
    // "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    return edgeInfo.iteration == -3 || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
}

bool WireJoiner::Impl::edgeInfoHasOpenWireCompoundLedgerSlot(
    const EdgeInfo& edgeInfo,
    bool materializedChildSlot
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() only emits final EdgeInfo states through openWireCompound. While
    // cad-core still discovers some of those final children through result-wire producer bindings,
    // keep a materialized child-wire ledger slot for that producer but do not let
    // result-slot endpoint materialization evidence become emitted geometry.
    return edgeInfoExportsOpenWireCompound(edgeInfo) || materializedChildSlot;
}

bool WireJoiner::Impl::wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() exports from final EdgeInfo rows, and ::getOpenWires() consumes
    // MapperHistory(aHistory). A cad-core materialized child candidate is therefore derived from
    // the final EdgeInfo row bound by the request-local materialization ledger, not copied into a
    // separate per-edge candidate flag.
    return std::any_of(
        materializationLedger.bindings.begin(),
        materializationLedger.bindings.end(),
        [edgeInfoIndex](const WireJoinerHistoryMaterializationBinding& binding) {
            return binding.edgeInfoIndex == edgeInfoIndex && !binding.resultSlotEdge.IsNull();
        }
    );
}

bool WireJoiner::Impl::resultWireProducerSlotHasFullAHistoryEvidence(
    const EdgeInfo& edgeInfo
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() marks the removed target and separately records
    // "aHistory->Remove(info.edge)" on the outer EdgeInfo source. Full M3 producer evidence
    // requires both sides plus request-local sourceEdgeArray lineage.
    return edgeInfo.buildClosedWireAHistoryRemoved && edgeInfo.buildClosedWireRemoved
        && !edgeInfo.sourceEdgeIndices.empty()
        && !edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices.empty();
}

bool WireJoiner::Impl::resultWireProducerRootHasFullAHistoryEvidence(
    const EdgeInfo& edgeInfo
) const
{
    if (resultWireProducerSlotHasFullAHistoryEvidence(edgeInfo)) {
        return true;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() primary/secondary removal sets
    // "vertex.edgeInfo()->iteration = -1" on the removed target but records
    // "aHistory->Remove(info.edge)" on the outer EdgeInfo source. A superEdge root result-wire
    // producer is complete when it has the removed target, the actual Remove source, and same-source
    // request-local lineage; foreign Remove lineage remains risk evidence for another producer.
    return edgeInfo.buildClosedWireRemoved && !edgeInfo.sourceEdgeIndices.empty()
        && !edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices.empty()
        && sourceEdgeIndicesIntersect(
               edgeInfo.sourceEdgeIndices,
               edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices
        );
}

bool WireJoiner::Impl::resultWireProducerRootCanSuppressPendingMember(
    const EdgeInfo& edgeInfo
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets each member "current->iteration = -1",
    // while ::WireJoinerP::buildClosedWire() later records consumed unowned/primary/secondary
    // members with "aHistory->Remove(info.edge)". M3/P4 can suppress a non-current member only when
    // that member is already proven to be owned by another result child; edge-level evidence covers
    // buildClosedWire branch ownership, while live source-edge child ownership is checked from the
    // openWireCompound ledger at the call site.
    return (edgeInfo.buildClosedWireRemovedByUnowned || edgeInfo.buildClosedWireRemovedByPrimaryOwner
            || edgeInfo.buildClosedWireRemovedBySecondaryOwner)
        && resultWireProducerRootHasFullAHistoryEvidence(edgeInfo);
}

bool WireJoiner::Impl::resultWireProducerSlotHasSafeAHistoryEvidence(
    const EdgeInfo& edgeInfo
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() separates the removed target
    // "vertex.edgeInfo()->iteration = -1" from the aHistory producer
    // "aHistory->Remove(info.edge)". A result-wire producer binding is safe for M3's rerun gate
    // only when the selected EdgeInfo is itself that Remove source with request-local source
    // lineage, or the recorded Remove source belongs to the same sourceEdgeArray lineage. Foreign
    // Remove lineage is producer evidence for another source, not a safe producer for this selected
    // EdgeInfo.
    if (edgeInfo.buildClosedWireAHistoryRemoved && !edgeInfo.sourceEdgeIndices.empty()) {
        return true;
    }
    return !edgeInfo.sourceEdgeIndices.empty()
        && sourceEdgeIndicesIntersect(
            edgeInfo.sourceEdgeIndices,
            edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices
        );
}

void WireJoiner::Impl::updateOpenWireCompoundNoOriginalPurgeVerdict(
    OpenWireCompoundWireInfo& childWire
) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(noOriginal=true) builds "source" from "sourceEdgeArray" and
    // purges a child only if every edge has a shared source vertex:
    // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1)).empty()".
    const std::vector<TopoDS_Edge> childEdges = wireEdges(childWire.wire);
    childWire.noOriginalSharedSourceEdgeLedgerRecorded = !sourceEdges_.empty();
    childWire.noOriginalSharedSourceEdgeCount = childEdges.size();
    childWire.noOriginalSharedSourceMatchedEdgeCount = !sourceEdges_.empty()
        ? static_cast<std::size_t>(std::count_if(
              childEdges.begin(),
              childEdges.end(),
              [&](const TopoDS_Edge& edge) {
                  return edgeMatchesSourceSharedVertexSearch(edge, sourceEdges_);
              }
          ))
        : 0U;
    childWire.noOriginalSharedSourceUnmatchedEdgeCount =
        childWire.noOriginalSharedSourceEdgeCount
        - childWire.noOriginalSharedSourceMatchedEdgeCount;
    childWire.sourceSharedVertexPurgeMatch =
        childWire.noOriginalSharedSourceEdgeLedgerRecorded
        && childWire.noOriginalSharedSourceMatchedEdgeCount
            == childWire.noOriginalSharedSourceEdgeCount;
    childWire.noOriginalPurgeMatch = childWire.sourceSharedVertexPurgeMatch;
    childWire.noOriginalPurgedByLedger = false;
}

void WireJoiner::Impl::updateOpenWireCompoundNoOriginalGroupPurgeVerdicts(WireInfo& info) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(noOriginal=true) gets
    // "TopoShape(openWireCompound, -1).getSubTopoShapes(TopAbs_WIRE)", then erases a wire only if
    // every edge in that wire is found by
    // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))". cad-core materializes some
    // FreeCAD child wires as multiple EdgeInfo child slots, so compute the purge verdict after
    // regrouping those slots by endpoint connectivity.
    for (OpenWireCompoundWireInfo& childWire : info.openWireCompoundWires) {
        childWire.noOriginalPurgedByLedger = false;
    }
    if (sourceEdges_.empty()) {
        return;
    }

    struct ChildWireEdgeRef
    {
        TopoDS_Edge edge;
        std::size_t childWireIndex = 0;
    };

    std::vector<ChildWireEdgeRef> edgeRefs;
    for (std::size_t childWireIndex = 0; childWireIndex < info.openWireCompoundWires.size();
         ++childWireIndex) {
        OpenWireCompoundWireInfo& childWire = info.openWireCompoundWires[childWireIndex];
        if (childWire.wire.IsNull()) {
            continue;
        }
        if (childWire.superEdgeWire) {
            childWire.noOriginalPurgedByLedger =
                allEdgesMatchSourceSharedVertexSearch(childWire.wire, sourceEdges_);
            continue;
        }
        for (const TopoDS_Edge& edge : wireEdges(childWire.wire)) {
            if (!edge.IsNull()) {
                edgeRefs.push_back(ChildWireEdgeRef {edge, childWireIndex});
            }
        }
    }

    std::vector<bool> used(edgeRefs.size(), false);
    for (std::size_t startIndex = 0; startIndex < edgeRefs.size(); ++startIndex) {
        if (used[startIndex]) {
            continue;
        }

        std::vector<std::size_t> groupRefIndices;
        std::deque<std::size_t> pending;
        used[startIndex] = true;
        pending.push_back(startIndex);
        while (!pending.empty()) {
            const std::size_t currentIndex = pending.front();
            pending.pop_front();
            groupRefIndices.push_back(currentIndex);
            for (std::size_t candidateIndex = 0; candidateIndex < edgeRefs.size();
                 ++candidateIndex) {
                if (used[candidateIndex]) {
                    continue;
                }
                if (!edgesShareEndpointByPoint(edgeRefs[currentIndex].edge, edgeRefs[candidateIndex].edge)) {
                    continue;
                }
                used[candidateIndex] = true;
                pending.push_back(candidateIndex);
            }
        }

        const bool groupPurgedByNoOriginal = !groupRefIndices.empty()
            && std::all_of(groupRefIndices.begin(), groupRefIndices.end(), [&](std::size_t refIndex) {
                   return edgeMatchesSourceSharedVertexSearch(edgeRefs[refIndex].edge, sourceEdges_);
               });
        if (!groupPurgedByNoOriginal) {
            continue;
        }
        for (std::size_t refIndex : groupRefIndices) {
            info.openWireCompoundWires[edgeRefs[refIndex].childWireIndex].noOriginalPurgedByLedger =
                true;
        }
    }
}

bool WireJoiner::Impl::openWireCompoundChildWirePurgedByNoOriginal(
    const OpenWireCompoundWireInfo& childWire,
    bool noOriginal
) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(noOriginal=true) removes matching original source wires after
    // openWireCompound has been materialized and grouped by final WIRE. The actual skip consumes the
    // child-wire ledger's group-level deletion verdict, not a source/split candidate bridge.
    return noOriginal && childWire.noOriginalPurgedByLedger;
}

std::optional<std::size_t> WireJoiner::Impl::superEdgeRootIndexForMember(
    const WireInfo& info,
    const EdgeInfo& edgeInfo
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() sets each non-root member to
    // "current->iteration = -1" and materializes the root with
    // "first->superEdge = makeCleanWire(false)". This maps a result-wire-selected member back to its
    // same superEdgeInfo root without changing the live openWireCompound export path.
    if (edgeInfo.superEdgeInfo == 0U || !edgeInfo.superEdgeLifecycleMemberMinusOne) {
        return std::nullopt;
    }
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        const EdgeInfo& candidate = info.edges[edgeIndex];
        if (candidate.superEdgeInfo == edgeInfo.superEdgeInfo && candidate.superEdgeRoot) {
            return edgeIndex;
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> WireJoiner::Impl::strictRemovedSourceEdgeInfoIndicesForSourceLineage(
    const WireInfo& info,
    const EdgeInfo& edgeInfo
) const
{
    std::vector<std::size_t> indices;
    if (edgeInfo.sourceEdgeIndices.empty()) {
        return indices;
    }

    for (std::size_t candidateIndex = 0; candidateIndex < info.edges.size(); ++candidateIndex) {
        const EdgeInfo& candidate = info.edges[candidateIndex];
        if (!candidate.buildClosedWireAHistoryRemoved || candidate.sourceEdgeIndices.empty()) {
            continue;
        }
        if (sourceEdgeIndicesIntersect(edgeInfo.sourceEdgeIndices, candidate.sourceEdgeIndices)) {
            appendUniqueSourceIndex(indices, candidateIndex);
        }
    }
    return indices;
}

const TopoDS_Edge* WireJoiner::Impl::wireJoinerMapperHistoryProducerEvidenceEdge(
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    const auto evidenceIt = std::find_if(
        materializationLedger.mapperHistoryProducerEvidence.begin(),
        materializationLedger.mapperHistoryProducerEvidence.end(),
        [edgeInfoIndex](const WireJoinerMapperHistoryProducerEvidence& evidence) {
            return evidence.edgeInfoIndex == edgeInfoIndex && !evidence.producerShape.IsNull();
        }
    );
    if (evidenceIt == materializationLedger.mapperHistoryProducerEvidence.end()) {
        return nullptr;
    }
    return &evidenceIt->producerShape;
}

bool WireJoiner::Impl::wireJoinerMapperHistoryProducerEvidenceReady(
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    return wireJoinerMapperHistoryProducerEvidenceEdge(
        materializationLedger,
        edgeInfoIndex
    ) != nullptr;
}

void WireJoiner::Impl::recordWireJoinerMapperHistoryProducerEvidence(
    WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex,
    const TopoDS_Edge& producerShape
) const
{
    if (producerShape.IsNull()) {
        return;
    }
    const auto evidenceIt = std::find_if(
        materializationLedger.mapperHistoryProducerEvidence.begin(),
        materializationLedger.mapperHistoryProducerEvidence.end(),
        [edgeInfoIndex](const WireJoinerMapperHistoryProducerEvidence& evidence) {
            return evidence.edgeInfoIndex == edgeInfoIndex;
        }
    );
    if (evidenceIt != materializationLedger.mapperHistoryProducerEvidence.end()) {
        evidenceIt->producerShape = producerShape;
        return;
    }
    WireJoinerMapperHistoryProducerEvidence evidence;
    evidence.edgeInfoIndex = edgeInfoIndex;
    evidence.producerShape = producerShape;
    materializationLedger.mapperHistoryProducerEvidence.push_back(std::move(evidence));
}

bool WireJoiner::Impl::wireJoinerMapperHistoryProducerEvidenceHasChildWire(
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() writes "aHistory->Remove(info.edge)", while
    // ::WireJoinerP::build() later publishes final children through
    // "builder.Add(openWireCompound, info.wire())". cad-core proves this child-wire producer slot
    // through the request-local MapperHistory producer evidence ledger so source/vmap fallback
    // cannot over-claim result slots outside the aHistory/openWireCompound lifecycle.
    return wireJoinerMapperHistoryProducerEvidenceReady(
        materializationLedger,
        edgeInfoIndex
    );
}

const TopoDS_Edge& WireJoiner::Impl::resultWireProducerMapperHistoryInputEdge(
    const EdgeInfo& edgeInfo,
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    if (wireJoinerMapperHistoryProducerEvidenceReady(
            materializationLedger,
            edgeInfoIndex
        )) {
        return *wireJoinerMapperHistoryProducerEvidenceEdge(
            materializationLedger,
            edgeInfoIndex
        );
    }
    return edgeInfo.edge;
}

std::optional<std::size_t> WireJoiner::Impl::producerLedgerReadyAHistoryRemoveProducerIndex(
    const WireInfo& info,
    const EdgeInfo& edgeInfo,
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    const TopoDS_Edge* resultEdge
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records the actual result-wire producer with
    // "aHistory->Remove(info.edge)". When that Remove source already has a producer-ledger export
    // edge, cad-core can use it as producer only if it matches the legacy result-slot geometry.
    for (const std::size_t sourceIndex :
         edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices) {
        if (sourceIndex >= info.edges.size()) {
            continue;
        }
        const EdgeInfo& producer = info.edges[sourceIndex];
        if (!wireJoinerMapperHistoryProducerEvidenceReady(
                materializationLedger,
                sourceIndex
            )) {
            continue;
        }
        if (resultEdge) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::buildClosedWire() records the actual producer with
            // "aHistory->Remove(info.edge)". The producer-ledger open export is preferred when it
            // already matches, but a strict Remove producer may still be represented by the
            // underlying EdgeInfo::edge curve and the legacy result-slot vertices.
            const bool openExportMatches
                = edgeEquivalentByGeometryAndEndpoints(
                      resultWireProducerMapperHistoryInputEdge(producer, materializationLedger, sourceIndex),
                      *resultEdge
                  )
                || edgeSamplesLieOnEdge(
                    *resultEdge,
                    resultWireProducerMapperHistoryInputEdge(producer, materializationLedger, sourceIndex)
                );
            const bool producerShapeMatches
                = edgeEquivalentByGeometryAndEndpoints(producer.edge, *resultEdge)
                || edgeSamplesLieOnEdge(*resultEdge, producer.edge);
            if (!openExportMatches && !producerShapeMatches) {
                continue;
            }
        }
        return sourceIndex;
    }
    return std::nullopt;
}

std::optional<std::size_t> WireJoiner::Impl::producerLedgerReadySameSourceSidecarProducerIndex(
    const WireInfo& info,
    const EdgeInfo& edgeInfo,
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    const TopoDS_Edge* resultEdge
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() records producer evidence with
    // "aHistory->Remove(info.edge)". If another strict Remove source in the same sourceEdgeArray
    // lineage already has producer-ledger output, it can provide the producer curve for this legacy
    // result slot only when that curve either matches or contains the result-slot edge.
    for (const std::size_t sourceIndex :
         strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo)) {
        if (sourceIndex >= info.edges.size()) {
            continue;
        }
        const EdgeInfo& producer = info.edges[sourceIndex];
        if (!wireJoinerMapperHistoryProducerEvidenceReady(
                materializationLedger,
                sourceIndex
            )) {
            continue;
        }
        if (resultEdge) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // Same-source sidecars are still strict Remove producers from
            // "aHistory->Remove(info.edge)"; if their already-exported producer shape does not
            // contain this result, the original producer shape can still be the valid curve source.
            const bool openExportMatches
                = edgeEquivalentByGeometryAndEndpoints(
                      resultWireProducerMapperHistoryInputEdge(producer, materializationLedger, sourceIndex),
                      *resultEdge
                  )
                || edgeSamplesLieOnEdge(
                    *resultEdge,
                    resultWireProducerMapperHistoryInputEdge(producer, materializationLedger, sourceIndex)
                );
            const bool producerShapeMatches
                = edgeEquivalentByGeometryAndEndpoints(producer.edge, *resultEdge)
                || edgeSamplesLieOnEdge(*resultEdge, producer.edge);
            if (!openExportMatches && !producerShapeMatches) {
                continue;
            }
        }
        return sourceIndex;
    }
    return std::nullopt;
}

ResultWireProducerIdentity WireJoiner::Impl::classifyResultWireProducerSlot(
    const WireInfo& info,
    std::size_t edgeInfoIndex,
    const WireJoinerHistoryMaterializationLedger& materializationLedger
) const
{
    ResultWireProducerIdentity identity;
    if (edgeInfoIndex >= info.edges.size()) {
        identity.blocker = ResultWireBlocker::UnknownInvariant;
        return identity;
    }

    const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
    const WireJoinerHistoryMaterializationEdgeEntry emptyMaterializationEntry;
    const WireJoinerHistoryMaterializationEdgeEntry& materializationEntry =
        edgeInfoIndex < materializationLedger.edgeEntries.size()
        ? materializationLedger.edgeEntries[edgeInfoIndex]
        : emptyMaterializationEntry;
    if (!wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
            materializationLedger,
            edgeInfoIndex
        )) {
        return identity;
    }

    identity.sourceEdgeInfoIndex = edgeInfoIndex;
    identity.rootEdgeInfoIndex = materializationEntry.superEdgeRoot
        ? materializationEntry.superEdgeRootIndex
        : resultWireProducerNpos;
    identity.currentMemberEdgeInfoIndex
        = materializationEntry.superEdgeRootCurrentMember
        ? edgeInfoIndex
        : resultWireProducerNpos;
    const bool hasSourceLineage = !edgeInfo.sourceEdgeIndices.empty();
    const bool hasStrictRemoveSource = edgeInfo.buildClosedWireAHistoryRemoved;
    const bool hasRemovedTarget = edgeInfo.buildClosedWireRemoved;
    const bool sourceExportsOpenEdge = edgeInfoExportsOpenWireCompound(edgeInfo);
    const bool sourceConsumedByBuildClosedWire = edgeInfo.buildClosedWireRemoved
        || edgeInfo.buildClosedWireAHistoryRemoved;
    const std::vector<std::size_t> aHistoryRemoveSourceEdgeInfoIndices =
        edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices;
    const bool hasAHistoryRemoveSourceEdgeInfo =
        !aHistoryRemoveSourceEdgeInfoIndices.empty();
    const bool hasAHistoryRemoveSourceLineage =
        !edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices.empty();
    const bool hasAHistoryRemoveSameSourceLineage = sourceEdgeIndicesIntersect(
        edgeInfo.sourceEdgeIndices,
        edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices
    );
    const bool hasAHistoryRemoveForeignSourceLineage =
        hasAHistoryRemoveSourceLineage && !hasAHistoryRemoveSameSourceLineage;
    const std::vector<std::size_t> sourceLineageRemovedSourceEdgeInfoIndices =
        strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo);
    const bool hasFullAHistoryEvidence = resultWireProducerSlotHasFullAHistoryEvidence(edgeInfo);
    const bool partialSharedClosedWireProducer = std::any_of(
        materializationLedger.bindings.begin(),
        materializationLedger.bindings.end(),
        [edgeInfoIndex](const WireJoinerHistoryMaterializationBinding& binding) {
            return binding.partialSharedClosedWireProducer
                && binding.edgeInfoIndex == edgeInfoIndex;
        }
    );
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire() calls "aHistory->Remove(info.edge)" from the outer
    // EdgeInfo source while marking removed targets separately through "vertex.edgeInfo()".
    // P5 accepts a same-source strict Remove sidecar as producer evidence availability, but does
    // not relabel this target EdgeInfo itself as the strict Remove source.
    const bool hasSameSourceStrictRemoveSourceEvidence
        = !sourceLineageRemovedSourceEdgeInfoIndices.empty();
    const bool hasRootFullAHistoryProducerEvidence
        = materializationEntry.superEdgeRootProducerFullAHistoryEvidence;
    const bool hasAHistoryRemoveSourceEvidence = hasAHistoryRemoveSourceEdgeInfo
        || hasStrictRemoveSource || hasSameSourceStrictRemoveSourceEvidence
        || hasRootFullAHistoryProducerEvidence;
    const bool hasFullAHistoryProducerEvidence = hasFullAHistoryEvidence
        || hasRootFullAHistoryProducerEvidence
        || (hasRemovedTarget && hasSourceLineage && hasSameSourceStrictRemoveSourceEvidence);
    const bool isLiveFinalGateOpenEdgeProducer = sourceExportsOpenEdge
        && !sourceConsumedByBuildClosedWire;
    const bool isRootOpenCurrentMemberProducer = materializationEntry.superEdgeMember
        && materializationEntry.superEdgeRoot
        && materializationEntry.superEdgeRootOpenLifecycle
        && materializationEntry.superEdgeRootOpenWireCompoundEligible;

    if (materializationEntry.superEdgeRootProducerCandidate) {
        identity.kind = ResultWireProducerKind::SuperEdgeRoot;
    }
    else if (isRootOpenCurrentMemberProducer) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() marks every member with
        // "current->iteration = -1" while keeping the open root as "first->superEdge".
        // That root can be emitted by ::build() without an aHistory Remove event; the member
        // result-wire candidate slot is therefore blocked by current-member child-wire identity, not missing
        // buildClosedWire() Remove-source/removed-target evidence.
        identity.kind = ResultWireProducerKind::CurrentMemberChildWire;
        identity.currentMemberEdgeInfoIndex = edgeInfoIndex;
    }
    else if (partialSharedClosedWireProducer) {
        identity.kind = ResultWireProducerKind::PartialSharedClosedWire;
    }
    else if (isLiveFinalGateOpenEdgeProducer) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() adds live open EdgeInfo wires to openWireCompound through
        // "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)". These result-wire
        // producers are not buildClosedWire() Remove-source events, so MissingAHistoryRemoveSource
        // is the wrong blocker once the final export gate and request-local source lineage exist.
        identity.kind = ResultWireProducerKind::LiveResetOpenEdge;
    }
    else {
        identity.kind = ResultWireProducerKind::ExistingSourceEdge;
    }

    if (identity.kind != ResultWireProducerKind::None) {
        identity.state = ResultWireProducerState::ProducerLocated;
    }

    if (identity.kind == ResultWireProducerKind::None) {
        identity.blocker = hasSourceLineage ? ResultWireBlocker::UnknownInvariant
                                            : ResultWireBlocker::MissingSourceLineage;
        return identity;
    }
    if (!hasSourceLineage) {
        identity.blocker = ResultWireBlocker::MissingSourceLineage;
        return identity;
    }
    if (isRootOpenCurrentMemberProducer) {
        identity.state = ResultWireProducerState::ChildWireReady;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) erases a wire when every edge is found by
        // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))". Root-open current-member
        // producers must keep that purge gate separate from the remaining child-wire vertex
        // identity blocker, because both are part of the same source-shape readiness check.
        const TopoDS_Edge& rootOpenCurrentMemberPurgeCandidate =
            resultWireProducerMapperHistoryInputEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        const bool rootOpenCurrentMemberWouldBePurgedAsOriginal = !sourceEdges_.empty()
            && edgeMatchesSourceSharedVertexSearch(
                rootOpenCurrentMemberPurgeCandidate.IsNull() ? edgeInfo.edge
                                                             : rootOpenCurrentMemberPurgeCandidate,
                sourceEdges_
            );
        if (rootOpenCurrentMemberWouldBePurgedAsOriginal) {
            identity.blocker = ResultWireBlocker::CurrentMemberSourceShapeWouldPurgeOriginal;
        }
        else if (
            !materializationEntry.superEdgeRootCurrentMember
        ) {
            const std::vector<std::size_t> sameSourceStrictRemoveSourceIndices
                = strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo);
            std::optional<std::size_t> sameSourceProducerLedgerSidecarIndex;
            for (const std::size_t sourceIndex : sameSourceStrictRemoveSourceIndices) {
                if (sourceIndex >= info.edges.size()) {
                    continue;
                }
                if (wireJoinerMapperHistoryProducerEvidenceReady(
                        materializationLedger,
                        sourceIndex
                    )) {
                    sameSourceProducerLedgerSidecarIndex = sourceIndex;
                    break;
                }
            }
            const TopoDS_Edge& resultEdge =
                resultWireProducerMapperHistoryInputEdge(edgeInfo, materializationLedger, edgeInfoIndex);
            const std::optional<std::size_t> sameSourceSidecarProducerLedgerReadyIndex
                = producerLedgerReadySameSourceSidecarProducerIndex(
                    info,
                    edgeInfo,
                    materializationLedger,
                    &resultEdge
                );
            if (sameSourceProducerLedgerSidecarIndex) {
                identity.sourceEdgeInfoIndex = *sameSourceProducerLedgerSidecarIndex;
            }
            else if (!sameSourceStrictRemoveSourceIndices.empty()) {
                identity.sourceEdgeInfoIndex = sameSourceStrictRemoveSourceIndices.front();
            }
            if (sameSourceStrictRemoveSourceIndices.empty()) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() records the producer as the open root
                // "first->superEdge"; ::build() exports that root via openWireCompound without a
                // buildClosedWire() sidecar. Do not report this as missing sidecar evidence.
                identity.blocker = ResultWireBlocker::CurrentMemberRootOpenProducerNotReady;
            }
            else {
                identity.blocker = (!sameSourceSidecarProducerLedgerReadyIndex
                                    && sameSourceProducerLedgerSidecarIndex)
                    ? ResultWireBlocker::CurrentMemberSidecarGeometryMismatch
                    : ResultWireBlocker::CurrentMemberChildWireIdentityNotReady;
            }
        }
        else {
            identity.blocker = ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady;
        }
        return identity;
    }
    if (hasAHistoryRemoveForeignSourceLineage && hasSameSourceStrictRemoveSourceEvidence) {
        const TopoDS_Edge& resultEdge =
            resultWireProducerMapperHistoryInputEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        const std::vector<std::size_t> sameSourceStrictRemoveSourceIndices
            = strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo);
        std::optional<std::size_t> sameSourceProducerLedgerSidecarIndex;
        for (const std::size_t sourceIndex : sameSourceStrictRemoveSourceIndices) {
            if (sourceIndex >= info.edges.size()) {
                continue;
            }
            if (wireJoinerMapperHistoryProducerEvidenceReady(
                    materializationLedger,
                    sourceIndex
                )) {
                sameSourceProducerLedgerSidecarIndex = sourceIndex;
                break;
            }
        }
        const std::optional<std::size_t> sameSourceSidecarProducerLedgerReadyIndex
            = producerLedgerReadySameSourceSidecarProducerIndex(
                info,
                edgeInfo,
                materializationLedger,
                &resultEdge
            );
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() records the actual Remove source through
        // "aHistory->Remove(info.edge)". If that source is foreign but another strict Remove
        // source exists in this result-wire candidate slot's sourceEdgeArray lineage, the remaining blocker is
        // either missing producer-ledger sidecar identity or a producer sidecar whose curve does
        // not represent this result edge, not absence of evidence.
        if (sameSourceSidecarProducerLedgerReadyIndex) {
            identity.sourceEdgeInfoIndex = *sameSourceSidecarProducerLedgerReadyIndex;
        }
        else if (sameSourceProducerLedgerSidecarIndex) {
            identity.sourceEdgeInfoIndex = *sameSourceProducerLedgerSidecarIndex;
        }
        else if (!sameSourceStrictRemoveSourceIndices.empty()) {
            identity.sourceEdgeInfoIndex = sameSourceStrictRemoveSourceIndices.front();
        }
        identity.state = ResultWireProducerState::AHistoryEvidenceReady;
        identity.blocker = (!sameSourceSidecarProducerLedgerReadyIndex
                            && sameSourceProducerLedgerSidecarIndex)
            ? ResultWireBlocker::SameSourceSidecarGeometryMismatch
            : ResultWireBlocker::SameSourceSidecarSourceShapeIdentityNotReady;
        return identity;
    }
    if (hasAHistoryRemoveForeignSourceLineage) {
        const TopoDS_Edge& resultEdge =
            resultWireProducerMapperHistoryInputEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        const std::optional<std::size_t> foreignRemoveProducerLedgerReadyIndex
            = producerLedgerReadyAHistoryRemoveProducerIndex(info, edgeInfo, materializationLedger);
        if (foreignRemoveProducerLedgerReadyIndex) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::buildClosedWire() stores the true producer through
            // "aHistory->Remove(info.edge)". When that foreign producer already has a producer-ledger
            // result-wire candidate slot but cannot represent this result edge, the remaining blocker is producer
            // geometry ownership, not missing geometry or missing aHistory evidence.
            identity.sourceEdgeInfoIndex = *foreignRemoveProducerLedgerReadyIndex;
            identity.state = ResultWireProducerState::AHistoryEvidenceReady;
            identity.blocker = ResultWireBlocker::ForeignAHistorySourceGeometryMismatch;
            return identity;
        }
        if (!aHistoryRemoveSourceEdgeInfoIndices.empty() && ![&]() {
                for (const std::size_t sourceIndex : aHistoryRemoveSourceEdgeInfoIndices) {
                    if (sourceIndex >= info.edges.size()) {
                        continue;
                    }
                    const EdgeInfo& producer = info.edges[sourceIndex];
                    if (edgeWithProducerCurveAndResultVertices(producer.edge, resultEdge)) {
                        return true;
                    }
                    if (edgeWithProducerCurveAndResultVertices(
                            resultWireProducerMapperHistoryInputEdge(
                                producer,
                                materializationLedger,
                                sourceIndex
                            ),
                            resultEdge
                        )) {
                        return true;
                    }
                }
                return false;
            }()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::buildClosedWire() stores the actual producer with
            // "aHistory->Remove(info.edge)". If that producer cannot be rebuilt on this
            // result-wire candidate edge, keeping the blocker as source-shape identity not ready hides a geometry
            // ownership mismatch in the producer ledger.
            identity.sourceEdgeInfoIndex = aHistoryRemoveSourceEdgeInfoIndices.front();
            identity.state = ResultWireProducerState::AHistoryEvidenceReady;
            identity.blocker = ResultWireBlocker::ForeignAHistorySourceGeometryMismatch;
            return identity;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire() records the result-wire producer through
        // "aHistory->Remove(info.edge)" even when that producer is from a foreign source lineage.
        // Keep that actual Remove-source EdgeInfo in the identity while the blocker explains why it
        // cannot be used as the result-wire candidate slot's producer-ledger output.
        if (!aHistoryRemoveSourceEdgeInfoIndices.empty()) {
            identity.sourceEdgeInfoIndex = aHistoryRemoveSourceEdgeInfoIndices.front();
            identity.state = ResultWireProducerState::AHistoryEvidenceReady;
            identity.blocker = ResultWireBlocker::ForeignAHistorySourceShapeIdentityNotReady;
            return identity;
        }
        identity.state = ResultWireProducerState::AHistoryEvidenceReady;
        identity.blocker = ResultWireBlocker::ForeignAHistorySourceLineage;
        return identity;
    }
    if (!hasAHistoryRemoveSourceEvidence && !isLiveFinalGateOpenEdgeProducer) {
        identity.blocker = ResultWireBlocker::MissingAHistoryRemoveSource;
        return identity;
    }
    if (!hasFullAHistoryProducerEvidence && !isLiveFinalGateOpenEdgeProducer) {
        if (hasAHistoryRemoveSourceEvidence && !hasRemovedTarget
            && !hasRootFullAHistoryProducerEvidence) {
            identity.blocker = ResultWireBlocker::MissingRemovedTargetEvidence;
            return identity;
        }
        identity.blocker = ResultWireBlocker::MissingFullAHistoryProducerEvidence;
        return identity;
    }

    identity.state = isLiveFinalGateOpenEdgeProducer
        ? ResultWireProducerState::ChildWireReady
        : ResultWireProducerState::AHistoryEvidenceReady;
    if (isLiveFinalGateOpenEdgeProducer) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(), with noOriginal=true, removes a wire only when every edge
        // is found by "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))". A live
        // final-gate producer that would be purged as an original source edge cannot replace the
        // result-wire candidate shape without losing the openWireCompound child.
        const bool liveFinalGateWouldBePurgedAsOriginal = !sourceEdges_.empty()
            && edgeUsesOnlyOriginalSourceVerticesByIdentity(edgeInfo.edge, sourceEdges_);
        identity.blocker = liveFinalGateWouldBePurgedAsOriginal
            ? ResultWireBlocker::LiveResetSourceShapeWouldPurgeOriginal
            : ResultWireBlocker::SourceShapeIdentityNotReady;
        return identity;
    }

    const bool rootIterationBlockedMissingRemovalBranch
        = materializationEntry.superEdgeRootExportBlockedByIteration
        && !materializationEntry.superEdgeRootIterationBlockedUnownedRemoval
        && !materializationEntry.superEdgeRootIterationBlockedPrimaryRemoval
        && !materializationEntry.superEdgeRootIterationBlockedSecondaryRemoval;
    const bool finalGateExportBlockedByIteration =
        !sourceExportsOpenEdge && edgeInfo.iteration < 0 && edgeInfo.iteration != -3;
    const bool finalGateExportBlockedByWireInfo =
        !sourceExportsOpenEdge && edgeInfo.iteration >= 0 && edgeInfo.wireInfo != 0U;
    if (rootIterationBlockedMissingRemovalBranch) {
        identity.blocker = ResultWireBlocker::UnknownInvariant;
    }
    else if (materializationEntry.superEdgeRootIterationBlockedUnownedRemoval) {
        identity.blocker = ResultWireBlocker::RootRemovedByUnownedBranch;
    }
    else if (materializationEntry.superEdgeRootIterationBlockedPrimaryRemoval) {
        identity.blocker = ResultWireBlocker::RootRemovedByPrimaryBranch;
    }
    else if (materializationEntry.superEdgeRootIterationBlockedSecondaryRemoval) {
        identity.blocker = ResultWireBlocker::RootRemovedBySecondaryBranch;
    }
    else if (finalGateExportBlockedByIteration) {
        identity.blocker = ResultWireBlocker::FinalGateBlockedByIteration;
    }
    else if (finalGateExportBlockedByWireInfo) {
        identity.blocker = ResultWireBlocker::FinalGateBlockedByWireInfo;
    }
    return identity;
}

void WireJoiner::Impl::attachResultWireProducerLedger(
    WireInfo& info,
    WireJoinerHistoryMaterializationLedger& materializationLedger
)
{
    if (materializationLedger.edgeEntries.size() < info.edges.size()) {
        materializationLedger.edgeEntries.resize(info.edges.size());
    }
    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
        if (!wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
                materializationLedger,
                edgeInfoIndex
            )) {
            continue;
        }
        materializationLedger.edgeEntries[edgeInfoIndex].resultWireProducer =
            classifyResultWireProducerSlot(
                info,
                edgeInfoIndex,
                materializationLedger
            );
    }
}

bool WireJoiner::Impl::resultWireProducerIdentityPublishesChildWireLedgerEntry(
    const ResultWireProducerIdentity& identity
) const
{
    return identity.kind != ResultWireProducerKind::None
        || identity.state != ResultWireProducerState::Unpublished
        || identity.blocker != ResultWireBlocker::None
        || identity.sourceEdgeInfoIndex != resultWireProducerNpos
        || identity.rootEdgeInfoIndex != resultWireProducerNpos
        || identity.currentMemberEdgeInfoIndex != resultWireProducerNpos;
}

bool WireJoiner::Impl::openWireCompoundChildWireHasSourceEdgeProducerOutput(
    const OpenWireCompoundWireInfo& childWire
) const
{
    return childWire.producerLedgerWireBuilt;
}

OpenWireCompoundExportSource WireJoiner::Impl::childWireFinalOpenExportSource(
    const OpenWireCompoundWireInfo& childWire
) const
{
    if (childWire.openLeafIterationMinus3) {
        return OpenWireCompoundExportSource::OpenLeafIterationMinus3;
    }
    if (childWire.unownedOpenEdge) {
        return OpenWireCompoundExportSource::UnownedOpenEdge;
    }
    if (childWire.currentMemberProducerOutput
        || childWire.currentMemberChildWireProducerReady
        || childWire.rootCurrentMemberChildProducer) {
        return OpenWireCompoundExportSource::RootCurrentMemberChildProducer;
    }
    if (childWire.producerLedgerWireBuilt
        || childWire.producerLedgerWireFromSourceVmap
        || openWireCompoundChildWireHasSourceEdgeProducerOutput(childWire)) {
        return OpenWireCompoundExportSource::AHistoryProducerChildWire;
    }
    return OpenWireCompoundExportSource::None;
}

ResultWireProducerIdentity WireJoiner::Impl::childWireResultWireProducerIdentity(
    const WireInfo& info,
    const OpenWireCompoundWireInfo& childWire,
    std::size_t childWireIndex,
    const WireJoinerHistoryMaterializationLedger& materializationLedger
) const
{
    ResultWireProducerIdentity identity;
    bool edgeProducerIdentity = false;
    if (childWire.edgeIndex < info.edges.size()) {
        if (childWire.edgeIndex < materializationLedger.edgeEntries.size()) {
            identity = materializationLedger.edgeEntries[childWire.edgeIndex].resultWireProducer;
            edgeProducerIdentity = resultWireProducerIdentityPublishesChildWireLedgerEntry(
                identity
            );
        }
    }
    const bool hasSourceEdgeProducerOutput =
        openWireCompoundChildWireHasSourceEdgeProducerOutput(childWire);
    const bool hasChildWireProducerSignal = childWire.currentMemberProducerOutput
        || hasSourceEdgeProducerOutput
        || childWire.currentMemberSplitLedgerVertexMultiplicityBlocked
        || childWire.currentMemberChildWireProducerReady
        || childWire.rootResultWireProducerRequiresMemberSuppression;
    if (!edgeProducerIdentity && !hasChildWireProducerSignal) {
        return identity;
    }
    identity.childWireInfoIndex = childWireIndex;
    identity.childWireBuilt = childWire.wireBuilt;
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() publishes final children with
    // "builder.Add(openWireCompound, info.wire())"; child-wire source readiness is derived from
    // the materialized openWireCompound child ledger, not stored on ResultWireProducerIdentity.
    const bool childWireProducerLedgerReady = hasSourceEdgeProducerOutput
        || childWire.currentMemberProducerOutput;
    if (childWire.currentMemberProducerOutput) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() stores root/member output on "first->superEdge",
        // then ::build() exports the final child with "builder.Add(openWireCompound, info.wire())".
        // Once the child wire has been replaced by the current-member producer output, publish the
        // child-wire owner instead of the root result-slot classifier.
        identity.kind = ResultWireProducerKind::CurrentMemberChildWire;
        identity.state = ResultWireProducerState::ExportedWithoutTransitionalSlot;
        identity.blocker = ResultWireBlocker::None;
        return identity;
    }
    if (hasSourceEdgeProducerOutput) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports the final producer child through openWireCompound, but the
        // producer category still comes from the EdgeInfo / WireInfo / aHistory lifecycle that selected
        // the child. Preserve that kind on the child-wire ledger instead of collapsing every
        // producer-ledger output to ExistingSourceEdge.
        if (identity.kind == ResultWireProducerKind::None) {
            identity.kind = ResultWireProducerKind::ExistingSourceEdge;
        }
        identity.state = ResultWireProducerState::ExportedWithoutTransitionalSlot;
        identity.blocker = ResultWireBlocker::None;
        return identity;
    }
    if (childWire.currentMemberSplitLedgerVertexMultiplicityBlocked) {
        // FreeCAD:
        // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports current-member output only after the EdgeInfo/superEdge
        // child shape can pass through openWireCompound and MapperHistory(aHistory). If the formal
        // member/split candidate would change vertex multiplicity, keep the child at ChildWireReady
        // with an explicit blocker instead of reporting it as exported.
        identity.kind = ResultWireProducerKind::CurrentMemberChildWire;
        identity.state = ResultWireProducerState::ChildWireReady;
        identity.blocker = ResultWireBlocker::CurrentMemberVertexMultiplicityBlocked;
        return identity;
    }
    if (childWire.currentMemberChildWireProducerReady) {
        identity.kind = ResultWireProducerKind::CurrentMemberChildWire;
        identity.state = ResultWireProducerState::ChildWireReady;
        if (childWire.currentMemberProducerBlockedByVertexMultiplicity) {
            // FreeCAD:
            // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::findSuperEdgesUpdateFirst() feeds current member shapes into
            // "first->superEdge"; ::getOpenWires() later consumes MapperHistory(aHistory). Keep this
            // as an explicit child-wire blocker when the member/split ledger candidate would change
            // vertex multiplicity instead of exporting a transitional result-slot child.
            identity.blocker = ResultWireBlocker::CurrentMemberVertexMultiplicityBlocked;
        }
        else if (childWire.currentMemberProducerBlockedBySourceShape) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() adds final child wires with "builder.Add(openWireCompound,
            // info.wire())"; ::getOpenWires(noOriginal=true) can then remove original source wires.
            // Keep purge and vertex-identity blockers separate so the current-member path does not
            // hide a missing child-wire vertex ledger behind the generic source-shape gate.
            const bool memberSuppressedWouldBePurgedAsOriginal = !sourceEdges_.empty()
                && allEdgesShareOriginalSourceVertexByIdentity(
                    childWire.currentMemberProducerWire,
                    sourceEdges_
                );
            identity.blocker = memberSuppressedWouldBePurgedAsOriginal
                ? ResultWireBlocker::CurrentMemberSourceShapeWouldPurgeOriginal
                : ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady;
        }
        else if (childWire.currentMemberProducerBlockedByPendingMember) {
            identity.blocker = ResultWireBlocker::MultiMemberRootPendingSuppression;
        }
        else if (!childWireProducerLedgerReady) {
            identity.blocker = ResultWireBlocker::CurrentMemberChildWireIdentityNotReady;
        }
        return identity;
    }
    if (childWire.rootResultWireProducerRequiresMemberSuppression) {
        identity.kind = ResultWireProducerKind::SuperEdgeRoot;
        identity.state = ResultWireProducerState::ChildWireReady;
        identity.blocker = ResultWireBlocker::MultiMemberRootPendingSuppression;
        return identity;
    }
    if (childWire.wireBuilt && identity.kind != ResultWireProducerKind::None) {
        identity.state = ResultWireProducerState::ChildWireReady;
    }
    if (childWireProducerLedgerReady) {
        identity.state = ResultWireProducerState::ProducerLedgerReady;
    }
    if (identity.blocker == ResultWireBlocker::None) {
        identity.blocker = ResultWireBlocker::SourceShapeIdentityNotReady;
    }
    return identity;
}

bool WireJoiner::Impl::memberSuppressedCurrentMemberProducerLedgerReady(
    const WireInfo& info,
    const OpenWireCompoundWireInfo& childWire,
    std::size_t childWireIndex
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores open current-member output on
    // "first->superEdge"; that producer reaches ::build() through openWireCompound, not through a
    // synthetic buildClosedWire() "aHistory->Remove(info.edge)" event.
    const bool rootOpenCurrentMemberProducerEvidence
        = childWire.currentMemberEdgeInfo
        && childWire.superEdgeRootEdgeInfoIndex < info.edges.size()
        && childWire.superEdgeRootOpenWireCompoundEligible
        && info.edges[childWire.superEdgeRootEdgeInfoIndex].superEdgeLifecycleOpenRoot
        && info.edges[childWire.superEdgeRootEdgeInfoIndex].superEdgeMaterialized
        && !info.edges[childWire.superEdgeRootEdgeInfoIndex].superEdge.IsNull();
    if (!childWire.currentMemberChildWireProducerReady
        || !childWire.currentMemberProducerWireBuilt) {
        return false;
    }
    if (!childWire.currentMemberChildWireProducerFullAHistoryEvidence
        && !rootOpenCurrentMemberProducerEvidence) {
        return false;
    }

    std::vector<TopoDS_Vertex> ledgerVertices;
    for (std::size_t index = 0; index < info.openWireCompoundWires.size(); ++index) {
        if (index == childWireIndex) {
            continue;
        }
        const OpenWireCompoundWireInfo& ledgerChildWire = info.openWireCompoundWires[index];
        if (ledgerChildWire.wire.IsNull()) {
            continue;
        }
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() emits each final "info.wire()" after vmap/sourceEdges replacement.
        // A current-member child must be covered by request-local child/member/split vertices that
        // have already left the transitional result-slot path; result-slot-only child wires cannot
        // prove another current-member child's source-shape readiness.
        const std::vector<TopoDS_Vertex> vertices = wireVertices(ledgerChildWire.wire);
        ledgerVertices.insert(ledgerVertices.end(), vertices.begin(), vertices.end());
    }
    if (rootOpenCurrentMemberProducerEvidence && childWire.edgeIndex < info.edges.size()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() calls "wireData->Add(current->shape(...))"
        // for every member before "first->superEdge = makeCleanWire(false)", and
        // ::makeCleanWire() merges ShapeFix_Wire history into aHistory. For open-root
        // current-member producers, the current member EdgeInfo input vertices are therefore part
        // of the same request-local producer ledger even when the member itself is not exported by
        // openWireCompound.
        const std::vector<TopoDS_Vertex> memberVertices = edgeVertices(
            info.edges[childWire.edgeIndex].edge
        );
        ledgerVertices.insert(ledgerVertices.end(), memberVertices.begin(), memberVertices.end());
    }
    const std::vector<TopoDS_Vertex> producerVertices = wireVertices(
        childWire.currentMemberProducerWire
    );
    const bool producerVerticesCoveredByLedger = !producerVertices.empty()
        && std::all_of(producerVertices.begin(), producerVertices.end(), [&](const TopoDS_Vertex& vertex) {
               return vertexMatchesAnyByIdentity(vertex, ledgerVertices);
           });

    if (!producerVerticesCoveredByLedger) {
        return false;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), with noOriginal=true, removes a wire only when every
    // openWireCompound edge finds a source edge via "findSubShapesWithSharedVertex(TopoShape(edge,
    // -1))"; TopoShapeExpansion.cpp then checks both endpoint coordinates and edge geometry.
    // A current-member child wire can replace the result-wire candidate shape once its producer vertices are
    // already present in the child-wire ledger. The final noOriginal filtering remains in
    // getOpenWires(), after all current-member children have been grouped into their final wires.
    return true;
}

ResultWireProducerLedgerEntry WireJoiner::Impl::resultWireProducerLedgerEntryForChildWire(
    const OpenWireCompoundWireInfo& childWire,
    std::size_t childWireIndex
) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() materializes children with "builder.Add(openWireCompound,
    // info.wire())", then ::getOpenWires() consumes that compound with MapperHistory(aHistory).
    // Publish result-wire producer ledger entries from the child-wire slot so history consumers do
    // not reassemble public producer entries from EdgeInfo sidecars after openWireCompound exists.
    ResultWireProducerLedgerEntry entry;
    entry.openExportIndex = childWireIndex + 1U;
    entry.sourceEdgeInfoIndex = childWire.resultWireProducer.sourceEdgeInfoIndex;
    entry.rootEdgeInfoIndex = childWire.resultWireProducer.rootEdgeInfoIndex;
    entry.currentMemberEdgeInfoIndex = childWire.resultWireProducer.currentMemberEdgeInfoIndex;
    entry.childWireInfoIndex = childWireIndex;
    entry.kind = childWire.resultWireProducer.kind;
    entry.state = childWire.resultWireProducer.state;
    entry.blocker = childWire.resultWireProducer.blocker;
    entry.openWireCompoundExportSource = childWire.openExportSource;
    entry.openWireCompoundEdgeInfoIteration = childWire.edgeInfoIteration;
    entry.openWireCompoundEdgeInfoIteration2 = childWire.edgeInfoIteration2;
    entry.openWireCompoundOwnerWireInfo = childWire.ownerWireInfo;
    entry.openWireCompoundOwnerWireInfo2 = childWire.ownerWireInfo2;
    entry.openWireCompoundOpenLeafExport = childWire.openLeafIterationMinus3;
    entry.openWireCompoundUnownedOpenEdgeExport = childWire.unownedOpenEdge;
    entry.openWireCompoundRootCurrentMemberChildProducer =
        childWire.rootCurrentMemberChildProducer;
    entry.wireJoinerHistoryEventIndex = childWire.wireJoinerHistoryEventIndex;
    entry.childShapeIdentityRecorded = childWire.childShapeIdentityRecorded;
    entry.childWireEdgeCount = childWire.childWireEdgeCount;
    entry.childWireVertexCount = childWire.childWireVertexCount;
    entry.sourceEdgeIndices = childWire.sourceEdgeIndices;
    return entry;
}

void WireJoiner::Impl::applyWireJoinerHistoryMaterialization(
    WireInfo& info,
    WireJoinerHistoryMaterializationLedger& materializationLedger
)
{
    if (!materializationLedger.needed) {
        return;
    }

    materializationLedger.edgeEntries.assign(
        info.edges.size(),
        WireJoinerHistoryMaterializationEdgeEntry {}
    );
    materializationLedger.mapperHistoryProducerEvidence.clear();
    for (const WireJoinerHistoryMaterializationBinding& binding : materializationLedger.bindings) {
        if (binding.resultSlotEdge.IsNull()) {
            continue;
        }
        if (binding.edgeInfoIndex >= info.edges.size()
            || wireJoinerMapperHistoryProducerEvidenceReady(
                materializationLedger,
                binding.edgeInfoIndex
            )) {
            continue;
        }
        const std::size_t selectedSourceEdgeInfoIndex = binding.edgeInfoIndex;

        {
            EdgeInfo& edgeInfo = info.edges[selectedSourceEdgeInfoIndex];
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports "info.wire()" from final EdgeInfo states. The
            // materialization binding now points at that final EdgeInfo row directly; the
            // resultSlotEdge remains request-local endpoint evidence; the producer child is now
            // published through MapperHistory producer evidence plus the openWireCompound ledger.
            const bool sourceExportsOpenEdge = edgeInfoExportsOpenWireCompound(edgeInfo);
            const bool sourceConsumedByBuildClosedWire = edgeInfo.buildClosedWireRemoved
                || edgeInfo.buildClosedWireAHistoryRemoved;
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() calls "builder.Add(openWireCompound, info.wire())" from the
            // final EdgeInfo, not from a generated transitional copy. When buildClosedWire() records full
            // "aHistory->Remove(info.edge)" evidence, the selected EdgeInfo supplies the producer
            // curve; reuse the equivalent result edge vertices so switching away from result-wire candidate output
            // does not introduce extra topological vertices before P6 removes the result-wire candidate finder.
            const bool hasFullAHistoryProducerEvidence
                = resultWireProducerSlotHasFullAHistoryEvidence(edgeInfo);
            const std::vector<std::size_t> sourceLineageRemovedSourceEdgeInfoIndices
                = strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo);
            const bool hasSameSourceStrictRemoveSourceEvidence
                = !sourceLineageRemovedSourceEdgeInfoIndices.empty();
            const bool hasForeignAHistoryRemoveSourceLineage
                = !edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices.empty()
                && !sourceEdgeIndicesIntersect(
                    edgeInfo.sourceEdgeIndices,
                    edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices
                );
            const bool hasFullResultWireProducerEvidence = hasFullAHistoryProducerEvidence
                || (edgeInfo.buildClosedWireRemoved && !edgeInfo.sourceEdgeIndices.empty()
                    && hasSameSourceStrictRemoveSourceEvidence
                    && !hasForeignAHistoryRemoveSourceLineage);
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports live result wires without a Remove-source event when
            // "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)". Such a slot is a
            // live final-gate producer, not a buildClosedWire() "aHistory->Remove(info.edge)"
            // producer. It can replace the result-slot edge only if getOpenWires(noOriginal=true)'s
            // source shared-vertex search would not purge the producer-ledger result edge.
            const bool hasLiveFinalGateProducerEvidence = sourceExportsOpenEdge
                && !sourceConsumedByBuildClosedWire && !edgeInfo.sourceEdgeIndices.empty()
                && !hasForeignAHistoryRemoveSourceLineage;
            const std::optional<std::size_t> producerLedgerReadyForeignAHistoryRemoveProducerIndex
                = hasForeignAHistoryRemoveSourceLineage && !hasSameSourceStrictRemoveSourceEvidence
                ? producerLedgerReadyAHistoryRemoveProducerIndex(
                    info,
                    edgeInfo,
                    materializationLedger,
                    &binding.resultSlotEdge
                )
                : std::optional<std::size_t> {};
            std::optional<TopoDS_Edge> foreignAHistoryRemoveProducerExportShape;
            if (hasForeignAHistoryRemoveSourceLineage && !hasSameSourceStrictRemoveSourceEvidence
                && !producerLedgerReadyForeignAHistoryRemoveProducerIndex) {
                for (const std::size_t sourceIndex :
                     edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices) {
                    if (sourceIndex >= info.edges.size()) {
                        continue;
                    }
                    const EdgeInfo& producer = info.edges[sourceIndex];
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::buildClosedWire() calls "aHistory->Remove(info.edge)" with
                    // the producer EdgeInfo, not with the removed target. For true foreign
                    // producer evidence, reuse that exact producer curve only when it can
                    // represent the legacy result-slot edge with the result vertices; this is still
                    // a producer-ledger switch, not a transitional geometry guess.
                    foreignAHistoryRemoveProducerExportShape = edgeWithProducerCurveAndResultVertices(
                        producer.edge,
                        binding.resultSlotEdge
                    );
                    if (!foreignAHistoryRemoveProducerExportShape
                        || foreignAHistoryRemoveProducerExportShape->IsNull()) {
                        foreignAHistoryRemoveProducerExportShape = edgeWithProducerCurveAndResultVertices(
                            resultWireProducerMapperHistoryInputEdge(
                                producer,
                                materializationLedger,
                                sourceIndex
                            ),
                            binding.resultSlotEdge
                        );
                    }
                    if (foreignAHistoryRemoveProducerExportShape
                        && !foreignAHistoryRemoveProducerExportShape->IsNull()) {
                        break;
                    }
                }
            }
            const std::optional<std::size_t> producerLedgerReadySameSourceSidecarProducerIndex
                = hasForeignAHistoryRemoveSourceLineage && hasSameSourceStrictRemoveSourceEvidence
                ? this->producerLedgerReadySameSourceSidecarProducerIndex(
                      info,
                      edgeInfo,
                      materializationLedger,
                      &binding.resultSlotEdge
                  )
                : std::optional<std::size_t> {};
            std::optional<TopoDS_Edge> sourceEdgeExportShape;
            std::optional<TopoDS_Edge> candidateSourceEdgeExportShape;
            if (edgeInfo.edge.IsSame(binding.resultSlotEdge)) {
                candidateSourceEdgeExportShape = edgeInfo.edge;
            }
            else if (hasFullResultWireProducerEvidence || hasLiveFinalGateProducerEvidence) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() exports the producer EdgeInfo's "info.wire()" and
                // ::getOpenWires() consumes MapperHistory(aHistory). Use the producer shape curve or
                // contained subsegment with result-slot vertices, so result-slot endpoint
                // materialization stays ledger evidence while the emitted child wire keeps the
                // same vertex identity.
                candidateSourceEdgeExportShape
                    = edgeWithProducerCurveAndResultVertices(edgeInfo.edge, binding.resultSlotEdge);
            }
            else if (
                hasForeignAHistoryRemoveSourceLineage && edgeInfo.sourceLineageFromSplitterHistory
            ) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::splitEdges() records split-result ownership with
                // "aHistory->AddModified(split.intersectShape, newInfo.edge)". If a later
                // buildClosedWire() removal points at a foreign Remove source, the current
                // source-lineage split fragment is still a request-local result-wire producer, but
                // it may need to be rebuilt with the legacy result-slot vertices.
                candidateSourceEdgeExportShape
                    = edgeWithProducerCurveAndResultVertices(edgeInfo.edge, binding.resultSlotEdge);
            }
            else if (producerLedgerReadyForeignAHistoryRemoveProducerIndex) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::buildClosedWire() records the result-wire source as
                // "aHistory->Remove(info.edge)". For a true foreign Remove source, only switch the
                // slot when that exact source already has a producer-ledger output and can be
                // rebuilt with the legacy result-slot vertices.
                const EdgeInfo& producer
                    = info.edges[*producerLedgerReadyForeignAHistoryRemoveProducerIndex];
                candidateSourceEdgeExportShape = edgeWithEquivalentResultVertices(
                    resultWireProducerMapperHistoryInputEdge(
                        producer,
                        materializationLedger,
                        *producerLedgerReadyForeignAHistoryRemoveProducerIndex
                    ),
                    binding.resultSlotEdge
                );
                if (!candidateSourceEdgeExportShape || candidateSourceEdgeExportShape->IsNull()) {
                    candidateSourceEdgeExportShape = edgeWithProducerCurveAndResultVertices(
                        producer.edge,
                        binding.resultSlotEdge
                    );
                }
            }
            else if (foreignAHistoryRemoveProducerExportShape) {
                candidateSourceEdgeExportShape = *foreignAHistoryRemoveProducerExportShape;
            }
            else if (producerLedgerReadySameSourceSidecarProducerIndex) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::buildClosedWire() can remove this target from a foreign
                // aHistory source while a strict same-lineage sidecar also records
                // "aHistory->Remove(info.edge)". When that sidecar already has producer-ledger
                // output, reuse its producer curve or contained subsegment with the legacy
                // result-slot vertices instead of keeping the legacy result-wire candidate child.
                const EdgeInfo& producer = info.edges[*producerLedgerReadySameSourceSidecarProducerIndex];
                candidateSourceEdgeExportShape = edgeWithProducerCurveAndResultVertices(
                    resultWireProducerMapperHistoryInputEdge(
                        producer,
                        materializationLedger,
                        *producerLedgerReadySameSourceSidecarProducerIndex
                    ),
                    binding.resultSlotEdge
                );
                if (!candidateSourceEdgeExportShape || candidateSourceEdgeExportShape->IsNull()) {
                    candidateSourceEdgeExportShape = edgeWithProducerCurveAndResultVertices(
                        producer.edge,
                        binding.resultSlotEdge
                    );
                }
            }
            if (candidateSourceEdgeExportShape && !candidateSourceEdgeExportShape->IsNull()) {
                const bool liveFinalGateWouldBePurgedAsOriginal = hasLiveFinalGateProducerEvidence
                    && !hasFullResultWireProducerEvidence && !sourceEdges_.empty()
                    && edgeUsesOnlyOriginalSourceVerticesByIdentity(*candidateSourceEdgeExportShape,
                                                                    sourceEdges_);
                if (!liveFinalGateWouldBePurgedAsOriginal) {
                    sourceEdgeExportShape = *candidateSourceEdgeExportShape;
                }
            }
            const bool useSourceEdgeExportShape = sourceEdgeExportShape.has_value()
                && !sourceEdgeExportShape->IsNull();
            const std::optional<std::size_t> superEdgeRootIndex
                = superEdgeRootIndexForMember(info, edgeInfo);
            WireJoinerHistoryMaterializationEdgeEntry& materializationEntry =
                materializationLedger.edgeEntries[selectedSourceEdgeInfoIndex];
            if (useSourceEdgeExportShape) {
                recordWireJoinerMapperHistoryProducerEvidence(
                    materializationLedger,
                    selectedSourceEdgeInfoIndex,
                    *sourceEdgeExportShape
                );
            }
            materializationEntry.superEdgeMember
                = edgeInfo.superEdgeLifecycleMemberMinusOne;
            materializationEntry.superEdgeRoot = superEdgeRootIndex.has_value();
            if (superEdgeRootIndex) {
                const EdgeInfo& rootEdgeInfo = info.edges[*superEdgeRootIndex];
                const bool rootExportsOpenEdge = edgeInfoExportsOpenWireCompound(rootEdgeInfo);
                materializationEntry.superEdgeRootIndex = *superEdgeRootIndex;
                materializationEntry.superEdgeRootOpenWireCompoundEligible
                    = rootExportsOpenEdge;
                materializationEntry.superEdgeRootOpenLifecycle
                    = rootEdgeInfo.superEdgeLifecycleOpenRoot;
                const bool rootFullAHistoryProducerEvidence
                    = resultWireProducerRootHasFullAHistoryEvidence(
                        rootEdgeInfo
                    );
                materializationEntry.superEdgeRootExportBlockedByIteration = !rootExportsOpenEdge
                    && rootEdgeInfo.iteration < 0 && rootEdgeInfo.iteration != -3;
                materializationEntry.superEdgeRootIterationBlockedUnownedRemoval
                    = materializationEntry.superEdgeRootExportBlockedByIteration
                    && rootEdgeInfo.buildClosedWireRemovedByUnowned;
                materializationEntry.superEdgeRootIterationBlockedPrimaryRemoval
                    = materializationEntry.superEdgeRootExportBlockedByIteration
                    && rootEdgeInfo.buildClosedWireRemovedByPrimaryOwner;
                materializationEntry.superEdgeRootIterationBlockedSecondaryRemoval
                    = materializationEntry.superEdgeRootExportBlockedByIteration
                    && rootEdgeInfo.buildClosedWireRemovedBySecondaryOwner;
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() stores an open root wire in
                // "first->superEdge = makeCleanWire(false)"; ::buildClosedWire() can later remove
                // that root before ::build() exports openWireCompound. Track this as the next M3
                // producer candidate, but do not export root.superEdge from the transitional result-slot path yet.
                materializationEntry.superEdgeRootProducerCandidate
                    = materializationEntry.superEdgeRootExportBlockedByIteration
                    && rootEdgeInfo.superEdgeLifecycleOpenRoot && rootEdgeInfo.superEdgeMaterialized
                    && !rootEdgeInfo.superEdge.IsNull();
                materializationEntry.superEdgeRootProducerFullAHistoryEvidence
                    = materializationEntry.superEdgeRootProducerCandidate
                    && rootFullAHistoryProducerEvidence;
                const bool rootResultWireProducerCandidateUnownedRemoval
                    = materializationEntry.superEdgeRootProducerCandidate
                    && materializationEntry.superEdgeRootIterationBlockedUnownedRemoval;
                materializationEntry.superEdgeRootProducerUnownedRemovalChildWireReady
                    = rootResultWireProducerCandidateUnownedRemoval
                    && materializationEntry.superEdgeRootProducerFullAHistoryEvidence;
                materializationEntry.superEdgeRootProducerPrimaryRemoval
                    = materializationEntry.superEdgeRootProducerCandidate
                    && materializationEntry.superEdgeRootIterationBlockedPrimaryRemoval;
                materializationEntry.superEdgeRootProducerSecondaryRemoval
                    = materializationEntry.superEdgeRootProducerCandidate
                    && materializationEntry.superEdgeRootIterationBlockedSecondaryRemoval;
                const bool rootOpenCurrentMemberProducerCandidate
                    = !materializationEntry.superEdgeRootProducerCandidate
                    && materializationEntry.superEdgeMember
                    && materializationEntry.superEdgeRootOpenLifecycle
                    && materializationEntry.superEdgeRootOpenWireCompoundEligible
                    && rootEdgeInfo.superEdgeMaterialized && !rootEdgeInfo.superEdge.IsNull();
                if ((materializationEntry.superEdgeRootProducerCandidate
                     || rootOpenCurrentMemberProducerCandidate)
                    && rootEdgeInfo.superEdgeInfo != 0U) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::findSuperEdgesUpdateFirst() walks every "current" member,
                    // sets members to "current->iteration = -1", and stores the materialized
                    // result on the root "first->superEdge". Track the whole member set here so
                    // the child-wire producer guard can explain exactly which non-current members
                    // a root export would still carry.
                    for (std::size_t memberIndex = 0; memberIndex < info.edges.size(); ++memberIndex) {
                        const EdgeInfo& member = info.edges[memberIndex];
                        if (member.superEdgeInfo != rootEdgeInfo.superEdgeInfo) {
                            continue;
                        }
                        appendUniqueSourceIndex(
                            materializationEntry.superEdgeRootCoveredMemberIndices,
                            memberIndex
                        );
                        if (memberIndex == selectedSourceEdgeInfoIndex) {
                            materializationEntry.superEdgeRootCurrentMember
                                = true;
                        }
                    }
                }
            }
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() never appends a detached EdgeInfo for result-wire export; it only
        // emits existing final EdgeInfo states through "openWireCompound". If the transitional
        // transitional result-slot cannot bind one generated export edge to one final EdgeInfo, keep it as M3 risk
        // evidence instead of turning it into output geometry.
    }

    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
        EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
        WireJoinerHistoryMaterializationEdgeEntry& materializationEntry =
            materializationLedger.edgeEntries[edgeInfoIndex];
        const bool hasAHistoryRemoveForeignSourceLineage =
            !edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices.empty()
            && !sourceEdgeIndicesIntersect(
                edgeInfo.sourceEdgeIndices,
                edgeInfo.buildClosedWireAHistoryRemoveSourceEdgeIndices
            );
        const bool hasSourceLineageRemovedSourceEdgeInfo =
            !strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo).empty();
        if (!wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
                materializationLedger,
                edgeInfoIndex
            )
            || wireJoinerMapperHistoryProducerEvidenceReady(
                materializationLedger,
                edgeInfoIndex
            )
            || !hasAHistoryRemoveForeignSourceLineage
            || !hasSourceLineageRemovedSourceEdgeInfo) {
            continue;
        }
        const TopoDS_Edge resultEdge =
            resultWireProducerMapperHistoryInputEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        std::optional<TopoDS_Edge> sidecarSourceEdgeExportShape;
        for (const std::size_t sidecarIndex :
             strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo)) {
            if (sidecarIndex >= info.edges.size()) {
                continue;
            }
            const EdgeInfo& sidecar = info.edges[sidecarIndex];
            if (!wireJoinerMapperHistoryProducerEvidenceReady(
                    materializationLedger,
                    sidecarIndex
                )) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::buildClosedWire() records producer evidence with
            // "aHistory->Remove(info.edge)". If a foreign Remove source blocks this result-wire candidate slot but
            // a strict same-lineage sidecar already has producer-ledger output, reuse that sidecar's producer
            // curve with the current result vertices. Prefer the already-shaped sidecar output,
            // then fall back to the exact EdgeInfo::edge that FreeCAD passed to aHistory.
            sidecarSourceEdgeExportShape
            = edgeWithProducerCurveAndResultVertices(
                resultWireProducerMapperHistoryInputEdge(sidecar, materializationLedger, sidecarIndex),
                resultEdge
            );
            if (!sidecarSourceEdgeExportShape || sidecarSourceEdgeExportShape->IsNull()) {
                sidecarSourceEdgeExportShape
                    = edgeWithProducerCurveAndResultVertices(sidecar.edge, resultEdge);
            }
            if (sidecarSourceEdgeExportShape && !sidecarSourceEdgeExportShape->IsNull()) {
                break;
            }
        }
        if (!sidecarSourceEdgeExportShape || sidecarSourceEdgeExportShape->IsNull()) {
            continue;
        }
        recordWireJoinerMapperHistoryProducerEvidence(
            materializationLedger,
            edgeInfoIndex,
            *sidecarSourceEdgeExportShape
        );
    }
}

void WireJoiner::Impl::buildFinalEdgeOwnership(
    const TopoDS_Shape* boundedFaceShape,
    const std::vector<TopoDS_Wire>* closedWires,
    const std::vector<TopoDS_Edge>* openEdges,
    bool splitProducedBoundedFaces
)
{
    historySummary_ = WireJoinerHistorySummary {};
    std::vector<TopoDS_Edge> inputEdges;
    std::vector<std::vector<std::size_t>> inputSourceEdgeIndices;
    for (const WireInfo& info : openWires_) {
        for (const EdgeInfo& edgeInfo : info.edges) {
            if (!edgeInfo.edge.IsNull()) {
                inputEdges.push_back(edgeInfo.edge);
                inputSourceEdgeIndices.push_back(edgeInfo.sourceEdgeIndices);
            }
        }
    }
    if (inputEdges.empty()) {
        return;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() keeps the original "sourceEdgeArray" / "sourceEdges" as the history
    // source set for ::getOpenWires(... MapperHistory(aHistory), {sourceEdges.begin(),
    // sourceEdges.end()}, op), while ::add()'s vmap replacement is an internal EdgeInfo vertex ledger.
    // Use sourceEdgeArray identity for split source lineage; sourceEdgeLedgerEdges_ remains only the
    // mutable vertex-identity ledger consumed by initializeEdgeInfo()/openWireCompound child output.
    const std::vector<TopoDS_Edge>& lineageSourceEdges =
        sourceEdges_.empty() ? inputEdges : sourceEdges_;
    const SplitEdgesResult splitResult =
        splitEdgesAtIntersections(inputEdges, lineageSourceEdges, inputSourceEdgeIndices);
    std::vector<TopoDS_Edge> splitEdges;
    splitEdges.reserve(splitResult.records.size());
    for (const SplitEdgeRecord& record : splitResult.records) {
        splitEdges.push_back(record.edge);
    }
    historySummary_ = splitResult.history;
    const std::vector<TopoDS_Face> boundedFaces = boundedFaceShape ? facesForShape(*boundedFaceShape)
                                                                   : std::vector<TopoDS_Face> {};
    const bool assignTightBoundOwners = splitProducedBoundedFaces || !openEdges || openEdges->empty();
    WireInfo finalInfo;
    finalInfo.id = nextWireInfoId_++;
    const std::vector<TopoDS_Wire> finalWires = wiresFromEdges(splitEdges);
    finalInfo.wire = finalWires.empty() ? TopoDS_Wire() : finalWires.front();
    finalInfo.edges.reserve(splitResult.records.size());
    for (std::size_t index = 0; index < splitResult.records.size(); ++index) {
        const SplitEdgeRecord& record = splitResult.records[index];
        EdgeInfo edgeInfo;
        initializeEdgeInfo(edgeInfo, record.edge);
        edgeInfo.sourceEdgeIndices = record.sourceEdgeIndices;
        edgeInfo.sourceLineageFromSplitterHistory = record.fromSplitterHistory;
        edgeInfo.splitFragmentSourceEdgeIndices = record.sourceEdgeIndices;
        edgeInfo.splitFragmentModifiedSourceEdgeIndices = record.modifiedSourceEdgeIndices;
        edgeInfo.splitFragmentGeneratedSourceEdgeIndices = record.generatedSourceEdgeIndices;
        edgeInfo.splitFragmentFromModifiedHistory = record.fromModifiedHistory;
        edgeInfo.splitFragmentFromGeneratedHistory = record.fromGeneratedHistory;
        edgeInfo.splitFragmentSourceLineageFromIdentityFallback
            = record.sourceLineageFromIdentityFallback;
        edgeInfo.splitFragmentSourceLineageFromSourceIdentityFallback
            = record.sourceLineageFromSourceIdentityFallback;
        edgeInfo.splitFragmentHistoryShapeGeometryBridge
            = record.sourceLineageFromHistoryShapeGeometryBridge;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges() replaces an EdgeInfo only when one source edge produces
        // multiple "aHistory->AddModified(split.intersectShape, newInfo.edge)" fragments. A single
        // Modified result is still an original open edge for noOriginal purposes; do not fall back to
        // endpoint-geometry comparison to decide this.
        edgeInfo.splitFromInputEdge = record.fromSplitterHistory
            && !record.fromSingleModifiedSourceEdge;
        finalInfo.edges.push_back(edgeInfo);
    }

    rebuildAdjacentList(finalInfo);
    recordSuperEdgeCandidates(finalInfo);
    markOpenLeafEdges(finalInfo);
    assignClosedWireOwners(finalInfo, assignTightBoundOwners);
    rebuildOrderedVertices(finalInfo);
    if (tightBound_ && !boundedFaces.empty()) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), calls "findClosedWires(true)",
        // "findTightBound()" and "exhaustTightBound()" before "openWireCompound" export.
        // Run the request-local branch/transfer ledger on the split EdgeInfo list before generated
        // open-export edges are added to the final WireJoiner EdgeInfo ledger.
        recordBranchSearchCandidates(finalInfo, boundedFaces);
        recordTightBoundLifecycle(finalInfo);
    }
    else {
        finalInfo.done = std::any_of(
            finalInfo.edges.begin(),
            finalInfo.edges.end(),
            [](const EdgeInfo& edgeInfo) { return edgeInfo.wireInfo != 0U; }
        );
    }
    const bool hasOpenWireOutput = std::any_of(
        finalInfo.edges.begin(),
        finalInfo.edges.end(),
        [this](const EdgeInfo& edgeInfo) { return edgeInfoExportsOpenWireCompound(edgeInfo); }
    );
    WireJoinerHistoryMaterializationLedger materializationLedger;
    if (boundedFaceShape && closedWires && openEdges) {
        materializationLedger = computeWireJoinerHistoryMaterializationLedger(
            finalInfo,
            *boundedFaceShape,
            *closedWires,
            *openEdges,
            splitProducedBoundedFaces,
            hasOpenWireOutput
        );
        finalInfo.closedWireCycleSplitLedgerSourceEdgeCount =
            materializationLedger.closedWireCycleSplitLedgerSourceEdgeCount;
        finalInfo.closedWireCycleSplitLedgerOpenExport =
            materializationLedger.closedWireCycleSplitLedgerOpenExport;
    }
    if (finalInfo.done) {
        recordExhaustTightBoundLifecycle(finalInfo);
        recordBuildClosedWireRemovalLifecycle(finalInfo);
        recordRepeatedSplitExhaustRerunLifecycle(finalInfo, boundedFaces, materializationLedger);
    }
    applyWireJoinerHistoryMaterialization(finalInfo, materializationLedger);
    attachResultWireProducerLedger(finalInfo, materializationLedger);

    rebuildAdjacentList(finalInfo);
    recordOpenWireCompoundLedger(finalInfo, materializationLedger);
    std::size_t openExportIndex = 0;
    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < finalInfo.edges.size(); ++edgeInfoIndex) {
        const EdgeInfo& edgeInfo = finalInfo.edges[edgeInfoIndex];
        const bool historyProducerChildWireSource =
            wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
                materializationLedger,
                edgeInfoIndex
            );
        const bool exportsOpenEdge =
            edgeInfoHasOpenWireCompoundLedgerSlot(edgeInfo, historyProducerChildWireSource);
        if (!exportsOpenEdge) {
            continue;
        }
        ++openExportIndex;
        WireJoinerOpenExportHistoryEntry entry;
        entry.openExportIndex = openExportIndex;
        entry.edgeInfoIndex = edgeInfoIndex;
        const auto childWireIt = std::find_if(
            finalInfo.openWireCompoundWires.begin(),
            finalInfo.openWireCompoundWires.end(),
            [&](const OpenWireCompoundWireInfo& childWire) {
                return childWire.edgeIndex == edgeInfoIndex;
            }
        );
        if (childWireIt != finalInfo.openWireCompoundWires.end()) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() first materializes open children with
            // "builder.Add(openWireCompound, info.wire())"; ::getOpenWires() then consumes that
            // compound with MapperHistory(aHistory). History exported to topo must therefore come
            // from the child-wire ledger, not by re-running an EdgeInfo output helper.
            entry.openExportWire = childWireIt->wire;
            const std::vector<TopoDS_Edge> childWireEdges = wireEdges(childWireIt->wire);
            if (childWireEdges.size() == 1U) {
                entry.openExportEdge = childWireEdges.front();
            }
            entry.openWireCompoundChildWireInfoIndex = static_cast<std::size_t>(
                childWireIt - finalInfo.openWireCompoundWires.begin()
            );
            entry.openWireCompoundExportSource = childWireIt->openExportSource;
            entry.openWireCompoundEdgeInfoIteration = childWireIt->edgeInfoIteration;
            entry.openWireCompoundEdgeInfoIteration2 = childWireIt->edgeInfoIteration2;
            entry.openWireCompoundOwnerWireInfo = childWireIt->ownerWireInfo;
            entry.openWireCompoundOwnerWireInfo2 = childWireIt->ownerWireInfo2;
            entry.openWireCompoundOpenLeafExport = childWireIt->openLeafIterationMinus3;
            entry.openWireCompoundUnownedOpenEdgeExport = childWireIt->unownedOpenEdge;
            entry.openWireCompoundRootCurrentMemberChildProducer =
                childWireIt->rootCurrentMemberChildProducer;
            entry.openWireCompoundChildShapeIdentityRecorded =
                childWireIt->childShapeIdentityRecorded;
            entry.openWireCompoundChildWireEdgeCount = childWireIt->childWireEdgeCount;
            entry.openWireCompoundChildWireVertexCount = childWireIt->childWireVertexCount;
            entry.openWireCompoundSourceEdgeIndices = childWireIt->sourceEdgeIndices;
            entry.openWireCompoundSourceLineageFromSplitterHistory =
                childWireIt->sourceLineageFromSplitterHistory;
            entry.openWireCompoundNoOriginalPurgeMatch = childWireIt->noOriginalPurgeMatch;
            entry.openWireCompoundNoOriginalPurgedByLedger =
                childWireIt->noOriginalPurgedByLedger;
            entry.openWireCompoundNoOriginalSharedSourceLedgerRecorded =
                childWireIt->noOriginalSharedSourceEdgeLedgerRecorded;
            entry.openWireCompoundNoOriginalSharedSourceEdgeCount =
                childWireIt->noOriginalSharedSourceEdgeCount;
            entry.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount =
                childWireIt->noOriginalSharedSourceMatchedEdgeCount;
            entry.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount =
                childWireIt->noOriginalSharedSourceUnmatchedEdgeCount;
            entry.openWireCompoundProducerLedgerWireBuilt =
                childWireIt->producerLedgerWireBuilt;
            entry.openWireCompoundProducerLedgerWireFromSourceVmap =
                childWireIt->producerLedgerWireFromSourceVmap;
            entry.openWireCompoundSourceVmapEndpointLedgerRecorded =
                childWireIt->sourceVmapEndpointLedgerRecorded;
            entry.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount =
                childWireIt->sourceVmapEndpointLedgerOutputVertexCount;
            entry.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount =
                childWireIt->sourceVmapEndpointLedgerMatchedVertexCount;
            entry.openWireCompoundEndpointProvenanceRecorded =
                childWireIt->endpointProvenanceRecorded;
            entry.openWireCompoundEndpointProvenanceOutputVertexCount =
                childWireIt->endpointProvenanceOutputVertexCount;
            entry.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount =
                childWireIt->endpointProvenanceSourceVmapMatchedVertexCount;
            entry.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount =
                childWireIt->endpointProvenanceVmapReplacementMatchedVertexCount;
            entry.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount =
                childWireIt->endpointProvenanceCandidateMatchedVertexCount;
            entry.openWireCompoundEndpointProvenanceUnmatchedVertexCount =
                childWireIt->endpointProvenanceUnmatchedVertexCount;
            entry.openWireCompoundVmapReplacementEvents =
                childWireIt->vmapReplacementEvents;
            entry.openWireCompoundVmapReplacementEventCount =
                childWireIt->vmapReplacementEventCount;
            entry.openWireCompoundCurrentMemberProducerOutput =
                childWireIt->currentMemberProducerOutput;
            entry.openWireCompoundCurrentMemberSplitLedgerVertexCandidate =
                childWireIt->currentMemberSplitLedgerVertexCandidate;
            entry.openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded =
                !childWireIt->currentMemberSplitLedgerOutputVertexDebt.empty();
            entry.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount =
                childWireIt->currentMemberSplitLedgerMemberVertices.size();
            entry.openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount =
                childWireIt->currentMemberSplitLedgerCandidateVertexCount;
            entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexCount =
                childWireIt->currentMemberSplitLedgerOutputVertexCount;
            entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount =
                childWireIt->currentMemberSplitLedgerOutputVertexDebt.size();
            entry.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount =
                childWireIt->currentMemberSplitLedgerOutputMatchedVertexCount;
            entry.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount =
                childWireIt->currentMemberSplitLedgerOutputCandidateMatchedVertexCount;
            entry.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount =
                childWireIt->currentMemberSplitLedgerOutputUnmatchedVertexCount;
            entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt.clear();
            entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt.reserve(
                childWireIt->currentMemberSplitLedgerOutputVertexDebt.size()
            );
            for (const auto& debt :
                 childWireIt->currentMemberSplitLedgerOutputVertexDebt) {
                WireJoinerEndpointIdentityDebt entryDebt;
                entryDebt.outputVertexIndex = debt.outputVertexIndex;
                entryDebt.matchedMemberSplitLedger = debt.matchedMemberSplitLedger;
                entryDebt.matchedCandidateLedger = debt.matchedCandidateLedger;
                entryDebt.currentChildWireOutputVertexMatchesOtherOutput =
                    debt.currentChildWireOutputVertexMatchesOtherOutput;
                entryDebt.candidateWireVertexMatchesOtherOutput =
                    debt.candidateWireVertexMatchesOtherOutput;
                entryDebt.explanation = debt.explanation;
                entryDebt.currentChildWireOutputVertexIdentity =
                    debt.currentChildWireOutputVertexIdentity;
                entryDebt.memberSplitLedgerVertexIdentity =
                    debt.memberSplitLedgerVertexIdentity;
                entryDebt.candidateWireVertexIdentity = debt.candidateWireVertexIdentity;
                entryDebt.mismatchReason = debt.mismatchReason;
                entry.openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt.push_back(
                    std::move(entryDebt)
                );
            }
            entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked =
                childWireIt->currentMemberSplitLedgerVertexMultiplicityBlocked;
            entry.sourceEdgeIndices = childWireIt->sourceEdgeIndices;
            entry.sourceLineageFromSplitterHistory =
                childWireIt->sourceLineageFromSplitterHistory;
            entry.splitFragmentSourceEdgeIndices = childWireIt->splitFragmentSourceEdgeIndices;
            entry.splitFragmentModifiedSourceEdgeIndices
                = childWireIt->splitFragmentModifiedSourceEdgeIndices;
            entry.splitFragmentGeneratedSourceEdgeIndices
                = childWireIt->splitFragmentGeneratedSourceEdgeIndices;
            entry.splitFragmentFromModifiedHistory = childWireIt->splitFragmentFromModifiedHistory;
            entry.splitFragmentFromGeneratedHistory = childWireIt->splitFragmentFromGeneratedHistory;
            entry.splitFragmentSourceLineageFromIdentityFallback
                = childWireIt->splitFragmentSourceLineageFromIdentityFallback;
            entry.splitFragmentSourceLineageFromSourceIdentityFallback
                = childWireIt->splitFragmentSourceLineageFromSourceIdentityFallback;
            entry.splitFragmentHistoryShapeGeometryBridge
                = childWireIt->splitFragmentHistoryShapeGeometryBridge;
            entry.sourceVertexIdentity = childWireIt->sourceVertexIdentity;
            entry.sourceVertexReplacementSourceEdgeIndices
                = childWireIt->sourceVertexReplacementSourceEdgeIndices;
            entry.sourceVertexReplacementEndpoints = childWireIt->sourceVertexReplacementEndpoints;
            entry.sourceVertexReplacementIdentity = childWireIt->sourceVertexReplacementIdentity;
            // FreeCAD:
            // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports result children through openWireCompound before
            // ::getOpenWires() maps them with MapperHistory(aHistory). Keep history producer identity
            // on the child-wire ledger boundary so topo evidence and summary entries do not diverge.
            entry.resultWireProducer = childWireIt->resultWireProducer;
            const std::vector<std::size_t>& relationSourceEdgeIndices =
                entry.openWireCompoundSourceEdgeIndices.empty()
                ? entry.sourceEdgeIndices
                : entry.openWireCompoundSourceEdgeIndices;
            const bool relationLineageFromSplitter =
                entry.openWireCompoundSourceEdgeIndices.empty()
                ? entry.sourceLineageFromSplitterHistory
                : entry.openWireCompoundSourceLineageFromSplitterHistory;
            entry.historyRelationFromChildWireLedger = true;
            if (entry.openWireCompoundNoOriginalPurgedByLedger) {
                entry.historyRelation = WireJoinerHistoryRelation::Deleted;
            }
            else if (
                entry.splitFragmentFromModifiedHistory || relationLineageFromSplitter
                || relationSourceEdgeIndices.size() > 1U
            ) {
                entry.historyRelation = WireJoinerHistoryRelation::Split;
            }
            else if (entry.splitFragmentFromGeneratedHistory) {
                entry.historyRelation = WireJoinerHistoryRelation::Generated;
            }
            else if (
                entry.resultWireProducer.kind == ResultWireProducerKind::ExistingSourceEdge
                || entry.resultWireProducer.kind == ResultWireProducerKind::None
            ) {
                entry.historyRelation = WireJoinerHistoryRelation::Preserved;
            }
            else {
                entry.historyRelation = WireJoinerHistoryRelation::Generated;
            }
            WireJoinerHistoryEvent event;
            event.eventIndex = historySummary_.historyEvents.size();
            event.openExportIndex = entry.openExportIndex;
            event.edgeInfoIndex = entry.edgeInfoIndex;
            event.openWireCompoundChildWireInfoIndex =
                entry.openWireCompoundChildWireInfoIndex;
            event.relation = entry.historyRelation;
            event.relationFromChildWireLedger = entry.historyRelationFromChildWireLedger;
            event.sourceEdgeIndices = relationSourceEdgeIndices;
            event.sourceLineageFromSplitterHistory = relationLineageFromSplitter;
            event.noOriginalPurgedByLedger = entry.openWireCompoundNoOriginalPurgedByLedger;
            event.splitFragmentFromModifiedHistory = entry.splitFragmentFromModifiedHistory;
            event.splitFragmentFromGeneratedHistory = entry.splitFragmentFromGeneratedHistory;
            entry.wireJoinerHistoryEventIndex = event.eventIndex;
            entry.wireJoinerHistoryEventFromChildWireLedger =
                event.relationFromChildWireLedger;
            childWireIt->wireJoinerHistoryEventIndex = event.eventIndex;
            if (event.relationFromChildWireLedger) {
                ++historySummary_.historyEventFromChildWireLedgerCount;
            }
            historySummary_.historyEvents.push_back(std::move(event));
        }
        else {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports only materialized "openWireCompound" children with
            // "builder.Add(openWireCompound, info.wire())". If the request-local mirror missed a
            // child-wire slot, keep that as an invariant diagnostic and do not reconstruct public
            // history output, source lineage, vertex replacement evidence, or producer identity from
            // EdgeInfo sidecars after the openWireCompound boundary.
            entry.missingOpenWireCompoundChildWire = true;
        }
        historySummary_.openExportEntries.push_back(std::move(entry));
    }
    openWires_.clear();
    openWires_.push_back(std::move(finalInfo));
}

void WireJoiner::Impl::initializeEdgeInfo(EdgeInfo& edgeInfo, const TopoDS_Edge& edge) const
{
    edgeInfo.edge = edge;
    edgeInfo.edgeReversed.Nullify();
    edgeInfo.superEdgeReversed.Nullify();
    edgeInfo.iStart = {-1, -1};
    edgeInfo.iEnd = {-1, -1};
    if (edge.IsNull()) {
        return;
    }
    const auto [start, end] = edgeEndpoints(edge);
    edgeInfo.p1 = start;
    edgeInfo.p2 = end;
    edgeInfo.mid = edgeMidpoint(edge);
    edgeInfo.sourceVertexIdentity = {
        vertexIsOriginalSourceByIdentity(TopExp::FirstVertex(edge), sourceEdges_),
        vertexIsOriginalSourceByIdentity(TopExp::LastVertex(edge), sourceEdges_),
    };
    const std::vector<TopoDS_Edge>& replacementSourceEdges =
        sourceEdgeLedgerEdges_.empty() ? sourceEdges_ : sourceEdgeLedgerEdges_;
    recordSourceVertexReplacementEvidence(
        TopExp::FirstVertex(edge),
        replacementSourceEdges,
        edgeInfo.sourceVertexReplacementSourceEdgeIndices[0],
        edgeInfo.sourceVertexReplacementEndpoints[0],
        edgeInfo.sourceVertexReplacementIdentity[0]
    );
    recordSourceVertexReplacementEvidence(
        TopExp::LastVertex(edge),
        replacementSourceEdges,
        edgeInfo.sourceVertexReplacementSourceEdgeIndices[1],
        edgeInfo.sourceVertexReplacementEndpoints[1],
        edgeInfo.sourceVertexReplacementIdentity[1]
    );
}

const TopoDS_Shape& WireJoiner::Impl::EdgeInfo::shape(bool forward) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo::shape(), "if (superEdge.IsNull())" returns edge/edgeReversed;
    // otherwise it returns superEdge/superEdgeReversed. This is the common entry used by
    // findSuperEdgesUpdateFirst(), bounds, debug output and openWireCompound construction.
    if (superEdge.IsNull()) {
        if (forward) {
            return edge;
        }
        if (edgeReversed.IsNull()) {
            edgeReversed = edge.Reversed();
        }
        return edgeReversed;
    }
    if (forward) {
        return superEdge;
    }
    if (superEdgeReversed.IsNull()) {
        superEdgeReversed = superEdge.Reversed();
    }
    return superEdgeReversed;
}

TopoDS_Wire WireJoiner::Impl::EdgeInfo::wire(bool forward) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo::wire(), "auto sForWire = shape(); if ... WIRE ... else
    // BRepBuilderAPI_MakeWire(TopoDS::Edge(sForWire)).Wire()". Keep this conversion local to
    // WireJoiner so future openWireCompound export can consume the same EdgeInfo shape contract.
    const TopoDS_Shape& shapeForWire = shape(forward);
    if (shapeForWire.IsNull()) {
        return TopoDS_Wire();
    }
    if (shapeForWire.ShapeType() == TopAbs_WIRE) {
        return TopoDS::Wire(shapeForWire);
    }
    if (shapeForWire.ShapeType() == TopAbs_EDGE) {
        return BRepBuilderAPI_MakeWire(TopoDS::Edge(shapeForWire)).Wire();
    }
    return TopoDS_Wire();
}

TopoDS_Wire WireJoiner::Impl::wireFromVertices(const WireInfo& info, const std::vector<WireVertex>& vertices) const
{
    BRepBuilderAPI_MakeWire builder;
    for (const WireVertex& vertex : vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        const TopoDS_Wire wire = info.edges[vertex.edgeIndex].wire(vertex.start);
        if (wire.IsNull()) {
            continue;
        }
        builder.Add(wire);
    }
    return builder.IsDone() ? builder.Wire() : TopoDS_Wire();
}

void WireJoiner::Impl::rebuildAdjacentList(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListPopulate(), fills EdgeInfo::iStart/iEnd ranges over the
    // request-local "adjacentList" so findTightBound()/exhaustTightBound() can walk connected
    // VertexInfo entries instead of scanning result geometry.
    info.adjacentVertices.clear();
    for (EdgeInfo& edge : info.edges) {
        edge.iStart = {-1, -1};
        edge.iEnd = {-1, -1};
    }

    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0) {
            continue;
        }
        const std::array<gp_Pnt, 2> endpoints {edge.p1, edge.p2};
        for (int endpointIndex = 0; endpointIndex < 2; ++endpointIndex) {
            if (edge.iStart[endpointIndex] >= 0) {
                continue;
            }
            const int rangeStart = static_cast<int>(info.adjacentVertices.size());
            for (std::size_t otherIndex = 0; otherIndex < info.edges.size(); ++otherIndex) {
                const EdgeInfo& other = info.edges[otherIndex];
                if (other.edge.IsNull() || other.iteration < 0) {
                    continue;
                }
                if (samePoint(other.p1, endpoints[endpointIndex])) {
                    info.adjacentVertices.push_back(WireVertex {otherIndex, true});
                }
                if (samePoint(other.p2, endpoints[endpointIndex])) {
                    info.adjacentVertices.push_back(WireVertex {otherIndex, false});
                }
            }
            const int rangeEnd = static_cast<int>(info.adjacentVertices.size());
            edge.iStart[endpointIndex] = rangeStart;
            edge.iEnd[endpointIndex] = rangeEnd;
            for (int adjacentIndex = rangeStart; adjacentIndex < rangeEnd; ++adjacentIndex) {
                const WireVertex& vertex = info.adjacentVertices[adjacentIndex];
                if (vertex.edgeIndex >= info.edges.size() || vertex.edgeIndex == edgeIndex) {
                    continue;
                }
                const int otherEndpointIndex = vertex.start ? 0 : 1;
                info.edges[vertex.edgeIndex].iStart[otherEndpointIndex] = rangeStart;
                info.edges[vertex.edgeIndex].iEnd[otherEndpointIndex] = rangeEnd;
            }
        }
    }
}

std::optional<WireJoiner::Impl::WireVertex> WireJoiner::Impl::soleActiveAdjacentEdge(
    const WireInfo& info,
    std::size_t edgeIndex,
    int endpointIndex
) const
{
    if (edgeIndex >= info.edges.size() || endpointIndex < 0 || endpointIndex > 1) {
        return std::nullopt;
    }
    const EdgeInfo& edge = info.edges[edgeIndex];
    if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
        return std::nullopt;
    }

    std::optional<WireVertex> found;
    for (int adjacentIndex = edge.iStart[endpointIndex]; adjacentIndex < edge.iEnd[endpointIndex];
         ++adjacentIndex) {
        if (adjacentIndex < 0
            || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
            continue;
        }
        const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
        if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
            continue;
        }
        const EdgeInfo& other = info.edges[adjacent.edgeIndex];
        if (other.edge.IsNull() || other.iteration < 0) {
            continue;
        }
        if (found) {
            return std::nullopt;
        }
        found = adjacent;
    }
    return found;
}

void WireJoiner::Impl::extendSuperEdgeCandidate(
    const WireInfo& info,
    std::deque<WireVertex>& vertices,
    std::vector<bool>& used,
    bool appendBack,
    bool& closed
) const
{
    while (!closed && !vertices.empty()) {
        const WireVertex current = appendBack ? vertices.back() : vertices.front();
        if (current.edgeIndex >= info.edges.size()) {
            return;
        }

        const int endpointIndex = appendBack ? (current.start ? 1 : 0) : (current.start ? 0 : 1);
        const std::optional<WireVertex> adjacent
            = soleActiveAdjacentEdge(info, current.edgeIndex, endpointIndex);
        if (!adjacent) {
            return;
        }
        if (adjacent->edgeIndex >= used.size()) {
            return;
        }
        if (used[adjacent->edgeIndex]) {
            closed = true;
            return;
        }

        used[adjacent->edgeIndex] = true;
        if (appendBack) {
            vertices.push_back(WireVertex {adjacent->edgeIndex, adjacent->start});
        }
        else {
            vertices.push_front(WireVertex {adjacent->edgeIndex, !adjacent->start});
        }
    }
}

void WireJoiner::Impl::recordSuperEdgeCandidates(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListSkipEdges(), calls findSuperEdges() before the
    // "Skip edges that are connected to only one end" loop when merge/tight-bound is enabled.
    // This records equivalent request-local candidate chains but does not yet replace
    // EdgeInfo::edge with EdgeInfo::superEdge or change openWireCompound export.
    info.superEdges.clear();
    for (EdgeInfo& edge : info.edges) {
        edge.superEdgeInfo = 0;
        edge.superEdgeMemberCount = 0;
        edge.superEdgeRoot = false;
        edge.superEdgeClosed = false;
        edge.superEdgeMaterialized = false;
        edge.superEdgeShadowedMember = false;
        edge.superEdgeLifecycleIteration = 0;
        edge.superEdgeLifecycleMemberMinusOne = false;
        edge.superEdgeLifecycleOpenRoot = false;
        edge.superEdgeLifecycleClosedRoot = false;
        edge.superEdgeAdjacentRangeRewritten = false;
        edge.superEdgeEndpointRewritten = false;
        edge.superEdgeEndpointRewriteIndex = -1;
        edge.superEdgeEndpointRewritePoint = {};
        edge.superEdgeAdjacentRangeSourceEdgeInfo = 0;
        edge.superEdgeAdjacentRangeSourceEndpoint = -1;
        edge.superEdgeAdjacentRangeStart = -1;
        edge.superEdgeAdjacentRangeEnd = -1;
        edge.edgeReversed.Nullify();
        edge.superEdgeReversed.Nullify();
        edge.superEdge.Nullify();
    }

    std::vector<bool> assigned(info.edges.size(), false);
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        if (assigned[edgeIndex]) {
            continue;
        }
        const EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0) {
            continue;
        }

        std::deque<WireVertex> vertices;
        std::vector<bool> used(info.edges.size(), false);
        vertices.push_back(WireVertex {edgeIndex, true});
        used[edgeIndex] = true;
        bool closed = false;
        extendSuperEdgeCandidate(info, vertices, used, false, closed);
        extendSuperEdgeCandidate(info, vertices, used, true, closed);
        if (vertices.size() <= 1U) {
            continue;
        }

        SuperEdgeInfo superEdge;
        superEdge.id = nextSuperEdgeId_++;
        superEdge.vertices.assign(vertices.begin(), vertices.end());
        superEdge.wire = wireFromVertices(info, superEdge.vertices);
        superEdge.materialized = !superEdge.wire.IsNull();
        superEdge.closed = superEdge.materialized ? BRep_Tool::IsClosed(superEdge.wire) : closed;
        const std::size_t memberCount = superEdge.vertices.size();
        const WireVertex rootVertex = superEdge.vertices.front();
        const WireVertex lastVertex = superEdge.vertices.back();
        const int rootRewriteEndpoint = rootVertex.start ? 1 : 0;
        const int sourceRangeEndpoint = lastVertex.start ? 1 : 0;
        const gp_Pnt rootRewritePoint = vertexOtherPoint(info, lastVertex);
        bool rootAssigned = false;
        for (const WireVertex& vertex : superEdge.vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            assigned[vertex.edgeIndex] = true;
            EdgeInfo& member = info.edges[vertex.edgeIndex];
            member.superEdgeInfo = superEdge.id;
            member.superEdgeMemberCount = memberCount;
            member.superEdgeClosed = superEdge.closed;
            if (superEdge.materialized) {
                member.superEdgeLifecycleIteration = -1;
                member.superEdgeLifecycleMemberMinusOne = true;
                member.iteration = -1;
            }
            if (!rootAssigned) {
                member.superEdgeRoot = true;
                member.superEdge = superEdge.wire;
                member.superEdgeReversed.Nullify();
                member.superEdgeMaterialized = superEdge.materialized;
                if (superEdge.materialized) {
                    member.superEdgeLifecycleMemberMinusOne = false;
                    if (superEdge.closed) {
                        member.superEdgeLifecycleIteration = -2;
                        member.superEdgeLifecycleClosedRoot = true;
                        member.iteration = -2;
                    }
                    else {
                        member.superEdgeLifecycleIteration = 1;
                        member.superEdgeLifecycleOpenRoot = true;
                        member.superEdgeAdjacentRangeRewritten = true;
                        member.superEdgeEndpointRewritten = true;
                        member.superEdgeEndpointRewriteIndex = rootRewriteEndpoint;
                        member.superEdgeEndpointRewritePoint = rootRewritePoint;
                        if (lastVertex.edgeIndex < info.edges.size()) {
                            const EdgeInfo& source = info.edges[lastVertex.edgeIndex];
                            member.superEdgeAdjacentRangeSourceEdgeInfo = lastVertex.edgeIndex + 1U;
                            member.superEdgeAdjacentRangeSourceEndpoint = sourceRangeEndpoint;
                            member.superEdgeAdjacentRangeStart = source.iStart[sourceRangeEndpoint];
                            member.superEdgeAdjacentRangeEnd = source.iEnd[sourceRangeEndpoint];
                        }
                        member.iteration = member.superEdgeLifecycleIteration;
                        if (member.superEdgeEndpointRewriteIndex == 0) {
                            member.p1 = member.superEdgeEndpointRewritePoint;
                        }
                        else if (member.superEdgeEndpointRewriteIndex == 1) {
                            member.p2 = member.superEdgeEndpointRewritePoint;
                        }
                        if (member.superEdgeAdjacentRangeStart >= 0
                            && member.superEdgeAdjacentRangeEnd >= 0) {
                            member.iStart[rootRewriteEndpoint] = member.superEdgeAdjacentRangeStart;
                            member.iEnd[rootRewriteEndpoint] = member.superEdgeAdjacentRangeEnd;
                            for (int adjacentIndex = member.superEdgeAdjacentRangeStart;
                                 adjacentIndex < member.superEdgeAdjacentRangeEnd;
                                 ++adjacentIndex) {
                                if (adjacentIndex < 0
                                    || static_cast<std::size_t>(adjacentIndex)
                                        >= info.adjacentVertices.size()) {
                                    continue;
                                }
                                WireVertex& adjacent
                                    = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                                if (adjacent.edgeIndex == lastVertex.edgeIndex) {
                                    adjacent.edgeIndex = rootVertex.edgeIndex;
                                    adjacent.start = !rootVertex.start;
                                }
                            }
                        }
                        const bool rootEndpointSourceIdentity = rootVertex.edgeIndex
                                < info.edges.size()
                            && info.edges[rootVertex.edgeIndex]
                                   .sourceVertexIdentity[rootVertex.start ? 0 : 1];
                        const bool rewrittenEndpointSourceIdentity = lastVertex.edgeIndex
                                < info.edges.size()
                            && info.edges[lastVertex.edgeIndex]
                                   .sourceVertexIdentity[lastVertex.start ? 1 : 0];
                        member.sourceVertexIdentity[rootVertex.start ? 0 : 1]
                            = rootEndpointSourceIdentity;
                        member.sourceVertexIdentity[rootRewriteEndpoint]
                            = rewrittenEndpointSourceIdentity;
                    }
                }
                rootAssigned = true;
            }
            else {
                member.superEdgeShadowedMember = superEdge.materialized;
            }
        }
        info.superEdges.push_back(std::move(superEdge));
    }
}

bool WireJoiner::Impl::markOpenLeafEdges(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListSkipEdges(), "Skip edges that are connected to only one end";
    // if no other active edge exists in an endpoint adjacent range, it writes "info.iteration = -3".
    // This runs before findClosedWires(true), so open leaf edges are exported by the same final
    // EdgeInfo condition used by ::WireJoinerP::build(): "iteration == -3 || (!wireInfo && ...)".
    bool changedAny = false;
    bool done = false;
    while (!done) {
        done = true;
        for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
            EdgeInfo& edge = info.edges[edgeIndex];
            if (edge.edge.IsNull() || edge.iteration < 0) {
                continue;
            }
            if (samePoint(edge.p1, edge.p2)) {
                continue;
            }
            for (int endpointIndex = 0; endpointIndex < 2; ++endpointIndex) {
                if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
                    continue;
                }
                bool hasActiveOther = false;
                for (int adjacentIndex = edge.iStart[endpointIndex];
                     adjacentIndex < edge.iEnd[endpointIndex];
                     ++adjacentIndex) {
                    if (adjacentIndex < 0
                        || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                        continue;
                    }
                    const WireVertex& adjacent
                        = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                    if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
                        continue;
                    }
                    const EdgeInfo& other = info.edges[adjacent.edgeIndex];
                    if (!other.edge.IsNull() && other.iteration >= 0) {
                        hasActiveOther = true;
                        break;
                    }
                }
                if (!hasActiveOther) {
                    edge.iteration = -3;
                    done = false;
                    changedAny = true;
                    break;
                }
            }
        }
    }
    return changedAny;
}

void WireJoiner::Impl::rebuildOrderedVertices(WireInfo& info)
{
    info.orderedVertices.clear();
    info.adjacentVertices.clear();
    info.hasNewWireSeed = false;
    info.hasSplitWireCandidate = false;
    info.done = false;
    info.splitWireCandidateCount = 0;
    info.ownerPropagationCandidateCount = 0;
    info.ownerPropagationUnassignedCandidateCount = 0;
    info.ownerPropagationOtherWireCandidateCount = 0;
    info.ownerPropagationOtherWireLiveEdgeInfoCount = 0;
    info.exhaustAdjacentSearchCount = 0;
    info.exhaustAdjacentSearchHitCount = 0;
    info.exhaustAdjacentSearchMissCount = 0;
    info.exhaustAdjacentSearchStackFrameCount = 0;
    info.exhaustAdjacentSearchVertexStackCount = 0;
    info.exhaustAdjacentSearchEdgeSetVisitCount = 0;
    info.exhaustAdjacentSearchBacktrackCount = 0;
    info.exhaustAdjacentWireSetInsertCount = 0;
    info.exhaustAdjacentWireSetEraseCount = 0;
    info.exhaustAdjacentWireSetAbortCount = 0;
    info.exhaustAdjacentWireInfo2AbortCount = 0;
    info.repeatedSplitExhaustCycleCount = 0;
    info.repeatedSplitExhaustRemovedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRemovedUnownedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRemovedSecondaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRemovedPrimaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunActiveEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunNoActiveSearchCount = 0;
    info.repeatedSplitExhaustRerunClosedWireSearchCount = 0;
    info.repeatedSplitExhaustRerunClosedWireMissCount = 0;
    info.repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunClosedWireInfoCount = 0;
    info.repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunClosedWireVertexCount = 0;
    info.repeatedSplitExhaustRerunResettableClosedWireInfoCount = 0;
    info.repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveClosedWireInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveClosedWireVertexCount = 0;
    info.repeatedSplitExhaustRerunLiveBranchSearchCandidateCount = 0;
    info.repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount = 0;
    info.repeatedSplitExhaustRerunLiveBranchSearchOutsideCandidateCount = 0;
    info.repeatedSplitExhaustRerunLiveTransferWireInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveTransferredOwnerEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLiveDoneWireInfoCount = 0;
    info.repeatedSplitExhaustRerunRemovalScanCount = 0;
    info.repeatedSplitExhaustRerunRemovalEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunRemovalUnownedEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunRemovalSecondaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunRemovalPrimaryEdgeInfoCount = 0;
    info.repeatedSplitExhaustRerunLoopExitNoRemovalCount = 0;
    info.repeatedSplitExhaustRerunBranchSearchCandidateCount = 0;
    info.repeatedSplitExhaustRerunBranchSearchInsideCandidateCount = 0;
    info.repeatedSplitExhaustRerunBranchSearchOutsideCandidateCount = 0;
    info.repeatedSplitExhaustRerunNewWireSeedCandidateCount = 0;
    for (OwnerWireInfo& owner : info.ownerWires) {
        owner.hasNewWireSeed = false;
        owner.hasSplitWireCandidate = false;
        owner.done = false;
        owner.splitWireId = 0;
        owner.purge = false;
        owner.exhaustVisited = false;
        owner.exhaustDone = false;
        owner.exhaustDiscardedByPurge = false;
        owner.splitWireCandidateCount = 0;
        owner.branchSearchCandidateCount = 0;
        owner.branchSearchInsideCandidateCount = 0;
        owner.branchSearchOutsideCandidateCount = 0;
        owner.tightBoundExistingWireSearchIdxVertexCount = 0;
        owner.tightBoundExistingWireSearchStackPosCount = 0;
        owner.tightBoundFullWireSetInsertCount = 0;
        owner.tightBoundFullWireSetEraseCount = 0;
        owner.tightBoundFullWireSetAbortCount = 0;
        owner.tightBoundFullWireSetPurgeCandidateCount = 0;
        owner.tightBoundFullWireSetBlockedTransferCount = 0;
        owner.tightBoundFullWireSetAbortSearchCount = 0;
        owner.tightBoundFullWireSetAbortResolvedByHitCount = 0;
        owner.tightBoundFullWireSetAbortBlockedSearchCount = 0;
        owner.branchCandidates.clear();
        owner.transferWires.clear();
        for (WireVertex& vertex : owner.vertices) {
            vertex.branchCandidateCount = 0;
        }
    }
    for (EdgeInfo& edge : info.edges) {
        edge.branchCandidateCount = 0;
        edge.branchInsideCandidateCount = 0;
        edge.branchOutsideCandidateCount = 0;
        edge.newWireSeedCandidateCount = 0;
        edge.splitWireCandidateCount = 0;
        edge.ownerPropagationCandidateCount = 0;
        edge.tightBoundOwnerTransferCandidate = false;
        edge.tightBoundTransferredOwner = false;
        edge.exhaustSeed = false;
        edge.exhaustSharedOwner = false;
        edge.exhaustDoneSecondary = false;
        edge.exhaustSearchCandidate = false;
    }
    std::vector<bool> used(info.edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < info.edges.size(); ++startIndex) {
        if (used[startIndex] || info.edges[startIndex].edge.IsNull()) {
            continue;
        }

        std::deque<WireVertex> component;
        component.push_back(WireVertex {startIndex, true});
        used[startIndex] = true;
        auto [currentStart, currentEnd] = edgeEndpoints(info.edges[startIndex].edge);

        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t index = 0; index < info.edges.size(); ++index) {
                if (used[index] || info.edges[index].edge.IsNull()) {
                    continue;
                }
                const auto [edgeStart, edgeEnd] = edgeEndpoints(info.edges[index].edge);
                if (samePoint(edgeStart, currentEnd)) {
                    component.push_back(WireVertex {index, true});
                    currentEnd = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentEnd)) {
                    component.push_back(WireVertex {index, false});
                    currentEnd = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentStart)) {
                    component.push_front(WireVertex {index, true});
                    currentStart = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeStart, currentStart)) {
                    component.push_front(WireVertex {index, false});
                    currentStart = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
            }
        }

        info.orderedVertices.insert(info.orderedVertices.end(), component.begin(), component.end());
    }

    rebuildAdjacentList(info);
    const int iteration2 = nextIteration2_++;
    for (const WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex < info.edges.size()) {
            info.edges[vertex.edgeIndex].iteration2 = iteration2;
        }
    }
}

std::optional<WireJoiner::Impl::ClosedWireSearchResult> WireJoiner::Impl::findClosedWirePath(
    const WireInfo& info,
    std::size_t beginEdgeIndex
) const
{
    if (beginEdgeIndex >= info.edges.size()) {
        return std::nullopt;
    }
    const EdgeInfo& beginInfo = info.edges[beginEdgeIndex];
    if (beginInfo.edge.IsNull() || beginInfo.iteration < 0) {
        return std::nullopt;
    }

    ClosedWireSearchResult result;
    result.vertices.push_back(WireVertex {beginEdgeIndex, true});
    if (samePoint(beginInfo.p1, beginInfo.p2)) {
        return result;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWires(), pushes StackInfo frames, fills vertexStack from the
    // current EdgeInfo adjacent range, tracks edgeSet to avoid self-intersections, and then builds
    // the closed WireInfo from beginInfo plus vertexStack[entry.iCurrent] for each stack frame.
    std::vector<bool> edgeSet(info.edges.size(), false);
    std::vector<WireVertex> vertexStack;
    std::vector<ClosedWireSearchFrame> stack;
    edgeSet[beginEdgeIndex] = true;

    const gp_Pnt pstart = beginInfo.p1;
    gp_Pnt pend = beginInfo.p2;
    std::size_t currentEdgeIndex = beginEdgeIndex;
    int currentEndpointIndex = 1;
    const std::size_t stackEnd = stack.size();

    while (true) {
        if (currentEdgeIndex >= info.edges.size()) {
            return std::nullopt;
        }
        const EdgeInfo& current = info.edges[currentEdgeIndex];
        ClosedWireSearchFrame frame;
        frame.start = vertexStack.size();
        frame.current = frame.start;
        frame.end = frame.start;
        const std::size_t originalVertexStackSize = vertexStack.size();
        if (currentEndpointIndex >= 0 && currentEndpointIndex < 2
            && current.iStart[currentEndpointIndex] >= 0
            && current.iEnd[currentEndpointIndex] >= current.iStart[currentEndpointIndex]) {
            for (int adjacentIndex = current.iStart[currentEndpointIndex];
                 adjacentIndex < current.iEnd[currentEndpointIndex];
                 ++adjacentIndex) {
                if (adjacentIndex < 0
                    || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                    continue;
                }
                const WireVertex& adjacent
                    = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                if (adjacent.edgeIndex >= info.edges.size()
                    || adjacent.edgeIndex == currentEdgeIndex) {
                    continue;
                }
                const EdgeInfo& candidate = info.edges[adjacent.edgeIndex];
                if (candidate.edge.IsNull() || candidate.iteration < 0) {
                    continue;
                }
                if (edgeSet[adjacent.edgeIndex]) {
                    ++result.intersectSkipCount;
                    frame.end = frame.start;
                    vertexStack.resize(originalVertexStackSize);
                    break;
                }
                vertexStack.push_back(adjacent);
                ++frame.end;
                ++result.vertexStackCount;
            }
        }
        stack.push_back(frame);
        ++result.stackFrameCount;

        bool selected = false;
        while (!stack.empty()) {
            ClosedWireSearchFrame& currentFrame = stack.back();
            if (currentFrame.current < currentFrame.end) {
                const WireVertex& currentVertex = vertexStack[currentFrame.current];
                currentEdgeIndex = currentVertex.edgeIndex;
                pend = vertexOtherPoint(info, currentVertex);
                currentEndpointIndex = currentVertex.start ? 1 : 0;
                if (currentEdgeIndex < edgeSet.size() && !edgeSet[currentEdgeIndex]) {
                    edgeSet[currentEdgeIndex] = true;
                    ++result.edgeSetVisitCount;
                }
                selected = true;
                break;
            }

            vertexStack.erase(
                vertexStack.begin() + static_cast<long>(currentFrame.start),
                vertexStack.end()
            );
            stack.pop_back();
            ++result.backtrackCount;
            if (stack.size() == stackEnd) {
                return std::nullopt;
            }

            ClosedWireSearchFrame& previousFrame = stack.back();
            if (previousFrame.current < vertexStack.size()) {
                const WireVertex& lastVertex = vertexStack[previousFrame.current];
                if (lastVertex.edgeIndex < edgeSet.size()) {
                    edgeSet[lastVertex.edgeIndex] = false;
                }
            }
            ++previousFrame.current;
        }
        if (!selected) {
            return std::nullopt;
        }
        if (!samePoint(pstart, pend)) {
            continue;
        }

        result.vertices.clear();
        result.vertices.push_back(WireVertex {beginEdgeIndex, true});
        for (const ClosedWireSearchFrame& selectedFrame : stack) {
            if (selectedFrame.current < vertexStack.size()) {
                result.vertices.push_back(vertexStack[selectedFrame.current]);
            }
        }
        return result;
    }
}

std::size_t WireJoiner::Impl::assignClosedWireOwners(WireInfo& info, bool assignOwners)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findClosedWires(true), for each unowned EdgeInfo, calls
    // "_findClosedWires(beginVertex, currentVertex)" and then assigns "beginInfo.wireInfo" to
    // every EdgeInfo reached through "stack". This is the first cad-core owner source before
    // findTightBound()/exhaustTightBound() split or add secondary owners.
    if (!assignOwners) {
        return 0;
    }

    std::size_t assigned = 0;
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        EdgeInfo& beginInfo = info.edges[edgeIndex];
        if (beginInfo.edge.IsNull() || beginInfo.iteration < 0 || beginInfo.wireInfo != 0U) {
            continue;
        }
        const std::optional<ClosedWireSearchResult> search = findClosedWirePath(info, edgeIndex);
        if (!search) {
            continue;
        }

        const std::size_t owner = nextWireInfoId_++;
        OwnerWireInfo ownerInfo;
        ownerInfo.id = owner;
        ownerInfo.vertices = search->vertices;
        ownerInfo.wire = wireFromVertices(info, ownerInfo.vertices);
        ownerInfo.closedWireSearchStackFrameCount = search->stackFrameCount;
        ownerInfo.closedWireSearchVertexStackCount = search->vertexStackCount;
        ownerInfo.closedWireSearchEdgeSetVisitCount = search->edgeSetVisitCount;
        ownerInfo.closedWireSearchBacktrackCount = search->backtrackCount;
        ownerInfo.closedWireSearchIntersectSkipCount = search->intersectSkipCount;
        for (const WireVertex& vertex : search->vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& edgeInfo = info.edges[vertex.edgeIndex];
            if (edgeInfo.iteration < 0 || edgeInfo.wireInfo != 0U) {
                continue;
            }
            edgeInfo.wireInfo = owner;
            edgeInfo.closedWireOwner = true;
            ++assigned;
        }
        info.ownerWires.push_back(std::move(ownerInfo));
    }
    return assigned;
}

gp_Pnt WireJoiner::Impl::vertexPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p1 : edge.p2;
}

gp_Pnt WireJoiner::Impl::vertexOtherPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p2 : edge.p1;
}

std::optional<std::size_t> WireJoiner::Impl::ownerVertexIndex(
    const OwnerWireInfo& owner,
    const WireVertex& vertex
) const
{
    for (std::size_t index = 0; index < owner.vertices.size(); ++index) {
        const WireVertex& ownerVertex = owner.vertices[index];
        if (ownerVertex.edgeIndex == vertex.edgeIndex && ownerVertex.start == vertex.start) {
            return index;
        }
    }
    return std::nullopt;
}

WireJoiner::Impl::TightBoundExistingWireSearchTrace WireJoiner::Impl::traceExistingWireSearchForCandidate(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const TightBoundBranchCandidate& candidate
) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWiresWithExisting(), called from
    // ::findTightBoundByVertices(), checks whether the adjacent-list search has reached an
    // existing "wireInfo" vertex, the reversed vertex, or an already visited edgeSet entry before
    // it decides idxVertex/stackPos or marks "wireInfo->purge = true".
    TightBoundExistingWireSearchTrace trace;
    if (!candidate.inside || !candidate.transfersOwnerEdge
        || candidate.adjacentVertex.edgeIndex >= info.edges.size()) {
        return trace;
    }

    std::vector<bool> edgeSet(info.edges.size(), false);
    edgeSet[candidate.adjacentVertex.edgeIndex] = true;
    ++trace.edgeSetVisitCount;

    std::vector<std::size_t> wireSet;
    const EdgeInfo& seedEdge = info.edges[candidate.adjacentVertex.edgeIndex];
    if (seedEdge.wireInfo != 0U) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::exhaustTightBoundWithAdjacent() seeds "wireSet.insert(next->wireInfo.get())"
        // before calling _findClosedWires(); _findClosedWiresUpdateEdges() then inserts each
        // current edge owner and erases it during backtracking.
        wireSet.push_back(seedEdge.wireInfo);
        ++trace.fullWireSetInsertCount;
    }
    const auto wireSetContains = [&](std::size_t ownerId) {
        return ownerId != 0U && std::find(wireSet.begin(), wireSet.end(), ownerId) != wireSet.end();
    };

    std::vector<WireVertex> currentPath;
    currentPath.push_back(candidate.adjacentVertex);
    std::function<bool(const WireVertex&, std::size_t)> visit = [&](const WireVertex& currentVertex,
                                                                    std::size_t depth) -> bool {
        if (currentVertex.edgeIndex >= info.edges.size() || depth > info.edges.size()) {
            return false;
        }
        const EdgeInfo& current = info.edges[currentVertex.edgeIndex];
        const int endpointIndex = currentVertex.start ? 1 : 0;
        if (endpointIndex < 0 || endpointIndex > 1 || current.iStart[endpointIndex] < 0
            || current.iEnd[endpointIndex] < current.iStart[endpointIndex]) {
            return false;
        }

        ++trace.stackFrameCount;
        for (int adjacentIndex = current.iStart[endpointIndex];
             adjacentIndex < current.iEnd[endpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0
                || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex >= info.edges.size()
                || adjacent.edgeIndex == currentVertex.edgeIndex) {
                continue;
            }
            const EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.edge.IsNull() || next.iteration < 0) {
                continue;
            }

            if (!wireSet.empty() && wireSetContains(next.wireInfo)) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::_findClosedWiresUpdateStack(), "if (!wireSet.empty() &&
                // wireSet.contains(info.wireInfo.get()))" aborts this branch and, when searching
                // with an existing wireInfo, marks "wireInfo->purge = true". This remains a trace
                // sidecar until full repeated split/exhaust is live.
                ++trace.fullWireSetAbortCount;
                ++trace.fullWireSetPurgeCandidateCount;
                continue;
            }

            if (const std::optional<std::size_t> ownerIndex = ownerVertexIndex(owner, adjacent)) {
                if (*ownerIndex != 0U) {
                    trace.hit = true;
                    trace.idxVertex = static_cast<int>(*ownerIndex) - 1;
                    trace.stackPos = static_cast<int>(trace.stackFrameCount) - 1;
                    trace.hitPath = currentPath;
                    trace.hitPath.push_back(adjacent);
                    ++trace.vertexStackCount;
                    return true;
                }
            }

            WireVertex reversed = adjacent;
            reversed.start = !reversed.start;
            if (const std::optional<std::size_t> reverseIndex = ownerVertexIndex(owner, reversed)) {
                if (*reverseIndex != 0U) {
                    trace.reverseHit = true;
                    trace.purge = true;
                    ++trace.vertexStackCount;
                    return true;
                }
            }

            if (edgeSet[adjacent.edgeIndex]) {
                ++trace.intersectSkipCount;
                continue;
            }

            edgeSet[adjacent.edgeIndex] = true;
            ++trace.edgeSetVisitCount;
            currentPath.push_back(adjacent);
            bool insertedWireSetOwner = false;
            if (!wireSet.empty() && next.wireInfo != 0U) {
                wireSet.push_back(next.wireInfo);
                insertedWireSetOwner = true;
                ++trace.fullWireSetInsertCount;
            }
            ++trace.vertexStackCount;
            if (visit(adjacent, depth + 1U)) {
                return true;
            }
            currentPath.pop_back();
            if (insertedWireSetOwner) {
                wireSet.pop_back();
                ++trace.fullWireSetEraseCount;
            }
            edgeSet[adjacent.edgeIndex] = false;
            ++trace.backtrackCount;
        }
        return false;
    };

    visit(candidate.adjacentVertex, 0U);
    return trace;
}

bool WireJoiner::Impl::findTightBoundBranchPathToPoint(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const gp_Pnt& current,
    const gp_Pnt& target,
    std::vector<bool>& usedEdges,
    std::vector<WireVertex>& path
) const
{
    return findBranchPathToPointSkippingOwner(info, owner.id, current, target, usedEdges, path);
}

bool WireJoiner::Impl::findBranchPathToPointSkippingOwner(
    const WireInfo& info,
    std::size_t skipOwnerId,
    const gp_Pnt& current,
    const gp_Pnt& target,
    std::vector<bool>& usedEdges,
    std::vector<WireVertex>& path
) const
{
    if (samePoint(current, target)) {
        return true;
    }

    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        if (edgeIndex >= usedEdges.size() || usedEdges[edgeIndex]) {
            continue;
        }
        const EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0 || edge.wireInfo == skipOwnerId) {
            continue;
        }

        std::optional<WireVertex> nextVertex;
        gp_Pnt nextPoint;
        if (samePoint(edge.p1, current)) {
            nextVertex = WireVertex {edgeIndex, true, 0U};
            nextPoint = edge.p2;
        }
        else if (samePoint(edge.p2, current)) {
            nextVertex = WireVertex {edgeIndex, false, 0U};
            nextPoint = edge.p1;
        }
        else {
            continue;
        }

        usedEdges[edgeIndex] = true;
        path.push_back(*nextVertex);
        if (findBranchPathToPointSkippingOwner(info, skipOwnerId, nextPoint, target, usedEdges, path)) {
            return true;
        }
        path.pop_back();
        usedEdges[edgeIndex] = false;
    }
    return false;
}

std::optional<WireJoiner::Impl::TightBoundTransferPath> WireJoiner::Impl::tightBoundTransferPathForCandidate(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const TightBoundBranchCandidate& candidate
) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices(), calls
    // "_findClosedWires(beginVertex, currentVertex, &idxEnd, beginInfo.wireInfo, &stackPos)"
    // when the inside branch does not immediately return to "pstart"; if no closed branch path
    // is found, the candidate is discarded instead of becoming a new WireInfo.
    if (!candidate.inside || !candidate.transfersOwnerEdge || owner.vertices.empty()) {
        return std::nullopt;
    }

    const auto ownerVertexIt
        = std::find_if(owner.vertices.begin(), owner.vertices.end(), [&](const WireVertex& vertex) {
              return vertex.edgeIndex == candidate.ownerVertex.edgeIndex
                  && vertex.start == candidate.ownerVertex.start;
          });
    if (ownerVertexIt == owner.vertices.end()) {
        return std::nullopt;
    }

    std::vector<WireVertex> transferVertices;
    transferVertices.push_back(owner.vertices.front());
    for (auto it = std::next(owner.vertices.begin()); it <= ownerVertexIt; ++it) {
        transferVertices.push_back(*it);
    }
    transferVertices.push_back(candidate.adjacentVertex);

    std::vector<bool> usedEdges(info.edges.size(), false);
    for (const WireVertex& vertex : transferVertices) {
        if (vertex.edgeIndex < usedEdges.size()) {
            usedEdges[vertex.edgeIndex] = true;
        }
    }

    const gp_Pnt target = vertexPoint(info, owner.vertices.front());
    const gp_Pnt current = vertexOtherPoint(info, candidate.adjacentVertex);
    std::vector<WireVertex> branchPath;
    if (!findTightBoundBranchPathToPoint(info, owner, current, target, usedEdges, branchPath)) {
        return std::nullopt;
    }
    transferVertices.insert(transferVertices.end(), branchPath.begin(), branchPath.end());

    TightBoundTransferPath path;
    path.transferVertices = std::move(transferVertices);

    for (auto it = std::next(ownerVertexIt); it != owner.vertices.end(); ++it) {
        path.splitOwnerVertices.push_back(*it);
    }
    path.splitWireVertices = path.splitOwnerVertices;

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundWithSplit(), after moving the remaining old-owner vertices
    // into splitWire, appends "vertexStack[stack[i].iCurrent]" from stackPos down to stackStart
    // as reversed vertices. The branch stack here is the adjacent branch plus the closed path
    // returned by _findClosedWires().
    std::vector<WireVertex> branchStack;
    branchStack.push_back(candidate.adjacentVertex);
    branchStack.insert(branchStack.end(), branchPath.begin(), branchPath.end());
    for (auto it = branchStack.rbegin(); it != branchStack.rend(); ++it) {
        WireVertex reversed = *it;
        reversed.start = !reversed.start;
        path.splitWireVertices.push_back(reversed);
    }
    return path;
}

std::optional<WireJoiner::Impl::TightBoundTransferPath> WireJoiner::Impl::tightBoundTransferPathForExistingWireHit(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const TightBoundBranchCandidate& candidate,
    const TightBoundExistingWireSearchTrace& trace,
    TightBoundExistingWirePathBlockReason* blockReason
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWiresWithExisting() writes "idxVertex" and "stackPos" when the
    // branch search reaches the existing "wireInfo"; ::findTightBoundSplitWire() then moves
    // owner vertices from the current idxV up to idxEnd into "splitWire".
    if (blockReason != nullptr) {
        *blockReason = TightBoundExistingWirePathBlockReason::None;
    }
    if (!trace.hit || trace.idxVertex < 0 || trace.stackPos < 0 || trace.hitPath.empty()) {
        return std::nullopt;
    }
    const auto ownerVertexIt
        = std::find_if(owner.vertices.begin(), owner.vertices.end(), [&](const WireVertex& vertex) {
              return vertex.edgeIndex == candidate.ownerVertex.edgeIndex
                  && vertex.start == candidate.ownerVertex.start;
          });
    if (ownerVertexIt == owner.vertices.end()) {
        if (blockReason != nullptr) {
            *blockReason = TightBoundExistingWirePathBlockReason::OwnerVertexMissing;
        }
        return std::nullopt;
    }
    const std::size_t ownerVertexIndex = static_cast<std::size_t>(
        std::distance(owner.vertices.begin(), ownerVertexIt)
    );
    const std::size_t hitIndex = static_cast<std::size_t>(trace.idxVertex);
    if (hitIndex >= owner.vertices.size() || ownerVertexIndex + 1U > hitIndex) {
        if (blockReason != nullptr) {
            *blockReason = TightBoundExistingWirePathBlockReason::OrderBlocked;
        }
        return std::nullopt;
    }

    TightBoundTransferPath path;
    path.transferVertices.push_back(owner.vertices.front());
    path.transferVertices
        .insert(path.transferVertices.end(), trace.hitPath.begin(), trace.hitPath.end());
    for (std::size_t index = hitIndex + 1U; index < owner.vertices.size(); ++index) {
        path.transferVertices.push_back(owner.vertices[index]);
    }
    if (wireFromVertices(info, path.transferVertices).IsNull()) {
        if (blockReason != nullptr) {
            *blockReason = TightBoundExistingWirePathBlockReason::WireBuildBlocked;
        }
        return std::nullopt;
    }

    for (std::size_t index = ownerVertexIndex + 1U; index < hitIndex; ++index) {
        path.splitOwnerVertices.push_back(owner.vertices[index]);
    }
    path.splitWireVertices = path.splitOwnerVertices;
    for (auto it = trace.hitPath.rbegin(); it != trace.hitPath.rend(); ++it) {
        WireVertex reversed = *it;
        reversed.start = !reversed.start;
        path.splitWireVertices.push_back(reversed);
    }
    path.existingWireHit = true;
    path.existingWireIdxVertex = trace.idxVertex;
    path.existingWireStackPos = trace.stackPos;
    return path;
}

bool WireJoiner::Impl::isDoneOwner(const WireInfo& info, std::size_t ownerId) const
{
    if (ownerId == 0U) {
        return false;
    }
    for (const OwnerWireInfo& owner : info.ownerWires) {
        if (owner.id == ownerId) {
            return owner.done && !owner.exhaustDiscardedByPurge;
        }
        if (owner.splitWireId != 0U && owner.splitWireId == ownerId) {
            return owner.done && !owner.exhaustDiscardedByPurge;
        }
        for (const TightBoundTransferWire& transfer : owner.transferWires) {
            if (transfer.id == ownerId) {
                return transfer.done;
            }
        }
    }
    return info.done && ownerId == info.id;
}

void WireJoiner::Impl::recordExhaustOwnerVertex(WireInfo& info, const WireVertex& vertex, std::size_t ownerId)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBound(), first loop:
    // "if (edgeInfo->wireInfo != info.wireInfo) edgeInfo->wireInfo2 = info.wireInfo".
    // This is the request-local secondary-owner ledger; edges still missing "wireInfo2" remain
    // an explicit exhaustTightBound() migration gap instead of being filled from graph degree.
    if (ownerId == 0U || vertex.edgeIndex >= info.edges.size()) {
        return;
    }
    EdgeInfo& edge = info.edges[vertex.edgeIndex];
    if (edge.wireInfo == ownerId) {
        edge.exhaustSeed = true;
        if (edge.wireInfo2 != 0U) {
            edge.exhaustSharedOwner = true;
            edge.exhaustDoneSecondary = true;
        }
        return;
    }
    if (edge.wireInfo == 0U) {
        return;
    }
    if (edge.wireInfo2 == 0U) {
        edge.wireInfo2 = ownerId;
        edge.exhaustSecondaryOwner = true;
    }
    edge.exhaustSharedOwner = true;
    edge.exhaustDoneSecondary = true;
}

WireJoiner::Impl::ExhaustAdjacentSearchTrace WireJoiner::Impl::traceExhaustAdjacentSearch(
    const WireInfo& info,
    const WireVertex& beginVertex,
    const WireVertex& adjacentVertex,
    std::size_t seedOwnerId
) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBoundWithAdjacent(), seeds "edgeSet.insert(next)" and
    // "wireSet.insert(next->wireInfo.get())", then calls _findClosedWires(beginVertex,
    // currentVertex). ::_findClosedWiresUpdateStack() aborts branches when "wireSet.contains(...)"
    // or "currentInfo->wireInfo2" is true.
    ExhaustAdjacentSearchTrace trace;
    if (beginVertex.edgeIndex >= info.edges.size() || adjacentVertex.edgeIndex >= info.edges.size()) {
        return trace;
    }

    const gp_Pnt target = vertexPoint(info, beginVertex);
    std::vector<bool> edgeSet(info.edges.size(), false);
    edgeSet[adjacentVertex.edgeIndex] = true;
    ++trace.edgeSetVisitCount;

    std::vector<std::size_t> wireSet;
    if (seedOwnerId != 0U) {
        wireSet.push_back(seedOwnerId);
        ++trace.wireSetInsertCount;
    }
    const auto wireSetContains = [&](std::size_t ownerId) {
        return ownerId != 0U && std::find(wireSet.begin(), wireSet.end(), ownerId) != wireSet.end();
    };

    std::function<bool(const WireVertex&, std::size_t)> visit = [&](const WireVertex& currentVertex,
                                                                    std::size_t depth) -> bool {
        if (currentVertex.edgeIndex >= info.edges.size() || depth > info.edges.size()) {
            return false;
        }
        if (samePoint(vertexOtherPoint(info, currentVertex), target)) {
            trace.hit = true;
            return true;
        }

        const EdgeInfo& current = info.edges[currentVertex.edgeIndex];
        const int endpointIndex = currentVertex.start ? 1 : 0;
        if (endpointIndex < 0 || endpointIndex > 1 || current.iStart[endpointIndex] < 0
            || current.iEnd[endpointIndex] < current.iStart[endpointIndex]) {
            return false;
        }

        ++trace.stackFrameCount;
        for (int adjacentIndex = current.iStart[endpointIndex];
             adjacentIndex < current.iEnd[endpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0
                || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& nextVertex
                = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (nextVertex.edgeIndex >= info.edges.size()
                || nextVertex.edgeIndex == currentVertex.edgeIndex) {
                continue;
            }
            const EdgeInfo& next = info.edges[nextVertex.edgeIndex];
            if (next.edge.IsNull() || next.iteration < 0) {
                continue;
            }
            if (edgeSet[nextVertex.edgeIndex]) {
                continue;
            }
            if (!wireSet.empty() && wireSetContains(next.wireInfo)) {
                ++trace.wireSetAbortCount;
                continue;
            }
            if (current.wireInfo2 != 0U) {
                ++trace.wireInfo2AbortCount;
                continue;
            }

            edgeSet[nextVertex.edgeIndex] = true;
            ++trace.edgeSetVisitCount;
            bool insertedWireSetOwner = false;
            if (!wireSet.empty() && next.wireInfo != 0U) {
                wireSet.push_back(next.wireInfo);
                insertedWireSetOwner = true;
                ++trace.wireSetInsertCount;
            }
            ++trace.vertexStackCount;
            if (visit(nextVertex, depth + 1U)) {
                return true;
            }
            if (insertedWireSetOwner) {
                wireSet.pop_back();
                ++trace.wireSetEraseCount;
            }
            edgeSet[nextVertex.edgeIndex] = false;
            ++trace.backtrackCount;
        }
        return false;
    };

    visit(adjacentVertex, 0U);
    return trace;
}

void WireJoiner::Impl::recordExhaustAdjacentSecondaryOwners(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBoundWithAdjacent(), for an edge whose second owner is still
    // missing, checks adjacent edges with a done "wireInfo" and searches a second tight-bound
    // wire from that branch. cad-core records the same done-owner adjacency as the next
    // request-local step before the full stack/wireSet search is migrated.
    for (EdgeInfo& edge : info.edges) {
        if (edge.edge.IsNull() || edge.iteration < 0 || edge.wireInfo == 0U || edge.wireInfo2 != 0U
            || !isDoneOwner(info, edge.wireInfo)) {
            continue;
        }
        const std::size_t edgeIndex = static_cast<std::size_t>(&edge - info.edges.data());
        for (int endpointIndex = 0; endpointIndex < 2 && edge.wireInfo2 == 0U; ++endpointIndex) {
            if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
                continue;
            }
            const WireVertex beginVertex {edgeIndex, endpointIndex == 0, 0U};
            for (int adjacentIndex = edge.iStart[endpointIndex];
                 adjacentIndex < edge.iEnd[endpointIndex];
                 ++adjacentIndex) {
                if (adjacentIndex < 0
                    || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                    continue;
                }
                const WireVertex& adjacent
                    = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
                    continue;
                }
                const EdgeInfo& candidate = info.edges[adjacent.edgeIndex];
                if (candidate.edge.IsNull() || candidate.iteration < 0 || candidate.wireInfo == 0U
                    || candidate.wireInfo == edge.wireInfo || candidate.wireInfo2 != 0U
                    || !isDoneOwner(info, candidate.wireInfo)) {
                    continue;
                }
                const ExhaustAdjacentSearchTrace trace
                    = traceExhaustAdjacentSearch(info, beginVertex, adjacent, candidate.wireInfo);
                ++info.exhaustAdjacentSearchCount;
                info.exhaustAdjacentSearchStackFrameCount += trace.stackFrameCount;
                info.exhaustAdjacentSearchVertexStackCount += trace.vertexStackCount;
                info.exhaustAdjacentSearchEdgeSetVisitCount += trace.edgeSetVisitCount;
                info.exhaustAdjacentSearchBacktrackCount += trace.backtrackCount;
                info.exhaustAdjacentWireSetInsertCount += trace.wireSetInsertCount;
                info.exhaustAdjacentWireSetEraseCount += trace.wireSetEraseCount;
                info.exhaustAdjacentWireSetAbortCount += trace.wireSetAbortCount;
                info.exhaustAdjacentWireInfo2AbortCount += trace.wireInfo2AbortCount;
                if (!trace.hit) {
                    ++info.exhaustAdjacentSearchMissCount;
                    continue;
                }
                ++info.exhaustAdjacentSearchHitCount;
                edge.wireInfo2 = candidate.wireInfo;
                edge.exhaustSecondaryOwner = true;
                edge.exhaustSharedOwner = true;
                edge.exhaustDoneSecondary = true;
                edge.exhaustSearchCandidate = true;
                break;
            }
        }
    }
}

void WireJoiner::Impl::recordExhaustTightBoundLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBound() first visits edges whose "wireInfo->done" is true,
    // copies a completed primary owner into "wireInfo2" for vertices owned by a different
    // WireInfo, skips edges where "wireInfo2 && wireInfo2->done", and otherwise calls
    // exhaustTightBoundUpdateWire() to search for the second tight-bound owner. This keeps
    // those request-local phases explicit before cad-core replaces the bounded classifier.
    if (!info.ownerWires.empty()) {
        for (OwnerWireInfo& owner : info.ownerWires) {
            if (owner.done) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::exhaustTightBoundUpdateEdge() consumes "wireInfo->purge"
                // after scanning all vertices: "wireInfo.reset()" for purge, otherwise
                // "wireInfo->done = true". cad-core now suppresses the discarded owner from
                // done2 seed/search/removal while leaving the primary EdgeInfo owner intact
                // until full repeated split/exhaust can replace the generated result-wire bridge.
                owner.exhaustVisited = true;
                const std::vector<WireVertex>& ownerVertices = owner.splitOwnerVertices.empty()
                    ? owner.vertices
                    : owner.splitOwnerVertices;
                const std::size_t exhaustOwnerId = owner.splitWireId == 0U ? owner.id
                                                                           : owner.splitWireId;
                if (owner.purge) {
                    owner.exhaustDiscardedByPurge = true;
                    owner.exhaustDone = false;
                    continue;
                }
                else {
                    owner.exhaustDone = true;
                }
                for (const WireVertex& vertex : ownerVertices) {
                    recordExhaustOwnerVertex(info, vertex, exhaustOwnerId);
                }
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (!transfer.done) {
                    continue;
                }
                for (const WireVertex& vertex : transfer.vertices) {
                    recordExhaustOwnerVertex(info, vertex, transfer.id);
                }
            }
        }
        recordExhaustAdjacentSecondaryOwners(info);
        return;
    }

    if (!info.done) {
        return;
    }
    for (EdgeInfo& edge : info.edges) {
        if (edge.wireInfo == 0U) {
            continue;
        }
        edge.exhaustSeed = true;
        if (edge.wireInfo2 != 0U) {
            edge.exhaustSharedOwner = true;
            edge.exhaustDoneSecondary = true;
        }
    }
}

void WireJoiner::Impl::recordBuildClosedWireRemovalLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire(), after "findTightBound(); exhaustTightBound();", counts
    // vertices from done "wireInfo" / "wireInfo2"; when "++counter[vertex.edgeInfo()] == 2",
    // it marks "vertex.edgeInfo()->iteration = -1" and calls "aHistory->Remove(info.edge)".
    // cad-core records that request-local Remove lifecycle before generated open-export edges are
    // added; the repeated findClosedWires()/findTightBound() loop remains a later migration step.
    if (!info.done) {
        return;
    }

    std::vector<int> counter(info.edges.size(), 0);
    std::vector<std::size_t> countedOwners;
    std::size_t removedCount = 0;
    std::size_t removedUnownedCount = 0;
    std::size_t removedSecondaryCount = 0;
    std::size_t removedPrimaryCount = 0;

    const auto ownerVertices = [&](std::size_t ownerId) -> const std::vector<WireVertex>* {
        for (const OwnerWireInfo& owner : info.ownerWires) {
            if (owner.id == ownerId) {
                return owner.splitWireId == 0U ? &owner.vertices : nullptr;
            }
            if (owner.splitWireId == ownerId) {
                return &owner.splitOwnerVertices;
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (transfer.id == ownerId) {
                    return &transfer.vertices;
                }
            }
        }
        return nullptr;
    };
    const auto ownerAlreadyCounted = [&](std::size_t ownerId) {
        return std::find(countedOwners.begin(), countedOwners.end(), ownerId) != countedOwners.end();
    };
    const auto isDiscardedPrimaryOwner = [&](std::size_t ownerId) {
        if (ownerId == 0U) {
            return false;
        }
        for (const OwnerWireInfo& owner : info.ownerWires) {
            if ((owner.id == ownerId || (owner.splitWireId != 0U && owner.splitWireId == ownerId))
                && owner.exhaustDiscardedByPurge) {
                return true;
            }
        }
        return false;
    };
    const auto countOwner = [&](std::size_t ownerId,
                                bool secondaryOwner,
                                std::size_t aHistoryRemoveSourceEdgeIndex) {
        if (ownerId == 0U || ownerAlreadyCounted(ownerId) || !isDoneOwner(info, ownerId)) {
            return;
        }
        const std::vector<WireVertex>* vertices = ownerVertices(ownerId);
        if (vertices == nullptr) {
            return;
        }
        countedOwners.push_back(ownerId);
        for (const WireVertex& vertex : *vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (edge.iteration == -2) {
                continue;
            }
            if (++counter[vertex.edgeIndex] == 2 && edge.iteration >= 0) {
                edge.iteration = -1;
                edge.buildClosedWireRemoved = true;
                if (secondaryOwner) {
                    edge.buildClosedWireRemovedBySecondaryOwner = true;
                }
                else {
                    edge.buildClosedWireRemovedByPrimaryOwner = true;
                }
                if (aHistoryRemoveSourceEdgeIndex < info.edges.size()) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::buildClosedWire(), after "vertex.edgeInfo()->iteration = -1",
                    // calls "aHistory->Remove(info.edge)" using the outer loop EdgeInfo. Keep the
                    // iteration-removal target and the aHistory Remove source as separate evidence.
                    info.edges[aHistoryRemoveSourceEdgeIndex].buildClosedWireAHistoryRemoved = true;
                    appendUniqueSourceIndex(
                        edge.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices,
                        aHistoryRemoveSourceEdgeIndex
                    );
                    appendUniqueSourceIndices(
                        edge.buildClosedWireAHistoryRemoveSourceEdgeIndices,
                        info.edges[aHistoryRemoveSourceEdgeIndex].sourceEdgeIndices
                    );
                }
                ++removedCount;
                if (secondaryOwner) {
                    ++removedSecondaryCount;
                }
                else {
                    ++removedPrimaryCount;
                }
            }
        }
    };

    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.iteration == -2) {
            continue;
        }
        if (edge.iteration < 0 || edge.wireInfo == 0U) {
            continue;
        }
        if (!isDoneOwner(info, edge.wireInfo)) {
            if (edge.iteration >= 0) {
                const bool resetDiscardedPrimaryOwner = isDiscardedPrimaryOwner(edge.wireInfo);
                edge.iteration = -1;
                edge.buildClosedWireRemoved = true;
                edge.buildClosedWireRemovedByUnowned = true;
                edge.buildClosedWireAHistoryRemoved = true;
                appendUniqueSourceIndex(
                    edge.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices,
                    edgeIndex
                );
                appendUniqueSourceIndices(
                    edge.buildClosedWireAHistoryRemoveSourceEdgeIndices,
                    edge.sourceEdgeIndices
                );
                ++removedCount;
                ++removedUnownedCount;
                if (resetDiscardedPrimaryOwner) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::exhaustTightBoundUpdateEdge(), "wireInfo.reset()" for a purged
                    // owner; ::buildClosedWire() immediately removes still-active unowned edges
                    // with "info.iteration = -1". Apply both transitions together so the reset does
                    // not create a transient openWireCompound export.
                    edge.wireInfo = 0U;
                    ++info.tightBoundExhaustPrimaryResetEdgeInfoCount;
                }
            }
            continue;
        }
        countOwner(edge.wireInfo2, true, edgeIndex);
        countOwner(edge.wireInfo, false, edgeIndex);
    }

    historySummary_.deletedHistoryCount += removedCount;
    historySummary_.splitterHistory = historySummary_.splitterHistory || removedCount > 0U;
    if (removedCount > 0U) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), after removing consumed edges, sets "done = false"
        // and immediately repeats "findClosedWires(true); findTightBound();" inside the loop.
        // This records the pending repeated split/exhaust lifecycle without changing current
        // openWireCompound/getOpenWires output.
        ++info.repeatedSplitExhaustCycleCount;
        info.repeatedSplitExhaustRemovedEdgeInfoCount += removedCount;
        info.repeatedSplitExhaustRemovedUnownedEdgeInfoCount += removedUnownedCount;
        info.repeatedSplitExhaustRemovedSecondaryEdgeInfoCount += removedSecondaryCount;
        info.repeatedSplitExhaustRemovedPrimaryEdgeInfoCount += removedPrimaryCount;
    }
}

void WireJoiner::Impl::recordRepeatedSplitExhaustRerunLifecycle(
    WireInfo& info,
    const std::vector<TopoDS_Face>& boundedFaces,
    const WireJoinerHistoryMaterializationLedger& materializationLedger
)
{
    if (info.repeatedSplitExhaustCycleCount == 0U) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire(), after consumed edges are removed, immediately runs
    // "findClosedWires(true); findTightBound();" again. ::findClosedWires(true) first clears
    // "info.wireInfo" and "info.wireInfo2" for every EdgeInfo, then rebuilds owners from the
    // current "iteration" state. cad-core records that reset/rebuild on a temporary WireInfo copy
    // so the M1 ledger can prove the next search boundary without changing
    // openWireCompound/getOpenWires output before generated result-wire identity is migrated.
    WireInfo rerunInfo = info;
    const std::size_t existingOwnerCount = rerunInfo.ownerWires.size();
    const std::size_t savedNextWireInfoId = nextWireInfoId_;
    const int nextIteration2 = nextIteration2_;
    const bool producerIdentityUnsafe =
        wireJoinerHistoryMaterializationLedgerHasUnsafeProducer(info, materializationLedger);
    const auto materializationLedgerHasCandidateEdgeInfo = [&](std::size_t edgeIndex) {
        for (const WireJoinerHistoryMaterializationBinding& binding :
             materializationLedger.bindings) {
            if (binding.edgeInfoIndex == edgeIndex) {
                return true;
            }
        }
        return false;
    };
    std::size_t assignedEdges = 0;
    for (EdgeInfo& edge : rerunInfo.edges) {
        if (edge.wireInfo != 0U) {
            ++info.repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount;
            edge.wireInfo = 0U;
        }
        if (edge.wireInfo2 != 0U) {
            ++info.repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount;
            edge.wireInfo2 = 0U;
        }
    }
    for (std::size_t edgeIndex = 0; edgeIndex < rerunInfo.edges.size(); ++edgeIndex) {
        EdgeInfo& beginInfo = rerunInfo.edges[edgeIndex];
        if (beginInfo.edge.IsNull()) {
            continue;
        }
        if (beginInfo.iteration == -3) {
            ++info.repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount;
            continue;
        }
        if (beginInfo.iteration < 0) {
            continue;
        }
        ++info.repeatedSplitExhaustRerunActiveEdgeInfoCount;
        const bool wasOwnedActive = edgeIndex < info.edges.size()
            && info.edges[edgeIndex].wireInfo != 0U;
        if (wasOwnedActive) {
            ++info.repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount;
        }
        if (beginInfo.wireInfo != 0U) {
            continue;
        }
        ++info.repeatedSplitExhaustRerunClosedWireSearchCount;
        const std::optional<ClosedWireSearchResult> search = findClosedWirePath(rerunInfo, edgeIndex);
        if (!search) {
            ++info.repeatedSplitExhaustRerunClosedWireMissCount;
            if (wasOwnedActive && materializationLedgerHasCandidateEdgeInfo(edgeIndex)
                && edgeIndex < info.edges.size()) {
                EdgeInfo& liveEdge = info.edges[edgeIndex];
                if (liveEdge.iteration >= 0 && liveEdge.wireInfo != 0U) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::findClosedWires(true), called from ::buildClosedWire() after
                    // consumed-edge removal, first clears "info.wireInfo" and "info.wireInfo2".
                    // If the rerun closed-wire search misses, the active EdgeInfo remains unowned
                    // and later satisfies ::build()'s openWireCompound gate without a candidate-forced
                    // export. Apply only the result-wire candidate subset here so M3 advances the
                    // FreeCAD lifecycle without broadening getOpenWires() or adding output geometry.
                    liveEdge.wireInfo = 0U;
                    liveEdge.wireInfo2 = 0U;
                    ++info.repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount;
                }
            }
            continue;
        }

        const std::size_t owner = nextWireInfoId_++;
        OwnerWireInfo ownerInfo;
        ownerInfo.id = owner;
        ownerInfo.vertices = search->vertices;
        ownerInfo.wire = wireFromVertices(rerunInfo, ownerInfo.vertices);
        ownerInfo.closedWireSearchStackFrameCount = search->stackFrameCount;
        ownerInfo.closedWireSearchVertexStackCount = search->vertexStackCount;
        ownerInfo.closedWireSearchEdgeSetVisitCount = search->edgeSetVisitCount;
        ownerInfo.closedWireSearchBacktrackCount = search->backtrackCount;
        ownerInfo.closedWireSearchIntersectSkipCount = search->intersectSkipCount;
        for (const WireVertex& vertex : search->vertices) {
            if (vertex.edgeIndex >= rerunInfo.edges.size()) {
                continue;
            }
            EdgeInfo& edgeInfo = rerunInfo.edges[vertex.edgeIndex];
            if (edgeInfo.iteration < 0 || edgeInfo.wireInfo != 0U) {
                continue;
            }
            edgeInfo.wireInfo = owner;
            edgeInfo.closedWireOwner = true;
            ++assignedEdges;
        }
        rerunInfo.ownerWires.push_back(std::move(ownerInfo));
    }
    if (info.repeatedSplitExhaustRerunActiveEdgeInfoCount == 0U) {
        ++info.repeatedSplitExhaustRerunNoActiveSearchCount;
    }
    nextWireInfoId_ = savedNextWireInfoId;
    const std::size_t newOwnerCount = rerunInfo.ownerWires.size() - existingOwnerCount;
    info.repeatedSplitExhaustRerunClosedWireInfoCount += newOwnerCount;
    info.repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount += assignedEdges;

    const auto canApplyLiveRerunOwner = [&](const OwnerWireInfo& owner) {
        if (owner.vertices.empty() || owner.wire.IsNull()) {
            return false;
        }
        for (const WireVertex& vertex : owner.vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                return false;
            }
            const EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (edge.iteration < 0 || edge.wireInfo != 0U) {
                return false;
            }
        }
        return true;
    };
    const auto resettableRerunOwnerAssignedEdgeCount = [&](const OwnerWireInfo& owner) -> std::size_t {
        if (owner.vertices.empty() || owner.wire.IsNull()) {
            return 0U;
        }
        bool needsPrimaryReset = false;
        std::vector<std::size_t> assignedEdgeIndices;
        for (const WireVertex& vertex : owner.vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                return 0U;
            }
            const EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (edge.iteration < 0) {
                return 0U;
            }
            needsPrimaryReset = needsPrimaryReset || edge.wireInfo != 0U;
            if (std::find(assignedEdgeIndices.begin(), assignedEdgeIndices.end(), vertex.edgeIndex)
                == assignedEdgeIndices.end()) {
                assignedEdgeIndices.push_back(vertex.edgeIndex);
            }
        }
        return needsPrimaryReset ? assignedEdgeIndices.size() : 0U;
    };

    for (std::size_t ownerIndex = existingOwnerCount; ownerIndex < rerunInfo.ownerWires.size();
         ++ownerIndex) {
        OwnerWireInfo& owner = rerunInfo.ownerWires[ownerIndex];
        info.repeatedSplitExhaustRerunClosedWireVertexCount += owner.vertices.size();
        recordBranchSearchCandidatesForOwner(rerunInfo, owner, boundedFaces);
        info.repeatedSplitExhaustRerunBranchSearchCandidateCount += owner.branchSearchCandidateCount;
        info.repeatedSplitExhaustRerunBranchSearchInsideCandidateCount
            += owner.branchSearchInsideCandidateCount;
        info.repeatedSplitExhaustRerunBranchSearchOutsideCandidateCount
            += owner.branchSearchOutsideCandidateCount;
        if (owner.hasNewWireSeed) {
            info.repeatedSplitExhaustRerunNewWireSeedCandidateCount
                += owner.branchSearchInsideCandidateCount;
        }

        const std::size_t resettableAssignedEdges = resettableRerunOwnerAssignedEdgeCount(owner);
        if (resettableAssignedEdges > 0U) {
            ++info.repeatedSplitExhaustRerunResettableClosedWireInfoCount;
            info.repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount += resettableAssignedEdges;
        }
        const bool canApplyWithoutReset = canApplyLiveRerunOwner(owner);
        const bool canApplyWithLiveReset = !canApplyWithoutReset && resettableAssignedEdges > 0U
            && (owner.branchSearchCandidateCount == 0U || !producerIdentityUnsafe);
        if (!canApplyWithoutReset && !canApplyWithLiveReset) {
            if (producerIdentityUnsafe && resettableAssignedEdges > 0U) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::buildClosedWire() reruns findClosedWires(true)/findTightBound()
                // before ::build() emits openWireCompound. If the only live-reset path would mutate
                // owners while generated result-wire identity is still result-wire-producer candidate, record the
                // actual rejected owner edges here instead of deriving it later from generated
                // output count.
                info.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount += resettableAssignedEdges;
            }
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), after marking consumed edges "iteration = -1",
        // immediately repeats "findClosedWires(true); findTightBound()". If the rerun finds a
        // closed WireInfo whose EdgeInfo entries are still active and unowned, or can be rebuilt by
        // the FreeCAD reset-before-rerun path without generated open-export identity, cad-core
        // writes that owner back to the live EdgeInfo ledger. Generated/open-export identity cases
        // still remain outside this path.
        OwnerWireInfo liveOwner;
        liveOwner.id = nextWireInfoId_++;
        liveOwner.vertices = owner.vertices;
        liveOwner.wire = owner.wire;
        liveOwner.closedWireSearchStackFrameCount = owner.closedWireSearchStackFrameCount;
        liveOwner.closedWireSearchVertexStackCount = owner.closedWireSearchVertexStackCount;
        liveOwner.closedWireSearchEdgeSetVisitCount = owner.closedWireSearchEdgeSetVisitCount;
        liveOwner.closedWireSearchBacktrackCount = owner.closedWireSearchBacktrackCount;
        liveOwner.closedWireSearchIntersectSkipCount = owner.closedWireSearchIntersectSkipCount;

        std::size_t liveAssignedEdges = 0;
        std::vector<std::size_t> liveResetPrimaryEdgeIndices;
        std::vector<std::size_t> liveResetSecondaryEdgeIndices;
        for (const WireVertex& vertex : liveOwner.vertices) {
            EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (canApplyWithLiveReset && edge.wireInfo != 0U
                && std::find(
                       liveResetPrimaryEdgeIndices.begin(),
                       liveResetPrimaryEdgeIndices.end(),
                       vertex.edgeIndex
                   ) == liveResetPrimaryEdgeIndices.end()) {
                liveResetPrimaryEdgeIndices.push_back(vertex.edgeIndex);
                ++info.repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount;
            }
            if (canApplyWithLiveReset && edge.wireInfo2 != 0U
                && std::find(
                       liveResetSecondaryEdgeIndices.begin(),
                       liveResetSecondaryEdgeIndices.end(),
                       vertex.edgeIndex
                   ) == liveResetSecondaryEdgeIndices.end()) {
                liveResetSecondaryEdgeIndices.push_back(vertex.edgeIndex);
                ++info.repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount;
                edge.wireInfo2 = 0U;
            }
            edge.wireInfo = liveOwner.id;
            edge.closedWireOwner = true;
            ++liveAssignedEdges;
        }
        info.repeatedSplitExhaustRerunLiveClosedWireVertexCount += liveOwner.vertices.size();
        info.repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount += liveAssignedEdges;
        ++info.repeatedSplitExhaustRerunLiveClosedWireInfoCount;
        info.ownerWires.push_back(std::move(liveOwner));
        OwnerWireInfo& insertedOwner = info.ownerWires.back();
        recordBranchSearchCandidatesForOwner(info, insertedOwner, boundedFaces);
        info.repeatedSplitExhaustRerunLiveBranchSearchCandidateCount
            += insertedOwner.branchSearchCandidateCount;
        info.repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount
            += insertedOwner.branchSearchInsideCandidateCount;
        info.repeatedSplitExhaustRerunLiveBranchSearchOutsideCandidateCount
            += insertedOwner.branchSearchOutsideCandidateCount;

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findTightBound(), after a rerun-created WireInfo is found, checks adjacent
        // branch candidates and either creates a new transfer WireInfo or calls
        // ::findTightBoundUpdateVertices() to mark the current WireInfo done. This is limited to the
        // live rerun owner; the next buildClosedWire() removal pass is tracked separately below.
        const bool transferRecorded = insertedOwner.branchSearchInsideCandidateCount > 0U
            && recordTightBoundTransferWire(info, insertedOwner);
        if (transferRecorded) {
            ++info.repeatedSplitExhaustRerunLiveTransferWireInfoCount;
            const TightBoundTransferWire& transfer = insertedOwner.transferWires.back();
            info.repeatedSplitExhaustRerunLiveTransferredOwnerEdgeInfoCount
                += transfer.transferredOwnerEdgeCount;
            insertedOwner.hasSplitWireCandidate = true;
            insertedOwner.splitWireCandidateCount = 1;
            info.hasSplitWireCandidate = true;
            ++info.splitWireCandidateCount;
        }
        else {
            insertedOwner.done = true;
            info.done = true;
            ++info.repeatedSplitExhaustRerunLiveDoneWireInfoCount;
        }
    }
    nextIteration2_ = nextIteration2;

    if (info.repeatedSplitExhaustRerunLiveClosedWireInfoCount > 0U) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), after the loop-tail
        // "findClosedWires(true); findTightBound()", the next while pass rebuilds a fresh
        // "counter" and removes only EdgeInfo entries whose done primary/secondary owner vertices
        // make "++counter[vertex.edgeInfo()] == 2"; the removal target receives
        // "iteration = -1" while "aHistory->Remove(info.edge)" uses the outer EdgeInfo source.
        // Keep this next-pass producer evidence on EdgeInfo, but leave iteration/wireInfo unchanged
        // until the generated open-export bridge is removed from the live output path.
        ++info.repeatedSplitExhaustRerunRemovalScanCount;
        std::vector<int> counter(info.edges.size(), 0);
        std::vector<std::size_t> countedOwners;
        std::size_t removalCount = 0;
        std::size_t unownedRemovalCount = 0;
        std::size_t secondaryRemovalCount = 0;
        std::size_t primaryRemovalCount = 0;

        const auto ownerVertices = [&](std::size_t ownerId) -> const std::vector<WireVertex>* {
            for (const OwnerWireInfo& owner : info.ownerWires) {
                if (owner.id == ownerId) {
                    return owner.splitWireId == 0U ? &owner.vertices : nullptr;
                }
                if (owner.splitWireId != 0U && owner.splitWireId == ownerId) {
                    return &owner.splitOwnerVertices;
                }
                for (const TightBoundTransferWire& transfer : owner.transferWires) {
                    if (transfer.id == ownerId) {
                        return &transfer.vertices;
                    }
                }
            }
            return nullptr;
        };
        const auto ownerAlreadyCounted = [&](std::size_t ownerId) {
            return std::find(countedOwners.begin(), countedOwners.end(), ownerId)
                != countedOwners.end();
        };
        const auto countOwner = [&](std::size_t ownerId,
                                    bool secondaryOwner,
                                    std::size_t aHistoryRemoveSourceEdgeIndex) {
            if (ownerId == 0U || ownerAlreadyCounted(ownerId) || !isDoneOwner(info, ownerId)) {
                return;
            }
            const std::vector<WireVertex>* vertices = ownerVertices(ownerId);
            if (vertices == nullptr) {
                return;
            }
            countedOwners.push_back(ownerId);
            for (const WireVertex& vertex : *vertices) {
                if (vertex.edgeIndex >= info.edges.size()) {
                    continue;
                }
                EdgeInfo& edge = info.edges[vertex.edgeIndex];
                if (edge.iteration == -2) {
                    continue;
                }
                if (++counter[vertex.edgeIndex] == 2 && edge.iteration >= 0) {
                    edge.buildClosedWireRemoved = true;
                    if (secondaryOwner) {
                        edge.buildClosedWireRemovedBySecondaryOwner = true;
                    }
                    else {
                        edge.buildClosedWireRemovedByPrimaryOwner = true;
                    }
                    if (aHistoryRemoveSourceEdgeIndex < info.edges.size()) {
                        info.edges[aHistoryRemoveSourceEdgeIndex].buildClosedWireAHistoryRemoved = true;
                        appendUniqueSourceIndex(
                            edge.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices,
                            aHistoryRemoveSourceEdgeIndex
                        );
                        appendUniqueSourceIndices(
                            edge.buildClosedWireAHistoryRemoveSourceEdgeIndices,
                            info.edges[aHistoryRemoveSourceEdgeIndex].sourceEdgeIndices
                        );
                    }
                    ++removalCount;
                    if (secondaryOwner) {
                        ++secondaryRemovalCount;
                    }
                    else {
                        ++primaryRemovalCount;
                    }
                }
            }
        };

        for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
            EdgeInfo& edge = info.edges[edgeIndex];
            if (edge.iteration == -2 || edge.iteration < 0) {
                continue;
            }
            if (edge.wireInfo == 0U || !isDoneOwner(info, edge.wireInfo)) {
                edge.buildClosedWireRemoved = true;
                edge.buildClosedWireRemovedByUnowned = true;
                edge.buildClosedWireAHistoryRemoved = true;
                appendUniqueSourceIndex(
                    edge.buildClosedWireAHistoryRemoveSourceEdgeInfoIndices,
                    edgeIndex
                );
                appendUniqueSourceIndices(
                    edge.buildClosedWireAHistoryRemoveSourceEdgeIndices,
                    edge.sourceEdgeIndices
                );
                ++removalCount;
                ++unownedRemovalCount;
                continue;
            }
            countOwner(edge.wireInfo2, true, edgeIndex);
            countOwner(edge.wireInfo, false, edgeIndex);
        }

        info.repeatedSplitExhaustRerunRemovalEdgeInfoCount += removalCount;
        info.repeatedSplitExhaustRerunRemovalUnownedEdgeInfoCount += unownedRemovalCount;
        info.repeatedSplitExhaustRerunRemovalSecondaryEdgeInfoCount += secondaryRemovalCount;
        info.repeatedSplitExhaustRerunRemovalPrimaryEdgeInfoCount += primaryRemovalCount;
        if (removalCount == 0U) {
            ++info.repeatedSplitExhaustRerunLoopExitNoRemovalCount;
        }
        historySummary_.deletedHistoryCount += removalCount;
        historySummary_.splitterHistory = historySummary_.splitterHistory || removalCount > 0U;
    }
}

void WireJoiner::Impl::recordBranchSearchCandidatesForOwner(
    WireInfo& info,
    OwnerWireInfo& owner,
    const std::vector<TopoDS_Face>& boundedFaces
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBound(), for each "wireInfo->vertices", marks
    // "vertex.it->iteration2 = iteration2" and then ::findTightBoundByVertices() walks the
    // current EdgeInfo adjacent range. This keeps branch candidates scoped to the closed
    // WireInfo produced by findClosedWires(), not to the whole split-edge component.
    std::vector<bool> ownerEdge(info.edges.size(), false);
    const int iteration2 = nextIteration2_++;
    for (const WireVertex& vertex : owner.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        ownerEdge[vertex.edgeIndex] = true;
        info.edges[vertex.edgeIndex].iteration2 = iteration2;
    }

    for (WireVertex& vertex : owner.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& current = info.edges[vertex.edgeIndex];
        const int adjacentEndpointIndex = vertex.start ? 1 : 0;
        const gp_Pnt point = adjacentEndpointIndex == 0 ? current.p1 : current.p2;
        std::size_t candidates = 0;
        std::size_t insideCandidates = 0;
        std::size_t outsideCandidates = 0;
        std::size_t newWireSeeds = 0;
        if (current.iStart[adjacentEndpointIndex] < 0 || current.iEnd[adjacentEndpointIndex] < 0) {
            continue;
        }
        for (int adjacentIndex = current.iStart[adjacentEndpointIndex];
             adjacentIndex < current.iEnd[adjacentEndpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0
                || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex == vertex.edgeIndex || adjacent.edgeIndex >= info.edges.size()) {
                continue;
            }
            if (ownerEdge[adjacent.edgeIndex]) {
                continue;
            }
            EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.iteration < 0) {
                continue;
            }
            const gp_Pnt adjacentPoint = adjacent.start ? next.p1 : next.p2;
            if (!samePoint(point, adjacentPoint)) {
                continue;
            }
            ++candidates;
            const bool inside = pointInsideOrOnAnyFace(next.mid, boundedFaces);
            if (inside) {
                ++insideCandidates;
                ++newWireSeeds;
                const bool transfersOwnerEdge = current.wireInfo == owner.id;
                owner.branchCandidates.push_back(
                    TightBoundBranchCandidate {
                        vertex,
                        adjacent,
                        true,
                        transfersOwnerEdge,
                    }
                );
                if (transfersOwnerEdge) {
                    current.tightBoundOwnerTransferCandidate = true;
                }
            }
            else {
                ++outsideCandidates;
                owner.branchCandidates.push_back(
                    TightBoundBranchCandidate {
                        vertex,
                        adjacent,
                        false,
                        false,
                    }
                );
            }
        }
        vertex.branchCandidateCount = candidates;
        owner.branchSearchCandidateCount += candidates;
        owner.branchSearchInsideCandidateCount += insideCandidates;
        owner.branchSearchOutsideCandidateCount += outsideCandidates;
        current.branchCandidateCount += candidates;
        current.branchInsideCandidateCount += insideCandidates;
        current.branchOutsideCandidateCount += outsideCandidates;
        current.newWireSeedCandidateCount += newWireSeeds;
        if (newWireSeeds > 0U) {
            owner.hasNewWireSeed = true;
            info.hasNewWireSeed = true;
        }
    }
}

void WireJoiner::Impl::recordBranchSearchCandidates(WireInfo& info, const std::vector<TopoDS_Face>& boundedFaces)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices() walks "current->iStart[idx]..iEnd[idx]" and
    // skips "next == current || next->iteration2 == iteration2 || next->iteration < 0" before
    // testing "isInside(*wireInfo, next->mid)". This records the same request-local adjacent
    // EdgeInfo candidates and the inside candidates that would seed the "new WireInfo" branch
    // without yet replacing the bounded-face ownership classifier with the full branch search.
    if (!info.ownerWires.empty()) {
        for (OwnerWireInfo& owner : info.ownerWires) {
            recordBranchSearchCandidatesForOwner(info, owner, boundedFaces);
        }
        return;
    }

    for (WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& current = info.edges[vertex.edgeIndex];
        const int adjacentEndpointIndex = vertex.start ? 1 : 0;
        const gp_Pnt point = adjacentEndpointIndex == 0 ? current.p1 : current.p2;
        std::size_t candidates = 0;
        std::size_t insideCandidates = 0;
        std::size_t outsideCandidates = 0;
        std::size_t newWireSeeds = 0;
        if (current.iStart[adjacentEndpointIndex] < 0 || current.iEnd[adjacentEndpointIndex] < 0) {
            continue;
        }
        for (int adjacentIndex = current.iStart[adjacentEndpointIndex];
             adjacentIndex < current.iEnd[adjacentEndpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0
                || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex == vertex.edgeIndex || adjacent.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.iteration < 0) {
                continue;
            }
            const gp_Pnt adjacentPoint = adjacent.start ? next.p1 : next.p2;
            if (!samePoint(point, adjacentPoint)) {
                continue;
            }
            ++candidates;
            if (pointInsideOrOnAnyFace(next.mid, boundedFaces)) {
                ++insideCandidates;
                ++newWireSeeds;
            }
            else {
                ++outsideCandidates;
            }
        }
        vertex.branchCandidateCount = candidates;
        current.branchCandidateCount += candidates;
        current.branchInsideCandidateCount += insideCandidates;
        current.branchOutsideCandidateCount += outsideCandidates;
        current.newWireSeedCandidateCount += newWireSeeds;
        if (newWireSeeds > 0U) {
            info.hasNewWireSeed = true;
        }
    }
}

bool WireJoiner::Impl::recordTightBoundTransferWire(WireInfo& info, OwnerWireInfo& owner)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices(), after finding an inside adjacent branch, creates
    // "newWire.reset(new WireInfo())", pushes "beginVertex" and the current search stack, then
    // transfers vertices whose "edgeInfo()->wireInfo == wireInfo" to the new owner, then
    // ::findTightBoundSplitWire() creates "splitWire" for the remaining old owner vertices.
    // cad-core now applies the transfer id to EdgeInfo and records the remaining owner vertices
    // as an explicit split-owner ledger while the wider open-export bridge is still temporary.
    if (!owner.transferWires.empty() || owner.vertices.empty()) {
        return false;
    }

    std::optional<TightBoundTransferPath> transferPath;
    std::optional<TightBoundExistingWireSearchTrace> selectedExistingWireTrace;
    for (const TightBoundBranchCandidate& candidate : owner.branchCandidates) {
        std::optional<TightBoundExistingWireSearchTrace> candidateTrace;
        if (candidate.inside && candidate.transfersOwnerEdge) {
            const TightBoundExistingWireSearchTrace trace
                = traceExistingWireSearchForCandidate(info, owner, candidate);
            candidateTrace = trace;
            ++owner.tightBoundExistingWireSearchCount;
            owner.tightBoundExistingWireSearchStackFrameCount += trace.stackFrameCount;
            owner.tightBoundExistingWireSearchVertexStackCount += trace.vertexStackCount;
            owner.tightBoundExistingWireSearchEdgeSetVisitCount += trace.edgeSetVisitCount;
            owner.tightBoundExistingWireSearchBacktrackCount += trace.backtrackCount;
            owner.tightBoundExistingWireSearchIntersectSkipCount += trace.intersectSkipCount;
            owner.tightBoundFullWireSetInsertCount += trace.fullWireSetInsertCount;
            owner.tightBoundFullWireSetEraseCount += trace.fullWireSetEraseCount;
            owner.tightBoundFullWireSetAbortCount += trace.fullWireSetAbortCount;
            owner.tightBoundFullWireSetPurgeCandidateCount += trace.fullWireSetPurgeCandidateCount;
            if (trace.fullWireSetAbortCount > 0U) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::_findClosedWiresUpdateStack(), "if (!wireSet.empty() &&
                // wireSet.contains(info.wireInfo.get()))" aborts the current branch. A later
                // existing-wire hit may still resolve the search; otherwise cad-core blocks this
                // candidate from becoming a transfer wire.
                ++owner.tightBoundFullWireSetAbortSearchCount;
                if (trace.hit) {
                    ++owner.tightBoundFullWireSetAbortResolvedByHitCount;
                }
                else {
                    ++owner.tightBoundFullWireSetAbortBlockedSearchCount;
                }
            }
            if (trace.hit) {
                ++owner.tightBoundExistingWireHitCount;
                owner.tightBoundExistingWireSearchPathVertexCount += trace.hitPath.size();
                if (trace.idxVertex >= 0) {
                    ++owner.tightBoundExistingWireSearchIdxVertexCount;
                }
                if (trace.stackPos >= 0) {
                    ++owner.tightBoundExistingWireSearchStackPosCount;
                }
            }
            if (trace.reverseHit) {
                ++owner.tightBoundExistingWireReverseHitCount;
            }
            const bool fullWireSetPurged = trace.fullWireSetPurgeCandidateCount > 0U;
            if (trace.purge || fullWireSetPurged) {
                ++owner.tightBoundExistingWirePurgeCount;
                owner.purge = true;
            }
            if (trace.purge || (trace.fullWireSetAbortCount > 0U && !trace.hit)) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::_findClosedWiresUpdateStack(), "if (!wireSet.empty() &&
                // wireSet.contains(info.wireInfo.get()))" aborts this branch; with an existing
                // wireInfo it marks "wireInfo->purge = true" and does not let the same candidate
                // become a transfer wire.
                ++owner.tightBoundFullWireSetBlockedTransferCount;
                continue;
            }
        }
        transferPath = tightBoundTransferPathForCandidate(info, owner, candidate);
        TightBoundExistingWirePathBlockReason existingWireBlockReason
            = TightBoundExistingWirePathBlockReason::None;
        if (!transferPath && candidateTrace && candidateTrace->hit) {
            transferPath = tightBoundTransferPathForExistingWireHit(
                info,
                owner,
                candidate,
                *candidateTrace,
                &existingWireBlockReason
            );
        }
        if (!transferPath && candidateTrace && candidateTrace->hit) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::_findClosedWiresWithExisting() can still report idxVertex/stackPos
            // for a branch that does not become the selected ::findTightBoundByVertices() transfer.
            // Keep that remaining M1 gap explicit instead of hiding it in hit-minus-selected math.
            ++owner.tightBoundExistingWireSearchOnlyPathBlockedCount;
            if (existingWireBlockReason == TightBoundExistingWirePathBlockReason::OwnerVertexMissing) {
                ++owner.tightBoundExistingWireSearchOnlyOwnerVertexBlockedCount;
            }
            else if (existingWireBlockReason == TightBoundExistingWirePathBlockReason::OrderBlocked) {
                ++owner.tightBoundExistingWireSearchOnlyOrderBlockedCount;
            }
            else if (
                existingWireBlockReason == TightBoundExistingWirePathBlockReason::WireBuildBlocked
            ) {
                ++owner.tightBoundExistingWireSearchOnlyWireBuildBlockedCount;
            }
        }
        if (transferPath) {
            selectedExistingWireTrace = candidateTrace;
            if (selectedExistingWireTrace) {
                transferPath->existingWireHit = selectedExistingWireTrace->hit;
                transferPath->existingWireIdxVertex = selectedExistingWireTrace->idxVertex;
                transferPath->existingWireStackPos = selectedExistingWireTrace->stackPos;
            }
            break;
        }
    }
    if (!transferPath || transferPath->transferVertices.empty()) {
        return false;
    }

    TightBoundTransferWire transfer;
    transfer.id = nextWireInfoId_++;
    transfer.vertices = transferPath->transferVertices;
    transfer.splitWireVertices = transferPath->splitWireVertices;
    transfer.existingWireHit = transferPath->existingWireHit;
    transfer.existingWireIdxVertex = transferPath->existingWireIdxVertex;
    transfer.existingWireStackPos = transferPath->existingWireStackPos;

    for (const WireVertex& vertex : transfer.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& edge = info.edges[vertex.edgeIndex];
        if (edge.wireInfo == owner.id) {
            edge.wireInfo = transfer.id;
            edge.tightBoundTransferredOwner = true;
            ++transfer.transferredOwnerEdgeCount;
        }
    }
    transfer.wireBuilt = !wireFromVertices(info, transfer.vertices).IsNull();
    transfer.splitWireBuilt = !transfer.splitWireVertices.empty()
        && !wireFromVertices(info, transfer.splitWireVertices).IsNull();
    transfer.done = transfer.transferredOwnerEdgeCount > 0U;

    if (!transferPath->splitOwnerVertices.empty()
        && transferPath->splitOwnerVertices.size() != owner.vertices.size()) {
        owner.splitOwnerVertices = transferPath->splitOwnerVertices;
        owner.splitOwnerWireBuilt = !wireFromVertices(info, owner.splitOwnerVertices).IsNull();
    }
    owner.transferWires.push_back(std::move(transfer));
    return true;
}

void WireJoiner::Impl::recordTightBoundLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundSplitWire() creates "splitWire.reset(new WireInfo())"
    // when the branch search slices an existing WireInfo, and ::findTightBoundUpdateVertices()
    // marks "beginInfo.wireInfo->done = true" before propagating that owner to vertices whose
    // EdgeInfo still points at another unfinished WireInfo. This records the equivalent
    // request-local lifecycle boundary without using it as an output pruning rule yet.
    bool hasOpenExportEdge = false;
    for (const EdgeInfo& edge : info.edges) {
        hasOpenExportEdge = hasOpenExportEdge || edgeInfoExportsOpenWireCompound(edge);
    }
    std::vector<bool> unassignedPropagationRecorded(info.edges.size(), false);
    std::vector<bool> otherWirePropagationRecorded(info.edges.size(), false);
    std::vector<bool> otherWireLivePropagationRecorded(info.edges.size(), false);
    const auto recordDoneOwnerPropagation = [&](std::size_t ownerId,
                                                const std::vector<WireVertex>& vertices) {
        for (const WireVertex& vertex : vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (edge.iteration < 0) {
                continue;
            }
            if (edge.wireInfo == 0U) {
                if (!unassignedPropagationRecorded[vertex.edgeIndex]) {
                    ++info.ownerPropagationCandidateCount;
                    ++info.ownerPropagationUnassignedCandidateCount;
                    ++edge.ownerPropagationCandidateCount;
                    unassignedPropagationRecorded[vertex.edgeIndex] = true;
                }
                continue;
            }
            if (edge.wireInfo == ownerId || isDoneOwner(info, edge.wireInfo)) {
                continue;
            }
            if (!otherWirePropagationRecorded[vertex.edgeIndex]) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findTightBoundUpdateVertices(), after "beginInfo.wireInfo->done =
                // true", keeps done owners stable while edges owned by an unfinished "otherWire" are
                // reassigned to the just-completed owner. cad-core records this propagation branch
                // without using it to prune openWireCompound/getOpenWires output.
                ++info.ownerPropagationCandidateCount;
                ++info.ownerPropagationOtherWireCandidateCount;
                ++edge.ownerPropagationCandidateCount;
                otherWirePropagationRecorded[vertex.edgeIndex] = true;
            }
            if (!otherWireLivePropagationRecorded[vertex.edgeIndex]) {
                edge.wireInfo = ownerId;
                ++info.ownerPropagationOtherWireLiveEdgeInfoCount;
                otherWireLivePropagationRecorded[vertex.edgeIndex] = true;
            }
        }
    };

    if (!info.ownerWires.empty()) {
        bool anyDone = false;
        for (OwnerWireInfo& owner : info.ownerWires) {
            const bool transferRecorded = owner.branchSearchInsideCandidateCount > 0U
                && recordTightBoundTransferWire(info, owner);
            if (transferRecorded) {
                owner.hasSplitWireCandidate = true;
                owner.splitWireCandidateCount = 1;
                info.hasSplitWireCandidate = true;
                info.splitWireCandidateCount += owner.splitWireCandidateCount;
                for (const WireVertex& vertex : owner.vertices) {
                    if (vertex.edgeIndex >= info.edges.size()) {
                        continue;
                    }
                    EdgeInfo& edge = info.edges[vertex.edgeIndex];
                    if (edge.branchInsideCandidateCount > 0U) {
                        edge.splitWireCandidateCount += edge.branchInsideCandidateCount;
                    }
                }
            }
            const bool hasOwnerVertices = !owner.splitOwnerVertices.empty()
                || (!owner.vertices.empty() && owner.transferWires.empty());
            if (hasOwnerVertices) {
                if (!owner.splitOwnerVertices.empty() && owner.splitWireId == 0U) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::findTightBoundSplitWire(), "splitWire.reset(new WireInfo())"
                    // followed by "info->wireInfo = splitWire" for remaining old-owner vertices.
                    // cad-core keeps that split owner live in EdgeInfo while output export still
                    // waits for the later openWireCompound / generated-result-wire milestones.
                    for (const WireVertex& vertex : owner.splitOwnerVertices) {
                        if (vertex.edgeIndex >= info.edges.size()) {
                            continue;
                        }
                        EdgeInfo& edge = info.edges[vertex.edgeIndex];
                        if (edge.wireInfo == owner.id) {
                            if (owner.splitWireId == 0U) {
                                owner.splitWireId = nextWireInfoId_++;
                            }
                            edge.wireInfo = owner.splitWireId;
                        }
                    }
                }
                owner.done = true;
                anyDone = true;
            }
            if (owner.done) {
                const std::vector<WireVertex>& ownerVertices = owner.splitOwnerVertices.empty()
                    ? owner.vertices
                    : owner.splitOwnerVertices;
                const std::size_t propagationOwnerId = owner.splitWireId == 0U ? owner.id
                                                                               : owner.splitWireId;
                recordDoneOwnerPropagation(propagationOwnerId, ownerVertices);
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (transfer.done) {
                    recordDoneOwnerPropagation(transfer.id, transfer.vertices);
                }
            }
        }
        info.done = anyDone;
        return;
    }

    bool hasOwnedEdge = false;
    std::size_t insideBranchCandidates = 0;
    std::size_t outsideBranchCandidates = 0;
    for (const EdgeInfo& edge : info.edges) {
        hasOwnedEdge = hasOwnedEdge || edge.wireInfo != 0U;
        insideBranchCandidates += edge.branchInsideCandidateCount;
        outsideBranchCandidates += edge.branchOutsideCandidateCount;
    }

    if (insideBranchCandidates > 0U && (outsideBranchCandidates > 0U || hasOpenExportEdge)) {
        info.hasSplitWireCandidate = true;
        info.splitWireCandidateCount = 1;
        for (EdgeInfo& edge : info.edges) {
            if (edge.branchInsideCandidateCount > 0U) {
                edge.splitWireCandidateCount += edge.branchInsideCandidateCount;
            }
        }
    }

    if (!info.orderedVertices.empty() && (hasOwnedEdge || insideBranchCandidates > 0U)) {
        info.done = true;
    }

    if (info.done && hasOwnedEdge) {
        recordDoneOwnerPropagation(info.id, info.orderedVertices);
    }
}

void WireJoiner::Impl::recordOpenWireCompoundLedger(
    WireInfo& info,
    WireJoinerHistoryMaterializationLedger& materializationLedger
)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), after buildClosedWire(), loops final EdgeInfo states and adds
    // "info.wire()" to openWireCompound when "iteration == -3 || (!info.wireInfo && info.iteration
    // >= 0)". This is a request-local mirror of that child-wire boundary. getOpenWires() consumes
    // this child-wire ledger first. Result-slot topology remains locator evidence only; child-wire
    // output is materialized from producer/source/root/current-member ledger state.
    info.openWireCompoundWires.clear();
    if (materializationLedger.edgeEntries.size() < info.edges.size()) {
        materializationLedger.edgeEntries.resize(info.edges.size());
    }
    std::vector<TopoDS_Edge> splitFragmentProducerLedgerEdgesByEdgeInfo(info.edges.size());
    std::vector<std::vector<WireJoinerVmapReplacementEvent>>
        splitFragmentProducerLedgerEventsByEdgeInfo(info.edges.size());
    std::vector<TopoDS_Edge> splitFragmentProducerLedgerEdges = sourceEdgeLedgerEdges_;
    std::size_t nextVmapReplacementEventIndex = sourceEdgeLedgerReplacementEvents_.size();
    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
        const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
        if (edgeInfo.edge.IsNull() || !edgeInfo.sourceLineageFromSplitterHistory) {
            continue;
        }
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges() calls "add(split.edge, false, split.bbox, it)" before
        // "aHistory->AddModified(split.intersectShape, newInfo.edge)". Feed those split EdgeInfo rows
        // back into the same request-local vmap ledger used by ::add() so openWireCompound child-wire
        // materialization can consume split-fragment ownership instead of borrowing result-slot
        // endpoint evidence.
        std::vector<WireJoinerVmapReplacementEvent> splitReplacementEvents;
        const std::size_t affectedSourceEdgeIndex = edgeInfo.sourceEdgeIndices.size() == 1U
            ? edgeInfo.sourceEdgeIndices.front()
            : resultWireProducerNpos;
        TopoDS_Edge ledgerEdge =
            edgeWithLedgerVertexReplacements(
                edgeInfo.edge,
                splitFragmentProducerLedgerEdges,
                &splitReplacementEvents,
                affectedSourceEdgeIndex,
                edgeInfoIndex,
                true,
                true
            );
        if (ledgerEdge.IsNull()) {
            ledgerEdge = edgeInfo.edge;
        }
        for (WireJoinerVmapReplacementEvent& event : splitReplacementEvents) {
            event.eventIndex = nextVmapReplacementEventIndex++;
            splitFragmentProducerLedgerEventsByEdgeInfo[edgeInfoIndex].push_back(
                std::move(event)
            );
        }
        splitFragmentProducerLedgerEdgesByEdgeInfo[edgeInfoIndex] = ledgerEdge;
        splitFragmentProducerLedgerEdges.push_back(ledgerEdge);
    }
    auto producerLedgerEdgesFor = [&](std::size_t edgeInfoIndex) {
        std::vector<TopoDS_Edge> ledgerEdges = sourceEdgeLedgerEdges_;
        ledgerEdges.reserve(
            ledgerEdges.size()
            + splitFragmentProducerLedgerEdgesByEdgeInfo.size()
        );
        for (std::size_t splitEdgeInfoIndex = 0;
             splitEdgeInfoIndex < splitFragmentProducerLedgerEdgesByEdgeInfo.size();
             ++splitEdgeInfoIndex) {
            if (splitEdgeInfoIndex == edgeInfoIndex) {
                continue;
            }
            const TopoDS_Edge& ledgerEdge =
                splitFragmentProducerLedgerEdgesByEdgeInfo[splitEdgeInfoIndex];
            if (!ledgerEdge.IsNull()) {
                ledgerEdges.push_back(ledgerEdge);
            }
        }
        return ledgerEdges;
    };
    auto edgeInfoReferencesClosedSourceEdge = [&](const EdgeInfo& edgeInfo) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::splitEdges() re-adds fragments through EdgeInfo/aHistory, but closed source
        // edges still need the later FaceMaker/MapperHistory vertex identity path before cad-core can
        // safely replace result-slot endpoint evidence for their openWireCompound children.
        for (const std::size_t sourceEdgeIndex : edgeInfo.sourceEdgeIndices) {
            if (sourceEdgeIndex >= sourceEdges_.size()) {
                continue;
            }
            const TopoDS_Edge& sourceEdge = sourceEdges_[sourceEdgeIndex];
            if (!sourceEdge.IsNull() && BRep_Tool::IsClosed(sourceEdge)) {
                return true;
            }
        }
        return false;
    };
    auto vmapReplacementEventsFor = [&](std::size_t edgeInfoIndex,
                                        const std::vector<std::size_t>& sourceEdgeIndices) {
        std::vector<WireJoinerVmapReplacementEvent> events;
        for (const WireJoinerVmapReplacementEvent& event : sourceEdgeLedgerReplacementEvents_) {
            const bool sourceRelevant = std::find(
                                            sourceEdgeIndices.begin(),
                                            sourceEdgeIndices.end(),
                                            event.affectedSourceEdgeIndex
                                        )
                    != sourceEdgeIndices.end()
                || std::find(
                       sourceEdgeIndices.begin(),
                       sourceEdgeIndices.end(),
                       event.replacementSourceEdgeIndex
                   )
                    != sourceEdgeIndices.end();
            if (sourceEdgeIndices.empty() || sourceRelevant) {
                events.push_back(event);
            }
        }
        for (std::size_t splitEdgeInfoIndex = 0;
             splitEdgeInfoIndex < splitFragmentProducerLedgerEventsByEdgeInfo.size();
             ++splitEdgeInfoIndex) {
            if (splitEdgeInfoIndex != edgeInfoIndex
                && splitEdgeInfoIndex < info.edges.size()
                && !sourceEdgeIndicesIntersect(
                    info.edges[splitEdgeInfoIndex].sourceEdgeIndices,
                    sourceEdgeIndices
                )) {
                continue;
            }
            const std::vector<WireJoinerVmapReplacementEvent>& splitEvents =
                splitFragmentProducerLedgerEventsByEdgeInfo[splitEdgeInfoIndex];
            events.insert(events.end(), splitEvents.begin(), splitEvents.end());
        }
        return events;
    };
    auto recordEndpointProvenance = [&](OpenWireCompoundWireInfo& childWire,
                                        bool updateSourceVmapEndpointLedger) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
        // TopoDS_Vertex"; ::build() then emits the final "info.wire()" into openWireCompound.
        // Record per-output endpoint identity at the child-wire boundary instead of proving
        // endpoint debt from aggregate counts or result-slot output shape.
        const std::vector<TopoDS_Vertex> outputVertices = wireVertices(childWire.wire);
        const std::vector<TopoDS_Vertex> sourceVmapLedgerVertices =
            edgeEndpointVertices(producerLedgerEdgesFor(childWire.edgeIndex));
        std::vector<WireJoinerVmapReplacementEvent> replacementEvents =
            vmapReplacementEventsFor(childWire.edgeIndex, childWire.sourceEdgeIndices);
        const std::vector<TopoDS_Vertex>& candidateVertices =
            childWire.currentMemberSplitLedgerCandidateVertices;

        childWire.endpointProvenance.clear();
        childWire.vmapReplacementEvents.clear();
        childWire.endpointProvenance.reserve(outputVertices.size());
        for (std::size_t outputVertexIndex = 0; outputVertexIndex < outputVertices.size();
             ++outputVertexIndex) {
            const TopoDS_Vertex& vertex = outputVertices[outputVertexIndex];
            OpenWireCompoundWireInfo::EndpointProvenance provenance;
            provenance.outputVertex = vertex;
            const auto replacementEventIt = std::find_if(
                replacementEvents.begin(),
                replacementEvents.end(),
                [&](const WireJoinerVmapReplacementEvent& event) {
                    return !vertex.IsNull() && !event.newSharedVertex.IsNull()
                        && vertex.IsSame(event.newSharedVertex);
                }
            );
            if (replacementEventIt != replacementEvents.end()) {
                provenance.matchedVmapReplacementLedger = true;
                provenance.vmapReplacementEventIndex = replacementEventIt->eventIndex;
                WireJoinerVmapReplacementEvent childEvent = *replacementEventIt;
                childEvent.affectedChildWireEdgeInfoIndex = childWire.edgeIndex;
                childEvent.affectedChildWireEndpoint =
                    static_cast<int>(outputVertexIndex);
                childWire.vmapReplacementEvents.push_back(std::move(childEvent));
            }
            provenance.matchedSourceVmapLedger =
                vertexMatchesAnyByIdentity(vertex, sourceVmapLedgerVertices)
                || provenance.matchedVmapReplacementLedger;
            provenance.matchedCurrentMemberCandidateLedger =
                vertexMatchesAnyByIdentity(vertex, candidateVertices);
            childWire.endpointProvenance.push_back(std::move(provenance));
        }
        childWire.vmapReplacementEventCount = childWire.vmapReplacementEvents.size();
        childWire.endpointProvenanceRecorded = !childWire.endpointProvenance.empty();
        childWire.endpointProvenanceOutputVertexCount =
            childWire.endpointProvenance.size();
        childWire.endpointProvenanceSourceVmapMatchedVertexCount =
            static_cast<std::size_t>(std::count_if(
                childWire.endpointProvenance.begin(),
                childWire.endpointProvenance.end(),
                [](const OpenWireCompoundWireInfo::EndpointProvenance& provenance) {
                    return provenance.matchedSourceVmapLedger;
                }
            ));
        childWire.endpointProvenanceVmapReplacementMatchedVertexCount =
            static_cast<std::size_t>(std::count_if(
                childWire.endpointProvenance.begin(),
                childWire.endpointProvenance.end(),
                [](const OpenWireCompoundWireInfo::EndpointProvenance& provenance) {
                    return provenance.matchedVmapReplacementLedger;
                }
            ));
        childWire.endpointProvenanceCandidateMatchedVertexCount =
            static_cast<std::size_t>(std::count_if(
                childWire.endpointProvenance.begin(),
                childWire.endpointProvenance.end(),
                [](const OpenWireCompoundWireInfo::EndpointProvenance& provenance) {
                    return provenance.matchedCurrentMemberCandidateLedger;
                }
            ));
        childWire.endpointProvenanceUnmatchedVertexCount =
            static_cast<std::size_t>(std::count_if(
                childWire.endpointProvenance.begin(),
                childWire.endpointProvenance.end(),
                [](const OpenWireCompoundWireInfo::EndpointProvenance& provenance) {
                    return !provenance.matchedSourceVmapLedger
                        && !provenance.matchedVmapReplacementLedger
                        && !provenance.matchedCurrentMemberCandidateLedger;
                }
            ));
        if (updateSourceVmapEndpointLedger) {
            childWire.sourceVmapEndpointLedgerRecorded =
                !outputVertices.empty() && !sourceVmapLedgerVertices.empty();
            childWire.sourceVmapEndpointLedgerOutputVertexCount = outputVertices.size();
            childWire.sourceVmapEndpointLedgerMatchedVertexCount =
                childWire.endpointProvenanceSourceVmapMatchedVertexCount;
        }
    };
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        const EdgeInfo& edgeInfo = info.edges[edgeIndex];
        WireJoinerHistoryMaterializationEdgeEntry& materializationEntry =
            materializationLedger.edgeEntries[edgeIndex];
        const bool exportsOpenEdge = edgeInfoHasOpenWireCompoundLedgerSlot(
            edgeInfo,
            wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
                materializationLedger,
                edgeIndex
            )
        );
        if (!exportsOpenEdge) {
            continue;
        }

        OpenWireCompoundWireInfo childWire;
        childWire.edgeIndex = edgeIndex;
        childWire.edgeInfoIteration = edgeInfo.iteration;
        childWire.edgeInfoIteration2 = edgeInfo.iteration2;
        childWire.ownerWireInfo = edgeInfo.wireInfo;
        childWire.ownerWireInfo2 = edgeInfo.wireInfo2;
        childWire.openLeafIterationMinus3 = edgeInfo.iteration == -3;
        childWire.unownedOpenEdge = edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0;
        if (childWire.openLeafIterationMinus3) {
            childWire.openExportSource = OpenWireCompoundExportSource::OpenLeafIterationMinus3;
        }
        else if (childWire.unownedOpenEdge) {
            childWire.openExportSource = OpenWireCompoundExportSource::UnownedOpenEdge;
        }
        const bool hasHistoryMaterializationProducerOpenExportShape =
            wireJoinerMapperHistoryProducerEvidenceReady(
                materializationLedger,
                edgeIndex
            );
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() stores final child wires in openWireCompound with
        // "builder.Add(openWireCompound, info.wire())", then ::getOpenWires() consumes
        // MapperHistory(aHistory). Use the source/aHistory producer shape only to materialize the
        // child-wire producer wire; do not store a copied producer shape on the child-wire ledger.
        const TopoDS_Edge* historyMaterializationProducerEdge =
            hasHistoryMaterializationProducerOpenExportShape
            ? wireJoinerMapperHistoryProducerEvidenceEdge(
                materializationLedger,
                edgeIndex
            )
            : nullptr;
        const bool hasChildProducerLedgerEdge =
            historyMaterializationProducerEdge && !historyMaterializationProducerEdge->IsNull();
        const TopoDS_Edge& childProducerEdge = hasChildProducerLedgerEdge
            ? *historyMaterializationProducerEdge
            : edgeInfo.edge;
        if (hasChildProducerLedgerEdge) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() exports the final child through "info.wire()" into
            // openWireCompound, and ::getOpenWires() later consumes that compound with
            // MapperHistory(aHistory). The materialized output wire belongs to the child-wire ledger;
            // the scoped producer shape is request-local MapperHistory input and is not published as
            // a per-edge output bridge.
            childWire.producerLedgerWire
                = BRepBuilderAPI_MakeWire(childProducerEdge).Wire();
            childWire.producerLedgerWireBuilt = !childWire.producerLedgerWire.IsNull();
            childWire.wire = childWire.producerLedgerWireBuilt
                ? childWire.producerLedgerWire
                : edgeInfo.wire();
        }
        else {
            childWire.wire = edgeInfo.wire();
        }
        const bool currentMemberProducerShape = hasChildProducerLedgerEdge
            && materializationEntry.resultWireProducer.kind
                == ResultWireProducerKind::CurrentMemberChildWire;
        const bool aHistoryProducerChildWire =
            wireJoinerMapperHistoryProducerEvidenceHasChildWire(
                materializationLedger,
                edgeIndex
            );
        if (wireJoinerHistoryMaterializationLedgerHasChildWireCandidate(
                materializationLedger,
                edgeIndex
            )
            && (!aHistoryProducerChildWire || currentMemberProducerShape)
            && !sourceEdgeLedgerEdges_.empty()) {
            // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::add(), key: "Make sure coincident vertices are actually the same
            // TopoDS_Vertex", replaces endpoints through the mutable vmap/sourceEdges ledger before
            // ::build() exports final openWireCompound children. Prefer that vmap ledger for the child
            // edge when its endpoint identities are compatible with the current result-slot endpoint
            // evidence.
            // Current-member children are rebuilt later from root superEdge/current-member child
            // ledger fields, so they must not borrow result-slot endpoint evidence during this
            // provisional child-wire materialization.
            const std::vector<TopoDS_Edge> producerLedgerEdges = producerLedgerEdgesFor(edgeIndex);
            std::optional<TopoDS_Edge> producerChildEdge;
            producerChildEdge = edgeWithLedgerVertexReplacements(
                childProducerEdge,
                producerLedgerEdges
            );
            bool producerChildEdgeFromSourceVmap = producerChildEdge && !producerChildEdge->IsNull();
            if (producerChildEdge && !producerChildEdge->IsNull()) {
                childWire.producerLedgerWire = BRepBuilderAPI_MakeWire(*producerChildEdge).Wire();
                childWire.producerLedgerWireBuilt = !childWire.producerLedgerWire.IsNull();
                if (childWire.producerLedgerWireBuilt) {
                    childWire.wire = childWire.producerLedgerWire;
                    childWire.producerLedgerWireFromSourceVmap =
                        producerChildEdgeFromSourceVmap;
                }
            }
        }
        childWire.wireBuilt = !childWire.wire.IsNull();
        childWire.superEdgeWire = !edgeInfo.superEdge.IsNull() && !hasChildProducerLedgerEdge;
        childWire.sourceEdgeIndices = edgeInfo.sourceEdgeIndices;
        childWire.sourceLineageFromSplitterHistory = edgeInfo.sourceLineageFromSplitterHistory;
        childWire.splitFragmentSourceEdgeIndices = edgeInfo.splitFragmentSourceEdgeIndices;
        childWire.splitFragmentModifiedSourceEdgeIndices
            = edgeInfo.splitFragmentModifiedSourceEdgeIndices;
        childWire.splitFragmentGeneratedSourceEdgeIndices
            = edgeInfo.splitFragmentGeneratedSourceEdgeIndices;
        childWire.splitFragmentFromModifiedHistory = edgeInfo.splitFragmentFromModifiedHistory;
        childWire.splitFragmentFromGeneratedHistory = edgeInfo.splitFragmentFromGeneratedHistory;
        childWire.splitFragmentSourceLineageFromIdentityFallback
            = edgeInfo.splitFragmentSourceLineageFromIdentityFallback;
        childWire.splitFragmentSourceLineageFromSourceIdentityFallback
            = edgeInfo.splitFragmentSourceLineageFromSourceIdentityFallback;
        childWire.splitFragmentHistoryShapeGeometryBridge
            = edgeInfo.splitFragmentHistoryShapeGeometryBridge;
        childWire.splitFromInputEdge = edgeInfo.splitFromInputEdge;
        childWire.sourceVertexIdentity = edgeInfo.sourceVertexIdentity;
        childWire.sourceVertexReplacementSourceEdgeIndices
            = edgeInfo.sourceVertexReplacementSourceEdgeIndices;
        childWire.sourceVertexReplacementEndpoints = edgeInfo.sourceVertexReplacementEndpoints;
        childWire.sourceVertexReplacementIdentity = edgeInfo.sourceVertexReplacementIdentity;
        recordEndpointProvenance(childWire, true);
        childWire.superEdgeRootEdgeInfoIndex
            = materializationEntry.superEdgeRootIndex;
        childWire.superEdgeRootOpenWireCompoundEligible
            = materializationEntry.superEdgeRootOpenWireCompoundEligible;
        childWire.rootResultWireProducerCandidate
            = materializationEntry.superEdgeRootProducerCandidate;
        childWire.rootResultWireProducerUnownedRemovalReady
            = materializationEntry.superEdgeRootProducerUnownedRemovalChildWireReady;
        childWire.currentMemberEdgeInfo
            = materializationEntry.superEdgeRootCurrentMember;
        childWire.rootCurrentMemberChildProducer = childWire.currentMemberEdgeInfo
            || materializationEntry.superEdgeMember
            || materializationEntry.superEdgeRootOpenLifecycle;
        childWire.rootResultWireProducerCoveredMemberEdgeInfoIndices
            = materializationEntry.superEdgeRootCoveredMemberIndices;
        if (childWire.superEdgeRootEdgeInfoIndex < info.edges.size()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::findSuperEdgesUpdateFirst() materializes the open root with
            // "first->superEdge = makeCleanWire(false)"; ::build() later emits child wires with
            // "builder.Add(openWireCompound, info.wire())". Once the root is from a branch that has
            // been migrated (P3 unowned, P4 primary) and carries full aHistory producer evidence,
            // the child-wire ledger can export that root producer wire directly instead of the
            // transitional result-wire producer candidate.
            const EdgeInfo& rootEdgeInfo
                = info.edges[childWire.superEdgeRootEdgeInfoIndex];
            const bool rootOpenCurrentMemberChildWireProducerReady
                = !childWire.rootResultWireProducerCandidate
                && childWire.currentMemberEdgeInfo
                && materializationEntry.superEdgeRootOpenLifecycle
                && childWire.superEdgeRootOpenWireCompoundEligible
                && rootEdgeInfo.superEdgeMaterialized && !rootEdgeInfo.superEdge.IsNull();
            if (childWire.rootResultWireProducerCandidate
                || rootOpenCurrentMemberChildWireProducerReady) {
                childWire.rootResultWireProducerWire
                    = rootEdgeInfo.superEdge;
                const bool primaryBranchChildWireProducerReady
                    = materializationEntry.superEdgeRootProducerPrimaryRemoval
                    && materializationEntry.superEdgeRootProducerFullAHistoryEvidence;
                const bool secondaryBranchChildWireProducerReady
                    = materializationEntry.superEdgeRootProducerSecondaryRemoval
                    && materializationEntry.superEdgeRootProducerFullAHistoryEvidence;
                const bool rootBranchChildWireProducerReady
                    = childWire.rootResultWireProducerUnownedRemovalReady
                    || primaryBranchChildWireProducerReady || secondaryBranchChildWireProducerReady
                    || rootOpenCurrentMemberChildWireProducerReady;
                const bool useRootResultWireProducer = rootBranchChildWireProducerReady
                    && !childWire.rootResultWireProducerWire.IsNull();
                const bool rootProducerIsSingleMember = rootEdgeInfo.superEdgeMemberCount <= 1U;
                if (useRootResultWireProducer && rootProducerIsSingleMember) {
                    childWire.wire
                        = childWire.rootResultWireProducerWire;
                    childWire.wireBuilt = true;
                    childWire.superEdgeWire = true;
                }
                else if (useRootResultWireProducer) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::findSuperEdgesUpdateFirst() stores a multi-member
                    // "superEdge" on the root. Exporting that whole root from a member producer
                    // child-wire carries sibling members; this needs a formal child-wire member
                    // suppression step before it can replace the result-wire candidate shape.
                    childWire.rootResultWireProducerRequiresMemberSuppression
                        = true;
                }
            }
        }
        updateOpenWireCompoundNoOriginalPurgeVerdict(childWire);
        info.openWireCompoundWires.push_back(std::move(childWire));
    }

    struct MemberSuppressionOutputGroup
    {
        std::size_t rootEdgeInfoIndex = 0;
        std::vector<std::size_t> childWireIndices;
        std::vector<std::size_t> coveredMemberEdgeInfoIndices;
        std::vector<std::size_t> currentMemberEdgeInfoIndices;
        std::vector<std::size_t> suppressedPendingMemberEdgeInfoIndices;
    };
    auto outputGroupFor = [](std::vector<MemberSuppressionOutputGroup>& groups,
                             std::size_t rootEdgeInfoIndex) -> MemberSuppressionOutputGroup& {
        const auto groupIt = std::find_if(
            groups.begin(),
            groups.end(),
            [&](const MemberSuppressionOutputGroup& group) {
                return group.rootEdgeInfoIndex == rootEdgeInfoIndex;
            }
        );
        if (groupIt != groups.end()) {
            return *groupIt;
        }
        groups.push_back(MemberSuppressionOutputGroup {rootEdgeInfoIndex, {}, {}, {}, {}});
        return groups.back();
    };

    auto memberHasRequestLocalSourceEdgeProducerChild = [&](std::size_t memberIndex) {
        return std::any_of(
            info.openWireCompoundWires.begin(),
            info.openWireCompoundWires.end(),
            [&](const OpenWireCompoundWireInfo& ledgerChildWire) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() exports each final open child with
                // "builder.Add(openWireCompound, info.wire())". If a non-current root member is
                // already emitted as a source-edge producer child in the same request ledger,
                // suppressing it from the current-member root export reflects completed child
                // ownership rather than output-side sibling pruning.
                return ledgerChildWire.edgeIndex == memberIndex
                    && openWireCompoundChildWireHasSourceEdgeProducerOutput(ledgerChildWire);
            }
        );
    };

    std::vector<MemberSuppressionOutputGroup> memberSuppressionOutputGroups;
    for (std::size_t childWireIndex = 0; childWireIndex < info.openWireCompoundWires.size();
         ++childWireIndex) {
        OpenWireCompoundWireInfo& childWire = info.openWireCompoundWires[childWireIndex];
        if (!childWire.rootResultWireProducerRequiresMemberSuppression) {
            continue;
        }
        if (childWire.edgeIndex >= info.edges.size()) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdgesUpdateFirst() suppresses member edges with
        // "current->iteration = -1" and stores the full root "superEdge". Build a request-local
        // current-member producer candidate here, but keep it out of output until every member in
        // the same root group has an explicit child owner.
        TopoDS_Wire memberSuppressedWire;
        if (childWire.superEdgeRootEdgeInfoIndex < info.edges.size()) {
            const EdgeInfo& rootEdgeInfo
                = info.edges[childWire.superEdgeRootEdgeInfoIndex];
            std::vector<TopoDS_Vertex> ledgerVertices;
            for (const OpenWireCompoundWireInfo& ledgerChildWire : info.openWireCompoundWires) {
                if (ledgerChildWire.wire.IsNull()) {
                    continue;
                }
                const std::vector<TopoDS_Vertex> vertices = wireVertices(ledgerChildWire.wire);
                ledgerVertices.insert(ledgerVertices.end(), vertices.begin(), vertices.end());
            }
            if (childWire.currentMemberEdgeInfo
                && rootEdgeInfo.superEdgeLifecycleOpenRoot
                && childWire.edgeIndex < info.edges.size()) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() builds the root superEdge from member
                // EdgeInfo shapes before ::build() emits openWireCompound. Include the current
                // member input vertices when rebuilding a request-local child-wire from that root
                // producer; this preserves producer identity without accepting arbitrary coordinate
                // matches from outside the WireJoiner ledger.
                const std::vector<TopoDS_Vertex> memberVertices = edgeVertices(
                    info.edges[childWire.edgeIndex].edge
                );
                ledgerVertices
                    .insert(ledgerVertices.end(), memberVertices.begin(), memberVertices.end());
            }
            if (!sourceEdges_.empty()) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::getOpenWires(noOriginal=true) removes source wires after
                // "first->superEdge" has been built. When both an original source vertex and a
                // request-local child-wire vertex exist at the same point, rebuild the current
                // member from the non-original ledger vertex so the root producer does not collapse
                // back into a wire that the noOriginal pass would purge.
                std::stable_partition(
                    ledgerVertices.begin(),
                    ledgerVertices.end(),
                    [this](const TopoDS_Vertex& vertex) {
                        return !vertexIsOriginalSourceByIdentity(vertex, sourceEdges_);
                    }
                );
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::findSuperEdgesUpdateFirst() gathers member shapes into wireData and
            // stores "first->superEdge = makeCleanWire(false)"; ::makeCleanWire() merges
            // ShapeFix_Wire history into aHistory. ::build() later exports final child wires with
            // "builder.Add(openWireCompound, info.wire())". For current-member output in migrated
            // branches, match that current child-wire geometry against the clean root producer and
            // reuse request-local child-wire vertices when they are already present, so the
            // producer shape can enter the ledger without creating new topological vertices.
            memberSuppressedWire = currentMemberWireFromRootSuperEdge(
                rootEdgeInfo.superEdge,
                childWire.wire,
                info.edges[childWire.edgeIndex].edge,
                ledgerVertices
            );
            const EdgeInfo& currentEdgeInfo = info.edges[childWire.edgeIndex];
            if (childWire.currentMemberEdgeInfo
                && rootEdgeInfo.superEdgeLifecycleOpenRoot
                && edgeInfoReferencesClosedSourceEdge(currentEdgeInfo)
                && !childWire.wire.IsNull()) {
                std::vector<TopoDS_Vertex> memberLedgerVertices;
                auto appendVertices = [](std::vector<TopoDS_Vertex>& target,
                                         const std::vector<TopoDS_Vertex>& vertices) {
                    target.insert(target.end(), vertices.begin(), vertices.end());
                };
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() feeds member "shape(...)" into wireData
                // before "first->superEdge = makeCleanWire(false)"; ::splitEdges() records
                // "aHistory->AddModified(split.intersectShape, newInfo.edge)". Build this candidate
                // from member/split and formal child-output vertices, and only let it become output
                // when every candidate endpoint is backed by that request-local ledger.
                appendVertices(memberLedgerVertices, edgeVertices(currentEdgeInfo.edge));
                if (childWire.edgeIndex < splitFragmentProducerLedgerEdgesByEdgeInfo.size()) {
                    const TopoDS_Edge& splitLedgerEdge =
                        splitFragmentProducerLedgerEdgesByEdgeInfo[childWire.edgeIndex];
                    if (!splitLedgerEdge.IsNull()) {
                        appendVertices(memberLedgerVertices, edgeVertices(splitLedgerEdge));
                    }
                }
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same
                // TopoDS_Vertex", and ::findSuperEdgesUpdateFirst() later stores
                // "first->superEdge". For current-member candidates, prefer already-emitted child-wire
                // vertices from already materialized children; result-slot endpoint materialization no
                // longer supplies producer child-wire output.
                std::vector<TopoDS_Vertex> formalSharedChildOutputVertices;
                for (const OpenWireCompoundWireInfo& ledgerChildWire : info.openWireCompoundWires) {
                    if (ledgerChildWire.edgeIndex == childWire.edgeIndex
                        || ledgerChildWire.wire.IsNull()) {
                        continue;
                    }
                    appendVertices(formalSharedChildOutputVertices, wireVertices(ledgerChildWire.wire));
                }
                std::vector<TopoDS_Vertex> candidateLedgerVertices = formalSharedChildOutputVertices;
                appendVertices(candidateLedgerVertices, memberLedgerVertices);
                const TopoDS_Wire candidateWire = currentMemberWireFromRootSuperEdge(
                    rootEdgeInfo.superEdge,
                    childWire.wire,
                    currentEdgeInfo.edge,
                    candidateLedgerVertices
                );
                if (!candidateWire.IsNull() && !memberLedgerVertices.empty()) {
                    const std::vector<TopoDS_Vertex> candidateVertices =
                        wireVertices(candidateWire);
                    const bool candidateUsesFormalCurrentMemberLedger =
                        !candidateVertices.empty()
                        && std::all_of(
                            candidateVertices.begin(),
                            candidateVertices.end(),
                            [&](const TopoDS_Vertex& vertex) {
                                return vertexMatchesAnyByIdentity(vertex, memberLedgerVertices)
                                    || vertexMatchesAnyByIdentity(
                                        vertex,
                                        formalSharedChildOutputVertices
                                    );
                            }
                        );
                    const std::vector<TopoDS_Vertex> outputVertices =
                        candidateUsesFormalCurrentMemberLedger
                        ? candidateVertices
                        : wireVertices(childWire.wire);
                    // FreeCAD:
                    // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                    // ::WireJoinerP::build() stores final "info.wire()" children in
                    // openWireCompound before ::getOpenWires() maps them with MapperHistory(aHistory).
                    // Count transitional endpoint debt from the materialized child-wire ledger, not by
                    // re-reading the EdgeInfo result-slot evidence sidecar.
                    std::vector<TopoDS_Vertex> otherOutputVertices;
                    for (const OpenWireCompoundWireInfo& ledgerChildWire :
                         info.openWireCompoundWires) {
                        if (ledgerChildWire.edgeIndex == childWire.edgeIndex
                            || ledgerChildWire.wire.IsNull()) {
                            continue;
                        }
                        const std::vector<TopoDS_Vertex> vertices =
                            wireVertices(ledgerChildWire.wire);
                        for (const TopoDS_Vertex& otherVertex : vertices) {
                            appendUniqueVertexByIdentity(otherOutputVertices, otherVertex);
                        }
                    }
                    childWire.currentMemberSplitLedgerMemberVertices = memberLedgerVertices;
                    childWire.currentMemberSplitLedgerCandidateVertices = candidateVertices;
                    childWire.currentMemberSplitLedgerOutputVertexDebt.clear();
                    childWire.currentMemberSplitLedgerOutputVertexDebt.reserve(
                        outputVertices.size()
                    );
                    auto indexedVertexIdentity = [](const std::vector<TopoDS_Vertex>& vertices,
                                                    std::size_t vertexIndex,
                                                    const TopoDS_Vertex& outputVertex,
                                                    bool matchedAny,
                                                    const char* missingReason) {
                        if (vertexIndex >= vertices.size() || vertices[vertexIndex].IsNull()) {
                            return matchedAny
                                ? std::string("same_as_current_child_wire_output_other_endpoint")
                                : std::string(missingReason);
                        }
                        if (outputVertex.IsSame(vertices[vertexIndex])) {
                            return std::string("same_as_current_child_wire_output");
                        }
                        if (matchedAny) {
                            return std::string("same_as_current_child_wire_output_other_endpoint");
                        }
                        return std::string("different_from_current_child_wire_output");
                    };
                    for (std::size_t outputVertexIndex = 0;
                         outputVertexIndex < outputVertices.size();
                         ++outputVertexIndex) {
                        const TopoDS_Vertex& vertex = outputVertices[outputVertexIndex];
                        OpenWireCompoundWireInfo::CurrentMemberSplitLedgerVertexDebt debt;
                        debt.outputVertexIndex = outputVertexIndex;
                        debt.outputVertex = vertex;
                        debt.matchedMemberSplitLedger =
                            vertexMatchesAnyByIdentity(vertex, memberLedgerVertices);
                        debt.matchedCandidateLedger =
                            vertexMatchesAnyByIdentity(vertex, candidateVertices);
                        debt.currentChildWireOutputVertexMatchesOtherOutput =
                            vertexMatchesAnyByIdentity(vertex, otherOutputVertices);
                        if (outputVertexIndex < candidateVertices.size()
                            && !candidateVertices[outputVertexIndex].IsNull()) {
                            debt.candidateWireVertexMatchesOtherOutput =
                                vertexMatchesAnyByIdentity(
                                    candidateVertices[outputVertexIndex],
                                    otherOutputVertices
                                );
                        }
                        debt.currentChildWireOutputVertexIdentity =
                            vertex.IsNull() ? "missing_current_child_wire_output_vertex"
                                            : "current_child_wire_output_vertex";
                        debt.memberSplitLedgerVertexIdentity = debt.matchedMemberSplitLedger
                            ? "same_as_current_child_wire_output"
                            : "no_identity_match";
                        debt.candidateWireVertexIdentity = indexedVertexIdentity(
                            candidateVertices,
                            outputVertexIndex,
                            vertex,
                            debt.matchedCandidateLedger,
                            "missing_candidate_wire_endpoint"
                        );
                        if (!debt.matchedMemberSplitLedger && !debt.matchedCandidateLedger) {
                            debt.explanation =
                                "output_endpoint_missing_member_split_and_candidate_identity";
                        }
                        else if (!debt.matchedMemberSplitLedger) {
                            debt.explanation =
                                "output_endpoint_matches_candidate_but_not_member_split_identity";
                        }
                        else if (!debt.matchedCandidateLedger) {
                            debt.explanation =
                                "output_endpoint_matches_member_split_but_not_candidate_identity";
                        }
                        else {
                            debt.explanation = "output_endpoint_matches_member_split_and_candidate_identity";
                        }
                        if (!debt.matchedCandidateLedger
                            && debt.currentChildWireOutputVertexMatchesOtherOutput
                            && !debt.candidateWireVertexMatchesOtherOutput) {
                            debt.mismatchReason =
                                "candidate_wire_endpoint_does_not_preserve_current_shared_output_identity";
                        }
                        else if (!debt.matchedCandidateLedger
                            && debt.candidateWireVertexMatchesOtherOutput
                            && !debt.currentChildWireOutputVertexMatchesOtherOutput) {
                            debt.mismatchReason =
                                "candidate_wire_endpoint_reuses_other_child_output_vertex_while_current_output_is_unmatched";
                        }
                        else {
                            debt.mismatchReason = debt.explanation;
                        }
                        childWire.currentMemberSplitLedgerOutputVertexDebt.push_back(
                            std::move(debt)
                        );
                    }
                    childWire.currentMemberSplitLedgerCandidateVertexCount =
                        childWire.currentMemberSplitLedgerCandidateVertices.size();
                    childWire.currentMemberSplitLedgerOutputVertexCount =
                        childWire.currentMemberSplitLedgerOutputVertexDebt.size();
                    childWire.currentMemberSplitLedgerOutputMatchedVertexCount =
                        static_cast<std::size_t>(std::count_if(
                            childWire.currentMemberSplitLedgerOutputVertexDebt.begin(),
                            childWire.currentMemberSplitLedgerOutputVertexDebt.end(),
                            [](const OpenWireCompoundWireInfo::CurrentMemberSplitLedgerVertexDebt&
                                   debt) { return debt.matchedMemberSplitLedger; }
                        ));
                    childWire.currentMemberSplitLedgerOutputCandidateMatchedVertexCount =
                        static_cast<std::size_t>(std::count_if(
                            childWire.currentMemberSplitLedgerOutputVertexDebt.begin(),
                            childWire.currentMemberSplitLedgerOutputVertexDebt.end(),
                            [](const OpenWireCompoundWireInfo::CurrentMemberSplitLedgerVertexDebt&
                                   debt) { return debt.matchedCandidateLedger; }
                        ));
                    childWire.currentMemberSplitLedgerOutputUnmatchedVertexCount =
                        static_cast<std::size_t>(std::count_if(
                            childWire.currentMemberSplitLedgerOutputVertexDebt.begin(),
                            childWire.currentMemberSplitLedgerOutputVertexDebt.end(),
                            [](const OpenWireCompoundWireInfo::CurrentMemberSplitLedgerVertexDebt&
                                   debt) {
                                return !debt.matchedMemberSplitLedger
                                    && !debt.matchedCandidateLedger;
                            }
                        ));
                    childWire.currentMemberSplitLedgerVertexCandidate =
                        candidateUsesFormalCurrentMemberLedger;
                    if (childWire.currentMemberSplitLedgerVertexCandidate
                        && childWire.currentMemberSplitLedgerOutputVertexCount > 0U
                        && childWire.currentMemberSplitLedgerOutputCandidateMatchedVertexCount
                            == childWire.currentMemberSplitLedgerOutputVertexCount) {
                        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                        // ::WireJoinerP::build() exports the final "info.wire()" after the
                        // current-member superEdge has been formed. When every current output endpoint
                        // is covered by the request-local candidate ledger and no endpoint remains
                        // result-slot-only, the candidate wire is the child-wire producer shape.
                        memberSuppressedWire = candidateWire;
                    }
                    childWire.currentMemberSplitLedgerVertexMultiplicityBlocked =
                        childWire.currentMemberSplitLedgerVertexCandidate
                        && childWire.currentMemberSplitLedgerOutputUnmatchedVertexCount > 0U;
                }
            }
        }
        if (memberSuppressedWire.IsNull()) {
            memberSuppressedWire = info.edges[childWire.edgeIndex].wire();
        }
        childWire.currentMemberProducerWire
            = memberSuppressedWire;
        childWire.currentMemberProducerWireBuilt
            = !childWire.currentMemberProducerWire
                   .IsNull();

        MemberSuppressionOutputGroup& group = outputGroupFor(
            memberSuppressionOutputGroups,
            childWire.superEdgeRootEdgeInfoIndex
        );
        appendUniqueSourceIndex(group.childWireIndices, childWireIndex);
        appendUniqueSourceIndices(
            group.coveredMemberEdgeInfoIndices,
            childWire.rootResultWireProducerCoveredMemberEdgeInfoIndices
        );
        if (childWire.currentMemberEdgeInfo) {
            appendUniqueSourceIndex(group.currentMemberEdgeInfoIndices, childWire.edgeIndex);
        }
    }

    for (const MemberSuppressionOutputGroup& group : memberSuppressionOutputGroups) {
        std::vector<std::size_t> pendingMemberEdgeInfoIndices;
        for (const std::size_t memberIndex : group.coveredMemberEdgeInfoIndices) {
            if (std::find(
                    group.currentMemberEdgeInfoIndices.begin(),
                    group.currentMemberEdgeInfoIndices.end(),
                    memberIndex
                )
                == group.currentMemberEdgeInfoIndices.end()) {
                if (memberIndex < info.edges.size()
                    && (memberHasRequestLocalSourceEdgeProducerChild(memberIndex)
                        || resultWireProducerRootCanSuppressPendingMember(
                            info.edges[memberIndex]
                        ))) {
                    continue;
                }
                appendUniqueSourceIndex(pendingMemberEdgeInfoIndices, memberIndex);
            }
        }
        const bool groupChildOwnershipComplete = pendingMemberEdgeInfoIndices.empty();
        for (const std::size_t childWireIndex : group.childWireIndices) {
            if (childWireIndex >= info.openWireCompoundWires.size()) {
                continue;
            }
            OpenWireCompoundWireInfo& childWire = info.openWireCompoundWires[childWireIndex];
            if (
                !childWire.currentMemberProducerWireBuilt
            ) {
                continue;
            }
            if (!groupChildOwnershipComplete) {
                childWire.currentMemberProducerBlockedByPendingMember
                    = true;
                continue;
            }
            WireJoinerHistoryMaterializationEdgeEntry& currentMaterializationEntry =
                materializationLedger.edgeEntries[childWire.edgeIndex];
            const bool currentMemberPrimaryBranchChildWireProducerReady
                = currentMaterializationEntry.superEdgeRootProducerPrimaryRemoval
                && currentMaterializationEntry.superEdgeRootProducerFullAHistoryEvidence;
            const bool currentMemberSecondaryBranchChildWireProducerReady
                = currentMaterializationEntry.superEdgeRootProducerSecondaryRemoval
                && currentMaterializationEntry.superEdgeRootProducerFullAHistoryEvidence;
            const bool currentMemberBranchChildWireProducerReady
                = childWire.rootResultWireProducerUnownedRemovalReady
                || currentMemberPrimaryBranchChildWireProducerReady
                || currentMemberSecondaryBranchChildWireProducerReady;
            const bool currentMemberRootOpenChildWireProducerReady
                = childWire.currentMemberEdgeInfo
                && childWire.superEdgeRootEdgeInfoIndex < info.edges.size()
                && currentMaterializationEntry.superEdgeRootOpenLifecycle
                && childWire.superEdgeRootOpenWireCompoundEligible
                && info.edges[childWire.superEdgeRootEdgeInfoIndex].superEdgeMaterialized
                && !info.edges[childWire.superEdgeRootEdgeInfoIndex]
                        .superEdge.IsNull();
            childWire.currentMemberChildWireProducerReady
                = (currentMemberBranchChildWireProducerReady
                   || currentMemberRootOpenChildWireProducerReady)
                && childWire.currentMemberEdgeInfo;
            childWire.currentMemberChildWireProducerFullAHistoryEvidence
                = currentMemberBranchChildWireProducerReady
                && childWire.currentMemberEdgeInfo;
            const bool memberSuppressedProducerLedgerReady
                = openWireCompoundChildWireHasSourceEdgeProducerOutput(childWire)
                || memberSuppressedCurrentMemberProducerLedgerReady(info, childWire, childWireIndex);
            if (childWire.currentMemberSplitLedgerVertexMultiplicityBlocked) {
                // FreeCAD:
                // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() exports final "info.wire()" only after the EdgeInfo /
                // superEdge lifecycle has stabilized; ::getOpenWires() then maps it with
                // MapperHistory(aHistory). The current member/split candidate is only diagnostic
                // while it would change the downstream vertex multiplicity.
                childWire.currentMemberProducerBlockedByVertexMultiplicity = true;
                continue;
            }
            if (!memberSuppressedProducerLedgerReady) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() exports the exact final child wire identity. Even after
                // every non-current member is formally suppressible, cad-core cannot replace the
                // result-wire candidate child shape with EdgeInfo::wire() until M2/source-edge child-wire identity
                // and getOpenWires(noOriginal=true) purge behavior are ready for this child.
                childWire.currentMemberProducerBlockedBySourceShape
                    = true;
                continue;
            }
            childWire.wire
                = childWire.currentMemberProducerWire;
            childWire.wireBuilt = true;
            childWire.superEdgeWire = false;
            childWire.currentMemberProducerOutput
                = true;
            updateOpenWireCompoundNoOriginalPurgeVerdict(childWire);
        }
    }

    for (std::size_t childWireIndex = 0; childWireIndex < info.openWireCompoundWires.size();
         ++childWireIndex) {
        OpenWireCompoundWireInfo& childWire = info.openWireCompoundWires[childWireIndex];
        recordEndpointProvenance(childWire, false);
        const std::vector<TopoDS_Edge> childWireEdges = wireEdges(childWire.wire);
        childWire.childShapeIdentityRecorded = childWire.wireBuilt && !childWire.wire.IsNull();
        childWire.childWireEdgeCount = childWireEdges.size();
        childWire.childWireVertexCount = wireVertices(childWire.wire).size();
        childWire.openExportSource = childWireFinalOpenExportSource(childWire);
        childWire.resultWireProducer
            = childWireResultWireProducerIdentity(
                info,
                childWire,
                childWireIndex,
                materializationLedger
            );
        childWire.resultWireProducerLedgerEntry =
            resultWireProducerIdentityPublishesChildWireLedgerEntry(
                childWire.resultWireProducer
            );
        if (childWire.edgeIndex < info.edges.size() && childWire.resultWireProducerLedgerEntry) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() emits final open-wire children with
            // "builder.Add(openWireCompound, info.wire())". Keep the edge-level producer identity
            // linked to that request-local child-wire slot so history / ElementMap consumers do not
            // have to recover the openWireCompound child from transitional geometry later.
            ResultWireProducerIdentity& edgeProducer =
                materializationLedger.edgeEntries[childWire.edgeIndex].resultWireProducer;
            edgeProducer.childWireInfoIndex = childWireIndex;
            const bool branchProducerBlockedByChildWireSourceShape = edgeProducer.kind
                    == ResultWireProducerKind::SuperEdgeRoot
                && (edgeProducer.blocker == ResultWireBlocker::RootRemovedByUnownedBranch
                    || edgeProducer.blocker == ResultWireBlocker::RootRemovedByPrimaryBranch
                    || edgeProducer.blocker == ResultWireBlocker::RootRemovedBySecondaryBranch)
                && childWire.resultWireProducer.kind == ResultWireProducerKind::CurrentMemberChildWire
                && resultWireProducerStateAtLeast(childWire.resultWireProducer.state,
                                                  ResultWireProducerState::ChildWireReady)
                && (childWire.resultWireProducer.blocker == ResultWireBlocker::SourceShapeIdentityNotReady
                    || childWire.resultWireProducer.blocker
                        == ResultWireBlocker::SourceShapeWouldPurgeOriginal
                    || childWire.resultWireProducer.blocker
                        == ResultWireBlocker::CurrentMemberSourceShapeWouldPurgeOriginal
                    || childWire.resultWireProducer.blocker
                        == ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady
                    || childWire.resultWireProducer.blocker
                        == ResultWireBlocker::CurrentMemberVertexMultiplicityBlocked
                    || childWire.resultWireProducer.blocker
                        == ResultWireBlocker::CurrentMemberChildWireIdentityNotReady);
            if (branchProducerBlockedByChildWireSourceShape) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::getOpenWires(noOriginal=true) applies the source-shape purge gate
                // after ::build() has materialized openWireCompound children. If a migrated
                // RootRemoved branch reaches child-wire readiness but is rejected by that
                // child-wire source-shape gate, expose the child-wire blocker on the producer
                // identity instead of leaving it as an untraceable summary-only count.
                edgeProducer = childWire.resultWireProducer;
            }
            const bool currentMemberProducerBlockedByChildWire = edgeProducer.kind
                    == ResultWireProducerKind::CurrentMemberChildWire
                && edgeProducer.blocker == ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady
                && childWire.resultWireProducer.blocker
                    == ResultWireBlocker::MultiMemberRootPendingSuppression
                && resultWireProducerStateAtLeast(childWire.resultWireProducer.state,
                                                  ResultWireProducerState::ChildWireReady);
            if (currentMemberProducerBlockedByChildWire) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findSuperEdgesUpdateFirst() suppresses member edges by setting
                // "current->iteration = -1"; cad-core first classifies that member at EdgeInfo
                // level, then validates the actual openWireCompound child. If the child-wire gate
                // proves the remaining blocker is pending sibling-member ownership rather than
                // source-shape vertex identity, expose that finite child-wire blocker on the same
                // producer identity.
                edgeProducer = childWire.resultWireProducer;
            }
            const bool currentMemberProducerBlockedByMultiplicity = edgeProducer.kind
                    == ResultWireProducerKind::CurrentMemberChildWire
                && childWire.resultWireProducer.kind == ResultWireProducerKind::CurrentMemberChildWire
                && childWire.resultWireProducer.blocker
                    == ResultWireBlocker::CurrentMemberVertexMultiplicityBlocked
                && resultWireProducerStateAtLeast(childWire.resultWireProducer.state,
                                                  ResultWireProducerState::ChildWireReady);
            if (currentMemberProducerBlockedByMultiplicity) {
                // FreeCAD:
                // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() emits the child after EdgeInfo/superEdge state settles; when
                // the child-wire ledger proves the formal current-member candidate is blocked by vertex
                // multiplicity, expose that child-wire blocker instead of the earlier edge-level
                // source-shape classifier.
                edgeProducer = childWire.resultWireProducer;
            }
            const bool currentMemberProducerResolvedByChildWire = edgeProducer.kind
                    == ResultWireProducerKind::CurrentMemberChildWire
                && edgeProducer.blocker == ResultWireBlocker::SourceShapeMemberVertexIdentityNotReady
                && childWire.resultWireProducer.kind == ResultWireProducerKind::CurrentMemberChildWire
                && childWire.resultWireProducer.state
                    == ResultWireProducerState::ExportedWithoutTransitionalSlot;
            if (currentMemberProducerResolvedByChildWire) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() emits the final child wire after member suppression; once
                // the request-local child-wire ledger has replaced the result-wire candidate output, the EdgeInfo
                // producer identity must point at that emitted child instead of retaining the
                // pre-output vertex-identity blocker.
                edgeProducer = childWire.resultWireProducer;
            }
            const bool currentMemberProducerExportedByChildWire =
                childWire.currentMemberProducerOutput
                && childWire.resultWireProducer.kind == ResultWireProducerKind::CurrentMemberChildWire
                && childWire.resultWireProducer.state
                    == ResultWireProducerState::ExportedWithoutTransitionalSlot;
            if (currentMemberProducerExportedByChildWire) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() publishes the actual openWireCompound child. When that child
                // is already a current-member producer output, the EdgeInfo history entry must follow
                // the emitted child owner instead of the earlier super-edge root discovery slot.
                edgeProducer = childWire.resultWireProducer;
            }
            if (childWire.resultWireProducer.state == ResultWireProducerState::ExportedWithoutTransitionalSlot) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::build() exports the final "info.wire()" child into
                // openWireCompound. Preserve the edge-level producer kind/index that explains why
                // this legacy result slot exists, but publish the child-wire final state/blocker so
                // runtime history and ElementMap consumers do not see stale transitional result-slot debt.
                edgeProducer.state = ResultWireProducerState::ExportedWithoutTransitionalSlot;
                edgeProducer.blocker = ResultWireBlocker::None;
                edgeProducer.childWireInfoIndex = childWireIndex;
                edgeProducer.childWireBuilt = childWire.resultWireProducer.childWireBuilt;
            }
        }
    }
    updateOpenWireCompoundNoOriginalGroupPurgeVerdicts(info);
}

WireJoinerLedgerSummary WireJoiner::Impl::ledgerSummary() const
{
    WireJoinerLedgerSummary summary;
    struct MemberSuppressionRootGroup
    {
        std::size_t rootEdgeInfoIndex = 0;
        std::vector<std::size_t> coveredMemberEdgeInfoIndices;
        std::vector<std::size_t> currentMemberEdgeInfoIndices;
    };
    auto memberSuppressionRootGroupFor =
        [](std::vector<MemberSuppressionRootGroup>& groups,
           std::size_t rootEdgeInfoIndex) -> MemberSuppressionRootGroup& {
        const auto groupIt
            = std::find_if(groups.begin(), groups.end(), [&](const MemberSuppressionRootGroup& group) {
                  return group.rootEdgeInfoIndex == rootEdgeInfoIndex;
              });
        if (groupIt != groups.end()) {
            return *groupIt;
        }
        groups.push_back(MemberSuppressionRootGroup {rootEdgeInfoIndex, {}, {}});
        return groups.back();
    };
    std::vector<TopoDS_Vertex> currentMemberSplitOutputDistinctVertices;
    std::vector<TopoDS_Vertex> currentMemberSplitCandidateDistinctVertices;
    for (const WireInfo& info : openWires_) {
        summary.superEdgeCandidateCount += info.superEdges.size();
        for (const SuperEdgeInfo& superEdge : info.superEdges) {
            summary.superEdgeCandidateEdgeInfoCount += superEdge.vertices.size();
            if (superEdge.closed) {
                ++summary.superEdgeClosedCandidateCount;
            }
            else {
                ++summary.superEdgeOpenCandidateCount;
            }
        }
        summary.openWireCompoundWireInfoCount += info.openWireCompoundWires.size();
        std::vector<std::size_t> childWireIndexByEdgeInfo(info.edges.size(), resultWireProducerNpos);
        for (std::size_t childWireIndex = 0; childWireIndex < info.openWireCompoundWires.size();
             ++childWireIndex) {
            const OpenWireCompoundWireInfo& childWire = info.openWireCompoundWires[childWireIndex];
            if (childWire.edgeIndex < childWireIndexByEdgeInfo.size()
                && childWireIndexByEdgeInfo[childWire.edgeIndex] == resultWireProducerNpos) {
                childWireIndexByEdgeInfo[childWire.edgeIndex] = childWireIndex;
            }
            if (childWire.resultWireProducerLedgerEntry) {
                summary.resultWireProducerLedgerEntries.push_back(
                    resultWireProducerLedgerEntryForChildWire(childWire, childWireIndex)
                );
            }
            ++summary.openWireCompoundEdgeInfoCount;
            if (childWire.wireBuilt) {
                ++summary.openWireCompoundBuiltWireInfoCount;
            }
            if (childWire.superEdgeWire) {
                ++summary.openWireCompoundSuperEdgeWireInfoCount;
            }
            if (!childWire.sourceEdgeIndices.empty()) {
                ++summary.openWireCompoundSourceLineageWireInfoCount;
            }
            if (childWire.sourceLineageFromSplitterHistory) {
                ++summary.openWireCompoundSplitterLineageWireInfoCount;
            }
            if (childWire.noOriginalPurgeMatch) {
                ++summary.openWireCompoundNoOriginalPurgeMatchWireInfoCount;
            }
            if (childWire.producerLedgerWireBuilt) {
                ++summary.openWireCompoundProducerLedgerWireBuiltWireInfoCount;
            }
            if (childWire.producerLedgerWireFromSourceVmap) {
                ++summary.openWireCompoundProducerLedgerWireFromSourceVmapWireInfoCount;
            }
            if (childWire.sourceVmapEndpointLedgerRecorded) {
                ++summary.openWireCompoundSourceVmapEndpointLedgerWireInfoCount;
            }
            summary.openWireCompoundSourceVmapEndpointLedgerOutputVertexCount
                += childWire.sourceVmapEndpointLedgerOutputVertexCount;
            summary.openWireCompoundSourceVmapEndpointLedgerMatchedVertexCount
                += childWire.sourceVmapEndpointLedgerMatchedVertexCount;
            if (childWire.endpointProvenanceRecorded) {
                ++summary.openWireCompoundEndpointProvenanceWireInfoCount;
            }
            summary.openWireCompoundEndpointProvenanceOutputVertexCount
                += childWire.endpointProvenanceOutputVertexCount;
            summary.openWireCompoundEndpointProvenanceSourceVmapMatchedVertexCount
                += childWire.endpointProvenanceSourceVmapMatchedVertexCount;
            summary.openWireCompoundEndpointProvenanceVmapReplacementMatchedVertexCount
                += childWire.endpointProvenanceVmapReplacementMatchedVertexCount;
            summary.openWireCompoundEndpointProvenanceCandidateMatchedVertexCount
                += childWire.endpointProvenanceCandidateMatchedVertexCount;
            summary.openWireCompoundEndpointProvenanceUnmatchedVertexCount
                += childWire.endpointProvenanceUnmatchedVertexCount;
            if (!childWire.vmapReplacementEvents.empty()) {
                ++summary.openWireCompoundVmapReplacementEventWireInfoCount;
            }
            summary.openWireCompoundVmapReplacementEventCount
                += childWire.vmapReplacementEventCount;
            if (childWire.currentMemberSplitLedgerVertexCandidate) {
                ++summary.openWireCompoundCurrentMemberSplitLedgerVertexCandidateWireInfoCount;
            }
            if (!childWire.currentMemberSplitLedgerOutputVertexDebt.empty()) {
                ++summary.openWireCompoundCurrentMemberSplitLedgerVertexDebtWireInfoCount;
            }
            summary.openWireCompoundCurrentMemberSplitLedgerMemberVertexCount
                += childWire.currentMemberSplitLedgerMemberVertices.size();
            summary.openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount
                += childWire.currentMemberSplitLedgerOutputVertexDebt.size();
            summary.openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount
                += childWire.currentMemberSplitLedgerOutputMatchedVertexCount;
            summary.openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount
                += childWire.currentMemberSplitLedgerOutputCandidateMatchedVertexCount;
            for (const OpenWireCompoundWireInfo::CurrentMemberSplitLedgerVertexDebt& debt :
                 childWire.currentMemberSplitLedgerOutputVertexDebt) {
                appendUniqueVertexByIdentity(
                    currentMemberSplitOutputDistinctVertices,
                    debt.outputVertex
                );
                if (debt.currentChildWireOutputVertexMatchesOtherOutput) {
                    ++summary
                        .openWireCompoundCurrentMemberSplitLedgerOutputOtherOutputMatchedVertexCount;
                }
                if (debt.candidateWireVertexMatchesOtherOutput) {
                    ++summary
                        .openWireCompoundCurrentMemberSplitLedgerCandidateOtherOutputMatchedVertexCount;
                }
                if (!debt.currentChildWireOutputVertexMatchesOtherOutput
                    && debt.candidateWireVertexMatchesOtherOutput) {
                    ++summary
                        .openWireCompoundCurrentMemberSplitLedgerCandidateVertexReuseRiskCount;
                }
                if (debt.currentChildWireOutputVertexMatchesOtherOutput
                    && !debt.candidateWireVertexMatchesOtherOutput) {
                    ++summary
                        .openWireCompoundCurrentMemberSplitLedgerCandidateMissingSharedOutputIdentityCount;
                }
            }
            if (!childWire.currentMemberSplitLedgerOutputVertexDebt.empty()) {
                for (const TopoDS_Vertex& vertex :
                     childWire.currentMemberSplitLedgerCandidateVertices) {
                    appendUniqueVertexByIdentity(
                        currentMemberSplitCandidateDistinctVertices,
                        vertex
                    );
                }
            }
            if (childWire.currentMemberSplitLedgerVertexMultiplicityBlocked) {
                ++summary
                    .openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlockedWireInfoCount;
            }
            summary.openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount
                += childWire.currentMemberSplitLedgerOutputUnmatchedVertexCount;
            if (childWire.currentMemberProducerOutput) {
                ++summary.openWireCompoundRootCurrentMemberProducerOutputWireInfoCount;
            }
            if (childWire.sourceSharedVertexPurgeMatch) {
                ++summary.openWireCompoundSourceSharedVertexWireInfoCount;
            }
            if (childWire.noOriginalSharedSourceEdgeLedgerRecorded) {
                ++summary.openWireCompoundNoOriginalSharedSourceLedgerWireInfoCount;
            }
            summary.openWireCompoundNoOriginalSharedSourceEdgeCount
                += childWire.noOriginalSharedSourceEdgeCount;
            summary.openWireCompoundNoOriginalSharedSourceMatchedEdgeCount
                += childWire.noOriginalSharedSourceMatchedEdgeCount;
            summary.openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount
                += childWire.noOriginalSharedSourceUnmatchedEdgeCount;
        }
        summary.closedWireInfoCount += info.ownerWires.size();
        for (const OwnerWireInfo& owner : info.ownerWires) {
            summary.closedWireVertexCount += owner.vertices.size();
            summary.closedWireSearchStackFrameCount += owner.closedWireSearchStackFrameCount;
            summary.closedWireSearchVertexStackCount += owner.closedWireSearchVertexStackCount;
            summary.closedWireSearchEdgeSetVisitCount += owner.closedWireSearchEdgeSetVisitCount;
            summary.closedWireSearchBacktrackCount += owner.closedWireSearchBacktrackCount;
            summary.closedWireSearchIntersectSkipCount += owner.closedWireSearchIntersectSkipCount;
            summary.tightBoundExistingWireSearchCount += owner.tightBoundExistingWireSearchCount;
            summary.tightBoundExistingWireHitCount += owner.tightBoundExistingWireHitCount;
            summary.tightBoundExistingWireReverseHitCount += owner.tightBoundExistingWireReverseHitCount;
            summary.tightBoundExistingWirePurgeCount += owner.tightBoundExistingWirePurgeCount;
            if (owner.purge) {
                ++summary.tightBoundPurgedWireInfoCount;
            }
            if (owner.exhaustVisited) {
                ++summary.tightBoundExhaustVisitedWireInfoCount;
            }
            if (owner.exhaustDone) {
                ++summary.tightBoundExhaustDoneWireInfoCount;
            }
            if (owner.exhaustDiscardedByPurge) {
                ++summary.tightBoundExhaustDiscardedPurgedWireInfoCount;
            }
            summary.tightBoundFullWireSetInsertCount += owner.tightBoundFullWireSetInsertCount;
            summary.tightBoundFullWireSetEraseCount += owner.tightBoundFullWireSetEraseCount;
            summary.tightBoundFullWireSetAbortCount += owner.tightBoundFullWireSetAbortCount;
            summary.tightBoundFullWireSetPurgeCandidateCount
                += owner.tightBoundFullWireSetPurgeCandidateCount;
            summary.tightBoundFullWireSetBlockedTransferCount
                += owner.tightBoundFullWireSetBlockedTransferCount;
            summary.tightBoundFullWireSetAbortSearchCount += owner.tightBoundFullWireSetAbortSearchCount;
            summary.tightBoundFullWireSetAbortResolvedByHitCount
                += owner.tightBoundFullWireSetAbortResolvedByHitCount;
            summary.tightBoundFullWireSetAbortBlockedSearchCount
                += owner.tightBoundFullWireSetAbortBlockedSearchCount;
            if (owner.tightBoundExistingWireSearchCount > 1U) {
                ++summary.tightBoundExistingWireMultiRoundWireInfoCount;
                summary.tightBoundExistingWireMultiRoundSearchCount
                    += owner.tightBoundExistingWireSearchCount - 1U;
            }
            summary.tightBoundExistingWireSearchStackFrameCount
                += owner.tightBoundExistingWireSearchStackFrameCount;
            summary.tightBoundExistingWireSearchVertexStackCount
                += owner.tightBoundExistingWireSearchVertexStackCount;
            summary.tightBoundExistingWireSearchEdgeSetVisitCount
                += owner.tightBoundExistingWireSearchEdgeSetVisitCount;
            summary.tightBoundExistingWireSearchBacktrackCount
                += owner.tightBoundExistingWireSearchBacktrackCount;
            summary.tightBoundExistingWireSearchIdxVertexCount
                += owner.tightBoundExistingWireSearchIdxVertexCount;
            summary.tightBoundExistingWireSearchStackPosCount
                += owner.tightBoundExistingWireSearchStackPosCount;
            summary.tightBoundExistingWireSearchPathVertexCount
                += owner.tightBoundExistingWireSearchPathVertexCount;
            for (const TightBoundBranchCandidate& candidate : owner.branchCandidates) {
                if (!candidate.inside) {
                    continue;
                }
                ++summary.tightBoundNewWireCandidateCount;
                summary.tightBoundNewWireVertexCount += 2U;
            }
            if (owner.branchSearchCandidateCount > 0U) {
                ++summary.branchSearchSeedWireInfoCount;
            }
            if (owner.hasNewWireSeed) {
                ++summary.newWireSeedWireInfoCount;
            }
            summary.tightBoundTransferWireInfoCount += owner.transferWires.size();
            std::size_t selectedExistingWireHitCount = 0;
            std::size_t selectedExistingWireIdxVertexCount = 0;
            std::size_t selectedExistingWireStackPosCount = 0;
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                summary.tightBoundTransferWireVertexCount += transfer.vertices.size();
                summary.tightBoundSplitWireVertexCount += transfer.splitWireVertices.size();
                if (transfer.existingWireHit) {
                    ++selectedExistingWireHitCount;
                }
                if (transfer.existingWireHit && transfer.existingWireIdxVertex >= 0) {
                    ++selectedExistingWireIdxVertexCount;
                }
                if (transfer.existingWireHit && transfer.existingWireStackPos >= 0) {
                    ++selectedExistingWireStackPosCount;
                }
                if (transfer.splitWireBuilt) {
                    ++summary.tightBoundSplitWireBuiltCount;
                }
            }
            summary.tightBoundExistingWireSelectedHitCount += selectedExistingWireHitCount;
            summary.tightBoundExistingWireIdxVertexCount += selectedExistingWireIdxVertexCount;
            summary.tightBoundExistingWireStackPosCount += selectedExistingWireStackPosCount;
            if (owner.tightBoundExistingWireHitCount > selectedExistingWireHitCount) {
                summary.tightBoundExistingWireSearchOnlyHitCount += owner.tightBoundExistingWireHitCount
                    - selectedExistingWireHitCount;
            }
            if (owner.tightBoundExistingWireSearchIdxVertexCount
                > selectedExistingWireIdxVertexCount) {
                summary.tightBoundExistingWireSearchOnlyIdxVertexCount
                    += owner.tightBoundExistingWireSearchIdxVertexCount
                    - selectedExistingWireIdxVertexCount;
            }
            if (owner.tightBoundExistingWireSearchStackPosCount > selectedExistingWireStackPosCount) {
                summary.tightBoundExistingWireSearchOnlyStackPosCount
                    += owner.tightBoundExistingWireSearchStackPosCount
                    - selectedExistingWireStackPosCount;
            }
            summary.tightBoundExistingWireSearchOnlyPathBlockedCount
                += owner.tightBoundExistingWireSearchOnlyPathBlockedCount;
            summary.tightBoundExistingWireSearchOnlyOrderBlockedCount
                += owner.tightBoundExistingWireSearchOnlyOrderBlockedCount;
            if (!owner.splitOwnerVertices.empty()) {
                ++summary.tightBoundSplitOwnerWireInfoCount;
                summary.tightBoundSplitOwnerVertexCount += owner.splitOwnerVertices.size();
                if (owner.splitOwnerWireBuilt) {
                    ++summary.tightBoundSplitOwnerBuiltWireCount;
                }
            }
            if (owner.done) {
                ++summary.tightBoundDoneWireInfoCount;
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (transfer.done) {
                    ++summary.tightBoundDoneWireInfoCount;
                }
            }
            if (owner.hasSplitWireCandidate) {
                summary.tightBoundSplitWireInfoCount += owner.splitWireCandidateCount;
            }
        }
        for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
            const EdgeInfo& edgeInfo = info.edges[edgeInfoIndex];
            ++summary.edgeInfoCount;
            if (edgeInfo.splitFromInputEdge) {
                ++summary.splitEdgeInfoCount;
            }
            if (edgeInfo.superEdgeRoot) {
                ++summary.superEdgeRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeMaterialized) {
                ++summary.superEdgeMaterializedRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeInfo != 0U
                && (edgeInfo.superEdgeMaterialized || edgeInfo.superEdgeShadowedMember)) {
                ++summary.superEdgeMaterializedEdgeInfoCount;
            }
            if (edgeInfo.superEdgeShadowedMember) {
                ++summary.superEdgeShadowedMemberEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleMemberMinusOne) {
                ++summary.superEdgeLifecycleMemberMinusOneEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleOpenRoot) {
                ++summary.superEdgeLifecycleOpenRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleClosedRoot) {
                ++summary.superEdgeLifecycleClosedRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeRewritten) {
                ++summary.superEdgeLifecycleAdjacentRangeRewriteCount;
            }
            if (edgeInfo.superEdgeEndpointRewritten) {
                ++summary.superEdgeLifecycleEndpointRewriteCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeSourceEdgeInfo != 0U) {
                ++summary.superEdgeLifecycleAdjacentRangeSourceEdgeInfoCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeStart >= 0
                && edgeInfo.superEdgeAdjacentRangeEnd >= edgeInfo.superEdgeAdjacentRangeStart) {
                summary.superEdgeLifecycleAdjacentRangeVertexCount += static_cast<std::size_t>(
                    edgeInfo.superEdgeAdjacentRangeEnd - edgeInfo.superEdgeAdjacentRangeStart
                );
            }
            const bool hasSourceIdentityVertex = edgeInfo.sourceVertexIdentity[0]
                || edgeInfo.sourceVertexIdentity[1];
            const bool hasOnlySourceIdentityVertices = edgeInfo.sourceVertexIdentity[0]
                && edgeInfo.sourceVertexIdentity[1];
            if (hasSourceIdentityVertex) {
                ++summary.sourceIdentitySharedVertexEdgeInfoCount;
            }
            if (hasOnlySourceIdentityVertices) {
                ++summary.sourceIdentityOnlySourceVerticesEdgeInfoCount;
            }
            const bool hasSourceLineage = !edgeInfo.sourceEdgeIndices.empty();
            if (hasSourceLineage) {
                ++summary.sourceLineageEdgeInfoCount;
                if (edgeInfo.sourceLineageFromSplitterHistory) {
                    ++summary.sourceLineageSplitEdgeInfoCount;
                }
                if (edgeInfo.sourceEdgeIndices.size() > 1U) {
                    ++summary.sourceLineageMultiSourceEdgeInfoCount;
                }
            }
            if (!edgeInfo.splitFragmentSourceEdgeIndices.empty()) {
                ++summary.splitFragmentSourceLineageEdgeInfoCount;
            }
            if (edgeInfo.splitFragmentFromModifiedHistory) {
                ++summary.splitFragmentModifiedHistoryEdgeInfoCount;
            }
            if (edgeInfo.splitFragmentFromGeneratedHistory) {
                ++summary.splitFragmentGeneratedHistoryEdgeInfoCount;
            }
            if (edgeInfo.splitFragmentSourceLineageFromIdentityFallback) {
                ++summary.splitFragmentIdentityFallbackEdgeInfoCount;
            }
            if (edgeInfo.splitFragmentSourceLineageFromSourceIdentityFallback) {
                ++summary.splitFragmentSourceIdentityFallbackEdgeInfoCount;
            }
            if (edgeInfo.splitFragmentHistoryShapeGeometryBridge) {
                ++summary.splitFragmentHistoryShapeGeometryBridgeEdgeInfoCount;
            }
            if (edgeInfo.iteration2 != 0) {
                ++summary.iteration2MarkedEdgeInfoCount;
            }
            summary.branchSearchCandidateCount += edgeInfo.branchCandidateCount;
            summary.branchSearchInsideCandidateCount += edgeInfo.branchInsideCandidateCount;
            summary.newWireSeedCandidateCount += edgeInfo.newWireSeedCandidateCount;
            summary.splitWireEdgeInfoCount += edgeInfo.splitWireCandidateCount;
            if (edgeInfo.exhaustSeed) {
                ++summary.exhaustSeedEdgeInfoCount;
            }
            if (edgeInfo.exhaustSharedOwner) {
                ++summary.exhaustSharedOwnerEdgeInfoCount;
            }
            if (edgeInfo.exhaustDoneSecondary) {
                ++summary.exhaustDoneSecondaryEdgeInfoCount;
            }
            if (edgeInfo.exhaustSearchCandidate) {
                ++summary.exhaustSearchCandidateEdgeInfoCount;
            }
            if (edgeInfo.exhaustSecondaryOwner) {
                ++summary.exhaustSecondaryOwnerEdgeInfoCount;
            }
            if (edgeInfo.wireInfo != 0U) {
                ++summary.primaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.wireInfo2 != 0U) {
                ++summary.secondaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.tightBoundOwnerTransferCandidate) {
                ++summary.tightBoundOwnerTransferCandidateEdgeInfoCount;
            }
            if (edgeInfo.tightBoundTransferredOwner) {
                ++summary.tightBoundTransferredOwnerEdgeInfoCount;
            }
            if (edgeInfo.closedWireOwner) {
                ++summary.closedWireAssignedEdgeInfoCount;
            }
            const bool hasOpenWireCompoundChildWire = edgeInfoIndex < childWireIndexByEdgeInfo.size()
                && childWireIndexByEdgeInfo[edgeInfoIndex] != resultWireProducerNpos;
            const bool exportsOpenEdge =
                edgeInfoHasOpenWireCompoundLedgerSlot(edgeInfo, hasOpenWireCompoundChildWire);
            if (exportsOpenEdge) {
                ++summary.openExportEdgeInfoCount;
                if (!hasOpenWireCompoundChildWire) {
                    ++summary.openWireCompoundMissingChildWireHistoryEdgeInfoCount;
                }
                if (hasSourceIdentityVertex) {
                    ++summary.sourceIdentityOpenExportSharedVertexEdgeInfoCount;
                }
                if (hasOnlySourceIdentityVertices) {
                    ++summary.sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount;
                }
                if (hasSourceLineage) {
                    ++summary.sourceLineageOpenExportEdgeInfoCount;
                }
                else {
                    ++summary.sourceLineageMissingOpenExportEdgeInfoCount;
                }
            }
        }
        if (!info.orderedVertices.empty()) {
            ++summary.orderedWireInfoCount;
            summary.orderedVertexCount += info.orderedVertices.size();
            if (std::any_of(
                    info.orderedVertices.begin(),
                    info.orderedVertices.end(),
                    [](const WireVertex& vertex) { return vertex.branchCandidateCount > 0U; }
                )) {
                ++summary.branchSearchSeedWireInfoCount;
            }
            if (info.hasNewWireSeed) {
                ++summary.newWireSeedWireInfoCount;
            }
            if (info.hasSplitWireCandidate) {
                summary.splitWireCandidateCount += info.splitWireCandidateCount;
            }
            if (info.done) {
                ++summary.doneWireInfoCount;
            }
            summary.doneOwnedEdgeInfoCount += std::count_if(
                info.edges.begin(),
                info.edges.end(),
                [](const EdgeInfo& edgeInfo) { return edgeInfo.wireInfo != 0U; }
            );
        }
        summary.ownerPropagationCandidateCount += info.ownerPropagationCandidateCount;
        summary.ownerPropagationOtherWireCandidateCount += info.ownerPropagationOtherWireCandidateCount;
        summary.ownerPropagationOtherWireLiveEdgeInfoCount
            += info.ownerPropagationOtherWireLiveEdgeInfoCount;
        summary.exhaustAdjacentSearchCount += info.exhaustAdjacentSearchCount;
        summary.exhaustAdjacentSearchHitCount += info.exhaustAdjacentSearchHitCount;
        summary.exhaustAdjacentSearchMissCount += info.exhaustAdjacentSearchMissCount;
        summary.exhaustAdjacentSearchStackFrameCount += info.exhaustAdjacentSearchStackFrameCount;
        summary.exhaustAdjacentSearchVertexStackCount += info.exhaustAdjacentSearchVertexStackCount;
        summary.exhaustAdjacentSearchEdgeSetVisitCount += info.exhaustAdjacentSearchEdgeSetVisitCount;
        summary.exhaustAdjacentSearchBacktrackCount += info.exhaustAdjacentSearchBacktrackCount;
        summary.exhaustAdjacentWireSetInsertCount += info.exhaustAdjacentWireSetInsertCount;
        summary.exhaustAdjacentWireSetEraseCount += info.exhaustAdjacentWireSetEraseCount;
        summary.exhaustAdjacentWireSetAbortCount += info.exhaustAdjacentWireSetAbortCount;
        summary.exhaustAdjacentWireInfo2AbortCount += info.exhaustAdjacentWireInfo2AbortCount;
        summary.closedWireCycleSplitLedgerSourceEdgeCount
            += info.closedWireCycleSplitLedgerSourceEdgeCount;
        if (info.closedWireCycleSplitLedgerOpenExport) {
            ++summary.closedWireCycleSplitLedgerOpenExportDecisionCount;
        }
        summary.tightBoundExhaustPrimaryResetEdgeInfoCount
            += info.tightBoundExhaustPrimaryResetEdgeInfoCount;
        summary.repeatedSplitExhaustCycleCount += info.repeatedSplitExhaustCycleCount;
        summary.repeatedSplitExhaustRemovedEdgeInfoCount += info.repeatedSplitExhaustRemovedEdgeInfoCount;
        summary.repeatedSplitExhaustRemovedUnownedEdgeInfoCount
            += info.repeatedSplitExhaustRemovedUnownedEdgeInfoCount;
        summary.repeatedSplitExhaustRemovedSecondaryEdgeInfoCount
            += info.repeatedSplitExhaustRemovedSecondaryEdgeInfoCount;
        summary.repeatedSplitExhaustRemovedPrimaryEdgeInfoCount
            += info.repeatedSplitExhaustRemovedPrimaryEdgeInfoCount;
        summary.repeatedSplitExhaustRerunActiveEdgeInfoCount
            += info.repeatedSplitExhaustRerunActiveEdgeInfoCount;
        summary.repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount
            += info.repeatedSplitExhaustRerunOwnedActiveEdgeInfoCount;
        summary.repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount
            += info.repeatedSplitExhaustRerunResetPrimaryEdgeInfoCount;
        summary.repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount
            += info.repeatedSplitExhaustRerunResetSecondaryEdgeInfoCount;
        summary.repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount
            += info.repeatedSplitExhaustRerunSkippedOpenLeafEdgeInfoCount;
        summary.repeatedSplitExhaustRerunNoActiveSearchCount
            += info.repeatedSplitExhaustRerunNoActiveSearchCount;
        summary.repeatedSplitExhaustRerunClosedWireSearchCount
            += info.repeatedSplitExhaustRerunClosedWireSearchCount;
        summary.repeatedSplitExhaustRerunClosedWireMissCount
            += info.repeatedSplitExhaustRerunClosedWireMissCount;
        summary.repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount
            += info.repeatedSplitExhaustRerunMissLiveResetEdgeInfoCount;
        summary.repeatedSplitExhaustRerunClosedWireInfoCount
            += info.repeatedSplitExhaustRerunClosedWireInfoCount;
        summary.repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount
            += info.repeatedSplitExhaustRerunClosedWireAssignedEdgeInfoCount;
        summary.repeatedSplitExhaustRerunClosedWireVertexCount
            += info.repeatedSplitExhaustRerunClosedWireVertexCount;
        summary.repeatedSplitExhaustRerunResettableClosedWireInfoCount
            += info.repeatedSplitExhaustRerunResettableClosedWireInfoCount;
        summary.repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount
            += info.repeatedSplitExhaustRerunResettableAssignedEdgeInfoCount;
        summary.repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount
            += info.repeatedSplitExhaustRerunLiveResetPrimaryEdgeInfoCount;
        summary.repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount
            += info.repeatedSplitExhaustRerunLiveResetSecondaryEdgeInfoCount;
        summary.repeatedSplitExhaustRerunLiveClosedWireInfoCount
            += info.repeatedSplitExhaustRerunLiveClosedWireInfoCount;
        summary.repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount
            += info.repeatedSplitExhaustRerunLiveAssignedEdgeInfoCount;
        summary.repeatedSplitExhaustRerunLiveClosedWireVertexCount
            += info.repeatedSplitExhaustRerunLiveClosedWireVertexCount;
        summary.repeatedSplitExhaustRerunLiveBranchSearchCandidateCount
            += info.repeatedSplitExhaustRerunLiveBranchSearchCandidateCount;
        summary.repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount
            += info.repeatedSplitExhaustRerunLiveBranchSearchInsideCandidateCount;
        summary.repeatedSplitExhaustRerunLiveDoneWireInfoCount
            += info.repeatedSplitExhaustRerunLiveDoneWireInfoCount;
        summary.repeatedSplitExhaustRerunRemovalScanCount
            += info.repeatedSplitExhaustRerunRemovalScanCount;
        summary.repeatedSplitExhaustRerunLoopExitNoRemovalCount
            += info.repeatedSplitExhaustRerunLoopExitNoRemovalCount;
        summary.repeatedSplitExhaustRerunBranchSearchCandidateCount
            += info.repeatedSplitExhaustRerunBranchSearchCandidateCount;
        summary.repeatedSplitExhaustRerunBranchSearchInsideCandidateCount
            += info.repeatedSplitExhaustRerunBranchSearchInsideCandidateCount;
        summary.repeatedSplitExhaustRerunNewWireSeedCandidateCount
            += info.repeatedSplitExhaustRerunNewWireSeedCandidateCount;
        summary.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount
            += info.repeatedSplitExhaustGeneratedIdentityBlockedEdgeInfoCount;
    }
    summary.openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount =
        currentMemberSplitOutputDistinctVertices.size();
    summary.openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount =
        currentMemberSplitCandidateDistinctVertices.size();
    if (summary.openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount
        > summary.openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount) {
        summary.openWireCompoundCurrentMemberSplitLedgerCandidateVertexMultiplicityLossCount =
            summary.openWireCompoundCurrentMemberSplitLedgerOutputDistinctVertexCount
            - summary.openWireCompoundCurrentMemberSplitLedgerCandidateDistinctVertexCount;
    }
    return summary;
}

WireJoinerHistorySummary WireJoiner::Impl::historySummary() const
{
    return historySummary_;
}

std::optional<TopoDS_Shape> WireJoiner::Impl::getOpenWires(const std::string& historyPrefix, bool noOriginal) const
{
    (void)historyPrefix;
    (void)tightBound_;

    std::vector<TopoDS_Edge> allLiveEdges;
    std::vector<TopoDS_Wire> liveWires;
    for (const WireInfo& info : openWires_) {
        if (!info.openWireCompoundWires.empty()) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::build() first materializes "openWireCompound" with
            // "builder.Add(openWireCompound, info.wire())"; ::getOpenWires() then consumes that
            // child-wire compound. Read the request-local OpenWireCompoundWireInfo ledger here
            // instead of re-deriving the export boundary from EdgeInfo/result-slot evidence.
            std::vector<TopoDS_Edge> liveEdges;
            for (const OpenWireCompoundWireInfo& childWire : info.openWireCompoundWires) {
                if (childWire.wire.IsNull()) {
                    continue;
                }
                if (openWireCompoundChildWirePurgedByNoOriginal(childWire, noOriginal)) {
                    continue;
                }
                if (childWire.superEdgeWire) {
                    liveWires.push_back(childWire.wire);
                    continue;
                }
                const std::vector<TopoDS_Edge> childWireEdges = wireEdges(childWire.wire);
                liveEdges.insert(liveEdges.end(), childWireEdges.begin(), childWireEdges.end());
            }
            if (liveEdges.empty()) {
                continue;
            }
            if (mergeEdges_) {
                allLiveEdges.insert(allLiveEdges.end(), liveEdges.begin(), liveEdges.end());
                continue;
            }
            const auto currentWires = wiresFromEdges(liveEdges);
            liveWires.insert(liveWires.end(), currentWires.begin(), currentWires.end());
            continue;
        }
    }
    if (mergeEdges_ && !allLiveEdges.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() collects final EdgeInfo states into openWireCompound after
        // splitEdges()/findTightBound(), so connected leftover fragments are result wires rather
        // than isolated per-input wires.
        const auto mergedWires = wiresFromEdges(allLiveEdges);
        liveWires.insert(liveWires.end(), mergedWires.begin(), mergedWires.end());
    }
    if (liveWires.empty()) {
        return std::nullopt;
    }
    if (liveWires.size() == 1U) {
        return liveWires.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Wire& wire : liveWires) {
        builder.Add(compound, wire);
    }
    return compound;
}

WireJoinerBuildResult WireJoiner::Impl::buildResult(
    const std::string& historyPrefix,
    bool noOriginal
) const
{
    const WireJoinerLedgerSummary ledger = ledgerSummary();
    const WireJoinerHistorySummary history = historySummary();

    WireJoinerBuildResult result;
    result.openWires = getOpenWires(historyPrefix, noOriginal);
    result.hasOpenWires = result.openWires && !result.openWires->IsNull();
    const bool missingChildWireInvariant =
        ledger.openWireCompoundMissingChildWireHistoryEdgeInfoCount > 0U
        || std::any_of(
            history.openExportEntries.begin(),
            history.openExportEntries.end(),
            [](const WireJoinerOpenExportHistoryEntry& entry) {
                return entry.missingOpenWireCompoundChildWire;
            }
        );
    const bool noOriginalPurged = std::any_of(
        history.openExportEntries.begin(),
        history.openExportEntries.end(),
        [](const WireJoinerOpenExportHistoryEntry& entry) {
            return entry.openWireCompoundNoOriginalPurgedByLedger;
        }
    );
    const bool hasMapperHistoryEvidence =
        history.splitterHistory
        || history.modifiedHistoryCount > 0U
        || history.generatedHistoryCount > 0U
        || history.deletedHistoryCount > 0U
        || !history.historyEvents.empty()
        || !history.openExportEntries.empty();
    result.historyLedger = wireJoinerHistoryEvidenceLedger(history);
    const nlohmann::json compatibilityLedger = wireJoinerLedgerToJson(ledger);
    const nlohmann::json compatibilityHistoryDetail = wireJoinerHistoryDetailToJson(history);

    nlohmann::json codes = nlohmann::json::array();
    if (missingChildWireInvariant) {
        codes.push_back("missing_child_wire_invariant");
    }
    if (noOriginalPurged) {
        codes.push_back("no_original_purged");
    }
    if (!result.hasOpenWires && !history.openExportEntries.empty()) {
        codes.push_back("open_wire_result_empty_after_filter");
    }

    const nlohmann::json diagnostics = {
        {"status", missingChildWireInvariant ? "invariant_failed" : "ok"},
        {"codes", std::move(codes)},
        {"summary",
         {
             {"has_open_wires", result.hasOpenWires},
             {"open_export_count", history.openExportEntries.size()},
             {"history_event_count", history.historyEvents.size()},
             {"source_edge_count", history.sourceEdgeCount},
             {"split_result_edge_count", history.splitResultEdgeCount},
             {"missing_child_wire_invariant", missingChildWireInvariant},
             {"no_original_purged", noOriginalPurged},
             {"has_mapper_history_evidence", hasMapperHistoryEvidence},
         }},
    };
    result.diagnostics = diagnostics;
    InternalShapeHistoryLedgerData& historyLedgerData =
        mutableInternalShapeHistoryLedgerData(result.historyLedger);
    historyLedgerData.wireJoinerDiagnostics = diagnostics;
    historyLedgerData.wireJoinerCompatibilityLedger = compatibilityLedger;
    historyLedgerData.wireJoinerCompatibilityHistoryDetail = compatibilityHistoryDetail;
    return result;
}


WireJoiner::WireJoiner()
    : impl_(std::make_unique<Impl>())
{
}

WireJoiner::~WireJoiner() = default;

WireJoiner::WireJoiner(WireJoiner&&) noexcept = default;

WireJoiner& WireJoiner::operator=(WireJoiner&&) noexcept = default;

void WireJoiner::setTightBound(bool enabled)
{
    impl_->setTightBound(enabled);
}

void WireJoiner::setMergeEdges(bool enabled)
{
    impl_->setMergeEdges(enabled);
}

void WireJoiner::attachProducerTrace(app::ElementMapProducerTrace* trace) noexcept
{
    impl_->producerTrace_ = trace;
}

void WireJoiner::addOpenWire(
    const TopoDS_Wire& wire,
    const std::vector<std::size_t>& sourceEdgeIndices
)
{
    impl_->addOpenWire(wire, sourceEdgeIndices);
}

void WireJoiner::buildFinalEdgeOwnership(
    const TopoDS_Shape* boundedFaceShape,
    const std::vector<TopoDS_Wire>* closedWires,
    const std::vector<TopoDS_Edge>* openEdges,
    bool splitProducedBoundedFaces
)
{
    impl_->buildFinalEdgeOwnership(
        boundedFaceShape,
        closedWires,
        openEdges,
        splitProducedBoundedFaces
    );
}

void WireJoiner::addSourceEdge(const TopoDS_Edge& edge)
{
    impl_->addSourceEdge(edge);
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(
    const std::string& historyPrefix,
    bool noOriginal
) const
{
    return impl_->getOpenWires(historyPrefix, noOriginal);
}

WireJoinerBuildResult WireJoiner::buildResult(
    const std::string& historyPrefix,
    bool noOriginal
) const
{
    app::ElementMapProducerTrace::Scope traceScope;
    if (impl_->producerTrace_ != nullptr) {
        traceScope = impl_->producerTrace_->scope(
            {"WireJoiner::getOpenWires",
             "",
             0,
             "Part::WireJoiner",
             {{"historyPrefix", historyPrefix},
              {"noOriginal", noOriginal},
              {"tightBound", impl_->tightBound_},
              {"mergeEdges", impl_->mergeEdges_}}}
        );
        impl_->producerTrace_->record({
            "wire_joiner.lifecycle",
            "begin",
            "wire_joiner_ledger_consumption_started",
            {{"historyPrefix", historyPrefix}, {"noOriginal", noOriginal}},
        });
    }
    WireJoinerBuildResult result = impl_->buildResult(historyPrefix, noOriginal);
    if (impl_->producerTrace_ != nullptr) {
        impl_->producerTrace_->record({
            "wire_joiner.lifecycle",
            "success",
            result.hasOpenWires ? "open_wire_history_published" : "no_open_wires_after_filter",
            {{"hasOpenWires", result.hasOpenWires},
             {"diagnostics", result.diagnostics},
             {"historyLedger", result.historyLedger.diagnosticsJson()}},
        });
    }
    return result;
}

}  // namespace cad_core::part
