#include "cad_core/part_design/feature_linear_pattern.h"
#include "cad_core/part_design/feature_mirrored.h"
#include "cad_core/part_design/feature_multi_transform.h"
#include "cad_core/part_design/feature_polar_pattern.h"
#include "cad_core/part_design/feature_scaled.h"

#include "datum_plane_reference.h"
#include "feature_transformed_support.h"

#include "cad_core/part/edge_axis.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <BRepGProp.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GProp_GProps.hxx>
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
#include <gp_Dir.hxx>
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

namespace cad_core::part_design
{

namespace transformed_detail
{

const nlohmann::json* propertyPayload(const app::DocumentObject& object, const std::string& property)
{
    const auto* value = app::propertyValue(object, property);
    if (value == nullptr) {
        return nullptr;
    }
    if (value->raw.is_object() && value->raw.contains("PropertyType")
        && value->raw.contains("value")) {
        return &value->raw.at("value");
    }
    return &value->raw;
}

std::string readEnumProperty(
    const app::DocumentObject& object,
    const std::string& property,
    const std::vector<std::string>& values,
    const std::string& fallback
)
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

std::optional<int> readIntegerProperty(const app::DocumentObject& object, const std::string& property)
{
    const nlohmann::json* payload = propertyPayload(object, property);
    if (payload == nullptr || !payload->is_number_integer()) {
        return std::nullopt;
    }
    return payload->get<int>();
}

double readNumberProperty(const app::DocumentObject& object, const std::string& property, double fallback)
{
    return app::readNumber(object, property).value_or(fallback);
}

bool readBoolProperty(const app::DocumentObject& object, const std::string& property)
{
    return app::readBool(object, property).value_or(false);
}

bool isTransformationTemplate(const app::DocumentObject& object, const runtime::ComputeContext& context)
{
    return context.transformationTemplateObjects.count(object.name) != 0U
        && app::readLinks(object, "Originals").empty()
        && app::propertyValue(object, "BaseFeature") == nullptr;
}

void publishTransformationTemplate(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::isMultiTransformChild(), treats transformed features with default
    // TransformMode=Features and empty Originals as children whose processing happens in
    // MultiTransform.
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

std::vector<double> readNumberListProperty(const app::DocumentObject& object, const std::string& property)
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

std::vector<double> normalizedSpacingList(
    const app::DocumentObject& object,
    const std::string& property,
    double offset,
    int occurrences
)
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

std::string stableSubnameDiagnosticCode(part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Deleted:
            return "deleted_stable_subname";
        case part::ElementResolveStatus::Split:
            return "split_stable_subname";
        case part::ElementResolveStatus::Resolved:
        case part::ElementResolveStatus::Unresolved:
            return "unsupported_stable_subname";
    }
    return "unsupported_stable_subname";
}

std::string stableSubnameDiagnosticMessage(
    const std::string& property,
    const std::string& target,
    const std::string& stableSubname,
    part::ElementResolveStatus status
)
{
    if (status == part::ElementResolveStatus::Deleted) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == part::ElementResolveStatus::Split) {
        return property + " target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return property + " target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

std::optional<TopoDS_Face> resolvePlanarFaceLink(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " must reference exactly one FaceN subshape",
            object.name,
            property,
            "runtime",
            link.object
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link.object + " did not produce a solid",
            object.name,
            property,
            "runtime",
            link.object,
            link.subnames.front()
        );
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front()
                                                                       : std::string {};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved
            = part::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage(property, link.object, stableSubname, resolved.status),
                object.name,
                property,
                "runtime",
                link.object,
                stableSubname
            );
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Invalid " + property + " subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " requires a planar face, not " + part::subshapeKindName(parsed->kind),
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " target " + link.object + " has no subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    return TopoDS::Face(*subshape);
}

std::optional<TopoDS_Shape> resolveDirectionSubshape(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " must reference exactly one EdgeN or FaceN subshape",
            object.name,
            property,
            "runtime",
            link.object
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link.object + " did not produce a shape",
            object.name,
            property,
            "runtime",
            link.object,
            link.subnames.front()
        );
        return std::nullopt;
    }

    std::string currentSubname = link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front()
                                                                       : std::string {};
    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        const auto resolved
            = part::resolveElementReference(namedShapeIt->second, currentSubname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != currentSubname) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage(property, link.object, stableSubname, resolved.status),
                object.name,
                property,
                "runtime",
                link.object,
                stableSubname
            );
            return std::nullopt;
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Invalid " + property + " subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " requires EdgeN or FaceN, not " + part::subshapeKindName(parsed->kind),
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " target " + link.object + " has no subshape " + currentSubname,
            object.name,
            property,
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }

    return subshape;
}

