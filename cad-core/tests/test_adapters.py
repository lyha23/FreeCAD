from __future__ import annotations

import json
import tempfile
from pathlib import Path

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import ROOT, CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import ROOT, CadCoreFixtureTestCase


class CadCoreAdapterTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_ffi_mesh_matches_expected_summary(self, result: dict, group: str, fixture: str) -> None:
        expected = self.expected_freecad(group, fixture)
        summary = expected["mesh_summary"]
        mesh = result["results"][0]["mesh"]
        vertices = mesh["vertices"]
        actual_bbox = {
            "min": [min(vertex[index] for vertex in vertices) for index in range(3)],
            "max": [max(vertex[index] for vertex in vertices) for index in range(3)],
        }

        self.assert_bbox_close_delta(
            actual_bbox,
            summary["bbox"]["min"],
            summary["bbox"]["max"],
            summary.get("bbox_delta", expected.get("bbox_delta", 1e-6)),
        )
        self.assertEqual(len(vertices), summary["vertex_count"])
        self.assertEqual(len(mesh["indices"]) // 3, summary["triangle_count"])

    def test_c_api_returns_sketch_internal_profile_mesh(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["object"], "Sketch")
        self.assertIsNotNone(sketch["mesh"])
        self.assertGreater(len(sketch["mesh"]["vertices"]), 0)
        self.assertGreater(len(sketch["mesh"]["indices"]), 0)
        self.assertIn("Sketch:InternalFace1", sketch["mesh"]["faceIds"])
        self.assertTrue(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))

    def test_c_api_keeps_open_sketch_internal_profile_mesh_null(self) -> None:
        result = self.run_recompute_ffi("sketch-open-wire-internal-empty", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["object"], "Sketch")
        self.assertIsNone(sketch["mesh"])
        self.assertFalse(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:Edge1" for item in sketch["subshapes"]))

    def test_c_api_applies_sketch_plane_frame_to_internal_profile_mesh(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-internal-face", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIsNotNone(sketch["mesh"])
        self.assert_ffi_mesh_matches_expected_summary(result, "p5", "sketch-plane-frame-internal-face")

    def test_c_api_composes_sketch_plane_frame_with_local_placement(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-placement", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIsNotNone(sketch["mesh"])
        self.assert_ffi_mesh_matches_expected_summary(result, "p5", "sketch-plane-frame-placement")

    def test_c_api_rejects_invalid_sketch_plane_frame(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-invalid", "p5")
        sketch = result["results"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_property_type"])
        self.assertIsNone(sketch["mesh"])

    def test_c_api_returns_split_internal_face_mesh_ids(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face-split-line", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("Sketch:InternalFace1", sketch["mesh"]["faceIds"])
        self.assertIn("Sketch:InternalFace2", sketch["mesh"]["faceIds"])
        self.assertTrue(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:InternalFace2" for item in sketch["subshapes"]))
        internal_subshapes = [
            item for item in sketch["subshapes"]
            if item["indexed"].startswith("Internal")
        ]
        self.assertGreater(len(internal_subshapes), 0)
        for item in internal_subshapes:
            if item["indexed"].startswith("InternalFace"):
                self.assertEqual(item["stableSubname"], "")
            elif item["stableSubname"]:
                self.assertRegex(item["stableSubname"], r"^(Edge|Vertex)\d+$")
        self.assertEqual(
            next(item for item in sketch["subshapes"] if item["indexed"] == "Edge1")["stableSubname"],
            "Edge1",
        )

    def test_c_api_matches_cli_for_p3b_recompute(self) -> None:
        ffi_result = self.run_recompute_ffi("pocket-custom-vector", "p3b")

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(ffi_result["elementReferenceUpdates"], [])
        self.assertNotIn("objects", ffi_result)
        self.assertNotIn("mesh", ffi_result)
        self.assertEqual([item["object"] for item in ffi_result["results"]], ["Body"])
        self.assertGreater(len(ffi_result["results"][0]["mesh"]["indices"]), 0)
        self.assertTrue(
            any(item["indexed"] == "Face1" for item in ffi_result["results"][0]["subshapes"])
        )

    def test_c_api_recompute_returns_reference_shadow_update(self) -> None:
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["PropertyType"], "App::PropertyLinkSub")
        self.assertEqual(updates[0]["value"], "Sketch")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])

        shadow = updates[0]["ReferenceShadow"][0]
        self.assertEqual(shadow["target"], "Sketch")
        self.assertEqual(shadow["property"], "InternalShape")
        self.assertEqual(shadow["indexed"], "Face1")
        self.assertEqual(shadow["subname"], "InternalFace1")
        self.assertEqual(shadow["fingerprint"]["shapeType"], "Face")
        self.assertAlmostEqual(shadow["fingerprint"]["area"], 25.0)
        self.assertEqual(shadow["fingerprint"]["centroid"], [2.5, 2.5, 0.0])

    def test_c_api_recompute_returns_recovered_reference_shadow_update(self) -> None:
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-recover-sublist", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["StableSubList"], ["g305:split1"])

        shadow = updates[0]["ReferenceShadow"][0]
        self.assertEqual(shadow["indexed"], "Face1")
        self.assertEqual(shadow["subname"], "InternalFace1")
        self.assertEqual(shadow["stableSubname"], "g305:split1")
        self.assertAlmostEqual(shadow["fingerprint"]["area"], 25.0)

    def test_c_api_recompute_preserves_full_sublist_on_reference_shadow_update(self) -> None:
        payload = json.loads((ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json").read_text())
        profile = payload["Objects"][1]["Properties"]["Profile"]
        profile["FullSubList"] = ["ExternalDoc#Sketch.InternalFace1"]

        ffi_result = self.run_recompute_ffi_payload(payload)
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["FullSubList"], ["ExternalDoc#Sketch.InternalFace1"])

    def test_c_api_recompute_returns_reference_shadow_update_for_link_sub_list(self) -> None:
        ffi_result = self.run_recompute_ffi("sketch-external-face-reference-shadow", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Sketch")
        self.assertEqual(updates[0]["property"], "ExternalGeometry")
        self.assertEqual(updates[0]["PropertyType"], "App::PropertyLinkSubList")
        self.assertEqual(len(updates[0]["SubSet"]), 1)

        item = updates[0]["SubSet"][0]
        self.assertEqual(item["value"], "Box")
        self.assertEqual(item["SubList"], ["Face5"])
        shadow = item["ReferenceShadow"][0]
        self.assertEqual(shadow["target"], "Box")
        self.assertEqual(shadow["property"], "Shape")
        self.assertEqual(shadow["indexed"], "Face5")
        self.assertEqual(shadow["subname"], "Face5")
        self.assertEqual(shadow["fingerprint"]["shapeType"], "Face")
        self.assertEqual(shadow["fingerprint"]["edgeCount"], 4)
        self.assertEqual(shadow["fingerprint"]["vertexCount"], 4)

    def test_c_api_recompute_preserves_full_sublist_on_link_sub_list_update(self) -> None:
        payload = json.loads((ROOT / "fixtures" / "p5" / "sketch-external-face-reference-shadow.json").read_text())
        external = payload["Objects"][1]["Properties"]["ExternalGeometry"]["SubSet"][0]
        external["FullSubList"] = ["ExternalDoc#Box.Face5"]

        ffi_result = self.run_recompute_ffi_payload(payload)
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        item = updates[0]["SubSet"][0]
        self.assertEqual(item["value"], "Box")
        self.assertEqual(item["SubList"], ["Face5"])
        self.assertEqual(item["FullSubList"], ["ExternalDoc#Box.Face5"])

    def test_c_api_capabilities_exposes_web_contract_facts(self) -> None:
        capabilities = self.run_capabilities_ffi()

        self.assertEqual(capabilities["status"], "ok")
        self.assertEqual(capabilities["schema_version"], "cad-web-v1")
        self.assertEqual(capabilities["cad_core"]["api"], "cad_core_ffi")
        self.assertIn("OCCT", capabilities["cad_core"]["kernel"])
        self.assertEqual(capabilities["document"]["source"], "DocumentObject graph")
        self.assertEqual(capabilities["export_formats"], ["brep", "step", "stl"])
        self.assertIn("value", capabilities["document"]["link_property_fields"])
        self.assertIn("values", capabilities["document"]["link_property_fields"])
        self.assertIn("SubList", capabilities["document"]["link_property_fields"])
        self.assertIn("StableSubList", capabilities["document"]["link_property_fields"])
        self.assertIn("FullSubList", capabilities["document"]["link_property_fields"])
        self.assertIn("ShadowSub", capabilities["document"]["link_property_fields"])
        self.assertIn("ReferenceShadow", capabilities["document"]["link_property_fields"])
        self.assertIn("ExternalFlags", capabilities["document"]["link_property_fields"])
        self.assertIn("Document", capabilities["document"]["link_property_fields"])
        self.assertIn("SubSet", capabilities["document"]["link_property_fields"])
        self.assertEqual(
            capabilities["document"]["document_reference_fields"],
            [
                "file",
                "name",
                "label",
                "stamp",
                "status",
                "currentName",
                "currentLabel",
                "currentStamp",
                "currentStatus",
                "allowPartial",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_native_slot_fields"],
            ["ExternalGeo", "Geometry", "Values", "Items", "Ref", "RefIndex", "ExternalFlags", "Flags"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_flags"],
            ["Defining", "Frozen", "Detached", "Missing", "Sync"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["state_updates"],
            [
                "sync_clears_sync_flag",
                "missing_clears_when_reference_resolves",
                "frozen_reuses_reference_shadow_brep",
                "missing_unresolved_reuses_reference_shadow_brep",
                "frozen_reuses_native_external_geo",
                "missing_unresolved_reuses_native_external_geo",
                "detached_keeps_native_external_geo_and_clears_ref",
                "detached_removes_external_geometry_link",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["request_local_boundaries"],
            [
                "frozen_current_source_without_reference_shadow_brep_skips_projection",
                "native_external_geo_pool_is_request_local_not_backend_session",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["diagnostics"],
            ["missing_external_geometry_snapshot"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["document"]["document_update_channels"],
            ["elementReferenceUpdates", "documentObjectUpdates"],
        )
        self.assertEqual(
            capabilities["link_transaction"]["document_object_updates"],
            [
                "show_element_create",
                "show_element_claim",
                "show_element_sync",
                "show_element_delete",
                "show_element_toggle_off",
                "element_count_owner_lists_sync",
                "element_list_owner_sync",
                "element_list_child_sync",
                "copy_on_change_owned_child_sync",
            ],
        )
        self.assertEqual(
            capabilities["link_transaction"]["writeback_properties"],
            [
                "ElementList",
                "ElementCount",
                "PlacementList",
                "ScaleList",
                "VisibilityList",
                "LinkedObject",
                "_LinkOwner",
                "LinkTransform",
            ],
        )
        self.assertIn(
            "plain_group_child_expansion_without_persistent_child_cache",
            capabilities["link_transaction"]["request_local_boundaries"],
        )
        self.assertIn(
            "full_child_cache_lifecycle",
            capabilities["link_transaction"]["remaining_gaps"],
        )
        self.assertIn(
            "copy_on_change_deep_copy_lifecycle",
            capabilities["link_transaction"]["remaining_gaps"],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["retag_aliases"],
            [
                "full_sublist_external_tag",
                "mapped_postfix_alias",
                "source_prefixed_stable_key",
                "label_qualified_subname",
                "multi_level_link_subname",
                "property_xlink_list_subset_compound",
            ],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["reference_update_fields"],
            [
                "FullSubList",
                "StableSubList",
                "ShadowSub",
                "ReferenceShadow",
                "ExternalFlags",
                "labelReferenceRename",
                "documentReference",
            ],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["reference_recovery"],
            [
                "source_object_rename_by_reference_shadow_target_id",
                "label_rename_by_link_target_label",
                "label_rename_nested_by_link_group_path",
                "label_rename_cross_document_nested_by_full_sublist",
                "document_name_label_restore",
                "missing_external_document_diagnostic",
                "external_document_pending_reload_diagnostic",
                "external_document_unloaded_diagnostic",
            ],
        )
        self.assertEqual(capabilities["link_reference_lifecycle"]["remaining_gaps"], [])
        self.assertNotIn("missing_external_document_lifecycle", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("source_object_rename_recovery", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("label_rename_recovery", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("label_rename_nested_lifecycle", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn(
            "label_rename_cross_document_nested_lifecycle",
            capabilities["link_reference_lifecycle"]["remaining_gaps"],
        )
        self.assertEqual(capabilities["sketcher"]["solver"]["status"], "done_eighteenth_slice")
        self.assertEqual(
            capabilities["sketcher"]["solver"]["diagnostics"],
            [
                "sketch_solver_conflict",
                "sketch_solver_malformed_constraint",
                "sketch_solver_partially_redundant",
                "sketch_solver_redundant",
            ],
        )
        self.assertIn(
            "horizontal_vertical_same_target_conflict",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "duplicate_orientation_constraint_redundant",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "malformed_constraint_diagnostics",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "partial_redundancy_diagnostics",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "unconstrained_geometry_underconstrained_state",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "whole_line_orientation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "endpoint_coordinate_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "circle_radius_diameter_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "line_length_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "arc_length_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "point_on_object_line_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "parallel_line_pair_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "perpendicular_line_pair_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "perpendicular_line_circle_arc_midpoint_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "equal_line_circle_arc_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "tangent_line_circle_arc_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_line_axis_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_arc_endpoint_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_center_point_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "solver_dof_driven_underconstrained_state",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "whole_line_orientation_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "endpoint_coordinate_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "circle_radius_diameter_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_length_update_preserves_start_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "arc_length_update_scales_radius_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "point_on_object_line_projection_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_pair_parallel_update_preserves_second_start_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_pair_perpendicular_update_preserves_second_start_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_circle_arc_perpendicular_midpoint_projection_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "equal_relation_updates_second_geometry_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "tangent_line_circle_arc_updates_round_center_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_line_axis_updates_second_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_arc_endpoint_updates_second_arc_angle_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_center_point_updates_second_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "request_local_dof_estimate_without_full_solver_rank",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "partial_redundancy_warning_without_full_dependent_parameter_group_analysis",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_arc_endpoint_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_center_point_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "tangent_equal_symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "tangent_symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "curve_and_coupled_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "perpendicular_and_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "line_pair_and_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertIn(
            "full_solver_dof",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "solver_dof_driven_underconstrained_state",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "arc_length_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "line_length_arc_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "length_radius_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "dimension_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn("solver_geometry_update", capabilities["sketcher"]["solver"]["remaining_gaps"])
        self.assertNotIn(
            "underconstrained_without_full_dof_count",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "diagnostics_only_without_backend_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "malformed_blocks_profile_output",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertNotIn("underconstrained_state", capabilities["sketcher"]["solver"]["remaining_gaps"])
        self.assertNotIn(
            "malformed_constraint_diagnostics",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "partial_redundancy_diagnostics",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["document_object_updates"],
            [
                "body_basefeature_featurebase_create",
                "body_basefeature_group_sync",
                "body_feature_basefeature_sync",
                "body_tip_deleted_feature_reroute",
                "body_feature_basefeature_delete_reroute",
                "body_origin_datum_relink",
            ],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["writeback_properties"],
            ["Body.Group", "Body.Tip", "Body.Origin", "FeatureBase.BaseFeature", "Feature.BaseFeature"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["origin_lifecycle"],
            ["explicit_body_origin_link", "origin_global_placement_from_body", "origin_feature_role_relink"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["addsub_replay"],
            ["group_order_replay", "additive_fuse", "subtractive_cut", "stop_at_tip"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["request_local_boundaries"],
            [
                "body_basefeature_writeback_keeps_request_graph_immutable",
                "body_delete_reroute_uses_request_local_stale_tip_evidence",
                "body_origin_parent_placement_without_backend_session",
            ],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["part_design"]["hole"]["thread_tables"],
            [
                "ISOMetricProfile",
                "ISOMetricFineProfile",
                "UNC",
                "UNF",
                "UNEF",
                "NPT",
                "BSP",
                "BSW",
                "BSF",
                "ISOTyre",
            ],
        )
        self.assertIn("thread_tap_drill", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertIn("thread_clearance", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertIn("thread_whitworth_fallback", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertEqual(
            capabilities["part_design"]["hole"]["head_cut_types"],
            ["None", "Counterbore", "Countersink", "Counterdrill"],
        )
        self.assertIn("iso2009.json", capabilities["part_design"]["hole"]["head_cut_definition_files"])
        self.assertIn("din7984.json", capabilities["part_design"]["hole"]["head_cut_definition_files"])
        self.assertEqual(capabilities["part_design"]["hole"]["model_thread"]["status"], "done_first_slice")
        self.assertEqual(capabilities["part_design"]["hole"]["model_thread"]["geometry"], "pipe_shell")
        self.assertIn("ThreadClass", capabilities["part_design"]["hole"]["model_thread"]["properties"])
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["status"],
            "element_map_freeze_first_slice",
        )
        self.assertIn(
            "find_holes_make_shape_with_element_map",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "profile_source_tool_face_mapper_history",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "point_profile_head_cut_history",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "model_thread_compound_tool_shape",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "threaded_model_thread_head_cut_native_oracle",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["known_gap_fixtures"],
            [],
        )
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["remaining"],
            [],
        )
        self.assertIn(
            "p7/hole-supported-model-thread-counterbore",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertIn(
            "p7/hole-supported-model-thread-metric",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertIn(
            "p7/hole-supported-point-counterbore",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertEqual(capabilities["part_design"]["hole"]["native_oracle_known_gap_fixtures"], [])
        self.assertEqual(
            capabilities["part_design"]["hole"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["adapters"]["core_entrypoints"],
            [
                "cad_core_recompute_json",
                "cad_core_export_json",
                "cad_core_capabilities_json",
                "cli_recompute",
            ],
        )
        self.assertEqual(
            capabilities["adapters"]["stateless_result_channels"],
            ["results", "elementReferenceUpdates", "documentObjectUpdates", "diagnostics"],
        )
        self.assertEqual(
            capabilities["adapters"]["c_api_export"],
            ["buffer_only", "rejects_server_file_paths", "metadata_diagnostics", "stl_deflection"],
        )
        self.assertEqual(
            capabilities["adapters"]["cli_export"],
            ["file_protocol", "requires_object_format_file", "stl_deflection"],
        )
        self.assertIn("worker_adapter", capabilities["adapters"]["remaining_gaps"])
        self.assertIn("wasm_adapter", capabilities["adapters"]["remaining_gaps"])
        self.assertIn("streaming_mesh_limits", capabilities["adapters"]["remaining_gaps"])
        link_sub_fields = [
            "value",
            "SubList",
            "StableSubList",
            "FullSubList",
            "ShadowSub",
            "ReferenceShadow",
            "ExternalFlags",
            "Document",
        ]
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLink"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkGlobal"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkHidden"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLink"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkList"],
            ["values"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkListHidden"],
            ["values"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkList"],
            ["values", "SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSub"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubHidden"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSub"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSubHidden"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubList"],
            ["SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubListHidden"],
            ["SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSubList"],
            ["SubSet"],
        )

        for type_id in [
            "Sketcher::SketchObject",
            "PartDesign::Draft",
            "PartDesign::Thickness",
            "PartDesign::Hole",
            "Part::Box",
            "Part::Offset",
            "Part::Offset2D",
            "Part::Thickness",
            "Part::BooleanFragments",
            "App::Link",
            "Assembly::AssemblyObject",
        ]:
            self.assertIn(type_id, capabilities["supported_type_ids"])

        for code in [
            "parse_error",
            "document_hash_mismatch",
            "external_document_pending_reload",
            "external_document_unloaded",
            "missing_external_document",
            "missing_external_geometry_snapshot",
            "missing_target",
            "missing_link_target",
            "cycle_dependency",
            "unsupported_type",
            "invalid_subshape",
            "unsupported_subshape_kind",
            "unsupported_stable_subname",
            "label_reference_ambiguous",
            "subname_resolve_ambiguous",
            "subname_resolve_failed",
            "subname_semantic_drift",
            "split_stable_subname",
            "deleted_stable_subname",
            "unsupported_assembly_solver",
            "unsupported_reference_shadow_brep",
            "sketch_solver_conflict",
            "sketch_solver_malformed_constraint",
            "sketch_solver_redundant",
        ]:
            self.assertIn(code, capabilities["diagnostic_codes"])

        self.assertEqual(
            capabilities["assembly"]["solver_adapter"],
            ["skipped_no_joints", "grounded_only_noop", "unsupported_joint_diagnostics"],
        )
        self.assertIn("full_ondsel_solver", capabilities["assembly"]["remaining_gaps"])
        self.assertIn("solver_placement_updates", capabilities["assembly"]["remaining_gaps"])

        self.assertEqual(
            capabilities["topo_history"]["stable_subname_resolution"],
            [
                "indexed",
                "source_preserved",
                "one_to_one_history",
                "unique_same_kind_split_recovery",
                "reference_shadow_recovery",
            ],
        )
        self.assertEqual(
            capabilities["topo_history"]["mapper_history_core"],
            [
                "source_endpoint",
                "target_endpoint",
                "shape_kind",
                "relation",
                "maker_stage",
                "evidence",
                "recoverability",
                "diagnostic_status",
                "summary_only_diagnostics",
                "legacy_history_conversion",
                "element_map_preserved_aliases",
            ],
        )
        self.assertIn("refine_modified_deleted_generated", capabilities["topo_history"]["maker_history"])
        self.assertIn("sketch_internalshape_producer_evidence", capabilities["topo_history"]["maker_history"])
        self.assertIn("taper_thru_sections", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_addsubshape_slot", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_multi_selection_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_draft_parameter_variants", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_thickness_parameter_variants", capabilities["topo_history"]["maker_history"])
        self.assertIn("thickness_multi_solid_fuse_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("chain_dressup_pattern_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("shapefix_deleted_small_edge", capabilities["topo_history"]["maker_history"])
        self.assertIn("shapefix_root_modified_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("import_shape_element_map", capabilities["topo_history"]["maker_history"])
        self.assertIn("part_offset", capabilities["topo_history"]["maker_history"])
        self.assertIn("transformed_pattern_addsub_ownership", capabilities["topo_history"]["maker_history"])
        self.assertIn("transformed_pattern_full_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("hole_find_holes_profile_source_history", capabilities["topo_history"]["maker_history"])
        producer_matrix = capabilities["topo_history"]["producer_matrix"]
        self.assertEqual(producer_matrix["shape_fix"]["status"], "covered_no_generated_producer")
        self.assertEqual(
            producer_matrix["shape_fix"]["covered"],
            ["deleted_small_edge", "root_modified", "generated_empty_review"],
        )
        self.assertEqual(producer_matrix["shape_fix"]["remaining"], [])
        self.assertEqual(producer_matrix["refine"]["status"], "covered")
        self.assertEqual(
            producer_matrix["refine"]["covered"],
            ["modified", "deleted", "generated"],
        )
        self.assertEqual(producer_matrix["section"]["status"], "covered")
        self.assertEqual(
            producer_matrix["section"]["covered"],
            [
                "approximation_property",
                "auto_fuzzy_value",
                "source_qualified_edge_history",
                "terminal_deleted_history",
            ],
        )
        self.assertEqual(producer_matrix["section"]["remaining"], [])
        self.assertEqual(producer_matrix["part_offset"]["status"], "done_second_slice")
        self.assertIn("face_source_offset", producer_matrix["part_offset"]["covered"])
        self.assertIn("fill_offset", producer_matrix["part_offset"]["covered"])
        self.assertIn("solid_source_make_element_solid", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_open_wire_no_fill", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_open_wire_fill", producer_matrix["part_offset"]["covered"])
        self.assertNotIn("fill_offset", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("solid_source_make_element_solid", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("offset2d_open_wire_no_fill", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("offset2d_open_wire_fill", producer_matrix["part_offset"]["remaining"])
        self.assertIn("offset2d_compound_child_recursive", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_compound_collective", producer_matrix["part_offset"]["covered"])
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_compound_collective",
            producer_matrix["part_offset"]["remaining"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_intersection_compound_collective",
            producer_matrix["part_offset"]["remaining"],
        )
        self.assertEqual(producer_matrix["import_shape"]["status"], "done_first_slice")
        self.assertIn("owner_qualified_alias", producer_matrix["import_shape"]["covered"])
        self.assertEqual(producer_matrix["sketch_internalshape"]["status"], "done_first_slice")
        self.assertIn(
            "mixed_bounded_faces_open_wires_oracle",
            producer_matrix["sketch_internalshape"]["covered"],
        )
        self.assertEqual(producer_matrix["sketch_internalshape"]["remaining"], [])
        self.assertEqual(producer_matrix["dressup"]["status"], "done_first_slice")
        self.assertIn("multi_selection_history", producer_matrix["dressup"]["covered"])
        self.assertIn("chamfer_parameter_variants", producer_matrix["dressup"]["covered"])
        self.assertIn("draft_datum_plane_line", producer_matrix["dressup"]["covered"])
        self.assertIn("draft_auto_neutral_plane_guess", producer_matrix["dressup"]["covered"])
        self.assertIn("thickness_parameter_variants", producer_matrix["dressup"]["covered"])
        self.assertIn("thickness_multi_solid_fuse_history", producer_matrix["dressup"]["covered"])
        self.assertIn("failure_diagnostics", producer_matrix["dressup"]["covered"])
        self.assertIn("chain_dressup_pattern_history", producer_matrix["dressup"]["covered"])
        self.assertNotIn("multi_selection_history", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("chamfer_parameter_variants", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("draft_datum_plane_line", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("draft_auto_neutral_plane_guess", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("thickness_parameter_variants", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("thickness_multi_solid_fuse_history", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("failure_diagnostics", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("chain_dressup_pattern_history", producer_matrix["dressup"]["remaining"])
        self.assertEqual(producer_matrix["dressup"]["remaining"], [])
        self.assertEqual(producer_matrix["transformed"]["status"], "covered")
        self.assertIn("link_retag_composition", producer_matrix["transformed"]["covered"])
        self.assertIn("terminal_split_deleted", producer_matrix["transformed"]["covered"])
        self.assertEqual(producer_matrix["transformed"]["remaining"], [])
        self.assertIn("flagged_compound_tool_expansion", producer_matrix["body_boolean"]["covered"])
        self.assertIn("flagged_compound_tool_expansion", producer_matrix["part_boolean"]["covered"])
        self.assertEqual(producer_matrix["hole"]["status"], "done_first_slice")
        self.assertIn("profile_source_tool_face_mapper_history", producer_matrix["hole"]["covered"])
        self.assertIn("point_profile_head_cut_history", producer_matrix["hole"]["covered"])
        self.assertIn("model_thread_compound_tool_shape", producer_matrix["hole"]["covered"])
        self.assertIn("threaded_model_thread_head_cut_native_oracle", producer_matrix["hole"]["covered"])
        self.assertEqual(producer_matrix["hole"]["known_gap_fixtures"], [])
        self.assertEqual(producer_matrix["hole"]["remaining"], [])
        hole_capability = capabilities["part_design"]["hole"]
        self.assertEqual(hole_capability["history"]["status"], "element_map_freeze_first_slice")
        self.assertIn("profile_source_tool_face_mapper_history", hole_capability["history"]["covered"])
        self.assertEqual(
            hole_capability["remaining_gaps"],
            [],
        )
        self.assertEqual(capabilities["topo_history"]["terminal_history"], ["deleted", "split", "merge"])
        self.assertEqual(
            capabilities["topo_history"]["element_history_status"],
            [
                "generated_modified",
                "terminal_split_deleted",
                "subname_split_requires_reselect",
                "merge",
                "facemaker_pre_split",
                "facemaker_splitter",
                "facemaker_summary_only",
                "import_shape_element_map",
                "shapefix_root_history_modified",
                "element_map_policy_drop",
                "element_map_child_map:preserve_source_ranges",
                "element_map_child_map:recursive_source_ranges",
                "element_map_child_map:postfix_source_ranges",
                "element_map_child_map:hashed_child_map_keys",
                "element_map_policy_propagate:make_element_wires",
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
                "part_offset2d:compound_collective_makeoffset",
                "part_thickness:make_thick_solid",
            ],
        )
        self.assertNotIn("sketch_internalshape_main_path", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("taper_full_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("import_shape_element_map", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_modified_generated_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_generated_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("transformed_pattern_full_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertEqual(capabilities["part_workbench"]["offset"]["status"], "done_second_slice")
        self.assertIn("Part::Compound", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("Part::Offset", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("Part::Offset2D", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("fill_offset", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("solid_source_make_element_solid", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_face_no_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_face_fill_closed", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_open_wire_no_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_open_wire_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_compound_child_recursive", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_compound_collective", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("thickness_single_solid_face", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("thickness_mode_join_oracle", capabilities["part_workbench"]["offset"]["covered"])
        self.assertNotIn("fill_offset", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "solid_source_make_element_solid",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn("offset2d", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_open_compound",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_open_wire_compound",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_open_wire_no_fill",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_open_wire_fill",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn("thickness", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "thickness_mode_join_oracle",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn("complete_mapper_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn(
            "element_map_child_map_preserve_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_recursive_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_postfix_hash_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_hash_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_policy_propagate_shell_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "hole_threaded_model_thread_profile_head_oracle_matrix",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn("hole_threaded_model_thread_profile_head_oracle_matrix", capabilities["known_gaps"])
        self.assertNotIn("complete_mapper_history", capabilities["known_gaps"])
        self.assertNotIn("assembly_joint_solver", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_preserve_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_recursive_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn(
            "element_map_child_map_postfix_hash_propagate_lifecycle",
            capabilities["known_gaps"],
        )
        self.assertNotIn("element_map_child_map_hash_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_policy_propagate_shell_lifecycle", capabilities["known_gaps"])
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn("hole_threaded_model_thread_profile_head_oracle_matrix", capabilities["known_gaps"])
        self.assertIn("assembly_full_ondsel_solver", capabilities["known_gaps"])
        self.assertIn("assembly_solver_placement_updates", capabilities["known_gaps"])
        self.assertNotIn("show_element_missing_child_lifecycle", capabilities["known_gaps"])

    def test_c_api_recompute_reports_show_element_lifecycle_updates(self) -> None:
        ffi_result = self.run_recompute_ffi("app-link-show-element-synthetic", "p8")
        updates = ffi_result["documentObjectUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual([item["action"] for item in updates], ["create", "create"])
        self.assertEqual(updates[0]["object"], "ArrayLink_i0")
        self.assertEqual(updates[0]["properties"]["LinkedObject"]["value"], "Box")

    def test_c_api_exports_recomputed_shape_buffers(self) -> None:
        document = json.loads((ROOT / "fixtures" / "p8" / "part-box.json").read_text(encoding="utf-8"))
        cases = {
            "brep": ("Part::ImportBrep", "ExportedBrep", "box.brep"),
            "step": ("Part::ImportStep", "ExportedStep", "box.step"),
            "stl": ("Mesh::Import", "ExportedStl", "box.stl"),
        }

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            for export_format, (type_id, object_name, file_name) in cases.items():
                with self.subTest(export_format=export_format):
                    status, metadata, data, error = self.call_export_ffi(
                        {"document": document, "object": "Box", "format": export_format}
                    )
                    self.assertEqual(status, 0, error)
                    self.assertIsNotNone(metadata)
                    assert metadata is not None
                    self.assertEqual(metadata["object"], "Box")
                    self.assertEqual(metadata["format"], export_format)
                    self.assertEqual(metadata["filename"], f"Box.{export_format}")
                    self.assertEqual(metadata["diagnostics"], [])
                    self.assertEqual(metadata["bytes"], len(data))
                    self.assertGreater(len(data), 0)

                    export_path = tmp_path / file_name
                    export_path.write_bytes(data)
                    import_request = {
                        "Objects": [
                            {
                                "Name": object_name,
                                "ID": 1,
                                "TypeId": type_id,
                                "Properties": {"FileName": str(export_path)},
                            }
                        ],
                        "recompute": {"objs": [object_name]},
                    }
                    import_path = tmp_path / f"import-{export_format}.json"
                    import_path.write_text(json.dumps(import_request), encoding="utf-8")
                    imported_result = self.run_recompute_file(import_path)
                    imported = imported_result["objects"][object_name]
                    expected = self.expected_freecad("p8", "part-box")

                    self.assertEqual(imported_result["diagnostics"], [])
                    self.assertEqual(imported["status"], "ok")
                    if export_format == "stl":
                        self.assertEqual(imported["primitive"], "import_stl")
                        self.assert_expected_object(imported_result, object_name, {"bbox": expected["bbox"]})
                        self.assertGreater(imported_result["mesh"][object_name]["summary"]["triangle_count"], 0)
                    else:
                        self.assert_expected_object(
                            imported_result,
                            object_name,
                            {"bbox": expected["bbox"], "volume": expected["volume"]},
                        )
    def test_c_api_export_reports_business_diagnostics_without_server_paths(self) -> None:
        document = json.loads((ROOT / "fixtures" / "p8" / "part-box.json").read_text(encoding="utf-8"))

        status, metadata, data, error = self.call_export_ffi(
            {"document": document, "object": "Missing", "format": "step"}
        )
        self.assertEqual(status, 0, error)
        self.assertEqual(data, b"")
        self.assertIsNotNone(metadata)
        assert metadata is not None
        self.assertEqual(metadata["bytes"], 0)
        self.assertEqual([item["code"] for item in metadata["diagnostics"]], ["missing_object"])

        status, metadata, data, error = self.call_export_ffi(
            {
                "document": document,
                "object": "Box",
                "format": "step",
                "export_file": "/tmp/box.step",
            }
        )
        self.assertEqual(status, 1)
        self.assertIsNone(metadata)
        self.assertEqual(data, b"")
        self.assertIn("server file path", error)

    def test_p8_cli_exports_recomputed_shape_files(self) -> None:
        cases = {
            "brep": ("Part::ImportBrep", "ExportedBrep", "box.brep"),
            "step": ("Part::ImportStep", "ExportedStep", "box.step"),
            "stl": ("Mesh::Import", "ExportedStl", "box.stl"),
        }

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            for export_format, (type_id, object_name, file_name) in cases.items():
                with self.subTest(export_format=export_format):
                    export_path = tmp_path / file_name
                    result = self.run_recompute_file(
                        ROOT / "fixtures" / "p8" / "part-box.json",
                        [
                            "--export-object",
                            "Box",
                            "--export-format",
                            export_format,
                            "--export-file",
                            str(export_path),
                        ],
                    )

                    self.assertEqual(result["diagnostics"], [])
                    self.assertEqual(
                        result["exports"],
                        [{"object": "Box", "format": export_format, "file": str(export_path)}],
                    )
                    self.assertTrue(export_path.exists())
                    self.assertGreater(export_path.stat().st_size, 0)

                    import_request = {
                        "Objects": [
                            {
                                "Name": object_name,
                                "ID": 1,
                                "TypeId": type_id,
                                "Properties": {"FileName": str(export_path)},
                            }
                        ],
                        "recompute": {"objs": [object_name]},
                    }
                    import_path = tmp_path / f"import-{export_format}.json"
                    import_path.write_text(json.dumps(import_request), encoding="utf-8")
                    imported_result = self.run_recompute_file(import_path)
                    imported = imported_result["objects"][object_name]
                    expected = self.expected_freecad("p8", "part-box")

                    self.assertEqual(imported_result["diagnostics"], [])
                    self.assertEqual(imported["status"], "ok")
                    if export_format == "stl":
                        self.assertEqual(imported["primitive"], "import_stl")
                        self.assert_expected_object(imported_result, object_name, {"bbox": expected["bbox"]})
                        self.assertGreater(imported_result["mesh"][object_name]["summary"]["triangle_count"], 0)
                    else:
                        self.assert_expected_object(
                            imported_result,
                            object_name,
                            {"bbox": expected["bbox"], "volume": expected["volume"]},
                        )
