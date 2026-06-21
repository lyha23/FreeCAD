#include "cad_core/runtime/recompute.h"

#include "cad_core/graph/recompute_plan.h"
#include "cad_core/part/brep_snapshot.h"
#include "cad_core/base/placement.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"
#include "cad_core/part/property_topo_shape.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace cad_core::runtime {

namespace {

gp_Trsf objectPlacement(const app::DocumentObject& object)
{
    if (object.typeId == "App::Origin") {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Origin.cpp::Origin::Origin(),
        // "App::Origin is a LCS for which placement is fixed to identity"; parent group
        // placement is still applied by resolveGlobalPlacement().
        return gp_Trsf();
    }
    if (const auto placement = app::readPlacement(object, "Placement")) {
        return base::placementFromComponents(placement->base, placement->rotation);
    }
    return gp_Trsf();
}

gp_Trsf resolveGlobalPlacement(const app::Document& document,
                               const std::string& objectName,
                               std::map<std::string, gp_Trsf>& placements,
                               std::set<std::string>& visiting)
{
    const auto cached = placements.find(objectName);
    if (cached != placements.end()) {
        return cached->second;
    }

    const auto objectIt = document.indexByName.find(objectName);
    if (objectIt == document.indexByName.end() || visiting.count(objectName) != 0U) {
        return gp_Trsf();
    }

    visiting.insert(objectName);
    const auto& object = document.objects.at(objectIt->second);
    gp_Trsf placement = objectPlacement(object);
    const auto parentIt = document.parentGroupByObject.find(objectName);
    if (parentIt != document.parentGroupByObject.end()) {
        placement = resolveGlobalPlacement(document, parentIt->second, placements, visiting) * placement;
    }
    visiting.erase(objectName);

    placements[objectName] = placement;
    return placement;
}

std::map<std::string, gp_Trsf> buildGlobalPlacements(const app::Document& document)
{
    std::map<std::string, gp_Trsf> placements;
    std::set<std::string> visiting;
    for (const auto& object : document.objects) {
        resolveGlobalPlacement(document, object.name, placements, visiting);
    }
    return placements;
}

std::map<std::string, const app::DocumentObject*> buildDocumentObjectMap(const app::Document& document)
{
    std::map<std::string, const app::DocumentObject*> objects;
    for (const auto& object : document.objects) {
        objects[object.name] = &object;
    }
    return objects;
}

std::set<std::string> findTransformationTemplateObjects(const app::Document& document)
{
    std::set<std::string> templates;
    for (const auto& object : document.objects) {
        if (object.typeId != "PartDesign::MultiTransform") {
            continue;
        }
        for (const auto& link : app::readLinks(object, "Transformations")) {
            templates.insert(link.object);
        }
    }
    return templates;
}

bool isFrozenExternalGeometryReference(const std::string& propertyName, const app::Link& link)
{
    return propertyName == "ExternalGeometry" && link.externalGeometryFlags.count("Frozen") != 0U
        && link.externalGeometryFlags.count("Sync") == 0U;
}

bool isMissingOldExternalGeometrySnapshotReference(const std::string& propertyName,
                                                   const app::Link& link,
                                                   const ComputeContext& context)
{
    if (propertyName != "ExternalGeometry" || link.externalGeometryFlags.count("Missing") == 0U
        || link.externalGeometryFlags.count("Sync") != 0U) {
        return false;
    }
    if (context.documentObjects.count(link.object) != 0U) {
        return false;
    }
    return std::any_of(link.referenceShadows.begin(), link.referenceShadows.end(), [](const auto& shadow) {
        return shadow.brep.has_value();
    });
}

struct ReferenceSubshapeResolution {
    std::string subname;
    TopoDS_Shape shape;
    bool recovered = false;
    std::string recoveryMethod;
    std::string recoveryReason;
};

struct ReferenceSubshapeRecovery {
    part::ReferenceMatchStatus status = part::ReferenceMatchStatus::Missing;
    std::optional<ReferenceSubshapeResolution> resolution;
    std::string reason;
    std::string diagnosticCode;
};

std::string internalSubnameFromStableElementMap(const ComputeContext& context,
                                                const std::string& objectName,
                                                const std::string& stableSubname)
{
    if (stableSubname.empty() || stableSubname.rfind("Internal", 0) == 0) {
        return {};
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return {};
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return {};
    }
    const auto mappedIt = mapIt->find(stableSubname);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return {};
    }
    const std::string currentInternal = mappedIt->get<std::string>();
    if (currentInternal.rfind("InternalEdge", 0) != 0 && currentInternal.rfind("InternalVertex", 0) != 0) {
        return {};
    }
    return currentInternal;
}

std::optional<ReferenceSubshapeResolution> internalSubshapeForCurrentName(const ShapeValue& shapeValue,
                                                                          const std::string& subname)
{
    const auto internal = part::parseInternalSubshapeName(subname);
    if (!internal || !shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return std::nullopt;
    }
    const auto subshape = part::subshapeByName(*shapeValue.internalShape, *internal);
    if (!subshape) {
        return std::nullopt;
    }
    return ReferenceSubshapeResolution {subname, *subshape, false};
}

