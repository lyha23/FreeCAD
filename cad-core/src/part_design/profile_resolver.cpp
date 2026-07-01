#include "cad_core/part_design/profile_resolver.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"
#include "cad_core/part_design/body_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Builder.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <string>
#include <utility>
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

struct InternalFaceProfileCandidate {
    TopoDS_Shape shape;
    std::string subname;
    std::string stableSubname;
    bool recoveredFromReferenceShadow = false;
    bool recoveredFromShadowSub = false;
};

struct RawOpenProfileResolveAttempt {
    bool attempted = false;
    std::optional<ProfileBasedProfileSelection> selection;
};

bool isRawSketchGeometryStableSubname(const std::string& value)
{
    if (value.size() < 2U || value.front() != 'g') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char item) {
        return std::isdigit(item) != 0;
    });
}

bool linkRequestsRawOpenSketchProfile(const app::Link& profileLink)
{
    if (profileLink.stableSubnamesExplicit
        && std::any_of(profileLink.stableSubnames.begin(),
                       profileLink.stableSubnames.end(),
                       isRawSketchGeometryStableSubname)) {
        return true;
    }
    return std::any_of(profileLink.subnames.begin(), profileLink.subnames.end(), [](const std::string& subname) {
        const auto parsed = part::parseSubshapeName(subname);
        return parsed && parsed->kind == TopAbs_EDGE;
    });
}