std::optional<TransformSource> solidSource(const std::string& objectName, runtime::ComputeContext& context)
{
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(objectName);
    return TransformSource {
        objectName,
        shapeIt->second.shape,
        namedShapeIt != context.namedShapes.end()
            ? std::optional<part::NamedShape> {namedShapeIt->second}
            : std::nullopt
    };
}

part::NamedShapeSource namedShapeSource(const TransformSource& source)
{
    return part::NamedShapeSource {
        source.namedShape ? source.namedShape->owner : source.owner,
        source.shape,
        source.namedShape ? &*source.namedShape : nullptr
    };
}

std::optional<std::vector<std::string>> bodyGroupNames(const app::DocumentObject& bodyObject)
{
    const std::vector<app::Link> links = app::readLinks(bodyObject, "Group");
    if (links.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> names;
    names.reserve(links.size());
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

std::optional<part::NamedShape> namedShapeForSource(
    const std::string& owner,
    const runtime::ComputeContext& context,
    const std::optional<part::NamedShape>& slotNamedShape
)
{
    if (slotNamedShape) {
        return slotNamedShape;
    }
    const auto namedShapeIt = context.namedShapes.find(owner);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    return std::nullopt;
}

std::optional<TransformSource> combineBodyPrefixSource(
    const app::DocumentObject& transformed,
    runtime::ComputeContext& context,
    const TransformSource& support,
    const TransformSource& tool,
    part::BooleanOperation operation,
    const std::string& owner
)
{
    // FreeCAD:
    // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute(),
    // calls "result.makeElementBoolean(maker, {base, prism}, ...)" and stores the final result
    // on the current PartDesign feature Shape before later transformed features use it as BaseFeature.
    const std::optional<long> supportTag = support.namedShape
        ? support.namedShape->producerTag
        : std::optional<long> {};
    const auto build = part::makeElementBooleanFromSources(
        owner,
        {namedShapeSource(support), namedShapeSource(tool)},
        operation,
        std::nullopt,
        supportTag
    );
    if (!build.error.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Transformed support prefix boolean failed for " + owner + ": " + build.error,
            transformed.name,
            "BaseFeature",
            "runtime",
            owner
        );
        return std::nullopt;
    }
    std::vector<std::string> refinedFeatures = support.refinedFeatures;
    refinedFeatures
        .insert(refinedFeatures.end(), tool.refinedFeatures.begin(), tool.refinedFeatures.end());
    return TransformSource {
        owner,
        build.shape,
        build.namedShape ? *build.namedShape : part::indexedNamedShapeForObject(owner, build.shape),
        refinedFeatures
    };
}

std::optional<TransformSource> refineBodyPrefixSourceIfActive(
    const std::string& feature,
    runtime::ComputeContext& context,
    const TransformSource& source
)
{
    const auto documentIt = context.documentObjects.find(feature);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr) {
        return source;
    }

    // FreeCAD:
    // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::execute(),
    // calls "solRes = refineShapeIfActive(result)" before assigning the feature Shape. Body
    // prefix support must consume that feature Shape, not just the unrefined add/sub tool cache.
    const auto refined = runtime::applyRefinePropertyForOwner(
        *documentIt->second,
        feature,
        context,
        source.shape,
        source.namedShape
    );
    if (!refined) {
        return std::nullopt;
    }
    std::vector<std::string> refinedFeatures = source.refinedFeatures;
    if (refined->applied) {
        refinedFeatures.push_back(feature);
    }
    return TransformSource {feature, refined->shape, refined->namedShape, refinedFeatures};
}

