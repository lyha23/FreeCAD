#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/part/topo_shape.h"

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::runtime
{

struct ShapeValue
{
    enum class Kind
    {
        Sketch,
        Profile,
        Solid,
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
        // ::Vertex::execute(), ::Line::execute() and ::Plane::execute() write non-solid
        // PropertyPartShape values that can still be exported and picked as Part features.
        PartPrimitive,
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Mesh/App/MeshFeature.cpp
        // ::Feature derives from App::GeoFeature and carries PropertyMeshKernel "Mesh"; cad-core
        // keeps imported mesh files as request-local display/pick results, not PartDesign solids.
        Mesh,
        DatumPlane,
        DatumLine,
        DatumPoint,
        DatumCoordinateSystem,
    };

    Kind kind;
    TopoDS_Shape shape;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::buildShape(),
    // sets the raw sketch "Shape" separately from PartDesign's later profile face construction.
    // cad-core keeps the raw sketch shape in shape and the PartDesign-ready closed face here.
    std::optional<TopoDS_Shape> profileShape;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::buildInternals(),
    // writes "InternalShape" from FaceMakerBuildFace and WireJoiner open-wire results.
    std::optional<TopoDS_Shape> internalShape;
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap(),
    // creates bidirectional InternalEdge/InternalVertex aliases only when "Shape.findSubShapesWithSharedVertex"
    // returns a single CheckGeometry match. cad-core keeps that auxiliary ElementMap separate from
    // the raw Sketch Shape NamedShape so raw EdgeN links and InternalEdgeN links do not overwrite
    // each other.
    std::optional<part::NamedShape> internalNamedShape;
    // Split-derived InternalFace regions are individually selectable profile domains; closed
    // wire compounds such as face-with-island can still be consumed as a whole profile.
    bool profileRequiresSubshapeSelection = false;
};

struct AddSubShape
{
    std::optional<TopoDS_Shape> addShape;
    std::optional<TopoDS_Shape> subShape;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp
    // ::FeatureAddSub::getAddSubShape() exposes additive/subtractive tools separately from the
    // feature's final Shape. DressUp::getAddSubShape() can publish delta slots whose ElementMap
    // is not the same as the replacement solid, so cad-core keeps slot-level NamedShape history.
    std::optional<part::NamedShape> addNamedShape;
    std::optional<part::NamedShape> subNamedShape;
};

struct ComputeContext
{
    std::vector<Diagnostic> diagnostics;
    std::map<std::string, ShapeValue> shapes;
    std::map<std::string, AddSubShape> addSubShapes;
    std::map<std::string, nlohmann::json> objects;
    std::map<std::string, nlohmann::json> mesh;
    std::map<std::string, nlohmann::json> subshapes;
    nlohmann::json elementReferenceUpdates = nlohmann::json::array();
    nlohmann::json documentObjectUpdates = nlohmann::json::array();
    std::map<std::string, part::NamedShape> namedShapes;
    std::map<std::string, std::vector<std::string>> dependencies;
    std::map<std::string, const app::DocumentObject*> documentObjects;
    std::map<std::string, std::string> parentGroupByObject;
    std::set<std::string> transformationTemplateObjects;
    std::map<std::string, gp_Trsf> globalPlacements;
    std::vector<std::string> executionOrder;
};

bool hasFailed(const ComputeContext& context, const std::string& object);

}  // namespace cad_core::runtime
