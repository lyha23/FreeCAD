#include "sketch_object_external.h"

#include "cad_core/part/brep_snapshot.h"
#include "cad_core/part/topo_shape.h"
#include "cad_core/part/topo_shape_reference.h"
#include "cad_core/runtime/compute_context.h"

#include <BRepAlgoAPI_Section.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <map>

namespace cad_core::sketcher
{

std::set<std::string> normalizedExternalGeometryFlagSet(ExternalGeometryFlags flags)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry() clears "Sync" after a rebuild and clears
    // "Missing" once "refSet" contains the reference again. cad-core returns the same mutation
    // as a documentObjectUpdates suggestion and keeps the request graph immutable.
    std::set<std::string> result;
    if (flags.defining) {
        result.insert("Defining");
    }
    if (flags.frozen) {
        result.insert("Frozen");
    }
    if (flags.detached) {
        result.insert("Detached");
    }
    if (flags.missing) {
        result.insert("Missing");
    }
    if (flags.sync) {
        result.insert("Sync");
    }
    return result;
}

ExternalGeometryFlags externalGeometryFlags(const app::Link& link)
{
    ExternalGeometryFlags flags;
    flags.defining = link.externalGeometryFlags.count("Defining") != 0U;
    flags.frozen = link.externalGeometryFlags.count("Frozen") != 0U;
    flags.detached = link.externalGeometryFlags.count("Detached") != 0U;
    flags.missing = link.externalGeometryFlags.count("Missing") != 0U;
    flags.sync = link.externalGeometryFlags.count("Sync") != 0U;
    return flags;
}

ExternalGeometryFlags externalGeometryFlags(const nlohmann::json& value)
{
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.cpp
    // ::ExternalGeometryExtension::saveAttributes() writes "Flags" as the persisted bitset,
    // while ::ExternalGeometryExtension::flag2str exposes the stable names.
    ExternalGeometryFlags flags;
    const auto addFlag = [&](const std::string& flag) {
        if (flag == "Defining") {
            flags.defining = true;
        }
        else if (flag == "Frozen") {
            flags.frozen = true;
        }
        else if (flag == "Detached") {
            flags.detached = true;
        }
        else if (flag == "Missing") {
            flags.missing = true;
        }
        else if (flag == "Sync") {
            flags.sync = true;
        }
    };
    const auto readFlagList = [&](const nlohmann::json& raw) {
        if (!raw.is_array()) {
            return;
        }
        for (const auto& item : raw) {
            if (item.is_string()) {
                addFlag(item.get<std::string>());
            }
        }
    };
    if (const auto it = value.find("ExternalFlags"); it != value.end()) {
        readFlagList(*it);
    }
    if (const auto it = value.find("Flags"); it != value.end()) {
        if (it->is_number_integer() || it->is_number_unsigned()) {
            const long long bits = it->get<long long>();
            if (bits >= 0) {
                flags.defining = flags.defining || ((bits & (1LL << 0)) != 0);
                flags.frozen = flags.frozen || ((bits & (1LL << 1)) != 0);
                flags.detached = flags.detached || ((bits & (1LL << 2)) != 0);
                flags.missing = flags.missing || ((bits & (1LL << 3)) != 0);
                flags.sync = flags.sync || ((bits & (1LL << 4)) != 0);
            }
        }
        else {
            readFlagList(*it);
        }
    }
    for (const char* flag : {"Defining", "Frozen", "Detached", "Missing", "Sync"}) {
        if (const auto it = value.find(flag); it != value.end() && it->is_boolean() && it->get<bool>()) {
            addFlag(flag);
        }
    }
    return flags;
}

nlohmann::json externalGeometryFlagsJson(const std::set<std::string>& flags)
{
    nlohmann::json result = nlohmann::json::array();
    for (const char* name : {"Defining", "Frozen", "Detached", "Missing", "Sync"}) {
        if (flags.count(name) != 0U) {
            result.push_back(name);
        }
    }
    return result;
}

nlohmann::json externalReferenceShadowsJson(const std::vector<app::ReferenceShadow>& shadows)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto& shadow : shadows) {
        nlohmann::json item = {
            {"target", shadow.target},
            {"targetId", shadow.targetId},
            {"property", shadow.property},
            {"shapeType", shadow.shapeType},
            {"indexed", shadow.indexed},
            {"subname", shadow.subname},
            {"fingerprint", shadow.fingerprint},
        };
        if (!shadow.stableSubname.empty()) {
            item["stableSubname"] = shadow.stableSubname;
        }
        if (shadow.brep) {
            item["brep"] = {
                {"format", shadow.brep->format},
                {"byteLength", shadow.brep->byteLength},
                {"sha256", shadow.brep->sha256},
                {"data", shadow.brep->data},
            };
        }
        result.push_back(std::move(item));
    }
    return result;
}

nlohmann::json externalGeometryLinkItemJson(const app::Link& link,
                                            const std::set<std::string>& flags)
{
    nlohmann::json item = {
        {"value", link.object},
        {"SubList", link.subnames},
    };
    if (link.stableSubnamesExplicit) {
        item["StableSubList"] = link.stableSubnames;
    }
    if (link.fullSubnamesExplicit) {
        item["FullSubList"] = link.fullSubnames;
    }
    if (!link.shadowSubs.empty()) {
        nlohmann::json shadowSubs = nlohmann::json::array();
        for (const auto& shadowSub : link.shadowSubs) {
            shadowSubs.push_back({
                {"newName", shadowSub.newName},
                {"oldName", shadowSub.oldName},
            });
        }
        item["ShadowSub"] = std::move(shadowSubs);
    }
    if (!link.referenceShadows.empty()) {
        item["ReferenceShadow"] = externalReferenceShadowsJson(link.referenceShadows);
    }
    if (!flags.empty()) {
        item["ExternalFlags"] = externalGeometryFlagsJson(flags);
    }
    return item;
}

