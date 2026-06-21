#pragma once

// Part-layer TopoShapeExpansion import element-map facade.
#include "cad_core/part/topo_shape.h"

#include <GeomAbs_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

struct ImportElementMapSource
{
    std::string format;
    std::string fileName;
};

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

struct FilledFaceDefaultParams
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
    bool hasSupport = false;
    std::string supportObject;
    std::string supportSubname;
    std::string supportStableSubname;
    bool hasOrder = false;
    std::string order;
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
    int boundaryEdgeCount = 0;
    int supportFaceCount = 0;
    int orderCount = 0;
};

enum class PipeShellMode
{
    Standard,
    Fixed,
    Frenet,
    Auxiliary,
    Binormal,
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
    std::array<double, 3> binormal {{0.0, 0.0, 1.0}};
    bool linearizeFaces = false;
    bool sewCaps = false;
};

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
// Add(edge, ..., IsBound=true), then calls makeElementShape(maker, _shapes, FilledFace).
FilledFaceBuild makeElementFilledFaceFromSources(
    const std::string& owner,
    const std::vector<FilledFaceSource>& boundarySources,
    const std::vector<NamedShapeSource>& historySources,
    const FilledFaceDefaultParams& params = FilledFaceDefaultParams {},
    const std::optional<FilledFaceSource>& initialSurface = std::nullopt,
    const std::vector<FilledFaceSupportSource>& supportSources = {},
    const std::vector<FilledFaceOrderSource>& orderSources = {}
);

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp::TopoShape::read(),
// dispatches to "importStep", "importIges" and "importBrep"; ImportStep::execute() then stores
// the resulting TopoShape in PropertyPartShape "Shape". cad-core keeps the same request-local
// imported TopoDS_Shape and records object-qualified element aliases so LinkSub references can
// survive a recompute without persisting BREP.
NamedShape namedShapeForImportedShape(
    const std::string& owner,
    const TopoDS_Shape& shape,
    const ImportElementMapSource& source
);

}  // namespace cad_core::part
