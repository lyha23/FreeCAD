#include "cad_core/part_design/datum_line.h"

#include "datum_attachment.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <utility>

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

void executeLineDatum(const app::DocumentObject& object,
                      runtime::ComputeContext& context,
                      const gp_Dir& baseDirection,
                      const std::string& datumKind,
                      detail::DatumAttachmentEngine attachmentEngine)
{
    if (!runtime::rejectUnsupportedProperties(object,
                                              context,
                                              {"ResizeMode",
                                               "Length",
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
    const auto attachment = detail::datumAttachmentPlacement(object, context, attachmentEngine);
    if (!attachment) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    BRepBuilderAPI_MakeEdge builder(gp_Lin(gp_Pnt(0, 0, 0), baseDirection));
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
    gp_Dir direction = baseDirection;
    const gp_Trsf placement = attachment->placement;
    shape = base::transformShape(shape, placement);
    base.Transform(placement);
    direction.Transform(placement);
    context.globalPlacements[object.name] = placement;

    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::DatumLine, shape};
    nlohmann::json result = {
        {"status", "ok"},
        {"datum", datumKind},
        {"attached", attachment->attached},
        {"base", pointToJson(base)},
        {"direction", directionToJson(direction)},
    };
    if (attachment->attached) {
        result["map_mode"] = attachment->mapMode;
        if (attachment->aliasSourceMode != attachment->mapMode) {
            result["alias_source_mode"] = attachment->aliasSourceMode;
        }
    }
    context.objects[object.name] = std::move(result);
}

}  // namespace

void executeDatumLine(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp::Line::Line(),
    // creates "BRepBuilderAPI_MakeEdge(gp_Lin(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)))"; getDirection()
    // returns Placement rotation applied to "Base::Vector3d(0, 0, 1)".
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp::DatumElement::DatumElement()
    // adds "Role" for lookup by LocalCoordinateSystem::getDatumElement().
    executeLineDatum(object, context, gp_Dir(0, 0, 1), "line", detail::DatumAttachmentEngine::Line);
}

void executeAppLine(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Datums.cpp::Line::Line(),
    // calls "setBaseDirection(Base::Vector3d(1, 0, 0))"; DatumElement::getDirection()
    // then rotates that base direction by the object's Placement.
    executeLineDatum(object, context, gp_Dir(1, 0, 0), "app_line", detail::DatumAttachmentEngine::Line);
}

}  // namespace cad_core::part_design
