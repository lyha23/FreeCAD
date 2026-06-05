#include "cad_core/adapters/c_api.h"

#include "cad_core/app/document.h"
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
        "invalid_direction",
        "invalid_length",
        "invalid_link_value",
        "invalid_placement",
        "invalid_property_type",
        "invalid_subshape",
        "invalid_taper",
        "label_reference_ambiguous",
        "missing_external_document",
        "missing_external_geometry_snapshot",
        "missing_link_target",
        "missing_object",
        "missing_property",
        "missing_target",
        "mesh_limit_exceeded",
        "open_profile",
        "parse_error",
        "refine_failed",
        "sketch_solver_conflict",
        "sketch_solver_malformed_constraint",
        "sketch_solver_redundant",
        "split_stable_subname",
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
        "unsupported_stable_subname",
        "unsupported_subshape_kind",
        "unsupported_type",
    });
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
              {"file", "name", "label", "stamp", "status", "currentName", "currentLabel", "currentStamp", "currentStatus", "allowPartial"
              }},
             {"external_geometry_native_slot_fields",
              {"ExternalGeo", "Geometry", "Values", "Items", "Ref", "RefIndex", "ExternalFlags", "Flags"
              }},
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
                  {"updates",
                   {"copy_on_change_group_sync",
                    "copy_on_change_deep_copy_lifecycle"}},
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
                  {"status", "done_c3m3"},
                  {"diagnostics",
                   {"sketch_solver_conflict",
                    "sketch_solver_malformed_constraint",
                    "sketch_solver_partially_redundant",
                    "sketch_solver_redundant"}},
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
                    "dependent_parameter_group_analysis"}},
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
                    "symmetric_center_point_updates_second_point_without_full_solver_session"}},
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
                   {"Source", "Faces", "Value", "Mode", "Join", "Intersection", "SelfIntersection", "Fill"
                   }},
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
         }},
        {"part_design",
         {
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
                   {"Body.Group", "Body.Tip", "Body.Origin", "FeatureBase.BaseFeature", "Feature.BaseFeature"
                   }},
                  {"origin_lifecycle",
                   {"explicit_body_origin_link", "origin_global_placement_from_body", "origin_feature_role_relink"
                   }},
                  {"addsub_replay",
                   {"group_order_replay", "additive_fuse", "subtractive_cut", "stop_at_tip"}},
                  {"request_local_boundaries",
                   {"body_basefeature_writeback_keeps_request_graph_immutable",
                    "body_delete_reroute_uses_request_local_stale_tip_evidence",
                    "body_origin_parent_placement_without_backend_session"}},
                  {"remaining_gaps", nlohmann::json::array()},
              }},
             {"hole",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp
                  // ::Hole::threadDescription[] stores thread rows, ::Hole::readCutDefinitions()
                  // loads "Resources/Hole", ::Hole::makeThread() builds the model thread, and
                  // ::Hole::findHoles() calls "makeShapeWithElementMap(protoHole, mapper, {baseshape})".
                  {"thread_tables",
                   {"ISOMetricProfile", "ISOMetricFineProfile", "UNC", "UNF", "UNEF", "NPT", "BSP", "BSW", "BSF", "ISOTyre"
                   }},
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
                        {"ModelThread", "ThreadClass", "ThreadDirection", "UseCustomThreadClearance", "CustomThreadClearance"
                        }},
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
             {"core_entrypoints",
              {"cad_core_recompute_json",
               "cad_core_export_json",
               "cad_core_capabilities_json",
               "cli_recompute",
               "worker_recompute",
               "wasm_recompute"
              }},
             {"stateless_result_channels",
              {"results", "elementReferenceUpdates", "documentObjectUpdates", "diagnostics", "binaryPayloads"}},
             {"c_api_export",
              {"buffer_only", "rejects_server_file_paths", "metadata_diagnostics", "stl_deflection"}},
             {"cli_export", {"file_protocol", "requires_object_format_file", "stl_deflection"}},
             {"worker_adapter",
              {
                  {"entrypoint", "cad_core_worker_recompute_json"},
                  {"core_recompute", true},
                  {"state", "stateless_request_local"},
              }},
             {"wasm_adapter",
              {
                  {"entrypoint", "cad_core_wasm_recompute_json"},
                  {"core_recompute", true},
                  {"toolchain_contract", "source_and_schema_delivered"},
              }},
             {"mesh",
              {
                  {"streaming_limits",
                   {"max_vertices", "max_triangles", "chunk_triangles", "mesh_limit_exceeded"}},
                  {"binary_payloads",
                   {"cad_core_mesh_binary_json",
                    "cad-core-binary-mesh-v1",
                    "f64x3_vertices",
                    "u32x3_triangles"}},
              }},
             {"remaining_gaps", nlohmann::json::array()},
         }},
        {"assembly",
         {
             // FreeCAD:
             // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
             // ::AssemblyObject::solve(), uses "fixGroundedParts()" before
             // "mbdAssembly->runPreDrag()"; ::setNewPlacements() writes
             // "propPlacement->setValue(newPlacement)" after solving.
             {"representative_solver_adapter",
              {
                  {"status", "covered_representative"},
                  {"mode", "stateless_representative_solver"},
                  {"covered",
                   {"skipped_no_joints",
                    "grounded_only_noop",
                    "fixed_joint",
                    "revolute_joint",
                    "slider_joint",
                    "ball_joint",
                    "distance_joint",
                    "angle_joint",
                    "unsupported_joint_diagnostics"}},
                  {"request_local_boundaries", {"no_ondsel_solver_session", "no_joint_constraint_solve"}},
              }},
             {"ondsel_solver_adapter",
              {
                  {"status", "not_implemented"},
                  {"remaining_gaps", {"ondsel_solver_session", "joint_constraint_solve"}},
              }},
             {"placement_writeback",
              {
                  {"status", "covered_contract"},
                  {"updates", {"documentObjectUpdates.action=assembly_set_placement"}},
                  {"remaining_gaps", {"validated_new_placements"}},
              }},
             {"unsupported_joint_matrix", {"RackPinion", "Screw", "Gears", "Belt", "Cylindrical"}},
             {"remaining_gaps", {"ondsel_solver_session", "joint_constraint_solve", "validated_new_placements"}},
         }},
        {"wire_joiner",
         {
             {"generated_open_export_bridge",
              {
                  {"status", "bridge"},
                  {"helper_fields", {"helperOpenExportOverride", "openExportOverride"}},
                  {"delete_when",
                   {"aHistory generated/removed source map drives generated open exports",
                    "myShapesToReturn result-wire identity replaces helper result slots",
                    "openWireCompound child-wire ownership is consumed directly",
                    "WireInfo/EdgeInfo lifecycle owns final export decisions"}},
                  {"bridge_reasons",
                   {
                       {"partial_shared_closed_wire",
                        "delete after WireInfo/wireInfo2 exhaust lifecycle and myShapesToReturn identify the surviving child wire without helperOpenExportOverride"},
                       {"closed_wire_cycle",
                        "delete after aHistory plus openWireCompound child-wire ownership produce the closed-cycle result edge identity without openExportOverride"},
                   }},
              }},
             {"purge_as_original_bridge",
              {
                  {"status", "bridge"},
                  {"helper_fields", {"purgeAsOriginalOpenEdge"}},
                  {"delete_when",
                   {"sourceEdgeArray to split EdgeInfo identity is complete",
                    "openWireCompound child-wire ownership handles getOpenWires(noOriginal)",
                    "aHistory/MapperHistory evidence replaces the request-local purge flag"}},
              }},
             {"bridge_rules", {"no_new_helper_reason", "diagnostic_or_bridge_only", "no_fixture_shaped_pruning"}},
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
              {"indexed", "source_preserved", "one_to_one_history", "unique_same_kind_split_recovery", "reference_shadow_recovery"
              }},
             {"maker_history",
              {"prism",
               "body_boolean",
               "part_boolean",
               "section",
               "part_offset",
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
                     {"approximation_property", "auto_fuzzy_value", "source_qualified_edge_history", "terminal_deleted_history"
                     }},
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
                     {"facemaker", "wire_joiner_producer_evidence", "mixed_bounded_faces_open_wires_oracle"
                     }},
                    {"remaining", nlohmann::json::array()}}},
                  {"taper_thru_sections", {{"status", "covered"}, {"covered", {"generated_history"}}}},
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
        {"object_metadata_gaps",
         {
             {"scope", "object result metadata local history gaps, not broad CADCore3 capability-board gaps"},
             {"local_history_gaps",
              {"part_design.pad.taper_history",
               "part_design.pocket.taper_history",
               "part.extrusion.taper_history"}},
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

std::optional<std::size_t> readSizeLimit(const nlohmann::json& limits,
                                         const std::string& snakeCase,
                                         const std::string& camelCase)
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

void appendMeshLimitDiagnostic(nlohmann::json& result,
                               const std::string& object,
                               std::size_t vertices,
                               std::size_t triangles,
                               std::optional<std::size_t> maxVertices,
                               std::optional<std::size_t> maxTriangles)
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
    const auto maxVertices = readSizeLimit(*limits, "max_vertices", "maxVertices");
    const auto maxTriangles = readSizeLimit(*limits, "max_triangles", "maxTriangles");
    const auto chunkTriangles = readSizeLimit(*limits, "chunk_triangles", "chunkTriangles").value_or(0U);
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
            : (mesh.contains("triangles") && mesh["triangles"].is_array() ? mesh["triangles"].size() : 0U);
        const bool exceedsVertices = maxVertices && vertices > *maxVertices;
        const bool exceedsTriangles = maxTriangles && triangles > *maxTriangles;
        if (!exceedsVertices && !exceedsTriangles) {
            if (chunkTriangles > 0U) {
                mesh["streaming"] = {
                    {"chunk_triangles", chunkTriangles},
                    {"chunk_count", triangles == 0U ? 0U : (triangles + chunkTriangles - 1U) / chunkTriangles},
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
                 {"max_triangles", maxTriangles ? nlohmann::json(*maxTriangles) : nlohmann::json(nullptr)},
                 {"chunk_triangles", chunkTriangles},
                 {"original_vertex_count", vertices},
                 {"original_triangle_count", triangles},
                 {"partial", true},
             }},
        };
    }
}

CadCoreResult recomputeJsonEntrypoint(const char* request_json,
                                      size_t request_json_len,
                                      std::string_view adapterName)
{
    if (request_json == nullptr || request_json_len == 0U) {
        return makeErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json raw = nlohmann::json::parse(payload);
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

template <typename T>
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
            return makeExportErrorResult(1, "binary mesh request field 'object' must be a non-empty string");
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
