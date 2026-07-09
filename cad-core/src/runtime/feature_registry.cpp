#include "cad_core/runtime/feature_registry.h"

#include "cad_core/app/link.h"
#include "cad_core/assembly/assembly_link.h"
#include "cad_core/assembly/assembly_object.h"
#include "cad_core/assembly/joint_group.h"
#include "cad_core/part_design/body.h"
#include "cad_core/part_design/feature_chamfer.h"
#include "cad_core/part_design/datum_coordinate_system.h"
#include "cad_core/part_design/datum_line.h"
#include "cad_core/part_design/datum_plane.h"
#include "cad_core/part_design/datum_point.h"
#include "cad_core/part_design/feature_draft.h"
#include "cad_core/part_design/feature_base.h"
#include "cad_core/part_design/feature_boolean.h"
#include "cad_core/part_design/feature_fillet.h"
#include "cad_core/part_design/feature_groove.h"
#include "cad_core/part_design/feature_hole.h"
#include "cad_core/part_design/feature_linear_pattern.h"
#include "cad_core/part_design/feature_loft.h"
#include "cad_core/mesh/feature_mesh_import.h"
#include "cad_core/part_design/feature_mirrored.h"
#include "cad_core/part_design/feature_multi_transform.h"
#include "cad_core/part/part_boolean.h"
#include "cad_core/part/part_feature.h"
#include "cad_core/part/part_geometry_curve.h"
#include "cad_core/part_design/feature_pad.h"
#include "cad_core/part_design/feature_pipe.h"
#include "cad_core/part_design/feature_pocket.h"
#include "cad_core/part_design/feature_polar_pattern.h"
#include "cad_core/part_design/feature_revolution.h"
#include "cad_core/part_design/feature_scaled.h"
#include "cad_core/part_design/feature_shape_binder.h"
#include "cad_core/sketcher/sketch_object.h"
#include "cad_core/part_design/feature_thicken.h"
#include "cad_core/part_design/feature_thickness.h"
#include "cad_core/runtime/topo_naming_state_probe.h"

#include <utility>

