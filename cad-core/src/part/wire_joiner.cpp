#include "cad_core/part/wire_joiner.h"

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
#include <utility>

namespace cad_core::part
{

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
    const TopoDS_Edge& producerEdge,
    const TopoDS_Edge& resultEdge
)
{
    if (producerEdge.IsNull() || resultEdge.IsNull()
        || !edgeEquivalentByGeometryAndEndpoints(producerEdge, resultEdge)) {
        return std::nullopt;
    }

    const std::vector<TopoDS_Vertex> resultVertices = edgeVertices(resultEdge);
    const auto [firstPoint, lastPoint] = edgeEndpoints(producerEdge);
    const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(resultVertices, firstPoint);
    const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(resultVertices, lastPoint);
    if (!firstVertex || !lastVertex) {
        return std::nullopt;
    }

    TopoDS_Edge outputEdge = edgeWithReusedVertices(producerEdge, *firstVertex, *lastVertex);
    if (outputEdge.IsNull()) {
        return std::nullopt;
    }
    return outputEdge;
}

std::optional<TopoDS_Edge> edgeSubsegmentWithReusedVertices(
    const TopoDS_Edge& producerEdge,
    const TopoDS_Vertex& firstVertex,
    const TopoDS_Vertex& lastVertex
)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(producerEdge, first, last);
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
    const TopoDS_Edge& producerEdge,
    const TopoDS_Edge& resultEdge
)
{
    if (const std::optional<TopoDS_Edge> equivalent
        = edgeWithEquivalentResultVertices(producerEdge, resultEdge)) {
        return equivalent;
    }
    if (producerEdge.IsNull() || resultEdge.IsNull()
        || !edgeSamplesLieOnEdge(resultEdge, producerEdge)) {
        return std::nullopt;
    }
    const std::vector<TopoDS_Vertex> resultVertices = edgeVertices(resultEdge);
    const auto [firstPoint, lastPoint] = edgeEndpoints(resultEdge);
    const std::optional<TopoDS_Vertex> firstVertex = vertexAtPoint(resultVertices, firstPoint);
    const std::optional<TopoDS_Vertex> lastVertex = vertexAtPoint(resultVertices, lastPoint);
    if (!firstVertex || !lastVertex) {
        return std::nullopt;
    }
    return edgeSubsegmentWithReusedVertices(producerEdge, *firstVertex, *lastVertex);
}

// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
// ::WireJoinerP::add(), key "Make sure coincident vertices are actually the same TopoDS_Vertex";
// ::build() exports "info.wire()" after that vertex replacement. Rebuild a producer subsegment
// only from child-wire endpoint vertex identity, not from an output-side result edge shape.
std::optional<TopoDS_Edge> edgeWithProducerCurveAndResultVertices(
    const TopoDS_Edge& producerEdge,
    const std::vector<TopoDS_Vertex>& resultVertices
)
{
    if (producerEdge.IsNull() || resultVertices.size() < 2U) {
        return std::nullopt;
    }
    const TopoDS_Vertex& firstVertex = resultVertices.front();
    const TopoDS_Vertex& lastVertex = resultVertices.back();
    if (firstVertex.IsNull() || lastVertex.IsNull()) {
        return std::nullopt;
    }
    if (!pointOnEdge(BRep_Tool::Pnt(firstVertex), producerEdge)
        || !pointOnEdge(BRep_Tool::Pnt(lastVertex), producerEdge)) {
        return std::nullopt;
    }
    return edgeSubsegmentWithReusedVertices(producerEdge, firstVertex, lastVertex);
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

}  // namespace

void WireJoiner::setTightBound(bool enabled)
{
    tightBound_ = enabled;
}

void WireJoiner::setMergeEdges(bool enabled)
{
    mergeEdges_ = enabled;
}

