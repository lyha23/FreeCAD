#pragma once

#include "cad_core/part/element_map_producer_trace_snapshot.h"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>

#include <cstddef>
#include <string>
#include <vector>

namespace cad_core::part
{

// Producer-owned ephemeral capture used only inside the Part implementation. It queries OCCT
// M/G/D once in the naming-consumption order. The recorder receives only the const JSON
// projection returned by inspectRawMakerMapper(), never these shapes or lists.
struct RawMakerHistoryEntry
{
    std::size_t sourceOrdinal = 0;
    std::string sourceOwner;
    std::string sourceIndexed;
    TopAbs_ShapeEnum sourceKind = TopAbs_SHAPE;
    TopoDS_Shape sourceShape;
    TopTools_ListOfShape modified;
    TopTools_ListOfShape generated;
    bool deleted = false;
    std::string modifiedError;
    std::string generatedError;
    std::string deletedError;
};

struct RawMakerHistoryCapture
{
    TopoDS_Shape output;
    nlohmann::json inputs = nlohmann::json::array();
    std::vector<RawMakerHistoryEntry> entries;
};

RawMakerHistoryCapture captureRawMakerHistory(const std::vector<NamedShapeSource>& sources,
                                              const TopoDS_Shape& output,
                                              BRepBuilderAPI_MakeShape& maker);
const RawMakerHistoryEntry* findRawMakerHistoryEntry(const RawMakerHistoryCapture& capture,
                                                     std::size_t sourceOrdinal,
                                                     TopAbs_ShapeEnum sourceKind,
                                                     int sourceIndex) noexcept;
nlohmann::json inspectRawMakerMapper(const RawMakerHistoryCapture& capture);

}  // namespace cad_core::part
