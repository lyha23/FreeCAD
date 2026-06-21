#include "cad_core/adapters/c_api.h"

#include "cad_core/app/document.h"
#include "cad_core/part/part_geometry_curve.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <cstdint>
#include <exception>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

CadCoreBuffer emptyBuffer()
{
    return CadCoreBuffer {nullptr, 0};
}

bool copyBuffer(std::string_view value, CadCoreBuffer& buffer) noexcept
{
    buffer = CadCoreBuffer {nullptr, value.size()};
    if (value.empty()) {
        return true;
    }

    buffer.ptr = new (std::nothrow) char[value.size()];
    if (buffer.ptr == nullptr) {
        buffer.len = 0;
        return false;
    }
    std::memcpy(buffer.ptr, value.data(), value.size());
    return true;
}

CadCoreResult makeErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreResult {status, emptyBuffer(), error};
}

CadCoreResult makeJsonResult(const nlohmann::json& payload)
{
    CadCoreBuffer json = emptyBuffer();
    if (!copyBuffer(payload.dump(), json)) {
        return makeErrorResult(2, "cad-core FFI failed to allocate result buffer");
    }
    return CadCoreResult {0, json, emptyBuffer()};
}

CadCoreExportResult makeExportErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreExportResult {status, emptyBuffer(), emptyBuffer(), error};
}

CadCoreExportResult makeExportResult(std::string_view data, const nlohmann::json& payload)
{
    CadCoreBuffer dataBuffer = emptyBuffer();
    if (!copyBuffer(data, dataBuffer)) {
        return makeExportErrorResult(2, "cad-core FFI failed to allocate export buffer");
    }

    CadCoreBuffer jsonBuffer = emptyBuffer();
    if (!copyBuffer(payload.dump(), jsonBuffer)) {
        delete[] dataBuffer.ptr;
        return makeExportErrorResult(2, "cad-core FFI failed to allocate export metadata buffer");
    }

    return CadCoreExportResult {0, dataBuffer, jsonBuffer, emptyBuffer()};
}

nlohmann::json cadCoreVersionJson()
{
    return {
        {"version", "0.1.0"},
        {"api", "cad_core_ffi"},
        {"kernel", cad_core::part::kernelVersion()},
    };
}

nlohmann::json diagnosticCodeList()
{
    return nlohmann::json::array({
        "approximation_failed",
        "conflicting_property",
        "cycle_dependency",
        "deleted_stable_subname",
        "document_hash_mismatch",
        "duplicate_object_id",
        "duplicate_object_name",
        "execution_failed",
        "external_document_pending_reload",
        "external_document_unloaded",
        "invalid_angle",
        "invalid_axis",
        "invalid_curve_source",
        "invalid_curve2d_source",
        "invalid_direction",
        "invalid_length",
        "invalid_link_value",
        "invalid_assembly_solver_result",
        "invalid_parameter",
        "invalid_part_conic_axis",
        "invalid_part_conic_curve_kind",
        "invalid_part_conic_focal",
        "invalid_part_conic_number",
        "invalid_part_conic_radius",
        "invalid_part_conic_trim",
        "invalid_placement",
        "invalid_point_constraint",
        "invalid_point2d_source",
        "invalid_property_type",
        "invalid_surface_source",
        "invalid_subshape",
        "invalid_taper",
        "label_reference_ambiguous",
        "missing_external_document",
        "missing_external_geometry_snapshot",
        "missing_link_target",
        "missing_constraints",
        "missing_curve_source",
        "missing_object",
        "missing_property",
        "missing_surface_source",
        "missing_target",
        "missing_grounded_part",
        "mesh_limit_exceeded",
        "multiple_solids_disallowed",
        "no_intersection",
        "ondsel_solver_failed",
        "partdesign_body_tip_non_solid",
        "open_profile",
        "parse_error",
        "perform_failed",
        "refine_failed",
        "sketch_solver_conflict",
        "sketch_solver_malformed_constraint",
        "sketch_solver_redundant",
        "split_stable_subname",
        "surface_not_done",
        "subname_deleted",
        "subname_resolve_ambiguous",
        "subname_resolve_failed",
        "subname_semantic_drift",
        "subname_split_requires_reselect",
        "unsupported_geometry",
        "unsupported_assembly_solver",
        "adapter_resource_limit",
        "unsupported_link_lifecycle",
        "unsupported_profile_region",
        "unsupported_property",
        "unsupported_reference_shadow_brep",
        "unsupported_sketch_constraint_relation",
        "unsupported_stable_subname",
        "unsupported_subshape_kind",
        "unsupported_type",
    });
}

nlohmann::json adapterResourceLimitDiagnostic(
    const std::string& message,
    const std::string& object,
    const std::string& property,
    const std::string& target,
    const nlohmann::json& details
)
{
    nlohmann::json diagnostic = {
        {"severity", "error"},
        {"code", "adapter_resource_limit"},
        {"message", message},
        {"object", object},
        {"property", property},
        {"stage", "adapter"},
        {"target", target},
    };
    if (!details.is_null() && (!details.is_object() || !details.empty())) {
        diagnostic["details"] = details;
    }
    return diagnostic;
}

void appendAdapterResourceLimitDiagnostic(
    nlohmann::json& payload,
    const std::string& message,
    const std::string& object,
    const std::string& property,
    const std::string& target,
    const nlohmann::json& details
)
{
    if (!payload.contains("diagnostics") || !payload["diagnostics"].is_array()) {
        payload["diagnostics"] = nlohmann::json::array();
    }
    payload["diagnostics"].push_back(
        adapterResourceLimitDiagnostic(message, object, property, target, details)
    );
}

