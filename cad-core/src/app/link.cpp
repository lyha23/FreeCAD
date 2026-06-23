#include "cad_core/app/link.h"

#include "cad_core/app/copy_on_change.h"
#include "cad_core/app/link_support.h"
#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

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

namespace cad_core::app {

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
    std::vector<part::LinkedSubshapeRetag> sourceToTargetElements;
};

struct LinkElementEntry {
    std::size_t index = 0U;
    std::string name;
    const app::DocumentObject* object = nullptr;
};

struct PlainGroupChildEntry {
    std::size_t index = 0U;
    std::string name;
    const app::DocumentObject* object = nullptr;
    std::vector<std::string> ownerAliases;
    bool isGroup = false;
};

struct PlainGroupSubpathMatch {
    const PlainGroupChildEntry* entry = nullptr;
    std::string ownerAlias;
    std::string localSubname;
    std::string localStableSubname;
};

struct CollapsedElementLists {
    std::vector<gp_Trsf> placements;
    std::vector<std::array<double, 3>> scales;
};

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);
std::optional<LinkShapeBuild> baseLinkedShape(const app::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const app::Link& link,
                                              bool linkTransform);
std::optional<std::size_t> materializedLinkElementIndex(const app::DocumentObject& element);
std::optional<CollapsedElementLists> collapsedElementListsForObject(const app::DocumentObject& object,
                                                                    runtime::ComputeContext& context,
                                                                    std::size_t elementCount);
void addCollapsedElementCountOwnerListSyncUpdate(runtime::ComputeContext& context,
                                                 const app::DocumentObject& owner,
                                                 std::size_t elementCount);
void addShowElementElementListOwnerSyncUpdate(runtime::ComputeContext& context,
                                              const app::DocumentObject& owner,
                                              std::size_t elementCount);
void addShowElementElementListChildSyncUpdates(runtime::ComputeContext& context,
                                               const app::DocumentObject& owner,
                                               const std::vector<app::Link>& elements);
void addCopyOnChangeLifecycleUpdates(runtime::ComputeContext& context,
                                     const app::DocumentObject& object,
                                     const app::Link& link);
std::optional<app::Link> linkedPlainGroupProperty(const app::DocumentObject& object,
                                                       const runtime::ComputeContext& context);

bool isPlainDocumentObjectGroup(const app::DocumentObject& object)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObjectGroup.cpp
    // ::DocumentObjectGroup::DocumentObjectGroup(), calls "GroupExtension::initExtension(this)".
    // This is the plain group subset used by LinkBaseExtension::linkedPlainGroup().
    return object.typeId == "App::DocumentObjectGroup"
        || object.typeId == "App::DocumentObjectGroupPython";
}

const app::DocumentObject* documentObjectByName(const runtime::ComputeContext& context,
                                                     const std::string& name)
{
    const auto objectIt = context.documentObjects.find(name);
    if (objectIt == context.documentObjects.end()) {
        return nullptr;
    }
    return objectIt->second;
}

nlohmann::json linkNamesJson(const std::vector<app::Link>& links)
{
    nlohmann::json names = nlohmann::json::array();
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

std::optional<gp_Trsf> placementProperty(const app::DocumentObject& object,
                                         const std::string& property)
{
    const auto placement = app::readPlacement(object, property);
    if (!placement) {
        return std::nullopt;
    }
    return base::placementFromComponents(placement->base, placement->rotation);
}

gp_Trsf parentPlacement(const app::DocumentObject& object, const runtime::ComputeContext& context)
{
    const auto parentIt = context.parentGroupByObject.find(object.name);
    if (parentIt == context.parentGroupByObject.end()) {
        return gp_Trsf();
    }
    const auto placementIt = context.globalPlacements.find(parentIt->second);
    return placementIt == context.globalPlacements.end() ? gp_Trsf() : placementIt->second;
}

gp_Trsf linkPlacement(const app::DocumentObject& object, const runtime::ComputeContext& context)
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

gp_Trsf objectGlobalPlacement(const app::DocumentObject& object, const runtime::ComputeContext& context)
{
    const auto placementIt = context.globalPlacements.find(object.name);
    return placementIt == context.globalPlacements.end() ? gp_Trsf() : placementIt->second;
}

std::array<double, 3> readScaleVector(const app::DocumentObject& object)
{
    const auto scaleVector = app::readVector3(object, "ScaleVector");
    if (scaleVector) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getScaleVector(), returns "getScaleVectorValue()" when the
        // "ScaleVector" property exists; scalar "Scale" is only the fallback.
        return *scaleVector;
    }
    const double scale = app::readNumber(object, "Scale").value_or(1.0);
    return {scale, scale, scale};
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

const nlohmann::json& rawPropertyPayload(const app::PropertyValue& value)
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

    return base::placementFromComponents(base, quaternion);
}

