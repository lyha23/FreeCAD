#include "cad_core/part/shape_exporter.h"

#include <APIHeaderSection_MakeHeader.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Version.hxx>
#include <StlAPI_Writer.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_FormatVersion.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace cad_core::part {

namespace {

double defaultAngularDeflection(double linearTolerance)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::defaultAngularDeflection(), "default to at most 0.1 radians" and
    // "linearTolerance * 5 + 0.005".
    return std::min(0.1, linearTolerance * 5 + 0.005);
}

void assertWritableShape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        throw std::runtime_error("Cannot export a null shape");
    }
}

void exportBrepFile(const TopoDS_Shape& shape, const std::filesystem::path& path)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::exportBrep(), writes with BRepTools::Write(...,
    // TopTools_FormatVersion_VERSION_1).
    if (!BRepTools::Write(shape,
                          path.string().c_str(),
                          Standard_False,
                          Standard_False,
                          TopTools_FormatVersion_VERSION_1)) {
        throw std::runtime_error("Writing of BREP failed: " + path.string());
    }
}

void exportStepFile(const TopoDS_Shape& shape, const std::filesystem::path& path)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::exportStep(), "Do not write out any assembly information" and
    // transfers with STEPControl_AsIs.
    Interface_Static::SetIVal("write.step.assembly", 0);

    STEPControl_Writer writer;
    if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) {
        throw std::runtime_error("Error in transferring STEP");
    }

    APIHeaderSection_MakeHeader makeHeader(writer.Model());
    makeHeader.SetAuthorValue(1, new TCollection_HAsciiString("FreeCAD"));
    makeHeader.SetOrganizationValue(1, new TCollection_HAsciiString("FreeCAD"));
    makeHeader.SetOriginatingSystem(new TCollection_HAsciiString("FreeCAD"));
    makeHeader.SetDescriptionValue(1, new TCollection_HAsciiString("FreeCAD Model"));

    if (writer.Write(path.string().c_str()) != IFSelect_RetDone) {
        throw std::runtime_error("Writing of STEP failed: " + path.string());
    }
}

void exportStlFile(const TopoDS_Shape& shape, const std::filesystem::path& path, double deflection)
{
    if (deflection <= 0.0) {
        throw std::runtime_error("STL deflection must be positive");
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // ::TopoShape::exportStl(), meshes with deflection and defaultAngularDeflection(deflection)
    // before StlAPI_Writer::Write().
    BRepMesh_IncrementalMesh mesh(shape,
                                  deflection,
                                  Standard_False,
                                  defaultAngularDeflection(deflection),
                                  true);
    mesh.Perform();

    StlAPI_Writer writer;
    if (!writer.Write(shape, path.string().c_str())) {
        throw std::runtime_error("Writing of STL failed: " + path.string());
    }
}

struct ScopedTempPath {
    explicit ScopedTempPath(std::filesystem::path value)
        : path(std::move(value))
    {
    }

    ~ScopedTempPath()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};

}  // namespace

nlohmann::json bboxForShape(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App
    // /TopoShapePyImp.cpp::TopoShapePy::optimalBoundingBox(), calls
    // "BRepBndLib::AddOptimal(shape, bounds, ...)" and then "bounds.SetGap(0.0)" before
    // exposing the bounds through Python.
    BRepBndLib::AddOptimal(shape, box, Standard_True, Standard_False);
    box.SetGap(0.0);
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

nlohmann::json meshForShape(const TopoDS_Shape& shape, const std::string& faceIdPrefix)
{
    BRepMesh_IncrementalMesh mesher(shape, 0.1);
    mesher.Perform();

    nlohmann::json vertices = nlohmann::json::array();
    nlohmann::json triangles = nlohmann::json::array();
    nlohmann::json faceIds = nlohmann::json::array();
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

    int faceIndex = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++faceIndex;
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
            faceIds.push_back(faceIdPrefix + std::to_string(faceIndex));
        }
    }

    return {
        {"vertices", vertices},
        {"triangles", triangles},
        {"faceIds", faceIds},
        {"summary",
         {
             {"vertex_count", vertices.size()},
             {"triangle_count", triangles.size()},
             {"bbox", bboxForShape(shape)},
             {"volume", volumeForShape(shape)},
         }},
    };
}

ShapeFileFormat shapeFileFormatFromString(const std::string& format)
{
    if (format == "brep" || format == "brp") {
        return ShapeFileFormat::Brep;
    }
    if (format == "step" || format == "stp") {
        return ShapeFileFormat::Step;
    }
    if (format == "stl") {
        return ShapeFileFormat::Stl;
    }
    throw std::runtime_error("Unsupported export format " + format);
}

std::string shapeFileFormatName(ShapeFileFormat format)
{
    switch (format) {
        case ShapeFileFormat::Brep:
            return "brep";
        case ShapeFileFormat::Step:
            return "step";
        case ShapeFileFormat::Stl:
            return "stl";
    }
    throw std::runtime_error("Unsupported export format");
}

std::string shapeFileFormatExtension(ShapeFileFormat format)
{
    switch (format) {
        case ShapeFileFormat::Brep:
            return "brep";
        case ShapeFileFormat::Step:
            return "step";
        case ShapeFileFormat::Stl:
            return "stl";
    }
    throw std::runtime_error("Unsupported export format");
}

std::string shapeFileFormatContentType(ShapeFileFormat format)
{
    switch (format) {
        case ShapeFileFormat::Brep:
            return "application/vnd.opencascade.brep";
        case ShapeFileFormat::Step:
            return "application/step";
        case ShapeFileFormat::Stl:
            return "model/stl";
    }
    throw std::runtime_error("Unsupported export format");
}

std::vector<std::string> supportedShapeFileFormats()
{
    return {
        shapeFileFormatName(ShapeFileFormat::Brep),
        shapeFileFormatName(ShapeFileFormat::Step),
        shapeFileFormatName(ShapeFileFormat::Stl),
    };
}

void exportShapeFile(const TopoDS_Shape& shape,
                     const std::filesystem::path& path,
                     ShapeFileFormat format,
                     double stlDeflection)
{
    assertWritableShape(shape);
    if (path.empty()) {
        throw std::runtime_error("Export path must not be empty");
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    switch (format) {
        case ShapeFileFormat::Brep:
            exportBrepFile(shape, path);
            return;
        case ShapeFileFormat::Step:
            exportStepFile(shape, path);
            return;
        case ShapeFileFormat::Stl:
            exportStlFile(shape, path, stlDeflection);
            return;
    }
    throw std::runtime_error("Unsupported export format");
}

std::string exportShapeBuffer(const TopoDS_Shape& shape,
                              ShapeFileFormat format,
                              double stlDeflection)
{
    const auto tempDir = std::filesystem::temp_directory_path();
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto path = tempDir / ("cad-core-export-" + std::to_string(distribution(generator)) + "."
                                    + shapeFileFormatExtension(format));
        if (std::filesystem::exists(path)) {
            continue;
        }

        ScopedTempPath scoped(path);
        exportShapeFile(shape, scoped.path, format, stlDeflection);

        std::ifstream input(scoped.path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Unable to read exported file " + scoped.path.string());
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    throw std::runtime_error("Unable to allocate a temporary export file");
}

std::string kernelVersion()
{
    return std::string("OCCT ") + OCC_VERSION_COMPLETE;
}

}  // namespace cad_core::part
