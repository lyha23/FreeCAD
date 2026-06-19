#include "cad_core/part/part_feature.h"

#include "part_feature_support.h"

#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/feature_executor.h"

#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepProj_Projection.hxx>
#include <BRep_Builder.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part
{

namespace
{

struct ProjectSubshape
{
    std::string objectName;
    std::string stableSubname;
    TopoDS_Shape shape;
};

void addProjectOnSurfaceDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property = {},
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
    context.objects[object.name] = {{"status", "error"}, {"feature", "part_project_on_surface"}};
}

std::string stableSubnameForLink(const app::Link& link)
{
    if (!link.stableSubnames.empty() && !link.stableSubnames.front().empty()) {
        return link.stableSubnames.front();
    }
    if (!link.subnames.empty()) {
        return link.subnames.front();
    }
    return {};
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
    const std::string stable = stableSubnameForLink(link);
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

std::optional<ProjectSubshape> resolveSingleSubshapeLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    TopAbs_ShapeEnum expectedKind,
    const std::string& expectedLabel
)
{
    const auto* propertyValue = app::propertyValue(object, property);
    if (propertyValue == nullptr) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_property",
            "Part::ProjectOnSurface " + property + " must be specified",
            property
        );
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, property);
    if (links.empty() || links.front().object.empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface " + property + " target is missing",
            property
        );
        return std::nullopt;
    }
    if (links.size() != 1U || links.front().subnames.size() != 1U
        || links.front().subnames.front().empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface " + property + " must reference exactly one subshape",
            property,
            links.front().object
        );
        return std::nullopt;
    }

    const app::Link& link = links.front();
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface " + property + " target did not produce a shape",
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
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface " + property + " subshape cannot be resolved",
            property,
            link.object
        );
        return std::nullopt;
    }
    if (selected->ShapeType() != expectedKind) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_subshape_kind",
            "Part::ProjectOnSurface " + property + " first slice requires a " + expectedLabel,
            property,
            link.object
        );
        return std::nullopt;
    }

    return ProjectSubshape {link.object, stableSubnameForLink(link), *selected};
}

std::optional<ProjectSubshape> resolveProjectionShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto* propertyValue = app::propertyValue(object, "Projection");
    if (propertyValue == nullptr) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_property",
            "Part::ProjectOnSurface Projection must be specified",
            "Projection"
        );
        return std::nullopt;
    }

    const std::vector<app::Link> links = app::readLinks(object, "Projection");
    if (links.empty() || links.front().object.empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface Projection target is missing",
            "Projection"
        );
        return std::nullopt;
    }
    if (links.size() != 1U || links.front().subnames.size() != 1U
        || links.front().subnames.front().empty()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::ProjectOnSurface first slice supports exactly one projected edge or wire",
            "Projection",
            links.front().object
        );
        return std::nullopt;
    }

    const app::Link& link = links.front();
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "missing_link_target",
            "Part::ProjectOnSurface Projection target did not produce a shape",
            "Projection",
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
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_subshape",
            "Part::ProjectOnSurface Projection subshape cannot be resolved",
            "Projection",
            link.object
        );
        return std::nullopt;
    }
    if (selected->ShapeType() != TopAbs_EDGE && selected->ShapeType() != TopAbs_WIRE) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_subshape_kind",
            "Part::ProjectOnSurface first slice projects only edges and wires",
            "Projection",
            link.object
        );
        return std::nullopt;
    }

    return ProjectSubshape {link.object, stableSubnameForLink(link), *selected};
}

std::optional<std::string> readProjectionMode(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
    // ::ProjectOnSurface::ProjectOnSurface(), registers Mode enums "All", "Faces", "Edges".
    std::string mode = "All";
    if (const auto label = app::readString(object, "Mode")) {
        mode = *label;
    }
    else if (const auto value = app::readNumber(object, "Mode")) {
        const auto index = static_cast<int>(std::llround(*value));
        constexpr std::array<const char*, 3> labels = {"All", "Faces", "Edges"};
        if (index >= 0 && index < static_cast<int>(labels.size())) {
            mode = labels.at(static_cast<std::size_t>(index));
        }
    }

    if (mode != "Edges") {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "unsupported_property",
            "Part::ProjectOnSurface first slice supports Mode=Edges only",
            "Mode"
        );
        return std::nullopt;
    }
    return mode;
}