TopoDS_Shape compoundOfShapes(const std::vector<TopoDS_Shape>& shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes) {
        if (!shape.IsNull()) {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

std::string openProfileModeName(OpenProfileMode mode)
{
    switch (mode) {
        case OpenProfileMode::Auto:
            return "Auto";
        case OpenProfileMode::Reject:
            return "Reject";
        case OpenProfileMode::SurfaceExtrusion:
            return "SurfaceExtrusion";
        case OpenProfileMode::ThinSolid:
            return "ThinSolid";
        case OpenProfileMode::ThinCut:
            return "ThinCut";
        case OpenProfileMode::SurfaceSplitCut:
            return "SurfaceSplitCut";
    }
    return "Auto";
}

void addOpenProfileDiagnostic(runtime::ComputeContext& context,
                              const app::DocumentObject& object,
                              const app::Link& profileLink,
                              const std::string& severity,
                              const std::string& code,
                              const std::string& message,
                              const std::string& subname = {})
{
    runtime::addDiagnostic(context.diagnostics,
                           severity,
                           code,
                           message,
                           object.name,
                           "Profile",
                           "runtime",
                           profileLink.object,
                           subname);
}

RawOpenProfileResolveAttempt resolveRawSketchOpenProfile(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& profileLink,
    OpenProfileMode openProfileMode,
    const std::string& featureName)
{
    RawOpenProfileResolveAttempt attempt;
    if (!linkRequestsRawOpenSketchProfile(profileLink)) {
        return attempt;
    }
    attempt.attempted = true;

    if (openProfileMode == OpenProfileMode::Reject) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "error",
                                 "open_profile",
                                 featureName + " OpenProfileMode=Reject does not accept open wire profiles",
                                 profileLink.stableSubnames.empty() ? std::string{} : profileLink.stableSubnames.front());
        return attempt;
    }

    const auto shapeIt = context.shapes.find(profileLink.object);
    if (shapeIt == context.shapes.end() || shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "error",
                                 "unsupported_open_profile_multi_target",
                                 featureName + " open wire profiles currently require a Sketcher::SketchObject target");
        return attempt;
    }

    const auto namedShapeIt = context.namedShapes.find(profileLink.object);
    const part::NamedShape* rawNamedShape = namedShapeIt == context.namedShapes.end()
        ? nullptr
        : &namedShapeIt->second;
    std::vector<std::string> requestedSubnames;
    std::vector<std::string> requestedStableSubnames;
    bool unstableOpenProfileReference = false;

    if (profileLink.stableSubnamesExplicit) {
        for (const std::string& stableSubname : profileLink.stableSubnames) {
            if (stableSubname.empty()) {
                continue;
            }
            if (!isRawSketchGeometryStableSubname(stableSubname)) {
                continue;
            }
            if (rawNamedShape == nullptr) {
                addOpenProfileDiagnostic(context,
                                         object,
                                         profileLink,
                                         "error",
                                         "ambiguous_open_profile_reference",
                                         featureName + " Profile.StableSubList requires raw sketch edge identity evidence",
                                         stableSubname);
                return attempt;
            }
            const auto resolved = part::resolveElementReference(*rawNamedShape, {}, stableSubname);
            if (resolved.status != part::ElementResolveStatus::Resolved || !resolved.element) {
                addOpenProfileDiagnostic(context,
                                         object,
                                         profileLink,
                                         "error",
                                         "ambiguous_open_profile_reference",
                                         featureName + " Profile.StableSubList cannot resolve " + stableSubname
                                             + " to a current raw sketch edge",
                                         stableSubname);
                return attempt;
            }
            requestedSubnames.push_back(*resolved.element);
            requestedStableSubnames.push_back(stableSubname);
        }
    }

    if (requestedSubnames.empty()) {
        for (const std::string& subname : profileLink.subnames) {
            const auto parsed = part::parseSubshapeName(subname);
            if (!parsed || parsed->kind != TopAbs_EDGE) {
                continue;
            }
            requestedSubnames.push_back(subname);
            const auto stableIt = std::find_if(
                profileLink.stableSubnames.begin(),
                profileLink.stableSubnames.end(),
                isRawSketchGeometryStableSubname);
            if (stableIt != profileLink.stableSubnames.end()) {
                requestedStableSubnames.push_back(*stableIt);
            }
            else {
                requestedStableSubnames.push_back(subname);
                unstableOpenProfileReference = true;
            }
        }
    }

    if (requestedSubnames.empty()) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "error",
                                 "ambiguous_open_profile_reference",
                                 featureName + " open wire Profile must reference raw sketch EdgeN or StableSubList g<ID>",
                                 profileLink.object);
        return attempt;
    }

    std::vector<TopoDS_Shape> selectedEdges;
    selectedEdges.reserve(requestedSubnames.size());
    for (const std::string& subname : requestedSubnames) {
        const auto parsed = part::parseSubshapeName(subname);
        if (!parsed || parsed->kind != TopAbs_EDGE) {
            addOpenProfileDiagnostic(context,
                                     object,
                                     profileLink,
                                     "error",
                                     "ambiguous_open_profile_reference",
                                     featureName + " open wire Profile resolved to non-edge " + subname,
                                     subname);
            return attempt;
        }

        std::optional<TopoDS_Shape> subshape;
        if (rawNamedShape != nullptr) {
            subshape = part::subshapeByName(*rawNamedShape, subname);
        }
        else {
            subshape = part::subshapeByName(shapeIt->second.shape, subname);
        }
        if (!subshape || subshape->IsNull() || subshape->ShapeType() != TopAbs_EDGE) {
            addOpenProfileDiagnostic(context,
                                     object,
                                     profileLink,
                                     "error",
                                     "ambiguous_open_profile_reference",
                                     featureName + " Profile target " + profileLink.object
                                         + " has no raw sketch edge " + subname,
                                     subname);
            return attempt;
        }
        selectedEdges.push_back(*subshape);
    }

    TopoDS_Shape profileShape;
    ProfileKind kind = ProfileKind::OpenWire;
    BRepBuilderAPI_MakeWire wireBuilder;
    for (const TopoDS_Shape& edge : selectedEdges) {
        wireBuilder.Add(TopoDS::Edge(edge));
    }
    if (wireBuilder.IsDone()) {
        profileShape = wireBuilder.Wire();
    }
    else {
        profileShape = compoundOfShapes(selectedEdges);
        kind = ProfileKind::EdgeCompound;
    }
    if (profileShape.IsNull()) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "error",
                                 "ambiguous_open_profile_reference",
                                 featureName + " open wire Profile did not produce an extrudable edge shape",
                                 profileLink.object);
        return attempt;
    }
    if (unstableOpenProfileReference) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "warning",
                                 "ambiguous_open_profile_reference",
                                 featureName + " open wire Profile.SubList uses raw EdgeN without stable g<ID> identity",
                                 requestedSubnames.front());
    }

    ProfileBasedProfileSelection selection {
        profileLink,
        profileShape,
        shapeIt->second.profileNormal,
        requestedSubnames.front(),
        requestedStableSubnames.empty() ? std::string{} : requestedStableSubnames.front(),
        !requestedStableSubnames.empty() && !unstableOpenProfileReference,
        false,
        false,
        false,
    };
    selection.kind = kind;
    selection.selectedSubnames = requestedSubnames;
    selection.selectedStableSubnames = requestedStableSubnames;
    selection.unstableOpenProfileReference = unstableOpenProfileReference;
    attempt.selection = std::move(selection);
    return attempt;
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

bool requestLocalInternalSubname(const std::string& subname)
{
    return part::parseInternalSubshapeName(subname).has_value();
}

bool requestLocalInternalFaceSubname(const std::string& subname)
{
    const auto parsed = part::parseInternalSubshapeName(subname);
    return parsed && parsed->kind == TopAbs_FACE;
}

