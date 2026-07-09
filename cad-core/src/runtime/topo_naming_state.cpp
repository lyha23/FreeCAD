#include "cad_core/runtime/topo_naming_state.h"

#include "cad_core/app/property_links.h"
#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/topo/freecad_mapped_name_codec.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::runtime {

namespace {

constexpr const char* topoStateSchemaVersion = "cad-core.topo-state.v1";
constexpr const char* elementMapVersion = "cad-core.element-map.v1";

std::string sha256Json(const nlohmann::json& value)
{
    return "sha256:" + part::sha256Hex(value.dump());
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

nlohmann::json producerJson(const app::Document& document)
{
    if (document.topoNamingState.is_object()) {
        const auto producerIt = document.topoNamingState.find("producer");
        if (producerIt != document.topoNamingState.end() && producerIt->is_object()) {
            return *producerIt;
        }
    }
    return {
        {"cadCoreVersion", "cad-core-runtime-v1"},
        {"freecadVersion", "cad-core-runtime"},
        {"occtVersion", part::kernelVersion()},
    };
}

nlohmann::json documentHashProjection(const app::Document& document)
{
    nlohmann::json objects = nlohmann::json::array();
    for (const app::DocumentObject& object : document.objects) {
        objects.push_back({
            {"Name", object.name},
            {"ID", object.id},
            {"TypeId", object.typeId},
            {"Properties", object.properties},
        });
    }

    nlohmann::json projection = {
        {"Objects", objects},
        {"recompute", {{"objs", document.targets}}},
    };
    if (document.displayMeshDeflection) {
        projection["displayMeshDeflection"] = *document.displayMeshDeflection;
    }
    return projection;
}

nlohmann::json documentObjectHashProjection(const app::DocumentObject& object)
{
    return {
        {"Name", object.name},
        {"ID", object.id},
        {"TypeId", object.typeId},
        {"Properties", object.properties},
    };
}

bool producerIsCompatible(const nlohmann::json& producer)
{
    if (!producer.is_object()) {
        return false;
    }
    const std::string cadCoreVersion = producer.value("cadCoreVersion", "");
    return cadCoreVersion == "fixture-contract-v1" || cadCoreVersion == "cad-core-runtime-v1";
}

bool explicitMismatchHash(const std::string& hash)
{
    constexpr const char* prefix = "sha256:";
    if (hash.rfind(prefix, 0) != 0 || hash.size() != std::string(prefix).size() + 64U) {
        return true;
    }
    const std::string digest = hash.substr(std::string(prefix).size());
    return std::all_of(digest.begin(), digest.end(), [](char value) { return value == '0'; })
        || std::all_of(digest.begin(), digest.end(), [](char value) { return value == '1'; });
}

bool shouldHardFailHashMismatch(const std::string& actualHash,
                                const std::string& expectedHash)
{
    return actualHash != expectedHash && explicitMismatchHash(actualHash);
}

nlohmann::json hardFailPayload(std::vector<Diagnostic> diagnostics)
{
    return {
        {"results", nlohmann::json::array()},
        {"elementReferenceUpdates", nlohmann::json::array()},
        {"diagnostics", diagnosticsToJson(diagnostics)},
    };
}

std::optional<Diagnostic> validateElementMapEncoding(const nlohmann::json& objectState,
                                                     const std::string& objectName)
{
    const auto elementMapIt = objectState.find("elementMap");
    if (elementMapIt == objectState.end() || !elementMapIt->is_object()) {
        return std::nullopt;
    }
    const std::string encoding = elementMapIt->value("encoding", "");
    if (encoding != elementMapVersion) {
        return Diagnostic {
            "error",
            "topo_state_element_map_encoding_incompatible",
            "topoNamingState elementMap encoding is incompatible; request-level recompute is refused",
            objectName,
            {},
            "",
            "",
            "",
            nlohmann::json {
                {"source", "topoNamingState"},
                {"actualEncoding", encoding},
                {"expectedEncoding", elementMapVersion},
            },
        };
    }
    return std::nullopt;
}

std::optional<Diagnostic> validateChildElementMapEncoding(const nlohmann::json& objectState,
                                                          const std::string& objectName)
{
    const auto childMapsIt = objectState.find("childElementMaps");
    if (childMapsIt == objectState.end() || !childMapsIt->is_array()) {
        return std::nullopt;
    }
    for (const nlohmann::json& childMap : *childMapsIt) {
        if (!childMap.is_object()) {
            continue;
        }
        const auto elementMapIt = childMap.find("elementMap");
        if (elementMapIt == childMap.end() || !elementMapIt->is_object()) {
            continue;
        }
        const std::string encoding = elementMapIt->value("encoding", "");
        if (encoding != elementMapVersion) {
            return Diagnostic {
                "error",
                "topo_state_element_map_encoding_incompatible",
                "topoNamingState childElementMaps elementMap encoding is incompatible; request-level recompute is refused",
                objectName,
                {},
                "",
                "",
                "",
                nlohmann::json {
                    {"source", "topoNamingState"},
                    {"actualEncoding", encoding},
                    {"expectedEncoding", elementMapVersion},
                    {"childElementMapKey", childMap.value("key", "")},
                },
            };
        }
    }
    return std::nullopt;
}

nlohmann::json fallbackObjectHashProjection(const std::string& objectName,
                                            const part::NamedShape* namedShape)
{
    nlohmann::json projection = {{"Name", objectName}};
    if (namedShape != nullptr) {
        nlohmann::json namedShapeProjection = part::namedShapeToJson(*namedShape);
        // mapped_name_provenance can include request-local producer tags before S4 closes.
        // Keep fallback object hashes stable across CLI/C API/worker/wasm channels.
        namedShapeProjection.erase("mapped_name_provenance");
        projection["namedShape"] = std::move(namedShapeProjection);
    }
    return projection;
}

struct ResponseSubshapeInfo {
    std::string indexed;
    std::string subname;
    std::string shapeKind;
    nlohmann::json stateSubshape = nlohmann::json::object();
};

struct ChildPathProjection {
    std::string childObject;
    std::string pathPrefix;
    std::string localSubname;
    std::size_t childIndex = 0U;
    std::string targetSubname;
    std::string shapeKind;
    std::string rawChildMappedName;
    std::string canonicalChildMappedName;
    std::string rawOwnerMappedName;
    std::string canonicalOwnerMappedName;
};

void copyStringIfPresent(nlohmann::json& target,
                         const nlohmann::json& source,
                         const std::string& field)
{
    const auto fieldIt = source.find(field);
    if (fieldIt != source.end() && fieldIt->is_string()) {
        target[field] = fieldIt->get<std::string>();
    }
}

nlohmann::json stateSubshapeFromResponse(const nlohmann::json& subshape)
{
    const std::string indexed = subshape.value("indexed", "");
    const std::string subname = subshape.value("subname", indexed);
    nlohmann::json result = {
        {"subname", subname},
        {"resolvedIndexed", indexed},
        {"identityStatus", subshape.value("identityStatus", "current_only")},
    };
    copyStringIfPresent(result, subshape, "stableSubname");
    copyStringIfPresent(result, subshape, "sourceStableSubname");
    copyStringIfPresent(result, subshape, "fragmentStableSubname");
    copyStringIfPresent(result, subshape, "fullSubname");
    return result;
}

std::map<std::string, ResponseSubshapeInfo> responseSubshapeInfoByIndexed(
    const nlohmann::json& responseSubshapes
)
{
    std::map<std::string, ResponseSubshapeInfo> result;
    if (!responseSubshapes.is_array()) {
        return result;
    }
    for (const nlohmann::json& subshape : responseSubshapes) {
        if (!subshape.is_object()) {
            continue;
        }
        const std::string indexed = subshape.value("indexed", "");
        if (indexed.empty()) {
            continue;
        }
        const std::string subname = subshape.value("subname", indexed);
        result[indexed] = ResponseSubshapeInfo {
            indexed,
            subname,
            lowerAscii(subshape.value("kind", "shape")),
            stateSubshapeFromResponse(subshape),
        };
    }
    return result;
}

std::map<std::string, ResponseSubshapeInfo> namedShapeSubshapeInfoByIndexed(
    const part::NamedShape& namedShape
)
{
    std::map<std::string, ResponseSubshapeInfo> result;
    for (const auto& [indexed, element] : namedShape.elements) {
        nlohmann::json stateSubshape = {
            {"subname", indexed},
            {"resolvedIndexed", indexed},
            {"identityStatus", "indexed_only"},
        };
        result[indexed] = ResponseSubshapeInfo {
            indexed,
            indexed,
            part::subshapeKindName(element.subshape.kind),
            std::move(stateSubshape),
        };
    }
    return result;
}

nlohmann::json subshapesStateJson(const std::map<std::string, ResponseSubshapeInfo>& subshapes)
{
    nlohmann::json result = nlohmann::json::object();
    for (const auto& [indexed, subshape] : subshapes) {
        result[indexed] = subshape.stateSubshape;
    }
    return result;
}

part::MapperHistoryEndpoint endpointFromElementName(const std::string& fallbackObject,
                                                    const std::string& elementName)
{
    const std::size_t dot = elementName.rfind('.');
    if (dot == std::string::npos || dot == 0U || dot + 1U >= elementName.size()) {
        return {fallbackObject, elementName};
    }
    return {elementName.substr(0, dot), elementName.substr(dot + 1U)};
}

part::MapperHistoryEndpoint endpointFromProvenance(const std::string& fallbackObject,
                                                   const part::MappedNameProvenance& provenance,
                                                   const std::string& fallbackElementName)
{
    if (!provenance.sourceElement.empty()) {
        return endpointFromElementName(fallbackObject, provenance.sourceElement);
    }
    return endpointFromElementName(fallbackObject, fallbackElementName);
}

bool mapperEndpointMatches(const nlohmann::json& endpoint,
                           const part::MapperHistoryEndpoint& expected)
{
    return endpoint.is_object()
        && endpoint.value("object", "") == expected.object
        && endpoint.value("subname", "") == expected.subname;
}

nlohmann::json matchingMapperHistoryIds(const nlohmann::json& mapperHistory,
                                        const part::MapperHistoryEndpoint& source,
                                        const part::MapperHistoryEndpoint& target)
{
    nlohmann::json ids = nlohmann::json::array();
    if (!mapperHistory.is_array()) {
        return ids;
    }
    for (const nlohmann::json& event : mapperHistory) {
        if (!event.is_object()) {
            continue;
        }
        const auto sourceIt = event.find("source");
        const auto targetIt = event.find("target");
        if (sourceIt == event.end() || targetIt == event.end()) {
            continue;
        }
        if (!mapperEndpointMatches(*sourceIt, source) || !mapperEndpointMatches(*targetIt, target)) {
            continue;
        }
        const auto idIt = event.find("id");
        if (idIt != event.end() && idIt->is_string()) {
            ids.push_back(idIt->get<std::string>());
        }
    }
    return ids;
}

std::string recoverabilityForEntry(const nlohmann::json& mapperHistory,
                                   const part::MapperHistoryEndpoint& source,
                                   const part::MapperHistoryEndpoint& target)
{
    if (!mapperHistory.is_array()) {
        return "resolved";
    }
    for (const nlohmann::json& event : mapperHistory) {
        if (!event.is_object()) {
            continue;
        }
        const auto sourceIt = event.find("source");
        const auto targetIt = event.find("target");
        if (sourceIt == event.end() || targetIt == event.end()) {
            continue;
        }
        if (mapperEndpointMatches(*sourceIt, source) && mapperEndpointMatches(*targetIt, target)) {
            return event.value("recoverability", "resolved");
        }
    }
    return "resolved";
}

bool indexedOnlyAlias(const std::string& objectName,
                      const std::string& stableName,
                      const std::string& currentName)
{
    return stableName == currentName || stableName == objectName + "." + currentName;
}

bool hasFreeCadEncodedElementToken(const std::string& rawMappedName)
{
    const std::size_t postfix = rawMappedName.find(';');
    const std::string data = rawMappedName.substr(0, postfix);
    return data.find('#') != std::string::npos;
}

bool requestTopoStateHasObject(const app::Document& document, const std::string& objectName)
{
    if (objectName.empty() || !document.topoNamingState.is_object()) {
        return false;
    }
    const auto objectsIt = document.topoNamingState.find("objects");
    return objectsIt != document.topoNamingState.end()
        && objectsIt->is_object()
        && objectsIt->find(objectName) != objectsIt->end();
}

bool linkHasFreeCadMappedStableSubname(const app::Link& link)
{
    return std::any_of(link.stableSubnames.begin(),
                       link.stableSubnames.end(),
                       [](const std::string& stableSubname) {
                           return hasFreeCadEncodedElementToken(stableSubname);
                       });
}

std::optional<std::string> linkedTopoStateOwner(const app::DocumentObject& object,
                                                const app::Document& document)
{
    if (object.typeId != "App::Link") {
        return std::nullopt;
    }
    const auto linkedObject = app::readLink(object, "LinkedObject");
    if (!linkedObject || linkedObject->object.empty()
        || linkedObject->stableSubnamesSource != "topoNamingState"
        || !linkHasFreeCadMappedStableSubname(*linkedObject)
        || !requestTopoStateHasObject(document, linkedObject->object)) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp::Link::getSubObject(),
    // "LinkedObject" delegates topology to the linked object. When the stable reference is a
    // FreeCAD mapped-name token carried by topoNamingState, the state owner stays the linked
    // source object instead of the App::Link display wrapper.
    return linkedObject->object;
}

void addTopoStateLinkTargets(std::set<std::string>& objectNames,
                             const app::Document& document)
{
    for (const app::DocumentObject& object : document.objects) {
        for (const auto& propertyItem : object.properties.items()) {
            std::vector<app::Link> links;
            app::collectLinks(propertyItem.value(), links);
            for (const app::Link& link : links) {
                for (const app::ReferenceShadow& shadow : link.referenceShadows) {
                    if (!shadow.target.empty()) {
                        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp
                        // ::updateElementReference(), carries ElementCache/ReferenceShadow for
                        // old subshape recovery. The old target object remains part of the next
                        // client-carried topoNamingState snapshot while that recovery evidence is
                        // still being returned.
                        objectNames.insert(shadow.target);
                    }
                }
                if (link.stableSubnamesSource == "topoNamingState"
                    && linkHasFreeCadMappedStableSubname(link)
                    && requestTopoStateHasObject(document, link.object)) {
                    objectNames.insert(link.object);
                }
            }
        }
    }
}

std::set<std::string> topoStateObjectNames(
    const app::Document& document,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject)
{
    std::set<std::string> objectNames;
    for (const auto& [name, _] : responseSubshapesByObject) {
        const auto documentObjectIt = document.indexByName.find(name);
        if (documentObjectIt != document.indexByName.end()) {
            const app::DocumentObject& object = document.objects.at(documentObjectIt->second);
            if (const auto linkedOwner = linkedTopoStateOwner(object, document)) {
                objectNames.insert(*linkedOwner);
                continue;
            }
        }
        objectNames.insert(name);
    }
    addTopoStateLinkTargets(objectNames, document);
    return objectNames;
}

const part::MappedNameProvenance* sourceBackedMappedNameProvenance(
    const part::NamedShape& namedShape,
    const std::string& entryKey
)
{
    // FreeCAD MappedName raw bytes come from ElementMap producer evidence, not from display or
    // stable tokens. S4 only publishes provenance that already carries an encoded "#" element
    // token from the part/topo ledger.
    const auto provenanceIt = namedShape.mappedNameProvenance.find(entryKey);
    if (provenanceIt == namedShape.mappedNameProvenance.end()) {
        return nullptr;
    }
    const part::MappedNameProvenance& provenance = provenanceIt->second;
    if (provenance.status != part::MappedNameProvenanceStatus::SourceBacked
        || provenance.rawMappedName.empty()
        || provenance.canonicalMappedName.empty()) {
        return nullptr;
    }
    if (!hasFreeCadEncodedElementToken(provenance.rawMappedName)) {
        return nullptr;
    }
    return &provenance;
}

bool sameEndpoint(const nlohmann::json& endpoint,
                  const std::string& objectName,
                  const std::string& subname)
{
    return endpoint.is_object()
        && endpoint.value("object", "") == objectName
        && endpoint.value("subname", "") == subname;
}

const nlohmann::json* resolvedMapperHistoryEventForStableName(
    const nlohmann::json& mapperHistory,
    const std::string& objectName,
    const std::string& stableName,
    const std::string& currentName
)
{
    if (!mapperHistory.is_array()) {
        return nullptr;
    }
    for (const nlohmann::json& event : mapperHistory) {
        if (!event.is_object()) {
            continue;
        }
        const auto sourceIt = event.find("source");
        const auto targetIt = event.find("target");
        if (sourceIt == event.end() || targetIt == event.end()) {
            continue;
        }
        if (!sameEndpoint(*targetIt, objectName, currentName)) {
            continue;
        }
        if (sameEndpoint(*sourceIt, objectName, stableName)
            || sameEndpoint(*sourceIt, objectName, currentName)) {
            return &event;
        }
        const nlohmann::json& mappedName = event.value("mappedName", nlohmann::json::object());
        if (mappedName.is_object()
            && (mappedName.value("raw", "") == stableName
                || mappedName.value("canonical", "") == stableName)) {
            return &event;
        }
    }
    return nullptr;
}

nlohmann::json mapperHistoryIdsForStableName(const nlohmann::json& mapperHistory,
                                             const std::string& objectName,
                                             const std::string& stableName,
                                             const std::string& currentName)
{
    nlohmann::json ids = nlohmann::json::array();
    const nlohmann::json* event =
        resolvedMapperHistoryEventForStableName(mapperHistory, objectName, stableName, currentName);
    if (event == nullptr) {
        return ids;
    }
    const auto idIt = event->find("id");
    if (idIt != event->end() && idIt->is_string()) {
        ids.push_back(idIt->get<std::string>());
    }
    return ids;
}

std::string recoverabilityForStableName(const nlohmann::json& mapperHistory,
                                        const std::string& objectName,
                                        const std::string& stableName,
                                        const std::string& currentName)
{
    const nlohmann::json* event =
        resolvedMapperHistoryEventForStableName(mapperHistory, objectName, stableName, currentName);
    return event == nullptr ? "resolved" : event->value("recoverability", "resolved");
}

nlohmann::json elementMapEntriesJson(
    const std::string& objectName,
    const part::NamedShape& namedShape,
    const std::map<std::string, ResponseSubshapeInfo>& currentSubshapes,
    const nlohmann::json& mapperHistory
)
{
    nlohmann::json entries = nlohmann::json::object();
    for (const auto& [stableName, currentName] : namedShape.elementMap) {
        if (stableName.empty() || currentName.empty()
            || indexedOnlyAlias(objectName, stableName, currentName)) {
            continue;
        }
        const auto currentIt = currentSubshapes.find(currentName);
        if (currentIt == currentSubshapes.end()) {
            continue;
        }
        const part::MappedNameProvenance* provenance =
            sourceBackedMappedNameProvenance(namedShape, stableName);
        if (provenance == nullptr) {
            continue;
        }

        const part::MapperHistoryEndpoint source =
            endpointFromProvenance(namedShape.owner, *provenance, stableName);
        const part::MapperHistoryEndpoint target {objectName, currentName};
        nlohmann::json evidence = {
            {"source", "element_map"},
            {"mapperHistoryIds", matchingMapperHistoryIds(mapperHistory, source, target)},
            {"childElementMapKey", nullptr},
        };
        entries[provenance->canonicalMappedName] = {
            {"target", {{"object", objectName}, {"subname", currentName}}},
            {"shapeKind", currentIt->second.shapeKind},
            {"source", {{"object", source.object}, {"subname", source.subname}}},
            {"mappedName",
             {
                 {"raw", provenance->rawMappedName},
                 {"canonical", provenance->canonicalMappedName},
             }},
            {"recoverability", recoverabilityForEntry(mapperHistory, source, target)},
            {"evidence", std::move(evidence)},
        };
    }
    return entries;
}

std::string childMapKey(const std::string& ownerObject,
                        const std::string& childObject,
                        const std::string& pathPrefix)
{
    std::string key = ownerObject + ":" + childObject;
    if (!pathPrefix.empty()) {
        key += ":" + pathPrefix;
    }
    return key;
}

std::optional<std::size_t> childIndexFromPathPrefix(const std::string& pathPrefix)
{
    constexpr const char* childPrefix = "Child";
    constexpr std::size_t childPrefixSize = 5U;
    if (pathPrefix.rfind(childPrefix, 0) != 0 || pathPrefix.size() == childPrefixSize) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    for (std::size_t i = childPrefixSize; i < pathPrefix.size(); ++i) {
        const char ch = pathPrefix.at(i);
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        index = index * 10U + static_cast<std::size_t>(ch - '0');
    }
    return index;
}

std::string localIndexedPrefix(const std::string& indexedName)
{
    std::string prefix;
    for (char ch : indexedName) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            break;
        }
        prefix.push_back(ch);
    }
    return prefix;
}

