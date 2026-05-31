#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace cad_core::geometry {

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

}  // namespace cad_core::geometry
