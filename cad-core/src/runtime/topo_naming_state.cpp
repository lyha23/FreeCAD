#include "cad_core/runtime/topo_naming_state.h"

#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <utility>

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

nlohmann::json topoNamingStateJson(
    const app::Document& document,
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject
)
{
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
