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

#include <algorithm>
#include <array>
#include <cctype>
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
    std::vector<topo::LinkedSubshapeRetag> sourceToTargetElements;
};

struct LinkElementEntry {
    std::size_t index = 0U;
    std::string name;
    const document::DocumentObject* object = nullptr;
};

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);
std::optional<LinkShapeBuild> baseLinkedShape(const document::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const document::Link& link,
                                              bool linkTransform);

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

void addDistinctAlias(std::vector<std::string>& aliases,
                      const std::string& alias,
                      const std::vector<std::string>& existingNames)
{
    if (alias.empty()) {
        return;
    }
    if (std::find(existingNames.begin(), existingNames.end(), alias) != existingNames.end()) {
        return;
    }
    if (std::find(aliases.begin(), aliases.end(), alias) != aliases.end()) {
        return;
    }
    aliases.push_back(alias);
}

std::string objectLabel(const std::string& objectName, const runtime::ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(objectName);
    if (objectIt == context.documentObjects.end() || objectIt->second == nullptr) {
        return objectName;
    }
    return document::readString(*objectIt->second, "Label").value_or(objectName);
}

std::string linkedObjectLabel(const document::Link& link, const runtime::ComputeContext& context)
{
    return objectLabel(link.object, context);
}

bool parseNonNegativeIndex(const std::string& text, std::size_t& index)
{
    if (text.empty()) {
        return false;
    }
    std::size_t parsed = 0U;
    for (const char item : text) {
        if (!std::isdigit(static_cast<unsigned char>(item))) {
            return false;
        }
        parsed = parsed * 10U + static_cast<std::size_t>(item - '0');
    }
    index = parsed;
    return true;
}

struct LinkSubpath {
    std::string token;
    std::string localSubname;
};

std::optional<LinkSubpath> splitLinkSubpath(const std::string& subname)
{
    const auto dot = subname.find('.');
    if (dot == std::string::npos || dot == 0U || dot + 1U >= subname.size()) {
        return std::nullopt;
    }
    return LinkSubpath{subname.substr(0U, dot), subname.substr(dot + 1U)};
}

std::string stripLinkedObjectPrefix(const std::string& subname,
                                    const document::Link& link,
                                    const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::extensionGetSubObject(), for paths that contain the linked object,
    // compares the first token with "linked->getNameInDocument()" or "$" + "linked->Label"
    // and then resolves "dot + 1". cad-core keeps the original subname as an exact alias,
    // but resolves topology through the linked-object-local suffix.
    const std::string prefix = link.object + ".";
    if (subname.rfind(prefix, 0U) == 0U) {
        return subname.substr(prefix.size());
    }

    const std::string labelPrefix = "$" + linkedObjectLabel(link, context) + ".";
    if (subname.rfind(labelPrefix, 0U) == 0U) {
        return subname.substr(labelPrefix.size());
    }

    return subname;
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

std::optional<TopoDS_Shape> resolveLocalSubshape(const TopoDS_Shape& shape,
                                                 const topo::NamedShape* namedShape,
                                                 const std::string& subname,
                                                 const std::string& stableSubname,
                                                 std::string& resolvedElement)
{
    if (subname.empty()) {
        resolvedElement.clear();
        return shape;
    }
    resolvedElement = subname;
    if (namedShape != nullptr) {
        const auto resolved = topo::resolveElementReference(*namedShape, subname, stableSubname);
        if (resolved.status == topo::ElementResolveStatus::Resolved && resolved.element) {
            resolvedElement = *resolved.element;
            return topo::subshapeByName(*namedShape, resolvedElement);
        }
        return std::nullopt;
    }
    return topo::subshapeByName(shape, subname);
}

void addRetagAliasCandidates(std::vector<std::string>& exactAliases,
                             const std::vector<std::string>& aliases,
                             const std::vector<std::string>& existingNames)
{
    for (const std::string& alias : aliases) {
        addDistinctAlias(exactAliases, alias, existingNames);
    }
}

bool looksLikeExternalFullSubname(const std::string& fullSubname,
                                  const document::Link& link,
                                  const runtime::ComputeContext& context)
{
    const auto dot = fullSubname.find('.');
    if (dot == std::string::npos || dot == 0U) {
        return false;
    }

    const std::string token = fullSubname.substr(0U, dot);
    if (token == link.object || token == "$" + linkedObjectLabel(link, context)) {
        return false;
    }

    return fullSubname.find('#') != std::string::npos || fullSubname.find('.') != fullSubname.rfind('.');
}

void addMappedPostfixAlias(std::vector<std::string>& exactAliases,
                           const std::string& sourceElementName,
                           const std::string& postfix,
                           const std::vector<std::string>& existingNames)
{
    if (sourceElementName.empty() || postfix.empty()) {
        return;
    }
    const std::string mappedAlias = postfix.front() == ';'
        ? sourceElementName + postfix
        : sourceElementName + ";" + postfix;
    addDistinctAlias(exactAliases, mappedAlias, existingNames);
}

void addExternalMappedPostfixAlias(std::vector<std::string>& exactAliases,
                                   const std::string& sourceElementName,
                                   const std::string& fullSubname,
                                   const document::Link& link,
                                   const runtime::ComputeContext& context,
                                   const std::vector<std::string>& existingNames)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap(), for external linked documents, builds postfix
    // "Data::POSTFIX_EXTERNAL_TAG" and prepends "Data::ComplexGeoData::elementMapPrefix()"
    // before calling "geoData->reTagElementMap(..., postfix)".
    if (!looksLikeExternalFullSubname(fullSubname, link, context)) {
        return;
    }
    addMappedPostfixAlias(exactAliases, sourceElementName, ";:X;" + fullSubname, existingNames);
}

