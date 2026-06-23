#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/reference_lifecycle.h"

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::runtime
{

nlohmann::json shadowSubsToJson(const std::vector<app::ShadowSub>& shadowSubs);
nlohmann::json externalGeometryFlagsToJson(const std::set<std::string>& flags);
nlohmann::json labelReferenceRenamesToJson(const std::vector<app::LabelReferenceRename>& renames);

// FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp
// ::PropertyLinkBase::_updateElementReference(), updates persisted subnames, "ShadowSub",
// and the "shadow" evidence together after resolving the current element.
nlohmann::json referenceShadowUpdateJson(const app::ReferenceShadow& shadow,
                                         const app::Link& link,
                                         const std::string& subname,
                                         const TopoDS_Shape& currentSubshape,
                                         const std::string& recoveryMethod = {},
                                         const std::string& recoveryReason = {});

void appendElementReferenceUpdate(const app::DocumentObject& object,
                                  const std::string& propertyName,
                                  const app::PropertyValue& propertyValue,
                                  const app::Link& link,
                                  const std::vector<std::string>& subnames,
                                  const nlohmann::json& referenceShadows,
                                  nlohmann::json& updates);

void appendElementReferenceSubListUpdate(const app::DocumentObject& object,
                                         const std::string& propertyName,
                                         const app::PropertyValue& propertyValue,
                                         const std::map<std::size_t, nlohmann::json>& referenceShadowUpdates,
                                         const std::map<std::size_t, std::vector<std::string>>& subnameUpdates,
                                         nlohmann::json& updates);

void appendReferenceMetadataUpdates(const app::DocumentObject& object,
                                    const ReferenceLifecycleView& lifecycleView,
                                    nlohmann::json& updates);

void appendDocumentReferenceDiagnostics(const app::DocumentObject& object,
                                        const ReferenceLifecycleView& lifecycleView,
                                        std::vector<Diagnostic>& diagnostics);

}  // namespace cad_core::runtime
