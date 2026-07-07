#include "cad_core/sketcher/sketch_internal_result.h"

#include "cad_core/app/element_map.h"
#include "cad_core/part/property_topo_shape.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/part/topo_shape.h"

#include "sketch_object_operations.h"

#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <map>

namespace cad_core::sketcher
{

namespace
{

bool shapeHasEdges(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return false;
    }
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        return true;
    }
    return false;
}

void appendRawSketchDisplayTopology(nlohmann::json& mesh, const nlohmann::json& rawMesh)
{
    for (const char* field : {"edgeSegments", "vertexPoints"}) {
        for (const auto& item : rawMesh.at(field)) {
            mesh.at(field).push_back(item);
        }
    }
}

std::string stableSubnameForLedgerIdentity(const RawSketchEdgeIdentity& identity)
{
    if (identity.stableSubname) {
        return *identity.stableSubname;
    }
    if (identity.source.geometryId) {
        return stableSubnameForGeometryId(*identity.source.geometryId);
    }
    return {};
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0U) == 0U;
}

std::map<std::string, std::string> internalEdgeMappedNamesFromLedger(
    const RawSketchEdgeIdentityLedger& ledger,
    const nlohmann::json& internalElementMap
)
{
    std::map<std::string, std::string> stableByIndexed;
    for (const RawSketchEdgeIdentity& identity : ledger.edges) {
        const std::string stable = stableSubnameForLedgerIdentity(identity);
        if (!stable.empty()) {
            stableByIndexed[identity.indexed] = stable;
        }
    }

    std::map<std::string, std::string> mappedNames;
    for (const auto& [indexed, stable] : stableByIndexed) {
        if (startsWith(indexed, "InternalEdge")) {
            mappedNames[indexed] = stable;
        }
    }
    if (!internalElementMap.is_object()) {
        return mappedNames;
    }
    for (const auto& [internalIndexed, mapped] : internalElementMap.items()) {
        if (!startsWith(internalIndexed, "InternalEdge") || !mapped.is_string()) {
            continue;
        }
        const auto stableIt = stableByIndexed.find(mapped.get<std::string>());
        if (stableIt != stableByIndexed.end()) {
            mappedNames[internalIndexed] = stableIt->second;
        }
    }
    return mappedNames;
}

}  // namespace

SketchInternalResult buildSketchInternalResult(const SketchInternalResultInput& input)
{
    SketchInternalResult result {
        runtime::ShapeValue {runtime::ShapeValue::Kind::Sketch, input.rawShape}
    };
    RawSketchEdgeIdentityLedger rawEdgeIdentityLedger = input.rawEdgeIdentityLedger;
    result.shapeValue.profileShape = input.profileShape;
    result.shapeValue.profileNormal = input.profileNormal;
    result.shapeValue.internalShape = input.internalShape;
    result.shapeValue.profileRequiresSubshapeSelection = input.profileRequiresSubshapeSelection;

    const bool hasNonEmptyInternalShape = input.internalShape && !input.internalShape->IsNull();
    if (hasNonEmptyInternalShape) {
        nlohmann::json internalElementMap =
            app::internalElementMapForSketch(input.rawShape, *input.internalShape);
        part::NamedShape preliminaryInternalNamedShape = part::namedShapeForSketchInternalShape(
            input.objectName,
            input.rawShape,
            *input.internalShape,
            input.historyLedger
        );
        addSplitFragmentIdentitiesFromInternalHistory(
            rawEdgeIdentityLedger,
            preliminaryInternalNamedShape
        );
        result.shapeValue.internalNamedShape = part::namedShapeForSketchInternalShape(
            input.objectName,
            input.rawShape,
            *input.internalShape,
            input.historyLedger,
            internalEdgeMappedNamesFromLedger(rawEdgeIdentityLedger, internalElementMap)
        );
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::buildInternals(), writes auxiliary "InternalShape"; the web response
        // renders that request-local shape with InternalFace ids matching subshapes.
        result.mesh
            = part::meshForShape(*input.internalShape, "InternalFace", "InternalEdge", "InternalVertex");
        if (!input.rawShape.IsNull()) {
            // FreeCAD keeps the public Sketch Shape namespace alive beside InternalShape. Publish
            // raw edge/vertex display geometry too, so raw EdgeN items that are not retained in
            // InternalShape still have mesh data for rendering and picking.
            appendRawSketchDisplayTopology(*result.mesh, part::meshForShape(input.rawShape));
        }
    }
    else if (shapeHasEdges(input.rawShape)) {
        // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp
        // ::SketchObject::execute(), keeps the raw sketch "Shape" even when buildInternals()
        // cannot build an "InternalShape" from an open wire. cad-core publishes that raw
        // EdgeN/VertexN display mesh for picking without synthesizing Internal* elements.
        result.mesh = part::meshForShape(input.rawShape);
    }
    if (!input.rawShape.IsNull()) {
        result.rawNamedShape = namedShapeForSketchRawEdgeIdentity(
            input.objectName,
            input.rawShape,
            rawEdgeIdentityLedger
        );
    }

    const nlohmann::json internalSubshapes = hasNonEmptyInternalShape
        ? part::subshapeMapForShape(*input.internalShape, "Internal")
        : nlohmann::json::object();
    const nlohmann::json internalElementMap = hasNonEmptyInternalShape
        ? app::internalElementMapForSketch(input.rawShape, *input.internalShape)
        : nlohmann::json::object();
    if (!input.rawShape.IsNull()) {
        result.subshapes = part::subshapeMapForShape(input.rawShape);
        if (hasNonEmptyInternalShape) {
            for (const auto& item : internalSubshapes.items()) {
                result.subshapes[item.key()] = item.value();
            }
        }
    }
    if (result.mesh && !result.subshapes.empty()) {
        RawSketchEdgeIdentityLedger responseEdgeIdentityLedger = rawEdgeIdentityLedger;
        addInternalEdgeIdentitiesFromInternalElementMap(
            responseEdgeIdentityLedger,
            internalElementMap
        );
        publishRawSketchEdgeIdentity(
            *result.mesh,
            result.subshapes,
            responseEdgeIdentityLedger
        );
    }

    result.objectFields = {
        {"profile", profileShapeLabel(input.profileShape)},
        {"profile_ready", input.profileShape.has_value()},
        {"internal_shape",
         input.internalShape ? (input.internalShape->IsNull() ? "empty" : "occt_internal_shape")
                             : "none"},
        {"internal_face_count", countSubshapesOfKind(internalSubshapes, "face")},
        {"internal_edge_count", countSubshapesOfKind(internalSubshapes, "edge")},
        {"internal_vertex_count", countSubshapesOfKind(internalSubshapes, "vertex")},
        {"internal_element_map", internalElementMap},
        {"raw_edge_identity", rawSketchEdgeIdentityObject(rawEdgeIdentityLedger)},
    };
    if (input.historyLedger) {
        result.objectFields["internal_shape_history_diagnostics"] =
            input.historyLedger->diagnosticsJson();
    }

    return result;
}

}  // namespace cad_core::sketcher
