#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/topo/named_shape.h"

#include <optional>
#include <set>
#include <string>

#include <TopoDS_Shape.hxx>

namespace cad_core::features {

using ExecuteFn = void (*)(const document::DocumentObject&, runtime::ComputeContext&);

bool rejectUnsupportedProperties(const document::DocumentObject& object,
                                 runtime::ComputeContext& context,
                                 const std::set<std::string>& allowed);
bool rejectActiveRefineProperty(const document::DocumentObject& object, runtime::ComputeContext& context);
bool isFeatureGroupedByBody(const document::DocumentObject& object, const runtime::ComputeContext& context);

struct RefineShapeResult {
    TopoDS_Shape shape;
    std::optional<topo::NamedShape> namedShape;
    bool applied = false;
};

std::optional<RefineShapeResult> applyRefineProperty(const document::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const TopoDS_Shape& shape,
                                                     const std::optional<topo::NamedShape>& namedShape);
std::optional<RefineShapeResult> applyRefinePropertyForOwner(const document::DocumentObject& propertyObject,
                                                             const std::string& outputOwner,
                                                             runtime::ComputeContext& context,
                                                             const TopoDS_Shape& shape,
                                                             const std::optional<topo::NamedShape>& namedShape);

}  // namespace cad_core::features