std::optional<TransformSource> bodyPrefixSupportSource(
    const app::DocumentObject& transformed,
    runtime::ComputeContext& context,
    const std::string& stopFeature,
    bool includeStopFeature
)
{
    const auto parentIt = context.parentGroupByObject.find(transformed.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return std::nullopt;
    }
    const auto bodyIt = context.documentObjects.find(parentIt->second);
    if (bodyIt == context.documentObjects.end() || bodyIt->second == nullptr
        || bodyIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }
    const app::DocumentObject& bodyObject = *bodyIt->second;
    const auto groupNames = bodyGroupNames(bodyObject);
    if (!groupNames) {
        return std::nullopt;
    }

    std::optional<TransformSource> current;
    if (app::propertyValue(bodyObject, "BaseFeature") != nullptr) {
        const auto baseFeature = app::readLink(bodyObject, "BaseFeature");
        if (baseFeature) {
            current = solidSource(baseFeature->object, context);
        }
    }

    bool reachedStop = false;
    for (const auto& feature : *groupNames) {
        if (feature == stopFeature && !includeStopFeature) {
            reachedStop = true;
            break;
        }
        if (context.transformationTemplateObjects.count(feature) != 0U) {
            if (feature == stopFeature && includeStopFeature) {
                reachedStop = true;
                break;
            }
            continue;
        }

        const auto shapeIt = context.shapes.find(feature);
        const auto objectResultIt = context.objects.find(feature);
        const bool replacesBodyShape = shapeIt != context.shapes.end()
            && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid
            && objectResultIt != context.objects.end()
            && objectResultIt->second.value("body_mode", "") == "replace";
        const auto addSubIt = context.addSubShapes.find(feature);

        if (replacesBodyShape
            || (shapeIt != context.shapes.end()
                && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid
                && addSubIt == context.addSubShapes.end())) {
            current = solidSource(feature, context);
        }
        else if (addSubIt != context.addSubShapes.end()) {
            const runtime::AddSubShape& addSubShape = addSubIt->second;
            if (addSubShape.addShape) {
                TransformSource tool {
                    feature,
                    *addSubShape.addShape,
                    namedShapeForSource(feature, context, addSubShape.addNamedShape)
                };
                if (!current) {
                    current = refineBodyPrefixSourceIfActive(feature, context, tool);
                    if (!current) {
                        return std::nullopt;
                    }
                }
                else {
                    current = combineBodyPrefixSource(
                        transformed,
                        context,
                        *current,
                        tool,
                        part::BooleanOperation::Fuse,
                        feature
                    );
                    if (!current) {
                        return std::nullopt;
                    }
                    current = refineBodyPrefixSourceIfActive(feature, context, *current);
                    if (!current) {
                        return std::nullopt;
                    }
                }
            }
            else if (addSubShape.subShape) {
                if (!current) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "execution_failed",
                        "Transformed support prefix cannot apply subtractive feature " + feature
                            + " without a base solid",
                        transformed.name,
                        "BaseFeature",
                        "runtime",
                        feature
                    );
                    return std::nullopt;
                }
                TransformSource tool {
                    feature,
                    *addSubShape.subShape,
                    namedShapeForSource(feature, context, addSubShape.subNamedShape)
                };
                current = combineBodyPrefixSource(
                    transformed,
                    context,
                    *current,
                    tool,
                    part::BooleanOperation::Cut,
                    feature
                );
                if (!current) {
                    return std::nullopt;
                }
                current = refineBodyPrefixSourceIfActive(feature, context, *current);
                if (!current) {
                    return std::nullopt;
                }
            }
        }

        if (feature == stopFeature && includeStopFeature) {
            reachedStop = true;
            break;
        }
    }

    if (!reachedStop) {
        return std::nullopt;
    }
    return current;
}

