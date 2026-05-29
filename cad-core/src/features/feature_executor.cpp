#include "cad_core/features/feature_executor.h"

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
    // ::FeatureRefine::refineShapeIfActive(), "if (!this->Refine.getValue()) return oldShape";
    // otherwise calls "shape.makeElementRefine()", which cad-core does not expose until the
    // BRepBuilderAPI_RefineModel / FaceUniter maker path is migrated.
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Refine=true requires the FreeCAD BRepBuilderAPI_RefineModel maker path",
                           object.name,
                           "Refine");
    return false;
}

}  // namespace cad_core::features
