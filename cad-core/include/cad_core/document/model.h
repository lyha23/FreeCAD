#pragma once

#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::document {

enum class PropertyKind {
    Unknown,
    Bool,
    Integer,
    Float,
    String,
    Enumeration,
    Vector,
    Placement,
    Link,
    LinkList,
    LinkSub,
    LinkSubList,
};

struct Link {
    std::string object;
    std::vector<std::string> subnames;
    std::vector<std::string> stableSubnames;
    std::vector<std::string> fullSubnames;
    std::string property;
};

struct Placement {
    std::array<double, 3> base;
    std::array<double, 4> rotation;
};

struct PropertyValue {
    std::string name;
    std::string propertyType;
    PropertyKind kind = PropertyKind::Unknown;
    nlohmann::json raw = nullptr;
    std::vector<Link> links;
    bool valid = true;
};

struct DocumentObject {
    std::string name;
    long long id = 0;
    std::string typeId;
    nlohmann::json properties = nlohmann::json::object();
    std::map<std::string, PropertyValue> propertyValues;
    std::vector<Link> dependencyLinks;
    std::set<std::string> invalidProperties;
};

struct Document {
    std::vector<DocumentObject> objects;
    std::vector<std::string> targets;
    std::map<std::string, std::size_t> indexByName;
    std::map<std::string, std::string> parentGroupByObject;
};

bool isLink(const nlohmann::json& value);
void collectLinks(const nlohmann::json& value, std::vector<Link>& links);
std::vector<Link> readLinks(const nlohmann::json& value);
std::optional<Link> readLink(const nlohmann::json& value);
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

}  // namespace cad_core::document
