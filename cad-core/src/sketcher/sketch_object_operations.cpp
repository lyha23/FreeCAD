#include "sketch_object_operations.h"

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/compute_context.h"

#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <ElCLib.hxx>
#include <GC_MakeArcOfHyperbola.hxx>
#include <GC_MakeArcOfParabola.hxx>
#include <GC_MakeHyperbola.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Hypr.hxx>
#include <gp_Parab.hxx>
#include <gce_MakeParab.hxx>

#include <algorithm>
#include <cmath>

namespace cad_core::sketcher
{

bool samePoint(const gp_Pnt& left, const gp_Pnt& right)
{
    constexpr double eps = 1e-9;
    return std::abs(left.X() - right.X()) < eps && std::abs(left.Y() - right.Y()) < eps;
}

gp_Ax2 ellipseAxis(const gp_Pnt& center, double angle)
{
    gp_Ax2 axis(center, gp_Dir(0, 0, 1));
    axis.Rotate(gp_Ax1(center, gp_Dir(0, 0, 1)), angle);
    return axis;
}

bool addCircleWire(const SketchCircle& circle, BRepBuilderAPI_MakeWire& wireBuilder)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
    // registers Part::GeomCircle as a Sketcher geometry type with a center point but no start/end
    // endpoint; a single non-construction circle is already a closed profile.
    BRepBuilderAPI_MakeEdge edgeBuilder(gp_Circ(gp_Ax2(circle.center, gp_Dir(0, 0, 1)), circle.radius));
    if (!edgeBuilder.IsDone()) {
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return wireBuilder.IsDone();
}

bool addEllipseWire(const SketchEllipse& ellipse, BRepBuilderAPI_MakeWire& wireBuilder)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
    // GeomEllipse::Restore() rebuilds from "MajorRadius", "MinorRadius" and "AngleXU".
    const gp_Ax2 axis = ellipseAxis(ellipse.center, ellipse.angle);
    BRepBuilderAPI_MakeEdge edgeBuilder(gp_Elips(axis, ellipse.majorRadius, ellipse.minorRadius));
    if (!edgeBuilder.IsDone()) {
        return false;
    }
    wireBuilder.Add(edgeBuilder.Edge());
    return wireBuilder.IsDone();
}

SketchProfileEdge profileEdgeWithIdentity(
    SketchProfileEdgeKind kind,
    std::size_t geometryIndex,
    std::optional<long> geometryId
)
{
    SketchProfileEdge edge;
    edge.kind = kind;
    std::string geometryKind;
    switch (kind) {
        case SketchProfileEdgeKind::Line:
            geometryKind = "LineSegment";
            break;
        case SketchProfileEdgeKind::ArcOfCircle:
            geometryKind = "ArcOfCircle";
            break;
        case SketchProfileEdgeKind::ArcOfEllipse:
            geometryKind = "ArcOfEllipse";
            break;
        case SketchProfileEdgeKind::ArcOfHyperbola:
            geometryKind = "ArcOfHyperbola";
            break;
        case SketchProfileEdgeKind::ArcOfParabola:
            geometryKind = "ArcOfParabola";
            break;
        case SketchProfileEdgeKind::BSpline:
            geometryKind = "BSpline";
            break;
        case SketchProfileEdgeKind::InterpolatedSpline:
            geometryKind = "InterpolatedSpline";
            break;
        case SketchProfileEdgeKind::Bezier:
            geometryKind = "Bezier";
            break;
    }
    edge.identity = sketchGeometryIdentity(geometryIndex, geometryId, geometryKind);
    return edge;
}

