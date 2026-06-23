#include "cad_core/runtime/element_reference_update.h"

#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"

#include <algorithm>

namespace cad_core::runtime
{

namespace
{

std::string indexedSubnameForReference(const std::string& subname)
{
    constexpr const char* internalPrefix = "Internal";
    const std::string prefix(internalPrefix);
    if (subname.rfind(prefix, 0) == 0U) {
        return subname.substr(prefix.size());
    }
    return subname;
}

nlohmann::json shadowSubToJson(const app::ShadowSub& shadowSub)
{
    return {
        {"newName", shadowSub.newName},
        {"oldName", shadowSub.oldName},
    };
}

bool requestLocalInternalSubname(const std::string& subname)
{
    return part::parseInternalSubshapeName(subname).has_value();
}

std::optional<std::vector<std::string>> stableSubnamesForReferenceUpdate(
    const app::Link& link,
    const nlohmann::json& referenceShadows,
    std::size_t subnameCount)
{
    if (link.stableSubnamesExplicit) {
        return link.stableSubnames;
    }
    if (!referenceShadows.is_array() || referenceShadows.size() != subnameCount) {
        return std::nullopt;
    }

    std::vector<std::string> stableSubnames;
    stableSubnames.reserve(subnameCount);
    for (const auto& shadow : referenceShadows) {
        const auto stableIt = shadow.find("stableSubname");
        if (stableIt == shadow.end() || !stableIt->is_string()) {
            return std::nullopt;
        }
        const std::string stableSubname = stableIt->get<std::string>();
        if (stableSubname.empty() || requestLocalInternalSubname(stableSubname)) {
            return std::nullopt;
        }
        stableSubnames.push_back(stableSubname);
    }
    return stableSubnames;
}

std::optional<std::vector<std::string>> fullSubnamesForReferenceUpdate(const app::Link& link,
                                                                       std::size_t subnameCount)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    // ::LinkBaseExtension::checkGeoElementMap() uses the full external subname as the retag
    // postfix evidence. Preserve explicitly supplied FullSubList when returning stateless
    // elementReferenceUpdates; generated stable names alone are not enough to reconstruct it.
    if (!link.fullSubnamesExplicit || link.fullSubnames.size() != subnameCount) {
        return std::nullopt;
    }
    return link.fullSubnames;
}

nlohmann::json shadowSubsForReferenceUpdate(const app::Link& link,
                                            const std::vector<std::string>& subnames,
                                            const std::optional<std::vector<std::string>>& stableSubnames)
{
    if (stableSubnames && stableSubnames->size() == subnames.size()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
        // ::PropertyLinkBase::_updateElementReference() uses ShadowSub ElementNamePair
        // "newName"/"oldName" as the stable element name and current visible subname pair.
        nlohmann::json items = nlohmann::json::array();
        for (std::size_t index = 0; index < subnames.size(); ++index) {
            items.push_back({
                {"newName", stableSubnames->at(index)},
                {"oldName", subnames.at(index)},
            });
        }
        return items;
    }
    if (!link.shadowSubs.empty()) {
        return shadowSubsToJson(link.shadowSubs);
    }
    return nlohmann::json::array();
}

nlohmann::json brepSnapshotToJson(const app::BrepSnapshot& brep)
{
    return {
        {"format", brep.format},
        {"byteLength", brep.byteLength},
        {"sha256", brep.sha256},
        {"data", brep.data},
    };
}

std::optional<app::BrepSnapshot> brepTextSnapshotForCurrentSubshape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return std::nullopt;
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp
    // ::Feature::onBeforeChange() stores the old referenced subshape in ElementCache before
    // a shape changes. cad-core's stateless ReferenceShadow update must therefore refresh the
    // single-subshape BREP evidence after a successful resolve, not return the stale snapshot.
    const auto snapshot = cad_core::part::brepTextSnapshotForShape(shape);
    if (!snapshot) {
        return std::nullopt;
    }
    return app::BrepSnapshot {
        snapshot->format,
        snapshot->byteLength,
        snapshot->sha256,
        snapshot->data,
    };
}

