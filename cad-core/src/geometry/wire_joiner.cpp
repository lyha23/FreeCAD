#include "cad_core/geometry/wire_joiner.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepAdaptor_Curve.hxx>
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
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <deque>
#include <functional>
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

struct SplitEdgeRecord {
    TopoDS_Edge edge;
    std::vector<std::size_t> sourceEdgeIndices;
    bool fromSplitterHistory = false;
};

struct SplitEdgesResult {
    std::vector<SplitEdgeRecord> records;
    WireJoinerHistorySummary history;
};

void appendUniqueSourceIndex(std::vector<std::size_t>& indices, std::size_t sourceIndex)
{
    if (std::find(indices.begin(), indices.end(), sourceIndex) == indices.end()) {
        indices.push_back(sourceIndex);
    }
}

void appendUniqueSourceIndices(std::vector<std::size_t>& target, const std::vector<std::size_t>& source)
{
    for (const std::size_t index : source) {
        appendUniqueSourceIndex(target, index);
    }
}

std::vector<std::size_t> sourceEdgeIndicesByIdentity(const TopoDS_Edge& edge,
                                                     const std::vector<TopoDS_Edge>& sourceEdges)
{
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < sourceEdges.size(); ++index) {
        if (!edge.IsNull() && !sourceEdges[index].IsNull() && edge.IsSame(sourceEdges[index])) {
            indices.push_back(index);
        }
    }
    return indices;
}

std::vector<TopoDS_Edge> shapeEdgesForLineage(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    if (shape.IsNull()) {
        return edges;
    }
    if (shape.ShapeType() == TopAbs_EDGE) {
        edges.push_back(TopoDS::Edge(shape));
        return edges;
    }
    return boundaryEdges(shape);
}

void appendLineageForHistoryShape(std::vector<SplitEdgeRecord>& records,
                                  const TopoDS_Shape& historyShape,
                                  const std::vector<std::size_t>& sourceIndices)
{
    if (sourceIndices.empty()) {
        return;
    }
    for (const TopoDS_Edge& historyEdge : shapeEdgesForLineage(historyShape)) {
        for (SplitEdgeRecord& record : records) {
            if (!record.edge.IsNull() && record.edge.IsSame(historyEdge)) {
                appendUniqueSourceIndices(record.sourceEdgeIndices, sourceIndices);
                record.fromSplitterHistory = true;
            }
        }
    }
}

void appendInputEdgeRecord(SplitEdgesResult& result,
                           const TopoDS_Edge& edge,
                           const std::vector<TopoDS_Edge>& sourceEdges)
{
    SplitEdgeRecord record;
    record.edge = edge;
    record.sourceEdgeIndices = sourceEdgeIndicesByIdentity(edge, sourceEdges);
    result.records.push_back(std::move(record));
}

SplitEdgesResult splitEdgesAtIntersections(const std::vector<TopoDS_Edge>& edges,
                                           const std::vector<TopoDS_Edge>& sourceEdges)
{
    SplitEdgesResult result;
    const std::vector<TopoDS_Edge>& lineageSources = sourceEdges.empty() ? edges : sourceEdges;
    result.history.sourceEdgeCount = lineageSources.size();
    if (edges.size() <= 1U) {
        for (const TopoDS_Edge& edge : edges) {
            appendInputEdgeRecord(result, edge, lineageSources);
        }
        return result;
    }

    TopTools_ListOfShape arguments;
    for (const TopoDS_Edge& edge : edges) {
        if (!edge.IsNull()) {
            arguments.Append(edge);
        }
    }
    if (arguments.Size() <= 1) {
        for (const TopoDS_Edge& edge : edges) {
            appendInputEdgeRecord(result, edge, lineageSources);
        }
        return result;
    }

    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(arguments);
    splitter.SetToFillHistory(Standard_True);
    splitter.SetRunParallel(Standard_True);
    splitter.SetNonDestructive(Standard_True);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        for (const TopoDS_Edge& edge : edges) {
            appendInputEdgeRecord(result, edge, lineageSources);
        }
        return result;
    }

    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_EDGE); explorer.More(); explorer.Next()) {
        SplitEdgeRecord record;
        record.edge = TopoDS::Edge(explorer.Current());
        result.records.push_back(std::move(record));
    }

    for (const TopoDS_Edge& edge : edges) {
        if (edge.IsNull()) {
            continue;
        }
        const TopTools_ListOfShape& modified = splitter.Modified(edge);
        if (!modified.IsEmpty()) {
            ++result.history.modifiedSourceEdgeCount;
            const std::vector<std::size_t> sourceIndices = sourceEdgeIndicesByIdentity(edge, lineageSources);
            for (TopTools_ListIteratorOfListOfShape it(modified); it.More(); it.Next()) {
                ++result.history.modifiedHistoryCount;
                appendLineageForHistoryShape(result.records, it.Value(), sourceIndices);
            }
        }
        const TopTools_ListOfShape& generated = splitter.Generated(edge);
        const std::vector<std::size_t> sourceIndices = sourceEdgeIndicesByIdentity(edge, lineageSources);
        for (TopTools_ListIteratorOfListOfShape it(generated); it.More(); it.Next()) {
            ++result.history.generatedHistoryCount;
            appendLineageForHistoryShape(result.records, it.Value(), sourceIndices);
        }
        if (splitter.IsDeleted(edge)) {
            ++result.history.deletedHistoryCount;
        }
    }

    if (result.records.empty()) {
        for (const TopoDS_Edge& edge : edges) {
            appendInputEdgeRecord(result, edge, lineageSources);
        }
    }
    for (SplitEdgeRecord& record : result.records) {
        if (record.sourceEdgeIndices.empty()) {
            record.sourceEdgeIndices = sourceEdgeIndicesByIdentity(record.edge, lineageSources);
        }
    }
    result.history.splitResultEdgeCount = result.records.size();
    result.history.splitterHistory = result.history.modifiedHistoryCount > 0U
        || result.history.generatedHistoryCount > 0U
        || result.history.deletedHistoryCount > 0U;
    return result;
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

bool vertexIsOriginalSourceByIdentity(const TopoDS_Vertex& vertex,
                                      const std::vector<TopoDS_Edge>& sourceEdges)
{
    if (vertex.IsNull()) {
        return false;
    }
    for (const TopoDS_Edge& sourceEdge : sourceEdges) {
        for (const TopoDS_Vertex& sourceVertex : edgeVertices(sourceEdge)) {
            if (vertex.IsSame(sourceVertex)) {
                return true;
            }
        }
    }
    return false;
}

bool edgeSharesOriginalSourceVertexByIdentity(const TopoDS_Edge& edge,
                                              const std::vector<TopoDS_Edge>& sourceEdges)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }

    return std::any_of(vertices.begin(), vertices.end(), [&](const TopoDS_Vertex& vertex) {
        return vertexIsOriginalSourceByIdentity(vertex, sourceEdges);
    });
}

bool edgeUsesOnlyOriginalSourceVerticesByIdentity(const TopoDS_Edge& edge,
                                                  const std::vector<TopoDS_Edge>& sourceEdges)
{
    const std::vector<TopoDS_Vertex> vertices = edgeVertices(edge);
    if (vertices.empty()) {
        return false;
    }

    return std::all_of(vertices.begin(), vertices.end(), [&](const TopoDS_Vertex& vertex) {
        return vertexIsOriginalSourceByIdentity(vertex, sourceEdges);
    });
}

bool edgeMatchesAnySourceByEndpoints(const TopoDS_Edge& edge, const std::vector<TopoDS_Edge>& sourceEdges)
{
    for (const TopoDS_Edge& sourceEdge : sourceEdges) {
        if (edgeMatchesSourceVertices(edge, sourceEdge)) {
            return true;
        }
    }
    return false;
}

bool allEdgesShareOriginalSourceVertexByIdentity(const TopoDS_Wire& wire,
                                                const std::vector<TopoDS_Edge>& sourceEdges)
{
    if (sourceEdges.empty()) {
        return false;
    }
    for (const TopoDS_Edge& edge : wireEdges(wire)) {
        if (!edgeUsesOnlyOriginalSourceVerticesByIdentity(edge, sourceEdges)) {
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

bool pointMatchesAny(const gp_Pnt& point, const std::vector<gp_Pnt>& candidates)
{
    return std::any_of(candidates.begin(), candidates.end(), [&](const gp_Pnt& candidate) {
        return samePoint(point, candidate);
    });
}

std::vector<gp_Pnt> wireVertexPoints(const std::vector<TopoDS_Wire>& wires)
{
    std::vector<gp_Pnt> points;
    for (const TopoDS_Wire& wire : wires) {
        for (TopExp_Explorer explorer(wire, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
            const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
            if (!pointMatchesAny(point, points)) {
                points.push_back(point);
            }
        }
    }
    return points;
}

bool closedWireEdgesAreLinear(const std::vector<TopoDS_Wire>& wires)
{
    for (const TopoDS_Wire& wire : wires) {
        for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
            if (curve.GetType() != GeomAbs_Line) {
                return false;
            }
        }
    }
    return !wires.empty();
}

TopoDS_Vertex resultWireVertex(const TopoDS_Vertex& sourceVertex,
                               const std::vector<TopoDS_Edge>& openEdges,
                               bool copyAllVertices,
                               const std::vector<gp_Pnt>& reusableVertexPoints,
                               std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices)
{
    const gp_Pnt point = BRep_Tool::Pnt(sourceVertex);
    if (pointMatchesAny(point, reusableVertexPoints)) {
        return sourceVertex;
    }
    if (copyAllVertices || pointOnOpenEdge(point, openEdges)) {
        return cachedCopiedVertex(copiedVertices, point);
    }
    return sourceVertex;
}

TopoDS_Edge copyEdgeWithResultWireVertices(const TopoDS_Edge& edge,
                                           const std::vector<TopoDS_Edge>& openEdges,
                                           bool copyAllVertices,
                                           const std::vector<gp_Pnt>& reusableVertexPoints,
                                           std::vector<std::pair<gp_Pnt, TopoDS_Vertex>>& copiedVertices)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
    const TopoDS_Vertex start = resultWireVertex(TopExp::FirstVertex(edge),
                                                 openEdges,
                                                 copyAllVertices,
                                                 reusableVertexPoints,
                                                 copiedVertices);
    const TopoDS_Vertex end = resultWireVertex(TopExp::LastVertex(edge),
                                               openEdges,
                                               copyAllVertices,
                                               reusableVertexPoints,
                                               copiedVertices);
    if (!curve.IsNull()) {
        const Handle(Geom_Curve) copiedCurve = Handle(Geom_Curve)::DownCast(curve->Copy());
        BRepBuilderAPI_MakeEdge builder(copiedCurve.IsNull() ? curve : copiedCurve, start, end, first, last);
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

bool closedWireCycleNeedsGeneratedOpenExport(const TopoDS_Shape& boundedFaceShape,
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
    // tight-bound WireInfo. Until the full EdgeInfo/WireInfo split ledger is migrated, this
    // predicate identifies closed-source cycles whose source edges have been replaced by multiple
    // bounded-result fragments instead of one exact edge.
    return splitSourceEdges >= 3U;
}

std::optional<TopoDS_Shape> generatedOpenExportShape(const TopoDS_Shape& boundedFaceShape,
                                                     const std::vector<TopoDS_Edge>& openEdges,
                                                     bool copyAllVertices,
                                                     const std::vector<gp_Pnt>& reusableVertexPoints = {})
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
        builder.Add(compound,
                    copyEdgeWithResultWireVertices(edge,
                                                   openEdges,
                                                   copyAllVertices,
                                                   reusableVertexPoints,
                                                   copiedVertices));
    }
    return compound;
}

std::optional<TopoDS_Shape> partialSharedClosedWireOpenExportShape(const TopoDS_Shape& boundedFaceShape,
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
        // shared partial closed-wire edges in final open export. Full coincident source edges are
        // not duplicated; partial overlaps need generated export edges so Sketch InternalShape
        // keeps the same edge/vertex ledger as FreeCAD.
        builder.Add(compound, copyEdgeWithResultWireVertices(edge, {}, true, {}, copiedVertices));
        added = true;
    }
    if (!added) {
        return std::nullopt;
    }
    return compound;
}

std::optional<TopoDS_Shape> partialJunctionOpenExportShape(const TopoDS_Shape& boundedFaceShape,
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
        builder.Add(compound, copyEdgeWithResultWireVertices(edge, openEdges, false, {}, copiedVertices));
        added = true;
    }

    if (!added) {
        return std::nullopt;
    }
    return compound;
}

}  // namespace

