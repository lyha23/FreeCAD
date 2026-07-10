#include "cad_core/sketcher/sketch_edge_identity.h"

#include "cad_core/topo/freecad_mapped_name_codec.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <set>
#include <utility>

namespace cad_core::sketcher
{

namespace
{

std::string sourceStableSubname(const SketchGeometryIdentity& identity)
{
    if (identity.geometryId) {
        return stableSubnameForGeometryId(*identity.geometryId);
    }
    return {};
}

std::string identityStatus(const SketchGeometryIdentity& identity)
{
    return identity.geometryId ? "stable" : "";
}

std::string stableSubnameForIdentity(const RawSketchEdgeIdentity& identity)
{
    if (identity.stableSubname) {
        return *identity.stableSubname;
    }
    if (identity.source.geometryId) {
        return stableSubnameForGeometryId(*identity.source.geometryId);
    }
    return {};
}

std::optional<int> indexedOrdinal(const std::string& indexed, const std::string& prefix)
{
    if (indexed.rfind(prefix, 0U) != 0U || indexed.size() == prefix.size()) {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t index = prefix.size(); index < indexed.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(indexed.at(index));
        if (std::isdigit(ch) == 0) {
            return std::nullopt;
        }
        value = value * 10 + static_cast<int>(ch - static_cast<unsigned char>('0'));
    }
    return value > 0 ? std::optional<int> {value} : std::nullopt;
}

std::string sketchMappedElementType(const std::string& indexed)
{
    if (indexed.rfind("Edge", 0U) == 0U) {
        return "Edge";
    }
    if (indexed.rfind("Vertex", 0U) == 0U) {
        return "Vertex";
    }
    return {};
}

std::string rawSketchMappedName(const std::string& token,
                                long ownerTag,
                                const std::string& elementType)
{
    if (token.empty() || elementType.empty()) {
        return {};
    }
    std::ostringstream raw;
    raw << token << ";SKT;:H" << std::hex << ownerTag << ',' << elementType.front();
    return raw.str();
}

void recordSketchMappedName(part::NamedShape& namedShape,
                            const std::string& token,
                            const std::string& indexed,
                            long ownerTag)
{
    const std::string elementType = sketchMappedElementType(indexed);
    if (token.empty() || elementType.empty() || namedShape.elements.count(indexed) == 0U) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildShape() calls getEdge(..., convertSubName(...)) and then
    // makeElementWires(shapes, Part::OpCodes::Sketch); ::convertSubName() emits g<ID> and
    // g<ID>v<point>.  Record that producer-side SKT name before a Part compound consumes it.
    part::MappedNameProvenance provenance;
    provenance.entryKey = token;
    provenance.currentElement = indexed;
    provenance.sourceElement = token;
    provenance.elementType = elementType;
    provenance.producerTag = ownerTag;
    provenance.masterTag = ownerTag;
    provenance.sourceTag = ownerTag;
    provenance.operationPostfix = ";SKT";
    provenance.rawMappedName = rawSketchMappedName(token, ownerTag, elementType);
    provenance.canonicalMappedName = topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
    provenance.status = part::MappedNameProvenanceStatus::SourceBacked;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildShape() hands this raw g<ID>;SKT ledger to Part makers.  It is not
    // itself a direct-Sketch response identity; record that boundary at the producer so a
    // downstream Part map can explicitly promote the consumed evidence.
    provenance.publicationScope = part::MappedNamePublicationScope::ProducerOnly;
    namedShape.elementMap[token] = indexed;
    namedShape.mappedNameProvenance[token] = std::move(provenance);
}

std::string identityStatusForEntry(const RawSketchEdgeIdentity& identity)
{
    if (!identity.explicitIdentityStatus.empty()) {
        return identity.explicitIdentityStatus;
    }
    return identityStatus(identity.source);
}

nlohmann::json edgeIdentityJson(const RawSketchEdgeIdentity& identity)
{
    nlohmann::json value {
        {"indexed", identity.indexed},
        {"sourceGeometryIndex", identity.source.geometryIndex},
    };
    const std::string sourceStable = sourceStableSubname(identity.source);
    if (!sourceStable.empty()) {
        value["sourceStableSubname"] = sourceStable;
    }
    const std::string status = identityStatusForEntry(identity);
    if (!status.empty()) {
        value["identityStatus"] = status;
    }
    const std::string stableSubname = stableSubnameForIdentity(identity);
    if (!stableSubname.empty()) {
        value["stableSubname"] = stableSubname;
    }
    if (identity.fragmentStableSubname) {
        value["fragmentStableSubname"] = *identity.fragmentStableSubname;
    }
    if (!identity.sourceIndexed.empty()) {
        value["sourceIndexed"] = identity.sourceIndexed;
    }
    if (identity.source.geometryId) {
        value["sourceGeometryId"] = *identity.source.geometryId;
    }
    if (!identity.source.geometryKind.empty()) {
        value["sourceGeometryKind"] = identity.source.geometryKind;
    }
    return value;
}

void applyIdentityFields(nlohmann::json& value, const RawSketchEdgeIdentity& identity)
{
    value["sourceGeometryIndex"] = identity.source.geometryIndex;
    const std::string sourceStable = sourceStableSubname(identity.source);
    if (!sourceStable.empty()) {
        value["sourceStableSubname"] = sourceStable;
    }
    const std::string status = identityStatusForEntry(identity);
    if (!status.empty()) {
        value["identityStatus"] = status;
    }
    const std::string stableSubname = stableSubnameForIdentity(identity);
    if (!stableSubname.empty()) {
        value["stableSubname"] = stableSubname;
    }
    if (identity.fragmentStableSubname) {
        value["fragmentStableSubname"] = *identity.fragmentStableSubname;
    }
    if (!identity.sourceIndexed.empty()) {
        value["sourceIndexed"] = identity.sourceIndexed;
    }
    if (identity.source.geometryId) {
        value["sourceGeometryId"] = *identity.source.geometryId;
    }
    if (!identity.source.geometryKind.empty()) {
        value["sourceGeometryKind"] = identity.source.geometryKind;
    }
}

std::optional<std::size_t> sourceIndexForRawEdge(
    const TopoDS_Edge& rawEdge,
    const std::vector<TopoDS_Edge>& sourceEdges,
    std::vector<bool>& usedSources)
{
    for (std::size_t index = 0; index < sourceEdges.size(); ++index) {
        if (usedSources[index]) {
            continue;
        }
        if (rawEdge.IsSame(sourceEdges[index])) {
            usedSources[index] = true;
            return index;
        }
    }
    return std::nullopt;
}

bool hasTopologicalPrefixAndDigits(const std::string& value, const std::string& prefix)
{
    if (value.rfind(prefix, 0) != 0 || value.size() == prefix.size()) {
        return false;
    }
    return std::all_of(value.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                       value.end(),
                       [](unsigned char item) { return std::isdigit(item) != 0; });
}

bool isRawEdgeName(const std::string& value)
{
    return hasTopologicalPrefixAndDigits(value, "Edge");
}

bool isInternalEdgeName(const std::string& value)
{
    return hasTopologicalPrefixAndDigits(value, "InternalEdge");
}

std::vector<RawSketchEdgeIdentity>::iterator findLedgerIdentity(
    RawSketchEdgeIdentityLedger& ledger,
    const std::string& indexed)
{
    return std::find_if(
        ledger.edges.begin(),
        ledger.edges.end(),
        [&indexed](const RawSketchEdgeIdentity& identity) {
            return identity.indexed == indexed;
        });
}

std::vector<RawSketchEdgeIdentity>::const_iterator findLedgerIdentity(
    const RawSketchEdgeIdentityLedger& ledger,
    const std::string& indexed)
{
    return std::find_if(
        ledger.edges.begin(),
        ledger.edges.end(),
        [&indexed](const RawSketchEdgeIdentity& identity) {
            return identity.indexed == indexed;
        });
}

}  // namespace

SketchGeometryIdentity sketchGeometryIdentity(std::size_t geometryIndex,
                                              std::optional<long> geometryId,
                                              std::string geometryKind)
{
    return SketchGeometryIdentity {geometryIndex, geometryId, std::move(geometryKind)};
}

std::string stableSubnameForGeometryId(long geometryId)
{
    return "g" + std::to_string(geometryId);
}

RawSketchEdgeIdentityLedger buildRawSketchEdgeIdentityLedger(
    const TopoDS_Shape& rawShape,
    const std::vector<TopoDS_Edge>& sourceEdges,
    const std::vector<SketchGeometryIdentity>& sourceIdentities,
    bool sourceOrderMatchesPublishedShape)
{
    RawSketchEdgeIdentityLedger ledger;
    if (rawShape.IsNull()) {
        return ledger;
    }

    TopTools_IndexedMapOfShape rawEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    std::vector<bool> usedSources(sourceEdges.size(), false);
    ledger.edges.reserve(static_cast<std::size_t>(rawEdges.Extent()));

    for (int index = 1; index <= rawEdges.Extent(); ++index) {
        const TopoDS_Edge rawEdge = TopoDS::Edge(rawEdges(index));
        std::optional<std::size_t> sourceIndex =
            sourceIndexForRawEdge(rawEdge, sourceEdges, usedSources);
        if (!sourceIndex && sourceOrderMatchesPublishedShape) {
            const std::size_t orderIndex = static_cast<std::size_t>(index - 1);
            if (orderIndex < sourceEdges.size() && orderIndex < sourceIdentities.size()) {
                sourceIndex = orderIndex;
                if (orderIndex < usedSources.size()) {
                    usedSources[orderIndex] = true;
                }
            }
        }

        SketchGeometryIdentity sourceIdentity {
            static_cast<std::size_t>(index - 1),
            std::nullopt,
            {},
        };
        if (sourceIndex && *sourceIndex < sourceIdentities.size()) {
            sourceIdentity = sourceIdentities[*sourceIndex];
        }

        ledger.edges.push_back(
            RawSketchEdgeIdentity {"Edge" + std::to_string(index), sourceIdentity});
        if (sourceIdentity.geometryId) {
            ++ledger.stableCount;
        }
        else {
            ++ledger.unresolvedCount;
        }
    }

    return ledger;
}

void addSplitFragmentIdentitiesFromInternalHistory(
    RawSketchEdgeIdentityLedger& ledger,
    const part::NamedShape& internalNamedShape)
{
    std::map<std::string, RawSketchEdgeIdentity> sourcesByIndexed;
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        if (identity.source.geometryId) {
            sourcesByIndexed[identity.indexed] = identity;
        }
    }