void addUnsupportedInternalStableSubnameDiagnostic(runtime::ComputeContext& context,
                                                   const app::DocumentObject& object,
                                                   const app::Link& profileLink,
                                                   const std::string& featureName,
                                                   const std::string& stableSubname,
                                                   const std::string& reason)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_stable_subname",
                           featureName + " Profile.StableSubList cannot reference request-local "
                               + stableSubname + " without " + reason,
                           object.name,
                           "Profile",
                           "runtime",
                           profileLink.object,
                           stableSubname);
}

const part::NamedShape* sketchInternalNamedShapeEvidence(const app::Link& profileLink,
                                                         const runtime::ComputeContext& context,
                                                         const runtime::ShapeValue& shapeValue)
{
    const auto namedShapeIt = context.namedShapes.find(profileLink.object + ".InternalShape");
    if (namedShapeIt != context.namedShapes.end()) {
        return &namedShapeIt->second;
    }
    if (shapeValue.internalNamedShape) {
        return &*shapeValue.internalNamedShape;
    }
    return nullptr;
}

std::optional<std::string> resolveInternalFaceStableSubname(const app::DocumentObject& object,
                                                            runtime::ComputeContext& context,
                                                            const app::Link& profileLink,
                                                            const runtime::ShapeValue& shapeValue,
                                                            const std::string& requestedSubname,
                                                            const std::string& stableSubname,
                                                            const std::string& featureName)
{
    const part::NamedShape* internalNamedShape =
        sketchInternalNamedShapeEvidence(profileLink, context, shapeValue);
    if (internalNamedShape == nullptr
        || internalNamedShape->elements.empty()
        || internalNamedShape->elementMap.empty()
        || !shapeValue.internalShape
        || shapeValue.internalShape->IsNull()) {
        addUnsupportedInternalStableSubnameDiagnostic(
            context,
            object,
            profileLink,
            featureName,
            stableSubname,
            "Sketch.InternalShape NamedShape/ElementMap evidence");
        return std::nullopt;
    }

    const std::string subname = requestedSubname.empty() ? stableSubname : requestedSubname;
    const auto resolved = part::resolveElementReference(*internalNamedShape, subname, stableSubname);
    if (resolved.status != part::ElementResolveStatus::Resolved || !resolved.element) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               stableSubnameDiagnosticCode(resolved.status),
                               stableSubnameDiagnosticMessage("Profile", profileLink.object, stableSubname, resolved.status),
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               stableSubname);
        return std::nullopt;
    }

    if (!requestLocalInternalFaceSubname(*resolved.element)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               featureName + " Profile.StableSubList resolved to non-face " + *resolved.element,
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               *resolved.element);
        return std::nullopt;
    }
    if (!part::subshapeByName(*internalNamedShape, *resolved.element)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile target " + profileLink.object + " has no subshape "
                                   + *resolved.element,
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               *resolved.element);
        return std::nullopt;
    }
    return *resolved.element;
}

std::vector<std::string> stableNameCandidatesForProfile(const app::Link& profileLink,
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
    if (!profileLink.stableSubnames.empty()) {
        addCandidate(profileLink.stableSubnames.front());
    }
    return candidates;
}

