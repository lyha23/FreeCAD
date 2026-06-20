#pragma once

#include <TopoDS_Shape.hxx>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp
// ::BuildPlateSurfacePy::PyInit(), parses "Degree", "NbPtsOnCur", "NbIter", "Tol2d",
// "Tol3d", "TolAng", "TolCurv" and "Anisotropy"; Tools.cpp::makeSurface() uses the same
// default tolerances while mapping request-local constraints into GeomPlate_BuildPlateSurface.
struct GeomPlateBuildParams
{
    int degree = 3;
    int nbPtsOnCur = 10;
    int nbIter = 3;
    double tol2d = 0.00001;
    double tol3d = 0.0001;
    double tolAng = 0.01;
    double tolCurv = 0.1;
    bool anisotropy = false;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp
// ::PlateSurfacePy::makeApprox(), parses "Tol3d", "MaxSegments", "MaxDegree",
// "MaxDistance", "CritOrder", "Continuity" and "EnlargeCoeff" before calling
// GeomPlate_MakeApprox on the transient GeomPlate_Surface.
struct GeomPlateApproximationParams
{
    double tol3d = 0.01;
    int maxSegments = 9;
    int maxDegree = 3;
    double maxDistance = 0.0001;
    int critOrder = 0;
    std::string continuity = "C1";
    double enlargeCoeff = 1.1;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp
// ::BuildPlateSurfacePy::PyInit(), parses "Surface" and calls "ptr->LoadInitSurface(handle)";
// ::BuildPlateSurfacePy::loadInitSurface() is the explicit "LoadInitSurface(handle)" entry.
struct GeomPlateSurfaceSource
{
    std::string objectName;
    std::string subname;
    std::string stableSubname;
    TopoDS_Shape shape;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp
// ::CurveConstraintPy::PyInit(), wraps a 3D GeometryCurvePy "Boundary" in GeomAdaptor_Curve
// and constructs GeomPlate_CurveConstraint(Boundary, Order, NbPts, TolDist, TolAng, TolCurv).
// Tools.cpp::Part::Tools::makeSurface() additionally routes Adaptor3d_CurveOnSurface through
// GeomPlate_CurveConstraint(..., 1 /*GeomAbs_G1*/, ...).
struct GeomPlateCurveConstraintSource
{
    std::string objectName;
    std::string subname;
    std::string stableSubname;
    TopoDS_Shape shape;
    std::optional<GeomPlateSurfaceSource> surface;
    int order = 0;
    int nbPts = 10;
    double tolDist = 0.0001;
    double tolAng = 0.01;
    double tolCurv = 0.1;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp
// ::PointConstraintPy::PyInit(), parses a Base::VectorPy "Point", "Order" and "TolDist" and
// constructs GeomPlate_PointConstraint(gp_Pnt(...), Order, TolDist).
struct GeomPlatePointConstraintSource
{
    std::array<double, 3> point {{0.0, 0.0, 0.0}};
    int order = 0;
    double tolDist = 0.0001;
};

struct GeomPlateSourceEvidence
{
    std::string kind;
    std::string objectName;
    std::string subname;
    std::string stableSubname;
    int order = 0;
    int nbPts = 0;
    double tolDist = 0.0;
    double tolAng = 0.0;
    double tolCurv = 0.0;
    std::array<double, 3> point {{0.0, 0.0, 0.0}};
    std::string surfaceObjectName;
    std::string surfaceSubname;
    std::string surfaceStableSubname;
};

struct GeomPlateBuildResult
{
    TopoDS_Shape shape;
    std::string errorCode;
    std::string errorMessage;
    bool isDone = false;
    bool approximationDone = false;
    std::string surfaceKind;
    std::string approximationSurfaceKind;
    std::optional<double> g0Error;
    std::optional<double> g1Error;
    std::optional<double> g2Error;
    std::optional<double> approxError;
    std::optional<double> criterionError;
    int curveConstraintCount = 0;
    int pointConstraintCount = 0;
    std::vector<GeomPlateSourceEvidence> sourceEvidence;
};

GeomPlateBuildResult makePartGeomPlateSurface(
    const std::vector<GeomPlateCurveConstraintSource>& curveConstraints,
    const std::vector<GeomPlatePointConstraintSource>& pointConstraints,
    const std::optional<GeomPlateSurfaceSource>& initialSurface,
    const GeomPlateBuildParams& buildParams,
    const GeomPlateApproximationParams& approximationParams
);

}  // namespace cad_core::part
