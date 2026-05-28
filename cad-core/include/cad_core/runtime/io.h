#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

namespace cad_core::runtime {

void writeJsonFile(const std::filesystem::path& path, const nlohmann::json& payload);

}  // namespace cad_core::runtime

