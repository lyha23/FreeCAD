#pragma once

#include "cad_core/part/face_maker.h"
#include "cad_core/part/wire_joiner.h"
#include "cad_core/runtime/compute_context.h"

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace cad_core::sketcher
{

struct SketchInternalResultInput
{
    std::string objectName;
    TopoDS_Shape rawShape;
    std::optional<TopoDS_Shape> profileShape;
    gp_Dir profileNormal;
    std::optional<TopoDS_Shape> internalShape;
    bool profileRequiresSubshapeSelection = false;
    std::optional<part::FaceMakerHistorySummary> faceMakerHistory;
    std::optional<part::WireJoinerLedgerSummary> wireJoinerLedger;
    std::optional<part::WireJoinerHistorySummary> wireJoinerHistory;
};

struct SketchInternalResult
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::buildInternals(), writes "InternalShape";
    // ::SketchObject::getInternalElementMap(), maps InternalEdge/InternalVertex; and
    // ::SketchObject::getElementTypes(), exposes "InternalEdge", "InternalFace",
    // "InternalVertex". This package keeps that request-local publication together so
    // SketchObject executor code does not rebuild the same InternalShape result state piecemeal.
    runtime::ShapeValue shapeValue;
    std::optional<nlohmann::json> mesh;
    nlohmann::json subshapes = nlohmann::json::object();
    nlohmann::json objectFields = nlohmann::json::object();
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::execute(), writes InternalShape before Shape because downstream references can
// target "InternalShape"; cad-core returns the publishable ShapeValue, InternalShape mesh,
// subshape map and debug/object fields for the executor to merge into ComputeContext.
SketchInternalResult buildSketchInternalResult(const SketchInternalResultInput& input);

}  // namespace cad_core::sketcher
