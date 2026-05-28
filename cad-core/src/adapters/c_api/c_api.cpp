#include "cad_core/c_api.h"

#include "cad_core/document/model.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

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

}  // namespace

CadCoreResult cad_core_version_json(void)
{
    try {
        return makeJsonResult({
            {"version", "0.1.0"},
            {"api", "cad_core_ffi"},
            {"kernel", cad_core::geometry::kernelVersion()},
        });
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