nlohmann::json linkSubListItemUpdateJson(const app::Link& link,
                                         const std::vector<std::string>& subnames,
                                         const nlohmann::json& referenceShadows)
{
    nlohmann::json item = {
        {"value", link.object},
        {"SubList", subnames},
    };
    if (!link.resolvedObjectFrom.empty() && link.resolvedObjectFrom != link.object) {
        item["sourceObjectRename"] = {
            {"oldName", link.resolvedObjectFrom},
            {"newName", link.object},
            {"method", "ReferenceShadow.targetId"},
        };
    }
    const auto stableSubnames = stableSubnamesForReferenceUpdate(link, referenceShadows, subnames.size());
    if (stableSubnames) {
        item["StableSubList"] = *stableSubnames;
    }
    if (const auto fullSubnames = fullSubnamesForReferenceUpdate(link, subnames.size())) {
        item["FullSubList"] = *fullSubnames;
    }
    if (!link.externalGeometryFlags.empty()) {
        item["ExternalFlags"] = externalGeometryFlagsToJson(link.externalGeometryFlags);
    }
    if (!link.labelReferenceRenames.empty()) {
        item["labelReferenceRename"] = labelReferenceRenamesToJson(link.labelReferenceRenames);
    }
    nlohmann::json shadowSubs = shadowSubsForReferenceUpdate(link, subnames, stableSubnames);
    if (!shadowSubs.empty()) {
        item["ShadowSub"] = std::move(shadowSubs);
    }
    if (!referenceShadows.empty()) {
        item["ReferenceShadow"] = referenceShadows;
    }
    return item;
}

void setIfNotEmpty(nlohmann::json& value, const std::string& field, const std::string& item)
{
    if (!item.empty()) {
        value[field] = item;
    }
}

nlohmann::json documentReferenceToJson(const app::LinkDocumentRef& ref)
{
    nlohmann::json value = {
        {"method", "PropertyXLinkContainer.DocMap"},
    };
    setIfNotEmpty(value, "file", ref.file);
    setIfNotEmpty(value, "oldName", ref.name);
    setIfNotEmpty(value, "newName", ref.currentName);
    setIfNotEmpty(value, "oldLabel", ref.label);
    setIfNotEmpty(value, "newLabel", ref.currentLabel);
    setIfNotEmpty(value, "oldStamp", ref.stamp);
    setIfNotEmpty(value, "currentStamp", ref.currentStamp);
    setIfNotEmpty(value, "status", ref.status);
    setIfNotEmpty(value, "currentStatus", ref.currentStatus);
    if (ref.allowPartialExplicit) {
        value["allowPartial"] = ref.allowPartial;
    }
    return value;
}

nlohmann::json referenceMetadataLinkUpdateJson(const app::Link& link,
                                               const ReferenceLifecycleDecision& lifecycle)
{
    nlohmann::json item = {
        {"value", link.object},
        {"SubList", link.subnames},
    };
    if (lifecycle.hasLabelReferenceRename) {
        item["labelReferenceRename"] = labelReferenceRenamesToJson(link.labelReferenceRenames);
    }
    if (link.documentRef && lifecycle.hasDocumentReferenceRename) {
        item["documentReference"] = documentReferenceToJson(*link.documentRef);
    }
    if (link.stableSubnamesExplicit) {
        item["StableSubList"] = link.stableSubnames;
    }
    if (link.fullSubnamesExplicit) {
        item["FullSubList"] = link.fullSubnames;
    }
    if (!link.externalGeometryFlags.empty()) {
        item["ExternalFlags"] = externalGeometryFlagsToJson(link.externalGeometryFlags);
    }
    if (!link.shadowSubs.empty()) {
        item["ShadowSub"] = shadowSubsToJson(link.shadowSubs);
    }
    return item;
}

}  // namespace

