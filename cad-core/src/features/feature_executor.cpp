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

}  // namespace cad_core::features
