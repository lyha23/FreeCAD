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
    std::size_t generatedOpenExportEdgeInfoCount = 0;
    std::size_t generatedOpenExportSourceEdgeInfoCount = 0;
    std::size_t generatedOpenExportSourceEdgeInfoConsumedCount = 0;
    std::size_t generatedOpenExportConsumedOpenCutterGraphEdgeInfoCount = 0;
    std::size_t generatedOpenExportPartialJunctionOpenCutterEdgeInfoCount = 0;
    std::size_t generatedOpenExportClosedWireCycleEdgeInfoCount = 0;
    std::size_t generatedOpenExportPartialSharedClosedWireEdgeInfoCount = 0;
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
    std::size_t openWireCompoundGeneratedWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedSourceEdgeInfoWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedSourceEdgeInfoConsumedWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedConsumedOpenCutterGraphWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedPartialJunctionOpenCutterWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedClosedWireCycleWireInfoCount = 0;
    std::size_t openWireCompoundGeneratedPartialSharedClosedWireWireInfoCount = 0;
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
    bool generatedOpenExport = false;
    std::string generatedOpenExportReason;
    bool generatedOpenExportSourceEdgeInfo = false;
    std::size_t generatedOpenExportSourceEdgeInfoIndex = 0;
    bool generatedOpenExportSourceEdgeInfoConsumed = false;
    bool purgeBridge = false;
};

struct WireJoinerHistorySummary {
    std::size_t sourceEdgeCount = 0;
    std::size_t splitResultEdgeCount = 0;
    std::size_t openExportEdgeCount = 0;
    std::size_t openExportSourceLineageEdgeCount = 0;
    std::size_t openExportMissingSourceLineageEdgeCount = 0;
    std::size_t openExportGeneratedEdgeCount = 0;
    std::size_t openExportGeneratedSourceEdgeInfoCount = 0;
    std::size_t openExportGeneratedSourceEdgeInfoConsumedCount = 0;
    std::size_t openExportGeneratedMissingSourceLineageEdgeCount = 0;
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
        std::string generatedOpenExportReason;
        bool generatedOpenExportSourceEdgeInfo = false;
        std::size_t generatedOpenExportSourceEdgeInfoIndex = 0;
        bool generatedOpenExportSourceEdgeInfoConsumed = false;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findSuperEdges(), "Join edges (let's call it super edge) that are connected
        // to only one other edges". This is currently diagnostic only; getOpenWires() still exports
        // original EdgeInfo edges until real superEdge/openWireCompound child-wire semantics migrate.
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
        bool purgeAsOriginalOpenEdge = false;
        bool generatedOpenExportEdge = false;
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
        bool generatedOpenExport = false;
        std::string generatedOpenExportReason;
        bool generatedOpenExportSourceEdgeInfo = false;
        std::size_t generatedOpenExportSourceEdgeInfoIndex = 0;
        bool generatedOpenExportSourceEdgeInfoConsumed = false;
        bool purgeBridge = false;
        bool sourceSharedVertexPurgeMatch = false;
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
                                                  const std::vector<TopoDS_Face>& boundedFaces);
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
