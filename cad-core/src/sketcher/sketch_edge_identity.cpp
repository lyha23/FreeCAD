#include "cad_core/sketcher/sketch_edge_identity.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <utility>

namespace cad_core::sketcher
{

namespace
{

std::string fallbackStableSubname(const SketchGeometryIdentity& identity)
{
    return "index:" + std::to_string(identity.geometryIndex);
}

std::string sourceStableSubname(const SketchGeometryIdentity& identity)
{
    if (identity.geometryId) {
        return stableSubnameForGeometryId(*identity.geometryId);
    }
    return fallbackStableSubname(identity);
}

std::string identityStatus(const SketchGeometryIdentity& identity)
{
    return identity.geometryId ? "stable" : "index_fallback";
}

nlohmann::json edgeIdentityJson(const RawSketchEdgeIdentity& identity)
{
    nlohmann::json value {
        {"indexed", identity.indexed},
        {"sourceGeometryIndex", identity.source.geometryIndex},
        {"sourceStableSubname", sourceStableSubname(identity.source)},
        {"identityStatus", identityStatus(identity.source)},
    };
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
    value["sourceStableSubname"] = sourceStableSubname(identity.source);
    value["sourceGeometryIndex"] = identity.source.geometryIndex;
    value["identityStatus"] = identityStatus(identity.source);
    if (identity.source.geometryId) {
        const std::string stable = stableSubnameForGeometryId(*identity.source.geometryId);
        value["sourceGeometryId"] = *identity.source.geometryId;
        value["stableSubname"] = stable;
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
    const std::vector<SketchGeometryIdentity>& sourceIdentities)
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
        if (!sourceIndex) {
            const std::size_t orderIndex = static_cast<std::size_t>(index - 1);
            if (orderIndex < sourceIdentities.size()) {
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
            ++ledger.fallbackCount;
        }
    }

    return ledger;
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
        if (identity.source.geometryId) {
            byStableSubname[stableSubnameForGeometryId(*identity.source.geometryId)] =
                identity.indexed;
        }
    }

    return {
        {"byIndexed", byIndexed},
        {"byStableSubname", byStableSubname},
        {"stable_count", ledger.stableCount},
        {"index_fallback_count", ledger.fallbackCount},
    };
}

}  // namespace cad_core::sketcher