std::optional<TransformSource> publishedBodyTipSource(
    const app::DocumentObject& transformed,
    runtime::ComputeContext& context,
    const std::string& stopFeature,
    bool includeStopFeature
)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
    // ::Body::setBaseProperty() selects the preceding solid feature as BaseFeature; and
    // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::execute() consumes `getBaseObject()->Shape`. The Body boundary inherits the
    // already-published Tip ElementMap. It must not replay Pad/Pocket AddSubShape booleans to
    // reconstruct a support ledger: that creates a different producer lifecycle and loses the
    // terminal Pocket refs before a Pattern can consume them.
    const auto parentIt = context.parentGroupByObject.find(transformed.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return std::nullopt;
    }
    const auto bodyIt = context.documentObjects.find(parentIt->second);
    if (bodyIt == context.documentObjects.end() || bodyIt->second == nullptr
        || bodyIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }
    const auto groupNames = bodyGroupNames(*bodyIt->second);
    if (!groupNames) {
        return std::nullopt;
    }

    std::optional<TransformSource> tip;
    bool reachedStop = false;
    for (const std::string& feature : *groupNames) {
        if (feature == stopFeature && !includeStopFeature) {
            reachedStop = true;
            break;
        }
        if (const auto published = solidSource(feature, context)) {
            tip = published;
        }
        if (feature == stopFeature && includeStopFeature) {
            reachedStop = true;
            break;
        }
    }
    return reachedStop ? tip : std::nullopt;
}

std::optional<TransformSource> resolveSupportSource(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals
)
{
    // FreeCAD:
    // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute(),
    // calls Body::setBaseProperty(this) when BaseFeature is unset, then reads getBaseObject().
    // Body::setBaseProperty() in src/Mod/PartDesign/App/Body.cpp sets BaseFeature to the previous
    // solid feature in Group, so multi-original patterns start from the body prefix rather than
    // from Originals.front().
    if (app::propertyValue(object, "BaseFeature") != nullptr) {
        const auto baseFeature = app::readLink(object, "BaseFeature");
        if (!baseFeature) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_property",
                "Transformed BaseFeature must link to a solid",
                object.name,
                "BaseFeature"
            );
            return std::nullopt;
        }
        const auto support = solidSource(baseFeature->object, context);
        if (support) {
            return support;
        }
        const auto publishedTip = publishedBodyTipSource(object, context, baseFeature->object, true);
        if (!publishedTip) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_link_target",
                "BaseFeature target " + baseFeature->object + " did not produce a solid",
                object.name,
                "BaseFeature",
                "runtime",
                baseFeature->object
            );
            return std::nullopt;
        }
        return publishedTip;
    }

    if (const auto publishedTip = publishedBodyTipSource(object, context, object.name, false)) {
        return publishedTip;
    }

    if (originals.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Transformed Originals must contain at least one feature or BaseFeature must link to a "
            "support solid",
            object.name,
            "Originals"
        );
        return std::nullopt;
    }

    const auto support = solidSource(originals.front().object, context);
    if (!support) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "First Original " + originals.front().object + " did not produce a solid support",
            object.name,
            "Originals",
            "runtime",
            originals.front().object
        );
        return std::nullopt;
    }
    return support;
}

part::NamedShape transformedCopyNamedShape(
    const std::string& owner,
    const TopoDS_Shape& resultShape,
    const TransformSource& source,
    const std::string& postfix
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
    // ::TopoShape::makeElementTransform(), applies BRepBuilderAPI_Transform then calls
    // "copyElementMap(tmp, op)" instead of guessing geometry ownership from the output.
    return part::namedShapeForTransformedCopy(
        owner,
        resultShape,
        part::NamedShapeSource {source.owner, source.shape, source.namedShape ? &*source.namedShape : nullptr},
        std::optional<std::string> {postfix}
    );
}

std::optional<TransformSource> transformedCopy(
    const std::string& owner,
    const TransformSource& source,
    const gp_Trsf& transform,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property,
    const std::string& postfix
)
{
    try {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/
        // TopoShapeExpansion.cpp::TopoShape::makeElementTransform() defaults to CopyType::noCopy
        // for a non-mirroring rigid transform. It moves the TopoDS location in that path, keeping
        // partner identity so copyElementMap(tmp, op) can retain the incoming child ledger. An
        // unconditional BRepBuilderAPI_Transform(..., true) creates new TShapes and changes the
        // following makeElementFuse/Cut mapSubElement lifecycle.
        const bool requiresCopy = transform.ScaleFactor() * transform.HVectorialPart().Determinant()
                < 0.0
            || std::abs(std::abs(transform.ScaleFactor()) - 1.0) > Precision::Confusion();
        TopoDS_Shape transformed;
        if (requiresCopy) {
            BRepBuilderAPI_Transform maker(source.shape, transform, true);
            maker.Build();
            if (!maker.IsDone()) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "execution_failed",
                    "OCCT could not transform " + source.owner,
                    object.name,
                    property
                );
                return std::nullopt;
            }
            transformed = maker.Shape().Moved(gp_Trsf());
        }
        else {
            transformed = source.shape;
            gp_Trsf locationTransform(transform);
            locationTransform.SetScaleFactor(1.0);
            transformed.Move(locationTransform);
        }
        part::NamedShape namedShape = transformedCopyNamedShape(owner, transformed, source, postfix);
        return TransformSource {owner, transformed, namedShape};
    }
    catch (Standard_Failure& failure) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            failure.GetMessageString(),
            object.name,
            property
        );
        return std::nullopt;
    }
}

