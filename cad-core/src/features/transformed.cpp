#include "cad_core/features/linear_pattern.h"
#include "cad_core/features/mirrored.h"
#include "cad_core/features/multi_transform.h"
#include "cad_core/features/polar_pattern.h"
#include "cad_core/features/scaled.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepGProp.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cad_core::features {

namespace {

struct TransformSource {
    std::string owner;
    TopoDS_Shape shape;
    std::optional<topo::NamedShape> namedShape;
};

struct MirrorPlane {
    gp_Pnt point;
    gp_Dir normal;
};

struct RotationAxis {
    gp_Pnt point;
    gp_Dir direction;
};

struct SketchAxis {
    gp_Pnt point;
    gp_Dir direction;
};

struct TransformApplication {
    TopoDS_Shape shape;
    topo::NamedShape namedShape;
    std::vector<std::string> originals;
};

struct TransformedBuild {
    TopoDS_Shape shape;
    topo::NamedShape namedShape;
    std::string mode;
    std::vector<std::string> originals;
    bool refineApplied = false;
};

struct TemplateTransforms {
    std::vector<gp_Trsf> transforms;
    bool scaled = false;
};

const nlohmann::json* propertyPayload(const document::DocumentObject& object, const std::string& property)
{
    const auto* value = document::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType") && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string readEnumProperty(const document::DocumentObject& object,
                             const std::string& property,
                             const std::vector<std::string>& values,
                             const std::string& fallback)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr) {
        return fallback;
    }
    if (payload->is_string()) {
        return payload->get<std::string>();
    }
    if (payload->is_number_integer()) {
        const int index = payload->get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < values.size()) {
            return values[static_cast<std::size_t>(index)];
        }
    }
    return fallback;
}

std::optional<int> readIntegerProperty(const document::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr || !payload->is_number_integer()) {
        return std::nullopt;
    }
    return payload->get<int>();
}

double readNumberProperty(const document::DocumentObject& object,
                          const std::string& property,
                          double fallback)
{
    return document::readNumber(object, property).value_or(fallback);
}

bool readBoolProperty(const document::DocumentObject& object, const std::string& property)
{
    return document::readBool(object, property).value_or(false);
}

bool isTransformationTemplate(const document::DocumentObject& object, const runtime::ComputeContext& context)
{
    return context.transformationTemplateObjects.count(object.name) != 0U
        && document::readLinks(object, "Originals").empty()
        && document::propertyValue(object, "BaseFeature") == nullptr;
}

void publishTransformationTemplate(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::isMultiTransformChild(), treats transformed features with default
    // TransformMode=Features and empty Originals as children whose processing happens in MultiTransform.
    context.objects[object.name] = {
        {"status", "ok"},
        {"transformation_template", true},
        {"type_id", object.typeId},
    };
}

double degreesToRadians(double degrees)
{
    constexpr double pi = 3.141592653589793238462643383279502884;
    return degrees * pi / 180.0;
}

std::vector<double> readNumberListProperty(const document::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    std::vector<double> values;
    if (payload == nullptr || !payload->is_array()) {
        return values;
    }
    for (const auto& entry : *payload) {
        if (entry.is_number()) {
            values.push_back(entry.get<double>());
        }
    }
    return values;
}

std::vector<double> normalizedSpacingList(const document::DocumentObject& object,
                                          const std::string& property,
                                          double offset,
                                          int occurrences)
{
    std::vector<double> spacings = readNumberListProperty(object, property);
    const auto targetCount = static_cast<std::size_t>(std::max(occurrences - 1, 0));
    for (double& spacing : spacings) {
        if (std::abs(spacing - offset) < Precision::Confusion()) {
            spacing = -1.0;
        }
    }
    if (spacings.size() < targetCount) {
        spacings.resize(targetCount, -1.0);
    }
    else if (spacings.size() > targetCount) {
        spacings.resize(targetCount);
    }
    return spacings;
}

std::string stableSubnameDiagnosticCode(topo::ElementResolveStatus status)
{
    switch (status) {
        case topo::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case topo::ElementResolveStatus::Split:
            return "split_stable_subname";
        case topo::ElementResolveStatus::Resolved:
        case topo::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(const std::string& property,
                                           const std::string& target,
                                           const std::string& stableSubname,
                                           topo::ElementResolveStatus status)
{
    if (status == topo::ElementResolveStatus::Deleted) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == topo::ElementResolveStatus::Split) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return property + " target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

std::optional<TopoDS_Face> resolvePlanarFaceLink(const document::Link& link,
                                                 const document::DocumentObject& object,
                                                 runtime::ComputeContext& context,
                                                 const std::string& property)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference exactly one FaceN subshape",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " did not produce a solid",
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               link.subnames.front());
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front() : std::string{};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved = topo::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(property, link.object, stableSubname, resolved.status),
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    const auto parsed = topo::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid " + property + " subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " requires a planar face, not " + topo::subshapeKindName(parsed->kind),
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = topo::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = topo::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + link.object + " has no subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    return TopoDS::Face(*subshape);
}

std::optional<TopoDS_Shape> resolveDirectionSubshape(const document::Link& link,
                                                     const document::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const std::string& property)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference exactly one EdgeN or FaceN subshape",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " did not produce a shape",
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               link.subnames.front());
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front() : std::string{};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved = topo::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(property, link.object, stableSubname, resolved.status),
                                   object.name,
                                   property,
                                   "runtime",
                                   link.object,
                                   stableSubname);
            return std::nullopt;
        }
    }

    const auto parsed = topo::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid " + property + " subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " requires EdgeN or FaceN, not " + topo::subshapeKindName(parsed->kind),
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = topo::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = topo::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + link.object + " has no subshape " + currentSubname,
                               object.name,
                               property,
                               "runtime",
                               link.object,
                               currentSubname);
        return std::nullopt;
    }

    return subshape;
}

std::optional<MirrorPlane> planeFromFace(const TopoDS_Face& face,
                                         const document::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::string& property)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " face must be planar",
                               object.name,
                               property);
        return std::nullopt;
    }
    const gp_Pln plane = surface.Plane();
    return MirrorPlane{plane.Location(), plane.Axis().Direction()};
}

