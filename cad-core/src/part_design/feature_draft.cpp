#include "cad_core/part_design/feature_draft.h"

#include "feature_dress_up_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAPI_IntSS.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <Geom_Plane.hxx>
#include <Geom_Surface.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design
{

namespace
{

using detail::applyDressUpRefine;
using detail::cacheDressUpAddSubShape;
using detail::DraftNeutralPlane;
using detail::DressUpResult;
using detail::propertyPayload;
using detail::publishDressUpResult;
using detail::readBoolProperty;
using detail::readNumberProperty;
using detail::resolveDressUpBase;
using detail::resolveReferenceSubshape;
using detail::selectedDraftFaces;

std::optional<TopoDS_Edge> firstEdge(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        return TopoDS::Edge(explorer.Current());
    }
    return std::nullopt;
}

std::optional<TopoDS_Face> firstFace(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        return TopoDS::Face(explorer.Current());
    }
    return std::nullopt;
}

std::optional<gp_Dir> lineDirectionFromEdge(
    const TopoDS_Edge& edge,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " reference edge must be linear",
            object.name,
            property
        );
        return std::nullopt;
    }
    return curve.Line().Direction();
}

std::optional<gp_Dir> resolveDraftPullDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const gp_Pln& neutralPlane
)
{
    const auto* property = app::propertyValue(object, "PullDirection");
    if (property == nullptr || property->raw.is_null()) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
        // when no PullDirection reference is set, "Choose pull direction normal to neutral plane".
        return neutralPlane.Axis().Direction();
    }

    const auto link = app::readLink(object, "PullDirection");
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "PullDirection must link to a datum line or linear EdgeN",
            object.name,
            "PullDirection"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "PullDirection target " + link->object + " did not produce a shape",
            object.name,
            "PullDirection",
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        const auto edge = firstEdge(shapeIt->second.shape);
        if (!edge) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "PullDirection DatumLine has no edge shape",
                object.name,
                "PullDirection",
                "runtime",
                link->object
            );
            return std::nullopt;
        }
        return lineDirectionFromEdge(*edge, object, context, "PullDirection");
    }

    TopoDS_Edge edge;
    if (link->subnames.empty() && shapeIt->second.kind == runtime::ShapeValue::Kind::PartPrimitive) {
        const auto directEdge = firstEdge(shapeIt->second.shape);
        if (!directEdge) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "PullDirection primitive must be a line or select a linear EdgeN",
                object.name,
                "PullDirection",
                "runtime",
                link->object
            );
            return std::nullopt;
        }
        edge = *directEdge;
    }
    else {
        const auto subshape
            = resolveReferenceSubshape(*link, object, context, "PullDirection", TopAbs_EDGE);
        if (!subshape) {
            return std::nullopt;
        }
        edge = TopoDS::Edge(*subshape);
    }
    return lineDirectionFromEdge(edge, object, context, "PullDirection");
}

std::optional<gp_Pln> planeFromFace(
    const TopoDS_Face& face,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " reference face must be planar",
            object.name,
            property
        );
        return std::nullopt;
    }
    return surface.Plane();
}

std::optional<DraftNeutralPlane> guessDraftNeutralPlaneFromFace(
    const TopoDS_Face& face,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
    // with no NeutralPlane, maps edges from the first selected face; circular edges use
    // "gp_Pln(p1, c.Circle().Axis().Direction())", while linear edges intersect an auxiliary
    // plane with the face surface and use the resulting Geom_Line direction.
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(face, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        try {
            BRepAdaptor_Curve curve(TopoDS::Edge(edges(index)));
            const gp_Pnt p1 = curve.Value(curve.FirstParameter());
            const gp_Pnt p2 = curve.Value(curve.LastParameter());

            if (curve.IsClosed()) {
                if (curve.GetType() == GeomAbs_Circle) {
                    return DraftNeutralPlane {
                        gp_Pln(p1, curve.Circle().Axis().Direction()),
                        "guessed_from_circular_edge"
                    };
                }
                continue;
            }

            if (p1.Distance(p2) <= Precision::Confusion()) {
                continue;
            }
            const gp_Pnt midpoint = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2.0);
            Handle(Geom_Plane) auxiliaryPlane
                = new Geom_Plane(midpoint, gp_Dir(p2.X() - p1.X(), p2.Y() - p1.Y(), p2.Z() - p1.Z()));
            BRepAdaptor_Surface surface(face, Standard_False);
            Handle(Geom_Surface) rawSurface = surface.Surface().Surface();
            GeomAPI_IntSS intersector(auxiliaryPlane, rawSurface, Precision::Confusion());
            if (!intersector.IsDone() || intersector.NbLines() < 1) {
                continue;
            }
            const Handle(Geom_Curve) & intersection = intersector.Line(1);
            if (!intersection->IsKind(STANDARD_TYPE(Geom_Line))) {
                continue;
            }
            Handle(Geom_Line) line = Handle(Geom_Line)::DownCast(intersection);
            return DraftNeutralPlane {
                gp_Pln(midpoint, line->Lin().Direction()),
                "guessed_from_linear_edge"
            };
        }
        catch (Standard_Failure&) {
            continue;
        }
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "execution_failed",
        "No neutral plane specified and none can be guessed",
        object.name,
        "NeutralPlane"
    );
    return std::nullopt;
}