std::optional<TransformSource> fuseOrCutTransformedSource(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TransformSource& support,
    const TransformSource& tool,
    part::BooleanOperation operation,
    const std::string& property
)
{
    // FreeCAD: src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute() copies
    // `supportTopShape` into `supportShape` and invokes supportShape.makeElementFuse/Cut(...).
    // The receiver keeps its Tag across intermediate pattern Booleans; only the final
    // PropertyPartShape::setValue performs the feature-property retag.
    const std::optional<long> supportTag = support.namedShape
        ? support.namedShape->producerTag
        : std::optional<long> {};
    const auto build = part::makeElementBooleanFromSources(
        object.name,
        {namedShapeSource(support), namedShapeSource(tool)},
        operation,
        std::nullopt,
        supportTag
    );
    if (!build.error.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "execution_failed",
            "Transformed boolean operation failed: " + build.error,
            object.name,
            property
        );
        return std::nullopt;
    }
    return TransformSource {
        object.name,
        build.shape,
        build.namedShape ? *build.namedShape
                         : part::indexedNamedShapeForObject(object.name, build.shape)
    };
}

std::optional<TransformApplication> applyFeatureTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& originals,
    const std::vector<gp_Trsf>& copyTransforms
)
{
    auto support = resolveSupportSource(object, context, originals);
    if (!support) {
        return std::nullopt;
    }

    TransformSource current = *support;
    std::vector<std::string> originalNames;
    int transformedIndex = 1;
    for (const app::Link& original : originals) {
        originalNames.push_back(original.object);
        const auto addSubIt = context.addSubShapes.find(original.object);
        if (addSubIt == context.addSubShapes.end()) {
            const auto objectResultIt = context.objects.find(original.object);
            const std::string addSubCacheStatus = objectResultIt != context.objects.end()
                ? objectResultIt->second.value("add_sub_cache", "")
                : "";
            if (addSubCacheStatus == "degraded" || addSubCacheStatus == "empty") {
                const std::string code = addSubCacheStatus == "degraded"
                    ? "degraded_addsub_cache"
                    : "missing_addsub_cache";
                const std::string message = addSubCacheStatus == "degraded"
                    ? "Transformed Features mode cannot consume degraded AddSubShape cache from "
                        + original.object
                    : "Transformed Features mode requires AddSubShape cache from "
                        + original.object;
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    code,
                    message,
                    object.name,
                    "Originals",
                    "runtime",
                    original.object
                );
                return std::nullopt;
            }
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_type",
                "Only additive and subtractive features can be transformed",
                object.name,
                "Originals",
                "runtime",
                original.object
            );
            return std::nullopt;
        }

        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/
        // FeatureTransformed.cpp::Transformed::execute() first applies the leading
        // transformation to the AddSubShape itself, then getTransformedCompShape() advances
        // past that entry and creates copies only for the remaining transforms. Re-fusing the
        // first (usually identity) entry adds a producer pass that FreeCAD never performs.
        int copyOrdinal = 1;
        for (std::size_t transformIndex = 1U; transformIndex < copyTransforms.size(); ++transformIndex) {
            const gp_Trsf& transform = copyTransforms.at(transformIndex);
            if (addSubIt->second.addShape) {
                TransformSource originalAdd {
                    original.object,
                    *addSubIt->second.addShape,
                    addSubIt->second.addNamedShape
                        ? addSubIt->second.addNamedShape
                        : (context.namedShapes.count(original.object) != 0U
                               ? std::optional<part::NamedShape> {context.namedShapes.at(original.object)}
                               : std::nullopt)
                };
                auto transformedTool = transformedCopy(
                    object.name + ".Transform" + std::to_string(transformedIndex++),
                    originalAdd,
                    transform,
                    object,
                    context,
                    "Originals",
                    copyOrdinal < 2 ? std::string {} : "_" + std::to_string(copyOrdinal)
                );
                if (!transformedTool) {
                    return std::nullopt;
                }
                auto fused = fuseOrCutTransformedSource(
                    object,
                    context,
                    current,
                    *transformedTool,
                    part::BooleanOperation::Fuse,
                    "Originals"
                );
                if (!fused) {
                    return std::nullopt;
                }
                current = *fused;
            }

            if (addSubIt->second.subShape) {
                TransformSource originalSub {
                    original.object,
                    *addSubIt->second.subShape,
                    addSubIt->second.subNamedShape
                        ? addSubIt->second.subNamedShape
                        : (context.namedShapes.count(original.object) != 0U
                               ? std::optional<part::NamedShape> {context.namedShapes.at(original.object)}
                               : std::nullopt)
                };
                auto transformedTool = transformedCopy(
                    object.name + ".Transform" + std::to_string(transformedIndex++),
                    originalSub,
                    transform,
                    object,
                    context,
                    "Originals",
                    copyOrdinal < 2 ? std::string {} : "_" + std::to_string(copyOrdinal)
                );
                if (!transformedTool) {
                    return std::nullopt;
                }
                auto cut = fuseOrCutTransformedSource(
                    object,
                    context,
                    current,
                    *transformedTool,
                    part::BooleanOperation::Cut,
                    "Originals"
                );
                if (!cut) {
                    return std::nullopt;
                }
                current = *cut;
            }
            ++copyOrdinal;
        }
    }

    part::NamedShape resultNamedShape = current.namedShape
        ? *current.namedShape
        : part::indexedNamedShapeForObject(current.owner, current.shape);
    if (resultNamedShape.owner != object.name) {
        resultNamedShape = part::namedShapeForPreservedSources(
            object.name,
            current.shape,
            {namedShapeSource(current)}
        );
    }
    return TransformApplication {current.shape, resultNamedShape, originalNames, support->refinedFeatures};
}

