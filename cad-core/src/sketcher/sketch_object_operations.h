#pragma once

#include "cad_core/sketcher/sketch_internal_builder.h"

#include "sketch_object_geometry.h"

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::app
{
struct DocumentObject;
}

namespace cad_core::runtime
{
struct ComputeContext;
}

namespace cad_core::sketcher
{

enum class SketchProfileEdgeKind
{
    Line,
    ArcOfCircle,
    ArcOfEllipse,
    ArcOfHyperbola,
    ArcOfParabola,
    BSpline,
    Bezier
};

struct SketchProfileEdge
{
    SketchProfileEdgeKind kind = SketchProfileEdgeKind::Line;
    gp_Pnt start;
    gp_Pnt end;
    gp_Pnt center;
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double focal = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    int degree = 0;
    std::vector<gp_Pnt> poles;
    std::vector<double> weights;
};

struct SketchProfileWires
{
    std::vector<TopoDS_Wire> closedWires;
    std::vector<TopoDS_Wire> openWires;
    std::vector<TopoDS_Edge> openEdges;
    std::vector<std::vector<std::size_t>> closedWireSourceEdgeIndices;
    std::vector<std::vector<std::size_t>> openWireSourceEdgeIndices;
    std::vector<TopoDS_Edge> sourceEdges;
};

struct ProfileFaceBuild
{
    std::optional<TopoDS_Shape> profileShape;
    std::optional<TopoDS_Shape> internalShape;
    bool faceMakerFailed = false;
    bool requiresSubshapeSelection = false;
    std::optional<part::FaceMakerHistorySummary> faceMakerHistory;
    std::optional<part::WireJoinerLedgerSummary> wireJoinerLedger;
    std::optional<part::WireJoinerHistorySummary> wireJoinerHistory;
};

std::string faceMakerRuntimeSourceName(part::FaceMakerBuildFaceRuntimeSource source);

std::vector<SketchProfileEdge> profileEdges(const std::vector<SketchSegment>& segments,
                                            const std::vector<SketchArc>& arcs,
                                            const std::vector<SketchEllipseArc>& ellipseArcs,
                                            const std::vector<SketchHyperbolaArc>& hyperbolaArcs,
                                            const std::vector<SketchParabolaArc>& parabolaArcs,
                                            const std::vector<SketchBSpline>& bsplines,
                                            const std::vector<SketchBezier>& beziers);

std::optional<TopoDS_Shape> buildRawSketchShape(const app::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const std::vector<SketchProfileEdge>& edges,
                                                const std::vector<SketchPoint>& points,
                                                const std::vector<SketchCircle>& circles,
                                                const std::vector<SketchEllipse>& ellipses);

ProfileFaceBuild buildOptionalProfileFace(const std::vector<SketchProfileEdge>& edges,
                                           const std::vector<SketchCircle>& circles,
                                           const std::vector<SketchEllipse>& ellipses);

std::size_t countSubshapesOfKind(const nlohmann::json& subshapes, const std::string& kind);
std::string profileShapeLabel(const std::optional<TopoDS_Shape>& profileShape);

} // namespace cad_core::sketcher
