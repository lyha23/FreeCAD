#include "cad_core/part_design/datum_point.h"

#include "datum_attachment.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace cad_core::part_design {

namespace {

nlohmann::json pointToJson(const gp_Pnt& point)
{
    return nlohmann::json::array({point.X(), point.Y(), point.Z()});
}

}  // namespace

void executeDatumPoint(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumPoint.cpp::Point::makeShape(),
    // creates "BRepBuilderAPI_MakeVertex(gp_Pnt(0, 0, 0))" and then sets the shape placement
    // from the object's Placement.
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp::DatumElement::DatumElement()
    // adds "Role" for lookup by LocalCoordinateSystem::getDatumElement().
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"AttachmentSupport",
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
    const auto attachment = detail::datumAttachmentPlacement(object, context, detail::DatumAttachmentEngine::Point);
    if (!attachment) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeVertex builder(gp_Pnt(0, 0, 0));
    if (!builder.IsDone()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "OCCT could not build DatumPoint vertex",
                               object.name);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape shape = builder.Shape();
    gp_Pnt point(0, 0, 0);
    const gp_Trsf placement = attachment->placement;
    shape = base::transformShape(shape, placement);
    point.Transform(placement);
    context.globalPlacements[object.name] = placement;

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::DatumPoint, shape};
    context.objects[object.name] = {
        {"status", "ok"},
        {"datum", "point"},
        {"attached", attachment->attached},
        {"point", pointToJson(point)},
    };
}

}  // namespace cad_core::part_design
