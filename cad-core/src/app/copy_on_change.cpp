#include "cad_core/app/copy_on_change.h"

#include "cad_core/app/document.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>

namespace cad_core::app {

namespace {

constexpr long long CopyOnChangeDisabled = 0LL;
constexpr long long CopyOnChangeEnabled = 1LL;
constexpr long long CopyOnChangeOwned = 2LL;
constexpr long long CopyOnChangeTracking = 3LL;

const DocumentObject* documentObjectByName(const CopyOnChangeDocumentView& view,
                                           const std::string& name)
{
    if (view.objects == nullptr) {
        return nullptr;
    }
    const auto it = view.objects->find(name);
    return it == view.objects->end() ? nullptr : it->second;
}

long long copyOnChangeMode(const DocumentObject& object)
{
    const auto value = readNumber(object, "LinkCopyOnChange");
    return value ? static_cast<long long>(*value) : CopyOnChangeDisabled;
}

nlohmann::json propertyLinkJson(const std::string& target)
{
    return {
        {"PropertyType", "App::PropertyLink"},
        {"value", target},
    };
}

nlohmann::json propertyXLinkJson(const std::string& target)
{
    return {
        {"PropertyType", "App::PropertyXLink"},
        {"value", target},
    };
}

nlohmann::json propertyBoolJson(bool value)
{
    return {
        {"PropertyType", "App::PropertyBool"},
        {"value", value},
    };
}

nlohmann::json propertyIntegerJson(long long value)
{
    return {
        {"PropertyType", "App::PropertyInteger"},
        {"value", value},
    };
}

nlohmann::json propertyStringJson(const std::string& value)
{
    return {
        {"PropertyType", "App::PropertyString"},
        {"value", value},
    };
}

nlohmann::json linkPropertyJson(const Link& link)
{
    nlohmann::json value = {
        {"PropertyType", "App::PropertyXLink"},
        {"value", link.object},
    };
    if (!link.subnames.empty()) {
        value["SubList"] = link.subnames;
    }
    if (link.stableSubnamesExplicit) {
        value["StableSubList"] = link.stableSubnames;
    }
    if (link.fullSubnamesExplicit) {
        value["FullSubList"] = link.fullSubnames;
    }
    return value;
}

std::string sanitizeObjectName(const std::string& name)
{
    std::string result;
    result.reserve(name.size());
    for (const char item : name) {
        const unsigned char ch = static_cast<unsigned char>(item);
        result.push_back(std::isalnum(ch) || item == '_' ? item : '_');
    }
    return result.empty() ? "Object" : result;
}

std::string defaultGroupName(const DocumentObject& linkObject)
{
    return linkObject.name + "_CopyOnChangeGroup";
}

std::string defaultRootCopyName(const DocumentObject& linkObject)
{
    return linkObject.name + "_CopyOnChangeObject";
}

std::string defaultDependencyCopyName(const DocumentObject& linkObject,
                                      const std::string& sourceName)
{
    return linkObject.name + "_CopyOnChange_" + sanitizeObjectName(sourceName);
}

bool stringPropertyEquals(const DocumentObject& object,
                          const std::string& property,
                          const std::string& value)
{
    const auto found = readString(object, property);
    return found && *found == value;
}

std::optional<std::string> existingCopyForSource(const CopyOnChangeDocumentView& view,
                                                 const DocumentObject& linkObject,
                                                 const std::string& sourceName)
{
    if (view.objects == nullptr) {
        return std::nullopt;
    }
    for (const auto& [name, object] : *view.objects) {
        if (object == nullptr) {
            continue;
        }
        if (stringPropertyEquals(*object, "_CopyOnChangeOwner", linkObject.name)
            && stringPropertyEquals(*object, "_CopyOnChangeSourceObject", sourceName)) {
            return name;
        }
    }
    return std::nullopt;
}

std::string uniqueCopyName(const CopyOnChangeDocumentView& view,
                           const std::set<std::string>& assigned,
                           std::string preferred)
{
    const auto available = [&](const std::string& candidate) {
        return assigned.count(candidate) == 0U && documentObjectByName(view, candidate) == nullptr;
    };
    if (available(preferred)) {
        return preferred;
    }
    for (int index = 1; index < 1000; ++index) {
        const std::string candidate = preferred + "_" + std::to_string(index);
        if (available(candidate)) {
            return candidate;
        }
    }
    return preferred + "_Copy";
}

long long maxObjectId(const CopyOnChangeDocumentView& view)
{
    long long result = 0;
    if (view.objects == nullptr) {
        return result;
    }
    for (const auto& [name, object] : *view.objects) {
        (void)name;
        if (object != nullptr) {
            result = std::max(result, object->id);
        }
    }
    return result;
}

bool copyOnChangeControlExcludes(const DocumentObject& object,
                                 const DocumentObject& linkObject)
{
    const auto propIt = object.properties.find("_CopyOnChangeControl");
    if (propIt == object.properties.end() || !propIt->is_object()) {
        return false;
    }
    const auto valuesIt = propIt->find("values");
    if (valuesIt == propIt->end() || !valuesIt->is_object()) {
        return false;
    }
    const auto valueFor = [&](const std::string& key) -> std::string {
        const auto it = valuesIt->find(key);
        return it != valuesIt->end() && it->is_string() ? it->get<std::string>() : std::string {};
    };
    return valueFor("*") == "-" || valueFor(linkObject.name) == "-";
}

void collectDependencyOrder(const CopyOnChangeDocumentView& view,
                            const DocumentObject& linkObject,
                            const std::string& name,
                            const std::string& rootName,
                            std::set<std::string>& visiting,
                            std::set<std::string>& visited,
                            std::vector<std::string>& order)
{
    if (visited.count(name) != 0U || visiting.count(name) != 0U) {
        return;
    }
    const auto* object = documentObjectByName(view, name);
    if (object == nullptr) {
        return;
    }
    if (name != rootName && copyOnChangeControlExcludes(*object, linkObject)) {
        return;
    }

    visiting.insert(name);
    if (view.dependencies != nullptr) {
        const auto depIt = view.dependencies->find(name);
        if (depIt != view.dependencies->end()) {
            for (const std::string& dependency : depIt->second) {
                collectDependencyOrder(view,
                                       linkObject,
                                       dependency,
                                       rootName,
                                       visiting,
                                       visited,
                                       order);
            }
        }
    }
    visiting.erase(name);
    visited.insert(name);
    order.push_back(name);
}

std::vector<std::string> sourceCopyOrder(const CopyOnChangeDocumentView& view,
                                         const DocumentObject& linkObject,
                                         const std::string& sourceName)
{
    std::set<std::string> visiting;
    std::set<std::string> visited;
    std::vector<std::string> order;
    collectDependencyOrder(view, linkObject, sourceName, sourceName, visiting, visited, order);
    if (order.empty() && documentObjectByName(view, sourceName) != nullptr) {
        order.push_back(sourceName);
    }
    return order;
}

std::string rewriteSubname(const std::string& value,
                           const std::map<std::string, std::string>& sourceToCopy)
{
    for (const auto& [source, copy] : sourceToCopy) {
        if (value == source) {
            return copy;
        }
        const std::string prefix = source + ".";
        if (value.rfind(prefix, 0U) == 0U) {
            return copy + value.substr(source.size());
        }
    }
    return value;
}

void rewriteStringList(nlohmann::json& value,
                       const std::map<std::string, std::string>& sourceToCopy)
{
    if (!value.is_array()) {
        return;
    }
    for (auto& item : value) {
        if (item.is_string()) {
            item = rewriteSubname(item.get<std::string>(), sourceToCopy);
        }
    }
}

void rewriteReferenceShadows(nlohmann::json& value,
                             const std::map<std::string, std::string>& sourceToCopy,
                             const std::map<std::string, long long>& copyIdBySource)
{
    if (!value.is_array()) {
        return;
    }
    for (auto& item : value) {
        if (!item.is_object()) {
            continue;
        }
        const auto targetIt = item.find("target");
        if (targetIt == item.end() || !targetIt->is_string()) {
            continue;
        }
        const std::string target = targetIt->get<std::string>();
        const auto copyIt = sourceToCopy.find(target);
        if (copyIt == sourceToCopy.end()) {
            continue;
        }
        item["target"] = copyIt->second;
        const auto idIt = copyIdBySource.find(target);
        if (idIt != copyIdBySource.end()) {
            item["targetId"] = idIt->second;
        }
    }
}

void rewriteLinkJson(nlohmann::json& value,
                     const std::map<std::string, std::string>& sourceToCopy,
                     const std::map<std::string, long long>& copyIdBySource)
{
    if (value.is_array()) {
        for (auto& item : value) {
            rewriteLinkJson(item, sourceToCopy, copyIdBySource);
        }
        return;
    }
    if (!value.is_object()) {
        return;
    }

    const auto typeIt = value.find("PropertyType");
    const bool linkProperty = typeIt != value.end() && typeIt->is_string()
        && typeIt->get<std::string>().find("Link") != std::string::npos;

    if (linkProperty) {
        const auto valueIt = value.find("value");
        if (valueIt != value.end() && valueIt->is_string()) {
            const std::string target = valueIt->get<std::string>();
            const auto copyIt = sourceToCopy.find(target);
            if (copyIt != sourceToCopy.end()) {
                value["value"] = copyIt->second;
            }
        }
        const auto valuesIt = value.find("values");
        if (valuesIt != value.end() && valuesIt->is_array()) {
            for (auto& item : *valuesIt) {
                if (!item.is_string()) {
                    continue;
                }
                const std::string target = item.get<std::string>();
                const auto copyIt = sourceToCopy.find(target);
                if (copyIt != sourceToCopy.end()) {
                    item = copyIt->second;
                }
            }
        }
        for (const char* field : {"SubList", "StableSubList", "FullSubList"}) {
            const auto listIt = value.find(field);
            if (listIt != value.end()) {
                rewriteStringList(*listIt, sourceToCopy);
            }
        }
        const auto shadowIt = value.find("ReferenceShadow");
        if (shadowIt != value.end()) {
            rewriteReferenceShadows(*shadowIt, sourceToCopy, copyIdBySource);
        }
    }

    const auto subSetIt = value.find("SubSet");
    if (subSetIt != value.end() && subSetIt->is_array()) {
        for (auto& item : *subSetIt) {
            rewriteLinkJson(item, sourceToCopy, copyIdBySource);
        }
    }
    for (auto& item : value.items()) {
        if (item.key() == "ReferenceShadow") {
            rewriteReferenceShadows(item.value(), sourceToCopy, copyIdBySource);
        }
        else if (item.value().is_object() || item.value().is_array()) {
            rewriteLinkJson(item.value(), sourceToCopy, copyIdBySource);
        }
    }
}

nlohmann::json groupValuesProperty(const std::vector<std::string>& values)
{
    return {
        {"PropertyType", "App::PropertyLinkList"},
        {"values", values},
    };
}

nlohmann::json historyPreserveSummary(const nlohmann::json& properties)
{
    bool hasReferenceShadow = false;
    bool hasStableSubList = false;
    bool hasFullSubList = false;
    for (const auto& property : properties.items()) {
        if (!property.value().is_object()) {
            continue;
        }
        hasReferenceShadow = hasReferenceShadow || property.value().contains("ReferenceShadow");
        hasStableSubList = hasStableSubList || property.value().contains("StableSubList");
        hasFullSubList = hasFullSubList || property.value().contains("FullSubList");
    }
    return {
        {"propertyTree", "deep_copy"},
        {"linkRelink", "copied_subtree"},
        {"referenceShadow", hasReferenceShadow ? "preserved_and_retargeted" : "not_present"},
        {"stableSubList", hasStableSubList ? "preserved" : "not_present"},
        {"fullSubList", hasFullSubList ? "preserved" : "not_present"},
    };
}

}  // namespace

CopyOnChangeLifecycleResult buildCopyOnChangeLifecycleUpdates(
    const DocumentObject& linkObject,
    const Link& linkedObject,
    const CopyOnChangeDocumentView& view)
{
    CopyOnChangeLifecycleResult result;
    const long long mode = copyOnChangeMode(linkObject);
    if (mode == CopyOnChangeDisabled) {
        return result;
    }

    const auto sourceLink = readLink(linkObject, "LinkCopyOnChangeSource");
    if (mode == CopyOnChangeOwned && !sourceLink) {
        return result;
    }
    const std::string sourceName =
        (mode == CopyOnChangeEnabled || !sourceLink) ? linkedObject.object : sourceLink->object;
    const auto* sourceObject = documentObjectByName(view, sourceName);
    if (sourceObject == nullptr) {
        runtime::addDiagnostic(result.diagnostics,
                               "error",
                               "copy_on_change_missing_source",
                               "CopyOnChange source object " + sourceName + " is missing",
                               linkObject.name,
                               "LinkCopyOnChangeSource",
                               "app",
                               sourceName);
        return result;
    }

    const bool touched = readBool(linkObject, "LinkCopyOnChangeTouched").value_or(false);
    const bool linkedIsSource = linkedObject.object == sourceName;
    if (mode == CopyOnChangeTracking && !touched) {
        return result;
    }
    if (mode == CopyOnChangeOwned && !linkedIsSource && !touched) {
        return result;
    }

    const auto group = readLink(linkObject, "LinkCopyOnChangeGroup");
    const std::string groupName = group ? group->object : defaultGroupName(linkObject);
    const std::vector<std::string> order = sourceCopyOrder(view, linkObject, sourceName);

    std::set<std::string> assignedNames;
    std::map<std::string, std::string> sourceToCopy;
    std::map<std::string, long long> copyIdBySource;
    long long nextId = maxObjectId(view) + 1LL;
    const auto* groupObject = documentObjectByName(view, groupName);
    const long long groupObjectId = groupObject == nullptr ? nextId++ : groupObject->id;

    for (const std::string& source : order) {
        const bool root = source == sourceName;
        std::optional<std::string> existing = existingCopyForSource(view, linkObject, source);
        if (root && !linkedIsSource && documentObjectByName(view, linkedObject.object) != nullptr) {
            existing = linkedObject.object;
        }
        std::string preferred = root ? defaultRootCopyName(linkObject)
                                     : defaultDependencyCopyName(linkObject, source);
        std::string copyName = existing.value_or(uniqueCopyName(view, assignedNames, preferred));
        assignedNames.insert(copyName);
        sourceToCopy.emplace(source, copyName);

        if (const auto* existingObject = documentObjectByName(view, copyName)) {
            copyIdBySource.emplace(source, existingObject->id);
        }
        else {
            copyIdBySource.emplace(source, nextId++);
        }
    }

    std::vector<std::string> groupValues;
    groupValues.reserve(order.size());
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        groupValues.push_back(sourceToCopy.at(*it));
    }

