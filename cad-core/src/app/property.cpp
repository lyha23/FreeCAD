#include "cad_core/app/document.h"

#include "property_internal.h"
#include "property_links_internal.h"

#include <algorithm>
#include <cmath>

namespace cad_core::app
{

using runtime::addDiagnostic;

namespace
{

PropertyKind kindFromPropertyType(const std::string& propertyType)
{
    if (propertyType == "App::PropertyBool") {
        return PropertyKind::Bool;
    }
    if (propertyType == "App::PropertyInteger") {
        return PropertyKind::Integer;
    }
    if (propertyType == "App::PropertyFloat" || propertyType == "App::PropertyLength" || propertyType == "App::PropertyAngle"
        || propertyType == "App::PropertyDistance") {
        return PropertyKind::Float;
    }
    if (propertyType == "App::PropertyString") {
        return PropertyKind::String;
    }
    if (propertyType == "App::PropertyEnumeration") {
        return PropertyKind::Enumeration;
    }
    if (propertyType == "App::PropertyVector" || propertyType == "App::PropertyVectorDistance") {
        return PropertyKind::Vector;
    }
    if (propertyType == "App::PropertyPlacement") {
        return PropertyKind::Placement;
    }
    if (propertyType == "App::PropertyLink" || propertyType == "App::PropertyLinkGlobal"
        || propertyType == "App::PropertyLinkHidden" || propertyType == "App::PropertyXLink") {
        return PropertyKind::Link;
    }
    if (propertyType == "App::PropertyLinkList" || propertyType == "App::PropertyLinkListHidden"
        || propertyType == "App::PropertyXLinkList") {
        return PropertyKind::LinkList;
    }
    if (propertyType == "App::PropertyLinkSub" || propertyType == "App::PropertyLinkSubHidden"
        || propertyType == "App::PropertyXLinkSub" || propertyType == "App::PropertyXLinkSubHidden") {
        return PropertyKind::LinkSub;
    }
    if (propertyType == "App::PropertyLinkSubList" || propertyType == "App::PropertyLinkSubListHidden"
        || propertyType == "App::PropertyXLinkSubList") {
        return PropertyKind::LinkSubList;
    }
    return PropertyKind::Unknown;
}

PropertyKind inferUntypedKind(const nlohmann::json& value)
{
    if (value.is_boolean()) {
        return PropertyKind::Bool;
    }
    if (value.is_number_integer()) {
        return PropertyKind::Integer;
    }
    if (value.is_number()) {
        return PropertyKind::Float;
    }
    if (value.is_string()) {
        return PropertyKind::String;
    }
    if (value.is_array() && value.size() == 3U) {
        const bool vectorLike = std::all_of(value.begin(), value.end(), [](const nlohmann::json& item) {
            return item.is_number();
        });
        if (vectorLike) {
            return PropertyKind::Vector;
        }
    }
    return PropertyKind::Unknown;
}

std::optional<std::string> propertyTypeOf(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("PropertyType") || !value.at("PropertyType").is_string()) {
        return std::nullopt;
    }
    return value.at("PropertyType").get<std::string>();
}

const nlohmann::json& propertyPayload(const nlohmann::json& value)
{
    if (value.is_object() && value.contains("PropertyType") && value.contains("value")) {
        return value.at("value");
    }
    return value;
}

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

bool hasInvalidTypedPayload(const nlohmann::json& raw, PropertyKind kind)
{
    if (!raw.is_object() || !raw.contains("value")) {
        return false;
    }

    const nlohmann::json& payload = propertyPayload(raw);
    switch (kind) {
        case PropertyKind::Bool:
            return !payload.is_boolean();
        case PropertyKind::Integer:
            return !payload.is_number_integer();
        case PropertyKind::Float:
            return !isNumberValue(payload);
        case PropertyKind::String:
            return !payload.is_string();
        case PropertyKind::Enumeration:
            return !payload.is_string() && !payload.is_number_integer();
        case PropertyKind::Vector:
            return !isValidVector3Value(payload);
        default:
            return false;
    }
}

} // namespace

PropertyValue parsePropertyValue(const std::string& objectName,
                                 const std::string& propertyName,
                                 const nlohmann::json& raw,
                                 std::vector<runtime::Diagnostic>& diagnostics)
{
    PropertyValue property;
    property.name = propertyName;
    property.raw = raw;

    const auto propertyType = propertyTypeOf(raw);
    if (!propertyType) {
        property.kind = inferUntypedKind(raw);
        collectLinks(raw, property.links);
        for (auto& link : property.links) {
            if (link.property.empty()) {
                link.property = propertyName;
            }
        }
        return property;
    }

    property.propertyType = *propertyType;
    property.kind = kindFromPropertyType(*propertyType);

    if (property.kind == PropertyKind::Placement && !isValidPlacementValue(raw)) {
        property.valid = false;
        addDiagnostic(diagnostics,
                      "error",
                      "invalid_placement",
                      "Property " + propertyName + " has an invalid App::PropertyPlacement value",
                      objectName,
                      propertyName,
                      "parse");
    }
    else if (hasInvalidTypedPayload(raw, property.kind)) {
        property.valid = false;
        addDiagnostic(diagnostics,
                      "error",
                      "invalid_property_type",
                      "Property " + propertyName + " has an invalid " + *propertyType + " value",
                      objectName,
                      propertyName,
                      "parse");
    }

    if (isLinkPropertyType(*propertyType)) {
        property.links = readLinks(raw);
        for (auto& link : property.links) {
            if (link.property.empty()) {
                link.property = propertyName;
            }
        }
        if (isMalformedLinkValue(raw, *propertyType)) {
            property.valid = false;
            addDiagnostic(diagnostics,
                          "error",
                          "invalid_link_value",
                          "Property " + propertyName + " has an invalid " + *propertyType + " value",
                          objectName,
                          propertyName,
                          "parse");
        }
    }

    return property;
}

const PropertyValue* propertyValue(const DocumentObject& object, const std::string& property)
{
    const auto it = object.propertyValues.find(property);
    if (it == object.propertyValues.end()) {
        return nullptr;
    }
    return &it->second;
}

bool hasPropertyType(const DocumentObject& object, const std::string& property, const std::string& propertyType)
{
    const auto* value = propertyValue(object, property);
    return value != nullptr && value->propertyType == propertyType;
}

std::vector<Link> readLinks(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return {};
    }
    return value->links;
}

std::optional<Link> readLink(const DocumentObject& object, const std::string& property)
{
    const auto links = readLinks(object, property);
    if (links.size() != 1U) {
        return std::nullopt;
    }
    return links.front();
}

std::optional<bool> readBool(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!payload.is_boolean()) {
        return std::nullopt;
    }
    return payload.get<bool>();
}

std::optional<double> readNumber(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!isNumberValue(payload)) {
        return std::nullopt;
    }
    return payload.get<double>();
}

std::optional<std::string> readString(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!payload.is_string()) {
        return std::nullopt;
    }
    return payload.get<std::string>();
}

std::optional<std::array<double, 3>> readVector3(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!isValidVector3Value(payload)) {
        return std::nullopt;
    }
    return std::array<double, 3>{payload.at(0).get<double>(), payload.at(1).get<double>(), payload.at(2).get<double>()};
}

} // namespace cad_core::app