std::optional<MirrorPlane> resolveMirrorPlane(const document::DocumentObject& object,
                                              runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp
    // ::Mirrored::getTransformations(), accepts a DatumPlane or a planar face from a Part feature.
    if (document::propertyValue(object, "MirrorPlane") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Mirrored MirrorPlane must link to a datum plane or planar face",
                               object.name,
                               "MirrorPlane");
        return std::nullopt;
    }

    const auto link = document::readLink(object, "MirrorPlane");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Mirrored MirrorPlane must be an App::PropertyLinkSub",
                               object.name,
                               "MirrorPlane");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "MirrorPlane target " + link->object + " did not produce a shape",
                               object.name,
                               "MirrorPlane",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPlane && link->subnames.empty()) {
        for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
            return planeFromFace(TopoDS::Face(explorer.Current()), object, context, "MirrorPlane");
        }
    }

    const auto face = resolvePlanarFaceLink(*link, object, context, "MirrorPlane");
    if (!face) {
        return std::nullopt;
    }
    return planeFromFace(*face, object, context, "MirrorPlane");
}

std::optional<std::vector<gp_Trsf>> mirroredTransforms(const document::DocumentObject& object,
                                                       runtime::ComputeContext& context)
{
    auto mirrorPlane = resolveMirrorPlane(object, context);
    if (!mirrorPlane) {
        return std::nullopt;
    }

    gp_Trsf mirrorTransform;
    mirrorTransform.SetMirror(gp_Ax2(mirrorPlane->point, mirrorPlane->normal));
    return std::vector<gp_Trsf>{mirrorTransform};
}

std::optional<TransformSource> solidSource(const std::string& objectName, runtime::ComputeContext& context)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(objectName);
    return TransformSource{objectName,
                           shapeIt->second.shape,
                           namedShapeIt != context.namedShapes.end() ? std::optional<topo::NamedShape>{namedShapeIt->second}
                                                                     : std::nullopt};
}

std::optional<TransformSource> resolveSupportSource(const document::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const std::vector<document::Link>& originals)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::getBaseObject(), returns BaseFeature if present, otherwise the first Original.
    if (document::propertyValue(object, "BaseFeature") != nullptr) {
        const auto baseFeature = document::readLink(object, "BaseFeature");
        if (!baseFeature) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_property",
                                   "Transformed BaseFeature must link to a solid",
                                   object.name,
                                   "BaseFeature");
            return std::nullopt;
        }
        const auto support = solidSource(baseFeature->object, context);
        if (!support) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "BaseFeature target " + baseFeature->object + " did not produce a solid",
                                   object.name,
                                   "BaseFeature",
                                   "runtime",
                                   baseFeature->object);
            return std::nullopt;
        }
        return support;
    }

    if (originals.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Transformed Originals must contain at least one feature or BaseFeature must link to a support solid",
                               object.name,
                               "Originals");
        return std::nullopt;
    }

    const auto support = solidSource(originals.front().object, context);
    if (!support) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "First Original " + originals.front().object + " did not produce a solid support",
                               object.name,
                               "Originals",
                               "runtime",
                               originals.front().object);
        return std::nullopt;
    }
    return support;
}

topo::NamedShape transformedCopyNamedShape(const std::string& owner,
                                           const TopoDS_Shape& resultShape,
                                           const TransformSource& source)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementTransform(), applies BRepBuilderAPI_Transform then calls
    // "copyElementMap(tmp, op)" instead of guessing geometry ownership from the output.
    topo::NamedShape namedShape = topo::indexedNamedShapeForObject(owner, resultShape);
    for (const auto& [elementName, element] : namedShape.elements) {
        namedShape.elementMap[source.owner + "." + elementName] = elementName;
        namedShape.history.push_back(topo::ElementHistory{topo::ElementHistoryKind::Modified,
                                                          elementName,
                                                          {source.owner + "." + elementName}});
    }
    if (source.namedShape) {
        for (const auto& [stableName, currentName] : source.namedShape->elementMap) {
            if (namedShape.elements.count(currentName) != 0U) {
                namedShape.elementMap[stableName] = currentName;
            }
        }
    }
    return namedShape;
}

std::optional<TransformSource> transformedCopy(const std::string& owner,
                                               const TransformSource& source,
                                               const gp_Trsf& transform,
                                               const document::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const std::string& property)
{
    try {
        BRepBuilderAPI_Transform maker(source.shape, transform, true);
        maker.Build();
        if (!maker.IsDone()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "execution_failed",
                                   "OCCT could not transform " + source.owner,
                                   object.name,
                                   property);
            return std::nullopt;
        }
        TopoDS_Shape transformed = maker.Shape();
        topo::NamedShape namedShape = transformedCopyNamedShape(owner, transformed, source);
        return TransformSource{owner, transformed, namedShape};
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               failure.GetMessageString(),
                               object.name,
                               property);
        return std::nullopt;
    }
}

topo::NamedShapeSource namedShapeSource(const TransformSource& source)
{
    return topo::NamedShapeSource{source.namedShape ? source.namedShape->owner : source.owner,
                                  source.shape,
                                  source.namedShape ? &*source.namedShape : nullptr};
}

std::optional<TransformSource> fuseOrCutTransformedSource(const document::DocumentObject& object,
                                                          runtime::ComputeContext& context,
                                                          const TransformSource& support,
                                                          const TransformSource& tool,
                                                          topo::BooleanOperation operation,
                                                          const std::string& property)
{
    const auto build = topo::makeElementBooleanFromSources(object.name,
                                                           {namedShapeSource(support), namedShapeSource(tool)},
                                                           operation);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Transformed boolean operation failed: " + build.error,
                               object.name,
                               property);
        return std::nullopt;
    }
    return TransformSource{object.name,
                           build.shape,
                           build.namedShape ? *build.namedShape : topo::indexedNamedShapeForObject(object.name, build.shape)};
}

