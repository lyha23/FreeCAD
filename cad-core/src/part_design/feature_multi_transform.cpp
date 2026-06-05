#include "cad_core/part_design/feature_multi_transform.h"

#include "feature_transformed_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <optional>
#include <string>
#include <vector>


namespace cad_core::part_design
{

namespace transformed_detail
{

void prependIdentity(std::vector<gp_Trsf>& transforms)
{
    gp_Trsf identity;
    transforms.insert(transforms.begin(), identity);
}

std::optional<gp_Pnt> firstOriginalCenterOfMass(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals
)
{
    if (originals.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "MultiTransform Originals must contain at least one feature",
            object.name,
            "Originals"
        );
        return std::nullopt;
    }

    const auto addSubIt = context.addSubShapes.find(originals.front().object);
    if (addSubIt == context.addSubShapes.end()
        || (!addSubIt->second.addShape && !addSubIt->second.subShape)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_type",
            "MultiTransform centre of mass requires an additive or subtractive first Original",
            object.name,
            "Originals",
            "runtime",
            originals.front().object
        );
        return std::nullopt;
    }

    const TopoDS_Shape& originalShape = addSubIt->second.addShape ? *addSubIt->second.addShape
                                                                  : *addSubIt->second.subShape;
    GProp_GProps props;
    BRepGProp::VolumeProperties(originalShape, props);
    return props.CentreOfMass();
}

std::optional<TemplateTransforms> childTemplateTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals,
    const app::DocumentObject& child
)
{
    std::string mode
        = readEnumProperty(child, "TransformMode", {"Features", "Whole shape"}, "Features");
    if (mode != "Features") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_property",
            "MultiTransform child transformations must use TransformMode=Features",
            object.name,
            "Transformations",
            "runtime",
            child.name
        );
        return std::nullopt;
    }

    TemplateTransforms result;
    if (child.typeId == "PartDesign::Mirrored") {
        auto transforms = mirroredTransforms(child, context);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
    }
    else if (child.typeId == "PartDesign::LinearPattern") {
        const int occurrences = readIntegerProperty(child, "Occurrences").value_or(2);
        const int occurrences2 = readIntegerProperty(child, "Occurrences2").value_or(1);
        if (occurrences < 1 || occurrences2 < 1) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_length",
                "LinearPattern requires at least one occurrence",
                object.name,
                "Transformations",
                "runtime",
                child.name
            );
            return std::nullopt;
        }
        const auto firstSteps = linearPatternSteps(child, context, "", occurrences);
        if (!firstSteps) {
            return std::nullopt;
        }
        const auto secondSteps = linearPatternSteps(child, context, "2", occurrences2);
        if (!secondSteps) {
            return std::nullopt;
        }
        result.transforms = combinedLinearPatternTransforms(*firstSteps, *secondSteps);
    }
    else if (child.typeId == "PartDesign::PolarPattern") {
        const int occurrences = readIntegerProperty(child, "Occurrences").value_or(3);
        if (occurrences < 1) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_length",
                "PolarPattern requires at least one occurrence",
                object.name,
                "Transformations",
                "runtime",
                child.name
            );
            return std::nullopt;
        }
        auto transforms = polarPatternTransforms(child, context, occurrences);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
    }
    else if (child.typeId == "PartDesign::Scaled") {
        auto transforms = scaledTransforms(child, context, originals);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
        result.scaled = true;
    }
    else {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_type",
            "MultiTransform child must be a transformed feature",
            object.name,
            "Transformations",
            "runtime",
            child.name
        );
        return std::nullopt;
    }
    prependIdentity(result.transforms);
    return result;
}

