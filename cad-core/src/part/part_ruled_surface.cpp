#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape_expansion.h"
#include "cad_core/runtime/feature_executor.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct PartRuledSurfaceCurve
{
    std::string objectName;
    TopoDS_Edge edge;
    std::vector<std::string> stableEdgeNames;
};

void addRuledSurfaceDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property,
    const std::string& target = {}
)
{
    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        code,
        message,
        object.name,
        property,
        "runtime",
        target
    );
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_ruled_surface"}};
}

std::string orientationLabel(short orientation)
{
    switch (orientation) {
        case 0:
            return "Automatic";
        case 1:
            return "Forward";
        case 2:
            return "Reversed";
        default:
            return "Unsupported";
    }
}

std::optional<short> readRuledSurfaceOrientation(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::RuledSurface::RuledSurface(), OrientationEnums are "Automatic", "Forward", "Reversed".
    constexpr std::array<const char*, 3> labels = {"Automatic", "Forward", "Reversed"};
    if (const auto stringValue = app::readString(object, "Orientation")) {
        for (std::size_t index = 0; index < labels.size(); ++index) {
            if (*stringValue == labels[index]) {
                return static_cast<short>(index);
            }
        }
        addRuledSurfaceDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::RuledSurface Orientation must be Automatic, Forward or Reversed",
            "Orientation"
        );
        return std::nullopt;
    }
    if (const auto numberValue = app::readNumber(object, "Orientation")) {
        const auto index = static_cast<int>(std::llround(*numberValue));
        if (index >= 0 && index < static_cast<int>(labels.size())) {
            return static_cast<short>(index);
        }
        addRuledSurfaceDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::RuledSurface Orientation must be Automatic, Forward or Reversed",
            "Orientation"
        );
        return std::nullopt;
    }
    return 0;
}

void addDistinct(std::vector<std::string>& values, const std::string& value)
{
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string qualifiedSubname(const std::string& objectName, const std::string& subname)
{
    if (subname.find('.') != std::string::npos) {
        return subname;
    }
    return objectName + "." + subname;
}

std::vector<std::string> stableEdgeNamesForLink(const app::Link& link)
{
    std::vector<std::string> names;
    if (link.subnames.empty() || link.subnames.front().empty()) {
        addDistinct(names, link.object + ".Edge1");
        return names;
    }
    const std::string current = link.subnames.front();
    const std::string stable = !link.stableSubnames.empty() && !link.stableSubnames.front().empty()
        ? link.stableSubnames.front()
        : current;
    addDistinct(names, qualifiedSubname(link.object, stable));
    addDistinct(names, qualifiedSubname(link.object, current));
    return names;
}

std::optional<TopoDS_Shape> linkedSubshape(
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape,
    const app::Link& link
)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return sourceShape;
    }

    const std::string current = link.subnames.front();
    const std::string stable = !link.stableSubnames.empty() && !link.stableSubnames.front().empty()
        ? link.stableSubnames.front()
        : current;
    if (namedShape != nullptr) {
        if (auto shape = part::subshapeByName(*namedShape, current, stable)) {
            return shape;
        }
    }
    if (auto shape = part::subshapeByName(sourceShape, current)) {
        return shape;
    }
    if (stable != current) {
        return part::subshapeByName(sourceShape, stable);
    }
    return std::nullopt;
}

int edgeCount(const TopoDS_Shape& shape)
{
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    return edges.Extent();
}

