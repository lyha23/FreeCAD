#include "cad_core/part_design/body.h"

#include "cad_core/runtime/feature_executor.h"
#include "cad_core/base/placement.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/property_topo_shape.h"

#include <TopoDS_Shape.hxx>
#include <gp_TrsfForm.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <algorithm>
#include <map>
#include <optional>
#include <set>

namespace cad_core::part_design {

namespace {

std::optional<std::vector<std::string>> readGroupNames(const app::DocumentObject& object)
{
    const std::vector<app::Link> links = app::readLinks(object, "Group");
    if (links.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> names;
    for (const auto& link : links) {
        names.push_back(link.object);
    }
    return names;
}

struct BooleanBuild {
    TopoDS_Shape shape;
    part::NamedShape namedShape;
};

struct OriginFeatureInfo {
    std::string origin;
    std::string role;
};

const app::DocumentObject* documentObjectByName(const runtime::ComputeContext& context,
                                                     const std::string& name)
{
    const auto objectIt = context.documentObjects.find(name);
    if (objectIt == context.documentObjects.end()) {
        return nullptr;
    }
    return objectIt->second;
}

bool documentObjectExists(const runtime::ComputeContext& context, const std::string& name)
{
    return documentObjectByName(context, name) != nullptr;
}

bool isPartDesignFeatureBase(const app::DocumentObject& object)
{
    return object.typeId == "PartDesign::FeatureBase";
}

bool groupContains(const std::vector<std::string>& groupNames, const std::string& name)
{
    return std::find(groupNames.begin(), groupNames.end(), name) != groupNames.end();
}

bool isPartDesignSolidFeature(const std::string& name, const runtime::ComputeContext& context)
{
    const app::DocumentObject* object = documentObjectByName(context, name);
    if (object == nullptr || object->typeId.rfind("PartDesign::", 0U) != 0U || isPartDesignFeatureBase(*object)
        || object->typeId == "PartDesign::Body") {
        return false;
    }
    const auto shapeIt = context.shapes.find(name);
    if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
        return true;
    }
    return context.addSubShapes.count(name) != 0U;
}

nlohmann::json linkPropertyJson(const std::string& target)
{
    return {
        {"PropertyType", "App::PropertyLink"},
        {"value", target},
    };
}

nlohmann::json nullLinkPropertyJson()
{
    return {
        {"PropertyType", "App::PropertyLink"},
        {"value", nullptr},
    };
}

nlohmann::json groupPropertyJson(const app::DocumentObject& body,
                                 const std::vector<std::string>& groupNames)
{
    nlohmann::json values = nlohmann::json::array();
    for (const auto& groupName : groupNames) {
        values.push_back(groupName);
    }

    const auto* groupProperty = app::propertyValue(body, "Group");
    const std::string propertyType = groupProperty == nullptr ? "App::PropertyLinkList" : groupProperty->propertyType;
    if (propertyType == "App::PropertyLinkSubList" || propertyType == "App::PropertyLinkSubListHidden"
        || propertyType == "App::PropertyXLinkSubList") {
        nlohmann::json subSet = nlohmann::json::array();
        for (const auto& groupName : groupNames) {
            subSet.push_back({
                {"value", groupName},
                {"SubList", nlohmann::json::array()},
            });
        }
        return {
            {"PropertyType", propertyType},
            {"SubSet", std::move(subSet)},
        };
    }

    return {
        {"PropertyType", propertyType.empty() ? "App::PropertyLinkList" : propertyType},
        {"values", std::move(values)},
    };
}

std::string shapeKind(const TopoDS_Shape& shape)
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
        case TopAbs_SHAPE:
            break;
    }
    return "occt_shape";
}

std::string uniqueFeatureBaseName(const runtime::ComputeContext& context, const std::string& bodyName)
{
    const auto available = [&context](const std::string& name) {
        return !documentObjectExists(context, name);
    };
    if (available("BaseFeature")) {
        return "BaseFeature";
    }
    const std::string prefix = bodyName + "_BaseFeature";
    if (available(prefix)) {
        return prefix;
    }
    for (std::size_t index = 1U;; ++index) {
        const std::string candidate = prefix + std::to_string(index);
        if (available(candidate)) {
            return candidate;
        }
    }
}

