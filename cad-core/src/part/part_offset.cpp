#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <array>
#include <cmath>
#include <optional>
#include <string>

namespace cad_core::part
{

namespace
{

using part_feature_detail::addPartOffsetDiagnostic;
using part_feature_detail::publishPartShape;
using part_feature_detail::readNumberProperty;
using part_feature_detail::resolvePartSourceLink;
using part_feature_detail::shapeContainsKind;
using part_feature_detail::sourceForPartLinkedShape;

std::optional<short> readEnumIndexProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::array<const char*, 3>& labels,
    short fallback
)
{
    if (const auto stringValue = app::readString(object, property)) {
        for (std::size_t index = 0; index < labels.size(); ++index) {
            if (*stringValue == labels[index]) {
                return static_cast<short>(index);
            }
        }
        return std::nullopt;
    }

    if (const auto numberValue = app::readNumber(object, property)) {
        const auto index = static_cast<int>(std::llround(*numberValue));
        if (index >= 0 && index < static_cast<int>(labels.size())) {
            return static_cast<short>(index);
        }
        return std::nullopt;
    }

    return fallback;
}

}  // namespace

void executePartOffset(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp
    // ::Offset::execute(), reads "Source", "Value", "Mode", "Join", "Intersection",
    // "SelfIntersection" and "Fill", then calls "TopoShape(0).makeElementOffset(...)".
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Source", "Value", "Mode", "Join", "Intersection", "SelfIntersection", "Fill"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    constexpr std::array<const char*, 3> offsetModes = {"Skin", "Pipe", "RectoVerso"};
    constexpr std::array<const char*, 3> joinTypes = {"Arc", "Tangent", "Intersection"};
    const auto mode = readEnumIndexProperty(object, "Mode", offsetModes, 0);
    if (!mode) {
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Offset Mode must be Skin, Pipe or RectoVerso",
            "Mode"
        );
        return;
    }
    const auto join = readEnumIndexProperty(object, "Join", joinTypes, 0);
    if (!join) {
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Offset Join must be Arc, Tangent or Intersection",
            "Join"
        );
        return;
    }

    const bool fill = app::readBool(object, "Fill").value_or(false);
    if (fill) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementOffset(), FillType::fill follows the free-bound wires and
        // OffsetEdgesFromShapes() images after mkOffset. cad-core keeps that branch explicit until
        // the fill-face/solid history route is migrated.
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Offset Fill=true requires FreeCAD fill-bound offset history and is not in the "
            "C3-M4 first slice",
            "Fill"
        );
        return;
    }

    const auto source = resolvePartSourceLink(object, context, "Source", "Part::Offset");
    if (!source) {
        return;
    }
    if (shapeContainsKind(source->shape, TopAbs_SOLID)) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
        // ::TopoShape::makeElementOffset(), when the source "hasSubShape(TopAbs_SOLID)" but the
        // offset result lacks one, tries "res.makeElementSolid()". The first C3-M4 slice is limited
        // to face/shell offset history, so solid-source recovery is diagnosed instead of approximated.
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_geometry",
            "Part::Offset solid Source requires makeElementSolid recovery and is not in the C3-M4 "
            "first slice",
            "Source",
            source->objectName
        );
        return;
    }

    const double offset = readNumberProperty(object, "Value", 1.0);
    const bool intersection = app::readBool(object, "Intersection").value_or(false);
    const bool selfIntersection = app::readBool(object, "SelfIntersection").value_or(false);
    const part::NamedShapeBuild build = part::makeElementOffsetFromSource(
        object.name,
        sourceForPartLinkedShape(*source),
        offset,
        Precision::Confusion(),
        intersection,
        selfIntersection,
        *mode,
        *join
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::Offset failed" : build.error,
            "Source",
            source->objectName
        );
        return;
    }

    publishPartShape(
        object,
        context,
        build.shape,
        {{"feature", "part_offset"},
         {"source", source->objectName},
         {"offset", offset},
         {"mode", offsetModes[*mode]},
         {"join", joinTypes[*join]},
         {"intersection", intersection},
         {"self_intersection", selfIntersection},
         {"fill", fill},
         {"topo_naming_history", "maker_history:offset"}},
        build.namedShape
    );
}

}  // namespace cad_core::part
