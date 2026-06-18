#pragma once

#include "cad_core/assembly/joint_solver.h"
#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cad_core::assembly::assembly_detail {

struct SolverSummary {
    std::string solve;
    nlohmann::json adapter;
    std::vector<AssemblyPlacementUpdate> placementUpdates;
};

const app::DocumentObject* documentObjectByName(const runtime::ComputeContext& context,
                                                     const std::string& name);
bool isAssemblyJointFeaturePython(const app::DocumentObject& object);
nlohmann::json linkNamesJson(const std::vector<app::Link>& links);
nlohmann::json jointReferenceJson(const app::DocumentObject& object, const std::string& property);
std::vector<std::string> jointNames(const app::DocumentObject& object,
                                    const runtime::ComputeContext& context);
std::vector<std::string> jointGroupNames(const app::DocumentObject& object,
                                         const runtime::ComputeContext& context);
SolverSummary solverSummary(const app::DocumentObject& object, runtime::ComputeContext& context);
runtime::ShapeValue::Kind shapeKindForShape(const TopoDS_Shape& shape);
TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);
void publishLinkedShape(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const TopoDS_Shape& shape,
                        runtime::ShapeValue::Kind kind,
                        const nlohmann::json& metadata,
                        std::optional<part::NamedShape> namedShape = std::nullopt);
void publishEmptyResult(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const nlohmann::json& metadata);

}  // namespace cad_core::assembly::assembly_detail
