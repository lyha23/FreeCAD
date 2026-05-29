#include "cad_core/features/link.h"

#include "cad_core/features/feature_executor.h"
#include "cad_core/geometry/placement.h"
#include "cad_core/geometry/shape_exporter.h"
#include "cad_core/topo/named_shape.h"
#include "cad_core/topo/subshape_map.h"

#include <BRepBuilderAPI_GTransform.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Trsf.hxx>

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::features {

namespace {

runtime::ShapeValue::Kind shapeKindForShape(const TopoDS_Shape& shape)
{
    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    return solidExplorer.More() ? runtime::ShapeValue::Kind::Solid
                                : runtime::ShapeValue::Kind::PartPrimitive;
}

std::string shapeLabelForShape(const TopoDS_Shape& shape)
{
    switch (shape.ShapeType()) {
        case TopAbs_COMPOUND:
            return "occt_compound";
        case TopAbs_COMPSOLID:
            return "occt_compsolid";
        case TopAbs_SOLID:
            return "occt_solid";
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

struct LinkShapeBuild {
    TopoDS_Shape shape;
    std::optional<std::string> sourceElementName;
    std::optional<std::string> targetElementName;
    std::vector<std::pair<std::string, std::string>> sourceToTargetElements;
};

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);

std::optional<gp_Trsf> placementProperty(const document::DocumentObject& object,
                                         const std::string& property)
{
    const auto placement = document::readPlacement(object, property);
    if (!placement) {
        return std::nullopt;
    }
    return geometry::placementFromComponents(placement->base, placement->rotation);
}

gp_Trsf parentPlacement(const document::DocumentObject& object, const runtime::ComputeContext& context)
{
    const auto parentIt = context.parentGroupByObject.find(object.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return gp_Trsf();
    }
    const auto placementIt = context.globalPlacements.find(parentIt->second);
    return placementIt == context.globalPlacements.end() ? gp_Trsf() : placementIt->second;
}

gp_Trsf linkPlacement(const document::DocumentObject& object, const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.h::LINK_PARAM_LINK_PLACEMENT
    // and LINK_PARAM_PLACEMENT define LinkPlacement plus "Alias to LinkPlacement"; cad-core uses
    // LinkPlacement first, then Placement, and prefixes the containing GeoFeatureGroup placement.
    auto local = placementProperty(object, "LinkPlacement");
    if (!local) {
        local = placementProperty(object, "Placement");
    }
    return parentPlacement(object, context) * local.value_or(gp_Trsf());
}

gp_Trsf objectGlobalPlacement(const document::DocumentObject& object, const runtime::ComputeContext& context)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    return placementIt == context.globalPlacements.end() ? gp_Trsf() : placementIt->second;
}

std::array<double, 3> readScaleVector(const document::DocumentObject& object)
{
    const double scale = document::readNumber(object, "Scale").value_or(1.0);
    const auto scaleVector = document::readVector3(object, "ScaleVector");
    if (!scaleVector) {
        return {scale, scale, scale};
    }
    return {
        scale * scaleVector->at(0),
        scale * scaleVector->at(1),
        scale * scaleVector->at(2),
    };
}

bool parseVector3Value(const nlohmann::json& value, std::array<double, 3>& vector)
{
    if (!value.is_array() || value.size() != 3U) {
        return false;
    }
    for (const auto& item : value) {
        if (!item.is_number()) {
            return false;
        }
    }
    vector = {value.at(0).get<double>(), value.at(1).get<double>(), value.at(2).get<double>()};
    return true;
}

const nlohmann::json& rawPropertyPayload(const document::PropertyValue& value)
{
    if (value.raw.is_object() && value.raw.contains("value")) {
        return value.raw.at("value");
    }
    return value.raw;
}

std::optional<gp_Trsf> parsePlacementValue(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("Base") || !value.contains("Rotation")) {
        return std::nullopt;
    }

    std::array<double, 3> base {};
    if (!parseVector3Value(value.at("Base"), base)) {
        return std::nullopt;
    }

    const auto& rotation = value.at("Rotation");
    if (!rotation.is_array() || rotation.size() != 4U) {
        return std::nullopt;
    }

    std::array<double, 4> quaternion {};
    double normSquared = 0.0;
    for (std::size_t index = 0; index < 4U; ++index) {
        if (!rotation.at(index).is_number()) {
            return std::nullopt;
        }
        quaternion.at(index) = rotation.at(index).get<double>();
        normSquared += quaternion.at(index) * quaternion.at(index);
    }
    if (normSquared <= 0.0) {
        return std::nullopt;
    }

    return geometry::placementFromComponents(base, quaternion);
}

std::optional<std::vector<gp_Trsf>> readPlacementList(const document::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    const auto* value = document::propertyValue(object, "PlacementList");
    if (value == nullptr) {
        return std::vector<gp_Trsf> {};
    }

    const auto& payload = rawPropertyPayload(*value);
    if (!payload.is_array()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_placement",
                               "PlacementList must be a list of placements",
                               object.name,
                               "PlacementList",
                               "runtime");
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    std::vector<gp_Trsf> placements;
    placements.reserve(payload.size());
    for (const auto& item : payload) {
        const auto placement = parsePlacementValue(item);
        if (!placement) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_placement",
                                   "PlacementList contains an invalid placement",
                                   object.name,
                                   "PlacementList",
                                   "runtime");
            context.objects[object.name] = {{"status", "error"}};
            return std::nullopt;
        }
        placements.push_back(*placement);
    }
    return placements;
}