namespace cad_core::runtime
{

void FeatureRegistry::registerExecutor(std::string typeId, runtime::ExecuteFn executor)
{
    executors_[std::move(typeId)] = executor;
}

runtime::ExecuteFn FeatureRegistry::executorFor(const std::string& typeId) const
{
    const auto it = executors_.find(typeId);
    return it == executors_.end() ? nullptr : it->second;
}

std::vector<std::string> FeatureRegistry::typeIds() const
{
    std::vector<std::string> ids;
    ids.reserve(executors_.size());
    for (const auto& [typeId, executor] : executors_) {
        (void)executor;
        ids.push_back(typeId);
    }
    return ids;
}

FeatureRegistry buildDefaultRegistry()
{
    FeatureRegistry registry;
    registry.registerExecutor("Sketcher::SketchObject", sketcher::executeSketchObject);
    registry.registerExecutor("Mesh::Import", mesh::executeMeshImport);
    registry.registerExecutor("App::Part", part::executePart);
    registry.registerExecutor("App::Link", app::executeAppLink);
    registry.registerExecutor("App::LinkElement", app::executeAppLinkElement);
    registry.registerExecutor("App::LinkGroup", app::executeAppLinkGroup);
    registry.registerExecutor("App::DocumentObjectGroup", app::executeDocumentObjectGroup);
    registry.registerExecutor("App::DocumentObjectGroupPython", app::executeDocumentObjectGroup);
    registry.registerExecutor("App::FeaturePython", assembly::executeAssemblyFeaturePython);
    registry.registerExecutor("Assembly::AssemblyObject", assembly::executeAssemblyObject);
    registry.registerExecutor("Assembly::AssemblyLink", assembly::executeAssemblyLink);
    registry.registerExecutor("Assembly::JointGroup", assembly::executeAssemblyJointGroup);
    registry.registerExecutor("Part::Vertex", part::executePartVertex);
    registry.registerExecutor("Part::Line", part::executePartLine);
    registry.registerExecutor("Part::Plane", part::executePartPlane);
    registry.registerExecutor("Part::Box", part::executePartBox);
    registry.registerExecutor("Part::Cylinder", part::executePartCylinder);
    registry.registerExecutor("Part::Prism", part::executePartPrism);
    registry.registerExecutor("Part::RegularPolygon", part::executePartRegularPolygon);
    registry.registerExecutor("Part::Sphere", part::executePartSphere);
    registry.registerExecutor("Part::Ellipsoid", part::executePartEllipsoid);
    registry.registerExecutor("Part::Cone", part::executePartCone);
    registry.registerExecutor("Part::Torus", part::executePartTorus);
    registry.registerExecutor("Part::Wedge", part::executePartWedge);
    registry.registerExecutor("Part::Ellipse", part::executePartEllipse);
    registry.registerExecutor("Part::GeometryCurve", part::executePartGeometryCurve);
    registry.registerExecutor("Part::Helix", part::executePartHelix);
    registry.registerExecutor("Part::Spiral", part::executePartSpiral);
    registry.registerExecutor("Part::Compound", part::executePartCompound);
    registry.registerExecutor("Part::Compound2", part::executePartCompound);
    registry.registerExecutor("Part::Extrusion", part::executePartExtrusion);
    registry.registerExecutor("Part::RuledSurface", part::executePartRuledSurface);
    registry.registerExecutor("Part::Loft", part::executePartLoft);
    registry.registerExecutor("Part::Sweep", part::executePartSweep);
    registry.registerExecutor("Part::ProjectOnSurface", part::executePartProjectOnSurface);
    registry.registerExecutor("Part::FilledFace", part::executePartFilledFace);
    registry.registerExecutor("Part::GeomPlateSurface", part::executePartGeomPlateSurface);
    registry.registerExecutor("Part::Offset", part::executePartOffset);
    registry.registerExecutor("Part::Offset2D", part::executePartOffset2D);
    registry.registerExecutor("Part::Thickness", part::executePartThickness);
    registry.registerExecutor("Part::ImportBrep", part::executePartImportBrep);
    registry.registerExecutor("Part::ImportStep", part::executePartImportStep);
    registry.registerExecutor("Part::ImportIges", part::executePartImportIges);
    registry.registerExecutor("Part::Fuse", part::executePartFuse);
    registry.registerExecutor("Part::Cut", part::executePartCut);
    registry.registerExecutor("Part::Common", part::executePartCommon);
    registry.registerExecutor("Part::Section", part::executePartSection);
    registry.registerExecutor("Part::MultiFuse", part::executePartMultiFuse);
    registry.registerExecutor("Part::MultiCommon", part::executePartMultiCommon);
    registry.registerExecutor("Part::XOR", part::executePartXor);
    registry.registerExecutor("Part::FeatureXOR", part::executePartXor);
    registry.registerExecutor("Part::BooleanFragments", part::executePartBooleanFragments);
    registry.registerExecutor("Part::FeatureBooleanFragments", part::executePartBooleanFragments);
    registry.registerExecutor("CadCore::TopoNamingStateProbe", runtime::executeTopoNamingStateProbe);
    registry.registerExecutor("App::Origin", part_design::executeDatumCoordinateSystem);
    registry.registerExecutor("App::Line", part_design::executeAppLine);
    registry.registerExecutor("App::Plane", part_design::executeDatumPlane);
    registry.registerExecutor("App::Point", part_design::executeDatumPoint);
    registry.registerExecutor("PartDesign::Body", part_design::executeBody);
    registry.registerExecutor("PartDesign::CoordinateSystem", part_design::executeDatumCoordinateSystem);
    registry.registerExecutor("PartDesign::Line", part_design::executeDatumLine);
    registry.registerExecutor("PartDesign::Plane", part_design::executeDatumPlane);
    registry.registerExecutor("PartDesign::Point", part_design::executeDatumPoint);
    registry.registerExecutor("PartDesign::FeatureBase", part_design::executeFeatureBase);
    registry.registerExecutor("PartDesign::Boolean", part_design::executeBoolean);
    registry.registerExecutor("PartDesign::Fillet", part_design::executeFillet);
    registry.registerExecutor("PartDesign::Draft", part_design::executeDraft);
    registry.registerExecutor("PartDesign::Thicken", part_design::executeThicken);
    registry.registerExecutor("PartDesign::Thickness", part_design::executeThickness);
    registry.registerExecutor("PartDesign::Hole", part_design::executeHole);
    registry.registerExecutor("PartDesign::LinearPattern", part_design::executeLinearPattern);
    registry.registerExecutor("PartDesign::Mirrored", part_design::executeMirrored);
    registry.registerExecutor("PartDesign::MultiTransform", part_design::executeMultiTransform);
    registry.registerExecutor("PartDesign::Pad", part_design::executePad);
    registry.registerExecutor("PartDesign::AdditivePipe", part_design::executeAdditivePipe);
    registry.registerExecutor("PartDesign::SubtractivePipe", part_design::executeSubtractivePipe);
    registry.registerExecutor("PartDesign::Pocket", part_design::executePocket);
    registry.registerExecutor("PartDesign::AdditiveLoft", part_design::executeAdditiveLoft);
    registry.registerExecutor("PartDesign::SubtractiveLoft", part_design::executeSubtractiveLoft);
    registry.registerExecutor("PartDesign::Revolution", part_design::executeRevolution);
    registry.registerExecutor("PartDesign::Groove", part_design::executeGroove);
    registry.registerExecutor("PartDesign::PolarPattern", part_design::executePolarPattern);
    registry.registerExecutor("PartDesign::Scaled", part_design::executeScaled);
    registry.registerExecutor("PartDesign::Chamfer", part_design::executeChamfer);
    registry.registerExecutor("PartDesign::ShapeBinder", part_design::executeShapeBinder);
    registry.registerExecutor("PartDesign::SubShapeBinder", part_design::executeSubShapeBinder);
    registry.registerExecutor("PartDesign::SubShapeBinderPython", part_design::executeSubShapeBinder);
    return registry;
}

}  // namespace cad_core::runtime
