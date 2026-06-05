#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

// Part/App export facade aligned with FreeCAD TopoShape export/tessellation entry points.
namespace cad_core::part {

enum class ShapeFileFormat {
    Brep,
    Step,
    Stl,
};

nlohmann::json bboxForShape(const TopoDS_Shape& shape);
double volumeForShape(const TopoDS_Shape& shape);
// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::getElementTypes(), exposes "InternalFace" elements for Sketch
// InternalShape. cad-core uses the same prefix when exporting display mesh face ids.
nlohmann::json meshForShape(const TopoDS_Shape& shape, const std::string& faceIdPrefix = "Face");
ShapeFileFormat shapeFileFormatFromString(const std::string& format);
std::string shapeFileFormatName(ShapeFileFormat format);
std::string shapeFileFormatExtension(ShapeFileFormat format);
std::string shapeFileFormatContentType(ShapeFileFormat format);
std::vector<std::string> supportedShapeFileFormats();
void exportShapeFile(const TopoDS_Shape& shape,
                     const std::filesystem::path& path,
                     ShapeFileFormat format,
                     double stlDeflection = 0.01);
std::string exportShapeBuffer(const TopoDS_Shape& shape,
                              ShapeFileFormat format,
                              double stlDeflection = 0.01);
std::string kernelVersion();

}  // namespace cad_core::part

namespace cad_core::geometry {

using cad_core::part::ShapeFileFormat;
using cad_core::part::bboxForShape;
using cad_core::part::exportShapeBuffer;
using cad_core::part::exportShapeFile;
using cad_core::part::kernelVersion;
using cad_core::part::meshForShape;
using cad_core::part::shapeFileFormatContentType;
using cad_core::part::shapeFileFormatExtension;
using cad_core::part::shapeFileFormatFromString;
using cad_core::part::shapeFileFormatName;
using cad_core::part::supportedShapeFileFormats;
using cad_core::part::volumeForShape;

}  // namespace cad_core::geometry
