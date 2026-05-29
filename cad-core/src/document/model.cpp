#include "cad_core/document/model.h"

#include <iterator>
#include <set>
#include <utility>

namespace cad_core::document {

using runtime::addDiagnostic;

namespace {

std::optional<Link> readLinkObject(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("PropertyType") || !value.at("PropertyType").is_string()) {
        return std::nullopt;
    }

    const std::string propertyType = value.at("PropertyType").get<std::string>();
    if (propertyType != "App::PropertyLink" && propertyType != "App::PropertyLinkSub") {
        return std::nullopt;
    }
    if (!value.contains("value") || !value.at("value").is_string() || value.at("value").get<std::string>().empty()) {
        return std::nullopt;
    }

    std::vector<std::string> subnames;
    const auto subListIt = value.find("SubList");
    if (subListIt != value.end()) {
        if (!subListIt->is_array()) {
            return std::nullopt;
        }
        for (const auto& subname : *subListIt) {
            if (!subname.is_string()) {
                return std::nullopt;
            }
            subnames.push_back(subname.get<std::string>());
        }
    }

    return Link{value.at("value").get<std::string>(), std::move(subnames)};
}

}  // namespace

bool isLink(const nlohmann::json& value)
{
    return readLinkObject(value).has_value();
}

std::vector<Link> readLinks(const nlohmann::json& value)
{
    std::vector<Link> links;
    if (!value.is_object() || !value.contains("PropertyType") || !value.at("PropertyType").is_string()) {
        return links;
    }

    const std::string propertyType = value.at("PropertyType").get<std::string>();
    if (propertyType == "App::PropertyLink" || propertyType == "App::PropertyLinkSub") {
        auto link = readLinkObject(value);
        if (link) {
            links.push_back(std::move(*link));
        }
        return links;
    }

    if (propertyType == "App::PropertyLinkList" || propertyType == "App::PropertyLinkSubList") {
        const auto rawLinksIt = value.find("value");
        if (rawLinksIt == value.end() || !rawLinksIt->is_array()) {
            return links;
        }
        for (const auto& item : *rawLinksIt) {
            if (item.is_string() && !item.get<std::string>().empty()) {
                links.push_back({item.get<std::string>(), {}});
                continue;
            }

            auto link = readLinkObject(item);
            if (link) {
                links.push_back(std::move(*link));
            }
        }
    }

    return links;
}

void collectLinks(const nlohmann::json& value, std::vector<Link>& links)
{
    auto normalized = readLinks(value);
    if (!normalized.empty()) {
        links.insert(links.end(), std::make_move_iterator(normalized.begin()), std::make_move_iterator(normalized.end()));
        return;
    }
    if (value.is_array()) {
        for (const auto& item : value) {
            collectLinks(item, links);
        }
        return;
    }
    if (value.is_object()) {
        for (const auto& item : value.items()) {
            collectLinks(item.value(), links);
        }
    }
}

std::optional<Link> readLink(const nlohmann::json& value)
{
    return readLinkObject(value);
}

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw)
{
    Document document;
    std::vector<runtime::Diagnostic> diagnostics;

    if (!raw.is_object()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document root must be a JSON object");
        return {document, diagnostics};
    }

    const auto objectsIt = raw.find("Objects");
    if (objectsIt == raw.end() || !objectsIt->is_array()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document field 'Objects' must be a list");
        return {document, diagnostics};
    }

    std::set<std::string> seenNames;
    std::set<long long> seenIds;
    for (std::size_t index = 0; index < objectsIt->size(); ++index) {
        const auto& item = objectsIt->at(index);
        if (!item.is_object()) {
            addDiagnostic(diagnostics, "error", "parse_error", "Objects[" + std::to_string(index) + "] must be an object");
            continue;
        }
        if (!item.contains("Name") || !item.at("Name").is_string() || item.at("Name").get<std::string>().empty()) {
            addDiagnostic(diagnostics, "error", "missing_property", "Objects[" + std::to_string(index) + "] is missing required field Name");
            continue;
        }

        std::string name = item.at("Name").get<std::string>();
        if (seenNames.count(name) != 0U) {
            addDiagnostic(diagnostics, "error", "duplicate_object_name", "Duplicate object name " + name, name);
            continue;
        }
        seenNames.insert(name);

        if (!item.contains("ID") || !item.at("ID").is_number_integer()) {
            addDiagnostic(diagnostics, "error", "missing_property", "Object " + name + " is missing required field ID", name, "ID");
            continue;
        }
        const long long id = item.at("ID").get<long long>();
        if (seenIds.count(id) != 0U) {
            addDiagnostic(diagnostics, "error", "duplicate_object_id", "Duplicate object ID " + std::to_string(id), name, "ID");
            continue;
        }
        seenIds.insert(id);

        if (!item.contains("TypeId") || !item.at("TypeId").is_string() || item.at("TypeId").get<std::string>().empty()) {
            addDiagnostic(diagnostics, "error", "missing_property", "Object " + name + " is missing required field TypeId", name);
            continue;
        }

        nlohmann::json properties = nlohmann::json::object();
        if (!item.contains("Properties") || !item.at("Properties").is_object()) {
            addDiagnostic(diagnostics, "error", "missing_property", "Object " + name + " is missing required field Properties", name, "Properties");
            continue;
        }
        properties = item.at("Properties");

        document.indexByName[name] = document.objects.size();
        document.objects.push_back({name, id, item.at("TypeId").get<std::string>(), std::move(properties)});
    }

    if (raw.contains("recompute") && raw.at("recompute").is_object() && raw.at("recompute").contains("objs")) {
        const auto& rawTargets = raw.at("recompute").at("objs");
        if (!rawTargets.is_array()) {
            addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must be a list of object names");
        }
        else {
            for (const auto& target : rawTargets) {
                if (!target.is_string()) {
                    addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must contain object names");
                    continue;
                }
                document.targets.push_back(target.get<std::string>());
            }
        }
    }
    else {
        for (const auto& object : document.objects) {
            document.targets.push_back(object.name);
        }
    }

    return {document, diagnostics};
}

}  // namespace cad_core::document
