#include "cad_core/part_design/datum_line.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace cad_core::part_design {

namespace {

nlohmann::json pointToJson(const gp_Pnt& point)
{
    return nlohmann::json::array({point.X(), point.Y(), point.Z()});
}

nlohmann::json directionToJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

}  // namespace

void executeDatumLine(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp::Line::Line(),
    // creates "BRepBuilderAPI_MakeEdge(gp_Lin(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)))"; getDirection()
    // returns Placement rotation applied to "Base::Vector3d(0, 0, 1)".
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp::DatumElement::DatumElement()
    // adds "Role" for lookup by LocalCoordinateSystem::getDatumElement().
    if (!runtime::rejectUnsupportedProperties(object, context, {"ResizeMode", "Length", "AttachmentSupport", "MapMode", "Role"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeEdge builder(gp_Lin(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    if (!builder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build DatumLine edge",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape shape = builder.Shape();
    gp_Pnt base(0, 0, 0);
    gp_Dir direction(0, 0, 1);
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt != context.globalPlacements.end()) {
        const gp_Trsf& placement = placementIt->second;
        shape = base::transformShape(shape, placement);
        base.Transform(placement);
        direction.Transform(placement);
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::DatumLine, shape};
    context.objects[object.name] = {
        {"status", "ok"},
        {"datum", "line"},
        {"base", pointToJson(base)},
        {"direction", directionToJson(direction)},
    };
}

}  // namespace cad_core::part_design
