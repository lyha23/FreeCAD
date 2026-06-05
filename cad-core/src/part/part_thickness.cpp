#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/runtime/feature_executor.h"

#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

using part_feature_detail::addPartOffsetDiagnostic;
using part_feature_detail::publishPartShape;
using part_feature_detail::readNumberProperty;

struct ThicknessSource
{
    std::string objectName;
    TopoDS_Shape shape;
    std::optional<NamedShape> fallbackNamedShape;
    const NamedShape* namedShape = nullptr;
    std::vector<TopoDS_Face> faces;
    std::vector<std::string> selectedFaceSubnames;
};

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

int solidCount(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

std::optional<ThicknessSource> resolveThicknessSource(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    if (app::propertyValue(object, "Faces") == nullptr) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_property",
            "Part::Thickness Faces must link to a source object",
            "Faces"
        );
        return std::nullopt;
    }

    const auto link = app::readLink(object, "Faces");
    if (!link || link->object.empty()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_property",
            "Part::Thickness Faces must be an App::PropertyLinkSub",
            "Faces"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::Thickness Faces target " + link->object + " did not produce a shape",
            "Faces",
            link->object
        );
        return std::nullopt;
    }
    if (solidCount(shapeIt->second.shape) != 1) {
        addPartOffsetDiagnostic(
            object,
            context,
            "execution_failed",
            "Source shape is not single solid.",
            "Faces",
            link->object
        );
        return std::nullopt;
    }

    ThicknessSource source;
    source.objectName = link->object;
    source.shape = shapeIt->second.shape;
    const auto namedShapeIt = context.namedShapes.find(link->object);
    if (namedShapeIt != context.namedShapes.end()) {
        source.namedShape = &namedShapeIt->second;
    }
    else {
        source.fallbackNamedShape = indexedNamedShapeForObject(link->object, source.shape);
        source.namedShape = &*source.fallbackNamedShape;
    }

    for (std::size_t index = 0; index < link->subnames.size(); ++index) {
        const std::string& subname = link->subnames[index];
        const std::string stableSubname = index < link->stableSubnames.size()
            ? link->stableSubnames[index]
            : subname;
        const auto parsed = parseSubshapeName(subname);
        if (!parsed) {
            addPartOffsetDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Invalid Part::Thickness Faces subshape " + subname,
                "Faces",
                link->object
            );
            return std::nullopt;
        }
        if (parsed->kind != TopAbs_FACE) {
            addPartOffsetDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Invalid face selection",
                "Faces",
                link->object
            );
            return std::nullopt;
        }

        const auto subshape = subshapeByName(*source.namedShape, subname, stableSubname);
        if (!subshape || subshape->IsNull()) {
            addPartOffsetDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Part::Thickness Faces cannot resolve " + subname,
                "Faces",
                link->object
            );
            return std::nullopt;
        }
        if (subshape->ShapeType() != TopAbs_FACE) {
            addPartOffsetDiagnostic(
                object,
                context,
                "invalid_subshape",
                "Invalid face selection",
                "Faces",
                link->object
            );
            return std::nullopt;
        }
        source.faces.push_back(TopoDS::Face(*subshape));
        source.selectedFaceSubnames.push_back(subname);
    }
    return source;
}

std::string modeName(short mode)
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

std::string joinName(short join)
{
    switch (join) {
        case 0:
            return "Arc";
        case 1:
            return "Tangent";
        case 2:
            return "Intersection";
        default:
            return "Arc";
    }
}

std::string effectiveJoinName(short join)
{
    return join == 0 ? "Arc" : "Intersection";
}

}  // namespace

void executePartThickness(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::Thickness::execute(), reads "Faces", "Value", "Mode", "Join", "Intersection" and
    // "SelfIntersection"; requires a single-solid source and selected FaceN subshapes before
    // calling "makeElementThickSolid(base, shapes, thickness, tol, inter, self, mode, join)".
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Faces", "Value", "Mode", "Join", "Intersection", "SelfIntersection"}
        )) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    constexpr std::array<const char*, 3> modes = {"Skin", "Pipe", "RectoVerso"};
    constexpr std::array<const char*, 3> joins = {"Arc", "Tangent", "Intersection"};
    const auto mode = readEnumIndexProperty(object, "Mode", modes, 0);
    if (!mode) {
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Thickness Mode must be Skin, Pipe or RectoVerso",
            "Mode"
        );
        return;
    }
    const auto join = readEnumIndexProperty(object, "Join", joins, 0);
    if (!join) {
        addPartOffsetDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::Thickness Join must be Arc, Tangent or Intersection",
            "Join"
        );
        return;
    }

    auto source = resolveThicknessSource(object, context);
    if (!source) {
        return;
    }

    const double thickness = readNumberProperty(object, "Value", 1.0);
    const bool intersection = app::readBool(object, "Intersection").value_or(false);
    const bool selfIntersection = app::readBool(object, "SelfIntersection").value_or(false);
    const NamedShapeSource namedSource {
        source->namedShape != nullptr ? source->namedShape->owner : source->objectName,
        source->shape,
        source->namedShape
    };
    const NamedShapeBuild build = makeElementThickSolidFromSource(
        object.name,
        namedSource,
        source->faces,
        thickness,
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
            build.error.empty() ? "Part::Thickness failed" : build.error,
            "Faces",
            source->objectName
        );
        return;
    }

    publishPartShape(
        object,
        context,
        build.shape,
        {{"feature", "part_thickness"},
         {"source", source->objectName},
         {"value", thickness},
         {"mode", modeName(*mode)},
         {"join", joinName(*join)},
         {"effective_join", effectiveJoinName(*join)},
         {"intersection", intersection},
         {"self_intersection", selfIntersection},
         {"selected_faces", source->selectedFaceSubnames},
         {"topo_naming_history", "maker_history:thick_solid"}},
        build.namedShape
    );
}

}  // namespace cad_core::part
