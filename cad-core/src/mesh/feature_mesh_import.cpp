#include "cad_core/mesh/feature_mesh_import.h"

#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <Standard_Failure.hxx>
#include <StlAPI_Reader.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

namespace cad_core::mesh
{

namespace
{

struct ImportFile
{
    std::string name;
    std::filesystem::path path;
};

TopoDS_Shape applyGlobalPlacement(
    const app::DocumentObject& object,
    const runtime::ComputeContext& context,
    const TopoDS_Shape& shape
)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt == context.globalPlacements.end()) {
        return shape;
    }
    return base::transformShape(shape, placementIt->second);
}

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

std::string shapeLabel(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_SHELL:
            return "occt_shell";
        case TopAbs_FACE:
            return "occt_face";
        case TopAbs_WIRE:
            return "occt_wire";
        case TopAbs_EDGE:
            return "occt_edge";
        case TopAbs_VERTEX:
            return "occt_vertex";
        default:
            return "occt_shape";
    }
}

std::optional<ImportFile> readImportFile(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto fileName = app::readString(object, "FileName");
    if (!fileName || fileName->empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Mesh::Import FileName is not set",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    std::error_code existsError;
    const std::filesystem::path filePath(*fileName);
    if (!std::filesystem::exists(filePath, existsError)
        || !std::filesystem::is_regular_file(filePath, existsError)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Cannot open file " + *fileName,
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    return ImportFile {*fileName, filePath};
}

void publishImportedStl(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& localShape,
    const std::string& fileName
)
{
    const TopoDS_Shape shape = applyGlobalPlacement(object, context, localShape);
    context.shapes[object.name] = runtime::ShapeValue {runtime::ShapeValue::Kind::Mesh, shape};
    context.mesh[object.name] = cad_core::part::meshForShape(shape);
    context.subshapes[object.name] = part::subshapeMapForShape(shape);
    context.namedShapes[object.name] = part::indexedNamedShapeForObject(object.name, shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"primitive", "import_stl"},
        {"shape", shapeLabel(shape)},
        {"file_name", fileName},
        {"bbox", cad_core::part::objectBBoxForShape(shape)},
        {"volume", cad_core::part::volumeForShape(shape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
}

} // namespace

void executeMeshImport(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Mesh/Init.py registers
    // "STL Mesh (*.stl *.STL *.ast *.AST)" with the Mesh module.
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Mesh/App/FeatureMeshImport.cpp
    // ::Import::execute() reads PropertyFile "FileName" and calls "apcKernel->load(...)";
    // MeshIO.cpp::MeshInput::LoadSTL() dispatches ASCII or binary STL. cad-core keeps
    // the same Mesh::Import object contract, then normalizes STL facets into a request-local
    // OCCT triangular-face shape via StlAPI_Reader so the existing mesh/subshape/named-shape
    // response path can display and pick it without persisting STL or BREP content.
    if (!runtime::rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context);
    if (!importFile) {
        return;
    }

    const std::string extension = lowerExtension(importFile->path);
    if (extension != ".stl" && extension != ".ast") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_geometry",
            "Mesh::Import currently supports STL/AST files only",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    try {
        TopoDS_Shape shape;
        StlAPI_Reader reader;
        if (!reader.Read(shape, importFile->path.string().c_str()) || shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to import STL file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishImportedStl(object, context, shape, importFile->name);
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import STL file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace cad_core::mesh
