#pragma once

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace cad_core::geometry {

enum class ShapeFileFormat {
    Brep,
    Step,
    Stl,
};

nlohmann::json bboxForShape(const TopoDS_Shape& shape);
double volumeForShape(const TopoDS_Shape& shape);
nlohmann::json meshForShape(const TopoDS_Shape& shape);
ShapeFileFormat shapeFileFormatFromString(const std::string& format);
std::string shapeFileFormatName(ShapeFileFormat format);
void exportShapeFile(const TopoDS_Shape& shape,
                     const std::filesystem::path& path,
                     ShapeFileFormat format,
                     double stlDeflection = 0.01);
std::string kernelVersion();

}  // namespace cad_core::geometry