std::optional<PartRuledSurfaceCurve> resolveRuledSurfaceCurve(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::RuledSurface::execute(), reads Curve1/Curve2 through getTopoShape with
    // "ResolveLink | Transform" and, when a sub-value exists, "NeedSubElement".
    if (app::propertyValue(object, property) == nullptr) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "missing_property",
            "No shape linked.",
            property
        );
        return std::nullopt;
    }

    const auto links = app::readLinks(object, property);
    if (links.empty() || links.front().object.empty()) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "No shape linked.",
            property
        );
        return std::nullopt;
    }
    if (links.size() != 1U || links.front().subnames.size() > 1U) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Not exactly one sub-shape linked.",
            property,
            links.front().object
        );
        return std::nullopt;
    }

    const auto& link = links.front();
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "No shape linked.",
            property,
            link.object
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link.object);
    const part::NamedShape* namedShape = namedShapeIt != context.namedShapes.end()
        ? &namedShapeIt->second
        : nullptr;
    const auto selected = linkedSubshape(shapeIt->second.shape, namedShape, link);
    if (!selected || selected->IsNull()) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "invalid_link",
            "Invalid link.",
            property,
            link.object
        );
        return std::nullopt;
    }

    if (selected->ShapeType() != TopAbs_EDGE) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            edgeCount(*selected) == 0 ? "no_edge" : "unsupported_subshape_kind",
            edgeCount(*selected) == 0 ? "Input shape has no edge"
                                      : "Part::RuledSurface edge/edge batch requires an edge",
            property,
            link.object
        );
        return std::nullopt;
    }

    return PartRuledSurfaceCurve {
        link.object,
        TopoDS::Edge(*selected),
        stableEdgeNamesForLink(link)
    };
}

nlohmann::json sourceMetadata(
    const runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& prefix
)
{
    nlohmann::json metadata = nlohmann::json::object();
    const auto sourceMetadataIt = context.objects.find(objectName);
    if (sourceMetadataIt == context.objects.end()) {
        return metadata;
    }
    const auto& source = sourceMetadataIt->second;
    constexpr std::array<std::pair<const char*, const char*>, 5> keys {{
        {"feature", "feature"},
        {"dto", "dto"},
        {"curve_kind", "curve_kind"},
        {"curve_type", "curve_type"},
        {"part_geometry_type", "part_geometry_type"},
    }};
    for (const auto& [sourceKey, suffix] : keys) {
        const auto item = source.find(sourceKey);
        if (item != source.end()) {
            metadata[prefix + "_" + suffix] = *item;
        }
    }
    return metadata;
}

}  // namespace

void executePartRuledSurface(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    // ::RuledSurface::execute(), loops over "Curve1" / "Curve2", reports "No shape linked.",
    // "Not exactly one sub-shape linked." or "Invalid link.", then calls
    // "res.makeElementRuledSurface(shapes, Orientation.getValue())".
    if (!runtime::rejectUnsupportedProperties(object, context, {"Curve1", "Curve2", "Orientation"})) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_ruled_surface"}};
        return;
    }

    const auto orientation = readRuledSurfaceOrientation(object, context);
    if (!orientation) {
        return;
    }
    const auto curve1 = resolveRuledSurfaceCurve(object, context, "Curve1");
    if (!curve1) {
        return;
    }
    const auto curve2 = resolveRuledSurfaceCurve(object, context, "Curve2");
    if (!curve2) {
        return;
    }

    const auto build = makeElementRuledSurfaceFromEdges(
        object.name,
        std::array<RuledSurfaceEdgeSource, 2> {{
            RuledSurfaceEdgeSource {curve1->objectName, curve1->edge, curve1->stableEdgeNames},
            RuledSurfaceEdgeSource {curve2->objectName, curve2->edge, curve2->stableEdgeNames},
        }},
        *orientation
    );
    if (!build.error.empty() || build.shape.IsNull()) {
        addRuledSurfaceDiagnostic(
            object,
            context,
            "execution_failed",
            build.error.empty() ? "Part::RuledSurface failed" : build.error,
            "Curve1"
        );
        return;
    }

    nlohmann::json metadata = {
        {"feature", "part_ruled_surface"},
        {"source_curve1", curve1->objectName},
        {"source_curve2", curve2->objectName},
        {"orientation", orientationLabel(*orientation)}
    };
    metadata.update(sourceMetadata(context, curve1->objectName, "source_curve1"));
    metadata.update(sourceMetadata(context, curve2->objectName, "source_curve2"));
    part_feature_detail::publishPartShape(object, context, build.shape, metadata, build.namedShape);
}

}  // namespace cad_core::part