std::optional<TransformApplication> applyFeatureTransforms(const document::DocumentObject& object,
                                                           runtime::ComputeContext& context,
                                                           const std::vector<document::Link>& originals,
                                                           const std::vector<gp_Trsf>& copyTransforms)
{
    auto support = resolveSupportSource(object, context, originals);
    if (!support) {
        return std::nullopt;
    }

    TransformSource current = *support;
    std::vector<std::string> originalNames;
    int transformedIndex = 1;
    for (const document::Link& original : originals) {
        originalNames.push_back(original.object);
        const auto addSubIt = context.addSubShapes.find(original.object);
        if (addSubIt == context.addSubShapes.end()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_type",
                                   "Only additive and subtractive features can be transformed",
                                   object.name,
                                   "Originals",
                                   "runtime",
                                   original.object);
            return std::nullopt;
        }

        for (const gp_Trsf& transform : copyTransforms) {
            if (addSubIt->second.addShape) {
                TransformSource originalAdd{original.object,
                                            *addSubIt->second.addShape,
                                            context.namedShapes.count(original.object) != 0U
                                                ? std::optional<topo::NamedShape>{context.namedShapes.at(original.object)}
                                                : std::nullopt};
                auto transformedTool = transformedCopy(object.name + ".Transform" + std::to_string(transformedIndex++),
                                                       originalAdd,
                                                       transform,
                                                       object,
                                                       context,
                                                       "Originals");
                if (!transformedTool) {
                    return std::nullopt;
                }
                auto fused =
                    fuseOrCutTransformedSource(object, context, current, *transformedTool, topo::BooleanOperation::Fuse, "Originals");
                if (!fused) {
                    return std::nullopt;
                }
                current = *fused;
            }

            if (addSubIt->second.subShape) {
                TransformSource originalSub{original.object,
                                            *addSubIt->second.subShape,
                                            context.namedShapes.count(original.object) != 0U
                                                ? std::optional<topo::NamedShape>{context.namedShapes.at(original.object)}
                                                : std::nullopt};
                auto transformedTool = transformedCopy(object.name + ".Transform" + std::to_string(transformedIndex++),
                                                       originalSub,
                                                       transform,
                                                       object,
                                                       context,
                                                       "Originals");
                if (!transformedTool) {
                    return std::nullopt;
                }
                auto cut =
                    fuseOrCutTransformedSource(object, context, current, *transformedTool, topo::BooleanOperation::Cut, "Originals");
                if (!cut) {
                    return std::nullopt;
                }
                current = *cut;
            }
        }
    }

    topo::NamedShape resultNamedShape =
        current.namedShape ? *current.namedShape : topo::indexedNamedShapeForObject(current.owner, current.shape);
    if (resultNamedShape.owner != object.name) {
        resultNamedShape = topo::namedShapeForPreservedSources(object.name, current.shape, {namedShapeSource(current)});
    }
    return TransformApplication{current.shape, resultNamedShape, originalNames};
}

std::optional<TransformApplication> applyWholeShapeTransforms(const document::DocumentObject& object,
                                                              runtime::ComputeContext& context,
                                                              const std::vector<document::Link>& supportLinks,
                                                              const std::vector<gp_Trsf>& copyTransforms)
{
    auto support = resolveSupportSource(object, context, supportLinks);
    if (!support) {
        return std::nullopt;
    }

    TransformSource current = *support;
    int transformedIndex = 1;
    for (const gp_Trsf& transform : copyTransforms) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
        // ::Transformed::execute(), Mode::WholeShape calls getTransformedCompShape(supportShape,
        // supportShape) and then makeElementFuse(shapes), keeping the untransformed support plus
        // transformed support copies.
        auto transformedTool = transformedCopy(object.name + ".Transform" + std::to_string(transformedIndex++),
                                               *support,
                                               transform,
                                               object,
                                               context,
                                               "TransformMode");
        if (!transformedTool) {
            return std::nullopt;
        }
        auto fused =
            fuseOrCutTransformedSource(object, context, current, *transformedTool, topo::BooleanOperation::Fuse, "TransformMode");
        if (!fused) {
            return std::nullopt;
        }
        current = *fused;
    }

    topo::NamedShape resultNamedShape =
        current.namedShape ? *current.namedShape : topo::indexedNamedShapeForObject(current.owner, current.shape);
    if (resultNamedShape.owner != object.name) {
        resultNamedShape = topo::namedShapeForPreservedSources(object.name, current.shape, {namedShapeSource(current)});
    }

    std::vector<std::string> supportNames;
    for (const document::Link& link : supportLinks) {
        supportNames.push_back(link.object);
    }
    return TransformApplication{current.shape, resultNamedShape, supportNames};
}

std::optional<TransformedBuild> buildMirroredFeatures(const document::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    const std::string mode = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const auto transforms = mirroredTransforms(object, context);
    if (!transforms) {
        return std::nullopt;
    }

    const std::vector<document::Link> originals = document::readLinks(object, "Originals");
    if (mode == "Whole shape") {
        const auto application = applyWholeShapeTransforms(object, context, originals, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild{application->shape, application->namedShape, mode, application->originals};
    }
    if (mode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported Mirrored TransformMode " + mode,
                               object.name,
                               "TransformMode");
        return std::nullopt;
    }

    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild{application->shape, application->namedShape, mode, application->originals};
}

std::optional<gp_Dir> directionFromShape(const TopoDS_Shape& shape,
                                         const document::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::string& property)
{
    if (shape.ShapeType() == TopAbs_EDGE) {
        const TopoDS_Edge edge = TopoDS::Edge(shape);
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Line) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_direction",
                                   property + " edge must be a straight line",
                                   object.name,
                                   property);
            return std::nullopt;
        }
        return curve.Line().Direction();
    }
    if (shape.ShapeType() == TopAbs_FACE) {
        const TopoDS_Face face = TopoDS::Face(shape);
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_direction",
                                   property + " face must be planar",
                                   object.name,
                                   property);
            return std::nullopt;
        }
        return surface.Plane().Axis().Direction();
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_subshape_kind",
                           property + " must reference a datum line, datum plane, straight edge, or planar face",
                           object.name,
                           property);
    return std::nullopt;
}

