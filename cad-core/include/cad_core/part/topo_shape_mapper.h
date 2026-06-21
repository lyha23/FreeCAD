#pragma once

// Part-layer MapperHistory core aligned with FreeCAD
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp
// and TopoShapeExpansion.cpp history consumption.
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::part
{

enum class MapperHistoryRelation
{
    Identity,
    Preserved,
    Generated,
    Modified,
    Split,
    Merge,
    Deleted,
};

enum class MapperHistoryRecoverability
{
    Resolved,
    Recoverable,
    NeedsReselect,
    Deleted,
    Ambiguous,
    Diagnostic,
    Unknown,
};

struct MapperHistoryEndpoint
{
    std::string object;
    std::string subname;
};

struct MapperHistoryEvent
{
    MapperHistoryEndpoint source;
    MapperHistoryEndpoint target;
    std::string shapeKind;
    MapperHistoryRelation relation = MapperHistoryRelation::Identity;
    std::string makerStage;
    nlohmann::json evidence = nlohmann::json::object();
    MapperHistoryRecoverability recoverability = MapperHistoryRecoverability::Unknown;
    std::string diagnosticStatus;
};

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeShapeWithElementMap(), consumes "MapperHistory" / "MapperMaker" before
// writing an ElementMap; this core event schema is cad-core's request-local equivalent ledger.
std::string mapperHistoryRelationName(MapperHistoryRelation relation);
std::string mapperHistoryRecoverabilityName(MapperHistoryRecoverability recoverability);
nlohmann::json mapperHistoryEventToJson(const MapperHistoryEvent& event);
nlohmann::json mapperHistoryToJson(const std::vector<MapperHistoryEvent>& events);
void addMapperHistoryEvent(std::vector<MapperHistoryEvent>& events, MapperHistoryEvent event);
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
// ::ProjectOnSurface::projectWire(), iterates "TopExp_Explorer xp(wireToTake, TopAbs_EDGE)";
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::ShapeMapper::insert()
// prevents one target from being both Generated and Modified. This helper keeps the
// ProjectOnSurface source endpoint, target endpoint, and projection-item evidence in topo
// MapperHistory instead of letting adapters infer provenance from output geometry.
MapperHistoryEvent projectOnSurfaceMapperHistoryEvent(
    MapperHistoryEndpoint source,
    MapperHistoryEndpoint target,
    std::string shapeKind,
    MapperHistoryRelation relation,
    std::string makerStage,
    nlohmann::json evidence,
    MapperHistoryRecoverability recoverability,
    std::string diagnosticStatus
);

}  // namespace cad_core::part