std::optional<std::vector<gp_Trsf>> readPlacementList(const app::DocumentObject& object,
                                                      runtime::ComputeContext& context)
{
    const auto* value = app::propertyValue(object, "PlacementList");
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

std::optional<std::vector<std::array<double, 3>>> readScaleList(const app::DocumentObject& object,
                                                               runtime::ComputeContext& context)
{
    const auto* value = app::propertyValue(object, "ScaleList");
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

std::size_t readElementCount(const app::DocumentObject& object)
{
    const double count = app::readNumber(object, "ElementCount").value_or(0.0);
    return count <= 0.0 ? 0U : static_cast<std::size_t>(count);
}

gp_Trsf defaultElementPlacement(std::size_t index)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when ShowElement is false, grows PlacementList with
    // "Base::Vector3d(i % 10, (i / 10) % 10, i / 100)" and identity rotation.
    return base::placementFromComponents(
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

std::string stableSubnameDiagnosticMessage(const std::string& target,
                                           const std::string& stableSubname,
                                           part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Deleted:
            return "Stable subname " + stableSubname + " on " + target
                + " was deleted by source history";
        case part::ElementResolveStatus::Split:
            return "Stable subname " + stableSubname + " on " + target
                + " was split by source history";
        case part::ElementResolveStatus::Resolved:
        case part::ElementResolveStatus::Unresolved:
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

std::vector<bool> readVisibilityList(const app::DocumentObject& object)
{
    const auto* value = app::propertyValue(object, "VisibilityList");
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
    return app::readString(*objectIt->second, "Label").value_or(objectName);
}

std::string linkedObjectLabel(const app::Link& link, const runtime::ComputeContext& context)
{
    return objectLabel(link.object, context);
}

void addPlainGroupOwnerAlias(std::vector<std::string>& aliases,
                             const std::string& alias,
                             const std::string& primaryName)
{
    if (alias.empty() || alias == primaryName) {
        return;
    }
    if (std::find(aliases.begin(), aliases.end(), alias) == aliases.end()) {
        aliases.push_back(alias);
    }
}

std::vector<std::string> plainGroupOwnerNames(const PlainGroupChildEntry& entry)
{
    std::vector<std::string> names {entry.name};
    for (const std::string& alias : entry.ownerAliases) {
        if (std::find(names.begin(), names.end(), alias) == names.end()) {
            names.push_back(alias);
        }
    }
    return names;
}

void appendPlainGroupChildren(const app::DocumentObject& groupObject,
                              const runtime::ComputeContext& context,
                              std::vector<PlainGroupChildEntry>& children,
                              std::set<std::string>& visited,
                              const std::vector<std::string>& groupPathAliases)
{
    for (const auto& link : app::readLinks(groupObject, "Group")) {
        const app::DocumentObject* child = documentObjectByName(context, link.object);
        if (child == nullptr || !visited.insert(link.object).second) {
            continue;
        }

        const std::size_t index = children.size();
        std::vector<std::string> aliases;
        addPlainGroupOwnerAlias(aliases, std::to_string(index), link.object);
        const std::string label = objectLabel(link.object, context);
        if (label != link.object) {
            addPlainGroupOwnerAlias(aliases, "$" + label, link.object);
        }
        for (const std::string& groupAlias : groupPathAliases) {
            addPlainGroupOwnerAlias(aliases, groupAlias + "." + link.object, link.object);
            if (label != link.object) {
                addPlainGroupOwnerAlias(aliases, groupAlias + ".$" + label, link.object);
            }
        }

        const bool childIsGroup = isPlainDocumentObjectGroup(*child);
        children.push_back(PlainGroupChildEntry{index, link.object, child, aliases, childIsGroup});
        if (!childIsGroup) {
            continue;
        }

        std::vector<std::string> childGroupPathAliases;
        childGroupPathAliases.push_back(link.object);
        childGroupPathAliases.push_back(std::to_string(index));
        if (label != link.object) {
            childGroupPathAliases.push_back("$" + label);
        }
        for (const std::string& groupAlias : groupPathAliases) {
            childGroupPathAliases.push_back(groupAlias + "." + link.object);
            if (label != link.object) {
                childGroupPathAliases.push_back(groupAlias + ".$" + label);
            }
        }
        appendPlainGroupChildren(*child, context, children, visited, childGroupPathAliases);
    }
}

std::vector<PlainGroupChildEntry> plainGroupChildren(const app::DocumentObject& groupObject,
                                                    const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GroupExtension.cpp
    // ::GroupExtension::getAllChildren() appends each direct Group child, then recurses into
    // nested GroupExtension children while guarding duplicates with "rset.insert(obj)".
    std::vector<PlainGroupChildEntry> children;
    std::set<std::string> visited;
    appendPlainGroupChildren(groupObject, context, children, visited, {});
    return children;
}

std::vector<PlainGroupChildEntry> elementListWithPlainGroupChildren(const std::vector<app::Link>& links,
                                                                    const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::updateGroup(), when ElementList contains a GroupExtension object,
    // starts from "children = getElementListValue()" and appends each group's getAllChildren().
    // This keeps explicit ElementList semantics distinct from linkedPlainGroup() while exposing
    // the same request-local _ChildCache-style traversal for grouped children.
    std::vector<PlainGroupChildEntry> children;
    std::set<std::string> visited;
    for (const auto& link : links) {
        const app::DocumentObject* child = documentObjectByName(context, link.object);
        if (child == nullptr || !visited.insert(link.object).second) {
            continue;
        }

        const std::size_t index = children.size();
        std::vector<std::string> aliases;
        addPlainGroupOwnerAlias(aliases, std::to_string(index), link.object);
        const std::string label = objectLabel(link.object, context);
        if (label != link.object) {
            addPlainGroupOwnerAlias(aliases, "$" + label, link.object);
        }
        const bool childIsGroup = isPlainDocumentObjectGroup(*child);
        children.push_back(PlainGroupChildEntry{index, link.object, child, aliases, childIsGroup});
        if (!childIsGroup) {
            continue;
        }

        std::vector<std::string> childGroupPathAliases {link.object, std::to_string(index)};
        if (label != link.object) {
            childGroupPathAliases.push_back("$" + label);
        }
        appendPlainGroupChildren(*child, context, children, visited, childGroupPathAliases);
    }
    return children;
}

std::optional<PlainGroupSubpathMatch> matchPlainGroupSubpath(const std::vector<PlainGroupChildEntry>& children,
                                                             const std::string& subname,
                                                             const std::string& stableSubname)
{
    PlainGroupSubpathMatch best;
    std::size_t bestPrefixSize = 0U;
    for (const auto& child : children) {
        if (child.isGroup) {
            continue;
        }
        for (const std::string& ownerName : plainGroupOwnerNames(child)) {
            const std::string prefix = ownerName + ".";
            if (subname.rfind(prefix, 0U) != 0U || subname.size() <= prefix.size()) {
                continue;
            }
            if (prefix.size() <= bestPrefixSize) {
                continue;
            }
            best.entry = &child;
            best.ownerAlias = ownerName;
            best.localSubname = subname.substr(prefix.size());
            best.localStableSubname = stableSubname == subname ? best.localSubname : stableSubname;
            for (const std::string& stableOwnerName : plainGroupOwnerNames(child)) {
                const std::string stablePrefix = stableOwnerName + ".";
                if (stableSubname.rfind(stablePrefix, 0U) == 0U
                    && stableSubname.size() > stablePrefix.size()) {
                    best.localStableSubname = stableSubname.substr(stablePrefix.size());
                    break;
                }
            }
            bestPrefixSize = prefix.size();
        }
    }
    if (best.entry == nullptr) {
        return std::nullopt;
    }
    return best;
}

std::vector<std::string> linkedObjectAliasOwners(const app::Link& link,
                                                 const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::getElementIndex() accepts "$" + child Label, and
    // ::extensionGetSubObject() compares subpath tokens with "linked->Label.getValue()".
    // Keep that label owner in the ElementMap retag so a later LinkSub can walk a Link chain.
    std::vector<std::string> aliases;
    const std::string label = linkedObjectLabel(link, context);
    if (!label.empty() && label != link.object) {
        aliases.push_back("$" + label);
    }
    return aliases;
}

std::vector<std::string> linkedTargetSubpathAliasOwners(const std::string& owner,
                                                        const app::Link& link,
                                                        const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::extensionGetSubObject(), after getElementIndex() consumes an
    // element token like "1", still accepts a linked-object prefix such as "Box.Face1".
    std::vector<std::string> aliases;
    if (owner.empty()) {
        return aliases;
    }
    aliases.push_back(owner + "." + link.object);
    const std::string label = linkedObjectLabel(link, context);
    if (!label.empty() && label != link.object) {
        aliases.push_back(owner + ".$" + label);
    }
    return aliases;
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
                                    const app::Link& link,
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
    const auto parsed = part::parseSubshapeName(sourceElementName);
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
    const auto parsed = part::parseSubshapeName(sourceElementName);
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
                                                 const part::NamedShape* namedShape,
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
        const auto resolved = part::resolveElementReference(*namedShape, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            resolvedElement = *resolved.element;
            return part::subshapeByName(*namedShape, resolvedElement);
        }
        return std::nullopt;
    }
    return part::subshapeByName(shape, subname);
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
                                  const app::Link& link,
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
    std::size_t index = 0U;
    if (parseNonNegativeIndex(token, index)) {
        return false;
    }
    const std::string ownerElementPrefix = link.object + "_i";
    if (token.rfind(ownerElementPrefix, 0U) == 0U
        && parseNonNegativeIndex(token.substr(ownerElementPrefix.size()), index)) {
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
                                   const app::Link& link,
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

std::optional<LinkShapeBuild> linkedElementListSubshape(const app::DocumentObject& object,
                                                        runtime::ComputeContext& context,
                                                        const app::Link& link,
                                                        const app::DocumentObject& linkedObject,
                                                        const std::string& subname,
                                                        const std::string& stableSubname,
                                                        const std::string& rawSubname,
                                                        const std::string& rawStableSubname,
                                                        const std::string& rawFullSubname)
{
    const auto links = app::readLinks(linkedObject, "ElementList");
    if (links.empty()) {
        return std::nullopt;
    }

    const auto subpath = splitLinkSubpath(subname);
    if (!subpath) {
        return std::nullopt;
    }
    const auto children = elementListWithPlainGroupChildren(links, context);
    const auto match = matchPlainGroupSubpath(children, subname, stableSubname);
    if (!match || match->entry == nullptr) {
        return std::nullopt;
    }

    const std::string& elementName = match->entry->name;
    const auto shapeIt = context.shapes.find(elementName);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(elementName);
    const part::NamedShape* elementNamedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    std::string resolvedElement;
    auto selected = resolveLocalSubshape(
        shapeIt->second.shape,
        elementNamedShape,
        match->localSubname,
        match->localStableSubname,
        resolvedElement
    );
    if (!selected) {
        return std::nullopt;
    }

    const TopoDS_Shape displayedShape =
        base::transformShape(*selected, objectGlobalPlacement(linkedObject, context));
    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::vector<std::string> existingNames = {subname, stableSubname, resolvedElement};
    std::vector<std::string> aliasCandidates = {rawSubname, rawStableSubname, rawFullSubname};
    for (const std::string& ownerName : plainGroupOwnerNames(*match->entry)) {
        aliasCandidates.push_back(ownerName + "." + match->localSubname);
        aliasCandidates.push_back(linkedObject.name + "." + ownerName + "." + match->localSubname);
    }
    addRetagAliasCandidates(exactAliases, aliasCandidates, existingNames);
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
        {part::LinkedSubshapeRetag{subname, targetElementName, exactAliases}},
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

std::optional<LinkShapeBuild> collapsedElementSubshape(const app::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const app::Link& link,
                                                       const app::DocumentObject& linkedObject,
                                                       const std::string& subname,
                                                       const std::string& stableSubname,
                                                       const std::string& rawSubname,
                                                       const std::string& rawStableSubname,
                                                       const std::string& rawFullSubname)
{
    const auto elementCount = readElementCount(linkedObject);
    if (elementCount == 0U || app::readBool(linkedObject, "ShowElement").value_or(true)) {
        return std::nullopt;
    }

    const auto subpath = splitLinkSubpath(subname);
    if (!subpath) {
        return std::nullopt;
    }

    const auto targetLink = app::readLink(linkedObject, "LinkedObject");
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

    const bool linkTransform = app::readBool(linkedObject, "LinkTransform").value_or(false);
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
    displayedElement = base::transformShape(
        displayedElement,
        index < placements->size() ? placements->at(index) : defaultElementPlacement(index)
    );
    displayedElement = applyScale(displayedElement, readScaleVector(linkedObject));
    displayedElement = base::transformShape(displayedElement, linkPlacement(linkedObject, context));

    const std::string localSubname = stripLinkedObjectPrefix(subpath->localSubname, *targetLink, context);
    std::string resolvedElement = localSubname;
    auto selected = part::subshapeByName(displayedElement, localSubname);
    if (!selected) {
        return std::nullopt;
    }

    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::string ownerAlias = linkedObject.name + "_i" + std::to_string(index) + "." + localSubname;
    const std::vector<std::string> existingNames = {subname, stableSubname, resolvedElement};
    std::vector<std::string> aliasCandidates = {
        rawSubname,
        rawStableSubname,
        rawFullSubname,
        ownerAlias,
        linkedObject.name + "_i" + std::to_string(index) + "."
            + targetLink->object + "." + localSubname,
        std::to_string(index) + "." + localSubname,
        std::to_string(index) + "." + targetLink->object + "." + localSubname,
        targetLink->object + "." + localSubname,
    };
    const std::string targetLabel = linkedObjectLabel(*targetLink, context);
    if (targetLabel != targetLink->object) {
        aliasCandidates.push_back("$" + targetLabel + "." + localSubname);
        aliasCandidates.push_back(std::to_string(index) + ".$" + targetLabel + "." + localSubname);
    }
    addRetagAliasCandidates(exactAliases, aliasCandidates, existingNames);
    addArrayIndexMappedPostfixAlias(exactAliases, localSubname, index, existingNames);
    addExternalMappedPostfixAlias(exactAliases,
                                  localSubname,
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
        {part::LinkedSubshapeRetag{subname, targetElementName, exactAliases}},
    };
}

std::optional<LinkShapeBuild> linkedPlainGroupSubshape(const app::DocumentObject& object,
                                                       runtime::ComputeContext& context,
                                                       const app::Link& link,
                                                       const app::DocumentObject& linkedObject,
                                                       const std::string& subname,
                                                       const std::string& stableSubname,
                                                       const std::string& rawSubname,
                                                       const std::string& rawStableSubname,
                                                       const std::string& rawFullSubname)
{
    const auto groupLink = linkedPlainGroupProperty(linkedObject, context);
    if (!groupLink) {
        return std::nullopt;
    }
    const app::DocumentObject* groupObject = documentObjectByName(context, groupLink->object);
    if (groupObject == nullptr) {
        return std::nullopt;
    }

    const auto children = plainGroupChildren(*groupObject, context);
    const auto match = matchPlainGroupSubpath(children, subname, stableSubname);
    if (!match || match->entry == nullptr) {
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(match->entry->name);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }
    const auto namedShapeIt = context.namedShapes.find(match->entry->name);
    const part::NamedShape* elementNamedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    std::string resolvedElement;
    auto selected = resolveLocalSubshape(shapeIt->second.shape,
                                         elementNamedShape,
                                         match->localSubname,
                                         match->localStableSubname,
                                         resolvedElement);
    if (!selected) {
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::getElementIndex() recurses through "_ChildCache" group entries,
    // and ::flattenSubname() keeps the first non-group child token as the effective subpath.
    // cad-core mirrors that for request-local plain group links without persisting _ChildCache.
    TopoDS_Shape displayedShape =
        base::transformShape(applyScale(*selected, readScaleVector(linkedObject)),
                                 linkPlacement(linkedObject, context));
    const std::string targetElementName = targetElementNameForResolvedSource(resolvedElement);
    std::vector<std::string> exactAliases;
    const std::vector<std::string> existingNames = {subname, stableSubname, resolvedElement};
    std::vector<std::string> aliasCandidates = {rawSubname, rawStableSubname, rawFullSubname};
    for (const std::string& ownerName : plainGroupOwnerNames(*match->entry)) {
        aliasCandidates.push_back(ownerName + "." + match->localSubname);
        aliasCandidates.push_back(linkedObject.name + "." + ownerName + "." + match->localSubname);
    }
    addRetagAliasCandidates(exactAliases, aliasCandidates, existingNames);
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
        {part::LinkedSubshapeRetag{subname, targetElementName, exactAliases}},
    };
}

std::optional<LinkShapeBuild> linkedGroupElementSubshape(const app::DocumentObject& object,
                                                         runtime::ComputeContext& context,
                                                         const app::Link& link,
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
    const app::DocumentObject& linkedObject = *objectIt->second;
    if (const auto selected = linkedPlainGroupSubshape(object,
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

std::optional<LinkShapeBuild> linkedSubshapeAt(const app::DocumentObject& object,
                                               runtime::ComputeContext& context,
                                               const app::Link& link,
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
        const auto resolved = part::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            resolvedElement = *resolved.element;
            shape = part::subshapeByName(namedShapeIt->second, resolvedElement);
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
        shape = part::subshapeByName(sourceShape, subname);
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
        {part::LinkedSubshapeRetag{resolvedElement, targetElementName, exactAliases}},
    };
}

std::optional<LinkShapeBuild> linkedSubshape(const app::DocumentObject& object,
                                             runtime::ComputeContext& context,
                                             const app::Link& link,
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
    std::vector<part::LinkedSubshapeRetag> sourceToTargetElements;
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
                    part::LinkedSubshapeRetag{*selected->sourceElementName, targetElementName, exactAliases}
                );
            }
        }
    }
    if (shapes.empty()) {
        return LinkShapeBuild{sourceShape, std::nullopt, std::nullopt, {}};
    }
    return LinkShapeBuild{compoundOf(shapes), std::nullopt, std::nullopt, sourceToTargetElements};
}

void publishLinkedShape(const app::DocumentObject& object,
                        runtime::ComputeContext& context,
                        const TopoDS_Shape& shape,
                        runtime::ShapeValue::Kind kind,
                        const nlohmann::json& metadata,
                        std::optional<part::NamedShape> namedShape = std::nullopt)
{
    context.shapes[object.name] = runtime::ShapeValue{kind, shape};
    context.mesh[object.name] = cad_core::part::meshForShape(shape);
    context.subshapes[object.name] = part::subshapeMapForShape(shape);
    context.namedShapes[object.name] = namedShape.value_or(part::indexedNamedShapeForObject(object.name, shape));

    nlohmann::json result = metadata;
    result["status"] = "ok";
    result["shape"] = shapeLabelForShape(shape);
    result["bbox"] = cad_core::part::objectBBoxForShape(shape);
    result["volume"] = cad_core::part::volumeForShape(shape);
    result["kernel"] = cad_core::part::kernelVersion();
    context.objects[object.name] = result;
}

void publishEmptyLink(const app::DocumentObject& object,
                      runtime::ComputeContext& context,
                      const nlohmann::json& metadata)
{
    nlohmann::json result = metadata;
    result["status"] = "ok";
    context.objects[object.name] = result;
}

std::optional<app::Link> linkedObjectProperty(const app::DocumentObject& object,
                                                   runtime::ComputeContext& context)
{
    const auto link = app::readLink(object, "LinkedObject");
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

std::optional<LinkShapeBuild> baseLinkedShape(const app::DocumentObject& object,
                                              runtime::ComputeContext& context,
                                              const app::Link& link,
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
            selected->shape = base::transformShape(selected->shape, sourcePlacementIt->second.Inverted());
        }
    }

    return selected;
}

std::optional<LinkShapeBuild> linkShape(const app::DocumentObject& object,
                                        runtime::ComputeContext& context,
                                        const app::Link& link,
                                        bool linkTransform)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    TopoDS_Shape shape = selected->shape;
    shape = applyScale(shape, readScaleVector(object));
    selected->shape = base::transformShape(shape, linkPlacement(object, context));
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
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& link,
    bool linkTransform,
    const app::DocumentObject& owner,
    std::size_t index)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), in the ShowElement ElementCount branch, assigns child
    // "obj->Placement" and "obj->Scale" from owner "PlacementList" / "ScaleList", or from
    // default grid placement and scale 1. Existing child placement/scale is not authoritative.
    const auto scales = readScaleList(owner, context);
    if (!scales) {
        return std::nullopt;
    }
    std::array<double, 3> scale = showElementScaleFromList(*scales, index);

    const auto placements = readPlacementList(owner, context);
    if (!placements) {
        return std::nullopt;
    }
    gp_Trsf placement = defaultElementPlacement(index);
    if (index < placements->size()) {
        placement = placements->at(index);
    }

    selected->shape = base::transformShape(applyScale(selected->shape, scale), placement);
    return selected;
}

