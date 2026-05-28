#include "cad_core/features/pad.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepPrimAPI_MakePrism.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Vec.hxx>

namespace cad_core::features {

void executePad(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources: src/Mod/PartDesign/App/FeaturePad.cpp and FeatureExtrude.cpp
    if (!rejectUnsupportedProperties(object, context, {"Profile", "Length", "Reversed", "SideType"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Profile")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Pad Profile must link to a Sketch object", object.name, "Profile");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto profileLink = document::readLink(object.properties.at("Profile"));
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Pad Profile must link to a Sketch object", object.name, "Profile");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const std::string sideType = object.properties.value("SideType", "One side");
    if (sideType != "One side") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Only SideType=One side is supported in the MVP",
                               object.name,
                               "SideType");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Length") || !object.properties.at("Length").is_number()) {
        runtime::addDiagnostic(context.diagnostics, "error", "invalid_length", "Pad Length must be a number greater than zero", object.name, "Length");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const double length = object.properties.at("Length").get<double>();
    if (length <= 0.0) {
        runtime::addDiagnostic(context.diagnostics, "error", "invalid_length", "Pad Length must be greater than zero", object.name, "Length");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const bool reversed = object.properties.value("Reversed", false);
    const auto shapeIt = context.shapes.find(profileLink->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Profile) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink->object + " did not produce a profile",
                               object.name,
                               "Profile");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const gp_Vec direction(0.0, 0.0, reversed ? -length : length);
    BRepPrimAPI_MakePrism prism(shapeIt->second.shape, direction);
    prism.Build();
    if (!prism.IsDone()) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "OCCT could not extrude Pad profile", object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const TopoDS_Shape solid = prism.Shape();
    const nlohmann::json mesh = geometry::meshForShape(solid);
    const nlohmann::json subshapeMap = topo::subshapeMapForShape(solid);

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.mesh[object.name] = mesh;
    context.subshapes[object.name] = subshapeMap;
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"source_profile", profileLink->object},
        {"bbox", geometry::bboxForShape(solid)},
        {"volume", geometry::volumeForShape(solid)},
        {"kernel", geometry::kernelVersion()},
    };
}

}  // namespace cad_core::features
