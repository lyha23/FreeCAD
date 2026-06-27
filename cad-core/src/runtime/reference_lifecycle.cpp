#include "cad_core/runtime/reference_lifecycle.h"

#include <algorithm>
#include <cctype>

namespace cad_core::runtime
{

namespace
{

std::string linkedDocumentName(const app::LinkDocumentRef& documentRef)
{
    if (!documentRef.file.empty()) {
        return documentRef.file;
    }
    if (!documentRef.currentName.empty()) {
        return documentRef.currentName;
    }
    if (!documentRef.name.empty()) {
        return documentRef.name;
    }
    if (!documentRef.currentLabel.empty()) {
        return documentRef.currentLabel;
    }
    return documentRef.label;
}

std::string normalizedDocumentStatus(const std::string& status)
{
    std::string normalized;
    normalized.reserve(status.size());
    for (const char item : status) {
        if (item == '-') {
            normalized.push_back('_');
        }
        else {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(item))));
        }
    }
    return normalized;
}

bool documentRefPendingReload(const app::LinkDocumentRef& documentRef)
{
    const std::string status = normalizedDocumentStatus(documentRef.status);
    const std::string currentStatus = normalizedDocumentStatus(documentRef.currentStatus);
    const std::string& effectiveStatus = currentStatus.empty() ? status : currentStatus;
    return effectiveStatus == "pending" || effectiveStatus == "pending_reload"
        || effectiveStatus == "partial";
}

bool documentRefUnloaded(const app::LinkDocumentRef& documentRef)
{
    const std::string status = normalizedDocumentStatus(documentRef.status);
    const std::string currentStatus = normalizedDocumentStatus(documentRef.currentStatus);
    const std::string& effectiveStatus = currentStatus.empty() ? status : currentStatus;
    return effectiveStatus == "unloaded" || effectiveStatus == "deleted"
        || effectiveStatus == "detached";
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

bool hasReferenceShadowBrepSnapshot(const app::Link& link)
{
    return std::any_of(link.referenceShadows.begin(), link.referenceShadows.end(), [](const auto& shadow) {
        return shadow.brep.has_value();
    });
}

const nlohmann::json* externalGeoGeometryItems(const app::DocumentObject& object)
{
    const auto it = object.properties.find("ExternalGeo");
    if (it == object.properties.end()) {
        return nullptr;
    }
    const auto& raw = *it;
    if (raw.is_array()) {
        return &raw;
    }
    if (!raw.is_object()) {
        return nullptr;
    }
    for (const char* key : {"Geometry", "Values", "Items"}) {
        const auto items = raw.find(key);
        if (items != raw.end() && items->is_array()) {
            return &*items;
        }
    }
    return nullptr;
}

std::string readExternalGeoRef(const nlohmann::json& value)
{
    for (const char* key : {"Ref", "ref", "reference"}) {
        const auto it = value.find(key);
        if (it != value.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return {};
}

bool hasNativeExternalGeoEvidence(const app::DocumentObject& object, const app::Link& link)
{
    const auto* items = externalGeoGeometryItems(object);
    if (items == nullptr) {
        return false;
    }
    const std::string key = externalGeometryReferenceKey(link);
    return std::any_of(items->begin(), items->end(), [&](const auto& item) {
        return item.is_object() && readExternalGeoRef(item) == key;
    });
}

std::string firstSubname(const app::Link& link)
{
    return link.subnames.empty() ? std::string{} : link.subnames.front();
}

Diagnostic makeGraphDiagnostic(const std::string& code,
                               const std::string& message,
                               const app::DocumentObject& owner,
                               const std::string& propertyName,
                               const app::Link& link)
{
    return Diagnostic {"error",
                       code,
                       message,
                       owner.name,
                       propertyName,
                       "graph",
                       link.object,
                       firstSubname(link)};
}

Diagnostic makeRuntimeWarning(const std::string& code,
                              const std::string& message,
                              const app::DocumentObject& owner,
                              const std::string& propertyName,
                              const app::Link& link)
{
    return Diagnostic {"warning", code, message, owner.name, propertyName, "runtime", link.object, {}};
}

bool targetExists(const app::Link& link, const ReferenceLifecycleView& view)
{
    return view.documentObjects.count(link.object) != 0U;
}

bool rejectsSubShapeBinderSupportSetterCycle(const app::DocumentObject& owner,
                                             const app::PropertyValue& propertyValue,
                                             const std::string& propertyName,
                                             const app::Link& link)
{
    // FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp
    // ::SubShapeBinder::setLinks() builds "auto inSet = getInListEx(true); inSet.insert(this)"
    // and throws "Cyclic reference to ..." before assigning Support when the target is inSet.
    // cad-core only replays the request-local self-link case for the Support
    // App::PropertyXLinkSubList setter; full property editor lifecycle/cache behavior remains out of scope.
    return owner.typeId == "PartDesign::SubShapeBinder" && propertyName == "Support"
        && propertyValue.propertyType == "App::PropertyXLinkSubList" && link.object == owner.name;
}

}  // namespace

ExternalGeometryLifecycleFlags externalGeometryLifecycleFlags(const app::Link& link)
{
    ExternalGeometryLifecycleFlags flags;
    flags.defining = link.externalGeometryFlags.count("Defining") != 0U;
    flags.frozen = link.externalGeometryFlags.count("Frozen") != 0U;
    flags.detached = link.externalGeometryFlags.count("Detached") != 0U;
    flags.missing = link.externalGeometryFlags.count("Missing") != 0U;
    flags.sync = link.externalGeometryFlags.count("Sync") != 0U;
    return flags;
}

std::set<std::string> normalizedExternalGeometryFlagSet(ExternalGeometryLifecycleFlags flags)
{
    std::set<std::string> result;
    if (flags.defining) {
        result.insert("Defining");
    }
    if (flags.frozen) {
        result.insert("Frozen");
    }
    if (flags.detached) {
        result.insert("Detached");
    }
    if (flags.missing) {
        result.insert("Missing");
    }
    if (flags.sync) {
        result.insert("Sync");
    }
    return result;
}

std::string externalGeometryReferenceKey(const app::Link& link)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return link.object;
    }
    return link.object + "." + link.subnames.front();
}

