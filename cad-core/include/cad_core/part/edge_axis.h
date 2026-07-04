#pragma once

#include <TopoDS_Edge.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <string>

namespace cad_core::part {

enum class EdgeAxisKind {
    Line,
    Circle,
    GeometricallyLinearCurve,
};

enum class EdgeAxisRejectReason {
    None,
    NullEdge,
    InvalidParameterRange,
    ZeroLength,
    CircleNotAllowed,
    GeometricLinearNotAllowed,
    NonLinearCurve,
    ReadFailure,
};

struct EdgeAxisOptions {
    bool allowCircleAxis = false;
    bool allowGeometricallyLinearCurve = false;
    bool orientLinearByEdge = false;
};

struct EdgeAxis {
    gp_Pnt base;
    gp_Dir direction;
    double length = 0.0;
    EdgeAxisKind kind = EdgeAxisKind::Line;
};

struct EdgeAxisResolution {
    std::optional<EdgeAxis> axis;
    EdgeAxisRejectReason rejectReason = EdgeAxisRejectReason::None;
    std::string message;
};

EdgeAxisResolution resolveEdgeAxis(const TopoDS_Edge& edge, const EdgeAxisOptions& options);

}  // namespace cad_core::part
