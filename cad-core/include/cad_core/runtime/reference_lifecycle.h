#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/diagnostics.h"

#include <map>
#include <optional>
#include <set>
#include <string>

namespace cad_core::runtime
{

enum class ReferenceLifecycleState
{
    CurrentTarget,
    MissingTarget,
    FrozenOldExternalGeometry,
    MissingOldExternalGeometry,
    DetachedExternalGeometry,
    ExternalDocumentMissing,
    ExternalDocumentPendingReload,
    ExternalDocumentUnloaded,
    MetadataOnlyUpdate,
};

enum class ReferenceLifecycleAction
{
    FollowDependency,
    IgnoreDependencyUseOldEvidence,
    DetachReference,
    BlockRecompute,
    PublishMetadataOnlyUpdate,
};

// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.h
// ::ExternalGeometryExtension::Flag stores "Defining", "Frozen", "Detached", "Missing",
// and "Sync"; SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry() interprets
// those flags before deciding whether to use current source geometry or old ExternalGeo evidence.
struct ExternalGeometryLifecycleFlags
{
    bool defining = false;
    bool frozen = false;
    bool detached = false;
    bool missing = false;
    bool sync = false;
};

// FreeCAD: /home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
// ::PropertyLinkBase::_updateElementReference() resolves current elements before publishing
// ShadowSub/reference updates; ::DocInfo::init()/attach()/slotDeleteDocument() move XLinks
// through pending, restored and detached states;
// /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
// ::SketchObject::rebuildExternalGeometry() keeps old external geometry for Frozen/Missing
// references when request-local evidence exists.
struct ReferenceLifecycleDecision
{
    ReferenceLifecycleState state = ReferenceLifecycleState::CurrentTarget;
    ReferenceLifecycleAction action = ReferenceLifecycleAction::FollowDependency;
    bool targetExists = true;
    bool requiresGraphDependency = true;
    bool shouldValidateReferenceShadow = true;
    bool canUseNativeExternalGeoEvidence = false;
    bool canUseReferenceShadowBrepEvidence = false;
    bool shouldPublishElementReferenceUpdate = false;
    bool hasLabelReferenceRename = false;
    bool hasDocumentReferenceRename = false;
    bool hasDocumentReferenceStampMismatch = false;
    std::optional<Diagnostic> diagnostic;
    std::optional<Diagnostic> runtimeWarning;
};

// The view is deliberately read-only and request-local. The lifecycle module must not receive
// ComputeContext or any session cache because cad-core's document graph is the single source of truth.
struct ReferenceLifecycleView
{
    const std::map<std::string, const app::DocumentObject*>& documentObjects;
    const app::Document* document = nullptr;
};

ExternalGeometryLifecycleFlags externalGeometryLifecycleFlags(const app::Link& link);
std::set<std::string> normalizedExternalGeometryFlagSet(ExternalGeometryLifecycleFlags flags);
std::string externalGeometryReferenceKey(const app::Link& link);

ReferenceLifecycleDecision classifyReferenceLifecycle(const app::DocumentObject& owner,
                                                      const app::PropertyValue& propertyValue,
                                                      const app::Link& link,
                                                      const ReferenceLifecycleView& view);

}  // namespace cad_core::runtime
