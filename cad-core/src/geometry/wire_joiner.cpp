#include "cad_core/geometry/wire_joiner.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <Precision.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <deque>
#include <utility>

namespace cad_core::geometry {

namespace {

std::vector<TopoDS_Edge> wireEdges(const TopoDS_Wire& wire)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

bool samePoint(const gp_Pnt& lhs, const gp_Pnt& rhs)
{
    return lhs.SquareDistance(rhs) <= Precision::SquareConfusion();
}

std::pair<gp_Pnt, gp_Pnt> edgeEndpoints(const TopoDS_Edge& edge)
{
    return {BRep_Tool::Pnt(TopExp::FirstVertex(edge)), BRep_Tool::Pnt(TopExp::LastVertex(edge))};
}

bool edgeMatchesSourceVertices(const TopoDS_Edge& edge, const TopoDS_Edge& source)
{
    const auto [first, last] = edgeEndpoints(edge);
    const auto [sourceFirst, sourceLast] = edgeEndpoints(source);
    return (samePoint(first, sourceFirst) && samePoint(last, sourceLast))
        || (samePoint(first, sourceLast) && samePoint(last, sourceFirst));
}

std::vector<TopoDS_Edge> boundaryEdges(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

std::vector<TopoDS_Face> facesForShape(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        faces.push_back(TopoDS::Face(explorer.Current()));
    }
    return faces;
}

std::vector<TopoDS_Edge> splitOpenEdgeByFaceBoundaries(const TopoDS_Edge& edge,
                                                       const std::vector<TopoDS_Edge>& faceEdges)
{
    if (faceEdges.empty()) {
        return {edge};
    }

    TopTools_ListOfShape arguments;
    arguments.Append(edge);
    TopTools_ListOfShape tools;
    for (const TopoDS_Edge& faceEdge : faceEdges) {
        if (!faceEdge.IsNull()) {
            tools.Append(faceEdge);
        }
    }
    if (tools.IsEmpty()) {
        return {edge};
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(arguments);
    splitter.SetTools(tools);
    splitter.SetNonDestructive(Standard_True);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return {edge};
    }

    std::vector<TopoDS_Edge> fragments;
    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_EDGE); explorer.More(); explorer.Next()) {
        fragments.push_back(TopoDS::Edge(explorer.Current()));
    }
    if (fragments.empty()) {
        return {edge};
    }
    return fragments;
}

std::vector<TopoDS_Wire> wiresFromEdges(const std::vector<TopoDS_Edge>& edges)
{
    std::vector<TopoDS_Wire> wires;
    std::vector<bool> used(edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < edges.size(); ++startIndex) {
        if (used[startIndex] || edges[startIndex].IsNull()) {
            continue;
        }

        BRepBuilderAPI_MakeWire builder;
        std::deque<TopoDS_Edge> ordered;
        ordered.push_back(edges[startIndex]);
        used[startIndex] = true;
        auto [currentStart, currentEnd] = edgeEndpoints(edges[startIndex]);

        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (used[index] || edges[index].IsNull()) {
                    continue;
                }
                const auto [edgeStart, edgeEnd] = edgeEndpoints(edges[index]);
                if (samePoint(edgeStart, currentEnd)) {
                    ordered.push_back(edges[index]);
                    currentEnd = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentEnd)) {
                    ordered.push_back(TopoDS::Edge(edges[index].Reversed()));
                    currentEnd = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentStart)) {
                    ordered.push_front(edges[index]);
                    currentStart = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeStart, currentStart)) {
                    ordered.push_front(TopoDS::Edge(edges[index].Reversed()));
                    currentStart = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
            }
        }

        for (const TopoDS_Edge& edge : ordered) {
            builder.Add(edge);
        }
        if (builder.IsDone()) {
            wires.push_back(builder.Wire());
        }
    }

    return wires;
}

bool edgeMatchesAnyBoundary(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& boundaryEdges)
{
    for (const TopoDS_Edge& boundary : boundaryEdges) {
        if (edgeMatchesSourceVertices(edge, boundary)) {
            return true;
        }
    }
    return false;
}

