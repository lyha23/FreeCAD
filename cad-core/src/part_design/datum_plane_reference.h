#pragma once

#include "cad_core/runtime/compute_context.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <optional>
#include <string>
#include <string_view>

namespace cad_core::part_design::detail {

struct ReferencePlaneProviderFrame
{
    gp_Trsf placement;
    gp_Pnt origin;
    gp_Dir xAxis;
    gp_Dir normal;
};

inline bool isReferencePlaneProviderType(std::string_view typeId)
{
    return typeId == "PartDesign::Plane" || typeId == "App::Plane";
}

inline std::optional<ReferencePlaneProviderFrame> referencePlaneProviderFrame(
    const std::string& objectName,
    const runtime::ComputeContext& context)
{
    const auto documentIt = context.documentObjects.find(objectName);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr
        || !isReferencePlaneProviderType(documentIt->second->typeId)) {
        return std::nullopt;
    }

    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()
        || objectIt->second.value("status", "") != "ok" || objectIt->second.value("datum", "") != "plane") {
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(objectName);
    const gp_Trsf placement = placementIt == context.globalPlacements.end() ? gp_Trsf{} : placementIt->second;

    gp_Pnt origin(0.0, 0.0, 0.0);
    gp_Dir xAxis(1.0, 0.0, 0.0);
    gp_Dir normal(0.0, 0.0, 1.0);
    origin.Transform(placement);
    xAxis.Transform(placement);
    normal.Transform(placement);
    return ReferencePlaneProviderFrame{placement, origin, xAxis, normal};
}

}  // namespace cad_core::part_design::detail
