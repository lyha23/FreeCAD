#include "cad_core/part_design/feature_chamfer.h"

#include "feature_dress_up_support.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/runtime/producer_trace_scope.h"

#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRep_Tool.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <cmath>
#include <optional>

namespace cad_core::part_design
{

namespace
{

using detail::applyDressUpRefine;
using detail::cacheDressUpAddSubShape;
using detail::publishDressUpResult;
using detail::readBoolProperty;
using detail::readEnumProperty;
using detail::readNumberProperty;
using detail::resolveDressUpBase;
using detail::selectedDressUpEdges;

using detail::DressUpResult;

std::optional<TopoDS_Face> ancestorFaceForChamfer(
    const TopoDS_Shape& baseShape,
    const TopoDS_Edge& edge,
    bool flipDirection,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementChamfer(), uses findAncestorShape(edge, TopAbs_FACE), or the
    // last ancestor face when "FlipDirection" is enabled.
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(baseShape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    if (!edgeFaces.Contains(edge)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Chamfer edge has no adjacent face",
            object.name,
            "Base"
        );
        return std::nullopt;
    }

    const TopTools_ListOfShape& faces = edgeFaces.FindFromKey(edge);
    if (faces.IsEmpty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Chamfer edge has no adjacent face",
            object.name,
            "Base"
        );
        return std::nullopt;
    }

    return TopoDS::Face(flipDirection ? faces.Last() : faces.First());
}

std::optional<DressUpResult> buildChamfer(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto base = resolveDressUpBase(object, context);
    if (!base) {
        return std::nullopt;
    }
    const auto edges = selectedDressUpEdges(*base, object, context);
    if (!edges) {
        return std::nullopt;
    }

    const std::string chamferType = readEnumProperty(
        object,
        "ChamferType",
        {"Equal distance", "Two distances", "Distance and Angle"},
        "Equal distance"
    );
    const double size = readNumberProperty(object, "Size", 1.0);
    double size2 = readNumberProperty(object, "Size2", 1.0);
    const double angle = readNumberProperty(object, "Angle", 45.0);
    if (size <= Precision::Confusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Chamfer Size must be greater than zero",
            object.name,
            "Size"
        );
        return std::nullopt;
    }
    if (chamferType == "Two distances" && size2 <= Precision::Confusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Chamfer Size2 must be greater than zero",
            object.name,
            "Size2"
        );
        return std::nullopt;
    }
    if (chamferType == "Distance and Angle" && (angle <= Precision::Confusion() || angle >= 180.0)) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_angle",
            "Chamfer Angle must be greater than 0 and less than 180",
            object.name,
            "Angle"
        );
        return std::nullopt;
    }

    const bool flipDirection = readBoolProperty(object, "FlipDirection");
    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementChamfer(), creates BRepFilletAPI_MakeChamfer and adds
        // selected edges with Equal distance, Two distances, or Distance and Angle parameters.
        BRepFilletAPI_MakeChamfer maker(base->shape);
        for (const auto& edge : edges->edges) {
            if (BRep_Tool::Degenerated(edge.edge)) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    "Chamfer edge is degenerated",
                    object.name,
                    "Base"
                );
                return std::nullopt;
            }
            const auto face
                = ancestorFaceForChamfer(base->shape, edge.edge, flipDirection, object, context);
            if (!face) {
                return std::nullopt;
            }
            if (chamferType == "Equal distance") {
                maker.Add(size, size, edge.edge, *face);
            }
            else if (chamferType == "Two distances") {
                maker.Add(size, size2, edge.edge, *face);
            }
            else if (chamferType == "Distance and Angle") {
                size2 = angle;
                maker.AddDA(size, angle * M_PI / 180.0, edge.edge, *face);
            }
            else {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_property",
                    "Unsupported ChamferType " + chamferType,
                    object.name,
                    "ChamferType"
                );
                return std::nullopt;
            }
        }
        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Chamfer operation failed",
                object.name,
                "Base"
            );
            return std::nullopt;
        }

        TopoDS_Shape result = maker.Shape();
        part::NamedShape namedShape = part::namedShapeForMakerHistory(
            object.name,
            result,
            {part::NamedShapeSource {
                base->link.object,
                base->shape,
                base->namedShape ? &*base->namedShape : nullptr
            }},
            maker,
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
            // TopoShapeExpansion.cpp::TopoShape::makeElementChamfer() forwards
            // OpCodes::Chamfer ("CHF") to makeShapeWithElementMap().  Preserve that
            // producer operation in the Part NamedShape ledger so runtime only projects
            // already-recorded canonical aliases.
            part::MakerHistoryOptions {"CHF", true, false, context.stringHasher, true}
        );
        DressUpResult dressUpResult {
            "chamfer",
            base->link.object,
            *base,
            result,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        dressUpResult.selection = edges->evidence;
        dressUpResult.parameters = {
            {"chamfer_type", chamferType},
            {"size", size},
            {"flip_direction", flipDirection},
        };
        if (chamferType == "Two distances") {
            dressUpResult.parameters["size2"] = size2;
        }
        if (chamferType == "Distance and Angle") {
            dressUpResult.parameters["angle"] = angle;
        }
        return dressUpResult;
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

void executeChamfer(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    runtime::ProducerTraceScope producerTrace(
        context,
        object,
        "partdesign.dressup",
        "Chamfer::execute",
        {{"maker", "CHF"},
         {"useAllEdges", app::readBool(object, "UseAllEdges").value_or(false)},
         {"size", app::readNumber(object, "Size").value_or(0.0)}}
    );
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base",
             "BaseFeature",
             "SupportTransform",
             "ChamferType",
             "Size",
             "Size2",
             "Angle",
             "FlipDirection",
             "UseAllEdges",
             "Refine",
             "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const std::size_t diagnosticStart = context.diagnostics.size();
    auto result = buildChamfer(object, context);
    if (!result) {
        const std::string reason = context.diagnostics.size() > diagnosticStart
            ? context.diagnostics.back().code
            : "dressup_build_failed";
        producerTrace.event(
            "reject",
            reason,
            {{"guard", "build_chamfer"},
             {"diagnostic",
              context.diagnostics.size() > diagnosticStart
                  ? context.diagnostics.back().message
                  : "no_result"}}
        );
        producerTrace.abort(reason);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    producerTrace.event(
        "selection",
        "ordered_dressup_selection_resolved",
        detail::selectionEvidenceJson(result->selection)
    );
    if (!applyDressUpRefine(object, context, *result)) {
        producerTrace.abort("dressup_refine_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        producerTrace.abort("add_sub_shape_cache_failed");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    producerTrace.event(
        "publish",
        "dressup_result_ready",
        {{"maker", "CHF"}, {"parameters", result->parameters}}
    );
    publishDressUpResult(object, context, *result);
}

}  // namespace cad_core::part_design