    std::map<std::string, std::vector<std::string>> candidateTargetsBySource;
    std::map<std::string, std::set<std::string>> candidateSourcesByTarget;
    for (const part::ElementHistory& history : internalNamedShape.history) {
        if (history.kind != part::ElementHistoryKind::Split || history.sources.size() != 1U) {
            continue;
        }
        const std::string& source = history.sources.front();
        if (sourcesByIndexed.find(source) == sourcesByIndexed.end()) {
            continue;
        }
        if (internalNamedShape.elements.find(history.element) == internalNamedShape.elements.end()) {
            continue;
        }
        std::vector<std::string>& targets = candidateTargetsBySource[source];
        if (std::find(targets.begin(), targets.end(), history.element) == targets.end()) {
            targets.push_back(history.element);
        }
        candidateSourcesByTarget[history.element].insert(source);
    }

    for (const auto& [source, targets] : candidateTargetsBySource) {
        if (targets.size() <= 1U) {
            continue;
        }
        const RawSketchEdgeIdentity sourceIdentity = sourcesByIndexed.at(source);
        std::size_t fragmentIndex = 0;
        for (const std::string& target : targets) {
            const auto targetSourcesIt = candidateSourcesByTarget.find(target);
            if (targetSourcesIt == candidateSourcesByTarget.end()
                || targetSourcesIt->second.size() != 1U) {
                continue;
            }
            if (std::any_of(
                    ledger.edges.begin(),
                    ledger.edges.end(),
                    [&target](const RawSketchEdgeIdentity& identity) {
                        return identity.indexed == target;
                    })) {
                continue;
            }
            ++fragmentIndex;
            const std::string fragmentStableSubname =
                stableSubnameForGeometryId(*sourceIdentity.source.geometryId) + ":split"
                + std::to_string(fragmentIndex);
            RawSketchEdgeIdentity fragment {
                target,
                sourceIdentity.source,
                source,
                fragmentStableSubname,
                fragmentStableSubname,
                "stable_split_fragment",
            };
            ledger.edges.push_back(std::move(fragment));
            ++ledger.stableCount;
            ++ledger.splitFragmentCount;
        }
    }
}

