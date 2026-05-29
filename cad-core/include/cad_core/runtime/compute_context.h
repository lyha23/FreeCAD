#pragma once

#include "cad_core/runtime/diagnostics.h"

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::runtime {

struct ShapeValue {
    enum class Kind {
        Profile,
        Solid,
        DatumPlane,
        DatumLine,
        DatumPoint,
    };

    Kind kind;
    TopoDS_Shape shape;
};

struct AddSubShape {
    std::optional<TopoDS_Shape> addShape;
    std::optional<TopoDS_Shape> subShape;
};

struct ComputeContext {
    std::vector<Diagnostic> diagnostics;
    std::map<std::string, ShapeValue> shapes;
    std::map<std::string, AddSubShape> addSubShapes;
    std::map<std::string, nlohmann::json> objects;
    std::map<std::string, nlohmann::json> mesh;
    std::map<std::string, nlohmann::json> subshapes;
    std::map<std::string, std::vector<std::string>> dependencies;
    std::map<std::string, std::string> parentGroupByObject;
    std::map<std::string, gp_Trsf> globalPlacements;
    std::vector<std::string> executionOrder;
};

bool hasFailed(const ComputeContext& context, const std::string& object);

}  // namespace cad_core::runtime