nlohmann::json shadowSubsToJson(const std::vector<app::ShadowSub>& shadowSubs)
{
    nlohmann::json items = nlohmann::json::array();
    for (const auto& shadowSub : shadowSubs) {
        items.push_back(shadowSubToJson(shadowSub));
    }
    return items;
}

nlohmann::json externalGeometryFlagsToJson(const std::set<std::string>& flags)
{
    nlohmann::json items = nlohmann::json::array();
    for (const char* flag : {"Defining", "Frozen", "Detached", "Missing", "Sync"}) {
        if (flags.count(flag) != 0U) {
            items.push_back(flag);
        }
    }
    return items;
}

nlohmann::json labelReferenceRenamesToJson(const std::vector<app::LabelReferenceRename>& renames)
{
    nlohmann::json items = nlohmann::json::array();
    for (const auto& rename : renames) {
        items.push_back({
            {"index", rename.index},
            {"oldLabel", rename.oldLabel},
            {"newLabel", rename.newLabel},
            {"oldSubname", rename.oldSubname},
            {"newSubname", rename.newSubname},
            {"method", "PropertyLinkBase.updateLabelReference"},
        });
    }
    return items;
}

nlohmann::json referenceShadowUpdateJson(const app::ReferenceShadow& shadow,
                                         const app::Link& link,
                                         const std::string& subname,
                                         const TopoDS_Shape& currentSubshape,
                                         const std::string& recoveryMethod,
                                         const std::string& recoveryReason)
{
    nlohmann::json update = {
        {"target", link.object},
        {"targetId", shadow.targetId},
        {"property", shadow.property},
        {"shapeType", shadow.shapeType},
        {"indexed", indexedSubnameForReference(subname)},
        {"subname", subname},
        {"stableSubname", shadow.stableSubname},
        {"fingerprint", part::referenceFingerprintForShape(currentSubshape)},
    };
    if (shadow.brep) {
        if (const auto currentBrep = brepTextSnapshotForCurrentSubshape(currentSubshape)) {
            update["brep"] = brepSnapshotToJson(*currentBrep);
        }
        else {
            update["brep"] = brepSnapshotToJson(*shadow.brep);
        }
    }
    if (!recoveryMethod.empty()) {
        update["reference_recovery"] = recoveryMethod;
        update["reference_recovery_reason"] = recoveryReason;
    }
    if (!link.resolvedObjectFrom.empty() && link.resolvedObjectFrom != link.object) {
        update["sourceObjectRename"] = {
            {"oldName", link.resolvedObjectFrom},
            {"newName", link.object},
            {"method", "ReferenceShadow.targetId"},
        };
    }
    return update;
}

void appendElementReferenceUpdate(const app::DocumentObject& object,
                                  const std::string& propertyName,
                                  const app::PropertyValue& propertyValue,
                                  const app::Link& link,
                                  const std::vector<std::string>& subnames,
                                  const nlohmann::json& referenceShadows,
                                  nlohmann::json& updates)
{
    if (propertyValue.kind != app::PropertyKind::LinkSub || referenceShadows.empty()) {
        return;
    }

    nlohmann::json update = {
        {"object", object.name},
        {"property", propertyName},
        {"PropertyType", propertyValue.propertyType},
        {"value", link.object},
        {"SubList", subnames},
        {"ReferenceShadow", referenceShadows},
    };
    const auto stableSubnames = stableSubnamesForReferenceUpdate(link, referenceShadows, subnames.size());
    if (stableSubnames) {
        update["StableSubList"] = *stableSubnames;
    }
    if (const auto fullSubnames = fullSubnamesForReferenceUpdate(link, subnames.size())) {
        update["FullSubList"] = *fullSubnames;
    }
    if (!link.externalGeometryFlags.empty()) {
        update["ExternalFlags"] = externalGeometryFlagsToJson(link.externalGeometryFlags);
    }
    if (!link.labelReferenceRenames.empty()) {
        update["labelReferenceRename"] = labelReferenceRenamesToJson(link.labelReferenceRenames);
    }
    if (!link.resolvedObjectFrom.empty() && link.resolvedObjectFrom != link.object) {
        update["sourceObjectRename"] = {
            {"oldName", link.resolvedObjectFrom},
            {"newName", link.object},
            {"method", "ReferenceShadow.targetId"},
        };
    }
    nlohmann::json shadowSubs = shadowSubsForReferenceUpdate(link, subnames, stableSubnames);
    if (!shadowSubs.empty()) {
        update["ShadowSub"] = std::move(shadowSubs);
    }
    updates.push_back(std::move(update));
}

