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
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapePyImp.cpp
// ::TopoShapePy::optimalBoundingBox(), exposes "BRepBndLib::AddOptimal" for Python oracle
// collection. Some maker outputs keep request-local triangulation that is wider than the
// geometric result, so cad-core can opt into a geometry-only AddOptimal call for those outputs.
nlohmann::json preciseBBoxForShape(const TopoDS_Shape& shape);
// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp
// ::TopoShape::getBoundBoxOptimal(), calls "BRepBndLib::AddOptimal(_Shape, bounds, false, false)"
// and "bounds.SetGap(0.0)". cad-core object "bbox" fields use that native expected oracle
// contract; mesh summaries can keep the display triangulation bbox from bboxForShape().
nlohmann::json objectBBoxForShape(const TopoDS_Shape& shape);
double volumeForShape(const TopoDS_Shape& shape);
// FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::getElementTypes(), exposes "InternalFace" elements for Sketch
// InternalShape. cad-core uses matching prefixes when exporting display mesh face
// ids, edgeSegments ids and vertexPoints ids.
nlohmann::json meshForShape(const TopoDS_Shape& shape,
                            const std::string& faceIdPrefix = "Face",
                            const std::string& edgeIdPrefix = "Edge",
                            const std::string& vertexIdPrefix = "Vertex",
                            double deflection = 0.1);
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