std::optional<RotationAxis> axisFromShape(const TopoDS_Shape& shape,
                                          const document::DocumentObject& object,
                                          runtime::ComputeContext& context,
                                          const std::string& property)
{
    if (shape.ShapeType() != TopAbs_EDGE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               property + " must reference a datum line or an edge",
                               object.name,
                               property);
        return std::nullopt;
    }

    const TopoDS_Edge edge = TopoDS::Edge(shape);
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Line) {
        const gp_Lin line = curve.Line();
        return RotationAxis{line.Location(), line.Direction()};
    }
    if (curve.GetType() == GeomAbs_Circle) {
        const gp_Circ circle = curve.Circle();
        return RotationAxis{circle.Location(), circle.Axis().Direction()};
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "invalid_axis",
                           property + " edge must be a straight line, circle, or arc of circle",
                           object.name,
                           property);
    return std::nullopt;
}

std::optional<gp_Pnt> readSketchPoint2d(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() < 2U || !value[0].is_number() || !value[1].is_number()) {
        return std::nullopt;
    }
    return gp_Pnt(value[0].get<double>(), value[1].get<double>(), 0.0);
}

std::optional<int> parseSketchConstructionAxisIndex(const std::string& subname)
{
    constexpr std::string_view prefix = "Axis";
    if (subname.rfind(prefix, 0) != 0 || subname.size() == prefix.size()) {
        return std::nullopt;
    }
    int index = 0;
    for (std::size_t position = prefix.size(); position < subname.size(); ++position) {
        const char ch = subname[position];
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        index = index * 10 + (ch - '0');
    }
    return index;
}

bool isSketchAxisSubname(const std::string& subname)
{
    return subname == "H_Axis" || subname == "V_Axis" || subname == "N_Axis"
        || parseSketchConstructionAxisIndex(subname).has_value();
}

std::optional<SketchAxis> sketchAxisFromObject(const document::DocumentObject& sketch,
                                               const std::string& subname,
                                               const document::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Part2DObject.cpp
    // ::Part2DObject::getAxis(), returns "H_Axis"=(1,0,0), "V_Axis"=(0,1,0),
    // and "N_Axis"=(0,0,1).
    SketchAxis axis{gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)};
    bool resolved = true;
    if (subname == "H_Axis") {
        axis.direction = gp_Dir(1.0, 0.0, 0.0);
    }
    else if (subname == "V_Axis") {
        axis.direction = gp_Dir(0.0, 1.0, 0.0);
    }
    else if (subname == "N_Axis") {
        axis.direction = gp_Dir(0.0, 0.0, 1.0);
    }
    else if (const auto axisIndex = parseSketchConstructionAxisIndex(subname)) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp
        // ::SketchObject::getAxis(), iterates construction "Part::GeomLineSegment" items and
        // returns Base::Axis(start, end - start) for AxisN indices starting at 0.
        const nlohmann::json* geometry = propertyPayload(sketch, "Geometry");
        if (geometry == nullptr || !geometry->is_array()) {
            resolved = false;
        }
        else {
            int constructionLineIndex = 0;
            resolved = false;
            for (const auto& item : *geometry) {
                if (!item.is_object() || item.value("kind", "") != "LineSegment" || !item.value("construction", false)) {
                    continue;
                }
                if (constructionLineIndex++ != *axisIndex) {
                    continue;
                }
                const auto startIt = item.find("start");
                const auto endIt = item.find("end");
                const auto start = startIt != item.end() ? readSketchPoint2d(*startIt) : std::nullopt;
                const auto end = endIt != item.end() ? readSketchPoint2d(*endIt) : std::nullopt;
                if (!start || !end || start->Distance(*end) < Precision::Confusion()) {
                    break;
                }
                axis.point = *start;
                axis.direction = gp_Dir(gp_Vec(*start, *end));
                resolved = true;
                break;
            }
        }
    }
    else {
        return std::nullopt;
    }

    if (!resolved) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " target " + sketch.name + " has no sketch axis " + subname,
                               object.name,
                               property,
                               "runtime",
                               sketch.name,
                               subname);
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(sketch.name);
    if (placementIt != context.globalPlacements.end()) {
        axis.point.Transform(placementIt->second);
        axis.direction.Transform(placementIt->second);
    }
    return axis;
}

std::optional<SketchAxis> resolveSketchAxis(const document::Link& link,
                                            const document::DocumentObject& object,
                                            runtime::ComputeContext& context,
                                            const std::string& property)
{
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch) {
        return std::nullopt;
    }
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               property + " must reference one Sketch axis subname",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }
    const auto documentIt = context.documentObjects.find(link.object);
    if (documentIt == context.documentObjects.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link.object + " is missing from the document graph",
                               object.name,
                               property,
                               "runtime",
                               link.object);
        return std::nullopt;
    }
    return sketchAxisFromObject(*documentIt->second, link.subnames.front(), object, context, property);
}

std::optional<gp_Dir> resolveLinearPatternDirection(const document::DocumentObject& object,
                                                    runtime::ComputeContext& context,
                                                    const std::string& property)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
    // ::LinearPattern::getDirectionFromProperty(), accepts a datum line/plane, sketch axis,
    // straight edge or planar face, then transforms direction into the object's local coordinates.
    const auto link = document::readLink(object, property);
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               property + " must link to a direction reference",
                               object.name,
                               property);
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               property + " target " + link->object + " did not produce a shape",
                               object.name,
                               property,
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch && link->subnames.size() == 1U
        && isSketchAxisSubname(link->subnames.front())) {
        const auto sketchAxis = resolveSketchAxis(*link, object, context, property);
        if (!sketchAxis) {
            return std::nullopt;
        }
        return sketchAxis->direction;
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            return directionFromShape(explorer.Current(), object, context, property);
        }
    }
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPlane && link->subnames.empty()) {
        for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
            return directionFromShape(explorer.Current(), object, context, property);
        }
    }

    const auto subshape = resolveDirectionSubshape(*link, object, context, property);
    if (!subshape) {
        return std::nullopt;
    }
    return directionFromShape(*subshape, object, context, property);
}

