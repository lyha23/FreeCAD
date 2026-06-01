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

bool closedWireCycleNeedsCopiedResultGraph(const TopoDS_Shape& boundedFaceShape,
                                           const std::vector<TopoDS_Wire>& closedWires)
{
    if (closedWires.size() < 3U || boundedFaceShape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> closedBoundaryEdges = closedWireBoundaryEdges(closedWires);
    const std::vector<TopoDS_Edge> resultEdges = uniqueEdgesForShape(boundedFaceShape);
    if (closedBoundaryEdges.empty() || resultEdges.empty()) {
        return false;
    }

    std::size_t splitSourceEdges = 0;
    for (const TopoDS_Edge& source : closedBoundaryEdges) {
        bool exactResultEdge = false;
        std::size_t splitFragments = 0;
        for (const TopoDS_Edge& resultEdge : resultEdges) {
            if (edgeEquivalentByGeometryAndEndpoints(resultEdge, source)) {
                exactResultEdge = true;
                break;
            }
            if (edgeSamplesLieOnEdge(resultEdge, source)) {
                ++splitFragments;
            }
        }
        if (!exactResultEdge && splitFragments >= 2U) {
            ++splitSourceEdges;
        }
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), seeds EdgeInfo from sourceEdgeArray, calls splitEdges() before
    // buildClosedWire(), and exports openWireCompound from final EdgeInfo states not owned by a
    // tight-bound WireInfo. Until the full EdgeInfo/WireInfo ledger is migrated, this predicate
    // uses result-edge ownership evidence: a closed-source cycle needs copied result wires when
    // source edges have been replaced by multiple bounded-result fragments instead of one exact edge.
    return splitSourceEdges >= 3U;
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
        info.id = nextWireInfoId_++;
        info.wire = wire;
        for (const TopoDS_Edge& edge : wireEdges(wire)) {
            EdgeInfo edgeInfo;
            edgeInfo.edge = edge;
            info.edges.push_back(edgeInfo);
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

void WireJoiner::rebuildOrderedVertices(WireInfo& info)
{
    info.orderedVertices.clear();
    info.hasNewWireSeed = false;
    info.hasSplitWireCandidate = false;
    info.done = false;
    info.splitWireCandidateCount = 0;
    info.ownerPropagationCandidateCount = 0;
    for (EdgeInfo& edge : info.edges) {
        edge.branchCandidateCount = 0;
        edge.branchInsideCandidateCount = 0;
        edge.branchOutsideCandidateCount = 0;
        edge.newWireSeedCandidateCount = 0;
        edge.splitWireCandidateCount = 0;
        edge.ownerPropagationCandidateCount = 0;
        edge.exhaustSeed = false;
        edge.exhaustSharedOwner = false;
        edge.exhaustDoneSecondary = false;
        edge.exhaustSearchCandidate = false;
    }
    std::vector<bool> used(info.edges.size(), false);

    for (std::size_t startIndex = 0; startIndex < info.edges.size(); ++startIndex) {
        if (used[startIndex] || info.edges[startIndex].edge.IsNull()) {
            continue;
        }

        std::deque<WireVertex> component;
        component.push_back(WireVertex{startIndex, true});
        used[startIndex] = true;
        auto [currentStart, currentEnd] = edgeEndpoints(info.edges[startIndex].edge);

        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t index = 0; index < info.edges.size(); ++index) {
                if (used[index] || info.edges[index].edge.IsNull()) {
                    continue;
                }
                const auto [edgeStart, edgeEnd] = edgeEndpoints(info.edges[index].edge);
                if (samePoint(edgeStart, currentEnd)) {
                    component.push_back(WireVertex{index, true});
                    currentEnd = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentEnd)) {
                    component.push_back(WireVertex{index, false});
                    currentEnd = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentStart)) {
                    component.push_front(WireVertex{index, true});
                    currentStart = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeStart, currentStart)) {
                    component.push_front(WireVertex{index, false});
                    currentStart = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
            }
        }

        info.orderedVertices.insert(info.orderedVertices.end(), component.begin(), component.end());
    }

    const int iteration2 = nextIteration2_++;
    for (const WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex < info.edges.size()) {
            info.edges[vertex.edgeIndex].iteration2 = iteration2;
        }
    }
}

