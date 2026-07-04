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
    std::string geometryKind;
};

struct RawSketchEdgeIdentity
{
    std::string indexed;
    SketchGeometryIdentity source;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::FaceMakerBuildFace::splitSelfIntersecting(), records
    // "myPreSplitHistory->AddModified(edge, fi.Value())"; and
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()
    // chains that through "MapperHistory(myPreSplitHistory)" before mapped names are published.
    // cad-core keeps the request-local source edge and fragment stable token beside the indexed
    // InternalEdge so response and reference resolution consume the same split history ledger.
    std::string sourceIndexed;
    std::optional<std::string> stableSubname;
    std::optional<std::string> fragmentStableSubname;
    std::string explicitIdentityStatus;
};

struct RawSketchEdgeIdentityLedger
{
    std::vector<RawSketchEdgeIdentity> edges;
    std::size_t stableCount = 0;
    std::size_t fallbackCount = 0;
    std::size_t splitFragmentCount = 0;
};

SketchGeometryIdentity sketchGeometryIdentity(std::size_t geometryIndex,
                                              std::optional<long> geometryId,
                                              std::string geometryKind = {});

std::string stableSubnameForGeometryId(long geometryId);

RawSketchEdgeIdentityLedger buildRawSketchEdgeIdentityLedger(
    const TopoDS_Shape& rawShape,
    const std::vector<TopoDS_Edge>& sourceEdges,
    const std::vector<SketchGeometryIdentity>& sourceIdentities,
    bool sourceOrderMatchesPublishedShape = false);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::buildInternals() publishes request-local "InternalShape";
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()
// consumes split history through MapperHistory. This view turns the already-published
// InternalShape split history into g<ID>:splitN tokens without consulting mesh/bbox/order.
void addSplitFragmentIdentitiesFromInternalHistory(
    RawSketchEdgeIdentityLedger& ledger,
    const part::NamedShape& internalNamedShape);

part::NamedShape namedShapeForSketchRawEdgeIdentity(
    const std::string& owner,
    const TopoDS_Shape& rawShape,
    const RawSketchEdgeIdentityLedger& ledger);

void publishRawSketchEdgeIdentity(nlohmann::json& mesh,
                                  nlohmann::json& subshapes,
                                  const RawSketchEdgeIdentityLedger& ledger);

nlohmann::json rawSketchEdgeIdentityObject(const RawSketchEdgeIdentityLedger& ledger);

}  // namespace cad_core::sketcher
