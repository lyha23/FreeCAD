#pragma once

#include "cad_core/runtime/diagnostics.h"
#include "cad_core/app/property_links.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::app
{

struct Document;

bool isLinkPropertyType(const std::string& propertyType);
bool isLinkObjectType(const std::string& propertyType);
bool isLinkSubObjectType(const std::string& propertyType);
bool isLinkListType(const std::string& propertyType);
bool isXLinkListType(const std::string& propertyType);
bool isLinkSubListType(const std::string& propertyType);
bool isHiddenLinkPropertyType(const std::string& propertyType);
bool isMalformedLinkValue(const nlohmann::json& value, const std::string& propertyType);
void normalizeSourceObjectRenameLinks(Document& document);
void normalizeLabelReferenceLinks(Document& document, std::vector<runtime::Diagnostic>& diagnostics);

} // namespace cad_core::app
