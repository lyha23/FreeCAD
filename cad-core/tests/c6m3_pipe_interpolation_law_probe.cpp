#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepLib.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

TopoDS_Wire makeSpineWire()
{
    BRepBuilderAPI_MakeEdge edge(gp_Pnt(0.0, 0.0, 0.0), gp_Pnt(8.0, 0.0, 0.0));
    if (!edge.IsDone()) {
        throw std::runtime_error("could not build pipe spine edge");
    }

    BRepBuilderAPI_MakeWire wire(edge.Edge());
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build pipe spine wire");
    }
    TopoDS_Wire result = wire.Wire();
    BRepLib::BuildCurves3d(result);
    return result;
}

TopoDS_Wire makeProfileWire()
{
    const std::vector<gp_Pnt> points {
        gp_Pnt(0.0, -0.5, -0.5),
        gp_Pnt(0.0, 0.5, -0.5),
        gp_Pnt(0.0, 0.5, 0.5),
        gp_Pnt(0.0, -0.5, 0.5),
    };

    BRepBuilderAPI_MakeWire wire;
    for (std::size_t index = 0; index < points.size(); ++index) {
        BRepBuilderAPI_MakeEdge edge(points.at(index), points.at((index + 1U) % points.size()));
        if (!edge.IsDone()) {
            throw std::runtime_error("could not build pipe profile edge");
        }
        wire.Add(edge.Edge());
    }
    if (!wire.IsDone()) {
        throw std::runtime_error("could not build pipe profile wire");
    }

    TopoDS_Wire result = wire.Wire();
    BRepLib::BuildCurves3d(result);
    BRepLib::SameParameter(result);
    return result;
}

double bboxSpan(const nlohmann::json& bbox, std::size_t axis)
{
    return bbox.at("max").at(axis).get<double>() - bbox.at("min").at(axis).get<double>();
}

std::vector<std::string> historyStatuses(const cad_core::part::NamedShapeBuild& build)
{
    if (!build.namedShape) {
        return {};
    }
    return build.namedShape->elementHistoryStatus;
}

cad_core::part::NamedShapeBuild buildPipeShell(
    const std::string& owner,
    const std::vector<cad_core::part::NamedShapeSource>& sources,
    const std::optional<cad_core::part::PipeScalingLaw>& law
)
{
    cad_core::part::PipeShellOptions options;
    options.solid = false;
    options.sewCaps = false;
    options.scalingLaw = law;
    return cad_core::part::makeElementPipeShellFromSources(owner, sources, options);
}

nlohmann::json buildProbeResult()
{
    const TopoDS_Shape spine = makeSpineWire();
    const TopoDS_Shape profile = makeProfileWire();
    const cad_core::part::NamedShape spineNamed
        = cad_core::part::indexedNamedShapeForObject("ProbeSpine", spine);
    const cad_core::part::NamedShape profileNamed
        = cad_core::part::indexedNamedShapeForObject("ProbeProfile", profile);
    const std::vector<cad_core::part::NamedShapeSource> sources {
        {"ProbeSpine", spine, &spineNamed},
        {"ProbeProfile", profile, &profileNamed},
    };

    const cad_core::part::NamedShapeBuild baseline = buildPipeShell(
        "BaselinePipe",
        sources,
        std::nullopt
    );
    if (!baseline.error.empty() || baseline.shape.IsNull() || !baseline.namedShape) {
        throw std::runtime_error(
            baseline.error.empty() ? "baseline PipeShell did not build" : baseline.error
        );
    }

    cad_core::part::PipeScalingLaw interpolation;
    interpolation.kind = cad_core::part::PipeScalingLawKind::Interpolation;
    interpolation.samples = {
        {0.0, 1.0},
        {0.5, 2.0},
        {1.0, 1.0},
    };
    const cad_core::part::NamedShapeBuild interpolated = buildPipeShell(
        "InterpolatedPipe",
        sources,
        interpolation
    );
    if (!interpolated.error.empty() || interpolated.shape.IsNull() || !interpolated.namedShape) {
        throw std::runtime_error(
            interpolated.error.empty() ? "interpolation PipeShell did not build"
                                       : interpolated.error
        );
    }

    const nlohmann::json baselineBbox = cad_core::part::bboxForShape(baseline.shape);
    const nlohmann::json interpolatedBbox = cad_core::part::bboxForShape(interpolated.shape);
    return {
        {"case", "c6m3-pipe-interpolation-law-kernel"},
        {"status", "ok"},
        {"pipe_law",
         {
             {"kind", "Interpolation"},
             {"contract", "cad_core_product_contract"},
             {"domain", {0.0, 1.0}},
             {"no_fallback", true},
             {"samples", {{{"parameter", 0.0}, {"scale", 1.0}},
                          {{"parameter", 0.5}, {"scale", 2.0}},
                          {{"parameter", 1.0}, {"scale", 1.0}}}},
         }},
        {"baseline",
         {
             {"bbox", baselineBbox},
             {"width_y", bboxSpan(baselineBbox, 1U)},
             {"width_z", bboxSpan(baselineBbox, 2U)},
             {"history_status", historyStatuses(baseline)},
         }},
        {"interpolation",
         {
             {"bbox", interpolatedBbox},
             {"width_y", bboxSpan(interpolatedBbox, 1U)},
             {"width_z", bboxSpan(interpolatedBbox, 2U)},
             {"history_status", historyStatuses(interpolated)},
         }},
    };
}

}  // namespace

int main()
{
    try {
        std::cout << buildProbeResult().dump(2) << '\n';
        return 0;
    }
    catch (const Standard_Failure& failure) {
        std::cerr << (failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                            : "OCCT failure")
                  << '\n';
    }
    catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
    }
    return 1;
}
