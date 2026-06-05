#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/part/topo_shape.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>

namespace cad_core::part_design {

enum class AddSubMode {
    Additive,
    Subtractive,
};

struct ExtrudeResult {
    app::Link profile;
    std::string method;
    double length = 0.0;
    bool reversed = false;
    TopoDS_Shape toolShape;
    nlohmann::json bbox;
    double volume = 0.0;
    bool topoNamingKnownGap = false;
    std::optional<part::NamedShape> namedShape;
};

std::optional<ExtrudeResult> buildFeatureExtrusion(const app::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   AddSubMode mode,
                                                   const std::string& featureName);

}  // namespace cad_core::part_design
