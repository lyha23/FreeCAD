#include "cad_core/geometry/face_maker.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BOPAlgo_BuilderFace.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Builder.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRepLib_FindSurface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAPI.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Conic.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <GProp_GProps.hxx>
#include <IntRes2d_IntersectionPoint.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs.hxx>
#include <TopAbs_State.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace cad_core::geometry {

namespace {

struct WireInfo {
    TopoDS_Wire wire;
    double area = 0.0;
    std::size_t depth = 0;
    bool hasBSplineEdge = false;
};

std::optional<double> faceAreaForWire(const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(wire);
    if (!faceBuilder.IsDone()) {
        return std::nullopt;
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(faceBuilder.Face(), props);
    return props.Mass();
}

double faceAreaForShape(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}

std::optional<gp_Pln> planeForWire(const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(wire);
    if (!faceBuilder.IsDone()) {
        return std::nullopt;
    }
    GeomAdaptor_Surface surface(BRep_Tool::Surface(faceBuilder.Face()));
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    return surface.Plane();
}

std::optional<gp_Pln> planeForEdges(const TopTools_ListOfShape& edges)
{
    if (edges.IsEmpty()) {
        return std::nullopt;
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        builder.Add(compound, BRepBuilderAPI_Copy(it.Value()).Shape());
    }

    BRepLib_FindSurface planeFinder(compound, -1, Standard_True);
    if (!planeFinder.Found()) {
        return std::nullopt;
    }

    GeomAdaptor_Surface surface(planeFinder.Surface());
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    return surface.Plane();
}

std::optional<int> wireDirection(const gp_Pln& plane, const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(plane, wire, Standard_True);
    if (!faceBuilder.IsDone() || faceBuilder.Face().IsNull()) {
        return std::nullopt;
    }

    TopoDS_Iterator it(faceBuilder.Face(), Standard_False);
    if (!it.More()) {
        return std::nullopt;
    }
    return it.Value().Orientation() == wire.Orientation() ? 1 : -1;
}

std::optional<gp_Pnt> samplePoint(const TopoDS_Wire& wire)
{
    for (TopExp_Explorer explorer(wire, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        return BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
    }
    return std::nullopt;
}

bool wireContainsPoint(const gp_Pln& plane, const TopoDS_Wire& wire, const gp_Pnt& point)
{
    BRepBuilderAPI_MakeFace faceBuilder(plane, wire);
    if (!faceBuilder.IsDone()) {
        return false;
    }
    BRepClass_FaceClassifier classifier(faceBuilder.Face(), point, Precision::Confusion());
    return classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON;
}

bool wireContainsWire(const gp_Pln& plane,
                      const TopoDS_Wire& outer,
                      const TopoDS_Wire& inner,
                      double innerArea)
{
    BRepBuilderAPI_MakeFace outerFaceBuilder(plane, outer);
    BRepBuilderAPI_MakeFace innerFaceBuilder(plane, inner);
    if (!outerFaceBuilder.IsDone() || !innerFaceBuilder.IsDone()) {
        const auto point = samplePoint(inner);
        return point && wireContainsPoint(plane, outer, *point);
    }

    BRepAlgoAPI_Common common(outerFaceBuilder.Face(), innerFaceBuilder.Face());
    common.Build();
    if (!common.IsDone() || common.Shape().IsNull()) {
        const auto point = samplePoint(inner);
        return point && wireContainsPoint(plane, outer, *point);
    }

    const double commonArea = faceAreaForShape(common.Shape());
    const double tolerance = std::max(Precision::Confusion(), innerArea * 1e-6);
    return std::abs(commonArea - innerArea) <= tolerance;
}

bool wireHasBSplineEdge(const TopoDS_Wire& wire)
{
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        if (!curve.IsNull() && curve->IsKind(STANDARD_TYPE(Geom_BSplineCurve))) {
            return true;
        }
    }
    return false;
}

bool wireInfosHavePartialBSplineCommonArea(const std::vector<WireInfo>& wireInfos)
{
    for (std::size_t leftIndex = 0; leftIndex < wireInfos.size(); ++leftIndex) {
        for (std::size_t rightIndex = leftIndex + 1U; rightIndex < wireInfos.size(); ++rightIndex) {
            const WireInfo& left = wireInfos[leftIndex];
            const WireInfo& right = wireInfos[rightIndex];
            if (!left.hasBSplineEdge && !right.hasBSplineEdge) {
                continue;
            }

            BRepBuilderAPI_MakeFace leftFace(left.wire);
            BRepBuilderAPI_MakeFace rightFace(right.wire);
            if (!leftFace.IsDone() || !rightFace.IsDone()) {
                continue;
            }

            BRepAlgoAPI_Common common(leftFace.Face(), rightFace.Face());
            common.Build();
            if (!common.IsDone() || common.Shape().IsNull()) {
                continue;
            }

            const double commonArea = faceAreaForShape(common.Shape());
            const double minArea = std::min(left.area, right.area);
            const double tolerance = std::max(Precision::Confusion(), minArea * 1e-6);
            if (commonArea > tolerance && std::abs(commonArea - minArea) > tolerance) {
                return true;
            }
        }
    }
    return false;
}

bool samePoint(const gp_Pnt& lhs, const gp_Pnt& rhs)
{
    return lhs.SquareDistance(rhs) <= Precision::SquareConfusion();
}

std::pair<gp_Pnt, gp_Pnt> edgeEndpoints(const TopoDS_Edge& edge)
{
    return {BRep_Tool::Pnt(TopExp::FirstVertex(edge)), BRep_Tool::Pnt(TopExp::LastVertex(edge))};
}

gp_Pnt edgeMidpoint(const TopoDS_Edge& edge)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (!curve.IsNull()) {
        return curve->Value((first + last) * 0.5);
    }
    const auto [start, end] = edgeEndpoints(edge);
    return gp_Pnt((start.X() + end.X()) * 0.5,
                  (start.Y() + end.Y()) * 0.5,
                  (start.Z() + end.Z()) * 0.5);
}

bool pointLiesOnEdge(const gp_Pnt& point, const TopoDS_Edge& edge)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull()) {
        return false;
    }

    GeomAPI_ProjectPointOnCurve projection(point, curve, first, last);
    return projection.NbPoints() > 0 && projection.LowerDistance() <= Precision::Confusion();
}

