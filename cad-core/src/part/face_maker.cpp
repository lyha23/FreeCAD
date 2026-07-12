#include "cad_core/part/face_maker.h"

#include "cad_core/app/element_map_producer_trace.h"
#include "cad_core/part/element_map_producer_trace_snapshot.h"

#include "internal_shape_history_ledger_detail.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BOPAlgo_BuilderFace.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Builder.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRepLib_FindSurface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
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
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace cad_core::part
{

namespace
{

struct WireInfo
{
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

bool wireContainsWire(const gp_Pln& plane, const TopoDS_Wire& outer, const TopoDS_Wire& inner, double innerArea)
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

TopTools_ListOfShape edgeNetworkForWiresAndEdges(
    const std::vector<TopoDS_Wire>& wires,
    const std::vector<TopoDS_Edge>& extraEdges
)
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

TopTools_ListOfShape splitSelfIntersectingEdges(
    const TopTools_ListOfShape& edges,
    const gp_Pln& plane,
    bool& producedSplit
)
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
                const IntRes2d_IntersectionPoint& intersectionPoint
                    = selfIntersection.Intersector().Point(index);
                for (const Standard_Real parameter : std::array<Standard_Real, 2> {
                         intersectionPoint.ParamOnFirst(),
                         intersectionPoint.ParamOnSecond()
                     }) {
                    if (parameter - first > tolerance && last - parameter > tolerance) {
                        parameters.push_back(parameter);
                    }
                }

                Geom2dAPI_ProjectPointOnCurve
                    projection(selfIntersection.Point(index), curve2d, first, last);
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
            parameters.erase(
                std::unique(
                    parameters.begin(),
                    parameters.end(),
                    [tolerance](double lhs, double rhs) { return rhs - lhs < tolerance; }
                ),
                parameters.end()
            );

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
            for (TopTools_ListIteratorOfListOfShape fragmentIt(fragments); fragmentIt.More();
                 fragmentIt.Next()) {
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

struct EdgeSplitPoint
{
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
            auto duplicate = std::find_if(
                splitPoints.begin(),
                splitPoints.end(),
                [&](const EdgeSplitPoint& existing) {
                    return std::abs(existing.parameter - *parameter) <= Precision::Confusion();
                }
            );
            if (duplicate == splitPoints.end()) {
                splitPoints.push_back(EdgeSplitPoint {*parameter, vertex});
            }
        }
        if (splitPoints.empty()) {
            result.Append(edge);
            continue;
        }

        std::sort(
            splitPoints.begin(),
            splitPoints.end(),
            [](const EdgeSplitPoint& lhs, const EdgeSplitPoint& rhs) {
                return lhs.parameter < rhs.parameter;
            }
        );

        bool failed = false;
        Standard_Real previous = first;
        TopoDS_Vertex previousVertex = TopExp::FirstVertex(edge);
        TopTools_ListOfShape fragments;
        for (const EdgeSplitPoint& splitPoint : splitPoints) {
            if (splitPoint.parameter - previous > Precision::Confusion()) {
                BRepBuilderAPI_MakeEdge edgeBuilder(
                    curve,
                    previousVertex,
                    splitPoint.vertex,
                    previous,
                    splitPoint.parameter
                );
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
            BRepBuilderAPI_MakeEdge
                edgeBuilder(curve, previousVertex, TopExp::LastVertex(edge), previous, last);
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

        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence(), "splitAtIntersections()" runs before BOPAlgo_BuilderFace. FreeCAD's
        // edge graph also carries T-junction endpoint vertices into BuilderFace; cad-core must
        // split a touched edge at an open-edge endpoint and reuse that endpoint vertex.
        touched = true;
        for (TopTools_ListIteratorOfListOfShape fragmentIt(fragments); fragmentIt.More();
             fragmentIt.Next()) {
            result.Append(fragmentIt.Value());
        }
    }

    producedSplit = producedSplit || touched;
    return result;
}

std::size_t shapeListSize(const TopTools_ListOfShape& shapes)
{
    return static_cast<std::size_t>(shapes.Size());
}

struct FaceMakerEdgeLineageRecord
{
    TopoDS_Edge edge;
    std::vector<std::size_t> sourceEdgeIndices;
    bool preSplitHistory = false;
    bool splitterHistory = false;
    std::size_t targetEdgeIndex = 0;
};

void appendUniqueIndex(std::vector<std::size_t>& indices, std::size_t value)
{
    if (value == 0U) {
        return;
    }
    if (std::find(indices.begin(), indices.end(), value) == indices.end()) {
        indices.push_back(value);
    }
}

void appendUniqueIndices(std::vector<std::size_t>& target, const std::vector<std::size_t>& source)
{
    for (const std::size_t index : source) {
        appendUniqueIndex(target, index);
    }
}

std::vector<FaceMakerEdgeLineageRecord> lineageRecordsFromSourceEdges(const TopTools_ListOfShape& edges)
{
    std::vector<FaceMakerEdgeLineageRecord> records;
    std::size_t index = 0;
    for (TopTools_ListIteratorOfListOfShape it(edges); it.More(); it.Next()) {
        ++index;
        FaceMakerEdgeLineageRecord record;
        record.edge = TopoDS::Edge(it.Value());
        record.sourceEdgeIndices.push_back(index);
        records.push_back(std::move(record));
    }
    return records;
}

TopTools_ListOfShape shapeListFromLineageRecords(const std::vector<FaceMakerEdgeLineageRecord>& records)
{
    TopTools_ListOfShape shapes;
    for (const FaceMakerEdgeLineageRecord& record : records) {
        if (!record.edge.IsNull()) {
            shapes.Append(record.edge);
        }
    }
    return shapes;
}

std::vector<FaceMakerEdgeLineageRecord> splitSelfIntersectingLineageRecords(
    const std::vector<FaceMakerEdgeLineageRecord>& records,
    const gp_Pln& plane,
    bool& producedSplit
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::FaceMakerBuildFace::splitSelfIntersecting(), records "myPreSplitHistory->AddModified"
    // from the original edge to every fragment so FaceMaker::postBuild() can chain the history.
    const Standard_Real tolerance = Precision::Confusion();
    std::vector<FaceMakerEdgeLineageRecord> result;

    for (const FaceMakerEdgeLineageRecord& record : records) {
        const TopoDS_Edge& edge = record.edge;
        try {
            Standard_Real first = 0.0;
            Standard_Real last = 0.0;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
            if (curve.IsNull() || curve->IsKind(STANDARD_TYPE(Geom_Line))
                || curve->IsKind(STANDARD_TYPE(Geom_Conic))) {
                result.push_back(record);
                continue;
            }

            Handle(Geom2d_Curve) curve2d = GeomAPI::To2d(curve, plane);
            if (curve2d.IsNull()) {
                result.push_back(record);
                continue;
            }

            Geom2dAPI_InterCurveCurve selfIntersection(curve2d, tolerance);
            if (selfIntersection.NbPoints() == 0) {
                result.push_back(record);
                continue;
            }

            std::vector<Standard_Real> parameters;
            for (int index = 1; index <= selfIntersection.NbPoints(); ++index) {
                const IntRes2d_IntersectionPoint& intersectionPoint
                    = selfIntersection.Intersector().Point(index);
                for (const Standard_Real parameter : std::array<Standard_Real, 2> {
                         intersectionPoint.ParamOnFirst(),
                         intersectionPoint.ParamOnSecond()
                     }) {
                    if (parameter - first > tolerance && last - parameter > tolerance) {
                        parameters.push_back(parameter);
                    }
                }

                Geom2dAPI_ProjectPointOnCurve
                    projection(selfIntersection.Point(index), curve2d, first, last);
                for (int pointIndex = 1; pointIndex <= projection.NbPoints(); ++pointIndex) {
                    const Standard_Real parameter = projection.Parameter(pointIndex);
                    if (parameter - first > tolerance && last - parameter > tolerance) {
                        parameters.push_back(parameter);
                    }
                }
            }

            if (parameters.empty()) {
                result.push_back(record);
                continue;
            }

            std::sort(parameters.begin(), parameters.end());
            parameters.erase(
                std::unique(
                    parameters.begin(),
                    parameters.end(),
                    [tolerance](double lhs, double rhs) { return rhs - lhs < tolerance; }
                ),
                parameters.end()
            );

            std::vector<TopoDS_Edge> fragments;
            Standard_Real previous = first;
            for (const Standard_Real parameter : parameters) {
                if (parameter - previous > tolerance) {
                    BRepBuilderAPI_MakeEdge edgeBuilder(curve, previous, parameter);
                    if (edgeBuilder.IsDone()) {
                        fragments.push_back(edgeBuilder.Edge());
                    }
                    previous = parameter;
                }
            }
            if (last - previous > tolerance) {
                BRepBuilderAPI_MakeEdge edgeBuilder(curve, previous, last);
                if (edgeBuilder.IsDone()) {
                    fragments.push_back(edgeBuilder.Edge());
                }
            }

            if (fragments.empty()) {
                result.push_back(record);
                continue;
            }

            producedSplit = true;
            for (const TopoDS_Edge& fragment : fragments) {
                FaceMakerEdgeLineageRecord fragmentRecord = record;
                fragmentRecord.edge = fragment;
                fragmentRecord.preSplitHistory = true;
                result.push_back(std::move(fragmentRecord));
            }
        }
        catch (const Standard_Failure&) {
            result.push_back(record);
        }
        catch (...) {
            result.push_back(record);
        }
    }

    return result;
}

std::vector<FaceMakerEdgeLineageRecord> splitLineageRecordsAtTouchingEndpoints(
    const std::vector<FaceMakerEdgeLineageRecord>& records,
    bool& producedSplit
)
{
    std::vector<TopoDS_Vertex> endpointVertices;
    for (const FaceMakerEdgeLineageRecord& record : records) {
        if (record.edge.IsNull()) {
            continue;
        }
        endpointVertices.push_back(TopExp::FirstVertex(record.edge));
        endpointVertices.push_back(TopExp::LastVertex(record.edge));
    }
    if (records.size() <= 1U) {
        return records;
    }

    std::vector<FaceMakerEdgeLineageRecord> result;
    bool touched = false;
    for (const FaceMakerEdgeLineageRecord& record : records) {
        const TopoDS_Edge& edge = record.edge;
        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        if (curve.IsNull()) {
            result.push_back(record);
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
            auto duplicate = std::find_if(
                splitPoints.begin(),
                splitPoints.end(),
                [&](const EdgeSplitPoint& existing) {
                    return std::abs(existing.parameter - *parameter) <= Precision::Confusion();
                }
            );
            if (duplicate == splitPoints.end()) {
                splitPoints.push_back(EdgeSplitPoint {*parameter, vertex});
            }
        }
        if (splitPoints.empty()) {
            result.push_back(record);
            continue;
        }

        std::sort(
            splitPoints.begin(),
            splitPoints.end(),
            [](const EdgeSplitPoint& lhs, const EdgeSplitPoint& rhs) {
                return lhs.parameter < rhs.parameter;
            }
        );

        bool failed = false;
        Standard_Real previous = first;
        TopoDS_Vertex previousVertex = TopExp::FirstVertex(edge);
        std::vector<TopoDS_Edge> fragments;
        for (const EdgeSplitPoint& splitPoint : splitPoints) {
            if (splitPoint.parameter - previous > Precision::Confusion()) {
                BRepBuilderAPI_MakeEdge edgeBuilder(
                    curve,
                    previousVertex,
                    splitPoint.vertex,
                    previous,
                    splitPoint.parameter
                );
                if (!edgeBuilder.IsDone() || edgeBuilder.Edge().IsNull()) {
                    failed = true;
                    break;
                }
                fragments.push_back(edgeBuilder.Edge());
            }
            previous = splitPoint.parameter;
            previousVertex = splitPoint.vertex;
        }
        if (!failed && last - previous > Precision::Confusion()) {
            BRepBuilderAPI_MakeEdge
                edgeBuilder(curve, previousVertex, TopExp::LastVertex(edge), previous, last);
            if (!edgeBuilder.IsDone() || edgeBuilder.Edge().IsNull()) {
                failed = true;
            }
            else {
                fragments.push_back(edgeBuilder.Edge());
            }
        }
        if (failed || fragments.empty()) {
            result.push_back(record);
            continue;
        }

        touched = true;
        for (const TopoDS_Edge& fragment : fragments) {
            FaceMakerEdgeLineageRecord fragmentRecord = record;
            fragmentRecord.edge = fragment;
            fragmentRecord.splitterHistory = true;
            result.push_back(std::move(fragmentRecord));
        }
    }

    producedSplit = producedSplit || touched;
    return result;
}

void appendLineageForFaceMakerHistoryShape(
    std::vector<FaceMakerEdgeLineageRecord>& records,
    const TopoDS_Shape& historyShape,
    const FaceMakerEdgeLineageRecord& sourceRecord,
    bool markSplitterHistory
)
{
    for (TopExp_Explorer explorer(historyShape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge historyEdge = TopoDS::Edge(explorer.Current());
        for (FaceMakerEdgeLineageRecord& record : records) {
            if (!record.edge.IsNull() && record.edge.IsSame(historyEdge)) {
                appendUniqueIndices(record.sourceEdgeIndices, sourceRecord.sourceEdgeIndices);
                record.preSplitHistory = record.preSplitHistory || sourceRecord.preSplitHistory;
                record.splitterHistory = record.splitterHistory || markSplitterHistory;
            }
        }
    }
}

std::vector<FaceMakerEdgeLineageRecord> splitLineageRecordsAtIntersections(
    const std::vector<FaceMakerEdgeLineageRecord>& records,
    bool& producedSplit
)
{
    if (records.size() <= 1U) {
        return records;
    }

    TopTools_ListOfShape arguments;
    for (const FaceMakerEdgeLineageRecord& record : records) {
        if (!record.edge.IsNull()) {
            arguments.Append(record.edge);
        }
    }
    if (arguments.Size() <= 1) {
        return records;
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(arguments);
    splitter.SetToFillHistory(Standard_True);
    splitter.SetRunParallel(Standard_True);
    splitter.SetNonDestructive(Standard_True);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return records;
    }

    std::vector<FaceMakerEdgeLineageRecord> result;
    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_EDGE); explorer.More(); explorer.Next()) {
        FaceMakerEdgeLineageRecord record;
        record.edge = TopoDS::Edge(explorer.Current());
        result.push_back(std::move(record));
    }
    if (result.empty()) {
        return records;
    }
    const bool splitterProducedNewTargets = result.size() > records.size();

    for (const FaceMakerEdgeLineageRecord& inputRecord : records) {
        bool consumedByHistory = false;
        const TopTools_ListOfShape& modified = splitter.Modified(inputRecord.edge);
        if (!modified.IsEmpty()) {
            consumedByHistory = true;
            for (TopTools_ListIteratorOfListOfShape it(modified); it.More(); it.Next()) {
                appendLineageForFaceMakerHistoryShape(
                    result,
                    it.Value(),
                    inputRecord,
                    splitterProducedNewTargets
                );
            }
        }
        const TopTools_ListOfShape& generated = splitter.Generated(inputRecord.edge);
        if (!generated.IsEmpty()) {
            consumedByHistory = true;
            for (TopTools_ListIteratorOfListOfShape it(generated); it.More(); it.Next()) {
                appendLineageForFaceMakerHistoryShape(
                    result,
                    it.Value(),
                    inputRecord,
                    splitterProducedNewTargets
                );
            }
        }
        if (consumedByHistory || splitter.IsDeleted(inputRecord.edge)) {
            continue;
        }
        for (FaceMakerEdgeLineageRecord& outputRecord : result) {
            if (!outputRecord.edge.IsNull() && outputRecord.edge.IsSame(inputRecord.edge)) {
                appendUniqueIndices(outputRecord.sourceEdgeIndices, inputRecord.sourceEdgeIndices);
                outputRecord.preSplitHistory = outputRecord.preSplitHistory
                    || inputRecord.preSplitHistory;
                outputRecord.splitterHistory = outputRecord.splitterHistory
                    || inputRecord.splitterHistory;
            }
        }
    }

    producedSplit = producedSplit || result.size() > records.size();
    return result;
}

std::string faceMakerStageForRecord(const FaceMakerEdgeLineageRecord& record)
{
    if (record.splitterHistory) {
        return "facemaker:splitter";
    }
    if (record.preSplitHistory) {
        return "facemaker:pre_split";
    }
    return "facemaker:source";
}

void populateFaceMakerEdgeEvidence(
    FaceMakerHistorySummary& summary,
    const std::vector<FaceMakerEdgeLineageRecord>& records
)
{
    std::map<std::size_t, std::size_t> targetCountBySource;
    for (const FaceMakerEdgeLineageRecord& record : records) {
        if (record.targetEdgeIndex == 0U) {
            continue;
        }
        for (const std::size_t sourceIndex : record.sourceEdgeIndices) {
            ++targetCountBySource[sourceIndex];
        }
    }

    for (const FaceMakerEdgeLineageRecord& record : records) {
        for (const std::size_t sourceIndex : record.sourceEdgeIndices) {
            FaceMakerEdgeHistoryEvidence evidence;
            evidence.makerStage = faceMakerStageForRecord(record);
            evidence.relation = targetCountBySource[sourceIndex] > 1U ? "split" : "preserved";
            evidence.sourceEdgeIndex = sourceIndex;
            evidence.targetEdgeIndex = record.targetEdgeIndex;
            evidence.targetEdge = record.edge;
            evidence.preSplitHistory = record.preSplitHistory;
            evidence.splitterHistory = record.splitterHistory;
            summary.edgeEvidence.push_back(std::move(evidence));
        }
    }

    for (std::size_t sourceIndex = 1; sourceIndex <= summary.sourceEdgeCount; ++sourceIndex) {
        if (targetCountBySource[sourceIndex] != 0U) {
            continue;
        }
        FaceMakerEdgeHistoryEvidence evidence;
        evidence.makerStage = summary.splitterHistory ? "facemaker:splitter" : "facemaker:pre_split";
        evidence.relation = "deleted";
        evidence.sourceEdgeIndex = sourceIndex;
        evidence.preSplitHistory = summary.preSplitHistory;
        evidence.splitterHistory = summary.splitterHistory;
        summary.edgeEvidence.push_back(std::move(evidence));
    }
}

const FaceMakerEdgeLineageRecord* findLineageRecordForEdge(
    const std::vector<FaceMakerEdgeLineageRecord>& records,
    const TopoDS_Edge& edge
)
{
    for (const FaceMakerEdgeLineageRecord& record : records) {
        if (!record.edge.IsNull() && record.edge.IsSame(edge)) {
            return &record;
        }
    }
    return nullptr;
}

void populateFaceMakerBoundedFaceEvidence(
    FaceMakerHistorySummary& summary,
    const std::vector<TopoDS_Face>& faces,
    const std::vector<FaceMakerEdgeLineageRecord>& records
)
{
    std::size_t faceIndex = 0;
    for (const TopoDS_Face& face : faces) {
        ++faceIndex;
        FaceMakerBoundedFaceHistoryEvidence faceEvidence;
        faceEvidence.boundedFaceIndex = faceIndex;
        faceEvidence.face = face;
        const TopoDS_Wire outerWire = BRepTools::OuterWire(face);
        TopTools_IndexedMapOfShape faceEdges;
        const TopoDS_Shape outerBoundaryShape = outerWire.IsNull() ? TopoDS_Shape(face)
                                                                   : TopoDS_Shape(outerWire);
        TopExp::MapShapes(outerBoundaryShape, TopAbs_EDGE, faceEdges);
        for (int edgeIndex = 1; edgeIndex <= faceEdges.Extent(); ++edgeIndex) {
            const FaceMakerEdgeLineageRecord* record
                = findLineageRecordForEdge(records, TopoDS::Edge(faceEdges(edgeIndex)));
            if (record == nullptr) {
                continue;
            }
            appendUniqueIndex(faceEvidence.outerBoundaryTargetEdgeIndices, record->targetEdgeIndex);
            for (const std::size_t sourceIndex : record->sourceEdgeIndices) {
                appendUniqueIndex(faceEvidence.sourceEdgeIndices, sourceIndex);
                FaceMakerBoundedFaceBoundaryEvidence boundary;
                boundary.sourceEdgeIndex = sourceIndex;
                boundary.targetEdgeIndex = record->targetEdgeIndex;
                boundary.makerStage = faceMakerStageForRecord(*record);
                boundary.relation = record->splitterHistory || record->preSplitHistory
                    ? "split"
                    : "preserved";
                boundary.targetEdge = record->edge;
                faceEvidence.outerBoundary.push_back(std::move(boundary));
            }
        }
        summary.boundedFaceEvidence.push_back(std::move(faceEvidence));
    }
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

std::optional<TopoDS_Shape> buildBoundedFacesFromEdgeNetwork(
    const TopTools_ListOfShape& sourceEdges,
    std::size_t& faceCount,
    bool& producedSplit,
    FaceMakerHistorySummary* historySummary = nullptr
)
{
    const auto plane = planeForEdges(sourceEdges);
    if (!plane) {
        return std::nullopt;
    }
    if (historySummary != nullptr) {
        historySummary->sourceEdgeCount = shapeListSize(sourceEdges);
    }

    const std::size_t beforeSelfSplit = shapeListSize(sourceEdges);
    std::vector<FaceMakerEdgeLineageRecord> edgeRecords = lineageRecordsFromSourceEdges(sourceEdges);
    edgeRecords = splitSelfIntersectingLineageRecords(edgeRecords, *plane, producedSplit);
    const std::size_t afterSelfSplit = edgeRecords.size();
    if (historySummary != nullptr) {
        historySummary->preSplitEdgeCount = afterSelfSplit;
        historySummary->preSplitHistory = afterSelfSplit > beforeSelfSplit;
    }
    edgeRecords = splitLineageRecordsAtTouchingEndpoints(edgeRecords, producedSplit);
    const std::size_t beforeIntersections = edgeRecords.size();
    edgeRecords = splitLineageRecordsAtIntersections(edgeRecords, producedSplit);
    edgeRecords = splitLineageRecordsAtTouchingEndpoints(edgeRecords, producedSplit);
    for (std::size_t index = 0; index < edgeRecords.size(); ++index) {
        edgeRecords[index].targetEdgeIndex = index + 1U;
    }
    TopTools_ListOfShape edges = shapeListFromLineageRecords(edgeRecords);
    if (historySummary != nullptr) {
        historySummary->splitterEdgeCount = edgeRecords.size();
        historySummary->splitterHistory = historySummary->splitterEdgeCount > beforeIntersections
            || std::any_of(edgeRecords.begin(),
                           edgeRecords.end(),
                           [](const FaceMakerEdgeLineageRecord& record) {
                               return record.splitterHistory;
                           });
        populateFaceMakerEdgeEvidence(*historySummary, edgeRecords);
    }
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
    if (historySummary != nullptr) {
        historySummary->boundedFaceCount = faceCount;
        populateFaceMakerBoundedFaceEvidence(*historySummary, faces, edgeRecords);
    }
    return compoundOrSingleFace(faces);
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

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWiresImpl(const std::vector<TopoDS_Wire>& wires)
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
        wireInfos.push_back(WireInfo {wire, *area, 0U, wireHasBSplineEdge(wire)});
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
            if (wireContainsWire(
                    *plane,
                    wireInfos[parent].wire,
                    wireInfos[index].wire,
                    wireInfos[index].area
                )) {
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
        BRepBuilderAPI_MakeFace faceBuilder(
            *plane,
            orientedWire(*plane, wireInfos[outerIndex].wire, true)
        );
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

    return splitOverlappingFaces(faces);
}

}  // namespace

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires)
{
    return makeFaceWithHolesFromClosedWiresImpl(wires);
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
        wireInfos.push_back(WireInfo {wire, *area, 0U});
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

FaceMakerBuildFaceResult makeFacesFromClosedWiresAndSplitEdgesDetailed(
    const std::vector<TopoDS_Wire>& wires,
    const std::vector<TopoDS_Edge>& splitEdges,
    app::ElementMapProducerTrace* producerTrace
)
{
    app::ElementMapProducerTrace::Scope traceScope;
    if (producerTrace != nullptr) {
        traceScope = producerTrace->scope(
            {"FaceMaker::postBuild",
             "",
             0,
             "Part::FaceMakerBuildFace",
             {{"closedWireCount", wires.size()},
              {"splitEdgeCount", splitEdges.size()},
              {"requiresFinalCheckpoint", true}}}
        );
        producerTrace->record({
            "face_maker.lifecycle",
            "begin",
            "face_maker_inputs_frozen",
            {{"closedWireCount", wires.size()},
             {"splitEdgeCount", splitEdges.size()},
             {"preSplit", !splitEdges.empty()}},
        });
    }
    if (wires.empty()) {
        if (producerTrace != nullptr) {
            producerTrace->record({
                "face_maker.lifecycle", "rejected", "no_closed_wires", nlohmann::json::object()
            });
            producerTrace->checkpoint(
                {"state",
                 {{"producer", "FaceMakerBuildFace"},
                  {"outcome", "rejected"},
                  {"reason", "no_closed_wires"},
                  {"closedWireCount", 0},
                  {"splitEdgeCount", splitEdges.size()}},
                 {},
                 {},
                 {},
                 "maker.final_checkpoint"}
            );
            traceScope.abort("no_closed_wires");
        }
        return {};
    }

    const auto profileFace = makeFaceWithHolesFromClosedWiresImpl(wires);
    if (!profileFace && splitEdges.empty() && wires.size() > 1U) {
        // FreeCAD:
        // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
        // ::Build_Essence(), intersecting BSpline closed profiles can fail before publishing a
        // usable InternalShape; SketchObject::buildInternals() then returns an empty TopoShape.
        if (producerTrace != nullptr) {
            producerTrace->record({
                "face_maker.lifecycle",
                "failed",
                "multi_wire_profile_reconstruction_failed",
                {{"closedWireCount", wires.size()},
                 {"splitEdgeCount", splitEdges.size()},
                 {"partialWrite", false}},
            });
            producerTrace->checkpoint(
                {"state",
                 {{"producer", "FaceMakerBuildFace"},
                  {"outcome", "failed"},
                  {"reason", "multi_wire_profile_reconstruction_failed"},
                  {"closedWireCount", wires.size()},
                  {"splitEdgeCount", splitEdges.size()}},
                 {},
                 {},
                 {},
                 "maker.final_checkpoint"}
            );
            traceScope.abort("multi_wire_profile_reconstruction_failed");
        }
        return {};
    }

    std::size_t boundedFaceCount = 0;
    bool producedSplit = false;
    FaceMakerHistorySummary historySummary;
    const TopTools_ListOfShape faceMakerEdges = edgeNetworkForWiresAndEdges(wires, splitEdges);
    const auto boundedFaces = buildBoundedFacesFromEdgeNetwork(
        faceMakerEdges,
        boundedFaceCount,
        producedSplit,
        &historySummary
    );
    if (!boundedFaces || boundedFaces->IsNull() || boundedFaceCount == 0U) {
        if (producerTrace != nullptr) {
            producerTrace->record({
                "face_maker.lifecycle",
                "failed",
                "builder_face_no_bounded_result",
                {{"splitterInputCount", faceMakerEdges.Size()},
                 {"boundedFaceCount", boundedFaceCount},
                 {"producedSplit", producedSplit}},
            });
            producerTrace->checkpoint(
                {"state",
                 {{"producer", "FaceMakerBuildFace"},
                  {"outcome", "failed"},
                  {"reason", "builder_face_no_bounded_result"},
                  {"splitterInputCount", faceMakerEdges.Size()},
                  {"boundedFaceCount", boundedFaceCount},
                  {"producedSplit", producedSplit}},
                 {},
                 {},
                 {},
                 "maker.final_checkpoint"}
            );
            traceScope.abort("builder_face_no_bounded_result");
        }
        return {};
    }

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp
    // ::Build_Essence(), after splitSelfIntersecting()/splitAtIntersections(), feeds every edge to
    // BOPAlgo_BuilderFace and stores each bounded area directly in "myShapesToReturn". This is the
    // shape SketchObject::buildInternals() publishes as InternalShape and the face a PartDesign
    // Profile consumes. A BRepBuilderAPI_MakeFace-compatible reconstruction has equivalent area,
    // but a different TShape traversal order and therefore produces different Prism history.
    const std::size_t profileFaceCount = profileFace ? facesForShape(*profileFace).size() : 0U;
    historySummary.profileResultSource = FaceMakerBuildFaceRuntimeSource::BuilderFace;
    historySummary.internalResultSource = FaceMakerBuildFaceRuntimeSource::BuilderFace;
    historySummary.topologySwitchUsed = false;

    const bool splitProducedBoundedFaces = producedSplit || boundedFaceCount > profileFaceCount;
    // The source-wire reconstruction only remains evidence for deciding whether a splitter created
    // additional regions. It must not replace the BuilderFace result: its traversal order is part
    // of the producer's ElementMap lifecycle, not a display-only ordering detail.
    const std::optional<TopoDS_Shape> internalShape = boundedFaces;
    InternalShapeHistoryLedger historyLedger;
    addFaceMakerEvidenceToLedger(historyLedger, historySummary);
    if (producerTrace != nullptr) {
        nlohmann::json namesUsed = nlohmann::json::array();
        nlohmann::json comboNames = nlohmann::json::array();
        for (const FaceMakerBoundedFaceHistoryEvidence& face :
             historySummary.boundedFaceEvidence) {
            nlohmann::json orderedNames = nlohmann::json::array();
            nlohmann::json orderedRefs = nlohmann::json::array();
            std::string comboName;
            for (const std::size_t sourceEdgeIndex : face.sourceEdgeIndices) {
                const std::string name = "Edge" + std::to_string(sourceEdgeIndex);
                orderedNames.push_back(name);
                orderedRefs.push_back({
                    {"sourceIndexed", name},
                    {"sourceEdgeIndex", sourceEdgeIndex},
                });
                if (!comboName.empty()) {
                    comboName += "+";
                }
                comboName += name;
            }
            namesUsed.push_back({
                {"boundedFaceIndex", face.boundedFaceIndex},
                {"orderedNames", orderedNames},
            });
            comboNames.push_back({
                {"boundedFaceIndex", face.boundedFaceIndex},
                {"rawName", comboName},
                {"orderedSourceRefs", orderedRefs},
                {"sidStatus", "not_allocated_in_face_maker_scope"},
            });
        }
        const nlohmann::json finalPayload = {
            {"producer", "FaceMakerBuildFace"},
            {"outcome", "success"},
            {"faceCount", boundedFaceCount},
            {"namesUsed", namesUsed},
            {"comboNames", comboNames},
            {"history", historyLedger.diagnosticsJson()},
            {"outputInventory", inspectShapeInventory(*boundedFaces)},
        };
        producerTrace->record({
            "face_maker.lifecycle",
            "success",
            "bounded_face_history_published",
            {{"faceCount", boundedFaceCount},
             {"profileFaceCount", profileFaceCount},
             {"splitProducedBoundedFaces", splitProducedBoundedFaces},
             {"namesUsed", namesUsed},
             {"comboNames", comboNames},
             {"history", historyLedger.diagnosticsJson()},
             {"outputInventory", inspectShapeInventory(*boundedFaces)}},
        });
        producerTrace->checkpoint(
            {"state", finalPayload, {}, {}, {}, "maker.final_checkpoint"}
        );
    }
    return FaceMakerBuildFaceResult {
        boundedFaces,
        internalShape,
        boundedFaceCount,
        splitProducedBoundedFaces,
        historyLedger,
    };
}

std::optional<TopoDS_Shape> makeFacesFromClosedWiresAndSplitEdges(
    const std::vector<TopoDS_Wire>& wires,
    const std::vector<TopoDS_Edge>& splitEdges
)
{
    return makeFacesFromClosedWiresAndSplitEdgesDetailed(wires, splitEdges, nullptr).shape;
}

}  // namespace cad_core::part