std::vector<SketchProfileEdge> profileEdges(
    const std::vector<SketchSegment>& segments,
    const std::vector<SketchArc>& arcs,
    const std::vector<SketchEllipseArc>& ellipseArcs,
    const std::vector<SketchHyperbolaArc>& hyperbolaArcs,
    const std::vector<SketchParabolaArc>& parabolaArcs,
    const std::vector<SketchBSpline>& bsplines,
    const std::vector<SketchInterpolatedSpline>& interpolatedSplines,
    const std::vector<SketchBezier>& beziers
)
{
    std::vector<SketchProfileEdge> edges;
    for (const auto& segment : segments) {
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::Line,
            segment.geometryIndex,
            segment.geometryId
        );
        edge.start = segment.start;
        edge.end = segment.end;
        edges.push_back(std::move(edge));
    }
    for (const auto& arc : arcs) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // SketchArcOfCircle::getPoint() exposes start/end/mid; cad-core keeps the same endpoint
        // semantics for profile connectivity while deferring full solver support.
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::ArcOfCircle,
            arc.geometryIndex,
            arc.geometryId
        );
        edge.start = pointAtAngle(arc.center, arc.radius, arc.startAngle);
        edge.end = pointAtAngle(arc.center, arc.radius, arc.endAngle);
        edge.center = arc.center;
        edge.radius = arc.radius;
        edge.startAngle = arc.startAngle;
        edge.endAngle = arc.endAngle;
        edges.push_back(std::move(edge));
    }
    for (const auto& arc : ellipseArcs) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // SketchArcOfEllipse::getPoint() exposes start/end/mid with emulateCCW=true.
        // cad-core uses the same start/end parameters for profile connectivity.
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::ArcOfEllipse,
            arc.geometryIndex,
            arc.geometryId
        );
        edge.start = pointAtEllipseAngle(
            arc.center,
            arc.majorRadius,
            arc.minorRadius,
            arc.angle,
            arc.startAngle
        );
        edge.end
            = pointAtEllipseAngle(arc.center, arc.majorRadius, arc.minorRadius, arc.angle, arc.endAngle);
        edge.center = arc.center;
        edge.majorRadius = arc.majorRadius;
        edge.minorRadius = arc.minorRadius;
        edge.angle = arc.angle;
        edge.startAngle = arc.startAngle;
        edge.endAngle = arc.endAngle;
        edges.push_back(std::move(edge));
    }
    for (const auto& arc : hyperbolaArcs) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App
        // /SketchObjectExternal.cpp::processEdge2(), for GeomAbs_Hyperbola builds a
        // Part::GeomArcOfHyperbola from the trimmed curve parameters.
        const gp_Hypr hyperbola(ellipseAxis(arc.center, arc.angle), arc.majorRadius, arc.minorRadius);
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::ArcOfHyperbola,
            arc.geometryIndex,
            arc.geometryId
        );
        edge.start = ElCLib::Value(arc.startAngle, hyperbola);
        edge.end = ElCLib::Value(arc.endAngle, hyperbola);
        edge.center = arc.center;
        edge.majorRadius = arc.majorRadius;
        edge.minorRadius = arc.minorRadius;
        edge.angle = arc.angle;
        edge.startAngle = arc.startAngle;
        edge.endAngle = arc.endAngle;
        edges.push_back(std::move(edge));
    }
    for (const auto& arc : parabolaArcs) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App
        // /SketchObjectExternal.cpp::processEdge2(), for GeomAbs_Parabola builds a
        // Part::GeomArcOfParabola from the trimmed curve parameters.
        const gp_Parab parabola(ellipseAxis(arc.center, arc.angle), arc.focal);
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::ArcOfParabola,
            arc.geometryIndex,
            arc.geometryId
        );
        edge.start = ElCLib::Value(arc.startAngle, parabola);
        edge.end = ElCLib::Value(arc.endAngle, parabola);
        edge.center = arc.center;
        edge.focal = arc.focal;
        edge.angle = arc.angle;
        edge.startAngle = arc.startAngle;
        edge.endAngle = arc.endAngle;
        edges.push_back(std::move(edge));
    }
    for (const auto& bspline : bsplines) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchGeometry.cpp
        // ::SketchBSplineCurve::getPoint() exposes start/end for the same wire connectivity
        // role as line and arc profile geometry.
        if (bspline.poles.size() < 2U) {
            continue;
        }
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::BSpline,
            bspline.geometryIndex,
            bspline.geometryId
        );
        edge.start = bspline.poles.front();
        edge.end = bspline.poles.back();
        edge.degree = bspline.degree;
        edge.poles = bspline.poles;
        edges.push_back(std::move(edge));
    }
    for (const auto& spline : interpolatedSplines) {
        if (spline.points.size() < 2U || (spline.periodic && spline.points.size() < 3U)) {
            continue;
        }
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::InterpolatedSpline,
            spline.geometryIndex,
            spline.geometryId
        );
        edge.start = spline.points.front();
        edge.end = spline.periodic ? spline.points.front() : spline.points.back();
        edge.poles = spline.points;
        edge.periodic = spline.periodic;
        edge.startTangent = spline.startTangent;
        edge.endTangent = spline.endTangent;
        edges.push_back(std::move(edge));
    }
    for (const auto& bezier : beziers) {
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomBezierCurve::Restore() reads "PolesCount" and each Pole's
        // "X/Y/Z/Weight"; Sketcher consumes the curve as a normal profile edge.
        if (bezier.poles.size() < 2U) {
            continue;
        }
        SketchProfileEdge edge = profileEdgeWithIdentity(
            SketchProfileEdgeKind::Bezier,
            bezier.geometryIndex,
            bezier.geometryId
        );
        edge.start = bezier.poles.front();
        edge.end = bezier.poles.back();
        edge.degree = static_cast<int>(bezier.poles.size() - 1U);
        edge.poles = bezier.poles;
        edge.weights = bezier.weights;
        edges.push_back(std::move(edge));
    }
    return edges;
}

