#pragma once

#include "cad_core/app/property.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cad_core::app {

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

}  // namespace cad_core::app