std::optional<std::string> featureBaseForBodyBaseTarget(const runtime::ComputeContext& context,
                                                       const std::vector<std::string>& groupNames,
                                                       const std::string& baseTarget)
{
    for (const auto& groupName : groupNames) {
        const app::DocumentObject* groupObject = documentObjectByName(context, groupName);
        if (groupObject == nullptr || !isPartDesignFeatureBase(*groupObject)) {
            continue;
        }
        const auto link = app::readLink(*groupObject, "BaseFeature");
        if (link && link->object == baseTarget) {
            return groupName;
        }
    }
    return std::nullopt;
}

std::optional<std::string> firstBodySolidFeature(const runtime::ComputeContext& context,
                                                const std::vector<std::string>& groupNames)
{
    for (const auto& groupName : groupNames) {
        if (isPartDesignSolidFeature(groupName, context)) {
            return groupName;
        }
    }
    return std::nullopt;
}

bool bodyChainSolidFeature(const runtime::ComputeContext& context,
                           const std::vector<std::string>& groupNames,
                           const std::string& name)
{
    if (!groupContains(groupNames, name)) {
        return false;
    }
    const app::DocumentObject* object = documentObjectByName(context, name);
    if (object == nullptr || object->typeId.rfind("PartDesign::", 0U) != 0U
        || object->typeId == "PartDesign::Body") {
        return false;
    }
    const auto shapeIt = context.shapes.find(name);
    if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
        return true;
    }
    return context.addSubShapes.count(name) != 0U;
}

std::optional<std::string> baseFeatureTarget(const app::DocumentObject& object)
{
    const auto link = app::readLink(object, "BaseFeature");
    if (!link) {
        return std::nullopt;
    }
    return link->object;
}

std::optional<std::string> readBodyOriginName(const app::DocumentObject& body,
                                              runtime::ComputeContext& context)
{
    if (!body.properties.contains("Origin")) {
        return std::nullopt;
    }

    const auto originLink = app::readLink(body, "Origin");
    if (!originLink || originLink->object.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               "Body Origin must link to an App::Origin object",
                               body.name,
                               "Origin");
        return std::nullopt;
    }

    const app::DocumentObject* originObject = documentObjectByName(context, originLink->object);
    if (originObject == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Body Origin target " + originLink->object + " is missing",
                               body.name,
                               "Origin",
                               "runtime",
                               originLink->object);
        return std::nullopt;
    }
    if (originObject->typeId != "App::Origin") {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_link_value",
                               "Body Origin target " + originLink->object + " must be App::Origin",
                               body.name,
                               "Origin",
                               "runtime",
                               originLink->object);
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/OriginGroupExtension.cpp
    // ::OriginGroupExtension::getOrigin() returns the hidden "Origin" child link and throws
    // when it is missing or not an App::Origin; PartDesign::Body inherits BodyBase, which
    // initializes OriginGroupExtension for Body origin ownership.
    return originLink->object;
}

std::map<std::string, OriginFeatureInfo> originFeatureInfoByName(const runtime::ComputeContext& context)
{
    std::map<std::string, OriginFeatureInfo> result;
    for (const auto& item : context.documentObjects) {
        const app::DocumentObject* origin = item.second;
        if (origin == nullptr || origin->typeId != "App::Origin") {
            continue;
        }
        for (const auto& link : app::readLinks(*origin, "OriginFeatures")) {
            const app::DocumentObject* feature = documentObjectByName(context, link.object);
            const auto role = feature == nullptr ? std::nullopt : app::readString(*feature, "Role");
            if (!role || role->empty()) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp
            // ::LocalCoordinateSystem::getDatumElement() searches "OriginFeatures" for
            // App::DatumElement objects whose "Role" matches X_Axis/Y_Axis/Z_Axis or planes.
            result.emplace(link.object, OriginFeatureInfo{origin->name, *role});
        }
    }
    return result;
}

std::map<std::string, std::string> originFeatureByRole(const runtime::ComputeContext& context,
                                                       const std::string& originName)
{
    std::map<std::string, std::string> result;
    const app::DocumentObject* origin = documentObjectByName(context, originName);
    if (origin == nullptr) {
        return result;
    }
    for (const auto& link : app::readLinks(*origin, "OriginFeatures")) {
        const app::DocumentObject* feature = documentObjectByName(context, link.object);
        const auto role = feature == nullptr ? std::nullopt : app::readString(*feature, "Role");
        if (!role || role->empty()) {
            continue;
        }
        result.emplace(*role, link.object);
    }
    return result;
}