std::optional<std::vector<std::array<double, 3>>> readScaleList(const document::DocumentObject& object,
                                                               runtime::ComputeContext& context)
{
    const auto* value = document::propertyValue(object, "ScaleList");
    if (value == nullptr) {
        return std::vector<std::array<double, 3>> {};
    }

    const auto& payload = rawPropertyPayload(*value);
    if (!payload.is_array()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_property_type",
                               "ScaleList must be a list of vectors",
                               object.name,
                               "ScaleList",
                               "runtime");
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    std::vector<std::array<double, 3>> scales;
    scales.reserve(payload.size());
    for (const auto& item : payload) {
        std::array<double, 3> scale {};
        if (!parseVector3Value(item, scale)) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_property_type",
                                   "ScaleList contains an invalid vector",
                                   object.name,
                                   "ScaleList",
                                   "runtime");
            context.objects[object.name] = {{"status", "error"}};
            return std::nullopt;
        }
        scales.push_back(scale);
    }
    return scales;
}

std::size_t readElementCount(const document::DocumentObject& object)
{
    const double count = document::readNumber(object, "ElementCount").value_or(0.0);
    return count <= 0.0 ? 0U : static_cast<std::size_t>(count);
}

gp_Trsf defaultElementPlacement(std::size_t index)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when ShowElement is false, grows PlacementList with
    // "Base::Vector3d(i % 10, (i / 10) % 10, i / 100)" and identity rotation.
    return geometry::placementFromComponents(
        {static_cast<double>(index % 10U),
         static_cast<double>((index / 10U) % 10U),
         static_cast<double>(index / 100U)},
        {0.0, 0.0, 0.0, 1.0}
    );
}

