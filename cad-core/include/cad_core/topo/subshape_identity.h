#pragma once

#include "cad_core/part/topo_shape.h"
#include "cad_core/runtime/diagnostics.h"

#include <optional>
#include <string>
#include <vector>

namespace cad_core::topo {

// FreeCAD:
// /Users/li/Chili3DProject/FreeCAD/src/App/ComplexGeoDataPyImp.cpp::getElementMappedName()
// exposes mapped element names, while
// /Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::updateElementReference()
// updates durable references from ElementMap history. cad-core keeps that distinction explicit:
// display publication may expose current "FaceN" names, but durable references require mapped
// NamedShape/ElementMap evidence.
enum class SubshapeReferenceUse {
    DisplayPublication,
    DurableFeatureReference,
    ReferenceRecovery,
};

// FreeCAD:
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp tracks deleted/split
// mapper outcomes separately from the current TopoShape, and
// /Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp stores unique mapped element names.
// These statuses are the request-local response contract derived from that ledger evidence.
enum class StableIdentityStatus {
    Stable,
    StableSplitFragment,
    CurrentOnly,
    BodyDisplayOnly,
    MissingEvidence,
    Ambiguous,
    Split,
    Deleted,
    Unsupported,
};

// FreeCAD:
// /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp keeps current
// subelement names, while App::ElementMap is the stable identity ledger. This decision object is
// cad-core's boundary value after choosing subname/fullSubname for display and stableSubname only
// from the ledger.
struct SubshapeIdentityDecision {
    std::string subname;
    std::string fullSubname;
    std::string stableSubname;
    StableIdentityStatus status = StableIdentityStatus::CurrentOnly;
    std::vector<runtime::Diagnostic> diagnostics;
};

struct DisplayPublicationIdentityRequest {
    std::string object;
    std::string indexed;
    std::string subname;
    std::string fullSubname;
    std::string candidateStableSubname;
    std::string rawIdentityStatus;
    const part::NamedShape* namedShape = nullptr;
    bool bodyDisplayOnly = false;
};

struct DurableSubshapeReferenceRequest {
    std::string consumerObject;
    std::string property;
    std::string targetObject;
    std::string subname;
    std::string stableSubname;
    std::string fullSubname;
    const part::NamedShape* namedShape = nullptr;
    bool stableSubnameExplicit = false;
};

std::string identityStatusName(StableIdentityStatus status);
std::string localElementName(const std::string& name);
std::optional<TopAbs_ShapeEnum> topologicalElementKind(const std::string& name);
bool isPlainTopologicalElementName(const std::string& name);
bool isBareTopologicalSubname(const std::string& name);
bool stableSubnameKindMatchesIndexed(const std::string& indexed, const std::string& stableSubname);
bool hasStableElementMapEvidence(const part::NamedShape* namedShape,
                                 const std::string& indexed,
                                 const std::string& stableSubname);
bool isCurrentOnlyTopologicalStableSubname(const std::string& indexed,
                                           const std::string& stableSubname);
std::string stableSubnameFromNamedShape(const std::string& indexed,
                                        const part::NamedShape* namedShape);
SubshapeIdentityDecision decideDisplayPublication(const DisplayPublicationIdentityRequest& request);
SubshapeIdentityDecision resolveDurableSubshapeReference(const DurableSubshapeReferenceRequest& request);

}  // namespace cad_core::topo