std::optional<std::string> replacementBodyOriginFeature(const std::string& target,
                                                        const std::string& bodyOriginName,
                                                        const std::map<std::string, OriginFeatureInfo>& originFeatures,
                                                        const std::map<std::string, std::string>& bodyOriginFeatureByRole)
{
    const auto targetInfoIt = originFeatures.find(target);
    if (targetInfoIt == originFeatures.end() || targetInfoIt->second.origin == bodyOriginName) {
        return std::nullopt;
    }
    const auto replacementIt = bodyOriginFeatureByRole.find(targetInfoIt->second.role);
    if (replacementIt == bodyOriginFeatureByRole.end() || replacementIt->second == target) {
        return std::nullopt;
    }
    return replacementIt->second;
}

bool relinkOriginFeatureTargets(nlohmann::json& property,
                                const std::string& bodyOriginName,
                                const std::map<std::string, OriginFeatureInfo>& originFeatures,
                                const std::map<std::string, std::string>& bodyOriginFeatureByRole)
{
    const auto relinkValue = [&](nlohmann::json& value) {
        if (!value.is_string()) {
            return false;
        }
        const auto replacement = replacementBodyOriginFeature(value.get<std::string>(),
                                                             bodyOriginName,
                                                             originFeatures,
                                                             bodyOriginFeatureByRole);
        if (!replacement) {
            return false;
        }
        value = *replacement;
        return true;
    };

    bool changed = false;
    if (property.is_object()) {
        auto valueIt = property.find("value");
        if (valueIt != property.end()) {
            changed = relinkValue(*valueIt) || changed;
        }

        auto valuesIt = property.find("values");
        if (valuesIt != property.end() && valuesIt->is_array()) {
            for (auto& value : *valuesIt) {
                changed = relinkValue(value) || changed;
            }
        }

        auto subSetIt = property.find("SubSet");
        if (subSetIt != property.end() && subSetIt->is_array()) {
            for (auto& item : *subSetIt) {
                if (!item.is_object()) {
                    continue;
                }
                auto itemValueIt = item.find("value");
                if (itemValueIt != item.end()) {
                    changed = relinkValue(*itemValueIt) || changed;
                }
            }
        }
    }
    return changed;
}

void appendBodyOriginDatumRelinkUpdates(runtime::ComputeContext& context,
                                        const app::DocumentObject& body,
                                        const std::vector<std::string>& groupNames,
                                        const std::string& bodyOriginName)
{
    const auto originFeatures = originFeatureInfoByName(context);
    const auto bodyOriginFeatures = originFeatureByRole(context, bodyOriginName);
    if (originFeatures.empty() || bodyOriginFeatures.empty()) {
        return;
    }

    for (const auto& groupName : groupNames) {
        const app::DocumentObject* feature = documentObjectByName(context, groupName);
        if (feature == nullptr || feature->name == bodyOriginName) {
            continue;
        }

        nlohmann::json properties = nlohmann::json::object();
        for (const auto& property : feature->propertyValues) {
            if (property.second.links.empty()) {
                continue;
            }
            nlohmann::json updated = property.second.raw;
            if (!relinkOriginFeatureTargets(updated, bodyOriginName, originFeatures, bodyOriginFeatures)) {
                continue;
            }
            properties[property.first] = std::move(updated);
        }
        if (properties.empty()) {
            continue;
        }

        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/OriginGroupExtension.cpp
        // ::OriginGroupExtension::relinkToOrigin(), when Body::insertObject() adds a feature,
        // replaces linked origin datum objects with "getOrigin()->getDatumElement(Role)".
        context.documentObjectUpdates.push_back({
            {"action", "update"},
            {"reason", "body_origin_datum_relink"},
            {"object", feature->name},
            {"objectId", feature->id},
            {"typeId", feature->typeId},
            {"owner", body.name},
            {"ownerId", body.id},
            {"properties", std::move(properties)},
        });
    }
}

std::optional<std::string> previousSolidFeatureForRemovedTip(const runtime::ComputeContext& context,
                                                             const std::vector<std::string>& groupNames,
                                                             const app::DocumentObject& removedFeature)
{
    const auto baseTarget = baseFeatureTarget(removedFeature);
    if (!baseTarget || !bodyChainSolidFeature(context, groupNames, *baseTarget)) {
        return std::nullopt;
    }
    return baseTarget;
}

