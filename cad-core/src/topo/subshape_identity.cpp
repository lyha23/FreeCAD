#include "cad_core/topo/subshape_identity.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace cad_core::topo {

namespace {

std::optional<TopAbs_ShapeEnum> parseTopologicalKind(const std::string& name)
{
    if (const auto parsed = part::parseSubshapeName(name)) {
        return parsed->kind;
    }
    if (const auto parsed = part::parseInternalSubshapeName(name)) {
        return parsed->kind;
    }
    return std::nullopt;
}

int stableSubnamePriority(const std::string& indexed, const std::string& stableSubname)
{
    if (stableSubname.empty() || stableSubname == indexed) {
        return -1;
    }
    const std::string local = localElementName(stableSubname);
    if (!topologicalElementKind(local)) {
        return 3;
    }
    return local == indexed ? 1 : 2;
}

std::string durableReferenceDiagnosticCode(StableIdentityStatus status,
                                           bool fullSubnameOnlyEvidence)
{
    switch (status) {
        case StableIdentityStatus::MissingEvidence:
            return fullSubnameOnlyEvidence ? "full_subname_not_stable_identity"
                                           : "missing_stable_subname";
        case StableIdentityStatus::Split:
        case StableIdentityStatus::Ambiguous:
            return "stable_identity_ambiguous";
        case StableIdentityStatus::Deleted:
            return "stable_identity_deleted";
        case StableIdentityStatus::Unsupported:
        case StableIdentityStatus::CurrentOnly:
        case StableIdentityStatus::BodyDisplayOnly:
            return "unstable_subshape_reference";
        case StableIdentityStatus::Stable:
        case StableIdentityStatus::StableSplitFragment:
            return {};
    }
    return "unstable_subshape_reference";
}

std::string durableReferenceDiagnosticMessage(const DurableSubshapeReferenceRequest& request,
                                              StableIdentityStatus status,
                                              bool fullSubnameOnlyEvidence)
{
    const std::string target = request.targetObject.empty() ? std::string{"target"}
                                                            : request.targetObject;
    const std::string subname = request.stableSubname.empty() ? request.subname
                                                              : request.stableSubname;
    switch (status) {
        case StableIdentityStatus::MissingEvidence:
            if (fullSubnameOnlyEvidence) {
                return request.property + " target " + target + " uses FullSubList "
                    + request.fullSubname
                    + " as display path only; FullSubList is not stable topology identity";
            }
            return request.property + " target " + target
                + " requires StableSubList evidence for durable subshape reference";
        case StableIdentityStatus::Split:
        case StableIdentityStatus::Ambiguous:
            return request.property + " target " + target + " stable subname " + subname
                + " is ambiguous in current mapper history";
        case StableIdentityStatus::Deleted:
            return request.property + " target " + target + " stable subname " + subname
                + " was deleted in current mapper history";
        case StableIdentityStatus::Unsupported:
        case StableIdentityStatus::CurrentOnly:
        case StableIdentityStatus::BodyDisplayOnly:
            return request.property + " target " + target + " subshape " + subname
                + " only names the current shape and has no NamedShape/ElementMap identity evidence";
        case StableIdentityStatus::Stable:
        case StableIdentityStatus::StableSplitFragment:
            return {};
    }
    return request.property + " target " + target + " subshape " + subname
        + " has no stable topology identity evidence";
}

StableIdentityStatus statusForElementResolve(part::ElementResolveStatus status)
{
    switch (status) {
        case part::ElementResolveStatus::Resolved:
            return StableIdentityStatus::Stable;
        case part::ElementResolveStatus::Deleted:
            return StableIdentityStatus::Deleted;
        case part::ElementResolveStatus::Split:
            return StableIdentityStatus::Split;
        case part::ElementResolveStatus::Unresolved:
            return StableIdentityStatus::MissingEvidence;
    }
    return StableIdentityStatus::MissingEvidence;
}

}  // namespace

std::string identityStatusName(StableIdentityStatus status)
{
    switch (status) {
        case StableIdentityStatus::Stable:
            return "stable";
        case StableIdentityStatus::StableSplitFragment:
            return "stable_split_fragment";
        case StableIdentityStatus::CurrentOnly:
            return "current_only";
        case StableIdentityStatus::BodyDisplayOnly:
            return "body_display_only";
        case StableIdentityStatus::MissingEvidence:
            return "missing_evidence";
        case StableIdentityStatus::Ambiguous:
            return "ambiguous";
        case StableIdentityStatus::Split:
            return "split";
        case StableIdentityStatus::Deleted:
            return "deleted";
        case StableIdentityStatus::Unsupported:
            return "unsupported";
    }
    return "unsupported";
}

