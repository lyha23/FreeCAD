#include "property_links_internal.h"

#include "cad_core/app/document.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace cad_core::app
{

using runtime::addDiagnostic;

namespace
{

std::optional<std::string> propertyTypeOf(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("PropertyType") || !value.at("PropertyType").is_string()) {
        return std::nullopt;
    }
    return value.at("PropertyType").get<std::string>();
}

std::optional<std::vector<std::string>> readStringList(const nlohmann::json& value)
{
    if (!value.is_array()) {
        return std::nullopt;
    }

    std::vector<std::string> items;
    for (const auto& item : value) {
        if (!item.is_string()) {
            return std::nullopt;
        }
        items.push_back(item.get<std::string>());
    }
    return items;
}

bool isSupportedReferenceShapeType(const std::string& shapeType)
{
    return shapeType == "Vertex" || shapeType == "Edge" || shapeType == "Face";
}

std::optional<std::vector<ShadowSub>> readShadowSubList(const nlohmann::json& value)
{
    if (!value.is_array()) {
        return std::nullopt;
    }

    std::vector<ShadowSub> items;
    for (const auto& item : value) {
        if (!item.is_object()) {
            return std::nullopt;
        }
        const auto newNameIt = item.find("newName");
        const auto oldNameIt = item.find("oldName");
        if (newNameIt == item.end() || oldNameIt == item.end() || !newNameIt->is_string()
            || !oldNameIt->is_string()) {
            return std::nullopt;
        }
        items.push_back({newNameIt->get<std::string>(), oldNameIt->get<std::string>()});
    }
    return items;
}

std::optional<BrepSnapshot> readBrepSnapshot(const nlohmann::json& value)
{
    if (!value.is_object()) {
        return std::nullopt;
    }
    const auto formatIt = value.find("format");
    const auto byteLengthIt = value.find("byteLength");
    const auto sha256It = value.find("sha256");
    const auto dataIt = value.find("data");
    if (formatIt == value.end() || byteLengthIt == value.end() || sha256It == value.end()
        || dataIt == value.end() || !formatIt->is_string() || !byteLengthIt->is_number_integer()
        || !sha256It->is_string() || !dataIt->is_string()) {
        return std::nullopt;
    }

    const std::string format = formatIt->get<std::string>();
    if (format != "brep-text" && format != "brep-bin-zstd-base64") {
        return std::nullopt;
    }
    const long long byteLength = byteLengthIt->get<long long>();
    if (byteLength < 0) {
        return std::nullopt;
    }

    return BrepSnapshot{format, byteLength, sha256It->get<std::string>(), dataIt->get<std::string>()};
}

std::optional<std::vector<ReferenceShadow>> readReferenceShadowList(const nlohmann::json& value)
{
    if (!value.is_array()) {
        return std::nullopt;
    }

    std::vector<ReferenceShadow> items;
    for (const auto& item : value) {
        if (!item.is_object()) {
            return std::nullopt;
        }
        const auto targetIt = item.find("target");
        const auto targetIdIt = item.find("targetId");
        const auto propertyIt = item.find("property");
        const auto shapeTypeIt = item.find("shapeType");
        const auto indexedIt = item.find("indexed");
        const auto subnameIt = item.find("subname");
        if (targetIt == item.end() || targetIdIt == item.end() || propertyIt == item.end()
            || shapeTypeIt == item.end() || indexedIt == item.end() || subnameIt == item.end()
            || !targetIt->is_string() || !targetIdIt->is_number_integer() || !propertyIt->is_string()
            || !shapeTypeIt->is_string() || !indexedIt->is_string() || !subnameIt->is_string()) {
            return std::nullopt;
        }

        const std::string shapeType = shapeTypeIt->get<std::string>();
        if (!isSupportedReferenceShapeType(shapeType)) {
            return std::nullopt;
        }

        ReferenceShadow shadow;
        shadow.target = targetIt->get<std::string>();
        shadow.targetId = targetIdIt->get<long long>();
        shadow.property = propertyIt->get<std::string>();
        shadow.shapeType = shapeType;
        shadow.indexed = indexedIt->get<std::string>();
        shadow.subname = subnameIt->get<std::string>();

        const auto stableSubnameIt = item.find("stableSubname");
        if (stableSubnameIt != item.end()) {
            if (!stableSubnameIt->is_string()) {
                return std::nullopt;
            }
            shadow.stableSubname = stableSubnameIt->get<std::string>();
        }

        const auto fingerprintIt = item.find("fingerprint");
        if (fingerprintIt != item.end()) {
            if (!fingerprintIt->is_object()) {
                return std::nullopt;
            }
            shadow.fingerprint = *fingerprintIt;
        }

        const auto brepIt = item.find("brep");
        if (brepIt != item.end()) {
            auto brep = readBrepSnapshot(*brepIt);
            if (!brep) {
                return std::nullopt;
            }
            shadow.brep = std::move(*brep);
        }

        items.push_back(std::move(shadow));
    }
    return items;
}

std::vector<std::string> readOptionalStringList(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end()) {
        return {};
    }
    auto items = readStringList(*it);
    return items.value_or(std::vector<std::string>{});
}

std::string readOptionalStringField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

std::optional<bool> readOptionalBoolField(const nlohmann::json& value, const std::string& field)
{
    const auto it = value.find(field);
    if (it == value.end() || !it->is_boolean()) {
        return std::nullopt;
    }
    return it->get<bool>();
}