std::optional<std::string> nextSolidFeatureForRemovedTip(const runtime::ComputeContext& context,
                                                         const std::vector<std::string>& groupNames,
                                                         const std::string& removedFeature)
{
    for (const auto& groupName : groupNames) {
        const app::DocumentObject* object = documentObjectByName(context, groupName);
        if (object == nullptr || !bodyChainSolidFeature(context, groupNames, groupName)) {
            continue;
        }
        const auto baseTarget = baseFeatureTarget(*object);
        if (baseTarget && *baseTarget == removedFeature) {
            return groupName;
        }
    }
    return std::nullopt;
}

std::optional<std::string> appendBodyRemovedTipRerouteUpdates(runtime::ComputeContext& context,
                                                             const app::DocumentObject& body,
                                                             const std::vector<std::string>& groupNames,
                                                             const std::string& staleTip)
{
    const app::DocumentObject* staleTipObject = documentObjectByName(context, staleTip);
    if (staleTipObject == nullptr || groupContains(groupNames, staleTip)) {
        return std::nullopt;
    }
    const auto staleTipShapeIt = context.shapes.find(staleTip);
    if (staleTipShapeIt == context.shapes.end()
        || staleTipShapeIt->second.kind != runtime::ShapeValue::Kind::Solid) {
        return std::nullopt;
    }

    const auto previousSolid = previousSolidFeatureForRemovedTip(context, groupNames, *staleTipObject);
    const auto nextSolid = nextSolidFeatureForRemovedTip(context, groupNames, staleTip);
    if (!previousSolid && !nextSolid) {
        return std::nullopt;
    }
    const std::string reroutedTip = previousSolid.value_or(*nextSolid);

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
    // ::Body::removeObject(), before erasing the feature from Group, says "Adjust Tip feature
    // if it is pointing to the deleted object" and sets Tip to prevSolidFeature or nextSolidFeature.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "body_tip_deleted_feature_reroute"},
        {"object", body.name},
        {"objectId", body.id},
        {"typeId", body.typeId},
        {"properties",
         {
             {"Tip", linkPropertyJson(reroutedTip)},
         }},
    });

    if (nextSolid) {
        const app::DocumentObject* nextObject = documentObjectByName(context, *nextSolid);
        if (nextObject != nullptr) {
            const auto nextBase = baseFeatureTarget(*nextObject);
            if (nextBase && *nextBase == staleTip) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
                // ::Body::removeObject(), if the next feature points to the deleted feature,
                // rewrites "nextPD->BaseFeature" to "prevSolidFeature".
                context.documentObjectUpdates.push_back({
                    {"action", "update"},
                    {"reason", "body_feature_basefeature_delete_reroute"},
                    {"object", nextObject->name},
                    {"objectId", nextObject->id},
                    {"typeId", nextObject->typeId},
                    {"owner", body.name},
                    {"ownerId", body.id},
                    {"properties",
                     {
                         {"BaseFeature", previousSolid ? linkPropertyJson(*previousSolid) : nullLinkPropertyJson()},
                     }},
                });
            }
        }
    }

    return reroutedTip;
}

