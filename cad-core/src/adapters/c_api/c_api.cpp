#include "cad_core/adapters/c_api.h"

#include "cad_core/app/document.h"
#include "cad_core/part/part_geometry_curve.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/runtime/capability_contract.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <cstdint>
#include <exception>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

CadCoreBuffer emptyBuffer()
{
    return CadCoreBuffer {nullptr, 0};
}

bool copyBuffer(std::string_view value, CadCoreBuffer& buffer) noexcept
{
    buffer = CadCoreBuffer {nullptr, value.size()};
    if (value.empty()) {
        return true;
    }

    buffer.ptr = new (std::nothrow) char[value.size()];
    if (buffer.ptr == nullptr) {
        buffer.len = 0;
        return false;
    }
    std::memcpy(buffer.ptr, value.data(), value.size());
    return true;
}

CadCoreResult makeErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreResult {status, emptyBuffer(), error};
}

CadCoreResult makeJsonResult(const nlohmann::json& payload)
{
    CadCoreBuffer json = emptyBuffer();
    if (!copyBuffer(payload.dump(), json)) {
        return makeErrorResult(2, "cad-core FFI failed to allocate result buffer");
    }
    return CadCoreResult {0, json, emptyBuffer()};
}

CadCoreExportResult makeExportErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreExportResult {status, emptyBuffer(), emptyBuffer(), error};
}

CadCoreExportResult makeExportResult(std::string_view data, const nlohmann::json& payload)
{
    CadCoreBuffer dataBuffer = emptyBuffer();
    if (!copyBuffer(data, dataBuffer)) {
        return makeExportErrorResult(2, "cad-core FFI failed to allocate export buffer");
    }

    CadCoreBuffer jsonBuffer = emptyBuffer();
    if (!copyBuffer(payload.dump(), jsonBuffer)) {
        delete[] dataBuffer.ptr;
        return makeExportErrorResult(2, "cad-core FFI failed to allocate export metadata buffer");
    }

    return CadCoreExportResult {0, dataBuffer, jsonBuffer, emptyBuffer()};
}

nlohmann::json adapterResourceLimitDiagnostic(
    const std::string& message,
    const std::string& object,
    const std::string& property,
    const std::string& target,
    const nlohmann::json& details
)
{
    nlohmann::json diagnostic = {
        {"severity", "error"},
        {"code", "adapter_resource_limit"},
        {"message", message},
        {"object", object},
        {"property", property},
        {"stage", "adapter"},
        {"target", target},
    };
    if (!details.is_null() && (!details.is_object() || !details.empty())) {
        diagnostic["details"] = details;
    }
    return diagnostic;
}

void appendAdapterResourceLimitDiagnostic(
    nlohmann::json& payload,
    const std::string& message,
    const std::string& object,
    const std::string& property,
    const std::string& target,
    const nlohmann::json& details
)
{
    if (!payload.contains("diagnostics") || !payload["diagnostics"].is_array()) {
        payload["diagnostics"] = nlohmann::json::array();
    }
    payload["diagnostics"].push_back(
        adapterResourceLimitDiagnostic(message, object, property, target, details)
    );
}

std::optional<double> readOptionalStlDeflection(const nlohmann::json& request)
{
    if (!request.contains("stl_deflection") || request["stl_deflection"].is_null()) {
        return std::nullopt;
    }
    if (!request["stl_deflection"].is_number()) {
        throw std::runtime_error("stl_deflection must be a positive number");
    }
    const double value = request["stl_deflection"].get<double>();
    if (value <= 0.0) {
        throw std::runtime_error("stl_deflection must be a positive number");
    }
    return value;
}

std::optional<std::size_t> readSizeLimit(
    const nlohmann::json& limits,
    const std::string& snakeCase,
    const std::string& camelCase
)
{
    const auto snake = limits.find(snakeCase);
    const auto camel = limits.find(camelCase);
    const auto it = snake != limits.end() ? snake : camel;
    if (it == limits.end() || it->is_null()) {
        return std::nullopt;
    }
    if (!it->is_number_unsigned() && !it->is_number_integer()) {
        throw std::runtime_error("mesh limit " + snakeCase + " must be an integer");
    }
    const long long value = it->get<long long>();
    if (value < 0) {
        throw std::runtime_error("mesh limit " + snakeCase + " must be non-negative");
    }
    return static_cast<std::size_t>(value);
}

std::optional<nlohmann::json> adapterMeshLimits(const nlohmann::json& request)
{
    if (request.contains("mesh_limits") && request["mesh_limits"].is_object()) {
        return std::make_optional<nlohmann::json>(request["mesh_limits"]);
    }
    const auto adapter = request.find("adapter");
    if (adapter != request.end() && adapter->is_object()) {
        const auto meshLimits = adapter->find("meshLimits");
        if (meshLimits != adapter->end() && meshLimits->is_object()) {
            return std::make_optional<nlohmann::json>(*meshLimits);
        }
    }
    return std::nullopt;
}