std::optional<TransformApplication> applyWholeShapeTransforms(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::vector<app::Link>& supportLinks,
    const std::vector<gp_Trsf>& copyTransforms
)
{
    auto support = resolveSupportSource(object, context, supportLinks);
    if (!support) {
        return std::nullopt;
    }

    TransformSource current = *support;
    int transformedIndex = 1;
    // FreeCAD: FeatureTransformed.cpp::Transformed::getTransformedCompShape() retains the
    // untransformed support in slot one and starts transformed copies at the second sequence
    // entry. Its `Data::indexSuffix(1)` is empty; later copies use `_2`, `_3`, ... .
    int copyOrdinal = 1;
    for (std::size_t transformIndex = 1U; transformIndex < copyTransforms.size(); ++transformIndex) {
        const gp_Trsf& transform = copyTransforms.at(transformIndex);
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
        // ::Transformed::execute(), Mode::WholeShape calls getTransformedCompShape(supportShape,
        // supportShape) and then makeElementFuse(shapes), keeping the untransformed support plus
        // transformed support copies.
        auto transformedTool = transformedCopy(
            object.name + ".Transform" + std::to_string(transformedIndex++),
            *support,
            transform,
            object,
            context,
            "TransformMode",
            copyOrdinal < 2 ? std::string {} : "_" + std::to_string(copyOrdinal)
        );
        if (!transformedTool) {
            return std::nullopt;
        }
        auto fused = fuseOrCutTransformedSource(
            object,
            context,
            current,
            *transformedTool,
            part::BooleanOperation::Fuse,
            "TransformMode"
        );
        if (!fused) {
            return std::nullopt;
        }
        current = *fused;
        ++copyOrdinal;
    }

    part::NamedShape resultNamedShape = current.namedShape
        ? *current.namedShape
        : part::indexedNamedShapeForObject(current.owner, current.shape);
    if (resultNamedShape.owner != object.name) {
        resultNamedShape = part::namedShapeForPreservedSources(
            object.name,
            current.shape,
            {namedShapeSource(current)}
        );
    }

    // FreeCAD:
    // /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::getOriginals(),
    // returns an empty vector in WholeShape mode. The semantic support is BaseFeature / Body
    // prefix from getBaseObject(), so report the resolved support owner instead of the hidden
    // Originals property.
    std::vector<std::string> supportNames {support->owner};
    return TransformApplication {current.shape, resultNamedShape, supportNames, support->refinedFeatures};
}

