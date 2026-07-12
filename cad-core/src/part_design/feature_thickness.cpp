#include "cad_core/part_design/feature_thickness.h"

#include "feature_dress_up_support.h"

#include "cad_core/part/part_boolean.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::part_design
{

namespace
{

using detail::applyDressUpRefine;
using detail::cacheDressUpAddSubShape;
using detail::DressUpBase;
using detail::DressUpResult;
using detail::publishDressUpResult;
using detail::readBoolProperty;
using detail::readEnumProperty;
using detail::readNumberProperty;
using detail::resolveDressUpBase;
using detail::selectedThicknessFaces;
using detail::solidSubshapes;
using detail::ThicknessFaceSelection;
using detail::ThicknessSolidBuild;
using detail::ThicknessSolidFaces;

std::optional<short> thicknessModeIndex(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    const std::string mode = readEnumProperty(object, "Mode", {"Skin", "Pipe", "RectoVerso"}, "Skin");
    if (mode == "Skin") {
        return 0;
    }
    if (mode == "Pipe") {
        return 1;
    }
    if (mode == "RectoVerso") {
        return 2;
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Thickness Mode must be Skin, Pipe or RectoVerso",
        object.name,
        "Mode"
    );
    return std::nullopt;
}

std::optional<short> thicknessJoinIndex(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), PartDesign exposes only "Arc" and "Intersection"; enum value 1 is
    // remapped to OCCT/Part JoinType::intersection because "we do not offer tangent join type".
    const std::string join = readEnumProperty(object, "Join", {"Arc", "Intersection"}, "Arc");
    if (join == "Arc") {
        return 0;
    }
    if (join == "Intersection") {
        return 2;
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_property",
        "Thickness Join must be Arc or Intersection",
        object.name,
        "Join"
    );
    return std::nullopt;
}

std::string thicknessModeName(short mode)
{
    switch (mode) {
        case 0:
            return "Skin";
        case 1:
            return "Pipe";
        case 2:
            return "RectoVerso";
        default:
            return "Skin";
    }
}

std::string thicknessJoinName(short join)
{
    return join == 2 ? "Intersection" : "Arc";
}

std::optional<int> ancestorSolidIndexForFace(
    const TopoDS_Shape& baseShape,
    const std::vector<TopoDS_Shape>& solids,
    const TopoDS_Face& face
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
    // ::Thickness::execute(), after resolving each selected face, calls
    // "TopShape.findAncestor(face, TopAbs_SOLID)" and groups close faces by that solid index.
    TopTools_IndexedDataMapOfShapeListOfShape faceSolids;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_FACE, TopAbs_SOLID, faceSolids);
    if (!faceSolids.Contains(face)) {
        return std::nullopt;
    }

    const TopTools_ListOfShape& ancestors = faceSolids.FindFromKey(face);
    for (TopTools_ListIteratorOfListOfShape it(ancestors); it.More(); it.Next()) {
        for (std::size_t index = 0; index < solids.size(); ++index) {
            if (solids[index].IsSame(it.Value())) {
                return static_cast<int>(index + 1U);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::map<int, ThicknessSolidFaces>> thicknessFacesBySolid(
    const DressUpBase& base,
    const ThicknessFaceSelection& selection,
    const std::vector<TopoDS_Shape>& solids,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    std::map<int, ThicknessSolidFaces> result;
    for (std::size_t index = 0; index < selection.faces.size(); ++index) {
        const auto solidIndex = ancestorSolidIndexForFace(base.shape, solids, selection.faces[index]);
        if (!solidIndex) {
            // FreeCAD logs and ignores non-solid faces. If all selected faces are ignored the
            // subsequent maker path fails; cad-core exposes that as a structured diagnostic below.
            continue;
        }
        result[*solidIndex].faces.push_back(selection.faces[index]);
        result[*solidIndex].selectedFaceSubnames.push_back(selection.selectedFaceSubnames[index]);
    }
    if (result.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Thickness selected faces do not belong to any solid",
            object.name,
            "Base"
        );
        return std::nullopt;
    }
    return result;
}

std::optional<DressUpResult> buildThickness(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto selection = selectedThicknessFaces(*base, object, context);
    if (!selection) {
        return std::nullopt;
    }

    const double value = readNumberProperty(object, "Value", 1.0);
    const bool reversed = readBoolProperty(object, "Reversed", true);
    const bool intersection = readBoolProperty(object, "Intersection");
    const double thickness = (reversed ? -1.0 : 1.0) * value;
    const auto modeIndex = thicknessModeIndex(object, context);
    const auto joinIndex = thicknessJoinIndex(object, context);
    if (!modeIndex || !joinIndex) {
        return std::nullopt;
    }

    part::NamedShapeSource baseSource {
        base->link.object,
        base->shape,
        base->namedShape ? &*base->namedShape : nullptr
    };
    if (selection->faces.empty() || std::abs(thickness) <= 2.0 * Precision::Confusion()) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
        // ::Thickness::execute(), if Base has no selected subelements, calls positionByBaseFeature()
        // and stores the unchanged TopShape; if fabs(thickness) <= 2*tol, no thick-solid maker runs.
        part::NamedShape namedShape
            = part::namedShapeForPreservedSources(object.name, base->shape, {baseSource});
        DressUpResult result {
            "thickness",
            base->link.object,
            *base,
            base->shape,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        result.selection = selection->evidence;
        result.parameters = {
            {"value", value},
            {"thickness", thickness},
            {"mode", thicknessModeName(*modeIndex)},
            {"join", thicknessJoinName(*joinIndex)},
            {"reversed", reversed},
            {"intersection", intersection},
            {"selected_faces", selection->selectedFaceSubnames},
            {"build_mode",
             selection->faces.empty() ? "copy_no_face_selection" : "copy_zero_thickness"},
        };
        return result;
    }

    const std::vector<TopoDS_Shape> solids = solidSubshapes(base->shape);
    if (solids.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Thickness Base has no solid",
            object.name,
            "Base"
        );
        return std::nullopt;
    }

    const auto closeFaces = thicknessFacesBySolid(*base, *selection, solids, object, context);
    if (!closeFaces) {
        return std::nullopt;
    }

    try {
        std::vector<ThicknessSolidBuild> solidBuilds;
        solidBuilds.reserve(solids.size());
        std::vector<int> processedSolids;
        nlohmann::json selectedFacesBySolid = nlohmann::json::object();
        for (std::size_t solidOffset = 0; solidOffset < solids.size(); ++solidOffset) {
            const int solidIndex = static_cast<int>(solidOffset + 1U);
            const auto facesIt = closeFaces->find(solidIndex);
            if (facesIt == closeFaces->end() || facesIt->second.faces.empty()) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
                // ::Thickness::execute(), passes an empty face list for solids without selected
                // close faces; TopoShape::makeElementThickSolid() then throws "Null input shape".
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    "Thickness multi-solid Base requires selected close faces for every solid",
                    object.name,
                    "Base"
                );
                return std::nullopt;
            }

            TopTools_ListOfShape removeFaces;
            for (const auto& face : facesIt->second.faces) {
                removeFaces.Append(face);
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
            // ::TopoShape::makeElementThickSolid(), calls "mkThick.MakeThickSolidByJoin(shape,
            // remFace, offset, tol, BRepOffset_Mode(offsetMode), intersection, selfInter,
            // GeomAbs_JoinType(join))" and then makeElementShape(mkThick, shape, op).
            BRepOffsetAPI_MakeThickSolid maker;
            maker.MakeThickSolidByJoin(
                solids[solidOffset],
                removeFaces,
                thickness,
                Precision::Confusion(),
                static_cast<BRepOffset_Mode>(*modeIndex),
                intersection ? Standard_True : Standard_False,
                Standard_False,
                static_cast<GeomAbs_JoinType>(*joinIndex)
            );
            TopoDS_Shape solidResultShape = maker.Shape();
            if (solidResultShape.IsNull()) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    "Thickness operation produced a null shape",
                    object.name,
                    "Base"
                );
                return std::nullopt;
            }

            part::NamedShape solidNamedShape = part::namedShapeForMakerHistory(
                object.name + ".Solid" + std::to_string(solidIndex),
                solidResultShape,
                {baseSource},
                maker
            );
            solidBuilds.push_back(ThicknessSolidBuild {solidIndex, solidResultShape, solidNamedShape});
            processedSolids.push_back(solidIndex);
            selectedFacesBySolid[std::to_string(solidIndex)] = facesIt->second.selectedFaceSubnames;
        }

        TopoDS_Shape resultShape;
        part::NamedShape namedShape;
        if (solidBuilds.size() == 1U) {
            resultShape = solidBuilds.front().shape;
            namedShape = solidBuilds.front().namedShape;
            namedShape.owner = object.name;
            namedShape.shape = resultShape;
        }
        else {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
            // ::Thickness::execute(), after per-solid makeElementThickSolid(), calls
            // "result.makeElementFuse(shapes)" when more than one result shape was produced.
            std::vector<part::NamedShapeSource> fuseSources;
            fuseSources.reserve(solidBuilds.size());
            for (const auto& solidBuild : solidBuilds) {
                fuseSources.push_back(
                    part::NamedShapeSource {
                        object.name + ".Solid" + std::to_string(solidBuild.solidIndex),
                        solidBuild.shape,
                        &solidBuild.namedShape
                    }
                );
            }
            const auto fuseBuild = part::makeElementBooleanFromSources(
                object.name,
                fuseSources,
                part::BooleanOperation::Fuse
            );
            if (!fuseBuild.error.empty() || fuseBuild.shape.IsNull() || !fuseBuild.namedShape) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    fuseBuild.error.empty() ? "Thickness multi-solid fuse failed" : fuseBuild.error,
                    object.name,
                    "Base"
                );
                return std::nullopt;
            }
            resultShape = fuseBuild.shape;
            namedShape = *fuseBuild.namedShape;
        }

        DressUpResult result {
            "thickness",
            base->link.object,
            *base,
            resultShape,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        result.selection = selection->evidence;
        result.parameters = {
            {"value", value},
            {"thickness", thickness},
            {"mode", thicknessModeName(*modeIndex)},
            {"join", thicknessJoinName(*joinIndex)},
            {"reversed", reversed},
            {"intersection", intersection},
            {"selected_faces", selection->selectedFaceSubnames},
            {"selected_faces_by_solid", selectedFacesBySolid},
            {"solid_count", static_cast<int>(solids.size())},
            {"processed_solids", processedSolids},
            {"build_mode", solidBuilds.size() > 1U ? "thick_solid_multi_fuse" : "thick_solid"},
        };
        return result;
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            "Base"
        );
        return std::nullopt;
    }
}

}  // namespace

void executeThickness(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp::Thickness::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementThickSolid()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base",
             "BaseFeature",
             "SupportTransform",
             "Value",
             "Mode",
             "Join",
             "Reversed",
             "Intersection",
             "Refine",
             "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildThickness(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

}  // namespace cad_core::part_design
