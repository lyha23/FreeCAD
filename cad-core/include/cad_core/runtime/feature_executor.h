#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <set>
#include <string>

namespace cad_core::runtime
{

using ExecuteFn = void (*)(const app::DocumentObject&, ComputeContext&);

bool rejectUnsupportedProperties(const app::DocumentObject& object,
                                 ComputeContext& context,
                                 const std::set<std::string>& allowed);
bool rejectActiveRefineProperty(const app::DocumentObject& object, ComputeContext& context);
bool isFeatureGroupedByBody(const app::DocumentObject& object, const ComputeContext& context);
bool shouldBuildDisplayTopology(const app::DocumentObject& object, const ComputeContext& context);

struct RefineShapeResult
{
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
    bool applied = false;
};

std::optional<RefineShapeResult> applyRefineProperty(
    const app::DocumentObject& object,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
);
std::optional<RefineShapeResult> applyRefinePropertyForOwner(
    const app::DocumentObject& propertyObject,
    const std::string& outputOwner,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
);
// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
// ::FeatureRefine::FeatureRefine(), initializes Refine from "GetBool(\"RefineModel\", true)";
// Feature.cpp::getPDRefineModelParameter() returns "GetBool(\"RefineModel\", true)".
bool readPartDesignFeatureRefine(const app::DocumentObject& object);
std::optional<RefineShapeResult> applyPartDesignFeatureRefineProperty(
    const app::DocumentObject& object,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
);
std::optional<RefineShapeResult> applyPartDesignFeatureRefinePropertyForOwner(
    const app::DocumentObject& propertyObject,
    const std::string& outputOwner,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
);

} // namespace cad_core::runtime