void executeLinkLike(const app::DocumentObject& object,
                     runtime::ComputeContext& context,
                     const std::set<std::string>& allowedProperties,
                     const std::string& kind)
{
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto link = linkedObjectProperty(object, context);
    if (!link) {
        return;
    }
    addCopyOnChangeLifecycleUpdates(context, object, *link);

    const bool linkTransform = app::readBool(object, "LinkTransform").value_or(false);
    const auto shape = linkShape(object, context, *link, linkTransform);
    nlohmann::json metadata = {
        {"link", kind},
        {"linked_object", link->object},
        {"link_transform", linkTransform},
    };
    if (object.typeId == "Assembly::AssemblyLink") {
        metadata["rigid"] = app::readBool(object, "Rigid").value_or(true);
    }

    if (!shape) {
        publishEmptyLink(object, context, metadata);
        return;
    }

    const auto sourceShapeIt = context.shapes.find(link->object);
    const runtime::ShapeValue::Kind kindValue = !shape->sourceToTargetElements.empty()
        ? shapeKindForShape(shape->shape)
        : (sourceShapeIt == context.shapes.end() ? shapeKindForShape(shape->shape) : sourceShapeIt->second.kind);
    std::optional<part::NamedShape> linkedNamedShape;
    if (sourceShapeIt != context.shapes.end()) {
        const auto sourceNamedShapeIt = context.namedShapes.find(link->object);
        const part::NamedShapeSource source{
            link->object,
            sourceShapeIt->second.shape,
            sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second,
            linkedObjectAliasOwners(*link, context),
        };
        if (!shape->sourceToTargetElements.empty()) {
            linkedNamedShape = part::namedShapeForLinkedSubshapes(
                object.name,
                shape->shape,
                source,
                shape->sourceToTargetElements
            );
        }
        else {
            linkedNamedShape = part::namedShapeForLinkedShape(object.name, shape->shape, source);
        }
    }
    publishLinkedShape(object, context, shape->shape, kindValue, metadata, linkedNamedShape);
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& shapes);