std::optional<TopoDS_Shape> generatedOpenExportShapeForSketchInternals(
    const TopoDS_Shape& boundedFaceShape,
    const std::vector<TopoDS_Edge>& openEdges,
    const std::vector<TopoDS_Wire>& closedWires,
    bool splitProducedBoundedFaces,
    bool hasOpenWireOutput);

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
            initializeEdgeInfo(edgeInfo, edge);
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

void WireJoiner::buildFinalEdgeOwnership(const TopoDS_Shape* boundedFaceShape,
                                         const std::vector<TopoDS_Wire>* closedWires,
                                         const std::vector<TopoDS_Edge>* openEdges,
                                         bool splitProducedBoundedFaces)
{
    historySummary_ = WireJoinerHistorySummary{};
    std::vector<TopoDS_Edge> inputEdges;
    for (const WireInfo& info : openWires_) {
        for (const EdgeInfo& edgeInfo : info.edges) {
            if (!edgeInfo.edge.IsNull()) {
                inputEdges.push_back(edgeInfo.edge);
            }
        }
    }
    if (inputEdges.empty()) {
        return;
    }

    const SplitEdgesResult splitResult = splitEdgesAtIntersections(inputEdges, sourceEdges_);
    std::vector<TopoDS_Edge> splitEdges;
    splitEdges.reserve(splitResult.records.size());
    for (const SplitEdgeRecord& record : splitResult.records) {
        splitEdges.push_back(record.edge);
    }
    historySummary_ = splitResult.history;
    const std::vector<TopoDS_Face> boundedFaces = boundedFaceShape ? facesForShape(*boundedFaceShape)
                                                                   : std::vector<TopoDS_Face>{};
    const bool hasSplitBoundedRegions = boundedFaces.size() > 1U;
    const bool assignTightBoundOwners =
        splitProducedBoundedFaces || !openEdges || openEdges->empty();
    WireInfo finalInfo;
    finalInfo.id = nextWireInfoId_++;
    const std::vector<TopoDS_Wire> finalWires = wiresFromEdges(splitEdges);
    finalInfo.wire = finalWires.empty() ? TopoDS_Wire() : finalWires.front();
    finalInfo.edges.reserve(splitResult.records.size());
    for (std::size_t index = 0; index < splitResult.records.size(); ++index) {
        const SplitEdgeRecord& record = splitResult.records[index];
        EdgeInfo edgeInfo;
        initializeEdgeInfo(edgeInfo, record.edge);
        edgeInfo.sourceEdgeIndices = record.sourceEdgeIndices;
        edgeInfo.sourceLineageFromSplitterHistory = record.fromSplitterHistory;
        edgeInfo.splitFromInputEdge = !edgeMatchesAnySourceByEndpoints(record.edge, inputEdges);
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::getOpenWires(noOriginal=true) purges open wires still sharing source
        // vertices. This temporary identity bridge is limited to unsplit open edges until the full
        // WireJoinerP VertexInfo/sourceEdgeArray identity ledger is migrated.
        const bool originalOpenInsideBoundedRegion =
            !edgeInfo.splitFromInputEdge && pointInsideOrOnAnyFace(edgeInfo.mid, boundedFaces);
        edgeInfo.purgeAsOriginalOpenEdge =
            !edgeInfo.splitFromInputEdge && (!hasSplitBoundedRegions || originalOpenInsideBoundedRegion);
        finalInfo.edges.push_back(edgeInfo);
    }

    rebuildAdjacentList(finalInfo);
    recordSuperEdgeCandidates(finalInfo);
    markOpenLeafEdges(finalInfo);
    assignClosedWireOwners(finalInfo, assignTightBoundOwners);
    rebuildOrderedVertices(finalInfo);
    if (tightBound_ && !boundedFaces.empty()) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::buildClosedWire(), calls "findClosedWires(true)",
        // "findTightBound()" and "exhaustTightBound()" before "openWireCompound" export.
        // Run the request-local branch/transfer ledger on the split EdgeInfo list before generated
        // open-export edges are added to the final WireJoiner EdgeInfo ledger.
        recordBranchSearchCandidates(finalInfo, boundedFaces);
        recordTightBoundLifecycle(finalInfo);
    }
    else {
        finalInfo.done = std::any_of(finalInfo.edges.begin(), finalInfo.edges.end(), [](const EdgeInfo& edgeInfo) {
            return edgeInfo.wireInfo != 0U;
        });
    }
    if (finalInfo.done) {
        recordExhaustTightBoundLifecycle(finalInfo);
        recordBuildClosedWireRemovalLifecycle(finalInfo);
    }

    const bool hasOpenWireOutput = std::any_of(finalInfo.edges.begin(), finalInfo.edges.end(), [](const EdgeInfo& edgeInfo) {
        return edgeInfo.iteration == -3 || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
    });
    if (boundedFaceShape && closedWires && openEdges) {
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build(), stores result-wire topology in "openWireCompound" after
        // splitEdges()/buildClosedWire()/findTightBound()/exhaustTightBound(). cad-core records
        // these as generated final EdgeInfo export entries inside WireJoiner so getOpenWires()
        // still consumes the same EdgeInfo lifecycle.
        const std::optional<TopoDS_Shape> generatedOpenExportShape = generatedOpenExportShapeForSketchInternals(
            *boundedFaceShape,
            *openEdges,
            *closedWires,
            splitProducedBoundedFaces,
            hasOpenWireOutput);
        if (generatedOpenExportShape && !generatedOpenExportShape->IsNull()) {
            std::size_t generatedEdgeCount = 0;
            for (const TopoDS_Edge& edge : boundaryEdges(*generatedOpenExportShape)) {
                EdgeInfo edgeInfo;
                initializeEdgeInfo(edgeInfo, edge);
                edgeInfo.splitFromInputEdge = true;
                edgeInfo.generatedOpenExportEdge = true;
                finalInfo.edges.push_back(edgeInfo);
                ++generatedEdgeCount;
            }
            historySummary_.generatedHistoryCount += generatedEdgeCount;
        }
    }

    rebuildAdjacentList(finalInfo);
    recordOpenWireCompoundLedger(finalInfo);
    historySummary_.openExportEdgeCount = std::count_if(
        finalInfo.edges.begin(),
        finalInfo.edges.end(),
        [](const EdgeInfo& edgeInfo) {
            return edgeInfo.iteration == -3 || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
        });
    std::size_t openExportIndex = 0;
    for (std::size_t edgeInfoIndex = 0; edgeInfoIndex < finalInfo.edges.size(); ++edgeInfoIndex) {
        const EdgeInfo& edgeInfo = finalInfo.edges[edgeInfoIndex];
        const bool exportsOpenEdge = edgeInfo.iteration == -3
            || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
        if (!exportsOpenEdge) {
            continue;
        }
        ++openExportIndex;
        WireJoinerOpenExportHistoryEntry entry;
        entry.openExportIndex = openExportIndex;
        entry.edgeInfoIndex = edgeInfoIndex;
        entry.sourceEdgeIndices = edgeInfo.sourceEdgeIndices;
        entry.sourceLineageFromSplitterHistory = edgeInfo.sourceLineageFromSplitterHistory;
        entry.generatedOpenExport = edgeInfo.generatedOpenExportEdge;
        entry.purgeBridge = edgeInfo.purgeAsOriginalOpenEdge;
        historySummary_.openExportEntries.push_back(std::move(entry));
        if (edgeInfo.sourceEdgeIndices.empty()) {
            ++historySummary_.openExportMissingSourceLineageEdgeCount;
        }
        else {
            ++historySummary_.openExportSourceLineageEdgeCount;
        }
        if (edgeInfo.generatedOpenExportEdge) {
            ++historySummary_.openExportGeneratedEdgeCount;
            if (edgeInfo.sourceEdgeIndices.empty()) {
                ++historySummary_.openExportGeneratedMissingSourceLineageEdgeCount;
            }
        }
        if (edgeInfo.purgeAsOriginalOpenEdge) {
            ++historySummary_.openExportPurgeBridgeEdgeCount;
        }
    }
    historySummary_.finalExportHistory = historySummary_.openExportEdgeCount > 0U;
    openWires_.clear();
    openWires_.push_back(std::move(finalInfo));
}

void WireJoiner::initializeEdgeInfo(EdgeInfo& edgeInfo, const TopoDS_Edge& edge) const
{
    edgeInfo.edge = edge;
    edgeInfo.edgeReversed.Nullify();
    edgeInfo.superEdgeReversed.Nullify();
    edgeInfo.iStart = {-1, -1};
    edgeInfo.iEnd = {-1, -1};
    if (edge.IsNull()) {
        return;
    }
    const auto [start, end] = edgeEndpoints(edge);
    edgeInfo.p1 = start;
    edgeInfo.p2 = end;
    edgeInfo.mid = edgeMidpoint(edge);
    edgeInfo.sourceVertexIdentity = {
        vertexIsOriginalSourceByIdentity(TopExp::FirstVertex(edge), sourceEdges_),
        vertexIsOriginalSourceByIdentity(TopExp::LastVertex(edge), sourceEdges_),
    };
}

const TopoDS_Shape& WireJoiner::EdgeInfo::shape(bool forward) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo::shape(), "if (superEdge.IsNull())" returns edge/edgeReversed;
    // otherwise it returns superEdge/superEdgeReversed. This is the common entry used by
    // findSuperEdgesUpdateFirst(), bounds, debug output and openWireCompound construction.
    if (superEdge.IsNull()) {
        if (forward) {
            return edge;
        }
        if (edgeReversed.IsNull()) {
            edgeReversed = edge.Reversed();
        }
        return edgeReversed;
    }
    if (forward) {
        return superEdge;
    }
    if (superEdgeReversed.IsNull()) {
        superEdgeReversed = superEdge.Reversed();
    }
    return superEdgeReversed;
}

