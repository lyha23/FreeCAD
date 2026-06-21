#include "cad_core/part_design/profile_resolver.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part_design/body_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <TopAbs_ShapeEnum.hxx>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::part_design {

namespace {

struct ResolveAttempt {
    std::optional<TopoDS_Shape> shape;
    std::string code;
    std::string message;
    std::string subname;
};

struct BodyTopoShapeProfileContext {
    const app::DocumentObject* body = nullptr;
    std::size_t targetIndex = 0U;
    std::size_t currentIndex = 0U;
    bool invalidOrder = false;
};

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

std::string stableSubnameDiagnosticMessage(const std::string& property,
                                           const std::string& target,
                                           const std::string& stableSubname,
                                           part::ElementResolveStatus status)
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

std::vector<std::string> readBodyGroupNames(const app::DocumentObject& body)
{
    std::vector<std::string> result;
    for (const auto& link : app::readLinks(body, "Group")) {
        result.push_back(link.object);
    }
    return result;
}

std::optional<std::size_t> indexOf(const std::vector<std::string>& values, const std::string& value)
{
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(values.begin(), it));
}

std::optional<BodyTopoShapeProfileContext> bodyTopoShapeContextForProfile(const app::DocumentObject& object,
                                                                          const app::Link& profileLink,
                                                                          const runtime::ComputeContext& context)
{
    // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp
    // ::Feature::getSubObject(), routes Body-local "Feature.FaceN" paths through
    // PartDesign::Body::findBodyOf(this) and Body.Group.findUsingMap(...). cad-core uses the same
    // same-Body boundary before reconstructing a cumulative feature Shape.
    const auto currentParentIt = context.parentGroupByObject.find(object.name);
    if (currentParentIt == context.parentGroupByObject.end()) {
        return std::nullopt;
    }
    const auto targetParentIt = context.parentGroupByObject.find(profileLink.object);
    if (targetParentIt == context.parentGroupByObject.end()
        || targetParentIt->second != currentParentIt->second) {
        return std::nullopt;
    }
    const auto bodyIt = context.documentObjects.find(currentParentIt->second);
    if (bodyIt == context.documentObjects.end() || bodyIt->second == nullptr
        || bodyIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }

    const std::vector<std::string> groupNames = readBodyGroupNames(*bodyIt->second);
    const auto targetIndex = indexOf(groupNames, profileLink.object);
    const auto currentIndex = indexOf(groupNames, object.name);
    if (!targetIndex || !currentIndex) {
        return std::nullopt;
    }
    return BodyTopoShapeProfileContext {
        bodyIt->second,
        *targetIndex,
        *currentIndex,
        *targetIndex >= *currentIndex,
    };
}

std::string stripObjectPrefix(const std::string& value, const std::string& objectName)
{
    const std::string prefix = objectName + ".";
    if (value.rfind(prefix, 0U) == 0U) {
        return value.substr(prefix.size());
    }
    return value;
}

app::Link bodyTopoShapeLink(const app::Link& profileLink)
{
    app::Link link = profileLink;
    for (auto& subname : link.subnames) {
        subname = stripObjectPrefix(subname, profileLink.object);
    }
    for (auto& stableSubname : link.stableSubnames) {
        stableSubname = stripObjectPrefix(stableSubname, profileLink.object);
    }
    return link;
}