std::optional<LinkDocumentRef> readLinkDocumentRef(const nlohmann::json& value)
{
    const auto it = value.find("Document");
    if (it == value.end()) {
        return std::nullopt;
    }
    if (!it->is_object()) {
        return std::nullopt;
    }
    LinkDocumentRef ref;
    ref.file = readOptionalStringField(*it, "file");
    ref.name = readOptionalStringField(*it, "name");
    ref.label = readOptionalStringField(*it, "label");
    ref.stamp = readOptionalStringField(*it, "stamp");
    ref.status = readOptionalStringField(*it, "status");
    ref.currentName = readOptionalStringField(*it, "currentName");
    ref.currentLabel = readOptionalStringField(*it, "currentLabel");
    ref.currentStamp = readOptionalStringField(*it, "currentStamp");
    ref.currentStatus = readOptionalStringField(*it, "currentStatus");
    if (const auto allowPartial = readOptionalBoolField(*it, "allowPartial")) {
        ref.allowPartial = *allowPartial;
        ref.allowPartialExplicit = true;
    }
    if (ref.file.empty() && ref.name.empty() && ref.label.empty() && ref.stamp.empty()
        && ref.status.empty() && ref.currentName.empty() && ref.currentLabel.empty()
        && ref.currentStamp.empty() && ref.currentStatus.empty() && !ref.allowPartialExplicit) {
        return std::nullopt;
    }
    return ref;
}

std::set<std::string> readExternalGeometryFlags(const nlohmann::json& value)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.cpp
    // ::ExternalGeometryExtension::saveAttributes() writes "Flags" as a bitset, while
    // ::flag2str exposes the stable names "Defining", "Frozen", "Detached", "Missing", "Sync".
    // cad-core accepts either the bitset-style integer or a fixture-facing string list.
    constexpr std::array<const char*, 5> knownFlags = {
        "Defining",
        "Frozen",
        "Detached",
        "Missing",
        "Sync",
    };
    std::set<std::string> flags;
    const auto addFlag = [&](const std::string& flag) {
        if (std::find(knownFlags.begin(), knownFlags.end(), flag) != knownFlags.end()) {
            flags.insert(flag);
        }
    };
    const auto readFlagList = [&](const nlohmann::json& raw) {
        if (!raw.is_array()) {
            return;
        }
        for (const auto& item : raw) {
            if (item.is_string()) {
                addFlag(item.get<std::string>());
            }
        }
    };

    if (const auto it = value.find("ExternalFlags"); it != value.end()) {
        readFlagList(*it);
    }
    if (const auto it = value.find("Flags"); it != value.end()) {
        if (it->is_number_unsigned() || it->is_number_integer()) {
            const long long bits = it->get<long long>();
            if (bits >= 0) {
                for (std::size_t index = 0; index < knownFlags.size(); ++index) {
                    if ((bits & (1LL << index)) != 0) {
                        flags.insert(knownFlags.at(index));
                    }
                }
            }
        }
        else {
            readFlagList(*it);
        }
    }
    for (const char* flag : knownFlags) {
        if (const auto it = value.find(flag); it != value.end() && it->is_boolean() && it->get<bool>()) {
            flags.insert(flag);
        }
    }
    return flags;
}

