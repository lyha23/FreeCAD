#include "cad_core/runtime/feature_registry.h"

#include "cad_core/features/body.h"
#include "cad_core/features/chamfer.h"
#include "cad_core/features/datum_coordinate_system.h"
#include "cad_core/features/datum_line.h"
#include "cad_core/features/datum_plane.h"
#include "cad_core/features/datum_point.h"
#include "cad_core/features/feature_base.h"
#include "cad_core/features/fillet.h"
#include "cad_core/features/hole.h"
#include "cad_core/features/linear_pattern.h"
#include "cad_core/features/mirrored.h"
#include "cad_core/features/multi_transform.h"
#include "cad_core/features/part.h"
#include "cad_core/features/pad.h"
#include "cad_core/features/pocket.h"
#include "cad_core/features/polar_pattern.h"
#include "cad_core/features/scaled.h"
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
    registry.registerExecutor("App::Part", features::executePart);
    registry.registerExecutor("Part::Box", features::executePartBox);
    registry.registerExecutor("Part::Cylinder", features::executePartCylinder);
    registry.registerExecutor("App::Origin", features::executeDatumCoordinateSystem);
    registry.registerExecutor("PartDesign::Body", features::executeBody);
    registry.registerExecutor("PartDesign::CoordinateSystem", features::executeDatumCoordinateSystem);
    registry.registerExecutor("PartDesign::Line", features::executeDatumLine);
    registry.registerExecutor("PartDesign::Plane", features::executeDatumPlane);
    registry.registerExecutor("PartDesign::Point", features::executeDatumPoint);
    registry.registerExecutor("PartDesign::FeatureBase", features::executeFeatureBase);
    registry.registerExecutor("PartDesign::Fillet", features::executeFillet);
    registry.registerExecutor("PartDesign::Hole", features::executeHole);
    registry.registerExecutor("PartDesign::LinearPattern", features::executeLinearPattern);
    registry.registerExecutor("PartDesign::Mirrored", features::executeMirrored);
    registry.registerExecutor("PartDesign::MultiTransform", features::executeMultiTransform);
    registry.registerExecutor("PartDesign::Pad", features::executePad);
    registry.registerExecutor("PartDesign::Pocket", features::executePocket);
    registry.registerExecutor("PartDesign::PolarPattern", features::executePolarPattern);
    registry.registerExecutor("PartDesign::Scaled", features::executeScaled);
    registry.registerExecutor("PartDesign::Chamfer", features::executeChamfer);
    return registry;
}

}  // namespace cad_core::runtime