bool isIdentityScale(const std::array<double, 3>& scale)
{
    constexpr double eps = 1e-12;
    return std::abs(scale.at(0) - 1.0) < eps
        && std::abs(scale.at(1) - 1.0) < eps
        && std::abs(scale.at(2) - 1.0) < eps;
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

std::string stableSubnameDiagnosticMessage(const std::string& target,
                                           const std::string& stableSubname,
                                           topo::ElementResolveStatus status)
{
    switch (status) {
        case topo::ElementResolveStatus::Deleted:
            return "Stable subname " + stableSubname + " on " + target
                + " was deleted by source history";
        case topo::ElementResolveStatus::Split:
            return "Stable subname " + stableSubname + " on " + target
                + " was split by source history";
        case topo::ElementResolveStatus::Resolved:
        case topo::ElementResolveStatus::Unresolved:
            return "Stable subname " + stableSubname + " on " + target
                + " is not available in the current ElementMap";
    }
    return "Stable subname " + stableSubname + " on " + target + " is not available";
}

TopoDS_Shape applyScale(const TopoDS_Shape& shape, const std::array<double, 3>& scale)
{
    if (isIdentityScale(scale)) {
        return shape;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::getTransform(), applies Scale or ScaleVector after link placement.
    gp_GTrsf transform;
    transform.SetValue(1, 1, scale.at(0));
    transform.SetValue(2, 2, scale.at(1));
    transform.SetValue(3, 3, scale.at(2));
    return BRepBuilderAPI_GTransform(shape, transform, true).Shape();
}

std::vector<bool> readVisibilityList(const document::DocumentObject& object)
{
    const auto* value = document::propertyValue(object, "VisibilityList");
    if (value == nullptr) {
        return {};
    }
    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (!payload->is_array()) {
        return {};
    }

    std::vector<bool> visibility;
    for (const auto& item : *payload) {
        if (!item.is_boolean()) {
            return {};
        }
        visibility.push_back(item.get<bool>());
    }
    return visibility;
}

bool isVisibleElement(std::size_t index, const std::vector<bool>& visibility)
{
    return index >= visibility.size() || visibility.at(index);
}

std::optional<std::string> firstSubname(const std::vector<std::string>& values)
{
    if (values.empty() || values.front().empty()) {
        return std::nullopt;
    }
    return values.front();
}

std::string targetElementNameForResolvedSource(const std::string& sourceElementName)
{
    const auto parsed = topo::parseSubshapeName(sourceElementName);
    if (!parsed) {
        return {};
    }
    switch (parsed->kind) {
        case TopAbs_FACE:
            return "Face1";
        case TopAbs_EDGE:
            return "Edge1";
        case TopAbs_VERTEX:
            return "Vertex1";
        default:
            return {};
    }
}

std::string targetElementNameForResolvedSource(const std::string& sourceElementName,
                                               std::map<TopAbs_ShapeEnum, int>& counts)
{
    const auto parsed = topo::parseSubshapeName(sourceElementName);
    if (!parsed) {
        return {};
    }
    const std::string prefix = parsed->kind == TopAbs_FACE ? "Face"
        : parsed->kind == TopAbs_EDGE                        ? "Edge"
        : parsed->kind == TopAbs_VERTEX                      ? "Vertex"
                                                            : "";
    if (prefix.empty()) {
        return {};
    }
    const int index = ++counts[parsed->kind];
    return prefix + std::to_string(index);
}

std::optional<LinkShapeBuild> linkedSubshapeAt(const document::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const document::Link& link,
                                               const TopoDS_Shape& sourceShape,
                                               std::size_t index)
{
    const std::string subname = link.subnames.at(index);
    const std::string stableSubname = index < link.stableSubnames.size() && !link.stableSubnames.at(index).empty()
        ? link.stableSubnames.at(index)
        : subname;
    const auto namedShapeIt = context.namedShapes.find(link.object);
    std::string resolvedElement = subname;
    std::optional<TopoDS_Shape> shape;
    if (namedShapeIt != context.namedShapes.end()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::parseSubName() stores PropertyXLink subvalues and
        // getTrueLinkedObject() resolves them through getSubObject().
        const auto resolved = topo::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            resolvedElement = *resolved.element;
            shape = topo::subshapeByName(namedShapeIt->second, resolvedElement);
        }
        else if (stableSubname != subname) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   stableSubnameDiagnosticCode(resolved.status),
                                   stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                                   object.name,
                                   "LinkedObject",
                                   "runtime",
                                   link.object,
                                   stableSubname);
            context.objects[object.name] = {{"status", "error"}};
            return std::nullopt;
        }
    }
    else {
        shape = topo::subshapeByName(sourceShape, subname);
    }

    if (!shape) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               "Invalid linked subshape " + subname,
                               object.name,
                               "LinkedObject",
                               "runtime",
                               link.object,
                               subname);
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    return LinkShapeBuild{*shape, resolvedElement, targetElementName, {{resolvedElement, targetElementName}}};
}

