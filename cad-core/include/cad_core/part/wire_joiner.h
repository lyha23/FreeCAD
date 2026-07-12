#pragma once

// Part-layer WireJoiner public facade aligned with FreeCAD
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp.
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <nlohmann/json.hpp>

#include "cad_core/part/internal_shape_history_ledger.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::app
{
class ElementMapProducerTrace;
}

namespace cad_core::part
{

struct WireJoinerBuildResult
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build() publishes children with "builder.Add(openWireCompound,
    // info.wire())"; ::WireJoinerP::getOpenWires() consumes "MapperHistory(aHistory)", and
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals() only appends that open-wire result after
    // Part::FaceMakerBuildFace. Keep this caller-facing package at the open-wire/topo-evidence
    // boundary so Sketcher no longer consumes producer/blocker ledger anatomy.
    std::optional<TopoDS_Shape> openWires;
    bool hasOpenWires = false;
    nlohmann::json diagnostics = nlohmann::json::object();
    InternalShapeHistoryLedger historyLedger;
};

class WireJoiner
{
public:
    WireJoiner();
    ~WireJoiner();
    WireJoiner(WireJoiner&&) noexcept;
    WireJoiner& operator=(WireJoiner&&) noexcept;
    WireJoiner(const WireJoiner&) = delete;
    WireJoiner& operator=(const WireJoiner&) = delete;

    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setTightBound(), SketchObject::buildInternals() enables tight bounds before
    // getOpenWires().
    void setTightBound(bool enabled);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoiner::setMergeEdges(), SketchObject::buildInternals() enables merge before
    // getOpenWires().
    void setMergeEdges(bool enabled);
    void attachProducerTrace(app::ElementMapProducerTrace* trace) noexcept;
    void addOpenWire(
        const TopoDS_Wire& wire,
        const std::vector<std::size_t>& sourceEdgeIndices = {}
    );
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::build(), calls splitEdges(), buildClosedWire(), findTightBound() and
    // exhaustTightBound() before exporting openWireCompound from final EdgeInfo ownership.
    void buildFinalEdgeOwnership(
        const TopoDS_Shape* boundedFaceShape = nullptr,
        const std::vector<TopoDS_Wire>* closedWires = nullptr,
        const std::vector<TopoDS_Edge>* openEdges = nullptr,
        bool splitProducedBoundedFaces = false
    );
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
    // ::WireJoinerP::getOpenWires(), when noOriginal=true, builds a source compound from
    // sourceEdgeArray and removes open-wire edges whose vertices are still shared with source.
    void addSourceEdge(const TopoDS_Edge& edge);
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), calls joiner.getOpenWires(openWires, "SKF").
    std::optional<TopoDS_Shape> getOpenWires(
        const std::string& historyPrefix,
        bool noOriginal = true
    ) const;
    WireJoinerBuildResult buildResult(
        const std::string& historyPrefix,
        bool noOriginal = true
    ) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cad_core::part