void executeElementGroupLike(const app::DocumentObject& object,
                             runtime::ComputeContext& context,
                             const std::set<std::string>& allowedProperties,
                             const std::string& kind)
{
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto links = app::readLinks(object, "ElementList");
    addShowElementElementListOwnerSyncUpdate(context, object, links.size());
    addShowElementElementListChildSyncUpdates(context, object, links);
    const auto children = elementListWithPlainGroupChildren(links, context);
    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<part::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const gp_Trsf groupPlacement = objectGlobalPlacement(object, context);

    for (const auto& child : children) {
        elements.push_back(child.name);
        if (!isVisibleElement(child.index, visibility)) {
            continue;
        }

        const auto shapeIt = context.shapes.find(child.name);
        if (shapeIt == context.shapes.end()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::extensionGetSubObject(), when ElementList is present, routes
        // subobject lookup through "elements[idx]->getSubObject(...)"; LinkGroup owns the
        // group placement, so cad-core applies this object's global placement to each child
        // shape before composing the request-local display compound.
        TopoDS_Shape displayedShape = base::transformShape(shapeIt->second.shape, groupPlacement);
        shapes.push_back(displayedShape);
        visibleElements.push_back(child.name);

        const auto namedShapeIt = context.namedShapes.find(child.name);
        const part::NamedShape* elementNamedShape =
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getElementIndex(), when ElementList or _ChildCache exists,
        // accepts digit indices, child names, "$" + child Label, and recurses through group
        // entries before returning the flattened child index.
        sources.push_back(
            part::NamedShapeSource{child.name, displayedShape, elementNamedShape, child.ownerAliases}
        );
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
        part::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

std::optional<app::Link> linkedPlainGroupProperty(const app::DocumentObject& object,
                                                       const runtime::ComputeContext& context)
{
    const auto link = app::readLink(object, "LinkedObject");
    if (!link || !link->subnames.empty()) {
        return std::nullopt;
    }
    const app::DocumentObject* linkedObject = documentObjectByName(context, link->object);
    if (linkedObject == nullptr || !isPlainDocumentObjectGroup(*linkedObject)) {
        return std::nullopt;
    }
    return link;
}

void executeLinkedPlainGroupLike(const app::DocumentObject& object,
                                 runtime::ComputeContext& context,
                                 const std::set<std::string>& allowedProperties,
                                 const std::string& kind)
{
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto link = linkedPlainGroupProperty(object, context);
    if (!link) {
        return;
    }
    addCopyOnChangeLifecycleUpdates(context, object, *link);
    const app::DocumentObject* groupObject = documentObjectByName(context, link->object);
    if (groupObject == nullptr) {
        return;
    }

    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<part::NamedShapeSource> sources;
    const gp_Trsf placement = linkPlacement(object, context);
    const auto scale = readScaleVector(object);
    const auto children = plainGroupChildren(*groupObject, context);

    for (const auto& child : children) {
        elements.push_back(child.name);
        const auto shapeIt = context.shapes.find(child.name);
        if (shapeIt == context.shapes.end()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::_getElementListProperty(), when "linkedPlainGroup()" exists,
        // returns "&group->Group"; updateGroup() then recursively expands GroupExtension
        // children into "_ChildCache". cad-core keeps the group graph immutable and builds an
        // equivalent request-local child list for display and LinkSub aliasing.
        TopoDS_Shape displayedShape = base::transformShape(
            applyScale(shapeIt->second.shape, scale),
            placement
        );
        shapes.push_back(displayedShape);
        visibleElements.push_back(child.name);

        const auto namedShapeIt = context.namedShapes.find(child.name);
        const part::NamedShape* elementNamedShape =
            namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
        sources.push_back(
            part::NamedShapeSource{child.name, displayedShape, elementNamedShape, child.ownerAliases}
        );
    }

    nlohmann::json metadata = {
        {"link", kind},
        {"linked_object", link->object},
        {"linked_plain_group", true},
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
        part::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

void executeCollapsedElementCountLink(const app::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      const std::set<std::string>& allowedProperties,
                                      const std::string& kind)
{
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto elementCount = readElementCount(object);
    const auto elementLists = collapsedElementListsForObject(object, context, elementCount);
    if (!elementLists) {
        return;
    }

    const auto link = linkedObjectProperty(object, context);
    if (!link) {
        return;
    }
    addCopyOnChangeLifecycleUpdates(context, object, *link);

    const bool linkTransform = app::readBool(object, "LinkTransform").value_or(false);
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
    std::vector<part::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const auto linkScale = readScaleVector(object);
    const gp_Trsf linkObjectPlacement = linkPlacement(object, context);
    const auto sourceNamedShapeIt = context.namedShapes.find(link->object);
    const part::NamedShape* sourceNamedShape =
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
            index < elementLists->scales.size() ? elementLists->scales.at(index)
                                                : std::array<double, 3> {1.0, 1.0, 1.0}
        );
        displayedShape = base::transformShape(
            displayedShape,
            index < elementLists->placements.size() ? elementLists->placements.at(index)
                                                    : defaultElementPlacement(index)
        );
        displayedShape = applyScale(displayedShape, linkScale);
        displayedShape = base::transformShape(displayedShape, linkObjectPlacement);

        shapes.push_back(displayedShape);
        visibleIndices.push_back(index);
        const std::string ownerName = object.name + "_i" + std::to_string(index);
        sources.push_back(part::NamedShapeSource{
            ownerName,
            displayedShape,
            sourceNamedShape,
            linkedTargetSubpathAliasOwners(ownerName, *link, context),
        });
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getElementIndex(), for collapsed ElementCount links, accepts
        // digit-prefixed subpaths like "1.Face1"; when the subpath starts with the linked
        // object's name or "$" + Label, it redirects that reference to the first array element.
        const std::string indexName = std::to_string(index);
        sources.push_back(part::NamedShapeSource{
            indexName,
            displayedShape,
            sourceNamedShape,
            linkedTargetSubpathAliasOwners(indexName, *link, context),
        });
        if (index == 0U) {
            sources.push_back(part::NamedShapeSource{link->object, displayedShape, sourceNamedShape});
            const std::string label = linkedObjectLabel(*link, context);
            if (label != link->object) {
                sources.push_back(part::NamedShapeSource{"$" + label, displayedShape, sourceNamedShape});
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
        part::namedShapeForPreservedSources(object.name, shape, sources)
    );
}

bool isOwnedMaterializedLinkElement(const app::DocumentObject& element,
                                    const app::DocumentObject& owner)
{
    if (element.typeId != "App::LinkElement") {
        return false;
    }
    const auto ownerValue = app::readNumber(element, "_LinkOwner");
    return !ownerValue || static_cast<long long>(*ownerValue) == 0 || static_cast<long long>(*ownerValue) == owner.id;
}

nlohmann::json defaultElementPlacementJson(std::size_t index)
{
    return {
        {"Base",
         {static_cast<double>(index % 10U),
          static_cast<double>((index / 10U) % 10U),
          static_cast<double>(index / 100U)}},
        {"Rotation", {0.0, 0.0, 0.0, 1.0}},
    };
}

nlohmann::json defaultElementScaleJson()
{
    return {1.0, 1.0, 1.0};
}

nlohmann::json placementValueJson(const app::Placement& placement)
{
    return {
        {"Base", {placement.base.at(0), placement.base.at(1), placement.base.at(2)}},
        {"Rotation",
         {placement.rotation.at(0),
          placement.rotation.at(1),
          placement.rotation.at(2),
          placement.rotation.at(3)}},
    };
}

nlohmann::json scaleValueJson(const std::array<double, 3>& scale)
{
    return {scale.at(0), scale.at(1), scale.at(2)};
}

bool isShowElementToggleOffState(const app::DocumentObject& owner)
{
    return !app::readBool(owner, "ShowElement").value_or(true)
        && !app::readLinks(owner, "ElementList").empty();
}

nlohmann::json normalizedCollapsedPlacementListJson(const app::DocumentObject& owner,
                                                    std::size_t elementCount,
                                                    bool& changed)
{
    changed = false;
    nlohmann::json values = nlohmann::json::array();
    const auto* property = app::propertyValue(owner, "PlacementList");
    const nlohmann::json* payload = nullptr;
    if (property != nullptr) {
        payload = &rawPropertyPayload(*property);
    }

    const std::size_t existingSize = payload != nullptr && payload->is_array() ? payload->size() : 0U;
    changed = property == nullptr || payload == nullptr || !payload->is_array() || existingSize != elementCount;
    const std::size_t preservedSize = std::min(existingSize, elementCount);
    for (std::size_t index = 0; index < preservedSize; ++index) {
        values.push_back(payload->at(index));
    }
    for (std::size_t index = preservedSize; index < elementCount; ++index) {
        values.push_back(defaultElementPlacementJson(index));
    }
    return values;
}

nlohmann::json normalizedCollapsedScaleListJson(const app::DocumentObject& owner,
                                                std::size_t elementCount,
                                                bool& changed)
{
    changed = false;
    nlohmann::json values = nlohmann::json::array();
    const auto* property = app::propertyValue(owner, "ScaleList");
    const nlohmann::json* payload = nullptr;
    if (property != nullptr) {
        payload = &rawPropertyPayload(*property);
    }

    const std::size_t existingSize = payload != nullptr && payload->is_array() ? payload->size() : 0U;
    changed = property == nullptr || payload == nullptr || !payload->is_array() || existingSize != elementCount;
    const std::size_t preservedSize = std::min(existingSize, elementCount);
    for (std::size_t index = 0; index < preservedSize; ++index) {
        values.push_back(payload->at(index));
    }
    for (std::size_t index = preservedSize; index < elementCount; ++index) {
        values.push_back(defaultElementScaleJson());
    }
    return values;
}

std::optional<nlohmann::json> collapsedVisibilityListResizeJson(const app::DocumentObject& owner,
                                                                std::size_t elementCount)
{
    const auto* property = app::propertyValue(owner, "VisibilityList");
    if (property == nullptr) {
        return std::nullopt;
    }
    const auto& payload = rawPropertyPayload(*property);
    if (!payload.is_array() || payload.size() <= elementCount) {
        return std::nullopt;
    }

    nlohmann::json values = nlohmann::json::array();
    for (std::size_t index = 0; index < elementCount; ++index) {
        if (!payload.at(index).is_boolean()) {
            return std::nullopt;
        }
        values.push_back(payload.at(index));
    }
    return values;
}

void addCollapsedElementCountOwnerListSyncUpdate(runtime::ComputeContext& context,
                                                 const app::DocumentObject& owner,
                                                 std::size_t elementCount)
{
    bool placementChanged = false;
    bool scaleChanged = false;
    nlohmann::json properties = nlohmann::json::object();
    const nlohmann::json placements =
        normalizedCollapsedPlacementListJson(owner, elementCount, placementChanged);
    const nlohmann::json scales = normalizedCollapsedScaleListJson(owner, elementCount, scaleChanged);
    if (placementChanged) {
        properties["PlacementList"] = {
            {"PropertyType", "App::PropertyPlacementList"},
            {"value", placements},
        };
    }
    if (scaleChanged) {
        properties["ScaleList"] = {
            {"PropertyType", "App::PropertyVectorList"},
            {"value", scales},
        };
    }
    if (auto visibility = collapsedVisibilityListResizeJson(owner, elementCount)) {
        properties["VisibilityList"] = {
            {"PropertyType", "App::PropertyBoolList"},
            {"value", *visibility},
        };
    }
    if (properties.empty()) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), in the "!_getShowElementValue()" ElementCount branch,
    // resizes "ScaleList" to ElementCount and grows/crops "PlacementList"; VisibilityList is
    // cropped when it is longer than ElementCount. cad-core returns that mutation as a stateless
    // documentObjectUpdates suggestion.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "element_count_owner_lists_sync"},
        {"object", owner.name},
        {"objectId", owner.id},
        {"typeId", owner.typeId},
        {"properties", properties},
    });
}

std::optional<nlohmann::json> showElementVisibilityListResizeJson(const app::DocumentObject& owner,
                                                                  std::size_t elementCount)
{
    const auto* property = app::propertyValue(owner, "VisibilityList");
    if (property == nullptr) {
        return std::nullopt;
    }
    const auto& payload = rawPropertyPayload(*property);
    if (!payload.is_array() || payload.size() == elementCount) {
        return std::nullopt;
    }

    nlohmann::json values = nlohmann::json::array();
    const std::size_t preservedSize = std::min<std::size_t>(payload.size(), elementCount);
    for (std::size_t index = 0; index < preservedSize; ++index) {
        if (!payload.at(index).is_boolean()) {
            return std::nullopt;
        }
        values.push_back(payload.at(index));
    }
    for (std::size_t index = preservedSize; index < elementCount; ++index) {
        values.push_back(true);
    }
    return values;
}

void addShowElementElementListOwnerSyncUpdate(runtime::ComputeContext& context,
                                              const app::DocumentObject& owner,
                                              std::size_t elementCount)
{
    if (owner.typeId != "App::Link" || !app::readBool(owner, "ShowElement").value_or(true)) {
        return;
    }

    nlohmann::json properties = nlohmann::json::object();
    if (app::propertyValue(owner, "ElementCount") != nullptr
        && readElementCount(owner) != elementCount) {
        properties["ElementCount"] = {
            {"PropertyType", "App::PropertyInteger"},
            {"value", elementCount},
        };
    }
    if (auto visibility = showElementVisibilityListResizeJson(owner, elementCount)) {
        properties["VisibilityList"] = {
            {"PropertyType", "App::PropertyBoolList"},
            {"value", *visibility},
        };
    }
    if (properties.empty()) {
        return;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when "prop == getElementListProperty()", rebuilds
    // "VisibilityList" to the ElementList size and then sets "ElementCount" from
    // "getElementListProperty()->getSize()". cad-core reports the same owner mutation as a
    // stateless documentObjectUpdates suggestion.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "show_element_element_list_owner_sync"},
        {"object", owner.name},
        {"objectId", owner.id},
        {"typeId", owner.typeId},
        {"properties", properties},
    });
}

void addShowElementToggleOffUpdates(runtime::ComputeContext& context,
                                    const app::DocumentObject& owner,
                                    const std::vector<app::Link>& elements,
                                    const nlohmann::json& placements,
                                    const nlohmann::json& scales)
{
    nlohmann::json properties = {
        {"ElementList", {{"PropertyType", "App::PropertyLinkList"}, {"values", nlohmann::json::array()}}},
        {"PlacementList", {{"PropertyType", "App::PropertyPlacementList"}, {"value", placements}}},
        {"ScaleList", {{"PropertyType", "App::PropertyVectorList"}, {"value", scales}}},
    };

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), when "_getShowElementValue()" becomes false, preserves
    // child "element->Placement" and "element->getScaleVector()", clears "ElementList", then
    // removes the materialized child objects. cad-core returns the equivalent writeback as
    // documentObjectUpdates and keeps the request graph immutable.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "show_element_toggle_off_owner_sync"},
        {"object", owner.name},
        {"objectId", owner.id},
        {"typeId", owner.typeId},
        {"properties", properties},
    });

    for (const auto& elementLink : elements) {
        const app::DocumentObject* element = documentObjectByName(context, elementLink.object);
        if (element == nullptr || !isOwnedMaterializedLinkElement(*element, owner)) {
            continue;
        }
        context.documentObjectUpdates.push_back({
            {"action", "delete"},
            {"reason", "show_element_toggle_off_child"},
            {"object", element->name},
            {"typeId", element->typeId},
            {"owner", owner.name},
            {"ownerId", owner.id},
        });
    }
}