void addArrayIndexMappedPostfixAlias(std::vector<std::string>& exactAliases,
                                     const std::string& sourceElementName,
                                     std::size_t index,
                                     const std::vector<std::string>& existingNames)
{
    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::extensionGetSubObject(), for collapsed ElementCount, sets postfix to
    // "Data::POSTFIX_INDEX + std::to_string(idx)" when idx is non-zero before retagging.
    if (index == 0U) {
        return;
    }
    addMappedPostfixAlias(exactAliases, sourceElementName, ";:I" + std::to_string(index), existingNames);
}

std::optional<LinkShapeBuild> linkedElementListSubshape(const document::DocumentObject& object,
                                                        runtime::ComputeContext& context,
                                                        const document::Link& link,
                                                        const document::DocumentObject& linkedObject,
                                                        const std::string& subname,
                                                        const std::string& stableSubname,
                                                        const std::string& rawSubname,
                                                        const std::string& rawStableSubname,
                                                        const std::string& rawFullSubname)
{
    const auto links = document::readLinks(linkedObject, "ElementList");
    if (links.empty()) {
        return std::nullopt;
    }

    const auto subpath = splitLinkSubpath(subname);
    if (!subpath) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    bool matched = parseNonNegativeIndex(subpath->token, index);
    if (!matched) {
        for (std::size_t candidate = 0U; candidate < links.size(); ++candidate) {
            const std::string& elementName = links.at(candidate).object;
            if (subpath->token == elementName || subpath->token == "$" + objectLabel(elementName, context)) {
                index = candidate;
                matched = true;
                break;
            }
        }
    }
    if (!matched || index >= links.size()) {
        return std::nullopt;
    }

    const std::string& elementName = links.at(index).object;
    const auto shapeIt = context.shapes.find(elementName);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(elementName);
    const topo::NamedShape* elementNamedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    std::string resolvedElement;
    const std::string localStableSubname =
        splitLinkSubpath(stableSubname).value_or(LinkSubpath{subpath->token, subpath->localSubname}).localSubname;
    auto selected = resolveLocalSubshape(
        shapeIt->second.shape,
        elementNamedShape,
        subpath->localSubname,
        localStableSubname,
        resolvedElement
    );
    if (!selected) {
        return std::nullopt;
    }

    const TopoDS_Shape displayedShape =
        geometry::transformShape(*selected, objectGlobalPlacement(linkedObject, context));
    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::vector<std::string> existingNames = {subname, stableSubname, resolvedElement};
    addRetagAliasCandidates(exactAliases,
                            {rawSubname,
                             rawStableSubname,
                             rawFullSubname,
                             elementName + "." + subpath->localSubname,
                             std::to_string(index) + "." + subpath->localSubname,
                             "$" + objectLabel(elementName, context) + "." + subpath->localSubname},
                            existingNames);
    addExternalMappedPostfixAlias(exactAliases,
                                  resolvedElement,
                                  rawFullSubname,
                                  link,
                                  context,
                                  existingNames);
    (void)object;
    return LinkShapeBuild{
        displayedShape,
        subname,
        targetElementName,
        {topo::LinkedSubshapeRetag{subname, targetElementName, exactAliases}},
    };
}

