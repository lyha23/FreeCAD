#include "cad_core/document/model.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <utility>

namespace cad_core::document {

using runtime::addDiagnostic;

namespace {

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

PropertyKind kindFromPropertyType(const std::string& propertyType)
{
    if (propertyType == "App::PropertyBool") {
        return PropertyKind::Bool;
    }
    if (propertyType == "App::PropertyInteger") {
        return PropertyKind::Integer;
    }
    if (propertyType == "App::PropertyFloat" || propertyType == "App::PropertyLength" || propertyType == "App::PropertyAngle"
        || propertyType == "App::PropertyDistance") {
        return PropertyKind::Float;
    }
    if (propertyType == "App::PropertyString") {
        return PropertyKind::String;
    }
    if (propertyType == "App::PropertyEnumeration") {
        return PropertyKind::Enumeration;
    }
    if (propertyType == "App::PropertyVector" || propertyType == "App::PropertyVectorDistance") {
        return PropertyKind::Vector;
    }
    if (propertyType == "App::PropertyPlacement") {
        return PropertyKind::Placement;
    }
    if (propertyType == "App::PropertyLink" || propertyType == "App::PropertyLinkGlobal"
        || propertyType == "App::PropertyLinkHidden" || propertyType == "App::PropertyXLink") {
        return PropertyKind::Link;
    }
    if (propertyType == "App::PropertyLinkList" || propertyType == "App::PropertyLinkListHidden"
        || propertyType == "App::PropertyXLinkList") {
        return PropertyKind::LinkList;
    }
    if (propertyType == "App::PropertyLinkSub" || propertyType == "App::PropertyLinkSubHidden"
        || propertyType == "App::PropertyXLinkSub" || propertyType == "App::PropertyXLinkSubHidden") {
        return PropertyKind::LinkSub;
    }
    if (propertyType == "App::PropertyLinkSubList" || propertyType == "App::PropertyLinkSubListHidden"
        || propertyType == "App::PropertyXLinkSubList") {
        return PropertyKind::LinkSubList;
    }
    return PropertyKind::Unknown;
}

PropertyKind inferUntypedKind(const nlohmann::json& value)
{
    if (value.is_boolean()) {
        return PropertyKind::Bool;
    }
    if (value.is_number_integer()) {
        return PropertyKind::Integer;
    }
    if (value.is_number()) {
        return PropertyKind::Float;
    }
    if (value.is_string()) {
        return PropertyKind::String;
    }
    if (value.is_array() && value.size() == 3U) {
        const bool vectorLike = std::all_of(value.begin(), value.end(), [](const nlohmann::json& item) {
            return item.is_number();
        });
        if (vectorLike) {
            return PropertyKind::Vector;
        }
    }
    return PropertyKind::Unknown;
}

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

const nlohmann::json& propertyPayload(const nlohmann::json& value)
{
    if (value.is_object() && value.contains("PropertyType") && value.contains("value")) {
        return value.at("value");
    }
    return value;
}

bool isFiniteNumber(double value)
{
    return std::isfinite(value);
}

bool isNumberValue(const nlohmann::json& value)
{
    return value.is_number() && isFiniteNumber(value.get<double>());
}

bool isValidVector3Value(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() != 3U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const nlohmann::json& item) {
        return isNumberValue(item);
    });
}

bool isValidPlacementValue(const nlohmann::json& value)
{
    if (!value.is_object()) {
        return false;
    }
    const auto baseIt = value.find("Base");
    const auto rotationIt = value.find("Rotation");
    if (baseIt == value.end() || rotationIt == value.end() || !isValidVector3Value(*baseIt)
        || !rotationIt->is_array() || rotationIt->size() != 4U) {
        return false;
    }

    double normSquared = 0.0;
    for (const auto& item : *rotationIt) {
        if (!isNumberValue(item)) {
            return false;
        }
        const double component = item.get<double>();
        normSquared += component * component;
    }
    return normSquared > 0.0;
}

bool isGeoFeatureGroupType(const std::string& typeId)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // calls GroupExtension::initExtension(this), and
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BodyBase.cpp::BodyBase::BodyBase()
    // calls App::OriginGroupExtension::initExtension(this).
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.h
    // ::AssemblyObject derives from App::Part, and
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyLink.h
    // ::AssemblyLink derives from App::Part.
    return typeId == "App::Part" || typeId == "PartDesign::Body"
        || typeId == "Assembly::AssemblyObject" || typeId == "Assembly::AssemblyLink";
}

bool isOwnedLinkElement(const DocumentObject& element, const DocumentObject& owner)
{
    if (element.typeId != "App::LinkElement") {
        return false;
    }
    const auto ownerValue = readNumber(element, "_LinkOwner");
    return !ownerValue || static_cast<long long>(*ownerValue) == 0 || static_cast<long long>(*ownerValue) == owner.id;
}