bool internalSubshapeMatchesReferenceShadow(const runtime::ShapeValue& shapeValue,
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

std::optional<InternalFaceProfileCandidate> internalFaceFromShadowSub(const app::Link& profileLink,
                                                                      const app::ReferenceShadow& shadow,
                                                                      const runtime::ShapeValue& shapeValue)
{
    if (profileLink.shadowSubs.empty() || !shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return std::nullopt;
    }

    for (const std::string& stableName : stableNameCandidatesForProfile(profileLink, shadow)) {
        for (const auto& shadowSub : profileLink.shadowSubs) {
            if (shadowSub.newName != stableName) {
                continue;
            }
            const auto parsed = part::parseInternalSubshapeName(shadowSub.oldName);
            if (!parsed || parsed->kind != TopAbs_FACE) {
                continue;
            }
            const auto subshape = part::subshapeByName(*shapeValue.internalShape, *parsed);
            if (!subshape || subshape->IsNull()) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
            // ::PropertyLinkBase::_updateElementReference() tries ShadowSub before
            // GeoFeature::searchElementCache(). cad-core accepts that paired InternalFace only
            // after ReferenceShadow proves it still matches the old referenced geometry.
            if (internalSubshapeMatchesReferenceShadow(shapeValue, shadowSub.oldName, *subshape, shadow)) {
                return InternalFaceProfileCandidate {
                    *subshape,
                    shadowSub.oldName,
                    stableName,
                    true,
                    true,
                };
            }
        }
    }
    return std::nullopt;
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

bool hasObjectPrefix(const std::string& value)
{
    return value.find('.') != std::string::npos;
}

std::string bodyStableSubnameForProfile(const app::Link& profileLink,
                                        const std::string& subname,
                                        const std::string& stableSubname)
{
    if (stableSubname.empty()) {
        return stableSubname;
    }

    const std::string strippedStableSubname = stripObjectPrefix(stableSubname, profileLink.object);
    const std::string strippedSubname = stripObjectPrefix(subname, profileLink.object);
    if (strippedStableSubname == strippedSubname
        || hasObjectPrefix(stableSubname)
        || !part::parseSubshapeName(strippedStableSubname)) {
        return strippedStableSubname;
    }

    return profileLink.object + "." + strippedStableSubname;
}

app::Link bodyTopoShapeLink(const app::Link& profileLink)
{
    app::Link link = profileLink;
    for (auto& subname : link.subnames) {
        subname = stripObjectPrefix(subname, profileLink.object);
    }
    for (std::size_t index = 0; index < link.stableSubnames.size(); ++index) {
        const std::string subname = index < profileLink.subnames.size() ? profileLink.subnames.at(index) : std::string {};
        link.stableSubnames[index] =
            bodyStableSubnameForProfile(profileLink, subname, profileLink.stableSubnames.at(index));
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

ProfileBasedProfileSelection selectionFromInternalFaceCandidate(const app::Link& profileLink,
                                                                const runtime::ShapeValue& shapeValue,
                                                                InternalFaceProfileCandidate candidate)
{
    const bool usedStableSubname = !candidate.stableSubname.empty();
    return ProfileBasedProfileSelection {
        profileLink,
        std::move(candidate.shape),
        shapeValue.profileNormal,
        std::move(candidate.subname),
        std::move(candidate.stableSubname),
        usedStableSubname,
        candidate.recoveredFromReferenceShadow,
        candidate.recoveredFromShadowSub,
        false,
    };
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
// ::SketchObject::getElementTypes() exposes "InternalFace"; PartDesign Profile is a LinkSub
// to the sketch, so cad-core resolves Profile.SubList InternalFaceN against InternalShape.
std::optional<ProfileBasedProfileSelection> resolveSketchInternalFaceProfile(const app::DocumentObject& object,
                                                                             runtime::ComputeContext& context,
                                                                             const app::Link& profileLink,
                                                                             const runtime::ShapeValue& shapeValue,
                                                                             const std::string& featureName)
{
    if (profileLink.subnames.empty()) {
        if (profileLink.stableSubnamesExplicit) {
            std::vector<std::string> internalFaceStableSubnames;
            for (const auto& stableSubname : profileLink.stableSubnames) {
                if (requestLocalInternalFaceSubname(stableSubname)) {
                    internalFaceStableSubnames.push_back(stableSubname);
                }
                else if (requestLocalInternalSubname(stableSubname)) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_subshape_kind",
                                           featureName + " Profile.StableSubList requires an InternalFaceN subshape",
                                           object.name,
                                           "Profile",
                                           "runtime",
                                           profileLink.object,
                                           stableSubname);
                    return std::nullopt;
                }
            }
            if (internalFaceStableSubnames.size() > 1U) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "invalid_subshape",
                                       featureName + " Profile.StableSubList must select exactly one InternalFaceN",
                                       object.name,
                                       "Profile",
                                       "runtime",
                                       profileLink.object);
                return std::nullopt;
            }
            if (internalFaceStableSubnames.size() == 1U) {
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
                // ::getInternalElementMap(), key "InternalFace"; PartDesign ProfileBased
                // consumes selected sketch faces through Profile LinkSub. cad-core accepts a
                // request-local StableSubList=InternalFaceN only when this recompute already has
                // the same Sketch.InternalShape NamedShape/ElementMap evidence.
                const auto currentSubname = resolveInternalFaceStableSubname(object,
                                                                             context,
                                                                             profileLink,
                                                                             shapeValue,
                                                                             {},
                                                                             internalFaceStableSubnames.front(),
                                                                             featureName);
                if (!currentSubname) {
                    return std::nullopt;
                }
                const auto parsed = part::parseInternalSubshapeName(*currentSubname);
                if (!parsed || parsed->kind != TopAbs_FACE) {
                    return std::nullopt;
                }
                const auto subshape = part::subshapeByName(*shapeValue.internalShape, *parsed);
                if (!subshape || subshape->IsNull()) {
                    return std::nullopt;
                }
                return ProfileBasedProfileSelection {
                    profileLink,
                    *subshape,
                    shapeValue.profileNormal,
                    *currentSubname,
                    internalFaceStableSubnames.front(),
                    true,
                    false,
                    false,
                    false,
                };
            }
        }
        if (shapeValue.profileRequiresSubshapeSelection) {
            for (const auto& shadow : profileLink.referenceShadows) {
                if (!shadow.target.empty() && shadow.target != profileLink.object) {
                    continue;
                }
                const auto targetObjectIt = context.documentObjects.find(profileLink.object);
                if (targetObjectIt != context.documentObjects.end() && shadow.targetId != targetObjectIt->second->id) {
                    continue;
                }
                if (auto shadowSubShape = internalFaceFromShadowSub(profileLink, shadow, shapeValue)) {
                    return selectionFromInternalFaceCandidate(profileLink, shapeValue, std::move(*shadowSubShape));
                }
            }
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   featureName + " Profile target " + profileLink.object
                                       + " has split InternalFace regions; Profile.SubList must select one InternalFaceN",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink.object);
            return std::nullopt;
        }
        if (shapeValue.profileShape && !shapeValue.profileShape->IsNull()) {
            return ProfileBasedProfileSelection {
                profileLink,
                *shapeValue.profileShape,
                shapeValue.profileNormal,
                {},
                {},
                false,
                false,
                false,
                false,
            };
        }
        return std::nullopt;
    }

    if (profileLink.subnames.size() != 1U || profileLink.subnames.front().empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile.SubList must select exactly one InternalFaceN",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }

    const std::string& subname = profileLink.subnames.front();
    const auto parsed = part::parseInternalSubshapeName(subname);
    if (!parsed || parsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               featureName + " Profile.SubList requires an InternalFaceN subshape",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               subname);
        return std::nullopt;
    }

    std::string currentSubname = subname;
    std::string stableSubname;
    std::string nonInternalStableSubname;
    if (profileLink.stableSubnamesExplicit) {
        stableSubname = profileLink.stableSubnames.size() == 1U ? profileLink.stableSubnames.front() : std::string {};
        if (!stableSubname.empty()) {
            const bool requestLocalStableSubname = part::parseInternalSubshapeName(stableSubname).has_value();
            if (requestLocalStableSubname) {
                if (!requestLocalInternalFaceSubname(stableSubname)) {
                    runtime::addDiagnostic(context.diagnostics,
                                           "error",
                                           "unsupported_subshape_kind",
                                           featureName + " Profile.StableSubList requires an InternalFaceN subshape",
                                           object.name,
                                           "Profile",
                                           "runtime",
                                           profileLink.object,
                                           stableSubname);
                    return std::nullopt;
                }
                // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
                // ::getInternalElementMap(), key "InternalFace"; PartDesign ProfileBased
                // consumes selected sketch faces through Profile LinkSub. cad-core accepts a
                // request-local StableSubList=InternalFaceN only when this recompute already has
                // the same Sketch.InternalShape NamedShape/ElementMap evidence.
                const auto resolvedSubname = resolveInternalFaceStableSubname(object,
                                                                              context,
                                                                              profileLink,
                                                                              shapeValue,
                                                                              subname,
                                                                              stableSubname,
                                                                              featureName);
                if (!resolvedSubname) {
                    return std::nullopt;
                }
                currentSubname = *resolvedSubname;
            }
            else {
                nonInternalStableSubname = stableSubname;
            }
        }
    }

    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::getTopoShapeVerifiedFace(), throws "Cannot make face from profile" when the linked
        // sketch cannot provide a closed face. An explicit InternalFaceN selection against an empty
        // Sketch InternalShape is a profile error, not a missing object/link target.
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               featureName + " Profile target " + profileLink.object
                                   + " has no closed InternalFace profile for " + profileLink.subnames.front(),
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               profileLink.subnames.front());
        return std::nullopt;
    }

    if (!nonInternalStableSubname.empty() && profileLink.referenceShadows.empty()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_stable_subname",
                               featureName + " Profile.StableSubList cannot reference stable subname "
                                   + nonInternalStableSubname + " without ReferenceShadow evidence",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               nonInternalStableSubname);
        return std::nullopt;
    }

    const auto currentParsed = part::parseInternalSubshapeName(currentSubname);
    if (!currentParsed || currentParsed->kind != TopAbs_FACE) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               featureName + " Profile.SubList requires an InternalFaceN subshape",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               currentSubname);
        return std::nullopt;
    }

    const auto subshape = part::subshapeByName(*shapeValue.internalShape, *currentParsed);
    if (!subshape || subshape->IsNull()) {
        for (const auto& shadow : profileLink.referenceShadows) {
            if (!shadow.target.empty() && shadow.target != profileLink.object) {
                continue;
            }
            const auto targetObjectIt = context.documentObjects.find(profileLink.object);
            if (targetObjectIt != context.documentObjects.end() && shadow.targetId != targetObjectIt->second->id) {
                continue;
            }
            if (auto shadowSubShape = internalFaceFromShadowSub(profileLink, shadow, shapeValue)) {
                return selectionFromInternalFaceCandidate(profileLink, shapeValue, std::move(*shadowSubShape));
            }
            const auto recovery = part::recoverReferenceShadowSubshape(*shapeValue.internalShape, "Internal", shadow);
            if (recovery.status == part::ReferenceMatchStatus::Unique
                && recovery.shape
                && !recovery.shape->IsNull()) {
                return ProfileBasedProfileSelection {
                    profileLink,
                    *recovery.shape,
                    shapeValue.profileNormal,
                    recovery.subname,
                    shadow.stableSubname,
                    !shadow.stableSubname.empty(),
                    true,
                    false,
                    false,
                };
            }
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile target " + profileLink.object + " has no subshape " + currentSubname,
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               currentSubname);
        return std::nullopt;
    }
    for (const auto& shadow : profileLink.referenceShadows) {
        if (!shadow.target.empty() && shadow.target != profileLink.object) {
            continue;
        }
        const auto targetObjectIt = context.documentObjects.find(profileLink.object);
        if (targetObjectIt != context.documentObjects.end() && shadow.targetId != targetObjectIt->second->id) {
            continue;
        }
        if (!part::referenceFingerprintDriftReason(*subshape, shadow.fingerprint, shadow.shapeType)) {
            continue;
        }

        if (auto shadowSubShape = internalFaceFromShadowSub(profileLink, shadow, shapeValue)) {
            return selectionFromInternalFaceCandidate(profileLink, shapeValue, std::move(*shadowSubShape));
        }
        if (!shadow.brep) {
            continue;
        }
        const auto recovery = part::recoverReferenceShadowSubshape(*shapeValue.internalShape, "Internal", shadow);
        if (recovery.status == part::ReferenceMatchStatus::Unique
            && recovery.shape
            && !recovery.shape->IsNull()
            && !part::referenceFingerprintDriftReason(*recovery.shape, shadow.fingerprint, shadow.shapeType)) {
            return ProfileBasedProfileSelection {
                profileLink,
                *recovery.shape,
                shapeValue.profileNormal,
                recovery.subname,
                shadow.stableSubname,
                !shadow.stableSubname.empty(),
                true,
                false,
                false,
            };
        }
    }
    return ProfileBasedProfileSelection {
        profileLink,
        *subshape,
        shapeValue.profileNormal,
        currentSubname,
        stableSubname,
        !stableSubname.empty(),
        false,
        false,
        false,
    };
}