std::string externalGeometryReferenceKey(const app::Link& link)
{
    if (link.subnames.empty() || link.subnames.front().empty()) {
        return link.object;
    }
    return link.object + "." + link.subnames.front();
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

std::string stableSubnameDiagnosticMessage(
    const std::string& target,
    const std::string& stableSubname,
    part::ElementResolveStatus status
)
{
    if (status == part::ElementResolveStatus::Deleted) {
        return "ExternalGeometry target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as deleted";
    }
    if (status == part::ElementResolveStatus::Split) {
        return "ExternalGeometry target " + target + " has stable subname " + stableSubname
            + ", but current ElementMap history marks it as split";
    }
    return "ExternalGeometry target " + target + " has stable subname " + stableSubname
        + ", but it is not in the current ElementMap";
}

std::optional<TopAbs_ShapeEnum> shapeKindFromReferenceShadow(const std::string& shapeType)
{
    if (shapeType == "Face") {
        return TopAbs_FACE;
    }
    if (shapeType == "Edge") {
        return TopAbs_EDGE;
    }
    if (shapeType == "Vertex") {
        return TopAbs_VERTEX;
    }
    return std::nullopt;
}

std::optional<ExternalSubshape> oldExternalSubshapeFromBrepSnapshot(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    for (const auto& shadow : link.referenceShadows) {
        if (!shadow.target.empty() && shadow.target != link.object) {
            continue;
        }
        if (!shadow.brep) {
            continue;
        }
        const auto shapeKind = shapeKindFromReferenceShadow(shadow.shapeType);
        if (!shapeKind) {
            continue;
        }
        std::string error;
        const auto oldShape = cad_core::part::readBrepSnapshot(
            shadow.brep->format,
            shadow.brep->data,
            shadow.brep->byteLength,
            shadow.brep->sha256,
            error
        );
        if (!oldShape || oldShape->IsNull()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_reference_shadow_brep",
                "ExternalGeometry old snapshot does not decode: " + error,
                object.name,
                "ExternalGeometry",
                "runtime",
                link.object,
                shadow.subname
            );
            return std::nullopt;
        }
        if (oldShape->ShapeType() != *shapeKind) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_reference_shadow_brep",
                "ExternalGeometry old snapshot shape type does not match ReferenceShadow shapeType",
                object.name,
                "ExternalGeometry",
                "runtime",
                link.object,
                shadow.subname
            );
            return std::nullopt;
        }
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // ::SketchObject::rebuildExternalGeometry(), for frozen entries, checks
        // "egf->testFlag(ExternalGeometryExtension::Frozen)" and inserts the key into "refSet"
        // before source object validation; for missing entries, the pre-pass says the linked
        // "external geometry will continue to work" and only appends a current object when one
        // can be resolved. cad-core mirrors the request-local equivalent by consuming the approved
        // ReferenceShadow.brep single-subshape snapshot.
        return ExternalSubshape {*shapeKind, *oldShape, shadow.subname.empty() ? shadow.indexed : shadow.subname};
    }
    return std::nullopt;
}

std::string internalSubnameFromStableElementMap(
    const runtime::ComputeContext& context,
    const std::string& objectName,
    const std::string& stableSubname
)
{
    if (stableSubname.empty() || stableSubname.rfind("Internal", 0) == 0) {
        return {};
    }
    const auto objectIt = context.objects.find(objectName);
    if (objectIt == context.objects.end() || !objectIt->second.is_object()) {
        return {};
    }
    const auto mapIt = objectIt->second.find("internal_element_map");
    if (mapIt == objectIt->second.end() || !mapIt->is_object()) {
        return {};
    }
    const auto mappedIt = mapIt->find(stableSubname);
    if (mappedIt == mapIt->end() || !mappedIt->is_string()) {
        return {};
    }
    const std::string currentInternal = mappedIt->get<std::string>();
    if (currentInternal.rfind("InternalEdge", 0) != 0
        && currentInternal.rfind("InternalVertex", 0) != 0) {
        return {};
    }
    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::getInternalElementMap() keeps InternalEdge/InternalVertex recoverable through their raw
    // Edge/Vertex names. cad-core consumes that map only for these element kinds; InternalFace
    // remains history-backed and is not recovered through this shortcut.
    return currentInternal;
}

std::vector<std::string> stableNameCandidatesForExternal(
    const app::Link& link,
    const app::ReferenceShadow& shadow
)
{
    std::vector<std::string> candidates;
    const auto addCandidate = [&](const std::string& stableSubname) {
        if (stableSubname.empty() || stableSubname.rfind("Internal", 0) == 0) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), stableSubname) == candidates.end()) {
            candidates.push_back(stableSubname);
        }
    };

    addCandidate(shadow.stableSubname);
    if (!link.stableSubnames.empty()) {
        addCandidate(link.stableSubnames.front());
    }
    return candidates;
}

bool hasSketchInternalSubshape(const runtime::ShapeValue& shapeValue, const std::string& subname)
{
    const auto parsed = part::parseInternalSubshapeName(subname);
    if (!parsed || !shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return false;
    }
    return part::subshapeByName(*shapeValue.internalShape, *parsed).has_value();
}

bool internalSubshapeMatchesReferenceShadow(
    const runtime::ShapeValue& shapeValue,
    const std::string& subname,
    const TopoDS_Shape& subshape,
    const app::ReferenceShadow& shadow
)
{
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return false;
    }
    return part::referenceShadowMatchesCurrentSubshape(
        *shapeValue.internalShape,
        "Internal",
        subname,
        subshape,
        shadow
    );
}

std::string internalSubnameFromShadowSub(
    const app::Link& link,
    const runtime::ShapeValue& shapeValue,
    const runtime::ComputeContext& context
)
{
    if (link.shadowSubs.empty() || !shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        return {};
    }

    for (const auto& shadow : link.referenceShadows) {
        if (!shadow.target.empty() && shadow.target != link.object) {
            continue;
        }
        const auto targetObjectIt = context.documentObjects.find(link.object);
        if (targetObjectIt != context.documentObjects.end()
            && shadow.targetId != targetObjectIt->second->id) {
            continue;
        }
        for (const std::string& stableName : stableNameCandidatesForExternal(link, shadow)) {
            for (const auto& shadowSub : link.shadowSubs) {
                if (shadowSub.newName != stableName) {
                    continue;
                }
                const auto parsed = part::parseInternalSubshapeName(shadowSub.oldName);
                if (!parsed || (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX)) {
                    continue;
                }
                const auto subshape = part::subshapeByName(*shapeValue.internalShape, *parsed);
                if (!subshape || subshape->IsNull()) {
                    continue;
                }
                // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp
                // ::PropertyLinkBase::_updateElementReference() tries ShadowSub before
                // GeoFeature::searchElementCache(); SketchObjectExternal then rebuilds the
                // transient external geometry from the resolved subshape. cad-core accepts the
                // paired InternalEdge/InternalVertex only after ReferenceShadow proves it still
                // matches the old referenced geometry.
                if (internalSubshapeMatchesReferenceShadow(shapeValue, shadowSub.oldName, *subshape, shadow)) {
                    return shadowSub.oldName;
                }
            }
        }
    }
    return {};
}

std::vector<ExternalSubshape> wholeShapeExternalSubshapes(const runtime::ShapeValue& shapeValue)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::addExternal(), when the selected shape is not a face but
    // "hasSubShape(TopAbs_FACE)", expands the external reference into each FaceN; otherwise it
    // expands into EdgeN when edges exist. cad-core keeps this as request-local expansion and does
    // not mutate ExternalGeometry.
    std::vector<ExternalSubshape> result;
    if (shapeValue.shape.IsNull()) {
        return result;
    }

    TopAbs_ShapeEnum kind = TopAbs_SHAPE;
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(shapeValue.shape, TopAbs_FACE, subshapes);
    if (subshapes.Extent() > 0) {
        kind = TopAbs_FACE;
    }
    else {
        TopExp::MapShapes(shapeValue.shape, TopAbs_EDGE, subshapes);
        if (subshapes.Extent() > 0) {
            kind = TopAbs_EDGE;
        }
    }
    if (kind != TopAbs_FACE && kind != TopAbs_EDGE) {
        return result;
    }

    const std::string prefix = kind == TopAbs_FACE ? "Face" : "Edge";
    result.reserve(static_cast<std::size_t>(subshapes.Extent()));
    for (int index = 1; index <= subshapes.Extent(); ++index) {
        result.push_back(ExternalSubshape {kind, subshapes(index), prefix + std::to_string(index)});
    }
    return result;
}

