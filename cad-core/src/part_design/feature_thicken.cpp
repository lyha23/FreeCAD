#include "cad_core/part_design/feature_thicken.h"

#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part_design/feature_extrude.h"
#include "cad_core/part_design/profile_resolver.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRep_Builder.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

std::string shapeKind(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        case TopAbs_SHAPE:
            break;
    }
    return "occt_shape";
}

bool containsOnlyPlanarFaces(const TopoDS_Shape& shape)
{
    bool hasFace = false;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        hasFace = true;
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            return false;
        }
    }
    return hasFace;
}

bool containsShapeKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    if (shape.IsNull()) {
        return false;
    }
    if (shape.ShapeType() == kind) {
        return true;
    }
    for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next()) {
        return true;
    }
    return false;
}

bool isSolidResult(const TopoDS_Shape& shape)
{
    return shape.ShapeType() == TopAbs_SOLID || shape.ShapeType() == TopAbs_COMPSOLID
        || containsShapeKind(shape, TopAbs_SOLID);
}

TopoDS_Shape compoundOfShapes(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.empty()) {
        return TopoDS_Shape {};
    }
    if (shapes.size() == 1U) {
        return shapes.front();
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        builder.Add(compound, shape);
    }
    return compound;
}

void appendProfileResolveFields(nlohmann::json& result, const ExtrudeResult& extrusion)
{
    if (!extrusion.profileResolveMode.empty()) {
        result["profileResolveMode"] = extrusion.profileResolveMode;
    }
    if (!extrusion.profileOwner.empty()) {
        result["profileOwner"] = extrusion.profileOwner;
    }
    if (!extrusion.requestedProfileSubname.empty()) {
        result["requestedSubname"] = extrusion.requestedProfileSubname;
    }
    if (!extrusion.currentProfileSubname.empty()) {
        result["currentSubname"] = extrusion.currentProfileSubname;
    }
}

void appendProfileSelectionFields(nlohmann::json& result,
                                  const std::vector<ProfileBasedProfileSelection>& profiles)
{
    nlohmann::json sourceProfiles = nlohmann::json::array();
    for (const auto& profile : profiles) {
        sourceProfiles.push_back({
            {"owner", profile.link.object},
            {"requestedSubname", profile.link.subnames.empty() ? std::string {} : profile.link.subnames.front()},
            {"currentSubname", profile.selectedSubname},
            {"resolveMode", profile.fromBodyCumulativeReplay ? "body_cumulative_replay" : "feature_local"},
        });
    }
    result["source_profiles"] = sourceProfiles;
    if (profiles.size() == 1U) {
        result["source_profile"] = profiles.front().link.object;
        if (!profiles.front().link.subnames.empty()) {
            result["requestedSubname"] = profiles.front().link.subnames.front();
        }
        if (!profiles.front().selectedSubname.empty()) {
            result["currentSubname"] = profiles.front().selectedSubname;
        }
    }
}

std::optional<std::vector<ProfileBasedProfileSelection>> resolveThickenProfiles(
    const app::DocumentObject& object,
    runtime::ComputeContext& context)
{
    const auto profiles = resolveProfileBasedProfilesForExtrusion(
        object,
        context,
        "Thicken",
        OpenProfileMode::Reject,
        "Thicken Profile must be App::PropertyLinkSubList with face entries");
    if (profiles.empty()) {
        return std::nullopt;
    }

    for (const auto& profile : profiles) {
        if (profile.kind != ProfileKind::ClosedFace) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_thicken_profile",
                                   "Thicken Profile must contain face or surface selections; open wire and edge profiles are not supported",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profile.link.object);
            return std::nullopt;
        }
    }
    return profiles;
}

std::optional<double> readSurfaceThickenLength(const app::DocumentObject& object,
                                               runtime::ComputeContext& context)
{
    const auto value = app::readNumber(object, "Length");
    if (!value || *value <= Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Thicken Length must be a positive number",
                               object.name,
                               "Length");
        return std::nullopt;
    }
    return *value;
}

std::optional<std::string> readSurfaceThickenSideType(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    const auto sideType = app::readString(object, "SideType");
    if (!sideType) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Thicken SideType must be One side for non-planar surface profiles",
                               object.name,
                               "SideType");
        return std::nullopt;
    }
    if (*sideType != "One side") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Non-planar Thicken currently supports only One side",
                               object.name,
                               "SideType");
        return std::nullopt;
    }
    return *sideType;
}

