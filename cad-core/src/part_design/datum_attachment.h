#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/feature_executor.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
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

inline std::string datumMapModeLabel(const app::DocumentObject& object)
{
    const auto* property = app::propertyValue(object, "MapMode");
    if (property == nullptr) {
        return "Deactivated";
    }

    const nlohmann::json& payload = propertyPayload(*property);
    if (payload.is_string()) {
        return payload.get<std::string>();
    }
    if (payload.is_number_integer()) {
        return std::to_string(payload.get<int>());
    }
    return {};
}

inline bool hasNonDefaultPlacement(const app::DocumentObject& object, const std::string& propertyName)
{
    const auto placement = app::readPlacement(object, propertyName);
    if (!placement) {
        return app::propertyValue(object, propertyName) != nullptr;
    }

    constexpr double tolerance = 1.0e-12;
    const auto near = [](double lhs, double rhs) {
        return std::abs(lhs - rhs) <= tolerance;
    };
    return !(near(placement->base[0], 0.0) && near(placement->base[1], 0.0)
             && near(placement->base[2], 0.0) && near(placement->rotation[0], 0.0)
             && near(placement->rotation[1], 0.0) && near(placement->rotation[2], 0.0)
             && near(placement->rotation[3], 1.0));
}

inline bool hasActiveNumberProperty(const app::DocumentObject& object, const std::string& propertyName)
{
    const auto value = app::readNumber(object, propertyName);
    if (!value) {
        return false;
    }
    return std::abs(*value) > 1.0e-12;
}

inline std::string firstSupportSubname(const app::Link& support)
{
    if (!support.subnames.empty()) {
        return support.subnames.front();
    }
    if (!support.stableSubnames.empty()) {
        return support.stableSubnames.front();
    }
    if (!support.shadowSubs.empty()) {
        if (!support.shadowSubs.front().oldName.empty()) {
            return support.shadowSubs.front().oldName;
        }
        return support.shadowSubs.front().newName;
    }
    return {};
}

inline bool hasReferenceStabilityEvidence(const app::Link& support)
{
    return support.stableSubnamesExplicit || !support.shadowSubs.empty() || !support.referenceShadows.empty()
        || support.fullSubnamesExplicit;
}

inline void addDatumAttachmentDiagnostic(runtime::ComputeContext& context,
                                         const app::DocumentObject& object,
                                         const app::Link& support,
                                         const std::string& property,
                                         const std::string& message)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           message,
                           object.name,
                           property,
                           "runtime",
                           support.object,
                           firstSupportSubname(support));
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
    const bool hasActiveAttachmentOffset = hasNonDefaultPlacement(object, "AttachmentOffset");
    const bool hasActiveMapReversed = app::readBool(object, "MapReversed").value_or(false)
        || app::readBool(object, "Reverse").value_or(false);
    const bool hasActiveMapPathParameter = hasActiveNumberProperty(object, "MapPathParameter")
        || hasActiveNumberProperty(object, "Parameter");

    if (!hasAttachmentSupport && !hasActiveMapMode && !hasActiveAttachmentOffset && !hasActiveMapReversed
        && !hasActiveMapPathParameter) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
    // ::AttachExtension::AttachExtension() initializes MapMode to "mmDeactivated";
    // ::AttachExtension::positionBySupport() calls "setOffset(AttachmentOffset.getValue() *
    // basePlacement.inverse())" and then "calculateAttachedPlacement(plaOriginal, &subChanged)".
    // Attacher.cpp::AttachEngine::calculateAttachedPlacement() can rewrite subnames only when the
    // recalculated placement "stays the same". DatumPoint.cpp, DatumLine.cpp, DatumPlane.cpp and
    // DatumCS.cpp each install AttachEnginePoint/Line/Plane/3D, so active support stays a locatable
    // diagnostic until cad-core owns the equivalent request-local AttachEngine and link-shadow path.
    const std::string mode = datumMapModeLabel(object);
    if (hasActiveMapMode) {
        addDatumAttachmentDiagnostic(
            context,
            object,
            support,
            "MapMode",
            "Datum MapMode=" + mode
                + " requires FreeCAD AttachEngine placement solving and subname-stability writeback"
        );
    }
    else if (hasAttachmentSupport) {
        addDatumAttachmentDiagnostic(
            context,
            object,
            support,
            "AttachmentSupport",
            "Datum AttachmentSupport requires a selected AttachEngine MapMode before cad-core can solve placement"
        );
    }
    if (hasActiveAttachmentOffset) {
        addDatumAttachmentDiagnostic(
            context,
            object,
            support,
            "AttachmentOffset",
            "Datum AttachmentOffset requires FreeCAD AttachExtension offset composition with AttachEngine placement"
        );
    }
    if (hasActiveMapReversed) {
        addDatumAttachmentDiagnostic(context,
                                     object,
                                     support,
                                     "MapReversed",
                                     "Datum MapReversed/Reverse requires AttachEngine axis reversal");
    }
    if (hasActiveMapPathParameter) {
        addDatumAttachmentDiagnostic(
            context,
            object,
            support,
            "MapPathParameter",
            "Datum MapPathParameter/Parameter requires AttachEngine point-on-curve parameter solving"
        );
    }
    if (hasAttachmentSupport && hasReferenceStabilityEvidence(support)) {
        addDatumAttachmentDiagnostic(
            context,
            object,
            support,
            "AttachmentSupport",
            "Datum AttachmentSupport carries stable/shadow subname evidence, but downstream reference writeback is not solved"
        );
    }
    return false;
}

} // namespace cad_core::part_design::detail