std::optional<nlohmann::json> adapterBinaryPayloadLimits(const nlohmann::json& request)
{
    if (request.contains("binary_payload_limits") && request["binary_payload_limits"].is_object()) {
        return std::make_optional<nlohmann::json>(request["binary_payload_limits"]);
    }
    const auto adapter = request.find("adapter");
    if (adapter != request.end() && adapter->is_object()) {
        const auto payloadLimits = adapter->find("binaryPayloadLimits");
        if (payloadLimits != adapter->end() && payloadLimits->is_object()) {
            return std::make_optional<nlohmann::json>(*payloadLimits);
        }
    }
    return std::nullopt;
}

void appendMeshLimitDiagnostic(
    nlohmann::json& result,
    const std::string& object,
    std::size_t vertices,
    std::size_t triangles,
    std::optional<std::size_t> maxVertices,
    std::optional<std::size_t> maxTriangles
)
{
    if (!result.contains("diagnostics") || !result["diagnostics"].is_array()) {
        result["diagnostics"] = nlohmann::json::array();
    }
    result["diagnostics"].push_back({
        {"severity", "error"},
        {"code", "mesh_limit_exceeded"},
        {"message", "Mesh result exceeds adapter streaming limits"},
        {"object", object},
        {"property", "mesh"},
        {"stage", "adapter"},
        {"target", "streaming_mesh_limits"},
        {"mesh", {{"vertices", vertices}, {"triangles", triangles}}},
        {"limits",
         {
             {"max_vertices", maxVertices ? nlohmann::json(*maxVertices) : nlohmann::json(nullptr)},
             {"max_triangles", maxTriangles ? nlohmann::json(*maxTriangles) : nlohmann::json(nullptr)},
         }},
    });
}

void applyStreamingMeshLimits(nlohmann::json& result, const nlohmann::json& request)
{
    const auto limits = adapterMeshLimits(request);
    if (!limits) {
        return;
    }
    std::optional<std::size_t> maxVertices;
    std::optional<std::size_t> maxTriangles;
    std::size_t chunkTriangles = 0U;
    try {
        maxVertices = readSizeLimit(*limits, "max_vertices", "maxVertices");
        maxTriangles = readSizeLimit(*limits, "max_triangles", "maxTriangles");
        chunkTriangles = readSizeLimit(*limits, "chunk_triangles", "chunkTriangles").value_or(0U);
    }
    catch (const std::exception& error) {
        appendAdapterResourceLimitDiagnostic(
            result,
            error.what(),
            "",
            "mesh_limits",
            "mesh_limits",
            {{"contract", "cad-core-result-v1"}}
        );
        return;
    }
    if (!maxVertices && !maxTriangles) {
        return;
    }
    if (!result.contains("results") || !result["results"].is_array()) {
        return;
    }

    for (auto& item : result["results"]) {
        if (!item.is_object() || !item.contains("mesh") || item["mesh"].is_null()) {
            continue;
        }
        nlohmann::json& mesh = item["mesh"];
        const std::size_t vertices = mesh.contains("vertices") && mesh["vertices"].is_array()
            ? mesh["vertices"].size()
            : 0U;
        const std::size_t triangles = mesh.contains("indices") && mesh["indices"].is_array()
            ? mesh["indices"].size() / 3U
            : (mesh.contains("triangles") && mesh["triangles"].is_array() ? mesh["triangles"].size()
                                                                          : 0U);
        const bool exceedsVertices = maxVertices && vertices > *maxVertices;
        const bool exceedsTriangles = maxTriangles && triangles > *maxTriangles;
        if (!exceedsVertices && !exceedsTriangles) {
            if (chunkTriangles > 0U) {
                mesh["streaming"] = {
                    {"chunk_triangles", chunkTriangles},
                    {"chunk_count",
                     triangles == 0U ? 0U : (triangles + chunkTriangles - 1U) / chunkTriangles},
                    {"partial", false},
                };
            }
            continue;
        }
        const std::string object = item.value("object", "");
        appendMeshLimitDiagnostic(result, object, vertices, triangles, maxVertices, maxTriangles);
        mesh = {
            {"limited", true},
            {"streaming",
             {
                 {"protocol", "cad-core-json-mesh-stream-v1"},
                 {"max_vertices", maxVertices ? nlohmann::json(*maxVertices) : nlohmann::json(nullptr)},
                 {"max_triangles",
                  maxTriangles ? nlohmann::json(*maxTriangles) : nlohmann::json(nullptr)},
                 {"chunk_triangles", chunkTriangles},
                 {"original_vertex_count", vertices},
                 {"original_triangle_count", triangles},
                 {"partial", true},
             }},
        };
    }
}