std::optional<std::string> sourcePrefixedExternalOldName(const std::string& stableSubname)
{
    const std::size_t dot = stableSubname.rfind('.');
    if (dot == std::string::npos || dot + 1 >= stableSubname.size()) {
        return std::nullopt;
    }
    std::string oldName = stableSubname.substr(dot + 1);
    const auto parsed = part::parseSubshapeName(oldName);
    if (!parsed
        || (parsed->kind != TopAbs_FACE && parsed->kind != TopAbs_EDGE
            && parsed->kind != TopAbs_VERTEX)) {
        return std::nullopt;
    }
    return oldName;
}

std::optional<ExternalSubshape> resolveSketchInternalSubshape(
    const app::Link& link,
    const app::DocumentObject& object,
    const runtime::ShapeValue& shapeValue,
    runtime::ComputeContext& context,
    const std::string& subname
)
{
    const auto parsed = part::parseInternalSubshapeName(subname);
    if (!parsed) {
        return std::nullopt;
    }
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getSubObject(),
    // convertInternalName("InternalEdge") resolves the subshape from InternalShape, not Shape.
    if (!shapeValue.internalShape || shapeValue.internalShape->IsNull()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "ExternalGeometry target " + link.object + " has no InternalShape for " + subname,
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            subname
        );
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            "ExternalGeometry projection currently supports InternalEdgeN and InternalVertexN "
            "subshapes",
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            subname
        );
        return std::nullopt;
    }

    const auto subshape = part::subshapeByName(*shapeValue.internalShape, *parsed);
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "ExternalGeometry target " + link.object + " has no subshape " + subname,
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            subname
        );
        return std::nullopt;
    }
    return ExternalSubshape {parsed->kind, *subshape, subname};
}

std::optional<std::vector<ExternalSubshape>> resolveExternalGeometryLink(
    const app::Link& link,
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const std::string subname = link.subnames.empty() ? std::string {} : link.subnames.front();
    const std::string stableSubname = link.stableSubnames.size() == 1U ? link.stableSubnames.front()
                                                                       : std::string {};
    const auto shapeIt = context.shapes.find(link.object);
    if (shapeIt == context.shapes.end()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_link_target",
            "ExternalGeometry target " + link.object + " did not produce a shape",
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            subname
        );
        return std::nullopt;
    }

    std::string currentSubname = subname;
    const auto namedShapeIt = context.namedShapes.find(link.object);
    const auto targetObjectIt = context.documentObjects.find(link.object);
    const bool targetIsBody = targetObjectIt != context.documentObjects.end()
        && targetObjectIt->second->typeId == "PartDesign::Body";
    bool resolvedViaBodyOldName = false;
    if (targetIsBody) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        // /SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry(), for Missing
        // external refs calls GeoFeature::resolveElement(obj, ref-subname, elementName) and
        // then appends the original object plus "elementName.oldName". Source-prefixed stable
        // refs such as Body.SketchPad.Edge1 therefore project Body.Edge1, not the current
        // ElementMap target for SketchPad.Edge1.
        if (const auto sourceOldName = sourcePrefixedExternalOldName(stableSubname)) {
            if (namedShapeIt != context.namedShapes.end()) {
                const auto resolved
                    = part::resolveElementReference(namedShapeIt->second, subname, stableSubname);
                if (resolved.status == part::ElementResolveStatus::Resolved) {
                    currentSubname = *sourceOldName;
                    resolvedViaBodyOldName = true;
                }
                else if (!stableSubname.empty() && stableSubname != subname) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        stableSubnameDiagnosticCode(resolved.status),
                        stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                        object.name,
                        "ExternalGeometry",
                        "runtime",
                        link.object,
                        stableSubname
                    );
                    return std::nullopt;
                }
            }
            else {
                currentSubname = *sourceOldName;
                resolvedViaBodyOldName = true;
            }
        }
    }
    if (!resolvedViaBodyOldName && namedShapeIt != context.namedShapes.end()) {
        const auto resolved
            = part::resolveElementReference(namedShapeIt->second, subname, stableSubname);
        if (resolved.status == part::ElementResolveStatus::Resolved && resolved.element) {
            currentSubname = *resolved.element;
        }
        else if (!stableSubname.empty() && stableSubname != subname) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                stableSubnameDiagnosticCode(resolved.status),
                stableSubnameDiagnosticMessage(link.object, stableSubname, resolved.status),
                object.name,
                "ExternalGeometry",
                "runtime",
                link.object,
                stableSubname
            );
            return std::nullopt;
        }
    }

    if (!subname.empty()) {
        std::string internalSubname = subname;
        if (const std::string shadowSubInternal
            = internalSubnameFromShadowSub(link, shapeIt->second, context);
            !shadowSubInternal.empty()) {
            internalSubname = shadowSubInternal;
        }
        if (part::parseInternalSubshapeName(subname)
            && !hasSketchInternalSubshape(shapeIt->second, subname)) {
            const std::string stableInternal
                = internalSubnameFromStableElementMap(context, link.object, stableSubname);
            if (!stableInternal.empty()) {
                internalSubname = stableInternal;
            }
        }
        if (auto internal
            = resolveSketchInternalSubshape(link, object, shapeIt->second, context, internalSubname)) {
            return std::vector<ExternalSubshape> {*internal};
        }
        if (part::parseInternalSubshapeName(subname)) {
            return std::nullopt;
        }
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumLine && link.subnames.empty()) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumLine and builds an edge from its shape.
        return std::vector<ExternalSubshape> {
            ExternalSubshape {TopAbs_EDGE, shapeIt->second.shape, {}}
        };
    }

    if (shapeIt->second.kind == runtime::ShapeValue::Kind::DatumPoint && link.subnames.empty()) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() accepts Part::DatumPoint and builds a vertex from its shape.
        return std::vector<ExternalSubshape> {
            ExternalSubshape {TopAbs_VERTEX, shapeIt->second.shape, {}}
        };
    }

    if (link.subnames.empty()) {
        std::vector<ExternalSubshape> expanded = wholeShapeExternalSubshapes(shapeIt->second);
        if (!expanded.empty()) {
            return expanded;
        }
    }

    if (link.subnames.size() != 1U || subname.empty()) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "ExternalGeometry must reference exactly one FaceN/EdgeN/VertexN subshape or a "
            "DatumLine/DatumPoint",
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            subname
        );
        return std::nullopt;
    }

    const auto parsed = part::parseSubshapeName(currentSubname);
    if (!parsed) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "Invalid ExternalGeometry subshape name " + currentSubname,
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    if (parsed->kind != TopAbs_FACE && parsed->kind != TopAbs_EDGE && parsed->kind != TopAbs_VERTEX) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "unsupported_subshape_kind",
            "ExternalGeometry projection currently supports FaceN, EdgeN and VertexN subshapes",
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }

    std::optional<TopoDS_Shape> subshape;
    if (namedShapeIt != context.namedShapes.end()) {
        subshape = part::subshapeByName(namedShapeIt->second, currentSubname);
    }
    else {
        subshape = part::subshapeByName(shapeIt->second.shape, currentSubname);
    }
    if (!subshape) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_subshape",
            "ExternalGeometry target " + link.object + " has no subshape " + currentSubname,
            object.name,
            "ExternalGeometry",
            "runtime",
            link.object,
            currentSubname
        );
        return std::nullopt;
    }
    return std::vector<ExternalSubshape> {ExternalSubshape {parsed->kind, *subshape, currentSubname}};
}