void WireJoiner::recordExhaustTightBoundLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBound() first visits edges whose "wireInfo->done" is true,
    // copies a completed primary owner into "wireInfo2" for vertices owned by a different
    // WireInfo, skips edges where "wireInfo2 && wireInfo2->done", and otherwise calls
    // exhaustTightBoundUpdateWire() to search for the second tight-bound owner. This keeps
    // those request-local phases explicit before cad-core replaces the bounded classifier.
    if (!info.done) {
        return;
    }

    for (EdgeInfo& edge : info.edges) {
        if (edge.wireInfo == 0U) {
            continue;
        }
        edge.exhaustSeed = true;
        if (edge.wireInfo2 != 0U) {
            edge.exhaustSharedOwner = true;
            edge.exhaustDoneSecondary = true;
        }
        else {
            edge.exhaustSearchCandidate = true;
        }
    }
}

void WireJoiner::recordBranchSearchCandidates(WireInfo& info, const std::vector<TopoDS_Face>& boundedFaces)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices() walks "current->iStart[idx]..iEnd[idx]" and
    // skips "next == current || next->iteration2 == iteration2 || next->iteration < 0" before
    // testing "isInside(*wireInfo, next->mid)". This records the same request-local adjacent
    // EdgeInfo candidates and the inside candidates that would seed the "new WireInfo" branch
    // without yet replacing the bounded-face ownership classifier with the full branch search.
    for (WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& current = info.edges[vertex.edgeIndex];
        const gp_Pnt point = vertex.start ? edgeEndpoints(current.edge).first : edgeEndpoints(current.edge).second;
        std::size_t candidates = 0;
        std::size_t insideCandidates = 0;
        std::size_t outsideCandidates = 0;
        std::size_t newWireSeeds = 0;
        for (std::size_t index = 0; index < info.edges.size(); ++index) {
            if (index == vertex.edgeIndex || info.edges[index].iteration < 0) {
                continue;
            }
            const auto [edgeStart, edgeEnd] = edgeEndpoints(info.edges[index].edge);
            if (samePoint(point, edgeStart) || samePoint(point, edgeEnd)) {
                ++candidates;
                if (pointInsideOrOnAnyFace(edgeMidpoint(info.edges[index].edge), boundedFaces)) {
                    ++insideCandidates;
                    ++newWireSeeds;
                }
                else {
                    ++outsideCandidates;
                }
            }
        }
        vertex.branchCandidateCount = candidates;
        current.branchCandidateCount += candidates;
        current.branchInsideCandidateCount += insideCandidates;
        current.branchOutsideCandidateCount += outsideCandidates;
        current.newWireSeedCandidateCount += newWireSeeds;
        if (newWireSeeds > 0U) {
            info.hasNewWireSeed = true;
        }
    }
}