bool edgeSamplesLieOnEdge(const TopoDS_Edge& edge, const TopoDS_Edge& source)
{
    if (edge.IsNull() || source.IsNull()) {
        return false;
    }
    const auto [first, last] = edgeEndpoints(edge);
    return pointLiesOnEdge(first, source) && pointLiesOnEdge(edgeMidpoint(edge), source)
        && pointLiesOnEdge(last, source);
}

bool edgeMatchesEndpoints(const TopoDS_Edge& lhs, const TopoDS_Edge& rhs)
{
    const auto [lhsFirst, lhsLast] = edgeEndpoints(lhs);
    const auto [rhsFirst, rhsLast] = edgeEndpoints(rhs);
    return (samePoint(lhsFirst, rhsFirst) && samePoint(lhsLast, rhsLast))
        || (samePoint(lhsFirst, rhsLast) && samePoint(lhsLast, rhsFirst));
}

bool edgeEquivalentByGeometryAndEndpoints(const TopoDS_Edge& lhs, const TopoDS_Edge& rhs)
{
    return edgeMatchesEndpoints(lhs, rhs) && edgeSamplesLieOnEdge(lhs, rhs) && edgeSamplesLieOnEdge(rhs, lhs);
}

bool closedWiresShareFullBoundaryEdge(const std::vector<TopoDS_Wire>& wires)
{
    for (std::size_t leftIndex = 0; leftIndex < wires.size(); ++leftIndex) {
        for (TopExp_Explorer leftExplorer(wires[leftIndex], TopAbs_EDGE); leftExplorer.More(); leftExplorer.Next()) {
            const TopoDS_Edge left = TopoDS::Edge(leftExplorer.Current());
            for (std::size_t rightIndex = leftIndex + 1U; rightIndex < wires.size(); ++rightIndex) {
                for (TopExp_Explorer rightExplorer(wires[rightIndex], TopAbs_EDGE);
                     rightExplorer.More();
                     rightExplorer.Next()) {
                    if (edgeEquivalentByGeometryAndEndpoints(left, TopoDS::Edge(rightExplorer.Current()))) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

TopoDS_Wire orientedWire(const gp_Pln& plane, const TopoDS_Wire& wire, bool outer)
{
    TopoDS_Wire result = wire;
    const auto direction = wireDirection(plane, result);
    if (!direction) {
        return result;
    }
    if (outer && *direction < 0) {
        result.Reverse();
    }
    if (!outer && *direction > 0) {
        result.Reverse();
    }
    return result;
}

std::optional<TopoDS_Shape> compoundOrSingleFace(const std::vector<TopoDS_Face>& faces)
{
    if (faces.empty()) {
        return std::nullopt;
    }
    if (faces.size() == 1U) {
        return faces.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Face& face : faces) {
        builder.Add(compound, face);
    }
    return compound;
}

std::vector<TopoDS_Face> facesForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        faces.push_back(TopoDS::Face(explorer.Current()));
    }
    return faces;
}

std::vector<TopoDS_Edge> edgesForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

std::size_t countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum shapeType)
{
    std::size_t count = 0;
    for (TopExp_Explorer explorer(shape, shapeType); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

bool topologyWasSplit(const TopoDS_Shape& candidate, const TopoDS_Shape& base)
{
    return countSubShapes(candidate, TopAbs_EDGE) > countSubShapes(base, TopAbs_EDGE)
        || countSubShapes(candidate, TopAbs_VERTEX) > countSubShapes(base, TopAbs_VERTEX);
}

TopTools_ListOfShape edgeNetworkForWiresAndEdges(const std::vector<TopoDS_Wire>& wires,
                                                 const std::vector<TopoDS_Edge>& extraEdges)
{
    TopTools_ListOfShape edges;
    for (const TopoDS_Wire& wire : wires) {
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            edges.Append(explorer.Current());
        }
    }
    for (const TopoDS_Edge& edge : extraEdges) {
        if (!edge.IsNull()) {
            edges.Append(edge);
        }
    }
    return edges;
}

bool shapesTouch(const TopoDS_Shape& lhs, const TopoDS_Shape& rhs)
{
    if (lhs.IsNull() || rhs.IsNull()) {
        return false;
    }
    BRepExtrema_DistShapeShape distance(lhs, rhs);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= Precision::Confusion();
}

bool edgeTouchesAnyBoundary(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& boundaryEdges)
{
    for (const TopoDS_Edge& boundary : boundaryEdges) {
        if (shapesTouch(edge, boundary)) {
            return true;
        }
    }
    return false;
}

std::vector<TopoDS_Edge> connectedSplitEdgesFromBoundary(const std::vector<TopoDS_Edge>& splitEdges,
                                                         const std::vector<TopoDS_Edge>& baseBoundaryEdges)
{
    std::vector<TopoDS_Edge> candidates;
    candidates.reserve(splitEdges.size());
    for (const TopoDS_Edge& edge : splitEdges) {
        if (!edge.IsNull()) {
            candidates.push_back(edge);
        }
    }

    std::vector<bool> selected(candidates.size(), false);
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (selected[index]) {
                continue;
            }
            bool touchesSelectedNetwork = edgeTouchesAnyBoundary(candidates[index], baseBoundaryEdges);
            if (!touchesSelectedNetwork) {
                for (std::size_t selectedIndex = 0; selectedIndex < candidates.size(); ++selectedIndex) {
                    if (selected[selectedIndex] && shapesTouch(candidates[index], candidates[selectedIndex])) {
                        touchesSelectedNetwork = true;
                        break;
                    }
                }
            }
            if (touchesSelectedNetwork) {
                selected[index] = true;
                changed = true;
            }
        }
    }

    std::vector<TopoDS_Edge> result;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (selected[index]) {
            result.push_back(candidates[index]);
        }
    }
    return result;
}