ResolveAttempt resolveFaceOnSource(const app::DocumentObject& object,
                                   const app::Link& profileLink,
                                   const TopoDS_Shape& sourceShape,
                                   const part::NamedShape* namedShape,
                                   const std::string& featureName)
{
    if (profileLink.subnames.size() != 1U || profileLink.subnames.front().empty()) {
        return {
            std::nullopt,
            "invalid_subshape",
            featureName + " Profile must reference exactly one FaceN subshape",
            {},
        };
    }

    const std::string& subname = profileLink.subnames.front();
    const std::string stableSubname =
        profileLink.stableSubnames.size() == 1U ? profileLink.stableSubnames.front() : std::string {};
    std::string currentSubname = subname;
    if (namedShape != nullptr) {
        const auto resolved = part::resolveElementReference(*namedShape, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != subname) {
            return {
                std::nullopt,
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage("Profile", profileLink.object, stableSubname, resolved.status),
                stableSubname,
            };
        }
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        return {
            std::nullopt,
            "invalid_subshape",
            "Invalid Profile subshape name " + currentSubname,
            currentSubname,
        };
    }
    if (parsed->kind != TopAbs_FACE) {
        return {
            std::nullopt,
            "unsupported_subshape_kind",
            featureName + " Profile requires a face subshape, not " + part::subshapeKindName(parsed->kind),
            currentSubname,
        };
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShape != nullptr) {
        subshape = part::subshapeByName(*namedShape, currentSubname);
    }
    else {
        subshape = part::subshapeByName(sourceShape, currentSubname);
    }
    if (!subshape || subshape->IsNull()) {
        return {
            std::nullopt,
            "invalid_subshape",
            featureName + " Profile target " + profileLink.object + " has no subshape " + currentSubname,
            currentSubname,
        };
    }
    if (subshape->ShapeType() != TopAbs_FACE) {
        return {
            std::nullopt,
            "unsupported_subshape_kind",
            featureName + " Profile resolved to " + part::subshapeKindName(subshape->ShapeType()) + ", not a face",
            currentSubname,
        };
    }

    return {*subshape, {}, {}, currentSubname};
}

void addResolveDiagnostic(runtime::ComputeContext& context,
                          const app::DocumentObject& object,
                          const app::Link& profileLink,
                          const ResolveAttempt& attempt)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           attempt.code.empty() ? "invalid_subshape" : attempt.code,
                           attempt.message,
                           object.name,
                           "Profile",
                           "runtime",
                           profileLink.object,
                           attempt.subname);
}

}  // namespace

std::optional<TopoDS_Shape> resolveLinkedFaceProfile(const app::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const app::Link& profileLink,
                                                     const runtime::ShapeValue& shapeValue,
                                                     const std::string& featureName)
{
    const auto bodyTopoShapeContext = bodyTopoShapeContextForProfile(object, profileLink, context);
    if (bodyTopoShapeContext && bodyTopoShapeContext->invalidOrder) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_body_profile_reference",
                               featureName + " Profile target " + profileLink.object
                                   + " is not before " + object.name + " in Body " + bodyTopoShapeContext->body->name,
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }

    const auto namedShapeIt = context.namedShapes.find(profileLink.object);
    const part::NamedShape* namedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    ResolveAttempt direct =
        resolveFaceOnSource(object, profileLink, shapeValue.shape, namedShape, featureName);
    if (direct.shape) {
        return direct.shape;
    }

    if (bodyTopoShapeContext && direct.code == "invalid_subshape") {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
        // ::Revolved::setResult(), stores the revolved tool in AddSubShape and the fused/cut body
        // state in Shape. When cad-core's direct feature output is still the tool shape, reconstruct
        // the owning Body state at that feature to recover the FreeCAD-visible cumulative Shape.
        const BodyTopoShapeOptions options {
            false,
            false,
        };
        const std::size_t diagnosticCount = context.diagnostics.size();
        const auto bodyTopoShape =
            getBodyTopoShapeAtFeature(*bodyTopoShapeContext->body, context, profileLink.object, options);
        if (bodyTopoShape) {
            const part::NamedShape bodyNamedShape =
                bodyTopoShape->namedShape.value_or(
                    part::indexedNamedShapeForObject(bodyTopoShapeContext->body->name, bodyTopoShape->shape));
            const app::Link bodyShapeLink = bodyTopoShapeLink(profileLink);
            ResolveAttempt bodyShapeAttempt =
                resolveFaceOnSource(object, bodyShapeLink, bodyTopoShape->shape, &bodyNamedShape, featureName);
            if (bodyShapeAttempt.shape) {
                return bodyShapeAttempt.shape;
            }
            direct.message +=
                " after Body " + bodyTopoShapeContext->body->name + " topo shape at " + profileLink.object;
        }
        if (context.diagnostics.size() > diagnosticCount) {
            context.diagnostics.erase(context.diagnostics.begin() + static_cast<std::ptrdiff_t>(diagnosticCount),
                                      context.diagnostics.end());
        }
    }

    addResolveDiagnostic(context, object, profileLink, direct);
    return std::nullopt;
}

}  // namespace cad_core::part_design