std::optional<std::size_t> collapsedOwnerIndex(const std::string& ownerName,
                                               const std::string& token,
                                               std::size_t elementCount)
{
    const std::string normalized = token.empty() || token.front() != '$' ? token : token.substr(1U);
    const std::string prefix = ownerName + "_i";
    if (normalized.rfind(prefix, 0U) != 0U) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    if (!parseNonNegativeIndex(normalized.substr(prefix.size()), index) || index >= elementCount) {
        return std::nullopt;
    }
    return index;
}

std::optional<LinkShapeBuild> collapsedElementSubshape(const document::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const document::Link& link,
                                                       const document::DocumentObject& linkedObject,
                                                       const std::string& subname,
                                                       const std::string& stableSubname,
                                                       const std::string& rawSubname,
                                                       const std::string& rawStableSubname,
                                                       const std::string& rawFullSubname)
{
    const auto elementCount = readElementCount(linkedObject);
    if (elementCount == 0U || document::readBool(linkedObject, "ShowElement").value_or(true)) {
        return std::nullopt;
    }

    const auto subpath = splitLinkSubpath(subname);
    if (!subpath) {
        return std::nullopt;
    }

    const auto targetLink = document::readLink(linkedObject, "LinkedObject");
    if (!targetLink) {
        return std::nullopt;
    }

    std::size_t index = 0U;
    bool matched = parseNonNegativeIndex(subpath->token, index);
    if (!matched) {
        const auto ownerIndex = collapsedOwnerIndex(linkedObject.name, subpath->token, elementCount);
        if (ownerIndex) {
            index = *ownerIndex;
            matched = true;
        }
    }
    if (!matched
        && (subpath->token == targetLink->object
            || subpath->token == "$" + linkedObjectLabel(*targetLink, context))) {
        index = 0U;
        matched = true;
    }
    if (!matched || index >= elementCount) {
        return std::nullopt;
    }

    const bool linkTransform = document::readBool(linkedObject, "LinkTransform").value_or(false);
    const auto baseShape = baseLinkedShape(linkedObject, context, *targetLink, linkTransform);
    if (!baseShape) {
        return std::nullopt;
    }

    const auto placements = readPlacementList(linkedObject, context);
    if (!placements) {
        return std::nullopt;
    }
    const auto scales = readScaleList(linkedObject, context);
    if (!scales) {
        return std::nullopt;
    }

    TopoDS_Shape displayedElement = applyScale(
        baseShape->shape,
        index < scales->size() ? scales->at(index) : std::array<double, 3> {1.0, 1.0, 1.0}
    );
    displayedElement = geometry::transformShape(
        displayedElement,
        index < placements->size() ? placements->at(index) : defaultElementPlacement(index)
    );
    displayedElement = applyScale(displayedElement, readScaleVector(linkedObject));
    displayedElement = geometry::transformShape(displayedElement, linkPlacement(linkedObject, context));

    std::string resolvedElement = subpath->localSubname;
    auto selected = topo::subshapeByName(displayedElement, subpath->localSubname);
    if (!selected) {
        return std::nullopt;
    }

    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::string ownerAlias = linkedObject.name + "_i" + std::to_string(index) + "." + subpath->localSubname;
    const std::vector<std::string> existingNames = {subname, stableSubname, resolvedElement};
    addRetagAliasCandidates(exactAliases,
                            {rawSubname,
                             rawStableSubname,
                             rawFullSubname,
                             ownerAlias,
                             std::to_string(index) + "." + subpath->localSubname,
                             targetLink->object + "." + subpath->localSubname,
                             "$" + linkedObjectLabel(*targetLink, context) + "." + subpath->localSubname},
                            existingNames);
    addArrayIndexMappedPostfixAlias(exactAliases, subpath->localSubname, index, existingNames);
    addExternalMappedPostfixAlias(exactAliases,
                                  subpath->localSubname,
                                  rawFullSubname,
                                  link,
                                  context,
                                  existingNames);
    (void)object;
    (void)link;
    return LinkShapeBuild{
        *selected,
        subname,
        targetElementName,
        {topo::LinkedSubshapeRetag{subname, targetElementName, exactAliases}},
    };
}