std::optional<part::NamedShapeBuild> buildSurfaceThicken(const app::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const std::vector<ProfileBasedProfileSelection>& profiles,
                                                         double length)
{
    std::vector<TopoDS_Shape> profileShapes;
    profileShapes.reserve(profiles.size());
    for (const auto& profile : profiles) {
        profileShapes.push_back(profile.shape);
    }
    const TopoDS_Shape sourceShape = compoundOfShapes(profileShapes);
    if (sourceShape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_thicken_profile",
                               "Thicken Profile did not resolve to surface geometry",
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const bool reversed = app::readBool(object, "Reversed").value_or(false);
    const double signedLength = reversed ? -length : length;
    const double tolerance = std::max(app::readNumber(object, "FuzzyTolerance").value_or(Precision::Confusion()),
                                      Precision::Confusion());
    const part::NamedShapeSource source {
        profiles.size() == 1U ? profiles.front().link.object : object.name + ".ProfileCompound",
        sourceShape,
        nullptr
    };
    const part::NamedShapeBuild build = part::makeElementOffsetFromSource(
        object.name,
        source,
        signedLength,
        tolerance,
        false,
        false,
        0,
        0,
        true
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               build.error.empty() ? "Thicken surface offset failed" : build.error,
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    if (!isSolidResult(build.shape)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Thicken surface offset produced a non-solid result",
                               object.name,
                               "Profile");
        return std::nullopt;
    }
    return build;
}

}  // namespace

void executeThicken(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"Profile",
                                               "Type",
                                               "Length",
                                               "Length2",
                                               "Reversed",
                                               "SideType",
                                               "BaseFeature",
                                               "UseCustomVector",
                                               "Direction",
                                               "ReferenceAxis",
                                               "AlongSketchNormal",
                                               "Refine",
                                               "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const auto profiles = resolveThickenProfiles(object, context);
    if (!profiles) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const bool hasNonPlanarProfile = std::any_of(
        profiles->begin(),
        profiles->end(),
        [](const ProfileBasedProfileSelection& profile) {
            return !containsOnlyPlanarFaces(profile.shape);
        }
    );
    if (hasNonPlanarProfile) {
        const auto length = readSurfaceThickenLength(object, context);
        const auto sideType = readSurfaceThickenSideType(object, context);
        if (!length || !sideType) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        auto build = buildSurfaceThicken(object, context, *profiles, *length);
        if (!build) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        std::optional<part::NamedShape> namedShape = build->namedShape;
        runtime::RefineShapeResult shapeResult{build->shape, namedShape, false};
        if (!runtime::isFeatureGroupedByBody(object, context)) {
            const auto refined = runtime::applyRefineProperty(object, context, build->shape, namedShape);
            if (!refined) {
                context.objects[object.name] = {{"status", "error"}};
                return;
            }
            shapeResult = *refined;
        }

        const TopoDS_Shape solid = shapeResult.shape;
        namedShape = shapeResult.namedShape;
        if (namedShape) {
            context.namedShapes[object.name] = *namedShape;
        }
        context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
        context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt, namedShape, std::nullopt};
        context.mesh[object.name] = cad_core::part::meshForShape(solid);
        context.subshapes[object.name] = part::subshapeMapForShape(solid);

        nlohmann::json result = {
            {"status", "ok"},
            {"shape", shapeKind(solid)},
            {"feature", "part_design_thicken"},
            {"add_sub", "add"},
            {"method", "SurfaceOffset"},
            {"thickness", *length},
            {"side_type", *sideType},
            {"reversed", app::readBool(object, "Reversed").value_or(false)},
            {"bbox", cad_core::part::objectBBoxForShape(solid)},
            {"volume", cad_core::part::volumeForShape(solid)},
            {"kernel", cad_core::part::kernelVersion()},
        };
        appendProfileSelectionFields(result, *profiles);
        if (shapeResult.applied) {
            result["refine"] = "applied";
        }
        context.objects[object.name] = result;
        return;
    }

    auto extrusion = buildFeatureExtrusion(object, context, AddSubMode::Additive, "Thicken");
    if (!extrusion) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (extrusion->bodyParticipation == "display_only") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_thicken_profile",
                               "Thicken does not support display-only open profile extrusion",
                               object.name,
                               "Profile",
                               "runtime",
                               extrusion->profile.object);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<part::NamedShape> namedShape = extrusion->namedShape;
    runtime::RefineShapeResult shapeResult{extrusion->toolShape, namedShape, false};
    if (!runtime::isFeatureGroupedByBody(object, context)) {
        const auto refined = runtime::applyRefineProperty(object, context, extrusion->toolShape, namedShape);
        if (!refined) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        shapeResult = *refined;
    }

    const TopoDS_Shape solid = shapeResult.shape;
    namedShape = shapeResult.namedShape;
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
    }
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, solid};
    context.addSubShapes[object.name] = runtime::AddSubShape{solid, std::nullopt, namedShape, std::nullopt};
    context.mesh[object.name] = cad_core::part::meshForShape(solid);
    context.subshapes[object.name] = part::subshapeMapForShape(solid);

    nlohmann::json result = {
        {"status", "ok"},
        {"shape", shapeKind(solid)},
        {"feature", "part_design_thicken"},
        {"add_sub", "add"},
        {"method", extrusion->method},
        {"source_profile", extrusion->profile.object},
        {"thickness", extrusion->length},
        {"bbox", extrusion->bbox},
        {"volume", extrusion->volume},
        {"kernel", cad_core::part::kernelVersion()},
    };
    appendProfileResolveFields(result, *extrusion);
    if (shapeResult.applied) {
        result["refine"] = "applied";
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::part_design