std::optional<Handle(Geom_BSplineCurve)> makeBSplineCurve(int degree, const std::vector<gp_Pnt>& poles)
{
    if (degree < 1 || poles.size() < static_cast<std::size_t>(degree + 1)) {
        return std::nullopt;
    }

    const int poleCount = static_cast<int>(poles.size());
    const int knotCount = poleCount - degree + 1;
    if (knotCount < 2) {
        return std::nullopt;
    }

    TColgp_Array1OfPnt poleArray(1, poleCount);
    for (int index = 1; index <= poleCount; ++index) {
        poleArray.SetValue(index, poles[static_cast<std::size_t>(index - 1)]);
    }

    TColStd_Array1OfReal knotArray(1, knotCount);
    TColStd_Array1OfInteger multiplicities(1, knotCount);
    for (int index = 1; index <= knotCount; ++index) {
        const double parameter = knotCount == 1
            ? 0.0
            : static_cast<double>(index - 1) / static_cast<double>(knotCount - 1);
        knotArray.SetValue(index, parameter);
        multiplicities.SetValue(index, 1);
    }
    multiplicities.SetValue(1, degree + 1);
    multiplicities.SetValue(knotCount, degree + 1);

    try {
        return Handle(Geom_BSplineCurve)(
            new Geom_BSplineCurve(poleArray, knotArray, multiplicities, degree, Standard_False)
        );
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

gp_Vec hermiteTangentAt(
    const std::vector<gp_Pnt>& points,
    std::size_t index,
    const gp_Vec& startTangent,
    const gp_Vec& endTangent
)
{
    if (index == 0U) {
        return startTangent;
    }
    if (index + 1U == points.size()) {
        return endTangent;
    }

    const gp_Pnt& previous = index > 0U ? points.at(index - 1U) : points.at(index);
    const gp_Pnt& next = index + 1U < points.size() ? points.at(index + 1U) : points.at(index);
    return gp_Vec(previous, next).Multiplied(0.5);
}

std::optional<Handle(Geom_BSplineCurve)> makeEndpointHermiteSplineCurve(
    const std::vector<gp_Pnt>& points,
    const gp_Vec& startTangent,
    const gp_Vec& endTangent
)
{
    if (points.size() < 2U) {
        return std::nullopt;
    }

    const std::size_t segmentCount = points.size() - 1U;
    const int degree = 3;
    const int poleCount = static_cast<int>(segmentCount * 3U + 1U);
    const int knotCount = static_cast<int>(segmentCount + 1U);

    TColgp_Array1OfPnt poleArray(1, poleCount);
    int poleIndex = 1;
    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        const gp_Pnt& p0 = points.at(segment);
        const gp_Pnt& p1 = points.at(segment + 1U);
        const gp_Vec m0 = hermiteTangentAt(points, segment, startTangent, endTangent);
        const gp_Vec m1 = hermiteTangentAt(points, segment + 1U, startTangent, endTangent);

        if (segment == 0U) {
            poleArray.SetValue(poleIndex++, p0);
        }
        poleArray.SetValue(poleIndex++, p0.Translated(m0.Multiplied(1.0 / 3.0)));
        poleArray.SetValue(poleIndex++, p1.Translated(m1.Multiplied(-1.0 / 3.0)));
        poleArray.SetValue(poleIndex++, p1);
    }

    TColStd_Array1OfReal knotArray(1, knotCount);
    TColStd_Array1OfInteger multiplicities(1, knotCount);
    for (int index = 1; index <= knotCount; ++index) {
        knotArray.SetValue(index, static_cast<double>(index - 1));
        multiplicities.SetValue(index, index == 1 || index == knotCount ? degree + 1 : degree);
    }

    try {
        return Handle(Geom_BSplineCurve)(
            new Geom_BSplineCurve(poleArray, knotArray, multiplicities, degree, Standard_False)
        );
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::optional<Handle(Geom_BSplineCurve)> makeInterpolatedSplineCurve(
    const std::vector<gp_Pnt>& points,
    bool periodic,
    const std::optional<gp_Vec>& startTangent,
    const std::optional<gp_Vec>& endTangent
)
{
    if (points.size() < 2U || (periodic && points.size() < 3U)) {
        return std::nullopt;
    }
    if (startTangent.has_value() != endTangent.has_value()) {
        return std::nullopt;
    }

    if (!periodic && startTangent && endTangent) {
        return makeEndpointHermiteSplineCurve(points, *startTangent, *endTangent);
    }

    Handle(TColgp_HArray1OfPnt) pointArray
        = new TColgp_HArray1OfPnt(1, static_cast<int>(points.size()));
    for (int index = 1; index <= static_cast<int>(points.size()); ++index) {
        pointArray->SetValue(index, points[static_cast<std::size_t>(index - 1)]);
    }

    try {
        GeomAPI_Interpolate interpolator(pointArray, periodic ? Standard_True : Standard_False, 1e-7);
        if (startTangent && endTangent) {
            interpolator.Load(*startTangent, *endTangent, Standard_True);
        }
        interpolator.Perform();
        if (!interpolator.IsDone()) {
            return std::nullopt;
        }
        return interpolator.Curve();
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::optional<Handle(Geom_BezierCurve)> makeBezierCurve(
    const std::vector<gp_Pnt>& poles,
    const std::vector<double>& weights
)
{
    if (poles.size() < 2U || (!weights.empty() && weights.size() != poles.size())) {
        return std::nullopt;
    }

    const int poleCount = static_cast<int>(poles.size());
    TColgp_Array1OfPnt poleArray(1, poleCount);
    for (int index = 1; index <= poleCount; ++index) {
        poleArray.SetValue(index, poles[static_cast<std::size_t>(index - 1)]);
    }

    try {
        if (weights.empty()) {
            return Handle(Geom_BezierCurve)(new Geom_BezierCurve(poleArray));
        }
        TColStd_Array1OfReal weightArray(1, poleCount);
        for (int index = 1; index <= poleCount; ++index) {
            const double weight = weights[static_cast<std::size_t>(index - 1)];
            if (weight <= 0.0) {
                return std::nullopt;
            }
            weightArray.SetValue(index, weight);
        }
        return Handle(Geom_BezierCurve)(new Geom_BezierCurve(poleArray, weightArray));
    }
    catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::optional<TopoDS_Edge> makeProfileEdge(const SketchProfileEdge& edge, bool reversed)
{
    BRepBuilderAPI_MakeEdge edgeBuilder;
    if (edge.kind == SketchProfileEdgeKind::Line) {
        edgeBuilder = BRepBuilderAPI_MakeEdge(edge.start, edge.end);
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfCircle) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomArcOfCircle::Restore() rebuilds from "Radius", "StartAngle" and "EndAngle".
        edgeBuilder = BRepBuilderAPI_MakeEdge(
            gp_Circ(gp_Ax2(edge.center, gp_Dir(0, 0, 1)), edge.radius),
            edge.startAngle,
            edge.endAngle
        );
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfEllipse) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomArcOfEllipse::Restore() rebuilds from "MajorRadius", "MinorRadius",
        // "AngleXU", "StartAngle" and "EndAngle".
        edgeBuilder = BRepBuilderAPI_MakeEdge(
            gp_Elips(ellipseAxis(edge.center, edge.angle), edge.majorRadius, edge.minorRadius),
            edge.startAngle,
            edge.endAngle
        );
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfHyperbola) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomArcOfHyperbola::Restore() creates "GC_MakeHyperbola" and
        // "GC_MakeArcOfHyperbola(..., StartAngle, EndAngle, Standard_True)".
        try {
            GC_MakeHyperbola hyperbolaMaker(
                ellipseAxis(edge.center, edge.angle),
                edge.majorRadius,
                edge.minorRadius
            );
            if (!hyperbolaMaker.IsDone()) {
                return std::nullopt;
            }
            GC_MakeArcOfHyperbola arcMaker(
                hyperbolaMaker.Value()->Hypr(),
                edge.startAngle,
                edge.endAngle,
                Standard_True
            );
            if (!arcMaker.IsDone()) {
                return std::nullopt;
            }
            Handle(Geom_TrimmedCurve) curve = arcMaker.Value();
            edgeBuilder = BRepBuilderAPI_MakeEdge(curve);
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    }
    else if (edge.kind == SketchProfileEdgeKind::ArcOfParabola) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomArcOfParabola::Restore() creates "gce_MakeParab" and
        // "GC_MakeArcOfParabola(..., StartAngle, EndAngle, Standard_True)".
        try {
            gce_MakeParab parabolaMaker(ellipseAxis(edge.center, edge.angle), edge.focal);
            if (!parabolaMaker.IsDone()) {
                return std::nullopt;
            }
            GC_MakeArcOfParabola
                arcMaker(parabolaMaker.Value(), edge.startAngle, edge.endAngle, Standard_True);
            if (!arcMaker.IsDone()) {
                return std::nullopt;
            }
            Handle(Geom_TrimmedCurve) curve = arcMaker.Value();
            edgeBuilder = BRepBuilderAPI_MakeEdge(curve);
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    }
    else if (edge.kind == SketchProfileEdgeKind::BSpline) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // GeomBSplineCurve stores "Poles", "Knots", "Multiplicity" and "Degree"; this P5
        // subset rebuilds a non-periodic clamped curve from fixture poles and degree.
        const auto curve = makeBSplineCurve(edge.degree, edge.poles);
        if (!curve) {
            return std::nullopt;
        }
        edgeBuilder = BRepBuilderAPI_MakeEdge(*curve);
    }
    else if (edge.kind == SketchProfileEdgeKind::InterpolatedSpline) {
        const auto curve
            = makeInterpolatedSplineCurve(edge.poles, edge.periodic, edge.startTangent, edge.endTangent);
        if (!curve) {
            return std::nullopt;
        }
        edgeBuilder = BRepBuilderAPI_MakeEdge(*curve);
    }
    else {
        // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomBezierCurve::Restore() rebuilds from Pole "X/Y/Z/Weight" values.
        const auto curve = makeBezierCurve(edge.poles, edge.weights);
        if (!curve) {
            return std::nullopt;
        }
        edgeBuilder = BRepBuilderAPI_MakeEdge(*curve);
    }
    if (!edgeBuilder.IsDone()) {
        return std::nullopt;
    }
    const TopoDS_Edge built = edgeBuilder.Edge();
    return reversed ? TopoDS::Edge(built.Reversed()) : built;
}

bool addConnectedWire(
    const std::vector<SketchProfileEdge>& edges,
    BRepBuilderAPI_MakeWire& wireBuilder,
    std::optional<gp_Pnt>& firstStart,
    std::optional<gp_Pnt>& lastEnd,
    bool requireClosed
)
{
    if (edges.empty()) {
        return false;
    }

    std::vector<bool> used(edges.size(), false);
    firstStart = edges.front().start;
    gp_Pnt currentEnd = edges.front().end;
    const auto firstEdge = makeProfileEdge(edges.front(), false);
    if (!firstEdge) {
        return false;
    }
    wireBuilder.Add(*firstEdge);
    used[0] = true;

    for (std::size_t usedCount = 1; usedCount < edges.size(); ++usedCount) {
        bool found = false;
        for (std::size_t index = 1; index < edges.size(); ++index) {
            if (used[index]) {
                continue;
            }
            if (samePoint(edges[index].start, currentEnd)) {
                const auto nextEdge = makeProfileEdge(edges[index], false);
                if (!nextEdge) {
                    return false;
                }
                wireBuilder.Add(*nextEdge);
                currentEnd = edges[index].end;
                used[index] = true;
                found = true;
                break;
            }
            if (samePoint(edges[index].end, currentEnd)) {
                const auto nextEdge = makeProfileEdge(edges[index], true);
                if (!nextEdge) {
                    return false;
                }
                wireBuilder.Add(*nextEdge);
                currentEnd = edges[index].start;
                used[index] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    lastEnd = currentEnd;
    return wireBuilder.IsDone() && (!requireClosed || samePoint(*firstStart, *lastEnd));
}

std::optional<SketchProfileWires> makeProfileWiresFromEdges(const std::vector<SketchProfileEdge>& edges)
{
    SketchProfileWires result;
    std::vector<bool> used(edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < edges.size(); ++startIndex) {
        if (used[startIndex]) {
            continue;
        }

        BRepBuilderAPI_MakeWire wireBuilder;
        std::vector<TopoDS_Edge> builtEdges;
        const gp_Pnt firstStart = edges[startIndex].start;
        gp_Pnt currentEnd = edges[startIndex].end;
        const auto firstEdge = makeProfileEdge(edges[startIndex], false);
        if (!firstEdge) {
            return std::nullopt;
        }
        wireBuilder.Add(*firstEdge);
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementWires(), after every `mkWire.Add()` assigns
        // `edges.back().setShape(mkWire.Edge(), false)` before `mapSubElement(edges, op)`.  OCCT can
        // replace the added edge's vertices while closing a wire; retain that final edge identity so
        // the Sketch g<ID>;SKT ledger is a real maker input, not a geometry/order fallback.
        builtEdges.push_back(TopoDS::Edge(wireBuilder.Edge()));
        std::vector<SketchGeometryIdentity> builtIdentities;
        builtIdentities.push_back(edges[startIndex].identity);
        used[startIndex] = true;

        while (!samePoint(firstStart, currentEnd)) {
            bool found = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (used[index]) {
                    continue;
                }
                if (samePoint(edges[index].start, currentEnd)) {
                    const auto nextEdge = makeProfileEdge(edges[index], false);
                    if (!nextEdge) {
                        return std::nullopt;
                    }
                    wireBuilder.Add(*nextEdge);
                    builtEdges.push_back(TopoDS::Edge(wireBuilder.Edge()));
                    builtIdentities.push_back(edges[index].identity);
                    currentEnd = edges[index].end;
                    used[index] = true;
                    found = true;
                    break;
                }
                if (samePoint(edges[index].end, currentEnd)) {
                    const auto nextEdge = makeProfileEdge(edges[index], true);
                    if (!nextEdge) {
                        return std::nullopt;
                    }
                    wireBuilder.Add(*nextEdge);
                    builtEdges.push_back(TopoDS::Edge(wireBuilder.Edge()));
                    builtIdentities.push_back(edges[index].identity);
                    currentEnd = edges[index].start;
                    used[index] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                break;
            }
        }

        if (!wireBuilder.IsDone()) {
            return std::nullopt;
        }
        std::vector<std::size_t> wireSourceEdgeIndices;
        wireSourceEdgeIndices.reserve(builtEdges.size());
        const std::size_t sourceEdgeOffset = result.sourceEdges.size();
        for (std::size_t edgeIndex = 0; edgeIndex < builtEdges.size(); ++edgeIndex) {
            wireSourceEdgeIndices.push_back(sourceEdgeOffset + edgeIndex);
        }
        result.sourceEdges.insert(result.sourceEdges.end(), builtEdges.begin(), builtEdges.end());
        result.sourceEdgeIdentities.insert(
            result.sourceEdgeIdentities.end(),
            builtIdentities.begin(),
            builtIdentities.end()
        );
        if (samePoint(firstStart, currentEnd)) {
            result.closedWires.push_back(wireBuilder.Wire());
            result.closedWireSourceEdgeIndices.push_back(std::move(wireSourceEdgeIndices));
        }
        else {
            result.openWires.push_back(wireBuilder.Wire());
            result.openWireSourceEdgeIndices.push_back(std::move(wireSourceEdgeIndices));
            result.openEdges.insert(result.openEdges.end(), builtEdges.begin(), builtEdges.end());
        }
    }

    return result;
}

std::optional<std::vector<TopoDS_Wire>> makeClosedWiresFromEdges(
    const std::vector<SketchProfileEdge>& edges
)
{
    auto wires = makeProfileWiresFromEdges(edges);
    if (!wires || !wires->openEdges.empty()) {
        return std::nullopt;
    }
    return wires->closedWires;
}

std::vector<std::size_t> appendSourceEdgesFromWire(
    std::vector<TopoDS_Edge>& sourceEdges,
    const TopoDS_Wire& wire
)
{
    std::vector<std::size_t> sourceEdgeIndices;
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        sourceEdgeIndices.push_back(sourceEdges.size());
        sourceEdges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return sourceEdgeIndices;
}

void appendWireSourcesInPublishedOrder(
    const SketchProfileWires& profileWires,
    const std::vector<std::vector<std::size_t>>& sourceIndexGroups,
    std::vector<TopoDS_Edge>& sourceEdges,
    std::vector<SketchGeometryIdentity>& sourceEdgeIdentities
)
{
    for (const auto& sourceIndices : sourceIndexGroups) {
        for (const std::size_t sourceIndex : sourceIndices) {
            if (sourceIndex >= profileWires.sourceEdges.size()
                || sourceIndex >= profileWires.sourceEdgeIdentities.size()) {
                continue;
            }
            sourceEdges.push_back(profileWires.sourceEdges[sourceIndex]);
            sourceEdgeIdentities.push_back(profileWires.sourceEdgeIdentities[sourceIndex]);
        }
    }
}

TopoDS_Shape compoundOrSingleShape(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.empty()) {
        return TopoDS_Shape {};
    }
    if (shapes.size() == 1U) {
        return shapes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

std::optional<TopoDS_Wire> makeWireFromCircle(const SketchCircle& circle)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    if (!addCircleWire(circle, wireBuilder)) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<TopoDS_Wire> makeWireFromEllipse(const SketchEllipse& ellipse)
{
    BRepBuilderAPI_MakeWire wireBuilder;
    if (!addEllipseWire(ellipse, wireBuilder)) {
        return std::nullopt;
    }
    return wireBuilder.Wire();
}

std::optional<RawSketchShapeBuild> buildRawSketchShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<SketchProfileEdge>& edges,
    const std::vector<SketchPoint>& points,
    const std::vector<SketchCircle>& circles,
    const std::vector<SketchEllipse>& ellipses
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::buildShape(),
    // "GeometryFacade::getConstruction(geo)" is skipped, then raw edges are collected into
    // makeElementWires() before PartDesign later asks ProfileBased to make a face.
    std::vector<TopoDS_Shape> shapes;
    std::vector<TopoDS_Edge> sourceEdges;
    std::vector<SketchGeometryIdentity> sourceEdgeIdentities;
    if (!edges.empty()) {
        const auto profileWires = makeProfileWiresFromEdges(edges);
        if (!profileWires) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "OCCT could not build raw Sketch Shape wire",
                object.name,
                "Geometry"
            );
            return std::nullopt;
        }
        shapes.insert(shapes.end(), profileWires->closedWires.begin(), profileWires->closedWires.end());
        shapes.insert(shapes.end(), profileWires->openWires.begin(), profileWires->openWires.end());
        appendWireSourcesInPublishedOrder(
            *profileWires,
            profileWires->closedWireSourceEdgeIndices,
            sourceEdges,
            sourceEdgeIdentities
        );
        appendWireSourcesInPublishedOrder(
            *profileWires,
            profileWires->openWireSourceEdgeIndices,
            sourceEdges,
            sourceEdgeIdentities
        );
        if (shapes.empty()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "OCCT could not build raw Sketch Shape wire",
                object.name,
                "Geometry"
            );
            return std::nullopt;
        }
    }
    for (const auto& point : points) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        // ::GeomPoint::toShape(), returns "BRepBuilderAPI_MakeVertex(myPoint->Pnt())".
        BRepBuilderAPI_MakeVertex vertexBuilder(point.point);
        if (!vertexBuilder.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "OCCT could not build raw Sketch Shape point vertex",
                object.name,
                "Geometry"
            );
            return std::nullopt;
        }
        shapes.push_back(vertexBuilder.Vertex());
    }
    for (const auto& circle : circles) {
        const auto wire = makeWireFromCircle(circle);
        if (!wire) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "OCCT could not build raw Sketch Shape circle wire",
                object.name,
                "Geometry"
            );
            return std::nullopt;
        }
        const std::size_t edgeOffset = sourceEdges.size();
        appendSourceEdgesFromWire(sourceEdges, *wire);
        sourceEdgeIdentities.resize(
            sourceEdges.size(),
            sketchGeometryIdentity(circle.geometryIndex, circle.geometryId, "Circle")
        );
        for (std::size_t index = edgeOffset; index < sourceEdges.size(); ++index) {
            sourceEdgeIdentities[index]
                = sketchGeometryIdentity(circle.geometryIndex, circle.geometryId, "Circle");
        }
        shapes.push_back(*wire);
    }
    for (const auto& ellipse : ellipses) {
        const auto wire = makeWireFromEllipse(ellipse);
        if (!wire) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "OCCT could not build raw Sketch Shape ellipse wire",
                object.name,
                "Geometry"
            );
            return std::nullopt;
        }
        const std::size_t edgeOffset = sourceEdges.size();
        appendSourceEdgesFromWire(sourceEdges, *wire);
        sourceEdgeIdentities.resize(
            sourceEdges.size(),
            sketchGeometryIdentity(ellipse.geometryIndex, ellipse.geometryId, "Ellipse")
        );
        for (std::size_t index = edgeOffset; index < sourceEdges.size(); ++index) {
            sourceEdgeIdentities[index]
                = sketchGeometryIdentity(ellipse.geometryIndex, ellipse.geometryId, "Ellipse");
        }
        shapes.push_back(*wire);
    }

    return RawSketchShapeBuild {
        compoundOrSingleShape(shapes),
        std::move(sourceEdges),
        std::move(sourceEdgeIdentities),
        true,
    };
}