std::vector<TopoDS_Vertex> edgeVertices(const TopoDS_Edge& edge)
{
    std::vector<TopoDS_Vertex> vertices;
    for (TopExp_Explorer explorer(edge, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
        vertices.push_back(TopoDS::Vertex(explorer.Current()));
    }
    return vertices;
}

bool vertexTouchesBoundary(const TopoDS_Vertex& vertex, const TopoDS_Edge& boundary)
{
    if (vertex.IsNull() || boundary.IsNull()) {
        return false;
    }
    BRepExtrema_DistShapeShape distance(vertex, boundary);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= Precision::Confusion();
}

bool edgeTouchesBoundary(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& boundaryEdges)
{
    for (const TopoDS_Vertex& vertex : edgeVertices(edge)) {
        for (const TopoDS_Edge& boundary : boundaryEdges) {
            if (vertexTouchesBoundary(vertex, boundary)) {
                return true;
            }
        }
    }
    return false;
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

bool pointInsideOrOnAnyFace(const gp_Pnt& point, const std::vector<TopoDS_Face>& faces)
{
    for (const TopoDS_Face& face : faces) {
        if (face.IsNull()) {
            continue;
        }
        BRepClass_FaceClassifier classifier(face, point, Precision::Confusion());
        if (classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON) {
            return true;
        }
    }
    return false;
}

bool wireContainsAnyEdge(const TopoDS_Wire& wire, const std::vector<TopoDS_Edge>& edges)
{
    if (edges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& wireEdge : wireEdges(wire)) {
        for (const TopoDS_Edge& edge : edges) {
            if (wireEdge.IsSame(edge) || edgeMatchesSourceVertices(wireEdge, edge)) {
                return true;
            }
        }
    }
    return false;
}

bool edgeSharesSourceVertex(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& sourceEdges)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }

    for (const TopoDS_Edge& sourceEdge : sourceEdges) {
        for (const TopoDS_Vertex& sourceVertex : edgeVertices(sourceEdge)) {
            for (const TopoDS_Vertex& vertex : vertices) {
                if (vertex.IsSame(sourceVertex)
                    || samePoint(BRep_Tool::Pnt(vertex), BRep_Tool::Pnt(sourceVertex))) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool allEdgesShareOriginalSourceVertex(const TopoDS_Wire& wire, const std::vector<TopoDS_Edge>& sourceEdges)
{
    if (sourceEdges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& edge : wireEdges(wire)) {
        if (!edgeSharesSourceVertex(edge, sourceEdges)) {
            return false;
        }
    }
    return true;
}

std::vector<TopoDS_Edge> uniqueEdgesForShape(const TopoDS_Shape& shape)
{
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);

    std::vector<TopoDS_Edge> result;
    result.reserve(static_cast<std::size_t>(edges.Extent()));
    for (int index = 1; index <= edges.Extent(); ++index) {
        result.push_back(TopoDS::Edge(edges(index)));
    }
    return result;
}

bool pointOnOpenEdge(const gp_Pnt& point, const std::vector<TopoDS_Edge>& openEdges)
{
    if (openEdges.empty()) {
        return false;
    }
    const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    for (const TopoDS_Edge& edge : openEdges) {
        BRepExtrema_DistShapeShape distance(vertex, edge);
        distance.Perform();
        if (distance.IsDone() && distance.Value() <= Precision::Confusion()) {
            return true;
        }
    }
    return false;
}

TopoDS_Vertex cachedCopiedVertex(std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices,
                                 const gp_Pnt& point)
{
    for (const auto& [existingPoint, vertex] : copiedVertices) {
        if (samePoint(existingPoint, point)) {
            return vertex;
        }
    }
    TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    copiedVertices.emplace_back(point, vertex);
    return vertex;
}

TopoDS_Vertex resultWireVertex(const TopoDS_Vertex& sourceVertex,
                               const std::vector<TopoDS_Edge>& openEdges,
                               bool copyAllVertices,
                               std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices)
{
    const gp_Pnt point = BRep_Tool::Pnt(sourceVertex);
    if (copyAllVertices || pointOnOpenEdge(point, openEdges)) {
        return cachedCopiedVertex(copiedVertices, point);
    }
    return sourceVertex;
}

TopoDS_Edge copyEdgeWithResultWireVertices(const TopoDS_Edge& edge,
                                           const std::vector<TopoDS_Edge>& openEdges,
                                           bool copyAllVertices,
                                           std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    const TopoDS_Vertex start = resultWireVertex(TopExp::FirstVertex(edge), openEdges, copyAllVertices, copiedVertices);
    const TopoDS_Vertex end = resultWireVertex(TopExp::LastVertex(edge), openEdges, copyAllVertices, copiedVertices);
    if (!curve.IsNull()) {
        BRepBuilderAPI_MakeEdge builder(curve, start, end, first, last);
        if (builder.IsDone() && !builder.Edge().IsNull()) {
            return builder.Edge();
        }
    }
    BRepBuilderAPI_MakeEdge fallback(start, end);
    return fallback.Edge();
}

bool allOpenEdgeEndpointsTouchBoundary(const std::vector<TopoDS_Edge>& openEdges, const TopoDS_Shape& faceShape)
{
    if (openEdges.empty() || faceShape.IsNull()) {
        return false;
    }
    const std::vector<TopoDS_Edge> faceBoundaryEdges = boundaryEdges(faceShape);
    if (faceBoundaryEdges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& edge : openEdges) {
        const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
        if (vertices.empty()) {
            return false;
        }
        for (const TopoDS_Vertex& vertex : vertices) {
            bool touches = false;
            for (const TopoDS_Edge& boundary : faceBoundaryEdges) {
                if (vertexTouchesBoundary(vertex, boundary)) {
                    touches = true;
                    break;
                }
            }
            if (!touches) {
                return false;
            }
        }
    }
    return true;
}

bool allOpenEdgeEndpointsTouchClosedWireBoundary(const std::vector<TopoDS_Edge>& openEdges,
                                                 const std::vector<TopoDS_Wire>& closedWires)
{
    if (openEdges.empty() || closedWires.empty()) {
        return false;
    }

    std::vector<TopoDS_Edge> boundaryEdges;
    for (const TopoDS_Wire& wire : closedWires) {
        const std::vector<TopoDS_Edge> edges = wireEdges(wire);
        boundaryEdges.insert(boundaryEdges.end(), edges.begin(), edges.end());
    }
    if (boundaryEdges.empty()) {
        return false;
    }

    for (const TopoDS_Edge& edge : openEdges) {
        const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
        if (vertices.empty()) {
            return false;
        }
        for (const TopoDS_Vertex& vertex : vertices) {
            bool touches = false;
            for (const TopoDS_Edge& boundary : boundaryEdges) {
                if (vertexTouchesBoundary(vertex, boundary)) {
                    touches = true;
                    break;
                }
            }
            if (!touches) {
                return false;
            }
        }
    }
    return true;
}

bool edgeEndpointsTouchBoundary(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& boundaryEdges)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }
    for (const TopoDS_Vertex& vertex : vertices) {
        bool touches = false;
        for (const TopoDS_Edge& boundary : boundaryEdges) {
            if (vertexTouchesBoundary(vertex, boundary)) {
                touches = true;
                break;
            }
        }
        if (!touches) {
            return false;
        }
    }
    return true;
}

bool pointOnEdge(const gp_Pnt& point, const TopoDS_Edge& edge)
{
    if (edge.IsNull()) {
        return false;
    }
    const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point).Vertex();
    BRepExtrema_DistShapeShape distance(vertex, edge);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= Precision::Confusion();
}

