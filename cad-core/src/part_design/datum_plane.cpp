#include "cad_core/part_design/datum_plane.h"

#include "datum_attachment.h"

#include "cad_core/runtime/feature_executor.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <utility>

namespace cad_core::part_design {

namespace {

nlohmann::json pointToJson(const gp_Pnt& point)
{
    return nlohmann::json::array({point.X(), point.Y(), point.Z()});
}

nlohmann::json directionToJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

}  // namespace

void executeDatumPlane(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // DatumPlane is a reference plane provider: Length/Width are display-only compatibility
    // inputs, while attachment consumers use the placement/frame below.
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"ResizeMode",
                                               "Length",
                                               "Width",
                                               "AttachmentSupport",
                                               "MapMode",
                                               "MapReversed",
                                               "Reverse",
                                               "MapPathParameter",
                                               "Parameter",
                                               "AttachmentOffset",
                                               "Role"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto attachment = detail::datumAttachmentPlacement(object, context, detail::DatumAttachmentEngine::Plane);
    if (!attachment) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const gp_Trsf placement = attachment->placement;
    gp_Pnt origin(0.0, 0.0, 0.0);
    gp_Dir xAxis(1.0, 0.0, 0.0);
    gp_Dir normal(0.0, 0.0, 1.0);
    origin.Transform(placement);
    xAxis.Transform(placement);
    normal.Transform(placement);
    context.globalPlacements[object.name] = placement;

    nlohmann::json result = {
        {"status", "ok"},
        {"datum", "plane"},
        {"attached", attachment->attached},
        {"origin", pointToJson(origin)},
        {"x_axis", directionToJson(xAxis)},
        {"normal", directionToJson(normal)},
    };
    if (attachment->attached) {
        result["map_mode"] = attachment->mapMode;
        if (attachment->aliasSourceMode != attachment->mapMode) {
            result["alias_source_mode"] = attachment->aliasSourceMode;
        }
    }
    context.objects[object.name] = std::move(result);
}

}  // namespace cad_core::part_design