std::optional<TopoDS_Face> faceFromWire(const TopoDS_Wire& wire)
{
    BRepBuilderAPI_MakeFace faceBuilder(wire);
    if (!faceBuilder.IsDone() || faceBuilder.Face().IsNull()) {
        return std::nullopt;
    }
    return faceBuilder.Face();
}

double wireSquareExtent(const TopoDS_Wire& wire)
{
    Bnd_Box box;
    BRepBndLib::Add(wire, box);
    box.SetGap(0.0);
    if (box.IsVoid()) {
        return 0.0;
    }
    return box.SquareExtent();
}

TopTools_ListOfShape wireEdges(const TopoDS_Wire& wire)
{
    TopTools_ListOfShape edges;
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.Append(explorer.Current());
    }
    return edges;
}

TopTools_ListOfShape splitSelfIntersectingEdges(const TopTools_ListOfShape& edges,
                                                const gp_Pln& plane,
                                                bool& producedSplit)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::FaceMakerBuildFace::splitSelfIntersecting(), "Split self-intersecting edges" before
    // BuilderFace because "BuilderAlgo only finds inter-edge intersections".
    const Standard_Real tolerance = Precision::Confusion();
    TopTools_ListOfShape result;

    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(it.Value());
        try {
            Standard_Real first = 0.0;
            Standard_Real last = 0.0;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
            if (curve.IsNull() || curve->IsKind(STANDARD_TYPE(Geom_Line))
                || curve->IsKind(STANDARD_TYPE(Geom_Conic))) {
                result.Append(edge);
                continue;
            }

            Handle(Geom2d_Curve) curve2d = GeomAPI::To2d(curve, plane);
            if (curve2d.IsNull()) {
                result.Append(edge);
                continue;
            }

            Geom2dAPI_InterCurveCurve selfIntersection(curve2d, tolerance);
            if (selfIntersection.NbPoints() == 0) {
                result.Append(edge);
                continue;
            }

            std::vector<Standard_Real> parameters;
            for (int index = 1; index <= selfIntersection.NbPoints(); ++index) {
                const IntRes2d_IntersectionPoint& intersectionPoint =
                    selfIntersection.Intersector().Point(index);
                for (const Standard_Real parameter :
                     std::array<Standard_Real, 2>{intersectionPoint.ParamOnFirst(),
                                                  intersectionPoint.ParamOnSecond()}) {
                    if (parameter - first > tolerance && last - parameter > tolerance) {
                        parameters.push_back(parameter);
                    }
                }

                Geom2dAPI_ProjectPointOnCurve projection(selfIntersection.Point(index), curve2d, first, last);
                for (int pointIndex = 1; pointIndex <= projection.NbPoints(); ++pointIndex) {
                    const Standard_Real parameter = projection.Parameter(pointIndex);
                    if (parameter - first > tolerance && last - parameter > tolerance) {
                        parameters.push_back(parameter);
                    }
                }
            }

            if (parameters.empty()) {
                result.Append(edge);
                continue;
            }

            std::sort(parameters.begin(), parameters.end());
            parameters.erase(std::unique(parameters.begin(),
                                         parameters.end(),
                                         [tolerance](double lhs, double rhs) {
                                             return rhs - lhs < tolerance;
                                         }),
                             parameters.end());

            TopTools_ListOfShape fragments;
            Standard_Real previous = first;
            for (const Standard_Real parameter : parameters) {
                if (parameter - previous > tolerance) {
                    BRepBuilderAPI_MakeEdge edgeBuilder(curve, previous, parameter);
                    if (edgeBuilder.IsDone()) {
                        fragments.Append(edgeBuilder.Edge());
                    }
                    previous = parameter;
                }
            }
            if (last - previous > tolerance) {
                BRepBuilderAPI_MakeEdge edgeBuilder(curve, previous, last);
                if (edgeBuilder.IsDone()) {
                    fragments.Append(edgeBuilder.Edge());
                }
            }

            if (fragments.IsEmpty()) {
                result.Append(edge);
                continue;
            }

            producedSplit = true;
            for (TopTools_ListIteratorOfListOfShape fragmentIt(fragments); fragmentIt.More(); fragmentIt.Next()) {
                result.Append(fragmentIt.Value());
            }
        }
        catch (const Standard_Failure&) {
            result.Append(edge);
        }
        catch (...) {
            result.Append(edge);
        }
    }

    return result;
}

