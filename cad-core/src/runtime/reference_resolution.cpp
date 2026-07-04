#include "cad_core/runtime/reference_resolution.h"

#include "cad_core/runtime/element_reference_update.h"
#include "cad_core/runtime/reference_lifecycle.h"

#include <algorithm>
#include <cctype>

namespace cad_core::runtime
{

namespace
{

std::string internalSubnameFromStableElementMap(const ReferenceResolutionView& view,
                                                const std::string& objectName,
                                                const std::string& stableSubname)
{
    if (stableSubname.empty() || stableSubname.rfind("Internal", 0) == 0) {
        return {};
    }
    const auto objectIt = view.objects.find(objectName);
    if (objectIt == view.objects.end() || !objectIt->second.is_object()) {
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

bool requestLocalInternalSubname(const std::string& subname)
{
    return part::parseInternalSubshapeName(subname).has_value();
}

bool sketchGeometryStableSubname(const std::string& stableSubname)
{
    if (stableSubname.size() < 2U || stableSubname.front() != 'g') {
        return false;
    }
    return std::all_of(stableSubname.begin() + 1,
                       stableSubname.end(),
                       [](unsigned char value) { return std::isdigit(value) != 0; });
}

bool sketchGeometrySplitFragmentStableSubname(const std::string& stableSubname)
{
    const std::string marker = ":split";
    const std::size_t markerPosition = stableSubname.find(marker);
    if (markerPosition == std::string::npos || markerPosition == 0U) {
        return false;
    }
    const std::string sourceStableSubname = stableSubname.substr(0, markerPosition);
    if (!sketchGeometryStableSubname(sourceStableSubname)) {
        return false;
    }
    const std::string fragment = stableSubname.substr(markerPosition + marker.size());
    return !fragment.empty()
        && std::all_of(fragment.begin(), fragment.end(), [](unsigned char value) {
               return std::isdigit(value) != 0;
           });
}

bool sketchGeometryIdentityStableSubname(const std::string& stableSubname)
{
    return sketchGeometryStableSubname(stableSubname)
        || sketchGeometrySplitFragmentStableSubname(stableSubname);
}

std::string sourceStableSubnameForReference(const app::Link& link,
                                            std::size_t index,
                                            const app::ReferenceShadow& shadow)
{
    const auto choose = [](const std::string& stableSubname) {
        return sketchGeometryIdentityStableSubname(stableSubname) ? stableSubname : std::string {};
    };
    if (index < link.stableSubnames.size()) {
        if (const std::string stableSubname = choose(link.stableSubnames.at(index));
            !stableSubname.empty()) {
            return stableSubname;
        }
    }
    if (const std::string stableSubname = choose(shadow.stableSubname); !stableSubname.empty()) {
        return stableSubname;
    }
    return choose(shadow.sourceStableSubname);
}

struct RawSketchSourceIdentity
{
    std::string indexed;
    std::optional<long> sourceGeometryId;
    std::string sourceGeometryKind;
    std::string sourceStableSubname;
};

bool objectPublishesRawSketchEdgeIdentity(const ReferenceResolutionView& view,
                                          const std::string& objectName)
{
    const auto objectIt = view.objects.find(objectName);
    if (objectIt == view.objects.end() || !objectIt->second.is_object()) {
        return false;
    }
    const auto identityIt = objectIt->second.find("raw_edge_identity");
    return identityIt != objectIt->second.end() && identityIt->is_object();
}

std::optional<RawSketchSourceIdentity> rawSketchSourceIdentityForStable(
    const ReferenceResolutionView& view,
    const std::string& objectName,
    const std::string& stableSubname)
{
    const auto objectIt = view.objects.find(objectName);
    if (objectIt == view.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto identityIt = objectIt->second.find("raw_edge_identity");
    if (identityIt == objectIt->second.end() || !identityIt->is_object()) {
        return std::nullopt;
    }
    const auto byStableIt = identityIt->find("byStableSubname");
    if (byStableIt == identityIt->end() || !byStableIt->is_object()) {
        return std::nullopt;
    }
    const auto indexedIt = byStableIt->find(stableSubname);
    if (indexedIt == byStableIt->end() || !indexedIt->is_string()) {
        return std::nullopt;
    }

    RawSketchSourceIdentity identity;
    identity.indexed = indexedIt->get<std::string>();
    const auto byIndexedIt = identityIt->find("byIndexed");
    if (byIndexedIt != identityIt->end() && byIndexedIt->is_object()) {
        const auto currentIt = byIndexedIt->find(identity.indexed);
        if (currentIt != byIndexedIt->end() && currentIt->is_object()) {
            const auto sourceGeometryIdIt = currentIt->find("sourceGeometryId");
            if (sourceGeometryIdIt != currentIt->end() && sourceGeometryIdIt->is_number_integer()) {
                identity.sourceGeometryId = sourceGeometryIdIt->get<long long>();
            }
            const auto sourceGeometryKindIt = currentIt->find("sourceGeometryKind");
            if (sourceGeometryKindIt != currentIt->end() && sourceGeometryKindIt->is_string()) {
                identity.sourceGeometryKind = sourceGeometryKindIt->get<std::string>();
            }
            const auto sourceStableSubnameIt = currentIt->find("sourceStableSubname");
            if (sourceStableSubnameIt != currentIt->end() && sourceStableSubnameIt->is_string()) {
                identity.sourceStableSubname = sourceStableSubnameIt->get<std::string>();
            }
        }
    }
    if (identity.sourceStableSubname.empty()) {
        identity.sourceStableSubname = stableSubname;
    }
    return identity;
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

ReferenceResolutionStatus statusForRecovery(part::ReferenceMatchStatus status)
{
    switch (status) {
        case part::ReferenceMatchStatus::Ambiguous:
            return ReferenceResolutionStatus::Ambiguous;
        case part::ReferenceMatchStatus::Split:
            return ReferenceResolutionStatus::Split;
        case part::ReferenceMatchStatus::Deleted:
            return ReferenceResolutionStatus::Deleted;
        case part::ReferenceMatchStatus::Unique:
            return ReferenceResolutionStatus::Recovered;
        case part::ReferenceMatchStatus::Missing:
            return ReferenceResolutionStatus::Missing;
    }
    return ReferenceResolutionStatus::Missing;
}

bool isSuccess(ReferenceResolutionStatus status)
{
    return status == ReferenceResolutionStatus::Resolved || status == ReferenceResolutionStatus::Recovered;
}

ReferenceResolutionResult makeRecoveryFailureResult(const app::Link& link,
                                                    std::size_t index,
                                                    const app::ReferenceShadow& shadow,
                                                    const std::string& propertyName,
                                                    const ReferenceSubshapeRecovery& recovery)
{
    ReferenceResolutionResult result;
    result.requestedObject = link.object;
    result.propertyName = propertyName;
    result.requestedSubname = index < link.subnames.size() ? link.subnames.at(index) : shadow.subname;
    result.requestedStableSubname =
        index < link.stableSubnames.size() ? link.stableSubnames.at(index) : shadow.stableSubname;
    result.status = statusForRecovery(recovery.status);
    result.recoveryStatus = recovery.status;
    result.diagnosticCode = referenceRecoveryDiagnosticCode(recovery);
    result.diagnosticReason = referenceRecoveryDiagnosticReason(recovery);
    result.mapperDiagnostic = true;
    return result;
}

}  // namespace

std::optional<ReferenceSubshapeResolution> currentSubshapeForReference(const app::Link& link,
                                                                       std::size_t index,
                                                                       const ReferenceResolutionView& view)
{
    if (index >= link.subnames.size() || link.subnames.at(index).empty()) {
        return std::nullopt;
    }

    const auto shapeIt = view.shapes.find(link.object);
    if (shapeIt == view.shapes.end()) {
        return std::nullopt;
    }

    const std::string& subname = link.subnames.at(index);
    const std::string stableSubname = index < link.stableSubnames.size() ? link.stableSubnames.at(index) : std::string{};
    if (part::parseInternalSubshapeName(subname)) {
        const auto current = internalSubshapeForCurrentName(shapeIt->second, subname);
        if (current) {
            return current;
        }
        if (const auto stableInternal = internalSubnameFromStableElementMap(view, link.object, stableSubname);
            !stableInternal.empty()) {
            auto recovered = internalSubshapeForCurrentName(shapeIt->second, stableInternal);
            if (recovered) {
                recovered->recovered = true;
                return recovered;
            }
        }
        return std::nullopt;
    }

    const auto namedShapeIt = view.namedShapes.find(link.object);
    if (namedShapeIt != view.namedShapes.end()) {
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
                                                      const ReferenceResolutionView& view)
{
    if (index >= link.subnames.size() || link.subnames.at(index).empty()) {
        return {};
    }
    const auto shapeIt = view.shapes.find(link.object);
    if (shapeIt == view.shapes.end()) {
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

std::optional<ReferenceSubshapeResolution> internalSubshapeFromShadowSub(const app::Link& link,
                                                                         std::size_t index,
                                                                         const app::ReferenceShadow& shadow,
                                                                         const ReferenceResolutionView& view)
{
    if (link.shadowSubs.empty()) {
        return std::nullopt;
    }
    const auto shapeIt = view.shapes.find(link.object);
    if (shapeIt == view.shapes.end()) {
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

ReferenceResolutionResult resolveReferenceShadow(const app::Link& link,
                                                 std::size_t index,
                                                 const app::ReferenceShadow& shadow,
                                                 const std::string& propertyName,
                                                 ReferenceResolutionView& view)
{
    ReferenceResolutionResult result;
    result.requestedObject = link.object;
    result.propertyName = propertyName;
    result.requestedSubname = index < link.subnames.size() ? link.subnames.at(index) : shadow.subname;
    result.requestedStableSubname =
        index < link.stableSubnames.size() ? link.stableSubnames.at(index) : shadow.stableSubname;

    std::optional<ReferenceSubshapeResolution> currentSubshape;
    bool resolvedBySketchGeometryId = false;
    const std::string sourceStableSubname = sourceStableSubnameForReference(link, index, shadow);
    const bool rawSketchEdgeReference =
        (shadow.property == "Shape" || shadow.property == "InternalShape")
        && shadow.shapeType == "Edge";
    if (rawSketchEdgeReference && !sourceStableSubname.empty()
        && objectPublishesRawSketchEdgeIdentity(view, link.object)) {
        const auto sourceIdentity =
            rawSketchSourceIdentityForStable(view, link.object, sourceStableSubname);
        if (!sourceIdentity) {
            if (sketchGeometrySplitFragmentStableSubname(sourceStableSubname)) {
                result.requestedSubname = sourceStableSubname;
                result.requestedStableSubname = sourceStableSubname;
                result.status = ReferenceResolutionStatus::Missing;
                result.recoveryStatus = part::ReferenceMatchStatus::Missing;
                result.diagnosticCode = "split_fragment_missing";
                result.diagnosticReason =
                    "split fragment " + sourceStableSubname
                    + " is missing from current Sketch raw edge identity";
                return result;
            }
            result.status = ReferenceResolutionStatus::Deleted;
            result.recoveryStatus = part::ReferenceMatchStatus::Deleted;
            result.diagnosticCode = "deleted_stable_subname";
            result.diagnosticReason =
                sourceStableSubname + " is deleted from current Sketch raw edge identity";
            return result;
        }
        if (!shadow.sourceGeometryKind.empty() && !sourceIdentity->sourceGeometryKind.empty()
            && shadow.sourceGeometryKind != sourceIdentity->sourceGeometryKind) {
            result.status = ReferenceResolutionStatus::SemanticDrift;
            result.diagnosticCode = "geometry_kind_changed";
            result.diagnosticReason = sourceStableSubname + " geometry kind changed from "
                + shadow.sourceGeometryKind + " to " + sourceIdentity->sourceGeometryKind;
            return result;
        }
        const std::string namedShapeKey =
            part::parseInternalSubshapeName(sourceIdentity->indexed) ? link.object + ".InternalShape"
                                                                     : link.object;
        const auto namedShapeIt = view.namedShapes.find(namedShapeKey);
        const auto subshape = namedShapeIt == view.namedShapes.end()
            ? std::nullopt
            : part::subshapeByName(namedShapeIt->second, sourceIdentity->indexed);
        if (!subshape) {
            result.status = ReferenceResolutionStatus::Missing;
            result.diagnosticCode = "unsupported_stable_subname";
            result.diagnosticReason = sourceStableSubname
                + " resolved in Sketch raw edge identity but not in the current NamedShape";
            return result;
        }
        currentSubshape = ReferenceSubshapeResolution {
            sourceIdentity->indexed,
            *subshape,
            sourceIdentity->indexed != result.requestedSubname,
            {},
            {},
            sourceIdentity->sourceGeometryId,
            sourceIdentity->sourceGeometryKind,
            sourceIdentity->sourceStableSubname,
        };
        resolvedBySketchGeometryId = true;
    }

    if (!currentSubshape) {
        currentSubshape = currentSubshapeForReference(link, index, view);
    }
    if (!currentSubshape) {
        currentSubshape = internalSubshapeFromShadowSub(link, index, shadow, view);
    }
    if (!currentSubshape) {
        const auto recovery = recoverSubshapeForReference(link, index, shadow, view);
        if (recovery.status == part::ReferenceMatchStatus::Unique) {
            currentSubshape = recovery.resolution;
        }
        else {
            return makeRecoveryFailureResult(link, index, shadow, propertyName, recovery);
        }
    }
    if (!currentSubshape || currentSubshape->shape.IsNull()) {
        return result;
    }
    if (!link.resolvedObjectFrom.empty() && link.resolvedObjectFrom != link.object
        && currentSubshape->recoveryMethod.empty()) {
        currentSubshape->recovered = true;
        currentSubshape->recoveryMethod = "source_object_rename";
        currentSubshape->recoveryReason = "ReferenceShadow.targetId matched current object ID";
    }

    const auto driftReason = resolvedBySketchGeometryId
        ? std::optional<std::string> {}
        : part::referenceFingerprintDriftReason(currentSubshape->shape,
                                                shadow.fingerprint,
                                                shadow.shapeType);
    if (driftReason) {
        if (const auto shadowSubResolution = internalSubshapeFromShadowSub(link, index, shadow, view);
            shadowSubResolution && shadowSubResolution->subname != currentSubshape->subname
            && !part::referenceFingerprintDriftReason(shadowSubResolution->shape,
                                                       shadow.fingerprint,
                                                       shadow.shapeType)) {
            result.status = ReferenceResolutionStatus::Recovered;
            result.resolvedSubname = shadowSubResolution->subname;
            result.resolvedShape = shadowSubResolution->shape;
            result.recoveryMethod = shadowSubResolution->recoveryMethod;
            result.recoveryReason = shadowSubResolution->recoveryReason;
            return result;
        }
        if (shadow.brep) {
            const auto recovery = recoverSubshapeForReference(link, index, shadow, view);
            if (recovery.status == part::ReferenceMatchStatus::Unique && recovery.resolution
                && !recovery.resolution->shape.IsNull()) {
                const auto recoveredDriftReason =
                    part::referenceFingerprintDriftReason(recovery.resolution->shape,
                                                           shadow.fingerprint,
                                                           shadow.shapeType);
                if (!recoveredDriftReason) {
                    result.status = ReferenceResolutionStatus::Recovered;
                    result.resolvedSubname = recovery.resolution->subname;
                    result.resolvedShape = recovery.resolution->shape;
                    result.recoveryMethod = recovery.resolution->recoveryMethod;
                    result.recoveryReason = recovery.resolution->recoveryReason;
                    return result;
                }
            }
            else if (!recovery.diagnosticCode.empty()
                     || recovery.status == part::ReferenceMatchStatus::Ambiguous
                     || recovery.status == part::ReferenceMatchStatus::Split
                     || recovery.status == part::ReferenceMatchStatus::Deleted) {
                return makeRecoveryFailureResult(link, index, shadow, propertyName, recovery);
            }
        }
        result.status = ReferenceResolutionStatus::SemanticDrift;
        result.diagnosticCode = "subname_semantic_drift";
        result.diagnosticReason = "no longer matches ReferenceShadow fingerprint: " + *driftReason;
        return result;
    }

    result.status =
        currentSubshape->recovered ? ReferenceResolutionStatus::Recovered : ReferenceResolutionStatus::Resolved;
    result.resolvedSubname = currentSubshape->subname;
    result.resolvedShape = currentSubshape->shape;
    result.recoveryMethod = currentSubshape->recoveryMethod;
    result.recoveryReason = currentSubshape->recoveryReason;
    result.sourceGeometryId = currentSubshape->sourceGeometryId;
    result.sourceGeometryKind = currentSubshape->sourceGeometryKind;
    result.sourceStableSubname = currentSubshape->sourceStableSubname;
    return result;
}

void recordReferenceRecoveryMapperDiagnostic(ReferenceResolutionView& view,
                                             const app::Link& link,
                                             const app::ReferenceShadow& shadow,
                                             const ReferenceResolutionResult& result)
{
    const bool internalReference = part::parseInternalSubshapeName(result.requestedSubname).has_value()
        || shadow.property == "InternalShape";
    const std::string namedShapeKey = internalReference ? link.object + ".InternalShape" : link.object;
    auto namedShapeIt = view.namedShapes.find(namedShapeKey);
    if (namedShapeIt == view.namedShapes.end()) {
        return;
    }

    part::MapperHistoryEvent event;
    event.source = part::MapperHistoryEndpoint {link.object, result.requestedSubname};
    event.target = part::MapperHistoryEndpoint {namedShapeIt->second.owner, {}};
    event.shapeKind = referenceSubnameShapeKind(result.requestedSubname);
    event.relation = referenceRelation(result.recoveryStatus);
    event.makerStage = "reference_shadow_recovery";
    event.evidence = {
        {"reference_shadow", true},
        {"requested_subname", result.requestedSubname},
        {"status", referenceMatchStatusName(result.recoveryStatus)},
        {"reason", result.diagnosticReason},
    };
    event.recoverability = referenceRecoverability(result.recoveryStatus);
    event.diagnosticStatus = result.diagnosticCode;
    part::addMapperHistoryEvent(namedShapeIt->second.mapperHistory, std::move(event));
}

ReferenceValidationResult validateObjectReferences(const app::DocumentObject& object,
                                                   ReferenceResolutionView& view,
                                                   const ReferenceLifecycleView& lifecycleView)
{
    ReferenceValidationResult validation;
    nlohmann::json pendingReferenceUpdates = nlohmann::json::array();
    for (const auto& [propertyName, propertyValue] : object.propertyValues) {
        std::map<std::size_t, nlohmann::json> subListReferenceUpdates;
        std::map<std::size_t, std::vector<std::string>> subListSubnameUpdates;
        for (std::size_t linkIndex = 0; linkIndex < propertyValue.links.size(); ++linkIndex) {
            const auto& link = propertyValue.links.at(linkIndex);
            if (link.referenceShadows.empty()) {
                continue;
            }
            const auto lifecycle =
                classifyReferenceLifecycle(object, propertyValue, link, lifecycleView);
            if (!lifecycle.shouldValidateReferenceShadow) {
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
                const auto targetObjectIt = view.documentObjects.find(link.object);
                if (targetObjectIt != view.documentObjects.end()
                    && shadow.targetId != targetObjectIt->second->id) {
                    continue;
                }

                const auto resolution = resolveReferenceShadow(link, index, shadow, propertyName, view);
                if (!isSuccess(resolution.status)) {
                    if (resolution.diagnosticCode.empty()) {
                        continue;
                    }
                    if (resolution.mapperDiagnostic) {
                        recordReferenceRecoveryMapperDiagnostic(view, link, shadow, resolution);
                    }
                    addDiagnostic(validation.diagnostics,
                                  "error",
                                  resolution.diagnosticCode,
                                  propertyName + " target " + link.object + " subname "
                                      + resolution.requestedSubname + " " + resolution.diagnosticReason,
                                  object.name,
                                  propertyName,
                                  "runtime",
                                  link.object,
                                  resolution.requestedSubname);
                    validation.valid = false;
                    linkValid = false;
                    continue;
                }

                if (resolution.status == ReferenceResolutionStatus::Recovered) {
                    setUpdatedSubname(index, resolution.resolvedSubname);
                }
                updatedReferenceShadows.push_back(
                    referenceShadowUpdateJson(shadow,
                                              link,
                                              resolution.resolvedSubname,
                                              resolution.resolvedShape,
                                              resolution.recoveryMethod,
                                              resolution.recoveryReason,
                                              resolution.sourceGeometryId,
                                              resolution.sourceGeometryKind,
                                              resolution.sourceStableSubname));
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
    if (validation.valid) {
        validation.elementReferenceUpdates = std::move(pendingReferenceUpdates);
    }
    return validation;
}

}  // namespace cad_core::runtime
