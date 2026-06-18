#pragma once

#include "cad_core/app/property_links.h"

#include "sketch_object_geometry.h"

#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::app
{
struct DocumentObject;
}

namespace cad_core::runtime
{
struct ComputeContext;
struct ShapeValue;
}

namespace cad_core::part
{
enum class ElementResolveStatus;
}

namespace cad_core::sketcher
{

struct ExternalGeometryFlags
{
    bool defining = false;
    bool frozen = false;
    bool detached = false;
    bool missing = false;
    bool sync = false;
};

struct ExternalSubshape
{
    TopAbs_ShapeEnum kind = TopAbs_SHAPE;
    TopoDS_Shape shape;
    std::string subname;
};

struct ExternalGeometryResult
{
    std::vector<SketchSegment> segments;
    std::vector<gp_Pnt> points;
    std::vector<SketchPoint> definingPoints;
    std::vector<SketchCircle> circles;
    std::vector<SketchArc> arcs;
    std::vector<SketchEllipse> ellipses;
    std::vector<SketchEllipseArc> ellipseArcs;
    std::vector<SketchBSpline> bsplines;
    std::vector<SketchBezier> beziers;
    std::size_t definingLinkCount = 0;
    std::size_t frozenLinkCount = 0;
    std::size_t detachedLinkCount = 0;
    std::size_t missingLinkCount = 0;
    std::size_t syncLinkCount = 0;
    std::size_t recoveredMissingLinkCount = 0;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
// ::SketchObject::rebuildExternalGeometry(), external geometry flags and ReferenceShadow
// updates are maintained with the external geometry link payload.
std::set<std::string> normalizedExternalGeometryFlagSet(ExternalGeometryFlags flags);
ExternalGeometryFlags externalGeometryFlags(const app::Link& link);
ExternalGeometryFlags externalGeometryFlags(const nlohmann::json& value);
nlohmann::json externalGeometryFlagsJson(const std::set<std::string>& flags);
nlohmann::json externalReferenceShadowsJson(const std::vector<app::ReferenceShadow>& shadows);
nlohmann::json externalGeometryLinkItemJson(const app::Link& link,
                                            const std::set<std::string>& flags);
std::string externalGeometryReferenceKey(const app::Link& link);
std::optional<ExternalSubshape> oldExternalSubshapeFromBrepSnapshot(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<std::vector<ExternalSubshape>> resolveExternalGeometryLink(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<ExternalGeometryResult> rebuildExternalGeometry(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const gp_Trsf& sketchPlacement
);

} // namespace cad_core::sketcher
