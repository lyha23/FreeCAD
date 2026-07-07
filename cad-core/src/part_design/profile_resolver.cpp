#include "cad_core/part_design/profile_resolver.h"

#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"
#include "cad_core/part_design/body_topo_shape.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/topo/subshape_identity.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Builder.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
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
    std::vector<std::string> subnames;
    std::vector<std::string> stableSubnames;
    std::vector<bool> downgradedStableSubnames;
    ProfileKind kind = ProfileKind::ClosedFace;
    bool unstableOpenProfileReference = false;
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

bool isInternalEdgeSubname(const std::string& value)
{
    const auto parsed = part::parseInternalSubshapeName(value);
    return parsed && parsed->kind == TopAbs_EDGE;
}

bool openProfileModeAcceptsInternalEdgeAlias(OpenProfileMode mode)
{
    return mode == OpenProfileMode::SurfaceExtrusion
        || mode == OpenProfileMode::ThinSolid
        || mode == OpenProfileMode::ThinCut
        || mode == OpenProfileMode::SurfaceSplitCut;
}

bool linkRequestsRawOpenSketchProfile(const app::Link& profileLink, OpenProfileMode openProfileMode)
{
    if (profileLink.stableSubnamesExplicit
        && std::any_of(profileLink.stableSubnames.begin(),
                       profileLink.stableSubnames.end(),
                       isRawSketchGeometryStableSubname)) {
        return true;
    }
    const bool acceptsInternalEdgeAlias = openProfileModeAcceptsInternalEdgeAlias(openProfileMode);
    return std::any_of(profileLink.subnames.begin(), profileLink.subnames.end(), [](const std::string& subname) {
        const auto parsed = part::parseSubshapeName(subname);
        return parsed && parsed->kind == TopAbs_EDGE;
    }) || (acceptsInternalEdgeAlias
           && std::any_of(profileLink.subnames.begin(),
                          profileLink.subnames.end(),
                          isInternalEdgeSubname));
}

bool linkHasExplicitStableSubshapeReference(const app::Link& profileLink)
{
    return profileLink.stableSubnamesExplicit
        && std::any_of(profileLink.stableSubnames.begin(),
                       profileLink.stableSubnames.end(),
                       [](const std::string& stableSubname) {
                           return !stableSubname.empty();
                       });
}

bool linkTargetsSketchProfile(const runtime::ComputeContext& context, const app::Link& profileLink)
{
    const auto shapeIt = context.shapes.find(profileLink.object);
    return shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch;
}

bool objectPublishesRawSketchEdgeIdentity(const runtime::ComputeContext& context,
                                          const std::string& objectName)
{
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return false;
    }
    const auto identityIt = objectIt->second.find("raw_edge_identity");
    return identityIt != objectIt->second.end() && identityIt->is_object();
}

std::optional<std::string> rawSketchCurrentEdgeForStable(
    const runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& stableSubname)
{
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
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
    return indexedIt->get<std::string>();
}

std::optional<std::string> rawSketchCurrentEdgeForInternalEdge(
    const runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& internalSubname)
{
    if (!isInternalEdgeSubname(internalSubname)) {
        return std::nullopt;
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return std::nullopt;
    }
    const auto mappedIt = mapIt->find(internalSubname);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return std::nullopt;
    }
    const std::string currentRawEdge = mappedIt->get<std::string>();
    const auto parsed = part::parseSubshapeName(currentRawEdge);
    if (!parsed || parsed->kind != TopAbs_EDGE) {
        return std::nullopt;
    }
    return currentRawEdge;
}

std::optional<std::string> rawSketchStableSubnameForCurrentEdge(
    const runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& indexedSubname)
{
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return std::nullopt;
    }
    const auto identityIt = objectIt->second.find("raw_edge_identity");
    if (identityIt == objectIt->second.end() || !identityIt->is_object()) {
        return std::nullopt;
    }
    const auto byIndexedIt = identityIt->find("byIndexed");
    if (byIndexedIt == identityIt->end() || !byIndexedIt->is_object()) {
        return std::nullopt;
    }
    const auto indexedIt = byIndexedIt->find(indexedSubname);
    if (indexedIt == byIndexedIt->end() || !indexedIt->is_object()) {
        return std::nullopt;
    }
    const auto stableIt = indexedIt->find("sourceStableSubname");
    if (stableIt == indexedIt->end() || !stableIt->is_string()) {
        return std::nullopt;
    }
    const std::string stableSubname = stableIt->get<std::string>();
    if (!isRawSketchGeometryStableSubname(stableSubname)) {
        return std::nullopt;
    }
    return stableSubname;
}

