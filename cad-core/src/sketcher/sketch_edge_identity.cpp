#include "cad_core/sketcher/sketch_edge_identity.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cctype>
#include <map>
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
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        if (!identity.source.geometryId || namedShape.elements.count(identity.indexed) == 0U) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::convertSubName(), for normal geometry writes
        // "'g' << GeometryFacade::getFacade(geo)->getId()" instead of preserving EdgeN.
        namedShape.elementMap[stableSubnameForGeometryId(*identity.source.geometryId)] =
            identity.indexed;
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