std::optional<gp_Dir> directionFromShape(
    const TopoDS_Shape& shape,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (shape.ShapeType() == TopAbs_EDGE) {
        const TopoDS_Edge edge = TopoDS::Edge(shape);
        // FreeCAD FeatureLinearPattern accepts only GeomAbs_Line for direction references.
        // Keep that strict boundary while sharing the low-level line extraction helper.
        part::EdgeAxisOptions options;
        const auto resolved = part::resolveEdgeAxis(edge, options);
        if (!resolved.axis) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_direction",
                property + " edge must be a straight line: " + resolved.message,
                object.name,
                property
            );
            return std::nullopt;
        }
        return resolved.axis->direction;
    }
    if (shape.ShapeType() == TopAbs_FACE) {
        const TopoDS_Face face = TopoDS::Face(shape);
        BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "invalid_direction",
                property + " face must be planar",
                object.name,
                property
            );
            return std::nullopt;
        }
        return surface.Plane().Axis().Direction();
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "unsupported_subshape_kind",
        property + " must reference a datum line, datum plane, straight edge, or planar face",
        object.name,
        property
    );
    return std::nullopt;
}

std::optional<RotationAxis> axisFromShape(
    const TopoDS_Shape& shape,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    if (shape.ShapeType() != TopAbs_EDGE) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            property + " must reference a datum line or an edge",
            object.name,
            property
        );
        return std::nullopt;
    }

    const TopoDS_Edge edge = TopoDS::Edge(shape);
    part::EdgeAxisOptions options;
    options.allowCircleAxis = true;
    options.allowGeometricallyLinearCurve = true;
    const auto resolved = part::resolveEdgeAxis(edge, options);
    if (resolved.axis) {
        return RotationAxis {resolved.axis->base, resolved.axis->direction};
    }

    runtime::addDiagnostic(
        context.diagnostics,
        "error",
        "invalid_axis",
        property + " edge must be a straight/geometrically linear edge, circle, or arc of circle: "
            + resolved.message,
        object.name,
        property
    );
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

std::optional<SketchAxis> sketchAxisFromObject(
    const app::DocumentObject& sketch,
    const std::string& subname,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Part2DObject.cpp
    // ::Part2DObject::getAxis(), returns "H_Axis"=(1,0,0), "V_Axis"=(0,1,0),
    // and "N_Axis"=(0,0,1).
    SketchAxis axis {gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)};
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
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp
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
                if (!item.is_object() || item.value("kind", "") != "LineSegment"
                    || !item.value("construction", false)) {
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
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " target " + sketch.name + " has no sketch axis " + subname,
            object.name,
            property,
            "runtime",
            sketch.name,
            subname
        );
        return std::nullopt;
    }

    const auto placementIt = context.globalPlacements.find(sketch.name);
    if (placementIt != context.globalPlacements.end()) {
        axis.point.Transform(placementIt->second);
        axis.direction.Transform(placementIt->second);
    }
    return axis;
}

std::optional<SketchAxis> resolveSketchAxis(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch) {
        return std::nullopt;
    }
    if (link.subnames.size() != 1U || link.subnames.front().empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            property + " must reference one Sketch axis subname",
            object.name,
            property,
            "runtime",
            link.object
        );
        return std::nullopt;
    }
    const auto documentIt = context.documentObjects.find(link.object);
    if (documentIt == context.documentObjects.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link.object + " is missing from the document graph",
            object.name,
            property,
            "runtime",
            link.object
        );
        return std::nullopt;
    }
    return sketchAxisFromObject(*documentIt->second, link.subnames.front(), object, context, property);
}