void WireJoiner::recordTightBoundLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundSplitWire() creates "splitWire.reset(new WireInfo())"
    // when the branch search slices an existing WireInfo, and ::findTightBoundUpdateVertices()
    // marks "beginInfo.wireInfo->done = true" before propagating that owner to vertices whose
    // EdgeInfo still points at another unfinished WireInfo. This records the equivalent
    // request-local lifecycle boundary without using it as an output pruning rule yet.
    bool hasOwnedEdge = false;
    bool hasOpenExportEdge = false;
    std::size_t insideBranchCandidates = 0;
    std::size_t outsideBranchCandidates = 0;
    for (const EdgeInfo& edge : info.edges) {
        hasOwnedEdge = hasOwnedEdge || edge.wireInfo != 0U;
        hasOpenExportEdge = hasOpenExportEdge || edge.iteration == -3 || (edge.wireInfo == 0U && edge.iteration >= 0);
        insideBranchCandidates += edge.branchInsideCandidateCount;
        outsideBranchCandidates += edge.branchOutsideCandidateCount;
    }

    if (insideBranchCandidates > 0U && (outsideBranchCandidates > 0U || hasOpenExportEdge)) {
        info.hasSplitWireCandidate = true;
        info.splitWireCandidateCount = 1;
        for (EdgeInfo& edge : info.edges) {
            if (edge.branchInsideCandidateCount > 0U) {
                edge.splitWireCandidateCount += edge.branchInsideCandidateCount;
            }
        }
    }

    if (!info.orderedVertices.empty() && (hasOwnedEdge || insideBranchCandidates > 0U)) {
        info.done = true;
    }

    if (info.done && hasOwnedEdge) {
        for (EdgeInfo& edge : info.edges) {
            if (edge.wireInfo == 0U) {
                ++info.ownerPropagationCandidateCount;
                ++edge.ownerPropagationCandidateCount;
            }
        }
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
    const std::size_t tightBoundOwner = nextWireInfoId_++;
    const std::size_t sharedTightBoundOwner = nextWireInfoId_++;

    for (WireInfo& info : openWires_) {
        std::vector<EdgeInfo> splitEdges;
        for (const EdgeInfo& edgeInfo : info.edges) {
            const auto split = splitOpenEdgeByFaceBoundaries(edgeInfo.edge, faceBoundaryEdges);
            const bool splitFromInputEdge = edgeInfo.splitFromInputEdge || split.size() != 1U
                || (split.size() == 1U && !edgeMatchesSourceVertices(split.front(), edgeInfo.edge));
            for (const TopoDS_Edge& edge : split) {
                EdgeInfo splitInfo;
                splitInfo.edge = edge;
                splitInfo.superEdge = edgeInfo.superEdge;
                splitInfo.iteration = edgeInfo.iteration;
                splitInfo.iteration2 = edgeInfo.iteration2;
                splitInfo.wireInfo = edgeInfo.wireInfo;
                splitInfo.wireInfo2 = edgeInfo.wireInfo2;
                splitInfo.splitFromInputEdge = splitFromInputEdge;
                splitEdges.push_back(splitInfo);
            }
        }
        info.edges = std::move(splitEdges);
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::findTightBound() iterates ordered "wireInfo->vertices" and marks
        // "vertex.it->iteration2 = iteration2" before branch search. cad-core now preserves the
        // request-local ordered EdgeInfo vertex ledger after splitter replacement; the full branch
        // search still remains a later migration step.
        rebuildOrderedVertices(info);
        recordBranchSearchCandidates(info, boundedFaces);
    }

    for (WireInfo& info : openWires_) {
        for (EdgeInfo& edgeInfo : info.edges) {
            if (edgeMatchesAnyBoundary(edgeInfo.edge, faceBoundaryEdges)) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
                // ::WireJoinerP::findTightBound() assigns the primary tight-bound WireInfo and
                // ::WireJoinerP::exhaustTightBound() may assign "wireInfo2" when one EdgeInfo is
                // shared by two tight-bound wires. cad-core does not yet build ordered WireInfo
                // vertices, but this keeps the same owner slots instead of a separate consumed
                // flag, so later migration can replace the classifier with the real search.
                edgeInfo.wireInfo = tightBoundOwner;
                if (countEquivalentEdges(edgeInfo.edge, faceBoundaryEdges) > 1) {
                    edgeInfo.wireInfo2 = sharedTightBoundOwner;
                }
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
            // ::WireJoinerP::splitEdges() and ::WireJoinerP::build() export copied/split open-wire
            // EdgeInfo states. This bounded-face subset keeps only boundary-touch fragments whose
            // midpoint is outside the bounded face and that came from a split/copy result edge;
            // unsplit original dangling edges still go through getOpenWires(noOriginal=true)
            // purging. Delete this marker when full EdgeInfo/WireInfo identity history is migrated.
            if (edgeInfo.splitFromInputEdge
                && edgeTouchesBoundary(edgeInfo.edge, faceBoundaryEdges)
                && !pointInsideOrOnAnyFace(edgeMidpoint(edgeInfo.edge), boundedFaces)) {
                edgeInfo.iteration = -3;
            }
        }
        recordTightBoundLifecycle(info);
        recordExhaustTightBoundLifecycle(info);
    }
}