std::optional<LinkShapeBuild> linkedGroupElementSubshape(const document::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const document::Link& link,
                                                         const std::string& subname,
                                                         const std::string& stableSubname,
                                                         const std::string& rawSubname,
                                                         const std::string& rawStableSubname,
                                                         const std::string& rawFullSubname)
{
    const auto objectIt = context.documentObjects.find(link.object);
    if (objectIt == context.documentObjects.end() || objectIt->second == nullptr) {
        return std::nullopt;
    }
    const document::DocumentObject& linkedObject = *objectIt->second;
    if (const auto selected = linkedElementListSubshape(object,
                                                       context,
                                                       link,
                                                       linkedObject,
                                                       subname,
                                                       stableSubname,
                                                       rawSubname,
                                                       rawStableSubname,
                                                       rawFullSubname)) {
        return selected;
    }
    return collapsedElementSubshape(object,
                                    context,
                                    link,
                                    linkedObject,
                                    subname,
                                    stableSubname,
                                    rawSubname,
                                    rawStableSubname,
                                    rawFullSubname);
}

std::optional<LinkShapeBuild> linkedSubshapeAt(const document::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const document::Link& link,
                                               const TopoDS_Shape& sourceShape,
                                               std::size_t index)
{
    const std::string rawSubname = link.subnames.at(index);
    const std::string rawStableSubname =
        index < link.stableSubnames.size() && !link.stableSubnames.at(index).empty()
        ? link.stableSubnames.at(index)
        : rawSubname;
    const std::string rawFullSubname =
        index < link.fullSubnames.size() ? link.fullSubnames.at(index) : rawSubname;
    const std::string subname = stripLinkedObjectPrefix(rawSubname, link, context);
    const std::string stableSubname = stripLinkedObjectPrefix(rawStableSubname, link, context);
    if (const auto selected = linkedGroupElementSubshape(object,
                                                        context,
                                                        link,
                                                        subname,
                                                        stableSubname,
                                                        rawSubname,
                                                        rawStableSubname,
                                                        rawFullSubname)) {
        return selected;
    }
    const auto namedShapeIt = context.namedShapes.find(link.object);
    std::string resolvedElement = subname;
    std::optional<TopoDS_Shape> shape;
    if (subname.empty()) {
        return LinkShapeBuild{sourceShape, std::nullopt, std::nullopt, {}};
    }
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
                                   stableSubnameDiagnosticMessage(link.object, rawStableSubname, resolved.status),
                                   object.name,
                                   "LinkedObject",
                                   "runtime",
                                   link.object,
                                   rawStableSubname);
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
                               "Invalid linked subshape " + rawSubname,
                               object.name,
                               "LinkedObject",
                               "runtime",
                               link.object,
                               rawSubname);
        context.objects[object.name] = {{"status", "error"}};
        return std::nullopt;
    }

    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::vector<std::string> resolvedNames = {subname, stableSubname, resolvedElement};
    addDistinctAlias(exactAliases, rawSubname, resolvedNames);
    addDistinctAlias(exactAliases, rawStableSubname, resolvedNames);
    addDistinctAlias(exactAliases, rawFullSubname, resolvedNames);
    addExternalMappedPostfixAlias(exactAliases,
                                  resolvedElement,
                                  rawFullSubname,
                                  link,
                                  context,
                                  resolvedNames);
    return LinkShapeBuild{
        *shape,
        resolvedElement,
        targetElementName,
        {topo::LinkedSubshapeRetag{resolvedElement, targetElementName, exactAliases}},
    };
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
    std::vector<topo::LinkedSubshapeRetag> sourceToTargetElements;
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
                std::vector<std::string> exactAliases;
                if (!selected->sourceToTargetElements.empty()) {
                    exactAliases = selected->sourceToTargetElements.front().exactAliases;
                }
                sourceToTargetElements.push_back(
                    topo::LinkedSubshapeRetag{*selected->sourceElementName, targetElementName, exactAliases}
                );
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

