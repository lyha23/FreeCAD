#pragma once

// Part-layer TopoShapeExpansion import element-map facade.
#include "cad_core/part/topo_shape.h"

#include <GeomAbs_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

struct RuledSurfaceEdgeEvidence
{
    TopoDS_Edge edge;
    std::vector<std::string> stableEdgeNames;
};

struct RuledSurfaceCurveSource
{
    std::string objectName;
    TopoDS_Shape curve;
    std::vector<RuledSurfaceEdgeEvidence> edges;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
// ::makeFilledFace() parses "degree", "ptsOnCurve", "numIter", "anisotropy",
// "tol2d", "tol3d", "tolG1", "tolG2", "maxDegree" and "maxSegments";
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp
// ::PyInit() passes the same group to "BRepOffsetAPI_MakeFilling".
struct FilledFaceParams
{
    unsigned int degree = 3;
    unsigned int pointsOnCurve = 15;
    unsigned int iterations = 2;
    bool anisotropy = false;
    double tolerance2d = 1e-5;
    double tolerance3d = 1e-4;
    double toleranceG1 = 0.01;
    double toleranceG2 = 0.1;
    unsigned int maxDegree = 8;
    unsigned int maxSegments = 9;
};

struct FilledFaceSource
{
    std::string objectName;
    TopoDS_Shape shape;
    const NamedShape* namedShape = nullptr;
    std::string subname;
    std::string stableSubname;
};

struct FilledFaceBoundaryEvidence
{
    std::string objectName;
    std::string subname;
    std::string stableSubname;
    std::string shapeKind;
};

struct FilledFaceSupportSource
{
    FilledFaceSource target;
    FilledFaceSource support;
};

struct FilledFaceOrderSource
{
    FilledFaceSource target;
    GeomAbs_Shape order = GeomAbs_C0;
    std::string orderName = "C0";
};

struct FilledFaceSupportOrderEvidence
{
    std::string targetObject;
    std::string targetSubname;
    std::string targetStableSubname;
    std::string targetShapeKind;
    bool isBoundary = true;
    std::string builderCall;
    bool hasSupport = false;
    std::string supportObject;
    std::string supportSubname;
    std::string supportStableSubname;
    bool hasOrder = false;
    std::string order;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementFilledFace(), after one boundary wire is selected, "other edges in
// shapes" are added with "IsBound" false; remaining face and vertex shapes call "maker.Add".
struct FilledFaceConstraintEvidence
{
    std::string objectName;
    std::string subname;
    std::string stableSubname;
    std::string shapeKind;
    std::string builderCall;
    bool isBoundary = false;
};

struct FilledFaceBuild
{
    TopoDS_Shape shape;
    std::optional<NamedShape> namedShape;
    std::string error;
    std::string boundaryMode;
    std::vector<FilledFaceBoundaryEvidence> boundarySources;
    std::optional<FilledFaceBoundaryEvidence> initialSurfaceSource;
    std::vector<FilledFaceSupportOrderEvidence> supportOrderSources;
    std::vector<FilledFaceConstraintEvidence> nonBoundarySources;
    int boundaryEdgeCount = 0;
    int compoundSourceCount = 0;
    int expandedSourceCount = 0;
    int nonBoundaryConstraintCount = 0;
    int supportFaceCount = 0;
    int orderCount = 0;
    std::string diagnosticCode;
    std::string diagnosticProperty;
    std::string diagnosticTarget;
    std::string diagnosticSubname;
};

enum class PipeShellMode
{
    Standard,
    Fixed,
    Frenet,
    Auxiliary,
    Binormal,
};

enum class PipeShellProfilePlacement
{
    OcctLocationOverload,
    AnchorLocationToSpineStartProductContract,
};

enum class PipeScalingLawKind
{
    Linear,
    SShape,
    Interpolation,
};

struct PipeScalingLawSample
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::TransformEnums exposes "Interpolation" as an enum label only. CAD Core defines the
    // Interpolation sample contract as request-local [parameter, scale] pairs on domain [0, 1].
    double parameter = 0.0;
    double scale = 1.0;
};

struct PipeScalingLaw
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::execute(), commented branches call "Law_Linear::Set(0, 1, 1,
    // ScalingData[0].x)" and "Law_S::Set(0, 1, ScalingData[0].y, 1, ScalingData[0].x,
    // ScalingData[0].z)" before the active "mkPS.SetLaw(..., scalinglaw)" hook.
    // OCCT: Law_Interpol.hxx::Set(TColgp_Array1OfPnt2d) defines sampled "parameter and value
    // pairs" with X as the parameter and Y as the function value; CAD Core uses that as the
    // product-contract basis for Interpolation without FreeCAD parity claims.
    PipeScalingLawKind kind = PipeScalingLawKind::Linear;
    double x = 1.0;
    double y = 1.0;
    double z = 1.0;
    std::vector<PipeScalingLawSample> samples;
};