void appendElementReferenceSubListUpdate(const app::DocumentObject& object,
                                         const std::string& propertyName,
                                         const app::PropertyValue& propertyValue,
                                         const std::map<std::size_t, nlohmann::json>& referenceShadowUpdates,
                                         const std::map<std::size_t, std::vector<std::string>>& subnameUpdates,
                                         nlohmann::json& updates)
{
    if (propertyValue.kind != app::PropertyKind::LinkSubList || referenceShadowUpdates.empty()) {
        return;
    }

    nlohmann::json subSet = nlohmann::json::array();
    for (std::size_t index = 0; index < propertyValue.links.size(); ++index) {
        const auto updated = referenceShadowUpdates.find(index);
        const auto& link = propertyValue.links.at(index);
        std::vector<std::string> subnames = link.subnames;
        const auto updatedSubnames = subnameUpdates.find(index);
        if (updatedSubnames != subnameUpdates.end()) {
            subnames = updatedSubnames->second;
        }
        subSet.push_back(linkSubListItemUpdateJson(link,
                                                   subnames,
                                                   updated == referenceShadowUpdates.end()
                                                       ? nlohmann::json::array()
                                                       : updated->second));
    }
    updates.push_back({
        {"object", object.name},
        {"property", propertyName},
        {"PropertyType", propertyValue.propertyType},
        {"SubSet", std::move(subSet)},
    });
}

void appendReferenceMetadataUpdates(const app::DocumentObject& object,
                                    const ReferenceLifecycleView& lifecycleView,
                                    nlohmann::json& updates)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        if (propertyValue.kind == app::PropertyKind::LinkSub) {
            for (const auto& link : propertyValue.links) {
                const auto lifecycle =
                    classifyReferenceLifecycle(object, propertyValue, link, lifecycleView);
                if (!lifecycle.shouldPublishElementReferenceUpdate) {
                    continue;
                }
                nlohmann::json update = referenceMetadataLinkUpdateJson(link, lifecycle);
                update["object"] = object.name;
                update["property"] = propertyName;
                update["PropertyType"] = propertyValue.propertyType;
                updates.push_back(std::move(update));
            }
            continue;
        }
        if (propertyValue.kind != app::PropertyKind::LinkSubList) {
            continue;
        }

        bool changed = false;
        nlohmann::json subSet = nlohmann::json::array();
        for (const auto& link : propertyValue.links) {
            const auto lifecycle =
                classifyReferenceLifecycle(object, propertyValue, link, lifecycleView);
            changed = changed || lifecycle.shouldPublishElementReferenceUpdate;
            subSet.push_back(referenceMetadataLinkUpdateJson(link, lifecycle));
        }
        if (changed) {
            updates.push_back({
                {"object", object.name},
                {"property", propertyName},
                {"PropertyType", propertyValue.propertyType},
                {"SubSet", std::move(subSet)},
            });
        }
    }
}

void appendDocumentReferenceDiagnostics(const app::DocumentObject& object,
                                        const ReferenceLifecycleView& lifecycleView,
                                        std::vector<Diagnostic>& diagnostics)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        for (const auto& link : propertyValue.links) {
            const auto lifecycle =
                classifyReferenceLifecycle(object, propertyValue, link, lifecycleView);
            if (lifecycle.runtimeWarning) {
                diagnostics.push_back(*lifecycle.runtimeWarning);
            }
        }
    }
}

}  // namespace cad_core::runtime
