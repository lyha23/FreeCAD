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
    const auto source = resolvePartSourceLink(object, context, "Source", "Part::Offset");
    if (!source) {
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
        *join,
        fill
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

void executePartOffset2D(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp
    // ::Offset2D::execute(), reads "Source", "Value", "Mode", "Join", "Fill" and
    // "Intersection"; mode 0 maps to "OpenResult::allowOpenResult", while mode 2 returns
    // "Mode 'Recto-Verso' is not supported for 2D offset."
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Source", "Value", "Mode", "Join", "Fill", "Intersection"}
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
            "Part::Offset2D Mode must be Skin, Pipe or RectoVerso",
            "Mode"
        );
        return;
    }
    if (*mode == 2) {
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Mode 'Recto-Verso' is not supported for 2D offset.",
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
            "Part::Offset2D Join must be Arc, Tangent or Intersection",
            "Join"
        );
        return;
    }

    const auto source = resolvePartSourceLink(object, context, "Source", "Part::Offset2D");
    if (!source) {
        return;
    }
    const double offset = readNumberProperty(object, "Value", 1.0);
    const bool fill = app::readBool(object, "Fill").value_or(false);
    const bool intersection = app::readBool(object, "Intersection").value_or(false);
    const bool allowOpenResult = *mode == 0;
    const bool faceSource = source->shape.ShapeType() == TopAbs_FACE;
    const bool compoundSource = source->shape.ShapeType() == TopAbs_COMPOUND;
    const part::NamedShapeBuild build = part::makeElementOffset2DFromSource(
        object.name,
        sourceForPartLinkedShape(*source),
        offset,
        *join,
        fill,
        allowOpenResult,
        intersection
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::Offset2D failed" : build.error,
            "Source",
            source->objectName
        );
        return;
    }

    publishPartShape(
        object,
        context,
        build.shape,
        {{"feature", "part_offset2d"},
         {"source", source->objectName},
         {"offset", offset},
         {"mode", offsetModes[*mode]},
         {"join", joinTypes[*join]},
         {"fill", fill},
         {"intersection", intersection},
         {"open_result", allowOpenResult},
         {"topo_naming_history",
          compoundSource ? (intersection ? "maker_history:offset2d_compound_collective"
                                         : "maker_history:offset2d_compound_recursive")
                         : (fill ? (faceSource ? "maker_history:offset2d_face_fill_closed"
                                               : "maker_history:offset2d_wire_fill_open")
                                 : (faceSource ? "maker_history:offset2d_face_no_fill"
                                               : "maker_history:offset2d_wire_no_fill"))}},
        build.namedShape
    );
}

}  // namespace cad_core::part