void WireJoiner::addOpenWire(
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

void WireJoiner::addSourceEdge(const TopoDS_Edge& edge)
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

std::size_t WireJoiner::closedWireCycleSplitLedgerSourceEdgeCount(
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

WireJoiner::WireJoinerHistoryMaterializationLedger WireJoiner::computeWireJoinerHistoryMaterializationLedger(
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

bool WireJoiner::wireJoinerHistoryMaterializationLedgerHasUnsafeProducer(
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

bool WireJoiner::edgeInfoExportsOpenWireCompound(const EdgeInfo& edgeInfo) const
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), exports an EdgeInfo into "openWireCompound" only when
    // "info.iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    return edgeInfo.iteration == -3 || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
}

bool WireJoiner::edgeInfoHasOpenWireCompoundLedgerSlot(
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

bool WireJoiner::resultWireProducerSlotHasFullAHistoryEvidence(
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

bool WireJoiner::resultWireProducerRootHasFullAHistoryEvidence(
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

bool WireJoiner::resultWireProducerRootCanSuppressPendingMember(
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

bool WireJoiner::resultWireProducerSlotHasSafeAHistoryEvidence(
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

void WireJoiner::updateOpenWireCompoundNoOriginalPurgeVerdict(
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

void WireJoiner::updateOpenWireCompoundNoOriginalGroupPurgeVerdicts(WireInfo& info) const
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

bool WireJoiner::openWireCompoundChildWirePurgedByNoOriginal(
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

std::optional<std::size_t> WireJoiner::superEdgeRootIndexForMember(
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

std::vector<std::size_t> WireJoiner::strictRemovedSourceEdgeInfoIndicesForSourceLineage(
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

bool WireJoiner::wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    if (edgeInfoIndex >= materializationLedger.edgeEntries.size()) {
        return false;
    }
    const std::optional<TopoDS_Edge>& producerEdge =
        materializationLedger.edgeEntries[edgeInfoIndex].openExportProducerEdge;
    return producerEdge && !producerEdge->IsNull();
}

const TopoDS_Edge& WireJoiner::resultWireProducerOpenExportEdge(
    const EdgeInfo& edgeInfo,
    const WireJoinerHistoryMaterializationLedger& materializationLedger,
    std::size_t edgeInfoIndex
) const
{
    if (wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
            materializationLedger,
            edgeInfoIndex
        )) {
        return *materializationLedger.edgeEntries[edgeInfoIndex].openExportProducerEdge;
    }
    return edgeInfo.edge;
}

std::optional<std::size_t> WireJoiner::producerLedgerReadyAHistoryRemoveProducerIndex(
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
        if (!wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
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
                      resultWireProducerOpenExportEdge(producer, materializationLedger, sourceIndex),
                      *resultEdge
                  )
                || edgeSamplesLieOnEdge(
                    *resultEdge,
                    resultWireProducerOpenExportEdge(producer, materializationLedger, sourceIndex)
                );
            const bool producerEdgeMatches
                = edgeEquivalentByGeometryAndEndpoints(producer.edge, *resultEdge)
                || edgeSamplesLieOnEdge(*resultEdge, producer.edge);
            if (!openExportMatches && !producerEdgeMatches) {
                continue;
            }
        }
        return sourceIndex;
    }
    return std::nullopt;
}

std::optional<std::size_t> WireJoiner::producerLedgerReadySameSourceSidecarProducerIndex(
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
        if (!wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
                materializationLedger,
                sourceIndex
            )) {
            continue;
        }
        if (resultEdge) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // Same-source sidecars are still strict Remove producers from
            // "aHistory->Remove(info.edge)"; if their already-exported producer edge does not
            // contain this result, the original producer edge can still be the valid curve source.
            const bool openExportMatches
                = edgeEquivalentByGeometryAndEndpoints(
                      resultWireProducerOpenExportEdge(producer, materializationLedger, sourceIndex),
                      *resultEdge
                  )
                || edgeSamplesLieOnEdge(
                    *resultEdge,
                    resultWireProducerOpenExportEdge(producer, materializationLedger, sourceIndex)
                );
            const bool producerEdgeMatches
                = edgeEquivalentByGeometryAndEndpoints(producer.edge, *resultEdge)
                || edgeSamplesLieOnEdge(*resultEdge, producer.edge);
            if (!openExportMatches && !producerEdgeMatches) {
                continue;
            }
        }
        return sourceIndex;
    }
    return std::nullopt;
}

ResultWireProducerIdentity WireJoiner::classifyResultWireProducerSlot(
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
    if (!materializationEntry.historyProducerChildWireCandidate) {
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
            resultWireProducerOpenExportEdge(edgeInfo, materializationLedger, edgeInfoIndex);
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
                if (wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
                        materializationLedger,
                        sourceIndex
                    )) {
                    sameSourceProducerLedgerSidecarIndex = sourceIndex;
                    break;
                }
            }
            const TopoDS_Edge& resultEdge =
                resultWireProducerOpenExportEdge(edgeInfo, materializationLedger, edgeInfoIndex);
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
            resultWireProducerOpenExportEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        const std::vector<std::size_t> sameSourceStrictRemoveSourceIndices
            = strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo);
        std::optional<std::size_t> sameSourceProducerLedgerSidecarIndex;
        for (const std::size_t sourceIndex : sameSourceStrictRemoveSourceIndices) {
            if (sourceIndex >= info.edges.size()) {
                continue;
            }
            if (wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
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
            resultWireProducerOpenExportEdge(edgeInfo, materializationLedger, edgeInfoIndex);
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
                            resultWireProducerOpenExportEdge(
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

void WireJoiner::attachResultWireProducerLedger(
    WireInfo& info,
    WireJoinerHistoryMaterializationLedger& materializationLedger
)
{
    if (materializationLedger.edgeEntries.size() < info.edges.size()) {
        materializationLedger.edgeEntries.resize(info.edges.size());
    }
    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < info.edges.size(); ++edgeInfoIndex) {
        if (!materializationLedger.edgeEntries[edgeInfoIndex].historyProducerChildWireCandidate) {
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

bool WireJoiner::resultWireProducerIdentityPublishesChildWireLedgerEntry(
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

bool WireJoiner::openWireCompoundChildWireHasSourceEdgeProducerOutput(
    const OpenWireCompoundWireInfo& childWire
) const
{
    return childWire.producerLedgerWireBuilt;
}

OpenWireCompoundExportSource WireJoiner::childWireFinalOpenExportSource(
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

ResultWireProducerIdentity WireJoiner::childWireResultWireProducerIdentity(
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

bool WireJoiner::memberSuppressedCurrentMemberProducerLedgerReady(
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

ResultWireProducerLedgerEntry WireJoiner::resultWireProducerLedgerEntryForChildWire(
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

void WireJoiner::applyWireJoinerHistoryMaterialization(
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
    for (const WireJoinerHistoryMaterializationBinding& binding : materializationLedger.bindings) {
        if (binding.resultSlotEdge.IsNull()) {
            continue;
        }
        if (binding.edgeInfoIndex >= info.edges.size()
            || wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
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
            // resultSlotEdge remains request-local endpoint evidence until the full MapperHistory
            // path can provide the producer child without a staged edge.
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
                            resultWireProducerOpenExportEdge(
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
                // ::getOpenWires() consumes MapperHistory(aHistory). Use the producer edge curve or
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
                    resultWireProducerOpenExportEdge(
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
                    resultWireProducerOpenExportEdge(
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
                materializationEntry.openExportProducerEdge = *sourceEdgeExportShape;
            }
            materializationEntry.historyProducerChildWireCandidate = true;
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
        if (!materializationEntry.historyProducerChildWireCandidate
            || wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
                materializationLedger,
                edgeInfoIndex
            )
            || !hasAHistoryRemoveForeignSourceLineage
            || !hasSourceLineageRemovedSourceEdgeInfo) {
            continue;
        }
        const TopoDS_Edge resultEdge =
            resultWireProducerOpenExportEdge(edgeInfo, materializationLedger, edgeInfoIndex);
        std::optional<TopoDS_Edge> sidecarSourceEdgeExportShape;
        for (const std::size_t sidecarIndex :
             strictRemovedSourceEdgeInfoIndicesForSourceLineage(info, edgeInfo)) {
            if (sidecarIndex >= info.edges.size()) {
                continue;
            }
            const EdgeInfo& sidecar = info.edges[sidecarIndex];
            if (!wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
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
                resultWireProducerOpenExportEdge(sidecar, materializationLedger, sidecarIndex),
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
        materializationEntry.openExportProducerEdge = *sidecarSourceEdgeExportShape;
    }
}

void WireJoiner::buildFinalEdgeOwnership(
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
            edgeInfoIndex < materializationLedger.edgeEntries.size()
            && materializationLedger.edgeEntries[edgeInfoIndex].historyProducerChildWireCandidate;
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

void WireJoiner::initializeEdgeInfo(EdgeInfo& edgeInfo, const TopoDS_Edge& edge) const
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

const TopoDS_Shape& WireJoiner::EdgeInfo::shape(bool forward) const
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

TopoDS_Wire WireJoiner::EdgeInfo::wire(bool forward) const
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

TopoDS_Wire WireJoiner::wireFromVertices(const WireInfo& info, const std::vector<WireVertex>& vertices) const
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

void WireJoiner::rebuildAdjacentList(WireInfo& info)
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

std::optional<WireJoiner::WireVertex> WireJoiner::soleActiveAdjacentEdge(
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

void WireJoiner::extendSuperEdgeCandidate(
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

void WireJoiner::recordSuperEdgeCandidates(WireInfo& info)
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

bool WireJoiner::markOpenLeafEdges(WireInfo& info)
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

void WireJoiner::rebuildOrderedVertices(WireInfo& info)
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

std::optional<WireJoiner::ClosedWireSearchResult> WireJoiner::findClosedWirePath(
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

std::size_t WireJoiner::assignClosedWireOwners(WireInfo& info, bool assignOwners)
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

gp_Pnt WireJoiner::vertexPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p1 : edge.p2;
}

gp_Pnt WireJoiner::vertexOtherPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p2 : edge.p1;
}

std::optional<std::size_t> WireJoiner::ownerVertexIndex(
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

WireJoiner::TightBoundExistingWireSearchTrace WireJoiner::traceExistingWireSearchForCandidate(
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

bool WireJoiner::findTightBoundBranchPathToPoint(
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

bool WireJoiner::findBranchPathToPointSkippingOwner(
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

std::optional<WireJoiner::TightBoundTransferPath> WireJoiner::tightBoundTransferPathForCandidate(
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

std::optional<WireJoiner::TightBoundTransferPath> WireJoiner::tightBoundTransferPathForExistingWireHit(
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

bool WireJoiner::isDoneOwner(const WireInfo& info, std::size_t ownerId) const
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

void WireJoiner::recordExhaustOwnerVertex(WireInfo& info, const WireVertex& vertex, std::size_t ownerId)
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

WireJoiner::ExhaustAdjacentSearchTrace WireJoiner::traceExhaustAdjacentSearch(
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

void WireJoiner::recordExhaustAdjacentSecondaryOwners(WireInfo& info)
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

void WireJoiner::recordExhaustTightBoundLifecycle(WireInfo& info)
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

void WireJoiner::recordBuildClosedWireRemovalLifecycle(WireInfo& info)
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

void WireJoiner::recordRepeatedSplitExhaustRerunLifecycle(
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

void WireJoiner::recordBranchSearchCandidatesForOwner(
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

void WireJoiner::recordBranchSearchCandidates(WireInfo& info, const std::vector<TopoDS_Face>& boundedFaces)
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

bool WireJoiner::recordTightBoundTransferWire(WireInfo& info, OwnerWireInfo& owner)
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

void WireJoiner::recordTightBoundLifecycle(WireInfo& info)
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

void WireJoiner::recordOpenWireCompoundLedger(
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
            materializationEntry.historyProducerChildWireCandidate
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
            wireJoinerHistoryMaterializationLedgerHasOpenExportProducerEdge(
                materializationLedger,
                edgeIndex
            );
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() stores final child wires in openWireCompound with
        // "builder.Add(openWireCompound, info.wire())", then ::getOpenWires() consumes
        // MapperHistory(aHistory). Use the source/aHistory producer shape only to materialize the
        // child-wire producer wire; do not store a copied producer edge on the child-wire ledger.
        std::optional<TopoDS_Edge> historyMaterializationProducerEdge =
            hasHistoryMaterializationProducerOpenExportShape
            ? materializationEntry.openExportProducerEdge
            : std::optional<TopoDS_Edge> {};
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
            // the scoped per-edge producer shape remains a temporary history materialization bridge.
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
        if (materializationEntry.historyProducerChildWireCandidate
            && (!hasChildProducerLedgerEdge || currentMemberProducerShape)
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

WireJoinerLedgerSummary WireJoiner::ledgerSummary() const
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

WireJoinerHistorySummary WireJoiner::historySummary() const
{
    return historySummary_;
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(const std::string& historyPrefix, bool noOriginal) const
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

}  // namespace cad_core::part
