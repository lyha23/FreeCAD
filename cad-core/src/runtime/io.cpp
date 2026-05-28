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
}

}  // namespace cad_core::runtime