struct EdgeSplitPoint {
    Standard_Real parameter = 0.0;
    TopoDS_Vertex vertex;
};

std::optional<Standard_Real> parameterForPointOnEdge(const TopoDS_Edge& edge, const gp_Pnt& point)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    if (curve.IsNull()) {
        return std::nullopt;
    }

    GeomAPI_ProjectPointOnCurve projection(point, curve, first, last);
    if (projection.NbPoints() == 0 || projection.LowerDistance() > Precision::Confusion()) {
        return std::nullopt;
    }

    const Standard_Real parameter = projection.LowerDistanceParameter();
    if (parameter - first <= Precision::Confusion() || last - parameter <= Precision::Confusion()) {
        return std::nullopt;
    }
    return parameter;
}

TopTools_ListOfShape splitEdgesAtTouchingEndpoints(const TopTools_ListOfShape& edges, bool& producedSplit)
{
    std::vector<TopoDS_Edge> edgeList;
    std::vector<TopoDS_Vertex> endpointVertices;
    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(it.Value());
        edgeList.push_back(edge);
        endpointVertices.push_back(TopExp::FirstVertex(edge));
        endpointVertices.push_back(TopExp::LastVertex(edge));
    }
    if (edgeList.size() <= 1U) {
        return edges;
    }

    TopTools_ListOfShape result;
    bool touched = false;
    for (const TopoDS_Edge& edge : edgeList) {
        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        if (curve.IsNull()) {
            result.Append(edge);
            continue;
        }

        std::vector<EdgeSplitPoint> splitPoints;
        for (const TopoDS_Vertex& vertex : endpointVertices) {
            if (vertex.IsNull()) {
                continue;
            }
            if (vertex.IsSame(TopExp::FirstVertex(edge)) || vertex.IsSame(TopExp::LastVertex(edge))) {
                continue;
            }
            const auto parameter = parameterForPointOnEdge(edge, BRep_Tool::Pnt(vertex));
            if (!parameter) {
                continue;
            }
            auto duplicate = std::find_if(splitPoints.begin(),
                                          splitPoints.end(),
                                          [&](const EdgeSplitPoint& existing) {
                                              return std::abs(existing.parameter - *parameter)
                                                  <= Precision::Confusion();
                                          });
            if (duplicate == splitPoints.end()) {
                splitPoints.push_back(EdgeSplitPoint{*parameter, vertex});
            }
        }
        if (splitPoints.empty()) {
            result.Append(edge);
            continue;
        }

        std::sort(splitPoints.begin(),
                  splitPoints.end(),
                  [](const EdgeSplitPoint& lhs, const EdgeSplitPoint& rhs) {
                      return lhs.parameter < rhs.parameter;
                  });

        bool failed = false;
        Standard_Real previous = first;
        TopoDS_Vertex previousVertex = TopExp::FirstVertex(edge);
        TopTools_ListOfShape fragments;
        for (const EdgeSplitPoint& splitPoint : splitPoints) {
            if (splitPoint.parameter - previous > Precision::Confusion()) {
                BRepBuilderAPI_MakeEdge edgeBuilder(curve,
                                                    previousVertex,
                                                    splitPoint.vertex,
                                                    previous,
                                                    splitPoint.parameter);
                if (!edgeBuilder.IsDone() || edgeBuilder.Edge().IsNull()) {
                    failed = true;
                    break;
                }
                fragments.Append(edgeBuilder.Edge());
            }
            previous = splitPoint.parameter;
            previousVertex = splitPoint.vertex;
        }
        if (!failed && last - previous > Precision::Confusion()) {
            BRepBuilderAPI_MakeEdge edgeBuilder(curve,
                                                previousVertex,
                                                TopExp::LastVertex(edge),
                                                previous,
                                                last);
            if (!edgeBuilder.IsDone() || edgeBuilder.Edge().IsNull()) {
                failed = true;
            }
            else {
                fragments.Append(edgeBuilder.Edge());
            }
        }
        if (failed || fragments.IsEmpty()) {
            result.Append(edge);
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence(), "splitAtIntersections()" runs before BOPAlgo_BuilderFace. FreeCAD's
        // edge graph also carries T-junction endpoint vertices into BuilderFace; cad-core must split
        // a touched edge at an open-edge endpoint and reuse that endpoint vertex.
        touched = true;
        for (TopTools_ListIteratorOfListOfShape fragmentIt(fragments); fragmentIt.More(); fragmentIt.Next()) {
            result.Append(fragmentIt.Value());
        }
    }

    producedSplit = producedSplit || touched;
    return result;
}

