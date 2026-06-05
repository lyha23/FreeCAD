#include "cad_core/app/document.h"

#include <algorithm>
#include <cmath>

namespace cad_core::app
{

namespace
{

bool isNumberValue(const nlohmann::json& value)
{
    return value.is_number() && std::isfinite(value.get<double>());
}

bool isValidVector3Value(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() != 3U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const nlohmann::json& item) {
        return isNumberValue(item);
    });
}

bool isValidPlacementValue(const nlohmann::json& value)
{
    if (!value.is_object()) {
        return false;
    }
    const auto baseIt = value.find("Base");
    const auto rotationIt = value.find("Rotation");
    if (baseIt == value.end() || rotationIt == value.end() || !isValidVector3Value(*baseIt)
        || !rotationIt->is_array() || rotationIt->size() != 4U) {
        return false;
    }

    double normSquared = 0.0;
    for (const auto& item : *rotationIt) {
        if (!isNumberValue(item)) {
            return false;
        }
        const double component = item.get<double>();
        normSquared += component * component;
    }
    return normSquared > 0.0;
}

} // namespace

std::optional<Placement> readPlacement(const DocumentObject& object, const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::GeoFeature(),
    // declares "ADD_PROPERTY_TYPE(Placement, (Base::Placement()), ... App::Prop_None, \"\")";
    // cad-core keeps Placement normalized in document and lets geometry convert it to gp_Trsf.
    const auto* value = propertyValue(object, property);
    if (value == nullptr || value->kind != PropertyKind::Placement || !isValidPlacementValue(value->raw)) {
        return std::nullopt;
    }

    const auto& base = value->raw.at("Base");
    const auto& rotation = value->raw.at("Rotation");
    return Placement{
        std::array<double, 3>{base.at(0).get<double>(), base.at(1).get<double>(), base.at(2).get<double>()},
        std::array<double, 4>{rotation.at(0).get<double>(),
                              rotation.at(1).get<double>(),
                              rotation.at(2).get<double>(),
                              rotation.at(3).get<double>()},
    };
}

} // namespace cad_core::app
