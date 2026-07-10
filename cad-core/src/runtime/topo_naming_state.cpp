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
            nlohmann::json producer = *producerIt;
            if (producer.value("occtVersion", "") == "fixture-occt-unspecified") {
                std::string kernelVersion = part::kernelVersion();
                constexpr const char* prefix = "OCCT ";
                if (kernelVersion.rfind(prefix, 0U) == 0U) {
                    kernelVersion = kernelVersion.substr(std::string(prefix).size());
                }
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
                // records OCCT producer metadata with the runtime kernel version. Fixture requests
                // use a placeholder, but the published topoNamingState should carry the actual
                // producer version for hash/recovery policy decisions.
                producer["occtVersion"] = std::move(kernelVersion);
            }
            return producer;
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

bool shouldHardFailHashMismatch(const std::string& actualHash,
                                const std::string& expectedHash)
{
    // CAD Core protocol authority: /Users/li/Chili3DProject/FreeCAD/AGENTS.md,
    // "拓扑命名与引用状态纪律" requires document/object hash mismatches to reject the
    // client-carried snapshot before recompute. This is a request-integrity boundary, not a
    // FreeCAD geometry decision: the request graph remains the only modeling authority.
    return actualHash != expectedHash;
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

const part::MappedNameProvenance* publicMappedNameProvenance(
    const part::NamedShape& namedShape,
    const std::string& entryKey
);
const part::MappedNameProvenance* sourceBackedMappedNameProvenance(
    const part::NamedShape& namedShape,
    const std::string& entryKey
);

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
        {"identityStatus", subshape.value("identityStatus", "current_only")},
    };
    copyStringIfPresent(result, subshape, "rawFreecadMappedName");
    copyStringIfPresent(result, subshape, "canonicalFreecadMappedName");
    if (result.contains("rawFreecadMappedName") || result.contains("canonicalFreecadMappedName")) {
        result["resolvedIndexed"] = subshape.value("resolvedIndexed", indexed);
    }
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
            {"identityStatus", "current_only"},
        };
        const part::MappedNameProvenance* selectedProvenance = nullptr;
        for (const auto& [stableName, currentName] : namedShape.elementMap) {
            if (currentName != indexed) {
                continue;
            }
            if (const part::MappedNameProvenance* provenance =
                    sourceBackedMappedNameProvenance(namedShape, stableName)) {
                selectedProvenance = provenance;
                break;
            }
            if (selectedProvenance == nullptr) {
                selectedProvenance = publicMappedNameProvenance(namedShape, stableName);
            }
        }
        if (selectedProvenance != nullptr) {
            stateSubshape["rawFreecadMappedName"] = selectedProvenance->rawMappedName;
            stateSubshape["canonicalFreecadMappedName"] = selectedProvenance->canonicalMappedName;
            stateSubshape["resolvedIndexed"] = indexed;
            stateSubshape["identityStatus"] = "stable";
        }
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

bool hasFreeCadMappedNameEvidence(const std::string& rawMappedName)
{
    return rawMappedName.find(';') != std::string::npos;
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
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject)
{
    std::set<std::string> objectNames;
    for (const std::string& name : context.executionOrder) {
        const auto documentObjectIt = document.indexByName.find(name);
        if (documentObjectIt == document.indexByName.end()
            || context.namedShapes.count(name) == 0U) {
            continue;
        }
        const app::DocumentObject& object = document.objects.at(documentObjectIt->second);
        if (object.typeId == "App::Link") {
            continue;
        }
        // FreeCAD:
        // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp
        // ::PropertyPartShape::setValue(), retags and hashes ElementMap when a shape-bearing
        // DocumentObject stores Shape; /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::findAll() then exposes mapped names for every shape object, not just
        // response targets. cad-core therefore publishes every executed public NamedShape object.
        objectNames.insert(name);
    }
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
    const part::MappedNameProvenance* provenance =
        publicMappedNameProvenance(namedShape, entryKey);
    if (provenance == nullptr) {
        return nullptr;
    }
    if (!hasFreeCadEncodedElementToken(provenance->rawMappedName)
        && provenance->rawMappedName.find(";SKT;") == std::string::npos) {
        return nullptr;
    }
    return provenance;
}

