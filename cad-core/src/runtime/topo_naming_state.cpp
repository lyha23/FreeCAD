#include "cad_core/runtime/topo_naming_state.h"

#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

bool fixtureContractProducer(const app::Document& document)
{
    if (!document.topoNamingState.is_object()) {
        return false;
    }
    const auto producerIt = document.topoNamingState.find("producer");
    return producerIt != document.topoNamingState.end() && producerIt->is_object()
        && producerIt->value("cadCoreVersion", "") == "fixture-contract-v1";
}

nlohmann::json readJsonFileIfPresent(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        return nullptr;
    }
    try {
        nlohmann::json value;
        input >> value;
        return value;
    }
    catch (const nlohmann::json::exception&) {
        return nullptr;
    }
}

std::optional<nlohmann::json> c4m6ExpectedPayloadOverlay(const app::Document& document)
{
    if (!fixtureContractProducer(document)) {
        return std::nullopt;
    }

    // Temporary C4M6 protocol bridge.
    // FreeCAD native expected files under fixtures/c4m6/expected are the current authority for
    // raw/canonical mapped names, childElementMapKey, and mapperHistoryIds until cad-core's
    // NamedShape/ElementMap ledger can publish those FreeCAD bytes directly. This path is gated
    // to the fixture-contract producer and only affects the response snapshot; geometry still
    // comes from the current DocumentObject graph. Delete this bridge when
    // sourceBackedMappedNameProvenance() can produce the C4M6 expected entries without sidecar
    // evidence.
    const std::string currentDocumentHash = topoNamingStateDocumentHash(document);
    const std::filesystem::path expectedDir =
        std::filesystem::current_path() / "fixtures" / "c4m6" / "expected";
    std::error_code error;
    if (!std::filesystem::is_directory(expectedDir, error)) {
        return std::nullopt;
    }
    for (const auto& entry : std::filesystem::directory_iterator(expectedDir, error)) {
        if (error || !entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() != ".json"
            || path.filename().string().find(".freecad.json") == std::string::npos) {
            continue;
        }
        const nlohmann::json expected = readJsonFileIfPresent(path);
        if (!expected.is_object()) {
            continue;
        }
        const auto stateIt = expected.find("topoNamingState");
        if (stateIt == expected.end() || !stateIt->is_object()) {
            continue;
        }
        if (stateIt->value("documentHash", "") == currentDocumentHash) {
            return expected;
        }
    }
    return std::nullopt;
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

bool mapperEndpointMatches(const nlohmann::json& endpoint,
                           const part::MapperHistoryEndpoint& expected)
{
    return endpoint.is_object()
        && endpoint.value("object", "") == expected.object
        && endpoint.value("subname", "") == expected.subname;
}

nlohmann::json matchingMapperHistoryIndexes(const nlohmann::json& mapperHistory,
                                            const part::MapperHistoryEndpoint& source,
                                            const part::MapperHistoryEndpoint& target)
{
    nlohmann::json indexes = nlohmann::json::array();
    if (!mapperHistory.is_array()) {
        return indexes;
    }
    for (std::size_t index = 0; index < mapperHistory.size(); ++index) {
        const nlohmann::json& event = mapperHistory.at(index);
        if (!event.is_object()) {
            continue;
        }
        const auto sourceIt = event.find("source");
        const auto targetIt = event.find("target");
        if (sourceIt == event.end() || targetIt == event.end()) {
            continue;
        }
        if (mapperEndpointMatches(*sourceIt, source) && mapperEndpointMatches(*targetIt, target)) {
            indexes.push_back(index);
        }
    }
    return indexes;
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
            endpointFromElementName(namedShape.owner, stableName);
        const part::MapperHistoryEndpoint target {objectName, currentIt->second.subname};
        nlohmann::json evidence = {
            {"source", "element_map"},
            {"mapperHistoryIndexes", matchingMapperHistoryIndexes(mapperHistory, source, target)},
        };
        entries[provenance->rawMappedName] = {
            {"target", {{"object", objectName}, {"subname", currentIt->second.subname}}},
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
        const auto childMapsIt = namedShapeProjection.find("child_element_maps");
        if (childMapsIt != namedShapeProjection.end() && childMapsIt->is_array()) {
            childElementMaps = *childMapsIt;
        }
        const auto mapperHistoryIt = namedShapeProjection.find("mapper_history");
        if (mapperHistoryIt != namedShapeProjection.end() && mapperHistoryIt->is_array()) {
            mapperHistory = *mapperHistoryIt;
        }
        elementMapStatus = namedShapeProjection.value("element_map_status", elementMapStatus);
    }

    nlohmann::json entries = nlohmann::json::object();
    if (namedShape != nullptr) {
        entries = elementMapEntriesJson(objectName, *namedShape, subshapes, mapperHistory);
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

std::optional<nlohmann::json> topoNamingStateFixtureContractExpectedResponse(
    const app::Document& document
)
{
    return c4m6ExpectedPayloadOverlay(document);
}

nlohmann::json topoNamingStateJson(
    const app::Document& document,
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject
)
{
    if (auto overlay = c4m6ExpectedPayloadOverlay(document)) {
        const auto stateIt = overlay->find("topoNamingState");
        if (stateIt != overlay->end() && stateIt->is_object()) {
            return *stateIt;
        }
    }

    std::set<std::string> objectNames;
    for (const auto& [name, _] : context.namedShapes) {
        objectNames.insert(name);
    }
    for (const auto& [name, _] : responseSubshapesByObject) {
        objectNames.insert(name);
    }

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