std::optional<ReferenceSubshapeResolution> currentSubshapeForReference(const app::Link& link,
                                                                       std::size_t index,
                                                                       const ComputeContext& context)
{
    if (index >= link.subnames.size() || link.subnames.at(index).empty()) {
        return std::nullopt;
    }

    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }

    const std::string& subname = link.subnames.at(index);
    const std::string stableSubname = index < link.stableSubnames.size() ? link.stableSubnames.at(index) : std::string{};
    if (const auto internal = part::parseInternalSubshapeName(subname)) {
        const auto current = internalSubshapeForCurrentName(shapeIt->second, subname);
        if (current) {
            return current;
        }
        if (const auto stableInternal = internalSubnameFromStableElementMap(context, link.object, stableSubname);
            !stableInternal.empty()) {
            auto recovered = internalSubshapeForCurrentName(shapeIt->second, stableInternal);
            if (recovered) {
                recovered->recovered = true;
                return recovered;
            }
        }
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(link.object);
    if (namedShapeIt != context.namedShapes.end()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
        // ::PropertyLinkBase::_updateElementReference(), calls GeoFeature::resolveElement()
        // before updating "shadow" and the persisted subname. cad-core mirrors that by
        // resolving StableSubList through the current NamedShape ElementMap before validating
        // ReferenceShadow evidence.
        const auto resolved = part::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            if (const auto subshape = part::subshapeByName(namedShapeIt->second, *resolved.element)) {
                return ReferenceSubshapeResolution {
                    *resolved.element,
                    *subshape,
                    *resolved.element != subname,
                };
            }
        }
    }
    const auto subshape = part::subshapeByName(shapeIt->second.shape, subname);
    if (!subshape) {
        return std::nullopt;
    }
    return ReferenceSubshapeResolution {subname, *subshape, false};
}

ReferenceSubshapeRecovery recoverSubshapeForReference(const app::Link& link,
                                                      std::size_t index,
                                                      const app::ReferenceShadow& shadow,
                                                      const ComputeContext& context)
{
    if (index >= link.subnames.size() || link.subnames.at(index).empty()) {
        return {};
    }
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        return {};
    }

    const std::string& subname = link.subnames.at(index);
    const bool internalReference = part::parseInternalSubshapeName(subname).has_value()
        || shadow.property == "InternalShape";
    const TopoDS_Shape* searchShape = nullptr;
    std::string prefix;
    if (internalReference) {
        if (!shapeIt->second.internalShape || shapeIt->second.internalShape->IsNull()) {
            return {};
        }
        searchShape = &*shapeIt->second.internalShape;
        prefix = "Internal";
    }
    else {
        searchShape = &shapeIt->second.shape;
    }

    const auto recovery = part::recoverReferenceShadowSubshape(*searchShape, prefix, shadow);
    if (recovery.status != part::ReferenceMatchStatus::Unique || !recovery.shape) {
        return ReferenceSubshapeRecovery {
            recovery.status,
            std::nullopt,
            recovery.reason,
            recovery.diagnosticCode,
        };
    }
    return ReferenceSubshapeRecovery {
        recovery.status,
        ReferenceSubshapeResolution {
            recovery.subname,
            *recovery.shape,
            true,
            "reference_shadow_single_subshape",
            "element_map_missing_or_split",
        },
        {},
        {},
    };
}

std::string referenceRecoveryDiagnosticCode(const ReferenceSubshapeRecovery& recovery)
{
    if (!recovery.diagnosticCode.empty()) {
        return recovery.diagnosticCode;
    }
    const part::ReferenceMatchStatus status = recovery.status;
    if (status == part::ReferenceMatchStatus::Ambiguous) {
        return "subname_resolve_ambiguous";
    }
    if (status == part::ReferenceMatchStatus::Split) {
        return "subname_split_requires_reselect";
    }
    if (status == part::ReferenceMatchStatus::Deleted) {
        return "subname_deleted";
    }
    return "subname_resolve_failed";
}

std::string referenceRecoveryDiagnosticReason(const ReferenceSubshapeRecovery& recovery)
{
    if (!recovery.reason.empty()) {
        return recovery.reason;
    }
    if (recovery.status == part::ReferenceMatchStatus::Ambiguous) {
        return "matches multiple ReferenceShadow candidates";
    }
    return "does not match a current ReferenceShadow candidate";
}

std::string referenceMatchStatusName(part::ReferenceMatchStatus status)
{
    switch (status) {
        case part::ReferenceMatchStatus::Unique:
            return "unique";
        case part::ReferenceMatchStatus::Missing:
            return "missing";
        case part::ReferenceMatchStatus::Ambiguous:
            return "ambiguous";
        case part::ReferenceMatchStatus::Split:
            return "split";
        case part::ReferenceMatchStatus::Deleted:
            return "deleted";
    }
    return "unknown";
}

std::string referenceSubnameShapeKind(const std::string& subname)
{
    if (const auto internal = part::parseInternalSubshapeName(subname)) {
        return part::subshapeKindName(internal->kind);
    }
    if (const auto parsed = part::parseSubshapeName(subname)) {
        return part::subshapeKindName(parsed->kind);
    }
    return "shape";
}

