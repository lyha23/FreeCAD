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
#include "cad_core/features/link.h"
#include "cad_core/features/linear_pattern.h"
#include "cad_core/features/mesh.h"
#include "cad_core/features/mirrored.h"
#include "cad_core/features/multi_transform.h"
#include "cad_core/features/part.h"
#include "cad_core/features/part_boolean.h"
#include "cad_core/features/pad.h"
#include "cad_core/features/pocket.h"
#include "cad_core/features/polar_pattern.h"
#include "cad_core/features/scaled.h"
#include "cad_core/features/sketch_object.h"

#include <utility>

namespace cad_core::runtime
{

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
    registry.registerExecutor("Mesh::Import", features::executeMeshImport);
    registry.registerExecutor("App::Part", features::executePart);
    registry.registerExecutor("App::Link", features::executeAppLink);
    registry.registerExecutor("App::LinkElement", features::executeAppLinkElement);
    registry.registerExecutor("App::LinkGroup", features::executeAppLinkGroup);
    registry.registerExecutor("Assembly::AssemblyObject", features::executeAssemblyObject);
    registry.registerExecutor("Assembly::AssemblyLink", features::executeAssemblyLink);
    registry.registerExecutor("Part::Vertex", features::executePartVertex);
    registry.registerExecutor("Part::Line", features::executePartLine);
    registry.registerExecutor("Part::Plane", features::executePartPlane);
    registry.registerExecutor("Part::Box", features::executePartBox);
    registry.registerExecutor("Part::Cylinder", features::executePartCylinder);
    registry.registerExecutor("Part::Prism", features::executePartPrism);
    registry.registerExecutor("Part::RegularPolygon", features::executePartRegularPolygon);
    registry.registerExecutor("Part::Sphere", features::executePartSphere);
    registry.registerExecutor("Part::Ellipsoid", features::executePartEllipsoid);
    registry.registerExecutor("Part::Cone", features::executePartCone);
    registry.registerExecutor("Part::Torus", features::executePartTorus);
    registry.registerExecutor("Part::Wedge", features::executePartWedge);
    registry.registerExecutor("Part::Ellipse", features::executePartEllipse);
    registry.registerExecutor("Part::Helix", features::executePartHelix);
    registry.registerExecutor("Part::Spiral", features::executePartSpiral);
    registry.registerExecutor("Part::ImportBrep", features::executePartImportBrep);
    registry.registerExecutor("Part::ImportStep", features::executePartImportStep);
    registry.registerExecutor("Part::ImportIges", features::executePartImportIges);
    registry.registerExecutor("Part::Fuse", features::executePartFuse);
    registry.registerExecutor("Part::Cut", features::executePartCut);
    registry.registerExecutor("Part::Common", features::executePartCommon);
    registry.registerExecutor("Part::Section", features::executePartSection);
    registry.registerExecutor("Part::MultiFuse", features::executePartMultiFuse);
    registry.registerExecutor("Part::MultiCommon", features::executePartMultiCommon);
    registry.registerExecutor("Part::XOR", features::executePartXor);
    registry.registerExecutor("Part::FeatureXOR", features::executePartXor);
    registry.registerExecutor("Part::BooleanFragments", features::executePartBooleanFragments);
    registry.registerExecutor("Part::FeatureBooleanFragments", features::executePartBooleanFragments);
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
