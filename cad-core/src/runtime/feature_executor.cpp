#include "cad_core/runtime/feature_executor.h"

#include "cad_core/part/topo_shape.h"

namespace cad_core::runtime
{
namespace
{

std::optional<RefineShapeResult> applyRefinePropertyForOwner(
    const app::DocumentObject& propertyObject,
    const std::string& outputOwner,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape,
    bool refine
)
{
    if (!refine) {
        return RefineShapeResult {shape, namedShape, false};
    }

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
    // ::FeatureRefine::refineShapeIfActive(), calls "shape.makeElementRefine()" when
    // Refine is true and returns the old shape only if the maker throws under Warn policy.
    const part::NamedShapeSource source {
        namedShape ? namedShape->owner : outputOwner,
        shape,
        namedShape ? &*namedShape : nullptr,
    };
    part::NamedShapeBuild refined = part::makeElementRefineFromSource(outputOwner, source);
    if (refined.error.empty() && !refined.shape.IsNull()) {
        return RefineShapeResult {refined.shape, refined.namedShape, true};
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "warning",
        "refine_failed",
        refined.error.empty() ? "Refine operation failed; keeping the unrefined shape" : refined.error,
        propertyObject.name,
        "Refine"
    );
    return RefineShapeResult {shape, namedShape, false};
}

} // namespace

bool rejectUnsupportedProperties(const app::DocumentObject& object,
                                 ComputeContext& context,
                                 const std::set<std::string>& allowed)
{
    const std::set<std::string> commonAllowed = {
        "Label",
        "Label2",
        "ExpressionEngine",
        "Visibility",
        "Placement",
        "_ElementMapVersion",
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getOnChangeCopyObjects() reads hidden
        // "_CopyOnChangeControl"; ::syncCopyOnChange() matches old/new copies through copy
        // provenance. cad-core keeps equivalent hidden metadata request-local so copied
        // objects can recompute as normal feature objects after the frontend persists updates.
        "_CopyOnChangeControl",
        "_CopyOnChangeOwner",
        "_CopyOnChangeSourceObject",
        "_CopyOnChangeSourceId",
    };

    bool ok = true;
    for (const auto& item : object.properties.items()) {
        if (allowed.count(item.key()) == 0U && commonAllowed.count(item.key()) == 0U) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_property",
                "Property " + item.key() + " is not supported for " + object.typeId + " in the MVP",
                object.name,
                item.key()
            );
            ok = false;
        }
    }
    return ok;
}

bool rejectActiveRefineProperty(const app::DocumentObject& object, ComputeContext& context)
{
    const auto refine = app::readBool(object, "Refine");
    if (!refine.value_or(false)) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
    // ::FeatureRefine::refineShapeIfActive(), "if (!this->Refine.getValue()) return oldShape".
    // Keep this rejection only for feature families whose FreeCAD refine point is not yet
    // represented in cad-core's execution model.
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Refine=true for this feature requires final-result RefineModel integration",
        object.name,
        "Refine"
    );
    return false;
}

bool isFeatureGroupedByBody(const app::DocumentObject& object, const ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // walks Group features to build the Body Tip result; AddSub feature refine belongs to that
    // final Body result instead of the intermediate add/sub tool shape.
    const auto parentIt = context.parentGroupByObject.find(object.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return false;
    }
    const auto parentObjectIt = context.documentObjects.find(parentIt->second);
    return parentObjectIt != context.documentObjects.end() && parentObjectIt->second != nullptr
        && parentObjectIt->second->typeId == "PartDesign::Body";
}

bool shouldBuildDisplayTopology(const app::DocumentObject& object, const ComputeContext& context)
{
    if (context.targetObjects.count(object.name) != 0U) {
        return true;
    }
    return !isFeatureGroupedByBody(object, context);
}

std::optional<RefineShapeResult> applyRefineProperty(
    const app::DocumentObject& object,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
)
{
    return applyRefinePropertyForOwner(object, object.name, context, shape, namedShape);
}

std::optional<RefineShapeResult> applyRefinePropertyForOwner(
    const app::DocumentObject& propertyObject,
    const std::string& outputOwner,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
)
{
    const auto refine = app::readBool(propertyObject, "Refine");
    return applyRefinePropertyForOwner(propertyObject, outputOwner, context, shape, namedShape, refine.value_or(false));
}

bool readPartDesignFeatureRefine(const app::DocumentObject& object)
{
    const auto refine = app::readBool(object, "Refine");
    if (refine) {
        return *refine;
    }

    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
    // ::FeatureRefine::FeatureRefine(), initializes Refine from "GetBool(\"RefineModel\", true)".
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::getPDRefineModelParameter(), returns "GetBool(\"RefineModel\", true)".
    return true;
}

std::optional<RefineShapeResult> applyPartDesignFeatureRefineProperty(
    const app::DocumentObject& object,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
)
{
    return applyPartDesignFeatureRefinePropertyForOwner(object, object.name, context, shape, namedShape);
}

std::optional<RefineShapeResult> applyPartDesignFeatureRefinePropertyForOwner(
    const app::DocumentObject& propertyObject,
    const std::string& outputOwner,
    ComputeContext& context,
    const TopoDS_Shape& shape,
    const std::optional<part::NamedShape>& namedShape
)
{
    return applyRefinePropertyForOwner(
        propertyObject,
        outputOwner,
        context,
        shape,
        namedShape,
        readPartDesignFeatureRefine(propertyObject)
    );
}

} // namespace cad_core::runtime
