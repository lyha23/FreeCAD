#include "cad_core/geometry/wire_joiner.h"

#include "cad_core/geometry/face_maker.h"

#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
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

std::vector<TopoDS_Edge> allOpenEdgesExcept(const std::vector<TopoDS_Wire>& wires,
                                            std::size_t omittedIndex)
{
    std::vector<TopoDS_Edge> edges;
    for (std::size_t index = 0; index < wires.size(); ++index) {
        if (index == omittedIndex) {
            continue;
        }
        const auto currentEdges = wireEdges(wires[index]);
        edges.insert(edges.end(), currentEdges.begin(), currentEdges.end());
    }
    return edges;
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
        openWires_.push_back(WireInfo{wire, false});
    }
}

void WireJoiner::classifyBoundedFaceOwnership(const std::vector<TopoDS_Wire>& faceWires, std::size_t fullFaceCount)
{
    if (faceWires.empty() || fullFaceCount == 0U || openWires_.empty()) {
        return;
    }

    std::vector<TopoDS_Wire> allWires;
    allWires.reserve(openWires_.size());
    for (const WireInfo& info : openWires_) {
        allWires.push_back(info.wire);
    }

    for (std::size_t index = 0; index < openWires_.size(); ++index) {
        const auto edgesWithoutCurrent = allOpenEdgesExcept(allWires, index);
        const FaceMakerBuildFaceResult withoutCurrent =
            makeFacesFromClosedWiresAndSplitEdgesDetailed(faceWires, edgesWithoutCurrent);
        openWires_[index].consumedByBoundedFace = withoutCurrent.faceCount < fullFaceCount;
    }
}

std::optional<TopoDS_Shape> WireJoiner::getOpenWires(const std::string& historyPrefix) const
{
    (void)historyPrefix;
    (void)tightBound_;
    (void)mergeEdges_;

    std::vector<TopoDS_Wire> liveWires;
    for (const WireInfo& info : openWires_) {
        if (!info.consumedByBoundedFace) {
            liveWires.push_back(info.wire);
        }
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
