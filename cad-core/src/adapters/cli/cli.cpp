#include "cad_core/adapters/cli.h"

#include "cad_core/document/model.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/io.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace cad_core::adapters {

namespace {

int runRecompute(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath)
{
    nlohmann::json raw;
    try {
        std::ifstream input(inputPath);
        if (!input) {
            throw std::runtime_error("Unable to read input file " + inputPath.string());
        }
        input >> raw;
    }
    catch (const nlohmann::json::parse_error& error) {
        nlohmann::json payload = {
            {"objects", nlohmann::json::object()},
            {"mesh", nlohmann::json::object()},
            {"subshapes", nlohmann::json::object()},
            {"diagnostics", runtime::diagnosticsToJson({{"error", "parse_error", error.what(), {}, {}}})},
        };
        runtime::writeJsonFile(outputPath, payload);
        return 0;
    }

    auto [document, diagnostics] = document::parseDocument(raw);
    const nlohmann::json result = runtime::recompute(document, std::move(diagnostics));
    runtime::writeJsonFile(outputPath, result);
    return 0;
}

void printHelp()
{
    std::cout << "usage: cad-core [--version] recompute <input.json> --output <output.json>\n";
}

}  // namespace

int cliMain(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "cad-core 0.1.0 (" << geometry::kernelVersion() << ")\n";
            return 0;
        }
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printHelp();
            return 0;
        }
        if (argc != 5 || std::string(argv[1]) != "recompute" || std::string(argv[3]) != "--output") {
            printHelp();
            return 2;
        }
        return runRecompute(argv[2], argv[4]);
    }
    catch (const std::exception& error) {
        std::cerr << "cad-core: " << error.what() << '\n';
        return 1;
    }
}

}  // namespace cad_core::adapters