struct ContinuousEdgeAdjacencyEvidence
{
    std::string sourceSubname;
    std::string candidateSubname;
    std::string sharedVertex;
    std::string continuity;
    std::string decision;
};

struct ContinuousEdgeLedger
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.h
    // ::getContinuousEdges(), comment says "get the given edges and all their tangent ones";
    // FeaturePipe.cpp::buildPipePath() keeps the call "getContinuousEdges(shape, subedge)"
    // commented out, so CAD Core stores this as a request-local product-extension ledger.
    std::string sourceObject;
    std::vector<std::string> requestedSubnames;
    std::vector<std::string> expandedSubnames;
    std::string continuityRule = "G1Include_C0BoundaryStops";
    std::vector<ContinuousEdgeAdjacencyEvidence> adjacencyEvidence;
    std::string rejectionReason;
    TopoDS_Shape expandedShape;
};

struct PipeShellSectionOption
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::add(), overloads
    // "Add(s, withContact, withCorrection)" and
    // "Add(s, v, withContact, withCorrection)" for per-profile placement/contact options.
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::add(), parses
    // "add(Profile, Location, WithContact, WithCorrection)"; C6-M4 keeps this as
    // request-local product-contract metadata when the OCCT location overload is not used.
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell(), maker history is consumed by
    // "makeElementShape(mkPipeShell, shapes, op)" after Add()/Build().
    TopoDS_Vertex location;
    bool hasLocation = false;
    PipeShellProfilePlacement profilePlacement = PipeShellProfilePlacement::OcctLocationOverload;
    bool withContact = false;
    bool withCorrection = false;
};

struct PipeShellTolerance
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::setTolerance(), parses "tol3d, boundTol,
    // tolAngular" and calls "SetTolerance(tol3d, boundTol, tolAngular)".
    double tol3d = 0.0;
    double boundTol = 0.0;
    double tolAngular = 0.0;
};

struct PipeShellOptions
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
    // ::Pipe::setupAlgorithm(), maps Transition to "SetTransitionMode", Mode=Fixed to
    // "SetMode(gp_Ax2(...))", Mode=Frenet to "SetMode(true)", Mode=Auxiliary to
    // "SetMode(TopoDS::Wire(auxshape), AuxiliaryCurvilinear.getValue())", and Mode=Binormal to
    // "SetMode(gp_Dir(bVec.x, bVec.y, bVec.z))".
    bool solid = true;
    PipeShellMode mode = PipeShellMode::Standard;
    int transition = 0;
    TopoDS_Shape auxiliarySpine;
    bool auxiliaryCurvilinear = true;
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
    // BRepOffsetAPI_MakePipeShellPyImp.cpp::setSpineSupport(), calls "SetMode(s)" and
    // returns the boolean result for the support surface/shape mode.
    TopoDS_Shape spineSupport;
    bool useSpineSupport = false;
    std::array<double, 3> binormal {{0.0, 0.0, 1.0}};
    std::vector<PipeShellSectionOption> sectionOptions;
    std::optional<PipeShellTolerance> tolerance;
    std::optional<PipeScalingLaw> scalingLaw;
    bool linearizeFaces = false;
    bool sewCaps = false;
};

