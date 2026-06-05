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

}  // namespace cad_core::part

namespace cad_core::topo {

using namespace cad_core::part;

}  // namespace cad_core::topo
