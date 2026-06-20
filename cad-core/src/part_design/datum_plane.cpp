#include "cad_core/part_design/datum_plane.h"

#include "datum_attachment.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

namespace cad_core::part_design {

void executeDatumPlane(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumPlane.cpp
    // ::Plane::Plane() creates a planar face on gp_Pln(..., gp_Dir(0,0,1)) and
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/DatumFeature.cpp::Datum::getShape()
    // applies the object's Placement to that datum shape.
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp::DatumElement::DatumElement()
    // adds "Role" for lookup by LocalCoordinateSystem::getDatumElement().
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"ResizeMode",
                                               "Length",
                                               "Width",
                                               "AttachmentSupport",
                                               "MapMode",
                                               "MapReversed",
                                               "Reverse",
                                               "MapPathParameter",
                                               "Parameter",
                                               "AttachmentOffset",
                                               "Role"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!detail::rejectUnsupportedDatumAttachment(object, context)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeFace builder(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    if (!builder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build DatumPlane face",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape shape = builder.Shape();
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt != context.globalPlacements.end()) {
        shape = base::transformShape(shape, placementIt->second);
    }

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::DatumPlane, shape};
    context.objects[object.name] = {
        {"status", "ok"},
        {"datum", "plane"},
    };
}

}  // namespace cad_core::part_design
