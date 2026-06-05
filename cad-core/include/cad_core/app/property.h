#pragma once

#include "cad_core/app/property_links.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::app {

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

struct PropertyValue {
    std::string name;
    std::string propertyType;
    PropertyKind kind = PropertyKind::Unknown;
    nlohmann::json raw = nullptr;
    std::vector<Link> links;
    bool valid = true;
};

}  // namespace cad_core::app
