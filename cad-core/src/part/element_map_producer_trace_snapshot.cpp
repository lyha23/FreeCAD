#include "cad_core/part/element_map_producer_trace_snapshot.h"

#include "element_map_producer_trace_snapshot_internal.h"

#include "cad_core/app/string_hasher.h"
#include "cad_core/part/property_topo_shape.h"

#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <Standard_Failure.hxx>

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <utility>

namespace cad_core::part
{

namespace
{

constexpr std::array<TopAbs_ShapeEnum, 3> kKinds {
    TopAbs_VERTEX,
    TopAbs_EDGE,
    TopAbs_FACE,
};

std::string traceShapeKindName(TopAbs_ShapeEnum kind)
{
    switch (kind) {
        case TopAbs_COMPOUND: return "Compound";
        case TopAbs_COMPSOLID: return "CompSolid";
        case TopAbs_SOLID: return "Solid";
        case TopAbs_SHELL: return "Shell";
        case TopAbs_WIRE: return "Wire";
        default: break;
    }
    std::string name = subshapeKindName(kind);
    if (!name.empty()) {
        name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
    }
    return name;
}

nlohmann::json stringIdJson(const app::StringId& id)
{
    return {{"value", id.value}, {"index", id.index}};
}

nlohmann::json stringIdsJson(const std::vector<app::StringId>& ids)
{
    nlohmann::json result = nlohmann::json::array();
    for (const app::StringId& id : ids) {
        result.push_back(stringIdJson(id));
    }
    return result;
}

std::vector<app::ElementMapProducerTrace::SidRef> ledgerSidRefs(const NamedShape& namedShape)
{
    std::vector<app::ElementMapProducerTrace::SidRef> refs;
    for (const auto& item : namedShape.elementMapEntries) {
        for (const ElementMapEntry& entry : item.second) {
            for (const app::StringId& ref : entry.elementIdRefs) {
                refs.push_back({ref.value, ref.index});
            }
        }
    }
    return refs;
}

std::vector<app::ElementMapProducerTrace::SidRef> stringTableSidRefs(
    const nlohmann::json& state
)
{
    std::vector<app::ElementMapProducerTrace::SidRef> refs;
    for (const auto& entry : state.value("entries", nlohmann::json::array())) {
        for (const auto& ref : entry.value("related", nlohmann::json::array())) {
            refs.push_back({ref.value("value", 0L), ref.value("index", 0)});
        }
    }
    return refs;
}

std::vector<long> stringTableDefinedSids(const nlohmann::json& state)
{
    std::vector<long> result;
    for (const auto& entry : state.value("entries", nlohmann::json::array())) {
        const long value = entry.value("value", 0L);
        if (value > 0) {
            result.push_back(value);
        }
    }
    return result;
}

nlohmann::json sidRefsJson(
    const std::vector<app::ElementMapProducerTrace::SidRef>& refs
)
{
    nlohmann::json result = nlohmann::json::array();
    for (const app::ElementMapProducerTrace::SidRef& ref : refs) {
        result.push_back({{"value", ref.value}, {"index", ref.index}});
    }
    return result;
}

std::string indexedNameForShape(const TopoDS_Shape& shape,
                                const TopoDS_Shape& candidate,
                                TopAbs_ShapeEnum kind)
{
    if (std::find(kKinds.begin(), kKinds.end(), kind) == kKinds.end()) {
        return {};
    }
    TopTools_IndexedMapOfShape indexed;
    TopExp::MapShapes(shape, kind, indexed);
    for (int index = 1; index <= indexed.Extent(); ++index) {
        if (indexed(index).IsSame(candidate)) {
            return traceShapeKindName(kind) + std::to_string(index);
        }
    }
    return {};
}

nlohmann::json mapperTargets(const TopTools_ListOfShape& values,
                             const TopoDS_Shape& output,
                             TopAbs_ShapeEnum kind)
{
    nlohmann::json targets = nlohmann::json::array();
    for (TopTools_ListIteratorOfListOfShape iterator(values); iterator.More(); iterator.Next()) {
        const TopoDS_Shape& target = iterator.Value();
        const std::string indexed = indexedNameForShape(output, target, target.ShapeType());
        nlohmann::json outputMembers = nlohmann::json::array();
        if (indexed.empty() && !target.IsNull()) {
            for (const TopAbs_ShapeEnum memberKind : kKinds) {
                TopTools_IndexedMapOfShape outputShapes;
                TopTools_IndexedMapOfShape targetShapes;
                TopExp::MapShapes(output, memberKind, outputShapes);
                TopExp::MapShapes(target, memberKind, targetShapes);
                for (int member = 1; member <= outputShapes.Extent(); ++member) {
                    if (targetShapes.FindIndex(outputShapes(member)) <= 0) {
                        continue;
                    }
                    outputMembers.push_back({
                        {"indexed", traceShapeKindName(memberKind) + std::to_string(member)},
                        {"shapeType", traceShapeKindName(memberKind)},
                    });
                }
            }
        }
        targets.push_back({
            {"indexed", indexed},
            {"shapeType", traceShapeKindName(target.ShapeType())},
            {"orientation", static_cast<int>(target.Orientation())},
            {"requestedSourceKind", traceShapeKindName(kind)},
            {"relationStatus", indexed.empty() ? "expanded_to_output" : "resolved"},
            {"outputMembers", std::move(outputMembers)},
        });
    }
    return targets;
}

}  // namespace

nlohmann::json inspectShapeInventory(const TopoDS_Shape& shape)
{
    nlohmann::json inventory = {
        {"isNull", shape.IsNull()},
        {"shapeType", shape.IsNull() ? "Null" : traceShapeKindName(shape.ShapeType())},
        {"orientation", shape.IsNull() ? 0 : static_cast<int>(shape.Orientation())},
        {"indexed", nlohmann::json::object()},
    };
    if (shape.IsNull()) {
        return inventory;
    }
    for (const TopAbs_ShapeEnum kind : kKinds) {
        const std::string kindName = traceShapeKindName(kind);
        TopTools_IndexedMapOfShape values;
        TopExp::MapShapes(shape, kind, values);
        nlohmann::json entries = nlohmann::json::array();
        for (int index = 1; index <= values.Extent(); ++index) {
            entries.push_back({
                {"indexed", kindName + std::to_string(index)},
                {"orientation", static_cast<int>(values(index).Orientation())},
            });
        }
        inventory["indexed"][kindName] = std::move(entries);
    }
    return inventory;
}

nlohmann::json inspectMapperHistory(const std::vector<MapperHistoryEvent>& history)
{
    return mapperHistoryToJson(history);
}

nlohmann::json inspectNamedShapeLedger(const NamedShape& namedShape, const std::string& role)
{
    nlohmann::json entries = nlohmann::json::object();
    for (const auto& item : namedShape.elementMapEntries) {
        nlohmann::json values = nlohmann::json::array();
        for (const ElementMapEntry& entry : item.second) {
            const auto provenance = namedShape.mappedNameProvenance.find(entry.mappedName);
            values.push_back({
                {"rawMappedName",
                 provenance != namedShape.mappedNameProvenance.end()
                     ? provenance->second.rawMappedName
                     : entry.mappedName},
                {"elementIdRefs", stringIdsJson(entry.elementIdRefs)},
            });
        }
        entries[item.first] = std::move(values);
    }

    nlohmann::json reverseLookup = nlohmann::json::object();
    for (const auto& item : namedShape.elementMap) {
        reverseLookup[item.first] = item.second;
    }

    nlohmann::json provenance = nlohmann::json::object();
    for (const auto& item : namedShape.mappedNameProvenance) {
        const MappedNameProvenance& value = item.second;
        provenance[item.first] = {
            {"entryKey", value.entryKey},
            {"currentElement", value.currentElement},
            {"sourceElement", value.sourceElement},
            {"elementType", value.elementType},
            {"producerTag", value.producerTag ? nlohmann::json(*value.producerTag)
                                               : nlohmann::json(nullptr)},
            {"masterTag", value.masterTag ? nlohmann::json(*value.masterTag)
                                           : nlohmann::json(nullptr)},
            {"sourceTag", value.sourceTag ? nlohmann::json(*value.sourceTag)
                                           : nlohmann::json(nullptr)},
            {"operationPostfix", value.operationPostfix},
            {"rawMappedName", value.rawMappedName},
            {"canonicalMappedName", value.canonicalMappedName},
            {"elementIdRefs", stringIdsJson(value.elementIdRefs)},
            {"publicationScope", static_cast<int>(value.publicationScope)},
            {"status", static_cast<int>(value.status)},
        };
    }

    nlohmann::json childMaps = nlohmann::json::array();
    for (const NamedShapeChildMap& child : namedShape.childElementMaps) {
        childMaps.push_back({
            {"sourceOwner", child.sourceOwner},
            {"kind", child.kind},
            {"indexedName", child.indexedName},
            {"protocolPathPrefix", child.protocolPathPrefix},
            {"offset", child.offset},
            {"count", child.count},
            {"targetStart", child.targetStart},
            {"targetEnd", child.targetEnd},
            {"tag", child.tag},
            {"postfix", child.postfix},
            {"encodedChildMapKey", child.encodedChildMapKey},
            {"hasSourceElementMap", child.hasSourceElementMap},
            {"sourceElementMapSize", child.sourceElementMapSize},
            {"sourceChildMapCount", child.sourceChildMapCount},
            {"recursiveExpansion", child.recursiveExpansion},
        });
    }

    std::set<std::string> named;
    for (const auto& item : namedShape.elementMapEntries) {
        if (!item.second.empty()) {
            named.insert(item.first);
        }
    }
    nlohmann::json unnamed = nlohmann::json::array();
    for (const auto& item : namedShape.elements) {
        if (named.count(item.first) == 0U) {
            unnamed.push_back(item.first);
        }
    }

    nlohmann::json canonicalCollisions = nlohmann::json::array();
    for (const MapperHistoryEvent& event : namedShape.mapperHistory) {
        if (event.canonicalCollision) {
            canonicalCollisions.push_back(mapperHistoryEventToJson(event));
        }
    }

    nlohmann::json history = nlohmann::json::array();
    for (const ElementHistory& value : namedShape.history) {
        history.push_back({
            {"kind", static_cast<int>(value.kind)},
            {"element", value.element},
            {"sources", value.sources},
        });
    }
    return {
        {"owner", namedShape.owner},
        {"role", role},
        {"producerTag", namedShape.producerTag ? nlohmann::json(*namedShape.producerTag)
                                                : nlohmann::json(nullptr)},
        {"shape", inspectShapeInventory(namedShape.shape)},
        {"entries", std::move(entries)},
        {"reverseLookup", {{"lookupOnly", true}, {"entries", std::move(reverseLookup)}}},
        {"provenance", std::move(provenance)},
        {"childRanges", std::move(childMaps)},
        {"history", std::move(history)},
        {"mapperHistory", inspectMapperHistory(namedShape.mapperHistory)},
        {"elementHistoryStatus", namedShape.elementHistoryStatus},
        {"unnamedIndexedNames", std::move(unnamed)},
        {"canonicalCollisions", std::move(canonicalCollisions)},
    };
}

RawMakerHistoryCapture captureRawMakerHistory(const std::vector<NamedShapeSource>& sources,
                                              const TopoDS_Shape& output,
                                              BRepBuilderAPI_MakeShape& maker)
{
    RawMakerHistoryCapture capture;
    capture.output = output;
    for (std::size_t sourceOrdinal = 0; sourceOrdinal < sources.size(); ++sourceOrdinal) {
        capture.inputs.push_back({
            {"sourceOrdinal", sourceOrdinal},
            {"sourceTag",
             sources[sourceOrdinal].producerTag.value_or(
                 sources[sourceOrdinal].namedShape != nullptr
                     ? sources[sourceOrdinal].namedShape->producerTag.value_or(0L)
                     : 0L
             )},
            {"inventory", inspectShapeInventory(sources[sourceOrdinal].shape)},
        });
    }
    for (const TopAbs_ShapeEnum kind : kKinds) {
        for (std::size_t sourceOrdinal = 0; sourceOrdinal < sources.size(); ++sourceOrdinal) {
            const NamedShapeSource& source = sources[sourceOrdinal];
            TopTools_IndexedMapOfShape sourceElements;
            TopExp::MapShapes(source.shape, kind, sourceElements);
            for (int index = 1; index <= sourceElements.Extent(); ++index) {
                const TopoDS_Shape& sourceElement = sourceElements(index);
                RawMakerHistoryEntry entry;
                entry.sourceOrdinal = sourceOrdinal;
                entry.sourceOwner = source.owner;
                entry.sourceTag = source.producerTag.value_or(
                    source.namedShape != nullptr
                        ? source.namedShape->producerTag.value_or(0L)
                        : 0L
                );
                entry.sourceIndexed = traceShapeKindName(kind) + std::to_string(index);
                entry.sourceKind = kind;
                entry.sourceShape = sourceElement;
                const auto message = [](const Standard_Failure& failure) {
                    return failure.GetMessageString() != nullptr
                        ? std::string(failure.GetMessageString())
                        : std::string("OCCT mapper query failed");
                };
                try {
                    entry.modified = maker.Modified(sourceElement);
                }
                catch (const Standard_Failure& failure) {
                    entry.modifiedError = message(failure);
                }
                if (entry.modifiedError.empty()) {
                    try {
                        entry.generated = maker.Generated(sourceElement);
                    }
                    catch (const Standard_Failure& failure) {
                        entry.generatedError = message(failure);
                    }
                }
                try {
                    entry.deleted = maker.IsDeleted(sourceElement);
                }
                catch (const Standard_Failure& failure) {
                    entry.deletedError = message(failure);
                }
                capture.entries.push_back(std::move(entry));
            }
        }
    }
    return capture;
}

const RawMakerHistoryEntry* findRawMakerHistoryEntry(const RawMakerHistoryCapture& capture,
                                                     std::size_t sourceOrdinal,
                                                     TopAbs_ShapeEnum sourceKind,
                                                     int sourceIndex) noexcept
{
    const std::string indexed = traceShapeKindName(sourceKind) + std::to_string(sourceIndex);
    const auto found = std::find_if(
        capture.entries.begin(),
        capture.entries.end(),
        [&](const RawMakerHistoryEntry& entry) {
            return entry.sourceOrdinal == sourceOrdinal && entry.sourceKind == sourceKind
                && entry.sourceIndexed == indexed;
        }
    );
    return found == capture.entries.end() ? nullptr : &*found;
}

nlohmann::json inspectRawMakerMapper(const RawMakerHistoryCapture& capture)
{
    nlohmann::json sourceRows = nlohmann::json::array();
    for (const RawMakerHistoryEntry& entry : capture.entries) {
        sourceRows.push_back({
            {"sourceOrdinal", entry.sourceOrdinal},
            {"sourceTag", entry.sourceTag},
            {"sourceIndexed", entry.sourceIndexed},
            {"sourceShapeType", traceShapeKindName(entry.sourceKind)},
            {"modified", mapperTargets(entry.modified, capture.output, entry.sourceKind)},
            {"generated", mapperTargets(entry.generated, capture.output, entry.sourceKind)},
            {"deleted", entry.deleted},
            {"queryErrors",
             {{"modified", entry.modifiedError},
              {"generated", entry.generatedError},
              {"deleted", entry.deletedError}}},
        });
    }
    return {
        {"sources", std::move(sourceRows)},
        {"inputs", capture.inputs},
        {"output", inspectShapeInventory(capture.output)},
    };
}

namespace
{

std::string checkpointNamedShapeLedgerRecursive(
    const NamedShape& namedShape,
    const std::string& role,
    const std::string& label,
    const std::string& relation,
    const std::string& relatedIdentity,
    app::ElementMapProducerTrace& trace,
    const app::StringHasher& fallbackHasher,
    std::set<const NamedShape*>& active
)
{
    const app::StringHasher& hasher = namedShape.stringHasher
        ? *namedShape.stringHasher
        : fallbackHasher;
    const nlohmann::json table = hasher.inspectProducerTraceState();
    const std::string tableId = trace.checkpoint(
        {"stringTable",
         table,
         stringTableSidRefs(table),
         stringTableDefinedSids(table),
         {},
         "table_checkpoint"}
    );
    const std::string identity = trace.firstSeenIdentity(
        "ledger",
        role.empty() ? namedShape.owner : role,
        relation,
        relatedIdentity
    );
    const std::string mapIdentity = trace.firstSeenIdentity(
        "elementMap",
        role.empty() ? namedShape.owner : role,
        relation,
        relatedIdentity
    );
    const std::string shapeIdentity = trace.firstSeenIdentity(
        "shapeRole",
        role.empty() ? namedShape.owner : role,
        relation,
        relatedIdentity
    );
    nlohmann::json ledger = inspectNamedShapeLedger(namedShape, role);
    if (label == "element_map.write_checkpoint") {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::setElementName() checkpoints the ElementMap value being grown. Its
        // indexed inventory extends through the written IndexedName; it is not the owning
        // TopoShape's complete BRep inventory at this intermediate boundary.
        std::map<std::string, int> maximum {{"Vertex", 0}, {"Edge", 0}, {"Face", 0}};
        for (const auto& item : namedShape.elementMapEntries) {
            for (auto& [kind, ordinal] : maximum) {
                if (item.first.rfind(kind, 0U) != 0U) {
                    continue;
                }
                try {
                    ordinal = std::max(ordinal, std::stoi(item.first.substr(kind.size())));
                }
                catch (const std::exception&) {
                }
            }
        }
        nlohmann::json indexed = nlohmann::json::object();
        for (const auto& [kind, maximumOrdinal] : maximum) {
            indexed[kind] = nlohmann::json::array();
            for (int ordinal = 1; ordinal <= maximumOrdinal; ++ordinal) {
                indexed[kind].push_back(
                    {{"indexed", kind + std::to_string(ordinal)}, {"orientation", 0}}
                );
            }
        }
        ledger["shape"] = {{"isNull", false},
                           {"shapeType", "ElementMap"},
                           {"orientation", 0},
                           {"indexed", std::move(indexed)}};
    }
    ledger["identity"] = identity;
    ledger["elementMapIdentity"] = mapIdentity;
    ledger["shapeRoleIdentity"] = shapeIdentity;
    ledger["stringTableSnapshot"] = tableId;
    std::vector<std::string> nestedSnapshotRefs {tableId};
    const bool recursiveCycle = active.count(&namedShape) != 0U;
    if (!recursiveCycle) {
        active.insert(&namedShape);
    }
    auto& childRanges = ledger["childRanges"];
    std::map<const NamedShape*, std::string> publishedChildLedgers;
    for (std::size_t index = 0; index < namedShape.childElementMaps.size(); ++index) {
        const NamedShapeChildMap& child = namedShape.childElementMaps[index];
        const NamedShape* source = child.sourceLedger ? child.sourceLedger.get()
                                                      : child.sourceNamedShape;
        nlohmann::json& childPayload = childRanges.at(index);
        if (source == nullptr) {
            childPayload["nestedSnapshot"] = nullptr;
            childPayload["nestedSnapshotStatus"] = "source_ledger_missing";
            childPayload["sourceElementIdRefs"] = nlohmann::json::array();
            continue;
        }
        childPayload["sourceElementIdRefs"] = sidRefsJson(ledgerSidRefs(*source));
        if (recursiveCycle) {
            childPayload["nestedSnapshot"] = nullptr;
            childPayload["nestedSnapshotStatus"] = "cycle_truncated";
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::addChildElements() stores `child.elementMap` by shared pointer. Vertex,
        // Edge and Face ranges from the same child therefore reference one nested ElementMap,
        // rather than publishing three independent ledger instances.
        auto nested = publishedChildLedgers.find(source);
        if (nested == publishedChildLedgers.end()) {
            const std::string nestedRole = role + "/child:" + child.sourceOwner + ":"
                + child.kind + ":" + std::to_string(index + 1U);
            const std::string nestedId = checkpointNamedShapeLedgerRecursive(
                *source,
                nestedRole,
                "child_map.nested_checkpoint",
                "share",
                identity,
                trace,
                hasher,
                active
            );
            nested = publishedChildLedgers.emplace(source, nestedId).first;
        }
        const std::string& nestedId = nested->second;
        childPayload["nestedSnapshot"] = nestedId;
        childPayload["nestedSnapshotStatus"] = "published";
        nestedSnapshotRefs.push_back(nestedId);
    }
    if (!recursiveCycle) {
        active.erase(&namedShape);
    }
    return trace.checkpoint(
        {"ledger",
         ledger,
         ledgerSidRefs(namedShape),
         {},
         std::move(nestedSnapshotRefs),
         label}
    );
}

}  // namespace

std::string checkpointNamedShapeLedger(const NamedShape& namedShape,
                                       const std::string& role,
                                       const std::string& label,
                                       const std::string& relation,
                                       const std::string& relatedIdentity)
{
    if (!namedShape.stringHasher || namedShape.stringHasher->producerTrace() == nullptr) {
        return {};
    }
    std::set<const NamedShape*> active;
    return checkpointNamedShapeLedgerRecursive(
        namedShape,
        role,
        label,
        relation,
        relatedIdentity,
        *namedShape.stringHasher->producerTrace(),
        *namedShape.stringHasher,
        active
    );
}

}  // namespace cad_core::part