std::optional<DraftNeutralPlane> resolveDraftNeutralPlane(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Face& firstSelectedFace
)
{
    const auto* property = app::propertyValue(object, "NeutralPlane");
    if (property == nullptr || property->raw.is_null()) {
        return guessDraftNeutralPlaneFromFace(firstSelectedFace, object, context);
    }

    const auto link = app::readLink(object, "NeutralPlane");
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "NeutralPlane must link to a datum plane or planar FaceN",
            object.name,
            "NeutralPlane"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "NeutralPlane target " + link->object + " did not produce a shape",
            object.name,
            "NeutralPlane",
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    TopoDS_Face face;
    if (link->subnames.empty()
        && (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPlane
            || shapeIt->second.kind == runtime::ShapeValue::Kind::PartPrimitive)) {
        const auto directFace = firstFace(shapeIt->second.shape);
        if (!directFace) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "NeutralPlane target must be a datum plane or select a planar FaceN",
                object.name,
                "NeutralPlane",
                "runtime",
                link->object
            );
            return std::nullopt;
        }
        face = *directFace;
    }
    else {
        const auto subshape
            = resolveReferenceSubshape(*link, object, context, "NeutralPlane", TopAbs_FACE);
        if (!subshape) {
            return std::nullopt;
        }
        face = TopoDS::Face(*subshape);
    }

    const auto plane = planeFromFace(face, object, context, "NeutralPlane");
    if (!plane) {
        return std::nullopt;
    }
    return DraftNeutralPlane {*plane, "explicit_reference"};
}

nlohmann::json directionJson(const gp_Dir& direction)
{
    return nlohmann::json::array({direction.X(), direction.Y(), direction.Z()});
}

std::optional<DressUpResult> buildDraft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto selection = selectedDraftFaces(*base, object, context);
    if (!selection) {
        return std::nullopt;
    }

    const double angleDegrees = readNumberProperty(object, "Angle", 1.5);
    bool reversed = readBoolProperty(object, "Reversed");

    part::NamedShapeSource baseSource {
        base->link.object,
        base->shape,
        base->namedShape ? &*base->namedShape : nullptr
    };
    if (selection->faces.empty()) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
        // when Base.getSubValuesStartsWith("Face") is empty, calls positionByBaseFeature()
        // and stores the unchanged TopShape instead of running the draft maker.
        part::NamedShape namedShape
            = part::namedShapeForPreservedSources(object.name, base->shape, {baseSource});
        DressUpResult result {
            "draft",
            base->link.object,
            *base,
            base->shape,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        result.selection = selection->evidence;
        result.parameters = {
            {"angle", angleDegrees},
            {"reversed", reversed},
            {"selected_faces", nlohmann::json::array()},
            {"mode", "copy_no_face_selection"},
        };
        return result;
    }

    const auto neutralPlane = resolveDraftNeutralPlane(object, context, selection->faces.front());
    if (!neutralPlane) {
        return std::nullopt;
    }
    const auto pullDirection = resolveDraftPullDirection(object, context, neutralPlane->plane);
    if (!pullDirection) {
        return std::nullopt;
    }

    double angle = angleDegrees * std::acos(-1.0) / 180.0;
    if (reversed) {
        angle *= -1.0;
    }

    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementDraft(), initializes BRepOffsetAPI_DraftAngle and calls
        // "mkDraft.Add(TopoDS::Face(...), pullDirection, angle, neutralPlane)" for every FaceN.
        std::vector<TopoDS_Face> faces = selection->faces;
        BRepOffsetAPI_DraftAngle maker;
        bool done = true;
        const bool retry = reversed;
        do {
            if (faces.empty()) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    "Draft operation has no usable faces",
                    object.name,
                    "Base"
                );
                return std::nullopt;
            }

            maker.Init(base->shape);
            done = true;
            for (auto it = faces.begin(); it != faces.end(); ++it) {
                maker.Add(*it, *pullDirection, angle, neutralPlane->plane);
                if (!maker.AddDone()) {
                    if (!retry) {
                        runtime::addDiagnostic(
                            context.diagnostics,
                            "error",
                            "execution_failed",
                            "Draft operation could not add selected face",
                            object.name,
                            "Base"
                        );
                        return std::nullopt;
                    }
                    done = false;
                    faces.erase(it);
                    break;
                }
            }
        } while (retry && !done);

        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Draft operation failed",
                object.name,
                "Base"
            );
            return std::nullopt;
        }

        TopoDS_Shape resultShape = maker.Shape();
        if (resultShape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Draft operation produced a null shape",
                object.name,
                "Base"
            );
            return std::nullopt;
        }

        part::NamedShape namedShape
            = part::namedShapeForMakerHistory(object.name, resultShape, {baseSource}, maker);
        DressUpResult result {
            "draft",
            base->link.object,
            *base,
            resultShape,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        result.selection = selection->evidence;
        result.parameters = {
            {"angle", angleDegrees},
            {"reversed", reversed},
            {"selected_faces", selection->selectedFaceSubnames},
            {"pull_direction", directionJson(*pullDirection)},
            {"neutral_plane_normal", directionJson(neutralPlane->plane.Axis().Direction())},
            {"neutral_plane_source", neutralPlane->source},
            {"mode", "draft_angle"},
        };
        return result;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            "Base"
        );
        return std::nullopt;
    }
}

}  // namespace

void executeDraft(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementDraft()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base",
             "BaseFeature",
             "SupportTransform",
             "Angle",
             "NeutralPlane",
             "PullDirection",
             "Reversed",
             "Refine",
             "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildDraft(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

}  // namespace cad_core::part_design