TopoDS_Wire WireJoiner::EdgeInfo::wire(bool forward) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::EdgeInfo::wire(), "auto sForWire = shape(); if ... WIRE ... else
    // BRepBuilderAPI_MakeWire(TopoDS::Edge(sForWire)).Wire()". Keep this conversion local to
    // WireJoiner so future openWireCompound export can consume the same EdgeInfo shape contract.
    const TopoDS_Shape& shapeForWire = shape(forward);
    if (shapeForWire.IsNull()) {
        return TopoDS_Wire();
    }
    if (shapeForWire.ShapeType() == TopAbs_WIRE) {
        return TopoDS::Wire(shapeForWire);
    }
    if (shapeForWire.ShapeType() == TopAbs_EDGE) {
        return BRepBuilderAPI_MakeWire(TopoDS::Edge(shapeForWire)).Wire();
    }
    return TopoDS_Wire();
}

TopoDS_Wire WireJoiner::wireFromVertices(const WireInfo& info, const std::vector<WireVertex>& vertices) const
{
    BRepBuilderAPI_MakeWire builder;
    for (const WireVertex& vertex : vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        const TopoDS_Wire wire = info.edges[vertex.edgeIndex].wire(vertex.start);
        if (wire.IsNull()) {
            continue;
        }
        builder.Add(wire);
    }
    return builder.IsDone() ? builder.Wire() : TopoDS_Wire();
}

void WireJoiner::rebuildAdjacentList(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListPopulate(), fills EdgeInfo::iStart/iEnd ranges over the
    // request-local "adjacentList" so findTightBound()/exhaustTightBound() can walk connected
    // VertexInfo entries instead of scanning result geometry.
    info.adjacentVertices.clear();
    for (EdgeInfo& edge : info.edges) {
        edge.iStart = {-1, -1};
        edge.iEnd = {-1, -1};
    }

    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0) {
            continue;
        }
        const std::array<gp_Pnt, 2> endpoints{edge.p1, edge.p2};
        for (int endpointIndex = 0; endpointIndex < 2; ++endpointIndex) {
            if (edge.iStart[endpointIndex] >= 0) {
                continue;
            }
            const int rangeStart = static_cast<int>(info.adjacentVertices.size());
            for (std::size_t otherIndex = 0; otherIndex < info.edges.size(); ++otherIndex) {
                const EdgeInfo& other = info.edges[otherIndex];
                if (other.edge.IsNull() || other.iteration < 0) {
                    continue;
                }
                if (samePoint(other.p1, endpoints[endpointIndex])) {
                    info.adjacentVertices.push_back(WireVertex{otherIndex, true});
                }
                if (samePoint(other.p2, endpoints[endpointIndex])) {
                    info.adjacentVertices.push_back(WireVertex{otherIndex, false});
                }
            }
            const int rangeEnd = static_cast<int>(info.adjacentVertices.size());
            edge.iStart[endpointIndex] = rangeStart;
            edge.iEnd[endpointIndex] = rangeEnd;
            for (int adjacentIndex = rangeStart; adjacentIndex < rangeEnd; ++adjacentIndex) {
                const WireVertex& vertex = info.adjacentVertices[adjacentIndex];
                if (vertex.edgeIndex >= info.edges.size() || vertex.edgeIndex == edgeIndex) {
                    continue;
                }
                const int otherEndpointIndex = vertex.start ? 0 : 1;
                info.edges[vertex.edgeIndex].iStart[otherEndpointIndex] = rangeStart;
                info.edges[vertex.edgeIndex].iEnd[otherEndpointIndex] = rangeEnd;
            }
        }
    }
}

std::optional<WireJoiner::WireVertex> WireJoiner::soleActiveAdjacentEdge(const WireInfo& info,
                                                                         std::size_t edgeIndex,
                                                                         int endpointIndex) const
{
    if (edgeIndex >= info.edges.size() || endpointIndex < 0 || endpointIndex > 1) {
        return std::nullopt;
    }
    const EdgeInfo& edge = info.edges[edgeIndex];
    if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
        return std::nullopt;
    }

    std::optional<WireVertex> found;
    for (int adjacentIndex = edge.iStart[endpointIndex]; adjacentIndex < edge.iEnd[endpointIndex]; ++adjacentIndex) {
        if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
            continue;
        }
        const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
        if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
            continue;
        }
        const EdgeInfo& other = info.edges[adjacent.edgeIndex];
        if (other.edge.IsNull() || other.iteration < 0) {
            continue;
        }
        if (found) {
            return std::nullopt;
        }
        found = adjacent;
    }
    return found;
}

void WireJoiner::extendSuperEdgeCandidate(const WireInfo& info,
                                          std::deque<WireVertex>& vertices,
                                          std::vector<bool>& used,
                                          bool appendBack,
                                          bool& closed) const
{
    while (!closed && !vertices.empty()) {
        const WireVertex current = appendBack ? vertices.back() : vertices.front();
        if (current.edgeIndex >= info.edges.size()) {
            return;
        }

        const int endpointIndex = appendBack ? (current.start ? 1 : 0) : (current.start ? 0 : 1);
        const std::optional<WireVertex> adjacent =
            soleActiveAdjacentEdge(info, current.edgeIndex, endpointIndex);
        if (!adjacent) {
            return;
        }
        if (adjacent->edgeIndex >= used.size()) {
            return;
        }
        if (used[adjacent->edgeIndex]) {
            closed = true;
            return;
        }

        used[adjacent->edgeIndex] = true;
        if (appendBack) {
            vertices.push_back(WireVertex{adjacent->edgeIndex, adjacent->start});
        }
        else {
            vertices.push_front(WireVertex{adjacent->edgeIndex, !adjacent->start});
        }
    }
}

void WireJoiner::recordSuperEdgeCandidates(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListSkipEdges(), calls findSuperEdges() before the
    // "Skip edges that are connected to only one end" loop when merge/tight-bound is enabled.
    // This records equivalent request-local candidate chains but does not yet replace EdgeInfo::edge
    // with EdgeInfo::superEdge or change openWireCompound export.
    info.superEdges.clear();
    for (EdgeInfo& edge : info.edges) {
        edge.superEdgeInfo = 0;
        edge.superEdgeMemberCount = 0;
        edge.superEdgeRoot = false;
        edge.superEdgeClosed = false;
        edge.superEdgeMaterialized = false;
        edge.superEdgeShadowedMember = false;
        edge.superEdgeLifecycleIteration = 0;
        edge.superEdgeLifecycleMemberMinusOne = false;
        edge.superEdgeLifecycleOpenRoot = false;
        edge.superEdgeLifecycleClosedRoot = false;
        edge.superEdgeAdjacentRangeRewritten = false;
        edge.superEdgeEndpointRewritten = false;
        edge.superEdgeEndpointRewriteIndex = -1;
        edge.superEdgeEndpointRewritePoint = {};
        edge.superEdgeAdjacentRangeSourceEdgeInfo = 0;
        edge.superEdgeAdjacentRangeSourceEndpoint = -1;
        edge.superEdgeAdjacentRangeStart = -1;
        edge.superEdgeAdjacentRangeEnd = -1;
        edge.edgeReversed.Nullify();
        edge.superEdgeReversed.Nullify();
        edge.superEdge.Nullify();
    }

    std::vector<bool> assigned(info.edges.size(), false);
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        if (assigned[edgeIndex]) {
            continue;
        }
        const EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0) {
            continue;
        }

        std::deque<WireVertex> vertices;
        std::vector<bool> used(info.edges.size(), false);
        vertices.push_back(WireVertex{edgeIndex, true});
        used[edgeIndex] = true;
        bool closed = false;
        extendSuperEdgeCandidate(info, vertices, used, false, closed);
        extendSuperEdgeCandidate(info, vertices, used, true, closed);
        if (vertices.size() <= 1U) {
            continue;
        }

        SuperEdgeInfo superEdge;
        superEdge.id = nextSuperEdgeId_++;
        superEdge.vertices.assign(vertices.begin(), vertices.end());
        superEdge.wire = wireFromVertices(info, superEdge.vertices);
        superEdge.materialized = !superEdge.wire.IsNull();
        superEdge.closed = superEdge.materialized ? BRep_Tool::IsClosed(superEdge.wire) : closed;
        const std::size_t memberCount = superEdge.vertices.size();
        const WireVertex rootVertex = superEdge.vertices.front();
        const WireVertex lastVertex = superEdge.vertices.back();
        const int rootRewriteEndpoint = rootVertex.start ? 1 : 0;
        const int sourceRangeEndpoint = lastVertex.start ? 1 : 0;
        const gp_Pnt rootRewritePoint = vertexOtherPoint(info, lastVertex);
        bool rootAssigned = false;
        for (const WireVertex& vertex : superEdge.vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            assigned[vertex.edgeIndex] = true;
            EdgeInfo& member = info.edges[vertex.edgeIndex];
            member.superEdgeInfo = superEdge.id;
            member.superEdgeMemberCount = memberCount;
            member.superEdgeClosed = superEdge.closed;
            if (superEdge.materialized) {
                member.superEdgeLifecycleIteration = -1;
                member.superEdgeLifecycleMemberMinusOne = true;
                member.iteration = -1;
            }
            if (!rootAssigned) {
                member.superEdgeRoot = true;
                member.superEdge = superEdge.wire;
                member.superEdgeReversed.Nullify();
                member.superEdgeMaterialized = superEdge.materialized;
                if (superEdge.materialized) {
                    member.superEdgeLifecycleMemberMinusOne = false;
                    if (superEdge.closed) {
                        member.superEdgeLifecycleIteration = -2;
                        member.superEdgeLifecycleClosedRoot = true;
                        member.iteration = -2;
                    }
                    else {
                        member.superEdgeLifecycleIteration = 1;
                        member.superEdgeLifecycleOpenRoot = true;
                        member.superEdgeAdjacentRangeRewritten = true;
                        member.superEdgeEndpointRewritten = true;
                        member.superEdgeEndpointRewriteIndex = rootRewriteEndpoint;
                        member.superEdgeEndpointRewritePoint = rootRewritePoint;
                        if (lastVertex.edgeIndex < info.edges.size()) {
                            const EdgeInfo& source = info.edges[lastVertex.edgeIndex];
                            member.superEdgeAdjacentRangeSourceEdgeInfo = lastVertex.edgeIndex + 1U;
                            member.superEdgeAdjacentRangeSourceEndpoint = sourceRangeEndpoint;
                            member.superEdgeAdjacentRangeStart = source.iStart[sourceRangeEndpoint];
                            member.superEdgeAdjacentRangeEnd = source.iEnd[sourceRangeEndpoint];
                        }
                        member.iteration = member.superEdgeLifecycleIteration;
                        if (member.superEdgeEndpointRewriteIndex == 0) {
                            member.p1 = member.superEdgeEndpointRewritePoint;
                        }
                        else if (member.superEdgeEndpointRewriteIndex == 1) {
                            member.p2 = member.superEdgeEndpointRewritePoint;
                        }
                        if (member.superEdgeAdjacentRangeStart >= 0 && member.superEdgeAdjacentRangeEnd >= 0) {
                            member.iStart[rootRewriteEndpoint] = member.superEdgeAdjacentRangeStart;
                            member.iEnd[rootRewriteEndpoint] = member.superEdgeAdjacentRangeEnd;
                            for (int adjacentIndex = member.superEdgeAdjacentRangeStart;
                                 adjacentIndex < member.superEdgeAdjacentRangeEnd;
                                 ++adjacentIndex) {
                                if (adjacentIndex < 0
                                    || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                                    continue;
                                }
                                WireVertex& adjacent =
                                    info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                                if (adjacent.edgeIndex == lastVertex.edgeIndex) {
                                    adjacent.edgeIndex = rootVertex.edgeIndex;
                                    adjacent.start = !rootVertex.start;
                                }
                            }
                        }
                        const bool rootEndpointSourceIdentity = rootVertex.edgeIndex < info.edges.size()
                            && info.edges[rootVertex.edgeIndex].sourceVertexIdentity[rootVertex.start ? 0 : 1];
                        const bool rewrittenEndpointSourceIdentity = lastVertex.edgeIndex < info.edges.size()
                            && info.edges[lastVertex.edgeIndex].sourceVertexIdentity[lastVertex.start ? 1 : 0];
                        member.sourceVertexIdentity[rootVertex.start ? 0 : 1] = rootEndpointSourceIdentity;
                        member.sourceVertexIdentity[rootRewriteEndpoint] = rewrittenEndpointSourceIdentity;
                    }
                }
                rootAssigned = true;
            }
            else {
                member.superEdgeShadowedMember = superEdge.materialized;
            }
        }
        info.superEdges.push_back(std::move(superEdge));
    }
}

