#pragma once

#include "cad_core/part/topo_shape.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::sketcher
{

struct SketchGeometryIdentity
{
    std::size_t geometryIndex = 0;
    std::optional<long> geometryId;
};

struct RawSketchEdgeIdentity
{
    std::string indexed;
    SketchGeometryIdentity source;
};

struct RawSketchEdgeIdentityLedger
{
    std::vector<RawSketchEdgeIdentity> edges;
    std::size_t stableCount = 0;
    std::size_t fallbackCount = 0;
};

SketchGeometryIdentity sketchGeometryIdentity(std::size_t geometryIndex,
                                              std::optional<long> geometryId);

std::string stableSubnameForGeometryId(long geometryId);

RawSketchEdgeIdentityLedger buildRawSketchEdgeIdentityLedger(
    const TopoDS_Shape& rawShape,
    const std::vector<TopoDS_Edge>& sourceEdges,
    const std::vector<SketchGeometryIdentity>& sourceIdentities);

part::NamedShape namedShapeForSketchRawEdgeIdentity(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const RawSketchEdgeIdentityLedger& ledger);

void publishRawSketchEdgeIdentity(nlohmann::json& mesh,
                                  nlohmann::json& subshapes,
                                  const RawSketchEdgeIdentityLedger& ledger);

nlohmann::json rawSketchEdgeIdentityObject(const RawSketchEdgeIdentityLedger& ledger);

}  // namespace cad_core::sketcher
