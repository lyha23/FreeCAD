#include "cad_core/app/document.h"

#include "document_object_internal.h"
#include "property_links_internal.h"

#include <algorithm>
#include <set>
#include <utility>

namespace cad_core::app {

using runtime::addDiagnostic;

namespace {

bool isGeoFeatureGroupType(const std::string& typeId)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Part.cpp::Part::Part()
    // calls GroupExtension::initExtension(this), and
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BodyBase.cpp::BodyBase::BodyBase()
    // calls App::OriginGroupExtension::initExtension(this).
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.h
    // ::AssemblyObject derives from App::Part, and
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyLink.h
    // ::AssemblyLink derives from App::Part.
    return typeId == "App::Part" || typeId == "PartDesign::Body"
        || typeId == "Assembly::AssemblyObject" || typeId == "Assembly::AssemblyLink";
}

bool isOwnedLinkElement(const DocumentObject& element, const DocumentObject& owner)
{
    if (element.typeId != "App::LinkElement") {
        return false;
    }
    const auto ownerValue = readNumber(element, "_LinkOwner");
    return !ownerValue || static_cast<long long>(*ownerValue) == 0 || static_cast<long long>(*ownerValue) == owner.id;
}

void addMaterializedLinkElementDependencies(Document& document)
{
    for (auto& object : document.objects) {
        if (object.typeId != "App::Link" || !readLinks(object, "ElementList").empty()) {
            continue;
        }
        const std::size_t elementCount = static_cast<std::size_t>(std::max(0.0, readNumber(object, "ElementCount").value_or(0.0)));
        if (elementCount == 0U || !readBool(object, "ShowElement").value_or(true)) {
            continue;
        }
        const auto ownerLinkedObject = readLink(object, "LinkedObject");

        for (std::size_t index = 0; index < elementCount; ++index) {
            const std::string elementName = object.name + "_i" + std::to_string(index);
            const auto elementIt = document.indexByName.find(elementName);
            if (elementIt == document.indexByName.end()) {
                continue;
            }
            auto& element = document.objects.at(elementIt->second);
            if (!isOwnedLinkElement(element, object)) {
                continue;
            }

            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
            // ::LinkBaseExtension::update(), when ShowElement is true, creates or re-claims
            // child LinkElement objects named owner "_i" index; cad-core keeps the graph
            // immutable, but still makes those materialized elements dependency-bearing.
            object.dependencyLinks.push_back(Link{elementName, {}, {}, {}, "ElementList"});
            if (!readLink(element, "LinkedObject") && ownerLinkedObject) {
                // FreeCAD: /Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
                // ::LinkBaseExtension::updateGroup(), for owned LinkElement children, copies
                // parent LinkedObject subvalues into "element->LinkedObject" and syncs transform.
                // cad-core keeps the request graph immutable, but the child still depends on the
                // inherited target so recompute order matches the FreeCAD-synchronized state.
                element.dependencyLinks.push_back(*ownerLinkedObject);
            }
        }
    }
}

std::optional<double> readDisplayMeshDeflection(const nlohmann::json& raw,
                                                std::vector<runtime::Diagnostic>& diagnostics)
{
    const nlohmann::json* value = nullptr;
    std::string property;
    const auto adapterIt = raw.find("adapter");
    if (adapterIt != raw.end() && adapterIt->is_object()) {
        const auto directIt = adapterIt->find("displayMeshDeflection");
        if (directIt != adapterIt->end()) {
            value = &*directIt;
            property = "adapter.displayMeshDeflection";
        }
        const auto displayMeshIt = adapterIt->find("displayMesh");
        if (value == nullptr && displayMeshIt != adapterIt->end() && displayMeshIt->is_object()) {
            const auto deflectionIt = displayMeshIt->find("deflection");
            if (deflectionIt != displayMeshIt->end()) {
                value = &*deflectionIt;
                property = "adapter.displayMesh.deflection";
            }
        }
    }
    const auto snakeIt = raw.find("display_mesh_deflection");
    if (value == nullptr && snakeIt != raw.end()) {
        value = &*snakeIt;
        property = "display_mesh_deflection";
    }
    if (value == nullptr || value->is_null()) {
        return std::nullopt;
    }
    if (!value->is_number()) {
        addDiagnostic(
            diagnostics,
            "error",
            "parse_error",
            property + " must be a positive number",
            {},
            property,
            "parse"
        );
        return std::nullopt;
    }
    const double deflection = value->get<double>();
    if (deflection <= 0.0) {
        addDiagnostic(
            diagnostics,
            "error",
            "parse_error",
            property + " must be a positive number",
            {},
            property,
            "parse"
        );
        return std::nullopt;
    }
    return deflection;
}

}  // namespace

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw)
{
    Document document;
    std::vector<runtime::Diagnostic> diagnostics;

    if (!raw.is_object()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document root must be a JSON object", {}, {}, "parse");
        return {document, diagnostics};
    }

    const auto objectsIt = raw.find("Objects");
    if (objectsIt == raw.end() || !objectsIt->is_array()) {
        addDiagnostic(diagnostics, "error", "parse_error", "Document field 'Objects' must be a list", {}, {}, "parse");
        return {document, diagnostics};
    }
    document.displayMeshDeflection = readDisplayMeshDeflection(raw, diagnostics);
    const auto topoStateIt = raw.find("topoNamingState");
    if (topoStateIt != raw.end() && topoStateIt->is_object()) {
        document.topoNamingState = *topoStateIt;
    }

    std::set<std::string> seenNames;
    std::set<long long> seenIds;
    for (std::size_t index = 0; index < objectsIt->size(); ++index) {
        const auto& item = objectsIt->at(index);
        auto object = parseDocumentObject(item, index, seenNames, seenIds, diagnostics);
        if (!object) {
            continue;
        }

        document.indexByName[object->name] = document.objects.size();
        document.objects.push_back(std::move(*object));
    }

    normalizeSourceObjectRenameLinks(document);
    normalizeLabelReferenceLinks(document, diagnostics);

    for (const auto& object : document.objects) {
        if (isGeoFeatureGroupType(object.typeId)) {
            const auto groupLinks = readLinks(object, "Group");
            for (const auto& link : groupLinks) {
                if (link.object.empty() || document.indexByName.count(link.object) == 0U) {
                    continue;
                }
                document.parentGroupByObject.emplace(link.object, object.name);
            }

            const auto originLink = readLink(object, "Origin");
            if (originLink && !originLink->object.empty()
                && document.indexByName.count(originLink->object) != 0U) {
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/OriginGroupExtension.cpp
                // ::OriginGroupExtension::Origin is "LinkScope::Child", and
                // ::extensionGetSubObject() composes the owner group's placement for its Origin.
                document.parentGroupByObject.emplace(originLink->object, object.name);
            }
        }

        const auto originFeatureLinks = readLinks(object, "OriginFeatures");
        for (const auto& link : originFeatureLinks) {
            if (link.object.empty() || document.indexByName.count(link.object) == 0U) {
                continue;
            }
            // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Datums.cpp
            // ::LocalCoordinateSystem::LocalCoordinateSystem() stores controlled axes and
            // base planes in hidden "OriginFeatures"; Datums.h says LCS "doesn't use Group".
            document.parentGroupByObject.emplace(link.object, object.name);
        }
    }
    addMaterializedLinkElementDependencies(document);

    if (raw.contains("recompute") && raw.at("recompute").is_object() && raw.at("recompute").contains("objs")) {
        const auto& rawTargets = raw.at("recompute").at("objs");
        if (!rawTargets.is_array()) {
            addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must be a list of object names", {}, {}, "parse");
        }
        else {
            for (const auto& target : rawTargets) {
                if (!target.is_string()) {
                    addDiagnostic(diagnostics, "error", "parse_error", "recompute.objs must contain object names", {}, {}, "parse");
                    continue;
                }
                document.targets.push_back(target.get<std::string>());
            }
        }
    }
    if (document.targets.empty() && !document.objects.empty()) {
        document.targets.push_back(document.objects.back().name);
    }

    return {document, diagnostics};
}

}  // namespace cad_core::app
