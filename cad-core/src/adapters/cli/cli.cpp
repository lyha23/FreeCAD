#include "cad_core/adapters/cli.h"

#include "cad_core/app/document.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/runtime/capability_contract.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/io.h"
#include "cad_core/runtime/recompute.h"
#include "cad_core/runtime/topo_naming_state.h"
#include "cad_core/part/topo_shape.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <cstdlib>

namespace cad_core::adapters {

namespace {

struct ExportRequest {
    std::string object;
    cad_core::part::ShapeFileFormat format;
    std::filesystem::path path;
    double stlDeflection = 0.01;
};

struct RecomputeOptions {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::optional<ExportRequest> exportRequest;
};

std::filesystem::path producerTracePathForOutput(const std::filesystem::path& outputPath)
{
    const std::string filename = outputPath.filename().string();
    constexpr const char* cadCoreSuffix = ".cad-core.json";
    std::string sidecar;
    if (filename.size() >= std::char_traits<char>::length(cadCoreSuffix)
        && filename.compare(
               filename.size() - std::char_traits<char>::length(cadCoreSuffix),
               std::char_traits<char>::length(cadCoreSuffix),
               cadCoreSuffix
           ) == 0) {
        sidecar = filename.substr(0, filename.size() - 5U) + ".producer-trace.json";
    }
    else {
        const std::string stem = outputPath.stem().string();
        sidecar = stem + ".cad-core.producer-trace.json";
    }
    return outputPath.parent_path() / sidecar;
}

void clearProducerTraceArtifact(const std::filesystem::path& path) noexcept
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::remove(temporary, ignored);
}

void validateProducerTraceArtifact(const nlohmann::json& trace,
                                   const nlohmann::json& input,
                                   const nlohmann::json& response)
{
    if (!trace.is_object()
        || trace.value("schemaVersion", "") != "freecad.element-map-producer-trace.v1"
        || !trace.contains("events") || !trace.at("events").is_array()
        || trace.at("events").empty()
        || !trace.contains("transactions") || !trace.at("transactions").is_array()
        || trace.at("transactions").empty()
        || !trace.contains("producer") || !trace.at("producer").is_object()) {
        throw std::runtime_error("CAD Core producer trace is missing required closed content");
    }
    const auto& producer = trace.at("producer");
    if (producer.value("inputSha256", "")
            != app::ElementMapProducerTrace::canonicalSha256(input)
        || producer.value("responseSha256", "")
            != app::ElementMapProducerTrace::canonicalSha256(response)) {
        throw std::runtime_error("CAD Core producer trace request/response binding mismatch");
    }
}

bool useLegacyTestOutput()
{
    const char* value = std::getenv("CAD_CORE_TEST_LEGACY_OUTPUT");
    return value != nullptr && std::string(value) == "1";
}

nlohmann::json legacyTestResultJson(const app::Document& document,
                                    const runtime::ComputeContext& context)
{
    nlohmann::json objects = nlohmann::json::object();
    for (const auto& object : document.objects) {
        auto it = context.objects.find(object.name);
        objects[object.name] = it == context.objects.end() ? nlohmann::json{{"status", "pending"}} : it->second;
    }

    return {
        {"objects", objects},
        {"mesh", context.mesh},
        {"subshapes", context.subshapes},
        {"named_shapes", part::namedShapesToJson(context.namedShapes)},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", runtime::diagnosticsToJson(context.diagnostics)},
    };
}

bool readValueArg(int argc, char** argv, int& index, std::string& value)
{
    if (index + 1 >= argc) {
        return false;
    }
    value = argv[index + 1];
    index += 2;
    return true;
}

bool parseRecomputeOptions(int argc, char** argv, RecomputeOptions& options, std::string& error)
{
    if (argc < 5 || std::string(argv[1]) != "recompute") {
        error = "expected recompute <input.json> --output <output.json>";
        return false;
    }

    options.inputPath = argv[2];
    std::optional<std::string> exportObject;
    std::optional<cad_core::part::ShapeFileFormat> exportFormat;
    std::optional<std::filesystem::path> exportFile;
    double stlDeflection = 0.01;

    int index = 3;
    while (index < argc) {
        const std::string option = argv[index];
        std::string value;
        if (!readValueArg(argc, argv, index, value)) {
            error = "missing value for " + option;
            return false;
        }

        if (option == "--output") {
            options.outputPath = value;
        }
        else if (option == "--export-object") {
            exportObject = value;
        }
        else if (option == "--export-format") {
            try {
                exportFormat = cad_core::part::shapeFileFormatFromString(value);
            }
            catch (const std::exception& parseError) {
                error = parseError.what();
                return false;
            }
        }
        else if (option == "--export-file") {
            exportFile = value;
        }
        else if (option == "--stl-deflection") {
            try {
                stlDeflection = std::stod(value);
            }
            catch (const std::exception&) {
                error = "--stl-deflection must be a number";
                return false;
            }
        }
        else {
            error = "unknown option " + option;
            return false;
        }
    }

    if (options.outputPath.empty()) {
        error = "--output is required";
        return false;
    }

    const int exportFields = (exportObject.has_value() ? 1 : 0)
                           + (exportFormat.has_value() ? 1 : 0)
                           + (exportFile.has_value() ? 1 : 0);
    if (exportFields != 0 && exportFields != 3) {
        error = "--export-object, --export-format and --export-file must be provided together";
        return false;
    }
    if (exportFields == 3) {
        options.exportRequest = ExportRequest{*exportObject, *exportFormat, *exportFile, stlDeflection};
    }
    return true;
}

int runRecompute(const RecomputeOptions& options)
{
    nlohmann::json raw;
    try {
        std::ifstream input(options.inputPath);
        if (!input) {
            throw std::runtime_error("Unable to read input file " + options.inputPath.string());
        }
        input >> raw;
    }
    catch (const nlohmann::json::parse_error& error) {
        const nlohmann::json diagnostics = runtime::diagnosticsToJson({{"error", "parse_error", error.what(), {}, {}}});
        const nlohmann::json emptyTopoNamingState =
            runtime::topoNamingStateJson(app::Document {}, runtime::ComputeContext {}, {});
        nlohmann::json payload = useLegacyTestOutput()
            ? nlohmann::json{
                  {"objects", nlohmann::json::object()},
                  {"mesh", nlohmann::json::object()},
                  {"subshapes", nlohmann::json::object()},
                  {"elementReferenceUpdates", nlohmann::json::array()},
                  {"documentObjectUpdates", nlohmann::json::array()},
                  {"diagnostics", diagnostics},
              }
            : nlohmann::json{
                  {"results", nlohmann::json::array()},
                  {"elementReferenceUpdates", nlohmann::json::array()},
                  {"documentObjectUpdates", nlohmann::json::array()},
                  {"diagnostics", diagnostics},
                  {"binaryPayloads", nlohmann::json::array()},
                  {"topoNamingState", emptyTopoNamingState},
              };
        runtime::writeJsonFile(options.outputPath, payload);
        return 0;
    }

    auto [document, diagnostics] = app::parseDocument(raw);
    const std::filesystem::path producerTracePath = producerTracePathForOutput(options.outputPath);
    clearProducerTraceArtifact(producerTracePath);
    runtime::RecomputeTraceMetadata traceMetadata;
    traceMetadata.document = options.inputPath.filename().string();
    traceMetadata.build = "cad-core-cli-0.1.0";
    traceMetadata.inputSha256 = app::ElementMapProducerTrace::canonicalSha256(raw);
    runtime::RecomputeArtifacts artifacts = runtime::recomputeArtifacts(
        document,
        std::move(diagnostics),
        std::move(traceMetadata)
    );
    const runtime::ComputeContext& context = artifacts.context;
    nlohmann::json result = useLegacyTestOutput()
        ? legacyTestResultJson(document, context)
        : std::move(artifacts.response);

    if (options.exportRequest.has_value()) {
        const auto& request = *options.exportRequest;
        const auto shapeIt = context.shapes.find(request.object);
        if (shapeIt == context.shapes.end()) {
            throw std::runtime_error("Export object has no computed shape: " + request.object);
        }
        cad_core::part::exportShapeFile(shapeIt->second.shape, request.path, request.format, request.stlDeflection);
        result["exports"] = nlohmann::json::array({
            {
                {"object", request.object},
                {"format", cad_core::part::shapeFileFormatName(request.format)},
                {"file", request.path.string()},
            },
        });
    }

    artifacts.producerTrace["producer"]["responseSha256"] =
        app::ElementMapProducerTrace::canonicalSha256(result);
    try {
        validateProducerTraceArtifact(artifacts.producerTrace, raw, result);
        runtime::writeJsonFileAtomically(options.outputPath, result);
        runtime::writeJsonFileAtomically(producerTracePath, artifacts.producerTrace);
    }
    catch (...) {
        clearProducerTraceArtifact(producerTracePath);
        throw;
    }
    return 0;
}

int runCapabilities()
{
    std::cout << runtime::capabilityContractJson().dump(2) << '\n';
    return 0;
}

void printHelp()
{
    std::cout << "usage: cad-core [--version] capabilities | recompute <input.json> --output <output.json>"
                 " [--export-object <name> --export-format <brep|step|stl> --export-file <path>]"
                 " [--stl-deflection <value>]\n";
}

}  // namespace

int cliMain(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "cad-core 0.1.0 (" << cad_core::part::kernelVersion() << ")\n";
            return 0;
        }
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printHelp();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "capabilities") {
            return runCapabilities();
        }
        if (argc < 2 || std::string(argv[1]) != "recompute") {
            printHelp();
            return 2;
        }
        RecomputeOptions options;
        std::string error;
        if (!parseRecomputeOptions(argc, argv, options, error)) {
            printHelp();
            std::cerr << "cad-core: " << error << '\n';
            return 2;
        }
        return runRecompute(options);
    }
    catch (const std::exception& error) {
        std::cerr << "cad-core: " << error.what() << '\n';
        return 1;
    }
}

}  // namespace cad_core::adapters