void addInternalEdgeIdentitiesFromInternalElementMap(
    RawSketchEdgeIdentityLedger& ledger,
    const nlohmann::json& internalElementMap)
{
    if (!internalElementMap.is_object()) {
        return;
    }

    for (const auto& [internalIndexed, mapped] : internalElementMap.items()) {
        if (!isInternalEdgeName(internalIndexed) || !mapped.is_string()) {
            continue;
        }
        const std::string rawIndexed = mapped.get<std::string>();
        if (!isRawEdgeName(rawIndexed)) {
            continue;
        }
        if (findLedgerIdentity(ledger, internalIndexed) != ledger.edges.end()) {
            continue;
        }

        const RawSketchEdgeIdentityLedger& sourceLedger = ledger;
        const auto sourceIt = findLedgerIdentity(sourceLedger, rawIndexed);
        if (sourceIt == ledger.edges.cend()) {
            continue;
        }
        if (!sourceIt->source.geometryId) {
            continue;
        }

        RawSketchEdgeIdentity internalIdentity = *sourceIt;
        internalIdentity.indexed = internalIndexed;
        internalIdentity.sourceIndexed = rawIndexed;
        ledger.edges.push_back(std::move(internalIdentity));
        ++ledger.stableCount;
    }
}

part::NamedShape namedShapeForSketchRawEdgeIdentity(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const RawSketchEdgeIdentityLedger& ledger)
{
    part::NamedShape namedShape = part::indexedNamedShapeForObject(owner, rawShape);
    const std::optional<long> producerTag = part::requestLocalProducerTagForShape(rawShape);
    if (!producerTag) {
        return namedShape;
    }
    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape rawVertices;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    std::set<int> mappedVertices;

    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        if (!identity.source.geometryId || namedShape.elements.count(identity.indexed) == 0U) {
            continue;
        }
        const std::string geometryToken = stableSubnameForGeometryId(*identity.source.geometryId);
        recordSketchMappedName(namedShape, geometryToken, identity.indexed, *producerTag);

        const auto edgeIndex = indexedOrdinal(identity.indexed, "Edge");
        if (!edgeIndex || *edgeIndex > rawEdges.Extent()) {
            continue;
        }
        TopoDS_Vertex first;
        TopoDS_Vertex last;
        TopExp::Vertices(TopoDS::Edge(rawEdges(*edgeIndex)), first, last);
        const auto recordEndpoint = [&](const TopoDS_Vertex& vertex, int endpoint) {
            if (vertex.IsNull()) {
                return;
            }
            const int vertexIndex = rawVertices.FindIndex(vertex);
            if (vertexIndex <= 0 || !mappedVertices.insert(vertexIndex).second) {
                return;
            }
            recordSketchMappedName(
                namedShape,
                geometryToken + "v" + std::to_string(endpoint),
                "Vertex" + std::to_string(vertexIndex),
                *producerTag
            );
        };
        recordEndpoint(first, 1);
        recordEndpoint(last, 2);
    }
    return namedShape;
}