const part::MappedNameProvenance* publicMappedNameProvenance(
    const part::NamedShape& namedShape,
    const std::string& entryKey
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
    // ::ElementMap::findAll() returns every mapped name for the current IndexedName, including
    // child-map postfix names and Sketch internal names such as "g1;SKT;FAC". Public
    // topoNamingState only publishes entries backed by producer provenance, not indexed aliases.
    // FreeCAD MappedName raw bytes come from ElementMap producer evidence, not from display or
    // stable tokens.
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
    if (!hasFreeCadMappedNameEvidence(provenance.rawMappedName)) {
        return nullptr;
    }
    return &provenance;
}

const nlohmann::json* requestObjectState(const app::Document& document,
                                         const std::string& objectName)
{
    if (!document.topoNamingState.is_object()) {
        return nullptr;
    }
    const auto objectsIt = document.topoNamingState.find("objects");
    if (objectsIt == document.topoNamingState.end() || !objectsIt->is_object()) {
        return nullptr;
    }
    const auto objectIt = objectsIt->find(objectName);
    return objectIt != objectsIt->end() && objectIt->is_object() ? &*objectIt : nullptr;
}

bool sameJsonValue(const nlohmann::json& left, const nlohmann::json& right)
{
    return left.dump() == right.dump();
}

void appendDistinctJson(nlohmann::json& target, const nlohmann::json& value)
{
    if (!target.is_array() || !value.is_object()) {
        return;
    }
    for (const nlohmann::json& existing : target) {
        if (existing.is_object() && sameJsonValue(existing, value)) {
            return;
        }
    }
    target.push_back(value);
}

bool publicMapperHistoryEvent(const nlohmann::json& event)
{
    if (!event.is_object()) {
        return false;
    }
    const std::string relation = event.value("relation", "");
    const std::string makerStage = event.value("maker_stage", "");
    if (relation == "identity" || makerStage == "indexed" || makerStage == "element_map_preserved") {
        return false;
    }
    if (!event.value("id", "").empty() || !event.value("diagnostic_status", "").empty()) {
        return relation == "generated" || relation == "modified" || relation == "split"
            || relation == "merge" || relation == "deleted" || relation == "ambiguous";
    }
    // FreeCAD:
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeShapeWithElementMap() consumes MapperHistory into ElementMap evidence;
    // the public topoNamingState keeps resolved ElementMap entries and diagnostic/id-bearing
    // recovery events, but does not republish every internal maker-history edge.
    if (relation == "ambiguous") {
        return event.value("source", "") == "freecad_expected_collector";
    }
    return false;
}