std::optional<Link> readLinkObject(const nlohmann::json& value, const std::string& property = {})
{
    const auto propertyType = propertyTypeOf(value);
    if (!propertyType || !isLinkObjectType(*propertyType)) {
        return std::nullopt;
    }

    const auto objectIt = value.find("value");
    if (objectIt == value.end() || objectIt->is_null()) {
        return std::nullopt;
    }
    if (!objectIt->is_string() || objectIt->get<std::string>().empty()) {
        return std::nullopt;
    }

    std::vector<std::string> subnames;
    const auto subListIt = value.find("SubList");
    if (subListIt != value.end()) {
        auto parsed = readStringList(*subListIt);
        if (!parsed) {
            return std::nullopt;
        }
        subnames = std::move(*parsed);
    }

    const bool stableSubnamesExplicit = value.contains("StableSubList");
    std::vector<std::string> stableSubnames = readOptionalStringList(value, "StableSubList");
    if (stableSubnames.empty()) {
        stableSubnames = subnames;
    }
    std::vector<ShadowSub> shadowSubs;
    const auto shadowSubIt = value.find("ShadowSub");
    if (shadowSubIt != value.end()) {
        auto parsed = readShadowSubList(*shadowSubIt);
        if (!parsed) {
            return std::nullopt;
        }
        shadowSubs = std::move(*parsed);
    }
    std::vector<ReferenceShadow> referenceShadows;
    const auto referenceShadowIt = value.find("ReferenceShadow");
    if (referenceShadowIt != value.end()) {
        auto parsed = readReferenceShadowList(*referenceShadowIt);
        if (!parsed) {
            return std::nullopt;
        }
        referenceShadows = std::move(*parsed);
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap(), for an external linked document, builds
    // "Data::POSTFIX_EXTERNAL_TAG" before reTagElementMap(). cad-core carries FullSubList as
    // request-side evidence so link retag can preserve the original full subname alias.
    const bool fullSubnamesExplicit = value.contains("FullSubList");
    std::vector<std::string> fullSubnames = readOptionalStringList(value, "FullSubList");
    if (fullSubnames.empty()) {
        fullSubnames = stableSubnames;
    }
    Link link{objectIt->get<std::string>(),
              std::move(subnames),
              std::move(stableSubnames),
              std::move(fullSubnames),
              property,
              stableSubnamesExplicit,
              std::move(shadowSubs),
              std::move(referenceShadows)};
    link.fullSubnamesExplicit = fullSubnamesExplicit;
    link.documentRef = readLinkDocumentRef(value);
    link.externalGeometryFlags = readExternalGeometryFlags(value);
    return link;
}

std::optional<Link> readLinkSubListItem(const nlohmann::json& value, const std::string& property = {})
{
    if (!value.is_object() || value.contains("PropertyType")) {
        return std::nullopt;
    }

    const auto objectIt = value.find("value");
    if (objectIt == value.end() || objectIt->is_null()) {
        return std::nullopt;
    }
    if (!objectIt->is_string() || objectIt->get<std::string>().empty()) {
        return std::nullopt;
    }

    std::vector<std::string> subnames;
    const auto subListIt = value.find("SubList");
    if (subListIt != value.end()) {
        auto parsed = readStringList(*subListIt);
        if (!parsed) {
            return std::nullopt;
        }
        subnames = std::move(*parsed);
    }

    const bool stableSubnamesExplicit = value.contains("StableSubList");
    std::vector<std::string> stableSubnames = readOptionalStringList(value, "StableSubList");
    if (stableSubnames.empty()) {
        stableSubnames = subnames;
    }
    std::vector<ShadowSub> shadowSubs;
    const auto shadowSubIt = value.find("ShadowSub");
    if (shadowSubIt != value.end()) {
        auto parsed = readShadowSubList(*shadowSubIt);
        if (!parsed) {
            return std::nullopt;
        }
        shadowSubs = std::move(*parsed);
    }
    std::vector<ReferenceShadow> referenceShadows;
    const auto referenceShadowIt = value.find("ReferenceShadow");
    if (referenceShadowIt != value.end()) {
        auto parsed = readReferenceShadowList(*referenceShadowIt);
        if (!parsed) {
            return std::nullopt;
        }
        referenceShadows = std::move(*parsed);
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap() retags external linked topology with
    // "Data::POSTFIX_EXTERNAL_TAG"; FullSubList keeps that original full subname separate from
    // the current SubList used for geometry resolution.
    const bool fullSubnamesExplicit = value.contains("FullSubList");
    std::vector<std::string> fullSubnames = readOptionalStringList(value, "FullSubList");
    if (fullSubnames.empty()) {
        fullSubnames = stableSubnames;
    }
    Link link{objectIt->get<std::string>(),
              std::move(subnames),
              std::move(stableSubnames),
              std::move(fullSubnames),
              property,
              stableSubnamesExplicit,
              std::move(shadowSubs),
              std::move(referenceShadows)};
    link.fullSubnamesExplicit = fullSubnamesExplicit;
    link.documentRef = readLinkDocumentRef(value);
    link.externalGeometryFlags = readExternalGeometryFlags(value);
    return link;
}

std::vector<Link> readLinkList(const nlohmann::json& value)
{
    std::vector<Link> links;
    const auto rawLinksIt = value.find("values");
    if (rawLinksIt == value.end() || !rawLinksIt->is_array()) {
        return links;
    }
    for (const auto& item : *rawLinksIt) {
        if (item.is_string() && !item.get<std::string>().empty()) {
            links.push_back({item.get<std::string>(), {}});
        }
    }
    return links;
}

std::vector<Link> readLinkSubList(const nlohmann::json& value)
{
    std::vector<Link> links;
    const auto rawLinksIt = value.find("SubSet");
    if (rawLinksIt == value.end() || !rawLinksIt->is_array()) {
        return links;
    }
    for (const auto& item : *rawLinksIt) {
        auto link = readLinkSubListItem(item);
        if (link) {
            links.push_back(std::move(*link));
        }
    }
    return links;
}

std::optional<std::string> renamedObjectTargetByReferenceShadow(const Link& link,
                                                                const Document& document,
                                                                const std::map<long long, std::string>& objectNameById)
{
    if (link.object.empty() || document.indexByName.count(link.object) != 0U || link.referenceShadows.empty()) {
        return std::nullopt;
    }

    std::set<std::string> candidates;
    for (const auto& shadow : link.referenceShadows) {
        if (shadow.targetId == 0) {
            continue;
        }
        if (!shadow.target.empty() && shadow.target != link.object) {
            continue;
        }
        const auto candidateIt = objectNameById.find(shadow.targetId);
        if (candidateIt != objectNameById.end()) {
            candidates.insert(candidateIt->second);
        }
    }
    if (candidates.size() != 1U) {
        return std::nullopt;
    }
    return *candidates.begin();
}

void rebuildDependencyLinks(DocumentObject& object)
{
    object.dependencyLinks.clear();
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        (void)propertyName;
        if (!isHiddenLinkPropertyType(propertyValue.propertyType)) {
            object.dependencyLinks.insert(object.dependencyLinks.end(),
                                          propertyValue.links.begin(),
                                          propertyValue.links.end());
        }
    }
}

std::string objectLabelOrName(const DocumentObject& object)
{
    const auto label = readString(object, "Label");
    if (label && !label->empty()) {
        return *label;
    }
    return object.name;
}

void addLabelReferenceAmbiguousDiagnostic(std::vector<runtime::Diagnostic>* diagnostics,
                                          const std::string& ownerObject,
                                          const std::string& propertyName,
                                          const std::string& targetObject,
                                          const std::string& subname,
                                          const std::string& reason)
{
    if (diagnostics == nullptr) {
        return;
    }
    addDiagnostic(*diagnostics,
                  "error",
                  "label_reference_ambiguous",
                  propertyName + " target " + targetObject + " label-qualified subname "
                      + subname + " is ambiguous: " + reason,
                  ownerObject,
                  propertyName,
                  "parse",
                  targetObject,
                  subname);
}

std::optional<std::pair<std::string, std::string>> splitExternalDocumentSubname(const std::string& subname);

std::optional<LabelReferenceRename> renameLeadingLabelReference(
    std::vector<std::string>& subnames,
    std::size_t index,
    const std::string& targetObject,
    const std::string& targetLabel,
    const std::map<std::string, std::set<std::string>>& labelOwners,
    std::vector<runtime::Diagnostic>* diagnostics = nullptr,
    const std::string& ownerObject = {},
    const std::string& propertyName = {},
    bool allowExternalDocumentPrefix = true)
{
    if (targetLabel.empty() || index >= subnames.size()) {
        return std::nullopt;
    }
    const std::string subname = subnames.at(index);
    if (allowExternalDocumentPrefix) {
        if (const auto externalSubname = splitExternalDocumentSubname(subname)) {
            std::vector<std::string> localSubnames {externalSubname->second};
            auto rename = renameLeadingLabelReference(localSubnames,
                                                      0U,
                                                      targetObject,
                                                      targetLabel,
                                                      labelOwners,
                                                      diagnostics,
                                                      ownerObject,
                                                      propertyName,
                                                      false);
            if (!rename) {
                return std::nullopt;
            }
            subnames.at(index) = externalSubname->first + localSubnames.front();
            rename->oldSubname = subname;
            rename->newSubname = subnames.at(index);
            return rename;
        }
    }
    if (subname.size() < 4U || subname.front() != '$') {
        return std::nullopt;
    }
    const std::size_t dot = subname.find('.');
    if (dot == std::string::npos || dot <= 1U) {
        return std::nullopt;
    }

    const std::string oldLabel = subname.substr(1U, dot - 1U);
    if (oldLabel == targetLabel) {
        return std::nullopt;
    }
    const auto targetOwnersIt = labelOwners.find(targetLabel);
    if (targetOwnersIt == labelOwners.end() || targetOwnersIt->second.size() != 1U
        || targetOwnersIt->second.count(targetObject) == 0U) {
        addLabelReferenceAmbiguousDiagnostic(diagnostics,
                                             ownerObject,
                                             propertyName,
                                             targetObject,
                                             subname,
                                             "current target Label is not unique");
        return std::nullopt;
    }
    if (const auto ownersIt = labelOwners.find(oldLabel);
        ownersIt != labelOwners.end() && ownersIt->second.count(targetObject) == 0U) {
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObject.cpp
    // ::DocumentObject::onProposedLabelChange() returns
    // "PropertyLinkBase::updateLabelReferences(this, newLabel.c_str())" after Label changes.
    const std::string oldSubname = subname;
    subnames.at(index) = "$" + targetLabel + subname.substr(dot);
    return LabelReferenceRename{index, oldLabel, targetLabel, oldSubname, subnames.at(index)};
}

struct SubnameToken {
    std::string value;
    std::size_t start = 0U;
    std::size_t size = 0U;
};

std::vector<SubnameToken> subnameTokens(const std::string& subname)
{
    std::vector<SubnameToken> tokens;
    std::size_t start = 0U;
    while (start < subname.size()) {
        const std::size_t dot = subname.find('.', start);
        const std::size_t end = dot == std::string::npos ? subname.size() : dot;
        if (end > start) {
            tokens.push_back(SubnameToken{subname.substr(start, end - start), start, end - start});
        }
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1U;
    }
    return tokens;
}

std::optional<std::pair<std::string, std::string>> splitExternalDocumentSubname(const std::string& subname)
{
    const std::size_t hash = subname.find('#');
    if (hash == std::string::npos || hash == 0U || hash + 1U >= subname.size()) {
        return std::nullopt;
    }
    return std::make_pair(subname.substr(0U, hash + 1U), subname.substr(hash + 1U));
}

std::string currentExternalDocumentPrefix(const std::string& persistedPrefix,
                                          const std::optional<LinkDocumentRef>& documentRef)
{
    if (!documentRef || persistedPrefix.empty() || persistedPrefix.back() != '#') {
        return persistedPrefix;
    }
    const std::string documentToken = persistedPrefix.substr(0U, persistedPrefix.size() - 1U);
    if (!documentRef->name.empty() && !documentRef->currentName.empty()
        && documentToken == documentRef->name) {
        return documentRef->currentName + "#";
    }
    if (!documentRef->label.empty() && !documentRef->currentLabel.empty()
        && documentToken == documentRef->label) {
        return documentRef->currentLabel + "#";
    }
    return persistedPrefix;
}

void normalizeExternalDocumentPrefix(std::vector<std::string>& subnames,
                                     std::size_t index,
                                     const Link& link)
{
    if (index >= subnames.size()) {
        return;
    }
    const auto externalSubname = splitExternalDocumentSubname(subnames.at(index));
    if (!externalSubname) {
        return;
    }
    const std::string currentPrefix = currentExternalDocumentPrefix(externalSubname->first, link.documentRef);
    if (currentPrefix == externalSubname->first) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
    // ::PropertyXLinkContainer::afterRestore() fills "_DocMap" from persisted DocMap
    // "name"/"label"; cad-core keeps the request graph stateless and normalizes the
    // FullSubList external document prefix from that Document evidence before writeback.
    subnames.at(index) = currentPrefix + externalSubname->second;
}

const DocumentObject* objectByName(const Document& document, const std::string& name)
{
    const auto it = document.indexByName.find(name);
    if (it == document.indexByName.end()) {
        return nullptr;
    }
    return &document.objects.at(it->second);
}

bool isPlainDocumentObjectGroupType(const std::string& typeId)
{
    return typeId == "App::DocumentObjectGroup" || typeId == "App::DocumentObjectGroupPython";
}

bool objectMatchesToken(const DocumentObject& object, const std::string& token)
{
    if (token == object.name) {
        return true;
    }
    const std::string label = objectLabelOrName(object);
    return !label.empty() && token == "$" + label;
}

std::optional<std::size_t> parseChildIndexToken(const std::string& token)
{
    if (token.empty()) {
        return std::nullopt;
    }
    std::size_t value = 0U;
    for (const char item : token) {
        if (item < '0' || item > '9') {
            return std::nullopt;
        }
        value = value * 10U + static_cast<std::size_t>(item - '0');
    }
    return value;
}

std::vector<Link> childLinksForObject(const DocumentObject& object)
{
    if (isPlainDocumentObjectGroupType(object.typeId)) {
        return readLinks(object, "Group");
    }
    if (object.typeId == "App::LinkGroup") {
        return readLinks(object, "ElementList");
    }
    return {};
}

const DocumentObject* childObjectByToken(const Document& document,
                                         const DocumentObject& object,
                                         const std::string& token)
{
    const auto children = childLinksForObject(object);
    if (children.empty()) {
        return nullptr;
    }
    if (const auto childIndex = parseChildIndexToken(token); childIndex && *childIndex < children.size()) {
        return objectByName(document, children.at(*childIndex).object);
    }
    for (const auto& childLink : children) {
        const auto* child = objectByName(document, childLink.object);
        if (child != nullptr && objectMatchesToken(*child, token)) {
            return child;
        }
    }
    return nullptr;
}

const DocumentObject* linkedObjectTarget(const Document& document, const DocumentObject& object)
{
    if (object.typeId != "App::Link") {
        return nullptr;
    }
    const auto link = readLink(object, "LinkedObject");
    if (!link || link->object.empty()) {
        return nullptr;
    }
    return objectByName(document, link->object);
}

const DocumentObject* resolveNestedLabelPrefix(const Document& document,
                                               const std::string& rootObject,
                                               const std::vector<SubnameToken>& tokens,
                                               std::size_t tokenIndex)
{
    const DocumentObject* current = objectByName(document, rootObject);
    if (current == nullptr || tokenIndex >= tokens.size()) {
        return nullptr;
    }

    for (std::size_t index = 0; index <= tokenIndex; ++index) {
        const std::string& token = tokens.at(index).value;
        if (const auto* linkedTarget = linkedObjectTarget(document, *current)) {
            if (objectMatchesToken(*linkedTarget, token)) {
                current = linkedTarget;
                continue;
            }
            if (const auto* child = childObjectByToken(document, *linkedTarget, token)) {
                current = child;
                continue;
            }
        }
        if (const auto* child = childObjectByToken(document, *current, token)) {
            current = child;
            continue;
        }
        if (objectMatchesToken(*current, token)) {
            continue;
        }
        return nullptr;
    }
    return current;
}

std::optional<LabelReferenceRename> renameNestedLabelReference(
    std::vector<std::string>& subnames,
    std::size_t index,
    const Link& link,
    const Document& document,
    const std::map<std::string, std::set<std::string>>& labelOwners,
    std::vector<runtime::Diagnostic>* diagnostics = nullptr,
    const std::string& ownerObject = {},
    const std::string& propertyName = {},
    bool allowExternalDocumentPrefix = true)
{
    if (index >= subnames.size() || link.object.empty()) {
        return std::nullopt;
    }

    const std::string originalSubname = subnames.at(index);
    if (allowExternalDocumentPrefix) {
        if (const auto externalSubname = splitExternalDocumentSubname(originalSubname)) {
            std::vector<std::string> localSubnames {externalSubname->second};
            auto rename = renameNestedLabelReference(localSubnames,
                                                     0U,
                                                     link,
                                                     document,
                                                     labelOwners,
                                                     diagnostics,
                                                     ownerObject,
                                                     propertyName,
                                                     false);
            if (!rename) {
                return std::nullopt;
            }
            subnames.at(index) = externalSubname->first + localSubnames.front();
            rename->oldSubname = originalSubname;
            rename->newSubname = subnames.at(index);
            return rename;
        }
    }
    const auto originalTokens = subnameTokens(originalSubname);
    for (std::size_t tokenIndex = 0; tokenIndex < originalTokens.size(); ++tokenIndex) {
        const auto& token = originalTokens.at(tokenIndex);
        if (token.value.size() < 2U || token.value.front() != '$') {
            continue;
        }
        const std::string oldLabel = token.value.substr(1U);
        std::vector<std::pair<std::string, std::string>> candidates;
        for (const auto& object : document.objects) {
            if (object.name == link.object) {
                continue;
            }
            const auto explicitLabel = readString(object, "Label");
            const std::string currentLabel = explicitLabel.value_or(std::string{});
            if (currentLabel.empty() || currentLabel == oldLabel) {
                continue;
            }
            const auto currentOwnersIt = labelOwners.find(currentLabel);
            if (currentOwnersIt == labelOwners.end() || currentOwnersIt->second.size() != 1U
                || currentOwnersIt->second.count(object.name) == 0U) {
                continue;
            }
            if (const auto oldOwnersIt = labelOwners.find(oldLabel);
                oldOwnersIt != labelOwners.end() && oldOwnersIt->second.count(object.name) == 0U) {
                continue;
            }

            std::string candidateSubname = originalSubname;
            candidateSubname.replace(token.start + 1U, token.size - 1U, currentLabel);
            const auto candidateTokens = subnameTokens(candidateSubname);
            const auto* resolved =
                resolveNestedLabelPrefix(document, link.object, candidateTokens, tokenIndex);
            if (resolved != nullptr && resolved->name == object.name) {
                candidates.push_back({object.name, currentLabel});
            }
        }
        if (candidates.size() == 1U) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
            // ::PropertyLinkBase::updateLabelReference() searches every "$Label." occurrence,
            // then accepts the replacement only when "parent->getSubObject(sub.c_str())"
            // resolves to the renamed object. cad-core mirrors that with document-only
            // Link/group prefix resolution before graph planning.
            const std::string newLabel = candidates.front().second;
            subnames.at(index).replace(token.start + 1U, token.size - 1U, newLabel);
            return LabelReferenceRename{index, oldLabel, newLabel, originalSubname, subnames.at(index)};
        }
        if (candidates.size() > 1U) {
            addLabelReferenceAmbiguousDiagnostic(diagnostics,
                                                 ownerObject,
                                                 propertyName,
                                                 link.object,
                                                 originalSubname,
                                                 "nested label reference matches multiple current objects");
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void normalizeLabelReferencesForLink(Link& link,
                                     const Document& document,
                                     const std::map<std::string, std::set<std::string>>& labelOwners,
                                     std::vector<runtime::Diagnostic>& diagnostics,
                                     const std::string& ownerObject,
                                     const std::string& propertyName)
{
    const auto targetIt = document.indexByName.find(link.object);
    if (targetIt == document.indexByName.end()) {
        return;
    }
    const auto& targetObject = document.objects.at(targetIt->second);
    const std::string targetLabel = objectLabelOrName(targetObject);
    const bool targetHasExplicitLabel = readString(targetObject, "Label").has_value();
    const bool targetLabelFallbackAllowed = targetHasExplicitLabel || targetObject.typeId != "App::Link";

    const bool stableDefaulted = !link.stableSubnamesExplicit;
    const bool fullDefaulted = !link.fullSubnamesExplicit;
    for (std::size_t index = 0; index < link.subnames.size(); ++index) {
        normalizeExternalDocumentPrefix(link.subnames, index, link);
        if (auto rename = renameNestedLabelReference(link.subnames,
                                                     index,
                                                     link,
                                                     document,
                                                     labelOwners,
                                                     &diagnostics,
                                                     ownerObject,
                                                     propertyName)) {
            link.labelReferenceRenames.push_back(std::move(*rename));
        }
        if (targetLabelFallbackAllowed) {
            if (auto rename = renameLeadingLabelReference(link.subnames,
                                                          index,
                                                          link.object,
                                                          targetLabel,
                                                          labelOwners,
                                                          &diagnostics,
                                                          ownerObject,
                                                          propertyName)) {
                link.labelReferenceRenames.push_back(std::move(*rename));
            }
        }
    }
    if (stableDefaulted) {
        link.stableSubnames = link.subnames;
    }
    else {
        for (std::size_t index = 0; index < link.stableSubnames.size(); ++index) {
            normalizeExternalDocumentPrefix(link.stableSubnames, index, link);
            (void)renameNestedLabelReference(link.stableSubnames, index, link, document, labelOwners);
            if (targetLabelFallbackAllowed) {
                (void)renameLeadingLabelReference(link.stableSubnames,
                                                  index,
                                                  link.object,
                                                  targetLabel,
                                                  labelOwners);
            }
        }
    }
    if (fullDefaulted) {
        link.fullSubnames = link.stableSubnames;
    }
    else {
        for (std::size_t index = 0; index < link.fullSubnames.size(); ++index) {
            normalizeExternalDocumentPrefix(link.fullSubnames, index, link);
            (void)renameNestedLabelReference(link.fullSubnames, index, link, document, labelOwners);
            if (targetLabelFallbackAllowed) {
                (void)renameLeadingLabelReference(link.fullSubnames,
                                                  index,
                                                  link.object,
                                                  targetLabel,
                                                  labelOwners);
            }
        }
    }
}

} // namespace

void normalizeSourceObjectRenameLinks(Document& document)
{
    std::map<long long, std::string> objectNameById;
    for (const auto& object : document.objects) {
        if (object.id != 0) {
            objectNameById[object.id] = object.name;
        }
    }

    for (auto& object : document.objects) {
        bool changed = false;
        for (auto& [propertyName, propertyValue] : object.propertyValues) {
            (void)propertyName;
            for (auto& link : propertyValue.links) {
                const auto resolved = renamedObjectTargetByReferenceShadow(link, document, objectNameById);
                if (!resolved) {
                    continue;
                }
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
                // ::GeoFeature::resolveElement() resolves the referenced subobject through the
                // current document object, and
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
                // ::PropertyLinkBase::_updateElementReference() then writes the updated element
                // reference. cad-core uses ReferenceShadow.targetId as request-side evidence that
                // the old link object name was renamed to the current unique object with the same ID.
                link.resolvedObjectFrom = link.object;
                link.object = *resolved;
                changed = true;
            }
        }
        if (changed) {
            rebuildDependencyLinks(object);
        }
    }
}

void normalizeLabelReferenceLinks(Document& document, std::vector<runtime::Diagnostic>& diagnostics)
{
    std::map<std::string, std::set<std::string>> labelOwners;
    for (const auto& object : document.objects) {
        const std::string label = objectLabelOrName(object);
        if (!label.empty()) {
            labelOwners[label].insert(object.name);
        }
    }

    for (auto& object : document.objects) {
        for (auto& [propertyName, propertyValue] : object.propertyValues) {
            for (auto& link : propertyValue.links) {
                normalizeLabelReferencesForLink(link,
                                                document,
                                                labelOwners,
                                                diagnostics,
                                                object.name,
                                                propertyName);
            }
        }
    }
}

bool isLinkPropertyType(const std::string& propertyType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h
    // ::PropertyXLink derives from PropertyLinkGlobal and is used by App::Link::LinkedObject
    // and Assembly::AssemblyLink::LinkedObject; XLink still contributes dependency edges.
    return propertyType == "App::PropertyLink" || propertyType == "App::PropertyLinkGlobal"
        || propertyType == "App::PropertyLinkHidden" || propertyType == "App::PropertyLinkList"
        || propertyType == "App::PropertyLinkListHidden" || propertyType == "App::PropertyLinkSub"
        || propertyType == "App::PropertyLinkSubHidden" || propertyType == "App::PropertyLinkSubList"
        || propertyType == "App::PropertyLinkSubListHidden" || propertyType == "App::PropertyXLink"
        || propertyType == "App::PropertyXLinkList" || propertyType == "App::PropertyXLinkSub"
        || propertyType == "App::PropertyXLinkSubHidden" || propertyType == "App::PropertyXLinkSubList";
}

bool isLinkObjectType(const std::string& propertyType)
{
    return propertyType == "App::PropertyLink" || propertyType == "App::PropertyLinkGlobal"
        || propertyType == "App::PropertyLinkHidden" || propertyType == "App::PropertyLinkSub"
        || propertyType == "App::PropertyLinkSubHidden" || propertyType == "App::PropertyXLink"
        || propertyType == "App::PropertyXLinkSub" || propertyType == "App::PropertyXLinkSubHidden";
}

bool isLinkSubObjectType(const std::string& propertyType)
{
    return propertyType == "App::PropertyLinkSub" || propertyType == "App::PropertyLinkSubHidden"
        || propertyType == "App::PropertyXLinkSub" || propertyType == "App::PropertyXLinkSubHidden";
}

bool isLinkListType(const std::string& propertyType)
{
    return propertyType == "App::PropertyLinkList" || propertyType == "App::PropertyLinkListHidden"
        || propertyType == "App::PropertyXLinkList";
}

bool isXLinkListType(const std::string& propertyType)
{
    return propertyType == "App::PropertyXLinkList";
}

bool isLinkSubListType(const std::string& propertyType)
{
    return propertyType == "App::PropertyLinkSubList" || propertyType == "App::PropertyLinkSubListHidden"
        || propertyType == "App::PropertyXLinkSubList";
}

bool isHiddenLinkPropertyType(const std::string& propertyType)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h
    // ::PropertyLinkHidden, ::PropertyLinkListHidden, ::PropertyLinkSubHidden,
    // ::PropertyLinkSubListHidden and ::PropertyXLinkSubHidden set "_pcScope = LinkScope::Hidden".
    // cad-core still parses them for feature executors, but graph dependency planning must not
    // treat them as dependency-bearing properties.
    return propertyType == "App::PropertyLinkHidden" || propertyType == "App::PropertyLinkListHidden"
        || propertyType == "App::PropertyLinkSubHidden" || propertyType == "App::PropertyLinkSubListHidden"
        || propertyType == "App::PropertyXLinkSubHidden";
}

bool isMalformedLinkValue(const nlohmann::json& value, const std::string& propertyType)
{
    auto hasUnsupportedSubnameFields = [](const nlohmann::json& item) {
        return item.contains("StableSubnames") || item.contains("FullSubnames");
    };
    auto hasReferenceRecoveryFields = [](const nlohmann::json& item) {
        return item.contains("ShadowSub") || item.contains("ReferenceShadow");
    };
    auto readFieldSize = [](const nlohmann::json& item,
                            const std::string& field,
                            std::optional<std::size_t>& size) {
        const auto it = item.find(field);
        if (it == item.end()) {
            return true;
        }
        const auto values = readStringList(*it);
        if (!values) {
            return false;
        }
        size = values->size();
        return true;
    };
    auto alignRecoverySize = [](std::optional<std::size_t>& recoverySize, std::size_t count) {
        if (recoverySize && *recoverySize != count) {
            return false;
        }
        recoverySize = count;
        return true;
    };
    auto hasValidSubnameFields = [&](const nlohmann::json& item) {
        if (hasUnsupportedSubnameFields(item)) {
            return false;
        }
        std::optional<std::size_t> subListSize;
        if (!readFieldSize(item, "SubList", subListSize)) {
            return false;
        }
        const bool hasCurrentSubnames = subListSize && *subListSize > 0U;

        std::optional<std::size_t> stableSubListSize;
        if (!readFieldSize(item, "StableSubList", stableSubListSize)) {
            return false;
        }
        if (hasCurrentSubnames && stableSubListSize && *stableSubListSize != *subListSize) {
            return false;
        }
        std::optional<std::size_t> fullSubListSize;
        if (!readFieldSize(item, "FullSubList", fullSubListSize)) {
            return false;
        }
        if (fullSubListSize && (!hasCurrentSubnames || *fullSubListSize != *subListSize)) {
            return false;
        }

        std::optional<std::size_t> recoverySize;
        const auto shadowSubIt = item.find("ShadowSub");
        if (shadowSubIt != item.end()) {
            const auto shadowSubs = readShadowSubList(*shadowSubIt);
            if (!shadowSubs) {
                return false;
            }
            if (hasCurrentSubnames && shadowSubs->size() != *subListSize) {
                return false;
            }
            if (!hasCurrentSubnames && !alignRecoverySize(recoverySize, shadowSubs->size())) {
                return false;
            }
        }

        const auto referenceShadowIt = item.find("ReferenceShadow");
        if (referenceShadowIt != item.end()) {
            const auto referenceShadows = readReferenceShadowList(*referenceShadowIt);
            if (!referenceShadows) {
                return false;
            }
            if (hasCurrentSubnames && referenceShadows->size() != *subListSize) {
                return false;
            }
            if (!hasCurrentSubnames && !alignRecoverySize(recoverySize, referenceShadows->size())) {
                return false;
            }
        }

        if (!hasCurrentSubnames && recoverySize) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
            // ::PropertyLinkBase::_updateElementReference() can recover a missing current
            // subname from ShadowSub before consulting the old geometry cache. cad-core accepts
            // this evidence-only request shape only when the stable name, ShadowSub pair, and
            // ReferenceShadow entry stay index-aligned.
            return item.contains("StableSubList") && item.contains("ShadowSub")
                && item.contains("ReferenceShadow") && stableSubListSize
                && *stableSubListSize == *recoverySize && *recoverySize > 0U;
        }
        if (!hasCurrentSubnames && stableSubListSize) {
            return false;
        }
        return true;
    };

    if (isLinkObjectType(propertyType)) {
        const auto objectIt = value.find("value");
        if (objectIt == value.end() || (!objectIt->is_string() && !objectIt->is_null())) {
            return true;
        }
        const bool supportsSubList = isLinkSubObjectType(propertyType) || propertyType == "App::PropertyXLink";
        if (supportsSubList) {
            if (value.contains("values") || value.contains("SubSet") || !hasValidSubnameFields(value)) {
                return true;
            }
        }
        if (!supportsSubList
            && (value.contains("SubList") || value.contains("SubSet") || value.contains("values")
                || value.contains("StableSubList") || value.contains("FullSubList")
                || hasReferenceRecoveryFields(value)
                || hasUnsupportedSubnameFields(value))) {
            return true;
        }
        return false;
    }

    if (isLinkListType(propertyType)) {
        if (isXLinkListType(propertyType)) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
            // ::PropertyXLinkList::setPyObject() first tries "PropertyLinkList syntax", then
            // falls back to "PropertyXLinkSubList::setPyObject(value)" for sub-element entries.
            if (value.contains("value") || value.contains("SubList") || value.contains("StableSubList")
                || value.contains("FullSubList") || hasReferenceRecoveryFields(value)
                || hasUnsupportedSubnameFields(value)) {
                return true;
            }
            const bool hasValues = value.contains("values");
            const bool hasSubSet = value.contains("SubSet");
            if (hasValues == hasSubSet) {
                return true;
            }
            if (hasValues) {
                const auto rawLinksIt = value.find("values");
                return rawLinksIt == value.end() || !readStringList(*rawLinksIt).has_value();
            }
            const auto rawLinksIt = value.find("SubSet");
            if (rawLinksIt == value.end() || !rawLinksIt->is_array()) {
                return true;
            }
            for (const auto& item : *rawLinksIt) {
                if (!item.is_object() || item.contains("PropertyType")) {
                    return true;
                }
                const auto objectIt = item.find("value");
                if (objectIt == item.end() || (!objectIt->is_string() && !objectIt->is_null())) {
                    return true;
                }
                if (item.contains("values") || item.contains("SubSet")) {
                    return true;
                }
                if (!hasValidSubnameFields(item)) {
                    return true;
                }
            }
            return false;
        }
        if (value.contains("value") || value.contains("SubList") || value.contains("SubSet")
            || value.contains("StableSubList") || value.contains("FullSubList")
            || hasReferenceRecoveryFields(value)
            || hasUnsupportedSubnameFields(value)) {
            return true;
        }
        const auto rawLinksIt = value.find("values");
        if (rawLinksIt == value.end()) {
            return true;
        }
        return !readStringList(*rawLinksIt).has_value();
    }

    if (isLinkSubListType(propertyType)) {
        if (value.contains("value") || value.contains("values") || value.contains("SubList")
            || value.contains("StableSubList") || value.contains("FullSubList")
            || hasReferenceRecoveryFields(value)
            || hasUnsupportedSubnameFields(value)) {
            return true;
        }
        const auto rawLinksIt = value.find("SubSet");
        if (rawLinksIt == value.end() || !rawLinksIt->is_array()) {
            return true;
        }
        for (const auto& item : *rawLinksIt) {
            if (!item.is_object() || item.contains("PropertyType")) {
                return true;
            }
            const auto objectIt = item.find("value");
            if (objectIt == item.end() || (!objectIt->is_string() && !objectIt->is_null())) {
                return true;
            }
            if (item.contains("values") || item.contains("SubSet")) {
                return true;
            }
            if (!hasValidSubnameFields(item)) {
                return true;
            }
        }
        return false;
    }

    return false;
}

bool isLink(const nlohmann::json& value)
{
    return readLinkObject(value).has_value();
}

std::vector<Link> readLinks(const nlohmann::json& value)
{
    std::vector<Link> links;
    const auto propertyType = propertyTypeOf(value);
    if (!propertyType) {
        return links;
    }

    if (isLinkObjectType(*propertyType)) {
        auto link = readLinkObject(value);
        if (link) {
            links.push_back(std::move(*link));
        }
        return links;
    }

    if (isLinkListType(*propertyType)) {
        if (isXLinkListType(*propertyType) && value.contains("SubSet")) {
            return readLinkSubList(value);
        }
        return readLinkList(value);
    }

    if (isLinkSubListType(*propertyType)) {
        return readLinkSubList(value);
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

} // namespace cad_core::app
