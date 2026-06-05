#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design::transformed_detail
{

struct TransformSource
{
    std::string owner;
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    std::vector<std::string> refinedFeatures;
};

struct MirrorPlane
{
    gp_Pnt point;
    gp_Dir normal;
};

struct RotationAxis
{
    gp_Pnt point;
    gp_Dir direction;
};

struct SketchAxis
{
    gp_Pnt point;
    gp_Dir direction;
};

struct TransformApplication
{
    TopoDS_Shape shape;
    part::NamedShape namedShape;
    std::vector<std::string> originals;
    std::vector<std::string> supportRefinedFeatures;
};

struct TransformedBuild
{
    TopoDS_Shape shape;
    part::NamedShape namedShape;
    std::string mode;
    std::vector<std::string> originals;
    std::vector<std::string> supportRefinedFeatures;
    bool refineApplied = false;
};

struct TemplateTransforms
{
    std::vector<gp_Trsf> transforms;
    bool scaled = false;
};

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property);
std::string readEnumProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::vector<std::string>& values,
    const std::string& fallback
);
std::optional<int> readIntegerProperty(const app::DocumentObject& object, const std::string& property);
double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback);
bool readBoolProperty(const app::DocumentObject& object, const std::string& property);
bool isTransformationTemplate(const app::DocumentObject& object, const runtime::ComputeContext& context);
void publishTransformationTemplate(const app::DocumentObject& object, runtime::ComputeContext& context);
double degreesToRadians(double degrees);
std::vector<double> readNumberListProperty(
    const app::DocumentObject& object,
    const std::string& property
);
std::vector<double> normalizedSpacingList(
    const app::DocumentObject& object,
    const std::string& property,
    double offset,
    int occurrences
);
std::optional<TopoDS_Face> resolvePlanarFaceLink(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
);
std::optional<TopoDS_Shape> resolveDirectionSubshape(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
);
std::optional<gp_Dir> resolveLinearPatternDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
);
std::optional<RotationAxis> resolvePolarPatternAxis(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<TransformApplication> applyFeatureTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals,
    const std::vector<gp_Trsf>& copyTransforms
);
std::optional<TransformApplication> applyWholeShapeTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& supportLinks,
    const std::vector<gp_Trsf>& copyTransforms
);
void publishTransformedResult(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TransformedBuild& result,
    const std::string& transformedKind
);
bool applyTransformedRefine(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    TransformedBuild& result
);

std::optional<std::vector<gp_Trsf>> mirroredTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
);
std::optional<std::vector<gp_Vec>> linearPatternSteps(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& suffix,
    int occurrences
);
std::vector<gp_Trsf> combinedLinearPatternTransforms(
    const std::vector<gp_Vec>& firstSteps,
    const std::vector<gp_Vec>& secondSteps
);
std::optional<std::vector<gp_Trsf>> polarPatternTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    int occurrences
);
std::optional<std::vector<gp_Trsf>> scaledTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals
);

}  // namespace cad_core::part_design::transformed_detail