bool edgeSamplesLieOnEdge(const TopoDS_Edge& edge, const TopoDS_Edge& source)
{
    if (edge.IsNull() || source.IsNull()) {
        return false;
    }
    const auto [start, end] = edgeEndpoints(edge);
    return pointOnEdge(start, source) && pointOnEdge(edgeMidpoint(edge), source) && pointOnEdge(end, source);
}

bool edgeLiesOnAnyEdge(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& sources)
{
    for (const TopoDS_Edge& source : sources) {
        if (edgeSamplesLieOnEdge(edge, source)) {
            return true;
        }
    }
    return false;
}

std::vector<TopoDS_Edge> closedWireBoundaryEdges(const std::vector<TopoDS_Wire>& closedWires)
{
    std::vector<TopoDS_Edge> edges;
    for (const TopoDS_Wire& wire : closedWires) {
        const std::vector<TopoDS_Edge> wireBoundary = wireEdges(wire);
        edges.insert(edges.end(), wireBoundary.begin(), wireBoundary.end());
    }
    return edges;
}

std::vector<TopoDS_Edge> edgesContainingEdge(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& sources)
{
    std::vector<TopoDS_Edge> matches;
    for (const TopoDS_Edge& source : sources) {
        if (edgeSamplesLieOnEdge(edge, source)) {
            matches.push_back(source);
        }
    }
    return matches;
}