part::MapperHistoryRecoverability referenceRecoverability(part::ReferenceMatchStatus status)
{
    switch (status) {
        case part::ReferenceMatchStatus::Ambiguous:
            return part::MapperHistoryRecoverability::Ambiguous;
        case part::ReferenceMatchStatus::Split:
            return part::MapperHistoryRecoverability::NeedsReselect;
        case part::ReferenceMatchStatus::Deleted:
            return part::MapperHistoryRecoverability::Deleted;
        default:
            return part::MapperHistoryRecoverability::Diagnostic;
    }
}

part::MapperHistoryRelation referenceRelation(part::ReferenceMatchStatus status)
{
    switch (status) {
        case part::ReferenceMatchStatus::Deleted:
            return part::MapperHistoryRelation::Deleted;
        case part::ReferenceMatchStatus::Ambiguous:
        case part::ReferenceMatchStatus::Split:
            return part::MapperHistoryRelation::Split;
        default:
            return part::MapperHistoryRelation::Modified;
    }
}

void recordReferenceRecoveryMapperDiagnostic(ComputeContext& context,
                                             const app::Link& link,
                                             const app::ReferenceShadow& shadow,
                                             const std::string& requestedSubname,
                                             const ReferenceSubshapeRecovery& recovery)
{
    const bool internalReference = part::parseInternalSubshapeName(requestedSubname).has_value()
        || shadow.property == "InternalShape";
    const std::string namedShapeKey = internalReference ? link.object + ".InternalShape" : link.object;
    auto namedShapeIt = context.namedShapes.find(namedShapeKey);
    if (namedShapeIt == context.namedShapes.end()) {
        return;
    }

    const std::string diagnosticCode = referenceRecoveryDiagnosticCode(recovery);
    part::MapperHistoryEvent event;
    event.source = part::MapperHistoryEndpoint {link.object, requestedSubname};
    event.target = part::MapperHistoryEndpoint {namedShapeIt->second.owner, {}};
    event.shapeKind = referenceSubnameShapeKind(requestedSubname);
    event.relation = referenceRelation(recovery.status);
    event.makerStage = "reference_shadow_recovery";
    event.evidence = {
        {"reference_shadow", true},
        {"requested_subname", requestedSubname},
        {"status", referenceMatchStatusName(recovery.status)},
        {"reason", referenceRecoveryDiagnosticReason(recovery)},
    };
    event.recoverability = referenceRecoverability(recovery.status);
    event.diagnosticStatus = diagnosticCode;
    part::addMapperHistoryEvent(namedShapeIt->second.mapperHistory, std::move(event));
}

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

bool documentReferenceRenameChanged(const app::LinkDocumentRef& ref)
{
    return (!ref.name.empty() && !ref.currentName.empty() && ref.name != ref.currentName)
        || (!ref.label.empty() && !ref.currentLabel.empty() && ref.label != ref.currentLabel);
}

bool documentReferenceStampChanged(const app::LinkDocumentRef& ref)
{
    return !ref.stamp.empty() && !ref.currentStamp.empty() && ref.stamp != ref.currentStamp;
}

bool requestLocalInternalSubname(const std::string& subname)
{
    return part::parseInternalSubshapeName(subname).has_value();
}

std::vector<std::string> stableNameCandidatesForReference(const app::Link& link,
                                                          std::size_t index,
                                                          const app::ReferenceShadow& shadow)
{
    std::vector<std::string> candidates;
    const auto addCandidate = [&](const std::string& stableSubname) {
        if (stableSubname.empty() || requestLocalInternalSubname(stableSubname)) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), stableSubname) == candidates.end()) {
            candidates.push_back(stableSubname);
        }
    };

    addCandidate(shadow.stableSubname);
    if (index < link.stableSubnames.size()) {
        addCandidate(link.stableSubnames.at(index));
    }
    return candidates;
}

bool internalSubshapeMatchesReferenceShadow(const ShapeValue& shapeValue,
                                            const std::string& subname,
                                            const TopoDS_Shape& subshape,
                                            const app::ReferenceShadow& shadow)
{
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return false;
    }
    return part::referenceShadowMatchesCurrentSubshape(*shapeValue.internalShape,
                                                       "Internal",
                                                       subname,
                                                       subshape,
                                                       shadow);
}