std::optional<gp_Dir> resolveLinearPatternDirection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& property
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp
    // ::LinearPattern::getDirectionFromProperty(), accepts a datum line/plane, sketch axis,
    // straight edge or planar face, then transforms direction into the object's local coordinates.
    const auto link = app::readLink(object, property);
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            property + " must link to a direction reference",
            object.name,
            property
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt != context.shapes.end()) {
        if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch && link->subnames.size() == 1U
            && isSketchAxisSubname(link->subnames.front())) {
            const auto sketchAxis = resolveSketchAxis(*link, object, context, property);
            if (!sketchAxis) {
                return std::nullopt;
            }
            return sketchAxis->direction;
        }

        if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
            for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_EDGE); explorer.More();
                 explorer.Next()) {
                return directionFromShape(explorer.Current(), object, context, property);
            }
        }
    }

    if (const auto planeFrame = detail::referencePlaneProviderFrame(link->object, context);
        planeFrame && link->subnames.empty()) {
        return planeFrame->normal;
    }

    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            property + " target " + link->object + " did not produce a shape",
            object.name,
            property,
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    const auto subshape = resolveDirectionSubshape(*link, object, context, property);
    if (!subshape) {
        return std::nullopt;
    }
    return directionFromShape(*subshape, object, context, property);
}

std::optional<RotationAxis> resolvePolarPatternAxis(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp
    // ::PolarPattern::getRotation(), accepts a Sketch Part2DObject axis, datum line, or a Part
    // feature edge; the edge may be a straight line, circle, or arc of circle.
    const auto link = app::readLink(object, "Axis");
    if (!link) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "Axis must link to a rotation reference",
            object.name,
            "Axis"
        );
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link->object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "Axis target " + link->object + " did not produce a shape",
            object.name,
            "Axis",
            "runtime",
            link->object
        );
        return std::nullopt;
    }

    std::optional<RotationAxis> axis;
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch && link->subnames.size() == 1U
        && isSketchAxisSubname(link->subnames.front())) {
        const auto sketchAxis = resolveSketchAxis(*link, object, context, "Axis");
        if (!sketchAxis) {
            return std::nullopt;
        }
        axis = RotationAxis {sketchAxis->point, sketchAxis->direction};
    }
    else if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link->subnames.empty()) {
        for (TopExp_Explorer explorer(shapeIt->second.shape, TopAbs_EDGE); explorer.More();
             explorer.Next()) {
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

void publishTransformedResult(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const TransformedBuild& result,
    const std::string& transformedKind
)
{
    context.shapes[object.name] = runtime::ShapeValue {runtime::ShapeValue::Kind::Solid, result.shape};
    // FreeCAD: src/Mod/Part/App/PropertyTopoShape.cpp::PropertyPartShape::setValue() assigns
    // the Linear/PolarPattern feature Tag after Transformed has produced its own ElementMap.
    // Do not let Body infer this producer identity from a display subname.
    context.namedShapes[object.name] = part::namedShapeForPropertyShapeValue(
        object.name, result.shape, result.namedShape, static_cast<long>(object.id)
    );
    context.mesh[object.name] = cad_core::part::meshForShape(result.shape);
    context.subshapes[object.name] = part::subshapeMapForShape(result.shape);
    context.objects[object.name] = {
        {"status", "ok"},
        {"shape", "occt_solid"},
        {"body_mode", "replace"},
        {"transformed", transformedKind},
        {"transform_mode", result.mode},
        {"originals", result.originals},
        {"bbox", cad_core::part::objectBBoxForShape(result.shape)},
        {"volume", cad_core::part::volumeForShape(result.shape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (result.refineApplied) {
        context.objects[object.name]["refine"] = "applied";
    }
    if (!result.supportRefinedFeatures.empty()) {
        context.objects[object.name]["support_refined_features"] = result.supportRefinedFeatures;
    }
}

bool applyTransformedRefine(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    TransformedBuild& result
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp
    // ::Transformed::execute(), after Features / WholeShape fuse-cut composition calls
    // "supportShape = refineShapeIfActive((supportShape))" before setting Shape.
    const auto refined = runtime::applyPartDesignFeatureRefineProperty(object, context, result.shape, result.namedShape);
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

}  // namespace transformed_detail

}  // namespace cad_core::part_design