enum class ExternalGeometryType
{
    Projection = 0,
    Intersection = 1,
    Both = 2,
};

struct NativeExternalGeometry
{
    std::size_t index = 0;
    std::string ref;
    int refIndex = -1;
    ExternalGeometryFlags flags;
    SketchGeometrySet geometry;
    nlohmann::json raw;
};

gp_Pnt pointInSketchLocalPlane(const gp_Pnt& worldPoint, const gp_Trsf& sketchPlacement)
{
    gp_Trsf inverse = sketchPlacement;
    inverse.Invert();
    gp_Pnt local = worldPoint.Transformed(inverse);
    return gp_Pnt(local.X(), local.Y(), 0.0);
}

gp_Dir directionInSketchLocalPlane(gp_Dir worldDirection, const gp_Trsf& sketchPlacement)
{
    gp_Trsf inverse = sketchPlacement;
    inverse.Invert();
    worldDirection.Transform(inverse);
    return worldDirection;
}

gp_Pln sketchPlaneFromPlacement(const gp_Trsf& sketchPlacement)
{
    gp_Pnt origin(0, 0, 0);
    origin.Transform(sketchPlacement);
    gp_Dir normal(0, 0, 1);
    normal.Transform(sketchPlacement);
    return gp_Pln(origin, normal);
}

std::optional<SketchEllipse> projectedEllipseFromAxes(
    const gp_Pnt& center,
    const gp_Dir& majorDirection,
    double majorRadius,
    const gp_Dir& minorDirection,
    double minorRadius,
    const gp_Trsf& sketchPlacement
)
{
    const gp_Dir localMajor = directionInSketchLocalPlane(majorDirection, sketchPlacement);
    const gp_Dir localMinor = directionInSketchLocalPlane(minorDirection, sketchPlacement);

    const double ax = majorRadius * localMajor.X();
    const double ay = majorRadius * localMajor.Y();
    const double bx = minorRadius * localMinor.X();
    const double by = minorRadius * localMinor.Y();

    const double sxx = ax * ax + bx * bx;
    const double syy = ay * ay + by * by;
    const double sxy = ax * ay + bx * by;
    const double trace = sxx + syy;
    const double delta = std::sqrt((sxx - syy) * (sxx - syy) + 4.0 * sxy * sxy);
    const double lambdaMajor = 0.5 * (trace + delta);
    const double lambdaMinor = 0.5 * (trace - delta);
    if (lambdaMajor <= Precision::SquareConfusion()) {
        return std::nullopt;
    }

    double angle = 0.0;
    if (std::abs(sxy) > Precision::Confusion()) {
        angle = std::atan2(lambdaMajor - sxx, sxy);
    }
    else if (syy > sxx) {
        angle = 0.5 * 3.14159265358979323846;
    }

    return SketchEllipse {
        0U,
        center,
        std::sqrt(lambdaMajor),
        lambdaMinor > Precision::SquareConfusion() ? std::sqrt(lambdaMinor) : 0.0,
        angle,
        true
    };
}

double angleXUInSketchPlane(gp_Dir worldXDirection, gp_Dir worldNormal, const gp_Trsf& sketchPlacement)
{
    const gp_Dir localX = directionInSketchLocalPlane(worldXDirection, sketchPlacement);
    const gp_Dir localNormal = directionInSketchLocalPlane(worldNormal, sketchPlacement);
    return -localX.AngleWithRef(gp_Dir(1, 0, 0), localNormal);
}

bool projectExternalLineEdge(
    const TopoDS_Edge& edge,
    const gp_Trsf& sketchPlacement,
    ExternalGeometryResult& result,
    bool defining
)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        return false;
    }

    double first = curve.FirstParameter();
    if (std::abs(first) > 1E99) {
        first = -10000.0;
    }
    double last = curve.LastParameter();
    if (std::abs(last) > 1E99) {
        last = 10000.0;
    }

    const gp_Pnt start = pointInSketchLocalPlane(curve.Value(first), sketchPlacement);
    const gp_Pnt end = pointInSketchLocalPlane(curve.Value(last), sketchPlacement);
    if (start.SquareDistance(end) < Precision::SquareConfusion()) {
        // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        // /SketchObjectExternal.cpp::projectLine(), when "Distance(p1, p2) < Precision::Confusion()",
        // returns a construction GeomPoint at the midpoint instead of dropping the projection.
        result.points.push_back(
            gp_Pnt((start.X() + end.X()) / 2.0, (start.Y() + end.Y()) / 2.0, (start.Z() + end.Z()) / 2.0)
        );
        if (defining) {
            result.definingPoints.push_back(SketchPoint {0U, result.points.back(), false});
        }
        return true;
    }
    result.segments.push_back(SketchSegment {0U, start, end, !defining});
    return true;
}

bool isCollapsedProjectedLineEdge(const TopoDS_Edge& edge, const gp_Trsf& sketchPlacement)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line) {
        return false;
    }

    double first = curve.FirstParameter();
    if (std::abs(first) > 1E99) {
        first = -10000.0;
    }
    double last = curve.LastParameter();
    if (std::abs(last) > 1E99) {
        last = 10000.0;
    }

    const gp_Pnt start = pointInSketchLocalPlane(curve.Value(first), sketchPlacement);
    const gp_Pnt end = pointInSketchLocalPlane(curve.Value(last), sketchPlacement);
    return start.SquareDistance(end) < Precision::SquareConfusion();
}

bool isFullPeriodicEdge(const BRepAdaptor_Curve& curve)
{
    constexpr double twoPi = 6.28318530717958647692;
    return std::abs(curve.LastParameter() - curve.FirstParameter() - twoPi) < Precision::PConfusion()
        || curve.Value(curve.FirstParameter()).SquareDistance(curve.Value(curve.LastParameter()))
        < Precision::SquareConfusion();
}