bool WireJoiner::markOpenLeafEdges(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildAdjacentListSkipEdges(), "Skip edges that are connected to only one end";
    // if no other active edge exists in an endpoint adjacent range, it writes "info.iteration = -3".
    // This runs before findClosedWires(true), so open leaf edges are exported by the same final
    // EdgeInfo condition used by ::WireJoinerP::build(): "iteration == -3 || (!wireInfo && ...)".
    bool changedAny = false;
    bool done = false;
    while (!done) {
        done = true;
        for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
            EdgeInfo& edge = info.edges[edgeIndex];
            if (edge.edge.IsNull() || edge.iteration < 0) {
                continue;
            }
            if (samePoint(edge.p1, edge.p2)) {
                continue;
            }
            for (int endpointIndex = 0; endpointIndex < 2; ++endpointIndex) {
                if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
                    continue;
                }
                bool hasActiveOther = false;
                for (int adjacentIndex = edge.iStart[endpointIndex];
                     adjacentIndex < edge.iEnd[endpointIndex];
                     ++adjacentIndex) {
                    if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                        continue;
                    }
                    const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                    if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
                        continue;
                    }
                    const EdgeInfo& other = info.edges[adjacent.edgeIndex];
                    if (!other.edge.IsNull() && other.iteration >= 0) {
                        hasActiveOther = true;
                        break;
                    }
                }
                if (!hasActiveOther) {
                    edge.iteration = -3;
                    done = false;
                    changedAny = true;
                    break;
                }
            }
        }
    }
    return changedAny;
}

void WireJoiner::rebuildOrderedVertices(WireInfo& info)
{
    info.orderedVertices.clear();
    info.adjacentVertices.clear();
    info.hasNewWireSeed = false;
    info.hasSplitWireCandidate = false;
    info.done = false;
    info.splitWireCandidateCount = 0;
    info.ownerPropagationCandidateCount = 0;
    for (OwnerWireInfo& owner : info.ownerWires) {
        owner.hasNewWireSeed = false;
        owner.hasSplitWireCandidate = false;
        owner.done = false;
        owner.splitWireCandidateCount = 0;
        owner.branchSearchCandidateCount = 0;
        owner.branchSearchInsideCandidateCount = 0;
        owner.branchSearchOutsideCandidateCount = 0;
        owner.branchCandidates.clear();
        owner.transferWires.clear();
        for (WireVertex& vertex : owner.vertices) {
            vertex.branchCandidateCount = 0;
        }
    }
    for (EdgeInfo& edge : info.edges) {
        edge.branchCandidateCount = 0;
        edge.branchInsideCandidateCount = 0;
        edge.branchOutsideCandidateCount = 0;
        edge.newWireSeedCandidateCount = 0;
        edge.splitWireCandidateCount = 0;
        edge.ownerPropagationCandidateCount = 0;
        edge.tightBoundOwnerTransferCandidate = false;
        edge.tightBoundTransferredOwner = false;
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

    rebuildAdjacentList(info);
    const int iteration2 = nextIteration2_++;
    for (const WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex < info.edges.size()) {
            info.edges[vertex.edgeIndex].iteration2 = iteration2;
        }
    }
}

std::optional<WireJoiner::ClosedWireSearchResult> WireJoiner::findClosedWirePath(
    const WireInfo& info,
    std::size_t beginEdgeIndex) const
{
    if (beginEdgeIndex >= info.edges.size()) {
        return std::nullopt;
    }
    const EdgeInfo& beginInfo = info.edges[beginEdgeIndex];
    if (beginInfo.edge.IsNull() || beginInfo.iteration < 0) {
        return std::nullopt;
    }

    ClosedWireSearchResult result;
    result.vertices.push_back(WireVertex{beginEdgeIndex, true});
    if (samePoint(beginInfo.p1, beginInfo.p2)) {
        return result;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWires(), pushes StackInfo frames, fills vertexStack from the
    // current EdgeInfo adjacent range, tracks edgeSet to avoid self-intersections, and then builds
    // the closed WireInfo from beginInfo plus vertexStack[entry.iCurrent] for each stack frame.
    std::vector<bool> edgeSet(info.edges.size(), false);
    std::vector<WireVertex> vertexStack;
    std::vector<ClosedWireSearchFrame> stack;
    edgeSet[beginEdgeIndex] = true;

    const gp_Pnt pstart = beginInfo.p1;
    gp_Pnt pend = beginInfo.p2;
    std::size_t currentEdgeIndex = beginEdgeIndex;
    int currentEndpointIndex = 1;
    const std::size_t stackEnd = stack.size();

    while (true) {
        if (currentEdgeIndex >= info.edges.size()) {
            return std::nullopt;
        }
        const EdgeInfo& current = info.edges[currentEdgeIndex];
        ClosedWireSearchFrame frame;
        frame.start = vertexStack.size();
        frame.current = frame.start;
        frame.end = frame.start;
        const std::size_t originalVertexStackSize = vertexStack.size();
        if (currentEndpointIndex >= 0 && currentEndpointIndex < 2
            && current.iStart[currentEndpointIndex] >= 0
            && current.iEnd[currentEndpointIndex] >= current.iStart[currentEndpointIndex]) {
            for (int adjacentIndex = current.iStart[currentEndpointIndex];
                 adjacentIndex < current.iEnd[currentEndpointIndex];
                 ++adjacentIndex) {
                if (adjacentIndex < 0
                    || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                    continue;
                }
                const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == currentEdgeIndex) {
                    continue;
                }
                const EdgeInfo& candidate = info.edges[adjacent.edgeIndex];
                if (candidate.edge.IsNull() || candidate.iteration < 0) {
                    continue;
                }
                if (edgeSet[adjacent.edgeIndex]) {
                    ++result.intersectSkipCount;
                    frame.end = frame.start;
                    vertexStack.resize(originalVertexStackSize);
                    break;
                }
                vertexStack.push_back(adjacent);
                ++frame.end;
                ++result.vertexStackCount;
            }
        }
        stack.push_back(frame);
        ++result.stackFrameCount;

        bool selected = false;
        while (!stack.empty()) {
            ClosedWireSearchFrame& currentFrame = stack.back();
            if (currentFrame.current < currentFrame.end) {
                const WireVertex& currentVertex = vertexStack[currentFrame.current];
                currentEdgeIndex = currentVertex.edgeIndex;
                pend = vertexOtherPoint(info, currentVertex);
                currentEndpointIndex = currentVertex.start ? 1 : 0;
                if (currentEdgeIndex < edgeSet.size() && !edgeSet[currentEdgeIndex]) {
                    edgeSet[currentEdgeIndex] = true;
                    ++result.edgeSetVisitCount;
                }
                selected = true;
                break;
            }

            vertexStack.erase(vertexStack.begin() + static_cast<long>(currentFrame.start), vertexStack.end());
            stack.pop_back();
            ++result.backtrackCount;
            if (stack.size() == stackEnd) {
                return std::nullopt;
            }

            ClosedWireSearchFrame& previousFrame = stack.back();
            if (previousFrame.current < vertexStack.size()) {
                const WireVertex& lastVertex = vertexStack[previousFrame.current];
                if (lastVertex.edgeIndex < edgeSet.size()) {
                    edgeSet[lastVertex.edgeIndex] = false;
                }
            }
            ++previousFrame.current;
        }
        if (!selected) {
            return std::nullopt;
        }
        if (!samePoint(pstart, pend)) {
            continue;
        }

        result.vertices.clear();
        result.vertices.push_back(WireVertex{beginEdgeIndex, true});
        for (const ClosedWireSearchFrame& selectedFrame : stack) {
            if (selectedFrame.current < vertexStack.size()) {
                result.vertices.push_back(vertexStack[selectedFrame.current]);
            }
        }
        return result;
    }
}

std::size_t WireJoiner::assignClosedWireOwners(WireInfo& info, bool assignOwners)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findClosedWires(true), for each unowned EdgeInfo, calls
    // "_findClosedWires(beginVertex, currentVertex)" and then assigns "beginInfo.wireInfo" to
    // every EdgeInfo reached through "stack". This is the first cad-core owner source before
    // findTightBound()/exhaustTightBound() split or add secondary owners.
    if (!assignOwners) {
        return 0;
    }

    std::size_t assigned = 0;
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        EdgeInfo& beginInfo = info.edges[edgeIndex];
        if (beginInfo.edge.IsNull() || beginInfo.iteration < 0 || beginInfo.wireInfo != 0U) {
            continue;
        }
        const std::optional<ClosedWireSearchResult> search = findClosedWirePath(info, edgeIndex);
        if (!search) {
            continue;
        }

        const std::size_t owner = nextWireInfoId_++;
        OwnerWireInfo ownerInfo;
        ownerInfo.id = owner;
        ownerInfo.vertices = search->vertices;
        ownerInfo.wire = wireFromVertices(info, ownerInfo.vertices);
        ownerInfo.closedWireSearchStackFrameCount = search->stackFrameCount;
        ownerInfo.closedWireSearchVertexStackCount = search->vertexStackCount;
        ownerInfo.closedWireSearchEdgeSetVisitCount = search->edgeSetVisitCount;
        ownerInfo.closedWireSearchBacktrackCount = search->backtrackCount;
        ownerInfo.closedWireSearchIntersectSkipCount = search->intersectSkipCount;
        for (const WireVertex& vertex : search->vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& edgeInfo = info.edges[vertex.edgeIndex];
            if (edgeInfo.iteration < 0 || edgeInfo.wireInfo != 0U) {
                continue;
            }
            edgeInfo.wireInfo = owner;
            edgeInfo.closedWireOwner = true;
            ++assigned;
        }
        info.ownerWires.push_back(std::move(ownerInfo));
    }
    return assigned;
}

gp_Pnt WireJoiner::vertexPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p1 : edge.p2;
}

gp_Pnt WireJoiner::vertexOtherPoint(const WireInfo& info, const WireVertex& vertex) const
{
    if (vertex.edgeIndex >= info.edges.size()) {
        return {};
    }
    const EdgeInfo& edge = info.edges[vertex.edgeIndex];
    return vertex.start ? edge.p2 : edge.p1;
}

