#include "cad_core/part_design/feature_scaled.h"

#include "feature_transformed_support.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/producer_trace_scope.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <gp_Trsf.hxx>

#include <optional>
#include <string>
#include <vector>


namespace cad_core::part_design
{

namespace transformed_detail
{

std::optional<std::vector<gp_Trsf>> scaledTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals
)
{
    const double factor = readNumberProperty(object, "Factor", 2.0);
    if (factor < Precision::Confusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Scaling factor too small",
            object.name,
            "Factor"
        );
        return std::nullopt;
    }

    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(2);
    if (occurrences < 2) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Scaled requires at least two occurrences",
            object.name,
            "Occurrences"
        );
        return std::nullopt;
    }
    gp_Pnt centerOfMass;
    if (!originals.empty()) {
        const auto addSubIt = context.addSubShapes.find(originals.front().object);
        if (addSubIt == context.addSubShapes.end()
            || (!addSubIt->second.addShape && !addSubIt->second.subShape)) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "Scaled centre of mass requires an additive or subtractive first Original",
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
        centerOfMass = props.CentreOfMass();
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp
    // ::Scaled::getTransformations(), computes "f = (factor - 1.0) / (occurrences - 1)"
    // and scales around the first original AddSubShape centre of mass; WholeShape passes empty
    // originals through Transformed::getOriginals(), leaving gp_Pnt() as the scale centre.
    const double step = (factor - 1.0) / static_cast<double>(occurrences - 1);
    std::vector<gp_Trsf> transforms;
    for (int index = 1; index < occurrences; ++index) {
        gp_Trsf transform;
        transform.SetScale(centerOfMass, 1.0 + static_cast<double>(index) * step);
        transforms.push_back(transform);
    }
    return transforms;
}

std::optional<TransformedBuild> buildScaledFeatures(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string transformMode
        = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const std::vector<app::Link> originals = app::readLinks(object, "Originals");
    const auto transforms = scaledTransforms(
        object,
        context,
        transformMode == "Whole shape" ? std::vector<app::Link> {} : originals
    );
    if (!transforms) {
        return std::nullopt;
    }
    if (transformMode == "Whole shape") {
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
            "Unsupported Scaled TransformMode " + transformMode,
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
        transformMode,
        application->originals,
        application->supportRefinedFeatures
    };
}

}  // namespace transformed_detail

using transformed_detail::applyTransformedRefine;
using transformed_detail::buildScaledFeatures;
using transformed_detail::isTransformationTemplate;
using transformed_detail::publishTransformationTemplate;
using transformed_detail::publishTransformedResult;

void executeScaled(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    runtime::ProducerTraceScope producerTrace(
        context,
        object,
        "partdesign.transform",
        "Scaled::execute",
        {{"kind", "scaled"},
         {"occurrences", transformed_detail::readIntegerProperty(object, "Occurrences").value_or(0)}}
    );
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp::Scaled::getTransformations()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Originals", "TransformMode", "Factor", "Occurrences", "BaseFeature", "Refine", "FuzzyTolerance"}
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

    auto result = buildScaledFeatures(object, context);
    if (!result) {
        producerTrace.abort("transform_build_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        producerTrace.abort("transform_refine_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    producerTrace.event(
        "publish",
        "transform_result_ready",
        {{"mode", result->mode}, {"originals", result->originals}}
    );
    publishTransformedResult(object, context, *result, "scaled");
}

}  // namespace cad_core::part_design