nlohmann::json ondselSolverCapabilityJson()
{
    return {
        {"status", "covered_full"},
        {"mode", "request_local_runPreDrag"},
        {"available", true},
        {"build_mode", "CAD_CORE_HAS_ONDSEL_SOLVER=1"},
        {"covered",
         {"grounded_fixed_joint",
          "grounded_ball_joint",
          "grounded_revolute_joint",
          "grounded_slider_joint",
          "grounded_distance_joint",
          "grounded_parallel_joint",
          "grounded_perpendicular_joint",
          "grounded_angle_joint",
          "grounded_gears_joint",
          "grounded_belt_joint",
          "grounded_rackpinion_joint",
          "grounded_screw_joint",
          "basic_distance_type",
          "distance_type_extended_geometry",
          "subshape_marker_placement",
          "runPreDrag",
          "grounded_placement_validation",
          "invalid_grounded_placement_rejected"}},
        {"distance_type_basic_geometry",
         {
             {"status", "covered_full"},
             {"mode", "request_local_getDistanceType_makeMbdJointDistance_basic"},
             {"supported",
              {"PointPoint", "LineLine", "PointLine", "PlanePlane", "PointPlane", "LinePlane"}},
             {"solver_joint_classes",
              {{"PointPoint", {"ASMTSphericalJoint", "ASMTSphSphJoint"}},
               {"LineLine", {"ASMTRevCylJoint"}},
               {"PointLine", {"ASMTLineInPlaneJoint"}},
               {"PlanePlane", {"ASMTPlanarJoint"}},
               {"PointPlane", {"ASMTPointInPlaneJoint"}},
               {"LinePlane", {"ASMTLineInPlaneJoint"}}}},
             {"source_notes",
              {{"PointLine",
                "FreeCADCmd native marker parity exports tInPlaneJointE/offset; nearby C++ source "
                "switch "
                "historically names ASMTCylSphJoint."}}},
             {"scalar_fields", {"distance_ij", "offset"}},
             {"remaining_radius_gaps", nlohmann::json::array()},
             {"extended_geometry_capability", "distance_type_extended_geometry"},
             {"non_goals", {"GUI/session", "persistent_solver_state"}},
         }},
        {"distance_type_extended_geometry",
         {
             {"status", "covered_supported_subset"},
             {"mode", "request_local_getDistanceType_makeMbdJointDistance_extended"},
             {"build_mode", "CAD_CORE_HAS_ONDSEL_SOLVER=1"},
             {"native_expected_count", 13},
             {"supported",
              {"LineCircle",
               "CircleCircle",
               "PlaneCylinder",
               "PlaneSphere",
               "CylinderCylinder",
               "CylinderSphere",
               "PointCylinder",
               "PointSphere",
               "PlaneTorus",
               "CylinderTorus",
               "TorusTorus",
               "TorusSphere",
               "SphereSphere"}},
             {"solver_joint_classes",
              {{"LineCircle", {"ASMTRevCylJoint"}},
               {"CircleCircle", {"ASMTRevCylJoint"}},
               {"PlaneCylinder", {"ASMTLineInPlaneJoint"}},
               {"PlaneSphere", {"ASMTPointInPlaneJoint"}},
               {"CylinderCylinder", {"ASMTRevCylJoint"}},
               {"CylinderSphere", {"ASMTCylSphJoint"}},
               {"PointCylinder", {"ASMTCylSphJoint"}},
               {"PointSphere", {"ASMTSphSphJoint"}},
               {"PlaneTorus", {"ASMTPlanarJoint"}},
               {"CylinderTorus", {"ASMTRevCylJoint"}},
               {"TorusTorus", {"ASMTPlanarJoint"}},
               {"TorusSphere", {"ASMTCylSphJoint"}},
               {"SphereSphere", {"ASMTSphSphJoint"}}}},
             {"scalar_fields", {"distance_ij", "offset"}},
             {"evidence_fields",
              {"reference1_primitive",
               "reference2_primitive",
               "reference1_radius",
               "reference2_radius",
               "reference1_radius_source",
               "reference2_radius_source"}},
             {"scalar_correction_fields",
              {"scalar_correction", "scalar_correction_source", "radius_source_side"}},
             {"request_local_boundaries",
              {"identity_offset_assembly_link_subset",
               "radius_correction_uses_getEdgeRadius_getFaceRadius",
               "distance_writeback_uses_user_distance_not_radius_corrected_scalar"}},
             {"deferred_diagnostic_cases", {"PointCurve"}},
             {"default_or_todo_boundaries",
              {"PlaneCone",
               "CylinderCone",
               "ConeCone",
               "ConeTorus",
               "ConeSphere",
               "PointCone",
               "PointTorus",
               "LineCylinder",
               "LineSphere",
               "LineCone",
               "LineTorus",
               "CurvePlane",
               "CurveCylinder",
               "CurveSphere",
               "CurveCone",
               "CurveTorus",
               "Other"}},
             {"diagnostic_expected_count", 5},
             {"non_goals",
              {"PointCurve",
               "default_or_todo_branch_support",
               "GUI/session",
               "persistent_solver_state",
               "non_identity_bundled_offsetPlc"}},
         }},
        {"subshape_marker_placement",
         {
             // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
             // ::AssemblyObject::handleOneSideOfJoint(), applies "getGlobalPlacement(nullptr,
             // ref)", then "getGlobalPlacement(part, ref).inverse()" before "offsetPlc".
             {"status", "covered_representative_subset"},
             {"mode", "request_local_handleOneSide_markerPlacement"},
             {"build_mode", "CAD_CORE_HAS_ONDSEL_SOLVER=1"},
             {"supported_reference_kinds", {"object", "Vertex", "Edge", "Face", "mixed"}},
             {"covered",
              {"object_level_baseline",
               "vertex_jcs_marker",
               "edge_jcs_marker",
               "face_jcs_marker",
               "assembly_link_identity_offset_subshape_marker",
               "non_linear_edge_and_non_planar_face_identity_offset",
               "mixed_swap_marker_sync",
               "real_ondsel_marker_consumption",
               "placement_updates_native_parity"}},
             {"active_expected_count", 28},
             {"active_expected_groups", {"S4/S5 marker expected", "S6 extended DistanceType expected"}},
             {"request_local_boundaries",
              {"identity_offset_assembly_link_subset", "request_graph_no_persistent_solver_state"}},
             {"non_goals",
              {"curve_default_distance_type",
               "GUI/session",
               "persistent_solver_state",
               "non_assembly_link_subshape_primitive_frame_generalization",
               "non_identity_bundled_offsetPlc"}},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"remaining_gaps", nlohmann::json::array()},
    };
}

nlohmann::json placementWritebackCapabilityJson()
{
    return {
        {"status", "covered_full"},
        {"mode", "request_local_runPreDrag"},
        {"build_mode", "CAD_CORE_HAS_ONDSEL_SOLVER=1"},
        {"solver_modes", nlohmann::json::array({"real_ondsel_solver"})},
        {"updates", {"documentObjectUpdates.action=assembly_set_placement"}},
        {"covered",
         {"solver_placement_delta",
          "unchanged_noop",
          "invalid_grounded_placement_rejected",
          "request_graph_apply_next_recompute_noop",
          "multi_component_writeback_order",
          "partial_writeback_subset",
          "target_field_Placement"}},
        {"remaining_gaps", nlohmann::json::array()},
    };
}

nlohmann::json representativeSolverCapabilityJson()
{
    return {
        {"status", "covered_representative"},
        {"mode", "fallback_metadata_only"},
        {"available", false},
        {"build_mode", "CAD_CORE_HAS_ONDSEL_SOLVER=1"},
        {"reason",
         "current cad-core build requires the real Ondsel adapter; representative output is "
         "retained only as fallback contract metadata"},
        {"diagnostics", {"missing_grounded_part", "unsupported_assembly_solver"}},
        {"non_goals", {"full_solver", "persistent_solver_state", "cross_request_assembly_session"}},
    };
}

nlohmann::json assemblyValidationCapabilityJson()
{
    return {
        {"status", "covered_diagnostic_boundaries"},
        {"mode", "request_local_product_validation"},
        {"diagnostic_codes",
         {"unsupported_assembly_solver",
          "missing_grounded_part",
          "ondsel_solver_failed",
          "invalid_assembly_solver_result"}},
        {"fixture_rows",
         {"assembly-runtime-adapter-missing-grounded-part-diagnostic",
          "assembly-runtime-adapter-pointcurve-unsupported-diagnostic",
          "assembly-runtime-adapter-partial-writeback"}},
        {"source_boundaries",
         {"AssemblyObject.cpp::solve() real runPreDrag path",
          "AssemblyObject.cpp::setNewPlacements() request-local Placement writeback",
          "AssemblyObject.cpp::validateNewPlacements() is a drag validation boundary, not a "
          "backend session"}},
        {"request_local_boundaries",
         {"documentObjectUpdates_only",
          "frontend_graph_is_source_of_truth",
          "no_backend_assembly_session"}},
    };
}

nlohmann::json capabilitiesJson()
{
    const cad_core::runtime::FeatureRegistry registry = cad_core::runtime::buildDefaultRegistry();
    const auto linkSubShapeFields = nlohmann::json::array(
        {"value",
         "SubList",
         "StableSubList",
         "FullSubList",
         "ShadowSub",
         "ReferenceShadow",
         "ExternalFlags",
         "Document"}
    );
    return {
        {"status", "ok"},
        {"schema_version", "cad-web-v1"},
        {"cad_core", cadCoreVersionJson()},
        {"document",
         {
             {"source", "DocumentObject graph"},
             {"required_object_fields", {"Name", "ID", "TypeId", "Properties"}},
             {"link_property_fields",
              {"value",
               "values",
               "SubList",
               "StableSubList",
               "FullSubList",
               "ShadowSub",
               "ReferenceShadow",
               "ExternalFlags",
               "Document",
               "SubSet"}},
             {"document_reference_fields",
              {"file",
               "name",
               "label",
               "stamp",
               "status",
               "currentName",
               "currentLabel",
               "currentStamp",
               "currentStatus",
               "allowPartial"}},
             {"external_geometry_native_slot_fields",
              {"ExternalGeo", "Geometry", "Values", "Items", "Ref", "RefIndex", "ExternalFlags", "Flags"}},
             {"external_geometry_flags", {"Defining", "Frozen", "Detached", "Missing", "Sync"}},
             {"external_geometry_lifecycle",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                  // ::SketchObject::rebuildExternalGeometry(), toggles "Missing" from whether
                  // "refSet" contains the rebuilt reference and clears "Sync" after rebuild.
                  {"state_updates",
                   {"sync_clears_sync_flag",
                    "missing_clears_when_reference_resolves",
                    "frozen_reuses_reference_shadow_brep",
                    "missing_unresolved_reuses_reference_shadow_brep",
                    "frozen_reuses_native_external_geo",
                    "missing_unresolved_reuses_native_external_geo",
                    "detached_keeps_native_external_geo_and_clears_ref",
                    "detached_removes_external_geometry_link"}},
                  {"request_local_boundaries",
                   {"frozen_current_source_without_reference_shadow_brep_skips_projection",
                    "native_external_geo_pool_is_request_local_not_backend_session"}},
                  {"diagnostics", {"missing_external_geometry_snapshot"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"link_property_shapes",
              {
                  {"App::PropertyLink", {"value"}},
                  {"App::PropertyLinkGlobal", {"value"}},
                  {"App::PropertyLinkHidden", {"value"}},
                  // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h
                  // ::PropertyXLink is a cross-document link with "_SubList";
                  // ::PropertyXLinkList first accepts object-only PropertyLinkList syntax, then
                  // falls back to PropertyXLinkSubList for sub-element entries.
                  {"App::PropertyXLink", linkSubShapeFields},
                  {"App::PropertyLinkList", {"values"}},
                  {"App::PropertyLinkListHidden", {"values"}},
                  {"App::PropertyXLinkList", {"values", "SubSet"}},
                  {"App::PropertyLinkSub", linkSubShapeFields},
                  {"App::PropertyLinkSubHidden", linkSubShapeFields},
                  {"App::PropertyXLinkSub", linkSubShapeFields},
                  {"App::PropertyXLinkSubHidden", linkSubShapeFields},
                  {"App::PropertyLinkSubList", {"SubSet"}},
                  {"App::PropertyLinkSubListHidden", {"SubSet"}},
                  {"App::PropertyXLinkSubList", {"SubSet"}},
              }},
             {"document_update_channels", {"elementReferenceUpdates", "documentObjectUpdates"}},
         }},
        {"link_transaction",
         {
             // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
             // ::LinkBaseExtension::update(), ShowElement false preserves "PlacementList" /
             // "ScaleList" before removeObject(); ElementCount true path creates or re-claims
             // LinkElement children, and ::syncElementList() writes "_LinkOwner",
             // "LinkTransform" and "LinkedObject" back to owned children.
             {"document_object_updates",
              {"show_element_create",
               "show_element_claim",
               "show_element_sync",
               "show_element_delete",
               "show_element_toggle_off",
               "child_cache_create",
               "child_cache_nested_plain_group",
               "child_cache_orphan_reclaim",
               "child_cache_stale_delete",
               "element_count_owner_lists_sync",
               "element_list_owner_sync",
               "element_list_child_sync",
               "copy_on_change_owned_child_sync",
               "copy_on_change_writeback_contract",
               "copy_on_change_group_sync",
               "copy_on_change_deep_copy_lifecycle",
               "copy_on_change_owned_child_mutation",
               "copy_on_change_touched_tracking"}},
             {"copy_on_change_writeback_contract",
              {
                  {"status", "covered"},
                  {"updates", {"copy_on_change_group_sync", "copy_on_change_deep_copy_lifecycle"}},
                  {"scope", "documentObjectUpdates transport for persisted CopyOnChange copied graph"},
              }},
             {"copy_on_change_deep_copy_lifecycle",
              {
                  {"status", "covered_full"},
                  {"covered",
                   {"copy_on_change_property_tree_copy",
                    "copy_on_change_child_object_copy",
                    "copy_on_change_internal_link_relink",
                    "copy_on_change_dependency_graph_rewrite",
                    "copy_on_change_history_preserve",
                    "copy_on_change_sync_copy_on_change"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"writeback_properties",
              {"ElementList",
               "ElementCount",
               "PlacementList",
               "ScaleList",
               "VisibilityList",
               "LinkedObject",
               "LinkCopyOnChange",
               "LinkCopyOnChangeSource",
               "LinkCopyOnChangeGroup",
               "LinkCopyOnChangeTouched",
               "_LinkOwner",
               "LinkTransform",
               "_CopyOnChangeControl",
               "_CopyOnChangeOwner",
               "_CopyOnChangeSourceObject",
               "_CopyOnChangeSourceId"}},
             {"request_local_boundaries",
              {"plain_group_child_cache_updates_are_document_object_updates",
               "show_element_missing_children_are_create_updates",
               "copy_on_change_keeps_request_graph_immutable",
               "copy_on_change_frontend_persists_copied_graph_between_requests"}},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"link_reference_lifecycle",
         {
             // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
             // ::LinkBaseExtension::checkGeoElementMap() builds "Data::POSTFIX_EXTERNAL_TAG"
             // retag postfix evidence for external links; ::parseSubName() keeps PropertyXLink
             // subvalues so getTrueLinkedObject() can resolve multi-level link subobjects.
             {"retag_aliases",
              {"full_sublist_external_tag",
               "mapped_postfix_alias",
               "source_prefixed_stable_key",
               "label_qualified_subname",
               "multi_level_link_subname",
               "property_xlink_list_subset_compound"}},
             {"reference_update_fields",
              {"FullSubList",
               "StableSubList",
               "ShadowSub",
               "ReferenceShadow",
               "ExternalFlags",
               "labelReferenceRename",
               "documentReference"}},
             {"reference_recovery",
              {"source_object_rename_by_reference_shadow_target_id",
               "label_rename_by_link_target_label",
               "label_rename_nested_by_link_group_path",
               "label_rename_cross_document_nested_by_full_sublist",
               "document_name_label_restore",
               "missing_external_document_diagnostic",
               "external_document_pending_reload_diagnostic",
               "external_document_unloaded_diagnostic"}},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"topo_reference_pressure",
         {
             {"status", "done_c4m4_topo_reference_pressure"},
             {"fixtures",
              {"topo-reference-pressure-multi-producer-updated",
               "topo-reference-pressure-rename-label-updated",
               "topo-reference-pressure-rename-label-ambiguous-diagnostic",
               "topo-reference-pressure-link-retag-updated",
               "topo-reference-pressure-copy-on-change-owned-child",
               "topo-reference-pressure-import-unchanged",
               "topo-reference-pressure-import-change-deleted",
               "topo-reference-pressure-import-change-ambiguous-diagnostic",
               "topo-reference-pressure-shapefix-refine-updated",
               "topo-reference-pressure-shapefix-refine-deleted",
               "topo-reference-pressure-boolean-split-needs-reselect",
               "topo-reference-pressure-dressup-transformed-updated"}},
             {"classifications",
              {
                  {"updated",
                   {"C4M4-TR-PRESS-001",
                    "C4M4-TR-PRESS-002",
                    "C4M4-TR-PRESS-004",
                    "C4M4-TR-PRESS-005",
                    "C4M4-TR-PRESS-009",
                    "C4M4-TR-PRESS-012"}},
                  {"unchanged", {"C4M4-TR-PRESS-006"}},
                  {"needs_reselect", {"C4M4-TR-PRESS-007", "C4M4-TR-PRESS-010", "C4M4-TR-PRESS-011"}},
                  {"diagnostic_only", {"C4M4-TR-PRESS-003", "C4M4-TR-PRESS-008"}},
              }},
             {"update_fields",
              {"elementReferenceUpdates",
               "documentObjectUpdates",
               "StableSubList",
               "ShadowSub",
               "ReferenceShadow",
               "labelReferenceRename"}},
             {"diagnostics",
              {"label_reference_ambiguous",
               "deleted_stable_subname",
               "split_stable_subname",
               "subname_resolve_ambiguous"}},
             {"request_local_boundaries",
              {"ReferenceShadow.brep_single_subshape_only",
               "split_deleted_ambiguous_remain_reselect_or_diagnostic",
               "no_output_order_bbox_or_fixture_name_alias",
               "adapter_publishes_contract_metadata_only"}},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"sketcher",
         {
             {"solver",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp
                  // ::SketchObject::solve(), "At this point we have the solver information about
                  // conflicting/redundant/over-constrained"; the same file records
                  // "lastHasMalformedConstraints = solvedSketch.hasMalformedConstraints()".
                  // SketchObject.cpp::execute() returns "Sketch with conflicting constraints",
                  // "Sketch with redundant constraints", and "Sketch with malformed constraints";
                  // the same solve() stores "lastDoF", while execute() proceeds to "buildShape()"
                  // when no solver error is returned. The same solve() path moves
                  // "solvedSketch.extractGeometry()" into "Geometry" when
                  // "updateGeoAfterSolving" is true. Sketch.cpp::Sketch::addConstraint()
                  // routes DistanceX / DistanceY single-point constraints through
                  // "addCoordinateXConstraint" / "addCoordinateYConstraint"; it routes Circle
                  // Radius / Diameter through "addConstraintCircleRadius/Diameter"; Distance
                  // line length calls "GCSsys.addConstraintP2PDistance(l.p1, l.p2, ...)" and
                  // arc length calls "GCSsys.addConstraintArcLength(a, ...)". Sketch.h documents
                  // that "positive degrees of freedom correspond to an under-constrained sketch".
                  // PointOnObject on a Line target calls
                  // "GCSsys.addConstraintPointOnLine(p1, l2, tag, driving)". Parallel line pairs
                  // route through "addParallelConstraint(constraint->First, constraint->Second)"
                  // and then "GCSsys.addConstraintParallel(l1, l2, tag)"; simple
                  // Perpendicular line pairs route through "addPerpendicularConstraint" and then
                  // "GCSsys.addConstraintPerpendicular(l1, l2, tag)". Line + Circle/Arc
                  // Perpendicular uses "Points[Geoms[geoId2].midPointId]" and
                  // "GCSsys.addConstraintPointOnLine(p2, l1, tag)". Equal constraints route to
                  // "GCSsys.addConstraintEqualLength" for Lines and
                  // "GCSsys.addConstraintEqualRadius" for Circle/Arc combinations. Tangent
                  // Line + Circle/Arc routes to "GCSsys.addConstraintTangent(l, c, ...)" /
                  // "GCSsys.addConstraintTangent(l, a, ...)". Symmetric point pairs about a
                  // Line route to "GCSsys.addConstraintP2PSymmetric(p1, p2, l, tag)";
                  // Symmetric point pairs about a center point route to
                  // "GCSsys.addConstraintP2PSymmetric(p1, p2, p, tag)". Arc endpoint writeback
                  // follows Sketch::updateArcOfCircle(), which writes "setRange(*myArc.startAngle,
                  // *myArc.endAngle, ...)" after solving.
                  {"status", "done_c4m3_constraint_facing_audit"},
                  {"diagnostics",
                   {"sketch_solver_conflict",
                    "sketch_solver_malformed_constraint",
                    "sketch_solver_partially_redundant",
                    "sketch_solver_redundant",
                    "unsupported_sketch_constraint_relation"}},
                  {"covered",
                   {"horizontal_vertical_same_target_conflict",
                    "malformed_constraint_diagnostics",
                    "partial_redundancy_diagnostics",
                    "duplicate_orientation_constraint_redundant",
                    "conflicting_same_target_datums",
                    "duplicate_same_target_datums",
                    "unconstrained_geometry_underconstrained_state",
                    "whole_line_orientation_solver_geometry_update",
                    "endpoint_coordinate_solver_geometry_update",
                    "circle_radius_diameter_solver_geometry_update",
                    "line_length_solver_geometry_update",
                    "arc_length_solver_geometry_update",
                    "point_on_object_line_solver_geometry_update",
                    "parallel_line_pair_solver_geometry_update",
                    "perpendicular_line_pair_solver_geometry_update",
                    "perpendicular_line_circle_arc_midpoint_solver_geometry_update",
                    "equal_line_circle_arc_solver_geometry_update",
                    "tangent_line_circle_arc_solver_geometry_update",
                    "symmetric_line_axis_solver_geometry_update",
                    "symmetric_arc_endpoint_solver_geometry_update",
                    "symmetric_center_point_solver_geometry_update",
                    "symmetric_coupled_curve_relation_solver_geometry_update",
                    "solver_dof_driven_underconstrained_state",
                    "full_solver_dof",
                    "dependent_parameter_group_analysis",
                    "unsupported_relation_adapter_visible_diagnostic"}},
                  {"request_local_boundaries",
                   {"diagnostics_only_without_backend_solver_session",
                    "conflict_or_redundant_blocks_profile_output",
                    "malformed_blocks_profile_output",
                    "whole_line_orientation_update_without_full_solver_session",
                    "endpoint_coordinate_update_without_full_solver_session",
                    "circle_radius_diameter_update_without_full_solver_session",
                    "line_length_update_preserves_start_point_without_full_solver_session",
                    "arc_length_update_scales_radius_without_full_solver_session",
                    "point_on_object_line_projection_without_full_solver_session",
                    "line_pair_parallel_update_preserves_second_start_without_full_solver_session",
                    "line_pair_perpendicular_update_preserves_second_start_without_full_solver_"
                    "session",
                    "line_circle_arc_perpendicular_midpoint_projection_without_full_solver_session",
                    "equal_relation_updates_second_geometry_without_full_solver_session",
                    "tangent_line_circle_arc_updates_round_center_without_full_solver_session",
                    "symmetric_line_axis_updates_second_point_without_full_solver_session",
                    "symmetric_arc_endpoint_updates_second_arc_angle_without_full_solver_session",
                    "symmetric_center_point_updates_second_point_without_full_solver_session",
                    "unsupported_relation_fails_without_fake_profile"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"external_internal_pressure",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                  // ::SketchObject::rebuildExternalGeometry(), skips frozen entries, clears
                  // "Missing" when a reference resolves, and keeps unresolved entries working
                  // from old external geometry. SketchObject.cpp::buildInternals() calls
                  // "Part::FaceMakerBuildFace" and appends "WireJoiner" open wires; FaceMaker.cpp
                  // consumes "myPreSplitHistory", while WireJoiner.cpp records "aHistory".
                  {"status", "done_c4m3_external_internal_pressure"},
                  {"expected_backed",
                   {"external_geometry_frozen_native_pool",
                    "external_geometry_detached_native_pool",
                    "external_geometry_missing_recovered",
                    "internal_shape_open_profile_empty",
                    "internal_shape_bounded_cross_cutters",
                    "internal_shape_self_intersection_bowtie",
                    "internal_shape_split_dangling_mixed"}},
                  {"reference_shadow",
                   {"single_subshape_snapshot_only",
                    "external_geometry_brep_snapshot_request_local",
                    "internal_shape_edge_stable_shadow_sub_update"}},
                  {"deferred_diagnostics",
                   {"missing_external_geometry_snapshot",
                    "unsupported_reference_shadow_brep",
                    "subname_split_requires_reselect",
                    "subname_deleted",
                    "subname_resolve_ambiguous",
                    "deleted_stable_subname"}},
                  {"request_local_boundaries",
                   {"no_full_brep_document_state",
                    "no_backend_external_geometry_session",
                    "no_sketch_executor_split_ownership_guess",
                    "no_wire_joiner_fallback_candidate_fields"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
         }},
        {"part_workbench",
         {
             {"offset",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp
                  // ::Offset::execute(), reads "Source", "Value", "Mode", "Join",
                  // "Intersection", "SelfIntersection" and "Fill" before calling
                  // TopoShape::makeElementOffset(); ::Offset2D::execute() calls
                  // "makeElementOffset2D(shape, offset, join, fill, openresult, inter)";
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
                  // ::Thickness::execute() calls "makeElementThickSolid(base, shapes, ...)".
                  {"status", "done_second_slice"},
                  {"type_ids", {"Part::Compound", "Part::Offset", "Part::Offset2D", "Part::Thickness"}},
                  {"properties",
                   {"Source", "Faces", "Value", "Mode", "Join", "Intersection", "SelfIntersection", "Fill"}},
                  {"covered",
                   {"face_source_offset",
                    "maker_history_generated_modified",
                    "fill_offset",
                    "solid_source_make_element_solid",
                    "offset2d_face_no_fill",
                    "offset2d_face_fill_closed",
                    "offset2d_open_wire_no_fill",
                    "offset2d_open_wire_fill",
                    "offset2d_compound_child_recursive",
                    "offset2d_compound_collective",
                    "thickness_single_solid_face",
                    "thickness_mode_join_oracle"}},
                  {"request_local_boundaries", {"source_shape_recomputed_from_document_graph"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"conic_curves",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
                  // ::GeomHyperbola::Save/Restore() persists "MajorRadius", "MinorRadius"
                  // and "AngleXU"; ::GeomParabola::Save/Restore() persists "Focal" and
                  // "AngleXU"; ::GeomArcOf*::Save/Restore() adds "StartAngle"/"EndAngle".
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp
                  // has no Part::Hyperbola or Part::Parabola DocumentObject source.
                  {"status", "done_part_geometry_curve_edge_consumer"},
                  {"dto", "PartConicCurveDTO"},
                  {"payload_keys", {"partGeometryCurve", "partGeometryCurveConsumers"}},
                  {"part_geometry_types", {"Part.Hyperbola", "Part.Parabola"}},
                  {"curve_types", {"GeomAbs_Hyperbola", "GeomAbs_Parabola"}},
                  {"covered",
                   {"hyperbola_finite_edge",
                    "parabola_finite_edge",
                    "typed_conic_curve_metadata",
                    "invalid_param_diagnostics",
                    "part_extrusion_edge_to_face_consumer",
                    "part_ruled_surface_edge_consumer"}},
                  {"fixtures",
                   {"p8/part-hyperbola-edge",
                    "p8/part-parabola-edge",
                    "p8/part-conic-edge-invalid-params",
                    "p8/part-conic-edge-extrusion",
                    "p8/part-ruled-surface-conic-line"}},
                  {"diagnostics",
                   {"invalid_part_conic_axis",
                    "invalid_part_conic_curve_kind",
                    "invalid_part_conic_focal",
                    "invalid_part_conic_number",
                    "invalid_part_conic_radius",
                    "invalid_part_conic_trim"}},
                  {"consumer_type_ids", {"Part::Extrusion", "Part::RuledSurface"}},
                  {"request_local_boundaries",
                   {"no_part_hyperbola_document_object_executor",
                    "no_part_parabola_document_object_executor",
                    "conic_edge_is_request_local_producer_not_document_object",
                    "source_shape_recomputed_from_document_graph"}},
                  {"remaining_gaps",
                   {"gui_conic_edit",
                    "full_sketcher_solver_conic_constraints",
                    "distance_type_default_todo"}},
              }},
             {"project_on_surface",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h
                  // declares Mode/Height/Offset/Direction/SupportFace/Projection; the matching
                  // FeatureProjectOnSurface.cpp::tryExecute() calls getSupportFace(),
                  // getProjectionShapes(), createProjectedWire(), filterShapes() and
                  // createCompound(). The published C4-M1 ProjectOnSurface slice covers Edges,
                  // Faces and All for ordered Projection LinkSubList items, including
                  // createSolidIfHeight() for Mode=All plus Height and getOffsetPlacement()
                  // child movement.
                  {"status", "supported_expected_backed_published_slice"},
                  {"type_ids", {"Part::ProjectOnSurface"}},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.Mode.value",
                    "Objects[].Properties.Height.value",
                    "Objects[].Properties.Offset.value",
                    "Objects[].Properties.Direction.value",
                    "Objects[].Properties.SupportFace.value",
                    "Objects[].Properties.SupportFace.SubList",
                    "Objects[].Properties.Projection.SubSet",
                    "recompute.objs"}},
                  {"properties", {"Mode", "Height", "Offset", "Direction", "SupportFace", "Projection"}},
                  {"property_types",
                   {"App::PropertyEnumeration",
                    "App::PropertyLength",
                    "App::PropertyDistance",
                    "App::PropertyDirection",
                    "App::PropertyLinkSub",
                    "App::PropertyLinkSubList"}},
                  {"mode_values", {"All", "Faces", "Edges"}},
                  {"covered",
                   {"source_backed_document_object_executor",
                    "support_face_property_link_sub",
                    "mode_edges_faces_all_values",
                    "projection_property_link_sub_list_ordered_edge_wire_or_face_items",
                    "multiple_projection_ordered_link_sub_list",
                    "multi_projection_result_append_order",
                    "multi_projection_metadata_order",
                    "mode_edges_project_wire",
                    "mode_faces_project_face_rebuild",
                    "face_rebuild_hole_wires",
                    "mode_all_project_face_rebuild",
                    "mode_all_height_prism",
                    "mode_faces_height_keeps_face",
                    "height_below_precision_keeps_face",
                    "offset_zero",
                    "offset_after_filter_compound_child_move",
                    "offset_direction_normalized",
                    "brepproj_projection_nearest_wire",
                    "projected_face_parametric_wire_rebuild",
                    "ordinary_indexed_named_shape",
                    "c5m9_projected_edge_wire_projection_item_ledger",
                    "c5m9_projected_edge_wire_mapper_history_source_backed",
                    "c5m9_projected_wire_fragment_ownership",
                    "c5m9_invalid_provenance_diagnostics",
                    "c5m9_projected_face_wire_source_evidence",
                    "c5m9_projected_face_rebuild_mapper_history_source_backed",
                    "c5m9_projected_all_compound_child_provenance",
                    "c5m9_projected_height_solid_provenance",
                    "c5m9_projected_reference_recovery_hook",
                    "expected_backed_fixture",
                    "deferred_branch_diagnostics"}},
                  {"fixtures",
                   {"c4m1/part-project-on-surface-edge-plane",
                    "c4m1/part-project-on-surface-face-plane",
                    "c4m1/part-project-on-surface-face-hole-plane",
                    "c4m1/part-project-on-surface-face-edges-mode",
                    "c4m1/part-project-on-surface-face-all-plane",
                    "c4m1/part-project-on-surface-height-boundaries",
                    "c4m1/part-project-on-surface-edge-offset",
                    "c4m1/part-project-on-surface-face-offset",
                    "c4m1/part-project-on-surface-height-offset-boundary",
                    "c4m1/part-project-on-surface-multi-edge-order",
                    "c4m1/part-project-on-surface-mixed-face-edge-order",
                    "c4m1/part-project-on-surface-deferred-boundaries",
                    "c5m9/part-project-on-surface-edge-provenance",
                    "c5m9/part-project-on-surface-wire-split-provenance",
                    "c5m9/part-project-on-surface-invalid-provenance-diagnostics",
                    "c5m9/part-project-on-surface-face-rebuild-provenance",
                    "c5m9/part-project-on-surface-all-compound-provenance"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "unsupported_subshape_kind",
                    "unsupported_property",
                    "invalid_direction",
                    "execution_failed",
                    "locatable_projection_target_subname"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "single_support_face",
                    "ordinary_link_sub_list_projection_order",
                    "ordinary_indexed_named_shape_without_freecad_mapper_history",
                    "projected_edge_wire_mapper_history_from_projection_item_ledger",
                    "projected_face_all_mapper_history_from_projection_item_ledger",
                    "native_project_on_surface_mapper_history_hidden_until_probe"}},
                  {"remaining_gaps",
                   {"gui_projection_task_panel",
                    "unverified_advanced_branches"}},
                  {"non_goals",
                   {"gui_projection_task_panel",
                    "unverified_advanced_branches"}},
              }},
             {"ruled_surface",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
                  // ::RuledSurface::execute(), reads "Curve1" / "Curve2" and calls
                  // "res.makeElementRuledSurface(shapes, Orientation.getValue())".
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
                  // ::TopoShape::makeElementRuledSurface(), edge inputs call
                  // "BRepFill::Face" and wire inputs call "BRepFill::Shell".
                  {"status", "supported_wire_wire_expected_backed"},
                  {"type_ids", {"Part::RuledSurface"}},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.Curve1.value",
                    "Objects[].Properties.Curve1.SubList",
                    "Objects[].Properties.Curve2.value",
                    "Objects[].Properties.Curve2.SubList",
                    "Objects[].Properties.Orientation.value",
                    "recompute.objs"}},
                  {"properties", {"Curve1", "Curve2", "Orientation"}},
                  {"property_types", {"App::PropertyLinkSub", "App::PropertyEnumeration"}},
                  {"orientation_values", {"Automatic", "Forward", "Reversed"}},
                  {"covered",
                   {"source_backed_document_object_executor",
                    "curve1_curve2_property_link_sub",
                    "orientation_automatic_forward_reversed",
                    "edge_edge_brepfill_face",
                    "wire_wire_brepfill_shell",
                    "source_edge_provenance",
                    "wire_edge_provenance",
                    "conic_edge_consumer",
                    "expected_backed_fixtures"}},
                  {"fixtures",
                   {"p8/part-ruled-surface-line-line",
                    "p8/part-ruled-surface-conic-line",
                    "p8/part-ruled-surface-orientation-reversed",
                    "p8/part-ruled-surface-invalid-input",
                    "c4m1/part-ruled-surface-wire-wire"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "unsupported_subshape_kind",
                    "no_edge"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "edge_edge_brepfill_face",
                    "wire_wire_brepfill_shell",
                    "conic_line_expected_uses_make_ruled_surface_after_link_resolve"}},
                  {"remaining_gaps", nlohmann::json::array()},
                  {"non_goals", nlohmann::json::array()},
              }},
             {"loft",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
                  // ::Loft::execute(), reads "Sections", "Solid", "Ruled", "Closed",
                  // "Linearize" and "MaxDegree" before calling
                  // "result.makeElementLoft(shapes, isSolid, isRuled, isClosed, degMax)".
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
                  // ::TopoShape::makeElementLoft(), uses BRepOffsetAPI_ThruSections,
                  // "SetMaxDegree()", "CheckCompatibility(Standard_True)" and
                  // MapperThruSections history.
                  {"status", "supported_profile_linearize_expected_backed"},
                  {"type_ids", {"Part::Loft"}},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.Sections.values",
                    "Objects[].Properties.Solid",
                    "Objects[].Properties.Ruled",
                    "Objects[].Properties.Closed",
                    "Objects[].Properties.Linearize",
                    "Objects[].Properties.MaxDegree",
                    "recompute.objs"}},
                  {"properties", {"Sections", "Solid", "Ruled", "Closed", "Linearize", "MaxDegree"}},
                  {"property_types",
                   {"App::PropertyLinkList", "App::PropertyBool", "App::PropertyInteger"}},
                  {"covered",
                   {"source_backed_document_object_executor",
                    "sections_property_link_list",
                    "solid_ruled_closed_max_degree",
                    "prepare_profiles_edge_wire_expected_batch",
                    "prepare_profiles_face_vertex_expected_batch",
                    "thru_sections_brepoffsetapi",
                    "linearize_planar_faces_post_processing",
                    "loft_thru_sections_maker_history",
                    "expected_backed_fixtures",
                    "invalid_sections_diagnostics"}},
                  {"fixtures",
                   {"c3m4/part-loft-two-section-surface",
                    "c3m4/part-loft-solid",
                    "c3m4/part-loft-ruled",
                    "c3m4/part-loft-closed",
                    "c3m4/part-loft-invalid-sections",
                    "c4m1/part-loft-linearize-profile-face",
                    "c4m1/part-loft-linearize-profile-vertex"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_property",
                    "unsupported_property",
                    "execution_failed"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "source_backed_part_loft_document_object_only",
                    "linearize_faces_no_edges_post_processing",
                    "face_vertex_profile_expected_backed",
                    "complex_profile_family_deferred"}},
                  {"remaining_gaps", {"complex_profile_family"}},
                  {"non_goals", {"complex_profile_family"}},
              }},
             {"sweep",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
                  // ::Sweep::execute(), reads "Sections", "Spine", "Solid", "Frenet",
                  // "Linearize" and "Transition" before calling makeElementPipeShell().
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
                  // ::TopoShape::makeElementPipeShell(), returns
                  // "makeElementShape(mkPipeShell, shapes, op)" so BRepOffsetAPI_MakePipeShell
                  // Modified/Generated history is part of the published boundary.
                  {"status",
                   "supported_multi_profile_linearize_c5m12_wrapper_expected_backed_with_narrowed_blockers"},
                  {"type_ids", {"Part::Sweep"}},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.Sections.values",
                    "Objects[].Properties.Spine.value",
                    "Objects[].Properties.Spine.SubList",
                    "Objects[].Properties.Solid",
                    "Objects[].Properties.Frenet",
                    "Objects[].Properties.Transition",
                    "Objects[].Properties.Linearize",
                    "Objects[].Properties.AuxiliarySpine.value",
                    "Objects[].Properties.AuxiliarySpine.SubList",
                    "Objects[].Properties.AuxiliaryCurvilinear",
                    "Objects[].Properties.SpineSupport.value",
                    "Objects[].Properties.SpineSupport.SubList",
                    "Objects[].Properties.SupportMode",
                    "Objects[].Properties.Binormal",
                    "Objects[].Properties.BiNormal",
                    "Objects[].Properties.SectionOptions[].Location.value",
                    "Objects[].Properties.SectionOptions[].Location.SubList",
                    "Objects[].Properties.SectionOptions[].WithContact",
                    "Objects[].Properties.SectionOptions[].WithCorrection",
                    "Objects[].Properties.Tolerance.tol3d",
                    "Objects[].Properties.Tolerance.boundTol",
                    "Objects[].Properties.Tolerance.tolAngular",
                    "recompute.objs"}},
                  {"properties",
                   {"Sections",
                    "Spine",
                    "Solid",
                    "Frenet",
                    "Transition",
                    "Linearize",
                    "AuxiliarySpine",
                    "AuxiliaryCurvilinear",
                    "SpineSupport",
                    "SupportMode",
                    "Binormal",
                    "BiNormal",
                    "SectionOptions",
                    "LocationMode",
                    "Tolerance"}},
                  {"property_types",
                   {"App::PropertyLinkList",
                    "App::PropertyLinkSub",
                    "App::PropertyBool",
                    "App::PropertyEnumeration",
                    "App::PropertyFloat",
                    "App::PropertyVector"}},
                  {"covered",
                   {"source_backed_document_object_executor",
                    "spine_property_link_sub_sublist_compound",
                    "sections_property_link_list",
                    "multi_profile_sections_expected_backed",
                    "solid_frenet_transition_modes",
                    "linearize_faces_no_edges_post_processing",
                    "pipeshell_maker_history",
                    "expected_backed_fixtures",
                    "invalid_input_diagnostics",
                    "auxiliary_spine_wrapper_expected_backed",
                    "auxiliary_curvilinear_wrapper_expected_backed",
                    "support_mode_wrapper_expected_backed",
                    "binormal_wrapper_expected_backed",
                    "advanced_mode_locatable_diagnostics",
                    "section_location_contact_correction_freecadcmd_blocker",
                    "tolerance_triple_wrapper_expected_backed",
                    "advanced_combined_freecadcmd_blocker",
                    "s3_location_tolerance_locatable_diagnostics",
                    "c5m11_wrapper_expected_capability_promotion",
                    "c5m12_support_wrapper_expected_recovery",
                    "c5m10_capability_docs_closeout"}},
                  {"fixtures",
                   {"c3m4/part-sweep-right-corner-surface",
                    "c3m4/part-sweep-solid",
                    "c3m4/part-sweep-frenet-off",
                    "c3m4/part-sweep-transition-transformed",
                    "c3m4/part-sweep-transition-round-corner",
                    "c3m4/part-sweep-spine-subedges",
                    "c3m4/part-sweep-open-profile-surface",
                    "c3m4/part-sweep-invalid-inputs",
                    "c4m1/part-sweep-multi-profile-linearize",
                    "c4m1/part-sweep-advanced-deferred",
                    "c5m10/part-sweep-auxiliary-spine-contract",
                    "c5m10/part-sweep-binormal-contract",
                    "c5m10/part-sweep-support-mode-diagnostics",
                    "c5m10/part-sweep-located-profile-contract",
                    "c5m10/part-sweep-tolerance-contract",
                    "c5m10/part-sweep-advanced-combined-contract",
                    "c5m12/part-sweep-spine-support-surface-normal"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "invalid_parameter",
                    "unsupported_property",
                    "execution_failed"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "source_backed_part_sweep_document_object_only",
                    "multi_profile_sections_expected_backed",
                    "linearize_faces_no_edges_post_processing",
                    "c5m11_auxiliary_binormal_tolerance_wrapper_expected_backed",
                    "c5m12_support_mode_wrapper_expected_backed",
                    "section_options_location_contact_correction_freecadcmd_blocker",
                    "advanced_combination_freecadcmd_blocker",
                    "hole_model_thread_internal_pipeshell_not_part_sweep"}},
                  {"field_boundaries",
                   {{"expected_backed",
                     {"Sections",
                      "Spine",
                      "Solid",
                      "Frenet",
                      "Transition",
                      "Linearize",
                      "AuxiliarySpine",
                      "AuxiliaryCurvilinear",
                      "SpineSupport",
                      "SupportMode",
                      "Binormal",
                      "BiNormal",
                      "Tolerance.tol3d",
                      "Tolerance.boundTol",
                      "Tolerance.tolAngular"}},
                    {"narrowed_gap",
                     {"SectionOptions[].Location",
                      "SectionOptions[].WithContact",
                      "SectionOptions[].WithCorrection",
                      "advanced_combination"}},
                    {"source_backed_known_gap", nlohmann::json::array()},
                    {"diagnostic_backed",
                     {"missing_link_target",
                      "invalid_subshape",
                      "SpineSupport",
                      "SupportMode",
                      "invalid SupportMode",
                      "invalid Binormal/BiNormal vector",
                      "invalid SectionOptions payload",
                      "invalid Tolerance object",
                      "legacy scalar Tolerance compatibility diagnostic"}},
                    {"non_goal",
                     {"upstream native Part::Sweep advanced direct properties",
                      "GUI Sweep/Pipe task panels",
                      "PartDesign Pipe/Hole product support",
                      "persistent Python BRepOffsetAPI_MakePipeShell lifecycle",
                      "output-order/bbox/fixture-name PipeShell fixups"}}}},
                  {"source_backed_known_gaps", nlohmann::json::object()},
                  {"narrowed_gaps",
                   {{"part_sweep_located_profile_freecadcmd_wrapper_build_blocker",
                     {{"status", "narrowed_freecadcmd_wrapper_build_blocker"},
                      {"fields",
                       {"SectionOptions[].Location",
                        "SectionOptions[].WithContact",
                        "SectionOptions[].WithCorrection"}},
                      {"fixtures", {"c5m10/part-sweep-located-profile-contract"}},
                      {"freecadcmd_evidence",
                       {{"helper", "Part.BRepOffsetAPI_MakePipeShell"},
                        {"runtime_helper", "Part.BRepOffsetAPI.MakePipeShell"},
                        {"error", "OCCError: NCollection_Array1::Value"}}},
                      {"delete_condition",
                       "Replace only after FreeCADCmd returns stable shape_summary for add(Profile, Location, WithContact, WithCorrection)."}}},
                    {"part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker",
                     {{"status", "narrowed_freecadcmd_wrapper_build_blocker"},
                      {"fields", {"advanced_combination"}},
                      {"fixtures", {"c5m10/part-sweep-advanced-combined-contract"}},
                      {"freecadcmd_evidence",
                       {{"helper", "Part.BRepOffsetAPI_MakePipeShell"},
                        {"runtime_helper", "Part.BRepOffsetAPI.MakePipeShell"},
                        {"error", "OCCError: NCollection_Array1::Value"}}},
                      {"delete_condition",
                       "Replace after the located-profile wrapper build blocker is isolated and combined auxiliary/section/tolerance metadata is collectable."}}}}},
                  {"non_goals",
                   {"upstream_native_part_sweep_advanced_direct_properties",
                    "gui_sweep_pipe_task_panels",
                    "part_design_pipe_hole_product_support",
                    "persistent_python_pipeshell_wrapper_lifecycle",
                    "output_order_bbox_fixture_name_pipeshell_fixups",
                    "hole_model_thread_internal_pipeshell"}},
                  {"remaining_gaps",
                   {"part_sweep_located_profile_freecadcmd_wrapper_build_blocker",
                    "part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker"}},
              }},
             {"filling",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
                  // ::makeFilledFace(), exposes the Python helper "Part.makeFilledFace";
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
                  // ::TopoShape::makeElementFilledFace(), creates "BRepOffsetAPI_MakeFilling",
                  // finds a boundary wire or builds one from connected edges, fixes the wire,
                  // adds remaining wire/edge/face/vertex constraints, and returns
                  // makeElementShape(maker, _shapes, FilledFace). cad-core
                  // Part::FilledFace is a source-backed helper request type, not a native
                  // FreeCAD DocumentObject TypeId.
                  {"status", "supported_expected_backed_with_c5m8_s5_capability_closeout"},
                  {"type_ids", {"Part::FilledFace"}},
                  {"helper", "Part.makeFilledFace"},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.Boundary.SubSet",
                    "Objects[].Properties.Surface.SubList",
                    "Objects[].Properties.Supports.SubSet",
                    "Objects[].Properties.Supports.SubSet[].Support",
                    "Objects[].Properties.Orders.SubSet",
                    "Objects[].Properties.Orders.SubSet[].Order",
                    "Objects[].Properties.BRepOffsetAPIMakeFillingWrapper",
                    "Objects[].Properties.BRepOffsetAPIMakeFillingUvPointOnSupport",
                    "Objects[].Properties.Degree",
                    "Objects[].Properties.PtsOnCurve",
                    "Objects[].Properties.NumIter",
                    "Objects[].Properties.Anisotropy",
                    "Objects[].Properties.Tol2d",
                    "Objects[].Properties.Tol3d",
                    "Objects[].Properties.TolG1",
                    "Objects[].Properties.TolG2",
                    "Objects[].Properties.MaxDegree",
                    "Objects[].Properties.MaxSegments",
                    "recompute.objs"}},
                  {"freecad_native_document_object", false},
                  {"operation_model", "source_backed_helper"},
                  {"properties",
                   {"Boundary",
                    "Degree",
                    "PtsOnCurve",
                    "NumIter",
                    "Anisotropy",
                    "Tol2d",
                    "Tol3d",
                    "TolG1",
                    "TolG2",
                    "MaxDegree",
                    "MaxSegments",
                    "Surface",
                    "Supports",
                    "Orders"}},
                  {"property_types", {"App::PropertyLinkSubList"}},
                  {"covered",
                   {"part_filled_face_source_backed_helper",
                    "boundary_property_link_sub_list",
                    "closed_wire_default",
                    "connected_boundary_edges_default",
                    "brepoffsetapi_makefilling",
                    "maker_history:filling",
                    "default_params_metadata",
                    "boundary_source_evidence",
                    "initial_surface_load_init_surface",
                    "support_face_source_map",
                    "order_source_map",
                    "support_order_source_evidence",
                    "source_backed_surface_support_order_known_gap",
                    "constructor_params_metadata",
                    "non_default_params_source_backed_known_gap",
                    "non_boundary_edge_isbound_false",
                    "non_boundary_wire_constraints",
                    "non_boundary_face_constraint",
                    "non_boundary_vertex_point_constraint",
                    "non_boundary_constraint_source_evidence",
                    "compound_source_expansion",
                    "compound_optional_expected_backed",
                    "direct_makefilling_wrapper_lifecycle_diagnostic",
                    "wrapper_uv_point_on_support_diagnostic",
                    "expected_backed_fixtures",
                    "invalid_diagnostics"}},
                  {"fixtures",
                   {"c3m4/part-filling-closed-wire-default",
                    "c3m4/part-filling-boundary-edges-default",
                    "c3m4/part-filling-invalid-inputs",
                    "c4m1/part-filling-advanced-deferred",
                    "c5m8/part-filling-initial-surface-boundary",
                    "c5m8/part-filling-support-order-edge-face",
                    "c5m8/part-filling-invalid-support-order",
                    "c5m8/part-filling-non-default-params",
                    "c5m8/part-filling-param-diagnostics",
                    "c5m8/part-filling-non-boundary-edge-support",
                    "c5m8/part-filling-non-boundary-face-point",
                    "c5m8/part-filling-non-boundary-wire",
                    "c5m8/part-filling-non-boundary-diagnostics",
                    "c5m8/part-filling-compound-optional-boundary",
                    "c5m8/part-filling-wrapper-boundary",
                    "c5m8/part-filling-wrapper-uv-point-boundary"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "invalid_surface_source",
                    "invalid_support_target",
                    "invalid_support_source",
                    "invalid_order_target",
                    "invalid_order_source",
                    "invalid_parameter",
                    "invalid_non_boundary_source",
                    "unsupported_wrapper_lifecycle",
                    "unsupported_property",
                    "execution_failed"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "source_backed_helper_not_freecad_document_object",
                    "boundary_property_link_sub_list",
                    "default_params_baseline",
                    "constructor_params_source_backed",
                    "initial_surface_face_source_backed",
                    "support_face_map_source_backed",
                    "order_map_source_backed_c0_g1_g2_parser",
                    "surface_support_order_native_helper_oracle_known_gap",
                    "support_order_g1_fixture_source_backed",
                    "non_default_params_native_helper_oracle_known_gap",
                    "non_boundary_edge_wire_face_vertex_source_backed",
                    "non_boundary_edge_support_order_native_helper_oracle_known_gap",
                    "compound_boundary_optional_expected_backed",
                    "no_cross_request_mutable_makefilling_wrapper_state",
                    "direct_makefilling_wrapper_diagnostic",
                    "wrapper_uv_point_on_support_not_helper_dto",
                    "native_surface_workbench_filling_feature_not_claimed",
                    "full_part_surface_family_not_claimed"}},
                  {"remaining_gaps",
                   {"surface_support_order_native_helper_expected",
                    "filling_support_order_g2_expected",
                    "non_default_params_native_helper_expected",
                    "non_boundary_edge_support_native_helper_expected"}},
                  {"non_goals",
                   {"native_freecad_part_filledface_document_object",
                    "surface_workbench_filling_feature",
                    "surface_workbench_gui_taskpanel_viewprovider",
                    "cross_request_mutable_brepoffsetapi_makefilling_wrapper",
                    "full_part_surface_family"}},
              }},
             {"geomplate",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp
                  // ::makeSurface(), builds a transient "GeomPlate_BuildPlateSurface" from
                  // curve and point constraints; /Users/li/Chili3DProject/FreeCAD/src/Mod/
                  // Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp exposes
                  // "Part.GeomPlate.BuildPlateSurface"; Geometry.cpp wraps the result as
                  // "GeomPlateSurface". cad-core Part::GeomPlateSurface is a source-backed
                  // geometry helper request type, not a GUI feature or native FreeCAD
                  // DocumentObject TypeId.
                  {"status",
                   "supported_expected_backed_point_criteria_with_curve_wrapper_diagnostics"},
                  {"type_ids", {"Part::GeomPlateSurface"}},
                  {"helper", "Part.GeomPlate.BuildPlateSurface"},
                  {"dto", "PartGeomPlateSurfaceDTO"},
                  {"payload_keys",
                   {"Objects[].TypeId",
                    "Objects[].Properties.CurveConstraints.SubSet",
                    "Objects[].Properties.CurveConstraints.SubSet[].Surface",
                    "Objects[].Properties.CurveConstraints.SubSet[].G0Criterion",
                    "Objects[].Properties.CurveConstraints.SubSet[].G1Criterion",
                    "Objects[].Properties.CurveConstraints.SubSet[].G2Criterion",
                    "Objects[].Properties.PointConstraints",
                    "Objects[].Properties.PointConstraints[].G0Criterion",
                    "Objects[].Properties.PointConstraints[].G1Criterion",
                    "Objects[].Properties.PointConstraints[].G2Criterion",
                    "Objects[].Properties.InitialSurface",
                    "Objects[].Properties.Surface",
                    "Objects[].Properties.Curve2dOnSurface",
                    "Objects[].Properties.Curve2dOnSurface[].Boundary",
                    "Objects[].Properties.Curve2dOnSurface[].Surface",
                    "Objects[].Properties.Curve2dOnSurface[].Curve2d",
                    "Objects[].Properties.ProjectedCurve2d",
                    "Objects[].Properties.ProjectedCurve2d[].Boundary",
                    "Objects[].Properties.ProjectedCurve2d[].Surface",
                    "Objects[].Properties.ProjectedCurve2d[].Curve2d",
                    "Objects[].Properties.ProjectedCurve2d[].TolU",
                    "Objects[].Properties.ProjectedCurve2d[].TolV",
                    "Objects[].Properties.Point2dOnSurface",
                    "Objects[].Properties.Point2dOnSurface[].Point",
                    "Objects[].Properties.Point2dOnSurface[].Point2d",
                    "Objects[].Properties.Point2dOnSurface[].Surface",
                    "Objects[].Properties.Degree",
                    "Objects[].Properties.NbPtsOnCur",
                    "Objects[].Properties.NbIter",
                    "Objects[].Properties.Tol2d",
                    "Objects[].Properties.Tol3d",
                    "Objects[].Properties.ApproxTol3d",
                    "Objects[].Properties.ApproxMaxSegments",
                    "Objects[].Properties.ApproxMaxDegree",
                    "Objects[].Properties.ApproxContinuity",
                    "recompute.objs"}},
                  {"freecad_native_document_object", false},
                  {"operation_model", "source_backed_geometry_helper"},
                  {"properties",
                   {"CurveConstraints",
                    "PointConstraints",
                    "Degree",
                    "NbPtsOnCur",
                    "NbIter",
                    "Tol2d",
                    "Tol3d",
                    "TolAng",
                    "TolCurv",
                    "Anisotropy",
                    "ApproxTol3d",
                    "ApproxMaxSegments",
                    "ApproxMaxDegree",
                    "ApproxMaxDistance",
                    "ApproxCritOrder",
                    "ApproxContinuity",
                    "ApproxEnlargeCoeff",
                    "InitialSurface",
                    "Surface",
                    "Curve2dOnSurface",
                    "ProjectedCurve2d",
                    "Point2dOnSurface",
                    "PlateSurfaceCurves"}},
                  {"property_types",
                   {"App::PropertyLinkSubList",
                    "App::PropertyLinkSub",
                    "JSON::PointConstraintList",
                    "JSON::Curve2dConstraintList",
                    "JSON::Point2dConstraintList",
                    "JSON::PointCriteria",
                    "JSON::NumericParams"}},
                  {"covered",
                   {"part_geomplate_surface_source_backed_helper",
                    "buildplate_surface_helper",
                    "default_3d_curve_point_expected_backed",
                    "source_backed_3d_curve_g0_constraints",
                    "point_3d_constraints",
                    "initial_surface_reference_expected_backed",
                    "g1_curve_on_surface_source_backed",
                    "curve2d_on_surface_expected_backed",
                    "point2d_on_surface_expected_backed",
                    "mixed_g0_2d_surface_constraints_expected_backed",
                    "point_constraint_criteria_expected_backed",
                    "projected_curve2d_source_backed",
                    "projected_curve2d_native_oracle_blocker",
                    "default_build_params_metadata",
                    "approximation_metadata",
                    "explicit_approximation_params_expected_backed",
                    "advanced_approximation_params_expected_backed",
                    "geomplate_makeapprox_face",
                    "source_evidence",
                    "expected_backed_fixtures",
                    "invalid_diagnostics",
                    "g1_curve_on_surface_native_oracle_blocker"}},
                  {"fixtures",
                   {"c3m4/part-geomplate-curve-point-default",
                    "c3m4/part-geomplate-invalid-inputs",
                    "c4m1/part-geomplate-advanced-constraints",
                    "c4m1/part-geomplate-advanced-deferred",
                    "c5m7/part-geomplate-initial-surface-g0",
                    "c5m7/part-geomplate-g1-curve-on-surface",
                    "c5m7/part-geomplate-curve2d-on-surface",
                    "c5m7/part-geomplate-projected-curve2d",
                    "c5m7/part-geomplate-point2d-on-surface",
                    "c5m7/part-geomplate-mixed-surface-constraints",
                    "c5m7/part-geomplate-point-custom-criteria",
                    "c5m7/part-geomplate-curve-criteria-diagnostic",
                    "c5m7/part-geomplate-wrapper-boundary"}},
                  {"diagnostics",
                   {"missing_constraints",
                    "missing_curve_source",
                    "missing_surface_source",
                    "invalid_curve_source",
                    "invalid_curve2d_source",
                    "invalid_surface_source",
                    "invalid_point_constraint",
                    "invalid_point2d_source",
                    "invalid_parameter",
                    "perform_failed",
                    "surface_not_done",
                    "approximation_failed",
                    "unsupported_property",
                    "unsupported_curve_criteria",
                    "unsupported_wrapper_lifecycle"}},
                  {"request_local_boundaries",
                   {"source_shape_recomputed_from_document_graph",
                    "source_backed_helper_not_freecad_document_object",
                    "request_local_geomplate_surface_not_persistent_geometry",
                    "curve_constraints_are_3d_edge_sources",
                    "point_constraints_are_3d_vectors",
                    "g0_curve_constraints_first_batch",
                    "initial_surface_load_init_surface",
                    "g1_curve_on_surface_source_backed_not_native_expected_backed",
                    "curve2d_on_surface_source_backed_expected_backed",
                    "point2d_on_surface_source_backed_expected_backed",
                    "projected_curve2d_source_backed_not_native_expected_backed",
                    "point_constraint_criteria_expected_backed",
                    "curve_constraint_criteria_setters_not_implemented_in_freecad",
                    "platesurface_curves_requires_wrapper_lifecycle",
                    "default_and_explicit_build_params",
                    "explicit_approximation_params",
                    "advanced_approximation_params_expected_backed",
                    "advanced_approximation_params_are_not_full_advanced_support",
                    "2d_constraints_require_explicit_boundary_surface_and_uv_payload",
                    "g1_native_oracle_blocked_by_python_wrapper",
                    "projected_curve2d_native_oracle_blocked_by_python_wrapper",
                    "filling_capability_not_expanded"}},
                  {"remaining_gaps",
                   {"g1_curve_on_surface_native_oracle",
                    "projected_2d_curve_native_oracle",
                    "curve_constraint_criteria_setters_not_implemented",
                    "platesurface_curves_wrapper_lifecycle"}},
                  {"non_goals",
                   {"gui_geomplate_feature",
                    "native_freecad_part_geomplate_document_object",
                    "fake_part_geomplate_document_object",
                    "fake_persistent_platesurface_object",
                    "filling_brepoffsetapi_makefilling_extension"}},
              }},
         }},
        {"part_design",
         {
             {"pad_pocket",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/
                  // FeaturePad.cpp::Pad::execute() and FeaturePocket.cpp::Pocket::execute()
                  // delegate UpToFace / UpToShape to FeatureExtrude.cpp, whose
                  // getUpToShapeFromLinkSubList path collects selected faces before prism-until.
                  {"up_to_shape_multi_face",
                   {
                       {"status", "supported"},
                       {"objects", {"part_design.pad", "part_design.pocket"}},
                       {"fixtures",
                        {"p3a/pocket-up-to-shape-multi-face", "p3a/pad-up-to-shape-multi-face"}},
                       {"failure_fixtures",
                        {{"offset", "p3a/pocket-up-to-shape-multiple-faces-offset"},
                         {"invalid_subshape", "p3a/pocket-up-to-shape-edge-subshape"}}},
                       {"diagnostics",
                        {"unsupported_property",
                         "unsupported_subshape_kind",
                         "invalid_subshape",
                         "missing_link_target"}},
                   }},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"body_chain",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
                  // ::Body::onChanged(),
                  // "createObject(\"PartDesign::FeatureBase\",\"BaseFeature\")" and
                  // "bf->BaseFeature.setValue(BaseFeature.getValue())"; ::Body::setBaseProperty()
                  // rewrites the next solid feature's "BaseFeature"; ::Body::removeObject()
                  // "Adjust Tip feature if it is pointing to the deleted object".
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/OriginGroupExtension.cpp
                  // ::OriginGroupExtension::Origin is a "LinkScope::Child" property whose
                  // Origin placement is owned by the Body's group placement; ::relinkToOrigin()
                  // replaces external origin datum links with "getOrigin()->getDatumElement(Role)".
                  {"document_object_updates",
                   {"body_basefeature_featurebase_create",
                    "body_basefeature_group_sync",
                    "body_feature_basefeature_sync",
                    "body_tip_deleted_feature_reroute",
                    "body_feature_basefeature_delete_reroute",
                    "body_origin_datum_relink"}},
                  {"writeback_properties",
                   {"Body.Group", "Body.Tip", "Body.Origin", "FeatureBase.BaseFeature", "Feature.BaseFeature"}},
                  {"origin_lifecycle",
                   {"explicit_body_origin_link",
                    "origin_global_placement_from_body",
                    "origin_feature_role_relink"}},
                  {"addsub_replay",
                   {"group_order_replay", "additive_fuse", "subtractive_cut", "stop_at_tip"}},
                  {"request_local_boundaries",
                   {"body_basefeature_writeback_keeps_request_graph_immutable",
                    "body_delete_reroute_uses_request_local_stale_tip_evidence",
                    "body_origin_parent_placement_without_backend_session"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"revolution_groove",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp
                  // ::Revolution::execute() calls "executeRevolved(Part::RevolMode::FuseWithBase)";
                  // FeatureGroove.cpp::Groove::execute() calls
                  // "executeRevolved(Part::RevolMode::CutFromBase)"; FeatureRevolved.cpp
                  // ::generateRevolution() uses "BRepPrimAPI_MakeRevol" for Type=Angle,
                  // "TwoAngles" and "ThroughAll", while UpTo* branches call
                  // TopoShape::makeElementRevolution() / BRepFeat_MakeRevol.
                  {"status", "supported_c51s1_advanced_with_exact_groove_upto_blocker"},
                  {"type_ids", {"PartDesign::Revolution", "PartDesign::Groove"}},
                  {"supported",
                   {"Type=Angle",
                    "Type=TwoAngles",
                    "Revolution Type=UpToFirst",
                    "Revolution Type=UpToLast",
                    "Revolution Type=UpToFace",
                    "Groove Type=ThroughAll",
                    "Profile.SubList=InternalFaceN",
                    "Sketch H_Axis/V_Axis ReferenceAxis",
                    "Sketch AxisN ReferenceAxis",
                    "PartDesign::Line ReferenceAxis",
                    "App::Line ReferenceAxis",
                    "linear_edge ReferenceAxis",
                    "FuseOrder=FeatureFirst",
                    "Angle degrees",
                    "Angle2 degrees",
                    "Reversed",
                    "Midplane",
                    "BRepFeat_MakeRevol UpTo body replacement",
                    "Body additive fuse replay",
                    "Body subtractive cut replay",
                    "Body additive FeatureFirst fuse replay",
                    "maker_history:revolve"}},
                  {"fixtures",
                   {"c4m2/partdesign-revolution-axis-angle-body",
                    "c4m2/partdesign-groove-axis-angle-body",
                    "c5m1/partdesign-revolution-two-angles-body",
                    "c5m1/partdesign-groove-two-angles-body",
                    "c5m1/partdesign-groove-through-all-body",
                    "c5m1/partdesign-revolution-part-edge-axis",
                    "c5m1/partdesign-revolved-zero-sum-diagnostic",
                    "c5m1/partdesign-revolved-upto-diagnostics",
                    "c5m1/partdesign-revolved-profile-fuse-diagnostics",
                    "c51m1/partdesign-revolution-internalface-profile",
                    "c51m1/partdesign-revolution-featurefirst-body",
                    "c51m1/partdesign-revolution-datumline-axis",
                    "c51m1/partdesign-revolution-appline-axis",
                    "c51m1/partdesign-revolution-sketch-axisn",
                    "c51m1/partdesign-revolution-uptoface-body",
                    "c51m1/partdesign-revolution-uptofirst-body",
                    "c51m1/partdesign-revolution-uptolast-body",
                    "c51m1/partdesign-groove-uptofirst-body",
                    "c51m1/partdesign-groove-uptoface-body"}},
                  {"diagnostics",
                   {"invalid_angle",
                    "invalid_axis",
                    "invalid_property_value",
                    "execution_failed",
                    "missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "unsupported_subshape_kind",
                    "unsupported_profile_region",
                    "unsupported_property"}},
                  {"deferred", nlohmann::json::array()},
                  {"exact_blockers",
                   {{"id", "partdesign_groove_upto_brepfeat_cut_native_failure"},
                    {"source",
                     "/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::"
                     "TopoShape::makeElementRevolution"},
                    {"freecad_message", "Revolution: Up to face: Could not revolve the sketch!"},
                    {"fixtures",
                     {"c51m1/partdesign-groove-uptofirst-body",
                      "c51m1/partdesign-groove-uptoface-body"}}}},
                  {"remaining_gaps", {"partdesign_groove_upto_brepfeat_cut_native_failure"}},
              }},
             {"boolean",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureBoolean.cpp
                  // ::Boolean::execute(), exposes Type "Fuse", "Cut" and "Common"; the
                  // LinkStage3 "Compound" / "Section" branches are commented out. C5.1
                  // productizes those two request Types through
                  // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
                  // ::TopoShape::makeElementBoolean(), where OpCodes::Compound routes to
                  // makeElementCompound and OpCodes::Section routes to FCBRepAlgoAPI_Section.
                  {"status", "supported_c51s2_boolean_compound_section_with_exact_body_policy"},
                  {"type_ids", {"PartDesign::Boolean"}},
                  {"supported",
                   {"Type=Fuse",
                    "Type=Cut",
                    "Type=Common",
                    "Type=Compound productized from Part TopoShape maker",
                    "Type=Section productized as non-solid edge/wire output",
                    "Group Body tool solids",
                    "multi-tool Group order",
                    "BaseFeature Body chain",
                    "AllowCompound=true multi-solid result",
                    "AllowCompound=false Compound multi-solid diagnostic",
                    "Section standalone edge/wire result",
                    "Section Body Tip non-solid diagnostic",
                    "Body replacement Tip replay",
                    "single solid result rule",
                    "maker_history:boolean"}},
                  {"fixtures",
                   {"c4m2/partdesign-boolean-cut-body-tool",
                    "c4m2/partdesign-boolean-fuse-body-tool",
                    "c4m2/partdesign-boolean-common-body-tool",
                    "c4m2/partdesign-boolean-deferred-diagnostics",
                    "c5m2/partdesign-boolean-allow-compound-multisolid",
                    "c5m2/partdesign-boolean-multi-tool-ownership",
                    "c5m2/partdesign-boolean-multisolid-rejected",
                    "c5m2/partdesign-boolean-tool-missing-shape-diagnostic",
                    "c51m2/partdesign-boolean-compound-body-tip",
                    "c51m2/partdesign-boolean-compound-disallowed",
                    "c51m2/partdesign-boolean-section-standalone",
                    "c51m2/partdesign-boolean-section-body-tip-diagnostic",
                    "c51m2/partdesign-boolean-section-no-intersection"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "missing_target",
                    "invalid_link_value",
                    "multiple_solids_disallowed",
                    "no_intersection",
                    "partdesign_body_tip_non_solid",
                    "unsupported_property",
                    "execution_failed"}},
                  {"deferred", nlohmann::json::array()},
                  {"product_contract",
                   {{"source_difference",
                     "PartDesign FeatureBoolean.cpp keeps Compound/Section disabled in TypeEnums; "
                     "CAD Core C5.1 supports request-local product Types from Part TopoShape maker "
                     "paths"},
                    {"section_body_policy",
                     "standalone section edge/wire output is supported; Body Tip replacement is "
                     "rejected with exact diagnostic"}}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"loft",
              {
                  // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLoft.cpp
                  // ::Loft::execute(), resolves "Profile" and "Sections", builds ThruSections
                  // shells, sews front/back faces, converts shells to solids, then exposes the
                  // result through AddSubShape for Body additive/subtractive replay.
                  {"status", "supported_c51s3_profile_section_closed_multiwire_sewing"},
                  {"type_ids", {"PartDesign::AdditiveLoft", "PartDesign::SubtractiveLoft"}},
                  {"supported",
                   {"Profile full sketch",
                    "Sections App::PropertyXLinkSubList full sketch",
                    "Profile/Sections explicit subelement selection",
                    "Closed=true with 3+ profile/sections",
                    "multi-section shell loft",
                    "multi-wire profile/section ordering",
                    "Ruled=false/true shell loft flag",
                    "front/back cap sewing",
                    "MapperThruSections history",
                    "MapperSewing modified history",
                    "solidification",
                    "Body additive fuse replay",
                    "Body subtractive cut replay",
                    "maker_history:partdesign_loft"}},
                  {"fixtures",
                   {"c4m2/partdesign-loft-additive-body",
                    "c4m2/partdesign-loft-subtractive-body",
                    "c5m3/partdesign-loft-closed-multisection",
                    "c5m3/partdesign-loft-multiwire-ordering",
                    "c5m3/partdesign-loft-allow-compound-diagnostic",
                    "c51m3/partdesign-loft-closed-multisection",
                    "c51m3/partdesign-loft-multiwire-ordering",
                    "c51m3/partdesign-loft-allow-compound-diagnostic"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_subshape",
                    "invalid_profile",
                    "invalid_sections",
                    "open_profile",
                    "multiple_solids_disallowed",
                    "execution_failed",
                    "unsupported_property"}},
                  {"deferred", nlohmann::json::array()},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"pipe",
              {
                  // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp
                  // ::Pipe::execute(), resolves "Profile" and "Spine", calls
                  // setupAlgorithm(BRepOffsetAPI_MakePipeShell), writes AddSubShape, then Body
                  // applies additive/subtractive fuse/cut replay.
                  {"status", "supported_c51s4_pipe_advanced_with_exact_source_blockers"},
                  {"type_ids", {"PartDesign::AdditivePipe", "PartDesign::SubtractivePipe"}},
                  {"supported",
                   {"Profile full sketch",
                    "Profile/Sections explicit first subelement selection",
                    "Spine selected Edge/Wire path",
                    "Mode=Standard",
                    "Mode=Fixed",
                    "Mode=Frenet",
                    "Mode=Auxiliary",
                    "Mode=Binormal",
                    "Transformation=Constant",
                    "Transformation=Multisection",
                    "Transition=Transformed",
                    "Transition=Right corner",
                    "Transition=Round corner",
                    "front/back cap sewing",
                    "MapperSewing modified history",
                    "solidification",
                    "PipeShell maker history",
                    "Body additive fuse replay",
                    "Body subtractive cut replay",
                    "maker_history:partdesign_pipe"}},
                  {"fixtures",
                   {"c4m2/partdesign-pipe-additive-body",
                    "c4m2/partdesign-pipe-subtractive-body",
                    "c4m2/partdesign-pipe-deferred-diagnostics",
                    "c5m3/partdesign-pipe-sections-transformation",
                    "c5m3/partdesign-pipe-transition-variants",
                    "c5m3/partdesign-pipe-auxiliary-binormal-diagnostics",
                    "c51m4/partdesign-pipe-fixed-round-body",
                    "c51m4/partdesign-pipe-auxiliary-binormal-modes",
                    "c51m4/partdesign-pipe-selected-spine-multisection",
                    "c51m4/partdesign-pipe-source-backed-blockers"}},
                  {"diagnostics",
                   {"missing_property",
                    "missing_link_target",
                    "invalid_property",
                    "invalid_subshape",
                    "execution_failed",
                    "unsupported_property"}},
                  {"deferred", nlohmann::json::array()},
                  {"exact_blockers",
                   {{"partdesign_pipe_transformation_laws_source_commented",
                     {{"properties",
                       {"Transformation=Linear", "Transformation=S-shape", "Transformation=Interpolation"}},
                      {"source", "/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute"},
                      {"evidence", "Linear/S-shape ScalingData law branches are present only in a commented block"}}},
                    {"partdesign_pipe_spine_tangent_source_commented",
                     {{"properties", {"SpineTangent", "AuxiliarySpineTangent"}},
                      {"source", "/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::buildPipePath"},
                      {"evidence", "getContinuousEdges(shape, subedge) call is commented out"}}}}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"datum_attachment",
              {
                  // FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPoint.cpp,
                  // DatumLine.cpp, DatumPlane.cpp and DatumCS.cpp create point/line/plane/LCS
                  // shapes from Placement; Body.cpp relinks datum roles through Origin.
                  // AttachExtension.cpp::positionBySupport() calls "calculateAttachedPlacement"
                  // and can write back AttachmentSupport subnames. cad-core supports the C51-S5
                  // selected non-GUI AttachEngine modes and returns request-local update
                  // suggestions instead of mutating a backend session.
                  {"status", "supported_c51x_selected_attach_engine_with_datum_point_single_input"},
                  {"type_ids",
                   {"PartDesign::Point",
                    "PartDesign::Line",
                    "PartDesign::Plane",
                    "PartDesign::CoordinateSystem",
                    "App::Origin"}},
                  {"supported",
                   {"DatumPoint Placement vertex",
                    "DatumLine Placement direction",
                    "DatumPlane Placement face",
                    "Datum CoordinateSystem axes",
                    "Body Origin datum role relink",
                    "downstream ReferenceAxis DatumLine/DatumCS use",
                    "FlatFace selected MapMode",
                    "ObjectXY/ObjectXZ/ObjectYZ selected MapModes",
                    "ObjectOrigin selected MapMode",
                    "ObjectX/ObjectY/ObjectZ selected MapModes",
                    "NormalToEdge selected MapMode",
                    "DatumPoint Vertex selected MapMode",
                    "DatumPoint OnEdge selected MapMode",
                    "DatumPoint CenterOfMass selected MapMode",
                    "AttachmentOffset composition",
                    "MapReversed/Reverse composition",
                    "MapPathParameter/Parameter normalized edge parameter",
                    "AttachmentSupport StableSubList/ShadowSub request-local writeback"}},
                  {"fixtures",
                   {"p7/datum-coordinate-system-reference-axis",
                    "p7/datum-coordinate-system-sketch-support",
                    "c3m5/body-origin-link-placement",
                    "c4m2/partdesign-datum-attachment-deferred-diagnostics",
                    "c5m4/partdesign-datum-attachment-mapmode-diagnostics",
                    "c51m5/partdesign-datum-selected-mapmodes",
                    "c51m5/partdesign-datum-offset-reverse-writeback",
                    "c51m5/partdesign-datum-point-single-input-modes"}},
                  {"diagnostics",
                   {"invalid_placement",
                    "missing_link_target",
                    "unsupported_property",
                    "attachment_support_invalid_shape",
                    "attachment_parameter_invalid",
                    "subname_resolve_failed"}},
                  {"deferred",
                   {"GUI Attachment editor / ViewProvider / TaskPanel",
                    "GUI interactive datum resize visual behavior"}},
                  {"non_goals",
                   {"GUI Attachment editor / ViewProvider / TaskPanel",
                    "GUI interactive datum resize visual behavior",
                    "cross-request backend attachment session"}},
                  {"exact_blockers",
                   {{"datum_attach_engine_remaining_modes",
                     {{"modes",
                       {"Translate",
                        "TangentPlane",
                        "FrenetNB",
                        "FrenetTN",
                        "FrenetTB",
                        "Concentric",
                        "SectionOfRevolution",
                        "ThreePointsPlane",
                        "ThreePointsNormal",
                        "Folding",
                        "AxisOfCurvature",
                        "Directrix1",
                        "Directrix2",
                        "Asymptote1",
                        "Asymptote2",
                        "Normal",
                        "Binormal",
                        "TangentU",
                        "TangentV",
                        "TwoPointLine",
                        "IntersectionLine",
                        "ProximityLine",
                        "Focus1",
                        "Focus2",
                        "CenterOfCurvature",
                        "IntersectionPoint",
                        "ProximityPoint1",
                        "ProximityPoint2"}},
                      {"source",
                       "/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/"
                       "Attacher.cpp::AttachEngine3D/Line/Point::_calculateAttachedPlacement"},
                      {"evidence",
                       "C51-S5 first batch supports FlatFace, ObjectXY/ObjectXZ/ObjectYZ, "
                       "ObjectOrigin/ObjectX/ObjectY/ObjectZ and NormalToEdge; "
                       "C51X supports AttachEnginePoint Vertex/OnEdge/CenterOfMass with FreeCADCmd "
                       "expected"}}}}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"hole",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
                  // ::Hole::threadDescription[] stores thread rows, ::Hole::readCutDefinitions()
                  // loads "Resources/Hole", ::Hole::makeThread() builds the model thread, and
                  // ::Hole::findHoles() calls "makeShapeWithElementMap(protoHole, mapper, {baseshape})".
                  {
                      "thread_tables",
                      {"ISOMetricProfile", "ISOMetricFineProfile", "UNC", "UNF", "UNEF", "NPT", "BSP", "BSW", "BSF", "ISOTyre"}
                  },
                  {"diameter_sources",
                   {"Diameter",
                    "thread_tap_drill",
                    "thread_clearance",
                    "thread_uts_clearance",
                    "thread_clearance_fallback",
                    "thread_whitworth_fallback",
                    "thread_npt_fallback",
                    "thread_pitch_fallback"}},
                  {"head_cut_types", {"None", "Counterbore", "Countersink", "Counterdrill"}},
                  {"head_cut_definition_files",
                   {"din7984.json",
                    "iso10642.json",
                    "iso10642-fine.json",
                    "iso14583.json",
                    "iso14583part.json",
                    "iso2009.json",
                    "iso4762.json",
                    "iso4762-fine.json",
                    "iso4762_7089.json",
                    "iso7046.json",
                    "iso12474-fine.json"}},
                  {"model_thread",
                   {
                       {"status", "done_first_slice"},
                       {"geometry", "pipe_shell"},
                       {"properties",
                        {"ModelThread",
                         "ThreadClass",
                         "ThreadDirection",
                         "UseCustomThreadClearance",
                         "CustomThreadClearance"}},
                   }},
                  {"history",
                   {
                       {"status", "element_map_freeze_first_slice"},
                       {"covered",
                        {"find_holes_make_shape_with_element_map",
                         "profile_source_tool_face_mapper_history",
                         "point_profile_head_cut_history",
                         "subtractive_body_cut_history",
                         "model_thread_tool_face_history",
                         "model_thread_compound_tool_shape",
                         "threaded_model_thread_head_cut_native_oracle"}},
                       {"known_gap_fixtures", nlohmann::json::array()},
                       {"remaining", nlohmann::json::array()},
                   }},
                  {"native_oracle_fixtures",
                   {"p7/hole-supported-threaded-dynamic-iso2009",
                    "p7/hole-supported-threaded-dynamic-din7984",
                    "p7/hole-supported-model-thread-metric",
                    "p7/hole-point-profile",
                    "p7/hole-supported-point-counterbore",
                    "p7/hole-supported-model-thread-counterbore"}},
                  {"native_oracle_known_gap_fixtures", nlohmann::json::array()},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
         }},
        {"supported_type_ids", registry.typeIds()},
        {"export_formats", cad_core::part::supportedShapeFileFormats()},
        {"diagnostic_codes", diagnosticCodeList()},
        {"adapters",
         {
             {"contract_version", "cad-core-result-v1"},
             {"core_entrypoints",
              {"cad_core_recompute_json",
               "cad_core_export_json",
               "cad_core_capabilities_json",
               "cli_recompute",
               "worker_recompute",
               "wasm_recompute"}},
             {"schema_parity",
              {
                  {"core_result_producers",
                   {"cad_core::runtime::recomputeResultJson",
                    "cad_core::part::partGeometryCurveResultJson"}},
                  {"entrypoints",
                   {"cli_recompute", "cad_core_recompute_json", "worker_recompute", "wasm_recompute"}},
                  {"contract", "same_request_local_core_result"},
              }},
             {"stateless_result_channels",
              {"results", "elementReferenceUpdates", "documentObjectUpdates", "diagnostics", "binaryPayloads"}},
             {"resource_diagnostics", {"mesh_limit_exceeded", "adapter_resource_limit"}},
             {"c_api_export",
              {"buffer_only", "rejects_server_file_paths", "metadata_diagnostics", "stl_deflection"}},
             {"cli_export", {"file_protocol", "requires_object_format_file", "stl_deflection"}},
             {"worker_adapter",
              {
                  {"entrypoint", "cad_core_worker_recompute_json"},
                  {"core_recompute", true},
                  {"result_contract", "cad-core-result-v1"},
                  {"resource_diagnostics", {"mesh_limit_exceeded", "adapter_resource_limit"}},
                  {"state", "stateless_request_local"},
              }},
             {"wasm_adapter",
              {
                  {"entrypoint", "cad_core_wasm_recompute_json"},
                  {"core_recompute", true},
                  {"result_contract", "cad-core-result-v1"},
                  {"resource_diagnostics", {"mesh_limit_exceeded", "adapter_resource_limit"}},
                  {"toolchain_contract", "source_and_schema_delivered"},
              }},
             {"mesh",
              {
                  {"streaming_limits",
                   {"max_vertices", "max_triangles", "chunk_triangles", "mesh_limit_exceeded"}},
                  {"binary_payload_limits",
                   {"max_bytes", "adapter_resource_limit", "metadata_diagnostics"}},
                  {"binary_payloads",
                   {"cad_core_mesh_binary_json",
                    "cad-core-binary-mesh-v1",
                    "f64x3_vertices",
                    "u32x3_triangles",
                    "metadata_diagnostics"}},
              }},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"assembly",
         {
             // FreeCAD:
             // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
             // ::AssemblyObject::solve(), uses "fixGroundedParts()" before
             // "mbdAssembly->runPreDrag()"; ::setNewPlacements() writes
             // "propPlacement->setValue(newPlacement)" after solving. CAD Core requires
             // OndselSolver at build time and publishes only the real request-local mode.
             {"ondsel_solver_adapter", ondselSolverCapabilityJson()},
             {"representative_solver_adapter", representativeSolverCapabilityJson()},
             {"solver_validation", assemblyValidationCapabilityJson()},
             {"placement_writeback", placementWritebackCapabilityJson()},
             {"supported_joint_matrix",
              {"Fixed",
               "Revolute",
               "Cylindrical",
               "Slider",
               "Ball",
               "Distance",
               "Parallel",
               "Perpendicular",
               "Angle",
               "Gears",
               "Belt",
               "RackPinion",
               "Screw"}},
             {"unsupported_joint_matrix", nlohmann::json::array()},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"wire_joiner",
         {
             {"generated_open_export_bridge",
              {
                  {"status", "covered_full"},
                  {"mode", "wire_joiner_mapper_history_producer_evidence"},
                  {"deleted_fields",
                   {"result_slot_vertex_evidence_output",
                    "open_wire_compound_result_slot_vertex_evidence_wire_info_count",
                    "open_wire_compound_wire_info_source_edge_producer_output_sidecar",
                    "child_wire_result_slot_endpoint_materialization_edge_shape",
                    "endpoint_materialization_edge_seed_removed",
                    "edge_info_producer_open_export_shape",
                    "edge_info_result_wire_producer_source_edge_export_shape",
                    "result_wire_producer_identity_source_shape_ready",
                    "result_wire_producer_identity_classifier_booleans",
                    "open_wire_compound_current_member_closed_source_result_slot_bridge",
                    "open_wire_compound_current_member_closed_source_result_slot_bridge_wire_info_"
                    "count",
                    "result_wire_producer_blocker_transitional_result_slot_shape_still_used",
                    "result_wire_producer_state_transitional_result_slot_candidate",
                    "edgeInfo_resultWireProducerCandidate_internal",
                    "child_wire_result_slot_endpoint_materialization_evidence_vertices_field",
                    "child_wire_producer_ledger_wire_from_result_slot_evidence_field",
                    "open_wire_compound_producer_ledger_wire_from_result_slot_evidence",
                    "wire_joiner_endpoint_materialization_ledger_vertex_seed",
                    "child_wire_result_slot_endpoint_materialization_vertex_ledger",
                    "producer_child_wire_result_slot_endpoint_materialization_counted",
                    "wire_joiner_endpoint_materialization_ledger_current_member_debt_scoped",
                    "child_wire_endpoint_materialization_evidence_field_renamed",
                    "child_wire_producer_ledger_wire_endpoint_materialization_evidence_field_"
                    "renamed",
                    "result_slot_endpoint_materialization_ledger",
                    "matched_endpoint_materialization_evidence",
                    "endpoint_materialization_evidence_vertex_matches_other_output",
                    "endpoint_materialization_evidence_vertex_identity",
                    "open_wire_compound_endpoint_provenance_endpoint_materialization_matched_"
                    "vertex_count",
                    "open_wire_compound_current_member_split_ledger_endpoint_materialization_"
                    "distinct_vertex_count",
                    "open_wire_compound_current_member_split_ledger_endpoint_materialization_other_"
                    "output_matched_vertex_count",
                    "result_slot_only_identity",
                    "open_wire_compound_current_member_split_ledger_result_slot_only_vertex",
                    "open_wire_compound_current_member_split_ledger_result_slot_only_vertex_total",
                    "open_wire_compound_export_source_result_wire_producer_slot_value",
                    "open_wire_compound_export_source_history_materialized_child_slot_value",
                    "result_wire_producer_state_source_shape_ready",
                    "history_materialization_entry_open_wire_compound_child_wire_candidate_bool",
                    "history_materialization_binding_source_edgeinfo_candidate_list",
                    "open_wire_compound_source_edge_producer_output",
                    "open_wire_compound_source_edge_producer_output_wire_info_count",
                    "child_wire_source_edge_producer_output_public_diagnostic",
                    "edge_level_producer_ledger_ready_from_history_materialization_ledger",
                    "source_shape_ready_derived_from_history_materialization_ledger_open_export_"
                    "edge",
                    "open_wire_compound_producer_ledger_edge_materialized",
                    "child_wire_producer_ledger_edge_copy_gate_from_history_materialization_ledger",
                    "mapper_evidence_result_wire_producer_identity_fields",
                    "mapper_diagnostic_result_wire_producer_blocker_status",
                    "mapper_diagnostic_missing_producer_identity_fallback",
                    "result_wire_producer_entry_gate_from_materialization_entry_identity",
                    "open_wire_compound_export_source_from_materialization_entry",
                    "history_materialization_entry_typed_open_wire_compound_export_source",
                    "wire_joiner_history_materialization_entry_open_export_producer_edge",
                    "named_shape_history_missing_result_wire_identity_count",
                    "element_map_result_wire_identity_mismatch_count",
                    "open_wire_compound_current_member_split_ledger_candidate_missing_shared_"
                    "output_identity_count",
                    "open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked_"
                    "wire_info_count",
                    "open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count",
                    "open_wire_compound_current_member_split_ledger_output_unmatched_vertex_total",
                    "open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked"}},
                  {"covered",
                   {"openWireCompound_child_wire_ownership",
                    "openWireCompound_child_wire_source_lineage",
                    "ResultWireProducerIdentity",
                    "history_materialization_entry_open_wire_compound_export_source_removed",
                    "producer_readiness_promoted_after_openWireCompound_child_materialization",
                    "edge_level_producer_ledger_ready_gate_removed",
                    "child_wire_producer_ledger_edge_copy_gate_removed",
                    "child_wire_producer_readiness_from_materialized_wire",
                    "history_materialization_binding_open_wire_compound_eligible_cache_removed",
                    "history_materialization_entry_source_edgeinfo_identity_cache_removed",
                    "history_materialization_entry_open_export_gate_cache_removed",
                    "history_materialization_entry_full_ahistory_cache_removed",
                    "history_materialization_entry_ahistory_source_lineage_cache_removed",
                    "history_materialization_binding_final_edgeinfo_index",
                    "result_wire_producer_identity_source_shape_ready_removed",
                    "result_wire_producer_identity_classifier_booleans_removed",
                    "active_legacy_helper_open_export_override_not_exported",
                    "producer_identity_consumed_without_legacy_helper_flag",
                    "legacy_helper_reason_not_exported",
                    "legacy_helper_reason_string_removed",
                    "edge_info_result_wire_producer_candidate_ledger",
                    "child_wire_legacy_helper_ahistory_sidecar_removed",
                    "child_wire_dead_helper_sidecars_removed",
                    "child_wire_producer_identity_candidate_ledger",
                    "result_wire_producer_ledger_entries_from_child_wire",
                    "child_wire_result_wire_producer_entry_gate",
                    "child_wire_result_wire_producer_entry_gate_from_classified_identity",
                    "result_wire_producer_entry_gate_after_child_wire_identity",
                    "edge_info_open_export_wire_helper_removed",
                    "edge_info_result_slot_vertex_evidence_removed",
                    "edge_info_identity_source_edge_output_gate_removed",
                    "child_wire_producer_ledger_wire_materialized",
                    "wire_joiner_history_relation_from_child_wire_ledger",
                    "wire_joiner_history_event_ledger",
                    "topo_consumes_wire_joiner_history_event_ledger",
                    "topo_consumes_openWireCompound_child_wire_ownership_ledger",
                    "wire_joiner_open_export_mapper_history_concrete_events",
                    "topo_mapper_evidence_result_wire_producer_identity_removed",
                    "topo_mapper_diagnostic_result_wire_producer_blocker_removed",
                    "topo_mapper_diagnostic_missing_producer_identity_removed",
                    "topo_result_wire_identity_counters_removed",
                    "wire_joiner_open_export_element_map_unique_child_wire_alias",
                    "topo_open_export_relation_fallback_removed",
                    "child_wire_open_export_ownership_source_ledger",
                    "open_wire_compound_export_source_from_child_wire_final_identity",
                    "open_wire_compound_export_source_ahistory_producer_child_wire",
                    "child_wire_shape_identity_inventory",
                    "result_wire_producer_entry_consumes_child_wire_ownership",
                    "current_member_endpoint_identity_debt_per_endpoint",
                    "child_wire_source_vmap_endpoint_ledger",
                    "child_wire_vmap_replacement_event_ledger",
                    "child_wire_endpoint_provenance_ledger",
                    "child_wire_root_member_producer_lifecycle_ledger",
                    "splitEdges_fragment_to_source_ledger",
                    "splitEdges_modified_generated_fragment_ledger",
                    "splitEdges_source_lineage_uses_sourceEdgeArray_identity",
                    "splitEdges_input_edgeinfo_source_sidecar",
                    "splitEdges_source_identity_fallback_counter_split",
                    "splitEdges_history_shape_geometry_bridge_counter",
                    "closed_cycle_open_export_from_split_fragment_ledger",
                    "result_slot_vertex_evidence_endpoint_materialization_not_export_shape",
                    "producer_child_wire_prefers_vmap_vertex_ledger",
                    "current_member_child_wire_uses_root_member_ledger_without_result_slot_output",
                    "current_member_split_ledger_vertex_multiplicity_diagnostic",
                    "current_member_split_ledger_vertex_debt_recorded_on_child_wire",
                    "current_member_split_ledger_candidate_output_identity_recorded",
                    "current_member_split_ledger_endpoint_identity_resolver",
                    "current_member_split_ledger_candidate_multiplicity_loss_diagnostic",
                    "current_member_closed_source_result_slot_bridge_diagnostic_removed",
                    "transitional_result_slot_shape_still_used_blocker_removed",
                    "transitional_result_slot_candidate_state_removed",
                    "open_export_history_consumes_openWireCompound_child_wire_output",
                    "open_export_history_no_edgeinfo_reexport_fallback",
                    "missing_child_wire_history_diagnostic_without_edgeinfo_payload",
                    "getOpenWires_consumes_openWireCompound_child_ledger_only",
                    "wire_joiner_mapper_history_producer_evidence_ledger",
                    "history_materialization_entry_open_export_producer_edge_removed",
                    "mapper_history_aHistory_open_export_element_map_lifecycle",
                    "mapper_history_aHistory_split_deleted_terminal_history",
                    "wire_joiner_noOriginal_deleted_relation_from_mapper_history"}},
                  {"diagnostic_fields",
                   {"open_wire_compound_export_source",
                    "open_wire_compound_edge_info_iteration",
                    "open_wire_compound_edge_info_iteration2",
                    "open_wire_compound_owner_wire_info",
                    "open_wire_compound_owner_wire_info2",
                    "open_wire_compound_open_leaf_export",
                    "open_wire_compound_unowned_open_edge_export",
                    "open_wire_compound_root_current_member_child_producer",
                    "open_wire_compound_child_shape_identity_recorded",
                    "open_wire_compound_child_wire_edge_count",
                    "open_wire_compound_child_wire_vertex_count",
                    "open_wire_compound_source_vmap_endpoint_ledger",
                    "open_wire_compound_source_vmap_endpoint_ledger_recorded",
                    "open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
                    "open_wire_compound_endpoint_provenance_recorded",
                    "open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
                    "open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
                    "open_wire_compound_endpoint_provenance_candidate_matched_vertex_count",
                    "open_wire_compound_endpoint_provenance_unmatched_vertex_count",
                    "open_wire_compound_vmap_replacement_event_count",
                    "open_wire_compound_vmap_replacement_events",
                    "wire_joiner_history_event_count",
                    "wire_joiner_history_event_from_child_wire_ledger_count",
                    "open_wire_compound_current_member_split_ledger_vertex_candidate",
                    "open_wire_compound_current_member_split_ledger_vertex_debt_recorded",
                    "open_wire_compound_current_member_split_ledger_member_vertex_count",
                    "open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
                    "open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
                    "open_wire_compound_current_member_split_ledger_output_candidate_matched_"
                    "vertex_count",
                    "open_wire_compound_current_member_split_ledger_output_distinct_vertex_count",
                    "open_wire_compound_current_member_split_ledger_candidate_distinct_vertex_"
                    "count",
                    "open_wire_compound_current_member_split_ledger_candidate_vertex_multiplicity_"
                    "loss_count",
                    "open_wire_compound_current_member_split_ledger_output_other_output_matched_"
                    "vertex_count",
                    "open_wire_compound_current_member_split_ledger_candidate_other_output_matched_"
                    "vertex_count",
                    "open_wire_compound_current_member_split_ledger_candidate_vertex_reuse_risk_"
                    "count",
                    "open_wire_compound_current_member_split_ledger_endpoint_identity_resolver",
                    "open_wire_compound_current_member_split_ledger_output_vertex_debt"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"purge_as_original_bridge",
              {
                  {"status", "covered_full"},
                  {"mode", "mapper_history_noOriginal_deleted_lifecycle"},
                  {"deleted_fields",
                   {"purgeAsOriginalOpenEdge",
                    "purge_bridge_history_field",
                    "source_identity_purge_bridge_summary_fields",
                    "open_wire_compound_purge_bridge_summary_fields",
                    "open_wire_compound_no_original_purge_candidate",
                    "open_wire_compound_no_original_purge_candidate_wire_info_count",
                    "source_identity_no_original_purge_candidate_edge_info_count",
                    "open_wire_compound_no_original_purge_unmatched_wire_info_count"}},
                  {"covered",
                   {"sourceEdgeArray_original_source_ledger",
                    "splitFromInputEdge",
                    "splitFromInputEdge_from_splitter_history_ledger",
                    "noOriginal_uses_splitter_history_fragment_ledger",
                    "noOriginal_final_output_prune_removed",
                    "edge_info_noOriginal_candidate_helper_removed",
                    "openWireCompound_noOriginal_candidate_public_bridge_removed",
                    "sourceVertexIdentity",
                    "openWireCompound_child_wire_noOriginal_match",
                    "openWireCompound_child_wire_noOriginal_purge_verdict",
                    "openWireCompound_noOriginal_group_purge_verdict",
                    "openWireCompound_source_shared_vertex_gate",
                    "openWireCompound_child_wire_noOriginal_shared_source_edge_ledger",
                    "noOriginal_deleted_relation_from_mapper_history",
                    "noOriginal_split_deleted_terminal_history"}},
                  {"diagnostic_fields",
                   {"open_wire_compound_no_original_shared_source_ledger_recorded",
                    "open_wire_compound_no_original_shared_source_edge_count",
                    "open_wire_compound_no_original_shared_source_matched_edge_count",
                    "open_wire_compound_no_original_shared_source_unmatched_edge_count"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"bridge_rules", {"no_new_helper_reason", "diagnostic_only", "no_fixture_shaped_pruning"}},
         }},
        {"topo_history",
         {
             {"mapper_history_core",
              {"source_endpoint",
               "target_endpoint",
               "shape_kind",
               "relation",
               "maker_stage",
               "evidence",
               "recoverability",
               "diagnostic_status",
               "summary_only_diagnostics",
               "legacy_history_conversion",
               "element_map_preserved_aliases"}},
             {"stable_subname_resolution",
              {"indexed",
               "source_preserved",
               "one_to_one_history",
               "unique_same_kind_split_recovery",
               "reference_shadow_recovery"}},
             {"maker_history",
              {"prism",
               "part_design_revolve",
               "body_boolean",
               "part_boolean",
               "section",
               "part_offset",
               "part_design_loft",
               "part_design_pipe",
               "general_fuse",
               "link_retag",
               "sketch_internalshape_producer_evidence",
               "taper_thru_sections",
               "dressup_addsubshape_slot",
               "dressup_multi_selection_history",
               "dressup_chamfer_parameter_variants",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
               // reads "Angle", "NeutralPlane", "PullDirection" and selected FaceN before
               // calling TopoShape::makeElementDraft().
               "dressup_draft_parameter_variants",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
               // ::Thickness::execute(), reads "Value", "Mode", "Join", "Reversed" and
               // "Intersection" before calling TopoShape::makeElementThickSolid().
               "dressup_thickness_parameter_variants",
               "thickness_multi_solid_fuse_history",
               "chain_dressup_pattern_history",
               "transformed_copy_terminal",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp::TopoShape::fix(),
               // calls "makeShapeWithElementMap(fixThis.Shape(), MapperHistory(fixThis), {*this})"
               // because "ShapeFix_Shape may delete ... or modify the input shape".
               // This C3-M1 slice covers ShapeFix small-edge deleted history and
               // ShapeFix_Root::Context()->History() modified history. FreeCAD test
               // tests/src/Mod/Part/App/WireJoiner.cpp::Generated says "no methods in
               // ShapeFix_Wire call AddGenerated()", so ShapeFix generated is recorded as an
               // audited non-producer rather than a fixture to synthesize.
               "shapefix_deleted_small_edge",
               "shapefix_root_modified_history",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute(),
               // Features mode calls "feature->getAddSubShape(fuseShape, cutShape)" and transforms
               // each original with "makeElementTransform", while WholeShape transforms the support.
               "transformed_pattern_addsub_ownership",
               "transformed_pattern_full_history",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/
               // FeatureHole.cpp::Hole::findHoles(), calls
               // "makeShapeWithElementMap(protoHole, mapper, {baseshape})" after populating
               // Modified history from profile Edge/Vertex sources to protoHole faces.
               "hole_find_holes_profile_source_history",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp
               // ::TopoShape::read(), dispatches importStep/importIges/importBrep and stores the
               // recomputed TopoShape; cad-core exposes request-local import ElementMap aliases.
               "import_shape_element_map",
               // FreeCAD:
               // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::MapperThruSections maps "GeneratedFace(s)", "FirstShape()" and "LastShape()"
               // for BRepOffsetAPI_ThruSections results created by Part::Loft.
               "loft_thru_sections",
               // FreeCAD:
               // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeElementPipeShell(), returns
               // "makeElementShape(mkPipeShell, shapes, op)" for Part::Sweep PipeShell history.
               "pipeshell",
               // FreeCAD:
               // /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeElementFilledFace(), returns
               // makeElementShape(maker, _shapes, Part::OpCodes::FilledFace) after
               // BRepOffsetAPI_MakeFilling::Build().
               "filling",
               "refine_modified_deleted_generated"}},
             // FreeCAD:
             // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
             // ::TopoShape::makeShapeWithElementMap(), consumes MapperHistory producers under the
             // key "history"; C3-M1 exposes the producer matrix so complete_mapper_history is no
             // longer an opaque capability gap.
             {"producer_matrix",
              {
                  {"prism", {{"status", "covered"}, {"covered", {"maker_history"}}}},
                  {"body_boolean",
                   {{"status", "covered"},
                    {"covered", {"maker_history", "flagged_compound_tool_expansion"}}}},
                  {"part_boolean",
                   {{"status", "covered"},
                    {"covered", {"maker_history", "flagged_compound_tool_expansion"}}}},
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
                  // FeaturePartSection.cpp::Section::makeOperation(), reads "Approximation",
                  // calls "setAutoFuzzy()", and Part::Boolean::execute() consumes
                  // "res.makeElementShape(*mkBool, shapes, opCode())".
                  {"section",
                   {{"status", "covered"},
                    {"covered",
                     {"approximation_property",
                      "auto_fuzzy_value",
                      "source_qualified_edge_history",
                      "terminal_deleted_history"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"part_offset",
                   {{"status", "done_second_slice"},
                    {"covered",
                     {"face_source_offset",
                      "maker_history",
                      "fill_offset",
                      "solid_source_make_element_solid",
                      "offset2d_face_no_fill",
                      "offset2d_face_fill_closed",
                      "offset2d_open_wire_no_fill",
                      "offset2d_open_wire_fill",
                      "offset2d_compound_child_recursive",
                      "offset2d_compound_collective",
                      "thickness_single_solid_face",
                      "thickness_mode_join_oracle"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"general_fuse", {{"status", "covered"}, {"covered", {"maker_history"}}}},
                  {"refine",
                   {{"status", "covered"},
                    {"covered", {"modified", "deleted", "generated"}},
                    {"capability", "refine_modified_deleted_generated"}}},
                  {"shape_fix",
                   {{"status", "covered_no_generated_producer"},
                    {"covered", {"deleted_small_edge", "root_modified", "generated_empty_review"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"import_shape",
                   {{"status", "done_first_slice"},
                    {"covered", {"step", "brep", "iges", "owner_qualified_alias"}},
                    {"remaining", {"changed_file_deleted_reference_recovery"}}}},
                  {"link_retag",
                   {{"status", "covered"},
                    {"covered", {"source_prefixed_stable_key", "mapped_postfix_alias"}}}},
                  {"sketch_internalshape",
                   {{"status", "done_first_slice"},
                    {"covered",
                     {"facemaker",
                      "wire_joiner_producer_evidence",
                      "wire_joiner_history_element_map_unique_child_wire_alias",
                      "mixed_bounded_faces_open_wires_oracle"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"taper_thru_sections", {{"status", "covered"}, {"covered", {"generated_history"}}}},
                  {"loft_thru_sections",
                   {{"status", "done_expected_backed_first_batch"},
                    {"covered",
                     {"generated_faces",
                      "first_shape_last_shape_history",
                      "source_profile_sections",
                      "solid_ruled_closed_max_degree_fixtures"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"pipeshell",
                   {{"status", "done_expected_backed_first_batch"},
                    {"covered",
                     {"generated_modified_history",
                      "spine_profile_sources",
                      "spine_sublist_compound",
                      "solid_frenet_transition_fixtures"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"part_filling",
                   {{"status", "done_expected_backed_first_batch_plus_c5m8_s5_capability_closeout"},
                    {"covered",
                     {"maker_history:filling",
                      "boundary_source_evidence",
                      "closed_wire_default",
                      "connected_boundary_edges_default",
                      "invalid_diagnostics",
                      "initial_surface_load_init_surface",
                      "support_face_source_map",
                      "order_source_map",
                      "locatable_support_order_diagnostics",
                      "constructor_params_metadata",
                      "locatable_param_diagnostics",
                      "non_boundary_constraint_source_evidence",
                      "locatable_non_boundary_diagnostics",
                      "compound_source_expansion",
                      "direct_makefilling_wrapper_lifecycle_diagnostic",
                      "wrapper_uv_point_on_support_lifecycle_diagnostic"}},
                    {"remaining",
                     {"surface_support_order_native_helper_expected",
                      "filling_support_order_g2_expected",
                      "non_default_params_native_helper_expected",
                      "non_boundary_edge_support_native_helper_expected"}}}},
                  {"dressup",
                   {{"status", "done_first_slice"},
                    {"covered",
                     {"addsubshape_slot",
                      "multi_selection_history",
                      "chamfer_parameter_variants",
                      "draft_datum_plane_line",
                      "draft_auto_neutral_plane_guess",
                      "thickness_parameter_variants",
                      "thickness_multi_solid_fuse_history",
                      "failure_diagnostics",
                      "chain_dressup_pattern_history"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"transformed",
                   {{"status", "covered"},
                    {"covered",
                     {"copy_terminal",
                      "pattern_addsub_ownership",
                      "features_single_original",
                      "features_multi_original",
                      "whole_shape_support",
                      "chain_dressup",
                      "refined_support_prefix",
                      "link_retag_composition",
                      "terminal_split_deleted"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"hole",
                   {{"status", "done_first_slice"},
                    {"covered",
                     {"find_holes_make_shape_with_element_map",
                      "profile_source_tool_face_mapper_history",
                      "point_profile_head_cut_history",
                      "model_thread_tool_face_history",
                      "model_thread_compound_tool_shape",
                      "threaded_model_thread_head_cut_native_oracle",
                      "subtractive_body_cut_history"}},
                    {"known_gap_fixtures", nlohmann::json::array()},
                    {"remaining", nlohmann::json::array()}}},
              }},
             {"terminal_history", {"deleted", "split", "merge"}},
             {"element_history_status",
              {"generated_modified",
               "terminal_split_deleted",
               "subname_split_requires_reselect",
               "merge",
               "facemaker_pre_split",
               "facemaker_splitter",
               "facemaker_summary_only",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp
               // ::WireJoinerP::getOpenWires(), calls
               // "shape.makeShapeWithElementMap(comp, MapperHistory(aHistory),
               // {sourceEdges.begin(), sourceEdges.end()}, op)".
               "wire_joiner_history:element_map",
               "import_shape_element_map",
               "shapefix_root_history_modified",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeShapeWithElementMap(), checks "ElementMapPolicy::Drop", calls
               // "dropElementNaming()" and returns before preserving aliases.
               "element_map_policy_drop",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::mapSubElement(shapes), calls "setMappedChildElements(children)"
               // when compound children are partner shapes.
               "element_map_child_map:preserve_source_ranges",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
               // ::ElementMap::addChildElements(), "try to resolve the grand child map now."
               "element_map_child_map:recursive_source_ranges",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::createChildMap(), writes "child.postfix = op" before
               // ElementMap::addChildElements() encodes the child map.
               "element_map_child_map:postfix_source_ranges",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
               // ::ElementMap::hashChildMaps(), rewrites eligible child map entries under
               // "MAPPED_CHILD_ELEMENTS_PREFIX" after addChildElements() creates the child map.
               "element_map_child_map:hashed_child_map_keys",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeElementWires(), "MakeWire will replace vertex of connected
               // edge" and ElementMapPolicy::Propagate then maps the post-maker edges.
               "element_map_policy_propagate:make_element_wires",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeElementShell(), calls "tmp.mapSubElement(*this, op)" before
               // resetting ElementMap on the generated shell.
               "element_map_policy_propagate:make_element_shell",
               "hole_find_holes:profile_source",
               "hole_cut_history:element_map_freeze",
               "hole_model_thread:pipe_shell_tool_history",
               "part_sweep:pipeshell_history",
               "boolean_compound_tool:expand_children",
               "part_compound:make_element_compound",
               "part_offset_fill:sewing_history",
               "part_make_solid:make_element_solid",
               "part_offset2d:face_no_fill_makeoffset",
               "part_offset2d:face_fill_closed_makeoffset",
               "part_offset2d:wire_no_fill_makeoffset",
               "part_offset2d:wire_fill_open_makeoffset",
               "part_offset2d:compound_child_recursive",
               // FreeCAD:
               // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp
               // ::TopoShape::makeElementOffset2D(), "intersection" compound branch collects
               // "non-compounds from this compound for collective offset" before one
               // BRepOffsetAPI_MakeOffsetFix AddWire/Perform pass.
               "part_offset2d:compound_collective_makeoffset",
               "part_thickness:make_thick_solid"}},
             // FreeCAD:
             // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp
             // ::ElementMap::addChildElements(), "To avoid possibly very long recursive child
             // map lookup"; ::hashChildMaps() hashes child maps. TopoShapeExpansion.cpp uses
             // ElementMapPolicy::Propagate to "preserve element mapping".
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"object_metadata",
         {
             {"local_history",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/
                  // FeatureExtrusion.cpp::Extrusion::extrudeShape(), taper calls
                  // "ExtrusionHelper::makeElementDraft"; PartDesign FeatureExtrude.cpp
                  // calls the same helper. TopoShapeExpansion.cpp::MapperThruSections
                  // maps "GeneratedFace(s)", "FirstShape()" and "LastShape()".
                  {"taper_history",
                   {
                       {"status", "covered_full"},
                       {"objects", {"part_design.pad", "part_design.pocket", "part.extrusion"}},
                       {"metadata",
                        {"NamedShape.element_map_status=history_partial",
                         "mapper_history.maker_stage=maker_history",
                         "object_result.topo_naming_history=maker_history:taper_thru_sections"}},
                       {"fixtures",
                        {"p3b/pad-length-taper",
                         "p3b/pad-two-sides-taper",
                         "p3b/pad-symmetric-taper",
                         "p3b/pad-length-taper-inner-wire",
                         "p3b/pocket-length-taper",
                         "p5/part-extrusion-taper",
                         "p5/part-extrusion-reverse-taper",
                         "p5/part-extrusion-two-sided-taper"}},
                       {"remaining_gaps", nlohmann::json::array()},
                   }},
              }},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"known_gaps", nlohmann::json::array()},
    };
}

std::optional<double> readOptionalStlDeflection(const nlohmann::json& request)
{
    if (!request.contains("stl_deflection") || request["stl_deflection"].is_null()) {
        return std::nullopt;
    }
    if (!request["stl_deflection"].is_number()) {
        throw std::runtime_error("stl_deflection must be a positive number");
    }
    const double value = request["stl_deflection"].get<double>();
    if (value <= 0.0) {
        throw std::runtime_error("stl_deflection must be a positive number");
    }
    return value;
}

std::optional<std::size_t> readSizeLimit(
    const nlohmann::json& limits,
    const std::string& snakeCase,
    const std::string& camelCase
)
{
    const auto snake = limits.find(snakeCase);
    const auto camel = limits.find(camelCase);
    const auto it = snake != limits.end() ? snake : camel;
    if (it == limits.end() || it->is_null()) {
        return std::nullopt;
    }
    if (!it->is_number_unsigned() && !it->is_number_integer()) {
        throw std::runtime_error("mesh limit " + snakeCase + " must be an integer");
    }
    const long long value = it->get<long long>();
    if (value < 0) {
        throw std::runtime_error("mesh limit " + snakeCase + " must be non-negative");
    }
    return static_cast<std::size_t>(value);
}

std::optional<nlohmann::json> adapterMeshLimits(const nlohmann::json& request)
{
    if (request.contains("mesh_limits") && request["mesh_limits"].is_object()) {
        return request["mesh_limits"];
    }
    const auto adapter = request.find("adapter");
    if (adapter != request.end() && adapter->is_object()) {
        const auto meshLimits = adapter->find("meshLimits");
        if (meshLimits != adapter->end() && meshLimits->is_object()) {
            return *meshLimits;
        }
    }
    return std::nullopt;
}

std::optional<nlohmann::json> adapterBinaryPayloadLimits(const nlohmann::json& request)
{
    if (request.contains("binary_payload_limits") && request["binary_payload_limits"].is_object()) {
        return request["binary_payload_limits"];
    }
    const auto adapter = request.find("adapter");
    if (adapter != request.end() && adapter->is_object()) {
        const auto payloadLimits = adapter->find("binaryPayloadLimits");
        if (payloadLimits != adapter->end() && payloadLimits->is_object()) {
            return *payloadLimits;
        }
    }
    return std::nullopt;
}

void appendMeshLimitDiagnostic(
    nlohmann::json& result,
    const std::string& object,
    std::size_t vertices,
    std::size_t triangles,
    std::optional<std::size_t> maxVertices,
    std::optional<std::size_t> maxTriangles
)
{
    if (!result.contains("diagnostics") || !result["diagnostics"].is_array()) {
        result["diagnostics"] = nlohmann::json::array();
    }
    result["diagnostics"].push_back({
        {"severity", "error"},
        {"code", "mesh_limit_exceeded"},
        {"message", "Mesh result exceeds adapter streaming limits"},
        {"object", object},
        {"property", "mesh"},
        {"stage", "adapter"},
        {"target", "streaming_mesh_limits"},
        {"mesh", {{"vertices", vertices}, {"triangles", triangles}}},
        {"limits",
         {
             {"max_vertices", maxVertices ? nlohmann::json(*maxVertices) : nlohmann::json(nullptr)},
             {"max_triangles", maxTriangles ? nlohmann::json(*maxTriangles) : nlohmann::json(nullptr)},
         }},
    });
}

void applyStreamingMeshLimits(nlohmann::json& result, const nlohmann::json& request)
{
    const auto limits = adapterMeshLimits(request);
    if (!limits) {
        return;
    }
    std::optional<std::size_t> maxVertices;
    std::optional<std::size_t> maxTriangles;
    std::size_t chunkTriangles = 0U;
    try {
        maxVertices = readSizeLimit(*limits, "max_vertices", "maxVertices");
        maxTriangles = readSizeLimit(*limits, "max_triangles", "maxTriangles");
        chunkTriangles = readSizeLimit(*limits, "chunk_triangles", "chunkTriangles").value_or(0U);
    }
    catch (const std::exception& error) {
        appendAdapterResourceLimitDiagnostic(
            result,
            error.what(),
            "",
            "mesh_limits",
            "mesh_limits",
            {{"contract", "cad-core-result-v1"}}
        );
        return;
    }
    if (!maxVertices && !maxTriangles) {
        return;
    }
    if (!result.contains("results") || !result["results"].is_array()) {
        return;
    }

    for (auto& item : result["results"]) {
        if (!item.is_object() || !item.contains("mesh") || item["mesh"].is_null()) {
            continue;
        }
        nlohmann::json& mesh = item["mesh"];
        const std::size_t vertices = mesh.contains("vertices") && mesh["vertices"].is_array()
            ? mesh["vertices"].size()
            : 0U;
        const std::size_t triangles = mesh.contains("indices") && mesh["indices"].is_array()
            ? mesh["indices"].size() / 3U
            : (mesh.contains("triangles") && mesh["triangles"].is_array() ? mesh["triangles"].size()
                                                                          : 0U);
        const bool exceedsVertices = maxVertices && vertices > *maxVertices;
        const bool exceedsTriangles = maxTriangles && triangles > *maxTriangles;
        if (!exceedsVertices && !exceedsTriangles) {
            if (chunkTriangles > 0U) {
                mesh["streaming"] = {
                    {"chunk_triangles", chunkTriangles},
                    {"chunk_count",
                     triangles == 0U ? 0U : (triangles + chunkTriangles - 1U) / chunkTriangles},
                    {"partial", false},
                };
            }
            continue;
        }
        const std::string object = item.value("object", "");
        appendMeshLimitDiagnostic(result, object, vertices, triangles, maxVertices, maxTriangles);
        mesh = {
            {"limited", true},
            {"streaming",
             {
                 {"protocol", "cad-core-json-mesh-stream-v1"},
                 {"max_vertices", maxVertices ? nlohmann::json(*maxVertices) : nlohmann::json(nullptr)},
                 {"max_triangles",
                  maxTriangles ? nlohmann::json(*maxTriangles) : nlohmann::json(nullptr)},
                 {"chunk_triangles", chunkTriangles},
                 {"original_vertex_count", vertices},
                 {"original_triangle_count", triangles},
                 {"partial", true},
             }},
        };
    }
}

CadCoreResult recomputeJsonEntrypoint(
    const char* request_json,
    size_t request_json_len,
    std::string_view adapterName
)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json raw = nlohmann::json::parse(payload);
        if (cad_core::part::isPartGeometryCurveRequest(raw)) {
            cad_core::runtime::ComputeContext context
                = cad_core::part::computePartGeometryCurveRequest(raw);
            nlohmann::json result = cad_core::part::partGeometryCurveResultJson(context);
            if (!adapterName.empty()) {
                result["adapter"] = adapterName;
            }
            applyStreamingMeshLimits(result, raw);
            return makeJsonResult(result);
        }
        auto [document, diagnostics] = cad_core::app::parseDocument(raw);
        nlohmann::json result = cad_core::runtime::recompute(document, std::move(diagnostics));
        if (!adapterName.empty()) {
            result["adapter"] = adapterName;
        }
        applyStreamingMeshLimits(result, raw);
        return makeJsonResult(result);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

template<typename T>
void appendPod(std::string& data, const T& value)
{
    const char* bytes = reinterpret_cast<const char*>(&value);
    data.append(bytes, sizeof(T));
}

CadCoreExportResult meshBinaryEntrypoint(const char* request_json, size_t request_json_len)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeExportErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json request = nlohmann::json::parse(payload);
        if (!request.is_object()) {
            return makeExportErrorResult(1, "binary mesh request root must be a JSON object");
        }
        if (!request.contains("document") || !request["document"].is_object()) {
            return makeExportErrorResult(1, "binary mesh request field 'document' must be a JSON object");
        }
        if (!request.contains("object") || !request["object"].is_string()
            || request["object"].get<std::string>().empty()) {
            return makeExportErrorResult(
                1,
                "binary mesh request field 'object' must be a non-empty string"
            );
        }

        const std::string objectName = request["object"].get<std::string>();
        auto [document, diagnostics] = cad_core::app::parseDocument(request["document"]);
        cad_core::runtime::ComputeContext context
            = cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        nlohmann::json metadata = {
            {"object", objectName},
            {"protocol", "cad-core-binary-mesh-v1"},
            {"content_type", "application/vnd.cad-core.mesh+bin"},
            {"layout",
             {
                 {"vertex_format", "f64x3_le"},
                 {"index_format", "u32x3_le"},
             }},
            {"diagnostics", cad_core::runtime::diagnosticsToJson(context.diagnostics)},
        };

        std::optional<std::size_t> maxBinaryBytes;
        if (const auto payloadLimits = adapterBinaryPayloadLimits(request)) {
            try {
                maxBinaryBytes = readSizeLimit(*payloadLimits, "max_bytes", "maxBytes");
            }
            catch (const std::exception& error) {
                metadata["bytes"] = 0;
                metadata["limited"] = true;
                appendAdapterResourceLimitDiagnostic(
                    metadata,
                    error.what(),
                    objectName,
                    "binaryPayloads",
                    "binary_payload_limits",
                    {{"protocol", "cad-core-binary-mesh-v1"}}
                );
                return makeExportResult({}, metadata);
            }
        }

        const auto meshIt = context.mesh.find(objectName);
        if (meshIt == context.mesh.end() || meshIt->second.is_null()) {
            metadata["bytes"] = 0;
            metadata["vertex_count"] = 0;
            metadata["triangle_count"] = 0;
            return makeExportResult({}, metadata);
        }

        const nlohmann::json& mesh = meshIt->second;
        std::string data;
        const std::size_t vertexOffset = 0U;
        for (const auto& vertex : mesh.at("vertices")) {
            for (std::size_t index = 0; index < 3U; ++index) {
                appendPod(data, vertex.at(index).get<double>());
            }
        }
        const std::size_t indexOffset = data.size();
        for (const auto& triangle : mesh.at("triangles")) {
            for (std::size_t index = 0; index < 3U; ++index) {
                const std::uint32_t value = triangle.at(index).get<std::uint32_t>();
                appendPod(data, value);
            }
        }

        metadata["bytes"] = data.size();
        metadata["vertex_count"] = mesh.at("vertices").size();
        metadata["triangle_count"] = mesh.at("triangles").size();
        metadata["vertex_offset"] = vertexOffset;
        metadata["index_offset"] = indexOffset;
        if (maxBinaryBytes && data.size() > *maxBinaryBytes) {
            metadata["bytes"] = 0;
            metadata["limited"] = true;
            metadata["original_bytes"] = data.size();
            metadata["byte_limit"] = *maxBinaryBytes;
            appendAdapterResourceLimitDiagnostic(
                metadata,
                "Binary mesh payload exceeds adapter byte limit",
                objectName,
                "binaryPayloads",
                "binary_payload_limits.max_bytes",
                {
                    {"protocol", "cad-core-binary-mesh-v1"},
                    {"actual_bytes", data.size()},
                    {"max_bytes", *maxBinaryBytes},
                }
            );
            return makeExportResult({}, metadata);
        }
        metadata["limited"] = false;
        return makeExportResult(data, metadata);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeExportErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeExportErrorResult(2, error.what());
    }
    catch (...) {
        return makeExportErrorResult(2, "unknown C++ exception");
    }
}

}  // namespace

CadCoreResult cad_core_version_json(void)
{
    try {
        return makeJsonResult(cadCoreVersionJson());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

CadCoreResult cad_core_capabilities_json(void)
{
    try {
        return makeJsonResult(capabilitiesJson());
    }
    catch (const std::exception& error) {
        return makeErrorResult(2, error.what());
    }
    catch (...) {
        return makeErrorResult(2, "unknown C++ exception");
    }
}

CadCoreResult cad_core_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, {});
}

CadCoreResult cad_core_worker_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, "worker");
}

CadCoreResult cad_core_wasm_recompute_json(const char* request_json, size_t request_json_len)
{
    return recomputeJsonEntrypoint(request_json, request_json_len, "wasm");
}

CadCoreExportResult cad_core_export_json(const char* request_json, size_t request_json_len)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeExportErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json request = nlohmann::json::parse(payload);
        if (!request.is_object()) {
            return makeExportErrorResult(1, "export request root must be a JSON object");
        }
        if (request.contains("export_file") || request.contains("path") || request.contains("file")) {
            return makeExportErrorResult(1, "export request must not contain a server file path");
        }
        if (!request.contains("document") || !request["document"].is_object()) {
            return makeExportErrorResult(1, "export request field 'document' must be a JSON object");
        }
        if (!request.contains("object") || !request["object"].is_string()
            || request["object"].get<std::string>().empty()) {
            return makeExportErrorResult(1, "export request field 'object' must be a non-empty string");
        }
        if (!request.contains("format") || !request["format"].is_string()) {
            return makeExportErrorResult(1, "export request field 'format' must be a string");
        }

        const std::string objectName = request["object"].get<std::string>();
        cad_core::part::ShapeFileFormat format;
        try {
            format = cad_core::part::shapeFileFormatFromString(request["format"].get<std::string>());
        }
        catch (const std::exception& error) {
            return makeExportErrorResult(1, error.what());
        }

        double stlDeflection = 0.01;
        try {
            stlDeflection = readOptionalStlDeflection(request).value_or(0.01);
        }
        catch (const std::exception& error) {
            return makeExportErrorResult(1, error.what());
        }

        auto [document, diagnostics] = cad_core::app::parseDocument(request["document"]);
        cad_core::runtime::ComputeContext context
            = cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        const auto shapeIt = context.shapes.find(objectName);
        if (document.indexByName.count(objectName) == 0U) {
            cad_core::runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "missing_object",
                "Export object does not exist: " + objectName,
                objectName,
                "object",
                "export",
                objectName
            );
        }
        else if (shapeIt == context.shapes.end()) {
            cad_core::runtime::addDiagnostic(
                context.diagnostics,
                "error",
                "execution_failed",
                "Export object has no computed shape: " + objectName,
                objectName,
                "object",
                "export",
                objectName
            );
        }

        nlohmann::json metadata = {
            {"object", objectName},
            {"format", cad_core::part::shapeFileFormatName(format)},
            {"content_type", cad_core::part::shapeFileFormatContentType(format)},
            {"filename", objectName + "." + cad_core::part::shapeFileFormatExtension(format)},
            {"diagnostics", cad_core::runtime::diagnosticsToJson(context.diagnostics)},
        };

        if (shapeIt == context.shapes.end()) {
            metadata["bytes"] = 0;
            return makeExportResult({}, metadata);
        }

        const std::string data
            = cad_core::part::exportShapeBuffer(shapeIt->second.shape, format, stlDeflection);
        metadata["bytes"] = data.size();
        return makeExportResult(data, metadata);
    }
    catch (const nlohmann::json::parse_error& error) {
        return makeExportErrorResult(1, error.what());
    }
    catch (const std::exception& error) {
        return makeExportErrorResult(2, error.what());
    }
    catch (...) {
        return makeExportErrorResult(2, "unknown C++ exception");
    }
}

CadCoreExportResult cad_core_mesh_binary_json(const char* request_json, size_t request_json_len)
{
    return meshBinaryEntrypoint(request_json, request_json_len);
}

void cad_core_free_result(CadCoreResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->json = CadCoreBuffer {nullptr, 0};
    result->error = CadCoreBuffer {nullptr, 0};
}

void cad_core_free_export_result(CadCoreExportResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->data.ptr;
    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->data = CadCoreBuffer {nullptr, 0};
    result->json = CadCoreBuffer {nullptr, 0};
    result->error = CadCoreBuffer {nullptr, 0};
}
