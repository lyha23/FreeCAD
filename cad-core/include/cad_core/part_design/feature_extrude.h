#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part_design/profile_resolver.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

enum class AddSubMode {
    Additive,
    Subtractive,
};

struct ExtrudeResult {
    app::Link profile;
    ProfileKind profileKind = ProfileKind::ClosedFace;
    OpenProfileMode openProfileMode = OpenProfileMode::Auto;
    std::optional<OpenProfileMode> resolvedOpenProfileMode;
    std::string bodyParticipation;
    std::vector<std::string> sourceProfileSubnames;
    std::vector<std::string> sourceProfileStableSubnames;
    std::string profileResolveMode;
    std::string profileOwner;
    std::string requestedProfileSubname;
    std::string currentProfileSubname;
    std::string method;
    double length = 0.0;
    bool reversed = false;
    TopoDS_Shape toolShape;
    nlohmann::json bbox;
    double volume = 0.0;
    bool topoNamingKnownGap = false;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/
    // FeatureExtrude.cpp calls "Part::ExtrusionHelper::makeElementDraft" for taper; the Part
    // layer then consumes TopoShapeExpansion.cpp::MapperThruSections "GeneratedFace(s)" history.
    bool taperHistory = false;
    std::optional<part::NamedShape> namedShape;
};

std::optional<ExtrudeResult> buildFeatureExtrusion(const app::DocumentObject& object,
                                                   runtime::ComputeContext& context,
                                                   AddSubMode mode,
                                                   const std::string& featureName);

}  // namespace cad_core::part_design
