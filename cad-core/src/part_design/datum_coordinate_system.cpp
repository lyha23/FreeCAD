#include "cad_core/part_design/datum_coordinate_system.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
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

void executeDatumCoordinateSystem(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumCS.cpp
    // ::CoordinateSystem::CoordinateSystem(), creates "BRepBuilderAPI_MakeFace(gp_Pln(...,
    // gp_Dir(0, 0, 1)))"; getXAxis/getYAxis/getZAxis apply Placement rotation to unit axes.
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Origin.cpp::Origin::Origin()
    // documents "App::Origin is a LCS for which placement is fixed to identity".
    if (!runtime::rejectUnsupportedProperties(
            object, context, {"ResizeMode", "Length", "Width", "AttachmentSupport", "MapMode", "OriginFeatures"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeFace builder(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    if (!builder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build CoordinateSystem face",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape shape = builder.Shape();
    gp_Pnt origin(0, 0, 0);
    gp_Dir xAxis(1, 0, 0);
    gp_Dir yAxis(0, 1, 0);
    gp_Dir zAxis(0, 0, 1);
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt != context.globalPlacements.end()) {
        const gp_Trsf& placement = placementIt->second;
        shape = base::transformShape(shape, placement);
        origin.Transform(placement);
        xAxis.Transform(placement);
        yAxis.Transform(placement);
        zAxis.Transform(placement);
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::DatumCoordinateSystem, shape};
    context.objects[object.name] = {
        {"status", "ok"},
        {"datum", object.typeId == "App::Origin" ? "origin" : "coordinate_system"},
        {"origin", pointToJson(origin)},
        {"x_axis", directionToJson(xAxis)},
        {"y_axis", directionToJson(yAxis)},
        {"z_axis", directionToJson(zAxis)},
    };
}

}  // namespace cad_core::part_design