std::optional<gp_Dir> orientedFaceNormal(const TopoDS_Face& face)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        const double u = surface.FirstUParameter()
            + (surface.LastUParameter() - surface.FirstUParameter()) / 2.0;
        const double v = surface.FirstVParameter()
            + (surface.LastVParameter() - surface.FirstVParameter()) / 2.0;
        BRepLProp_SLProps props(surface, u, v, 2, Precision::Confusion());
        if (!props.IsNormalDefined()) {
            return std::nullopt;
        }

        gp_Pnt point;
        gp_Vec normal;
        // FreeCAD ProfileBased::getProfileNormal() uses BRepGProp_Face::Normal()
        // here so the face orientation is reflected in the returned vector.
        BRepGProp_Face(face).Normal(u, v, point, normal);
        if (normal.Magnitude() < Precision::Confusion()) {
            return std::nullopt;
        }
        return gp_Dir(normal);
    }

    gp_Dir normal = surface.Plane().Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }
    return normal;
}

std::optional<ProfileBasedProfileSelection> resolveLinkedFaceProfileSelection(
    const app::DocumentObject& object,
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

    const auto selectionFromAttempt = [&](const ResolveAttempt& attempt, bool fromBodyCumulativeReplay) {
        const std::string stableSubname =
            profileLink.stableSubnames.size() == 1U ? profileLink.stableSubnames.front() : std::string {};
        return ProfileBasedProfileSelection {
            profileLink,
            *attempt.shape,
            orientedFaceNormal(TopoDS::Face(*attempt.shape)),
            attempt.subname,
            stableSubname,
            !stableSubname.empty(),
            false,
            false,
            fromBodyCumulativeReplay,
        };
    };

    const auto namedShapeIt = context.namedShapes.find(profileLink.object);
    const part::NamedShape* namedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    ResolveAttempt direct =
        resolveFaceOnSource(object, profileLink, shapeValue.shape, namedShape, featureName);

    if (bodyTopoShapeContext) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp
        // ::Revolved::setResult(), stores the revolved tool in AddSubShape and the fused/cut body
        // state in Shape. Same-Body feature FaceN links are resolved through Body::getSubObject()
        // against the cumulative body state at the target feature, not the direct tool shape.
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
                return selectionFromAttempt(bodyShapeAttempt, true);
            }
            if (!direct.shape && direct.code == "invalid_subshape") {
                direct.message +=
                    " after Body " + bodyTopoShapeContext->body->name + " topo shape at " + profileLink.object;
            }
        }
        if (context.diagnostics.size() > diagnosticCount) {
            context.diagnostics.erase(context.diagnostics.begin() + static_cast<std::ptrdiff_t>(diagnosticCount),
                                      context.diagnostics.end());
        }
    }

    if (direct.shape) {
        return selectionFromAttempt(direct, false);
    }

    addResolveDiagnostic(context, object, profileLink, direct);
    return std::nullopt;
}