TopTools_ListOfShape splitEdgesAtIntersections(const TopTools_ListOfShape& edges, bool& producedSplit)
{
    if (edges.Size() <= 1) {
        return edges;
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(edges);
    splitter.SetRunParallel(Standard_True);
    splitter.SetNonDestructive(Standard_True);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return edges;
    }

    TopTools_ListOfShape result;
    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_EDGE); explorer.More(); explorer.Next()) {
        result.Append(explorer.Current());
    }
    if (result.IsEmpty()) {
        return edges;
    }
    producedSplit = producedSplit || result.Size() > edges.Size();
    return result;
}

std::optional<TopoDS_Shape> buildBoundedFacesFromEdgeNetwork(const TopTools_ListOfShape& sourceEdges,
                                                             std::size_t& faceCount,
                                                             bool& producedSplit)
{
    const auto plane = planeForEdges(sourceEdges);
    if (!plane) {
        return std::nullopt;
    }

    TopTools_ListOfShape edges = splitSelfIntersectingEdges(sourceEdges, *plane, producedSplit);
    edges = splitEdgesAtTouchingEndpoints(edges, producedSplit);
    edges = splitEdgesAtIntersections(edges, producedSplit);
    edges = splitEdgesAtTouchingEndpoints(edges, producedSplit);
    if (edges.IsEmpty()) {
        return std::nullopt;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::Build_Essence(), builds a large planar base face, feeds every edge in FORWARD and
    // REVERSED orientation to BOPAlgo_BuilderFace, and enables "SetAvoidInternalShapes".
    Bnd_Box geomBox;
    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        BRepBndLib::Add(it.Value(), geomBox);
    }
    if (geomBox.IsVoid()) {
        return std::nullopt;
    }

    const Standard_Real extent = std::sqrt(geomBox.SquareExtent());
    const Standard_Real aMax = std::max(1.0e8, 10.0 * extent);
    TopoDS_Face baseFace = BRepBuilderAPI_MakeFace(*plane, -aMax, aMax, -aMax, aMax).Face();
    baseFace.Orientation(TopAbs_FORWARD);

    TopTools_ListOfShape faceEdges;
    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(it.Value());
        faceEdges.Append(edge.Oriented(TopAbs_FORWARD));
        faceEdges.Append(edge.Oriented(TopAbs_REVERSED));
    }
    BRepLib::BuildPCurveForEdgesOnPlane(faceEdges, baseFace);

    BOPAlgo_BuilderFace faceBuilder;
    faceBuilder.SetFace(baseFace);
    faceBuilder.SetShapes(faceEdges);
    faceBuilder.SetAvoidInternalShapes(Standard_True);
    faceBuilder.Perform();
    if (faceBuilder.HasErrors()) {
        return std::nullopt;
    }

    const double outerThreshold = aMax * aMax;
    std::vector<TopoDS_Face> faces;
    for (TopTools_ListIteratorOfListOfShape it(faceBuilder.Areas()); it.More(); it.Next()) {
        Bnd_Box box;
        BRepBndLib::Add(it.Value(), box);
        if (box.SquareExtent() > outerThreshold) {
            continue;
        }

        GProp_GProps props;
        BRepGProp::SurfaceProperties(it.Value(), props);
        if (props.Mass() < Precision::Confusion()) {
            continue;
        }
        faces.push_back(TopoDS::Face(it.Value()));
    }

    faceCount = faces.size();
    return compoundOrSingleFace(faces);
}