std::optional<LinkShapeBuild> linkedSubshape(const document::DocumentObject& object,
                                             runtime::ComputeContext& context,
                                             const document::Link& link,
                                             const TopoDS_Shape& sourceShape)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return LinkShapeBuild{sourceShape, std::nullopt, std::nullopt, {}};
    }
    if (link.subnames.size() == 1U) {
        return linkedSubshapeAt(object, context, link, sourceShape, 0U);
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::parseSubName() keeps multiple PropertyXLink sub-elements that share
    // the same linked-object prefix; cad-core returns them as a request-local compound and lets
    // topo retag each selected source element onto the compound's FaceN/EdgeN/VertexN ledger.
    std::vector<TopoDS_Shape> shapes;
    std::vector<std::pair<std::string, std::string>> sourceToTargetElements;
    std::map<TopAbs_ShapeEnum, int> targetCounts;
    for (std::size_t index = 0; index < link.subnames.size(); ++index) {
        if (link.subnames.at(index).empty()) {
            continue;
        }
        auto selected = linkedSubshapeAt(object, context, link, sourceShape, index);
        if (!selected) {
            return std::nullopt;
        }
        shapes.push_back(selected->shape);
        if (selected->sourceElementName) {
            const std::string targetElementName =
                targetElementNameForResolvedSource(*selected->sourceElementName, targetCounts);
            if (!targetElementName.empty()) {
                sourceToTargetElements.emplace_back(*selected->sourceElementName, targetElementName);
            }
        }
    }
    if (shapes.empty()) {
        return LinkShapeBuild{sourceShape, std::nullopt, std::nullopt, {}};
    }
    return LinkShapeBuild{compoundOf(shapes), std::nullopt, std::nullopt, sourceToTargetElements};
}

void publishLinkedShape(const document::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const TopoDS_Shape& shape,
                        runtime::ShapeValue::Kind kind,
                        const nlohmann::json& metadata,
                        std::optional<topo::NamedShape> namedShape = std::nullopt)
{
    context.shapes[object.name] = runtime::ShapeValue{kind, shape};
    context.mesh[object.name] = geometry::meshForShape(shape);
    context.subshapes[object.name] = topo::subshapeMapForShape(shape);
    context.namedShapes[object.name] = namedShape.value_or(topo::indexedNamedShapeForObject(object.name, shape));

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabelForShape(shape);
    result["bbox"] = geometry::bboxForShape(shape);
    result["volume"] = geometry::volumeForShape(shape);
    result["kernel"] = geometry::kernelVersion();
    context.objects[object.name] = result;
}

void publishEmptyLink(const document::DocumentObject& object,
                      runtime::ComputeContext& context,
                      const nlohmann::json& metadata)
{
    nlohmann::json result = metadata;
    result["status"] = "ok";
    context.objects[object.name] = result;
}

std::optional<document::Link> linkedObjectProperty(const document::DocumentObject& object,
                                                   runtime::ComputeContext& context)
{
    const auto link = document::readLink(object, "LinkedObject");
    if (link) {
        return link;
    }

    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "missing_property",
                           object.typeId + " LinkedObject is not set",
                           object.name,
                           "LinkedObject",
                           "runtime");
    context.objects[object.name] = {{"status", "error"}};
    return std::nullopt;
}