ReferenceLifecycleDecision classifyReferenceLifecycle(const app::DocumentObject& owner,
                                                      const app::PropertyValue& propertyValue,
                                                      const app::Link& link,
                                                      const ReferenceLifecycleView& view)
{
    ReferenceLifecycleDecision decision;
    const std::string propertyName = link.property.empty() ? propertyValue.name : link.property;
    decision.targetExists = targetExists(link, view);
    decision.requiresGraphDependency = decision.targetExists;
    if (rejectsSubShapeBinderSupportSetterCycle(owner, propertyValue, propertyName, link)) {
        decision.state = ReferenceLifecycleState::PropertyLinkSetterCycleRejected;
        decision.action = ReferenceLifecycleAction::BlockRecompute;
        decision.diagnostic = makeGraphDiagnostic(
            "cycle_rejected_by_property_link",
            "PartDesign::SubShapeBinder Support rejected cyclic link to " + link.object
                + " before recompute",
            owner,
            propertyName,
            link
        );
        return decision;
    }
    decision.hasLabelReferenceRename = !link.labelReferenceRenames.empty();
    if (link.documentRef) {
        decision.hasDocumentReferenceRename = documentReferenceRenameChanged(*link.documentRef);
        decision.hasDocumentReferenceStampMismatch = documentReferenceStampChanged(*link.documentRef);
        if (decision.hasDocumentReferenceStampMismatch) {
            decision.runtimeWarning = makeRuntimeWarning(
                "document_hash_mismatch",
                propertyName + " target " + link.object + " linked document stamp changed",
                owner,
                propertyName,
                link
            );
        }
    }
    decision.shouldPublishElementReferenceUpdate =
        link.referenceShadows.empty()
        && (decision.hasLabelReferenceRename || decision.hasDocumentReferenceRename);
    if (decision.shouldPublishElementReferenceUpdate) {
        decision.state = ReferenceLifecycleState::MetadataOnlyUpdate;
        decision.action = ReferenceLifecycleAction::PublishMetadataOnlyUpdate;
    }

    const bool externalGeometry = propertyName == "ExternalGeometry";
    const ExternalGeometryLifecycleFlags flags = externalGeometryLifecycleFlags(link);
    const bool frozenOldExternal = externalGeometry && flags.frozen && !flags.sync;
    const bool missingOldExternal = externalGeometry && flags.missing && !flags.sync;
    if (externalGeometry && flags.detached) {
        decision.state = ReferenceLifecycleState::DetachedExternalGeometry;
        decision.action = ReferenceLifecycleAction::DetachReference;
        decision.shouldValidateReferenceShadow = false;
        decision.requiresGraphDependency = decision.targetExists;
    }
    else if (frozenOldExternal) {
        decision.state = ReferenceLifecycleState::FrozenOldExternalGeometry;
        decision.shouldValidateReferenceShadow = false;
    }

    if (decision.targetExists) {
        return decision;
    }

    decision.requiresGraphDependency = false;
    if (externalGeometry && flags.detached) {
        return decision;
    }

    const bool oldExternalGeometrySnapshot = frozenOldExternal || missingOldExternal;
    if (oldExternalGeometrySnapshot) {
        decision.state = frozenOldExternal ? ReferenceLifecycleState::FrozenOldExternalGeometry
                                           : ReferenceLifecycleState::MissingOldExternalGeometry;
        decision.shouldValidateReferenceShadow = false;
        decision.canUseNativeExternalGeoEvidence = hasNativeExternalGeoEvidence(owner, link);
        decision.canUseReferenceShadowBrepEvidence = hasReferenceShadowBrepSnapshot(link);
        if (decision.canUseNativeExternalGeoEvidence || decision.canUseReferenceShadowBrepEvidence) {
            decision.action = ReferenceLifecycleAction::IgnoreDependencyUseOldEvidence;
            return decision;
        }

        const std::string state = frozenOldExternal ? "Frozen" : "Missing";
        decision.action = ReferenceLifecycleAction::BlockRecompute;
        decision.diagnostic = makeGraphDiagnostic(
            "missing_external_geometry_snapshot",
            state + " ExternalGeometry target " + link.object
                + " is missing and the request does not include native ExternalGeo or "
                  "ReferenceShadow.brep evidence",
            owner,
            propertyName,
            link
        );
        return decision;
    }

    if (link.documentRef) {
        const std::string documentName = linkedDocumentName(*link.documentRef);
        if (!documentName.empty()) {
            std::string code = "missing_external_document";
            std::string message =
                "XLink target " + link.object + " from linked document " + documentName + " is not available";
            decision.state = ReferenceLifecycleState::ExternalDocumentMissing;
            if (documentRefPendingReload(*link.documentRef)) {
                code = "external_document_pending_reload";
                message = "XLink target " + link.object + " from linked document " + documentName
                    + " is pending reload";
                if (link.documentRef->allowPartialExplicit) {
                    message += link.documentRef->allowPartial ? " with partial load allowed"
                                                              : " with partial load disabled";
                }
                decision.state = ReferenceLifecycleState::ExternalDocumentPendingReload;
            }
            else if (documentRefUnloaded(*link.documentRef)) {
                code = "external_document_unloaded";
                message = "XLink target " + link.object + " from linked document " + documentName
                    + " was unloaded or deleted";
                decision.state = ReferenceLifecycleState::ExternalDocumentUnloaded;
            }
            decision.action = ReferenceLifecycleAction::BlockRecompute;
            decision.diagnostic = makeGraphDiagnostic(code, message, owner, propertyName, link);
            return decision;
        }
    }

    decision.state = ReferenceLifecycleState::MissingTarget;
    decision.action = ReferenceLifecycleAction::BlockRecompute;
    decision.diagnostic = makeGraphDiagnostic("missing_link_target",
                                              "Object links to missing object " + link.object,
                                              owner,
                                              propertyName,
                                              link);
    return decision;
}

}  // namespace cad_core::runtime
