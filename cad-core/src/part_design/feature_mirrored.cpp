#include "cad_core/part_design/feature_mirrored.h"

#include "datum_plane_reference.h"
#include "feature_transformed_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <optional>
#include <string>
#include <vector>


namespace cad_core::part_design
{

namespace transformed_detail
{

std::optional<MirrorPlane> planeFromFace(
    const TopoDS_Face& face,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " face must be planar",
            object.name,
            property
        );
        return std::nullopt;
    }
    const gp_Pln plane = surface.Plane();
    return MirrorPlane {plane.Location(), plane.Axis().Direction()};
}

std::optional<MirrorPlane> resolveMirrorPlane(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp
    // ::Mirrored::getTransformations(), accepts a DatumPlane or a planar face from a Part feature.
    if (app::propertyValue(object, "MirrorPlane") == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Mirrored MirrorPlane must link to a datum plane or planar face",
            object.name,
            "MirrorPlane"
        );
        return std::nullopt;
    }

    const auto link = app::readLink(object, "MirrorPlane");
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Mirrored MirrorPlane must be an App::PropertyLinkSub",
            object.name,
            "MirrorPlane"
        );
        return std::nullopt;
    }

    if (const auto planeFrame = detail::referencePlaneProviderFrame(link->object, context);
        planeFrame && link->subnames.empty()) {
        return MirrorPlane {planeFrame->origin, planeFrame->normal};
    }

    if (context.shapes.count(link->object) == 0U) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "MirrorPlane target " + link->object + " did not produce a shape",
            object.name,
            "MirrorPlane",
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    const auto face = resolvePlanarFaceLink(*link, object, context, "MirrorPlane");
    if (!face) {
        return std::nullopt;
    }
    return planeFromFace(*face, object, context, "MirrorPlane");
}

std::optional<std::vector<gp_Trsf>> mirroredTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    auto mirrorPlane = resolveMirrorPlane(object, context);
    if (!mirrorPlane) {
        return std::nullopt;
    }

    gp_Trsf mirrorTransform;
    mirrorTransform.SetMirror(gp_Ax2(mirrorPlane->point, mirrorPlane->normal));
    return std::vector<gp_Trsf> {mirrorTransform};
}

std::optional<TransformedBuild> buildMirroredFeatures(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string mode
        = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const auto transforms = mirroredTransforms(object, context);
    if (!transforms) {
        return std::nullopt;
    }

    const std::vector<app::Link> originals = app::readLinks(object, "Originals");
    if (mode == "Whole shape") {
        const auto application = applyWholeShapeTransforms(object, context, originals, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild {
            application->shape,
            application->namedShape,
            mode,
            application->originals,
            application->supportRefinedFeatures
        };
    }
    if (mode != "Features") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Unsupported Mirrored TransformMode " + mode,
            object.name,
            "TransformMode"
        );
        return std::nullopt;
    }

    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild {
        application->shape,
        application->namedShape,
        mode,
        application->originals,
        application->supportRefinedFeatures
    };
}

}  // namespace transformed_detail

using transformed_detail::applyTransformedRefine;
using transformed_detail::buildMirroredFeatures;
using transformed_detail::isTransformationTemplate;
using transformed_detail::publishTransformationTemplate;
using transformed_detail::publishTransformedResult;

void executeMirrored(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp::Mirrored::getTransformations()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Originals", "TransformMode", "MirrorPlane", "BaseFeature", "Refine", "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildMirroredFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "mirrored");
}

}  // namespace cad_core::part_design
