#pragma once

#include "cad_core/part/internal_shape_history_ledger.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace cad_core::part
{

struct SketchInternalWireJoinerEndpointIdentityDebt
{
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

struct SketchInternalWireJoinerVmapReplacementEvent
{
    std::size_t eventIndex = 0;
    std::size_t affectedSourceEdgeIndex = 0;
    std::size_t affectedChildWireEdgeInfoIndex = 0;
    int affectedEndpoint = -1;
    int affectedSourceEndpoint = -1;
    int affectedChildWireEndpoint = -1;
    std::size_t replacementSourceEdgeIndex = 0;
    int replacementSourceEndpoint = -1;
    bool replacementFromMutableSourceEdgeLedger = false;
    bool replacementFromSplitFragmentLedger = false;
};

struct SketchInternalWireJoinerOpenExportHistoryEntry
{
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = 0;
    TopoDS_Wire openExportWire;
    TopoDS_Edge openExportEdge;
    std::string openWireCompoundExportSource;
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
    std::string wireJoinerHistoryRelation;
    bool wireJoinerHistoryRelationFromChildWireLedger = false;
    std::string resultWireProducerKind;
    std::string resultWireProducerState;
    std::string resultWireProducerBlocker;
    std::size_t resultWireProducerSourceEdgeInfoIndex = 0;
    std::size_t resultWireProducerRootEdgeInfoIndex = 0;
    std::size_t resultWireProducerCurrentMemberEdgeInfoIndex = 0;
    std::size_t resultWireProducerChildWireInfoIndex = 0;
    std::size_t openWireCompoundChildWireInfoIndex = 0;
    std::vector<std::size_t> openWireCompoundSourceEdgeIndices;
    bool openWireCompoundSourceLineageFromSplitterHistory = false;
    bool openWireCompoundNoOriginalPurgeMatch = false;
    bool openWireCompoundNoOriginalPurgedByLedger = false;
    bool openWireCompoundNoOriginalSharedSourceLedgerRecorded = false;
    std::size_t openWireCompoundNoOriginalSharedSourceEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceMatchedEdgeCount = 0;
    std::size_t openWireCompoundNoOriginalSharedSourceUnmatchedEdgeCount = 0;
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
    std::vector<SketchInternalWireJoinerVmapReplacementEvent>
        openWireCompoundVmapReplacementEvents;
    std::size_t openWireCompoundVmapReplacementEventCount = 0;
    bool openWireCompoundCurrentMemberProducerOutput = false;
    bool openWireCompoundCurrentMemberSplitLedgerVertexCandidate = false;
    bool openWireCompoundCurrentMemberSplitLedgerVertexDebtRecorded = false;
    std::size_t openWireCompoundCurrentMemberSplitLedgerMemberVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerCandidateVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputVertexLedgerCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputCandidateMatchedVertexCount = 0;
    std::size_t openWireCompoundCurrentMemberSplitLedgerOutputUnmatchedVertexCount = 0;
    std::vector<SketchInternalWireJoinerEndpointIdentityDebt>
        openWireCompoundCurrentMemberSplitLedgerOutputVertexDebt;
    bool openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked = false;
    bool missingOpenWireCompoundChildWire = false;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    std::size_t wireJoinerHistoryEventIndex = 0;
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
};

struct SketchInternalWireJoinerHistoryEvent
{
    std::size_t eventIndex = 0;
    std::size_t openExportIndex = 0;
    std::size_t edgeInfoIndex = 0;
    std::size_t openWireCompoundChildWireInfoIndex = 0;
    std::string relation;
    bool relationFromChildWireLedger = false;
    std::vector<std::size_t> sourceEdgeIndices;
    bool sourceLineageFromSplitterHistory = false;
    bool noOriginalPurgedByLedger = false;
    bool splitFragmentFromModifiedHistory = false;
    bool splitFragmentFromGeneratedHistory = false;
};

struct SketchInternalFaceMakerEdgeEvidence
{
    std::string makerStage;
    std::string relation;
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    TopoDS_Edge targetEdge;
    bool preSplitHistory = false;
    bool splitterHistory = false;
};

struct SketchInternalFaceMakerBoundedFaceBoundaryEvidence
{
    std::size_t sourceEdgeIndex = 0;
    std::size_t targetEdgeIndex = 0;
    std::string makerStage;
    std::string relation;
    TopoDS_Edge targetEdge;
};

struct SketchInternalFaceMakerBoundedFaceEvidence
{
    std::size_t boundedFaceIndex = 0;
    TopoDS_Face face;
    std::vector<std::size_t> sourceEdgeIndices;
    std::vector<std::size_t> outerBoundaryTargetEdgeIndices;
    std::vector<SketchInternalFaceMakerBoundedFaceBoundaryEvidence> outerBoundary;
};

struct SketchInternalHistoryContext
{
    std::size_t sourceEdgeCount = 0;
    std::size_t preSplitEdgeCount = 0;
    std::size_t splitterEdgeCount = 0;
    std::size_t boundedFaceCount = 0;
    bool preSplitHistory = false;
    bool splitterHistory = false;
    std::vector<SketchInternalFaceMakerEdgeEvidence> faceMakerEdgeEvidence;
    std::vector<SketchInternalFaceMakerBoundedFaceEvidence> faceMakerBoundedFaceEvidence;
    std::size_t wireJoinerSourceEdgeCount = 0;
    std::size_t wireJoinerSplitResultEdgeCount = 0;
    std::vector<SketchInternalWireJoinerHistoryEvent> wireJoinerHistoryEvents;
    std::vector<SketchInternalWireJoinerOpenExportHistoryEntry>
        wireJoinerOpenExportHistoryEntries;
    std::size_t wireJoinerModifiedSourceEdgeCount = 0;
    std::size_t wireJoinerModifiedHistoryCount = 0;
    std::size_t wireJoinerGeneratedHistoryCount = 0;
    std::size_t wireJoinerDeletedHistoryCount = 0;
    bool wireJoinerSplitterHistory = false;
};

struct InternalShapeHistoryLedgerData
{
    SketchInternalHistoryContext compatibilityHistory;
    std::vector<InternalShapeHistoryEvent> events;
    bool hasFaceMakerEvidence = false;
    bool hasWireJoinerEvidence = false;
    nlohmann::json faceMakerCompatibility = nlohmann::json::object();
    nlohmann::json wireJoinerDiagnostics = nlohmann::json::object();
    nlohmann::json wireJoinerCompatibilityLedger = nlohmann::json::object();
    nlohmann::json wireJoinerCompatibilityHistoryDetail = nlohmann::json::object();
};

InternalShapeHistoryLedgerData& mutableInternalShapeHistoryLedgerData(
    InternalShapeHistoryLedger& ledger
);
const InternalShapeHistoryLedgerData& internalShapeHistoryLedgerData(
    const InternalShapeHistoryLedger& ledger
);

}  // namespace cad_core::part