std::optional<CollapsedElementLists> materializedElementListsForToggleOff(
    const app::DocumentObject& owner,
    runtime::ComputeContext& context,
    std::size_t elementCount)
{
    CollapsedElementLists lists;
    nlohmann::json placements = nlohmann::json::array();
    nlohmann::json scales = nlohmann::json::array();
    const auto elements = app::readLinks(owner, "ElementList");
    lists.placements.reserve(std::max(elementCount, elements.size()));
    lists.scales.reserve(std::max(elementCount, elements.size()));

    for (std::size_t index = 0; index < elements.size(); ++index) {
        const app::DocumentObject* element = documentObjectByName(context, elements.at(index).object);
        const auto placement = element == nullptr ? std::nullopt : app::readPlacement(*element, "Placement");
        if (placement) {
            placements.push_back(placementValueJson(*placement));
            lists.placements.push_back(base::placementFromComponents(placement->base, placement->rotation));
        }
        else {
            placements.push_back(defaultElementPlacementJson(index));
            lists.placements.push_back(defaultElementPlacement(index));
        }

        const std::array<double, 3> scale = element == nullptr ? std::array<double, 3> {1.0, 1.0, 1.0}
                                                               : readScaleVector(*element);
        scales.push_back(scaleValueJson(scale));
        lists.scales.push_back(scale);
    }
    addShowElementToggleOffUpdates(context, owner, elements, placements, scales);
    return lists;
}

std::optional<CollapsedElementLists> collapsedElementListsForObject(const app::DocumentObject& object,
                                                                    runtime::ComputeContext& context,
                                                                    std::size_t elementCount)
{
    if (isShowElementToggleOffState(object)) {
        return materializedElementListsForToggleOff(object, context, elementCount);
    }

    const auto placements = readPlacementList(object, context);
    if (!placements) {
        return std::nullopt;
    }
    const auto scales = readScaleList(object, context);
    if (!scales) {
        return std::nullopt;
    }
    addCollapsedElementCountOwnerListSyncUpdate(context, object, elementCount);
    return CollapsedElementLists{*placements, *scales};
}

nlohmann::json showElementPlacementJson(const app::DocumentObject& owner, std::size_t index)
{
    const auto* value = app::propertyValue(owner, "PlacementList");
    if (value != nullptr) {
        const auto& payload = rawPropertyPayload(*value);
        if (payload.is_array() && index < payload.size() && payload.at(index).is_object()) {
            return payload.at(index);
        }
    }
    return defaultElementPlacementJson(index);
}