std::optional<std::size_t> WireJoiner::ownerVertexIndex(const OwnerWireInfo& owner,
                                                        const WireVertex& vertex) const
{
    for (std::size_t index = 0; index < owner.vertices.size(); ++index) {
        const WireVertex& ownerVertex = owner.vertices[index];
        if (ownerVertex.edgeIndex == vertex.edgeIndex && ownerVertex.start == vertex.start) {
            return index;
        }
    }
    return std::nullopt;
}

WireJoiner::TightBoundExistingWireSearchTrace WireJoiner::traceExistingWireSearchForCandidate(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const TightBoundBranchCandidate& candidate) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::_findClosedWiresWithExisting(), called from
    // ::findTightBoundByVertices(), checks whether the adjacent-list search has reached an
    // existing "wireInfo" vertex, the reversed vertex, or an already visited edgeSet entry before
    // it decides idxVertex/stackPos or marks "wireInfo->purge = true".
    TightBoundExistingWireSearchTrace trace;
    if (!candidate.inside || !candidate.transfersOwnerEdge || candidate.adjacentVertex.edgeIndex >= info.edges.size()) {
        return trace;
    }

    std::vector<bool> edgeSet(info.edges.size(), false);
    edgeSet[candidate.adjacentVertex.edgeIndex] = true;
    ++trace.edgeSetVisitCount;

    std::function<bool(const WireVertex&, std::size_t)> visit = [&](const WireVertex& currentVertex,
                                                                    std::size_t depth) -> bool {
        if (currentVertex.edgeIndex >= info.edges.size() || depth > info.edges.size()) {
            return false;
        }
        const EdgeInfo& current = info.edges[currentVertex.edgeIndex];
        const int endpointIndex = currentVertex.start ? 1 : 0;
        if (endpointIndex < 0 || endpointIndex > 1 || current.iStart[endpointIndex] < 0
            || current.iEnd[endpointIndex] < current.iStart[endpointIndex]) {
            return false;
        }

        ++trace.stackFrameCount;
        for (int adjacentIndex = current.iStart[endpointIndex]; adjacentIndex < current.iEnd[endpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == currentVertex.edgeIndex) {
                continue;
            }
            const EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.edge.IsNull() || next.iteration < 0) {
                continue;
            }

            if (const std::optional<std::size_t> ownerIndex = ownerVertexIndex(owner, adjacent)) {
                if (*ownerIndex != 0U) {
                    trace.hit = true;
                    trace.idxVertex = static_cast<int>(*ownerIndex) - 1;
                    trace.stackPos = static_cast<int>(trace.stackFrameCount) - 1;
                    ++trace.vertexStackCount;
                    return true;
                }
            }

            WireVertex reversed = adjacent;
            reversed.start = !reversed.start;
            if (const std::optional<std::size_t> reverseIndex = ownerVertexIndex(owner, reversed)) {
                if (*reverseIndex != 0U) {
                    trace.reverseHit = true;
                    trace.purge = true;
                    ++trace.vertexStackCount;
                    return true;
                }
            }

            if (edgeSet[adjacent.edgeIndex]) {
                ++trace.intersectSkipCount;
                continue;
            }

            edgeSet[adjacent.edgeIndex] = true;
            ++trace.edgeSetVisitCount;
            ++trace.vertexStackCount;
            if (visit(adjacent, depth + 1U)) {
                return true;
            }
            edgeSet[adjacent.edgeIndex] = false;
            ++trace.backtrackCount;
        }
        return false;
    };

    visit(candidate.adjacentVertex, 0U);
    return trace;
}

bool WireJoiner::findTightBoundBranchPathToPoint(const WireInfo& info,
                                                 const OwnerWireInfo& owner,
                                                 const gp_Pnt& current,
                                                 const gp_Pnt& target,
                                                 std::vector<bool>& usedEdges,
                                                 std::vector<WireVertex>& path) const
{
    return findBranchPathToPointSkippingOwner(info, owner.id, current, target, usedEdges, path);
}

bool WireJoiner::findBranchPathToPointSkippingOwner(const WireInfo& info,
                                                    std::size_t skipOwnerId,
                                                    const gp_Pnt& current,
                                                    const gp_Pnt& target,
                                                    std::vector<bool>& usedEdges,
                                                    std::vector<WireVertex>& path) const
{
    if (samePoint(current, target)) {
        return true;
    }

    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        if (edgeIndex >= usedEdges.size() || usedEdges[edgeIndex]) {
            continue;
        }
        const EdgeInfo& edge = info.edges[edgeIndex];
        if (edge.edge.IsNull() || edge.iteration < 0 || edge.wireInfo == skipOwnerId) {
            continue;
        }

        std::optional<WireVertex> nextVertex;
        gp_Pnt nextPoint;
        if (samePoint(edge.p1, current)) {
            nextVertex = WireVertex{edgeIndex, true, 0U};
            nextPoint = edge.p2;
        }
        else if (samePoint(edge.p2, current)) {
            nextVertex = WireVertex{edgeIndex, false, 0U};
            nextPoint = edge.p1;
        }
        else {
            continue;
        }

        usedEdges[edgeIndex] = true;
        path.push_back(*nextVertex);
        if (findBranchPathToPointSkippingOwner(info, skipOwnerId, nextPoint, target, usedEdges, path)) {
            return true;
        }
        path.pop_back();
        usedEdges[edgeIndex] = false;
    }
    return false;
}

std::optional<WireJoiner::TightBoundTransferPath> WireJoiner::tightBoundTransferPathForCandidate(
    const WireInfo& info,
    const OwnerWireInfo& owner,
    const TightBoundBranchCandidate& candidate) const
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices(), calls
    // "_findClosedWires(beginVertex, currentVertex, &idxEnd, beginInfo.wireInfo, &stackPos)"
    // when the inside branch does not immediately return to "pstart"; if no closed branch path
    // is found, the candidate is discarded instead of becoming a new WireInfo.
    if (!candidate.inside || !candidate.transfersOwnerEdge || owner.vertices.empty()) {
        return std::nullopt;
    }

    const auto ownerVertexIt = std::find_if(owner.vertices.begin(),
                                            owner.vertices.end(),
                                            [&](const WireVertex& vertex) {
                                                return vertex.edgeIndex == candidate.ownerVertex.edgeIndex
                                                    && vertex.start == candidate.ownerVertex.start;
                                            });
    if (ownerVertexIt == owner.vertices.end()) {
        return std::nullopt;
    }

    std::vector<WireVertex> transferVertices;
    transferVertices.push_back(owner.vertices.front());
    for (auto it = std::next(owner.vertices.begin()); it <= ownerVertexIt; ++it) {
        transferVertices.push_back(*it);
    }
    transferVertices.push_back(candidate.adjacentVertex);

    std::vector<bool> usedEdges(info.edges.size(), false);
    for (const WireVertex& vertex : transferVertices) {
        if (vertex.edgeIndex < usedEdges.size()) {
            usedEdges[vertex.edgeIndex] = true;
        }
    }

    const gp_Pnt target = vertexPoint(info, owner.vertices.front());
    const gp_Pnt current = vertexOtherPoint(info, candidate.adjacentVertex);
    std::vector<WireVertex> branchPath;
    if (!findTightBoundBranchPathToPoint(info, owner, current, target, usedEdges, branchPath)) {
        return std::nullopt;
    }
    transferVertices.insert(transferVertices.end(), branchPath.begin(), branchPath.end());

    TightBoundTransferPath path;
    path.transferVertices = std::move(transferVertices);

    for (auto it = std::next(ownerVertexIt); it != owner.vertices.end(); ++it) {
        path.splitOwnerVertices.push_back(*it);
    }
    path.splitWireVertices = path.splitOwnerVertices;

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundWithSplit(), after moving the remaining old-owner vertices
    // into splitWire, appends "vertexStack[stack[i].iCurrent]" from stackPos down to stackStart
    // as reversed vertices. The branch stack here is the adjacent branch plus the closed path
    // returned by _findClosedWires().
    std::vector<WireVertex> branchStack;
    branchStack.push_back(candidate.adjacentVertex);
    branchStack.insert(branchStack.end(), branchPath.begin(), branchPath.end());
    for (auto it = branchStack.rbegin(); it != branchStack.rend(); ++it) {
        WireVertex reversed = *it;
        reversed.start = !reversed.start;
        path.splitWireVertices.push_back(reversed);
    }
    return path;
}

bool WireJoiner::isDoneOwner(const WireInfo& info, std::size_t ownerId) const
{
    if (ownerId == 0U) {
        return false;
    }
    for (const OwnerWireInfo& owner : info.ownerWires) {
        if (owner.id == ownerId) {
            return owner.done;
        }
        for (const TightBoundTransferWire& transfer : owner.transferWires) {
            if (transfer.id == ownerId) {
                return transfer.done;
            }
        }
    }
    return info.done && ownerId == info.id;
}

std::vector<std::size_t> WireJoiner::doneAdjacentOwnersAtEndpoint(const WireInfo& info,
                                                                  const EdgeInfo& edge,
                                                                  int endpointIndex) const
{
    std::vector<std::size_t> owners;
    if (endpointIndex < 0 || endpointIndex > 1 || edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
        return owners;
    }

    for (int adjacentIndex = edge.iStart[endpointIndex]; adjacentIndex < edge.iEnd[endpointIndex]; ++adjacentIndex) {
        if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
            continue;
        }
        const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
        if (adjacent.edgeIndex >= info.edges.size()) {
            continue;
        }
        const EdgeInfo& candidate = info.edges[adjacent.edgeIndex];
        if (&candidate == &edge || candidate.edge.IsNull() || candidate.iteration < 0 || candidate.wireInfo == 0U
            || candidate.wireInfo == edge.wireInfo || !isDoneOwner(info, candidate.wireInfo)) {
            continue;
        }
        if (std::find(owners.begin(), owners.end(), candidate.wireInfo) == owners.end()) {
            owners.push_back(candidate.wireInfo);
        }
    }
    return owners;
}

void WireJoiner::recordExhaustOwnerVertex(WireInfo& info, const WireVertex& vertex, std::size_t ownerId)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBound(), first loop:
    // "if (edgeInfo->wireInfo != info.wireInfo) edgeInfo->wireInfo2 = info.wireInfo".
    // This is the request-local secondary-owner ledger; edges still missing "wireInfo2" remain
    // an explicit exhaustTightBound() migration gap instead of being filled from graph degree.
    if (ownerId == 0U || vertex.edgeIndex >= info.edges.size()) {
        return;
    }
    EdgeInfo& edge = info.edges[vertex.edgeIndex];
    if (edge.wireInfo == ownerId) {
        edge.exhaustSeed = true;
        if (edge.wireInfo2 != 0U) {
            edge.exhaustSharedOwner = true;
            edge.exhaustDoneSecondary = true;
        }
        return;
    }
    if (edge.wireInfo == 0U) {
        return;
    }
    if (edge.wireInfo2 == 0U) {
        edge.wireInfo2 = ownerId;
        edge.exhaustSecondaryOwner = true;
    }
    edge.exhaustSharedOwner = true;
    edge.exhaustDoneSecondary = true;
}