void appendBodyBaseFeatureChainUpdates(runtime::ComputeContext& context,
                                       const app::DocumentObject& body,
                                       const std::vector<std::string>& groupNames,
                                       const std::string& baseTarget)
{
    const app::DocumentObject* baseObject = documentObjectByName(context, baseTarget);
    if (baseObject == nullptr || isPartDesignFeatureBase(*baseObject)) {
        return;
    }

    std::optional<std::string> featureBaseName = featureBaseForBodyBaseTarget(context, groupNames, baseTarget);
    if (!featureBaseName) {
        featureBaseName = uniqueFeatureBaseName(context, body.name);
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
        // ::Body::onChanged(), when "prop == &BaseFeature", creates "PartDesign::FeatureBase"
        // and then calls "bf->BaseFeature.setValue(BaseFeature.getValue())".
        context.documentObjectUpdates.push_back({
            {"action", "create"},
            {"reason", "body_basefeature_featurebase_create"},
            {"object", *featureBaseName},
            {"typeId", "PartDesign::FeatureBase"},
            {"owner", body.name},
            {"ownerId", body.id},
            {"index", 0},
            {"properties",
             {
                 {"BaseFeature", linkPropertyJson(baseTarget)},
             }},
        });
    }

    std::vector<std::string> syncedGroup = groupNames;
    if (std::find(syncedGroup.begin(), syncedGroup.end(), *featureBaseName) == syncedGroup.end()) {
        syncedGroup.insert(syncedGroup.begin(), *featureBaseName);
    }
    else if (syncedGroup.front() != *featureBaseName) {
        syncedGroup.erase(std::remove(syncedGroup.begin(), syncedGroup.end(), *featureBaseName), syncedGroup.end());
        syncedGroup.insert(syncedGroup.begin(), *featureBaseName);
    }
    if (syncedGroup != groupNames) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
        // ::Feature::onChanged(), when "prop == &BaseFeature", calls "body->insertObject(this, base)".
        context.documentObjectUpdates.push_back({
            {"action", "update"},
            {"reason", "body_basefeature_group_sync"},
            {"object", body.name},
            {"objectId", body.id},
            {"typeId", body.typeId},
            {"properties",
             {
                 {"Group", groupPropertyJson(body, syncedGroup)},
             }},
        });
    }

    const auto firstSolid = firstBodySolidFeature(context, groupNames);
    if (!firstSolid) {
        return;
    }
    const app::DocumentObject* solidObject = documentObjectByName(context, *firstSolid);
    if (solidObject == nullptr) {
        return;
    }
    const auto currentBase = app::readLink(*solidObject, "BaseFeature");
    if (currentBase && currentBase->object == *featureBaseName) {
        return;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
    // ::Body::setBaseProperty(), after inserting a solid feature, writes the next feature's
    // "BaseFeature" to preserve the Body chain; Feature::getBaseShape() then reads "BaseFeature".
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "body_feature_basefeature_sync"},
        {"object", solidObject->name},
        {"objectId", solidObject->id},
        {"typeId", solidObject->typeId},
        {"owner", body.name},
        {"ownerId", body.id},
        {"properties",
         {
             {"BaseFeature", linkPropertyJson(*featureBaseName)},
         }},
    });
}

part::NamedShape namedShapeForFeatureOrIndexed(const std::string& feature,
                                               const TopoDS_Shape& shape,
                                               const runtime::ComputeContext& context)
{
    const auto namedShapeIt = context.namedShapes.find(feature);
    if (namedShapeIt != context.namedShapes.end()) {
        return namedShapeIt->second;
    }
    return part::indexedNamedShapeForObject(feature, shape);
}

part::NamedShapeSource sourceForCurrentBody(const std::string& bodyName,
                                            const TopoDS_Shape& shape,
                                            const std::optional<part::NamedShape>& namedShape)
{
    return part::NamedShapeSource{namedShape ? namedShape->owner : bodyName, shape, namedShape ? &*namedShape : nullptr};
}

part::NamedShapeSource sourceForFeature(const std::string& feature,
                                        const TopoDS_Shape& shape,
                                        const runtime::ComputeContext& context,
                                        const std::optional<part::NamedShape>* slotNamedShape = nullptr)
{
    if (slotNamedShape != nullptr && *slotNamedShape) {
        part::NamedShapeSource source{(*slotNamedShape)->owner, shape, &**slotNamedShape};
        source.expandCompoundForBoolean =
            std::find((*slotNamedShape)->elementHistoryStatus.begin(),
                      (*slotNamedShape)->elementHistoryStatus.end(),
                      "boolean_compound_tool:expand_children")
            != (*slotNamedShape)->elementHistoryStatus.end();
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/
        // FeatureHole.cpp::Hole::execute(), "builder.MakeCompound(holeWithThread)" stores
        // protoHole/protoThread as the subtractive AddSubShape; Part boolean then handles the
        // compound tool through FCBRepAlgoAPI_BooleanOperation.cpp::RecursiveCutFusedTools().
        source.fuseCompoundForCut =
            std::find((*slotNamedShape)->elementHistoryStatus.begin(),
                      (*slotNamedShape)->elementHistoryStatus.end(),
                      "hole_model_thread:pipe_shell_tool_history")
            != (*slotNamedShape)->elementHistoryStatus.end();
        return source;
    }
    const auto namedShapeIt = context.namedShapes.find(feature);
    part::NamedShapeSource source{namedShapeIt != context.namedShapes.end() ? namedShapeIt->second.owner : feature,
                                  shape,
                                  namedShapeIt != context.namedShapes.end() ? &namedShapeIt->second : nullptr};
    if (namedShapeIt != context.namedShapes.end()) {
        source.expandCompoundForBoolean =
            std::find(namedShapeIt->second.elementHistoryStatus.begin(),
                      namedShapeIt->second.elementHistoryStatus.end(),
                      "boolean_compound_tool:expand_children")
            != namedShapeIt->second.elementHistoryStatus.end();
        source.fuseCompoundForCut =
            std::find(namedShapeIt->second.elementHistoryStatus.begin(),
                      namedShapeIt->second.elementHistoryStatus.end(),
                      "hole_model_thread:pipe_shell_tool_history")
            != namedShapeIt->second.elementHistoryStatus.end();
    }
    return source;
}

