#pragma once

#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad_core::document {

enum class PropertyKind {
    Unknown,
    Bool,
    Integer,
    Float,
    String,
    Enumeration,
    Vector,
    Placement,
    Link,
    LinkList,
    LinkSub,
    LinkSubList,
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h
// ::PropertyLinkBase::ShadowSub stores App::ElementNamePair with "newName" and "oldName".
struct ShadowSub {
    std::string newName;
    std::string oldName;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp
// ::Feature::ElementCache keeps old referenced subshape geometry; cad-core carries the
// approved single-subshape snapshot metadata in request JSON instead of a session cache.
struct BrepSnapshot {
    std::string format;
    long long byteLength = 0;
    std::string sha256;
    std::string data;
};

// FreeCAD basis: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.h
// ::searchElementCache() and SketchObject.cpp::SketchObject() "registerElementCache".
// ReferenceShadow is cad-core's stateless evidence channel for that old-subshape cache.
struct ReferenceShadow {
    std::string target;
    long long targetId = 0;
    std::string property;
    std::string shapeType;
    std::string indexed;
    std::string subname;
    std::string stableSubname;
    nlohmann::json fingerprint = nlohmann::json::object();
    std::optional<BrepSnapshot> brep;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
// ::PropertyLinkBase::updateLabelReference() replaces "$" + old Label + "."
// with "$" + new Label + "." after verifying the subobject still resolves to obj.
struct LabelReferenceRename {
    std::size_t index = 0;
    std::string oldLabel;
    std::string newLabel;
    std::string oldSubname;
    std::string newSubname;
};

// FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
// ::PropertyXLink::Save() persists "file" and "stamp"; PropertyXLinkContainer::Save()
// persists DocMap "name"/"label" so afterRestore() can populate "_DocMap".
// ::DocInfo::init() calls "addPendingDocument(...)" for unloaded/partial external docs,
// and ::DocInfo::slotDeleteDocument() calls "detach()"; cad-core carries those states as
// request evidence instead of keeping a backend document session.
struct LinkDocumentRef {
    std::string file;
    std::string name;
    std::string label;
    std::string stamp;
    std::string status;
    std::string currentName;
    std::string currentLabel;
    std::string currentStamp;
    std::string currentStatus;
    bool allowPartial = false;
    bool allowPartialExplicit = false;
};

struct Link {
    std::string object;
    std::vector<std::string> subnames;
    std::vector<std::string> stableSubnames;
    std::vector<std::string> fullSubnames;
    std::string property;
    bool stableSubnamesExplicit = false;
    std::vector<ShadowSub> shadowSubs;
    std::vector<ReferenceShadow> referenceShadows;
    bool fullSubnamesExplicit = false;
    std::string resolvedObjectFrom;
    std::vector<LabelReferenceRename> labelReferenceRenames;
    std::optional<LinkDocumentRef> documentRef;
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.h
    // ::ExternalGeometryExtension::Flag stores "Defining", "Frozen", "Detached", "Missing", "Sync"
    // on external geometry entries. cad-core carries the request-side flag names on normalized
    // LinkSub entries so SketchObject can rebuild ExternalGeometry without mutating the graph.
    std::set<std::string> externalGeometryFlags;
};

struct Placement {
    std::array<double, 3> base;
    std::array<double, 4> rotation;
};

struct PropertyValue {
    std::string name;
    std::string propertyType;
    PropertyKind kind = PropertyKind::Unknown;
    nlohmann::json raw = nullptr;
    std::vector<Link> links;
    bool valid = true;
};

struct DocumentObject {
    std::string name;
    long long id = 0;
    std::string typeId;
    nlohmann::json properties = nlohmann::json::object();
    std::map<std::string, PropertyValue> propertyValues;
    std::vector<Link> dependencyLinks;
    std::set<std::string> invalidProperties;
};

struct Document {
    std::vector<DocumentObject> objects;
    std::vector<std::string> targets;
    std::map<std::string, std::size_t> indexByName;
    std::map<std::string, std::string> parentGroupByObject;
};

bool isLink(const nlohmann::json& value);
void collectLinks(const nlohmann::json& value, std::vector<Link>& links);
std::vector<Link> readLinks(const nlohmann::json& value);
std::optional<Link> readLink(const nlohmann::json& value);
const PropertyValue* propertyValue(const DocumentObject& object, const std::string& property);
bool hasPropertyType(const DocumentObject& object, const std::string& property, const std::string& propertyType);
std::vector<Link> readLinks(const DocumentObject& object, const std::string& property);
std::optional<Link> readLink(const DocumentObject& object, const std::string& property);
std::optional<bool> readBool(const DocumentObject& object, const std::string& property);
std::optional<double> readNumber(const DocumentObject& object, const std::string& property);
std::optional<std::string> readString(const DocumentObject& object, const std::string& property);
std::optional<std::array<double, 3>> readVector3(const DocumentObject& object, const std::string& property);
std::optional<Placement> readPlacement(const DocumentObject& object, const std::string& property);

std::pair<Document, std::vector<runtime::Diagnostic>> parseDocument(const nlohmann::json& raw);

}  // namespace cad_core::document
