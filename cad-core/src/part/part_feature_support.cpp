#include "part_feature_support.h"

#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"

#include <TopExp_Explorer.hxx>

#include <cmath>
#include <utility>

namespace cad_core::part::part_feature_detail
{

double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback)
{
    return app::readNumber(object, property).value_or(fallback);
}

double radians(double degrees)
{
    return degrees * std::acos(-1.0) / 180.0;
}

void addPartOffsetDiagnostic(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& code,
    const std::string& message,
    const std::string& property,
    const std::string& target
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
    context.objects[object.name] = {{"status", "error"}};
}

std::optional<PartLinkedShape> resolvePartSourceLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& featureName
)
{
    if (app::propertyValue(object, property) == nullptr) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_property",
            featureName + " " + property + " must link to an object",
            property
        );
        return std::nullopt;
    }

    const auto link = app::readLink(object, property);
    if (!link || link->object.empty()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_property",
            featureName + " " + property + " must be an App::PropertyLink",
            property
        );
        return std::nullopt;
    }
    if (!link->subnames.empty()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "invalid_subshape",
            featureName + " " + property + " uses App::PropertyLink and cannot select subshapes",
            property,
            link->object
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end() || shapeIt->second.shape.IsNull()) {
        addPartOffsetDiagnostic(
            object,
            context,
            "missing_link_target",
            featureName + " " + property + " target " + link->object + " did not produce a shape",
            property,
            link->object
        );
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link->object);
    return PartLinkedShape {
        link->object,
        shapeIt->second.shape,
        namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr,
    };
}

part::NamedShapeSource sourceForPartLinkedShape(const PartLinkedShape& input)
{
    return part::NamedShapeSource {
        input.namedShape != nullptr ? input.namedShape->owner : input.objectName,
        input.shape,
        input.namedShape
    };
}

bool shapeContainsKind(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind)
{
    TopExp_Explorer explorer(shape, kind);
    return explorer.More();
}

TopoDS_Shape applyGlobalPlacement(
    const app::DocumentObject& object,
    const runtime::ComputeContext& context,
    const TopoDS_Shape& shape
)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    if (placementIt == context.globalPlacements.end()
        || placementIt->second.Form() == gp_Identity) {
        return shape;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp
    // ::GeoFeature::getGlobalPlacement() leaves an identity-placed PropertyTopoShape unchanged.
    // Avoid a BRepBuilderAPI_Transform copy for identity: downstream helper makers consume the
    // producer's original edge/wire representation, just as Document recompute does.
    return base::transformShape(shape, placementIt->second);
}

std::string shapeLabelForPartShape(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_SOLID:
            return "occt_solid";
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
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

runtime::ShapeValue::Kind shapeKindForPartShape(const TopoDS_Shape& shape)
{
    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    return solidExplorer.More() ? runtime::ShapeValue::Kind::Solid
                                : runtime::ShapeValue::Kind::PartPrimitive;
}

nlohmann::json topologyCountsForPartSubshapes(const nlohmann::json& subshapes)
{
    nlohmann::json counts = {
        {"faces", 0},
        {"edges", 0},
        {"vertices", 0},
    };
    if (!subshapes.is_object()) {
        return counts;
    }
    for (const auto& [name, subshape] : subshapes.items()) {
        static_cast<void>(name);
        if (!subshape.is_object()) {
            continue;
        }
        const std::string kind = subshape.value("kind", "");
        if (kind == "face") {
            counts["faces"] = counts["faces"].get<int>() + 1;
        }
        else if (kind == "edge") {
            counts["edges"] = counts["edges"].get<int>() + 1;
        }
        else if (kind == "vertex") {
            counts["vertices"] = counts["vertices"].get<int>() + 1;
        }
    }
    return counts;
}

void publishPartShape(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TopoDS_Shape& localShape,
    const nlohmann::json& metadata,
    const std::optional<part::NamedShape>& namedShape,
    PartPublicResultFields publicResultFields
)
{
    const TopoDS_Shape shape = applyGlobalPlacement(object, context, localShape);
    context.shapes[object.name] = runtime::ShapeValue {shapeKindForPartShape(shape), shape};
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapePyImp.cpp
    // ::TopoShapePy::optimalBoundingBox() reads the imported TopoShape before display meshing.
    // meshForShape() may create or replace OCCT triangulation, so select the public bbox before
    // generating the request-local mesh. GeometryOnly callers are unchanged because AddOptimal
    // with useTriangulation=false ignores that display artifact.
    const nlohmann::json bbox = publicResultFields.boundingBoxMode
            == PartBoundingBoxMode::UseTriangulation
        ? cad_core::part::bboxForShape(shape)
        : cad_core::part::objectBBoxForShape(shape);
    context.mesh[object.name] = cad_core::part::meshForShape(shape);
    const nlohmann::json subshapes = part::subshapeMapForShape(shape);
    context.subshapes[object.name] = subshapes;
    if (namedShape) {
        context.namedShapes[object.name] = *namedShape;
        context.namedShapes[object.name].owner = object.name;
        context.namedShapes[object.name].shape = shape;
    }
    else {
        context.namedShapes[object.name] = part::indexedNamedShapeForObject(object.name, shape);
    }

    const double volume = cad_core::part::volumeForShape(shape);
    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabelForPartShape(shape);
    result["bbox"] = bbox;
    result["volume"] = volume;
    result["kernel"] = cad_core::part::kernelVersion();
    context.objects[object.name] = result;

    if (publicResultFields.objectFields || publicResultFields.includeShapeSummary) {
        runtime::PublicResultFields& published = context.publicResultFields[object.name];
        published.objectFields = std::move(publicResultFields.objectFields);
        if (publicResultFields.includeShapeSummary) {
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapePyImp.cpp
            // ::TopoShapePy::optimalBoundingBox(), ::getVolume(), ::getFaces(), ::getEdges()
            // and ::getVertexes() expose the summary of the shape returned by
            // AppPartPy.cpp::makeFilledFace(). Materialize it at the Part producer seam.
            published.shapeSummary = {
                {"bbox", bbox},
                {"topology_counts", topologyCountsForPartSubshapes(subshapes)},
                {"volume", volume},
            };
        }
    }
}

}  // namespace cad_core::part::part_feature_detail