std::optional<FaceMakerBuildFaceResult> makeSelfIntersectingSingleWireFaces(const TopoDS_Wire& wire)
{
    TopTools_ListOfShape edges = wireEdges(wire);
    if (edges.IsEmpty()) {
        return std::nullopt;
    }

    std::size_t faceCount = 0;
    bool producedSplit = false;
    const auto shape = buildBoundedFacesFromEdgeNetwork(edges, faceCount, producedSplit);
    if (!shape || shape->IsNull() || faceCount <= 1U) {
        return std::nullopt;
    }
    return FaceMakerBuildFaceResult{shape, faceCount, producedSplit};
}

std::optional<TopoDS_Shape> splitOverlappingFaces(const std::vector<TopoDS_Face>& faces)
{
    if (faces.size() < 2U) {
        return compoundOrSingleFace(faces);
    }

    TopTools_ListOfShape objects;
    for (const TopoDS_Face& face : faces) {
        if (!face.IsNull()) {
            objects.Append(face);
        }
    }
    if (objects.Extent() < 2) {
        return compoundOrSingleFace(faces);
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(objects);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return compoundOrSingleFace(faces);
    }

    const std::vector<TopoDS_Face> splitFaces = facesForShape(splitter.Shape());
    if (splitFaces.size() <= faces.size()) {
        return compoundOrSingleFace(faces);
    }
    return compoundOrSingleFace(splitFaces);
}

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWiresImpl(const std::vector<TopoDS_Wire>& wires,
                                                                 bool allowBuilderFaceSplitExpansion)
{
    if (wires.empty()) {
        return std::nullopt;
    }

    std::vector<WireInfo> wireInfos;
    wireInfos.reserve(wires.size());
    for (const TopoDS_Wire& wire : wires) {
        const auto area = faceAreaForWire(wire);
        if (!area) {
            return std::nullopt;
        }
        wireInfos.push_back(WireInfo{wire, *area, 0U, wireHasBSplineEdge(wire)});
    }
    std::stable_sort(wireInfos.begin(), wireInfos.end(), [](const WireInfo& lhs, const WireInfo& rhs) {
        return lhs.area > rhs.area;
    });

    const auto plane = planeForWire(wireInfos.front().wire);
    if (!plane) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < wireInfos.size(); ++index) {
        const auto point = samplePoint(wireInfos[index].wire);
        if (!point) {
            return std::nullopt;
        }
        for (std::size_t parent = 0; parent < wireInfos.size(); ++parent) {
            if (parent == index || wireInfos[parent].area <= wireInfos[index].area) {
                continue;
            }
            if (wireContainsWire(*plane, wireInfos[parent].wire, wireInfos[index].wire, wireInfos[index].area)) {
                ++wireInfos[index].depth;
            }
        }
    }

    if (wireInfosHavePartialBSplineCommonArea(wireInfos)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence() delegates curve-network splitting to BOPAlgo_BuilderFace; intersecting
        // BSpline closed profiles can fail there and SketchObject keeps an empty InternalShape
        // instead of manufacturing overlapping fallback faces.
        return std::nullopt;
    }

    std::vector<TopoDS_Face> faces;
    for (std::size_t outerIndex = 0; outerIndex < wireInfos.size(); ++outerIndex) {
        if (wireInfos[outerIndex].depth % 2U != 0U) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBullseye.cpp
        // ::Build_Essence(), "Shape in outer wire but not on face, which means it is within a
        // hole. So it's a hit and we shall make a new face with the wire." This keeps islands
        // as separate faces while odd-depth wires are added as holes to their containing face.
        BRepBuilderAPI_MakeFace faceBuilder(*plane, orientedWire(*plane, wireInfos[outerIndex].wire, true));
        if (!faceBuilder.IsDone()) {
            return std::nullopt;
        }

        for (std::size_t holeIndex = 0; holeIndex < wireInfos.size(); ++holeIndex) {
            if (wireInfos[holeIndex].depth != wireInfos[outerIndex].depth + 1U) {
                continue;
            }
            const auto point = samplePoint(wireInfos[holeIndex].wire);
            if (!point || !wireContainsPoint(*plane, wireInfos[outerIndex].wire, *point)) {
                continue;
            }
            faceBuilder.Add(orientedWire(*plane, wireInfos[holeIndex].wire, false));
            if (!faceBuilder.IsDone()) {
                return std::nullopt;
            }
        }

        faces.push_back(faceBuilder.Face());
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::Build_Essence() feeds all profile edges into BOPAlgo_BuilderFace so overlapping closed
    // profiles become disjoint bounded regions instead of overlapping face products.
    const auto result = splitOverlappingFaces(faces);
    if (!result || result->IsNull()) {
        return result;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::Build_Essence(), after splitAtIntersections(), BOPAlgo_BuilderFace is the owner of
    // "myShapesToReturn". When it produces the same bounded region count as the fallback
    // hole/island path, prefer its face network because it preserves FreeCAD edge ownership.
    std::size_t rebuiltFaceCount = 0;
    bool rebuiltProducedSplit = false;
    const auto rebuilt = buildBoundedFacesFromEdgeNetwork(edgeNetworkForWiresAndEdges(wires, {}),
                                                          rebuiltFaceCount,
                                                          rebuiltProducedSplit);
    const std::vector<TopoDS_Face> resultFaces = facesForShape(*result);
    const bool useEquivalentBuilderFaceTopology =
        rebuiltFaceCount == resultFaces.size()
        && ((wires.size() >= 3U && rebuiltFaceCount > wires.size()) || closedWiresShareFullBoundaryEdge(wires));
    const bool hasContainedWire =
        std::any_of(wireInfos.begin(), wireInfos.end(), [](const WireInfo& info) { return info.depth > 0U; });
    const bool useSplitBuilderFaceTopology =
        allowBuilderFaceSplitExpansion && !hasContainedWire && rebuiltProducedSplit
        && rebuiltFaceCount > resultFaces.size();
    if (rebuilt && !rebuilt->IsNull() && (useEquivalentBuilderFaceTopology || useSplitBuilderFaceTopology)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence() owns "myShapesToReturn" through BOPAlgo_BuilderFace. Adjacent closed
        // profiles with a full shared boundary edge should keep one shared result edge instead of
        // two independent fallback face edges.
        return rebuilt;
    }
    return result;
}

}  // namespace

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires)
{
    return makeFaceWithHolesFromClosedWiresImpl(wires, false);
}