std::optional<BooleanBuild> fuseShapes(const TopoDS_Shape& base,
                                       const TopoDS_Shape& tool,
                                       const app::DocumentObject& object,
                                       runtime::ComputeContext& context,
                                       const std::string& feature,
                                       const std::optional<part::NamedShape>* toolNamedShape,
                                       const std::optional<part::NamedShape>& baseNamedShape)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
    // selects BRepAlgoAPI_Fuse and calls makeElementShape(*mk, inputs, ...), where MapperMaker
    // consumes "BRepBuilderAPI_MakeShape::Modified/Generated()" for every boolean input.
    const auto build = part::makeElementBooleanFromSources(object.name,
                                                           {sourceForCurrentBody(object.name, base, baseNamedShape),
                                                            sourceForFeature(feature, tool, context, toolNamedShape)},
                                                           part::BooleanOperation::Fuse);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Body could not fuse additive feature " + feature + ": " + build.error,
                               object.name);
        return std::nullopt;
    }
    return BooleanBuild{build.shape,
                        build.namedShape ? *build.namedShape : part::indexedNamedShapeForObject(object.name, build.shape)};
}

std::optional<BooleanBuild> cutShapes(const TopoDS_Shape& base,
                                      const TopoDS_Shape& tool,
                                      const app::DocumentObject& object,
                                      runtime::ComputeContext& context,
                                      const std::string& feature,
                                      const std::optional<part::NamedShape>* toolNamedShape,
                                      const std::optional<part::NamedShape>& baseNamedShape)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementBoolean(),
    // selects BRepAlgoAPI_Cut and calls makeElementShape(*mk, inputs, ...), where MapperMaker
    // consumes "BRepBuilderAPI_MakeShape::Modified/Generated()" for every boolean input.
    const auto build = part::makeElementBooleanFromSources(object.name,
                                                           {sourceForCurrentBody(object.name, base, baseNamedShape),
                                                            sourceForFeature(feature, tool, context, toolNamedShape)},
                                                           part::BooleanOperation::Cut);
    if (!build.error.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "execution_failed",
                               "Body could not cut subtractive feature " + feature + ": " + build.error,
                               object.name);
        return std::nullopt;
    }
    return BooleanBuild{build.shape,
                        build.namedShape ? *build.namedShape : part::indexedNamedShapeForObject(object.name, build.shape)};
}

bool isIdentityPlacement(const gp_Trsf& placement)
{
    return placement.Form() == gp_Identity;
}

bool applyFinalResultRefineForFeature(const app::DocumentObject& bodyObject,
                                      const std::string& feature,
                                      runtime::ComputeContext& context,
                                      std::optional<TopoDS_Shape>& bodyShape,
                                      std::optional<part::NamedShape>& bodyNamedShape,
                                      std::vector<std::string>& refinedFeatures)
{
    if (!bodyShape) {
        return true;
    }
    const auto documentIt = context.documentObjects.find(feature);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr) {
        return true;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp
    // ::FeatureAddSub::execute(), "result = refineShapeIfActive(result)" after the feature's
    // add/sub boolean has produced the final body result.
    const auto refined = runtime::applyRefinePropertyForOwner(*documentIt->second, bodyObject.name, context, *bodyShape, bodyNamedShape);
    if (!refined) {
        return false;
    }
    bodyShape = refined->shape;
    bodyNamedShape = refined->namedShape;
    if (refined->applied) {
        refinedFeatures.push_back(feature);
    }
    return true;
}

}  // namespace

