#include "cad_core/part_design/feature_linear_pattern.h"

#include "feature_transformed_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <Precision.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>


namespace cad_core::part_design
{

namespace transformed_detail
{

std::optional<std::vector<gp_Vec>> linearPatternSteps(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& suffix,
    int occurrences
)
{
    std::vector<gp_Vec> steps {gp_Vec()};
    steps.reserve(static_cast<std::size_t>(std::max(occurrences, 1)));
    if (occurrences <= 1) {
        return steps;
    }

    const std::string directionProperty = "Direction" + suffix;
    auto direction = resolveLinearPatternDirection(object, context, directionProperty);
    if (!direction) {
        return std::nullopt;
    }
    if (readBoolProperty(object, "Reversed" + suffix)) {
        direction->Reverse();
    }

    const std::string modeProperty = "Mode" + suffix;
    const std::string mode = readEnumProperty(object, modeProperty, {"Extent", "Spacing"}, "Extent");
    if (mode == "Extent") {
        const std::string lengthProperty = "Length" + suffix;
        const double length = readNumberProperty(object, lengthProperty, 100.0);
        if (length < Precision::Confusion()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_length",
                "Pattern length too small",
                object.name,
                lengthProperty
            );
            return std::nullopt;
        }
        const double stepDistance = length / static_cast<double>(occurrences - 1);
        for (int index = 1; index < occurrences; ++index) {
            steps.push_back(gp_Vec(*direction) * (stepDistance * index));
        }
        return steps;
    }

    if (mode == "Spacing") {
        const std::string offsetProperty = "Offset" + suffix;
        const double offset = readNumberProperty(object, offsetProperty, 10.0);
        if (offset < Precision::Confusion()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_length",
                "Pattern offset too small",
                object.name,
                offsetProperty
            );
            return std::nullopt;
        }

        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
        // ::LinearPattern::calculateSteps(), uses spacing priority "individual spacing > pattern >
        // global offset".
        const std::vector<double> spacings
            = normalizedSpacingList(object, "Spacings" + suffix, offset, occurrences);
        const std::vector<double> spacingPattern
            = readNumberListProperty(object, "SpacingPattern" + suffix);
        const bool usePattern = spacingPattern.size() > 1U;
        double cumulativeDistance = 0.0;
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
            cumulativeDistance += spacing;
            steps.push_back(gp_Vec(*direction) * cumulativeDistance);
        }
        return steps;
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Unsupported LinearPattern Mode " + mode,
        object.name,
        modeProperty
    );
    return std::nullopt;
}

std::vector<gp_Trsf> combinedLinearPatternTransforms(
    const std::vector<gp_Vec>& firstSteps,
    const std::vector<gp_Vec>& secondSteps
)
{
    std::vector<gp_Trsf> transforms;
    for (const gp_Vec& firstStep : firstSteps) {
        for (const gp_Vec& secondStep : secondSteps) {
            const gp_Vec step = firstStep + secondStep;
            if (step.Magnitude() < Precision::Confusion()) {
                continue;
            }
            gp_Trsf transform;
            transform.SetTranslation(step);
            transforms.push_back(transform);
        }
    }
    return transforms;
}

std::optional<TransformedBuild> buildLinearPatternFeatures(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string transformMode
        = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(2);
    const int occurrences2 = readIntegerProperty(object, "Occurrences2").value_or(1);
    if (occurrences < 1 || occurrences2 < 1) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "LinearPattern requires at least one occurrence",
            object.name,
            occurrences < 1 ? "Occurrences" : "Occurrences2"
        );
        return std::nullopt;
    }

    const auto firstSteps = linearPatternSteps(object, context, "", occurrences);
    if (!firstSteps) {
        return std::nullopt;
    }
    const auto secondSteps = linearPatternSteps(object, context, "2", occurrences2);
    if (!secondSteps) {
        return std::nullopt;
    }

    if (transformMode == "Whole shape") {
        const std::vector<app::Link> supportLinks = app::readLinks(object, "Originals");
        const auto application = applyWholeShapeTransforms(
            object,
            context,
            supportLinks,
            combinedLinearPatternTransforms(*firstSteps, *secondSteps)
        );
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
            "Unsupported LinearPattern TransformMode " + transformMode,
            object.name,
            "TransformMode"
        );
        return std::nullopt;
    }

    const std::vector<app::Link> originals = app::readLinks(object, "Originals");
    const auto application = applyFeatureTransforms(
        object,
        context,
        originals,
        combinedLinearPatternTransforms(*firstSteps, *secondSteps)
    );
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
using transformed_detail::buildLinearPatternFeatures;
using transformed_detail::isTransformationTemplate;
using transformed_detail::publishTransformationTemplate;
using transformed_detail::publishTransformedResult;

void executeLinearPattern(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp::LinearPattern::getTransformations()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Originals",     "TransformMode",   "Direction",    "Reversed",       "Mode",
             "Length",        "Offset",          "Spacings",     "SpacingPattern", "Occurrences",
             "Direction2",    "Reversed2",       "Mode2",        "Length2",        "Offset2",
             "Spacings2",     "SpacingPattern2", "Occurrences2", "BaseFeature",    "Refine",
             "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildLinearPatternFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "linear_pattern");
}

}  // namespace cad_core::part_design