std::optional<LinkShapeBuild> baseLinkedShape(const document::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const document::Link& link,
                                              bool linkTransform)
{
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }

    auto selected = linkedSubshape(object, context, link, shapeIt->second.shape);
    if (!selected) {
        return std::nullopt;
    }

    if (!linkTransform) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.h::LINK_PARAM_TRANSFORM
        // documents LinkTransform=false as "override linked object's placement"; cad-core removes
        // the already-applied source global placement when it is available in this request.
        const auto sourcePlacementIt = context.globalPlacements.find(link.object);
        if (sourcePlacementIt != context.globalPlacements.end()) {
            selected->shape = geometry::transformShape(selected->shape, sourcePlacementIt->second.Inverted());
        }
    }

    return selected;
}

std::optional<LinkShapeBuild> linkShape(const document::DocumentObject& object,
                                        runtime::ComputeContext& context,
                                        const document::Link& link,
                                        bool linkTransform)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    TopoDS_Shape shape = selected->shape;
    shape = applyScale(shape, readScaleVector(object));
    selected->shape = geometry::transformShape(shape, linkPlacement(object, context));
    return selected;
}

void executeLinkLike(const document::DocumentObject& object,
                     runtime::ComputeContext& context,
                     const std::set<std::string>& allowedProperties,
                     const std::string& kind)
{
    if (!rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto link = linkedObjectProperty(object, context);
    if (!link) {
        return;
    }

    const bool linkTransform = document::readBool(object, "LinkTransform").value_or(false);
    const auto shape = linkShape(object, context, *link, linkTransform);
    nlohmann::json metadata = {
        {"link", kind},
        {"linked_object", link->object},
        {"link_transform", linkTransform},
    };
    if (object.typeId == "Assembly::AssemblyLink") {
        metadata["rigid"] = document::readBool(object, "Rigid").value_or(true);
    }

    if (!shape) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const auto sourceShapeIt = context.shapes.find(link->object);
    const runtime::ShapeValue::Kind kindValue = !shape->sourceToTargetElements.empty()
        ? shapeKindForShape(shape->shape)
        : (sourceShapeIt == context.shapes.end() ? shapeKindForShape(shape->shape) : sourceShapeIt->second.kind);
    std::optional<topo::NamedShape> linkedNamedShape;
    if (sourceShapeIt != context.shapes.end()) {
        const auto sourceNamedShapeIt = context.namedShapes.find(link->object);
        const topo::NamedShapeSource source{
            link->object,
            sourceShapeIt->second.shape,
            sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second,
        };
        if (!shape->sourceToTargetElements.empty()) {
            linkedNamedShape = topo::namedShapeForLinkedSubshapes(
                object.name,
                shape->shape,
                source,
                shape->sourceToTargetElements
            );
        }
        else {
            linkedNamedShape = topo::namedShapeForLinkedShape(object.name, shape->shape, source);
        }
    }
    publishLinkedShape(object, context, shape->shape, kindValue, metadata, linkedNamedShape);
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);

void executeElementGroupLike(const document::DocumentObject& object,
                             runtime::ComputeContext& context,
                             const std::set<std::string>& allowedProperties,
                             const std::string& kind)
{
    if (!rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto links = document::readLinks(object, "ElementList");
    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<topo::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const gp_Trsf groupPlacement = objectGlobalPlacement(object, context);

    for (std::size_t index = 0; index < links.size(); ++index) {
        const auto& link = links.at(index);
        elements.push_back(link.object);
        if (!isVisibleElement(index, visibility)) {
            continue;
        }

        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::extensionGetSubObject(), when ElementList is present, routes
        // subobject lookup through "elements[idx]->getSubObject(...)"; LinkGroup owns the
        // group placement, so cad-core applies this object's global placement to each child
        // shape before composing the request-local display compound.
        TopoDS_Shape displayedShape = geometry::transformShape(shapeIt->second.shape, groupPlacement);
        shapes.push_back(displayedShape);
        visibleElements.push_back(link.object);

        const auto namedShapeIt = context.namedShapes.find(link.object);
        sources.push_back(topo::NamedShapeSource{
            link.object,
            displayedShape,
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
        });
    }

    nlohmann::json metadata = {
        {"link", kind},
        {"elements", elements},
        {"visible_elements", visibleElements},
    };
    if (shapes.empty()) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const TopoDS_Shape shape = compoundOf(shapes);
    publishLinkedShape(
        object,
        context,
        shape,
        shapeKindForShape(shape),
        metadata,
        topo::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

void executeCollapsedElementCountLink(const document::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      const std::set<std::string>& allowedProperties,
                                      const std::string& kind)
{
    if (!rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto elementCount = readElementCount(object);
    const auto placements = readPlacementList(object, context);
    if (!placements) {
        return;
    }
    const auto scales = readScaleList(object, context);
    if (!scales) {
        return;
    }

    const auto link = linkedObjectProperty(object, context);
    if (!link) {
        return;
    }

    const bool linkTransform = document::readBool(object, "LinkTransform").value_or(false);
    const auto baseShape = baseLinkedShape(object, context, *link, linkTransform);
    nlohmann::json visibleIndices = nlohmann::json::array();
    nlohmann::json metadata = {
        {"link", kind},
        {"linked_object", link->object},
        {"link_transform", linkTransform},
        {"element_count", elementCount},
        {"collapsed_elements", true},
    };

    if (!baseShape) {
        metadata["visible_indices"] = visibleIndices;
        publishEmptyLink(object, context, metadata);
        return;
    }

    std::vector<TopoDS_Shape> shapes;
    std::vector<topo::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const auto linkScale = readScaleVector(object);
    const gp_Trsf linkObjectPlacement = linkPlacement(object, context);
    const auto sourceNamedShapeIt = context.namedShapes.find(link->object);
    const topo::NamedShape* sourceNamedShape =
        sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second;

    for (std::size_t index = 0; index < elementCount; ++index) {
        if (!isVisibleElement(index, visibility)) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::extensionGetSubObject(), when ElementList is empty and
        // ElementCount is set, multiplies PlacementList[idx] and ScaleList[idx] before
        // resolving the true linked object; getElementIndex() also recognizes owner "_iN"
        // names for collapsed elements.
        TopoDS_Shape displayedShape = applyScale(
            baseShape->shape,
            index < scales->size() ? scales->at(index) : std::array<double, 3> {1.0, 1.0, 1.0}
        );
        displayedShape = geometry::transformShape(
            displayedShape,
            index < placements->size() ? placements->at(index) : defaultElementPlacement(index)
        );
        displayedShape = applyScale(displayedShape, linkScale);
        displayedShape = geometry::transformShape(displayedShape, linkObjectPlacement);

        shapes.push_back(displayedShape);
        visibleIndices.push_back(index);
        sources.push_back(topo::NamedShapeSource{
            object.name + "_i" + std::to_string(index),
            displayedShape,
            sourceNamedShape,
        });
    }

    metadata["visible_indices"] = visibleIndices;
    if (shapes.empty()) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const TopoDS_Shape shape = compoundOf(shapes);
    publishLinkedShape(
        object,
        context,
        shape,
        shapeKindForShape(shape),
        metadata,
        topo::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

bool isOwnedMaterializedLinkElement(const document::DocumentObject& element,
                                    const document::DocumentObject& owner)
{
    if (element.typeId != "App::LinkElement") {
        return false;
    }
    const auto ownerValue = document::readNumber(element, "_LinkOwner");
    return !ownerValue || static_cast<long long>(*ownerValue) == owner.id;
}

std::vector<document::Link> materializedElementLinks(const document::DocumentObject& object,
                                                     const runtime::ComputeContext& context)
{
    std::vector<document::Link> links;
    const auto elementCount = readElementCount(object);
    links.reserve(elementCount);
    for (std::size_t index = 0; index < elementCount; ++index) {
        const std::string elementName = object.name + "_i" + std::to_string(index);
        const auto elementIt = context.documentObjects.find(elementName);
        if (elementIt == context.documentObjects.end() || elementIt->second == nullptr
            || !isOwnedMaterializedLinkElement(*elementIt->second, object)) {
            continue;
        }
        links.push_back(document::Link{elementName, {}, {}, {}, "ElementList"});
    }
    return links;
}

void executeMaterializedElementGroupLike(const document::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::set<std::string>& allowedProperties,
                                         const std::string& kind)
{
    if (!rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto links = materializedElementLinks(object, context);
    if (links.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_link_lifecycle",
                               "ShowElement=true ElementCount requires materialized LinkElement objects",
                               object.name,
                               "ElementCount",
                               "runtime");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<topo::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const gp_Trsf groupPlacement = objectGlobalPlacement(object, context);

    for (std::size_t index = 0; index < links.size(); ++index) {
        const auto& link = links.at(index);
        elements.push_back(link.object);
        if (!isVisibleElement(index, visibility)) {
            continue;
        }

        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt == context.shapes.end()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::update() creates or re-claims owner "_iN" LinkElement
        // children when ShowElement is true. cad-core does not mutate the request graph,
        // so it only groups already materialized child elements by the same naming rule.
        TopoDS_Shape displayedShape = geometry::transformShape(shapeIt->second.shape, groupPlacement);
        shapes.push_back(displayedShape);
        visibleElements.push_back(link.object);

        const auto namedShapeIt = context.namedShapes.find(link.object);
        sources.push_back(topo::NamedShapeSource{
            link.object,
            displayedShape,
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
        });
    }

    nlohmann::json metadata = {
        {"link", kind},
        {"elements", elements},
        {"visible_elements", visibleElements},
        {"element_count", readElementCount(object)},
        {"materialized_elements", true},
    };
    if (shapes.empty()) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const TopoDS_Shape shape = compoundOf(shapes);
    publishLinkedShape(
        object,
        context,
        shape,
        shapeKindForShape(shape),
        metadata,
        topo::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes)
{
    if (shapes.size() == 1U) {
        return shapes.front();
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        builder.Add(compound, shape);
    }
    return compound;
}

}  // namespace

void executeAppLink(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::Link::Link(),
    // adds LINK_PARAMS_LINK including "LinkedObject", "LinkTransform", "LinkPlacement" and
    // "Placement"; Link::isLinkGroup() returns ElementCount > 0, and LinkBaseExtension routes
    // ElementList through child LinkElement objects when the link acts as an element group.
    if (!document::readLinks(object, "ElementList").empty()) {
        executeElementGroupLike(object,
                                context,
                                {"LinkedObject",
                                 "LinkTransform",
                                 "LinkPlacement",
                                 "LinkClaimChild",
                                 "Scale",
                                 "ScaleVector",
                                 "PlacementList",
                                 "ScaleList",
                                 "VisibilityList",
                                 "ShowElement",
                                 "ElementCount",
                                 "ElementList",
                                 "LinkMode",
                                 "LinkExecute",
                                 "ColoredElements",
                                 "LinkCopyOnChange",
                                 "LinkCopyOnChangeSource",
                                 "LinkCopyOnChangeGroup",
                                 "LinkCopyOnChangeTouched"},
                                "app_link_group");
        return;
    }

    if (readElementCount(object) > 0U && document::readBool(object, "ShowElement").value_or(true)) {
        executeMaterializedElementGroupLike(object,
                                            context,
                                            {"LinkedObject",
                                             "LinkTransform",
                                             "LinkPlacement",
                                             "LinkClaimChild",
                                             "Scale",
                                             "ScaleVector",
                                             "PlacementList",
                                             "ScaleList",
                                             "VisibilityList",
                                             "ShowElement",
                                             "ElementCount",
                                             "ElementList",
                                             "LinkMode",
                                             "LinkExecute",
                                             "ColoredElements",
                                             "LinkCopyOnChange",
                                             "LinkCopyOnChangeSource",
                                             "LinkCopyOnChangeGroup",
                                             "LinkCopyOnChangeTouched"},
                                            "app_link_group");
        return;
    }

    if (readElementCount(object) > 0U) {
        executeCollapsedElementCountLink(object,
                                         context,
                                         {"LinkedObject",
                                          "LinkTransform",
                                          "LinkPlacement",
                                          "LinkClaimChild",
                                          "Scale",
                                          "ScaleVector",
                                          "PlacementList",
                                          "ScaleList",
                                          "VisibilityList",
                                          "ShowElement",
                                          "ElementCount",
                                          "ElementList",
                                          "LinkMode",
                                          "LinkExecute",
                                          "ColoredElements",
                                          "LinkCopyOnChange",
                                          "LinkCopyOnChangeSource",
                                          "LinkCopyOnChangeGroup",
                                          "LinkCopyOnChangeTouched"},
                                         "app_link_group");
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::extensionGetLinkedObject() resolves the target shape.
    executeLinkLike(object,
                    context,
                    {"LinkedObject",
                     "LinkTransform",
                     "LinkPlacement",
                     "LinkClaimChild",
                     "Scale",
                     "ScaleVector",
                     "ShowElement",
                     "ElementCount",
                     "LinkExecute",
                     "LinkCopyOnChange",
                     "LinkCopyOnChangeSource",
                     "LinkCopyOnChangeGroup",
                     "LinkCopyOnChangeTouched"},
                    "app_link");
}

void executeAppLinkElement(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkElement::LinkElement(), LINK_PARAMS_ELEMENT includes "LinkedObject",
    // "LinkTransform", "LinkPlacement", "Placement" and scale properties; LinkElement
    // reuses LinkBaseExtension's linked-object resolution and transform semantics.
    executeLinkLike(object,
                    context,
                    {"LinkedObject",
                     "LinkTransform",
                     "LinkPlacement",
                     "_LinkOwner",
                     "Scale",
                     "ScaleVector",
                     "LinkCopyOnChange",
                     "LinkCopyOnChangeSource",
                     "LinkCopyOnChangeGroup",
                     "LinkCopyOnChangeTouched"},
                    "app_link_element");
}

void executeAppLinkGroup(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkGroup::LinkGroup(), LINK_PARAMS_GROUP includes "ElementList", "Placement",
    // "VisibilityList" and "LinkMode"; the group displays its element objects with group
    // placement while keeping each child object's own link semantics.
    executeElementGroupLike(object,
                            context,
                            {"ElementList", "VisibilityList", "LinkMode", "ColoredElements"},
                            "app_link_group");
}

void executeAssemblyObject(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    // ::AssemblyObject::execute(), calls "App::Part::execute()" before optional solve().
    // cad-core currently exposes the request-local grouped display shape and explicit solve gap.
    if (!rejectUnsupportedProperties(object, context, {"Group"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    nlohmann::json group = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<topo::NamedShapeSource> sources;
    for (const auto& link : document::readLinks(object, "Group")) {
        group.push_back(link.object);
        const auto shapeIt = context.shapes.find(link.object);
        if (shapeIt != context.shapes.end()) {
            shapes.push_back(shapeIt->second.shape);
            const auto namedShapeIt = context.namedShapes.find(link.object);
            sources.push_back(topo::NamedShapeSource{
                link.object,
                shapeIt->second.shape,
                namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second,
            });
        }
    }

    nlohmann::json metadata = {
        {"assembly", "object"},
        {"group", group},
        {"solve", "not_migrated"},
    };
    if (shapes.empty()) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const TopoDS_Shape shape = compoundOf(shapes);
    publishLinkedShape(
        object,
        context,
        shape,
        shapeKindForShape(shape),
        metadata,
        topo::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

void executeAssemblyLink(const document::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyLink.cpp
    // ::AssemblyLink::execute(), calls "updateContents()" and then "App::Part::execute()";
    // LinkedObject is the sub-assembly or component link target, Rigid remains metadata here.
    executeLinkLike(object,
                    context,
                    {"LinkedObject", "Rigid", "Group", "Type", "Id", "Uid"},
                    "assembly_link");
}

}  // namespace cad_core::features