std::optional<RotationAxis> resolvePolarPatternAxis(const document::DocumentObject& object,
                                                    runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp
    // ::PolarPattern::getRotation(), accepts a Sketch Part2DObject axis, datum line, or a Part
    // feature edge; the edge may be a straight line, circle, or arc of circle.
    const auto link = document::readLink(object, "Axis");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Axis must link to a rotation reference",
                               object.name,
                               "Axis");
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Axis target " + link->object + " did not produce a shape",
                               object.name,
                               "Axis",
                               "runtime",
                               link->object);
        return std::nullopt;
    }

    std::optional<RotationAxis> axis;
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch && link->subnames.size() == 1U
        && isSketchAxisSubname(link->subnames.front())) {
        const auto sketchAxis = resolveSketchAxis(*link, object, context, "Axis");
        if (!sketchAxis) {
            return std::nullopt;
        }
        axis = RotationAxis{sketchAxis->point, sketchAxis->direction};
    }
    else if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            axis = axisFromShape(explorer.Current(), object, context, "Axis");
            break;
        }
    }
    else {
        const auto subshape = resolveDirectionSubshape(*link, object, context, "Axis");
        if (!subshape) {
            return std::nullopt;
        }
        axis = axisFromShape(*subshape, object, context, "Axis");
    }
    if (!axis) {
        return std::nullopt;
    }
    if (readBoolProperty(object, "Reversed")) {
        axis->direction.Reverse();
    }
    return axis;
}

std::optional<std::vector<gp_Vec>> linearPatternSteps(const document::DocumentObject& object,
                                                      runtime::ComputeContext& context,
                                                      const std::string& suffix,
                                                      int occurrences)
{
    std::vector<gp_Vec> steps{gp_Vec()};
    steps.reserve(static_cast<std::size_t>(std::max(occurrences, 1)));
    if (occurrences <= 1) {
        return steps;
    }

    const std::string directionProperty = "Direction" + suffix;
    auto direction = resolveLinearPatternDirection(object, context, directionProperty);
    if (!direction) {
        return std::nullopt;
    }
    if (readBoolProperty(object, "Reversed" + suffix)) {
        direction->Reverse();
    }

    const std::string modeProperty = "Mode" + suffix;
    const std::string mode = readEnumProperty(object, modeProperty, {"Extent", "Spacing"}, "Extent");
    if (mode == "Extent") {
        const std::string lengthProperty = "Length" + suffix;
        const double length = readNumberProperty(object, lengthProperty, 100.0);
        if (length < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Pattern length too small",
                                   object.name,
                                   lengthProperty);
            return std::nullopt;
        }
        const double stepDistance = length / static_cast<double>(occurrences - 1);
        for (int index = 1; index < occurrences; ++index) {
            steps.push_back(gp_Vec(*direction) * (stepDistance * index));
        }
        return steps;
    }

    if (mode == "Spacing") {
        const std::string offsetProperty = "Offset" + suffix;
        const double offset = readNumberProperty(object, offsetProperty, 10.0);
        if (offset < Precision::Confusion()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "Pattern offset too small",
                                   object.name,
                                   offsetProperty);
            return std::nullopt;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
        // ::LinearPattern::calculateSteps(), uses spacing priority "individual spacing > pattern > global offset".
        const std::vector<double> spacings = normalizedSpacingList(object, "Spacings" + suffix, offset, occurrences);
        const std::vector<double> spacingPattern = readNumberListProperty(object, "SpacingPattern" + suffix);
        const bool usePattern = spacingPattern.size() > 1U;
        double cumulativeDistance = 0.0;
        for (int index = 1; index < occurrences; ++index) {
            double spacing = offset;
            const auto spacingIndex = static_cast<std::size_t>(index - 1);
            if (spacingIndex < spacings.size() && std::abs(spacings[spacingIndex] + 1.0) > Precision::Confusion()) {
                spacing = spacings[spacingIndex];
            }
            else if (usePattern) {
                spacing = spacingPattern[static_cast<std::size_t>(std::fmod(index - 1, spacingPattern.size()))];
            }
            cumulativeDistance += spacing;
            steps.push_back(gp_Vec(*direction) * cumulativeDistance);
        }
        return steps;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Unsupported LinearPattern Mode " + mode,
                           object.name,
                           modeProperty);
    return std::nullopt;
}

std::vector<gp_Trsf> combinedLinearPatternTransforms(const std::vector<gp_Vec>& firstSteps,
                                                     const std::vector<gp_Vec>& secondSteps)
{
    std::vector<gp_Trsf> transforms;
    for (const gp_Vec& firstStep : firstSteps) {
        for (const gp_Vec& secondStep : secondSteps) {
            const gp_Vec step = firstStep + secondStep;
            if (step.Magnitude() < Precision::Confusion()) {
                continue;
            }
            gp_Trsf transform;
            transform.SetTranslation(step);
            transforms.push_back(transform);
        }
    }
    return transforms;
}

