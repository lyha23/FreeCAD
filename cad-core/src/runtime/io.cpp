#include "cad_core/runtime/io.h"

#include <fstream>
#include <stdexcept>

namespace cad_core::runtime {

void writeJsonFile(const std::filesystem::path& path, const nlohmann::json& payload)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    if (!stream) {
        throw std::runtime_error("Unable to write " + path.string());
    }
    stream << payload.dump(2) << '\n';
    if (!stream) {
        throw std::runtime_error("Unable to complete write " + path.string());
    }
}

void writeJsonFileAtomically(const std::filesystem::path& path, const nlohmann::json& payload)
{
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    try {
        writeJsonFile(temporary, payload);
        std::filesystem::rename(temporary, path);
    }
    catch (...) {
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}  // namespace cad_core::runtime
