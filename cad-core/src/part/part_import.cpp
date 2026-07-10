#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <Standard_Failure.hxx>

#include <filesystem>
#include <optional>
#include <string>

namespace cad_core::part
{

namespace
{

using part_feature_detail::publishPartShape;

struct ImportFile
{
    std::string name;
    std::filesystem::path path;
};

std::optional<ImportFile> readImportFile(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName
)
{
    const auto fileName = app::readString(object, "FileName");
    if (!fileName || fileName->empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            featureName + " FileName is not set",
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

}  // namespace

void executePartImportBrep(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportBrep.cpp
    // ::ImportBrep::execute(), reads PropertyString "FileName", checks "fi.isReadable()",
    // then calls "TopoShape aShape; aShape.importBrep(FileName.getValue())" before writing Shape.
    // cad-core keeps the file path as request input and only returns derived mesh/subshape data.
    if (!runtime::rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportBrep");
    if (!importFile) {
        return;
    }

    try {
        TopoDS_Shape shape;
        BRep_Builder builder;
        if (!BRepTools::Read(shape, importFile->path.string().c_str(), builder) || shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Failed to import BREP file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_brep"}, {"file_name", importFile->name}},
            part::namedShapeForImportedShape(object.name, shape),
            part_feature_detail::PartPublicResultFields {
                std::nullopt,
                false,
                part_feature_detail::PartBoundingBoxMode::UseTriangulation,
            }
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import BREP file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartImportStep(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportStep.cpp
    // ::ImportStep::execute(), reads PropertyString "FileName" and calls
    // "TopoShape aShape; aShape.importStep(FileName.getValue())". TopoShape::importStep()
    // in /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // uses "STEPControl_Reader", "ReadFile(...)", "TransferRoots()" and "OneShape()".
    if (!runtime::rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportStep");
    if (!importFile) {
        return;
    }

    try {
        STEPControl_Reader reader;
        if (reader.ReadFile(importFile->path.string().c_str()) != IFSelect_RetDone) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Error in reading STEP file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        reader.TransferRoots();
        const TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Imported STEP shape is null",
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_step"}, {"file_name", importFile->name}},
            part::namedShapeForImportedShape(object.name, shape)
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import STEP file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

void executePartImportIges(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeaturePartImportIges.cpp
    // ::ImportIges::execute(), reads PropertyString "FileName" and calls
    // "TopoShape aShape; aShape.importIges(FileName.getValue())". TopoShape::importIges()
    // in /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
    // calls "IGESControl_Controller::Init()", sets "SetReadVisible(Standard_True)",
    // then uses "ReadFile(...)", "TransferRoots()" and "OneShape()".
    if (!runtime::rejectUnsupportedProperties(object, context, {"FileName"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto importFile = readImportFile(object, context, "ImportIges");
    if (!importFile) {
        return;
    }

    try {
        IGESControl_Controller::Init();
        IGESControl_Reader reader;
        reader.SetReadVisible(Standard_True);
        if (reader.ReadFile(importFile->path.string().c_str()) != IFSelect_RetDone) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Error in reading IGES file " + importFile->name,
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        reader.ClearShapes();
        reader.TransferRoots();
        const TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Imported IGES shape is null",
                object.name,
                "FileName",
                "runtime"
            );
            context.objects[object.name] = {{"status", "error"}};
            return;
        }

        publishPartShape(
            object,
            context,
            shape,
            {{"primitive", "import_iges"}, {"file_name", importFile->name}},
            part::namedShapeForImportedShape(object.name, shape)
        );
    }
    catch (const Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString() != nullptr ? failure.GetMessageString()
                                                  : "Failed to import IGES file",
            object.name,
            "FileName",
            "runtime"
        );
        context.objects[object.name] = {{"status", "error"}};
    }
}

}  // namespace cad_core::part