void publishRawSketchEdgeIdentity(nlohmann::json& mesh,
                                  nlohmann::json& subshapes,
                                  const RawSketchEdgeIdentityLedger& ledger)
{
    if (mesh.is_object()) {
        auto edgeSegmentsIt = mesh.find("edgeSegments");
        if (edgeSegmentsIt != mesh.end() && edgeSegmentsIt->is_array()) {
            for (nlohmann::json& segment : *edgeSegmentsIt) {
                if (!segment.is_object()) {
                    continue;
                }
                const std::string indexed = segment.value("indexed", segment.value("id", ""));
                const auto identityIt = std::find_if(
                    ledger.edges.begin(),
                    ledger.edges.end(),
                    [&indexed](const RawSketchEdgeIdentity& identity) {
                        return identity.indexed == indexed;
                    });
                if (identityIt != ledger.edges.end()) {
                    applyIdentityFields(segment, *identityIt);
                }
            }
        }
    }

    if (!subshapes.is_object()) {
        return;
    }
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        auto subshapeIt = subshapes.find(identity.indexed);
        if (subshapeIt == subshapes.end() || !subshapeIt->is_object()) {
            continue;
        }
        applyIdentityFields(*subshapeIt, identity);
    }
}

nlohmann::json rawSketchEdgeIdentityObject(const RawSketchEdgeIdentityLedger& ledger)
{
    nlohmann::json byIndexed = nlohmann::json::object();
    nlohmann::json byStableSubname = nlohmann::json::object();
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        byIndexed[identity.indexed] = edgeIdentityJson(identity);
        const std::string stableSubname = stableSubnameForIdentity(identity);
        if (!stableSubname.empty()) {
            byStableSubname[stableSubname] = identity.indexed;
        }
    }

    return {
        {"byIndexed", byIndexed},
        {"byStableSubname", byStableSubname},
        {"stable_count", ledger.stableCount},
        {"unresolved_identity_count", ledger.unresolvedCount},
        {"split_fragment_count", ledger.splitFragmentCount},
    };
}

}  // namespace cad_core::sketcher