    nlohmann::json groupProperties = {
        {"Group", groupValuesProperty(groupValues)},
    };
    const bool groupExists = groupObject != nullptr;
    result.documentObjectUpdates.push_back({
        {"action", groupExists ? "update" : "create"},
        {"reason", "copy_on_change_group_sync"},
        {"object", groupName},
        {"objectId", groupObjectId},
        {"typeId", "App::DocumentObjectGroup"},
        {"owner", linkObject.name},
        {"ownerId", linkObject.id},
        {"properties", groupProperties},
    });

    for (const std::string& source : order) {
        const auto* object = documentObjectByName(view, source);
        if (object == nullptr) {
            continue;
        }
        const std::string& copyName = sourceToCopy.at(source);
        const bool exists = documentObjectByName(view, copyName) != nullptr;
        nlohmann::json properties = object->properties;
        rewriteLinkJson(properties, sourceToCopy, copyIdBySource);
        properties["_CopyOnChangeOwner"] = propertyStringJson(linkObject.name);
        properties["_CopyOnChangeSourceObject"] = propertyStringJson(source);
        properties["_CopyOnChangeSourceId"] = propertyIntegerJson(object->id);
        if (source == sourceName) {
            properties["Visibility"] = propertyBoolJson(false);
        }

        result.documentObjectUpdates.push_back({
            {"action", exists ? "update" : "create"},
            {"reason", "copy_on_change_deep_copy_lifecycle"},
            {"object", copyName},
            {"objectId", exists ? documentObjectByName(view, copyName)->id : copyIdBySource.at(source)},
            {"typeId", object->typeId},
            {"sourceObject", source},
            {"sourceObjectId", object->id},
            {"group", groupName},
            {"properties", properties},
            {"dependencyRewrite", sourceToCopy},
            {"historyPreserve", historyPreserveSummary(properties)},
        });
    }

    const std::string copyRoot = sourceToCopy.at(sourceName);
    nlohmann::json linkProperties = {
        {"LinkedObject", propertyXLinkJson(copyRoot)},
        {"LinkCopyOnChangeSource", propertyLinkJson(sourceName)},
        {"LinkCopyOnChangeGroup", propertyLinkJson(groupName)},
        {"LinkCopyOnChangeTouched", propertyBoolJson(false)},
    };
    if (mode == CopyOnChangeEnabled || mode == CopyOnChangeOwned) {
        linkProperties["LinkCopyOnChange"] = propertyIntegerJson(CopyOnChangeOwned);
    }
    else {
        linkProperties["LinkCopyOnChange"] = propertyIntegerJson(CopyOnChangeTracking);
    }

    result.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "copy_on_change_deep_copy_lifecycle"},
        {"object", linkObject.name},
        {"objectId", linkObject.id},
        {"typeId", linkObject.typeId},
        {"sourceObject", sourceName},
        {"copyObject", copyRoot},
        {"group", groupName},
        {"properties", linkProperties},
    });

    return result;
}

}  // namespace cad_core::app
