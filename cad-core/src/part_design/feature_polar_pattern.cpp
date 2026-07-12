#include "cad_core/part_design/feature_polar_pattern.h"

#include "feature_transformed_support.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/producer_trace_scope.h"

#include <Precision.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <optional>
#include <string>
#include <vector>


namespace cad_core::part_design
{

namespace transformed_detail
{

std::optional<std::vector<gp_Trsf>> polarPatternTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    int occurrences
)
{
    std::vector<gp_Trsf> transforms;
    if (occurrences <= 1) {
        return transforms;
    }

    const auto axis = resolvePolarPatternAxis(object, context);
    if (!axis) {
        return std::nullopt;
    }
    const gp_Ax1 rotationAxis(axis->point, axis->direction);

    const std::string mode = readEnumProperty(object, "Mode", {"Extent", "Spacing"}, "Extent");
    if (mode == "Extent") {
        double angleDegrees = readNumberProperty(object, "Angle", 360.0);
        if (std::abs(angleDegrees - 360.0) < Precision::Confusion()) {
            angleDegrees /= static_cast<double>(occurrences);
        }
        else {
            angleDegrees /= static_cast<double>(occurrences - 1);
        }
        const double angleRadians = degreesToRadians(angleDegrees);
        if (std::abs(angleRadians) < Precision::Angular()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_angle",
                "Pattern angle cannot be null",
                object.name,
                "Angle"
            );
            return std::nullopt;
        }

        for (int index = 1; index < occurrences; ++index) {
            gp_Trsf transform;
            transform.SetRotation(rotationAxis, angleRadians * index);
            transforms.push_back(transform);
        }
        return transforms;
    }

    if (mode == "Spacing") {
        const double offset = readNumberProperty(object, "Offset", 120.0);
        const std::vector<double> spacings
            = normalizedSpacingList(object, "Spacings", offset, occurrences);
        const std::vector<double> spacingPattern = readNumberListProperty(object, "SpacingPattern");
        const bool usePattern = spacingPattern.size() > 1U;
        double cumulativeAngle = 0.0;
        for (int index = 1; index < occurrences; ++index) {
            double spacing = offset;
            const auto spacingIndex = static_cast<std::size_t>(index - 1);
            if (spacingIndex < spacings.size()
                && std::abs(spacings[spacingIndex] + 1.0) > Precision::Confusion()) {
                spacing = spacings[spacingIndex];
            }
            else if (usePattern) {
                spacing
                    = spacingPattern[static_cast<std::size_t>(std::fmod(index - 1, spacingPattern.size()))];
            }
            cumulativeAngle += degreesToRadians(spacing);
            gp_Trsf transform;
            transform.SetRotation(rotationAxis, cumulativeAngle);
            transforms.push_back(transform);
        }
        return transforms;
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Unsupported PolarPattern Mode " + mode,
        object.name,
        "Mode"
    );
    return std::nullopt;
}

std::optional<TransformedBuild> buildPolarPatternFeatures(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string transformMode
        = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(3);
    if (occurrences < 1) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "PolarPattern requires at least one occurrence",
            object.name,
            "Occurrences"
        );
        return std::nullopt;
    }

    const auto transforms = polarPatternTransforms(object, context, occurrences);
    if (!transforms) {
        return std::nullopt;
    }
    if (transformMode == "Whole shape") {
        const std::vector<app::Link> supportLinks = app::readLinks(object, "Originals");
        const auto application = applyWholeShapeTransforms(object, context, supportLinks, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild {
            application->shape,
            application->namedShape,
            transformMode,
            application->originals,
            application->supportRefinedFeatures
        };
    }
    if (transformMode != "Features") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "Unsupported PolarPattern TransformMode " + transformMode,
            object.name,
            "TransformMode"
        );
        return std::nullopt;
    }
    const std::vector<app::Link> originals = app::readLinks(object, "Originals");
    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild {
        application->shape,
        application->namedShape,
        transformMode,
        application->originals,
        application->supportRefinedFeatures
    };
}

}  // namespace transformed_detail

using transformed_detail::applyTransformedRefine;
using transformed_detail::buildPolarPatternFeatures;
using transformed_detail::isTransformationTemplate;
using transformed_detail::publishTransformationTemplate;
using transformed_detail::publishTransformedResult;

void executePolarPattern(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    runtime::ProducerTraceScope producerTrace(
        context,
        object,
        "partdesign.pattern",
        "PolarPattern::execute",
        {{"kind", "polar_pattern"},
         {"occurrences", transformed_detail::readIntegerProperty(object, "Occurrences").value_or(0)}}
    );
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp::PolarPattern::getTransformations()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Originals",
             "TransformMode",
             "Axis",
             "Reversed",
             "Mode",
             "Angle",
             "Offset",
             "Spacings",
             "SpacingPattern",
             "Occurrences",
             "BaseFeature",
             "Refine",
             "FuzzyTolerance"}
        )) {
        producerTrace.abort("unsupported_property");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        producerTrace.event("template", "multi_transform_child_deferred");
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildPolarPatternFeatures(object, context);
    if (!result) {
        producerTrace.abort("pattern_build_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        producerTrace.abort("pattern_refine_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    producerTrace.event(
        "publish",
        "pattern_result_ready",
        {{"mode", result->mode}, {"originals", result->originals}}
    );
    publishTransformedResult(object, context, *result, "polar_pattern");
}

}  // namespace cad_core::part_design
