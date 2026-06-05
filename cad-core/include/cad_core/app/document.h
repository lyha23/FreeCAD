#pragma once

// App-layer request model aligned with FreeCAD src/App DocumentObject,
// Property*, PropertyLinks, PropertyGeo and GeoFeature ownership.
#include "cad_core/app/document_object.h"
#include "cad_core/app/property_geo.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::app {

const PropertyValue* propertyValue(const DocumentObject& object, const std::string& property);
bool hasPropertyType(const DocumentObject& object, const std::string& property, const std::string& propertyType);
std::vector<Link> readLinks(const DocumentObject& object, const std::string& property);
std::optional<Link> readLink(const DocumentObject& object, const std::string& property);
std::optional<bool> readBool(const DocumentObject& object, const std::string& property);
std::optional<double> readNumber(const DocumentObject& object, const std::string& property);
std::optional<std::string> readString(const DocumentObject& object, const std::string& property);
std::optional<std::array<double, 3>> readVector3(const DocumentObject& object, const std::string& property);
std::optional<Placement> readPlacement(const DocumentObject& object, const std::string& property);

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw);

}  // namespace cad_core::app

namespace cad_core::document {

using cad_core::app::hasPropertyType;
using cad_core::app::parseDocument;
using cad_core::app::propertyValue;
using cad_core::app::readBool;
using cad_core::app::readLink;
using cad_core::app::readLinks;
using cad_core::app::readNumber;
using cad_core::app::readPlacement;
using cad_core::app::readString;
using cad_core::app::readVector3;

}  // namespace cad_core::document
