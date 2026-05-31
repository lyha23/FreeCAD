#include "cad_core/geometry/wire_joiner.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

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
        builder.Add(edges[startIndex]);
        used[startIndex] = true;
        auto [firstPoint, currentEnd] = edgeEndpoints(edges[startIndex]);
        (void)firstPoint;

        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (used[index] || edges[index].IsNull()) {
                    continue;
                }
                const auto [edgeStart, edgeEnd] = edgeEndpoints(edges[index]);
                if (samePoint(edgeStart, currentEnd)) {
                    builder.Add(edges[index]);
                    currentEnd = edgeEnd;
                    used[index] = true;
                    extended = true;
                    break;
                }
                if (samePoint(edgeEnd, currentEnd)) {
                    builder.Add(TopoDS::Edge(edges[index].Reversed()));
                    currentEnd = edgeStart;
                    used[index] = true;
                    extended = true;
                    break;
                }
            }
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
        openWires_.push_back(WireInfo{wire, wireEdges(wire), {}});
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
    if (faceBoundaryEdges.empty()) {
        return;
    }

    for (WireInfo& info : openWires_) {
        std::vector<TopoDS_Edge> fragments;
        for (const TopoDS_Edge& edge : info.edges) {
            const auto split = splitOpenEdgeByFaceBoundaries(edge, faceBoundaryEdges);
            fragments.insert(fragments.end(), split.begin(), split.end());
        }
        info.edges = fragments;
        info.consumedByBoundedFace.assign(info.edges.size(), false);
    }

    for (WireInfo& info : openWires_) {
        for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
            info.consumedByBoundedFace[edgeIndex] = edgeMatchesAnyBoundary(info.edges[edgeIndex], faceBoundaryEdges);
        }
    }
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(const std::string& historyPrefix, bool noOriginal) const
{
    (void)historyPrefix;
    (void)tightBound_;
    (void)mergeEdges_;

    std::vector<TopoDS_Wire> liveWires;
    for (const WireInfo& info : openWires_) {
        std::vector<TopoDS_Edge> liveEdges;
        for (std::size_t edgeIndex = 0; edgeIndex < info.edges.size(); ++edgeIndex) {
            if (edgeIndex < info.consumedByBoundedFace.size() && info.consumedByBoundedFace[edgeIndex]) {
                continue;
            }
            liveEdges.push_back(info.edges[edgeIndex]);
        }
        if (liveEdges.empty()) {
            continue;
        }
        if (noOriginal && !sourceEdges_.empty()) {
            bool original = true;
            for (const TopoDS_Edge& edge : liveEdges) {
                bool found = false;
                for (const TopoDS_Edge& source : sourceEdges_) {
                    if (edgeMatchesSourceVertices(edge, source)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    original = false;
                    break;
                }
            }
            if (original) {
                continue;
            }
        }
        const auto currentWires = wiresFromEdges(liveEdges);
        liveWires.insert(liveWires.end(), currentWires.begin(), currentWires.end());
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

}  // namespace cad_core::geometry