double showElementScaleValue(const app::DocumentObject& owner, std::size_t index)
{
    const auto* value = app::propertyValue(owner, "ScaleList");
    if (value != nullptr) {
        const auto& payload = rawPropertyPayload(*value);
        if (payload.is_array() && index < payload.size()) {
            std::array<double, 3> scale {};
            if (parseVector3Value(payload.at(index), scale)) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
                // ::LinkBaseExtension::update(), ShowElement branch assigns
                // "obj->Scale.setValue(scaleProp->getValues()[i].x)".
                return scale.at(0);
            }
        }
    }
    return 1.0;
}

bool nearlyEqual(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= 1e-9;
}

bool jsonArrayMatches(const nlohmann::json& value, const std::vector<double>& expected)
{
    if (!value.is_array() || value.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (!value.at(index).is_number() || !nearlyEqual(value.at(index).get<double>(), expected.at(index))) {
            return false;
        }
    }
    return true;
}

bool showElementPlacementMatches(const app::DocumentObject& element,
                                 const app::DocumentObject& owner,
                                 std::size_t index)
{
    const auto placement = app::readPlacement(element, "Placement");
    if (!placement) {
        return false;
    }
    const nlohmann::json expected = showElementPlacementJson(owner, index);
    return jsonArrayMatches(expected.at("Base"), {placement->base.at(0), placement->base.at(1), placement->base.at(2)})
        && jsonArrayMatches(expected.at("Rotation"),
                            {placement->rotation.at(0),
                             placement->rotation.at(1),
                             placement->rotation.at(2),
                             placement->rotation.at(3)});
}

bool linkMatches(const app::Link& actual, const app::Link& expected)
{
    return actual.object == expected.object && actual.subnames == expected.subnames
        && actual.stableSubnamesExplicit == expected.stableSubnamesExplicit
        && actual.stableSubnames == expected.stableSubnames
        && actual.fullSubnamesExplicit == expected.fullSubnamesExplicit
        && actual.fullSubnames == expected.fullSubnames;
}

bool showElementLifecyclePropertiesMatch(const app::DocumentObject& element,
                                         const app::DocumentObject& owner,
                                         const app::Link& link,
                                         bool linkTransform,
                                         std::size_t index)
{
    const auto ownerValue = app::readNumber(element, "_LinkOwner");
    if (!ownerValue || static_cast<long long>(*ownerValue) != owner.id) {
        return false;
    }
    const auto linkedObject = app::readLink(element, "LinkedObject");
    if (!linkedObject || !linkMatches(*linkedObject, link)) {
        return false;
    }
    const auto childLinkTransform = app::readBool(element, "LinkTransform");
    if (!childLinkTransform || *childLinkTransform != linkTransform) {
        return false;
    }
    if (!showElementPlacementMatches(element, owner, index)) {
        return false;
    }
    const auto scale = app::readNumber(element, "Scale");
    return scale && nearlyEqual(*scale, showElementScaleValue(owner, index));
}

nlohmann::json linkElementLinkedObjectJson(const app::Link& link)
{
    nlohmann::json value = {
        {"PropertyType", "App::PropertyXLink"},
        {"value", link.object},
    };
    if (!link.subnames.empty()) {
        value["SubList"] = link.subnames;
    }
    if (link.stableSubnamesExplicit) {
        value["StableSubList"] = link.stableSubnames;
    }
    if (link.fullSubnamesExplicit) {
        value["FullSubList"] = link.fullSubnames;
    }
    return value;
}

long long copyOnChangeMode(const app::DocumentObject& object)
{
    const auto value = app::readNumber(object, "LinkCopyOnChange");
    return value ? static_cast<long long>(*value) : 0LL;
}

nlohmann::json propertyLinkJson(const std::string& target)
{
    return {
        {"PropertyType", "App::PropertyLink"},
        {"value", target},
    };
}

nlohmann::json propertyXLinkJson(const std::string& target)
{
    return {
        {"PropertyType", "App::PropertyXLink"},
        {"value", target},
    };
}

nlohmann::json propertyBoolJson(bool value)
{
    return {
        {"PropertyType", "App::PropertyBool"},
        {"value", value},
    };
}

nlohmann::json propertyIntegerJson(long long value)
{
    return {
        {"PropertyType", "App::PropertyInteger"},
        {"value", value},
    };
}

std::string copyOnChangeGroupName(const app::DocumentObject& object,
                                  const std::optional<app::Link>& group)
{
    return group ? group->object : object.name + "_CopyOnChangeGroup";
}

std::string copyOnChangeObjectName(const app::DocumentObject& object,
                                   const app::Link& link)
{
    (void)link;
    return object.name + "_CopyOnChangeObject";
}

void addCopyOnChangeGroupCreateUpdate(runtime::ComputeContext& context,
                                      const app::DocumentObject& object,
                                      const std::string& groupName)
{
    if (documentObjectByName(context, groupName) != nullptr) {
        return;
    }

    context.documentObjectUpdates.push_back({
        {"action", "create"},
        {"reason", "copy_on_change_group_create"},
        {"object", groupName},
        {"typeId", "App::DocumentObjectGroup"},
        {"owner", object.name},
        {"ownerId", object.id},
    });
}

void addCopyOnChangeLifecycleUpdates(runtime::ComputeContext& context,
                                     const app::DocumentObject& object,
                                     const app::Link& link)
{
    const app::CopyOnChangeDocumentView view {&context.documentObjects, &context.dependencies};
    const app::CopyOnChangeLifecycleResult result =
        app::buildCopyOnChangeLifecycleUpdates(object, link, view);
    for (const auto& diagnostic : result.diagnostics) {
        context.diagnostics.push_back(diagnostic);
    }
    for (const auto& update : result.documentObjectUpdates) {
        context.documentObjectUpdates.push_back(update);
    }
}

nlohmann::json linkElementLifecycleProperties(const app::DocumentObject& owner,
                                              const app::Link& link,
                                              bool linkTransform,
                                              std::size_t index)
{
    nlohmann::json placement = showElementPlacementJson(owner, index);
    placement["PropertyType"] = "App::PropertyPlacement";
    return {
        {"_LinkOwner", {{"PropertyType", "App::PropertyInteger"}, {"value", owner.id}}},
        {"LinkedObject", linkElementLinkedObjectJson(link)},
        {"LinkTransform", {{"PropertyType", "App::PropertyBool"}, {"value", linkTransform}}},
        {"Placement", placement},
        {"Scale", {{"PropertyType", "App::PropertyFloat"}, {"value", showElementScaleValue(owner, index)}}},
    };
}

nlohmann::json linkElementListSyncProperties(const app::DocumentObject& owner,
                                             const app::Link& link,
                                             bool linkTransform,
                                             bool syncLinkedObject)
{
    nlohmann::json properties = {
        {"_LinkOwner", {{"PropertyType", "App::PropertyInteger"}, {"value", owner.id}}},
        {"LinkTransform", {{"PropertyType", "App::PropertyBool"}, {"value", linkTransform}}},
    };
    if (syncLinkedObject) {
        properties["LinkedObject"] = linkElementLinkedObjectJson(link);
    }
    return properties;
}

bool syncableElementListChild(const app::DocumentObject& element,
                              const app::DocumentObject& owner)
{
    if (element.typeId != "App::LinkElement") {
        return false;
    }
    const auto ownerValue = app::readNumber(element, "_LinkOwner");
    return !ownerValue || static_cast<long long>(*ownerValue) == 0
        || static_cast<long long>(*ownerValue) == owner.id;
}

bool elementListChildSyncsLinkedObject(const app::DocumentObject& element)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::syncElementList(), after syncing "_LinkOwner" and "LinkTransform",
    // returns early when "element->LinkCopyOnChange.getValue() == 2"; Link.h names 2 as
    // "CopyOnChangeOwned", so cad-core must not overwrite that child's LinkedObject.
    const auto copyOnChange = app::readNumber(element, "LinkCopyOnChange");
    return !copyOnChange || static_cast<long long>(*copyOnChange) != 2;
}

bool elementListChildSyncPropertiesMatch(const app::DocumentObject& element,
                                         const app::DocumentObject& owner,
                                         const app::Link& link,
                                         bool linkTransform)
{
    const auto ownerValue = app::readNumber(element, "_LinkOwner");
    if (!ownerValue || static_cast<long long>(*ownerValue) != owner.id) {
        return false;
    }
    if (elementListChildSyncsLinkedObject(element)) {
        const auto linkedObject = app::readLink(element, "LinkedObject");
        if (!linkedObject || !linkMatches(*linkedObject, link)) {
            return false;
        }
    }
    const auto childLinkTransform = app::readBool(element, "LinkTransform");
    return childLinkTransform && *childLinkTransform == linkTransform;
}

