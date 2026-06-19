#include "cad_core/part_design/feature_revolution.h"

#include "cad_core/part_design/feature_revolved.h"

namespace cad_core::part_design {

void executeRevolution(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeRevolvedFeature(object, context, RevolvedAddSubMode::Additive, "Revolution");
}

}  // namespace cad_core::part_design
