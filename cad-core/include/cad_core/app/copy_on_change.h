#pragma once

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace cad_core::app {

struct CopyOnChangeDocumentView {
    const std::map<std::string, const DocumentObject*>* objects = nullptr;
    const std::map<std::string, std::vector<std::string>>* dependencies = nullptr;
};

struct CopyOnChangeLifecycleResult {
    nlohmann::json documentObjectUpdates = nlohmann::json::array();
    std::vector<runtime::Diagnostic> diagnostics;
};

// FreeCAD:
// /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
// ::LinkBaseExtension::makeCopyOnChange(), key: "getOnChangeCopyObjects" then
// "copyObject(srcobjs)", and ::syncCopyOnChange(), key: "CopyOnLinkReplace".
// cad-core mirrors the lifecycle as stateless documentObjectUpdates: cloned
// objects, relinked copied-subtree properties, group ownership, link writeback
// and touched sync are emitted for the frontend to persist into the next request
// graph.
CopyOnChangeLifecycleResult buildCopyOnChangeLifecycleUpdates(
    const DocumentObject& linkObject,
    const Link& linkedObject,
    const CopyOnChangeDocumentView& view);

}  // namespace cad_core::app
