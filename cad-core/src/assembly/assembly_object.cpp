#include "cad_core/assembly/assembly_object.h"

#include "assembly_support.h"
#include "cad_core/runtime/feature_executor.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <vector>

namespace cad_core::assembly {

void executeAssemblyObject(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::execute(), calls "App::Part::execute()" before optional solve().
    // cad-core currently exposes the request-local grouped display shape and explicit solve gap.
    if (!runtime::rejectUnsupportedProperties(object, context, {"Group"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    nlohmann::json group = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<part::NamedShapeSource> sources;
    for (const auto& link : app::readLinks(object, "Group")) {
        group.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt != context.shapes.end()) {
            shapes.push_back(shapeIt->second.shape);
            const auto namedShapeIt = context.namedShapes.find(link.object);
            sources.push_back(part::NamedShapeSource{
                link.object,
                shapeIt->second.shape,
                namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
            });
        }
    }

    const auto solver = assembly_detail::solverSummary(object, context);
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
