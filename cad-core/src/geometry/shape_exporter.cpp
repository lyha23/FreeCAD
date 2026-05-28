#include "cad_core/geometry/shape_exporter.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Version.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <map>
#include <sstream>

namespace cad_core::geometry {

nlohmann::json bboxForShape(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, Standard_True, Standard_False);
    double xmin = 0.0;
    double ymin = 0.0;
    double zmin = 0.0;
    double xmax = 0.0;
    double ymax = 0.0;
    double zmax = 0.0;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    auto clean = [](double value) {
        constexpr double eps = 1e-6;
        if (std::abs(value) < eps) {
            return 0.0;
        }
        const double rounded = std::round(value);
        return std::abs(value - rounded) < eps ? rounded : value;
    };

    return {
        {"min", {clean(xmin), clean(ymin), clean(zmin)}},
        {"max", {clean(xmax), clean(ymax), clean(zmax)}},
    };
}

double volumeForShape(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

namespace {

std::string pointKey(const gp_Pnt& point)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(9);
    out << point.X() << ',' << point.Y() << ',' << point.Z();
    return out.str();
}

}  // namespace

nlohmann::json meshForShape(const TopoDS_Shape& shape)
{
    BRepMesh_IncrementalMesh mesher(shape, 0.1);
    mesher.Perform();

    nlohmann::json vertices = nlohmann::json::array();
    nlohmann::json triangles = nlohmann::json::array();
    std::map<std::string, int> vertexIndexByPoint;

    auto addVertex = [&](const gp_Pnt& point) {
        const auto key = pointKey(point);
        const auto it = vertexIndexByPoint.find(key);
        if (it != vertexIndexByPoint.end()) {
            return it->second;
        }
        const int index = static_cast<int>(vertices.size());
        vertices.push_back({point.X(), point.Y(), point.Z()});
        vertexIndexByPoint[key] = index;
        return index;
    };

    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }

        for (int triangleIndex = 1; triangleIndex <= triangulation->NbTriangles(); ++triangleIndex) {
            int n1 = 0;
            int n2 = 0;
            int n3 = 0;
            triangulation->Triangle(triangleIndex).Get(n1, n2, n3);
            gp_Pnt p1 = triangulation->Node(n1).Transformed(location.Transformation());
            gp_Pnt p2 = triangulation->Node(n2).Transformed(location.Transformation());
            gp_Pnt p3 = triangulation->Node(n3).Transformed(location.Transformation());
            triangles.push_back({addVertex(p1), addVertex(p2), addVertex(p3)});
        }
    }

    return {
        {"vertices", vertices},
        {"triangles", triangles},
        {"summary",
         {
             {"vertex_count", vertices.size()},
             {"triangle_count", triangles.size()},
             {"bbox", bboxForShape(shape)},
             {"volume", volumeForShape(shape)},
         }},
    };
}

std::string kernelVersion()
{
    return std::string("OCCT ") + OCC_VERSION_COMPLETE;
}

}  // namespace cad_core::geometry

