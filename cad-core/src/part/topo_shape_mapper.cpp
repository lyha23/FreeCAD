#include "cad_core/part/topo_shape_mapper.h"

#include <algorithm>
#include <utility>

namespace cad_core::part
{

std::string mapperHistoryRelationName(MapperHistoryRelation relation)
{
    switch (relation) {
        case MapperHistoryRelation::Identity:
            return "identity";
        case MapperHistoryRelation::Preserved:
            return "preserved";
        case MapperHistoryRelation::Generated:
            return "generated";
        case MapperHistoryRelation::Modified:
            return "modified";
        case MapperHistoryRelation::Split:
            return "split";
        case MapperHistoryRelation::Merge:
            return "merge";
        case MapperHistoryRelation::Deleted:
            return "deleted";
        case MapperHistoryRelation::Ambiguous:
            return "ambiguous";
    }
    return "unknown";
}

std::string mapperHistoryRecoverabilityName(MapperHistoryRecoverability recoverability)
{
    switch (recoverability) {
        case MapperHistoryRecoverability::Resolved:
            return "resolved";
        case MapperHistoryRecoverability::Recoverable:
            return "recoverable";
        case MapperHistoryRecoverability::NeedsReselect:
            return "needs_reselect";
        case MapperHistoryRecoverability::Deleted:
            return "deleted";
        case MapperHistoryRecoverability::Ambiguous:
            return "ambiguous";
        case MapperHistoryRecoverability::Diagnostic:
            return "diagnostic";
        case MapperHistoryRecoverability::Unknown:
            return "unknown";
    }
    return "unknown";
}

namespace
{

nlohmann::json endpointToJson(const MapperHistoryEndpoint& endpoint)
{
    return {
        {"object", endpoint.object},
        {"subname", endpoint.subname},
    };
}

bool sameMapperHistoryEvent(const MapperHistoryEvent& left, const MapperHistoryEvent& right)
{
    const auto sameCollision = [&] {
        if (left.canonicalCollision.has_value() != right.canonicalCollision.has_value()) {
            return false;
        }
        if (!left.canonicalCollision) {
            return true;
        }
        const MapperHistoryCanonicalCollision& leftCollision = *left.canonicalCollision;
        const MapperHistoryCanonicalCollision& rightCollision = *right.canonicalCollision;
        if (leftCollision.context != rightCollision.context
            || leftCollision.rawMappedName != rightCollision.rawMappedName
            || leftCollision.canonicalMappedName != rightCollision.canonicalMappedName
            || leftCollision.candidates.size() != rightCollision.candidates.size()) {
            return false;
        }
        for (std::size_t index = 0; index < leftCollision.candidates.size(); ++index) {
            const MapperHistoryCollisionCandidate& leftCandidate = leftCollision.candidates.at(index);
            const MapperHistoryCollisionCandidate& rightCandidate = rightCollision.candidates.at(index);
            if (leftCandidate.source.object != rightCandidate.source.object
                || leftCandidate.source.subname != rightCandidate.source.subname
                || leftCandidate.target.object != rightCandidate.target.object
                || leftCandidate.target.subname != rightCandidate.target.subname
                || leftCandidate.shapeKind != rightCandidate.shapeKind
                || leftCandidate.rawMappedName != rightCandidate.rawMappedName
                || leftCandidate.canonicalMappedName != rightCandidate.canonicalMappedName
                || leftCandidate.recoverability != rightCandidate.recoverability) {
                return false;
            }
        }
        return true;
    };
    return left.id == right.id
        && left.source.object == right.source.object && left.source.subname == right.source.subname
        && left.target.object == right.target.object && left.target.subname == right.target.subname
        && left.shapeKind == right.shapeKind && left.relation == right.relation
        && left.makerStage == right.makerStage && left.recoverability == right.recoverability
        && left.diagnosticStatus == right.diagnosticStatus
        && left.evidence.dump() == right.evidence.dump() && sameCollision();
}

}  // namespace

nlohmann::json mapperHistoryEventToJson(const MapperHistoryEvent& event)
{
    if (event.canonicalCollision) {
        const MapperHistoryCanonicalCollision& collision = *event.canonicalCollision;
        nlohmann::json candidates = nlohmann::json::array();
        for (const MapperHistoryCollisionCandidate& candidate : collision.candidates) {
            candidates.push_back({
                {"target", endpointToJson(candidate.target)},
                {"shapeKind", candidate.shapeKind},
                {"source", endpointToJson(candidate.source)},
                {"mappedName",
                 {
                     {"raw", candidate.rawMappedName},
                     {"canonical", candidate.canonicalMappedName},
                 }},
                {"recoverability", mapperHistoryRecoverabilityName(candidate.recoverability)},
            });
        }
        return {
            {"id", event.id},
            {"relation", "ambiguous"},
            {"recoverability", "ambiguous"},
            // This describes the CAD Core Part ledger that detected the collision.  It is not a
            // fabricated FreeCAD collector provenance and carries no geometry identity itself.
            {"source", "part_element_map"},
            {"mappedName",
             {
                 {"raw", collision.rawMappedName},
                 {"canonical", collision.canonicalMappedName},
             }},
            {"candidates", std::move(candidates)},
            {"message",
             "Canonical elementMap key " + collision.canonicalMappedName
                 + " maps to multiple current targets; Part keeps it out of ElementMap"},
        };
    }
    nlohmann::json result = {
        {"source", endpointToJson(event.source)},
        {"target", endpointToJson(event.target)},
        {"shape_kind", event.shapeKind},
        {"relation", mapperHistoryRelationName(event.relation)},
        {"maker_stage", event.makerStage},
        {"evidence", event.evidence.is_null() ? nlohmann::json::object() : event.evidence},
        {"recoverability", mapperHistoryRecoverabilityName(event.recoverability)},
        {"diagnostic_status", event.diagnosticStatus},
    };
    if (!event.id.empty()) {
        result["id"] = event.id;
    }
    return result;
}

nlohmann::json mapperHistoryToJson(const std::vector<MapperHistoryEvent>& events)
{
    nlohmann::json result = nlohmann::json::array();
    for (const MapperHistoryEvent& event : events) {
        result.push_back(mapperHistoryEventToJson(event));
    }
    return result;
}

void addMapperHistoryEvent(std::vector<MapperHistoryEvent>& events, MapperHistoryEvent event)
{
    const auto duplicate = std::find_if(
        events.begin(),
        events.end(),
        [&](const MapperHistoryEvent& current) { return sameMapperHistoryEvent(current, event); }
    );
    if (duplicate == events.end()) {
        events.push_back(std::move(event));
    }
}

MapperHistoryEvent projectOnSurfaceMapperHistoryEvent(
    MapperHistoryEndpoint source,
    MapperHistoryEndpoint target,
    std::string shapeKind,
    MapperHistoryRelation relation,
    std::string makerStage,
    nlohmann::json evidence,
    MapperHistoryRecoverability recoverability,
    std::string diagnosticStatus
)
{
    MapperHistoryEvent event;
    event.source = std::move(source);
    event.target = std::move(target);
    event.shapeKind = std::move(shapeKind);
    event.relation = relation;
    event.makerStage = std::move(makerStage);
    event.evidence = evidence.is_null() ? nlohmann::json::object() : std::move(evidence);
    event.recoverability = recoverability;
    event.diagnosticStatus = std::move(diagnosticStatus);
    return event;
}

}  // namespace cad_core::part