void WireJoiner::recordExhaustAdjacentSecondaryOwners(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::exhaustTightBoundWithAdjacent(), for an edge whose second owner is still
    // missing, checks adjacent edges with a done "wireInfo" and searches a second tight-bound
    // wire from that branch. cad-core records the same done-owner adjacency as the next
    // request-local step before the full stack/wireSet search is migrated.
    for (EdgeInfo& edge : info.edges) {
        if (edge.edge.IsNull() || edge.iteration < 0 || edge.wireInfo == 0U || edge.wireInfo2 != 0U
            || !isDoneOwner(info, edge.wireInfo)) {
            continue;
        }
        const std::size_t edgeIndex = static_cast<std::size_t>(&edge - info.edges.data());
        for (int endpointIndex = 0; endpointIndex < 2 && edge.wireInfo2 == 0U; ++endpointIndex) {
            if (edge.iStart[endpointIndex] < 0 || edge.iEnd[endpointIndex] < 0) {
                continue;
            }
            const WireVertex beginVertex{edgeIndex, endpointIndex == 0, 0U};
            const gp_Pnt target = vertexPoint(info, beginVertex);
            for (int adjacentIndex = edge.iStart[endpointIndex];
                 adjacentIndex < edge.iEnd[endpointIndex];
                 ++adjacentIndex) {
                if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                    continue;
                }
                const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
                if (adjacent.edgeIndex >= info.edges.size() || adjacent.edgeIndex == edgeIndex) {
                    continue;
                }
                const EdgeInfo& candidate = info.edges[adjacent.edgeIndex];
                if (candidate.edge.IsNull() || candidate.iteration < 0 || candidate.wireInfo == 0U
                    || candidate.wireInfo == edge.wireInfo || candidate.wireInfo2 != 0U
                    || !isDoneOwner(info, candidate.wireInfo)) {
                    continue;
                }
                std::vector<bool> usedEdges(info.edges.size(), false);
                usedEdges[edgeIndex] = true;
                usedEdges[adjacent.edgeIndex] = true;
                std::vector<WireVertex> branchPath;
                const gp_Pnt current = vertexOtherPoint(info, adjacent);
                if (!findBranchPathToPointSkippingOwner(
                        info, edge.wireInfo, current, target, usedEdges, branchPath)) {
                    continue;
                }
                edge.wireInfo2 = candidate.wireInfo;
                edge.exhaustSecondaryOwner = true;
                edge.exhaustSharedOwner = true;
                edge.exhaustDoneSecondary = true;
                edge.exhaustSearchCandidate = true;
                break;
            }
        }
        if (edge.wireInfo2 != 0U) {
            continue;
        }
        const std::vector<std::size_t> startOwners = doneAdjacentOwnersAtEndpoint(info, edge, 0);
        const std::vector<std::size_t> endOwners = doneAdjacentOwnersAtEndpoint(info, edge, 1);
        for (std::size_t ownerId : startOwners) {
            if (std::find(endOwners.begin(), endOwners.end(), ownerId) == endOwners.end()) {
                continue;
            }
            edge.wireInfo2 = ownerId;
            edge.exhaustSecondaryOwner = true;
            edge.exhaustSharedOwner = true;
            edge.exhaustDoneSecondary = true;
            edge.exhaustSearchCandidate = true;
            break;
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
    if (!info.ownerWires.empty()) {
        for (const OwnerWireInfo& owner : info.ownerWires) {
            if (owner.done) {
                const std::vector<WireVertex>& ownerVertices =
                    owner.splitOwnerVertices.empty() ? owner.vertices : owner.splitOwnerVertices;
                for (const WireVertex& vertex : ownerVertices) {
                    recordExhaustOwnerVertex(info, vertex, owner.id);
                }
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (!transfer.done) {
                    continue;
                }
                for (const WireVertex& vertex : transfer.vertices) {
                    recordExhaustOwnerVertex(info, vertex, transfer.id);
                }
            }
        }
        recordExhaustAdjacentSecondaryOwners(info);
        return;
    }

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
    }
}

void WireJoiner::recordBuildClosedWireRemovalLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::buildClosedWire(), after "findTightBound(); exhaustTightBound();", counts
    // vertices from done "wireInfo" / "wireInfo2"; when "++counter[vertex.edgeInfo()] == 2",
    // it marks "vertex.edgeInfo()->iteration = -1" and calls "aHistory->Remove(info.edge)".
    // cad-core records that request-local Remove lifecycle before generated open-export edges are
    // added; the repeated findClosedWires()/findTightBound() loop remains a later migration step.
    if (!info.done) {
        return;
    }

    std::vector<int> counter(info.edges.size(), 0);
    std::vector<std::size_t> countedOwners;
    std::size_t removedCount = 0;

    const auto ownerVertices = [&](std::size_t ownerId) -> const std::vector<WireVertex>* {
        for (const OwnerWireInfo& owner : info.ownerWires) {
            if (owner.id == ownerId) {
                return owner.splitOwnerVertices.empty() ? &owner.vertices : &owner.splitOwnerVertices;
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (transfer.id == ownerId) {
                    return &transfer.vertices;
                }
            }
        }
        return nullptr;
    };
    const auto ownerAlreadyCounted = [&](std::size_t ownerId) {
        return std::find(countedOwners.begin(), countedOwners.end(), ownerId) != countedOwners.end();
    };
    const auto countOwner = [&](std::size_t ownerId) {
        if (ownerId == 0U || ownerAlreadyCounted(ownerId) || !isDoneOwner(info, ownerId)) {
            return;
        }
        const std::vector<WireVertex>* vertices = ownerVertices(ownerId);
        if (vertices == nullptr) {
            return;
        }
        countedOwners.push_back(ownerId);
        for (const WireVertex& vertex : *vertices) {
            if (vertex.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& edge = info.edges[vertex.edgeIndex];
            if (edge.iteration == -2) {
                continue;
            }
            if (++counter[vertex.edgeIndex] == 2 && edge.iteration >= 0) {
                edge.iteration = -1;
                ++removedCount;
            }
        }
    };

    for (EdgeInfo& edge : info.edges) {
        if (edge.iteration == -2) {
            continue;
        }
        if (edge.iteration < 0 || edge.wireInfo == 0U) {
            continue;
        }
        if (!isDoneOwner(info, edge.wireInfo)) {
            if (edge.iteration >= 0) {
                edge.iteration = -1;
                ++removedCount;
            }
            continue;
        }
        countOwner(edge.wireInfo2);
        countOwner(edge.wireInfo);
    }

    historySummary_.deletedHistoryCount += removedCount;
    historySummary_.splitterHistory = historySummary_.splitterHistory || removedCount > 0U;
}