bool projectExternalCurveEdge(
    const TopoDS_Edge& edge,
    const gp_Trsf& sketchPlacement,
    ExternalGeometryResult& result,
    bool defining
)
{
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Circle) {
        const gp_Circ circle = curve.Circle();
        const gp_Dir localNormal
            = directionInSketchLocalPlane(circle.Axis().Direction(), sketchPlacement);
        const gp_Pnt center = pointInSketchLocalPlane(circle.Location(), sketchPlacement);
        if (!localNormal.IsParallel(gp_Dir(0, 0, 1), Precision::Angular())) {
            gp_Vec majorVector(gp_Dir(0, 0, 1));
            majorVector.Cross(gp_Vec(localNormal));
            if (majorVector.Magnitude() <= Precision::Confusion()) {
                return false;
            }
            const gp_Dir majorDirection(majorVector);
            const gp_Pnt start(
                center.X() - circle.Radius() * majorDirection.X(),
                center.Y() - circle.Radius() * majorDirection.Y(),
                0.0
            );
            const gp_Pnt end(
                center.X() + circle.Radius() * majorDirection.X(),
                center.Y() + circle.Radius() * majorDirection.Y(),
                0.0
            );

            if (localNormal.IsNormal(gp_Dir(0, 0, 1), Precision::Angular())) {
                // FreeCAD:
                // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                // processEdge(), for a circle with normal vector in the sketch plane,
                // says "projection is a line".
                result.segments.push_back(SketchSegment {0U, start, end, !defining});
                return true;
            }

            if (!isFullPeriodicEdge(curve)) {
                return false;
            }
            const double angle = std::atan2(majorDirection.Y(), majorDirection.X());
            const double minorRadius = circle.Radius() * std::abs(localNormal.Dot(gp_Dir(0, 0, 1)));
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge(), for the general non-parallel circle case, projects a full circle
            // to a construction Part::GeomEllipse.
            result.ellipses.push_back(
                SketchEllipse {0U, center, circle.Radius(), minorRadius, angle, !defining}
            );
            return true;
        }

        if (isFullPeriodicEdge(curve)) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge() projects a full circle edge parallel to the sketch plane as
            // construction Part::GeomCircle in ExternalGeo.
            result.circles.push_back(SketchCircle {0U, center, circle.Radius(), !defining});
            return true;
        }

        result.arcs.push_back(
            SketchArc {0U, center, circle.Radius(), curve.FirstParameter(), curve.LastParameter(), !defining}
        );
        return true;
    }

    if (curve.GetType() == GeomAbs_Ellipse) {
        const gp_Elips ellipse = curve.Ellipse();
        const gp_Dir localNormal
            = directionInSketchLocalPlane(ellipse.Axis().Direction(), sketchPlacement);
        if (!localNormal.IsParallel(gp_Dir(0, 0, 1), Precision::Angular())) {
            if (!isFullPeriodicEdge(curve)) {
                return false;
            }

            const gp_Pnt center = pointInSketchLocalPlane(ellipse.Location(), sketchPlacement);
            const auto projected = projectedEllipseFromAxes(
                center,
                ellipse.XAxis().Direction(),
                ellipse.MajorRadius(),
                ellipse.YAxis().Direction(),
                ellipse.MinorRadius(),
                sketchPlacement
            );
            if (!projected) {
                return false;
            }
            if (projected->minorRadius <= Precision::Confusion()) {
                const gp_Pnt start(
                    center.X() - projected->majorRadius * std::cos(projected->angle),
                    center.Y() - projected->majorRadius * std::sin(projected->angle),
                    0.0
                );
                const gp_Pnt end(
                    center.X() + projected->majorRadius * std::cos(projected->angle),
                    center.Y() + projected->majorRadius * std::sin(projected->angle),
                    0.0
                );
                result.segments.push_back(SketchSegment {0U, start, end, !defining});
                return true;
            }

            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge(), GeomAbs_Ellipse uses a general projected-ellipse construction
            // algorithm from the original major/minor axes.
            SketchEllipse ellipse = *projected;
            ellipse.construction = !defining;
            result.ellipses.push_back(ellipse);
            return true;
        }

        const gp_Pnt center = pointInSketchLocalPlane(ellipse.Location(), sketchPlacement);
        const double angle = angleXUInSketchPlane(
            ellipse.XAxis().Direction(),
            ellipse.Axis().Direction(),
            sketchPlacement
        );
        if (isFullPeriodicEdge(curve)) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // processEdge2() keeps a full ellipse edge as construction Part::GeomEllipse.
            result.ellipses.push_back(
                SketchEllipse {0U, center, ellipse.MajorRadius(), ellipse.MinorRadius(), angle, !defining}
            );
            return true;
        }

        result.ellipseArcs.push_back(
            SketchEllipseArc {
                0U,
                center,
                ellipse.MajorRadius(),
                ellipse.MinorRadius(),
                angle,
                curve.FirstParameter(),
                curve.LastParameter(),
                !defining
            }
        );
        return true;
    }

    return false;
}

bool projectExternalEdgeIntoResult(
    const TopoDS_Edge& edge,
    const gp_Trsf& sketchPlacement,
    ExternalGeometryResult& result,
    bool defining
)
{
    if (projectExternalLineEdge(edge, sketchPlacement, result, defining)) {
        return true;
    }
    return projectExternalCurveEdge(edge, sketchPlacement, result, defining);
}

void appendExternalGeometry(ExternalGeometryResult& result, const ExternalGeometryResult& source)
{
    result.segments.insert(result.segments.end(), source.segments.begin(), source.segments.end());
    result.points.insert(result.points.end(), source.points.begin(), source.points.end());
    result.definingPoints.insert(
        result.definingPoints.end(),
        source.definingPoints.begin(),
        source.definingPoints.end()
    );
    result.circles.insert(result.circles.end(), source.circles.begin(), source.circles.end());
    result.arcs.insert(result.arcs.end(), source.arcs.begin(), source.arcs.end());
    result.ellipses.insert(result.ellipses.end(), source.ellipses.begin(), source.ellipses.end());
    result.ellipseArcs
        .insert(result.ellipseArcs.end(), source.ellipseArcs.begin(), source.ellipseArcs.end());
}

bool appendUnifiedNormalFaceLine(
    const ExternalGeometryResult& boundary,
    ExternalGeometryResult& result,
    bool defining
)
{
    if (!boundary.points.empty() || !boundary.circles.empty() || !boundary.arcs.empty()
        || !boundary.ellipses.empty() || !boundary.ellipseArcs.empty() || boundary.segments.empty()) {
        return false;
    }

    gp_Pnt start = boundary.segments.front().start;
    gp_Pnt end = boundary.segments.front().end;
    auto updateExtremes = [&](const gp_Pnt& point) {
        const double currentLength = start.SquareDistance(end);
        if (point.SquareDistance(start) < point.SquareDistance(end)) {
            if (point.SquareDistance(end) > currentLength) {
                start = point;
            }
            return;
        }
        if (point.SquareDistance(start) > currentLength) {
            end = point;
        }
    };

    for (const SketchSegment& segment : boundary.segments) {
        updateExtremes(segment.start);
        updateExtremes(segment.end);
    }
    if (start.SquareDistance(end) < Precision::SquareConfusion()) {
        return false;
    }
    result.segments.push_back(SketchSegment {0U, start, end, !defining});
    return true;
}

