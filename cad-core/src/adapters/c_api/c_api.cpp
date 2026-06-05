#include "cad_core/c_api.h"

#include "cad_core/app/document.h"
#include "cad_core/part/shape_exporter.h"
#include "cad_core/runtime/diagnostics.h"
#include "cad_core/runtime/feature_registry.h"
#include "cad_core/runtime/recompute.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

CadCoreBuffer emptyBuffer()
{
    return CadCoreBuffer{nullptr, 0};
}

bool copyBuffer(std::string_view value, CadCoreBuffer& buffer) noexcept
{
    buffer = CadCoreBuffer{nullptr, value.size()};
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
    return CadCoreResult{status, emptyBuffer(), error};
}

CadCoreResult makeJsonResult(const nlohmann::json& payload)
{
    CadCoreBuffer json = emptyBuffer();
    if (!copyBuffer(payload.dump(), json)) {
        return makeErrorResult(2, "cad-core FFI failed to allocate result buffer");
    }
    return CadCoreResult{0, json, emptyBuffer()};
}

CadCoreExportResult makeExportErrorResult(int32_t status, std::string_view message) noexcept
{
    CadCoreBuffer error = emptyBuffer();
    copyBuffer(message, error);
    return CadCoreExportResult{status, emptyBuffer(), emptyBuffer(), error};
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

    return CadCoreExportResult{0, dataBuffer, jsonBuffer, emptyBuffer()};
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
        {"value", "SubList", "StableSubList", "FullSubList", "ShadowSub", "ReferenceShadow", "ExternalFlags", "Document"});
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
               "element_count_owner_lists_sync",
               "element_list_owner_sync",
               "element_list_child_sync",
               "copy_on_change_owned_child_sync"}},
             {"writeback_properties",
              {"ElementList",
               "ElementCount",
               "PlacementList",
               "ScaleList",
               "VisibilityList",
               "LinkedObject",
               "_LinkOwner",
               "LinkTransform"}},
             {"request_local_boundaries",
              {"plain_group_child_expansion_without_persistent_child_cache",
               "show_element_missing_children_are_create_updates"}},
             {"remaining_gaps",
              {"full_child_cache_lifecycle", "copy_on_change_deep_copy_lifecycle"}},
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
                  // "Sketch with redundant constraints", and "Sketch with malformed constraints".
                  {"status", "done_first_slice"},
                  {"diagnostics",
                   {"sketch_solver_conflict",
                    "sketch_solver_malformed_constraint",
                    "sketch_solver_redundant"}},
                  {"covered",
                   {"horizontal_vertical_same_target_conflict",
                    "malformed_constraint_diagnostics",
                    "duplicate_orientation_constraint_redundant",
                    "conflicting_same_target_datums",
                    "duplicate_same_target_datums"}},
                  {"request_local_boundaries",
                   {"diagnostics_only_without_backend_solver_session",
                    "conflict_or_redundant_blocks_profile_output",
                    "malformed_blocks_profile_output"}},
                  {"remaining_gaps",
                   {"full_solver_dof",
                    "underconstrained_state",
                    "solver_geometry_update",
                    "partial_redundancy_diagnostics"}},
              }},
         }},
        {"part_workbench",
         {
             {"offset",
              {
                  // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp
                  // ::Offset::execute(), reads "Source", "Value", "Mode", "Join",
                  // "Intersection", "SelfIntersection" and "Fill" before calling
                  // TopoShape::makeElementOffset().
                  {"status", "done_first_slice"},
                  {"type_ids", {"Part::Offset"}},
                  {"properties",
                   {"Source", "Value", "Mode", "Join", "Intersection", "SelfIntersection", "Fill"}},
                  {"covered", {"face_source_offset", "maker_history_generated_modified"}},
                  {"request_local_boundaries", {"source_shape_recomputed_from_document_graph"}},
                  {"remaining_gaps", {"fill_offset", "solid_source_make_element_solid", "offset2d", "thickness"}},
              }},
         }},
        {"part_design",
         {
             {"body_chain",
              {
                  // FreeCAD:
                  // /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
                  // ::Body::onChanged(), "createObject(\"PartDesign::FeatureBase\",\"BaseFeature\")"
                  // and "bf->BaseFeature.setValue(BaseFeature.getValue())"; ::Body::setBaseProperty()
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
                   {"explicit_body_origin_link", "origin_global_placement_from_body", "origin_feature_role_relink"}},
                  {"addsub_replay", {"group_order_replay", "additive_fuse", "subtractive_cut", "stop_at_tip"}},
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
                   {"ISOMetricProfile", "ISOMetricFineProfile", "UNC", "UNF", "UNEF", "NPT", "BSP", "BSW", "BSF", "ISOTyre"}},
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
                       {"status", "history_partial"},
                       {"covered", {"find_holes_make_shape_with_element_map", "subtractive_body_cut_history"}},
                   }},
                  {"native_oracle_fixtures",
                   {"p7/hole-supported-threaded-dynamic-iso2009",
                    "p7/hole-supported-threaded-dynamic-din7984",
                    "p7/hole-supported-model-thread-metric"}},
                  {"remaining_gaps", {"hole_cut_history_full_element_map_freeze"}},
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
               "cli_recompute"}},
             {"stateless_result_channels",
              {"results", "elementReferenceUpdates", "documentObjectUpdates", "diagnostics"}},
             {"c_api_export",
              {"buffer_only", "rejects_server_file_paths", "metadata_diagnostics", "stl_deflection"}},
             {"cli_export",
              {"file_protocol", "requires_object_format_file", "stl_deflection"}},
             {"remaining_gaps",
              {"worker_adapter", "wasm_adapter", "streaming_mesh_limits", "binary_mesh_protocol"}},
         }},
        {"assembly",
         {
             // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
             // ::AssemblyObject::solve(), uses "fixGroundedParts()" before "mbdAssembly->runPreDrag()";
             // cad-core exposes the stateless adapter states while full Ondsel solving remains a gap.
             {"solver_adapter",
              {"skipped_no_joints", "grounded_only_noop", "unsupported_joint_diagnostics"}},
             {"remaining_gaps", {"full_ondsel_solver", "solver_placement_updates"}},
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
               // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute(),
               // reads "Angle", "NeutralPlane", "PullDirection" and selected FaceN before
               // calling TopoShape::makeElementDraft().
               "dressup_draft_parameter_variants",
               // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp
               // ::Thickness::execute(), reads "Value", "Mode", "Join", "Reversed" and
               // "Intersection" before calling TopoShape::makeElementThickSolid().
               "dressup_thickness_parameter_variants",
               "thickness_multi_solid_fuse_history",
               "chain_dressup_pattern_history",
               "transformed_copy_terminal",
               // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp::TopoShape::fix(),
               // calls "makeShapeWithElementMap(fixThis.Shape(), MapperHistory(fixThis), {*this})"
               // because "ShapeFix_Shape may delete ... or modify the input shape".
               // This C3-M1 slice covers ShapeFix small-edge deleted history and
               // ShapeFix_Root::Context()->History() modified history. FreeCAD test
               // tests/src/Mod/Part/App/WireJoiner.cpp::Generated says "no methods in
               // ShapeFix_Wire call AddGenerated()", so ShapeFix generated is recorded as an
               // audited non-producer rather than a fixture to synthesize.
               "shapefix_deleted_small_edge",
               "shapefix_root_modified_history",
               // FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute(),
               // Features mode calls "feature->getAddSubShape(fuseShape, cutShape)" and transforms
               // each original with "makeElementTransform", while WholeShape transforms the support.
               "transformed_pattern_addsub_ownership",
               "transformed_pattern_full_history",
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
                  {"body_boolean", {{"status", "covered"}, {"covered", {"maker_history"}}}},
                  {"part_boolean", {{"status", "covered"}, {"covered", {"maker_history"}}}},
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
                   {{"status", "done_first_slice"},
                    {"covered", {"face_source_offset", "maker_history"}},
                    {"remaining", {"fill_offset", "solid_source_make_element_solid", "offset2d", "thickness"}}}},
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
                      "mixed_bounded_faces_open_wires_oracle"}},
                    {"remaining", nlohmann::json::array()}}},
                  {"taper_thru_sections",
                   {{"status", "covered"}, {"covered", {"generated_history"}}}},
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
               "element_map_policy_drop"}},
             {"remaining_gaps", {"complete_mapper_history"}},
         }},
        {"known_gaps",
         {
             "complete_mapper_history",
             "assembly_joint_solver",
         }},
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
    if (request_json == nullptr || request_json_len == 0U) {
        return makeErrorResult(1, "request_json must be a non-empty UTF-8 JSON buffer");
    }

    try {
        const std::string payload(request_json, request_json_len);
        const nlohmann::json raw = nlohmann::json::parse(payload);
        auto [document, diagnostics] = cad_core::app::parseDocument(raw);
        return makeJsonResult(cad_core::runtime::recompute(document, std::move(diagnostics)));
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
        cad_core::runtime::ComputeContext context =
            cad_core::runtime::recomputeContext(document, std::move(diagnostics));

        const auto shapeIt = context.shapes.find(objectName);
        if (document.indexByName.count(objectName) == 0U) {
            cad_core::runtime::addDiagnostic(context.diagnostics,
                                             "error",
                                             "missing_object",
                                             "Export object does not exist: " + objectName,
                                             objectName,
                                             "object",
                                             "export",
                                             objectName);
        }
        else if (shapeIt == context.shapes.end()) {
            cad_core::runtime::addDiagnostic(context.diagnostics,
                                             "error",
                                             "execution_failed",
                                             "Export object has no computed shape: " + objectName,
                                             objectName,
                                             "object",
                                             "export",
                                             objectName);
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

        const std::string data =
            cad_core::part::exportShapeBuffer(shapeIt->second.shape, format, stlDeflection);
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

void cad_core_free_result(CadCoreResult* result)
{
    if (result == nullptr) {
        return;
    }

    delete[] result->json.ptr;
    delete[] result->error.ptr;
    result->status = 0;
    result->json = CadCoreBuffer{nullptr, 0};
    result->error = CadCoreBuffer{nullptr, 0};
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
    result->data = CadCoreBuffer{nullptr, 0};
    result->json = CadCoreBuffer{nullptr, 0};
    result->error = CadCoreBuffer{nullptr, 0};
}