void addShowElementElementListChildSyncUpdates(runtime::ComputeContext& context,
                                               const app::DocumentObject& owner,
                                               const std::vector<app::Link>& elements)
{
    if (owner.typeId != "App::Link" || !app::readBool(owner, "ShowElement").value_or(true)) {
        return;
    }
    const auto link = app::readLink(owner, "LinkedObject");
    if (!link) {
        return;
    }
    const bool linkTransform = app::readBool(owner, "LinkTransform").value_or(false);
    for (std::size_t index = 0; index < elements.size(); ++index) {
        const app::DocumentObject* element = documentObjectByName(context, elements.at(index).object);
        if (element == nullptr || !syncableElementListChild(*element, owner)
            || elementListChildSyncPropertiesMatch(*element, owner, *link, linkTransform)) {
            continue;
        }
        const auto ownerValue = app::readNumber(*element, "_LinkOwner");
        const bool orphan = !ownerValue || static_cast<long long>(*ownerValue) == 0;
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::syncElementList(), for each LinkElement in "ElementList", sets
        // "_LinkOwner" to the owner ID and synchronizes child "LinkTransform". It synchronizes
        // "LinkedObject" unless "LinkCopyOnChange == 2" marks the child as an owned copy.
        // Unlike the ElementCount branch, this does not assign child Placement or Scale.
        const bool syncLinkedObject = elementListChildSyncsLinkedObject(*element);
        context.documentObjectUpdates.push_back({
            {"action", orphan ? "claim" : "update"},
            {"reason", "show_element_element_list_child_sync"},
            {"object", element->name},
            {"typeId", element->typeId},
            {"owner", owner.name},
            {"ownerId", owner.id},
            {"index", index},
            {"properties", linkElementListSyncProperties(owner, *link, linkTransform, syncLinkedObject)},
        });
    }
}

void addLinkElementCreateUpdate(runtime::ComputeContext& context,
                                const app::DocumentObject& owner,
                                const app::Link& link,
                                bool linkTransform,
                                const LinkElementEntry& entry)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update(), ShowElement branch creates missing LinkElement children
    // named owner "_i" index and copies owner LinkedObject / LinkTransform / placement / scale.
    // cad-core keeps the graph stateless and returns the equivalent mutation as a frontend
    // documentObjectUpdates suggestion.
    context.documentObjectUpdates.push_back({
        {"action", "create"},
        {"reason", "show_element_missing_child"},
        {"object", entry.name},
        {"typeId", "App::LinkElement"},
        {"owner", owner.name},
        {"ownerId", owner.id},
        {"index", entry.index},
        {"properties", linkElementLifecycleProperties(owner, link, linkTransform, entry.index)},
    });
}

void addLinkElementSyncUpdate(runtime::ComputeContext& context,
                              const app::DocumentObject& owner,
                              const app::Link& link,
                              bool linkTransform,
                              const LinkElementEntry& entry)
{
    if (entry.object == nullptr
        || showElementLifecyclePropertiesMatch(*entry.object, owner, link, linkTransform, entry.index)) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update() keeps existing owned ShowElement children in sync by assigning
    // "obj->Placement", "obj->Scale", and updateGroup() synchronizes LinkedObject/LinkTransform.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "show_element_child_sync"},
        {"object", entry.name},
        {"typeId", "App::LinkElement"},
        {"owner", owner.name},
        {"ownerId", owner.id},
        {"index", entry.index},
        {"properties", linkElementLifecycleProperties(owner, link, linkTransform, entry.index)},
    });
}