void addMaterializedLinkElementDependencies(Document& document)
{
    for (auto& object : document.objects) {
        if (object.typeId != "App::Link" || !readLinks(object, "ElementList").empty()) {
            continue;
        }
        const std::size_t elementCount = static_cast<std::size_t>(std::max(0.0, readNumber(object, "ElementCount").value_or(0.0)));
        if (elementCount == 0U || !readBool(object, "ShowElement").value_or(true)) {
            continue;
        }
        const auto ownerLinkedObject = readLink(object, "LinkedObject");

        for (std::size_t index = 0; index < elementCount; ++index) {
            const std::string elementName = object.name + "_i" + std::to_string(index);
            const auto elementIt = document.indexByName.find(elementName);
            if (elementIt == document.indexByName.end()) {
                continue;
            }
            auto& element = document.objects.at(elementIt->second);
            if (!isOwnedLinkElement(element, object)) {
                continue;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
            // ::LinkBaseExtension::update(), when ShowElement is true, creates or re-claims
            // child LinkElement objects named owner "_i" index; cad-core keeps the graph
            // immutable, but still makes those materialized elements dependency-bearing.
            object.dependencyLinks.push_back(Link{elementName, {}, {}, {}, "ElementList"});
            if (!readLink(element, "LinkedObject") && ownerLinkedObject) {
                // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
                // ::LinkBaseExtension::updateGroup(), for owned LinkElement children, copies
                // parent LinkedObject subvalues into "element->LinkedObject" and syncs transform.
                // cad-core keeps the request graph immutable, but the child still depends on the
                // inherited target so recompute order matches the FreeCAD-synchronized state.
                element.dependencyLinks.push_back(*ownerLinkedObject);
            }
        }
    }
}

bool hasInvalidTypedPayload(const nlohmann::json& raw, PropertyKind kind)
{
    if (!raw.is_object() || !raw.contains("value")) {
        return false;
    }

    const nlohmann::json& payload = propertyPayload(raw);
    switch (kind) {
        case PropertyKind::Bool:
            return !payload.is_boolean();
        case PropertyKind::Integer:
            return !payload.is_number_integer();
        case PropertyKind::Float:
            return !isNumberValue(payload);
        case PropertyKind::String:
            return !payload.is_string();
        case PropertyKind::Enumeration:
            return !payload.is_string() && !payload.is_number_integer();
        case PropertyKind::Vector:
            return !isValidVector3Value(payload);
        default:
            return false;
    }
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

PropertyValue parsePropertyValue(const std::string& objectName,
                                 const std::string& propertyName,
                                 const nlohmann::json& raw,
                                 std::vector<runtime::Diagnostic>& diagnostics)
{
    PropertyValue property;
    property.name = propertyName;
    property.raw = raw;

    const auto propertyType = propertyTypeOf(raw);
    if (!propertyType) {
        property.kind = inferUntypedKind(raw);
        collectLinks(raw, property.links);
        for (auto& link : property.links) {
            if (link.property.empty()) {
                link.property = propertyName;
            }
        }
        return property;
    }

    property.propertyType = *propertyType;
    property.kind = kindFromPropertyType(*propertyType);

    if (property.kind == PropertyKind::Placement && !isValidPlacementValue(raw)) {
        property.valid = false;
        addDiagnostic(diagnostics,
                      "error",
                      "invalid_placement",
                      "Property " + propertyName + " has an invalid App::PropertyPlacement value",
                      objectName,
                      propertyName,
                      "parse");
    }
    else if (hasInvalidTypedPayload(raw, property.kind)) {
        property.valid = false;
        addDiagnostic(diagnostics,
                      "error",
                      "invalid_property_type",
                      "Property " + propertyName + " has an invalid " + *propertyType + " value",
                      objectName,
                      propertyName,
                      "parse");
    }

    if (isLinkPropertyType(*propertyType)) {
        property.links = readLinks(raw);
        for (auto& link : property.links) {
            if (link.property.empty()) {
                link.property = propertyName;
            }
        }
        if (isMalformedLinkValue(raw, *propertyType)) {
            property.valid = false;
            addDiagnostic(diagnostics,
                          "error",
                          "invalid_link_value",
                          "Property " + propertyName + " has an invalid " + *propertyType + " value",
                          objectName,
                          propertyName,
                          "parse");
        }
    }

    return property;
}

}  // namespace

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

const PropertyValue* propertyValue(const DocumentObject& object, const std::string& property)
{
    const auto it = object.propertyValues.find(property);
    if (it == object.propertyValues.end()) {
        return nullptr;
    }
    return &it->second;
}

bool hasPropertyType(const DocumentObject& object, const std::string& property, const std::string& propertyType)
{
    const auto* value = propertyValue(object, property);
    return value != nullptr && value->propertyType == propertyType;
}

std::vector<Link> readLinks(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return {};
    }
    return value->links;
}

std::optional<Link> readLink(const DocumentObject& object, const std::string& property)
{
    const auto links = readLinks(object, property);
    if (links.size() != 1U) {
        return std::nullopt;
    }
    return links.front();
}

std::optional<bool> readBool(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!payload.is_boolean()) {
        return std::nullopt;
    }
    return payload.get<bool>();
}

std::optional<double> readNumber(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!isNumberValue(payload)) {
        return std::nullopt;
    }
    return payload.get<double>();
}