std::optional<ReferenceSubshapeResolution> internalSubshapeFromShadowSub(const app::Link& link,
                                                                         std::size_t index,
                                                                         const app::ReferenceShadow& shadow,
                                                                         const ComputeContext& context)
{
    if (link.shadowSubs.empty()) {
        return std::nullopt;
    }
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        return std::nullopt;
    }

    for (const std::string& stableName : stableNameCandidatesForReference(link, index, shadow)) {
        for (const auto& shadowSub : link.shadowSubs) {
            if (shadowSub.newName != stableName || !requestLocalInternalSubname(shadowSub.oldName)) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
            // ::PropertyLinkBase::_updateElementReference() tries ShadowSub ElementNamePair
            // before falling back to GeoFeature::searchElementCache(). cad-core uses the
            // stable "newName" to try the paired visible Internal* name, then still validates it
            // with ReferenceShadow fingerprint/BREP before accepting the update.
            auto recovered = internalSubshapeForCurrentName(shapeIt->second, shadowSub.oldName);
            if (recovered
                && internalSubshapeMatchesReferenceShadow(shapeIt->second,
                                                          shadowSub.oldName,
                                                          recovered->shape,
                                                          shadow)) {
                recovered->recovered = true;
                recovered->recoveryMethod = "reference_shadow_single_subshape";
                recovered->recoveryReason = "shadow_sub_verified_by_reference_shadow";
                return recovered;
            }
        }
    }
    return std::nullopt;
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

nlohmann::json referenceShadowUpdateJson(const app::ReferenceShadow& shadow,
                                         const app::Link& link,
                                         const std::string& subname,
                                         const TopoDS_Shape& currentSubshape,
                                         const std::string& recoveryMethod = {},
                                         const std::string& recoveryReason = {})
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

bool hasStandaloneLabelReferenceRename(const app::Link& link)
{
    return !link.labelReferenceRenames.empty() && link.referenceShadows.empty();
}

bool hasStandaloneDocumentReferenceRename(const app::Link& link)
{
    return link.referenceShadows.empty() && link.documentRef
        && documentReferenceRenameChanged(*link.documentRef);
}

bool hasStandaloneReferenceMetadataUpdate(const app::Link& link)
{
    return hasStandaloneLabelReferenceRename(link) || hasStandaloneDocumentReferenceRename(link);
}

nlohmann::json referenceMetadataLinkUpdateJson(const app::Link& link)
{
    nlohmann::json item = {
        {"value", link.object},
        {"SubList", link.subnames},
    };
    if (!link.labelReferenceRenames.empty()) {
        item["labelReferenceRename"] = labelReferenceRenamesToJson(link.labelReferenceRenames);
    }
    if (link.documentRef && documentReferenceRenameChanged(*link.documentRef)) {
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

void appendReferenceMetadataUpdates(const app::DocumentObject& object,
                                    ComputeContext& context)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        if (propertyValue.kind == app::PropertyKind::LinkSub) {
            for (const auto& link : propertyValue.links) {
                if (!hasStandaloneReferenceMetadataUpdate(link)) {
                    continue;
                }
                nlohmann::json update = referenceMetadataLinkUpdateJson(link);
                update["object"] = object.name;
                update["property"] = propertyName;
                update["PropertyType"] = propertyValue.propertyType;
                context.elementReferenceUpdates.push_back(std::move(update));
            }
            continue;
        }
        if (propertyValue.kind != app::PropertyKind::LinkSubList) {
            continue;
        }

        bool changed = false;
        nlohmann::json subSet = nlohmann::json::array();
        for (const auto& link : propertyValue.links) {
            if (hasStandaloneReferenceMetadataUpdate(link)) {
                changed = true;
            }
            subSet.push_back(referenceMetadataLinkUpdateJson(link));
        }
        if (changed) {
            context.elementReferenceUpdates.push_back({
                {"object", object.name},
                {"property", propertyName},
                {"PropertyType", propertyValue.propertyType},
                {"SubSet", std::move(subSet)},
            });
        }
    }
}

void appendDocumentReferenceDiagnostics(const app::DocumentObject& object,
                                        ComputeContext& context)
{
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        for (const auto& link : propertyValue.links) {
            if (!link.documentRef) {
                continue;
            }
            if (documentReferenceStampChanged(*link.documentRef)) {
                addDiagnostic(context.diagnostics,
                              "warning",
                              "document_hash_mismatch",
                              propertyName + " target " + link.object
                                  + " linked document stamp changed",
                              object.name,
                              propertyName,
                              "runtime",
                              link.object);
            }
        }
    }
}