CadCoreResult recomputeJsonEntrypoint(
    const char* request_json,
    size_t request_json_len,
    std::string_view adapterName
)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json raw = nlohmann::json::parse(payload);
        if (cad_core::part::isPartGeometryCurveRequest(raw)) {
            cad_core::runtime::ComputeContext context
                = cad_core::part::computePartGeometryCurveRequest(raw);
            nlohmann::json result = cad_core::part::partGeometryCurveResultJson(context);
            if (!adapterName.empty()) {
                result["adapter"] = adapterName;
            }
            applyStreamingMeshLimits(result, raw);
            return makeJsonResult(result);
        }
        auto [document, diagnostics] = cad_core::app::parseDocument(raw);
        nlohmann::json result = cad_core::runtime::recompute(document, std::move(diagnostics));
        if (!adapterName.empty()) {
            result["adapter"] = adapterName;
        }
        applyStreamingMeshLimits(result, raw);
        return makeJsonResult(result);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

template<typename T>
void appendPod(std::string& data, const T& value)
{
    const char* bytes = reinterpret_cast<const char*>(&value);
    data.append(bytes, sizeof(T));
}

CadCoreExportResult meshBinaryEntrypoint(const char* request_json, size_t request_json_len)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeExportErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json request = nlohmann::json::parse(payload);
        if (!request.is_object()) {
            return makeExportErrorResult(1, "binary mesh request root must be a JSON object");
        }
        if (!request.contains("document") || !request["document"].is_object()) {
            return makeExportErrorResult(1, "binary mesh request field 'document' must be a JSON object");
        }
        if (!request.contains("object") || !request["object"].is_string()
            || request["object"].get<std::string>().empty()) {
            return makeExportErrorResult(
                1,
                "binary mesh request field 'object' must be a non-empty string"
            );
        }

        const std::string objectName = request["object"].get<std::string>();
        auto [document, diagnostics] = cad_core::app::parseDocument(request["document"]);
        cad_core::runtime::ComputeContext context
            = cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        nlohmann::json metadata = {
            {"object", objectName},
            {"protocol", "cad-core-binary-mesh-v1"},
            {"content_type", "application/vnd.cad-core.mesh+bin"},
            {"layout",
             {
                 {"vertex_format", "f64x3_le"},
                 {"index_format", "u32x3_le"},
             }},
            {"diagnostics", cad_core::runtime::diagnosticsToJson(context.diagnostics)},
        };

        std::optional<std::size_t> maxBinaryBytes;
        if (const auto payloadLimits = adapterBinaryPayloadLimits(request)) {
            try {
                maxBinaryBytes = readSizeLimit(*payloadLimits, "max_bytes", "maxBytes");
            }
            catch (const std::exception& error) {
                metadata["bytes"] = 0;
                metadata["limited"] = true;
                appendAdapterResourceLimitDiagnostic(
                    metadata,
                    error.what(),
                    objectName,
                    "binaryPayloads",
                    "binary_payload_limits",
                    {{"protocol", "cad-core-binary-mesh-v1"}}
                );
                return makeExportResult({}, metadata);
            }
        }

        const auto meshIt = context.mesh.find(objectName);
        if (meshIt == context.mesh.end() || meshIt->second.is_null()) {
            metadata["bytes"] = 0;
            metadata["vertex_count"] = 0;
            metadata["triangle_count"] = 0;
            return makeExportResult({}, metadata);
        }

        const nlohmann::json& mesh = meshIt->second;
        std::string data;
        const std::size_t vertexOffset = 0U;
        for (const auto& vertex : mesh.at("vertices")) {
            for (std::size_t index = 0; index < 3U; ++index) {
                appendPod(data, vertex.at(index).get<double>());
            }
        }
        const std::size_t indexOffset = data.size();
        for (const auto& triangle : mesh.at("triangles")) {
            for (std::size_t index = 0; index < 3U; ++index) {
                const std::uint32_t value = triangle.at(index).get<std::uint32_t>();
                appendPod(data, value);
            }
        }

        metadata["bytes"] = data.size();
        metadata["vertex_count"] = mesh.at("vertices").size();
        metadata["triangle_count"] = mesh.at("triangles").size();
        metadata["vertex_offset"] = vertexOffset;
        metadata["index_offset"] = indexOffset;
        if (maxBinaryBytes && data.size() > *maxBinaryBytes) {
            metadata["bytes"] = 0;
            metadata["limited"] = true;
            metadata["original_bytes"] = data.size();
            metadata["byte_limit"] = *maxBinaryBytes;
            appendAdapterResourceLimitDiagnostic(
                metadata,
                "Binary mesh payload exceeds adapter byte limit",
                objectName,
                "binaryPayloads",
                "binary_payload_limits.max_bytes",
                {
                    {"protocol", "cad-core-binary-mesh-v1"},
                    {"actual_bytes", data.size()},
                    {"max_bytes", *maxBinaryBytes},
                }
            );
            return makeExportResult({}, metadata);
        }
        metadata["limited"] = false;
        return makeExportResult(data, metadata);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeExportErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeExportErrorResult(2, error.what());
    }
    catch (...) {
        return makeExportErrorResult(2, "unknown C++ exception");
    }
}

}  // namespace