std::optional<ProfileBasedProfileSelection> resolveProfileBasedProfileLink(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& profileLink,
    const std::string& featureName,
    std::string profileRequirementMessage)
{
    if (profileRequirementMessage.empty()) {
        profileRequirementMessage = featureName + " Profile must link to a sketch profile or face";
    }
    const auto shapeIt = context.shapes.find(profileLink.object);
    if (shapeIt == context.shapes.end()
        || (shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Profile
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Solid)) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_link_target",
                               "Profile target " + profileLink.object + " did not produce a profile",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }

    std::optional<ProfileBasedProfileSelection> selection;
    if (shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch) {
        selection = resolveSketchInternalFaceProfile(object, context, profileLink, shapeIt->second, featureName);
        const bool explicitSubshape = !profileLink.subnames.empty();
        const bool ambiguousMultiFace = shapeIt->second.profileRequiresSubshapeSelection;
        if (!selection && (explicitSubshape || ambiguousMultiFace)) {
            return std::nullopt;
        }
    }
    else if (!profileLink.subnames.empty() || shapeIt->second.kind == runtime::ShapeValue::Kind::Solid) {
        selection = resolveLinkedFaceProfileSelection(object, context, profileLink, shapeIt->second, featureName);
        if (!selection) {
            return std::nullopt;
        }
    }
    else {
        selection = ProfileBasedProfileSelection {
            profileLink,
            shapeIt->second.shape,
            shapeIt->second.profileNormal,
            {},
            {},
            false,
            false,
            false,
            false,
        };
    }

    if (!selection || selection->shape.IsNull()) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "open_profile",
                               "Profile target " + profileLink.object + " did not produce a closed profile face",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }
    return selection;
}