bool validateReferenceShadows(const app::DocumentObject& object,
                              ComputeContext& context)
{
    bool valid = true;
    nlohmann::json pendingReferenceUpdates = nlohmann::json::array();
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        std::map<std::size_t, nlohmann::json> subListReferenceUpdates;
        std::map<std::size_t, std::vector<std::string>> subListSubnameUpdates;
        for (std::size_t linkIndex = 0; linkIndex < propertyValue.links.size(); ++linkIndex) {
            const auto& link = propertyValue.links.at(linkIndex);
                if (link.referenceShadows.empty()) {
                    continue;
                }
                if (isFrozenExternalGeometryReference(propertyName, link)
                    || isMissingOldExternalGeometrySnapshotReference(propertyName, link, context)) {
                    // FreeCAD:
                    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                    // ::SketchObject::rebuildExternalGeometry(), for frozen refs, inserts "key" into
                    // "refSet" before source object validation; for missing refs, the old ExternalGeo
                    // remains when the source object cannot be found. cad-core therefore must not
                    // validate request-local old snapshots against a current source subshape.
                    continue;
                }

                bool linkValid = true;
                nlohmann::json updatedReferenceShadows = nlohmann::json::array();
                std::vector<std::string> updatedSubnames = link.subnames;
            const auto setUpdatedSubname = [&](std::size_t index, const std::string& subname) {
                if (index >= updatedSubnames.size()) {
                    updatedSubnames.resize(index + 1U);
                }
                updatedSubnames[index] = subname;
            };
            for (std::size_t index = 0; index < link.referenceShadows.size(); ++index) {
                const auto& shadow = link.referenceShadows.at(index);
                if (!shadow.target.empty() && shadow.target != link.object
                    && shadow.target != link.resolvedObjectFrom) {
                    continue;
                }
                const auto targetObjectIt = context.documentObjects.find(link.object);
                if (targetObjectIt != context.documentObjects.end()
                    && shadow.targetId != targetObjectIt->second->id) {
                    continue;
                }

                auto currentSubshape = currentSubshapeForReference(link, index, context);
                if (!currentSubshape) {
                    currentSubshape = internalSubshapeFromShadowSub(link, index, shadow, context);
                }
                if (!currentSubshape) {
                    const auto recovery = recoverSubshapeForReference(link, index, shadow, context);
                    if (recovery.status == part::ReferenceMatchStatus::Unique) {
                        currentSubshape = recovery.resolution;
                    }
                    else {
                        const std::string subname =
                            index < link.subnames.size() ? link.subnames.at(index) : shadow.subname;
                        recordReferenceRecoveryMapperDiagnostic(context, link, shadow, subname, recovery);
                        const std::string code = referenceRecoveryDiagnosticCode(recovery);
                        const std::string reason = referenceRecoveryDiagnosticReason(recovery);
                        addDiagnostic(context.diagnostics,
                                      "error",
                                      code,
                                      propertyName + " target " + link.object + " subname " + subname + " " + reason,
                                      object.name,
                                      propertyName,
                                      "runtime",
                                      link.object,
                                      subname);
                        valid = false;
                        linkValid = false;
                        continue;
                    }
                }
                if (!currentSubshape || currentSubshape->shape.IsNull()) {
                    continue;
                }
                if (!link.resolvedObjectFrom.empty() && link.resolvedObjectFrom != link.object
                    && currentSubshape->recoveryMethod.empty()) {
                    currentSubshape->recovered = true;
                    currentSubshape->recoveryMethod = "source_object_rename";
                    currentSubshape->recoveryReason = "ReferenceShadow.targetId matched current object ID";
                }
                // Failed ReferenceShadow validation reports the persisted property subname; only
                // successful updates below replace SubList with the recovered current subshape.
                const std::string requestedSubname =
                    index < link.subnames.size() ? link.subnames.at(index) : shadow.subname;
                if (currentSubshape->recovered) {
                    setUpdatedSubname(index, currentSubshape->subname);
                }
                const std::string subname = currentSubshape->subname;
                const auto driftReason =
                    part::referenceFingerprintDriftReason(currentSubshape->shape, shadow.fingerprint, shadow.shapeType);
                if (driftReason) {
                    if (const auto shadowSubResolution = internalSubshapeFromShadowSub(link, index, shadow, context);
                        shadowSubResolution && shadowSubResolution->subname != subname
                        && !part::referenceFingerprintDriftReason(shadowSubResolution->shape,
                                                                   shadow.fingerprint,
                                                                  shadow.shapeType)) {
                        currentSubshape = shadowSubResolution;
                        setUpdatedSubname(index, currentSubshape->subname);
                        updatedReferenceShadows.push_back(
                            referenceShadowUpdateJson(shadow,
                                                      link,
                                                      currentSubshape->subname,
                                                      currentSubshape->shape,
                                                      currentSubshape->recoveryMethod,
                                                      currentSubshape->recoveryReason));
                        continue;
                    }
                    if (shadow.brep) {
                        const auto recovery = recoverSubshapeForReference(link, index, shadow, context);
                        if (recovery.status == part::ReferenceMatchStatus::Unique && recovery.resolution
                            && !recovery.resolution->shape.IsNull()) {
                            currentSubshape = recovery.resolution;
                            setUpdatedSubname(index, currentSubshape->subname);
                            const auto recoveredDriftReason =
                                part::referenceFingerprintDriftReason(currentSubshape->shape,
                                                                       shadow.fingerprint,
                                                                       shadow.shapeType);
                            if (!recoveredDriftReason) {
                                updatedReferenceShadows.push_back(
                                    referenceShadowUpdateJson(shadow,
                                                              link,
                                                              currentSubshape->subname,
                                                              currentSubshape->shape,
                                                              currentSubshape->recoveryMethod,
                                                              currentSubshape->recoveryReason));
                                continue;
                            }
                        }
                        else if (!recovery.diagnosticCode.empty()
                                 || recovery.status == part::ReferenceMatchStatus::Ambiguous
                                 || recovery.status == part::ReferenceMatchStatus::Split
                                 || recovery.status == part::ReferenceMatchStatus::Deleted) {
                            recordReferenceRecoveryMapperDiagnostic(
                                context, link, shadow, requestedSubname, recovery);
                            const std::string code = referenceRecoveryDiagnosticCode(recovery);
                            const std::string reason = referenceRecoveryDiagnosticReason(recovery);
                            addDiagnostic(context.diagnostics,
                                          "error",
                                          code,
                                          propertyName + " target " + link.object + " subname "
                                              + requestedSubname + " " + reason,
                                          object.name,
                                          propertyName,
                                          "runtime",
                                          link.object,
                                          requestedSubname);
                            valid = false;
                            linkValid = false;
                            continue;
                        }
                    }
                    addDiagnostic(context.diagnostics,
                                  "error",
                                  "subname_semantic_drift",
                                  propertyName + " target " + link.object + " subname " + requestedSubname
                                      + " no longer matches ReferenceShadow fingerprint: " + *driftReason,
                                  object.name,
                                  propertyName,
                                  "runtime",
                                  link.object,
                                  requestedSubname);
                    valid = false;
                    linkValid = false;
                    continue;
                }
                updatedReferenceShadows.push_back(
                    referenceShadowUpdateJson(shadow,
                                              link,
                                              subname,
                                              currentSubshape->shape,
                                              currentSubshape->recoveryMethod,
                                              currentSubshape->recoveryReason));
            }
            if (linkValid) {
                appendElementReferenceUpdate(object,
                                             propertyName,
                                             propertyValue,
                                             link,
                                             updatedSubnames,
                                             updatedReferenceShadows,
                                             pendingReferenceUpdates);
                if (propertyValue.kind == app::PropertyKind::LinkSubList) {
                    subListReferenceUpdates[linkIndex] = updatedReferenceShadows;
                    subListSubnameUpdates[linkIndex] = updatedSubnames;
                }
            }
        }
        appendElementReferenceSubListUpdate(object,
                                            propertyName,
                                            propertyValue,
                                            subListReferenceUpdates,
                                            subListSubnameUpdates,
                                            pendingReferenceUpdates);
    }
    if (valid) {
        for (auto& update : pendingReferenceUpdates) {
            context.elementReferenceUpdates.push_back(std::move(update));
        }
    }
    return valid;
}