void executeBody(const app::DocumentObject& object, runtime::ComputeContext& context)
{
    // FreeCAD semantic sources:
    // src/Mod/PartDesign/App/Body.cpp Body::execute()
    // src/Mod/PartDesign/App/FeatureAddSub.cpp FeatureAddSub::getAddSubShape()
    if (!runtime::rejectUnsupportedProperties(object, context, {"Group", "Tip", "BaseFeature", "Origin", "AllowCompound"})) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Group")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Group must be a list of object links", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    if (!object.properties.contains("Tip")) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    const auto tip = app::readLink(object, "Tip");
    if (!tip) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body Tip must link to the final feature", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    auto group = readGroupNames(object);
    if (!group) {
        runtime::addDiagnostic(context.diagnostics, "error", "missing_link_target", "Body Group item must be an object link", object.name, "Group");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    const std::vector<std::string>& groupNames = *group;

    const auto bodyOriginName = readBodyOriginName(object, context);
    if (object.properties.contains("Origin") && !bodyOriginName) {
        context.objects[object.name] = {{"status", "error"}};
        return;
    }
    if (bodyOriginName) {
        appendBodyOriginDatumRelinkUpdates(context, object, groupNames, *bodyOriginName);
    }

    std::string resolvedTip = tip->object;
    if (!groupContains(groupNames, resolvedTip)) {
        const auto reroutedTip = appendBodyRemovedTipRerouteUpdates(context, object, groupNames, resolvedTip);
        if (reroutedTip) {
            resolvedTip = *reroutedTip;
        }
    }

    if (!groupContains(groupNames, resolvedTip)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Body Tip is not present in Group",
                               object.name,
                               "Tip",
                               "runtime",
                               tip->object);
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    std::optional<TopoDS_Shape> bodyShape;
    std::optional<part::NamedShape> bodyNamedShape;
    std::vector<std::string> refinedFeatures;
    std::vector<std::string> replayedAdditiveFeatures;
    std::vector<std::string> replayedSubtractiveFeatures;
    std::vector<std::string> replayedReplacementFeatures;
    if (object.properties.contains("BaseFeature")) {
        const auto baseLink = app::readLink(object, "BaseFeature");
        if (!baseLink) {
            runtime::addDiagnostic(context.diagnostics, "error", "missing_property", "Body BaseFeature must link to a solid feature", object.name, "BaseFeature");
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        const auto baseIt = context.shapes.find(baseLink->object);
        if (baseIt == context.shapes.end() || baseIt->second.kind != runtime::ShapeValue::Kind::Solid) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "missing_link_target",
                                   "Body BaseFeature target " + baseLink->object + " did not produce a solid",
                                   object.name,
                                   "BaseFeature",
                                   "runtime",
                                   baseLink->object);
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        bodyShape = baseIt->second.shape;
        bodyNamedShape = namedShapeForFeatureOrIndexed(baseLink->object, *bodyShape, context);
        appendBodyBaseFeatureChainUpdates(context, object, groupNames, baseLink->object);
    }

    for (const auto& feature : groupNames) {
        const auto shapeIt = context.shapes.find(feature);
        const auto objectIt = context.objects.find(feature);
        const bool replacesBodyShape = shapeIt != context.shapes.end()
            && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid
            && objectIt != context.objects.end() && objectIt->second.value("body_mode", "") == "replace";
        if (replacesBodyShape) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
            // ::Body::execute(), reads only the Tip feature's "Shape". DressUp and Transformed
            // features publish full replacement solids even when they also expose AddSubShape
            // caches for later pattern features.
            bodyShape = shapeIt->second.shape;
            bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
            replayedReplacementFeatures.push_back(feature);
            if (feature == resolvedTip) {
                break;
            }
            continue;
        }

        const auto addSubIt = context.addSubShapes.find(feature);
        if (addSubIt == context.addSubShapes.end()) {
            if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp,
                // derives from FeatureAddSub but execute() writes a full dressed "Shape"; Body Tip
                // must be able to become that replacement solid instead of reusing the previous Pad/Pocket.
                bodyShape = shapeIt->second.shape;
                bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
                replayedReplacementFeatures.push_back(feature);
                if (feature == resolvedTip) {
                    break;
                }
            }
            continue;
        }

        const runtime::AddSubShape& addSubShape = addSubIt->second;
        if (addSubShape.addShape) {
            replayedAdditiveFeatures.push_back(feature);
            if (!bodyShape) {
                bodyShape = *addSubShape.addShape;
                bodyNamedShape = namedShapeForFeatureOrIndexed(feature, *bodyShape, context);
            }
            else {
                const auto build = fuseShapes(*bodyShape,
                                              *addSubShape.addShape,
                                              object,
                                              context,
                                              feature,
                                              &addSubShape.addNamedShape,
                                              bodyNamedShape);
                if (build) {
                    bodyShape = build->shape;
                    bodyNamedShape = build->namedShape;
                }
                else {
                    bodyShape = std::nullopt;
                }
            }
        }
        else if (addSubShape.subShape) {
            replayedSubtractiveFeatures.push_back(feature);
            if (!bodyShape) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "execution_failed",
                                       "Body cannot apply subtractive feature " + feature + " without a base solid",
                                       object.name,
                                       "Group");
                context.objects[object.name] = {{"status", "error"}};
                return;
            }
            const auto build = cutShapes(*bodyShape,
                                         *addSubShape.subShape,
                                         object,
                                         context,
                                         feature,
                                         &addSubShape.subNamedShape,
                                         bodyNamedShape);
            if (build) {
                bodyShape = build->shape;
                bodyNamedShape = build->namedShape;
            }
            else {
                bodyShape = std::nullopt;
            }
        }

        if (!bodyShape) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        if (!applyFinalResultRefineForFeature(object, feature, context, bodyShape, bodyNamedShape, refinedFeatures)) {
            context.objects[object.name] = {{"status", "error"}};
            return;
        }
        if (feature == resolvedTip) {
            break;
        }
    }

    if (!bodyShape) {
        runtime::addDiagnostic(context.diagnostics, "error", "execution_failed", "Body Tip did not produce a shape", object.name, "Tip");
        context.objects[object.name] = {{"status", "error"}};
        return;
    }

    TopoDS_Shape resultShape = *bodyShape;
    const auto placementIt = context.globalPlacements.find(object.name);
    const bool hasNonIdentityPlacement = placementIt != context.globalPlacements.end() && !isIdentityPlacement(placementIt->second);
    if (hasNonIdentityPlacement) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
        // ::GeoFeature::getGlobalPlacement(), "return ext->globalGroupPlacement() * placementProperty->getValue()".
        resultShape = base::transformShape(resultShape, placementIt->second);
    }

    if (bodyNamedShape && !hasNonIdentityPlacement) {
        bodyNamedShape->owner = object.name;
        bodyNamedShape->shape = resultShape;
        context.namedShapes[object.name] = *bodyNamedShape;
    }
    else {
        context.namedShapes[object.name] = part::indexedNamedShapeForObject(object.name, resultShape);
    }
    context.shapes[object.name] = runtime::ShapeValue{runtime::ShapeValue::Kind::Solid, resultShape};
    context.mesh[object.name] = cad_core::part::meshForShape(resultShape);
    context.subshapes[object.name] = part::subshapeMapForShape(resultShape);
    nlohmann::json result = {
        {"status", "ok"},
        {"tip", resolvedTip},
        {"group", groupNames},
        {"shape", shapeKind(resultShape)},
        {"allow_compound", app::readBool(object, "AllowCompound").value_or(true)},
        {"bbox", cad_core::part::bboxForShape(resultShape)},
        {"volume", cad_core::part::volumeForShape(resultShape)},
        {"kernel", cad_core::part::kernelVersion()},
    };
    if (bodyOriginName) {
        result["origin"] = *bodyOriginName;
    }
    if (!replayedAdditiveFeatures.empty()) {
        result["replayed_additive_features"] = replayedAdditiveFeatures;
    }
    if (!replayedSubtractiveFeatures.empty()) {
        result["replayed_subtractive_features"] = replayedSubtractiveFeatures;
    }
    if (!replayedReplacementFeatures.empty()) {
        result["replayed_replacement_features"] = replayedReplacementFeatures;
    }
    result["replay_stopped_at_tip"] = resolvedTip;
    if (!refinedFeatures.empty()) {
        result["refined_features"] = refinedFeatures;
    }
    context.objects[object.name] = result;
}

}  // namespace cad_core::part_design
