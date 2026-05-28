#pragma once

#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::document {

struct DocumentObject {
    std::string name;
    long long id = 0;
    std::string typeId;
    nlohmann::json properties = nlohmann::json::object();
};

struct Document {
    std::vector<DocumentObject> objects;
    std::vector<std::string> targets;
    std::map<std::string, std::size_t> indexByName;
};

struct Link {
    std::string object;
    std::vector<std::string> subnames;
};

bool isLink(const nlohmann::json& value);
void collectLinks(const nlohmann::json& value, std::vector<Link>& links);
std::optional<Link> readLink(const nlohmann::json& value);

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw);

}  // namespace cad_core::document