void registerIndexedNamedShape(const std::string& name, ComputeContext& context)
{
    if (context.namedShapes.count(name) != 0U) {
        return;
    }
    const auto shapeIt = context.shapes.find(name);
    if (shapeIt != context.shapes.end()) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, shapeIt->second.shape);
        return;
    }
    const auto addSubIt = context.addSubShapes.find(name);
    if (addSubIt == context.addSubShapes.end()) {
        return;
    }
    if (addSubIt->second.addNamedShape) {
        context.namedShapes[name] = *addSubIt->second.addNamedShape;
    }
    else if (addSubIt->second.subNamedShape) {
        context.namedShapes[name] = *addSubIt->second.subNamedShape;
    }
    else if (addSubIt->second.addShape) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, *addSubIt->second.addShape);
    }
    else if (addSubIt->second.subShape) {
        context.namedShapes[name] = part::indexedNamedShapeForObject(name, *addSubIt->second.subShape);
    }
}

std::string displayKind(const nlohmann::json& subshape)
{
    const std::string kind = subshape.value("kind", "");
    if (kind == "face") {
        return "Face";
    }
    if (kind == "edge") {
        return "Edge";
    }
    if (kind == "vertex") {
        return "Vertex";
    }
    return kind.empty() ? "Unknown" : kind;
}

std::string stableSubnameFor(const std::string& indexed,
                             const part::NamedShape* namedShape)
{
    const bool internalIndexed = indexed.rfind("InternalFace", 0) == 0
        || indexed.rfind("InternalEdge", 0) == 0
        || indexed.rfind("InternalVertex", 0) == 0;
    if (namedShape == nullptr) {
        return internalIndexed ? std::string{} : indexed;
    }

    std::string fallback;
    for (const auto& [stableSubname, currentSubname] : namedShape->elementMap) {
        if (currentSubname != indexed) {
            continue;
        }
        if (stableSubname != indexed) {
            return stableSubname;
        }
        fallback = stableSubname;
    }
    if (!fallback.empty()) {
        if (internalIndexed && fallback == indexed) {
            return {};
        }
        return fallback;
    }
    // Sketch Internal* names are request-local until the sketch InternalShape has a real
    // NamedShape/ElementMap. Do not synthesize a stable name from the current indexed name.
    return internalIndexed ? std::string{} : indexed;
}

std::string internalElementStableSubnameFor(const std::string& objectName,
                                            const std::string& indexed,
                                            const ComputeContext& context)
{
    if (indexed.rfind("InternalEdge", 0) != 0 && indexed.rfind("InternalVertex", 0) != 0) {
        return {};
    }

    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return {};
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return {};
    }
    const auto mappedIt = mapIt->find(indexed);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return {};
    }

    const std::string stableSubname = mappedIt->get<std::string>();
    if (stableSubname.rfind("Internal", 0) == 0) {
        return {};
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::getInternalElementMap() only maps InternalVertex/InternalEdge to raw Vertex/Edge names
    // with findSubShapesWithSharedVertex(..., CheckGeometry | SingleResult). InternalFace still
    // waits for FaceMaker/WireJoiner history before cad-core can publish a stable name.
    return stableSubname;
}

