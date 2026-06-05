#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::part::part_feature_detail
{

struct PartLinkedShape
{
    std::string objectName;
    TopoDS_Shape shape;
    const part::NamedShape* namedShape = nullptr;
};

double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback);
double radians(double degrees);
void addPartOffsetDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
    const std::string& target = {}
);
std::optional<PartLinkedShape> resolvePartSourceLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& featureName
);
part::NamedShapeSource sourceForPartLinkedShape(const PartLinkedShape& input);
bool shapeContainsKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind);
TopoDS_Shape applyGlobalPlacement(
    const app::DocumentObject& object,
    const runtime::ComputeContext& context,
    const TopoDS_Shape& shape
);
std::string shapeLabelForPartShape(const TopoDS_Shape& shape);
runtime::ShapeValue::Kind shapeKindForPartShape(const TopoDS_Shape& shape);
void publishPartShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& localShape,
    const nlohmann::json& metadata,
    const std::optional<part::NamedShape>& namedShape = std::nullopt
);

}  // namespace cad_core::part::part_feature_detail