void WireJoiner::recordBranchSearchCandidatesForOwner(WireInfo& info,
                                                      OwnerWireInfo& owner,
                                                      const std::vector<TopoDS_Face>& boundedFaces)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBound(), for each "wireInfo->vertices", marks
    // "vertex.it->iteration2 = iteration2" and then ::findTightBoundByVertices() walks the
    // current EdgeInfo adjacent range. This keeps branch candidates scoped to the closed
    // WireInfo produced by findClosedWires(), not to the whole split-edge component.
    std::vector<bool> ownerEdge(info.edges.size(), false);
    const int iteration2 = nextIteration2_++;
    for (const WireVertex& vertex : owner.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        ownerEdge[vertex.edgeIndex] = true;
        info.edges[vertex.edgeIndex].iteration2 = iteration2;
    }

    for (WireVertex& vertex : owner.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& current = info.edges[vertex.edgeIndex];
        const int adjacentEndpointIndex = vertex.start ? 1 : 0;
        const gp_Pnt point = adjacentEndpointIndex == 0 ? current.p1 : current.p2;
        std::size_t candidates = 0;
        std::size_t insideCandidates = 0;
        std::size_t outsideCandidates = 0;
        std::size_t newWireSeeds = 0;
        if (current.iStart[adjacentEndpointIndex] < 0 || current.iEnd[adjacentEndpointIndex] < 0) {
            continue;
        }
        for (int adjacentIndex = current.iStart[adjacentEndpointIndex];
             adjacentIndex < current.iEnd[adjacentEndpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex == vertex.edgeIndex || adjacent.edgeIndex >= info.edges.size()) {
                continue;
            }
            if (ownerEdge[adjacent.edgeIndex]) {
                continue;
            }
            EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.iteration < 0) {
                continue;
            }
            const gp_Pnt adjacentPoint = adjacent.start ? next.p1 : next.p2;
            if (!samePoint(point, adjacentPoint)) {
                continue;
            }
            ++candidates;
            const bool inside = pointInsideOrOnAnyFace(next.mid, boundedFaces);
            if (inside) {
                ++insideCandidates;
                ++newWireSeeds;
                const bool transfersOwnerEdge = current.wireInfo == owner.id;
                owner.branchCandidates.push_back(TightBoundBranchCandidate{
                    vertex,
                    adjacent,
                    true,
                    transfersOwnerEdge,
                });
                if (transfersOwnerEdge) {
                    current.tightBoundOwnerTransferCandidate = true;
                }
            }
            else {
                ++outsideCandidates;
                owner.branchCandidates.push_back(TightBoundBranchCandidate{
                    vertex,
                    adjacent,
                    false,
                    false,
                });
            }
        }
        vertex.branchCandidateCount = candidates;
        owner.branchSearchCandidateCount += candidates;
        owner.branchSearchInsideCandidateCount += insideCandidates;
        owner.branchSearchOutsideCandidateCount += outsideCandidates;
        current.branchCandidateCount += candidates;
        current.branchInsideCandidateCount += insideCandidates;
        current.branchOutsideCandidateCount += outsideCandidates;
        current.newWireSeedCandidateCount += newWireSeeds;
        if (newWireSeeds > 0U) {
            owner.hasNewWireSeed = true;
            info.hasNewWireSeed = true;
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
    if (!info.ownerWires.empty()) {
        for (OwnerWireInfo& owner : info.ownerWires) {
            recordBranchSearchCandidatesForOwner(info, owner, boundedFaces);
        }
        return;
    }

    for (WireVertex& vertex : info.orderedVertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& current = info.edges[vertex.edgeIndex];
        const int adjacentEndpointIndex = vertex.start ? 1 : 0;
        const gp_Pnt point = adjacentEndpointIndex == 0 ? current.p1 : current.p2;
        std::size_t candidates = 0;
        std::size_t insideCandidates = 0;
        std::size_t outsideCandidates = 0;
        std::size_t newWireSeeds = 0;
        if (current.iStart[adjacentEndpointIndex] < 0 || current.iEnd[adjacentEndpointIndex] < 0) {
            continue;
        }
        for (int adjacentIndex = current.iStart[adjacentEndpointIndex];
             adjacentIndex < current.iEnd[adjacentEndpointIndex];
             ++adjacentIndex) {
            if (adjacentIndex < 0 || static_cast<std::size_t>(adjacentIndex) >= info.adjacentVertices.size()) {
                continue;
            }
            const WireVertex& adjacent = info.adjacentVertices[static_cast<std::size_t>(adjacentIndex)];
            if (adjacent.edgeIndex == vertex.edgeIndex || adjacent.edgeIndex >= info.edges.size()) {
                continue;
            }
            EdgeInfo& next = info.edges[adjacent.edgeIndex];
            if (next.iteration < 0) {
                continue;
            }
            const gp_Pnt adjacentPoint = adjacent.start ? next.p1 : next.p2;
            if (!samePoint(point, adjacentPoint)) {
                continue;
            }
            ++candidates;
            if (pointInsideOrOnAnyFace(next.mid, boundedFaces)) {
                ++insideCandidates;
                ++newWireSeeds;
            }
            else {
                ++outsideCandidates;
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

bool WireJoiner::recordTightBoundTransferWire(WireInfo& info, OwnerWireInfo& owner)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundByVertices(), after finding an inside adjacent branch, creates
    // "newWire.reset(new WireInfo())", pushes "beginVertex" and the current search stack, then
    // transfers vertices whose "edgeInfo()->wireInfo == wireInfo" to the new owner, then
    // ::findTightBoundSplitWire() creates "splitWire" for the remaining old owner vertices.
    // cad-core now applies the transfer id to EdgeInfo and records the remaining owner vertices
    // as an explicit split-owner ledger while the wider open-export bridge is still temporary.
    if (!owner.transferWires.empty() || owner.vertices.empty()) {
        return false;
    }

    std::optional<TightBoundTransferPath> transferPath;
    std::optional<TightBoundExistingWireSearchTrace> selectedExistingWireTrace;
    for (const TightBoundBranchCandidate& candidate : owner.branchCandidates) {
        std::optional<TightBoundExistingWireSearchTrace> candidateTrace;
        if (candidate.inside && candidate.transfersOwnerEdge) {
            const TightBoundExistingWireSearchTrace trace =
                traceExistingWireSearchForCandidate(info, owner, candidate);
            candidateTrace = trace;
            ++owner.tightBoundExistingWireSearchCount;
            owner.tightBoundExistingWireSearchStackFrameCount += trace.stackFrameCount;
            owner.tightBoundExistingWireSearchVertexStackCount += trace.vertexStackCount;
            owner.tightBoundExistingWireSearchEdgeSetVisitCount += trace.edgeSetVisitCount;
            owner.tightBoundExistingWireSearchBacktrackCount += trace.backtrackCount;
            owner.tightBoundExistingWireSearchIntersectSkipCount += trace.intersectSkipCount;
            if (trace.hit) {
                ++owner.tightBoundExistingWireHitCount;
            }
            if (trace.reverseHit) {
                ++owner.tightBoundExistingWireReverseHitCount;
            }
            if (trace.purge) {
                ++owner.tightBoundExistingWirePurgeCount;
            }
        }
        transferPath = tightBoundTransferPathForCandidate(info, owner, candidate);
        if (transferPath) {
            selectedExistingWireTrace = candidateTrace;
            if (selectedExistingWireTrace) {
                transferPath->existingWireHit = selectedExistingWireTrace->hit;
                transferPath->existingWireIdxVertex = selectedExistingWireTrace->idxVertex;
                transferPath->existingWireStackPos = selectedExistingWireTrace->stackPos;
            }
            break;
        }
    }
    if (!transferPath || transferPath->transferVertices.empty()) {
        return false;
    }

    TightBoundTransferWire transfer;
    transfer.id = nextWireInfoId_++;
    transfer.vertices = transferPath->transferVertices;
    transfer.splitWireVertices = transferPath->splitWireVertices;
    transfer.existingWireHit = transferPath->existingWireHit;
    transfer.existingWireIdxVertex = transferPath->existingWireIdxVertex;
    transfer.existingWireStackPos = transferPath->existingWireStackPos;

    for (const WireVertex& vertex : transfer.vertices) {
        if (vertex.edgeIndex >= info.edges.size()) {
            continue;
        }
        EdgeInfo& edge = info.edges[vertex.edgeIndex];
        if (edge.wireInfo == owner.id) {
            edge.wireInfo = transfer.id;
            edge.tightBoundTransferredOwner = true;
            ++transfer.transferredOwnerEdgeCount;
        }
    }
    transfer.wireBuilt = !wireFromVertices(info, transfer.vertices).IsNull();
    transfer.splitWireBuilt =
        !transfer.splitWireVertices.empty() && !wireFromVertices(info, transfer.splitWireVertices).IsNull();
    transfer.done = transfer.transferredOwnerEdgeCount > 0U;

    if (!transferPath->splitOwnerVertices.empty()
        && transferPath->splitOwnerVertices.size() != owner.vertices.size()) {
        owner.splitOwnerVertices = transferPath->splitOwnerVertices;
        owner.splitOwnerWireBuilt = !wireFromVertices(info, owner.splitOwnerVertices).IsNull();
    }
    owner.transferWires.push_back(std::move(transfer));
    return true;
}

void WireJoiner::recordTightBoundLifecycle(WireInfo& info)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::findTightBoundSplitWire() creates "splitWire.reset(new WireInfo())"
    // when the branch search slices an existing WireInfo, and ::findTightBoundUpdateVertices()
    // marks "beginInfo.wireInfo->done = true" before propagating that owner to vertices whose
    // EdgeInfo still points at another unfinished WireInfo. This records the equivalent
    // request-local lifecycle boundary without using it as an output pruning rule yet.
    bool hasOpenExportEdge = false;
    for (const EdgeInfo& edge : info.edges) {
        hasOpenExportEdge = hasOpenExportEdge || edge.iteration == -3 || (edge.wireInfo == 0U && edge.iteration >= 0);
    }

    if (!info.ownerWires.empty()) {
        bool anyDone = false;
        for (OwnerWireInfo& owner : info.ownerWires) {
            const bool transferRecorded =
                owner.branchSearchInsideCandidateCount > 0U && recordTightBoundTransferWire(info, owner);
            if (transferRecorded) {
                owner.hasSplitWireCandidate = true;
                owner.splitWireCandidateCount = 1;
                info.hasSplitWireCandidate = true;
                info.splitWireCandidateCount += owner.splitWireCandidateCount;
                for (const WireVertex& vertex : owner.vertices) {
                    if (vertex.edgeIndex >= info.edges.size()) {
                        continue;
                    }
                    EdgeInfo& edge = info.edges[vertex.edgeIndex];
                    if (edge.branchInsideCandidateCount > 0U) {
                        edge.splitWireCandidateCount += edge.branchInsideCandidateCount;
                    }
                }
            }
            const bool hasOwnerVertices =
                !owner.splitOwnerVertices.empty() || (!owner.vertices.empty() && owner.transferWires.empty());
            if (hasOwnerVertices) {
                owner.done = true;
                anyDone = true;
            }
        }
        info.done = anyDone;
        if (info.done) {
            for (EdgeInfo& edge : info.edges) {
                if (edge.wireInfo == 0U) {
                    ++info.ownerPropagationCandidateCount;
                    ++edge.ownerPropagationCandidateCount;
                }
            }
        }
        return;
    }

    bool hasOwnedEdge = false;
    std::size_t insideBranchCandidates = 0;
    std::size_t outsideBranchCandidates = 0;
    for (const EdgeInfo& edge : info.edges) {
        hasOwnedEdge = hasOwnedEdge || edge.wireInfo != 0U;
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

void WireJoiner::recordOpenWireCompoundLedger(WireInfo& info)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), after buildClosedWire(), loops final EdgeInfo states and adds
    // "info.wire()" to openWireCompound when "iteration == -3 || (!info.wireInfo && info.iteration >= 0)".
    // This is a request-local mirror of that child-wire boundary; getOpenWires() still keeps the
    // transitional merge/purge bridge until generated open-export and source identity are complete.
    info.openWireCompoundWires.clear();
    for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
        const EdgeInfo& edgeInfo = info.edges[edgeIndex];
        const bool exportsOpenEdge = edgeInfo.iteration == -3
            || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
        if (!exportsOpenEdge) {
            continue;
        }

        OpenWireCompoundWireInfo childWire;
        childWire.edgeIndex = edgeIndex;
        childWire.wire = edgeInfo.wire();
        childWire.wireBuilt = !childWire.wire.IsNull();
        childWire.superEdgeWire = !edgeInfo.superEdge.IsNull();
        childWire.generatedOpenExport = edgeInfo.generatedOpenExportEdge;
        childWire.purgeBridge = edgeInfo.purgeAsOriginalOpenEdge;
        childWire.sourceSharedVertexPurgeMatch =
            !sourceEdges_.empty() && allEdgesShareOriginalSourceVertexByIdentity(childWire.wire, sourceEdges_);
        info.openWireCompoundWires.push_back(std::move(childWire));
    }
}

WireJoinerLedgerSummary WireJoiner::ledgerSummary() const
{
    WireJoinerLedgerSummary summary;
    for (const WireInfo& info : openWires_) {
        summary.superEdgeCandidateCount += info.superEdges.size();
        for (const SuperEdgeInfo& superEdge : info.superEdges) {
            summary.superEdgeCandidateEdgeInfoCount += superEdge.vertices.size();
            if (superEdge.closed) {
                ++summary.superEdgeClosedCandidateCount;
            }
            else {
                ++summary.superEdgeOpenCandidateCount;
            }
        }
        summary.openWireCompoundWireInfoCount += info.openWireCompoundWires.size();
        for (const OpenWireCompoundWireInfo& childWire : info.openWireCompoundWires) {
            ++summary.openWireCompoundEdgeInfoCount;
            if (childWire.wireBuilt) {
                ++summary.openWireCompoundBuiltWireInfoCount;
            }
            if (childWire.superEdgeWire) {
                ++summary.openWireCompoundSuperEdgeWireInfoCount;
            }
            if (childWire.generatedOpenExport) {
                ++summary.openWireCompoundGeneratedWireInfoCount;
            }
            if (childWire.purgeBridge) {
                ++summary.openWireCompoundPurgeBridgeWireInfoCount;
            }
            if (childWire.sourceSharedVertexPurgeMatch) {
                ++summary.openWireCompoundSourceSharedVertexWireInfoCount;
                if (childWire.purgeBridge) {
                    ++summary.openWireCompoundPurgeBridgeSourceSharedVertexWireInfoCount;
                }
            }
            if (childWire.purgeBridge && !childWire.sourceSharedVertexPurgeMatch) {
                ++summary.openWireCompoundPurgeBridgeUnmatchedWireInfoCount;
            }
        }
        summary.closedWireInfoCount += info.ownerWires.size();
        for (const OwnerWireInfo& owner : info.ownerWires) {
            summary.closedWireVertexCount += owner.vertices.size();
            summary.closedWireSearchStackFrameCount += owner.closedWireSearchStackFrameCount;
            summary.closedWireSearchVertexStackCount += owner.closedWireSearchVertexStackCount;
            summary.closedWireSearchEdgeSetVisitCount += owner.closedWireSearchEdgeSetVisitCount;
            summary.closedWireSearchBacktrackCount += owner.closedWireSearchBacktrackCount;
            summary.closedWireSearchIntersectSkipCount += owner.closedWireSearchIntersectSkipCount;
            summary.tightBoundExistingWireSearchCount += owner.tightBoundExistingWireSearchCount;
            summary.tightBoundExistingWireHitCount += owner.tightBoundExistingWireHitCount;
            summary.tightBoundExistingWireReverseHitCount += owner.tightBoundExistingWireReverseHitCount;
            summary.tightBoundExistingWirePurgeCount += owner.tightBoundExistingWirePurgeCount;
            summary.tightBoundExistingWireSearchStackFrameCount += owner.tightBoundExistingWireSearchStackFrameCount;
            summary.tightBoundExistingWireSearchVertexStackCount += owner.tightBoundExistingWireSearchVertexStackCount;
            summary.tightBoundExistingWireSearchEdgeSetVisitCount += owner.tightBoundExistingWireSearchEdgeSetVisitCount;
            summary.tightBoundExistingWireSearchBacktrackCount += owner.tightBoundExistingWireSearchBacktrackCount;
            summary.tightBoundExistingWireSearchIntersectSkipCount += owner.tightBoundExistingWireSearchIntersectSkipCount;
            for (const TightBoundBranchCandidate& candidate : owner.branchCandidates) {
                if (!candidate.inside) {
                    continue;
                }
                ++summary.tightBoundNewWireCandidateCount;
                summary.tightBoundNewWireVertexCount += 2U;
            }
            if (owner.branchSearchCandidateCount > 0U) {
                ++summary.branchSearchSeedWireInfoCount;
            }
            if (owner.hasNewWireSeed) {
                ++summary.newWireSeedWireInfoCount;
            }
            summary.tightBoundTransferWireInfoCount += owner.transferWires.size();
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                summary.tightBoundTransferWireVertexCount += transfer.vertices.size();
                summary.tightBoundSplitWireVertexCount += transfer.splitWireVertices.size();
                if (transfer.existingWireHit && transfer.existingWireIdxVertex >= 0) {
                    ++summary.tightBoundExistingWireIdxVertexCount;
                }
                if (transfer.existingWireHit && transfer.existingWireStackPos >= 0) {
                    ++summary.tightBoundExistingWireStackPosCount;
                }
                if (transfer.splitWireBuilt) {
                    ++summary.tightBoundSplitWireBuiltCount;
                }
            }
            if (!owner.splitOwnerVertices.empty()) {
                ++summary.tightBoundSplitOwnerWireInfoCount;
                summary.tightBoundSplitOwnerVertexCount += owner.splitOwnerVertices.size();
                if (owner.splitOwnerWireBuilt) {
                    ++summary.tightBoundSplitOwnerBuiltWireCount;
                }
            }
            if (owner.done) {
                ++summary.tightBoundDoneWireInfoCount;
            }
            for (const TightBoundTransferWire& transfer : owner.transferWires) {
                if (transfer.done) {
                    ++summary.tightBoundDoneWireInfoCount;
                }
            }
            if (owner.hasSplitWireCandidate) {
                summary.tightBoundSplitWireInfoCount += owner.splitWireCandidateCount;
            }
        }
        for (const EdgeInfo& edgeInfo : info.edges) {
            ++summary.edgeInfoCount;
            if (edgeInfo.splitFromInputEdge) {
                ++summary.splitEdgeInfoCount;
            }
            if (edgeInfo.generatedOpenExportEdge) {
                ++summary.generatedOpenExportEdgeInfoCount;
            }
            if (edgeInfo.superEdgeRoot) {
                ++summary.superEdgeRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeMaterialized) {
                ++summary.superEdgeMaterializedRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeInfo != 0U
                && (edgeInfo.superEdgeMaterialized || edgeInfo.superEdgeShadowedMember)) {
                ++summary.superEdgeMaterializedEdgeInfoCount;
            }
            if (edgeInfo.superEdgeShadowedMember) {
                ++summary.superEdgeShadowedMemberEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleMemberMinusOne) {
                ++summary.superEdgeLifecycleMemberMinusOneEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleOpenRoot) {
                ++summary.superEdgeLifecycleOpenRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeLifecycleClosedRoot) {
                ++summary.superEdgeLifecycleClosedRootEdgeInfoCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeRewritten) {
                ++summary.superEdgeLifecycleAdjacentRangeRewriteCount;
            }
            if (edgeInfo.superEdgeEndpointRewritten) {
                ++summary.superEdgeLifecycleEndpointRewriteCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeSourceEdgeInfo != 0U) {
                ++summary.superEdgeLifecycleAdjacentRangeSourceEdgeInfoCount;
            }
            if (edgeInfo.superEdgeAdjacentRangeStart >= 0
                && edgeInfo.superEdgeAdjacentRangeEnd >= edgeInfo.superEdgeAdjacentRangeStart) {
                summary.superEdgeLifecycleAdjacentRangeVertexCount += static_cast<std::size_t>(
                    edgeInfo.superEdgeAdjacentRangeEnd - edgeInfo.superEdgeAdjacentRangeStart);
            }
            const bool hasSourceIdentityVertex =
                edgeInfo.sourceVertexIdentity[0] || edgeInfo.sourceVertexIdentity[1];
            const bool hasOnlySourceIdentityVertices =
                edgeInfo.sourceVertexIdentity[0] && edgeInfo.sourceVertexIdentity[1];
            if (hasSourceIdentityVertex) {
                ++summary.sourceIdentitySharedVertexEdgeInfoCount;
            }
            if (hasOnlySourceIdentityVertices) {
                ++summary.sourceIdentityOnlySourceVerticesEdgeInfoCount;
            }
            const bool hasSourceLineage = !edgeInfo.sourceEdgeIndices.empty();
            if (hasSourceLineage) {
                ++summary.sourceLineageEdgeInfoCount;
                if (edgeInfo.sourceLineageFromSplitterHistory) {
                    ++summary.sourceLineageSplitEdgeInfoCount;
                }
                if (edgeInfo.sourceEdgeIndices.size() > 1U) {
                    ++summary.sourceLineageMultiSourceEdgeInfoCount;
                }
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
            if (edgeInfo.exhaustSecondaryOwner) {
                ++summary.exhaustSecondaryOwnerEdgeInfoCount;
            }
            if (edgeInfo.wireInfo != 0U) {
                ++summary.primaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.wireInfo2 != 0U) {
                ++summary.secondaryOwnedEdgeInfoCount;
            }
            if (edgeInfo.tightBoundOwnerTransferCandidate) {
                ++summary.tightBoundOwnerTransferCandidateEdgeInfoCount;
            }
            if (edgeInfo.tightBoundTransferredOwner) {
                ++summary.tightBoundTransferredOwnerEdgeInfoCount;
            }
            if (edgeInfo.closedWireOwner) {
                ++summary.closedWireAssignedEdgeInfoCount;
            }
            const bool exportsOpenEdge = edgeInfo.iteration == -3
                || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
            if (exportsOpenEdge) {
                ++summary.openExportEdgeInfoCount;
                if (hasSourceIdentityVertex) {
                    ++summary.sourceIdentityOpenExportSharedVertexEdgeInfoCount;
                }
                if (hasOnlySourceIdentityVertices) {
                    ++summary.sourceIdentityOpenExportOnlySourceVerticesEdgeInfoCount;
                }
                if (edgeInfo.purgeAsOriginalOpenEdge && hasSourceIdentityVertex) {
                    ++summary.sourceIdentityPurgeBridgeEdgeInfoCount;
                }
                if (hasSourceLineage) {
                    ++summary.sourceLineageOpenExportEdgeInfoCount;
                }
                else {
                    ++summary.sourceLineageMissingOpenExportEdgeInfoCount;
                }
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

WireJoinerHistorySummary WireJoiner::historySummary() const
{
    return historySummary_;
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(const std::string& historyPrefix, bool noOriginal) const
{
    (void)historyPrefix;
    (void)tightBound_;

    std::vector<TopoDS_Edge> allLiveEdges;
    std::vector<TopoDS_Wire> liveWires;
    for (const WireInfo& info : openWires_) {
        std::vector<TopoDS_Edge> liveEdges;
        for (const EdgeInfo& edgeInfo : info.edges) {
            const bool exportsOpenEdge = edgeInfo.iteration == -3
                || (edgeInfo.wireInfo == 0U && edgeInfo.iteration >= 0);
            if (!exportsOpenEdge) {
                continue;
            }
            if (!edgeInfo.superEdge.IsNull()) {
                const TopoDS_Wire wire = edgeInfo.wire();
                if (wire.IsNull()) {
                    continue;
                }
                if (noOriginal && !sourceEdges_.empty() && edgeInfo.purgeAsOriginalOpenEdge
                    && allEdgesShareOriginalSourceVertexByIdentity(wire, sourceEdges_)) {
                    continue;
                }
                liveWires.push_back(wire);
                continue;
            }
            if (noOriginal && !sourceEdges_.empty() && edgeInfo.purgeAsOriginalOpenEdge
                && edgeSharesOriginalSourceVertexByIdentity(edgeInfo.edge, sourceEdges_)) {
                continue;
            }
            liveEdges.push_back(edgeInfo.edge);
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
        const auto mergedWires = wiresFromEdges(allLiveEdges);
        liveWires.insert(liveWires.end(), mergedWires.begin(), mergedWires.end());
    }
    if (noOriginal && !sourceEdges_.empty()) {
        liveWires.erase(std::remove_if(liveWires.begin(),
                                       liveWires.end(),
                                       [&](const TopoDS_Wire& wire) {
	                                           // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
	                                           // ::WireJoinerP::getOpenWires(), calls
	                                           // "source.findSubShapesWithSharedVertex(TopoShape(edge, -1))".
	                                           // Keep the main path topological: same-coordinate source endpoint
	                                           // matching and bounded-face boundary-touch retention are diagnostic
	                                           // compatibility probes, not FreeCAD's open-wire export rule.
	                                           return allEdgesShareOriginalSourceVertexByIdentity(wire, sourceEdges_);
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

std::optional<TopoDS_Shape> generatedOpenExportShapeForSketchInternals(
    const TopoDS_Shape& boundedFaceShape,
    const std::vector<TopoDS_Edge>& openEdges,
    const std::vector<TopoDS_Wire>& closedWires,
    bool splitProducedBoundedFaces,
    bool hasOpenWireOutput)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()
    // exports result-wire topology from final EdgeInfo states into "openWireCompound". cad-core
    // converts the remaining generated open-export boundary edges into final EdgeInfo entries here
    // so getOpenWires() consumes the EdgeInfo ledger instead of SketchInternalBuilder injecting
    // result-wire geometry.
    if (boundedFaceShape.IsNull()) {
        return std::nullopt;
    }

    const bool consumedOpenCutterGraph = splitProducedBoundedFaces && !hasOpenWireOutput
        && openEdges.size() >= 2U && allOpenEdgeEndpointsTouchBoundary(openEdges, boundedFaceShape);
    const bool closedWireCycleExport =
        openEdges.empty() && closedWireCycleNeedsGeneratedOpenExport(boundedFaceShape, closedWires);
    if (openEdges.empty() && closedWires.size() >= 2U) {
        if (const auto partialSharedEdges = partialSharedClosedWireOpenExportShape(boundedFaceShape, closedWires)) {
            return partialSharedEdges;
        }
    }
    if (!consumedOpenCutterGraph && !closedWireCycleExport) {
        return std::nullopt;
    }

    if (consumedOpenCutterGraph && !allOpenEdgeEndpointsTouchClosedWireBoundary(openEdges, closedWires)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
        // ::WireJoinerP::build() exports only final EdgeInfo states with no tight-bound WireInfo.
        // In T-junction cutter networks, through-cutter fragments are owned by tight-bound faces
        // while the branch cutter and original closed boundary remain generated open-export edges.
        return partialJunctionOpenExportShape(boundedFaceShape, closedWires, openEdges);
    }

    return generatedOpenExportShape(boundedFaceShape,
                                    openEdges,
                                    openEdges.empty(),
                                    openEdges.empty() && closedWireEdgesAreLinear(closedWires)
                                        ? wireVertexPoints(closedWires)
                                        : std::vector<gp_Pnt>{});
}

}  // namespace cad_core::geometry
