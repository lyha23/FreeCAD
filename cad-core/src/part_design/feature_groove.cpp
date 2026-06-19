#include "cad_core/part_design/feature_groove.h"

#include "cad_core/part_design/feature_revolved.h"

namespace cad_core::part_design {

void executeGroove(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    executeRevolvedFeature(object, context, RevolvedAddSubMode::Subtractive, "Groove");
}

}  // namespace cad_core::part_design