ContinuousEdgeLedger expandContinuousEdgesForPipePath(
    const std::string& sourceObject,
    const TopoDS_Shape& sourceShape,
    const std::vector<std::string>& requestedSubnames
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementRuledSurface(), for two edges calls "BRepFill::Face(Edge, Edge)"
// and for two wires calls "BRepFill::Shell(Wire, Wire)" after applying "Automatic" / "Reversed"
// orientation; it then rebuilds edge relation because "Both BRepFill::Face() and Shell()
// modifies the original input edges".
NamedShapeBuild makeElementRuledSurfaceFromCurves(
    const std::string& owner,
    const std::array<RuledSurfaceCurveSource, 2>& sources,
    short orientation
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementLoft(), prepares profiles, calls "BRepOffsetAPI_ThruSections",
// "SetMaxDegree()", "CheckCompatibility(Standard_True)" and records MapperThruSections
// "GeneratedFace(s)", "FirstShape()" and "LastShape()" history through makeShapeWithElementMap().
NamedShapeBuild makeElementLoftFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool ruled,
    bool closed,
    int maxDegree,
    bool linearizeFaces = false
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementPipeShell(), first source is converted with "makeElementWires()",
// profiles are prepared with "prepareProfiles(shapes, 1)", then BRepOffsetAPI_MakePipeShell
// keeps Modified/Generated history for makeElementShape(mkPipeShell, shapes, op).
NamedShapeBuild makeElementPipeShellFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    const PipeShellOptions& options
);

NamedShapeBuild makeElementPipeShellFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool frenet,
    int transition,
    bool linearizeFaces = false
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementRevolve(), calls "BRepPrimAPI_MakeRevol" and then
// makeElementShape(mkRevol, base, Part::OpCodes::Revolve) so MapperMaker history is consumed.
NamedShapeBuild makeElementRevolveFromSource(
    const std::string& owner,
    const NamedShapeSource& source,
    const gp_Ax1& axis,
    double angleRadians
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementRevolution(), creates "BRepFeat_MakeRevol", calls
// "mkRevol.Init(base.getShape(), xp.Current(), supportface, axis, static_cast<int>(Mode), Modify)"
// and then "mkRevol.Perform(uptoface)" for UpToFirst/UpToLast/UpToFace PartDesign Revolved.
NamedShapeBuild makeElementRevolutionUntilFromSources(
    const std::string& owner,
    const NamedShapeSource& baseSource,
    const NamedShapeSource& profileSource,
    const gp_Ax1& axis,
    const TopoDS_Face& supportFace,
    const TopoDS_Face& upToFace,
    int revolMode,
    bool modify
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementFilledFace(), creates "BRepOffsetAPI_MakeFilling", expands compounds,
// finds a closed/preferred boundary wire or builds one from edges, fixes the boundary wire before
// Add(edge, ..., IsBound=true), adds remaining wire/edge constraints with IsBound=false and
// face/vertex constraints with "maker.Add", then calls makeElementShape(maker, _shapes, FilledFace).
FilledFaceBuild makeElementFilledFaceFromSources(
    const std::string& owner,
    const std::vector<FilledFaceSource>& boundarySources,
    const std::vector<NamedShapeSource>& historySources,
    const FilledFaceParams& params = FilledFaceParams {},
    const std::optional<FilledFaceSource>& initialSurface = std::nullopt,
    const std::vector<FilledFaceSupportSource>& supportSources = {},
    const std::vector<FilledFaceOrderSource>& orderSources = {}
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeaturePartImportBrep.cpp
// ::ImportBrep::execute() and FeaturePartImportStep.cpp::ImportStep::execute() call
// "aShape.importBrep(...)" / "aShape.importStep(...)" followed directly by
// "this->Shape.setValue(aShape)". Neither path calls TopoShape::mapSubElement(),
// makeShapeWithElementMap(), or a MapperHistory producer, so an imported shape exposes only its
// current FaceN/EdgeN/VertexN indexes until real producer-side mapping evidence exists.
NamedShape namedShapeForImportedShape(
    const std::string& owner,
    const TopoDS_Shape& shape
);

}  // namespace cad_core::part
