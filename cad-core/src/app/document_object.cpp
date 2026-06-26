#include "document_object_internal.h"

#include "property_internal.h"
#include "property_links_internal.h"

#include <algorithm>
#include <utility>

namespace cad_core::app {

using runtime::addDiagnostic;

namespace
{

bool sameDependencyLink(const Link& left, const Link& right)
{
    return left.object == right.object && left.subnames == right.subnames
        && left.stableSubnames == right.stableSubnames && left.property == right.property;
}

bool isIntrinsicReferenceAxis(const Link& link)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::getBaseObject() and Body origin flows can resolve local "X_Axis", "Y_Axis" and
    // "Z_Axis" names through the coordinate system instead of requiring a normal document object.
    return link.property == "ReferenceAxis" && link.subnames.empty()
        && (link.object == "X_Axis" || link.object == "Y_Axis" || link.object == "Z_Axis");
}

void addDependencyLinks(DocumentObject& object, const PropertyValue& property)
{
    std::vector<Link> dependencies = property.links;
    std::vector<Link> nested;
    collectLinks(property.raw, nested);
    for (Link& link : nested) {
        if (link.property.empty()) {
            link.property = property.name;
        }
        if (isIntrinsicReferenceAxis(link)) {
            continue;
        }
        const bool duplicate = std::any_of(
            dependencies.begin(),
            dependencies.end(),
            [&](const Link& current) { return sameDependencyLink(current, link); }
        );
        if (!duplicate) {
            dependencies.push_back(std::move(link));
        }
    }
    for (const Link& dependency : dependencies) {
        if (!isIntrinsicReferenceAxis(dependency)) {
            object.dependencyLinks.push_back(dependency);
        }
    }
}

}  // namespace

std::optional<DocumentObject> parseDocumentObject(const nlohmann::json& item,
                                                  std::size_t index,
                                                  std::set<std::string>& seenNames,
                                                  std::set<long long>& seenIds,
                                                  std::vector<runtime::Diagnostic>& diagnostics)
{
    if (!item.is_object()) {
        addDiagnostic(diagnostics,
                      "error",
                      "parse_error",
                      "Objects[" + std::to_string(index) + "] must be an object",
                      {},
                      {},
                      "parse");
        return std::nullopt;
    }
    if (!item.contains("Name") || !item.at("Name").is_string() || item.at("Name").get<std::string>().empty()) {
        addDiagnostic(diagnostics,
                      "error",
                      "missing_property",
                      "Objects[" + std::to_string(index) + "] is missing required field Name",
                      {},
                      {},
                      "parse");
        return std::nullopt;
    }

    std::string name = item.at("Name").get<std::string>();
    if (seenNames.count(name) != 0U) {
        addDiagnostic(diagnostics, "error", "duplicate_object_name", "Duplicate object name " + name, name, {}, "parse");
        return std::nullopt;
    }
    seenNames.insert(name);

    if (!item.contains("ID") || !item.at("ID").is_number_integer()) {
        addDiagnostic(diagnostics,
                      "error",
                      "missing_property",
                      "Object " + name + " is missing required field ID",
                      name,
                      "ID",
                      "parse");
        return std::nullopt;
    }
    const long long id = item.at("ID").get<long long>();
    if (seenIds.count(id) != 0U) {
        addDiagnostic(diagnostics, "error", "duplicate_object_id", "Duplicate object ID " + std::to_string(id), name, "ID", "parse");
        return std::nullopt;
    }
    seenIds.insert(id);

    if (!item.contains("TypeId") || !item.at("TypeId").is_string() || item.at("TypeId").get<std::string>().empty()) {
        addDiagnostic(diagnostics,
                      "error",
                      "missing_property",
                      "Object " + name + " is missing required field TypeId",
                      name,
                      {},
                      "parse");
        return std::nullopt;
    }

    nlohmann::json properties = nlohmann::json::object();
    if (!item.contains("Properties") || !item.at("Properties").is_object()) {
        addDiagnostic(diagnostics,
                      "error",
                      "missing_property",
                      "Object " + name + " is missing required field Properties",
                      name,
                      "Properties",
                      "parse");
        return std::nullopt;
    }
    properties = item.at("Properties");

    DocumentObject object;
    object.name = name;
    object.id = id;
    object.typeId = item.at("TypeId").get<std::string>();
    object.properties = std::move(properties);

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h::PropertyLinkSub::getSubValues()
    // and PropertyLinkSubList::getSubListValues() expose linked object plus sub-element lists.
    // cad-core keeps the raw JSON for compatibility, but graph/runtime consume this normalized property map.
    for (const auto& property : object.properties.items()) {
        auto parsed = parsePropertyValue(object.name, property.key(), property.value(), diagnostics);
        if (!isHiddenLinkPropertyType(parsed.propertyType)) {
            addDependencyLinks(object, parsed);
        }
        if (!parsed.valid) {
            object.invalidProperties.insert(property.key());
        }
        object.propertyValues.emplace(property.key(), std::move(parsed));
    }

    return object;
}

}  // namespace cad_core::app
