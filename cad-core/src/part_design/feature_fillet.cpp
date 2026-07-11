#include "cad_core/part_design/feature_fillet.h"

#include "feature_dress_up_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <BRepFilletAPI_MakeFillet.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>

#include <optional>

namespace cad_core::part_design
{

namespace
{

using detail::applyDressUpRefine;
using detail::cacheDressUpAddSubShape;
using detail::publishDressUpResult;
using detail::readBoolProperty;
using detail::readNumberProperty;
using detail::resolveDressUpBase;
using detail::selectedDressUpEdges;

using detail::DressUpResult;

std::optional<DressUpResult> buildFillet(
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

    const double radius = readNumberProperty(object, "Radius", 1.0);
    if (radius <= Precision::Confusion()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_length",
            "Fillet radius must be greater than zero",
            object.name,
            "Radius"
        );
        return std::nullopt;
    }

    try {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementFillet(), creates BRepFilletAPI_MakeFillet and calls
        // "mkFillet.Add(radius1, radius2, TopoDS::Edge(edge))" for every selected edge.
        BRepFilletAPI_MakeFillet maker(base->shape);
        for (const auto& edge : edges->edges) {
            maker.Add(radius, radius, edge.edge);
        }
        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Fillet operation failed",
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
            // TopoShapeExpansion.cpp::TopoShape::makeElementFillet() forwards
            // OpCodes::Fillet ("FLT") to makeShapeWithElementMap().  The Part producer,
            // rather than the runtime publisher, must retain that maker operation while it
            // converts Generated/Modified history into source-backed ElementMap aliases.
            part::MakerHistoryOptions {"FLT", true, false, context.stringHasher, true}
        );
        DressUpResult dressUpResult {
            "fillet",
            base->link.object,
            *base,
            result,
            namedShape,
            readBoolProperty(object, "SupportTransform")
        };
        dressUpResult.selection = edges->evidence;
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

void executeFillet(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Base", "BaseFeature", "SupportTransform", "Radius", "UseAllEdges", "Refine", "FuzzyTolerance"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildFillet(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyDressUpRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!cacheDressUpAddSubShape(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishDressUpResult(object, context, *result);
}

}  // namespace cad_core::part_design