std::optional<int> localIndexedOrdinal(const std::string& indexedName)
{
    const std::string prefix = localIndexedPrefix(indexedName);
    if (prefix.empty() || prefix.size() >= indexedName.size()) {
        return std::nullopt;
    }
    try {
        return std::stoi(indexedName.substr(prefix.size()));
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string targetNameForChildLocal(const std::string& localSubname, int offset)
{
    const auto ordinal = localIndexedOrdinal(localSubname);
    if (!ordinal) {
        return {};
    }
    return localIndexedPrefix(localSubname) + std::to_string(offset + *ordinal);
}

std::string sourceLocalNameFromStable(const std::string& stableName)
{
    const std::size_t dot = stableName.rfind('.');
    return dot == std::string::npos ? stableName : stableName.substr(dot + 1U);
}

std::optional<std::pair<std::string, std::string>> childPathSubnameParts(
    const std::string& subname)
{
    const std::size_t dot = subname.find('.');
    if (dot == std::string::npos || dot == 0U || dot + 1U >= subname.size()) {
        return std::nullopt;
    }
    std::string pathPrefix = subname.substr(0, dot);
    if (!childIndexFromPathPrefix(pathPrefix)) {
        return std::nullopt;
    }
    return std::make_pair(std::move(pathPrefix), subname.substr(dot + 1U));
}

std::string shapeKindFromIndexedName(const std::string& indexedName)
{
    if (indexedName.rfind("Face", 0) == 0) {
        return "face";
    }
    if (indexedName.rfind("Edge", 0) == 0) {
        return "edge";
    }
    if (indexedName.rfind("Vertex", 0) == 0) {
        return "vertex";
    }
    return "shape";
}

bool hasMappedName(const part::MappedNameProvenance& provenance)
{
    return provenance.status == part::MappedNameProvenanceStatus::SourceBacked
        && !provenance.rawMappedName.empty()
        && !provenance.canonicalMappedName.empty();
}

std::optional<ChildPathProjection> childPathProjectionFromStableReference(
    const std::string& ownerObject,
    const std::string& subname,
    const std::string& stableSubname)
{
    const auto pathParts = childPathSubnameParts(subname);
    if (!pathParts) {
        return std::nullopt;
    }

    const std::string ownerPrefix = ownerObject + "/";
    if (stableSubname.rfind(ownerPrefix, 0) != 0) {
        return std::nullopt;
    }

    std::string childMappedName = stableSubname.substr(ownerPrefix.size());
    const std::size_t dot = childMappedName.find('.');
    if (dot == std::string::npos || dot == 0U || dot + 1U >= childMappedName.size()) {
        return std::nullopt;
    }
    if (!hasFreeCadEncodedElementToken(childMappedName)) {
        return std::nullopt;
    }

    const std::optional<std::size_t> childIndex = childIndexFromPathPrefix(pathParts->first);
    if (!childIndex) {
        return std::nullopt;
    }

    ChildPathProjection projection;
    projection.childObject = childMappedName.substr(0, dot);
    projection.pathPrefix = pathParts->first;
    projection.localSubname = pathParts->second;
    projection.childIndex = *childIndex;
    projection.targetSubname = subname;
    projection.shapeKind = shapeKindFromIndexedName(projection.localSubname);
    projection.rawChildMappedName = std::move(childMappedName);
    projection.canonicalChildMappedName =
        topo::canonicalizeFreeCadMappedName(projection.rawChildMappedName);
    projection.rawOwnerMappedName = stableSubname;
    projection.canonicalOwnerMappedName =
        topo::canonicalizeFreeCadMappedName(projection.rawOwnerMappedName);
    return projection;
}

const part::NamedShapeChildMap* matchingChildPathMap(
    const part::NamedShape& namedShape,
    const ChildPathProjection& projection)
{
    const auto localOrdinal = localIndexedOrdinal(projection.localSubname);
    if (!localOrdinal || *localOrdinal <= 0) {
        return nullptr;
    }
    for (const part::NamedShapeChildMap& childMap : namedShape.childElementMaps) {
        if (childMap.sourceOwner != projection.childObject
            || childMap.indexedName != projection.pathPrefix
            || childMap.kind != projection.shapeKind
            || childMap.count <= 0
            || *localOrdinal > childMap.count) {
            continue;
        }
        if (childMap.sourceNamedShape != nullptr
            && childMap.sourceNamedShape->elements.count(projection.localSubname) == 0U) {
            continue;
        }
        return &childMap;
    }
    return nullptr;
}

bool stripPrefix(std::string& value, const std::string& prefix)
{
    if (prefix.empty() || value.rfind(prefix, 0) != 0) {
        return false;
    }
    value = value.substr(prefix.size());
    return true;
}

void localizeChildMapProvenance(part::MappedNameProvenance& provenance,
                                const std::string& childPrefix)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::addChildElements(), in the direct child map expansion path, reads
    // "name = child.elementMap->find(childIdx, &sids)" before encoding the parent index. The
    // child map entry is therefore child-local; owner/source object evidence stays in the entry
    // endpoints, not inside mappedName.raw/canonical.
    stripPrefix(provenance.sourceElement, childPrefix);
    if (stripPrefix(provenance.rawMappedName, childPrefix)) {
        provenance.canonicalMappedName =
            topo::canonicalizeFreeCadMappedName(provenance.rawMappedName);
        return;
    }
    stripPrefix(provenance.canonicalMappedName, childPrefix);
}

void prefixMappedNameProvenance(part::MappedNameProvenance& provenance,
                                const std::string& prefix)
{
    if (prefix.empty() || provenance.rawMappedName.rfind(prefix, 0) == 0) {
        return;
    }
    provenance.rawMappedName = prefix + provenance.rawMappedName;
    provenance.canonicalMappedName = prefix + provenance.canonicalMappedName;
}

std::optional<std::pair<std::string, nlohmann::json>> childEntryFromProvenance(
    const std::string& ownerObject,
    const std::string& childObject,
    const std::string& childKey,
    const std::string& pathPrefix,
    const std::string& childKind,
    int offset,
    const std::string& stableName,
    const part::MappedNameProvenance& provenance,
    const std::string& evidenceSource
)
{
    if (!hasMappedName(provenance)) {
        return std::nullopt;
    }
    std::string localSubname = sourceLocalNameFromStable(stableName);
    if (shapeKindFromIndexedName(localSubname) == "shape" && !provenance.sourceElement.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::addChildElements() reads a child mapped name but keeps the child indexed
        // endpoint beside it. For hash-form names such as "Pocket.#11:2;..." the target/source
        // subshape must come from producer evidence, not from parsing the mapped-name token.
        localSubname = sourceLocalNameFromStable(provenance.sourceElement);
    }
    if (!childKind.empty() && shapeKindFromIndexedName(localSubname) != childKind) {
        return std::nullopt;
    }
    const std::string targetSubname = targetNameForChildLocal(localSubname, offset);
    if (targetSubname.empty()) {
        return std::nullopt;
    }
    (void)pathPrefix;
    return std::make_pair(
        provenance.canonicalMappedName,
        nlohmann::json {
            {"target", {{"object", ownerObject}, {"subname", targetSubname}}},
            {"shapeKind", shapeKindFromIndexedName(localSubname)},
            {"source", {{"object", childObject}, {"subname", localSubname}}},
            {"mappedName",
             {
                 {"raw", provenance.rawMappedName},
                 {"canonical", provenance.canonicalMappedName},
             }},
            {"recoverability", "resolved"},
            {"evidence",
             {
                 {"source", evidenceSource},
                 {"mapperHistoryIds", nlohmann::json::array()},
                 {"childElementMapKey", childKey},
             }},
        }
    );
}

nlohmann::json childElementMapEntriesFromNamedShape(
    const std::string& ownerObject,
    const std::string& childObject,
    const std::string& childKey,
    const std::string& pathPrefix,
    const std::string& childKind,
    int offset,
    const part::NamedShape& childShape,
    const std::string& evidenceSource
)
{
    nlohmann::json entries = nlohmann::json::object();
    for (const auto& [stableName, currentName] : childShape.elementMap) {
        if (stableName.empty() || currentName.empty()) {
            continue;
        }
        const auto provenanceIt = childShape.mappedNameProvenance.find(stableName);
        if (provenanceIt == childShape.mappedNameProvenance.end()) {
            continue;
        }
        if (!hasFreeCadEncodedElementToken(provenanceIt->second.rawMappedName)
            && (ownerObject == "Body" || indexedOnlyAlias(childShape.owner, stableName, currentName))) {
            continue;
        }
        auto provenance = provenanceIt->second;
        if (ownerObject == "Body") {
            prefixMappedNameProvenance(provenance, childObject + ".");
        }
        auto entry = childEntryFromProvenance(
            ownerObject,
            childObject,
            childKey,
            pathPrefix,
            childKind,
            offset,
            currentName,
            provenance,
            evidenceSource
        );
        if (entry) {
            entries[entry->first] = std::move(entry->second);
        }
    }
    return entries;
}

nlohmann::json childElementMapEntriesFromOwnerProvenance(
    const std::string& ownerObject,
    const std::string& childObject,
    const std::string& childKey,
    const std::string& pathPrefix,
    const std::string& childKind,
    int offset,
    const part::NamedShape& ownerShape,
    const std::string& evidenceSource
)
{
    nlohmann::json entries = nlohmann::json::object();
    const std::string childPrefix = childObject + ".";
    for (const auto& [stableName, currentName] : ownerShape.elementMap) {
        if (stableName.rfind(childPrefix, 0) != 0 || currentName.empty()) {
            continue;
        }
        const auto provenanceIt = ownerShape.mappedNameProvenance.find(stableName);
        if (provenanceIt == ownerShape.mappedNameProvenance.end()) {
            continue;
        }
        auto provenance = provenanceIt->second;
        if (ownerObject != "Body") {
            localizeChildMapProvenance(provenance, childPrefix);
        }
        if (ownerObject == "Body" && !hasFreeCadEncodedElementToken(provenance.rawMappedName)) {
            continue;
        }
        if (!hasMappedName(provenance)
            && provenance.sourceElement.rfind(childPrefix, 0) == 0
            && provenance.sourceTag
            && !provenance.elementType.empty()) {
            provenance.sourceElement = provenance.sourceElement.substr(childPrefix.size());
            provenance = topo::encodedMappedNameProvenance(std::move(provenance));
        }
        auto entry = childEntryFromProvenance(
            ownerObject,
            childObject,
            childKey,
            pathPrefix,
            childKind,
            offset,
            stableName,
            provenance,
            evidenceSource
        );
        if (entry) {
            entries[entry->first] = std::move(entry->second);
        }
    }
    return entries;
}

nlohmann::json childElementMapsForTopoState(
    const std::string& objectName,
    const part::NamedShape& namedShape
)
{
    struct ProtocolChildMap {
        std::string key;
        std::string childObject;
        std::string pathPrefix;
        nlohmann::json entries = nlohmann::json::object();
    };

    std::vector<std::string> order;
    std::map<std::string, ProtocolChildMap> grouped;
    for (const part::NamedShapeChildMap& childMap : namedShape.childElementMaps) {
        if (objectName != "Body" && childMap.indexedName.rfind("Child", 0) != 0) {
            continue;
        }
        const std::string childKey = childMapKey(objectName, childMap.sourceOwner, childMap.indexedName);
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::addChildElements()
        // carries the child ElementMap as a nested ledger. cad-core first consumes the owner's
        // source-target provenance for this child range, then falls back to child-local provenance.
        const std::string evidenceSource = objectName == "Body"
            ? "freecad_partdesign_body_tip"
            : "freecad_part_compound_links";
        const part::NamedShape* childNamedShape = childMap.sourceNamedShape;
        nlohmann::json entries = nlohmann::json::object();
        if (objectName == "Body") {
            entries = childElementMapEntriesFromOwnerProvenance(
                objectName,
                childMap.sourceOwner,
                childKey,
                childMap.indexedName,
                childMap.kind,
                childMap.offset,
                namedShape,
                evidenceSource
            );
        }
        if (entries.empty() && objectName == "Body" && childNamedShape != nullptr) {
            entries = childElementMapEntriesFromNamedShape(
                objectName,
                childMap.sourceOwner,
                childKey,
                childMap.indexedName,
                childMap.kind,
                childMap.offset,
                *childNamedShape,
                evidenceSource
            );
        }
        if (entries.empty()) {
            entries = childElementMapEntriesFromOwnerProvenance(
                objectName,
                childMap.sourceOwner,
                childKey,
                childMap.indexedName,
                childMap.kind,
                childMap.offset,
                namedShape,
                evidenceSource
            );
        }
        if (entries.empty() && childNamedShape != nullptr) {
            entries = childElementMapEntriesFromNamedShape(
                objectName,
                childMap.sourceOwner,
                childKey,
                childMap.indexedName,
                childMap.kind,
                childMap.offset,
                *childNamedShape,
                evidenceSource
            );
        }
        if (!entries.empty()) {
            if (grouped.count(childKey) == 0U) {
                grouped[childKey] = ProtocolChildMap {
                    childKey,
                    childMap.sourceOwner,
                    childMap.indexedName,
                    nlohmann::json::object(),
                };
                order.push_back(childKey);
            }
            for (const auto& entryItem : entries.items()) {
                grouped[childKey].entries[entryItem.key()] = entryItem.value();
            }
        }
    }

    nlohmann::json childMaps = nlohmann::json::array();
    for (const std::string& key : order) {
        ProtocolChildMap& childMap = grouped[key];
        if (childMap.entries.empty()) {
            continue;
        }
        childMaps.push_back({
            {"key", childMap.key},
            {"ownerObject", objectName},
            {"childObject", childMap.childObject},
            {"childIndex", childMaps.size()},
            {"pathPrefix", childMap.pathPrefix},
            {"elementMap",
             {
                 {"encoding", elementMapVersion},
                 {"status", "history_partial"},
                 {"entries", std::move(childMap.entries)},
             }},
        });
    }
    return childMaps;
}

void mergeChildEntriesIntoTopLevel(nlohmann::json& entries,
                                   const nlohmann::json& childElementMaps)
{
    if (!childElementMaps.is_array()) {
        return;
    }
    for (const nlohmann::json& childMap : childElementMaps) {
        const auto mapIt = childMap.find("elementMap");
        if (mapIt == childMap.end() || !mapIt->is_object()) {
            continue;
        }
        const auto entriesIt = mapIt->find("entries");
        if (entriesIt == mapIt->end() || !entriesIt->is_object()) {
            continue;
        }
        for (const auto& entryItem : entriesIt->items()) {
            entries[entryItem.key()] = entryItem.value();
        }
    }
}

nlohmann::json childPathProjectionEntry(const std::string& ownerObject,
                                        const ChildPathProjection& projection,
                                        const std::string& childKey,
                                        bool ownerQualified)
{
    return {
        {"target", {{"object", ownerObject}, {"subname", projection.targetSubname}}},
        {"shapeKind", projection.shapeKind},
        {"source",
         {
             {"object", projection.childObject},
             {"subname", projection.localSubname},
         }},
        {"mappedName",
         {
             {"raw",
              ownerQualified ? projection.rawOwnerMappedName : projection.rawChildMappedName},
             {"canonical",
              ownerQualified ? projection.canonicalOwnerMappedName
                             : projection.canonicalChildMappedName},
         }},
        {"recoverability", "resolved"},
        {"evidence",
         {
             {"source", "child_element_map"},
             {"mapperHistoryIds", nlohmann::json::array()},
             {"childElementMapKey", childKey},
         }},
    };
}

nlohmann::json* childElementMapByKey(nlohmann::json& childElementMaps,
                                     const std::string& key)
{
    if (!childElementMaps.is_array()) {
        return nullptr;
    }
    for (nlohmann::json& childMap : childElementMaps) {
        if (childMap.is_object() && childMap.value("key", "") == key) {
            return &childMap;
        }
    }
    return nullptr;
}

void mergeProjectionChildEntry(nlohmann::json& childElementMaps,
                               const std::string& ownerObject,
                               const ChildPathProjection& projection,
                               const std::string& childKey)
{
    nlohmann::json* childMap = childElementMapByKey(childElementMaps, childKey);
    if (childMap == nullptr) {
        childElementMaps.push_back({
            {"key", childKey},
            {"ownerObject", ownerObject},
            {"childObject", projection.childObject},
            {"childIndex", projection.childIndex},
            {"pathPrefix", projection.pathPrefix},
            {"elementMap",
             {
                 {"encoding", elementMapVersion},
                 {"status", "history_partial"},
                 {"entries", nlohmann::json::object()},
             }},
        });
        childMap = &childElementMaps.back();
    }
    nlohmann::json& entries = (*childMap)["elementMap"]["entries"];
    if (!entries.contains(projection.canonicalChildMappedName)) {
        entries[projection.canonicalChildMappedName] =
            childPathProjectionEntry(ownerObject, projection, childKey, false);
    }
}

void mergeProjectionSubshape(std::map<std::string, ResponseSubshapeInfo>& subshapes,
                             const ChildPathProjection& projection)
{
    subshapes[projection.targetSubname] = ResponseSubshapeInfo {
        projection.targetSubname,
        projection.targetSubname,
        projection.shapeKind,
        nlohmann::json {
            {"subname", projection.targetSubname},
            {"rawFreecadMappedName", projection.rawOwnerMappedName},
            {"canonicalFreecadMappedName", projection.canonicalOwnerMappedName},
            {"resolvedIndexed", projection.targetSubname},
            {"identityStatus", "stable"},
        },
    };
}

void publishReferenceDrivenChildPathProjections(
    const std::string& objectName,
    const app::Document& document,
    const part::NamedShape& namedShape,
    std::map<std::string, ResponseSubshapeInfo>& subshapes,
    nlohmann::json& childElementMaps,
    nlohmann::json& entries)
{
    if (objectName == "Body") {
        return;
    }

    std::set<std::pair<std::string, std::string>> published;
    for (const app::DocumentObject& object : document.objects) {
        for (const auto& propertyItem : object.properties.items()) {
            std::vector<app::Link> links;
            app::collectLinks(propertyItem.value(), links);
            for (const app::Link& link : links) {
                if (link.object != objectName
                    || link.stableSubnamesSource != "topoNamingState") {
                    continue;
                }
                const std::size_t count =
                    std::min(link.subnames.size(), link.stableSubnames.size());
                for (std::size_t index = 0U; index < count; ++index) {
                    auto projection = childPathProjectionFromStableReference(
                        objectName,
                        link.subnames.at(index),
                        link.stableSubnames.at(index)
                    );
                    if (!projection || projection->shapeKind == "shape") {
                        continue;
                    }
                    if (matchingChildPathMap(namedShape, *projection) == nullptr) {
                        continue;
                    }
                    if (!published.insert({projection->targetSubname,
                                           projection->canonicalOwnerMappedName})
                             .second) {
                        continue;
                    }

                    // FreeCAD:
                    // /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
                    // ::ElementMap::addChildElements(), getAll() appends "child.postfix" to
                    // child mapped names; /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
                    // TopoShapeExpansion.cpp::TopoShape::createChildMap() fills the child range
                    // that explains "Child0.Face1". Public topoNamingState keeps this as a
                    // projection of request StableSubList evidence, not as geometry input.
                    const std::string childKey =
                        childMapKey(objectName, projection->childObject, {});
                    mergeProjectionSubshape(subshapes, *projection);
                    mergeProjectionChildEntry(childElementMaps, objectName, *projection, childKey);
                    if (!entries.contains(projection->canonicalOwnerMappedName)) {
                        entries[projection->canonicalOwnerMappedName] =
                            childPathProjectionEntry(objectName, *projection, childKey, true);
                    }
                }
            }
        }
    }
}

void mergeMapperHistoryEntriesIntoTopLevel(nlohmann::json& entries,
                                           const std::string& objectName,
                                           const part::NamedShape& namedShape,
                                           const std::map<std::string, ResponseSubshapeInfo>& currentSubshapes,
                                           const nlohmann::json& mapperHistory)
{
    for (const auto& [stableName, currentName] : namedShape.elementMap) {
        const auto currentIt = currentSubshapes.find(currentName);
        if (currentIt == currentSubshapes.end()) {
            continue;
        }
        const auto provenanceIt = namedShape.mappedNameProvenance.find(stableName);
        if (provenanceIt == namedShape.mappedNameProvenance.end()
            || !hasMappedName(provenanceIt->second)) {
            continue;
        }
        const nlohmann::json ids =
            mapperHistoryIdsForStableName(mapperHistory, objectName, stableName, currentName);
        const part::MapperHistoryEndpoint source =
            endpointFromProvenance(objectName, provenanceIt->second, stableName);
        const part::MapperHistoryEndpoint target {objectName, currentName};
        nlohmann::json eventIds = ids;
        if (eventIds.empty()) {
            eventIds = matchingMapperHistoryIds(mapperHistory, source, target);
        }
        if (eventIds.empty()) {
            continue;
        }
        entries[provenanceIt->second.canonicalMappedName] = {
            {"target", {{"object", objectName}, {"subname", currentName}}},
            {"shapeKind", currentIt->second.shapeKind},
            {"source",
             {
                 {"object", source.object},
                 {"subname", source.subname},
             }},
            {"mappedName",
             {
                 {"raw", provenanceIt->second.rawMappedName},
                 {"canonical", provenanceIt->second.canonicalMappedName},
             }},
            {"recoverability",
             recoverabilityForEntry(mapperHistory, source, target)},
            {"evidence",
             {
                 {"source", "mapper_history"},
                 {"mapperHistoryIds", eventIds},
                 {"childElementMapKey", nullptr},
             }},
        };
    }
}

nlohmann::json objectTopoStateJson(
    const std::string& objectName,
    const app::Document& document,
    const ComputeContext& context,
    const nlohmann::json* responseSubshapes
)
{
    const auto namedShapeIt = context.namedShapes.find(objectName);
    const part::NamedShape* namedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    const nlohmann::json namedShapeProjection =
        namedShape == nullptr ? nlohmann::json::object() : part::namedShapeToJson(*namedShape);

    std::map<std::string, ResponseSubshapeInfo> subshapes =
        responseSubshapes == nullptr
            ? std::map<std::string, ResponseSubshapeInfo> {}
            : responseSubshapeInfoByIndexed(*responseSubshapes);
    if (subshapes.empty() && namedShape != nullptr) {
        subshapes = namedShapeSubshapeInfoByIndexed(*namedShape);
    }

    nlohmann::json childElementMaps = nlohmann::json::array();
    nlohmann::json mapperHistory = nlohmann::json::array();
    std::string elementMapStatus = "indexed_only";
    if (namedShapeProjection.is_object() && !namedShapeProjection.empty()) {
        const auto mapperHistoryIt = namedShapeProjection.find("mapper_history");
        if (mapperHistoryIt != namedShapeProjection.end() && mapperHistoryIt->is_array()) {
            mapperHistory = *mapperHistoryIt;
        }
        elementMapStatus = namedShapeProjection.value("element_map_status", elementMapStatus);
    }

    nlohmann::json entries = nlohmann::json::object();
    if (namedShape != nullptr) {
        entries = elementMapEntriesJson(objectName, *namedShape, subshapes, mapperHistory);
        childElementMaps = childElementMapsForTopoState(objectName, *namedShape);
        publishReferenceDrivenChildPathProjections(
            objectName,
            document,
            *namedShape,
            subshapes,
            childElementMaps,
            entries
        );
        if (objectName == "Body") {
            mergeChildEntriesIntoTopLevel(entries, childElementMaps);
        }
        mergeMapperHistoryEntriesIntoTopLevel(entries, objectName, *namedShape, subshapes, mapperHistory);
    }
    if (entries.empty()) {
        elementMapStatus = "indexed_only";
    }

    nlohmann::json objectHashProjection;
    const auto documentObjectIt = document.indexByName.find(objectName);
    if (documentObjectIt != document.indexByName.end()) {
        objectHashProjection =
            documentObjectHashProjection(document.objects.at(documentObjectIt->second));
    }
    else {
        objectHashProjection = fallbackObjectHashProjection(objectName, namedShape);
    }

    return {
        {"objectHash", sha256Json(objectHashProjection)},
        {"elementMapVersion", elementMapVersion},
        {"subshapes", subshapesStateJson(subshapes)},
        {"elementMap",
         {
             {"encoding", elementMapVersion},
             {"status", elementMapStatus},
             {"entries", std::move(entries)},
         }},
        {"childElementMaps", std::move(childElementMaps)},
        {"mapperHistory", std::move(mapperHistory)},
    };
}

}  // namespace

