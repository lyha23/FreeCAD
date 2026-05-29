#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

#include <optional>
#include <string>

namespace cad_core::geometry {

struct TaperedExtrusionOptions {
    gp_Dir direction;
    double length = 0.0;
    double taperAngleRadians = 0.0;
    bool solid = true;
};

struct TaperedExtrusionResult {
    TopoDS_Shape shape;
    bool topoNamingKnownGap = true;
};

std::optional<TaperedExtrusionResult> makeTaperedExtrusion(const TopoDS_Shape& profile,
                                                           const TaperedExtrusionOptions& options,
                                                           std::string& error);

}  // namespace cad_core::geometry