std::optional<std::vector<gp_Trsf>> multiTransformTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals
)
{
    const std::vector<app::Link> transformationLinks = app::readLinks(object, "Transformations");
    if (transformationLinks.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "MultiTransform Transformations must contain at least one transformed feature",
            object.name,
            "Transformations"
        );
        return std::nullopt;
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::getOriginals(), returns an empty vector for Mode::WholeShape. MultiTransform
    // still runs getTransformations(originals), so FeatureMultiTransform.cpp leaves "gp_Pnt cog"
    // at its default origin when originals is empty.
    gp_Pnt baseCog;
    if (!originals.empty()) {
        const auto originalCog = firstOriginalCenterOfMass(object, context, originals);
        if (!originalCog) {
            return std::nullopt;
        }
        baseCog = *originalCog;
    }

    std::vector<gp_Trsf> result;
    std::vector<gp_Pnt> cogs;
    for (const auto& link : transformationLinks) {
        const auto childIt = context.documentObjects.find(link.object);
        if (childIt == context.documentObjects.end()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_link_target",
                "Transformation target " + link.object + " does not exist",
                object.name,
                "Transformations",
                "runtime",
                link.object
            );
            return std::nullopt;
        }
        const auto childTransforms
            = childTemplateTransforms(object, context, originals, *childIt->second);
        if (!childTransforms) {
            return std::nullopt;
        }

        if (result.empty()) {
            result = childTransforms->transforms;
            cogs.clear();
            for (const gp_Trsf& transform : result) {
                cogs.push_back(baseCog.Transformed(transform));
            }
            continue;
        }

        const std::vector<gp_Trsf> oldTransformations = result;
        const std::vector<gp_Pnt> oldCogs = cogs;
        result.clear();
        cogs.clear();

        if (childTransforms->scaled) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp
            // ::MultiTransform::getTransformations(), applies Scaled children by a diagonal
            // method and recreates each scale transform around the current slice COG.
            if (childTransforms->transforms.empty()
                || oldTransformations.size() % childTransforms->transforms.size() != 0U) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "invalid_length",
                    "Number of occurrences must be a divisor of previous number of occurrences",
                    object.name,
                    "Transformations",
                    "runtime",
                    childIt->second->name
                );
                return std::nullopt;
            }
            const std::size_t sliceLength = oldTransformations.size()
                / childTransforms->transforms.size();
            std::size_t oldIndex = 0;
            for (const gp_Trsf& newTransform : childTransforms->transforms) {
                for (std::size_t slice = 0; slice < sliceLength; ++slice) {
                    gp_Trsf transform;
                    const double factor = newTransform.ScaleFactor();
                    if (factor > Precision::Confusion()) {
                        transform.SetScale(oldCogs[oldIndex], factor);
                        transform = transform * oldTransformations[oldIndex];
                        cogs.push_back(oldCogs[oldIndex]);
                    }
                    else {
                        transform = newTransform * oldTransformations[oldIndex];
                        cogs.push_back(oldCogs[oldIndex].Transformed(newTransform));
                    }
                    result.push_back(transform);
                    ++oldIndex;
                }
            }
        }
        else {
            for (const gp_Trsf& newTransform : childTransforms->transforms) {
                for (std::size_t oldIndex = 0; oldIndex < oldTransformations.size(); ++oldIndex) {
                    result.push_back(newTransform * oldTransformations[oldIndex]);
                    cogs.push_back(oldCogs[oldIndex].Transformed(newTransform));
                }
            }
        }
    }

    if (!result.empty()) {
        result.erase(result.begin());
    }
    return result;
}

std::optional<TransformedBuild> buildMultiTransformFeatures(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string transformMode
        = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const std::vector<app::Link> originals = app::readLinks(object, "Originals");
    if (transformMode == "Whole shape") {
        const auto transforms = multiTransformTransforms(object, context, {});
        if (!transforms) {
            return std::nullopt;
        }
        const auto application = applyWholeShapeTransforms(object, context, originals, *transforms);
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
            "Unsupported MultiTransform TransformMode " + transformMode,
            object.name,
            "TransformMode"
        );
        return std::nullopt;
    }

    const auto transforms = multiTransformTransforms(object, context, originals);
    if (!transforms) {
        return std::nullopt;
    }
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
using transformed_detail::buildMultiTransformFeatures;
using transformed_detail::publishTransformedResult;

void executeMultiTransform(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Originals", "TransformMode", "Transformations", "BaseFeature", "Refine", "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildMultiTransformFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "multi_transform");
}

}  // namespace cad_core::part_design
