#pragma once

#include "cad_core/runtime/diagnostics.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace cad_core::runtime {

struct ShapeValue {
    enum class Kind {
        Profile,
        Solid,
    };

    Kind kind;
    TopoDS_Shape shape;
};

struct ComputeContext {
    std::vector<Diagnostic> diagnostics;
    std::map<std::string, ShapeValue> shapes;
    std::map<std::string, nlohmann::json> objects;
    std::map<std::string, nlohmann::json> mesh;
    std::map<std::string, nlohmann::json> subshapes;
    std::map<std::string, std::vector<std::string>> dependencies;
};

bool hasFailed(const ComputeContext& context, const std::string& object);

}  // namespace cad_core::runtime