bool isProfileSubListType(const std::string& propertyType)
{
    return propertyType == "App::PropertyLinkSubList";
}

void addInvalidProfileLinkTypeDiagnostic(runtime::ComputeContext& context,
                                         const app::DocumentObject& object,
                                         const std::string& propertyType,
                                         const std::string& featureName)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "invalid_profile_link_type",
                           featureName + " Profile must be App::PropertyLinkSubList with SubSet[]",
                           object.name,
                           "Profile",
                           "runtime",
                           {},
                           propertyType);
}

void addEmptyProfileDiagnostic(runtime::ComputeContext& context,
                               const app::DocumentObject& object,
                               const std::string& featureName,
                               const std::string& message)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "invalid_profile",
                           featureName + " " + message,
                           object.name,
                           "Profile");
}

}  // namespace

std::optional<ProfileBasedProfileSelection> resolveProfileBasedProfile(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName,
    std::string profileRequirementMessage)
{
    if (profileRequirementMessage.empty()) {
        profileRequirementMessage = featureName + " Profile must link to a sketch profile or face";
    }
    if (app::propertyValue(object, "Profile") == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               profileRequirementMessage,
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    const auto profileLink = app::readLink(object, "Profile");
    if (!profileLink) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               profileRequirementMessage,
                               object.name,
                               "Profile");
        return std::nullopt;
    }

    return resolveProfileBasedProfileLink(object, context, *profileLink, featureName, profileRequirementMessage);
}

