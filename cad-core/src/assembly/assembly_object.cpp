#include "cad_core/assembly/assembly_object.h"

#include "assembly_support.h"
#include "cad_core/base/placement.h"
#include "cad_core/runtime/feature_executor.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <vector>

namespace cad_core::assembly {
namespace {

std::map<std::string, app::Placement> placementUpdatesByObject(
    const std::vector<AssemblyPlacementUpdate>& updates)
{
    std::map<std::string, app::Placement> placements;
    for (const AssemblyPlacementUpdate& update : updates) {
        placements[update.object] = update.placement;
    }
    return placements;
}

gp_Trsf parentPlacementForObject(const app::DocumentObject& object,
                                 const runtime::ComputeContext& context)
{
    const auto parentIt = context.parentGroupByObject.find(object.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return gp_Trsf();
    }
    const auto placementIt = context.globalPlacements.find(parentIt->second);
    return placementIt == context.globalPlacements.end() ? gp_Trsf() : placementIt->second;
}

gp_Trsf placementPropertyTransform(const app::Placement& placement)
{
    return base::placementFromComponents(placement.base, placement.rotation);
}

gp_Trsf linkDisplayPlacement(const app::DocumentObject& object,
                             const runtime::ComputeContext& context,
                             const std::optional<app::Placement>& placementOverride)
{
    gp_Trsf local;
    if (placementOverride) {
        local = placementPropertyTransform(*placementOverride);
    }
    else if (const auto linkPlacement = app::readPlacement(object, "LinkPlacement")) {
        local = placementPropertyTransform(*linkPlacement);
    }
    else if (const auto placement = app::readPlacement(object, "Placement")) {
        local = placementPropertyTransform(*placement);
    }
    return parentPlacementForObject(object, context) * local;
}

TopoDS_Shape applySolverPlacementToDisplayShape(
    const TopoDS_Shape& shape,
    const app::DocumentObject& child,
    const runtime::ComputeContext& context,
    const std::map<std::string, app::Placement>& solverPlacements)
{
    const auto updateIt = solverPlacements.find(child.name);
    if (updateIt == solverPlacements.end()) {
        return shape;
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::setNewPlacements(), writes "propPlacement->setValue(newPlacement)" after
    // the Ondsel run. CAD Core keeps the persistent writeback in documentObjectUpdates, and applies
    // the same request-local Placement to the Assembly display compound summary.
    const gp_Trsf oldPlacement = linkDisplayPlacement(child, context, std::nullopt);
    const gp_Trsf newPlacement = linkDisplayPlacement(child, context, updateIt->second);
    return base::transformShape(shape, newPlacement * oldPlacement.Inverted());
}

}  // namespace

void executeAssemblyObject(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::execute(), calls "App::Part::execute()" before optional solve().
    // cad-core currently exposes the request-local grouped display shape and explicit solve gap.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Group"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto solver = assembly_detail::solverSummary(object, context);
    const auto solverPlacements = placementUpdatesByObject(solver.placementUpdates);

    nlohmann::json group = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<part::NamedShapeSource> sources;
    for (const auto& link : app::readLinks(object, "Group")) {
        group.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt != context.shapes.end()) {
            const app::DocumentObject* child = assembly_detail::documentObjectByName(context, link.object);
            const TopoDS_Shape displayShape =
                child == nullptr
                    ? shapeIt->second.shape
                    : applySolverPlacementToDisplayShape(
                          shapeIt->second.shape,
                          *child,
                          context,
                          solverPlacements
                      );
            shapes.push_back(displayShape);
            const auto namedShapeIt = context.namedShapes.find(link.object);
            sources.push_back(part::NamedShapeSource{
                link.object,
                displayShape,
                namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
            });
        }
    }

    nlohmann::json metadata = {
        {"assembly", "object"},
        {"group", group},
        {"joint_groups", assembly_detail::jointGroupNames(object, context)},
        {"joints", assembly_detail::jointNames(object, context)},
        {"solve", solver.solve},
        {"solver_adapter", solver.adapter},
    };
    if (shapes.empty()) {
        assembly_detail::publishEmptyResult(object, context, metadata);
        return;
    }

    const TopoDS_Shape shape = assembly_detail::compoundOf(shapes);
    assembly_detail::publishLinkedShape(
        object,
        context,
        shape,
        assembly_detail::shapeKindForShape(shape),
        metadata,
        part::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

}  // namespace cad_core::assembly