bool projectExternalFaceBoundary(
    const TopoDS_Face& face,
    const gp_Trsf& sketchPlacement,
    ExternalGeometryResult& result,
    bool defining
)
{
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        return false;
    }

    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::processFace(), for a planar face, says "Extract all edges from the face" and then
    // "Process each edge" through processEdge(). This is the planar-boundary subset; the HLR
    // projection path for non-planar faces remains a later Face/ExternalGeometry task.
    ExternalGeometryResult boundary;
    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        if (isCollapsedProjectedLineEdge(edge, sketchPlacement)) {
            continue;
        }
        if (!projectExternalEdgeIntoResult(edge, sketchPlacement, boundary, defining)) {
            return false;
        }
    }
    if (boundary.segments.empty() && boundary.points.empty() && boundary.circles.empty()
        && boundary.arcs.empty() && boundary.ellipses.empty() && boundary.ellipseArcs.empty()) {
        return false;
    }

    const gp_Dir localFaceNormal
        = directionInSketchLocalPlane(surface.Plane().Axis().Direction(), sketchPlacement);
    if (localFaceNormal.IsNormal(gp_Dir(0, 0, 1), Precision::Angular())) {
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // ::processFace(), when "The face is normal to the sketch plane", discards the separate
        // edge projections and keeps "a single line that goes from min to max of all the projections".
        return appendUnifiedNormalFaceLine(boundary, result, defining);
    }

    appendExternalGeometry(result, boundary);
    return true;
}

std::vector<ExternalGeometryType> readExternalGeometryTypes(
    const app::DocumentObject& object,
    std::size_t count
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry(), reads "ExternalTypes.getValues()" and then
    // "Types.resize(Objects.size(), static_cast<long>(ExtType::Projection))".
    std::vector<ExternalGeometryType> types(count, ExternalGeometryType::Projection);
    const auto* value = app::propertyValue(object, "ExternalTypes");
    if (value == nullptr) {
        return types;
    }

    const nlohmann::json* payload = &value->raw;
    if (payload->is_object() && payload->contains("value")) {
        payload = &payload->at("value");
    }
    if (!payload->is_array()) {
        return types;
    }

    const std::size_t limit = std::min(count, payload->size());
    for (std::size_t index = 0; index < limit; ++index) {
        if (!payload->at(index).is_number_integer()) {
            continue;
        }
        switch (payload->at(index).get<int>()) {
            case 1:
                types.at(index) = ExternalGeometryType::Intersection;
                break;
            case 2:
                types.at(index) = ExternalGeometryType::Both;
                break;
            default:
                types.at(index) = ExternalGeometryType::Projection;
                break;
        }
    }
    return types;
}

std::string readOptionalStringAlias(const nlohmann::json& value, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const auto it = value.find(key);
        if (it != value.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return {};
}

int readOptionalIntAlias(const nlohmann::json& value, std::initializer_list<const char*> keys, int fallback)
{
    for (const char* key : keys) {
        const auto it = value.find(key);
        if (it != value.end() && it->is_number_integer()) {
            return it->get<int>();
        }
    }
    return fallback;
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

std::optional<std::vector<NativeExternalGeometry>> readNativeExternalGeometry(
    const app::DocumentObject& object,
    runtime::ComputeContext& context
)
{
    const auto propertyIt = object.properties.find("ExternalGeo");
    if (propertyIt == object.properties.end()) {
        return std::vector<NativeExternalGeometry> {};
    }
    const nlohmann::json* items = externalGeoGeometryItems(object);
    if (items == nullptr) {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "invalid_property_type",
            "ExternalGeo must be a Part::PropertyGeometryList-compatible geometry list",
            object.name,
            "ExternalGeo",
            "runtime"
        );
        return std::nullopt;
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::onExternalGeoChanged() reads "ExternalGeo.getValues()" as persisted
    // Part::Geometry entries and ExternalGeometryExtension stores "Ref", "RefIndex" and "Flags".
    // cad-core consumes the same request-side pool in a single recompute without keeping a backend
    // SketchObject session.
    std::vector<NativeExternalGeometry> result;
    result.reserve(items->size());
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto& item = items->at(index);
        if (!item.is_object()) {
            runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "unsupported_geometry",
                "ExternalGeo item must be a geometry object",
                object.name,
                "ExternalGeo",
                "runtime"
            );
            return std::nullopt;
        }

        SketchGeometrySet parsed;
        if (!parseSketchGeometry(nlohmann::json::array({item}), object, context, parsed, "ExternalGeo")) {
            return std::nullopt;
        }
        result.push_back(NativeExternalGeometry {
            index,
            readOptionalStringAlias(item, {"Ref", "ref", "reference"}),
            readOptionalIntAlias(item, {"RefIndex", "refIndex"}, -1),
            externalGeometryFlags(item),
            std::move(parsed),
            item,
        });
    }
    return result;
}

bool appendNativeExternalGeometry(
    ExternalGeometryResult& result,
    const NativeExternalGeometry& native,
    bool linkDefining
)
{
    const bool defining = linkDefining || native.flags.defining;
    bool appended = false;
    for (auto segment : native.geometry.segments) {
        segment.construction = !defining;
        result.segments.push_back(segment);
        appended = true;
    }
    for (const auto& point : native.geometry.points) {
        if (defining) {
            result.definingPoints.push_back(SketchPoint {point.geometryIndex, point.point, false});
        }
        else {
            result.points.push_back(point.point);
        }
        appended = true;
    }
    for (auto circle : native.geometry.circles) {
        circle.construction = !defining;
        result.circles.push_back(circle);
        appended = true;
    }
    for (auto arc : native.geometry.arcs) {
        arc.construction = !defining;
        result.arcs.push_back(arc);
        appended = true;
    }
    for (auto ellipse : native.geometry.ellipses) {
        ellipse.construction = !defining;
        result.ellipses.push_back(ellipse);
        appended = true;
    }
    for (auto arc : native.geometry.ellipseArcs) {
        arc.construction = !defining;
        result.ellipseArcs.push_back(arc);
        appended = true;
    }
    return appended;
}

bool appendNativeExternalGeometryForRef(
    ExternalGeometryResult& result,
    const std::vector<NativeExternalGeometry>& nativeGeometries,
    const std::string& ref,
    bool defining
)
{
    bool appended = false;
    for (const auto& native : nativeGeometries) {
        if (native.ref == ref) {
            appended = appendNativeExternalGeometry(result, native, defining) || appended;
        }
    }
    return appended;
}

nlohmann::json nativeExternalGeometryItemJson(const NativeExternalGeometry& native, bool detach)
{
    nlohmann::json item = native.raw;
    if (!detach) {
        return item;
    }

    ExternalGeometryFlags flags = native.flags;
    flags.detached = false;
    flags.missing = false;
    item.erase("Ref");
    item.erase("ref");
    item.erase("reference");
    item.erase("RefIndex");
    item.erase("refIndex");
    item.erase("Flags");
    item.erase("ExternalFlags");
    item.erase("Detached");
    item.erase("Missing");
    const auto normalizedFlags = normalizedExternalGeometryFlagSet(flags);
    if (!normalizedFlags.empty()) {
        item["ExternalFlags"] = externalGeometryFlagsJson(normalizedFlags);
    }
    return item;
}