std::optional<TopoDS_Shape> makeSeparateFacesFromClosedWires(const std::vector<TopoDS_Wire>& wires)
{
    if (wires.empty()) {
        return std::nullopt;
    }

    std::vector<TopoDS_Face> faces;
    faces.reserve(wires.size());
    for (const TopoDS_Wire& wire : wires) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp
        // ::FaceMakerSimple::Build_Essence(), pushes "BRepBuilderAPI_MakeFace(w).Shape()" for
        // every wire instead of classifying holes or nesting.
        const auto face = faceFromWire(wire);
        if (!face) {
            return std::nullopt;
        }
        faces.push_back(*face);
    }
    return compoundOrSingleFace(faces);
}

std::optional<TopoDS_Shape> makeCheeseFaceFromClosedWires(const std::vector<TopoDS_Wire>& wires)
{
    if (wires.empty()) {
        return std::nullopt;
    }

    std::vector<WireInfo> wireInfos;
    wireInfos.reserve(wires.size());
    for (const TopoDS_Wire& wire : wires) {
        const auto area = faceAreaForWire(wire);
        if (!area) {
            return std::nullopt;
        }
        wireInfos.push_back(WireInfo{wire, *area, 0U});
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerCheese.cpp
    // ::makeFace(), sorts wires by bounding-box diagonal and then groups every wire inside the
    // current outer wire as a hole; it intentionally does not promote nested holes back to islands.
    std::stable_sort(wireInfos.begin(), wireInfos.end(), [](const WireInfo& lhs, const WireInfo& rhs) {
        return wireSquareExtent(lhs.wire) > wireSquareExtent(rhs.wire);
    });

    std::vector<TopoDS_Face> faces;
    while (!wireInfos.empty()) {
        WireInfo outer = wireInfos.front();
        wireInfos.erase(wireInfos.begin());
        const auto plane = planeForWire(outer.wire);
        if (!plane) {
            return std::nullopt;
        }

        BRepBuilderAPI_MakeFace faceBuilder(*plane, orientedWire(*plane, outer.wire, true));
        if (!faceBuilder.IsDone() || faceBuilder.Face().IsNull()) {
            return std::nullopt;
        }

        for (auto it = wireInfos.begin(); it != wireInfos.end();) {
            if (wireContainsWire(*plane, outer.wire, it->wire, it->area)) {
                faceBuilder.Add(orientedWire(*plane, it->wire, false));
                if (!faceBuilder.IsDone()) {
                    return std::nullopt;
                }
                it = wireInfos.erase(it);
            }
            else {
                ++it;
            }
        }

        faces.push_back(faceBuilder.Face());
    }

    return compoundOrSingleFace(faces);
}

FaceMakerBuildFaceResult makeFacesFromClosedWiresAndSplitEdgesDetailed(const std::vector<TopoDS_Wire>& wires,
                                                                       const std::vector<TopoDS_Edge>& splitEdges)
{
    if (wires.size() == 1U) {
        if (const auto singleWireFaces = makeSelfIntersectingSingleWireFaces(wires.front())) {
            if (splitEdges.empty()) {
                return *singleWireFaces;
            }
        }
    }

    const auto base = makeFaceWithHolesFromClosedWiresImpl(wires, true);
    if (!base || base->IsNull()) {
        return {};
    }
    const std::vector<TopoDS_Face> baseFaces = facesForShape(*base);
    if (splitEdges.empty()) {
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }

    TopTools_ListOfShape objects;
    objects.Append(*base);
    TopTools_ListOfShape tools;
    const std::vector<TopoDS_Edge> baseBoundaryEdges = edgesForShape(*base);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals() passes the sketch edge graph to FaceMakerBuildFace before
    // WireJoiner::getOpenWires(). Keep the open-edge component connected to the closed profile so
    // T-junction endpoints split generated boundaries; isolated internal dangling lines remain
    // WireJoiner open-wire candidates and are later filtered by noOriginal.
    const std::vector<TopoDS_Edge> selectedSplitEdges =
        connectedSplitEdgesFromBoundary(splitEdges, baseBoundaryEdges);
    for (const TopoDS_Edge& edge : selectedSplitEdges) {
        tools.Append(edge);
    }
    if (tools.IsEmpty()) {
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(objects);
    splitter.SetTools(tools);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence(), splitAtIntersections() failure continues with original "edges" instead
        // of dropping already valid bounded faces.
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }

    std::vector<TopoDS_Face> splitFaces = facesForShape(splitter.Shape());
    if (splitFaces.size() < baseFaces.size()) {
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }
    const auto splitShape = compoundOrSingleFace(splitFaces);
    if (!splitShape || splitShape->IsNull()) {
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }
    if (splitFaces.size() == baseFaces.size()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence(), BOPAlgo_BuilderFace uses "SetAvoidInternalShapes" after
        // splitAtIntersections(). A dangling line touching a face boundary splits the boundary edge
        // without keeping the dangling line as an internal face edge.
        const TopTools_ListOfShape faceMakerEdges = edgeNetworkForWiresAndEdges(wires, selectedSplitEdges);

        std::size_t rebuiltFaceCount = 0;
        bool rebuiltProducedSplit = false;
        const auto rebuilt = buildBoundedFacesFromEdgeNetwork(faceMakerEdges, rebuiltFaceCount, rebuiltProducedSplit);
        if (rebuilt && !rebuilt->IsNull() && rebuiltFaceCount == baseFaces.size()
            && topologyWasSplit(*rebuilt, *base)) {
            return FaceMakerBuildFaceResult{rebuilt, rebuiltFaceCount, false};
        }
        return FaceMakerBuildFaceResult{base, baseFaces.size(), false};
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::Build_Essence(), BOPAlgo_BuilderFace owns the split edge network. The splitter result is
    // only the bounded-region detector here; when BuilderFace reaches the same face count, keep its
    // topology so shared/result edge ownership matches SketchObject::buildInternals() more closely.
    std::size_t rebuiltFaceCount = 0;
    bool rebuiltProducedSplit = false;
    const auto rebuilt = buildBoundedFacesFromEdgeNetwork(edgeNetworkForWiresAndEdges(wires, selectedSplitEdges),
                                                          rebuiltFaceCount,
                                                          rebuiltProducedSplit);
    if (rebuilt && !rebuilt->IsNull() && rebuiltFaceCount == splitFaces.size()) {
        return FaceMakerBuildFaceResult{rebuilt, rebuiltFaceCount, true};
    }
    return FaceMakerBuildFaceResult{splitShape, splitFaces.size(), true};
}

std::optional<TopoDS_Shape> makeFacesFromClosedWiresAndSplitEdges(const std::vector<TopoDS_Wire>& wires,
                                                                  const std::vector<TopoDS_Edge>& splitEdges)
{
    return makeFacesFromClosedWiresAndSplitEdgesDetailed(wires, splitEdges).shape;
}

}  // namespace cad_core::geometry