ProfileFaceBuild buildOptionalProfileFace(
    const TopoDS_Shape& rawShape,
    app::ElementMapProducerTrace* producerTrace
)
{
    if (rawShape.IsNull()) {
        return {};
    }
    SketchInternalBuildInput input;
    input.producerTrace = producerTrace;
    TopTools_IndexedMapOfShape rawEdges;
    TopExp::MapShapes(rawShape, TopAbs_EDGE, rawEdges);
    input.sourceEdges.reserve(static_cast<std::size_t>(rawEdges.Extent()));
    for (int index = 1; index <= rawEdges.Extent(); ++index) {
        input.sourceEdges.push_back(TopoDS::Edge(rawEdges(index)));
    }

    // FreeCAD: src/Mod/Sketcher/App/SketchObject.cpp::buildInternals(), passes the raw Shape's
    // TopoShape wires to FaceMakerBuildFace. Do not rebuild equivalent wires from Geometry here:
    // FaceMaker::postBuild() relies on shared edge identity to map raw g<ID>;SKT names.
    for (TopExp_Explorer explorer(rawShape, TopAbs_WIRE); explorer.More(); explorer.Next()) {
        const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
        std::vector<std::size_t> sourceIndices;
        for (TopExp_Explorer edgeExplorer(wire, TopAbs_EDGE); edgeExplorer.More();
             edgeExplorer.Next()) {
            const int rawIndex = rawEdges.FindIndex(edgeExplorer.Current());
            if (rawIndex > 0) {
                sourceIndices.push_back(static_cast<std::size_t>(rawIndex - 1));
            }
        }
        if (sourceIndices.empty()) {
            continue;
        }
        if (BRep_Tool::IsClosed(wire)) {
            input.faceWires.push_back(wire);
            input.faceWireSourceEdgeIndices.push_back(std::move(sourceIndices));
        }
        else {
            input.openWires.push_back(wire);
            input.openWireSourceEdgeIndices.push_back(std::move(sourceIndices));
        }
    }

    if (input.faceWires.empty() && input.openWires.empty() && input.openEdges.empty()) {
        return {};
    }
    const auto result = buildSketchInternals(input);
    return ProfileFaceBuild {
        result.profileShape,
        result.internalShape,
        result.faceMakerFailed,
        result.requiresSubshapeSelection,
        result.historyLedger,
    };
}

std::size_t countSubshapesOfKind(const nlohmann::json& subshapes, const std::string& kind)
{
    std::size_t count = 0;
    for (const auto& item : subshapes.items()) {
        const nlohmann::json& value = item.value();
        if (value.is_object() && value.value("kind", std::string {}) == kind) {
            ++count;
        }
    }
    return count;
}

std::string profileShapeLabel(const std::optional<TopoDS_Shape>& profileShape)
{
    if (!profileShape) {
        return "none";
    }
    if (profileShape->ShapeType() == TopAbs_FACE) {
        return "occt_face";
    }
    if (profileShape->ShapeType() == TopAbs_COMPOUND) {
        return "occt_compound";
    }
    return "occt_profile_shape";
}


}  // namespace cad_core::sketcher