std::optional<std::vector<gp_Trsf>> polarPatternTransforms(const document::DocumentObject& object,
                                                           runtime::ComputeContext& context,
                                                           int occurrences)
{
    std::vector<gp_Trsf> transforms;
    if (occurrences <= 1) {
        return transforms;
    }

    const auto axis = resolvePolarPatternAxis(object, context);
    if (!axis) {
        return std::nullopt;
    }
    const gp_Ax1 rotationAxis(axis->point, axis->direction);

    const std::string mode = readEnumProperty(object, "Mode", {"Extent", "Spacing"}, "Extent");
    if (mode == "Extent") {
        double angleDegrees = readNumberProperty(object, "Angle", 360.0);
        if (std::abs(angleDegrees - 360.0) < Precision::Confusion()) {
            angleDegrees /= static_cast<double>(occurrences);
        }
        else {
            angleDegrees /= static_cast<double>(occurrences - 1);
        }
        const double angleRadians = degreesToRadians(angleDegrees);
        if (std::abs(angleRadians) < Precision::Angular()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_angle",
                                   "Pattern angle cannot be null",
                                   object.name,
                                   "Angle");
            return std::nullopt;
        }

        for (int index = 1; index < occurrences; ++index) {
            gp_Trsf transform;
            transform.SetRotation(rotationAxis, angleRadians * index);
            transforms.push_back(transform);
        }
        return transforms;
    }

    if (mode == "Spacing") {
        const double offset = readNumberProperty(object, "Offset", 120.0);
        const std::vector<double> spacings = normalizedSpacingList(object, "Spacings", offset, occurrences);
        const std::vector<double> spacingPattern = readNumberListProperty(object, "SpacingPattern");
        const bool usePattern = spacingPattern.size() > 1U;
        double cumulativeAngle = 0.0;
        for (int index = 1; index < occurrences; ++index) {
            double spacing = offset;
            const auto spacingIndex = static_cast<std::size_t>(index - 1);
            if (spacingIndex < spacings.size() && std::abs(spacings[spacingIndex] + 1.0) > Precision::Confusion()) {
                spacing = spacings[spacingIndex];
            }
            else if (usePattern) {
                spacing = spacingPattern[static_cast<std::size_t>(std::fmod(index - 1, spacingPattern.size()))];
            }
            cumulativeAngle += degreesToRadians(spacing);
            gp_Trsf transform;
            transform.SetRotation(rotationAxis, cumulativeAngle);
            transforms.push_back(transform);
        }
        return transforms;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_property",
                           "Unsupported PolarPattern Mode " + mode,
                           object.name,
                           "Mode");
    return std::nullopt;
}

std::optional<std::vector<gp_Trsf>> scaledTransforms(const document::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const std::vector<document::Link>& originals)
{
    const double factor = readNumberProperty(object, "Factor", 2.0);
    if (factor < Precision::Confusion()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Scaling factor too small",
                               object.name,
                               "Factor");
        return std::nullopt;
    }

    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(2);
    if (occurrences < 2) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "Scaled requires at least two occurrences",
                               object.name,
                               "Occurrences");
        return std::nullopt;
    }
    gp_Pnt centerOfMass;
    if (!originals.empty()) {
        const auto addSubIt = context.addSubShapes.find(originals.front().object);
        if (addSubIt == context.addSubShapes.end() || (!addSubIt->second.addShape && !addSubIt->second.subShape)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_type",
                                   "Scaled centre of mass requires an additive or subtractive first Original",
                                   object.name,
                                   "Originals",
                                   "runtime",
                                   originals.front().object);
            return std::nullopt;
        }

        const TopoDS_Shape& originalShape =
            addSubIt->second.addShape ? *addSubIt->second.addShape : *addSubIt->second.subShape;
        GProp_GProps props;
        BRepGProp::VolumeProperties(originalShape, props);
        centerOfMass = props.CentreOfMass();
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp
    // ::Scaled::getTransformations(), computes "f = (factor - 1.0) / (occurrences - 1)"
    // and scales around the first original AddSubShape centre of mass; WholeShape passes empty
    // originals through Transformed::getOriginals(), leaving gp_Pnt() as the scale centre.
    const double step = (factor - 1.0) / static_cast<double>(occurrences - 1);
    std::vector<gp_Trsf> transforms;
    for (int index = 1; index < occurrences; ++index) {
        gp_Trsf transform;
        transform.SetScale(centerOfMass, 1.0 + static_cast<double>(index) * step);
        transforms.push_back(transform);
    }
    return transforms;
}

void prependIdentity(std::vector<gp_Trsf>& transforms)
{
    gp_Trsf identity;
    transforms.insert(transforms.begin(), identity);
}

std::optional<gp_Pnt> firstOriginalCenterOfMass(const document::DocumentObject& object,
                                                runtime::ComputeContext& context,
                                                const std::vector<document::Link>& originals)
{
    if (originals.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "MultiTransform Originals must contain at least one feature",
                               object.name,
                               "Originals");
        return std::nullopt;
    }

    const auto addSubIt = context.addSubShapes.find(originals.front().object);
    if (addSubIt == context.addSubShapes.end() || (!addSubIt->second.addShape && !addSubIt->second.subShape)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_type",
                               "MultiTransform centre of mass requires an additive or subtractive first Original",
                               object.name,
                               "Originals",
                               "runtime",
                               originals.front().object);
        return std::nullopt;
    }

    const TopoDS_Shape& originalShape =
        addSubIt->second.addShape ? *addSubIt->second.addShape : *addSubIt->second.subShape;
    GProp_GProps props;
    BRepGProp::VolumeProperties(originalShape, props);
    return props.CentreOfMass();
}

std::optional<TemplateTransforms> childTemplateTransforms(const document::DocumentObject& object,
                                                          runtime::ComputeContext& context,
                                                          const std::vector<document::Link>& originals,
                                                          const document::DocumentObject& child)
{
    std::string mode = readEnumProperty(child, "TransformMode", {"Features", "Whole shape"}, "Features");
    if (mode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "MultiTransform child transformations must use TransformMode=Features",
                               object.name,
                               "Transformations",
                               "runtime",
                               child.name);
        return std::nullopt;
    }

    TemplateTransforms result;
    if (child.typeId == "PartDesign::Mirrored") {
        auto transforms = mirroredTransforms(child, context);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
    }
    else if (child.typeId == "PartDesign::LinearPattern") {
        const int occurrences = readIntegerProperty(child, "Occurrences").value_or(2);
        const int occurrences2 = readIntegerProperty(child, "Occurrences2").value_or(1);
        if (occurrences < 1 || occurrences2 < 1) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "LinearPattern requires at least one occurrence",
                                   object.name,
                                   "Transformations",
                                   "runtime",
                                   child.name);
            return std::nullopt;
        }
        const auto firstSteps = linearPatternSteps(child, context, "", occurrences);
        if (!firstSteps) {
            return std::nullopt;
        }
        const auto secondSteps = linearPatternSteps(child, context, "2", occurrences2);
        if (!secondSteps) {
            return std::nullopt;
        }
        result.transforms = combinedLinearPatternTransforms(*firstSteps, *secondSteps);
    }
    else if (child.typeId == "PartDesign::PolarPattern") {
        const int occurrences = readIntegerProperty(child, "Occurrences").value_or(3);
        if (occurrences < 1) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_length",
                                   "PolarPattern requires at least one occurrence",
                                   object.name,
                                   "Transformations",
                                   "runtime",
                                   child.name);
            return std::nullopt;
        }
        auto transforms = polarPatternTransforms(child, context, occurrences);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
    }
    else if (child.typeId == "PartDesign::Scaled") {
        auto transforms = scaledTransforms(child, context, originals);
        if (!transforms) {
            return std::nullopt;
        }
        result.transforms = *transforms;
        result.scaled = true;
    }
    else {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_type",
                               "MultiTransform child must be a transformed feature",
                               object.name,
                               "Transformations",
                               "runtime",
                               child.name);
        return std::nullopt;
    }
    prependIdentity(result.transforms);
    return result;
}