std::vector<ProfileBasedProfileSelection> resolveProfileBasedProfileSelections(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName,
    std::string profileRequirementMessage)
{
    if (profileRequirementMessage.empty()) {
        profileRequirementMessage = featureName + " Profile must link to a sketch profile or face";
    }
    const auto* profileProperty = app::propertyValue(object, "Profile");
    if (profileProperty == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               profileRequirementMessage,
                               object.name,
                               "Profile");
        return {};
    }
    if (!isProfileSubListType(profileProperty->propertyType)) {
        addInvalidProfileLinkTypeDiagnostic(context, object, profileProperty->propertyType, featureName);
        return {};
    }
    if (!profileProperty->valid) {
        return {};
    }

    const auto profileLinks = app::readLinks(object, "Profile");
    if (profileLinks.empty()) {
        addEmptyProfileDiagnostic(context, object, featureName, "Profile.SubSet must contain at least one profile");
        return {};
    }

    std::vector<ProfileBasedProfileSelection> selections;
    selections.reserve(profileLinks.size());
    for (const auto& profileLink : profileLinks) {
        if (profileLink.subnames.empty()
            || std::any_of(profileLink.subnames.begin(), profileLink.subnames.end(), [](const std::string& subname) {
                   return subname.empty();
               })) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_profile",
                                   featureName + " Profile.SubSet[].SubList must contain at least one subname",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink.object);
            return {};
        }
        auto selection =
            resolveProfileBasedProfileLink(object, context, profileLink, featureName, profileRequirementMessage);
        if (!selection) {
            return {};
        }
        selections.push_back(std::move(*selection));
    }
    return selections;
}

std::vector<ProfileBasedProfileSelection> resolveProfileBasedProfilesForExtrusion(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName,
    OpenProfileMode openProfileMode,
    std::string profileRequirementMessage)
{
    if (profileRequirementMessage.empty()) {
        profileRequirementMessage = featureName + " Profile must link to a sketch profile or face";
    }
    const auto* profileProperty = app::propertyValue(object, "Profile");
    if (profileProperty == nullptr) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "missing_property",
                               profileRequirementMessage,
                               object.name,
                               "Profile");
        return {};
    }
    if (!isProfileSubListType(profileProperty->propertyType)) {
        addInvalidProfileLinkTypeDiagnostic(context, object, profileProperty->propertyType, featureName);
        return {};
    }
    if (!profileProperty->valid) {
        return {};
    }

    const auto profileLinks = app::readLinks(object, "Profile");
    if (profileLinks.empty()) {
        addEmptyProfileDiagnostic(context, object, featureName, "Profile.SubSet must contain at least one profile");
        return {};
    }

    std::vector<ProfileBasedProfileSelection> selections;
    selections.reserve(profileLinks.size());
    for (const auto& profileLink : profileLinks) {
        RawOpenProfileResolveAttempt openAttempt =
            resolveRawSketchOpenProfile(object, context, profileLink, openProfileMode, featureName);
        if (openAttempt.attempted) {
            if (!openAttempt.selection) {
                return {};
            }
            selections.push_back(std::move(*openAttempt.selection));
            continue;
        }

        if (profileLink.subnames.empty()
            || std::any_of(profileLink.subnames.begin(), profileLink.subnames.end(), [](const std::string& subname) {
                   return subname.empty();
               })) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_profile",
                                   featureName + " Profile.SubSet[].SubList must contain at least one subname",
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink.object);
            return {};
        }
        auto selection =
            resolveProfileBasedProfileLink(object, context, profileLink, featureName, profileRequirementMessage);
        if (!selection) {
            return {};
        }
        selections.push_back(std::move(*selection));
    }

    const bool hasOpenProfile =
        std::any_of(selections.begin(), selections.end(), [](const ProfileBasedProfileSelection& selection) {
            return selection.kind == ProfileKind::OpenWire || selection.kind == ProfileKind::EdgeCompound;
        });
    if (hasOpenProfile) {
        std::set<std::string> openOwners;
        for (const auto& selection : selections) {
            if (selection.kind == ProfileKind::ClosedFace) {
                runtime::addDiagnostic(context.diagnostics,
                                       "error",
                                       "unsupported_open_profile_multi_target",
                                       featureName + " Profile cannot mix closed faces and open wire selections",
                                       object.name,
                                       "Profile");
                return {};
            }
            openOwners.insert(selection.link.object);
        }
        if (openOwners.size() > 1U) {
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "unsupported_open_profile_multi_target",
                                   featureName + " open wire Profile.SubSet currently requires one sketch target",
                                   object.name,
                                   "Profile");
            return {};
        }
    }

    return selections;
}

std::optional<TopoDS_Shape> resolveLinkedFaceProfile(const app::DocumentObject& object,
                                                     runtime::ComputeContext& context,
                                                     const app::Link& profileLink,
                                                     const runtime::ShapeValue& shapeValue,
                                                     const std::string& featureName)
{
    const auto selection = resolveLinkedFaceProfileSelection(object, context, profileLink, shapeValue, featureName);
    if (!selection) {
        return std::nullopt;
    }
    return selection->shape;
}

}  // namespace cad_core::part_design
