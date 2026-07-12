#include "cad_core/sketcher/sketch_edge_identity.h"

#include "cad_core/app/string_hasher.h"
#include "cad_core/topo/freecad_mapped_name_codec.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <array>
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

std::optional<int> sketchEndpointOrdinal(const std::string& token)
{
    const std::size_t marker = token.rfind('v');
    if (marker == std::string::npos || marker + 1U == token.size()) {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t index = marker + 1U; index < token.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(token.at(index));
        if (std::isdigit(ch) == 0) {
            return std::nullopt;
        }
        value = value * 10 + static_cast<int>(ch - static_cast<unsigned char>('0'));
    }
    return value > 0 ? std::optional<int> {value} : std::nullopt;
}

bool isGeometryToken(const std::string& token)
{
    return token.size() > 1U && token.front() == 'g'
        && std::all_of(token.begin() + 1, token.end(), [](unsigned char ch) {
               return std::isdigit(ch) != 0;
           });
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

std::string rawSketchMappedName(const std::string& token, const std::string& elementType)
{
    if (token.empty() || elementType.empty()) {
        return {};
    }
    // SketchObject::buildShape() records g<ID>/g<ID>v<point> with the SKT operation. The
    // receiving Part maker appends its terminal tag while it calls ElementMap::encodeElementName;
    // putting that tag on the source ledger would change StringHasher's postfix and ID sequence.
    return token + ";SKT";
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
    provenance.rawMappedName = rawSketchMappedName(token, elementType);
    provenance.canonicalMappedName = topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
    provenance.status = part::MappedNameProvenanceStatus::SourceBacked;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildShape() hands this raw g<ID>;SKT ledger to Part makers.  It is not
    // itself a direct-Sketch response identity; record that boundary at the producer so a
    // downstream Part map can explicitly promote the consumed evidence.
    provenance.publicationScope = part::MappedNamePublicationScope::ProducerOnly;
    namedShape.elementMap[token] = indexed;
    namedShape.mappedNameProvenance[token] = std::move(provenance);
    part::recordElementMapEntry(namedShape, token, indexed);
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
    const RawSketchEdgeIdentityLedger& ledger,
    long ownerTag)
{
    part::NamedShape namedShape = part::indexedNamedShapeForObject(owner, rawShape);
    if (ownerTag == 0) {
        return namedShape;
    }
    // FreeCAD: SketchObject::buildShape() is stored through PropertyPartShape, whose
    // TopoShape Tag is the owning DocumentObject::getID().  This is document identity, not a
    // geometry-derived surrogate; downstream makeShapeWithElementMap() uses it in NameKey.
    namedShape.producerTag = ownerTag;
    TopTools_IndexedMapOfShape rawEdges;
    TopTools_IndexedMapOfShape rawVertices;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    TopExp::MapShapes(rawShape, TopAbs_VERTEX, rawVertices);
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        if (!identity.source.geometryId || namedShape.elements.count(identity.indexed) == 0U) {
            continue;
        }
        const std::string geometryToken = stableSubnameForGeometryId(*identity.source.geometryId);
        recordSketchMappedName(namedShape, geometryToken, identity.indexed, ownerTag);

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
            if (vertexIndex <= 0) {
                return;
            }
            recordSketchMappedName(
                namedShape,
                geometryToken + "v" + std::to_string(endpoint),
                "Vertex" + std::to_string(vertexIndex),
                ownerTag
            );
        };
        recordEndpoint(first, 1);
        recordEndpoint(last, 2);
    }
    return namedShape;
}