nlohmann::json publicMapperHistoryJson(const nlohmann::json& currentMapperHistory,
                                       const nlohmann::json* previousObjectState)
{
    nlohmann::json result = nlohmann::json::array();
    const auto appendPublicEvents = [&](const nlohmann::json& mapperHistory) {
        if (!mapperHistory.is_array()) {
            return;
        }
        for (const nlohmann::json& event : mapperHistory) {
            if (publicMapperHistoryEvent(event)) {
                appendDistinctJson(result, event);
            }
        }
    };

    appendPublicEvents(currentMapperHistory);
    if (previousObjectState != nullptr) {
        const auto previousIt = previousObjectState->find("mapperHistory");
        if (previousIt != previousObjectState->end()) {
            appendPublicEvents(*previousIt);
        }
    }
    return result;
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
        if (stableName.empty() || currentName.empty()) {
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
        if (indexedOnlyAlias(objectName, stableName, currentName)
            && provenance->canonicalMappedName == stableName) {
            continue;
        }

        const part::MapperHistoryEndpoint source =
            endpointFromProvenance(namedShape.owner, *provenance, stableName);
        part::MapperHistoryEndpoint publicSource = source;
        if (!publicSource.subname.empty() && publicSource.subname.front() == '#') {
            publicSource.subname = currentName;
        }
        const part::MapperHistoryEndpoint target {objectName, currentName};
        nlohmann::json evidence = {
            {"source",
             !provenance->rawMappedName.empty() && provenance->rawMappedName.front() == '#'
                 ? "freecad_expected_collector"
                 : "element_map"},
            {"mapperHistoryIds", matchingMapperHistoryIds(mapperHistory, source, target)},
            {"childElementMapKey", nullptr},
        };
        entries[provenance->canonicalMappedName] = {
            {"target", {{"object", objectName}, {"subname", currentName}}},
            {"shapeKind", currentIt->second.shapeKind},
            {"source", {{"object", publicSource.object}, {"subname", publicSource.subname}}},
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

int shapeKindOrder(const std::string& shapeKind)
{
    if (shapeKind == "face") {
        return 0;
    }
    if (shapeKind == "edge") {
        return 1;
    }
    if (shapeKind == "vertex") {
        return 2;
    }
    return 3;
}

int subshapeOrdinal(const std::string& subname)
{
    return localIndexedOrdinal(subname).value_or(0);
}

bool hasMappedName(const part::MappedNameProvenance& provenance)
{
    return provenance.status == part::MappedNameProvenanceStatus::SourceBacked
        && !provenance.rawMappedName.empty()
        && !provenance.canonicalMappedName.empty();
}

bool isSketchInternalFaceMappedName(const std::string& stableName,
                                    const std::string& currentName)
{
    return stableName.find(";SKT;FAC") != std::string::npos
        && currentName.rfind("InternalFace", 0) == 0;
}

std::string internalFaceMapperHistoryId(const nlohmann::json& event)
{
    if (!event.is_object() || !event.contains("id") || !event["id"].is_string()) {
        return {};
    }
    const std::string relation = event.value("relation", "");
    if (relation != "generated" && relation != "modified" && relation != "merge") {
        return {};
    }
    return event["id"].get<std::string>();
}

nlohmann::json mapperHistoryIdsForTarget(const nlohmann::json& mapperHistory,
                                         const std::string& objectName,
                                         const std::string& subname)
{
    nlohmann::json ids = nlohmann::json::array();
    if (!mapperHistory.is_array()) {
        return ids;
    }
    for (const nlohmann::json& event : mapperHistory) {
        if (!sameEndpoint(event.value("target", nlohmann::json::object()), objectName, subname)) {
            continue;
        }
        const std::string id = internalFaceMapperHistoryId(event);
        if (!id.empty()
            && std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    return ids;
}

bool applySketchInternalFaceState(const std::string& objectName,
                                  const ComputeContext& context,
                                  std::map<std::string, ResponseSubshapeInfo>& subshapes,
                                  nlohmann::json& entries,
                                  const nlohmann::json& publicMapperHistory)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || !shapeIt->second.internalNamedShape) {
        return false;
    }

    const part::NamedShape& internalNamedShape = *shapeIt->second.internalNamedShape;
    std::map<std::string, ResponseSubshapeInfo> internalSubshapes;
    nlohmann::json internalEntries = nlohmann::json::object();
    for (const auto& [stableName, currentName] : internalNamedShape.elementMap) {
        if (!isSketchInternalFaceMappedName(stableName, currentName)
            || internalNamedShape.elements.count(currentName) == 0U) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::getInternalElementMap() exposes generated InternalFace entries through InternalShape;
        // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementMapper() preserves the "g1;SKT;FAC" mapped name. cad-core
        // publishes that internal face as Sketch public topoNamingState evidence, not the raw
        // sketch Edge/Vertex current-only map.
        internalSubshapes[currentName] = ResponseSubshapeInfo {
            currentName,
            currentName,
            "face",
            nlohmann::json {
                {"subname", currentName},
                {"rawFreecadMappedName", stableName},
                {"canonicalFreecadMappedName", stableName},
                {"resolvedIndexed", currentName},
                {"identityStatus", "stable"},
            },
        };
        const nlohmann::json ids =
            mapperHistoryIdsForTarget(publicMapperHistory, objectName, currentName);
        internalEntries[stableName] = {
            {"target", {{"object", objectName}, {"subname", currentName}}},
            {"shapeKind", "face"},
            {"source", {{"object", objectName}, {"subname", currentName}}},
            {"mappedName", {{"raw", stableName}, {"canonical", stableName}}},
            {"recoverability", "resolved"},
            {"evidence",
             {
                 {"source", "element_map"},
                 {"mapperHistoryIds", ids},
                 {"childElementMapKey", nullptr},
             }},
        };
    }

    if (internalEntries.empty()) {
        return false;
    }
    subshapes = std::move(internalSubshapes);
    entries = std::move(internalEntries);
    return true;
}

bool applyMapperHistoryOnlySubshapeState(std::map<std::string, ResponseSubshapeInfo>& subshapes,
                                         const nlohmann::json& entries)
{
    if (!entries.is_object() || entries.empty()) {
        return false;
    }
    std::map<std::string, ResponseSubshapeInfo> mapperSubshapes;
    for (const auto& entryItem : entries.items()) {
        const nlohmann::json& entry = entryItem.value();
        const nlohmann::json& evidence = entry.value("evidence", nlohmann::json::object());
        if (!evidence.is_object() || evidence.value("source", "") != "mapper_history") {
            return false;
        }
        const nlohmann::json& target = entry.value("target", nlohmann::json::object());
        const std::string subname = target.value("subname", "");
        if (subname.empty()) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeShapeWithElementMap() turns MapperHistory into selected current
        // ElementMap targets. Public mapper-history-only state publishes those resolved targets
        // and keeps mapped-name evidence in elementMap/mapperHistory, not repeated on every
        // current subshape.
        mapperSubshapes[subname] = ResponseSubshapeInfo {
            subname,
            subname,
            entry.value("shapeKind", shapeKindFromIndexedName(subname)),
            nlohmann::json {
                {"subname", subname},
                {"identityStatus", "stable"},
            },
        };
    }
    if (mapperSubshapes.empty()) {
        return false;
    }
    subshapes = std::move(mapperSubshapes);
    return true;
}

bool isSketchObject(const app::Document& document, const std::string& objectName)
{
    const auto objectIt = document.indexByName.find(objectName);
    if (objectIt == document.indexByName.end()) {
        return false;
    }
    return document.objects.at(objectIt->second).typeId == "Sketcher::SketchObject";
}

void applySketchResponseOnlySubshapePolicy(const std::string& objectName,
                                           const app::Document& document,
                                           std::map<std::string, ResponseSubshapeInfo>& subshapes,
                                           const nlohmann::json& entries,
                                           const nlohmann::json& childElementMaps,
                                           const nlohmann::json& mapperHistory)
{
    if (!isSketchObject(document, objectName)
        || (entries.is_object() && !entries.empty())
        || subshapes.empty()
        || (childElementMaps.is_array() && !childElementMaps.empty())
        || (mapperHistory.is_array() && !mapperHistory.empty())) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // stores public Shape and "InternalShape" separately; when no ElementMap/MapperHistory
    // backs a Sketch edge, the response edge is display evidence, not published topo state.
    subshapes.clear();
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

void mergeChildEntrySubshapeEvidence(std::map<std::string, ResponseSubshapeInfo>& subshapes,
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
            const nlohmann::json& entry = entryItem.value();
            if (!entry.is_object()) {
                continue;
            }
            const auto targetIt = entry.find("target");
            const auto mappedNameIt = entry.find("mappedName");
            if (targetIt == entry.end() || !targetIt->is_object()
                || mappedNameIt == entry.end() || !mappedNameIt->is_object()) {
                continue;
            }
            const std::string targetSubname = targetIt->value("subname", "");
            if (targetSubname.empty()) {
                continue;
            }
            auto subshapeIt = subshapes.find(targetSubname);
            if (subshapeIt == subshapes.end()) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
            // ::ElementMap::findAll(), for child elements, appends "child.postfix" to mapped
            // names returned by the child ElementMap. The public state keeps that mapped-name
            // evidence on the resolved owner subshape while the full child map remains nested.
            nlohmann::json& stateSubshape = subshapeIt->second.stateSubshape;
            const std::string existingCanonical =
                stateSubshape.value("canonicalFreecadMappedName", "");
            if (existingCanonical.find('/') != std::string::npos) {
                continue;
            }
            stateSubshape["rawFreecadMappedName"] = mappedNameIt->value("raw", "");
            stateSubshape["canonicalFreecadMappedName"] = mappedNameIt->value("canonical", entryItem.key());
            stateSubshape["resolvedIndexed"] = targetSubname;
            stateSubshape["identityStatus"] = "stable";
        }
    }
}

std::string collisionEventHash(const std::string& objectName,
                               const std::string& canonical,
                               const std::vector<nlohmann::json>& candidates)
{
    nlohmann::json seedCandidates = nlohmann::json::array();
    for (const nlohmann::json& candidate : candidates) {
        seedCandidates.push_back({
            {"target", candidate.value("target", nlohmann::json(nullptr))},
            {"shapeKind", candidate.value("shapeKind", nlohmann::json(nullptr))},
            {"source", candidate.value("source", nlohmann::json(nullptr))},
            {"mappedNameCanonical", canonical},
            {"recoverability", candidate.value("recoverability", nlohmann::json(nullptr))},
        });
    }
    const nlohmann::json seed = {
        {"context", "topoNamingState.objects." + objectName + ".elementMap.entries"},
        {"canonical", canonical},
        {"candidates", std::move(seedCandidates)},
    };
    return part::sha256Hex(seed.dump()).substr(0U, 16U);
}

void appendCanonicalCollisionEvents(nlohmann::json& mapperHistory,
                                    const std::string& objectName,
                                    const std::map<std::string, ResponseSubshapeInfo>& subshapes)
{
    if (!mapperHistory.is_array()) {
        return;
    }
    std::map<std::string, std::vector<nlohmann::json>> grouped;
    for (const auto& [indexed, subshapeInfo] : subshapes) {
        const nlohmann::json& stateSubshape = subshapeInfo.stateSubshape;
        const auto rawIt = stateSubshape.find("rawFreecadMappedName");
        const auto canonicalIt = stateSubshape.find("canonicalFreecadMappedName");
        if (rawIt == stateSubshape.end() || canonicalIt == stateSubshape.end()
            || !rawIt->is_string() || !canonicalIt->is_string()) {
            continue;
        }
        const std::string canonical = canonicalIt->get<std::string>();
        if (canonical.empty()) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp
        // ::ElementMap::findAll() reports collisions at the owner ElementMap level. Public
        // ambiguity evidence therefore uses the owner object's current subshape as both source
        // and target instead of leaking the child source object's internal ledger.
        grouped[canonical].push_back({
            {"target", {{"object", objectName}, {"subname", indexed}}},
            {"shapeKind", subshapeInfo.shapeKind},
            {"source", {{"object", objectName}, {"subname", indexed}}},
            {"mappedName",
             {
                 {"raw", rawIt->get<std::string>()},
                 {"canonical", canonical},
             }},
            {"recoverability", "resolved"},
        });
    }
    for (const auto& [canonical, group] : grouped) {
        if (group.size() <= 1U) {
            continue;
        }
        auto sortedGroup = group;
        std::sort(sortedGroup.begin(), sortedGroup.end(), [](const nlohmann::json& left,
                                                             const nlohmann::json& right) {
            const std::string leftKind = left.value("shapeKind", "");
            const std::string rightKind = right.value("shapeKind", "");
            if (shapeKindOrder(leftKind) != shapeKindOrder(rightKind)) {
                return shapeKindOrder(leftKind) < shapeKindOrder(rightKind);
            }
            const std::string leftSubname =
                left.value("target", nlohmann::json::object()).value("subname", "");
            const std::string rightSubname =
                right.value("target", nlohmann::json::object()).value("subname", "");
            if (subshapeOrdinal(leftSubname) != subshapeOrdinal(rightSubname)) {
                return subshapeOrdinal(leftSubname) < subshapeOrdinal(rightSubname);
            }
            return leftSubname < rightSubname;
        });
        std::set<std::string> signatures;
        for (const nlohmann::json& entry : sortedGroup) {
            signatures.insert(nlohmann::json({
                {"target", entry.value("target", nlohmann::json(nullptr))},
                {"shapeKind", entry.value("shapeKind", nlohmann::json(nullptr))},
                {"source", entry.value("source", nlohmann::json(nullptr))},
                {"mappedNameCanonical", canonical},
                {"recoverability", entry.value("recoverability", nlohmann::json(nullptr))},
            }).dump());
        }
        if (signatures.size() <= 1U) {
            continue;
        }
        nlohmann::json candidates = nlohmann::json::array();
        for (const nlohmann::json& entry : sortedGroup) {
            candidates.push_back({
                {"target", entry.value("target", nlohmann::json(nullptr))},
                {"shapeKind", entry.value("shapeKind", nlohmann::json(nullptr))},
                {"source", entry.value("source", nlohmann::json(nullptr))},
                {"mappedName", entry.value("mappedName", nlohmann::json(nullptr))},
                {"recoverability", entry.value("recoverability", nlohmann::json(nullptr))},
            });
        }
        const nlohmann::json& mappedName =
            sortedGroup.front().value("mappedName", nlohmann::json::object());
        appendDistinctJson(mapperHistory, {
            {"id", "canonical-collision-" + collisionEventHash(objectName, canonical, sortedGroup)},
            {"relation", "ambiguous"},
            {"recoverability", "ambiguous"},
            {"source", "freecad_expected_collector"},
            {"mappedName",
             {
                 {"raw", mappedName.is_object() ? mappedName.value("raw", "") : ""},
                 {"canonical", canonical},
             }},
            {"candidates", std::move(candidates)},
            {"message",
             "Canonical elementMap key " + canonical
                 + " maps to multiple current targets; the collector keeps it out of elementMap"},
        });
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
        const part::MappedNameProvenance* provenance =
            sourceBackedMappedNameProvenance(namedShape, stableName);
        if (provenance == nullptr) {
            continue;
        }
        const nlohmann::json ids =
            mapperHistoryIdsForStableName(mapperHistory, objectName, stableName, currentName);
        const part::MapperHistoryEndpoint source =
            endpointFromProvenance(objectName, *provenance, stableName);
        const part::MapperHistoryEndpoint target {objectName, currentName};
        nlohmann::json eventIds = ids;
        if (eventIds.empty()) {
            eventIds = matchingMapperHistoryIds(mapperHistory, source, target);
        }
        if (eventIds.empty()) {
            continue;
        }
        entries[provenance->canonicalMappedName] = {
            {"target", {{"object", objectName}, {"subname", currentName}}},
            {"shapeKind", currentIt->second.shapeKind},
            {"source",
             {
                 {"object", source.object},
                 {"subname", source.subname},
            }},
            {"mappedName",
             {
                 {"raw", provenance->rawMappedName},
                 {"canonical", provenance->canonicalMappedName},
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
        mergeChildEntrySubshapeEvidence(subshapes, childElementMaps);
        mergeMapperHistoryEntriesIntoTopLevel(entries, objectName, *namedShape, subshapes, mapperHistory);
    }
    const nlohmann::json* previousObjectState = requestObjectState(document, objectName);
    mapperHistory = publicMapperHistoryJson(mapperHistory, previousObjectState);
    applySketchInternalFaceState(objectName, context, subshapes, entries, mapperHistory);
    applyMapperHistoryOnlySubshapeState(subshapes, entries);
    applySketchResponseOnlySubshapePolicy(
        objectName,
        document,
        subshapes,
        entries,
        childElementMaps,
        mapperHistory
    );
    appendCanonicalCollisionEvents(mapperHistory, objectName, subshapes);
    if (entries.empty()) {
        elementMapStatus = "indexed_only";
    }
    else if (objectName != "Body") {
        bool onlyChildElementMapEvidence = true;
        for (const auto& entryItem : entries.items()) {
            const nlohmann::json& evidence = entryItem.value().value("evidence", nlohmann::json::object());
            if (!evidence.is_object()
                || (evidence.value("source", "") != "child_element_map"
                    && evidence.value("source", "") != "element_map")) {
                onlyChildElementMapEvidence = false;
                break;
            }
        }
        if (onlyChildElementMapEvidence) {
            elementMapStatus = "indexed_only";
        }
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
    if (!document.hasTopoNamingState || document.topoNamingState.is_null()
        || (document.topoNamingState.is_object() && document.topoNamingState.empty())) {
        return std::nullopt;
    }

    std::vector<Diagnostic> resultDiagnostics = diagnostics;
    if (!document.topoNamingState.is_object()) {
        resultDiagnostics.push_back({
            "error",
            "topo_state_schema_incompatible",
            "topoNamingState must be an object or null; request-level recompute is refused",
            {},
            {},
            {},
            {},
            {},
            {
                {"source", "topoNamingState"},
                {"actualTopoNamingState", document.topoNamingState},
                {"expectedTopoNamingStateType", "object_or_null"},
            },
        });
        return hardFailPayload(std::move(resultDiagnostics));
    }
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
    if (objectsIt == state.end() || !objectsIt->is_object()) {
        resultDiagnostics.push_back({
            "error",
            "topo_state_schema_incompatible",
            "topoNamingState.objects must be an object; request-level recompute is refused",
            {},
            {},
            {},
            {},
            {},
            {
                {"source", "topoNamingState"},
                {"actualObjects", objectsIt == state.end() ? nlohmann::json(nullptr) : *objectsIt},
                {"expectedObjectsType", "object"},
            },
        });
        return hardFailPayload(std::move(resultDiagnostics));
    }
    for (const auto& objectItem : objectsIt->items()) {
        const std::string objectName = objectItem.key();
        const nlohmann::json& objectState = objectItem.value();

        const auto documentObjectIt = document.indexByName.find(objectName);
        if (documentObjectIt == document.indexByName.end()) {
            // A top-level state object is allowed only when it names an object in the current
            // request graph. Link children and property-local references stay nested evidence;
            // they must not be promoted to a foreign top-level snapshot owner.
            resultDiagnostics.push_back({
                "error",
                "topo_state_object_owner_incompatible",
                "topoNamingState contains an object that is not present in the current DocumentObject graph; request-level recompute is refused",
                objectName,
                {},
                {},
                {},
                {},
                {
                    {"source", "topoNamingState"},
                    {"offendingObject", objectName},
                },
            });
            return hardFailPayload(std::move(resultDiagnostics));
        }
        if (!objectState.is_object()) {
            resultDiagnostics.push_back({
                "error",
                "topo_state_object_owner_incompatible",
                "topoNamingState object state must be an object owned by the current DocumentObject graph; request-level recompute is refused",
                objectName,
                {},
                {},
                {},
                {},
                {
                    {"source", "topoNamingState"},
                    {"offendingObject", objectName},
                },
            });
            return hardFailPayload(std::move(resultDiagnostics));
        }

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

        if (auto diagnostic = validateElementMapEncoding(objectState, objectName)) {
            resultDiagnostics.push_back(std::move(*diagnostic));
            return hardFailPayload(std::move(resultDiagnostics));
        }
        if (auto diagnostic = validateChildElementMapEncoding(objectState, objectName)) {
            resultDiagnostics.push_back(std::move(*diagnostic));
            return hardFailPayload(std::move(resultDiagnostics));
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
        topoStateObjectNames(document, context, responseSubshapesByObject);

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
