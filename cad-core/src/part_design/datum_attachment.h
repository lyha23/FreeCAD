#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/feature_executor.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::part_design::detail {

inline const nlohmann::json& propertyPayload(const app::PropertyValue& property)
{
    if (property.raw.is_object() && property.raw.contains("value")) {
        return property.raw.at("value");
    }
    return property.raw;
}

inline bool isDefaultDatumMapMode(const app::DocumentObject& object)
{
    const auto* property = app::propertyValue(object, "MapMode");
    if (property == nullptr) {
        return true;
    }

    const nlohmann::json& payload = propertyPayload(*property);
    if (payload.is_string()) {
        const std::string value = payload.get<std::string>();
        return value.empty() || value == "Deactivated";
    }
    if (payload.is_number_integer()) {
        return payload.get<int>() == 0;
    }
    return true;
}

inline bool rejectUnsupportedDatumAttachment(const app::DocumentObject& object,
                                             runtime::ComputeContext& context)
{
    const std::vector<app::Link> supportLinks = app::readLinks(object, "AttachmentSupport");
    app::Link support;
    bool hasAttachmentSupport = false;
    for (const auto& link : supportLinks) {
        if (link.object.empty()) {
            continue;
        }
        support = link;
        hasAttachmentSupport = true;
        break;
    }

    const bool hasActiveMapMode = !isDefaultDatumMapMode(object);
    if (!hasAttachmentSupport && !hasActiveMapMode) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::AttachExtension() initializes MapMode to "mmDeactivated";
    // ::AttachExtension::positionBySupport() returns before solving when
    // "_props.attacher->mapMode == mmDeactivated". DatumPoint.cpp, DatumLine.cpp,
    // DatumPlane.cpp and DatumCS.cpp each install AttachEnginePoint/Line/Plane/3D.
    // cad-core does not migrate that AttachEngine solver in the Datum placement slice.
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Datum AttachmentSupport/MapMode requires AttachEngine map-mode solving",
        object.name,
        hasActiveMapMode ? "MapMode" : "AttachmentSupport",
        "runtime",
        support.object,
        support.subnames.empty() ? std::string {} : support.subnames.front()
    );
    return false;
}

} // namespace cad_core::part_design::detail
