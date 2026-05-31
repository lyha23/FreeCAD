#include "cad_core/c_api.h"

#include "cad_core/document/model.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

CadCoreBuffer emptyBuffer()
{
    return CadCoreBuffer{nullptr, 0};
}

bool copyBuffer(std::string_view value, CadCoreBuffer& buffer) noexcept
{
    buffer = CadCoreBuffer{nullptr, value.size()};
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
    return CadCoreResult{status, emptyBuffer(), error};
}

CadCoreResult makeJsonResult(const nlohmann::json& payload)
{
    CadCoreBuffer json = emptyBuffer();
    if (!copyBuffer(payload.dump(), json)) {
        return makeErrorResult(2, "cad-core FFI failed to allocate result buffer");
    }
    return CadCoreResult{0, json, emptyBuffer()};
}

CadCoreExportResult makeExportErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreExportResult{status, emptyBuffer(), emptyBuffer(), error};
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

    return CadCoreExportResult{0, dataBuffer, jsonBuffer, emptyBuffer()};
}

nlohmann::json cadCoreVersionJson()
{
    return {
        {"version", "0.1.0"},
        {"api", "cad_core_ffi"},
        {"kernel", cad_core::geometry::kernelVersion()},
    };
}

nlohmann::json diagnosticCodeList()
{
    return nlohmann::json::array({
        "conflicting_property",
        "cycle_dependency",
        "deleted_stable_subname",
        "duplicate_object_id",
        "duplicate_object_name",
        "execution_failed",
        "invalid_angle",
        "invalid_axis",
        "invalid_direction",
        "invalid_length",
        "invalid_link_value",
        "invalid_placement",
        "invalid_property_type",
        "invalid_subshape",
        "invalid_taper",
        "missing_link_target",
        "missing_object",
        "missing_property",
        "missing_target",
        "open_profile",
        "parse_error",
        "refine_failed",
        "split_stable_subname",
        "subname_deleted",
        "subname_resolve_ambiguous",
        "subname_resolve_failed",
        "subname_semantic_drift",
        "subname_split_requires_reselect",
        "unsupported_geometry",
        "unsupported_link_lifecycle",
        "unsupported_profile_region",
        "unsupported_property",
        "unsupported_stable_subname",
        "unsupported_subshape_kind",
        "unsupported_type",
    });
}

nlohmann::json capabilitiesJson()
{
    const cad_core::runtime::FeatureRegistry registry = cad_core::runtime::buildDefaultRegistry();
    return {
        {"status", "ok"},
        {"schema_version", "cad-web-v1"},
        {"cad_core", cadCoreVersionJson()},
        {"document",
         {
             {"source", "DocumentObject graph"},
             {"required_object_fields", {"Name", "ID", "TypeId", "Properties"}},
             {"link_property_fields",
              {"value", "values", "SubList", "StableSubList", "ShadowSub", "ReferenceShadow", "SubSet"}},
             {"link_property_shapes",
              {
                  {"App::PropertyLink", {"value"}},
                  {"App::PropertyLinkList", {"values"}},
                  {"App::PropertyLinkSub",
                   {"value", "SubList", "StableSubList", "ShadowSub", "ReferenceShadow"}},
                  {"App::PropertyLinkSubList", {"SubSet"}},
              }},
         }},
        {"supported_type_ids", registry.typeIds()},
        {"export_formats", cad_core::geometry::supportedShapeFileFormats()},
        {"diagnostic_codes", diagnosticCodeList()},
        {"known_gaps",
         {
             "complete_mapper_history",
             "assembly_joint_solver",
             "show_element_missing_child_lifecycle",
         }},
    };
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

}  // namespace

CadCoreResult cad_core_version_json(void)
{
    try {
        return makeJsonResult(cadCoreVersionJson());
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
        return makeJsonResult(capabilitiesJson());
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
    if (request_json == nullptr || request_json_len == 0U) {
        return makeErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json raw = nlohmann::json::parse(payload);
        auto [document, diagnostics] = cad_core::document::parseDocument(raw);
        return makeJsonResult(cad_core::runtime::recompute(document, std::move(diagnostics)));
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
        cad_core::geometry::ShapeFileFormat format;
        try {
            format = cad_core::geometry::shapeFileFormatFromString(request["format"].get<std::string>());
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

        auto [document, diagnostics] = cad_core::document::parseDocument(request["document"]);
        cad_core::runtime::ComputeContext context =
            cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        const auto shapeIt = context.shapes.find(objectName);
        if (document.indexByName.count(objectName) == 0U) {
            cad_core::runtime::addDiagnostic(context.diagnostics,
                                             "error",
                                             "missing_object",
                                             "Export object does not exist: " + objectName,
                                             objectName,
                                             "object",
                                             "export",
                                             objectName);
        }
        else if (shapeIt == context.shapes.end()) {
            cad_core::runtime::addDiagnostic(context.diagnostics,
                                             "error",
                                             "execution_failed",
                                             "Export object has no computed shape: " + objectName,
                                             objectName,
                                             "object",
                                             "export",
                                             objectName);
        }

        nlohmann::json metadata = {
            {"object", objectName},
            {"format", cad_core::geometry::shapeFileFormatName(format)},
            {"content_type", cad_core::geometry::shapeFileFormatContentType(format)},
            {"filename", objectName + "." + cad_core::geometry::shapeFileFormatExtension(format)},
            {"diagnostics", cad_core::runtime::diagnosticsToJson(context.diagnostics)},
        };

        if (shapeIt == context.shapes.end()) {
            metadata["bytes"] = 0;
            return makeExportResult({}, metadata);
        }

        const std::string data =
            cad_core::geometry::exportShapeBuffer(shapeIt->second.shape, format, stlDeflection);
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

void cad_core_free_result(CadCoreResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->json = CadCoreBuffer{nullptr, 0};
    result->error = CadCoreBuffer{nullptr, 0};
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
    result->data = CadCoreBuffer{nullptr, 0};
    result->json = CadCoreBuffer{nullptr, 0};
    result->error = CadCoreBuffer{nullptr, 0};
}