void appendExternalGeometryFlagsUpdate(runtime::ComputeContext& context,
                                       const app::DocumentObject& object,
                                       const std::vector<app::Link>& links,
                                       const std::map<std::size_t, ExternalGeometryFlags>& replacementFlags,
                                       const std::string& reason)
{
    if (replacementFlags.empty()) {
        return;
    }

    nlohmann::json subSet = nlohmann::json::array();
    for (std::size_t index = 0; index < links.size(); ++index) {
        const auto replacement = replacementFlags.find(index);
        std::set<std::string> flags = replacement == replacementFlags.end()
            ? links.at(index).externalGeometryFlags
            : normalizedExternalGeometryFlagSet(replacement->second);
        subSet.push_back(externalGeometryLinkItemJson(links.at(index), flags));
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry(), after successful rebuild, calls
    // "egf->setFlag(ExternalGeometryExtension::Sync,false)" and toggles "Missing" from
    // whether "refSet" contains the reference. cad-core reports that mutation as a stateless
    // documentObjectUpdates suggestion.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", reason},
        {"object", object.name},
        {"objectId", object.id},
        {"typeId", object.typeId},
        {"properties",
         {
             {"ExternalGeometry",
              {
                  {"PropertyType", "App::PropertyLinkSubList"},
                  {"SubSet", std::move(subSet)},
              }},
         }},
    });
}

void appendExternalGeometryDetachUpdate(runtime::ComputeContext& context,
                                        const app::DocumentObject& object,
                                        const std::vector<app::Link>& links,
                                        const std::vector<NativeExternalGeometry>& nativeGeometries,
                                        const std::set<std::size_t>& detachedLinks)
{
    if (detachedLinks.empty()) {
        return;
    }

    nlohmann::json subSet = nlohmann::json::array();
    for (std::size_t index = 0; index < links.size(); ++index) {
        if (detachedLinks.count(index) != 0U) {
            continue;
        }
        subSet.push_back(externalGeometryLinkItemJson(
            links.at(index),
            links.at(index).externalGeometryFlags
        ));
    }

    std::set<std::string> detachedRefs;
    for (const std::size_t index : detachedLinks) {
        if (index < links.size()) {
            detachedRefs.insert(externalGeometryReferenceKey(links.at(index)));
        }
    }
    nlohmann::json properties = {
        {"ExternalGeometry",
         {
             {"PropertyType", "App::PropertyLinkSubList"},
             {"SubSet", std::move(subSet)},
         }},
    };
    if (!nativeGeometries.empty()) {
        nlohmann::json externalGeo = nlohmann::json::array();
        for (const auto& native : nativeGeometries) {
            externalGeo.push_back(nativeExternalGeometryItemJson(
                native,
                !native.ref.empty() && detachedRefs.count(native.ref) != 0U
            ));
        }
        properties["ExternalGeo"] = {
            {"PropertyType", "Part::PropertyGeometryList"},
            {"Geometry", std::move(externalGeo)},
        };
    }

    // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
    // ::SketchObject::onExternalGeoChanged(), for Detached old geometry, calls
    // "egf->setRef(std::string())", clears "Detached" / "Missing", and then erases the matching
    // entries before "ExternalGeometry.setValues(objs, subs)". cad-core reports the same request-
    // graph mutation for ExternalGeometry and, when present, the request-local ExternalGeo pool.
    context.documentObjectUpdates.push_back({
        {"action", "update"},
        {"reason", "external_geometry_detach"},
        {"object", object.name},
        {"objectId", object.id},
        {"typeId", object.typeId},
        {"properties", std::move(properties)},
    });
}