std::string currentSubnameForStable(const std::string& indexed,
                                    const std::string& stableSubname)
{
    const std::size_t dot = stableSubname.rfind('.');
    if (dot == std::string::npos) {
        return indexed;
    }
    return stableSubname.substr(0, dot + 1) + indexed;
}

bool isPlainTopologicalElementName(const std::string& name)
{
    const auto hasPrefixAndDigits = [&name](const std::string& prefix) {
        if (name.rfind(prefix, 0) != 0 || name.size() == prefix.size()) {
            return false;
        }
        return std::all_of(name.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                           name.end(),
                           [](unsigned char value) { return std::isdigit(value) != 0; });
    };
    return hasPrefixAndDigits("Face") || hasPrefixAndDigits("Edge") || hasPrefixAndDigits("Vertex");
}

std::string bodyTipQualifiedStableSubname(const std::string& objectName,
                                          const std::string& indexed,
                                          const std::string& stableSubname,
                                          const ComputeContext& context)
{
    if (stableSubname != indexed || !isPlainTopologicalElementName(stableSubname)) {
        return stableSubname;
    }

    const auto documentIt = context.documentObjects.find(objectName);
    if (documentIt == context.documentObjects.end() || documentIt->second == nullptr
        || documentIt->second->typeId != "PartDesign::Body") {
        return stableSubname;
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return stableSubname;
    }
    const auto ownerIt = objectIt->second.find("direct_tip_subshape_owner");
    if (ownerIt == objectIt->second.end() || !ownerIt->is_string() || ownerIt->get<std::string>().empty()) {
        return stableSubname;
    }
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute(),
    // "Shape.setValue(tipShape)" publishes the Tip feature shape as the Body display shape.
    // Existing NamedShape ElementMap paths win; this only qualifies unresolved bare
    // FaceN/EdgeN/VertexN entries when Body proved a direct Tip owner.
    return ownerIt->get<std::string>() + "." + stableSubname;
}

nlohmann::json responseMesh(const std::string& objectName, const nlohmann::json& mesh)
{
    if (!mesh.is_object()) {
        return nullptr;
    }

    nlohmann::json indices = nlohmann::json::array();
    const auto trianglesIt = mesh.find("triangles");
    if (trianglesIt != mesh.end() && trianglesIt->is_array()) {
        for (const auto& triangle : *trianglesIt) {
            if (!triangle.is_array()) {
                continue;
            }
            for (const auto& index : triangle) {
                if (index.is_number_integer()) {
                    indices.push_back(index.get<int>());
                }
            }
        }
    }

    nlohmann::json faceIds = nlohmann::json::array();
    const auto faceIdsIt = mesh.find("faceIds");
    if (faceIdsIt != mesh.end() && faceIdsIt->is_array()) {
        for (const auto& faceId : *faceIdsIt) {
            if (faceId.is_string()) {
                faceIds.push_back(objectName + ":" + faceId.get<std::string>());
            }
        }
    }

    nlohmann::json edgeSegments = nlohmann::json::array();
    const auto edgeSegmentsIt = mesh.find("edgeSegments");
    if (edgeSegmentsIt != mesh.end() && edgeSegmentsIt->is_array()) {
        for (const auto& segment : *edgeSegmentsIt) {
            if (!segment.is_object()) {
                continue;
            }
            const std::string id = segment.value("id", "");
            const std::string indexed = segment.value("indexed", id);
            const auto pointsIt = segment.find("points");
            if (id.empty() || indexed.empty() || pointsIt == segment.end()
                || !pointsIt->is_array()) {
                continue;
            }
            edgeSegments.push_back({
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"points", *pointsIt},
            });
        }
    }

    nlohmann::json vertexPoints = nlohmann::json::array();
    const auto vertexPointsIt = mesh.find("vertexPoints");
    if (vertexPointsIt != mesh.end() && vertexPointsIt->is_array()) {
        for (const auto& point : *vertexPointsIt) {
            if (!point.is_object()) {
                continue;
            }
            const std::string id = point.value("id", "");
            const std::string indexed = point.value("indexed", id);
            const auto pointIt = point.find("point");
            if (id.empty() || indexed.empty() || pointIt == point.end() || !pointIt->is_array()) {
                continue;
            }
            vertexPoints.push_back({
                {"id", objectName + ":" + id},
                {"indexed", indexed},
                {"point", *pointIt},
            });
        }
    }

    return {
        {"vertices", mesh.value("vertices", nlohmann::json::array())},
        {"normals", mesh.value("normals", nlohmann::json::array())},
        {"indices", indices},
        {"faceIds", faceIds},
        {"edgeSegments", edgeSegments},
        {"vertexPoints", vertexPoints},
    };
}

