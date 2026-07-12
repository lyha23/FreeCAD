#pragma once

#include "cad_core/app/document.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/reference_lifecycle.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cad_core::runtime
{

struct ReferenceResolutionView
{
    const std::map<std::string, ShapeValue>& shapes;
    const std::map<std::string, nlohmann::json>& objects;
    const std::map<std::string, const app::DocumentObject*>& documentObjects;
    std::map<std::string, part::NamedShape>& namedShapes;
    app::ElementMapProducerTrace* producerTrace = nullptr;
};

enum class ReferenceResolutionStatus
{
    Resolved,
    Recovered,
    Ambiguous,
    Split,
    Deleted,
    SemanticDrift,
    Missing,
};

// FreeCAD:
// /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference(),
// resolves the current element before updating persisted subnames;
// /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.h::searchElementCache() and
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::onBeforeChange()
// provide the old subshape evidence that cad-core receives as request-local ReferenceShadow.
struct ReferenceResolutionResult
{
    std::string requestedObject;
    std::string propertyName;
    std::string requestedSubname;
    std::string requestedStableSubname;
    std::string resolvedSubname;
    TopoDS_Shape resolvedShape;
    ReferenceResolutionStatus status = ReferenceResolutionStatus::Missing;
    part::ReferenceMatchStatus recoveryStatus = part::ReferenceMatchStatus::Missing;
    std::string recoveryMethod;
    std::string recoveryReason;
    std::string diagnosticCode;
    std::string diagnosticReason;
    bool mapperDiagnostic = false;
    std::optional<long> sourceGeometryId;
    std::string sourceGeometryKind;
    std::string sourceStableSubname;
};

struct ReferenceSubshapeResolution
{
    std::string subname;
    TopoDS_Shape shape;
    bool recovered = false;
    std::string recoveryMethod;
    std::string recoveryReason;
    std::optional<long> sourceGeometryId;
    std::string sourceGeometryKind;
    std::string sourceStableSubname;
};

struct ReferenceSubshapeRecovery
{
    part::ReferenceMatchStatus status = part::ReferenceMatchStatus::Missing;
    std::optional<ReferenceSubshapeResolution> resolution;
    std::string reason;
    std::string diagnosticCode;
};

struct ReferenceValidationResult
{
    bool valid = true;
    std::vector<Diagnostic> diagnostics;
    nlohmann::json elementReferenceUpdates = nlohmann::json::array();
};

std::optional<ReferenceSubshapeResolution> currentSubshapeForReference(const app::Link& link,
                                                                       std::size_t index,
                                                                       const ReferenceResolutionView& view);

std::optional<ReferenceSubshapeResolution> internalSubshapeFromShadowSub(const app::Link& link,
                                                                         std::size_t index,
                                                                         const app::ReferenceShadow& shadow,
                                                                         const ReferenceResolutionView& view);

ReferenceSubshapeRecovery recoverSubshapeForReference(const app::Link& link,
                                                      std::size_t index,
                                                      const app::ReferenceShadow& shadow,
                                                      const ReferenceResolutionView& view);

std::string referenceRecoveryDiagnosticCode(const ReferenceSubshapeRecovery& recovery);
std::string referenceRecoveryDiagnosticReason(const ReferenceSubshapeRecovery& recovery);

ReferenceResolutionResult resolveReferenceShadow(const app::Link& link,
                                                 std::size_t index,
                                                 const app::ReferenceShadow& shadow,
                                                 const std::string& propertyName,
                                                 ReferenceResolutionView& view);

void recordReferenceRecoveryMapperDiagnostic(ReferenceResolutionView& view,
                                             const app::Link& link,
                                             const app::ReferenceShadow& shadow,
                                             const ReferenceResolutionResult& result);

ReferenceValidationResult validateObjectReferences(const app::DocumentObject& object,
                                                   ReferenceResolutionView& view,
                                                   const ReferenceLifecycleView& lifecycleView);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp
// ::GeoFeature::updateElementReferences() walks downstream LinkSub properties when Shape changes.
std::vector<std::string> downstreamElementReferenceSubnames(
    const std::string& producer,
    const ComputeContext& context
);

}  // namespace cad_core::runtime
