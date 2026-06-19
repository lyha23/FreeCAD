#pragma once

#include <gp_Pnt.hxx>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace cad_core::app
{
struct DocumentObject;
}

namespace cad_core::runtime
{
struct ComputeContext;
}

namespace cad_core::sketcher
{

struct SketchSegment
{
    std::size_t geometryIndex = 0;
    gp_Pnt start;
    gp_Pnt end;
    bool construction = false;
};

struct SketchPoint
{
    std::size_t geometryIndex = 0;
    gp_Pnt point;
    bool construction = false;
};

struct SketchCircle
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double radius = 0.0;
    bool construction = false;
};

struct SketchEllipse
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    bool construction = false;
};

struct SketchArc
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

struct SketchBSpline
{
    std::size_t geometryIndex = 0;
    int degree = 0;
    std::vector<gp_Pnt> poles;
    bool construction = false;
};

struct SketchBezier
{
    std::size_t geometryIndex = 0;
    std::vector<gp_Pnt> poles;
    std::vector<double> weights;
    bool construction = false;
};

struct SketchEllipseArc
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
// ::GeomArcOfHyperbola::Restore(), reads "MajorRadius/MinorRadius/AngleXU/StartAngle/EndAngle".
struct SketchHyperbolaArc
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
// ::GeomArcOfParabola::Restore(), reads "Focal/AngleXU/StartAngle/EndAngle".
struct SketchParabolaArc
{
    std::size_t geometryIndex = 0;
    gp_Pnt center;
    double focal = 0.0;
    double angle = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    bool construction = false;
};

struct SketchGeometrySet
{
    std::vector<SketchSegment> segments;
    std::vector<SketchPoint> points;
    std::vector<SketchCircle> circles;
    std::vector<SketchEllipse> ellipses;
    std::vector<SketchArc> arcs;
    std::vector<SketchEllipseArc> ellipseArcs;
    std::vector<SketchHyperbolaArc> hyperbolaArcs;
    std::vector<SketchParabolaArc> parabolaArcs;
    std::vector<SketchBSpline> bsplines;
    std::vector<SketchBezier> beziers;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp
// and SketchGeometry.cpp own geometry list parsing and geometry-family dispatch for SketchObject.
bool parseSketchGeometry(const nlohmann::json& geometry,
                         const app::DocumentObject& object,
                         runtime::ComputeContext& context,
                         SketchGeometrySet& parsed,
                         const std::string& propertyName = "Geometry");

std::vector<SketchSegment> profileSegments(const std::vector<SketchSegment>& segments);
std::vector<SketchPoint> profilePoints(const std::vector<SketchPoint>& points);
std::vector<SketchCircle> profileCircles(const std::vector<SketchCircle>& circles);
std::vector<SketchEllipse> profileEllipses(const std::vector<SketchEllipse>& ellipses);
std::vector<SketchArc> profileArcs(const std::vector<SketchArc>& arcs);
std::vector<SketchEllipseArc> profileEllipseArcs(const std::vector<SketchEllipseArc>& arcs);
std::vector<SketchHyperbolaArc> profileHyperbolaArcs(const std::vector<SketchHyperbolaArc>& arcs);
std::vector<SketchParabolaArc> profileParabolaArcs(const std::vector<SketchParabolaArc>& arcs);
std::vector<SketchBSpline> profileBSplines(const std::vector<SketchBSpline>& bsplines);
std::vector<SketchBezier> profileBeziers(const std::vector<SketchBezier>& beziers);

gp_Pnt pointAtAngle(const gp_Pnt& center, double radius, double angle);
gp_Pnt pointAtEllipseAngle(const gp_Pnt& center,
                           double majorRadius,
                           double minorRadius,
                           double angleXU,
                           double parameter);

} // namespace cad_core::sketcher