bool qualifiedSubnameHasKind(const std::string& value, TopAbs_ShapeEnum kind)
{
    const auto parsed = part::parseSubshapeName(value);
    if (parsed) {
        return parsed->kind == kind;
    }
    const std::size_t dot = value.find('.');
    if (dot == std::string::npos || dot + 1U >= value.size()) {
        return false;
    }
    const auto qualified = part::parseSubshapeName(value.substr(dot + 1U));
    return qualified && qualified->kind == kind;
}

bool linkRequestsCurrentEdgeSubshape(const app::Link& profileLink)
{
    return std::any_of(profileLink.subnames.begin(), profileLink.subnames.end(), [](const std::string& subname) {
        return qualifiedSubnameHasKind(subname, TopAbs_EDGE);
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
    if (!linkRequestsRawOpenSketchProfile(profileLink, openProfileMode)) {
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
                addOpenProfileDiagnostic(context,
                                         object,
                                         profileLink,
                                         "error",
                                         "unsupported_stable_subname",
                                         featureName + " open wire Profile.StableSubList must use g<ID> raw sketch identity",
                                         stableSubname);
                return attempt;
            }
            if (objectPublishesRawSketchEdgeIdentity(context, profileLink.object)) {
                const auto indexed = rawSketchCurrentEdgeForStable(context, profileLink.object, stableSubname);
                if (!indexed) {
                    addOpenProfileDiagnostic(context,
                                             object,
                                             profileLink,
                                             "error",
                                             "unsupported_stable_subname",
                                             featureName + " Profile.StableSubList cannot resolve " + stableSubname
                                                 + " to a current raw sketch edge",
                                             stableSubname);
                    return attempt;
                }
                requestedSubnames.push_back(*indexed);
                requestedStableSubnames.push_back(stableSubname);
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
        for (std::size_t index = 0; index < profileLink.subnames.size(); ++index) {
            const std::string& subname = profileLink.subnames.at(index);
            const auto parsed = part::parseSubshapeName(subname);
            std::optional<std::string> requestedSubname;
            if (parsed && parsed->kind == TopAbs_EDGE) {
                requestedSubname = subname;
            }
            else if (openProfileModeAcceptsInternalEdgeAlias(openProfileMode) && isInternalEdgeSubname(subname)) {
                requestedSubname = rawSketchCurrentEdgeForInternalEdge(context, profileLink.object, subname);
                if (!requestedSubname) {
                    addOpenProfileDiagnostic(context,
                                             object,
                                             profileLink,
                                             "error",
                                             "ambiguous_open_profile_reference",
                                             featureName + " open wire Profile.SubList cannot map " + subname
                                                 + " to a raw sketch EdgeN",
                                             subname);
                    return attempt;
                }
            }
            else {
                continue;
            }

            requestedSubnames.push_back(*requestedSubname);
            const std::string explicitStableSubname = index < profileLink.stableSubnames.size()
                ? profileLink.stableSubnames.at(index)
                : std::string {};
            if (isRawSketchGeometryStableSubname(explicitStableSubname)) {
                requestedStableSubnames.push_back(explicitStableSubname);
            }
            else if (const auto stableSubname =
                         rawSketchStableSubnameForCurrentEdge(context, profileLink.object, *requestedSubname)) {
                requestedStableSubnames.push_back(*stableSubname);
            }
            else {
                requestedStableSubnames.push_back(*requestedSubname);
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
                                 featureName
                                     + " open wire Profile must reference raw sketch EdgeN, InternalEdgeN, or StableSubList g<ID>",
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
        if (!subshape) {
            subshape = part::subshapeByName(shapeIt->second.shape, subname);
        }
        if (!subshape || subshape->IsNull() || subshape->ShapeType() != TopAbs_EDGE) {
            const std::size_t requestIndex = selectedEdges.size();
            const std::string stableSubname = requestIndex < requestedStableSubnames.size()
                ? requestedStableSubnames.at(requestIndex)
                : std::string {};
            const bool stableIdentityRequested = isRawSketchGeometryStableSubname(stableSubname)
                && !unstableOpenProfileReference;
            addOpenProfileDiagnostic(context,
                                     object,
                                     profileLink,
                                     "error",
                                     stableIdentityRequested
                                         ? "unsupported_stable_subname"
                                         : "ambiguous_open_profile_reference",
                                     stableIdentityRequested
                                         ? featureName + " Profile.StableSubList resolved " + stableSubname
                                             + " to " + subname + ", but the current raw sketch edge is missing"
                                         : featureName + " Profile target " + profileLink.object
                                             + " has no raw sketch edge " + subname,
                                     stableIdentityRequested ? stableSubname : subname);
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

void addLegacyInternalFaceSubListDiagnostic(runtime::ComputeContext& context,
                                            const app::DocumentObject& object,
                                            const app::Link& profileLink,
                                            const std::string& featureName,
                                            const std::string& subname)
{
    runtime::addDiagnostic(context.diagnostics,
                           "error",
                           "unsupported_legacy_internal_sublist",
                           featureName + " Profile.SubList must not persist request-local "
                               + (subname.empty() ? std::string("InternalFaceN") : subname)
                               + "; use StableSubList with the backend InternalFace stable alias",
                           object.name,
                           "Profile",
                           "runtime",
                           profileLink.object,
                           subname);
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

bool isBareTopologicalSubname(const std::string& value)
{
    return !hasObjectPrefix(value) && part::parseSubshapeName(value).has_value();
}

std::optional<std::string> ownerPrefixFromQualifiedSubshape(const std::string& value)
{
    const std::size_t dot = value.find('.');
    if (dot == std::string::npos || dot == 0U || dot + 1U >= value.size()) {
        return std::nullopt;
    }
    if (!part::parseSubshapeName(value.substr(dot + 1U))) {
        return std::nullopt;
    }
    return value.substr(0U, dot);
}

std::optional<std::string> qualifiedProfileOwner(const app::Link& profileLink)
{
    std::optional<std::string> owner;
    const auto consider = [&](const std::string& value) {
        const auto candidate = ownerPrefixFromQualifiedSubshape(value);
        if (!candidate) {
            return true;
        }
        if (!owner) {
            owner = *candidate;
            return true;
        }
        return *owner == *candidate;
    };

    for (const std::string& subname : profileLink.subnames) {
        if (!consider(subname)) {
            return std::nullopt;
        }
    }
    for (const std::string& fullSubname : profileLink.fullSubnames) {
        if (!consider(fullSubname)) {
            return std::nullopt;
        }
    }
    return owner;
}

std::optional<app::Link> normalizedBodyQualifiedProfileLink(const app::Link& profileLink,
                                                            const runtime::ComputeContext& context)
{
    const auto objectIt = context.documentObjects.find(profileLink.object);
    if (objectIt == context.documentObjects.end()
        || objectIt->second == nullptr
        || objectIt->second->typeId != "PartDesign::Body") {
        return std::nullopt;
    }
    const auto owner = qualifiedProfileOwner(profileLink);
    if (!owner || context.documentObjects.count(*owner) == 0U) {
        return std::nullopt;
    }

    app::Link link = profileLink;
    link.object = *owner;
    for (std::string& subname : link.subnames) {
        subname = stripObjectPrefix(subname, *owner);
    }
    for (std::string& stableSubname : link.stableSubnames) {
        stableSubname = stripObjectPrefix(stableSubname, *owner);
    }
    for (std::string& fullSubname : link.fullSubnames) {
        fullSubname = stripObjectPrefix(fullSubname, *owner);
    }
    return link;
}

bool profileTargetIsDisplayOnly(const runtime::ComputeContext& context, const std::string& objectName)
{
    const auto objectIt = context.objects.find(objectName);
    return objectIt != context.objects.end()
        && objectIt->second.is_object()
        && objectIt->second.value("bodyParticipation", "") == "display_only";
}

bool linkHasExplicitBodyReplayEvidence(const app::Link& profileLink)
{
    if (!profileLink.fullSubnames.empty()) {
        return true;
    }
    const auto hasQualifiedSubshape = [](const std::vector<std::string>& values) {
        return std::any_of(values.begin(), values.end(), [](const std::string& value) {
            return ownerPrefixFromQualifiedSubshape(value).has_value();
        });
    };
    return hasQualifiedSubshape(profileLink.subnames)
        || hasQualifiedSubshape(profileLink.stableSubnames);
}

bool targetUsesFeatureLocalShapeBoundary(const runtime::ComputeContext& context,
                                         const std::string& objectName)
{
    const auto objectIt = context.documentObjects.find(objectName);
    return objectIt != context.documentObjects.end()
        && objectIt->second != nullptr
        && objectIt->second->typeId == "PartDesign::Revolution";
}

std::optional<std::string> bodyDisplayFullSubnameForTarget(
    const app::Link& profileLink,
    const BodyTopoShapeProfileContext& bodyTopoShapeContext)
{
    const std::string prefix = bodyTopoShapeContext.body->name + "." + profileLink.object + ".";
    for (const std::string& fullSubname : profileLink.fullSubnames) {
        std::string localSubname;
        if (fullSubname.rfind(prefix, 0U) == 0U) {
            localSubname = fullSubname.substr(prefix.size());
        }
        else {
            const std::size_t ownerDot = fullSubname.find('.');
            if (ownerDot == std::string::npos || ownerDot + 1U >= fullSubname.size()) {
                continue;
            }
            const std::string featurePrefix = profileLink.object + ".";
            const std::string featurePath = fullSubname.substr(ownerDot + 1U);
            if (featurePath.rfind(featurePrefix, 0U) != 0U) {
                continue;
            }
            localSubname = featurePath.substr(featurePrefix.size());
        }
        if (!part::parseSubshapeName(localSubname)) {
            continue;
        }
        return fullSubname;
    }
    return std::nullopt;
}

bool linkHasStrongSubshapeEvidenceAt(const app::Link& profileLink, std::size_t index)
{
    (void)profileLink;
    (void)index;
    return false;
}

std::string stableSubnameForElementReference(const app::Link& profileLink,
                                             std::size_t index,
                                             const std::string& subname,
                                             const std::string& stableSubname,
                                             bool currentSubnameIsResolvable)
{
    if (stableSubname.empty()) {
        return {};
    }

    const std::string strippedSubname = stripObjectPrefix(subname, profileLink.object);
    const std::string strippedStableSubname = stripObjectPrefix(stableSubname, profileLink.object);
    if (strippedStableSubname == strippedSubname) {
        return strippedStableSubname;
    }
    if (!isBareTopologicalSubname(strippedStableSubname)
        || !isBareTopologicalSubname(strippedSubname)
        || !currentSubnameIsResolvable
        || !profileLink.fullSubnamesExplicit
        || linkHasStrongSubshapeEvidenceAt(profileLink, index)) {
        return stableSubname;
    }

    return {};
}

std::string bodyStableSubnameForProfile(const app::Link& profileLink,
                                        std::size_t index,
                                        const std::string& subname,
                                        const std::string& stableSubname,
                                        bool downgradeWeakStableSubname)
{
    if (downgradeWeakStableSubname) {
        return {};
    }
    const std::string elementStableSubname =
        stableSubnameForElementReference(profileLink, index, subname, stableSubname, false);
    if (elementStableSubname.empty()) {
        return {};
    }

    const std::string strippedStableSubname = stripObjectPrefix(elementStableSubname, profileLink.object);
    const std::string strippedSubname = stripObjectPrefix(subname, profileLink.object);
    if (strippedStableSubname == strippedSubname
        || hasObjectPrefix(elementStableSubname)
        || !part::parseSubshapeName(strippedStableSubname)) {
        return strippedStableSubname;
    }

    return profileLink.object + "." + strippedStableSubname;
}

app::Link bodyTopoShapeLink(const app::Link& profileLink,
                            const std::vector<bool>& downgradedStableSubnames = {})
{
    app::Link link = profileLink;
    for (auto& subname : link.subnames) {
        subname = stripObjectPrefix(subname, profileLink.object);
    }
    for (std::size_t index = 0; index < link.stableSubnames.size(); ++index) {
        const std::string subname = index < profileLink.subnames.size() ? profileLink.subnames.at(index) : std::string {};
        const bool downgradeWeakStableSubname =
            index < downgradedStableSubnames.size() && downgradedStableSubnames.at(index);
        link.stableSubnames[index] =
            bodyStableSubnameForProfile(
                profileLink,
                index,
                subname,
                profileLink.stableSubnames.at(index),
                downgradeWeakStableSubname);
    }
    return link;
}

ResolveAttempt resolveAttemptFromIdentityDecision(const topo::SubshapeIdentityDecision& decision);

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
    const std::string rawStableSubname =
        profileLink.stableSubnames.size() == 1U ? profileLink.stableSubnames.front() : std::string {};
    const std::string rawFullSubname =
        profileLink.fullSubnames.size() == 1U ? profileLink.fullSubnames.front() : std::string {};
    const topo::SubshapeIdentityDecision identity =
        topo::resolveDurableSubshapeReference({
            object.name,
            "Profile",
            profileLink.object,
            subname,
            rawStableSubname,
            rawFullSubname,
            namedShape,
            profileLink.stableSubnamesExplicit,
        });
    if (!identity.diagnostics.empty()) {
        return resolveAttemptFromIdentityDecision(identity);
    }
    const std::string stableSubname = identity.stableSubname;
    const bool downgradedStableSubname = !rawStableSubname.empty() && stableSubname.empty();
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

    return {
        *subshape,
        {},
        {},
        currentSubname,
        {},
        stableSubname.empty() ? std::vector<std::string> {} : std::vector<std::string> {stableSubname},
        downgradedStableSubname ? std::vector<bool> {true} : std::vector<bool> {},
        ProfileKind::ClosedFace,
        false,
    };
}

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.h
// ::ProfileBased::Profile is "App::PropertyLinkSub", and FeatureSketchBased.cpp
// ::ProfileBased::getVerifiedObject() accepts "Part::Feature"; ::getProfileShape() calls
// "Part::Feature::getTopoShape(profile, subShapeOptions, sub.c_str())". FeatureExtrude.cpp
// ::FeatureExtrude::buildExtrusion(), makeface=false path reads "Profile.getSubValues(false)",
// calls getTopoShape(... "NeedSubElement | ResolveLink | Transform"), and if shapes contain
// edges calls "sketchshape.makeElementWires(shapes)". Part/App/PartFeature.cpp::_getTopoShape()
// resolves getSubObject()/links/subname ownership, and FeatureExtrusion.cpp::extrudeShape() only
// makes faces when "params.solid"; otherwise it extrudes the edge/wire with makeElementPrism().
std::optional<TopoDS_Shape> recoverOpenProfileEdgeFromReferenceShadow(const app::Link& profileLink,
                                                                       const TopoDS_Shape& sourceShape,
                                                                       const std::string& currentSubname,
                                                                       const std::string& stableSubname)
{
    for (const auto& shadow : profileLink.referenceShadows) {
        if (!shadow.target.empty() && shadow.target != profileLink.object) {
            continue;
        }
        if (!shadow.shapeType.empty() && shadow.shapeType != "Edge") {
            continue;
        }
        if (!shadow.subname.empty() && stableSubname.empty() && shadow.subname != currentSubname) {
            continue;
        }
        if (!shadow.stableSubname.empty() && !stableSubname.empty() && shadow.stableSubname != stableSubname) {
            continue;
        }
        const auto recovery = part::recoverReferenceShadowSubshape(sourceShape, {}, shadow);
        if (recovery.status == part::ReferenceMatchStatus::Unique
            && recovery.shape
            && !recovery.shape->IsNull()
            && recovery.shape->ShapeType() == TopAbs_EDGE) {
            return *recovery.shape;
        }
    }
    return std::nullopt;
}

ResolveAttempt resolveEdgesOnSource(const app::DocumentObject& object,
                                    const app::Link& profileLink,
                                    const TopoDS_Shape& sourceShape,
                                    const part::NamedShape* namedShape,
                                    const std::string& featureName)
{
    (void)object;
    if (profileLink.subnames.empty()) {
        return {
            std::nullopt,
            "invalid_subshape",
            featureName + " Profile must reference at least one EdgeN subshape",
            {},
        };
    }

    std::vector<TopoDS_Shape> selectedEdges;
    std::vector<std::string> selectedSubnames;
    std::vector<std::string> selectedStableSubnames;
    std::vector<bool> downgradedStableSubnames;
    bool unstableOpenProfileReference = false;

    for (std::size_t index = 0; index < profileLink.subnames.size(); ++index) {
        const std::string subname = stripObjectPrefix(profileLink.subnames.at(index), profileLink.object);
        const std::string rawStableSubname = index < profileLink.stableSubnames.size()
            ? profileLink.stableSubnames.at(index)
            : std::string {};
        const std::string rawFullSubname = index < profileLink.fullSubnames.size()
            ? profileLink.fullSubnames.at(index)
            : std::string {};
        const topo::SubshapeIdentityDecision identity =
            topo::resolveDurableSubshapeReference({
                object.name,
                "Profile",
                profileLink.object,
                subname,
                rawStableSubname,
                rawFullSubname,
                namedShape,
                profileLink.stableSubnamesExplicit,
            });
        if (!identity.diagnostics.empty()) {
            return resolveAttemptFromIdentityDecision(identity);
        }
        const std::string stableSubname = identity.stableSubname;

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
        if (parsed->kind != TopAbs_EDGE) {
            return {
                std::nullopt,
                "unsupported_subshape_kind",
                featureName + " Profile requires an edge subshape, not " + part::subshapeKindName(parsed->kind),
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
        if ((!subshape || subshape->IsNull()) && !profileLink.referenceShadows.empty()) {
            subshape = recoverOpenProfileEdgeFromReferenceShadow(profileLink,
                                                                 sourceShape,
                                                                 currentSubname,
                                                                 stableSubname);
        }
        if (!subshape || subshape->IsNull()) {
            return {
                std::nullopt,
                "invalid_subshape",
                featureName + " Profile target " + profileLink.object + " has no subshape " + currentSubname,
                currentSubname,
            };
        }
        if (subshape->ShapeType() != TopAbs_EDGE) {
            return {
                std::nullopt,
                "unsupported_subshape_kind",
                featureName + " Profile resolved to " + part::subshapeKindName(subshape->ShapeType()) + ", not an edge",
                currentSubname,
            };
        }

        selectedEdges.push_back(*subshape);
        selectedSubnames.push_back(currentSubname);
        if (!stableSubname.empty()) {
            selectedStableSubnames.push_back(stableSubname);
        }
        else {
            selectedStableSubnames.push_back(currentSubname);
        }
        if (!profileLink.stableSubnamesExplicit
            || stableSubname.empty()
            || stableSubname == subname
            || qualifiedSubnameHasKind(stableSubname, TopAbs_EDGE)) {
            unstableOpenProfileReference = true;
        }
        downgradedStableSubnames.push_back(!rawStableSubname.empty() && stableSubname.empty());
    }

    TopoDS_Shape profileShape;
    ProfileKind kind = ProfileKind::OpenWire;
    BRepBuilderAPI_MakeWire wireBuilder;
    try {
        for (const TopoDS_Shape& edge : selectedEdges) {
            wireBuilder.Add(TopoDS::Edge(edge));
        }
    }
    catch (const Standard_Failure&) {
        profileShape = compoundOfShapes(selectedEdges);
        kind = ProfileKind::EdgeCompound;
    }
    if (profileShape.IsNull()) {
        if (wireBuilder.IsDone() && !wireBuilder.Wire().IsNull()) {
            profileShape = wireBuilder.Wire();
        }
        else {
            profileShape = compoundOfShapes(selectedEdges);
            kind = ProfileKind::EdgeCompound;
        }
    }
    if (profileShape.IsNull()) {
        return {
            std::nullopt,
            "ambiguous_open_profile_reference",
            featureName + " open wire Profile did not produce an extrudable edge shape",
            profileLink.object,
        };
    }

    return {
        profileShape,
        {},
        {},
        selectedSubnames.empty() ? std::string{} : selectedSubnames.front(),
        std::move(selectedSubnames),
        std::move(selectedStableSubnames),
        std::move(downgradedStableSubnames),
        kind,
        unstableOpenProfileReference,
    };
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

ResolveAttempt resolveAttemptFromIdentityDecision(const topo::SubshapeIdentityDecision& decision)
{
    if (decision.diagnostics.empty()) {
        return {};
    }
    const runtime::Diagnostic& diagnostic = decision.diagnostics.front();
    return {
        std::nullopt,
        diagnostic.code,
        diagnostic.message,
        diagnostic.subname,
    };
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
    const auto nonEmptyStableSubnames = [&]() {
        std::vector<std::string> values;
        if (!profileLink.stableSubnamesExplicit) {
            return values;
        }
        for (const std::string& stableSubname : profileLink.stableSubnames) {
            if (!stableSubname.empty()) {
                values.push_back(stableSubname);
            }
        }
        return values;
    }();

    if (!profileLink.subnames.empty()) {
        const std::string subname = profileLink.subnames.front();
        if (requestLocalInternalFaceSubname(subname)) {
            addLegacyInternalFaceSubListDiagnostic(
                context,
                object,
                profileLink,
                featureName,
                subname);
            return std::nullopt;
        }
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "unsupported_subshape_kind",
                               featureName + " Profile.SubList requires a backend InternalFace stable alias in StableSubList",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object,
                               subname);
        return std::nullopt;
    }

    if (nonEmptyStableSubnames.size() > 1U) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile.StableSubList must select exactly one sketch InternalFace stable alias",
                               object.name,
                               "Profile",
                               "runtime",
                               profileLink.object);
        return std::nullopt;
    }

    if (nonEmptyStableSubnames.size() == 1U) {
        const std::string& stableSubname = nonEmptyStableSubnames.front();
        if (requestLocalInternalSubname(stableSubname)) {
            addLegacyInternalFaceSubListDiagnostic(
                context,
                object,
                profileLink,
                featureName,
                stableSubname);
            return std::nullopt;
        }
        if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
            addUnsupportedInternalStableSubnameDiagnostic(
                context,
                object,
                profileLink,
                featureName,
                stableSubname,
                "Sketch.InternalShape NamedShape/ElementMap evidence");
            return std::nullopt;
        }
        const auto currentSubname = resolveInternalFaceStableSubname(object,
                                                                     context,
                                                                     profileLink,
                                                                     shapeValue,
                                                                     {},
                                                                     stableSubname,
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
            runtime::addDiagnostic(context.diagnostics,
                                   "error",
                                   "invalid_subshape",
                                   featureName + " Profile target " + profileLink.object + " has no subshape "
                                       + *currentSubname,
                                   object.name,
                                   "Profile",
                                   "runtime",
                                   profileLink.object,
                                   *currentSubname);
            return std::nullopt;
        }
        return ProfileBasedProfileSelection {
            profileLink,
            *subshape,
            shapeValue.profileNormal,
            *currentSubname,
            stableSubname,
            true,
            false,
            false,
            false,
        };
    }

    if (shapeValue.profileRequiresSubshapeSelection) {
        runtime::addDiagnostic(context.diagnostics,
                               "error",
                               "invalid_subshape",
                               featureName + " Profile target " + profileLink.object
                                   + " has split InternalFace regions; Profile.StableSubList must select one backend stable alias",
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
            attempt.stableSubnames.size() == 1U ? attempt.stableSubnames.front() : std::string {};
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
        resolveFaceOnSource(object,
                            profileLink,
                            shapeValue.shape,
                            namedShape,
                            featureName);
    if (!direct.shape
        && direct.code == "invalid_subshape"
        && bodyTopoShapeContext
        && targetUsesFeatureLocalShapeBoundary(context, profileLink.object)) {
        if (const auto fullSubname = bodyDisplayFullSubnameForTarget(profileLink, *bodyTopoShapeContext)) {
            direct.code = "body_display_subname_not_feature_local";
            direct.message = featureName + " Profile uses Body display path " + *fullSubname
                + ", but " + profileLink.object + ".Shape has no local " + direct.subname;
            addResolveDiagnostic(context, object, profileLink, direct);
            return std::nullopt;
        }
    }
    const bool targetIsDisplayOnly = profileTargetIsDisplayOnly(context, profileLink.object);
    if (targetIsDisplayOnly && direct.shape) {
        return selectionFromAttempt(direct, false);
    }

    if (bodyTopoShapeContext
        && (!targetIsDisplayOnly || linkHasExplicitBodyReplayEvidence(profileLink))) {
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
            const app::Link bodyShapeLink = bodyTopoShapeLink(profileLink, direct.downgradedStableSubnames);
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

std::optional<ProfileBasedProfileSelection> resolveLinkedOpenProfileSelection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& profileLink,
    const runtime::ShapeValue& shapeValue,
    OpenProfileMode openProfileMode,
    const std::string& featureName)
{
    if (openProfileMode == OpenProfileMode::Reject) {
        addOpenProfileDiagnostic(context,
                                 object,
                                 profileLink,
                                 "error",
                                 "open_profile",
                                 featureName + " OpenProfileMode=Reject does not accept open wire profiles",
                                 profileLink.subnames.empty() ? std::string{} : profileLink.subnames.front());
        return std::nullopt;
    }

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

    const auto selectionFromAttempt = [&](const app::Link& link,
                                          const ResolveAttempt& attempt,
                                          bool fromBodyCumulativeReplay) {
        ProfileBasedProfileSelection selection {
            link,
            *attempt.shape,
            shapeValue.profileNormal,
            attempt.subname,
            attempt.stableSubnames.empty() ? std::string{} : attempt.stableSubnames.front(),
            !attempt.unstableOpenProfileReference && !attempt.stableSubnames.empty(),
            false,
            false,
            fromBodyCumulativeReplay,
        };
        selection.kind = attempt.kind;
        selection.selectedSubnames = attempt.subnames;
        selection.selectedStableSubnames = attempt.stableSubnames;
        selection.unstableOpenProfileReference = attempt.unstableOpenProfileReference;
        return selection;
    };
    const auto warnIfUnstable = [&](const ProfileBasedProfileSelection& selection) {
        if (!selection.unstableOpenProfileReference) {
            return;
        }
        addOpenProfileDiagnostic(context,
                                 object,
                                 selection.link,
                                 "warning",
                                 "ambiguous_open_profile_reference",
                                 featureName + " open wire Profile.SubList uses current EdgeN without stable topology identity",
                                 selection.selectedSubnames.empty() ? std::string{} : selection.selectedSubnames.front());
    };

    const auto namedShapeIt = context.namedShapes.find(profileLink.object);
    const part::NamedShape* namedShape =
        namedShapeIt == context.namedShapes.end() ? nullptr : &namedShapeIt->second;
    ResolveAttempt direct =
        resolveEdgesOnSource(object, profileLink, shapeValue.shape, namedShape, featureName);
    const bool targetIsDisplayOnly = profileTargetIsDisplayOnly(context, profileLink.object);
    if (targetIsDisplayOnly && direct.shape) {
        auto selection = selectionFromAttempt(profileLink, direct, false);
        warnIfUnstable(selection);
        return selection;
    }

    if (bodyTopoShapeContext
        && (!targetIsDisplayOnly || linkHasExplicitBodyReplayEvidence(profileLink))) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
        // ::ProfileBased::getProfileShape(), calls Part::Feature::getTopoShape(..., sub.c_str()).
        // Same-Body "Pad.EdgeN" profile links must resolve against the cumulative Body shape at
        // Pad, just like linked FaceN profiles; only the requested subshape kind changes to Edge.
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
            const app::Link bodyShapeLink = bodyTopoShapeLink(profileLink, direct.downgradedStableSubnames);
            ResolveAttempt bodyShapeAttempt =
                resolveEdgesOnSource(object, bodyShapeLink, bodyTopoShape->shape, &bodyNamedShape, featureName);
            if (bodyShapeAttempt.shape) {
                auto selection = selectionFromAttempt(profileLink, bodyShapeAttempt, true);
                warnIfUnstable(selection);
                return selection;
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
        auto selection = selectionFromAttempt(profileLink, direct, false);
        warnIfUnstable(selection);
        return selection;
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
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Solid
            && shapeIt->second.kind != runtime::ShapeValue::Kind::PartPrimitive)) {
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
        const bool explicitStableSubshape = linkHasExplicitStableSubshapeReference(profileLink);
        const bool ambiguousMultiFace = shapeIt->second.profileRequiresSubshapeSelection;
        if (!selection && (explicitSubshape || explicitStableSubshape || ambiguousMultiFace)) {
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
        if ((profileLink.subnames.empty() && !linkHasExplicitStableSubshapeReference(profileLink)
             && !linkTargetsSketchProfile(context, profileLink))
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
    for (const auto& rawProfileLink : profileLinks) {
        app::Link profileLink =
            normalizedBodyQualifiedProfileLink(rawProfileLink, context).value_or(rawProfileLink);

        const auto shapeIt = context.shapes.find(profileLink.object);
        if (shapeIt != context.shapes.end() && shapeIt->second.kind == runtime::ShapeValue::Kind::Sketch) {
            RawOpenProfileResolveAttempt openAttempt =
                resolveRawSketchOpenProfile(object, context, profileLink, openProfileMode, featureName);
            if (openAttempt.attempted) {
                if (!openAttempt.selection) {
                    return {};
                }
                selections.push_back(std::move(*openAttempt.selection));
                continue;
            }
        }

        if ((profileLink.subnames.empty() && !linkHasExplicitStableSubshapeReference(profileLink)
             && !linkTargetsSketchProfile(context, profileLink))
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

        if (shapeIt != context.shapes.end()
            && shapeIt->second.kind != runtime::ShapeValue::Kind::Sketch
            && linkRequestsCurrentEdgeSubshape(profileLink)) {
            auto selection = resolveLinkedOpenProfileSelection(object,
                                                               context,
                                                               profileLink,
                                                               shapeIt->second,
                                                               openProfileMode,
                                                               featureName);
            if (!selection) {
                return {};
            }
            selections.push_back(std::move(*selection));
            continue;
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
                                   featureName + " open wire Profile.SubSet currently requires one source target",
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