std::optional<std::vector<gp_Trsf>> multiTransformTransforms(const document::DocumentObject& object,
                                                             runtime::ComputeContext& context,
                                                             const std::vector<document::Link>& originals)
{
    const std::vector<document::Link> transformationLinks = document::readLinks(object, "Transformations");
    if (transformationLinks.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "MultiTransform Transformations must contain at least one transformed feature",
                               object.name,
                               "Transformations");
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::getOriginals(), returns an empty vector for Mode::WholeShape. MultiTransform
    // still runs getTransformations(originals), so FeatureMultiTransform.cpp leaves "gp_Pnt cog"
    // at its default origin when originals is empty.
    gp_Pnt baseCog;
    if (!originals.empty()) {
        const auto originalCog = firstOriginalCenterOfMass(object, context, originals);
        if (!originalCog) {
            return std::nullopt;
        }
        baseCog = *originalCog;
    }

    std::vector<gp_Trsf> result;
    std::vector<gp_Pnt> cogs;
    for (const auto& link : transformationLinks) {
        const auto childIt = context.documentObjects.find(link.object);
        if (childIt == context.documentObjects.end()) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "Transformation target " + link.object + " does not exist",
                                   object.name,
                                   "Transformations",
                                   "runtime",
                                   link.object);
            return std::nullopt;
        }
        const auto childTransforms = childTemplateTransforms(object, context, originals, *childIt->second);
        if (!childTransforms) {
            return std::nullopt;
        }

        if (result.empty()) {
            result = childTransforms->transforms;
            cogs.clear();
            for (const gp_Trsf& transform : result) {
                cogs.push_back(baseCog.Transformed(transform));
            }
            continue;
        }

        const std::vector<gp_Trsf> oldTransformations = result;
        const std::vector<gp_Pnt> oldCogs = cogs;
        result.clear();
        cogs.clear();

        if (childTransforms->scaled) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp
            // ::MultiTransform::getTransformations(), applies Scaled children by a diagonal
            // method and recreates each scale transform around the current slice COG.
            if (childTransforms->transforms.empty()
                || oldTransformations.size() % childTransforms->transforms.size() != 0U) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_length",
                                       "Number of occurrences must be a divisor of previous number of occurrences",
                                       object.name,
                                       "Transformations",
                                       "runtime",
                                       childIt->second->name);
                return std::nullopt;
            }
            const std::size_t sliceLength = oldTransformations.size() / childTransforms->transforms.size();
            std::size_t oldIndex = 0;
            for (const gp_Trsf& newTransform : childTransforms->transforms) {
                for (std::size_t slice = 0; slice < sliceLength; ++slice) {
                    gp_Trsf transform;
                    const double factor = newTransform.ScaleFactor();
                    if (factor > Precision::Confusion()) {
                        transform.SetScale(oldCogs[oldIndex], factor);
                        transform = transform * oldTransformations[oldIndex];
                        cogs.push_back(oldCogs[oldIndex]);
                    }
                    else {
                        transform = newTransform * oldTransformations[oldIndex];
                        cogs.push_back(oldCogs[oldIndex].Transformed(newTransform));
                    }
                    result.push_back(transform);
                    ++oldIndex;
                }
            }
        }
        else {
            for (const gp_Trsf& newTransform : childTransforms->transforms) {
                for (std::size_t oldIndex = 0; oldIndex < oldTransformations.size(); ++oldIndex) {
                    result.push_back(newTransform * oldTransformations[oldIndex]);
                    cogs.push_back(oldCogs[oldIndex].Transformed(newTransform));
                }
            }
        }
    }

    if (!result.empty()) {
        result.erase(result.begin());
    }
    return result;
}

std::optional<TransformedBuild> buildLinearPatternFeatures(const document::DocumentObject& object,
                                                           runtime::ComputeContext& context)
{
    const std::string transformMode = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(2);
    const int occurrences2 = readIntegerProperty(object, "Occurrences2").value_or(1);
    if (occurrences < 1 || occurrences2 < 1) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "LinearPattern requires at least one occurrence",
                               object.name,
                               occurrences < 1 ? "Occurrences" : "Occurrences2");
        return std::nullopt;
    }

    const auto firstSteps = linearPatternSteps(object, context, "", occurrences);
    if (!firstSteps) {
        return std::nullopt;
    }
    const auto secondSteps = linearPatternSteps(object, context, "2", occurrences2);
    if (!secondSteps) {
        return std::nullopt;
    }

    if (transformMode == "Whole shape") {
        const std::vector<document::Link> supportLinks = document::readLinks(object, "Originals");
        const auto application =
            applyWholeShapeTransforms(object, context, supportLinks, combinedLinearPatternTransforms(*firstSteps, *secondSteps));
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
    }
    if (transformMode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported LinearPattern TransformMode " + transformMode,
                               object.name,
                               "TransformMode");
        return std::nullopt;
    }

    const std::vector<document::Link> originals = document::readLinks(object, "Originals");
    const auto application = applyFeatureTransforms(object,
                                                    context,
                                                    originals,
                                                    combinedLinearPatternTransforms(*firstSteps, *secondSteps));
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
}