std::array<double, 3> showElementScaleFromList(const std::vector<std::array<double, 3>>& scales,
                                               std::size_t index)
{
    if (index >= scales.size()) {
        return {1.0, 1.0, 1.0};
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when ShowElement is true, assigns
    // "obj->Scale.setValue(scaleProp->getValues()[i].x)" to generated LinkElement children.
    const double scale = scales.at(index).at(0);
    return {scale, scale, scale};
}

std::optional<LinkShapeBuild> inheritedMaterializedLinkElementShape(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const document::Link& link,
    bool linkTransform,
    const document::DocumentObject& owner,
    std::size_t index)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    const bool hasOwnScale = document::propertyValue(object, "Scale") != nullptr
        || document::propertyValue(object, "ScaleVector") != nullptr;
    std::array<double, 3> scale = hasOwnScale ? readScaleVector(object) : std::array<double, 3>{1.0, 1.0, 1.0};
    if (!hasOwnScale) {
        const auto scales = readScaleList(owner, context);
        if (!scales) {
            return std::nullopt;
        }
        scale = showElementScaleFromList(*scales, index);
    }

    const bool hasOwnPlacement = document::propertyValue(object, "LinkPlacement") != nullptr
        || document::propertyValue(object, "Placement") != nullptr;
    gp_Trsf placement = hasOwnPlacement ? linkPlacement(object, context) : defaultElementPlacement(index);
    if (!hasOwnPlacement) {
        const auto placements = readPlacementList(owner, context);
        if (!placements) {
            return std::nullopt;
        }
        if (index < placements->size()) {
            placement = placements->at(index);
        }
    }

    selected->shape = geometry::transformShape(applyScale(selected->shape, scale), placement);
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
        const topo::NamedShape* elementNamedShape =
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
        sources.push_back(topo::NamedShapeSource{link.object, displayedShape, elementNamedShape});
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getElementIndex(), when ElementList exists, accepts digit
        // indices and "$" + child Label in addition to the element object's internal name.
        sources.push_back(topo::NamedShapeSource{std::to_string(index), displayedShape, elementNamedShape});
        const std::string label = objectLabel(link.object, context);
        if (label != link.object) {
            sources.push_back(topo::NamedShapeSource{"$" + label, displayedShape, elementNamedShape});
        }
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
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getElementIndex(), for collapsed ElementCount links, accepts
        // digit-prefixed subpaths like "1.Face1"; when the subpath starts with the linked
        // object's name or "$" + Label, it redirects that reference to the first array element.
        sources.push_back(topo::NamedShapeSource{std::to_string(index), displayedShape, sourceNamedShape});
        if (index == 0U) {
            sources.push_back(topo::NamedShapeSource{link->object, displayedShape, sourceNamedShape});
            const std::string label = linkedObjectLabel(*link, context);
            if (label != link->object) {
                sources.push_back(topo::NamedShapeSource{"$" + label, displayedShape, sourceNamedShape});
            }
        }
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

std::optional<std::size_t> materializedLinkElementIndex(const document::DocumentObject& element)
{
    const auto marker = element.name.rfind("_i");
    if (marker == std::string::npos || marker + 2U >= element.name.size()) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    for (std::size_t offset = marker + 2U; offset < element.name.size(); ++offset) {
        const unsigned char ch = static_cast<unsigned char>(element.name.at(offset));
        if (!std::isdigit(ch)) {
            return std::nullopt;
        }
        index = index * 10U + static_cast<std::size_t>(element.name.at(offset) - '0');
    }
    return index;
}

const document::DocumentObject* materializedLinkElementOwner(const document::DocumentObject& element,
                                                             const runtime::ComputeContext& context)
{
    const auto index = materializedLinkElementIndex(element);
    if (!index) {
        return nullptr;
    }

    const auto ownerValue = document::readNumber(element, "_LinkOwner");
    if (ownerValue) {
        for (const auto& [name, object] : context.documentObjects) {
            (void)name;
            if (object != nullptr && object->typeId == "App::Link"
                && object->id == static_cast<long long>(*ownerValue)
                && readElementCount(*object) > *index
                && document::readBool(*object, "ShowElement").value_or(true)) {
                return object;
            }
        }
    }

    const std::string ownerName = element.name.substr(0U, element.name.rfind("_i"));
    const auto ownerIt = context.documentObjects.find(ownerName);
    if (ownerIt == context.documentObjects.end() || ownerIt->second == nullptr) {
        return nullptr;
    }
    const document::DocumentObject* owner = ownerIt->second;
    if (owner->typeId != "App::Link" || readElementCount(*owner) <= *index
        || !document::readBool(*owner, "ShowElement").value_or(true)) {
        return nullptr;
    }
    return owner;
}

std::vector<LinkElementEntry> linkElementEntries(const document::DocumentObject& object,
                                                 const runtime::ComputeContext& context)
{
    std::vector<LinkElementEntry> entries;
    const auto elementCount = readElementCount(object);
    entries.reserve(elementCount);
    for (std::size_t index = 0; index < elementCount; ++index) {
        const std::string elementName = object.name + "_i" + std::to_string(index);
        const auto elementIt = context.documentObjects.find(elementName);
        const document::DocumentObject* element =
            elementIt == context.documentObjects.end() ? nullptr : elementIt->second;
        if (element == nullptr || !isOwnedMaterializedLinkElement(*element, object)) {
            entries.push_back(LinkElementEntry{index, elementName, nullptr});
            continue;
        }
        entries.push_back(LinkElementEntry{index, elementName, element});
    }
    return entries;
}

void publishLinkShapeBuild(const document::DocumentObject& object,
                           runtime::ComputeContext& context,
                           const document::Link& link,
                           std::optional<LinkShapeBuild> shape,
                           nlohmann::json metadata)
{
    if (!shape) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const auto sourceShapeIt = context.shapes.find(link.object);
    const runtime::ShapeValue::Kind kindValue = !shape->sourceToTargetElements.empty()
        ? shapeKindForShape(shape->shape)
        : (sourceShapeIt == context.shapes.end() ? shapeKindForShape(shape->shape) : sourceShapeIt->second.kind);
    std::optional<topo::NamedShape> linkedNamedShape;
    if (sourceShapeIt != context.shapes.end()) {
        const auto sourceNamedShapeIt = context.namedShapes.find(link.object);
        const topo::NamedShapeSource source{
            link.object,
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

bool executeInheritedMaterializedLinkElement(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::set<std::string>& allowedProperties,
    const std::string& kind)
{
    if (document::propertyValue(object, "LinkedObject") != nullptr) {
        return false;
    }

    const document::DocumentObject* owner = materializedLinkElementOwner(object, context);
    if (owner == nullptr) {
        return false;
    }

    if (!rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return true;
    }

    const auto link = document::readLink(*owner, "LinkedObject");
    if (!link) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Materialized LinkElement owner LinkedObject is not set",
                               object.name,
                               "LinkedObject",
                               "runtime",
                               owner->name);
        context.objects[object.name] = {{"status", "error"}};
        return true;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::updateGroup(), for owned children, copies parent
    // "element->LinkedObject.setValue(xlink->getValue(), xlink->getSubValues())" and syncs
    // "element->LinkTransform" with the owner link. cad-core computes that inherited state
    // request-locally without mutating the DocumentObject graph.
    const bool linkTransform = document::readBool(*owner, "LinkTransform").value_or(false);
    const auto index = materializedLinkElementIndex(object).value_or(0U);
    const auto shape = inheritedMaterializedLinkElementShape(
        object,
        context,
        *link,
        linkTransform,
        *owner,
        index
    );
    nlohmann::json metadata = {
        {"link", kind},
        {"linked_object", link->object},
        {"link_transform", linkTransform},
        {"link_owner", owner->name},
        {"inherited_linked_object", true},
    };
    publishLinkShapeBuild(object, context, *link, shape, metadata);
    return true;
}

std::optional<LinkShapeBuild> syntheticLinkElementShape(
    const document::DocumentObject& object,
    runtime::ComputeContext& context,
    const document::Link& link,
    bool linkTransform,
    std::size_t index,
    const std::vector<gp_Trsf>& placements,
    const std::vector<std::array<double, 3>>& scales)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when ShowElement is true, creates or re-claims
    // child LinkElement objects named owner "_iN"; it copies PlacementList[idx] and
    // ScaleList[idx] into the child, otherwise using default grid placement and scale 1.
    TopoDS_Shape displayedShape = applyScale(
        selected->shape,
        showElementScaleFromList(scales, index)
    );
    displayedShape = geometry::transformShape(
        displayedShape,
        index < placements.size() ? placements.at(index) : defaultElementPlacement(index)
    );
    selected->shape = displayedShape;
    return selected;
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

    const auto entries = linkElementEntries(object, context);
    bool needsSyntheticElements = false;
    for (const auto& entry : entries) {
        if (entry.object == nullptr) {
            needsSyntheticElements = true;
            break;
        }
    }

    std::vector<gp_Trsf> placements;
    std::vector<std::array<double, 3>> scales;
    std::optional<document::Link> link;
    bool linkTransform = false;
    if (needsSyntheticElements) {
        const auto parsedPlacements = readPlacementList(object, context);
        if (!parsedPlacements) {
            return;
        }
        placements = *parsedPlacements;
        const auto parsedScales = readScaleList(object, context);
        if (!parsedScales) {
            return;
        }
        scales = *parsedScales;
        link = linkedObjectProperty(object, context);
        if (!link) {
            return;
        }
        linkTransform = document::readBool(object, "LinkTransform").value_or(false);
    }

    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<topo::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const gp_Trsf groupPlacement = objectGlobalPlacement(object, context);
    const auto sourceNamedShapeIt =
        link ? context.namedShapes.find(link->object) : context.namedShapes.end();
    const topo::NamedShape* sourceNamedShape =
        sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second;
    bool hasMaterializedElement = false;
    bool hasSyntheticElement = false;

    for (const auto& entry : entries) {
        elements.push_back(entry.name);
        if (!isVisibleElement(entry.index, visibility)) {
            continue;
        }

        TopoDS_Shape displayedShape;
        const topo::NamedShape* elementNamedShape = nullptr;
        if (entry.object != nullptr) {
            const auto shapeIt = context.shapes.find(entry.name);
            if (shapeIt == context.shapes.end()) {
                continue;
            }
            displayedShape = shapeIt->second.shape;
            const auto namedShapeIt = context.namedShapes.find(entry.name);
            elementNamedShape =
                namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
            hasMaterializedElement = true;
        }
        else {
            const auto synthetic = syntheticLinkElementShape(
                object,
                context,
                *link,
                linkTransform,
                entry.index,
                placements,
                scales
            );
            if (!synthetic) {
                continue;
            }
            displayedShape = synthetic->shape;
            elementNamedShape = sourceNamedShape;
            hasSyntheticElement = true;
        }

        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::update() creates or re-claims owner "_iN" LinkElement
        // children when ShowElement is true. cad-core keeps the graph immutable, so missing
        // children are represented only in this recompute result under the same request-local names.
        displayedShape = geometry::transformShape(displayedShape, groupPlacement);
        shapes.push_back(displayedShape);
        visibleElements.push_back(entry.name);
        sources.push_back(topo::NamedShapeSource{
            entry.name,
            displayedShape,
            elementNamedShape,
        });
    }

    nlohmann::json metadata = {
        {"link", kind},
        {"elements", elements},
        {"visible_elements", visibleElements},
        {"element_count", readElementCount(object)},
        {"materialized_elements", hasMaterializedElement},
        {"synthetic_elements", hasSyntheticElement},
        {"request_local_elements", hasSyntheticElement},
    };
    if (link) {
        metadata["linked_object"] = link->object;
        metadata["link_transform"] = linkTransform;
    }
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
    const std::set<std::string> allowedProperties = {"LinkedObject",
                                                     "LinkTransform",
                                                     "LinkPlacement",
                                                     "_LinkOwner",
                                                     "Scale",
                                                     "ScaleVector",
                                                     "LinkCopyOnChange",
                                                     "LinkCopyOnChangeSource",
                                                     "LinkCopyOnChangeGroup",
                                                     "LinkCopyOnChangeTouched"};
    if (executeInheritedMaterializedLinkElement(object, context, allowedProperties, "app_link_element")) {
        return;
    }

    executeLinkLike(object,
                    context,
                    allowedProperties,
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