bool edgeEquivalentByGeometryAndEndpoints(const TopoDS_Edge& left, const TopoDS_Edge& right)
{
    return edgeMatchesSourceVertices(left, right) && edgeSamplesLieOnEdge(left, right)
        && edgeSamplesLieOnEdge(right, left);
}

int countEquivalentEdges(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& candidates)
{
    int count = 0;
    for (const TopoDS_Edge& candidate : candidates) {
        if (edgeEquivalentByGeometryAndEndpoints(edge, candidate)) {
            ++count;
        }
    }
    return count;
}

std::vector<TopoDS_Edge> openEdgesWithInteriorEndpoint(const std::vector<TopoDS_Edge>& openEdges,
                                                       const std::vector<TopoDS_Edge>& closedBoundaryEdges)
{
    std::vector<TopoDS_Edge> edges;
    for (const TopoDS_Edge& edge : openEdges) {
        if (!edgeEndpointsTouchBoundary(edge, closedBoundaryEdges)) {
            edges.push_back(edge);
        }
    }
    return edges;
}

std::optional<TopoDS_Shape> copiedResultWireGraph(const TopoDS_Shape& boundedFaceShape,
                                                  const std::vector<TopoDS_Edge>& openEdges,
                                                  bool copyAllVertices)
{
    const std::vector<TopoDS_Edge> edges = uniqueEdgesForShape(boundedFaceShape);
    if (edges.empty()) {
        return std::nullopt;
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>> copiedVertices;
    for (const TopoDS_Edge& edge : edges) {
        builder.Add(compound, copyEdgeWithResultWireVertices(edge, openEdges, copyAllVertices, copiedVertices));
    }
    return compound;
}

std::optional<TopoDS_Shape> copiedPartialSharedClosedWireEdges(const TopoDS_Shape& boundedFaceShape,
                                                              const std::vector<TopoDS_Wire>& closedWires)
{
    const std::vector<TopoDS_Edge> closedBoundaryEdges = closedWireBoundaryEdges(closedWires);
    const std::vector<TopoDS_Edge> resultEdges = uniqueEdgesForShape(boundedFaceShape);
    if (closedBoundaryEdges.empty() || resultEdges.empty()) {
        return std::nullopt;
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>> copiedVertices;
    bool added = false;
    for (const TopoDS_Edge& edge : resultEdges) {
        const std::vector<TopoDS_Edge> containingSources = edgesContainingEdge(edge, closedBoundaryEdges);
        if (containingSources.size() < 2U) {
            continue;
        }
        const bool partialOverlap = std::any_of(containingSources.begin(),
                                                containingSources.end(),
                                                [&](const TopoDS_Edge& source) {
                                                    return !edgeEquivalentByGeometryAndEndpoints(edge, source);
                                                });
        if (!partialOverlap || countEquivalentEdges(edge, resultEdges) > 1) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), after findTightBound()/exhaustTightBound(), keeps
        // shared partial closed-wire edges as result-wire evidence. Full coincident source edges
        // are not duplicated; partial overlaps need a copied edge so Sketch InternalShape keeps
        // the same edge/vertex ledger as FreeCAD.
        builder.Add(compound, copyEdgeWithResultWireVertices(edge, {}, true, copiedVertices));
        added = true;
    }
    if (!added) {
        return std::nullopt;
    }
    return compound;
}

std::optional<TopoDS_Shape> copiedPartialJunctionResultWireGraph(const TopoDS_Shape& boundedFaceShape,
                                                                 const std::vector<TopoDS_Wire>& closedWires,
                                                                 const std::vector<TopoDS_Edge>& openEdges)
{
    const std::vector<TopoDS_Edge> closedBoundaryEdges = closedWireBoundaryEdges(closedWires);
    const std::vector<TopoDS_Edge> partialOpenEdges = openEdgesWithInteriorEndpoint(openEdges, closedBoundaryEdges);
    if (closedBoundaryEdges.empty() || partialOpenEdges.empty()) {
        return std::nullopt;
    }

    const std::vector<TopoDS_Edge> edges = uniqueEdgesForShape(boundedFaceShape);
    if (edges.empty()) {
        return std::nullopt;
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    std::vector<std::pair<gp_Pnt, TopoDS_Vertex>> copiedVertices;
    bool added = false;
    for (const TopoDS_Edge& edge : edges) {
        if (!edgeLiesOnAnyEdge(edge, closedBoundaryEdges) && !edgeLiesOnAnyEdge(edge, partialOpenEdges)) {
            continue;
        }
        builder.Add(compound, copyEdgeWithResultWireVertices(edge, openEdges, false, copiedVertices));
        added = true;
    }

    if (!added) {
        return std::nullopt;
    }
    return compound;
}

}  // namespace

void WireJoiner::setTightBound(bool enabled)
{
    tightBound_ = enabled;
}

void WireJoiner::setMergeEdges(bool enabled)
{
    mergeEdges_ = enabled;
}

void WireJoiner::addOpenWire(const TopoDS_Wire& wire)
{
    if (!wire.IsNull()) {
        WireInfo info;
        info.wire = wire;
        for (const TopoDS_Edge& edge : wireEdges(wire)) {
            info.fragments.push_back(WireInfo::EdgeFragment{edge});
        }
        openWires_.push_back(std::move(info));
    }
}

void WireJoiner::addSourceEdge(const TopoDS_Edge& edge)
{
    if (!edge.IsNull()) {
        sourceEdges_.push_back(edge);
    }
}

void WireJoiner::classifyBoundedFaceOwnership(const TopoDS_Shape& boundedFaceShape)
{
    if (boundedFaceShape.IsNull() || openWires_.empty()) {
        return;
    }

    const std::vector<TopoDS_Edge> faceBoundaryEdges = boundaryEdges(boundedFaceShape);
    const std::vector<TopoDS_Face> boundedFaces = facesForShape(boundedFaceShape);
    if (faceBoundaryEdges.empty() || boundedFaces.empty()) {
        return;
    }

    for (WireInfo& info : openWires_) {
        std::vector<WireInfo::EdgeFragment> fragments;
        for (const WireInfo::EdgeFragment& fragment : info.fragments) {
            const auto split = splitOpenEdgeByFaceBoundaries(fragment.edge, faceBoundaryEdges);
            const bool splitFromInputEdge = fragment.splitFromInputEdge || split.size() != 1U
                || (split.size() == 1U && !edgeMatchesSourceVertices(split.front(), fragment.edge));
            for (const TopoDS_Edge& edge : split) {
                fragments.push_back(WireInfo::EdgeFragment{edge, splitFromInputEdge});
            }
        }
        info.fragments = std::move(fragments);
    }

    for (WireInfo& info : openWires_) {
        for (WireInfo::EdgeFragment& fragment : info.fragments) {
            if (edgeMatchesAnyBoundary(fragment.edge, faceBoundaryEdges)) {
                fragment.ownership = WireInfo::FragmentOwnership::ConsumedByBoundedFace;
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::splitEdges() and ::WireJoinerP::build() export copied/split open-wire
            // EdgeInfo states. This bounded-face subset keeps only boundary-touch fragments whose
            // midpoint is outside the bounded face and that came from a split/copy result edge;
            // unsplit original dangling edges still go through getOpenWires(noOriginal=true)
            // purging. Delete this marker when full EdgeInfo/WireInfo identity history is migrated.
            if (fragment.splitFromInputEdge
                && edgeTouchesBoundary(fragment.edge, faceBoundaryEdges)
                && !pointInsideOrOnAnyFace(edgeMidpoint(fragment.edge), boundedFaces)) {
                fragment.ownership = WireInfo::FragmentOwnership::RetainedResultFragment;
            }
        }
    }
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(const std::string& historyPrefix, bool noOriginal) const
{
    (void)historyPrefix;
    (void)tightBound_;

    std::vector<TopoDS_Edge> allLiveEdges;
    std::vector<TopoDS_Edge> retainedByBoundaryTouchEdges;
    std::vector<TopoDS_Wire> liveWires;
    for (const WireInfo& info : openWires_) {
        std::vector<TopoDS_Edge> liveEdges;
        for (const WireInfo::EdgeFragment& fragment : info.fragments) {
            if (fragment.ownership == WireInfo::FragmentOwnership::ConsumedByBoundedFace) {
                continue;
            }
            liveEdges.push_back(fragment.edge);
            if (fragment.ownership == WireInfo::FragmentOwnership::RetainedResultFragment) {
                retainedByBoundaryTouchEdges.push_back(fragment.edge);
            }
        }
        if (liveEdges.empty()) {
            continue;
        }
        if (mergeEdges_) {
            allLiveEdges.insert(allLiveEdges.end(), liveEdges.begin(), liveEdges.end());
            continue;
        }
        const auto currentWires = wiresFromEdges(liveEdges);
        liveWires.insert(liveWires.end(), currentWires.begin(), currentWires.end());
    }
    if (mergeEdges_ && !allLiveEdges.empty()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() collects final EdgeInfo states into openWireCompound after
        // splitEdges()/findTightBound(), so connected leftover fragments are result wires rather
        // than isolated per-input wires.
        liveWires = wiresFromEdges(allLiveEdges);
    }
    if (noOriginal && !sourceEdges_.empty()) {
        liveWires.erase(std::remove_if(liveWires.begin(),
                                       liveWires.end(),
                                       [&](const TopoDS_Wire& wire) {
                                           // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                                           // ::WireJoinerP::getOpenWires(), calls
                                           // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))".
                                           // FreeCAD uses topological shared vertices here. This subset
                                           // also accepts same-coordinate source endpoints because cad-core
                                           // does not yet preserve the full EdgeInfo identity ledger; edges
                                           // marked by bounded-face boundary touch represent the copied/split
                                           // remnants that must survive this compatibility filter.
                                           return !wireContainsAnyEdge(wire, retainedByBoundaryTouchEdges)
                                               && allEdgesShareOriginalSourceVertex(wire, sourceEdges_);
                                       }),
                        liveWires.end());
    }
    if (liveWires.empty()) {
        return std::nullopt;
    }
    if (liveWires.size() == 1U) {
        return liveWires.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Wire& wire : liveWires) {
        builder.Add(compound, wire);
    }
    return compound;
}

std::optional<TopoDS_Shape> copiedResultWireGraphForSketchInternals(const TopoDS_Shape& boundedFaceShape,
                                                                    const std::vector<TopoDS_Edge>& openEdges,
                                                                    const std::vector<TopoDS_Wire>& closedWires,
                                                                    std::size_t closedWireCount,
                                                                    std::size_t boundedFaceCount,
                                                                    bool splitProducedBoundedFaces,
                                                                    bool hasOpenWireOutput)
{
    if (boundedFaceShape.IsNull()) {
        return std::nullopt;
    }

    const bool consumedOpenCutterGraph = splitProducedBoundedFaces && !hasOpenWireOutput && openEdges.size() >= 2U
        && allOpenEdgeEndpointsTouchBoundary(openEdges, boundedFaceShape);
    const bool closedWireCycleGraph =
        openEdges.empty() && closedWireCount >= 3U && boundedFaceCount > closedWireCount;
    if (openEdges.empty() && closedWireCount >= 2U) {
        if (const auto partialSharedEdges = copiedPartialSharedClosedWireEdges(boundedFaceShape, closedWires)) {
            return partialSharedEdges;
        }
    }
    if (!consumedOpenCutterGraph && !closedWireCycleGraph) {
        return std::nullopt;
    }

    if (consumedOpenCutterGraph && !allOpenEdgeEndpointsTouchClosedWireBoundary(openEdges, closedWires)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports only final EdgeInfo states with no tight-bound WireInfo.
        // In T-junction cutter networks, through-cutter fragments are owned by tight-bound faces
        // while the branch cutter and original closed boundary remain result-wire evidence.
        return copiedPartialJunctionResultWireGraph(boundedFaceShape, closedWires, openEdges);
    }

    return copiedResultWireGraph(boundedFaceShape, openEdges, openEdges.empty());
}

}  // namespace cad_core::geometry