std::string localElementName(const std::string& name)
{
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::optional<TopAbs_ShapeEnum> topologicalElementKind(const std::string& name)
{
    return parseTopologicalKind(localElementName(name));
}

bool isPlainTopologicalElementName(const std::string& name)
{
    return name.find('.') == std::string::npos && topologicalElementKind(name).has_value();
}

bool isBareTopologicalSubname(const std::string& name)
{
    return isPlainTopologicalElementName(name);
}

bool stableSubnameKindMatchesIndexed(const std::string& indexed, const std::string& stableSubname)
{
    const auto indexedKind = topologicalElementKind(indexed);
    const auto stableKind = topologicalElementKind(stableSubname);
    if (!indexedKind || !stableKind) {
        return true;
    }
    return *indexedKind == *stableKind;
}

bool isCurrentOnlyTopologicalStableSubname(const std::string& indexed,
                                           const std::string& stableSubname)
{
    return !stableSubname.empty()
        && stableSubname == indexed
        && isPlainTopologicalElementName(stableSubname);
}

bool hasStableElementMapEvidence(const part::NamedShape* namedShape,
                                 const std::string& indexed,
                                 const std::string& stableSubname)
{
    if (namedShape == nullptr || stableSubname.empty()) {
        return false;
    }
    const auto mapped = namedShape->elementMap.find(stableSubname);
    if (mapped == namedShape->elementMap.end() || mapped->second != indexed) {
        return false;
    }
    if (!stableSubnameKindMatchesIndexed(indexed, stableSubname)) {
        return false;
    }
    return !isCurrentOnlyTopologicalStableSubname(indexed, stableSubname);
}

std::string stableSubnameFromNamedShape(const std::string& indexed,
                                        const part::NamedShape* namedShape)
{
    if (namedShape == nullptr) {
        return {};
    }

    std::string bestStableSubname;
    int bestPriority = -1;
    for (const auto& [stableSubname, currentSubname] : namedShape->elementMap) {
        if (currentSubname != indexed) {
            continue;
        }
        if (!hasStableElementMapEvidence(namedShape, indexed, stableSubname)) {
            continue;
        }
        const int priority = stableSubnamePriority(indexed, stableSubname);
        if (priority > bestPriority) {
            bestStableSubname = stableSubname;
            bestPriority = priority;
        }
    }
    return bestStableSubname;
}

SubshapeIdentityDecision decideDisplayPublication(const DisplayPublicationIdentityRequest& request)
{
    SubshapeIdentityDecision decision;
    decision.subname = request.subname;
    decision.fullSubname = request.fullSubname;
    decision.status = request.bodyDisplayOnly ? StableIdentityStatus::BodyDisplayOnly
                                              : StableIdentityStatus::CurrentOnly;

    if (request.candidateStableSubname.empty()
        || isCurrentOnlyTopologicalStableSubname(request.indexed, request.candidateStableSubname)) {
        return decision;
    }

    const bool candidateIsQualifiedTopological =
        request.candidateStableSubname.find('.') != std::string::npos
        && topologicalElementKind(request.candidateStableSubname).has_value();
    if ((isBareTopologicalSubname(request.candidateStableSubname) || candidateIsQualifiedTopological)
        && !hasStableElementMapEvidence(request.namedShape,
                                        request.indexed,
                                        request.candidateStableSubname)) {
        return decision;
    }

    decision.stableSubname = request.candidateStableSubname;
    decision.status = request.rawIdentityStatus == "stable_split_fragment"
        ? StableIdentityStatus::StableSplitFragment
        : StableIdentityStatus::Stable;
    return decision;
}

SubshapeIdentityDecision resolveDurableSubshapeReference(const DurableSubshapeReferenceRequest& request)
{
    SubshapeIdentityDecision decision;
    decision.subname = request.subname;
    decision.fullSubname = request.fullSubname;

    const bool fullSubnameOnlyEvidence =
        !request.fullSubname.empty()
        && (request.stableSubname.empty() || request.stableSubname == request.subname);

    auto addDiagnostic = [&](StableIdentityStatus status) {
        decision.status = status;
        const std::string code = durableReferenceDiagnosticCode(status, fullSubnameOnlyEvidence);
        if (code.empty()) {
            return;
        }
        runtime::addDiagnostic(decision.diagnostics,
                               "error",
                               code,
                               durableReferenceDiagnosticMessage(request, status, fullSubnameOnlyEvidence),
                               request.consumerObject,
                               request.property,
                               "runtime",
                               request.targetObject,
                               request.stableSubname.empty() ? request.subname : request.stableSubname);
    };

    if (request.stableSubname.empty()) {
        addDiagnostic(StableIdentityStatus::MissingEvidence);
        return decision;
    }

    if (request.namedShape == nullptr) {
        addDiagnostic(StableIdentityStatus::MissingEvidence);
        return decision;
    }

    const auto resolved =
        part::resolveElementReference(*request.namedShape, request.subname, request.stableSubname);
    const StableIdentityStatus resolvedStatus = statusForElementResolve(resolved.status);
    if (resolved.status != part::ElementResolveStatus::Resolved || !resolved.element) {
        addDiagnostic(resolvedStatus);
        return decision;
    }

    if (!hasStableElementMapEvidence(request.namedShape, *resolved.element, request.stableSubname)) {
        addDiagnostic(StableIdentityStatus::CurrentOnly);
        return decision;
    }

    decision.stableSubname = request.stableSubname;
    decision.status = StableIdentityStatus::Stable;
    return decision;
}

}  // namespace cad_core::topo