std::optional<std::string> readString(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!payload.is_string()) {
        return std::nullopt;
    }
    return payload.get<std::string>();
}

std::optional<std::array<double, 3>> readVector3(const DocumentObject& object, const std::string& property)
{
    const auto* value = propertyValue(object, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    const nlohmann::json& payload = propertyPayload(value->raw);
    if (!isValidVector3Value(payload)) {
        return std::nullopt;
    }
    return std::array<double, 3>{payload.at(0).get<double>(), payload.at(1).get<double>(), payload.at(2).get<double>()};
}

std::optional<Placement> readPlacement(const DocumentObject& object, const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::GeoFeature(),
    // declares "ADD_PROPERTY_TYPE(Placement, (Base::Placement()), ... App::Prop_None, \"\")";
    // cad-core keeps Placement normalized in document and lets geometry convert it to gp_Trsf.
    const auto* value = propertyValue(object, property);
    if (value == nullptr || value->kind != PropertyKind::Placement || !isValidPlacementValue(value->raw)) {
        return std::nullopt;
    }

    const auto& base = value->raw.at("Base");
    const auto& rotation = value->raw.at("Rotation");
    return Placement{
        std::array<double, 3>{base.at(0).get<double>(), base.at(1).get<double>(), base.at(2).get<double>()},
        std::array<double, 4>{rotation.at(0).get<double>(),
                              rotation.at(1).get<double>(),
                              rotation.at(2).get<double>(),
                              rotation.at(3).get<double>()},
    };
}

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw)
{
    Document document;
    std::vector<runtime::Diagnostic> diagnostics;

    if (!raw.is_object()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document root must be a JSON object", {}, {}, "parse");
        return {document, diagnostics};
    }

    const auto objectsIt = raw.find("Objects");
    if (objectsIt == raw.end() || !objectsIt->is_array()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document field 'Objects' must be a list", {}, {}, "parse");
        return {document, diagnostics};
    }

    std::set<std::string> seenNames;
    std::set<long long> seenIds;
    for (std::size_t index = 0; index < objectsIt->size(); ++index) {
        const auto& item = objectsIt->at(index);
        if (!item.is_object()) {
            addDiagnostic(diagnostics, "error", "parse_error", "Objects[" + std::to_string(index) + "] must be an object", {}, {}, "parse");
            continue;
        }
        if (!item.contains("Name") || !item.at("Name").is_string() || item.at("Name").get<std::string>().empty()) {
            addDiagnostic(diagnostics,
                          "error",
                          "missing_property",
                          "Objects[" + std::to_string(index) + "] is missing required field Name",
                          {},
                          {},
                          "parse");
            continue;
        }

        std::string name = item.at("Name").get<std::string>();
        if (seenNames.count(name) != 0U) {
            addDiagnostic(diagnostics, "error", "duplicate_object_name", "Duplicate object name " + name, name, {}, "parse");
            continue;
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
            continue;
        }
        const long long id = item.at("ID").get<long long>();
        if (seenIds.count(id) != 0U) {
            addDiagnostic(diagnostics, "error", "duplicate_object_id", "Duplicate object ID " + std::to_string(id), name, "ID", "parse");
            continue;
        }
        seenIds.insert(id);

        if (!item.contains("TypeId") || !item.at("TypeId").is_string() || item.at("TypeId").get<std::string>().empty()) {
            addDiagnostic(diagnostics, "error", "missing_property", "Object " + name + " is missing required field TypeId", name, {}, "parse");
            continue;
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
            continue;
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
                object.dependencyLinks.insert(object.dependencyLinks.end(), parsed.links.begin(), parsed.links.end());
            }
            if (!parsed.valid) {
                object.invalidProperties.insert(property.key());
            }
            object.propertyValues.emplace(property.key(), std::move(parsed));
        }

        document.indexByName[name] = document.objects.size();
        document.objects.push_back(std::move(object));
    }

    for (const auto& object : document.objects) {
        if (!isGeoFeatureGroupType(object.typeId)) {
            continue;
        }

        const auto groupLinks = readLinks(object, "Group");
        for (const auto& link : groupLinks) {
            if (link.object.empty() || document.indexByName.count(link.object) == 0U) {
                continue;
            }
            document.parentGroupByObject.emplace(link.object, object.name);
        }
    }
    addMaterializedLinkElementDependencies(document);

    if (raw.contains("recompute") && raw.at("recompute").is_object() && raw.at("recompute").contains("objs")) {
        const auto& rawTargets = raw.at("recompute").at("objs");
        if (!rawTargets.is_array()) {
            addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must be a list of object names", {}, {}, "parse");
        }
        else {
            for (const auto& target : rawTargets) {
                if (!target.is_string()) {
                    addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must contain object names", {}, {}, "parse");
                    continue;
                }
                document.targets.push_back(target.get<std::string>());
            }
        }
    }
    if (document.targets.empty() && !document.objects.empty()) {
        document.targets.push_back(document.objects.back().name);
    }

    return {document, diagnostics};
}

}  // namespace cad_core::document