std::string topoNamingStateDocumentHash(const app::Document& document)
{
    return sha256Json(documentHashProjection(document));
}

std::string topoNamingStateObjectHash(const app::DocumentObject& object)
{
    return sha256Json(documentObjectHashProjection(object));
}

std::optional<nlohmann::json> topoNamingStateRequestFailureJson(
    const app::Document& document,
    const std::vector<Diagnostic>& diagnostics
)
{
    if (!document.hasTopoNamingState || !document.topoNamingState.is_object()
        || document.topoNamingState.empty()) {
        return std::nullopt;
    }

    std::vector<Diagnostic> resultDiagnostics = diagnostics;
    const nlohmann::json& state = document.topoNamingState;
    const std::string schemaVersion = state.value("schemaVersion", "");
    if (schemaVersion != topoStateSchemaVersion) {
        resultDiagnostics.push_back({
            "error",
            "topo_state_schema_incompatible",
            "topoNamingState schemaVersion is incompatible; request-level recompute is refused",
            {},
            {},
            {},
            {},
            {},
            {
                {"source", "topoNamingState"},
                {"actualSchemaVersion", schemaVersion},
                {"expectedSchemaVersion", topoStateSchemaVersion},
            },
        });
        return hardFailPayload(std::move(resultDiagnostics));
    }

    const auto producerIt = state.find("producer");
    const nlohmann::json producer =
        producerIt == state.end() ? nlohmann::json::object() : *producerIt;
    if (!producerIsCompatible(producer)) {
        resultDiagnostics.push_back({
            "error",
            "topo_state_producer_incompatible",
            "topoNamingState producer is incompatible; request-level recompute is refused",
            {},
            {},
            {},
            {},
            {},
            {
                {"source", "topoNamingState"},
                {"actualProducer", producer},
                {"expectedProducer", {{"cadCoreVersion", "fixture-contract-v1"}}},
            },
        });
        return hardFailPayload(std::move(resultDiagnostics));
    }

    const std::string actualDocumentHash = state.value("documentHash", "");
    const std::string expectedDocumentHash = topoNamingStateDocumentHash(document);
    if (shouldHardFailHashMismatch(actualDocumentHash, expectedDocumentHash)) {
        resultDiagnostics.push_back({
            "error",
            "topo_state_document_hash_mismatch",
            "topoNamingState documentHash does not match the current DocumentObject graph; request-level recompute is refused",
            {},
            {},
            {},
            {},
            {},
            {
                {"source", "topoNamingState"},
                {"actualDocumentHash", actualDocumentHash},
                {"expectedDocumentHash", expectedDocumentHash},
            },
        });
        return hardFailPayload(std::move(resultDiagnostics));
    }

    const auto objectsIt = state.find("objects");
    if (objectsIt != state.end() && objectsIt->is_object()) {
        for (const auto& objectItem : objectsIt->items()) {
            const std::string objectName = objectItem.key();
            const nlohmann::json& objectState = objectItem.value();
            if (!objectState.is_object()) {
                continue;
            }

            const auto documentObjectIt = document.indexByName.find(objectName);
            if (documentObjectIt != document.indexByName.end()) {
                const std::string actualObjectHash = objectState.value("objectHash", "");
                const std::string expectedObjectHash =
                    topoNamingStateObjectHash(document.objects.at(documentObjectIt->second));
                if (shouldHardFailHashMismatch(actualObjectHash, expectedObjectHash)) {
                    resultDiagnostics.push_back({
                        "error",
                        "topo_state_object_hash_mismatch",
                        "topoNamingState objectHash does not match the current object input; request-level recompute is refused",
                        objectName,
                        {},
                        {},
                        {},
                        {},
                        {
                            {"source", "topoNamingState"},
                            {"actualObjectHash", actualObjectHash},
                            {"expectedObjectHash", expectedObjectHash},
                        },
                    });
                    return hardFailPayload(std::move(resultDiagnostics));
                }
            }

            if (auto diagnostic = validateElementMapEncoding(objectState, objectName)) {
                resultDiagnostics.push_back(std::move(*diagnostic));
                return hardFailPayload(std::move(resultDiagnostics));
            }
            if (auto diagnostic = validateChildElementMapEncoding(objectState, objectName)) {
                resultDiagnostics.push_back(std::move(*diagnostic));
                return hardFailPayload(std::move(resultDiagnostics));
            }
        }
    }

    return std::nullopt;
}

nlohmann::json topoNamingStateJson(
    const app::Document& document,
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject
)
{
    const std::set<std::string> objectNames =
        topoStateObjectNames(document, responseSubshapesByObject);

    nlohmann::json objects = nlohmann::json::object();
    for (const std::string& objectName : objectNames) {
        const auto responseIt = responseSubshapesByObject.find(objectName);
        objects[objectName] = objectTopoStateJson(
            objectName,
            document,
            context,
            responseIt == responseSubshapesByObject.end() ? nullptr : &responseIt->second
        );
    }

    return {
        {"schemaVersion", topoStateSchemaVersion},
        {"producer", producerJson(document)},
        {"documentHash", sha256Json(documentHashProjection(document))},
        {"objects", std::move(objects)},
    };
}

}  // namespace cad_core::runtime