nlohmann::json responseSubshapes(const std::string& objectName,
                                 const ComputeContext& context)
{
    nlohmann::json subshapes = nlohmann::json::array();
    const auto subshapeIt = context.subshapes.find(objectName);
    if (subshapeIt == context.subshapes.end() || !subshapeIt->second.is_object()) {
        return subshapes;
    }

    const part::NamedShape* namedShape = nullptr;
    const auto namedShapeIt = context.namedShapes.find(objectName);
    if (namedShapeIt != context.namedShapes.end()) {
        namedShape = &namedShapeIt->second;
    }
    const ShapeValue* shapeValue = nullptr;
    const auto shapeIt = context.shapes.find(objectName);
    if (shapeIt != context.shapes.end()) {
        shapeValue = &shapeIt->second;
    }

    for (const auto& [indexed, subshape] : subshapeIt->second.items()) {
        const bool internalIndexed = indexed.rfind("InternalFace", 0) == 0
            || indexed.rfind("InternalEdge", 0) == 0
            || indexed.rfind("InternalVertex", 0) == 0;
        const part::NamedShape* stableSource = namedShape;
        if (internalIndexed && shapeValue != nullptr && shapeValue->internalNamedShape) {
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
            // ::getInternalElementMap() maps InternalEdge/InternalVertex through InternalShape,
            // while the public Sketch Shape keeps its own EdgeN/VertexN namespace.
            stableSource = &*shapeValue->internalNamedShape;
        }
        std::string stableSubname = stableSubnameFor(indexed, stableSource);
        if (stableSubname.empty()) {
            stableSubname = internalElementStableSubnameFor(objectName, indexed, context);
        }
        stableSubname = bodyTipQualifiedStableSubname(objectName, indexed, stableSubname, context);
        const std::string subname = currentSubnameForStable(indexed, stableSubname);
        subshapes.push_back({
            {"id", objectName + ":" + indexed},
            {"kind", displayKind(subshape)},
            {"indexed", indexed},
            {"subname", subname},
            {"stableSubname", stableSubname},
        });
    }
    return subshapes;
}

}  // namespace

ComputeContext recomputeContext(const app::Document& document,
                                std::vector<Diagnostic> diagnostics)
{
    graph::RecomputePlan plan = graph::buildPlan(document, diagnostics);
    FeatureRegistry registry = buildDefaultRegistry();

    ComputeContext context;
    context.diagnostics = std::move(diagnostics);
    context.dependencies = plan.dependencies;
    context.documentObjects = buildDocumentObjectMap(document);
    context.parentGroupByObject = document.parentGroupByObject;
    context.transformationTemplateObjects = findTransformationTemplateObjects(document);
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp
    // ::GeoFeature::getGlobalPlacement() returns parent GeoFeatureGroup::globalGroupPlacement()
    // multiplied by the object's own Placement.
    context.globalPlacements = buildGlobalPlacements(document);

    for (const auto& name : plan.order) {
        const auto& object = document.objects.at(document.indexByName.at(name));
        if (plan.blockedObjects.count(name) != 0U) {
            context.objects[name] = {{"status", "error"}};
            continue;
        }

        auto depIt = context.dependencies.find(name);
        if (depIt != context.dependencies.end()) {
            const auto failedIt = std::find_if(depIt->second.begin(), depIt->second.end(), [&](const std::string& dependency) {
                return hasFailed(context, dependency);
            });
            if (failedIt != depIt->second.end()) {
                context.objects[name] = {
                    {"status", "skipped"},
                    {"reason", "dependency " + *failedIt + " failed"},
                };
                continue;
            }
        }

        if (!validateReferenceShadows(object, context)) {
            context.objects[object.name] = {{"status", "error"}};
            continue;
        }
        appendReferenceMetadataUpdates(object, context);
        appendDocumentReferenceDiagnostics(object, context);

        auto executor = registry.executorFor(object.typeId);
        if (executor == nullptr) {
            addDiagnostic(context.diagnostics, "error", "unsupported_type", "Unsupported TypeId " + object.typeId, object.name);
            context.objects[object.name] = {{"status", "error"}};
            continue;
        }
        executor(object, context);
        registerIndexedNamedShape(name, context);
        context.executionOrder.push_back(name);
    }

    return context;
}

nlohmann::json recomputeResultJson(const app::Document& document,
                                   const ComputeContext& context)
{
    nlohmann::json results = nlohmann::json::array();
    for (const std::string& target : document.targets) {
        if (document.indexByName.count(target) == 0U) {
            continue;
        }
        const auto meshIt = context.mesh.find(target);
        results.push_back({
            {"object", target},
            {"mesh", meshIt == context.mesh.end() ? nlohmann::json(nullptr) : responseMesh(target, meshIt->second)},
            {"subshapes", responseSubshapes(target, context)},
        });
    }

    return {
        {"results", results},
        {"elementReferenceUpdates", context.elementReferenceUpdates},
        {"documentObjectUpdates", context.documentObjectUpdates},
        {"diagnostics", diagnosticsToJson(context.diagnostics)},
        {"binaryPayloads", nlohmann::json::array()},
    };
}

nlohmann::json recompute(const app::Document& document,
                         std::vector<Diagnostic> diagnostics)
{
    const ComputeContext context = recomputeContext(document, std::move(diagnostics));
    return recomputeResultJson(document, context);
}

}  // namespace cad_core::runtime
