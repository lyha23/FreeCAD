#include "cad_core/features/feature_extrude.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"

#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Vec.hxx>

namespace cad_core::features {

std::optional<ExtrudeResult> buildLengthExtrusion(const document::DocumentObject& object,
                                                  runtime::ComputeContext& context,
                                                  AddSubMode mode,
                                                  const std::string& featureName)
{
    // FreeCAD semantic source:
    // src/Mod/PartDesign/App/FeatureExtrude.cpp FeatureExtrude::buildExtrusion().
    if (!object.properties.contains("Profile")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", featureName + " Profile must link to a Sketch object", object.name, "Profile");
        return std::nullopt;
    }

    const auto profileLink = document::readLink(object.properties.at("Profile"));
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", featureName + " Profile must link to a Sketch object", object.name, "Profile");
        return std::nullopt;
    }

    const std::string type = object.properties.value("Type", "Length");
    if (type != "Length") {
        runtime::addDiagnostic(context.diagnostics, "error", "unsupported_property", "Only Type=Length is supported in P2", object.name, "Type");
        return std::nullopt;
    }

    const std::string sideType = object.properties.value("SideType", "One side");
    if (sideType != "One side") {
        runtime::addDiagnostic(context.diagnostics, "error", "unsupported_property", "Only SideType=One side is supported in P2", object.name, "SideType");
        return std::nullopt;
    }

    if (!object.properties.contains("Length") || !object.properties.at("Length").is_number()) {
        runtime::addDiagnostic(context.diagnostics, "error", "invalid_length", featureName + " Length must be a number greater than zero", object.name, "Length");
        return std::nullopt;
    }

    const double length = object.properties.at("Length").get<double>();
    if (length <= 0.0) {
        runtime::addDiagnostic(context.diagnostics, "error", "invalid_length", featureName + " Length must be greater than zero", object.name, "Length");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Profile) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a profile",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const bool reversed = object.properties.value("Reversed", false);
    const bool pocketDirection = mode == AddSubMode::Subtractive;
    const double sign = (reversed ^ pocketDirection) ? -1.0 : 1.0;
    const gp_Vec direction(0.0, 0.0, sign * length);
    BRepPrimAPI_MakePrism prism(shapeIt->second.shape, direction);
    prism.Build();
    if (!prism.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "OCCT could not extrude " + featureName + " profile", object.name);
        return std::nullopt;
    }

    TopoDS_Shape toolShape = prism.Shape();
    return ExtrudeResult{
        *profileLink,
        length,
        reversed,
        toolShape,
        geometry::bboxForShape(toolShape),
        geometry::volumeForShape(toolShape),
    };
}

}  // namespace cad_core::features