void materializeSketchMappedNameStringId(
    part::NamedShape& namedShape,
    const std::string& entryKey,
    const std::shared_ptr<app::StringHasher>& stringHasher
)
{
    if (!stringHasher) {
        return;
    }
    namedShape.stringHasher = stringHasher;
    auto provenance = namedShape.mappedNameProvenance.find(entryKey);
    if (provenance == namedShape.mappedNameProvenance.end()
        || provenance->second.status != part::MappedNameProvenanceStatus::SourceBacked
        || provenance->second.operationPostfix != ";SKT"
        || !provenance->second.elementIdRefs.empty()) {
        return;
    }
    const std::string& raw = provenance->second.rawMappedName;
    const std::size_t postfix = raw.find(';');
    if (postfix == std::string::npos || postfix == 0U) {
        return;
    }
    const bool alreadyMaterialized = stringHasher->mappedNameId(raw).has_value();
    const std::string token = raw.substr(0U, postfix);
    std::string data = token;
    int mappedIndex = 0;
    int displayedIndex = 0;
    if (isGeometryToken(token)) {
        mappedIndex = std::stoi(token.substr(1U));
        data = "g";
    }
    else if (const auto endpoint = sketchEndpointOrdinal(token)) {
        displayedIndex = *endpoint;
    }
    app::StringId sid = stringHasher->getMappedNameId(
        data,
        displayedIndex != 0 && alreadyMaterialized
            ? 0
            : (displayedIndex != 0 ? displayedIndex : mappedIndex),
        raw.substr(postfix)
    );
    if (displayedIndex != 0 && !alreadyMaterialized) {
        sid.index = displayedIndex;
    }
    app::StringId elementRef = sid;
    if (displayedIndex != 0 && !alreadyMaterialized) {
        elementRef.index = displayedIndex;
    }
    provenance->second.elementIdRefs = {elementRef};
    stringHasher->rememberMappedName(raw, sid, {elementRef});
}

void materializeSketchMappedNameStringIds(
    part::NamedShape& namedShape,
    const std::shared_ptr<app::StringHasher>& stringHasher
)
{
    if (!stringHasher || namedShape.shape.IsNull()) {
        return;
    }
    namedShape.stringHasher = stringHasher;

    // FreeCAD TopoShape::mapSubElement() follows Vertex -> Edge -> Face and ElementMap::findAll()
    // returns aliases in the child map's stored order. Keep that order while internally materializing
    // the source g<ID>;SKT IDs; the public provenance below remains the raw Sketch ledger.
    const std::array<std::pair<TopAbs_ShapeEnum, const char*>, 2> kinds {
        std::pair {TopAbs_VERTEX, "Vertex"},
        std::pair {TopAbs_EDGE, "Edge"},
    };
    for (const auto& [kind, prefix] : kinds) {
        TopTools_IndexedMapOfShape elements;
        TopExp::MapShapes(namedShape.shape, kind, elements);
        for (int index = 1; index <= elements.Extent(); ++index) {
            const std::string indexed = std::string(prefix) + std::to_string(index);
            for (const auto& [entryKey, current] : namedShape.elementMap) {
                if (current != indexed) {
                    continue;
                }
                const auto provenance = namedShape.mappedNameProvenance.find(entryKey);
                if (provenance == namedShape.mappedNameProvenance.end()
                    || provenance->second.status != part::MappedNameProvenanceStatus::SourceBacked
                    || provenance->second.operationPostfix != ";SKT") {
                    continue;
                }
                const std::string& raw = provenance->second.rawMappedName;
                const std::size_t postfix = raw.find(';');
                if (postfix == std::string::npos || postfix == 0U) {
                    continue;
                }
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/StringHasher.cpp
                // ::StringHasher::getID() owns a document-wide StringID table, but
                // ElementMap.cpp::hashElementName() carries ElementIDRefs on the *producer
                // map*.  Reusing a raw Sketch token in a later Sketch may reuse its StringID
                // without giving the later ElementMap the first producer's endpoint ref.  The
                // raw g<ID>;SKT ledger stays identical; only a first materialization creates
                // the local g<ID>v<point> StringIDRef index.
                const bool alreadyMaterialized = stringHasher->mappedNameId(raw).has_value();
                const std::string token = raw.substr(0U, postfix);
                std::string data = token;
                int mappedIndex = 0;
                int displayedIndex = 0;
                if (isGeometryToken(token)) {
                    mappedIndex = std::stoi(token.substr(1U));
                    data = "g";
                }
                else if (const auto endpoint = sketchEndpointOrdinal(token)) {
                    displayedIndex = *endpoint;
                }
                app::StringId sid = stringHasher->getMappedNameId(
                    data,
                    displayedIndex != 0 ? displayedIndex : mappedIndex,
                    raw.substr(postfix)
                );
                if (displayedIndex != 0 && !alreadyMaterialized) {
                    sid.index = displayedIndex;
                }
                app::StringId elementRef = sid;
                if (displayedIndex != 0) {
                    elementRef.index = displayedIndex;
                }
                // Keep this producer's refs on its own entry. A later Sketch may reuse the
                // same StringID but must not inherit this endpoint ref from a global raw-name
                // lookup.
                provenance->second.elementIdRefs = {elementRef};
                stringHasher->rememberMappedName(raw, sid, {elementRef});
            }
        }
    }
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