bool rejectNonZeroProperty(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    const double value = app::readNumber(object, property).value_or(0.0);
    if (std::abs(value) <= Precision::Confusion()) {
        return false;
    }
    addProjectOnSurfaceDiagnostic(
        object,
        context,
        "unsupported_property",
        "Part::ProjectOnSurface first slice requires " + property + "=0",
        property
    );
    return true;
}

std::optional<gp_Dir> readProjectionDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto vector = app::readVector3(object, "Direction").value_or(std::array<double, 3> {0.0, 0.0, 1.0});
    const double magnitudeSquared =
        vector.at(0) * vector.at(0) + vector.at(1) * vector.at(1) + vector.at(2) * vector.at(2);
    if (magnitudeSquared <= Precision::Confusion() * Precision::Confusion()) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "invalid_direction",
            "Part::ProjectOnSurface Direction must be non-zero",
            "Direction"
        );
        return std::nullopt;
    }
    return gp_Dir(vector.at(0), vector.at(1), vector.at(2));
}

TopoDS_Wire closestProjectedWire(BRepProj_Projection& projection, const TopoDS_Shape& reference)
{
    double minDistance = std::numeric_limits<double>::max();
    TopoDS_Wire wireToTake;
    for (; projection.More(); projection.Next()) {
        const TopoDS_Wire current = projection.Current();
        BRepExtrema_DistShapeShape distanceMeasure(current, reference);
        distanceMeasure.Perform();
        const double currentDistance = distanceMeasure.Value();
        if (currentDistance > minDistance) {
            continue;
        }
        wireToTake = current;
        minDistance = currentDistance;
    }
    return wireToTake;
}

std::vector<TopoDS_Shape> projectWireEdges(
    const TopoDS_Shape& wireOrEdge,
    const TopoDS_Face& supportFace,
    const gp_Dir& direction
)
{
    BRepProj_Projection projection(wireOrEdge, supportFace, direction);
    const TopoDS_Wire projectedWire = closestProjectedWire(projection, wireOrEdge);
    std::vector<TopoDS_Shape> edges;
    for (TopExp_Explorer explorer(projectedWire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        edges.push_back(TopoDS::Edge(explorer.Current()));
    }
    return edges;
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

}  // namespace

void executePartProjectOnSurface(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp
    // ::ProjectOnSurface::tryExecute(), calls getSupportFace(), getProjectionShapes(),
    // createProjectedWire(), filterShapes(), createCompound(), then restores Placement.
    if (!runtime::rejectUnsupportedProperties(
            object,
            context,
            {"Mode", "Height", "Offset", "Direction", "SupportFace", "Projection"}
        )) {
        context.objects[object.name] = {{"status", "error"}, {"feature", "part_project_on_surface"}};
        return;
    }

    const auto mode = readProjectionMode(object, context);
    if (!mode || rejectNonZeroProperty(object, context, "Height")
        || rejectNonZeroProperty(object, context, "Offset")) {
        return;
    }

    const auto direction = readProjectionDirection(object, context);
    if (!direction) {
        return;
    }
    const auto support = resolveSingleSubshapeLink(
        object,
        context,
        "SupportFace",
        TopAbs_FACE,
        "face"
    );
    if (!support) {
        return;
    }
    const auto projection = resolveProjectionShape(object, context);
    if (!projection) {
        return;
    }

    try {
        const std::vector<TopoDS_Shape> edges =
            projectWireEdges(projection->shape, TopoDS::Face(support->shape), *direction);
        if (edges.empty()) {
            addProjectOnSurfaceDiagnostic(
                object,
                context,
                "execution_failed",
                "Part::ProjectOnSurface did not produce projected edges",
                "Projection",
                projection->objectName
            );
            return;
        }

        part_feature_detail::publishPartShape(
            object,
            context,
            compoundOf(edges),
            {
                {"feature", "part_project_on_surface"},
                {"source_support", support->objectName},
                {"support_face", support->stableSubname},
                {"source_projection", projection->objectName},
                {"projection_subshape", projection->stableSubname},
                {"mode", *mode},
                {"height", 0.0},
                {"offset", 0.0},
                {"topo_naming_history", "indexed_projected_edges_no_mapper_history"},
            }
        );
    }
    catch (const Standard_Failure& failure) {
        addProjectOnSurfaceDiagnostic(
            object,
            context,
            "execution_failed",
            failure.GetMessageString(),
            "Projection",
            projection->objectName
        );
    }
}

}  // namespace cad_core::part
