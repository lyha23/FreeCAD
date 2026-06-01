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
#include <GeomAdaptor_Surface.hxx>
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

namespace cad_core::geometry {

namespace {

struct WireInfo {
    TopoDS_Wire wire;
    double area = 0.0;
    std::size_t depth = 0;
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
    edges = splitEdgesAtIntersections(edges, producedSplit);
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

}  // namespace

std::optional<TopoDS_Shape> makeFaceWithHolesFromClosedWires(const std::vector<TopoDS_Wire>& wires)
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
    if (wires.size() >= 3U && rebuilt && !rebuilt->IsNull() && rebuiltFaceCount == facesForShape(*result).size()
        && rebuiltFaceCount > wires.size()) {
        return rebuilt;
    }
    return result;
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

    const auto base = makeFaceWithHolesFromClosedWires(wires);
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
    std::vector<TopoDS_Edge> selectedSplitEdges;
    const std::vector<TopoDS_Edge> baseBoundaryEdges = edgesForShape(*base);
    for (const TopoDS_Edge& edge : splitEdges) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals() lets FaceMakerBuildFace form bounded faces first, then
        // WireJoiner::getOpenWires() accounts for leftover open wires. In this bounded-face subset,
        // only open edges that touch the bounded-face boundary can split the face; isolated internal
        // dangling lines remain WireJoiner open-wire candidates and are later filtered by noOriginal.
        if (!edge.IsNull() && edgeTouchesAnyBoundary(edge, baseBoundaryEdges)) {
            tools.Append(edge);
            selectedSplitEdges.push_back(edge);
        }
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
