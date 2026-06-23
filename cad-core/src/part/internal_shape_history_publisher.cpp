#include "cad_core/part/internal_shape_history_ledger.h"

#include "cad_core/part/property_topo_shape.h"
#include "internal_shape_history_ledger_detail.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace cad_core::part
{

namespace
{

constexpr const char* internalShapeSuffix = ".InternalShape";

void addDistinctString(std::vector<std::string>& values, const std::string& value)
{
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

int findSameShapeIndex(const TopTools_IndexedMapOfShape& shapes, const TopoDS_Shape& shape)
{
    for (int index = 1; index <= shapes.Extent(); ++index) {
        if (shapes(index).IsSame(shape)) {
            return index;
        }
    }
    return 0;
}

std::string sourceOwnerForInternalShape(const std::string& owner)
{
    if (owner.size() > std::char_traits<char>::length(internalShapeSuffix)
        && owner.compare(
               owner.size() - std::char_traits<char>::length(internalShapeSuffix),
               std::char_traits<char>::length(internalShapeSuffix),
               internalShapeSuffix
           )
            == 0) {
        return owner.substr(0, owner.size() - std::char_traits<char>::length(internalShapeSuffix));
    }
    return owner;
}

std::string sourceEdgeName(std::size_t sourceEdgeIndex)
{
    if (sourceEdgeIndex == 0U) {
        return {};
    }
    return "Edge" + std::to_string(sourceEdgeIndex);
}

std::optional<TopAbs_ShapeEnum> elementKindFromName(const std::string& elementName)
{
    const std::size_t dot = elementName.rfind('.');
    const std::string localName = dot == std::string::npos ? elementName
                                                           : elementName.substr(dot + 1);
    const auto parsed = parseSubshapeName(localName);
    if (!parsed) {
        return std::nullopt;
    }
    return parsed->kind;
}

std::string shapeKindForHistoryElement(const std::string& elementName)
{
    const auto kind = elementKindFromName(elementName);
    return kind ? subshapeKindName(*kind) : "shape";
}

MapperHistoryRelation mapperRelationForProducerRelation(const std::string& relation)
{
    if (relation == "preserved") {
        return MapperHistoryRelation::Preserved;
    }
    if (relation == "generated") {
        return MapperHistoryRelation::Generated;
    }
    if (relation == "split") {
        return MapperHistoryRelation::Split;
    }
    if (relation == "deleted") {
        return MapperHistoryRelation::Deleted;
    }
    return MapperHistoryRelation::Modified;
}

MapperHistoryRecoverability mapperRecoverabilityForProducerRelation(const std::string& relation)
{
    if (relation == "deleted") {
        return MapperHistoryRecoverability::Deleted;
    }
    if (relation == "split") {
        return MapperHistoryRecoverability::NeedsReselect;
    }
    return MapperHistoryRecoverability::Resolved;
}

std::string diagnosticStatusForProducerRelation(const std::string& relation)
{
    if (relation == "deleted") {
        return "deleted_stable_subname";
    }
    if (relation == "split") {
        return "split_stable_subname";
    }
    return {};
}

InternalShapeHistoryRelation publicationRelationFromName(const std::string& relation)
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

void addPublishedHistory(
    InternalShapeHistoryPublication& publication,
    InternalShapeHistoryRelation relation,
    const std::string& element,
    const std::vector<std::string>& sources
)
{
    if (element.empty()) {
        return;
    }
    const auto duplicate = std::find_if(
        publication.elementHistory.begin(),
        publication.elementHistory.end(),
        [&](const InternalShapePublishedElementHistory& entry) {
            return entry.relation == relation && entry.element == element && entry.sources == sources;
        }
    );
    if (duplicate == publication.elementHistory.end()) {
        publication.elementHistory.push_back(
            InternalShapePublishedElementHistory {relation, element, sources}
        );
    }
}

void appendProducerMapperEvent(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    const std::string& sourceSubname,
    const std::string& targetSubname,
    const std::string& shapeKind,
    const std::string& relation,
    const std::string& makerStage,
    nlohmann::json evidence,
    const std::string& diagnosticStatus = {}
)
{
    MapperHistoryEvent event;
    event.source = MapperHistoryEndpoint {sourceOwnerForInternalShape(input.owner), sourceSubname};
    event.target = MapperHistoryEndpoint {input.owner, targetSubname};
    event.shapeKind = shapeKind;
    event.relation = mapperRelationForProducerRelation(relation);
    event.makerStage = makerStage;
    event.evidence = std::move(evidence);
    event.recoverability = diagnosticStatus.empty()
        ? mapperRecoverabilityForProducerRelation(relation)
        : MapperHistoryRecoverability::Diagnostic;
    event.diagnosticStatus = diagnosticStatus.empty() ? diagnosticStatusForProducerRelation(relation)
                                                      : diagnosticStatus;
    addMapperHistoryEvent(publication.mapperHistory, std::move(event));
}

void appendSummaryMapperEvent(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    MapperHistoryRelation relation,
    const std::string& makerStage,
    const std::string& diagnosticStatus,
    nlohmann::json evidence
)
{
    MapperHistoryEvent event;
    event.source = MapperHistoryEndpoint {input.owner, {}};
    event.target = MapperHistoryEndpoint {input.owner, {}};
    event.shapeKind = "shape";
    event.relation = relation;
    event.makerStage = makerStage;
    event.evidence = std::move(evidence);
    event.recoverability = MapperHistoryRecoverability::Diagnostic;
    event.diagnosticStatus = "summary_only:" + diagnosticStatus;
    addMapperHistoryEvent(publication.mapperHistory, std::move(event));
}

nlohmann::json faceMakerSummaryEvidence(const SketchInternalHistoryContext& history)
{
    return {
        {"producer", "FaceMakerBuildFace"},
        {"source_edge_count", history.sourceEdgeCount},
        {"pre_split_edge_count", history.preSplitEdgeCount},
        {"splitter_edge_count", history.splitterEdgeCount},
        {"bounded_face_count", history.boundedFaceCount},
    };
}

nlohmann::json wireJoinerSummaryEvidence(const SketchInternalHistoryContext& history)
{
    return {
        {"producer", "WireJoiner"},
        {"source_edge_count", history.wireJoinerSourceEdgeCount},
        {"split_result_edge_count", history.wireJoinerSplitResultEdgeCount},
        {"history_event_count", history.wireJoinerHistoryEvents.size()},
        {"open_export_count", history.wireJoinerOpenExportHistoryEntries.size()},
        {"modified_history_count", history.wireJoinerModifiedHistoryCount},
        {"generated_history_count", history.wireJoinerGeneratedHistoryCount},
        {"deleted_history_count", history.wireJoinerDeletedHistoryCount},
    };
}

void publishGeneratedFaceHistory(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    const SketchInternalHistoryContext& history
)
{
    if (history.faceMakerBoundedFaceEvidence.empty()) {
        return;
    }

    TopTools_IndexedMapOfShape internalFaces;
    TopExp::MapShapes(input.internalShape, TopAbs_FACE, internalFaces);
    for (const SketchInternalFaceMakerBoundedFaceEvidence& faceEvidence :
         history.faceMakerBoundedFaceEvidence) {
        int internalFaceIndex = 0;
        if (!faceEvidence.face.IsNull()) {
            internalFaceIndex = findSameShapeIndex(internalFaces, faceEvidence.face);
        }
        if (internalFaceIndex <= 0
            && faceEvidence.boundedFaceIndex <= static_cast<std::size_t>(internalFaces.Extent())) {
            internalFaceIndex = static_cast<int>(faceEvidence.boundedFaceIndex);
        }
        if (internalFaceIndex <= 0) {
            continue;
        }

        std::vector<std::string> sources;
        for (const std::size_t sourceIndex : faceEvidence.sourceEdgeIndices) {
            addDistinctString(sources, sourceEdgeName(sourceIndex));
        }
        const std::string targetName = "InternalFace" + std::to_string(internalFaceIndex);
        addPublishedHistory(
            publication,
            InternalShapeHistoryRelation::Generated,
            targetName,
            sources
        );
        for (const std::size_t sourceIndex : faceEvidence.sourceEdgeIndices) {
            appendProducerMapperEvent(
                publication,
                input,
                sourceEdgeName(sourceIndex),
                targetName,
                "face",
                "generated",
                "facemaker:outer_boundary",
                {
                    {"producer", "FaceMakerBuildFace"},
                    {"source_edge_index", sourceIndex},
                    {"source_edge", sourceEdgeName(sourceIndex)},
                    {"target_internal_element", targetName},
                    {"bounded_face_index", faceEvidence.boundedFaceIndex},
                }
            );
        }
    }
}

void publishFaceMakerTerminalHistory(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    const SketchInternalHistoryContext& history
)
{
    if (history.faceMakerEdgeEvidence.empty()) {
        const bool hasSummaryOnlyHistory = history.preSplitHistory || history.splitterHistory
            || history.preSplitEdgeCount > history.sourceEdgeCount
            || history.splitterEdgeCount > history.sourceEdgeCount;
        if (hasSummaryOnlyHistory) {
            addDistinctString(publication.elementHistoryStatus, "facemaker_history:summary_only");
        }
        return;
    }

    TopTools_IndexedMapOfShape internalEdges;
    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape rawVertices;
    TopExp::MapShapes(input.internalShape, TopAbs_EDGE, internalEdges);
    TopExp::MapShapes(input.rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(input.rawShape, TopAbs_VERTEX, rawVertices);
    std::set<std::size_t> deletedSourceEdges;

    for (const SketchInternalFaceMakerEdgeEvidence& evidence : history.faceMakerEdgeEvidence) {
        const std::string sourceName = sourceEdgeName(evidence.sourceEdgeIndex);
        if (sourceName.empty()) {
            continue;
        }
        if (evidence.relation == "deleted") {
            deletedSourceEdges.insert(evidence.sourceEdgeIndex);
            addPublishedHistory(
                publication,
                InternalShapeHistoryRelation::Deleted,
                sourceName,
                {sourceName}
            );
            appendProducerMapperEvent(
                publication,
                input,
                sourceName,
                {},
                "edge",
                "deleted",
                evidence.makerStage,
                {
                    {"producer", "FaceMakerBuildFace"},
                    {"source_edge_index", evidence.sourceEdgeIndex},
                    {"source_edge", sourceName},
                    {"target_edge_index", evidence.targetEdgeIndex},
                }
            );
            continue;
        }
        if (evidence.relation != "split" || evidence.targetEdge.IsNull()) {
            continue;
        }
        const int internalEdgeIndex = findSameShapeIndex(internalEdges, evidence.targetEdge);
        if (internalEdgeIndex <= 0) {
            appendProducerMapperEvent(
                publication,
                input,
                sourceName,
                {},
                "edge",
                "split",
                evidence.makerStage,
                {
                    {"producer", "FaceMakerBuildFace"},
                    {"source_edge_index", evidence.sourceEdgeIndex},
                    {"source_edge", sourceName},
                    {"target_edge_index", evidence.targetEdgeIndex},
                },
                "missing_producer_identity"
            );
            continue;
        }
        const std::string targetName = "InternalEdge" + std::to_string(internalEdgeIndex);
        addPublishedHistory(
            publication,
            InternalShapeHistoryRelation::Split,
            targetName,
            {sourceName}
        );
        appendProducerMapperEvent(
            publication,
            input,
            sourceName,
            targetName,
            "edge",
            "split",
            evidence.makerStage,
            {
                {"producer", "FaceMakerBuildFace"},
                {"source_edge_index", evidence.sourceEdgeIndex},
                {"source_edge", sourceName},
                {"target_internal_element", targetName},
                {"target_edge_index", evidence.targetEdgeIndex},
            }
        );
    }

    if (deletedSourceEdges.empty() || !input.internalElementMap.is_object()) {
        return;
    }
    std::set<int> deletedVertexIndices;
    for (const std::size_t sourceEdgeIndex : deletedSourceEdges) {
        if (sourceEdgeIndex == 0U || sourceEdgeIndex > static_cast<std::size_t>(rawEdges.Extent())) {
            continue;
        }
        for (TopExp_Explorer explorer(rawEdges(static_cast<int>(sourceEdgeIndex)), TopAbs_VERTEX);
             explorer.More();
             explorer.Next()) {
            const int vertexIndex = findSameShapeIndex(rawVertices, explorer.Current());
            if (vertexIndex > 0) {
                deletedVertexIndices.insert(vertexIndex);
            }
        }
    }
    for (const int index : deletedVertexIndices) {
        const std::string sourceVertex = "Vertex" + std::to_string(index);
        if (input.internalElementMap.contains(sourceVertex)) {
            continue;
        }
        addPublishedHistory(
            publication,
            InternalShapeHistoryRelation::Deleted,
            sourceVertex,
            {sourceVertex}
        );
        appendProducerMapperEvent(
            publication,
            input,
            sourceVertex,
            {},
            "vertex",
            "deleted",
            "facemaker:vertex",
            {
                {"producer", "FaceMakerBuildFace"},
                {"deleted_source_edge_indices", deletedSourceEdges},
            },
            "deleted_stable_subname"
        );
    }
}

const std::vector<std::size_t>& wireJoinerOpenCompoundSourceEdgeIndices(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry,
    const SketchInternalWireJoinerHistoryEvent* event
)
{
    if (event && !event->sourceEdgeIndices.empty()) {
        return event->sourceEdgeIndices;
    }
    if (!entry.openWireCompoundSourceEdgeIndices.empty()) {
        return entry.openWireCompoundSourceEdgeIndices;
    }
    return entry.sourceEdgeIndices;
}

bool wireJoinerOpenExportEntryConsumesChildWireOwnership(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry,
    const SketchInternalWireJoinerHistoryEvent* event
)
{
    return event != nullptr && entry.wireJoinerHistoryEventFromChildWireLedger
        && entry.openWireCompoundChildShapeIdentityRecorded
        && entry.openWireCompoundChildWireEdgeCount > 0U
        && entry.openWireCompoundChildWireVertexCount > 0U
        && event->openWireCompoundChildWireInfoIndex == entry.openWireCompoundChildWireInfoIndex;
}

std::string wireJoinerRelationForOpenExportEntry(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry,
    const SketchInternalWireJoinerHistoryEvent* event
)
{
    if (event && event->relationFromChildWireLedger) {
        return event->relation;
    }
    return entry.wireJoinerHistoryRelation;
}

const SketchInternalWireJoinerHistoryEvent* wireJoinerHistoryEventForOpenExportEntry(
    const SketchInternalHistoryContext& history,
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry
)
{
    if (!entry.wireJoinerHistoryEventFromChildWireLedger) {
        return nullptr;
    }
    const auto eventIt = std::find_if(
        history.wireJoinerHistoryEvents.begin(),
        history.wireJoinerHistoryEvents.end(),
        [&](const SketchInternalWireJoinerHistoryEvent& event) {
            return event.eventIndex == entry.wireJoinerHistoryEventIndex
                && event.openExportIndex == entry.openExportIndex;
        }
    );
    if (eventIt == history.wireJoinerHistoryEvents.end()) {
        return nullptr;
    }
    return &(*eventIt);
}

std::string wireJoinerDiagnosticStatusForOpenExportEntry(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry,
    bool targetFound
)
{
    if (entry.openWireCompoundNoOriginalPurgedByLedger) {
        return "no_original_purge";
    }
    if (entry.missingOpenWireCompoundChildWire) {
        return "missing_open_wire_compound_child_wire";
    }
    if (entry.openWireCompoundCurrentMemberSplitLedgerVertexMultiplicityBlocked) {
        return "wire_joiner_current_member_vertex_multiplicity_blocked";
    }
    if (!targetFound) {
        return "open_wire_compound_target_not_found";
    }
    return {};
}

nlohmann::json wireJoinerOpenExportEvidenceJson(
    const SketchInternalWireJoinerOpenExportHistoryEntry& entry,
    const SketchInternalWireJoinerHistoryEvent* event,
    std::size_t sourceEdgeIndex,
    const std::string& targetName,
    const std::string& diagnosticStatus
)
{
    const std::string relation = wireJoinerRelationForOpenExportEntry(entry, event);
    nlohmann::json evidence = {
        {"producer", "WireJoiner"},
        {"source_edge_index", sourceEdgeIndex},
        {"source_edge", sourceEdgeName(sourceEdgeIndex)},
        {"target_internal_element", targetName},
        {"relation", relation},
        {"target_open_export_index", entry.openExportIndex},
    };
    if (!diagnosticStatus.empty()) {
        evidence["diagnostic_code"] = diagnosticStatus;
    }
    return evidence;
}

bool publishWireJoinerOpenExportHistory(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    const SketchInternalHistoryContext& history
)
{
    if (history.wireJoinerOpenExportHistoryEntries.empty()) {
        return false;
    }

    TopTools_IndexedMapOfShape internalEdges;
    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape rawVertices;
    TopExp::MapShapes(input.internalShape, TopAbs_EDGE, internalEdges);
    TopExp::MapShapes(input.rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(input.rawShape, TopAbs_VERTEX, rawVertices);

    bool hasConcreteChildWireOwnershipEvent = false;
    std::map<std::string, std::set<std::string>> childWireElementMapTargets;
    for (const SketchInternalWireJoinerOpenExportHistoryEntry& entry :
         history.wireJoinerOpenExportHistoryEntries) {
        const SketchInternalWireJoinerHistoryEvent* historyEvent =
            wireJoinerHistoryEventForOpenExportEntry(history, entry);
        const std::string relation = wireJoinerRelationForOpenExportEntry(entry, historyEvent);
        int internalEdgeIndex = 0;
        if (!entry.openExportEdge.IsNull()) {
            internalEdgeIndex = findSameShapeIndex(internalEdges, entry.openExportEdge);
        }
        const bool targetFound = internalEdgeIndex > 0;
        const std::string targetName = targetFound
            ? "InternalEdge" + std::to_string(internalEdgeIndex)
            : std::string();
        const std::string diagnosticStatus =
            wireJoinerDiagnosticStatusForOpenExportEntry(entry, targetFound);
        const std::vector<std::size_t>& sourceEdgeIndices =
            wireJoinerOpenCompoundSourceEdgeIndices(entry, historyEvent);
        if (sourceEdgeIndices.empty()) {
            appendProducerMapperEvent(
                publication,
                input,
                {},
                targetName,
                "edge",
                relation,
                "wire_joiner:open_export",
                wireJoinerOpenExportEvidenceJson(entry, historyEvent, 0U, targetName, diagnosticStatus),
                diagnosticStatus.empty() ? "missing_open_wire_compound_source_lineage"
                                         : diagnosticStatus
            );
            continue;
        }

        const bool consumesChildWireOwnership =
            wireJoinerOpenExportEntryConsumesChildWireOwnership(entry, historyEvent);
        if (consumesChildWireOwnership && targetFound) {
            hasConcreteChildWireOwnershipEvent = true;
        }
        for (const std::size_t zeroBasedSourceIndex : sourceEdgeIndices) {
            const std::size_t sourceEdgeIndex = zeroBasedSourceIndex + 1U;
            const std::string sourceName = sourceEdgeName(sourceEdgeIndex);
            if (targetFound && relation != "deleted" && consumesChildWireOwnership) {
                childWireElementMapTargets[sourceName].insert(targetName);
            }
            if (relation == "deleted") {
                addPublishedHistory(
                    publication,
                    InternalShapeHistoryRelation::Deleted,
                    sourceName,
                    {sourceName}
                );
                if (input.internalElementMap.is_object()
                    && sourceEdgeIndex <= static_cast<std::size_t>(rawEdges.Extent())) {
                    for (TopExp_Explorer explorer(
                             rawEdges(static_cast<int>(sourceEdgeIndex)),
                             TopAbs_VERTEX
                         );
                         explorer.More();
                         explorer.Next()) {
                        const int vertexIndex = findSameShapeIndex(rawVertices, explorer.Current());
                        if (vertexIndex <= 0) {
                            continue;
                        }
                        const std::string sourceVertex = "Vertex" + std::to_string(vertexIndex);
                        if (input.internalElementMap.contains(sourceVertex)) {
                            continue;
                        }
                        addPublishedHistory(
                            publication,
                            InternalShapeHistoryRelation::Deleted,
                            sourceVertex,
                            {sourceVertex}
                        );
                        appendProducerMapperEvent(
                            publication,
                            input,
                            sourceVertex,
                            {},
                            "vertex",
                            "deleted",
                            "wire_joiner:no_original",
                            {
                                {"producer", "WireJoiner"},
                                {"source_edge_index", sourceEdgeIndex},
                                {"source_edge", sourceName},
                                {"target_open_export_index", entry.openExportIndex},
                                {"diagnostic_code", "no_original_purge"},
                            },
                            "no_original_purge"
                        );
                    }
                }
            }
            else if (relation == "split" && targetFound) {
                addPublishedHistory(
                    publication,
                    InternalShapeHistoryRelation::Split,
                    targetName,
                    {sourceName}
                );
            }
            else if (relation == "generated" && targetFound) {
                addPublishedHistory(
                    publication,
                    InternalShapeHistoryRelation::Generated,
                    targetName,
                    {sourceName}
                );
            }
            appendProducerMapperEvent(
                publication,
                input,
                sourceName,
                targetName,
                "edge",
                relation,
                "wire_joiner:open_export",
                wireJoinerOpenExportEvidenceJson(
                    entry,
                    historyEvent,
                    sourceEdgeIndex,
                    targetName,
                    diagnosticStatus
                ),
                diagnosticStatus
            );
        }
    }

    for (const auto& [sourceName, targets] : childWireElementMapTargets) {
        if (targets.size() != 1U) {
            continue;
        }
        publication.elementMapAliases[sourceName] = *targets.begin();
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:element_map");
    }
    return hasConcreteChildWireOwnershipEvent;
}

void publishSummaryStatuses(
    InternalShapeHistoryPublication& publication,
    const SketchInternalHistoryContext& history
)
{
    if (history.preSplitHistory) {
        addDistinctString(publication.elementHistoryStatus, "facemaker_history:pre_split");
    }
    if (history.splitterHistory) {
        addDistinctString(publication.elementHistoryStatus, "facemaker_history:splitter");
    }
    if (history.wireJoinerSplitterHistory) {
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:splitter");
    }
    if (history.wireJoinerModifiedHistoryCount > 0U) {
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:modified");
    }
    if (history.wireJoinerGeneratedHistoryCount > 0U) {
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:generated");
    }
    if (history.wireJoinerDeletedHistoryCount > 0U) {
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:deleted");
    }
    if (!history.wireJoinerOpenExportHistoryEntries.empty()) {
        addDistinctString(publication.elementHistoryStatus, "wire_joiner_history:open_export");
    }
}

void publishSummaryMapperEvents(
    InternalShapeHistoryPublication& publication,
    const InternalShapeHistoryPublishInput& input,
    const SketchInternalHistoryContext& history,
    bool hasConcreteWireJoinerOpenExportEvent
)
{
    if (history.preSplitHistory) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Split,
            "facemaker:pre_split",
            "facemaker_history:pre_split",
            faceMakerSummaryEvidence(history)
        );
    }
    if (history.splitterHistory) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Split,
            "facemaker:splitter",
            "facemaker_history:splitter",
            faceMakerSummaryEvidence(history)
        );
    }
    if (history.wireJoinerSplitterHistory) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Split,
            "wire_joiner:splitter",
            "wire_joiner_history:splitter",
            wireJoinerSummaryEvidence(history)
        );
    }
    if (history.wireJoinerModifiedHistoryCount > 0U) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Modified,
            "wire_joiner:modified",
            "wire_joiner_history:modified",
            wireJoinerSummaryEvidence(history)
        );
    }
    if (history.wireJoinerGeneratedHistoryCount > 0U) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Generated,
            "wire_joiner:generated",
            "wire_joiner_history:generated",
            wireJoinerSummaryEvidence(history)
        );
    }
    if (history.wireJoinerDeletedHistoryCount > 0U) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Deleted,
            "wire_joiner:deleted",
            "wire_joiner_history:deleted",
            wireJoinerSummaryEvidence(history)
        );
    }
    if (!history.wireJoinerOpenExportHistoryEntries.empty()
        && !hasConcreteWireJoinerOpenExportEvent) {
        appendSummaryMapperEvent(
            publication,
            input,
            MapperHistoryRelation::Preserved,
            "wire_joiner:open_export",
            "wire_joiner_history:open_export",
            wireJoinerSummaryEvidence(history)
        );
    }
}

}  // namespace

InternalShapeHistoryPublication InternalShapeHistoryLedger::publishForInternalShape(
    const InternalShapeHistoryPublishInput& input
) const
{
    InternalShapeHistoryPublication publication;
    publication.diagnostics = diagnosticsJson();

    const SketchInternalHistoryContext& history = data_->compatibilityHistory;
    publishGeneratedFaceHistory(publication, input, history);
    publishFaceMakerTerminalHistory(publication, input, history);
    const bool hasConcreteWireJoinerOpenExportEvent =
        publishWireJoinerOpenExportHistory(publication, input, history);
    publishSummaryStatuses(publication, history);
    publishSummaryMapperEvents(publication, input, history, hasConcreteWireJoinerOpenExportEvent);
    return publication;
}

}  // namespace cad_core::part
