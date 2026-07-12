#pragma once

#include "cad_core/app/document.h"
#include "cad_core/app/element_map_producer_trace.h"
#include "cad_core/app/string_hasher.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/part/topo_shape.h"

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <optional>
#include <set>
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
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
    // ::ProfileBased::getProfileNormal(), for "Part::Part2DObject", returns the object's
    // Placement rotation applied to "Base::Vector3d(0, 0, 1)" instead of deriving the normal from
    // the generated profile face orientation.
    std::optional<gp_Dir> profileNormal;
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
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapePyImp.cpp
    // ::TopoShapePy::optimalBoundingBox(), exposes "BRepBndLib::AddOptimal" for object oracle
    // collection. Request-local OCCT makers such as Loft/Sewing can carry stale triangulation, so
    // shape producers may opt into geometry-only bbox export without changing global Part bboxes.
    bool usePreciseBoundingBox = false;
};

struct AddSubShape
{
    enum class AdditiveFuseOrder
    {
        BaseFirst,
        FeatureFirst,
    };

    std::optional<TopoDS_Shape> addShape;
    std::optional<TopoDS_Shape> subShape;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp
    // ::FeatureAddSub::getAddSubShape() exposes additive/subtractive tools separately from the
    // feature's final Shape. DressUp::getAddSubShape() can publish delta slots whose ElementMap
    // is not the same as the replacement solid, so cad-core keeps slot-level NamedShape history.
    std::optional<part::NamedShape> addNamedShape;
    std::optional<part::NamedShape> subNamedShape;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
    // ::Hole::execute(), stores AddSubShape for pattern consumers but also writes the post-boolean
    // "this->Shape" after makeElementBoolean(), getSolid(), and refineShapeIfActive().
    std::optional<TopoDS_Shape> replacementShape;
    std::optional<part::NamedShape> replacementNamedShape;
    AdditiveFuseOrder additiveFuseOrder = AdditiveFuseOrder::BaseFirst;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // reads FeatureAddSub deltas through getAddSubShape(); Body should preserve a producer's
    // bbox policy when it directly adopts that additive/subtractive delta, but boolean replay
    // clears it because a new maker result has been built.
    bool addUsesPreciseBoundingBox = false;
    bool subUsesPreciseBoundingBox = false;
    bool replacementUsesPreciseBoundingBox = false;
    bool replacementRefined = false;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
// ::Loft::execute() and ::Sweep::execute() publish their feature properties together with the
// Shape written through PropertyPartShape::setValue(); helper producers such as
// AppPartPy.cpp::makeFilledFace() return an equivalent public shape summary. Keep those
// producer-selected fields separate from cad-core's complete internal object metadata.
struct PublicResultFields
{
    std::optional<nlohmann::json> objectFields;
    std::optional<nlohmann::json> shapeSummary;
    std::optional<std::string> nativeError;
    std::optional<std::string> nativeErrorCode;
};

struct ComputeContext
{
    explicit ComputeContext(
        std::shared_ptr<app::ElementMapProducerTrace> trace =
            std::make_shared<app::ElementMapProducerTrace>()
    );

    std::vector<Diagnostic> diagnostics;
    std::map<std::string, ShapeValue> shapes;
    std::map<std::string, AddSubShape> addSubShapes;
    std::map<std::string, nlohmann::json> objects;
    std::map<std::string, PublicResultFields> publicResultFields;
    std::map<std::string, nlohmann::json> mesh;
    std::map<std::string, nlohmann::json> subshapes;
    nlohmann::json elementReferenceUpdates = nlohmann::json::array();
    nlohmann::json documentObjectUpdates = nlohmann::json::array();
    std::map<std::string, part::NamedShape> namedShapes;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/private/DocumentP.h owns one
    // DocumentP::Hasher.  Recompute creates this request-local table once and producers share it
    // in document execution order; it is never read from a prior response or session.
    std::shared_ptr<app::ElementMapProducerTrace> producerTrace;
    std::shared_ptr<app::StringHasher> stringHasher;
    std::map<std::string, std::vector<std::string>> dependencies;
    std::map<std::string, const app::DocumentObject*> documentObjects;
    std::map<std::string, std::string> parentGroupByObject;
    std::set<std::string> targetObjects;
    std::set<std::string> transformationTemplateObjects;
    std::map<std::string, gp_Trsf> globalPlacements;
    nlohmann::json topoNamingState = nlohmann::json::object();
    double displayMeshDeflection = 0.1;
    std::vector<std::string> executionOrder;
};

bool hasFailed(const ComputeContext& context, const std::string& object);

}  // namespace cad_core::runtime