WireJoinerLedgerSummary WireJoiner::ledgerSummary() const
{
    WireJoinerLedgerSummary summary;
    for (const WireInfo& info : openWires_) {
        for (const EdgeInfo& edgeInfo : info.edges) {
            ++summary.edgeInfoCount;
            if (edgeInfo.splitFromInputEdge) {
                ++summary.splitEdgeInfoCount;
            }
            if (edgeInfo.iteration2 != 0) {
                ++summary.iteration2MarkedEdgeInfoCount;
            }
            summary.branchSearchCandidateCount += edgeInfo.branchCandidateCount;
            summary.branchSearchInsideCandidateCount += edgeInfo.branchInsideCandidateCount;
            summary.branchSearchOutsideCandidateCount += edgeInfo.branchOutsideCandidateCount;
            summary.newWireSeedCandidateCount += edgeInfo.newWireSeedCandidateCount;
            summary.splitWireEdgeInfoCount += edgeInfo.splitWireCandidateCount;
            if (edgeInfo.exhaustSeed) {
                ++summary.exhaustSeedEdgeInfoCount;
            }
            if (edgeInfo.exhaustSharedOwner) {
                ++summary.exhaustSharedOwnerEdgeInfoCount;
            }
            if (edgeInfo.exhaustDoneSecondary) {
                ++summary.exhaustDoneSecondaryEdgeInfoCount;
            }
            if (edgeInfo.exhaustSearchCandidate) {
                ++summary.exhaustSearchCandidateEdgeInfoCount;
            }
            if (edgeInfo.wireInfo != 0U) {
                ++summary.primaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.wireInfo2 != 0U) {
                ++summary.secondaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.iteration == -3 || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0)) {
                ++summary.openExportEdgeInfoCount;
            }
        }
        if (!info.orderedVertices.empty()) {
            ++summary.orderedWireInfoCount;
            summary.orderedVertexCount += info.orderedVertices.size();
            if (std::any_of(info.orderedVertices.begin(),
                            info.orderedVertices.end(),
                            [](const WireVertex& vertex) {
                                return vertex.branchCandidateCount > 0U;
                            })) {
                ++summary.branchSearchSeedWireInfoCount;
            }
            if (info.hasNewWireSeed) {
                ++summary.newWireSeedWireInfoCount;
            }
            if (info.hasSplitWireCandidate) {
                summary.splitWireCandidateCount += info.splitWireCandidateCount;
            }
            if (info.done) {
                ++summary.doneWireInfoCount;
            }
            summary.doneOwnedEdgeInfoCount += std::count_if(info.edges.begin(),
                                                            info.edges.end(),
                                                            [](const EdgeInfo& edgeInfo) {
                                                                return edgeInfo.wireInfo != 0U;
                                                            });
        }
        summary.ownerPropagationCandidateCount += info.ownerPropagationCandidateCount;
    }
    return summary;
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
        for (const EdgeInfo& edgeInfo : info.edges) {
            const bool exportsOpenEdge = edgeInfo.iteration == -3
                || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
            if (!exportsOpenEdge) {
                continue;
            }
            liveEdges.push_back(edgeInfo.edge);
            if (edgeInfo.iteration == -3) {
                retainedByBoundaryTouchEdges.push_back(edgeInfo.edge);
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
                                                                    bool splitProducedBoundedFaces,
                                                                    bool hasOpenWireOutput)
{
    if (boundedFaceShape.IsNull()) {
        return std::nullopt;
    }

    const bool consumedOpenCutterGraph = splitProducedBoundedFaces && !hasOpenWireOutput
        && openEdges.size() >= 2U && allOpenEdgeEndpointsTouchBoundary(openEdges, boundedFaceShape);
    const bool closedWireCycleGraph =
        openEdges.empty() && closedWireCycleNeedsCopiedResultGraph(boundedFaceShape, closedWires);
    if (openEdges.empty() && closedWires.size() >= 2U) {
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
