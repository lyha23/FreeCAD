#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>

namespace cad_core::features {

enum class AddSubMode {
    Additive,
    Subtractive,
};

struct ExtrudeResult {
    document::Link profile;
    std::string method;
    double length = 0.0;
    bool reversed = false;
    TopoDS_Shape toolShape;
    nlohmann::json bbox;
    double volume = 0.0;
    bool topoNamingKnownGap = false;
};

std::optional<ExtrudeResult> buildFeatureExtrusion(const document::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   AddSubMode mode,
                                                   const std::string& featureName);

}  // namespace cad_core::features
