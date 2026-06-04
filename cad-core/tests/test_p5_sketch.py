from __future__ import annotations

import ctypes
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import ROOT, CadCoreFixtureTestCase


class CadCoreP5SketchTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    P7_RESULT_WIRE_IDENTITY_FIXTURES = (
        "sketch-internal-face-cross-cutters",
        "sketch-internal-face-segmented-cross-cutter",
        "sketch-internal-face-t-cutter",
        "sketch-internal-face-three-overlap-circles",
        "sketch-internal-face-arc-lens",
        "part-extrusion-facemaker-bullseye-intersected-holes",
    )

    def run_payload(self, payload: dict) -> dict:
        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)
            return self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

    def external_geometry_state_payload(self, flags: list[str]) -> dict:
        return {
            "Objects": [
                {
                    "Name": "Box",
                    "ID": 1,
                    "TypeId": "Part::Box",
                    "Properties": {
                        "Length": 10,
                        "Width": 5,
                        "Height": 1,
                    },
                },
                {
                    "Name": "Sketch",
                    "ID": 2,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {
                        "Geometry": [],
                        "ExternalGeometry": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "Box",
                                    "SubList": ["Face5"],
                                    "ExternalFlags": flags,
                                }
                            ],
                        },
                        "Constraints": [],
                    },
                },
            ],
            "recompute": {"objs": ["Sketch"]},
        }

    def native_external_geo_state_payload(self, flags: list[str]) -> dict:
        ref = "MissingBox.Face5"
        old_geometry = [
            {"kind": "LineSegment", "start": [0, 0], "end": [10, 0]},
            {"kind": "LineSegment", "start": [10, 0], "end": [10, 5]},
            {"kind": "LineSegment", "start": [10, 5], "end": [0, 5]},
            {"kind": "LineSegment", "start": [0, 5], "end": [0, 0]},
        ]
        for item in old_geometry:
            item["construction"] = True
            item["Ref"] = ref
            item["ExternalFlags"] = flags
        return {
            "Objects": [
                {
                    "Name": "Sketch",
                    "ID": 2,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {
                        "Geometry": [],
                        "ExternalGeo": {
                            "PropertyType": "Part::PropertyGeometryList",
                            "Geometry": old_geometry,
                        },
                        "ExternalGeometry": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "MissingBox",
                                    "SubList": ["Face5"],
                                    "ExternalFlags": flags,
                                }
                            ],
                        },
                        "Constraints": [],
                    },
                },
            ],
            "recompute": {"objs": ["Sketch"]},
        }

    def assert_super_edge_lifecycle_ledger(self, ledger: dict[str, int]) -> None:
        self.assertGreater(ledger["super_edge_candidate_count"], 0)
        self.assertEqual(ledger["super_edge_root_edge_info_count"], ledger["super_edge_candidate_count"])
        self.assertEqual(
            ledger["super_edge_open_candidate_count"] + ledger["super_edge_closed_candidate_count"],
            ledger["super_edge_candidate_count"],
        )
        self.assertEqual(
            ledger["super_edge_materialized_root_edge_info_count"],
            ledger["super_edge_root_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_materialized_edge_info_count"],
            ledger["super_edge_candidate_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_shadowed_member_edge_info_count"]
            + ledger["super_edge_materialized_root_edge_info_count"],
            ledger["super_edge_materialized_edge_info_count"],
        )
        self.assertGreaterEqual(
            ledger["super_edge_candidate_edge_info_count"],
            ledger["super_edge_candidate_count"],
        )
        self.assertEqual(
            ledger["super_edge_lifecycle_member_minus_one_edge_info_count"],
            ledger["super_edge_shadowed_member_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_lifecycle_open_root_edge_info_count"]
            + ledger["super_edge_lifecycle_closed_root_edge_info_count"],
            ledger["super_edge_materialized_root_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_lifecycle_adjacent_range_rewrite_count"],
            ledger["super_edge_lifecycle_open_root_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_lifecycle_endpoint_rewrite_count"],
            ledger["super_edge_lifecycle_open_root_edge_info_count"],
        )
        self.assertEqual(
            ledger["super_edge_lifecycle_adjacent_range_source_edge_info_count"],
            ledger["super_edge_lifecycle_open_root_edge_info_count"],
        )
        self.assertGreaterEqual(
            ledger["super_edge_lifecycle_adjacent_range_vertex_count"],
            ledger["super_edge_lifecycle_adjacent_range_rewrite_count"],
        )

    def assert_closed_wire_stack_ledger(self, ledger: dict[str, int]) -> None:
        if ledger["closed_wire_info_count"] == 0:
            self.assertEqual(ledger["closed_wire_vertex_count"], 0)
            self.assertEqual(ledger["closed_wire_search_stack_frame_count"], 0)
            self.assertEqual(ledger["closed_wire_search_vertex_stack_count"], 0)
            self.assertEqual(ledger["closed_wire_search_edge_set_visit_count"], 0)
            self.assertEqual(ledger["closed_wire_search_backtrack_count"], 0)
            self.assertEqual(ledger["closed_wire_search_intersect_skip_count"], 0)
            return

        self.assertGreaterEqual(
            ledger["closed_wire_search_stack_frame_count"],
            ledger["closed_wire_info_count"],
        )
        if ledger["closed_wire_vertex_count"] > ledger["closed_wire_info_count"]:
            self.assertGreater(ledger["closed_wire_search_vertex_stack_count"], 0)
            self.assertGreater(ledger["closed_wire_search_edge_set_visit_count"], 0)
        self.assertGreaterEqual(
            ledger["closed_wire_search_vertex_stack_count"],
            ledger["closed_wire_search_edge_set_visit_count"],
        )

    def assert_existing_wire_search_ledger(self, ledger: dict[str, int]) -> None:
        if ledger["tight_bound_existing_wire_search_count"] == 0:
            self.assertEqual(ledger["tight_bound_existing_wire_hit_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_reverse_hit_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_purge_count"], 0)
            self.assertEqual(ledger["tight_bound_purged_wire_info_count"], 0)
            self.assertEqual(ledger["tight_bound_exhaust_discarded_purged_wire_info_count"], 0)
            self.assertEqual(ledger["tight_bound_exhaust_primary_reset_edge_info_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_insert_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_erase_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_abort_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_purge_candidate_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_blocked_transfer_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_abort_search_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_abort_resolved_by_hit_count"], 0)
            self.assertEqual(ledger["tight_bound_full_wire_set_abort_blocked_search_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_multi_round_wire_info_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_multi_round_search_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_stack_frame_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_vertex_stack_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_edge_set_visit_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_idx_vertex_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_stack_pos_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_path_vertex_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_selected_hit_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_hit_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_idx_vertex_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_stack_pos_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_path_blocked_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_order_blocked_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_idx_vertex_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_stack_pos_count"], 0)
            self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], 0)
            return

        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_search_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_hit_count"]
            + ledger["tight_bound_existing_wire_reverse_hit_count"],
            ledger["tight_bound_existing_wire_search_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_purge_count"],
            ledger["tight_bound_existing_wire_reverse_hit_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_purge_count"],
            ledger["tight_bound_existing_wire_reverse_hit_count"]
            + ledger["tight_bound_full_wire_set_purge_candidate_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_purged_wire_info_count"],
            ledger["tight_bound_existing_wire_purge_count"],
        )
        if ledger["tight_bound_existing_wire_purge_count"] > 0:
            self.assertGreater(ledger["tight_bound_purged_wire_info_count"], 0)
        if ledger["tight_bound_existing_wire_hit_count"] > 0:
            self.assertGreaterEqual(
                ledger["tight_bound_existing_wire_search_path_vertex_count"],
                ledger["tight_bound_existing_wire_hit_count"],
            )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_search_only_path_blocked_count"],
            ledger["tight_bound_existing_wire_search_only_hit_count"],
        )
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_path_blocked_count"],
            ledger["tight_bound_existing_wire_search_only_order_blocked_count"],
        )
        if ledger["tight_bound_existing_wire_search_only_hit_count"] > 0:
            self.assertGreater(ledger["tight_bound_existing_wire_search_only_path_blocked_count"], 0)
        self.assertLessEqual(
            ledger["tight_bound_exhaust_discarded_purged_wire_info_count"],
            ledger["tight_bound_purged_wire_info_count"],
        )
        if ledger["tight_bound_purged_wire_info_count"] > 0:
            self.assertEqual(
                ledger["tight_bound_exhaust_discarded_purged_wire_info_count"],
                ledger["tight_bound_purged_wire_info_count"],
            )
        self.assertLessEqual(
            ledger["tight_bound_exhaust_primary_reset_edge_info_count"],
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_exhaust_done_wire_info_count"]
            + ledger["tight_bound_exhaust_discarded_purged_wire_info_count"],
            ledger["tight_bound_exhaust_visited_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_exhaust_visited_wire_info_count"],
            ledger["tight_bound_done_wire_info_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_full_wire_set_insert_count"],
            ledger["tight_bound_full_wire_set_erase_count"],
        )
        self.assertGreater(ledger["tight_bound_full_wire_set_insert_count"], 0)
        self.assertEqual(
            ledger["tight_bound_full_wire_set_abort_count"],
            ledger["tight_bound_full_wire_set_purge_candidate_count"],
        )
        self.assertEqual(
            ledger["tight_bound_full_wire_set_abort_search_count"],
            ledger["tight_bound_full_wire_set_abort_resolved_by_hit_count"]
            + ledger["tight_bound_full_wire_set_abort_blocked_search_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_full_wire_set_abort_search_count"],
            ledger["tight_bound_existing_wire_search_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_full_wire_set_abort_resolved_by_hit_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_full_wire_set_abort_count"],
            ledger["tight_bound_full_wire_set_abort_search_count"],
        )
        if ledger["tight_bound_full_wire_set_abort_count"] > 0:
            self.assertGreater(ledger["tight_bound_full_wire_set_abort_search_count"], 0)
        self.assertLessEqual(
            ledger["tight_bound_full_wire_set_blocked_transfer_count"],
            ledger["tight_bound_full_wire_set_abort_blocked_search_count"]
            + ledger["tight_bound_existing_wire_purge_count"],
        )
        if ledger["tight_bound_full_wire_set_abort_count"] > 0:
            self.assertGreater(ledger["tight_bound_full_wire_set_blocked_transfer_count"], 0)
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_multi_round_wire_info_count"],
            ledger["tight_bound_existing_wire_search_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_multi_round_search_count"],
            ledger["tight_bound_existing_wire_search_count"],
        )
        self.assertGreater(ledger["tight_bound_existing_wire_search_stack_frame_count"], 0)
        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_search_vertex_stack_count"],
            ledger["tight_bound_existing_wire_hit_count"]
            + ledger["tight_bound_existing_wire_reverse_hit_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_search_edge_set_visit_count"],
            ledger["tight_bound_existing_wire_search_stack_frame_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_search_idx_vertex_count"],
            ledger["tight_bound_existing_wire_idx_vertex_count"],
        )
        self.assertGreaterEqual(
            ledger["tight_bound_existing_wire_search_stack_pos_count"],
            ledger["tight_bound_existing_wire_stack_pos_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_search_idx_vertex_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_search_stack_pos_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        if ledger["tight_bound_existing_wire_hit_count"] > 0:
            self.assertGreater(ledger["tight_bound_existing_wire_search_idx_vertex_count"], 0)
            self.assertGreater(ledger["tight_bound_existing_wire_search_stack_pos_count"], 0)
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_selected_hit_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_selected_hit_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_hit_count"]
            + ledger["tight_bound_existing_wire_selected_hit_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_idx_vertex_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_stack_pos_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_idx_vertex_count"],
            ledger["tight_bound_existing_wire_selected_hit_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_existing_wire_stack_pos_count"],
            ledger["tight_bound_existing_wire_selected_hit_count"],
        )
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_idx_vertex_count"]
            + ledger["tight_bound_existing_wire_idx_vertex_count"],
            ledger["tight_bound_existing_wire_search_idx_vertex_count"],
        )
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_stack_pos_count"]
            + ledger["tight_bound_existing_wire_stack_pos_count"],
            ledger["tight_bound_existing_wire_search_stack_pos_count"],
        )

    def assert_existing_wire_search_only_order_blocked_ledger(self, ledger: dict[str, int]) -> None:
        blocked = ledger["tight_bound_existing_wire_search_only_path_blocked_count"]
        self.assertEqual(ledger["tight_bound_existing_wire_search_only_order_blocked_count"], blocked)

    def test_p5_existing_wire_search_only_path_blocks_are_order_limited(self) -> None:
        for fixture in [
            "sketch-internal-face-through-open-cutter",
            "sketch-internal-face-split-line",
            "sketch-internal-face-split-and-dangling",
            "sketch-internal-face-branch-open-cutter",
            "sketch-internal-face-adjacent-rectangles",
            "sketch-internal-face-arc-lens",
            "pad-internal-face-sublist",
            "pad-internal-face-reference-shadow",
            "pad-internal-face-reference-shadow-brep-recover-sublist",
            "part-extrusion-facemaker-bullseye-intersected-holes",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                ledger = result["objects"]["Sketch"]["wire_joiner_ledger"]

                self.assert_existing_wire_search_ledger(ledger)
                self.assertGreater(ledger["tight_bound_existing_wire_search_only_path_blocked_count"], 0)
                self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
                self.assertEqual(ledger["repeated_split_exhaust_rerun_active_edge_info_count"], 0)
                self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_search_count"], 0)
                self.assertEqual(ledger["repeated_split_exhaust_rerun_no_active_search_count"], 1)

    def test_p5_all_fixtures_omit_generated_and_disable_legacy_helper_output(self) -> None:
        p5_fixture_dir = ROOT / "fixtures" / "p5"
        for fixture_path in sorted(p5_fixture_dir.glob("*.json")):
            with self.subTest(fixture=fixture_path.stem):
                result = self.run_recompute_file(fixture_path)
                for object_name, obj in result["objects"].items():
                    ledger = obj.get("wire_joiner_ledger")
                    if not isinstance(ledger, dict):
                        continue
                    with self.subTest(fixture=fixture_path.stem, object=object_name):
                        producer_entries = ledger["result_wire_producer_ledger_entries"]
                        removed_summary_fields = (
                            "generated_open_export_edge_info_count",
                            "open_wire_compound_generated_wire_info_count",
                            "open_wire_compound_legacy_helper_shape_wire_info_count",
                            "result_wire_producer_blocker_legacy_helper_shape_still_used_count",
                            "unowned_removal_ready_legacy_helper_shape_output_count",
                            "result_wire_producer_ledger_entry_count",
                            "migrated_legacy_helper_slot_count",
                            "open_wire_compound_helper_open_export_override_wire_info_count",
                            "helper_open_export_override_edge_info_count",
                            "result_wire_producer_exported_without_helper_wire_info_count",
                            "result_wire_producer_none_count",
                            "result_wire_producer_existing_source_edge_count",
                            "result_wire_producer_partial_shared_closed_wire_count",
                            "result_wire_producer_live_reset_open_edge_count",
                            "result_wire_producer_super_edge_root_count",
                            "result_wire_producer_current_member_child_wire_count",
                            "result_wire_producer_located_count",
                            "result_wire_producer_ahistory_evidence_ready_count",
                            "result_wire_producer_none_without_blocker_count",
                            "source_shape_identity_unknown_count",
                            "result_wire_producer_child_wire_ready_count",
                            "result_wire_producer_source_shape_ready_count",
                            "result_wire_producer_source_shape_not_ready_count",
                            "graph_fallback_assigned_edge_info_count",
                            "graph_secondary_owner_edge_info_count",
                            "result_wire_producer_unknown_invariant_count",
                            "result_wire_producer_blocker_missing_source_lineage_count",
                            "result_wire_producer_blocker_missing_ahistory_remove_source_count",
                            "result_wire_producer_blocker_foreign_ahistory_source_lineage_count",
                            "result_wire_producer_blocker_foreign_ahistory_source_shape_ready_lineage_mismatch_count",
                            "result_wire_producer_blocker_foreign_ahistory_source_shape_identity_not_ready_count",
                            "result_wire_producer_blocker_foreign_ahistory_source_geometry_mismatch_count",
                            "result_wire_producer_blocker_missing_removed_target_evidence_count",
                            "result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count",
                            "result_wire_producer_blocker_final_gate_blocked_by_iteration_count",
                            "result_wire_producer_blocker_final_gate_blocked_by_wire_info_count",
                            "result_wire_producer_blocker_root_removed_by_unowned_branch_count",
                            "result_wire_producer_blocker_root_removed_by_primary_branch_count",
                            "result_wire_producer_blocker_root_removed_by_secondary_branch_count",
                            "result_wire_producer_blocker_multi_member_root_pending_suppression_count",
                            "result_wire_producer_blocker_source_shape_identity_not_ready_count",
                            "result_wire_producer_blocker_source_shape_would_purge_original_count",
                            "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count",
                            "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count",
                            "result_wire_producer_blocker_same_source_sidecar_source_shape_identity_not_ready_count",
                            "result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count",
                            "result_wire_producer_blocker_source_shape_member_vertex_identity_not_ready_count",
                            "result_wire_producer_blocker_current_member_child_wire_identity_not_ready_count",
                            "result_wire_producer_blocker_current_member_missing_sidecar_evidence_count",
                            "result_wire_producer_blocker_current_member_root_open_producer_not_ready_count",
                            "result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count",
                            "unowned_removal_ready_slot_count",
                            "unowned_removal_current_member_producer_output_count",
                            "multi_member_root_direct_output_count",
                            "branch_search_outside_candidate_count",
                            "owner_propagation_unassigned_candidate_count",
                            "repeated_split_exhaust_rerun_branch_search_outside_candidate_count",
                            "repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count",
                            "repeated_split_exhaust_rerun_live_transfer_wire_info_count",
                            "repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count",
                            "repeated_split_exhaust_rerun_removal_edge_info_count",
                            "repeated_split_exhaust_rerun_removal_primary_edge_info_count",
                            "repeated_split_exhaust_rerun_removal_secondary_edge_info_count",
                            "repeated_split_exhaust_rerun_removal_unowned_edge_info_count",
                            "tight_bound_exhaust_primary_reset_blocked_edge_info_count",
                            "tight_bound_existing_wire_search_intersect_skip_count",
                            "tight_bound_existing_wire_search_only_owner_vertex_blocked_count",
                            "tight_bound_existing_wire_search_only_wire_build_blocked_count",
                            "tight_bound_live_split_wire_edge_info_count",
                            "tight_bound_live_split_wire_info_count",
                        )
                        for field in removed_summary_fields:
                            self.assertNotIn(field, ledger)
                        self.assertTrue(
                            all(
                                entry["state"] == "ExportedWithoutHelper" and entry["blocker"] == "None"
                                for entry in producer_entries
                            )
                        )
                        if isinstance(obj.get("wire_joiner_history_detail"), dict):
                            removed_history_summary_fields = (
                                "open_export_source_lineage_edge_count",
                                "open_export_missing_source_lineage_edge_count",
                                "open_export_helper_override_edge_count",
                                "open_export_helper_override_missing_source_lineage_edge_count",
                                "open_export_purge_bridge_edge_count",
                            )
                            for field in removed_history_summary_fields:
                                self.assertNotIn(field, obj["wire_joiner_history_detail"])
                            self.assert_result_wire_producer_ledger(obj)

    def test_p7_sketch_internal_history_consumes_result_wire_producer_identity(self) -> None:
        for fixture in self.P7_RESULT_WIRE_IDENTITY_FIXTURES:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                self.assert_sketch_internal_history_consumes_result_wire_producer_identity(result)

    def assert_sketch_internal_history_consumes_result_wire_producer_identity(
        self,
        result: dict[str, object],
    ) -> None:
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]
        runtime_history = sketch["wire_joiner_history_detail"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_history = named_shape["sketch_internal_history"]
        runtime_entries = runtime_history["open_export_history_entries"]
        internal_entries = internal_history["wire_joiner_open_export_history_entries"]
        producer_entries = ledger["result_wire_producer_ledger_entries"]
        runtime_entry_counts = self.open_export_entry_counts(runtime_entries)
        internal_entry_counts = self.open_export_entry_counts(internal_entries)

        self.assertEqual(named_shape["sketch_internal_history_status"], "history_evidence:facemaker_wirejoiner")
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertEqual(len(internal_entries), len(runtime_entries))
        self.assertEqual(internal_entry_counts, runtime_entry_counts)
        self.assertEqual(internal_history["named_shape_history_missing_result_wire_identity_count"], 0)
        self.assertEqual(internal_history["element_map_result_wire_identity_mismatch_count"], 0)
        self.assertEqual(len(producer_entries), runtime_entry_counts["helper_override"])
        removed_internal_summary_fields = (
            "wire_joiner_open_export_edge_count",
            "wire_joiner_open_export_source_lineage_edge_count",
            "wire_joiner_open_export_missing_source_lineage_edge_count",
            "wire_joiner_open_export_helper_override_edge_count",
            "wire_joiner_open_export_helper_override_missing_source_lineage_edge_count",
            "wire_joiner_open_export_purge_bridge_edge_count",
            "wire_joiner_final_export_history",
        )
        for field in removed_internal_summary_fields:
            self.assertNotIn(field, internal_history)

        identity_pairs = (
            ("kind", "result_wire_producer_kind"),
            ("state", "result_wire_producer_state"),
            ("blocker", "result_wire_producer_blocker"),
            ("source_edge_info_index", "result_wire_producer_source_edge_info_index"),
            ("root_edge_info_index", "result_wire_producer_root_edge_info_index"),
            ("current_member_edge_info_index", "result_wire_producer_current_member_edge_info_index"),
            ("child_wire_info_index", "result_wire_producer_child_wire_info_index"),
        )
        passthrough_keys = (
            "open_export_index",
            "edge_info_index",
            "helper_open_export_override",
            "helper_open_export_override_reason",
            "source_edge_indices",
        )
        for runtime_entry, internal_entry in zip(runtime_entries, internal_entries):
            with self.subTest(open_export_index=runtime_entry["open_export_index"]):
                for key in passthrough_keys:
                    self.assertEqual(internal_entry[key], runtime_entry[key])
                for _, history_key in identity_pairs:
                    self.assertIn(history_key, internal_entry)
                    self.assertEqual(internal_entry[history_key], runtime_entry[history_key])
                if internal_entry["helper_open_export_override"]:
                    self.assertIn(
                        internal_entry["result_wire_producer_state"],
                        {
                            "LegacyHelperCandidate",
                            "ProducerLocated",
                            "AHistoryEvidenceReady",
                            "ChildWireReady",
                            "SourceShapeReady",
                            "ExportedWithoutHelper",
                        },
                    )
                    if internal_entry["result_wire_producer_kind"] == "None":
                        self.assertNotEqual(internal_entry["result_wire_producer_blocker"], "None")

        internal_by_open_export = {
            entry["open_export_index"]: entry
            for entry in internal_entries
            if entry["helper_open_export_override"]
        }
        self.assertEqual(len(internal_by_open_export), len(producer_entries))
        for producer_entry in producer_entries:
            with self.subTest(open_export_index=producer_entry["open_export_index"]):
                internal_entry = internal_by_open_export[producer_entry["open_export_index"]]
                for producer_key, history_key in identity_pairs:
                    self.assertEqual(internal_entry[history_key], producer_entry[producer_key])

    def assert_exhaust_adjacent_search_ledger(self, ledger: dict[str, int]) -> None:
        self.assertEqual(
            ledger["exhaust_adjacent_search_count"],
            ledger["exhaust_adjacent_search_hit_count"] + ledger["exhaust_adjacent_search_miss_count"],
        )
        self.assertGreaterEqual(
            ledger["exhaust_adjacent_wire_set_insert_count"],
            ledger["exhaust_adjacent_wire_set_erase_count"],
        )
        self.assertGreaterEqual(
            ledger["exhaust_adjacent_search_edge_set_visit_count"],
            ledger["exhaust_adjacent_search_hit_count"],
        )
        self.assertGreaterEqual(
            ledger["exhaust_adjacent_search_vertex_stack_count"],
            ledger["exhaust_adjacent_search_backtrack_count"],
        )
        if ledger["exhaust_adjacent_search_count"] == 0:
            self.assertEqual(ledger["exhaust_adjacent_search_hit_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_search_miss_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_search_stack_frame_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_search_vertex_stack_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_search_edge_set_visit_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_search_backtrack_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_wire_set_insert_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_wire_set_erase_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_wire_set_abort_count"], 0)
            self.assertEqual(ledger["exhaust_adjacent_wire_info2_abort_count"], 0)
            return

        self.assertGreater(ledger["exhaust_adjacent_search_stack_frame_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_search_edge_set_visit_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_wire_set_insert_count"], 0)
        if ledger["exhaust_adjacent_search_hit_count"] > 0:
            self.assertGreater(ledger["exhaust_search_candidate_edge_info_count"], 0)

    def assert_open_wire_compound_ledger(self, ledger: dict[str, int]) -> None:
        if ledger["open_export_edge_info_count"] == 0:
            self.assertEqual(ledger["open_wire_compound_wire_info_count"], 0)
            self.assertEqual(ledger["open_wire_compound_built_wire_info_count"], 0)
            self.assertEqual(ledger["open_wire_compound_edge_info_count"], 0)
            self.assertEqual(ledger["open_wire_compound_super_edge_wire_info_count"], 0)
            self.assertEqual(ledger["open_wire_compound_purge_bridge_wire_info_count"], 0)
            self.assertEqual(ledger["open_wire_compound_source_shared_vertex_wire_info_count"], 0)
            self.assertEqual(
                ledger["open_wire_compound_purge_bridge_source_shared_vertex_wire_info_count"],
                0,
            )
            self.assertEqual(ledger["open_wire_compound_purge_bridge_unmatched_wire_info_count"], 0)
            return

        self.assertEqual(
            ledger["open_wire_compound_wire_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_built_wire_info_count"],
            ledger["open_wire_compound_wire_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_purge_bridge_wire_info_count"],
            ledger["source_identity_purge_bridge_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["open_wire_compound_source_shared_vertex_wire_info_count"],
            ledger["open_wire_compound_wire_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_purge_bridge_source_shared_vertex_wire_info_count"]
            + ledger["open_wire_compound_purge_bridge_unmatched_wire_info_count"],
            ledger["open_wire_compound_purge_bridge_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["open_wire_compound_super_edge_wire_info_count"],
            ledger["open_wire_compound_wire_info_count"],
        )

    def assert_open_export_history_entries(self, history: dict[str, object]) -> list[dict[str, object]]:
        entries = history["open_export_history_entries"]
        self.assertIsInstance(entries, list)
        self.assertNotIn("open_export_generated_edge_count", history)
        removed_history_summary_fields = (
            "open_export_edge_count",
            "open_export_source_lineage_edge_count",
            "open_export_missing_source_lineage_edge_count",
            "open_export_helper_override_edge_count",
            "open_export_helper_override_missing_source_lineage_edge_count",
            "open_export_purge_bridge_edge_count",
            "final_export_history",
        )
        for field in removed_history_summary_fields:
            self.assertNotIn(field, history)
        for entry in entries:
            self.assertNotIn("generated_open_export", entry)
            self.assertNotIn("generated_open_export_reason", entry)
        self.assertTrue(
            all(
                bool(entry["helper_open_export_override_reason"]) == entry["helper_open_export_override"]
                for entry in entries
            )
        )
        self.assertEqual(
            [entry["open_export_index"] for entry in entries],
            list(range(1, len(entries) + 1)),
        )
        return entries

    @staticmethod
    def open_export_entry_counts(entries: list[dict[str, object]]) -> dict[str, int]:
        return {
            "source_lineage": sum(1 for entry in entries if entry["source_edge_indices"]),
            "missing_source_lineage": sum(1 for entry in entries if not entry["source_edge_indices"]),
            "helper_override": sum(1 for entry in entries if entry["helper_open_export_override"]),
            "helper_override_missing_source_lineage": sum(
                1
                for entry in entries
                if entry["helper_open_export_override"] and not entry["source_edge_indices"]
            ),
            "purge_bridge": sum(1 for entry in entries if entry["purge_bridge"]),
        }

    def assert_repeated_split_exhaust_removal_ledger(self, ledger: dict[str, int]) -> None:
        rerun_keys = (
            "repeated_split_exhaust_rerun_active_edge_info_count",
            "repeated_split_exhaust_rerun_owned_active_edge_info_count",
            "repeated_split_exhaust_rerun_reset_primary_edge_info_count",
            "repeated_split_exhaust_rerun_reset_secondary_edge_info_count",
            "repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count",
            "repeated_split_exhaust_rerun_closed_wire_search_count",
            "repeated_split_exhaust_rerun_closed_wire_miss_count",
            "repeated_split_exhaust_rerun_miss_live_reset_edge_info_count",
            "repeated_split_exhaust_rerun_closed_wire_info_count",
            "repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count",
            "repeated_split_exhaust_rerun_closed_wire_vertex_count",
            "repeated_split_exhaust_rerun_resettable_closed_wire_info_count",
            "repeated_split_exhaust_rerun_resettable_assigned_edge_info_count",
            "repeated_split_exhaust_rerun_live_reset_primary_edge_info_count",
            "repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count",
            "repeated_split_exhaust_rerun_live_closed_wire_info_count",
            "repeated_split_exhaust_rerun_live_assigned_edge_info_count",
            "repeated_split_exhaust_rerun_live_closed_wire_vertex_count",
            "repeated_split_exhaust_rerun_live_branch_search_candidate_count",
            "repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count",
            "repeated_split_exhaust_rerun_live_done_wire_info_count",
            "repeated_split_exhaust_rerun_removal_scan_count",
            "repeated_split_exhaust_rerun_loop_exit_no_removal_count",
            "repeated_split_exhaust_rerun_branch_search_candidate_count",
            "repeated_split_exhaust_rerun_branch_search_inside_candidate_count",
            "repeated_split_exhaust_rerun_new_wire_seed_candidate_count",
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_removed_edge_info_count"],
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"]
            + ledger["repeated_split_exhaust_removed_secondary_edge_info_count"]
            + ledger["repeated_split_exhaust_removed_primary_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_owned_active_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_reset_primary_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_closed_wire_search_count"],
            ledger["repeated_split_exhaust_rerun_active_edge_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_closed_wire_search_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_info_count"]
            + ledger["repeated_split_exhaust_rerun_closed_wire_miss_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_miss_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_vertex_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_resettable_closed_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_reset_primary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_reset_secondary_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_assigned_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_closed_wire_vertex_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_vertex_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_inside_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_loop_exit_no_removal_count"],
            ledger["repeated_split_exhaust_rerun_removal_scan_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_inside_candidate_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_new_wire_seed_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_inside_candidate_count"],
        )
        if ledger["repeated_split_exhaust_rerun_closed_wire_info_count"] == 0:
            self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_assigned_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_vertex_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_resettable_closed_wire_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_reset_primary_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_reset_secondary_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_assigned_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_closed_wire_vertex_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_branch_search_candidate_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_done_wire_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_scan_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_loop_exit_no_removal_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"], 0)
        if ledger["repeated_split_exhaust_cycle_count"] == 0:
            self.assertEqual(ledger["repeated_split_exhaust_removed_edge_info_count"], 0)
            for key in rerun_keys:
                self.assertEqual(ledger[key], 0)
            return

        self.assertGreater(ledger["repeated_split_exhaust_removed_edge_info_count"], 0)
        self.assertLessEqual(
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"],
            ledger["repeated_split_exhaust_removed_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_removed_secondary_edge_info_count"]
            + ledger["repeated_split_exhaust_removed_primary_edge_info_count"],
            ledger["repeated_split_exhaust_removed_edge_info_count"],
        )

    def assert_helper_open_export_override_reason_ledger(
        self,
        sketch: dict[str, object],
        expected_counts: dict[str, int],
    ) -> list[dict[str, object]]:
        ledger = sketch["wire_joiner_ledger"]
        history = sketch["wire_joiner_history_detail"]
        entries = self.assert_open_export_history_entries(history)
        helper_entries = [entry for entry in entries if entry["helper_open_export_override"]]
        reason_counts = Counter(entry["helper_open_export_override_reason"] for entry in helper_entries)
        entry_counts = self.open_export_entry_counts(entries)

        self.assertEqual(reason_counts, Counter(expected_counts))
        self.assertEqual(sum(expected_counts.values()), len(helper_entries))
        self.assertEqual(sum(expected_counts.values()), len(ledger["result_wire_producer_ledger_entries"]))
        self.assertEqual(
            entry_counts["helper_override_missing_source_lineage"],
            ledger["source_lineage_missing_open_export_edge_info_count"],
        )
        self.assert_exhaust_adjacent_search_ledger(ledger)
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(
            ledger["owner_propagation_candidate_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertEqual(
            ledger["owner_propagation_other_wire_live_edge_info_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_generated_identity_blocked_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"],
        )
        if ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"] == 0:
            self.assertEqual(ledger["repeated_split_exhaust_generated_identity_blocked_edge_info_count"], 0)
        if ledger["repeated_split_exhaust_generated_identity_blocked_edge_info_count"] > 0:
            self.assertGreater(len(ledger["result_wire_producer_ledger_entries"]), 0)
            self.assertGreater(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertGreaterEqual(
            history["deleted_history_count"],
            ledger["repeated_split_exhaust_removed_edge_info_count"],
        )
        self.assert_result_wire_producer_ledger(sketch)
        return entries

    @staticmethod
    def result_wire_producer_kind_count(ledger: dict[str, object], kind: str) -> int:
        return sum(
            1
            for entry in ledger["result_wire_producer_ledger_entries"]
            if entry["kind"] == kind
        )

    def assert_result_wire_producer_ledger(self, sketch: dict[str, object]) -> None:
        ledger = sketch["wire_joiner_ledger"]
        history = sketch["wire_joiner_history_detail"]
        producer_entries = ledger["result_wire_producer_ledger_entries"]

        self.assertEqual(
            len(producer_entries),
            self.open_export_entry_counts(history["open_export_history_entries"])["helper_override"],
        )
        valid_kinds = {
            "None",
            "ExistingSourceEdge",
            "PartialSharedClosedWire",
            "LiveResetOpenEdge",
            "SuperEdgeRoot",
            "CurrentMemberChildWire",
        }
        valid_states = {
            "LegacyHelperCandidate",
            "ProducerLocated",
            "AHistoryEvidenceReady",
            "ChildWireReady",
            "SourceShapeReady",
            "ExportedWithoutHelper",
        }
        valid_blockers = {
            "None",
            "MissingSourceLineage",
            "MissingAHistoryRemoveSource",
            "ForeignAHistorySourceLineage",
            "ForeignAHistorySourceShapeReadyLineageMismatch",
            "ForeignAHistorySourceShapeIdentityNotReady",
            "ForeignAHistorySourceGeometryMismatch",
            "MissingRemovedTargetEvidence",
            "MissingFullAHistoryProducerEvidence",
            "FinalGateBlockedByIteration",
            "FinalGateBlockedByWireInfo",
            "RootRemovedByUnownedBranch",
            "RootRemovedByPrimaryBranch",
            "RootRemovedBySecondaryBranch",
            "MultiMemberRootPendingSuppression",
            "SourceShapeIdentityNotReady",
            "SourceShapeWouldPurgeOriginal",
            "LiveResetSourceShapeWouldPurgeOriginal",
            "CurrentMemberSourceShapeWouldPurgeOriginal",
            "SameSourceSidecarSourceShapeIdentityNotReady",
            "SameSourceSidecarGeometryMismatch",
            "SourceShapeMemberVertexIdentityNotReady",
            "CurrentMemberChildWireIdentityNotReady",
            "CurrentMemberMissingSidecarEvidence",
            "CurrentMemberRootOpenProducerNotReady",
            "CurrentMemberSidecarGeometryMismatch",
            "LegacyHelperShapeStillUsed",
            "UnknownInvariant",
        }
        for entry in producer_entries:
            self.assertIn(entry["kind"], valid_kinds)
            self.assertIn(entry["state"], valid_states)
            self.assertIn(entry["blocker"], valid_blockers)
            if entry["kind"] == "None":
                self.assertNotEqual(entry["blocker"], "None")

        open_export_entries = self.assert_open_export_history_entries(history)
        helper_entries = [entry for entry in open_export_entries if entry["helper_open_export_override"]]
        self.assertEqual(len(helper_entries), len(producer_entries))
        self.assertEqual(
            [entry["open_export_index"] for entry in producer_entries],
            [entry["open_export_index"] for entry in helper_entries],
        )
        for producer_entry, history_entry in zip(producer_entries, helper_entries):
            self.assertEqual(producer_entry["kind"], history_entry["result_wire_producer_kind"])
            self.assertEqual(producer_entry["state"], history_entry["result_wire_producer_state"])
            self.assertEqual(producer_entry["blocker"], history_entry["result_wire_producer_blocker"])
            self.assertEqual(producer_entry["state"], "ExportedWithoutHelper")
            self.assertEqual(producer_entry["blocker"], "None")
            self.assertEqual(
                producer_entry["child_wire_info_index"],
                producer_entry["open_export_index"] - 1,
            )
            self.assertEqual(
                producer_entry["child_wire_info_index"],
                history_entry["result_wire_producer_child_wire_info_index"],
            )
            self.assertEqual(
                producer_entry["source_edge_info_index"],
                history_entry["result_wire_producer_source_edge_info_index"],
            )
            self.assertEqual(
                producer_entry["root_edge_info_index"],
                history_entry["result_wire_producer_root_edge_info_index"],
            )
            self.assertEqual(
                producer_entry["current_member_edge_info_index"],
                history_entry["result_wire_producer_current_member_edge_info_index"],
            )

        self.assertEqual(
            sum(1 for entry in helper_entries if entry["result_wire_producer_kind"] == "LiveResetOpenEdge"),
            self.result_wire_producer_kind_count(ledger, "LiveResetOpenEdge"),
        )
    def test_p5_coincident_constraints_merge_profile_endpoints(self) -> None:
        result = self.run_recompute("sketch-coincident-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-coincident-profile")

    def test_p5_horizontal_vertical_constraints_accept_satisfied_lines(self) -> None:
        result = self.run_recompute("sketch-horizontal-vertical-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 4)
        self.assert_object_matches_expected(result, "p5", "sketch-horizontal-vertical-profile")

    def test_p5_dimension_constraints_accept_satisfied_datums(self) -> None:
        result = self.run_recompute("sketch-dimensional-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 4)
        self.assert_object_matches_expected(result, "p5", "sketch-dimensional-constraints-profile")

    def test_p5_diameter_constraints_accept_satisfied_datums(self) -> None:
        result = self.run_recompute("sketch-diameter-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-diameter-constraints-profile")

    def test_p5_point_pair_constraints_accept_satisfied_datums(self) -> None:
        result = self.run_recompute("sketch-point-pair-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 2)
        self.assertEqual(sketch["dimension_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-point-pair-constraints-profile")

    def test_p5_coordinate_constraints_accept_satisfied_line_end_datums(self) -> None:
        result = self.run_recompute("sketch-coordinate-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 4)
        self.assert_object_matches_expected(result, "p5", "sketch-coordinate-constraints-profile")

    def test_p5_line_relation_constraints_accept_satisfied_lines(self) -> None:
        result = self.run_recompute("sketch-line-relation-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-line-relation-constraints-profile")

    def test_p5_tangent_constraints_accept_satisfied_direct_and_pointwise_tangency(self) -> None:
        result = self.run_recompute("sketch-tangent-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 6)
        self.assertEqual(sketch["block_constraints_applied"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-tangent-constraints-profile")

    def test_p5_perpendicular_constraints_accept_satisfied_pointwise_relations(self) -> None:
        result = self.run_recompute("sketch-perpendicular-pointwise-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 4)
        self.assertEqual(sketch["block_constraints_applied"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-perpendicular-pointwise-constraints-profile")

    def test_p5_perpendicular_constraints_accept_satisfied_curve_midpoints(self) -> None:
        result = self.run_recompute("sketch-perpendicular-curve-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assertEqual(sketch["block_constraints_applied"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-perpendicular-curve-constraints-profile")

    def test_p5_point_on_object_constraints_accept_satisfied_points(self) -> None:
        result = self.run_recompute("sketch-point-on-object-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 3)
        self.assert_object_matches_expected(result, "p5", "sketch-point-on-object-constraints-profile")

    def test_p5_symmetric_constraints_accept_satisfied_points(self) -> None:
        result = self.run_recompute("sketch-symmetric-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-symmetric-constraints-profile")

    def test_p5_block_constraints_accept_supported_geometry(self) -> None:
        result = self.run_recompute("sketch-block-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 0)
        self.assertEqual(sketch["block_constraints_applied"], 3)
        self.assert_object_matches_expected(result, "p5", "sketch-block-constraints-profile")

    def test_p5_equal_constraints_accept_satisfied_geometries(self) -> None:
        result = self.run_recompute("sketch-equal-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-equal-constraints-profile")

    def test_p5_angle_constraints_accept_satisfied_line_pair_datums(self) -> None:
        result = self.run_recompute("sketch-angle-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-angle-constraints-profile")

    def test_p5_angle_constraints_accept_satisfied_pointwise_datums(self) -> None:
        result = self.run_recompute("sketch-angle-pointwise-constraints-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 0)
        self.assertEqual(sketch["orientation_constraints_applied"], 0)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 3)
        self.assertEqual(sketch["block_constraints_applied"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-angle-pointwise-constraints-profile")

    def test_p5_sketch_rejects_constraint_that_requires_solver_movement(self) -> None:
        result = self.run_recompute("sketch-solver-movement-rejected", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_property"])
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertIn("requires solver movement", diagnostic["message"])
        self.assertEqual(result["objects"]["Sketch"]["status"], "error")

    def test_c3m3_sketch_conflicting_constraints_block_profile_output(self) -> None:
        result = self.run_recompute("sketch-conflicting-constraints", "c3m3")
        diagnostic = result["diagnostics"][0]
        sketch = result["objects"]["Sketch"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["sketch_solver_conflict"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver")
        self.assertEqual(diagnostic["target"], "Constraints[1,2]")
        self.assertEqual(sketch["status"], "error")
        self.assertEqual(sketch["profile"], "none")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "conflict")
        self.assertEqual(sketch["solver_conflicting_constraints"], [1, 2])
        self.assertEqual(sketch["solver_redundant_constraints"], [])

    def test_c3m3_sketch_redundant_constraints_block_profile_output(self) -> None:
        result = self.run_recompute("sketch-redundant-constraints", "c3m3")
        diagnostic = result["diagnostics"][0]
        sketch = result["objects"]["Sketch"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["sketch_solver_redundant"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver")
        self.assertEqual(diagnostic["target"], "Constraints[1,2]")
        self.assertEqual(sketch["status"], "error")
        self.assertEqual(sketch["profile"], "none")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "redundant")
        self.assertEqual(sketch["solver_conflicting_constraints"], [])
        self.assertEqual(sketch["solver_redundant_constraints"], [1, 2])

    def test_c3m3_sketch_malformed_constraints_block_profile_output(self) -> None:
        result = self.run_recompute("sketch-malformed-constraints", "c3m3")
        diagnostic = result["diagnostics"][0]
        sketch = result["objects"]["Sketch"]

        self.assertEqual(
            [item["code"] for item in result["diagnostics"]],
            ["sketch_solver_malformed_constraint"],
        )
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver")
        self.assertEqual(diagnostic["target"], "Constraints[1,2]")
        self.assertEqual(sketch["status"], "error")
        self.assertEqual(sketch["profile"], "none")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "malformed")
        self.assertEqual(sketch["solver_malformed_constraints"], [1, 2])
        self.assertEqual(sketch["solver_conflicting_constraints"], [])
        self.assertEqual(sketch["solver_redundant_constraints"], [])

    def test_p5_circle_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-circle-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 1)
        self.assert_object_matches_expected(result, "p5", "sketch-circle-profile")

    def test_p5_mixed_closed_wires_make_profile_with_hole(self) -> None:
        result = self.run_recompute("sketch-rect-circle-hole", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 5)
        generated_face_sources = [
            set(item["sources"])
            for item in result["named_shapes"]["Sketch.InternalShape"]["history"]
            if item["kind"] == "generated" and item["element"].startswith("InternalFace")
        ]
        self.assertIn({"Edge1", "Edge2", "Edge3", "Edge4"}, generated_face_sources)
        self.assertIn({"Edge5"}, generated_face_sources)
        self.assertNotIn({"Edge1", "Edge2", "Edge3", "Edge4", "Edge5"}, generated_face_sources)
        self.assert_object_matches_expected(result, "p5", "sketch-rect-circle-hole")

    def test_p5_nested_closed_wires_keep_island_face(self) -> None:
        result = self.run_recompute("sketch-rect-circle-island", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 6)
        self.assert_object_matches_expected(result, "p5", "sketch-rect-circle-island")

    def test_p5_arc_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-arc-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-arc-profile")

    def test_p5_arc_ellipse_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-arc-ellipse-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-arc-ellipse-profile")

    def test_p5_ellipse_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-ellipse-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 1)
        self.assert_object_matches_expected(result, "p5", "sketch-ellipse-profile")

    def test_p5_bspline_profile_builds_internal_shape(self) -> None:
        result = self.run_recompute("sketch-bspline-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["profile"], "occt_face")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["raw_edge_count"], 2)
        self.assertEqual(sketch["internal_shape"], "occt_internal_shape")
        self.assert_object_matches_expected(result, "p5", "sketch-bspline-profile")

    def test_p5_construction_geometry_is_ignored_for_profile(self) -> None:
        result = self.run_recompute("sketch-construction-ignored", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 1)
        self.assert_object_matches_expected(result, "p5", "sketch-construction-ignored")

    def test_p5_external_edge_projects_as_construction_geometry(self) -> None:
        result = self.run_recompute("sketch-external-edge", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assert_object_matches_expected(result, "p5", "sketch-external-edge")

    def test_p5_external_vertex_projects_as_construction_geometry(self) -> None:
        result = self.run_recompute("sketch-external-vertex", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_point_count"], 1)
        self.assert_object_matches_expected(result, "p5", "sketch-external-vertex")

    def test_p5_external_curve_edges_project_as_construction_geometry(self) -> None:
        for fixture in ["sketch-external-circle-edge", "sketch-external-ellipse-edge"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 4)
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], 1)
                self.assert_object_matches_expected(result, "p5", fixture)

    def test_p5_external_face_projects_boundary_as_construction_geometry(self) -> None:
        for fixture, expected_count in [
            ("sketch-external-face", 4),
            ("sketch-external-face-normal", 1),
            ("sketch-external-face-intersection", 1),
            ("sketch-external-face-both", 2),
            ("sketch-external-whole-box", 12),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 4)
                self.assertEqual(sketch["external_geometry_count"], expected_count)
                self.assertEqual(sketch["external_curve_count"], 0)
                self.assertEqual(sketch["external_point_count"], 0)
                self.assert_object_matches_expected(result, "p5", fixture)

    def test_p5_external_geometry_defining_participates_in_profile(self) -> None:
        result = self.run_payload(self.external_geometry_state_payload(["Defining"]))
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["profile"], "occt_face")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["raw_edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["defining"], 1)

    def test_p5_external_geometry_frozen_and_detached_do_not_follow_source(self) -> None:
        for flags, state in [(["Frozen"], "frozen"), (["Detached"], "detached")]:
            with self.subTest(flags=flags):
                result = self.run_payload(self.external_geometry_state_payload(flags))
                sketch = result["objects"]["Sketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertFalse(sketch["profile_ready"])
                self.assertEqual(sketch["edge_count"], 0)
                self.assertEqual(sketch["external_geometry_count"], 0)
                self.assertEqual(sketch["external_geometry_state_counts"][state], 1)

    def test_p5_external_geometry_detached_removes_link_from_request_graph(self) -> None:
        result = self.run_payload(self.external_geometry_state_payload(["Detached", "Missing"]))
        sketch = result["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["external_geometry_count"], 0)
        self.assertEqual(sketch["external_geometry_state_counts"]["detached"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_detach"])
        self.assertEqual(updates[0]["object"], "Sketch")
        self.assertEqual(updates[0]["properties"]["ExternalGeometry"]["SubSet"], [])

    def test_p5_external_geometry_sync_refreshes_and_clears_sync_flag(self) -> None:
        result = self.run_payload(self.external_geometry_state_payload(["Frozen", "Sync"]))
        sketch = result["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["frozen"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["sync"], 1)
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_flags_sync"])
        sub_set = updates[0]["properties"]["ExternalGeometry"]["SubSet"]
        self.assertEqual(sub_set[0]["ExternalFlags"], ["Frozen"])

    def test_p5_external_geometry_missing_recovery_clears_missing_flag(self) -> None:
        result = self.run_payload(self.external_geometry_state_payload(["Missing"]))
        sketch = result["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["recovered_missing"], 1)
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_flags_sync"])
        sub_set = updates[0]["properties"]["ExternalGeometry"]["SubSet"]
        self.assertNotIn("ExternalFlags", sub_set[0])

    def test_c3m2_external_geometry_missing_recovery_fixture_clears_missing_flag(self) -> None:
        result = self.run_recompute("sketch-external-missing-fix", "c3m2")
        sketch = result["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["recovered_missing"], 1)
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_flags_sync"])
        sub_set = updates[0]["properties"]["ExternalGeometry"]["SubSet"]
        self.assertEqual(sub_set[0]["value"], "Box")
        self.assertEqual(sub_set[0]["SubList"], ["Face5"])
        self.assertNotIn("ExternalFlags", sub_set[0])

    def test_c3m2_external_geometry_frozen_brep_snapshot_reuses_old_subshape(self) -> None:
        result = self.run_recompute("sketch-external-frozen-brep-reuse", "c3m2")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["frozen"], 1)
        self.assertNotIn("missing_link_target", [item["code"] for item in result["diagnostics"]])

    def test_c3m2_external_geometry_frozen_missing_snapshot_reports_diagnostic(self) -> None:
        result = self.run_recompute("sketch-external-frozen-missing-snapshot", "c3m2")
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["missing_external_geometry_snapshot"])
        self.assertNotIn("missing_link_target", [item["code"] for item in diagnostics])
        self.assertEqual(diagnostics[0]["stage"], "graph")
        self.assertEqual(diagnostics[0]["object"], "Sketch")
        self.assertEqual(diagnostics[0]["property"], "ExternalGeometry")
        self.assertEqual(diagnostics[0]["target"], "MissingBox")
        self.assertEqual(diagnostics[0]["subname"], "Face5")

    def test_c3m2_external_geometry_missing_brep_snapshot_reuses_old_subshape(self) -> None:
        result = self.run_recompute("sketch-external-missing-brep-reuse", "c3m2")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["recovered_missing"], 0)
        self.assertNotIn("missing_link_target", [item["code"] for item in result["diagnostics"]])

    def test_c3m2_external_geometry_missing_without_snapshot_reports_diagnostic(self) -> None:
        result = self.run_recompute("sketch-external-missing-missing-snapshot", "c3m2")
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["missing_external_geometry_snapshot"])
        self.assertIn("Missing ExternalGeometry target MissingBox", diagnostics[0]["message"])
        self.assertNotIn("missing_link_target", [item["code"] for item in diagnostics])
        self.assertEqual(diagnostics[0]["stage"], "graph")
        self.assertEqual(diagnostics[0]["object"], "Sketch")
        self.assertEqual(diagnostics[0]["property"], "ExternalGeometry")
        self.assertEqual(diagnostics[0]["target"], "MissingBox")
        self.assertEqual(diagnostics[0]["subname"], "Face5")

    def test_c3m2_external_geometry_frozen_native_external_geo_reuses_old_geometry(self) -> None:
        result = self.run_payload(self.native_external_geo_state_payload(["Frozen"]))
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["frozen"], 1)

    def test_c3m2_external_geometry_missing_native_external_geo_keeps_missing_flag(self) -> None:
        result = self.run_payload(self.native_external_geo_state_payload(["Missing"]))
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["recovered_missing"], 0)

    def test_c3m2_external_geometry_detached_native_external_geo_clears_ref_update(self) -> None:
        result = self.run_payload(self.native_external_geo_state_payload(["Detached", "Missing"]))
        sketch = result["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["detached"], 1)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_detach"])
        properties = updates[0]["properties"]
        self.assertEqual(properties["ExternalGeometry"]["SubSet"], [])
        external_geo = properties["ExternalGeo"]["Geometry"]
        self.assertEqual(len(external_geo), 4)
        for item in external_geo:
            self.assertNotIn("Ref", item)
            self.assertNotIn("ExternalFlags", item)

    def test_p5_non_parallel_external_circle_edge_projection_variants(self) -> None:
        for fixture, expected_curve_count in {
            "sketch-external-circle-edge-as-line": 0,
            "sketch-external-tilted-circle-edge": 1,
            "sketch-external-tilted-ellipse-edge": 1,
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 4)
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], expected_curve_count)
                self.assertEqual(sketch["external_point_count"], 0)
                self.assert_object_matches_expected(result, "p5", fixture)

    def test_p5_closed_sketch_exports_internal_subshapes(self) -> None:
        result = self.run_recompute("sketch-internal-face", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face")

    def test_p5_sketch_exports_internal_edge_vertex_stable_subnames(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face", "p5")
        subshapes = {
            item["indexed"]: item
            for item in result["results"][0]["subshapes"]
        }

        self.assertEqual(subshapes["InternalEdge1"]["stableSubname"], "Edge1")
        self.assertEqual(subshapes["InternalVertex1"]["stableSubname"], "Vertex1")
        self.assertEqual(subshapes["InternalFace1"]["stableSubname"], "")

    def test_p5_sketch_internal_shape_named_shape_keeps_face_unstable(self) -> None:
        result = self.run_recompute("sketch-internal-face", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        self.assertIn("InternalFace1", named_shape["elements"])
        self.assertIn("InternalEdge1", named_shape["elements"])
        self.assertIn("InternalVertex1", named_shape["elements"])
        self.assertEqual(named_shape["elements"]["InternalFace1"]["status"], "generated")
        self.assertEqual(
            set(named_shape["elements"]["InternalFace1"]["sources"]),
            {"Edge1", "Edge2", "Edge3", "Edge4"},
        )
        self.assertEqual(named_shape["element_map"]["Edge1"], "InternalEdge1")
        self.assertEqual(named_shape["element_map"]["Vertex1"], "InternalVertex1")
        self.assertNotIn("Face1", named_shape["element_map"])
        generated_face_history = [
            item
            for item in named_shape["history"]
            if item["kind"] == "generated" and item["element"] == "InternalFace1"
        ]
        self.assertEqual(len(generated_face_history), 1)
        self.assertEqual(set(generated_face_history[0]["sources"]), {"Edge1", "Edge2", "Edge3", "Edge4"})

    def test_p5_sketch_internal_edge_stable_subname_checks_geometry(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-edge-arc-line-same-endpoints", "p5")
        subshapes = {
            item["indexed"]: item
            for item in result["results"][0]["subshapes"]
        }

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(subshapes["InternalEdge1"]["stableSubname"], "Edge1")
        self.assertEqual(subshapes["InternalEdge2"]["stableSubname"], "Edge2")
        self.assertEqual(subshapes["InternalFace1"]["stableSubname"], "")

    def test_p5_split_line_builds_multiple_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-split-line", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_existing_wire_search_ledger(ledger)
        self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-split-line")

    def test_p5_through_open_cutter_keeps_non_original_split_fragments(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]
        face_history = sketch["facemaker_history"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["facemaker_history_status"], "history_evidence:facemaker_buildface")
        self.assertEqual(face_history["source_edge_count"], 5)
        self.assertEqual(face_history["bounded_face_count"], 2)
        self.assertFalse(face_history["pre_split_history"])
        self.assertTrue(face_history["splitter_history"])
        self.assertEqual(sketch["wire_joiner_history"], "history_partial:edge_info_wire_info_split_done_exhaust")
        self.assertGreaterEqual(ledger["split_edge_info_count"], 2)
        self.assertGreater(ledger["tight_bound_exhaust_primary_reset_edge_info_count"], 0)
        self.assertEqual(ledger["secondary_owned_edge_info_count"], 0)
        self.assertEqual(
            ledger["closed_wire_assigned_edge_info_count"],
            ledger["primary_owned_edge_info_count"]
            + ledger["tight_bound_exhaust_primary_reset_edge_info_count"],
        )
        self.assertGreaterEqual(ledger["closed_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["closed_wire_vertex_count"], ledger["closed_wire_assigned_edge_info_count"])
        self.assert_closed_wire_stack_ledger(ledger)
        self.assertGreaterEqual(ledger["tight_bound_done_wire_info_count"], ledger["closed_wire_info_count"])
        self.assertEqual(ledger["tight_bound_new_wire_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["tight_bound_new_wire_vertex_count"], ledger["tight_bound_new_wire_candidate_count"] * 2)
        self.assertGreaterEqual(ledger["tight_bound_owner_transfer_candidate_edge_info_count"], 1)
        self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], ledger["tight_bound_split_wire_info_count"])
        self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], 0)
        self.assertGreater(ledger["tight_bound_full_wire_set_blocked_transfer_count"], 0)
        self.assertGreater(ledger["tight_bound_full_wire_set_abort_blocked_search_count"], 0)
        self.assertEqual(ledger["exhaust_search_candidate_edge_info_count"], 0)
        self.assertGreaterEqual(ledger["tight_bound_transfer_wire_vertex_count"], ledger["tight_bound_transfer_wire_info_count"] * 2)
        self.assertGreaterEqual(ledger["tight_bound_transferred_owner_edge_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_split_owner_wire_info_count"], 0)
        self.assertLessEqual(ledger["tight_bound_split_owner_wire_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_done_wire_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_split_owner_vertex_count"], 0)
        self.assertLess(ledger["tight_bound_split_owner_vertex_count"], ledger["closed_wire_vertex_count"])
        self.assertLessEqual(
            ledger["tight_bound_split_owner_built_wire_count"],
            ledger["tight_bound_split_owner_wire_info_count"],
        )
        self.assertEqual(ledger["tight_bound_split_wire_vertex_count"], 0)
        self.assertEqual(ledger["tight_bound_split_owner_vertex_count"], 0)
        self.assertEqual(
            ledger["tight_bound_split_wire_built_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assert_existing_wire_search_ledger(ledger)
        self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
        self.assert_exhaust_adjacent_search_ledger(ledger)
        self.assertGreater(ledger["tight_bound_full_wire_set_abort_resolved_by_hit_count"], 0)
        self.assertGreater(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertGreater(ledger["repeated_split_exhaust_removed_edge_info_count"], 0)
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(ledger["repeated_split_exhaust_generated_identity_blocked_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_active_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_no_active_search_count"], 1)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_search_count"], 0)
        self.assertGreaterEqual(ledger["open_export_edge_info_count"], 1)
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assert_open_wire_compound_ledger(ledger)
        self.assertGreater(ledger["source_identity_shared_vertex_edge_info_count"], 0)
        self.assertGreaterEqual(
            ledger["source_identity_shared_vertex_edge_info_count"],
            ledger["source_identity_only_source_vertices_edge_info_count"],
        )
        self.assertEqual(
            ledger["source_identity_open_export_shared_vertex_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(ledger["source_identity_purge_bridge_edge_info_count"], 0)
        self.assertGreater(ledger["source_lineage_split_edge_info_count"], 0)
        self.assertEqual(
            ledger["source_lineage_open_export_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        history = sketch["wire_joiner_history_detail"]
        entries = self.assert_open_export_history_entries(history)
        self.assertEqual(len(entries), ledger["open_export_edge_info_count"])
        entry_counts = self.open_export_entry_counts(entries)
        self.assertEqual(entry_counts["source_lineage"], ledger["source_lineage_open_export_edge_info_count"])
        self.assertEqual(entry_counts["missing_source_lineage"], 0)
        self.assertEqual(entry_counts["purge_bridge"], 0)
        self.assertTrue(all(entry["source_edge_indices"] for entry in entries))
        self.assertTrue(all(not entry["purge_bridge"] for entry in entries))
        self.assertEqual(ledger["source_lineage_multi_source_edge_info_count"], 0)
        self.assert_super_edge_lifecycle_ledger(ledger)
        self.assertGreaterEqual(ledger["ordered_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["ordered_vertex_count"], ledger["edge_info_count"])
        self.assertEqual(ledger["iteration2_marked_edge_info_count"], ledger["edge_info_count"])
        self.assertGreater(ledger["branch_search_candidate_count"], 0)
        self.assertGreaterEqual(ledger["branch_search_seed_wire_info_count"], 1)
        self.assertGreater(ledger["branch_search_inside_candidate_count"], 0)
        self.assertEqual(ledger["branch_search_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertGreaterEqual(ledger["new_wire_seed_wire_info_count"], 1)
        self.assertEqual(ledger["split_wire_candidate_count"], 0)
        self.assertEqual(ledger["split_wire_edge_info_count"], 0)
        self.assertGreaterEqual(ledger["done_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_owned_edge_info_count"], ledger["primary_owned_edge_info_count"])
        self.assertEqual(ledger["owner_propagation_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(ledger["tight_bound_exhaust_done_wire_info_count"], 0)
        self.assertEqual(
            ledger["tight_bound_exhaust_primary_reset_edge_info_count"],
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"],
        )
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(
            ledger["secondary_owned_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"],
            ledger["exhaust_secondary_owner_edge_info_count"],
        )
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_search_candidate_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_adjacent_search_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-through-open-cutter")

    def test_p5_internal_shape_records_split_history_for_open_cutter_fragments(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_history = named_shape["sketch_internal_history"]

        split_entries = [
            item
            for item in named_shape["history"]
            if item["kind"] == "split" and item["sources"] == ["Edge5"]
        ]
        self.assertEqual(named_shape["sketch_internal_history_status"], "history_evidence:facemaker_wirejoiner")
        self.assertIn("facemaker_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:modified", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:generated", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:deleted", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        mapper_history_stages = {event["maker_stage"] for event in named_shape["mapper_history"]}
        mapper_history_diagnostics = {
            event["diagnostic_status"] for event in named_shape["mapper_history"]
        }
        self.assertIn("facemaker:splitter", mapper_history_stages)
        self.assertIn("wire_joiner:open_export", mapper_history_stages)
        self.assertIn("summary_only:wire_joiner_history:open_export", mapper_history_diagnostics)
        self.assertEqual(internal_history["source_edge_count"], 5)
        self.assertEqual(internal_history["bounded_face_count"], 2)
        self.assertFalse(internal_history["pre_split_history"])
        self.assertTrue(internal_history["splitter_history"])
        self.assertEqual(internal_history["wire_joiner_source_edge_count"], 5)
        self.assertEqual(internal_history["wire_joiner_split_result_edge_count"], 9)
        self.assertGreater(internal_history["wire_joiner_modified_history_count"], 0)
        self.assertGreater(internal_history["wire_joiner_generated_history_count"], 0)
        self.assertGreater(internal_history["wire_joiner_deleted_history_count"], 0)
        internal_entries = internal_history["wire_joiner_open_export_history_entries"]
        self.assertIsInstance(internal_entries, list)
        self.assertGreater(len(internal_entries), 0)
        self.assertTrue(all(entry["source_edge_indices"] for entry in internal_entries))
        self.assertTrue(internal_history["wire_joiner_splitter_history"])
        self.assertNotIn("Edge5", named_shape["element_map"])
        self.assertGreaterEqual(len(split_entries), 2)
        for entry in split_entries:
            self.assertTrue(entry["element"].startswith("InternalEdge"))
            self.assertEqual(named_shape["elements"][entry["element"]]["status"], "split")

    def test_p5_c2_m2_facemaker_bounded_faces_emit_outer_boundary_evidence(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_history = named_shape["sketch_internal_history"]

        face_evidence = sketch["facemaker_history"]["bounded_face_evidence"]
        self.assertEqual(sketch["facemaker_history_status"], "history_evidence:facemaker_buildface")
        self.assertEqual(len(face_evidence), sketch["internal_face_count"])
        self.assertEqual(face_evidence, internal_history["facemaker_bounded_face_evidence"])
        self.assertTrue(all(entry["source_edge_indices"] for entry in face_evidence))
        self.assertTrue(all(entry["outer_boundary"] for entry in face_evidence))

        generated_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "facemaker:outer_boundary"
            and event["relation"] == "generated"
            and event["shape_kind"] == "face"
        ]
        self.assertGreaterEqual(len(generated_events), sketch["internal_face_count"])
        for event in generated_events:
            self.assertEqual(event["evidence"]["producer"], "FaceMakerBuildFace")
            self.assertTrue(event["target"]["subname"].startswith("InternalFace"))
            self.assertGreater(event["evidence"]["source_edge_index"], 0)
            self.assertTrue(event["evidence"]["outer_boundary_target_edge_indices"])

    def test_p5_c2_m4_summary_history_is_diagnostic_only(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        mapper_history = named_shape["mapper_history"]

        summary_events = [
            event
            for event in mapper_history
            if event["diagnostic_status"].startswith("summary_only:")
        ]
        self.assertTrue(summary_events)
        for event in summary_events:
            self.assertEqual(event["recoverability"], "diagnostic")
            self.assertEqual(event["source"], {"object": "Sketch.InternalShape", "subname": ""})
            self.assertEqual(event["target"], {"object": "Sketch.InternalShape", "subname": ""})

        producer_events = [
            event
            for event in mapper_history
            if event["evidence"].get("producer") in {"FaceMakerBuildFace", "WireJoiner"}
        ]
        self.assertTrue(producer_events)
        self.assertTrue(
            all(
                not event["diagnostic_status"].startswith("summary_only:")
                for event in producer_events
            )
        )
        self.assertTrue(
            any(
                event["relation"] == "split"
                and event["evidence"]["producer"] == "FaceMakerBuildFace"
                for event in producer_events
            )
        )
        self.assertTrue(
            any(
                event["relation"] in {"generated", "split"}
                and event["evidence"]["producer"] == "WireJoiner"
                for event in producer_events
            )
        )

    def test_p5_c2_m2_source_edge_one_to_many_split_uses_producer_evidence(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_map = sketch["internal_element_map"]

        edge5_evidence = [
            entry
            for entry in sketch["facemaker_history"]["edge_evidence"]
            if entry["source_edge_index"] == 5 and entry["relation"] == "split"
        ]
        self.assertGreaterEqual(len(edge5_evidence), 2)
        self.assertTrue(all(entry["maker_stage"] == "facemaker:splitter" for entry in edge5_evidence))
        self.assertNotIn("Edge5", internal_map)
        self.assertNotIn("Edge5", named_shape["element_map"])

        edge5_events = [
            event
            for event in named_shape["mapper_history"]
            if event["source"] == {"object": "Sketch", "subname": "Edge5"}
            and event["relation"] == "split"
            and event["shape_kind"] == "edge"
        ]
        self.assertGreaterEqual(len(edge5_events), 2)
        self.assertTrue(all(event["evidence"]["producer"] in {"FaceMakerBuildFace", "WireJoiner"} for event in edge5_events))

    def test_p5_c2_m2_deleted_no_original_purge_is_diagnostic_not_unique_map(self) -> None:
        result = self.run_recompute("sketch-internal-face-dangling-line", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_map = result["objects"]["Sketch"]["internal_element_map"]

        self.assertNotIn("Edge5", internal_map)
        self.assertNotIn("Vertex6", internal_map)
        self.assertNotIn("Edge5", named_shape["element_map"])
        self.assertNotIn("Vertex6", named_shape["element_map"])
        purge_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"].startswith("wire_joiner")
            and event["diagnostic_status"] == "no_original_purge"
        ]
        self.assertTrue(any(event["source"] == {"object": "Sketch", "subname": "Edge5"} for event in purge_events))
        self.assertTrue(any(event["source"] == {"object": "Sketch", "subname": "Vertex6"} for event in purge_events))

    def test_p5_c2_m2_wire_joiner_open_export_uses_producer_identity(self) -> None:
        result = self.run_recompute("sketch-internal-face-t-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        entries = sketch["wire_joiner_history_detail"]["open_export_history_entries"]
        producer_entries = sketch["wire_joiner_ledger"]["result_wire_producer_ledger_entries"]

        self.assertTrue(entries)
        self.assertEqual(len(entries), len(producer_entries))
        events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "wire_joiner:open_export"
            and event["shape_kind"] == "edge"
        ]
        self.assertTrue(events)
        self.assertTrue(any(event["relation"] == "generated" for event in events))
        self.assertTrue(any(event["relation"] == "split" for event in events))
        for event in events:
            self.assertEqual(event["evidence"]["producer"], "WireJoiner")
            self.assertIn("result_wire_producer_kind", event["evidence"])
            self.assertIn(event["evidence"]["result_wire_producer_state"], {
                "LegacyHelperCandidate",
                "ProducerLocated",
                "AHistoryEvidenceReady",
                "ChildWireReady",
                "SourceShapeReady",
                "ExportedWithoutHelper",
            })

    def test_p5_branch_open_cutter_keeps_connected_result_wire(self) -> None:
        result = self.run_recompute("sketch-internal-face-branch-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_existing_wire_search_ledger(ledger)
        self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-branch-open-cutter")

    def test_p5_cross_cutters_build_four_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-cross-cutters", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]
        producer_entries = ledger["result_wire_producer_ledger_entries"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertGreater(len(producer_entries), 0)
        self.assertEqual(
            len(producer_entries),
            ledger["open_export_edge_info_count"],
        )
        self.assert_open_wire_compound_ledger(ledger)
        self.assertGreater(ledger["source_lineage_open_export_edge_info_count"], 0)
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        self.assertEqual(
            ledger["source_lineage_open_export_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assert_super_edge_lifecycle_ledger(ledger)
        self.assert_closed_wire_stack_ledger(ledger)
        self.assertEqual(
            ledger["secondary_owned_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"],
            ledger["exhaust_secondary_owner_edge_info_count"],
        )
        self.assertGreater(ledger["tight_bound_split_wire_vertex_count"], 0)
        self.assertGreaterEqual(
            ledger["tight_bound_split_wire_vertex_count"],
            ledger["tight_bound_split_owner_vertex_count"],
        )
        self.assertEqual(
            ledger["tight_bound_split_wire_built_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assert_existing_wire_search_ledger(ledger)
        self.assertGreater(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertGreater(ledger["repeated_split_exhaust_removed_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_active_edge_info_count"], 1)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_owned_active_edge_info_count"], 1)
        self.assertGreater(
            ledger["repeated_split_exhaust_rerun_reset_primary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_owned_active_edge_info_count"],
        )
        self.assertGreater(ledger["repeated_split_exhaust_rerun_reset_secondary_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_search_count"], 1)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_miss_count"], 1)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count"], 0)
        self.assertGreater(ledger["tight_bound_full_wire_set_blocked_transfer_count"], 0)
        self.assertGreater(ledger["exhaust_search_candidate_edge_info_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_search_hit_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_wire_set_abort_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_wire_info2_abort_count"], 0)
        entries = self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"consumed_open_cutter_graph": 12},
        )
        self.assertTrue(
            all(entry["helper_open_export_override_reason"] == "consumed_open_cutter_graph" for entry in entries)
        )
        self.assertTrue(all(entry["source_edge_indices"] for entry in entries))
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 1)
        self.assertEqual(self.result_wire_producer_kind_count(ledger, "LiveResetOpenEdge"), 1)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cross-cutters")

    def test_p5_t_junction_cutter_records_result_wire_open_export(self) -> None:
        result = self.run_recompute("sketch-internal-face-t-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]
        history = sketch["wire_joiner_history_detail"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["wire_joiner_history"], "history_partial:edge_info_wire_info_split_done_exhaust")
        self.assertGreaterEqual(ledger["split_edge_info_count"], 2)
        self.assertGreaterEqual(ledger["primary_owned_edge_info_count"], 1)
        self.assertGreaterEqual(ledger["secondary_owned_edge_info_count"], 1)
        self.assertEqual(
            ledger["closed_wire_assigned_edge_info_count"],
            ledger["primary_owned_edge_info_count"]
            + ledger["tight_bound_exhaust_primary_reset_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"],
        )
        self.assertGreaterEqual(ledger["closed_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["closed_wire_vertex_count"], ledger["closed_wire_assigned_edge_info_count"])
        self.assert_closed_wire_stack_ledger(ledger)
        self.assertGreaterEqual(ledger["tight_bound_done_wire_info_count"], ledger["closed_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_split_wire_info_count"], 1)
        self.assertEqual(ledger["tight_bound_new_wire_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["tight_bound_new_wire_vertex_count"], ledger["tight_bound_new_wire_candidate_count"] * 2)
        self.assertGreaterEqual(ledger["tight_bound_owner_transfer_candidate_edge_info_count"], 1)
        self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], ledger["tight_bound_split_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_transfer_wire_vertex_count"], ledger["tight_bound_transfer_wire_info_count"] * 2)
        self.assertGreaterEqual(ledger["tight_bound_transferred_owner_edge_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreater(ledger["tight_bound_split_owner_wire_info_count"], 0)
        self.assertLessEqual(ledger["tight_bound_split_owner_wire_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreaterEqual(ledger["tight_bound_done_wire_info_count"], ledger["tight_bound_transfer_wire_info_count"])
        self.assertGreater(ledger["tight_bound_split_owner_vertex_count"], 0)
        self.assertLess(ledger["tight_bound_split_owner_vertex_count"], ledger["closed_wire_vertex_count"])
        self.assertLessEqual(
            ledger["tight_bound_split_owner_built_wire_count"],
            ledger["tight_bound_split_owner_wire_info_count"],
        )
        self.assertGreater(ledger["tight_bound_split_wire_vertex_count"], 0)
        self.assertGreaterEqual(
            ledger["tight_bound_split_wire_vertex_count"],
            ledger["tight_bound_split_owner_vertex_count"],
        )
        self.assertEqual(
            ledger["tight_bound_split_wire_built_count"],
            ledger["tight_bound_transfer_wire_info_count"],
        )
        self.assert_existing_wire_search_ledger(ledger)
        self.assertGreater(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertGreater(ledger["repeated_split_exhaust_removed_edge_info_count"], 0)
        self.assertEqual(ledger["owner_propagation_other_wire_live_edge_info_count"], 2)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_active_edge_info_count"], 2)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_owned_active_edge_info_count"], 2)
        self.assertGreater(
            ledger["repeated_split_exhaust_rerun_reset_primary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_owned_active_edge_info_count"],
        )
        self.assertGreater(ledger["repeated_split_exhaust_rerun_reset_secondary_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_search_count"], 2)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_miss_count"], 2)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_skipped_open_leaf_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_adjacent_search_hit_count"], 0)
        self.assertEqual(ledger["exhaust_adjacent_wire_set_abort_count"], 0)
        self.assertEqual(ledger["exhaust_adjacent_wire_info2_abort_count"], 0)
        self.assertGreater(ledger["open_export_edge_info_count"], 0)
        self.assert_open_wire_compound_ledger(ledger)
        entries = self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"partial_junction_open_cutter": 8},
        )
        self.assertEqual(len(entries), ledger["open_export_edge_info_count"])
        entry_counts = self.open_export_entry_counts(entries)
        self.assertEqual(entry_counts["helper_override"], len(ledger["result_wire_producer_ledger_entries"]))
        self.assertEqual(
            entry_counts["helper_override_missing_source_lineage"],
            ledger["source_lineage_missing_open_export_edge_info_count"],
        )
        self.assertGreater(entry_counts["source_lineage"], 0)
        self.assertEqual(entry_counts["missing_source_lineage"], 0)
        self.assertEqual(entry_counts["source_lineage"], len(entries))
        self.assertTrue(all(entry["source_edge_indices"] for entry in entries))
        self.assertTrue(all(entry["helper_open_export_override"] for entry in entries))
        self.assertTrue(
            all(entry["helper_open_export_override_reason"] == "partial_junction_open_cutter" for entry in entries)
        )
        self.assertTrue(all(not entry["purge_bridge"] for entry in entries))
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_history = named_shape["sketch_internal_history"]
        internal_entries = internal_history["wire_joiner_open_export_history_entries"]
        self.assertEqual(
            [entry["helper_open_export_override_reason"] for entry in internal_entries],
            [entry["helper_open_export_override_reason"] for entry in entries],
        )
        self.assertGreater(history["modified_history_count"], 0)
        self.assertGreater(history["deleted_history_count"], 0)
        self.assertTrue(history["splitter_history"])
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertGreater(len(ledger["result_wire_producer_ledger_entries"]), 0)
        self.assertEqual(
            len(ledger["result_wire_producer_ledger_entries"]),
            ledger["open_export_edge_info_count"],
        )
        self.assertGreater(ledger["source_identity_open_export_shared_vertex_edge_info_count"], 0)
        self.assertGreater(
            ledger["source_identity_open_export_shared_vertex_edge_info_count"],
            ledger["source_identity_open_export_only_source_vertices_edge_info_count"],
        )
        self.assertEqual(ledger["source_identity_purge_bridge_edge_info_count"], 0)
        self.assertLess(
            ledger["source_identity_purge_bridge_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertGreater(ledger["source_lineage_split_edge_info_count"], 0)
        self.assertGreater(ledger["source_lineage_open_export_edge_info_count"], 0)
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        self.assertEqual(
            ledger["source_lineage_open_export_edge_info_count"],
            len(ledger["result_wire_producer_ledger_entries"]),
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 2)
        self.assertEqual(self.result_wire_producer_kind_count(ledger, "LiveResetOpenEdge"), 2)
        self.assert_super_edge_lifecycle_ledger(ledger)
        self.assertGreaterEqual(ledger["ordered_wire_info_count"], 1)
        self.assertLessEqual(ledger["ordered_vertex_count"], ledger["edge_info_count"])
        self.assertLessEqual(ledger["iteration2_marked_edge_info_count"], ledger["edge_info_count"])
        self.assertLessEqual(len(ledger["result_wire_producer_ledger_entries"]), ledger["edge_info_count"])
        self.assertGreater(ledger["branch_search_candidate_count"], 0)
        self.assertGreaterEqual(ledger["branch_search_seed_wire_info_count"], 1)
        self.assertGreater(ledger["branch_search_inside_candidate_count"], 0)
        self.assertEqual(ledger["branch_search_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertGreaterEqual(ledger["new_wire_seed_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_owned_edge_info_count"], ledger["primary_owned_edge_info_count"])
        self.assertGreaterEqual(ledger["split_wire_candidate_count"], 1)
        self.assertGreater(ledger["split_wire_edge_info_count"], 0)
        self.assertEqual(
            ledger["owner_propagation_candidate_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertGreater(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(
            ledger["owner_propagation_other_wire_live_edge_info_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertGreater(ledger["tight_bound_exhaust_done_wire_info_count"], 0)
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(ledger["secondary_owned_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertGreater(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_search_candidate_edge_info_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-t-cutter")

    def test_p5_internal_branch_cutter_splits_generated_boundary_endpoint(self) -> None:
        result = self.run_recompute("sketch-internal-face-internal-branch-cutter", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-internal-branch-cutter")

    def test_p5_segmented_cross_cutter_preserves_edgeinfo_ownership(self) -> None:
        result = self.run_recompute("sketch-internal-face-segmented-cross-cutter", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        entries = self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"partial_junction_open_cutter": 10},
        )
        ledger = sketch["wire_joiner_ledger"]
        self.assertTrue(all(entry["source_edge_indices"] for entry in entries))
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 1)
        self.assertEqual(self.result_wire_producer_kind_count(ledger, "LiveResetOpenEdge"), 1)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-segmented-cross-cutter")

    def test_p5_overlapping_closed_profiles_split_into_disjoint_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-overlap-rectangles", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(
            ledger["owner_propagation_other_wire_live_edge_info_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertEqual(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_resettable_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_scan_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-overlap-rectangles")

    def test_p5_adjacent_closed_profiles_share_boundary_edge(self) -> None:
        result = self.run_recompute("sketch-internal-face-adjacent-rectangles", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_existing_wire_search_ledger(ledger)
        self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-adjacent-rectangles")

    def test_p5_overlapping_circles_split_into_lens_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-overlap-circles", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(
            ledger["owner_propagation_other_wire_live_edge_info_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertEqual(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_resettable_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"], 0)
        self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_scan_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-overlap-circles")

    def test_p5_three_overlapping_circles_split_into_venn_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-three-overlap-circles", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"closed_wire_cycle": 15},
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 3)
        self.assertEqual(self.result_wire_producer_kind_count(ledger, "LiveResetOpenEdge"), 3)
        self.assertGreater(ledger["exhaust_adjacent_search_miss_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_search_hit_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_search_backtrack_count"], 0)
        self.assertGreater(ledger["exhaust_adjacent_wire_info2_abort_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-three-overlap-circles")

    def test_p5_arc_lens_closed_profiles_keep_partial_shared_result_edge(self) -> None:
        result = self.run_recompute("sketch-internal-face-arc-lens", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]
        history = sketch["wire_joiner_history_detail"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        entries = self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"partial_shared_closed_wire": 1},
        )
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        self.assertEqual(
            self.open_export_entry_counts(history["open_export_history_entries"])[
                "helper_override_missing_source_lineage"
            ],
            0,
        )
        self.assert_existing_wire_search_ledger(ledger)
        self.assert_existing_wire_search_only_order_blocked_ledger(ledger)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-arc-lens")

    def test_p5_line_arc_same_endpoints_use_builderface_split_regions(self) -> None:
        result = self.run_recompute("sketch-internal-face-line-arc-same-endpoints", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_reset_primary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_done_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
        )
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-line-arc-same-endpoints")

    def test_p5_self_intersecting_single_wire_splits_into_bounded_regions(self) -> None:
        result = self.run_recompute("sketch-internal-face-bowtie-lines", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-bowtie-lines")

    def test_p5_self_intersecting_bspline_splits_into_bounded_regions(self) -> None:
        result = self.run_recompute("sketch-internal-face-figure8-bspline", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-figure8-bspline")

    def test_p5_overlapping_bspline_profiles_keep_empty_internal_shape(self) -> None:
        result = self.run_recompute("sketch-internal-face-overlap-bsplines-empty", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-overlap-bsplines-empty")

    def test_p5_self_intersecting_cubic_bspline_splits_into_bounded_regions(self) -> None:
        result = self.run_recompute("sketch-internal-face-cubic-figure8-bspline", "p5")
        sketch = result["objects"]["Sketch"]
        face_history = sketch["facemaker_history"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["facemaker_history_status"], "history_evidence:facemaker_buildface")
        self.assertEqual(face_history["source_edge_count"], 1)
        self.assertTrue(face_history["pre_split_history"])
        self.assertFalse(face_history["splitter_history"])
        self.assertEqual(face_history["bounded_face_count"], sketch["internal_face_count"])
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cubic-figure8-bspline")

    def test_p5_self_intersecting_cubic_bspline_records_terminal_split_history(self) -> None:
        result = self.run_recompute("sketch-internal-face-cubic-figure8-bspline", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        internal_history = named_shape["sketch_internal_history"]

        split_entries = [
            item
            for item in named_shape["history"]
            if item["kind"] == "split" and item["sources"] == ["Edge1"]
        ]

        self.assertEqual(named_shape["sketch_internal_history_status"], "history_evidence:facemaker_wirejoiner")
        self.assertIn("facemaker_history:pre_split", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertEqual(internal_history["source_edge_count"], 1)
        self.assertTrue(internal_history["pre_split_history"])
        self.assertFalse(internal_history["splitter_history"])
        self.assertNotIn("Edge1", named_shape["element_map"])
        self.assertGreaterEqual(len(split_entries), 4)
        for entry in split_entries:
            self.assertTrue(entry["element"].startswith("InternalEdge"))
            self.assertEqual(named_shape["elements"][entry["element"]]["status"], "split")
        generated_face_sources = [
            item["sources"]
            for item in named_shape["history"]
            if item["kind"] == "generated" and item["element"].startswith("InternalFace")
        ]
        self.assertTrue(generated_face_sources)
        for sources in generated_face_sources:
            self.assertEqual(sources, ["Edge1"])

    def test_p5_cross_pattern_closed_profiles_split_into_five_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-cross-pattern", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_closed_wire_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_reset_primary_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_resettable_assigned_edge_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_live_done_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
        )
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cross-pattern")

    def test_p5_dangling_open_line_keeps_bounded_internal_face(self) -> None:
        result = self.run_recompute("sketch-internal-face-dangling-line", "p5")
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["wire_joiner_history"], "history_partial:edge_info_wire_info_split_done_exhaust")
        self.assertEqual(ledger["primary_owned_edge_info_count"], 0)
        self.assertEqual(ledger["secondary_owned_edge_info_count"], 0)
        self.assertEqual(ledger["closed_wire_assigned_edge_info_count"], 0)
        self.assertEqual(ledger["closed_wire_info_count"], 0)
        self.assertEqual(ledger["closed_wire_vertex_count"], 0)
        self.assert_closed_wire_stack_ledger(ledger)
        self.assertEqual(ledger["super_edge_lifecycle_closed_root_edge_info_count"], 1)
        self.assertEqual(ledger["tight_bound_done_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_split_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_new_wire_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["tight_bound_new_wire_vertex_count"], ledger["tight_bound_new_wire_candidate_count"] * 2)
        self.assertEqual(ledger["tight_bound_owner_transfer_candidate_edge_info_count"], 0)
        self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_transfer_wire_vertex_count"], 0)
        self.assertEqual(ledger["tight_bound_transferred_owner_edge_info_count"], 0)
        self.assertEqual(ledger["tight_bound_split_owner_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_split_owner_vertex_count"], 0)
        self.assertEqual(ledger["tight_bound_split_owner_built_wire_count"], 0)
        self.assertEqual(ledger["tight_bound_split_wire_vertex_count"], 0)
        self.assertEqual(ledger["tight_bound_split_wire_built_count"], 0)
        self.assert_existing_wire_search_ledger(ledger)
        self.assertEqual(ledger["open_export_edge_info_count"], 1)
        self.assert_open_wire_compound_ledger(ledger)
        self.assertGreater(ledger["source_identity_shared_vertex_edge_info_count"], 0)
        self.assertEqual(
            ledger["source_identity_open_export_shared_vertex_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(
            ledger["source_identity_purge_bridge_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_purge_bridge_source_shared_vertex_wire_info_count"],
            ledger["open_wire_compound_purge_bridge_wire_info_count"],
        )
        self.assertEqual(ledger["open_wire_compound_purge_bridge_unmatched_wire_info_count"], 0)
        self.assertGreater(ledger["source_lineage_split_edge_info_count"], 0)
        self.assertEqual(
            ledger["source_lineage_open_export_edge_info_count"],
            ledger["open_export_edge_info_count"],
        )
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        history = sketch["wire_joiner_history_detail"]
        entries = self.assert_open_export_history_entries(history)
        self.assertEqual(len(entries), ledger["open_export_edge_info_count"])
        entry_counts = self.open_export_entry_counts(entries)
        self.assertEqual(entry_counts["source_lineage"], ledger["source_lineage_open_export_edge_info_count"])
        self.assertEqual(entry_counts["purge_bridge"], ledger["source_identity_purge_bridge_edge_info_count"])
        self.assertEqual(entry_counts["missing_source_lineage"], 0)
        self.assertEqual(len(entries), 1)
        self.assertTrue(entries[0]["source_edge_indices"])
        self.assertTrue(entries[0]["purge_bridge"])
        self.assertEqual(ledger["source_lineage_multi_source_edge_info_count"], 0)
        self.assert_super_edge_lifecycle_ledger(ledger)
        self.assertEqual(ledger["ordered_wire_info_count"], 1)
        self.assertEqual(ledger["ordered_vertex_count"], ledger["edge_info_count"])
        self.assertEqual(ledger["iteration2_marked_edge_info_count"], ledger["edge_info_count"])
        self.assertEqual(ledger["branch_search_candidate_count"], 0)
        self.assertEqual(ledger["branch_search_seed_wire_info_count"], 0)
        self.assertEqual(ledger["branch_search_inside_candidate_count"], 0)
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["new_wire_seed_wire_info_count"], 0)
        self.assertEqual(ledger["split_wire_candidate_count"], 0)
        self.assertEqual(ledger["split_wire_edge_info_count"], 0)
        self.assertEqual(ledger["done_wire_info_count"], 0)
        self.assertEqual(ledger["done_owned_edge_info_count"], 0)
        self.assertEqual(ledger["owner_propagation_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_search_candidate_edge_info_count"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-dangling-line")
        self.assertIn("Sketch", result["mesh"])

    def test_p5_internal_shape_records_deleted_history_for_filtered_dangling_edge(self) -> None:
        result = self.run_recompute("sketch-internal-face-dangling-line", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        deleted_edges = [
            item
            for item in named_shape["history"]
            if item["kind"] == "deleted" and item["element"] == "Edge5"
        ]
        deleted_vertices = [
            item
            for item in named_shape["history"]
            if item["kind"] == "deleted" and item["element"] == "Vertex6"
        ]
        self.assertNotIn("Edge5", named_shape["element_map"])
        self.assertNotIn("Vertex6", named_shape["element_map"])
        self.assertEqual(deleted_edges, [{"element": "Edge5", "kind": "deleted", "sources": ["Edge5"]}])
        self.assertEqual(deleted_vertices, [{"element": "Vertex6", "kind": "deleted", "sources": ["Vertex6"]}])

    def test_p5_split_and_dangling_open_wires_keep_leftover_internal_edge(self) -> None:
        result = self.run_recompute("sketch-internal-face-split-and-dangling", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-split-and-dangling")
        self.assertIn("Sketch", result["mesh"])

    def test_p5_pad_uses_selected_internal_face_sublist(self) -> None:
        result = self.run_recompute("pad-internal-face-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "pad-internal-face-sublist")

    def test_p5_pad_rejects_internal_face_stable_sublist_until_element_map_exists(self) -> None:
        result = self.run_recompute("pad-internal-face-stable-sublist-unsupported", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_stable_subname"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace1")
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_accepts_reference_shadow_as_recovery_evidence_only(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["StableSubList"], ["g305:split1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])
        self.assertEqual(update["ReferenceShadow"][0]["stableSubname"], "g305:split1")

    def test_p5_pad_uses_shadow_sub_before_global_reference_shadow_recovery(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload["Objects"][1]["Properties"]["Profile"]["SubList"] = ["InternalFace2"]

        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)
            result = self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

        pad = result["objects"]["Pad"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})

        library = self.ffi_library()
        raw = json.dumps(payload).encode("utf-8")
        ffi = library.cad_core_recompute_json(raw, len(raw))
        try:
            if ffi.status != 0:
                error = ctypes.string_at(ffi.error.ptr, ffi.error.len).decode("utf-8") if ffi.error.ptr else ""
                self.fail(f"cad_core_recompute_json failed with status {ffi.status}: {error}")
            ffi_result = json.loads(ctypes.string_at(ffi.json.ptr, ffi.json.len).decode("utf-8"))
        finally:
            library.cad_core_free_result(ctypes.byref(ffi))

        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["StableSubList"], ["g305:split1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])

    def test_p5_pad_recovers_empty_sublist_from_shadow_sub_reference_shadow(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json"
        for sublist_mode in ["omitted", "empty"]:
            with self.subTest(sublist_mode=sublist_mode):
                payload = json.loads(fixture_path.read_text(encoding="utf-8"))
                profile = payload["Objects"][1]["Properties"]["Profile"]
                if sublist_mode == "omitted":
                    del profile["SubList"]
                else:
                    profile["SubList"] = []
                profile["StableSubList"] = ["g305:split1"]

                temp_path: Path | None = None
                try:
                    with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                        json.dump(payload, temp)
                        temp_path = Path(temp.name)
                    result = self.run_recompute_file(temp_path)
                finally:
                    if temp_path is not None:
                        temp_path.unlink(missing_ok=True)

                pad = result["objects"]["Pad"]
                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(pad["status"], "ok")
                self.assertAlmostEqual(pad["volume"], 250.0)
                self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})

                library = self.ffi_library()
                raw = json.dumps(payload).encode("utf-8")
                ffi = library.cad_core_recompute_json(raw, len(raw))
                try:
                    if ffi.status != 0:
                        error = ctypes.string_at(ffi.error.ptr, ffi.error.len).decode("utf-8") if ffi.error.ptr else ""
                        self.fail(f"cad_core_recompute_json failed with status {ffi.status}: {error}")
                    ffi_result = json.loads(ctypes.string_at(ffi.json.ptr, ffi.json.len).decode("utf-8"))
                finally:
                    library.cad_core_free_result(ctypes.byref(ffi))

                update = ffi_result["elementReferenceUpdates"][0]
                self.assertEqual(update["SubList"], ["InternalFace1"])
                self.assertEqual(update["StableSubList"], ["g305:split1"])
                self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])
                self.assertEqual(update["ReferenceShadow"][0]["stableSubname"], "g305:split1")

    def test_p5_pad_rejects_malformed_reference_shadow(self) -> None:
        for fixture in [
            "pad-internal-face-reference-shadow-invalid-length",
            "pad-internal-face-reference-shadow-invalid-brep",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                diagnostic_codes = [item["code"] for item in result["diagnostics"]]

                self.assertIn("invalid_link_value", diagnostic_codes)
                self.assertEqual(result["diagnostics"][0]["object"], "Pad")
                self.assertEqual(result["diagnostics"][0]["property"], "Profile")
                self.assertEqual(result["diagnostics"][0]["stage"], "parse")
                self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_reference_shadow_semantic_drift(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-drift", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_semantic_drift"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace1")
        self.assertIn("centroid changed", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_recovers_missing_internal_face_sublist_from_reference_shadow(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-recover-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-recover-sublist", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["StableSubList"], ["g305:split1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])

    def test_p5_pad_recovers_ambiguous_fingerprint_with_reference_shadow_brep(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-brep-recover-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-brep-recover-sublist", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["ReferenceShadow"][0]["subname"], "InternalFace1")
        brep = update["ReferenceShadow"][0]["brep"]
        self.assertEqual(brep["format"], "brep-text")
        self.assertEqual(brep["byteLength"], len(brep["data"]))
        self.assertEqual(brep["sha256"], hashlib.sha256(brep["data"].encode()).hexdigest())
        self.assertNotEqual(brep["byteLength"], 662)

    def test_p5_pad_recovers_reference_shadow_brep_zstd_base64(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-brep-bin-recover-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-brep-bin-recover-sublist", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["ReferenceShadow"][0]["subname"], "InternalFace1")
        brep = update["ReferenceShadow"][0]["brep"]
        self.assertEqual(brep["format"], "brep-text")
        self.assertEqual(brep["byteLength"], len(brep["data"]))
        self.assertEqual(brep["sha256"], hashlib.sha256(brep["data"].encode()).hexdigest())

    def test_p5_pad_rejects_reference_shadow_brep_zstd_base64_decode_error(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow-brep-bin-recover-sublist.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload["Objects"][1]["Properties"]["Profile"]["ReferenceShadow"][0]["brep"]["data"] = "not-base64"

        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)

            result = self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

        diagnostic = result["diagnostics"][0]
        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_resolve_failed"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("valid base64", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_reference_shadow_brep_sha_mismatch(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow-brep-recover-sublist.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload["Objects"][1]["Properties"]["Profile"]["ReferenceShadow"][0]["brep"]["sha256"] = "0" * 64

        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)
            result = self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

        diagnostic = result["diagnostics"][0]
        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_resolve_failed"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("sha256 does not match data", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_reference_shadow_brep_uses_shared_vertex_geometry_not_bbox_fingerprint(self) -> None:
        dummy_fixture = ROOT / "fixtures" / "p5" / "part-extrusion-dirlink-reference-shadow-brep-edge-split.json"
        dummy_brep = json.loads(dummy_fixture.read_text(encoding="utf-8"))["Objects"][1]["Properties"]["DirLink"][
            "ReferenceShadow"
        ][0]["brep"]

        def payload_for_axis(start: list[float], end: list[float], sublist: list[str], shadow: dict) -> dict:
            return {
                "Objects": [
                    {
                        "Name": "BaseLine",
                        "ID": 1,
                        "TypeId": "Part::Line",
                        "Properties": {"X1": 0, "Y1": 0, "Z1": 0, "X2": 2, "Y2": 0, "Z2": 0},
                    },
                    {
                        "Name": "Axis",
                        "ID": 2,
                        "TypeId": "Sketcher::SketchObject",
                        "Properties": {
                            "Geometry": [{"kind": "LineSegment", "start": [start[0], start[1]], "end": [end[0], end[1]]}],
                            "Constraints": [],
                        },
                    },
                    {
                        "Name": "Extrude",
                        "ID": 3,
                        "TypeId": "Part::Extrusion",
                        "Properties": {
                            "Base": {"PropertyType": "App::PropertyLink", "value": "BaseLine"},
                            "DirMode": "Edge",
                            "DirLink": {
                                "PropertyType": "App::PropertyLinkSub",
                                "value": "Axis",
                                "SubList": sublist,
                                "ReferenceShadow": [shadow],
                            },
                            "LengthFwd": 0,
                            "LengthRev": 0,
                            "Solid": False,
                        },
                    },
                ],
                "recompute": {"objs": ["Extrude"]},
            }

        def run_payload(payload: dict) -> dict:
            temp_path: Path | None = None
            try:
                with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                    json.dump(payload, temp)
                    temp_path = Path(temp.name)
                return self.run_recompute_file(temp_path)
            finally:
                if temp_path is not None:
                    temp_path.unlink(missing_ok=True)

        def run_payload_ffi(payload: dict) -> dict:
            library = self.ffi_library()
            raw = json.dumps(payload).encode("utf-8")
            result = library.cad_core_recompute_json(raw, len(raw))
            try:
                if result.status != 0:
                    error = (
                        ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8")
                        if result.error.ptr
                        else ""
                    )
                    self.fail(f"cad_core_recompute_json failed with status {result.status}: {error}")
                return json.loads(ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8"))
            finally:
                library.cad_core_free_result(ctypes.byref(result))

        seed_shadow = {
            "target": "Axis",
            "targetId": 2,
            "property": "Shape",
            "shapeType": "Edge",
            "indexed": "Edge1",
            "subname": "Edge1",
            "stableSubname": "Edge1",
            "fingerprint": {},
            "brep": dummy_brep,
        }
        old_axis_payload = payload_for_axis([0, 1, 0], [1, 0, 0], ["Edge1"], seed_shadow)
        old_axis_result = run_payload_ffi(old_axis_payload)
        self.assertEqual(old_axis_result["diagnostics"], [])
        old_shadow = old_axis_result["elementReferenceUpdates"][0]["ReferenceShadow"][0]
        self.assertAlmostEqual(old_shadow["fingerprint"]["length"], 2**0.5)
        self.assertEqual(old_shadow["fingerprint"]["centroid"], [0.5, 0.5, 0.0])

        current_axis_payload = payload_for_axis([0, 0, 0], [1, 1, 0], ["Edge99"], old_shadow)
        result = run_payload(current_axis_payload)
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "Axis")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_pad_recovers_drifted_sublist_with_reference_shadow_brep(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-brep-drift-recover", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 5.0, 10.0]})
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-brep-drift-recover", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["ReferenceShadow"][0]["subname"], "InternalFace1")

    def test_p5_pad_requires_reselect_when_reference_shadow_brep_is_split(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-brep-split", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_split_requires_reselect"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("split", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_reports_deleted_reference_shadow_brep(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-brep-deleted", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_ambiguous_reference_shadow_recovery(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-ambiguous", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_resolve_ambiguous"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("multiple", diagnostic["message"])
        self.assertEqual(result["elementReferenceUpdates"], [])
        ambiguous_events = [
            event
            for event in result["named_shapes"]["Sketch.InternalShape"]["mapper_history"]
            if event["recoverability"] == "ambiguous"
            and event["diagnostic_status"] == "subname_resolve_ambiguous"
        ]
        self.assertEqual(len(ambiguous_events), 1)
        self.assertEqual(ambiguous_events[0]["source"], {"object": "Sketch", "subname": "InternalFace99"})
        self.assertEqual(ambiguous_events[0]["target"], {"object": "Sketch.InternalShape", "subname": ""})
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_missing_reference_shadow_recovery(self) -> None:
        result = self.run_recompute("pad-internal-face-reference-shadow-missing", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_resolve_failed"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertIn("does not match", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_uses_selected_internal_face_from_cross_cutters(self) -> None:
        result = self.run_recompute("pad-internal-face-cross-cutters-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "pad-internal-face-cross-cutters-sublist")

    def test_p5_pad_uses_bounded_profile_when_sketch_has_dangling_open_line(self) -> None:
        result = self.run_recompute("pad-dangling-line-profile", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "pad-dangling-line-profile")

    def test_p5_pad_requires_sublist_for_multi_internal_face_sketch(self) -> None:
        result = self.run_recompute("pad-internal-face-missing-sublist", "p5")

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_subshape"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_reports_missing_internal_face_subshape_context(self) -> None:
        result = self.run_recompute("pad-internal-face-missing-subshape", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_subshape"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_reports_open_profile_for_explicit_empty_internal_face_selection(self) -> None:
        result = self.run_recompute("pad-open-wire-profile", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["open_profile"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace1")
        self.assertIn("no closed InternalFace profile", diagnostic["message"])
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_non_face_internal_profile_subshape(self) -> None:
        result = self.run_recompute("pad-internal-face-invalid-kind", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_subshape_kind"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalEdge1")
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_part_extrusion_uses_whole_sketch_base_not_internal_face_profile(self) -> None:
        result = self.run_recompute("part-extrusion-sketch-solid", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["feature"], "part_extrusion")
        self.assertEqual(extrusion["source_base"], "Sketch")
        self.assertEqual(extrusion["solid"], True)
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 300.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 5.0, 6.0]})
        self.assertIn("Extrude", result["mesh"])
        self.assertNotIn("Profile", extrusion)

    def test_p5_part_extrusion_keeps_edge_base_as_surface_when_solid_false(self) -> None:
        result = self.run_recompute("part-extrusion-edge-surface", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["source_base"], "Line")
        self.assertEqual(extrusion["solid"], False)
        self.assertEqual(extrusion["shape"], "occt_face")
        self.assertAlmostEqual(extrusion["volume"], 0.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 0.0, 5.0]})

    def test_p5_part_extrusion_uses_dirlink_edge_magnitude_when_lengths_are_zero(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-edge", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["source_base"], "BaseLine")
        self.assertEqual(extrusion["shape"], "occt_face")
        self.assertAlmostEqual(extrusion["length_fwd"], 4.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 0.0, 4.0]})

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_edge_split(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-edge-split", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_split_requires_reselect"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("split", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_edge_deleted(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-edge-deleted", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_arc_edge_split(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-arc-edge-split", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_split_requires_reselect"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "SourceSketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("split", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_arc_edge_deleted(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-arc-edge-deleted", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "SourceSketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_bspline_edge_split(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-bspline-edge-split", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_split_requires_reselect"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "SourceSketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("split", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_bspline_edge_deleted(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-bspline-edge-deleted", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "SourceSketch")
        self.assertEqual(diagnostic["subname"], "Edge99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_dirlink_reports_reference_shadow_vertex_deleted(self) -> None:
        result = self.run_recompute("part-extrusion-dirlink-reference-shadow-brep-vertex-deleted", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["subname_deleted"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "DirLink")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "Vertex99")
        self.assertIn("deleted", diagnostic["message"])
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_part_extrusion_uses_planar_base_normal(self) -> None:
        result = self.run_recompute("part-extrusion-normal-plane", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["source_base"], "Plane")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 30.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [2.0, 3.0, 5.0]})

    def test_p5_part_extrusion_supports_forward_taper(self) -> None:
        result = self.run_recompute("part-extrusion-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertNotIn("topo_naming", extrusion)
        self.assertNotIn("topo_naming_history", extrusion)
        self.assertGreater(extrusion["volume"], 0.0)
        self.assertIn("Extrude", result["mesh"])

    def test_p5_part_extrusion_publishes_prism_and_taper_history(self) -> None:
        for fixture in ["part-extrusion-sketch-solid", "part-extrusion-taper"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                named_shape = result["named_shapes"]["Extrude"]
                history_kinds = {item["kind"] for item in named_shape["history"]}

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertIn("generated", history_kinds)
                self.assertIn("Sketch.Edge1", named_shape["element_map"])
                self.assertTrue(named_shape["element_map"]["Sketch.Edge1"])
                if fixture == "part-extrusion-taper":
                    maker_events = [
                        event
                        for event in named_shape["mapper_history"]
                        if event["maker_stage"] == "maker_history"
                    ]
                    self.assertTrue(
                        any(
                            event["relation"] == "generated"
                            and event["source"]["object"] == "Sketch"
                            for event in maker_events
                        )
                    )
                    self.assertTrue(
                        any(
                            event["relation"] == "generated"
                            and event["source"]["object"].startswith("Extrude.TaperSection")
                            for event in maker_events
                        )
                    )

    def test_p5_part_extrusion_supports_reverse_taper(self) -> None:
        result = self.run_recompute("part-extrusion-reverse-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertNotIn("topo_naming", extrusion)
        self.assertNotIn("topo_naming_history", extrusion)
        self.assertGreater(extrusion["volume"], 0.0)
        self.assertLess(extrusion["bbox"]["min"][2], -5.99)
        self.assertLess(extrusion["bbox"]["max"][2], 1.0)

    def test_p5_part_extrusion_supports_two_sided_taper(self) -> None:
        result = self.run_recompute("part-extrusion-two-sided-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertNotIn("topo_naming", extrusion)
        self.assertNotIn("topo_naming_history", extrusion)
        self.assertGreater(extrusion["volume"], 0.0)
        self.assertLess(extrusion["bbox"]["min"][2], -2.99)
        self.assertGreater(extrusion["bbox"]["max"][2], 5.99)

    def test_p5_part_extrusion_supports_facemaker_simple_without_holes(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-simple", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_compound")
        self.assertAlmostEqual(extrusion["volume"], 580.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})

    def test_p5_part_extrusion_supports_facemaker_cheese_holes(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-cheese", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 420.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})

    def test_p5_part_extrusion_default_bullseye_promotes_nested_island(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-bullseye-nested-island", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_compound")
        self.assertAlmostEqual(extrusion["volume"], 340.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})

    def test_p5_part_extrusion_default_bullseye_handles_intersected_inner_wires(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-bullseye-intersected-holes", "p5")
        extrusion = result["objects"]["Extrude"]
        sketch = result["objects"]["Sketch"]
        ledger = sketch["wire_joiner_ledger"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 340.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})
        self.assertEqual(ledger["tight_bound_transfer_wire_info_count"], 0)
        self.assertGreater(ledger["tight_bound_existing_wire_hit_count"], 0)
        self.assertEqual(ledger["tight_bound_existing_wire_selected_hit_count"], 0)
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_hit_count"],
            ledger["tight_bound_existing_wire_hit_count"],
        )
        self.assertEqual(ledger["tight_bound_existing_wire_idx_vertex_count"], 0)
        self.assertGreater(ledger["tight_bound_existing_wire_search_idx_vertex_count"], 0)
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_idx_vertex_count"],
            ledger["tight_bound_existing_wire_search_idx_vertex_count"],
        )
        self.assertGreater(ledger["tight_bound_existing_wire_search_stack_pos_count"], 0)
        self.assertEqual(
            ledger["tight_bound_existing_wire_search_only_stack_pos_count"],
            ledger["tight_bound_existing_wire_search_stack_pos_count"],
        )
        self.assert_repeated_split_exhaust_removal_ledger(ledger)

    def test_p5_part_extrusion_cheese_keeps_nested_wire_as_hole(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-cheese-nested-holes", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 300.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})

    def test_p5_part_extrusion_supports_facemaker_mode_extrusion(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-mode-extrusion", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 420.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})

    def test_p5_part_extrusion_rejects_unknown_facemaker_class(self) -> None:
        result = self.run_recompute("part-extrusion-facemaker-unknown", "p5")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_property"])
        self.assertEqual(diagnostic["object"], "Extrude")
        self.assertEqual(diagnostic["property"], "FaceMakerClass")
        self.assertEqual(result["objects"]["Extrude"]["status"], "error")

    def test_p5_external_geometry_resolves_internal_edge(self) -> None:
        result = self.run_recompute("sketch-external-internal-edge", "p5")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p5", "sketch-external-internal-edge")

    def test_p5_external_geometry_recovers_internal_edge_from_stable_sublist(self) -> None:
        result = self.run_recompute("sketch-external-internal-edge-stable-recover", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 36.0)
        ffi_result = self.run_recompute_ffi("sketch-external-internal-edge-stable-recover", "p5")
        update = ffi_result["elementReferenceUpdates"][0]
        sub_set = update["SubSet"][0]
        self.assertEqual(sub_set["SubList"], ["InternalEdge1"])
        self.assertEqual(sub_set["StableSubList"], ["Edge1"])
        self.assertEqual(sub_set["ReferenceShadow"][0]["subname"], "InternalEdge1")

    def test_p5_external_geometry_uses_shadow_sub_before_projection(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "sketch-external-internal-edge-stable-recover.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        external = payload["Objects"][1]["Properties"]["ExternalGeometry"]["SubSet"][0]
        external["SubList"] = ["InternalVertex1"]
        external["ShadowSub"] = [{"newName": "Edge1", "oldName": "InternalEdge1"}]

        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)
            result = self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_point_count"], 0)
        self.assertEqual(pad["status"], "ok")

        result = self.run_recompute("sketch-external-internal-vertex", "p5")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p5", "sketch-external-internal-vertex")

    def test_p5_open_sketch_keeps_raw_shape_without_profile_face(self) -> None:
        result = self.run_recompute("sketch-open-wire-internal-empty", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["shape"], "occt_sketch_shape")
        self.assert_object_matches_expected(result, "p5", "sketch-open-wire-internal-empty")
