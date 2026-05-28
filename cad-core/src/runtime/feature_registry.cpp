#include "cad_core/runtime/feature_registry.h"

#include "cad_core/features/body.h"
#include "cad_core/features/pad.h"
#include "cad_core/features/sketch_object.h"

#include <utility>

namespace cad_core::runtime {

void FeatureRegistry::registerExecutor(std::string typeId, features::ExecuteFn executor)
{
    executors_[std::move(typeId)] = executor;
}

features::ExecuteFn FeatureRegistry::executorFor(const std::string& typeId) const
{
    const auto it = executors_.find(typeId);
    return it == executors_.end() ? nullptr : it->second;
}

FeatureRegistry buildDefaultRegistry()
{
    FeatureRegistry registry;
    registry.registerExecutor("Sketcher::SketchObject", features::executeSketchObject);
    registry.registerExecutor("PartDesign::Body", features::executeBody);
    registry.registerExecutor("PartDesign::Pad", features::executePad);
    return registry;
}

}  // namespace cad_core::runtime

