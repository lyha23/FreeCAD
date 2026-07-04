#include "cad_core/part/edge_axis.h"

#include <BRepAdaptor_Curve.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Circ.hxx>
#include <gp_Lin.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace cad_core::part {

namespace {

struct LinearSpan {
    gp_Pnt start;
    gp_Pnt end;
    gp_Dir direction;
    double length = 0.0;
};

EdgeAxisResolution rejected(EdgeAxisRejectReason reason, std::string message)
{
    EdgeAxisResolution result;
    result.rejectReason = reason;
    result.message = std::move(message);
    return result;
}

std::optional<LinearSpan> endpointSpan(
    const TopoDS_Edge& edge,
    const BRepAdaptor_Curve& curve,
    bool orientLinearByEdge,
    EdgeAxisResolution& failure
)
{
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    if (!std::isfinite(first) || !std::isfinite(last)
        || std::abs(last - first) <= Precision::Confusion()) {
        failure = rejected(
            EdgeAxisRejectReason::InvalidParameterRange,
            "edge parameter range is not finite or is too small"
        );
        return std::nullopt;
    }

    gp_Pnt start = curve.Value(first);
    gp_Pnt end = curve.Value(last);
    if (orientLinearByEdge && edge.Orientation() == TopAbs_REVERSED) {
        std::swap(start, end);
    }

    const gp_Vec vector(start, end);
    const double length = vector.Magnitude();
    if (length <= Precision::Confusion()) {
        failure = rejected(EdgeAxisRejectReason::ZeroLength, "edge span is zero-length");
        return std::nullopt;
    }
    return LinearSpan {start, end, gp_Dir(vector), length};
}

EdgeAxisResolution geometricallyLinearAxis(
    const TopoDS_Edge& edge,
    const BRepAdaptor_Curve& curve,
    bool orientLinearByEdge
)
{
    EdgeAxisResolution failure;
    const auto span = endpointSpan(edge, curve, orientLinearByEdge, failure);
    if (!span) {
        return failure;
    }

    const gp_Lin line(span->start, span->direction);
    const double tolerance = std::max(Precision::Confusion() * 10.0, span->length * 1.0e-9);

    constexpr int kSegments = 64;
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    for (int index = 1; index < kSegments; ++index) {
        const double parameter = first + (last - first) * static_cast<double>(index)
            / static_cast<double>(kSegments);
        if (line.Distance(curve.Value(parameter)) > tolerance) {
            return rejected(EdgeAxisRejectReason::NonLinearCurve, "edge is not geometrically linear");
        }
    }

    EdgeAxisResolution result;
    result.axis = EdgeAxis {
        span->start,
        span->direction,
        span->length,
        EdgeAxisKind::GeometricallyLinearCurve,
    };
    return result;
}

}  // namespace

EdgeAxisResolution resolveEdgeAxis(const TopoDS_Edge& edge, const EdgeAxisOptions& options)
{
    if (edge.IsNull()) {
        return rejected(EdgeAxisRejectReason::NullEdge, "edge is null");
    }

    try {
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Circle) {
            if (!options.allowCircleAxis) {
                return rejected(
                    EdgeAxisRejectReason::CircleNotAllowed,
                    "edge is circular, but this property does not accept circle axes"
                );
            }
            const gp_Circ circle = curve.Circle();
            EdgeAxisResolution result;
            result.axis = EdgeAxis {
                circle.Location(),
                circle.Axis().Direction(),
                0.0,
                EdgeAxisKind::Circle,
            };
            return result;
        }

        if (curve.GetType() == GeomAbs_Line) {
            EdgeAxisResolution failure;
            const auto span = endpointSpan(edge, curve, options.orientLinearByEdge, failure);
            if (!span) {
                return failure;
            }
            const gp_Lin line = curve.Line();
            EdgeAxisResolution result;
            result.axis = EdgeAxis {
                options.orientLinearByEdge ? span->start : line.Location(),
                options.orientLinearByEdge ? span->direction : line.Direction(),
                span->length,
                EdgeAxisKind::Line,
            };
            return result;
        }

        if (!options.allowGeometricallyLinearCurve) {
            return rejected(
                EdgeAxisRejectReason::GeometricLinearNotAllowed,
                "edge curve type is not GeomAbs_Line"
            );
        }

        return geometricallyLinearAxis(edge, curve, options.orientLinearByEdge);
    }
    catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        return rejected(
            EdgeAxisRejectReason::ReadFailure,
            std::string("edge curve could not be read: ") + (message != nullptr ? message : "unknown OCCT error")
        );
    }
}

}  // namespace cad_core::part
