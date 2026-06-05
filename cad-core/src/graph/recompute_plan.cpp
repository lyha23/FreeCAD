#include "cad_core/graph/recompute_plan.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace cad_core::graph {

using runtime::addDiagnostic;

namespace {

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

bool isFrozenExternalGeometryReference(const app::Link& link)
{
    return link.property == "ExternalGeometry" && link.externalGeometryFlags.count("Frozen") != 0U
        && link.externalGeometryFlags.count("Sync") == 0U;
}

bool isMissingExternalGeometryReference(const app::Link& link)
{
    return link.property == "ExternalGeometry" && link.externalGeometryFlags.count("Missing") != 0U
        && link.externalGeometryFlags.count("Sync") == 0U;
}

bool isDetachedExternalGeometryReference(const app::Link& link)
{
    return link.property == "ExternalGeometry" && link.externalGeometryFlags.count("Detached") != 0U;
}

bool hasReferenceShadowBrepSnapshot(const app::Link& link)
{
    return std::any_of(link.referenceShadows.begin(), link.referenceShadows.end(), [](const auto& shadow) {
        return shadow.brep.has_value();
    });
}

std::string externalGeometryReferenceKey(const app::Link& link)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return link.object;
    }
    return link.object + "." + link.subnames.front();
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

bool hasNativeExternalGeoEvidence(
    const app::DocumentObject& object,
    const app::Link& link
)
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

void visitObject(const std::string& name,
                 const app::Document& document,
                 RecomputePlan& plan,
                 std::vector<runtime::Diagnostic>& diagnostics,
                 std::vector<std::string>& visiting,
                 std::set<std::string>& visited)
{
    if (visited.count(name) != 0U) {
        return;
    }

    const auto cycleIt = std::find(visiting.begin(), visiting.end(), name);
    if (cycleIt != visiting.end()) {
        std::vector<std::string> cycle(cycleIt, visiting.end());
        cycle.push_back(name);
        std::ostringstream message;
        message << "Cycle dependency:";
        for (const auto& item : cycle) {
            message << ' ' << item;
            plan.blockedObjects.insert(item);
        }
        addDiagnostic(diagnostics, "error", "cycle_dependency", message.str(), name, {}, "graph", name);
        return;
    }

    visiting.push_back(name);
    const auto& object = document.objects.at(document.indexByName.at(name));
    if (!object.invalidProperties.empty()) {
        plan.blockedObjects.insert(name);
        visiting.pop_back();
        visited.insert(name);
        if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
            plan.order.push_back(name);
        }
        return;
    }

    std::set<std::string> seenDependencies;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase
    // is the dependency-bearing property base for PropertyLink, PropertyLinkList, PropertyLinkSub
    // and PropertyLinkSubList. cad-core graph consumes document-normalized links only.
    for (const auto& link : object.dependencyLinks) {
        if (seenDependencies.count(link.object) != 0U) {
            continue;
        }
        seenDependencies.insert(link.object);
        if (document.indexByName.count(link.object) == 0U) {
            if (isDetachedExternalGeometryReference(link)) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
                // ::SketchObject::onExternalGeoChanged(), for Detached old geometry, clears the
                // reference and erases the ExternalGeometry link before source object validation.
                continue;
            }
            const bool oldExternalGeometrySnapshot = isFrozenExternalGeometryReference(link)
                || isMissingExternalGeometryReference(link);
            if (oldExternalGeometrySnapshot && hasNativeExternalGeoEvidence(object, link)) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                // ::SketchObject::rebuildExternalGeometry() keeps old ExternalGeo entries for
                // Frozen refs and says Missing "linked external geometry will continue to work".
                // cad-core treats the request-local ExternalGeo pool as old geometry evidence.
                continue;
            }
            if (oldExternalGeometrySnapshot && hasReferenceShadowBrepSnapshot(link)) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                // ::SketchObject::rebuildExternalGeometry(), for frozen refs, inserts "key" into
                // "refSet" and continues before validating "Obj"; for missing refs, the pre-pass
                // says "linked external geometry will continue to work" and only appends a current
                // object when it can resolve one. cad-core mirrors the old ExternalGeo evidence
                // only when the request carries ReferenceShadow.brep single-subshape evidence.
                continue;
            }
            const std::string subname = link.subnames.empty() ? std::string{} : link.subnames.front();
            if (oldExternalGeometrySnapshot) {
                const std::string state = isFrozenExternalGeometryReference(link) ? "Frozen" : "Missing";
                addDiagnostic(
                    diagnostics,
                    "error",
                    "missing_external_geometry_snapshot",
                    state + " ExternalGeometry target " + link.object
                        + " is missing and the request does not include native ExternalGeo or "
                          "ReferenceShadow.brep evidence",
                    name,
                    link.property,
                    "graph",
                    link.object,
                    subname
                );
                plan.blockedObjects.insert(name);
                continue;
            }
            const std::string documentName = link.documentRef ? linkedDocumentName(*link.documentRef) : std::string{};
            if (!documentName.empty()) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
                // ::DocInfo::init(), calls "addPendingDocument(..., objName, ...)" when the
                // external document is not loaded; ::DocInfo::attach() calls "restoreLink(obj)"
                // after the document arrives, and ::slotDeleteDocument() calls "detach()".
                // cad-core has no backend document session, so the request-side Document status
                // selects the graph diagnostic while a later request with the target object present
                // recomputes normally from the DocumentObject graph.
                std::string code = "missing_external_document";
                std::string message = "XLink target " + link.object + " from linked document "
                    + documentName + " is not available";
                if (documentRefPendingReload(*link.documentRef)) {
                    code = "external_document_pending_reload";
                    message = "XLink target " + link.object + " from linked document "
                        + documentName + " is pending reload";
                    if (link.documentRef->allowPartialExplicit) {
                        message += link.documentRef->allowPartial ? " with partial load allowed"
                                                                  : " with partial load disabled";
                    }
                }
                else if (documentRefUnloaded(*link.documentRef)) {
                    code = "external_document_unloaded";
                    message = "XLink target " + link.object + " from linked document "
                        + documentName + " was unloaded or deleted";
                }
                addDiagnostic(diagnostics,
                              "error",
                              code,
                              message,
                              name,
                              link.property,
                              "graph",
                              link.object,
                              subname);
                plan.blockedObjects.insert(name);
                continue;
            }
            addDiagnostic(diagnostics,
                          "error",
                          "missing_link_target",
                          "Object links to missing object " + link.object,
                          name,
                          link.property,
                          "graph",
                          link.object,
                          subname);
            plan.blockedObjects.insert(name);
            continue;
        }
        plan.dependencies[name].push_back(link.object);
        visitObject(link.object, document, plan, diagnostics, visiting, visited);
    }

    visiting.pop_back();
    visited.insert(name);
    if (std::find(plan.order.begin(), plan.order.end(), name) == plan.order.end()) {
        plan.order.push_back(name);
    }
}

}  // namespace

RecomputePlan buildPlan(const app::Document& document, std::vector<runtime::Diagnostic>& diagnostics)
{
    RecomputePlan plan;
    std::set<std::string> visited;
    std::vector<std::string> visiting;

    for (const auto& target : document.targets) {
        if (document.indexByName.count(target) == 0U) {
            addDiagnostic(diagnostics,
                          "error",
                          "missing_object",
                          "Recompute target " + target + " does not exist",
                          target,
                          {},
                          "graph",
                          target);
            continue;
        }
        visitObject(target, document, plan, diagnostics, visiting, visited);
    }

    return plan;
}

}  // namespace cad_core::graph