std::optional<TransformedBuild> buildMultiTransformFeatures(const document::DocumentObject& object,
                                                            runtime::ComputeContext& context)
{
    const std::string transformMode = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const std::vector<document::Link> originals = document::readLinks(object, "Originals");
    if (transformMode == "Whole shape") {
        const auto transforms = multiTransformTransforms(object, context, {});
        if (!transforms) {
            return std::nullopt;
        }
        const auto application = applyWholeShapeTransforms(object, context, originals, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
    }
    if (transformMode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported MultiTransform TransformMode " + transformMode,
                               object.name,
                               "TransformMode");
        return std::nullopt;
    }

    const auto transforms = multiTransformTransforms(object, context, originals);
    if (!transforms) {
        return std::nullopt;
    }
    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
}

std::optional<TransformedBuild> buildPolarPatternFeatures(const document::DocumentObject& object,
                                                          runtime::ComputeContext& context)
{
    const std::string transformMode = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const int occurrences = readIntegerProperty(object, "Occurrences").value_or(3);
    if (occurrences < 1) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_length",
                               "PolarPattern requires at least one occurrence",
                               object.name,
                               "Occurrences");
        return std::nullopt;
    }

    const auto transforms = polarPatternTransforms(object, context, occurrences);
    if (!transforms) {
        return std::nullopt;
    }
    if (transformMode == "Whole shape") {
        const std::vector<document::Link> supportLinks = document::readLinks(object, "Originals");
        const auto application = applyWholeShapeTransforms(object, context, supportLinks, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
    }
    if (transformMode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported PolarPattern TransformMode " + transformMode,
                               object.name,
                               "TransformMode");
        return std::nullopt;
    }
    const std::vector<document::Link> originals = document::readLinks(object, "Originals");
    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
}

std::optional<TransformedBuild> buildScaledFeatures(const document::DocumentObject& object,
                                                    runtime::ComputeContext& context)
{
    const std::string transformMode = readEnumProperty(object, "TransformMode", {"Features", "Whole shape"}, "Features");
    const std::vector<document::Link> originals = document::readLinks(object, "Originals");
    const auto transforms =
        scaledTransforms(object, context, transformMode == "Whole shape" ? std::vector<document::Link>{} : originals);
    if (!transforms) {
        return std::nullopt;
    }
    if (transformMode == "Whole shape") {
        const auto application = applyWholeShapeTransforms(object, context, originals, *transforms);
        if (!application) {
            return std::nullopt;
        }
        return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
    }
    if (transformMode != "Features") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_property",
                               "Unsupported Scaled TransformMode " + transformMode,
                               object.name,
                               "TransformMode");
        return std::nullopt;
    }
    const auto application = applyFeatureTransforms(object, context, originals, *transforms);
    if (!application) {
        return std::nullopt;
    }
    return TransformedBuild{application->shape, application->namedShape, transformMode, application->originals};
}

void publishTransformedResult(const document::DocumentObject& object,
                              runtime::ComputeContext& context,
                              const TransformedBuild& result,
                              const std::string& transformedKind)
{
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, result.shape};
    context.namedShapes[object.name] = result.namedShape;
    context.mesh[object.name] = geometry::meshForShape(result.shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(result.shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"body_mode", "replace"},
        {"transformed", transformedKind},
        {"transform_mode", result.mode},
        {"originals", result.originals},
        {"bbox", geometry::bboxForShape(result.shape)},
        {"volume", geometry::volumeForShape(result.shape)},
        {"kernel", geometry::kernelVersion()},
    };
    if (result.refineApplied) {
        context.objects[object.name]["refine"] = "applied";
    }
}

bool applyTransformedRefine(const document::DocumentObject& object,
                            runtime::ComputeContext& context,
                            TransformedBuild& result)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::execute(), after Features / WholeShape fuse-cut composition calls
    // "supportShape = refineShapeIfActive((supportShape))" before setting Shape.
    const auto refined = applyRefineProperty(object, context, result.shape, result.namedShape);
    if (!refined) {
        return false;
    }
    result.shape = refined->shape;
    if (refined->namedShape) {
        result.namedShape = *refined->namedShape;
    }
    result.refineApplied = refined->applied;
    return true;
}

}  // namespace

void executeMirrored(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp::Mirrored::getTransformations()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Originals",
                                      "TransformMode",
                                      "MirrorPlane",
                                      "BaseFeature",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildMirroredFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "mirrored");
}

void executeMultiTransform(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Originals",
                                      "TransformMode",
                                      "Transformations",
                                      "BaseFeature",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    auto result = buildMultiTransformFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "multi_transform");
}

void executeLinearPattern(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp::LinearPattern::getTransformations()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Originals",
                                      "TransformMode",
                                      "Direction",
                                      "Reversed",
                                      "Mode",
                                      "Length",
                                      "Offset",
                                      "Spacings",
                                      "SpacingPattern",
                                      "Occurrences",
                                      "Direction2",
                                      "Reversed2",
                                      "Mode2",
                                      "Length2",
                                      "Offset2",
                                      "Spacings2",
                                      "SpacingPattern2",
                                      "Occurrences2",
                                      "BaseFeature",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildLinearPatternFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "linear_pattern");
}

void executePolarPattern(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp::PolarPattern::getTransformations()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Originals",
                                      "TransformMode",
                                      "Axis",
                                      "Reversed",
                                      "Mode",
                                      "Angle",
                                      "Offset",
                                      "Spacings",
                                      "SpacingPattern",
                                      "Occurrences",
                                      "BaseFeature",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildPolarPatternFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "polar_pattern");
}

void executeScaled(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp::Scaled::getTransformations()
    if (!rejectUnsupportedProperties(object,
                                     context,
                                     {"Originals",
                                      "TransformMode",
                                      "Factor",
                                      "Occurrences",
                                      "BaseFeature",
                                      "Refine",
                                      "FuzzyTolerance"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (isTransformationTemplate(object, context)) {
        publishTransformationTemplate(object, context);
        return;
    }

    auto result = buildScaledFeatures(object, context);
    if (!result) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (!applyTransformedRefine(object, context, *result)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    publishTransformedResult(object, context, *result, "scaled");
}

}  // namespace cad_core::features