void addLinkElementClaimUpdate(runtime::ComputeContext& context,
                               const app::DocumentObject& owner,
                               const app::Link& link,
                               bool linkTransform,
                               const LinkElementEntry& entry)
{
    if (entry.object == nullptr) {
        return;
    }
    const auto ownerValue = app::readNumber(*entry.object, "_LinkOwner");
    if (ownerValue && static_cast<long long>(*ownerValue) == owner.id) {
        addLinkElementSyncUpdate(context, owner, link, linkTransform, entry);
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::update() re-claims orphan children when "_LinkOwner" is empty/0,
    // then updateGroup() synchronizes owner id, LinkedObject and LinkTransform.
    context.documentObjectUpdates.push_back({
        {"action", "claim"},
        {"reason", "show_element_orphan_child"},
        {"object", entry.name},
        {"typeId", "App::LinkElement"},
        {"owner", owner.name},
        {"ownerId", owner.id},
        {"index", entry.index},
        {"properties", linkElementLifecycleProperties(owner, link, linkTransform, entry.index)},
    });
}

void addStaleLinkElementDeleteUpdates(runtime::ComputeContext& context,
                                      const app::DocumentObject& owner,
                                      std::size_t elementCount)
{
    const std::string prefix = owner.name + "_i";
    for (const auto& [name, object] : context.documentObjects) {
        if (object == nullptr || object->typeId != "App::LinkElement" || name.rfind(prefix, 0U) != 0U) {
            continue;
        }
        const auto index = materializedLinkElementIndex(*object);
        if (!index || *index < elementCount) {
            continue;
        }
        const auto ownerValue = app::readNumber(*object, "_LinkOwner");
        if (!ownerValue || static_cast<long long>(*ownerValue) != owner.id) {
            continue;
        }
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::update(), when ElementCount shrinks, removes trailing owned
        // LinkElement children after cutting them from ElementList.
        context.documentObjectUpdates.push_back({
            {"action", "delete"},
            {"reason", "show_element_excess_child"},
            {"object", object->name},
            {"typeId", "App::LinkElement"},
            {"owner", owner.name},
            {"ownerId", owner.id},
            {"index", *index},
        });
    }
}

std::optional<std::size_t> materializedLinkElementIndex(const app::DocumentObject& element)
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

const app::DocumentObject* materializedLinkElementOwner(const app::DocumentObject& element,
                                                             const runtime::ComputeContext& context)
{
    const auto index = materializedLinkElementIndex(element);
    if (!index) {
        return nullptr;
    }

    const auto ownerValue = app::readNumber(element, "_LinkOwner");
    if (ownerValue) {
        for (const auto& [name, object] : context.documentObjects) {
            (void)name;
            if (object != nullptr && object->typeId == "App::Link"
                && object->id == static_cast<long long>(*ownerValue)
                && readElementCount(*object) > *index
                && app::readBool(*object, "ShowElement").value_or(true)) {
                return object;
            }
        }
    }

    const std::string ownerName = element.name.substr(0U, element.name.rfind("_i"));
    const auto ownerIt = context.documentObjects.find(ownerName);
    if (ownerIt == context.documentObjects.end() || ownerIt->second == nullptr) {
        return nullptr;
    }
    const app::DocumentObject* owner = ownerIt->second;
    if (owner->typeId != "App::Link" || readElementCount(*owner) <= *index
        || !app::readBool(*owner, "ShowElement").value_or(true)) {
        return nullptr;
    }
    return owner;
}

std::vector<LinkElementEntry> linkElementEntries(const app::DocumentObject& object,
                                                 const runtime::ComputeContext& context)
{
    std::vector<LinkElementEntry> entries;
    const auto elementCount = readElementCount(object);
    entries.reserve(elementCount);
    for (std::size_t index = 0; index < elementCount; ++index) {
        const std::string elementName = object.name + "_i" + std::to_string(index);
        const auto elementIt = context.documentObjects.find(elementName);
        const app::DocumentObject* element =
            elementIt == context.documentObjects.end() ? nullptr : elementIt->second;
        if (element == nullptr || !isOwnedMaterializedLinkElement(*element, object)) {
            entries.push_back(LinkElementEntry{index, elementName, nullptr});
            continue;
        }
        entries.push_back(LinkElementEntry{index, elementName, element});
    }
    return entries;
}

void publishLinkShapeBuild(const app::DocumentObject& object,
                           runtime::ComputeContext& context,
                           const app::Link& link,
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
    std::optional<part::NamedShape> linkedNamedShape;
    if (sourceShapeIt != context.shapes.end()) {
        const auto sourceNamedShapeIt = context.namedShapes.find(link.object);
        const part::NamedShapeSource source{
            link.object,
            sourceShapeIt->second.shape,
            sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second,
            linkedObjectAliasOwners(link, context),
        };
        if (!shape->sourceToTargetElements.empty()) {
            linkedNamedShape = part::namedShapeForLinkedSubshapes(
                object.name,
                shape->shape,
                source,
                shape->sourceToTargetElements
            );
        }
        else {
            linkedNamedShape = part::namedShapeForLinkedShape(object.name, shape->shape, source);
        }
    }
    publishLinkedShape(object, context, shape->shape, kindValue, metadata, linkedNamedShape);
}

bool executeInheritedMaterializedLinkElement(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::set<std::string>& allowedProperties,
    const std::string& kind)
{
    const app::DocumentObject* owner = materializedLinkElementOwner(object, context);
    if (owner == nullptr) {
        return false;
    }

    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return true;
    }
    if (const auto childLink = app::readLink(object, "LinkedObject")) {
        addCopyOnChangeLifecycleUpdates(context, object, *childLink);
    }

    const bool linkTransform = app::readBool(*owner, "LinkTransform").value_or(false);
    if (!elementListChildSyncsLinkedObject(object)) {
        const auto childLink = app::readLink(object, "LinkedObject");
        if (!childLink) {
            return false;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::syncElementList(), "LinkCopyOnChange.getValue() == 2"
        // returns before replacing child "LinkedObject", but owner "LinkTransform" has already
        // been synchronized. cad-core computes that mixed state request-locally.
        const auto shape = linkShape(object, context, *childLink, linkTransform);
        nlohmann::json metadata = {
            {"link", kind},
            {"linked_object", childLink->object},
            {"link_transform", linkTransform},
            {"link_owner", owner->name},
            {"owned_copy_linked_object", true},
        };
        publishLinkShapeBuild(object, context, *childLink, shape, metadata);
        return true;
    }

    const auto link = app::readLink(*owner, "LinkedObject");
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
    // ::LinkBaseExtension::updateGroup(), for non CopyOnChangeOwned children, copies parent
    // "element->LinkedObject.setValue(xlink->getValue(), xlink->getSubValues())" and syncs
    // "element->LinkTransform" with the owner link. cad-core computes that inherited state
    // request-locally without mutating the DocumentObject graph.
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
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& link,
    bool linkTransform,
    std::size_t index,
    const std::vector<gp_Trsf>& placements)
{
    auto selected = baseLinkedShape(object, context, link, linkTransform);
    if (!selected) {
        return std::nullopt;
    }

    // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::extensionGetSubObject(), when traversing owner ElementList,
    // delegates to child getSubObject(); the child "Scale will be included here" branch is
    // guarded by pyObj, so the owner's Shape collection uses child placement but not child
    // ScaleList geometry.
    TopoDS_Shape displayedShape = base::transformShape(
        selected->shape,
        index < placements.size() ? placements.at(index) : defaultElementPlacement(index)
    );
    selected->shape = displayedShape;
    return selected;
}

void executeMaterializedElementGroupLike(const app::DocumentObject& object,
                                         runtime::ComputeContext& context,
                                         const std::set<std::string>& allowedProperties,
                                         const std::string& kind)
{
    if (!runtime::rejectUnsupportedProperties(object, context, allowedProperties)) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto entries = linkElementEntries(object, context);

    const auto parsedPlacements = readPlacementList(object, context);
    if (!parsedPlacements) {
        return;
    }
    const std::vector<gp_Trsf> placements = *parsedPlacements;
    const auto link = linkedObjectProperty(object, context);
    if (!link) {
        return;
    }
    addCopyOnChangeLifecycleUpdates(context, object, *link);
    const bool linkTransform = app::readBool(object, "LinkTransform").value_or(false);

    nlohmann::json elements = nlohmann::json::array();
    nlohmann::json visibleElements = nlohmann::json::array();
    std::vector<TopoDS_Shape> shapes;
    std::vector<part::NamedShapeSource> sources;
    const auto visibility = readVisibilityList(object);
    const gp_Trsf groupPlacement = objectGlobalPlacement(object, context);
    const auto ownerLink = app::readLink(object, "LinkedObject");
    const auto sourceNamedShapeIt = context.namedShapes.find(link->object);
    const part::NamedShape* sourceNamedShape =
        sourceNamedShapeIt == context.namedShapes.end() ? nullptr : &sourceNamedShapeIt->second;
    bool hasMaterializedElement = false;
    bool hasSyntheticElement = false;
    addStaleLinkElementDeleteUpdates(context, object, readElementCount(object));

    for (const auto& entry : entries) {
        elements.push_back(entry.name);
        if (entry.object == nullptr) {
            addLinkElementCreateUpdate(context, object, *link, linkTransform, entry);
        }
        else {
            addLinkElementClaimUpdate(context, object, *link, linkTransform, entry);
        }
        if (!isVisibleElement(entry.index, visibility)) {
            continue;
        }

        TopoDS_Shape displayedShape;
        const part::NamedShape* elementNamedShape = nullptr;
        if (entry.object != nullptr) {
            const auto namedShapeIt = context.namedShapes.find(entry.name);
            elementNamedShape =
                namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
            hasMaterializedElement = true;
        }
        else {
            elementNamedShape = sourceNamedShape;
            hasSyntheticElement = true;
        }
        const auto synthetic = syntheticLinkElementShape(
            object,
            context,
            *link,
            linkTransform,
            entry.index,
            placements
        );
        if (!synthetic) {
            continue;
        }
        displayedShape = synthetic->shape;

        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::update() creates or re-claims owner "_iN" LinkElement
        // children when ShowElement is true. cad-core keeps the graph immutable, so missing
        // children are represented only in this recompute result under the same request-local names.
        displayedShape = base::transformShape(displayedShape, groupPlacement);
        shapes.push_back(displayedShape);
        visibleElements.push_back(entry.name);
        const std::optional<app::Link> elementLink =
            entry.object == nullptr ? std::nullopt : app::readLink(*entry.object, "LinkedObject");
        const app::Link* aliasLink = nullptr;
        if (elementLink) {
            aliasLink = &*elementLink;
        }
        else if (ownerLink) {
            aliasLink = &*ownerLink;
        }

        sources.push_back(part::NamedShapeSource{
            entry.name,
            displayedShape,
            elementNamedShape,
            aliasLink == nullptr
                ? std::vector<std::string> {}
                : linkedTargetSubpathAliasOwners(entry.name, *aliasLink, context),
        });
        // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        // ::LinkBaseExtension::getElementIndex(), when ShowElement creates child LinkElement
        // objects, still accepts digit-prefixed subpaths before searching child object names
        // or "$" + child labels.
        sources.push_back(part::NamedShapeSource{
            std::to_string(entry.index),
            displayedShape,
            elementNamedShape,
            aliasLink == nullptr
                ? std::vector<std::string> {}
                : linkedTargetSubpathAliasOwners(std::to_string(entry.index), *aliasLink, context),
        });
        if (entry.object != nullptr) {
            const std::string label = objectLabel(entry.name, context);
            if (label != entry.name) {
                sources.push_back(part::NamedShapeSource{
                    "$" + label,
                    displayedShape,
                    elementNamedShape,
                    aliasLink == nullptr
                        ? std::vector<std::string> {}
                        : linkedTargetSubpathAliasOwners("$" + label, *aliasLink, context),
                });
            }
        }
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
        part::namedShapeForPreservedSources(object.name, shape, sources)
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

void executeAppLinkBaseLike(const app::DocumentObject& object,
                            runtime::ComputeContext& context,
                            const std::set<std::string>& allowedProperties,
                            const std::string& kind)
{
    executeLinkLike(object, context, allowedProperties, kind);
}

void executeAppLink(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp::Link::Link(),
    // adds LINK_PARAMS_LINK including "LinkedObject", "LinkTransform", "LinkPlacement" and
    // "Placement"; Link::isLinkGroup() returns ElementCount > 0, and LinkBaseExtension routes
    // ElementList through child LinkElement objects when the link acts as an element group.
    const auto elementCount = readElementCount(object);
    const bool showElement = app::readBool(object, "ShowElement").value_or(true);
    if (linkedPlainGroupProperty(object, context)) {
        executeLinkedPlainGroupLike(object,
                                    context,
                                    {"LinkedObject",
                                     "LinkTransform",
                                     "LinkPlacement",
                                     "Placement",
                                     "LinkClaimChild",
                                     "Scale",
                                     "ScaleVector",
                                     "ShowElement",
                                     "ElementCount",
                                     "ElementList",
                                     "LinkExecute",
                                     "LinkCopyOnChange",
                                     "LinkCopyOnChangeSource",
                                     "LinkCopyOnChangeGroup",
                                     "LinkCopyOnChangeTouched"},
                                    "app_link_group");
        return;
    }

    if (!app::readLinks(object, "ElementList").empty() && (showElement || elementCount == 0U)) {
        executeElementGroupLike(object,
                                context,
                                {"LinkedObject",
                                 "LinkTransform",
                                 "LinkPlacement",
                                 "Placement",
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

    if (elementCount > 0U && showElement) {
        executeMaterializedElementGroupLike(object,
                                            context,
                                            {"LinkedObject",
                                             "LinkTransform",
                                             "LinkPlacement",
                                             "Placement",
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

    if (elementCount > 0U) {
        executeCollapsedElementCountLink(object,
                                         context,
                                         {"LinkedObject",
                                          "LinkTransform",
                                          "LinkPlacement",
                                          "Placement",
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
                      "Placement",
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

void executeAppLinkElement(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkElement::LinkElement(), LINK_PARAMS_ELEMENT includes "LinkedObject",
    // "LinkTransform", "LinkPlacement", "Placement" and scale properties; LinkElement
    // reuses LinkBaseExtension's linked-object resolution and transform semantics.
    const std::set<std::string> allowedProperties = {"LinkedObject",
                                                     "LinkTransform",
                                                     "LinkPlacement",
                                                     "Placement",
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

void executeAppLinkGroup(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkGroup::LinkGroup(), LINK_PARAMS_GROUP includes "ElementList", "Placement",
    // "VisibilityList" and "LinkMode"; the group displays its element objects with group
    // placement while keeping each child object's own link semantics.
    executeElementGroupLike(object,
                            context,
                            {"ElementList", "Placement", "VisibilityList", "LinkMode", "ColoredElements"},
                            "app_link_group");
}

void executeDocumentObjectGroup(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObjectGroup.cpp
    // ::DocumentObjectGroup::DocumentObjectGroup(), calls "GroupExtension::initExtension(this)".
    // The plain group has no Shape of its own; App::Link consumes its "Group" children through
    // LinkBaseExtension::linkedPlainGroup() / updateGroup().
    if (!runtime::rejectUnsupportedProperties(object, context, {"Group", "Label"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    context.objects[object.name] = {
        {"status", "ok"},
        {"container", "document_object_group"},
        {"group", linkNamesJson(app::readLinks(object, "Group"))},
    };
}

}  // namespace cad_core::app