bool addExternalGeometryIntersection(
    const ExternalSubshape& external,
    const gp_Trsf& sketchPlacement,
    ExternalGeometryResult& result,
    bool defining
)
{
    // FreeCAD:
    // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    // ::SketchObject::rebuildExternalGeometry(), for "intersection", runs
    // "FCBRepAlgoAPI_Section maker(refSubShape, sketchPlane)" and then processes section edges
    // through processEdge(); standalone section vertices are imported as points.
    try {
        BRepAlgoAPI_Section maker(
            external.shape,
            sketchPlaneFromPlacement(sketchPlacement),
            Standard_False
        );
        maker.Approximation(Standard_True);
        maker.Build();
        if (!maker.IsDone()) {
            return false;
        }
        const TopoDS_Shape sectionShape = maker.Shape();
        if (sectionShape.IsNull()) {
            return false;
        }

        bool added = false;
        TopTools_IndexedMapOfShape edgeVertices;
        for (TopExp_Explorer explorer(sectionShape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            if (!projectExternalEdgeIntoResult(edge, sketchPlacement, result, defining)) {
                return false;
            }
            TopExp::MapShapes(edge, TopAbs_VERTEX, edgeVertices);
            added = true;
        }

        for (TopExp_Explorer explorer(sectionShape, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
            const TopoDS_Vertex vertex = TopoDS::Vertex(explorer.Current());
            if (edgeVertices.Contains(vertex)) {
                continue;
            }
            result.points.push_back(pointInSketchLocalPlane(BRep_Tool::Pnt(vertex), sketchPlacement));
            if (defining) {
                result.definingPoints.push_back(SketchPoint {0U, result.points.back(), false});
            }
            added = true;
        }
        return added;
    }
    catch (const Standard_Failure&) {
        return false;
    }
}

std::optional<ExternalGeometryResult> rebuildExternalGeometry(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const gp_Trsf& sketchPlacement
)
{
    const auto* externalProperty = app::propertyValue(object, "ExternalGeometry");
    if (externalProperty == nullptr) {
        return ExternalGeometryResult {};
    }
    if (externalProperty->propertyType != "App::PropertyLinkSubList") {
        runtime::addDiagnostic(
            context.diagnostics,
            "error",
            "missing_property",
            "ExternalGeometry must be an App::PropertyLinkSubList",
            object.name,
            "ExternalGeometry",
            "runtime"
        );
        return std::nullopt;
    }

    ExternalGeometryResult result;
    const std::vector<app::Link> links = app::readLinks(object, "ExternalGeometry");
    const std::vector<ExternalGeometryType> externalTypes
        = readExternalGeometryTypes(object, links.size());
    const auto nativeExternalGeometry = readNativeExternalGeometry(object, context);
    if (!nativeExternalGeometry) {
        return std::nullopt;
    }
    std::map<std::size_t, ExternalGeometryFlags> stateUpdates;
    std::set<std::size_t> detachedLinks;
    for (std::size_t index = 0; index < links.size(); ++index) {
        const auto& link = links.at(index);
        const std::string refKey = externalGeometryReferenceKey(link);
        ExternalGeometryFlags flags = externalGeometryFlags(link);
        if (flags.defining) {
            ++result.definingLinkCount;
        }
        if (flags.frozen) {
            ++result.frozenLinkCount;
        }
        if (flags.detached) {
            ++result.detachedLinkCount;
        }
        if (flags.missing) {
            ++result.missingLinkCount;
        }
        if (flags.sync) {
            ++result.syncLinkCount;
        }
        if (flags.detached) {
            detachedLinks.insert(index);
            appendNativeExternalGeometryForRef(
                result,
                *nativeExternalGeometry,
                refKey,
                flags.defining
            );
            continue;
        }
        const bool unresolvedMissingOldExternal = flags.missing && !flags.sync
            && context.shapes.find(link.object) == context.shapes.end();
        if ((flags.frozen && !flags.sync) || unresolvedMissingOldExternal) {
            // FreeCAD:
            // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
            // ::SketchObject::rebuildExternalGeometry(), for frozen refs, inserts the key into
            // "refSet" and keeps the old ExternalGeo entries; for unresolved Missing refs, the
            // pre-pass says the "linked external geometry will continue to work". cad-core first
            // consumes the native request-side ExternalGeo pool and only falls back to
            // ReferenceShadow.brep single-subshape evidence when no native geometry is present.
            if (appendNativeExternalGeometryForRef(
                    result,
                    *nativeExternalGeometry,
                    refKey,
                    flags.defining
                )) {
                continue;
            }
            if (auto oldExternal = oldExternalSubshapeFromBrepSnapshot(link, object, context)) {
                const ExternalGeometryType externalType = externalTypes.at(index);
                const bool projection = externalType == ExternalGeometryType::Projection
                    || externalType == ExternalGeometryType::Both;
                const bool intersection = externalType == ExternalGeometryType::Intersection
                    || externalType == ExternalGeometryType::Both;
                if (projection && oldExternal->kind == TopAbs_VERTEX) {
                    result.points.push_back(pointInSketchLocalPlane(
                        BRep_Tool::Pnt(TopoDS::Vertex(oldExternal->shape)),
                        sketchPlacement
                    ));
                    if (flags.defining) {
                        result.definingPoints.push_back(SketchPoint {0U, result.points.back(), false});
                    }
                }
                if (projection && oldExternal->kind == TopAbs_FACE) {
                    if (!projectExternalFaceBoundary(
                            TopoDS::Face(oldExternal->shape),
                            sketchPlacement,
                            result,
                            flags.defining
                        )) {
                        runtime::addDiagnostic(
                            context.diagnostics,
                            "error",
                            "unsupported_geometry",
                            "ExternalGeometry currently projects old planar face boundary edges, "
                            "line edges, circle edges and ellipse edges",
                            object.name,
                            "ExternalGeometry",
                            "runtime",
                            link.object,
                            oldExternal->subname
                        );
                        return std::nullopt;
                    }
                }
                if (
                    projection && oldExternal->kind == TopAbs_EDGE
                    && !projectExternalEdgeIntoResult(
                        TopoDS::Edge(oldExternal->shape),
                        sketchPlacement,
                        result,
                        flags.defining
                    )
                ) {
                    runtime::addDiagnostic(
                            context.diagnostics,
                            "error",
                            "unsupported_geometry",
                            "ExternalGeometry currently projects old line edges, circle edges and "
                            "ellipse edges",
                            object.name,
                            "ExternalGeometry",
                            "runtime",
                            link.object,
                            oldExternal->subname
                        );
                    return std::nullopt;
                }
                if (intersection && !addExternalGeometryIntersection(
                        *oldExternal,
                        sketchPlacement,
                        result,
                        flags.defining
                    )) {
                    runtime::addDiagnostic(
                            context.diagnostics,
                            "error",
                            "unsupported_geometry",
                            "ExternalGeometry could not intersect old target with the sketch plane",
                            object.name,
                            "ExternalGeometry",
                            "runtime",
                            link.object,
                            oldExternal->subname
                        );
                    return std::nullopt;
                }
            }
            continue;
        }
        const ExternalGeometryType externalType = externalTypes.at(index);
        const bool projection = externalType == ExternalGeometryType::Projection
            || externalType == ExternalGeometryType::Both;
        const bool intersection = externalType == ExternalGeometryType::Intersection
            || externalType == ExternalGeometryType::Both;
        const auto externals = resolveExternalGeometryLink(link, object, context);
        if (!externals) {
            return std::nullopt;
        }
        if (flags.missing || flags.sync) {
            if (flags.missing) {
                ++result.recoveredMissingLinkCount;
            }
            flags.missing = false;
            flags.sync = false;
            stateUpdates[index] = flags;
        }
        // FreeCAD:
        // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
        // rebuildExternalGeometry() reads "ExternalGeometry" links and fills transient
        // "ExternalGeo" with projected construction geometry before Constraints.acceptGeometry().
        for (const ExternalSubshape& external : *externals) {
            if (projection && external.kind == TopAbs_VERTEX) {
                result.points.push_back(pointInSketchLocalPlane(
                    BRep_Tool::Pnt(TopoDS::Vertex(external.shape)),
                    sketchPlacement
                ));
                if (flags.defining) {
                    result.definingPoints.push_back(SketchPoint {0U, result.points.back(), false});
                }
            }

            if (projection && external.kind == TopAbs_FACE) {
                if (!projectExternalFaceBoundary(
                        TopoDS::Face(external.shape),
                        sketchPlacement,
                        result,
                        flags.defining
                    )) {
                    runtime::addDiagnostic(
                        context.diagnostics,
                        "error",
                        "unsupported_geometry",
                        "ExternalGeometry currently projects planar face boundary edges, line "
                        "edges, circle edges and ellipse edges",
                        object.name,
                        "ExternalGeometry",
                        "runtime",
                        link.object,
                        external.subname
                    );
                    return std::nullopt;
                }
            }

            if (
                projection && external.kind == TopAbs_EDGE
                && !projectExternalEdgeIntoResult(
                    TopoDS::Edge(external.shape),
                    sketchPlacement,
                    result,
                    flags.defining
                )
            ) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ExternalGeometry currently projects planar face boundary edges, line edges, "
                    "circle edges and ellipse edges",
                    object.name,
                    "ExternalGeometry",
                    "runtime",
                    link.object,
                    external.subname
                );
                return std::nullopt;
            }

            if (intersection && !addExternalGeometryIntersection(
                    external,
                    sketchPlacement,
                    result,
                    flags.defining
                )) {
                runtime::addDiagnostic(
                    context.diagnostics,
                    "error",
                    "unsupported_geometry",
                    "ExternalGeometry could not intersect target with the sketch plane",
                    object.name,
                    "ExternalGeometry",
                    "runtime",
                    link.object,
                    external.subname
                );
                return std::nullopt;
            }
        }
    }
    appendExternalGeometryFlagsUpdate(
        context,
        object,
        links,
        stateUpdates,
        "external_geometry_flags_sync"
    );
    appendExternalGeometryDetachUpdate(context, object, links, *nativeExternalGeometry, detachedLinks);
    return result;
}


} // namespace cad_core::sketcher
