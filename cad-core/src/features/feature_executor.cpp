#include "cad_core/features/feature_executor.h"

#include "cad_core/topo/named_shape.h"

namespace cad_core::features {

bool rejectUnsupportedProperties(const document::DocumentObject& object,
                                 runtime::ComputeContext& context,
                                 const std::set<std::string>& allowed)
{
    const std::set<std::string> commonAllowed = {
        "Label",
        "Label2",
        "ExpressionEngine",
        "Visibility",
        "Placement",
        "_ElementMapVersion",
    };

    bool ok = true;
    for (const auto& item : object.properties.items()) {
        if (allowed.count(item.key()) == 0U && commonAllowed.count(item.key()) == 0U) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_property",
                                   "Property " + item.key() + " is not supported for " + object.typeId + " in the MVP",
                                   object.name,
                                   item.key());
            ok = false;
        }
    }
    return ok;
}

bool rejectActiveRefineProperty(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto refine = document::readBool(object, "Refine");
    if (!refine.value_or(false)) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
    // ::FeatureRefine::refineShapeIfActive(), "if (!this->Refine.getValue()) return oldShape".
    // Keep this rejection only for feature families whose FreeCAD refine point is not yet
    // represented in cad-core's execution model.
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Refine=true for this feature requires final-result RefineModel integration",
                           object.name,
                           "Refine");
    return false;
}

bool isFeatureGroupedByBody(const document::DocumentObject& object, const runtime::ComputeContext& context)
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

std::optional<RefineShapeResult> applyRefineProperty(const document::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const TopoDS_Shape& shape,
                                                     const std::optional<topo::NamedShape>& namedShape)
{
    return applyRefinePropertyForOwner(object, object.name, context, shape, namedShape);
}

std::optional<RefineShapeResult> applyRefinePropertyForOwner(const document::DocumentObject& propertyObject,
                                                             const std::string& outputOwner,
                                                             runtime::ComputeContext& context,
                                                             const TopoDS_Shape& shape,
                                                             const std::optional<topo::NamedShape>& namedShape)
{
    const auto refine = document::readBool(propertyObject, "Refine");
    if (!refine.value_or(false)) {
        return RefineShapeResult{shape, namedShape, false};
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp
    // ::FeatureRefine::refineShapeIfActive(), calls "shape.makeElementRefine()" when
    // Refine is true and returns the old shape only if the maker throws under Warn policy.
    const topo::NamedShapeSource source{
        namedShape ? namedShape->owner : outputOwner,
        shape,
        namedShape ? &*namedShape : nullptr,
    };
    topo::NamedShapeBuild refined = topo::makeElementRefineFromSource(outputOwner, source);
    if (refined.error.empty() && !refined.shape.IsNull()) {
        return RefineShapeResult{refined.shape, refined.namedShape, true};
    }

    runtime::addDiagnostic(context.diagnostics,
                           "warning",
                           "refine_failed",
                           refined.error.empty() ? "Refine operation failed; keeping the unrefined shape" : refined.error,
                           propertyObject.name,
                           "Refine");
    return RefineShapeResult{shape, namedShape, false};
}

}  // namespace cad_core::features
