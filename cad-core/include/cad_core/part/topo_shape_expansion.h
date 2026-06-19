#pragma once

// Part-layer TopoShapeExpansion import element-map facade.
#include "cad_core/part/topo_shape.h"

#include <TopoDS_Edge.hxx>

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

struct RuledSurfaceEdgeSource
{
    std::string objectName;
    TopoDS_Edge edge;
    std::vector<std::string> stableEdgeNames;
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

struct FilledFaceBuild
{
    TopoDS_Shape shape;
    std::optional<NamedShape> namedShape;
    std::string error;
    std::string boundaryMode;
    std::vector<FilledFaceBoundaryEvidence> boundarySources;
    int boundaryEdgeCount = 0;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementRuledSurface(), for two edges calls "BRepFill::Face(Edge, Edge)"
// after applying "Automatic" / "Reversed" orientation and then rebuilds edge relation because
// "Both BRepFill::Face() and Shell() modifies the original input edges".
NamedShapeBuild makeElementRuledSurfaceFromEdges(
    const std::string& owner,
    const std::array<RuledSurfaceEdgeSource, 2>& sources,
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
    int maxDegree
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementPipeShell(), first source is converted with "makeElementWires()",
// profiles are prepared with "prepareProfiles(shapes, 1)", then BRepOffsetAPI_MakePipeShell
// keeps Modified/Generated history for makeElementShape(mkPipeShell, shapes, op).
NamedShapeBuild makeElementPipeShellFromSources(
    const std::string& owner,
    const std::vector<NamedShapeSource>& sources,
    bool solid,
    bool frenet,
    int transition
);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
// ::TopoShape::makeElementFilledFace(), creates "BRepOffsetAPI_MakeFilling", expands compounds,
// finds a closed/preferred boundary wire or builds one from edges, fixes the boundary wire before
// Add(edge, ..., IsBound=true), then calls makeElementShape(maker, _shapes, FilledFace).
FilledFaceBuild makeElementFilledFaceFromSources(
    const std::string& owner,
    const std::vector<FilledFaceSource>& boundarySources,
    const std::vector<NamedShapeSource>& historySources,
    const FilledFaceDefaultParams& params = FilledFaceDefaultParams {}
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