CadCoreResult cad_core_version_json(void)
{
    try {
        return makeJsonResult(cad_core::runtime::cadCoreVersionJson());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

CadCoreResult cad_core_capabilities_json(void)
{
    try {
        return makeJsonResult(cad_core::runtime::capabilityContractJson());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

CadCoreResult cad_core_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, {});
}

CadCoreResult cad_core_worker_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, "worker");
}

CadCoreResult cad_core_wasm_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, "wasm");
}

CadCoreExportResult cad_core_export_json(const char* request_json, size_t request_json_len)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeExportErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json request = nlohmann::json::parse(payload);
        if (!request.is_object()) {
            return makeExportErrorResult(1, "export request root must be a JSON object");
        }
        if (request.contains("export_file") || request.contains("path") || request.contains("file")) {
            return makeExportErrorResult(1, "export request must not contain a server file path");
        }
        if (!request.contains("document") || !request["document"].is_object()) {
            return makeExportErrorResult(1, "export request field 'document' must be a JSON object");
        }
        if (!request.contains("object") || !request["object"].is_string()
            || request["object"].get<std::string>().empty()) {
            return makeExportErrorResult(1, "export request field 'object' must be a non-empty string");
        }
        if (!request.contains("format") || !request["format"].is_string()) {
            return makeExportErrorResult(1, "export request field 'format' must be a string");
        }

        const std::string objectName = request["object"].get<std::string>();
        cad_core::part::ShapeFileFormat format;
        try {
            format = cad_core::part::shapeFileFormatFromString(request["format"].get<std::string>());
        }
        catch (const std::exception& error) {
            return makeExportErrorResult(1, error.what());
        }

        double stlDeflection = 0.01;
        try {
            stlDeflection = readOptionalStlDeflection(request).value_or(0.01);
        }
        catch (const std::exception& error) {
            return makeExportErrorResult(1, error.what());
        }

        auto [document, diagnostics] = cad_core::app::parseDocument(request["document"]);
        cad_core::runtime::ComputeContext context
            = cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        const auto shapeIt = context.shapes.find(objectName);
        if (document.indexByName.count(objectName) == 0U) {
            cad_core::runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_object",
                "Export object does not exist: " + objectName,
                objectName,
                "object",
                "export",
                objectName
            );
        }
        else if (shapeIt == context.shapes.end()) {
            cad_core::runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Export object has no computed shape: " + objectName,
                objectName,
                "object",
                "export",
                objectName
            );
        }

        nlohmann::json metadata = {
            {"object", objectName},
            {"format", cad_core::part::shapeFileFormatName(format)},
            {"content_type", cad_core::part::shapeFileFormatContentType(format)},
            {"filename", objectName + "." + cad_core::part::shapeFileFormatExtension(format)},
            {"diagnostics", cad_core::runtime::diagnosticsToJson(context.diagnostics)},
        };

        if (shapeIt == context.shapes.end()) {
            metadata["bytes"] = 0;
            return makeExportResult({}, metadata);
        }

        const std::string data
            = cad_core::part::exportShapeBuffer(shapeIt->second.shape, format, stlDeflection);
        metadata["bytes"] = data.size();
        return makeExportResult(data, metadata);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeExportErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeExportErrorResult(2, error.what());
    }
    catch (...) {
        return makeExportErrorResult(2, "unknown C++ exception");
    }
}

CadCoreExportResult cad_core_mesh_binary_json(const char* request_json, size_t request_json_len)
{
    return meshBinaryEntrypoint(request_json, request_json_len);
}

void cad_core_free_result(CadCoreResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->json = CadCoreBuffer {nullptr, 0};
    result->error = CadCoreBuffer {nullptr, 0};
}

void cad_core_free_export_result(CadCoreExportResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->data.ptr;
    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->data = CadCoreBuffer {nullptr, 0};
    result->json = CadCoreBuffer {nullptr, 0};
    result->error = CadCoreBuffer {nullptr, 0};
}
