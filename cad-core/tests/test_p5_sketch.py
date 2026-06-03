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

    HELPER_OVERRIDE_REASON_LEDGER_KEYS = {
        "consumed_open_cutter_graph": (
            "helper_open_export_override_consumed_open_cutter_graph_edge_info_count",
            "open_wire_compound_helper_open_export_override_consumed_open_cutter_graph_wire_info_count",
        ),
        "partial_junction_open_cutter": (
            "helper_open_export_override_partial_junction_open_cutter_edge_info_count",
            "open_wire_compound_helper_open_export_override_partial_junction_open_cutter_wire_info_count",
        ),
        "closed_wire_cycle": (
            "helper_open_export_override_closed_wire_cycle_edge_info_count",
            "open_wire_compound_helper_open_export_override_closed_wire_cycle_wire_info_count",
        ),
        "partial_shared_closed_wire": (
            "helper_open_export_override_partial_shared_closed_wire_edge_info_count",
            "open_wire_compound_helper_open_export_override_partial_shared_closed_wire_wire_info_count",
        ),
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
            self.assertEqual(ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"], 0)
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
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_owner_vertex_blocked_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_order_blocked_count"], 0)
            self.assertEqual(ledger["tight_bound_existing_wire_search_only_wire_build_blocked_count"], 0)
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
            ledger["tight_bound_existing_wire_search_only_owner_vertex_blocked_count"]
            + ledger["tight_bound_existing_wire_search_only_order_blocked_count"]
            + ledger["tight_bound_existing_wire_search_only_wire_build_blocked_count"],
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
            self.assertGreater(
                ledger["tight_bound_exhaust_primary_reset_edge_info_count"]
                + ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"],
                0,
            )
        self.assertLessEqual(
            ledger["tight_bound_exhaust_primary_reset_edge_info_count"],
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"],
            ledger["primary_owned_edge_info_count"],
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
        self.assertEqual(ledger["tight_bound_existing_wire_search_only_owner_vertex_blocked_count"], 0)
        self.assertEqual(ledger["tight_bound_existing_wire_search_only_wire_build_blocked_count"], 0)

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
                        self.assertNotIn("generated_open_export_edge_info_count", ledger)
                        self.assertNotIn("open_wire_compound_generated_wire_info_count", ledger)
                        self.assertEqual(
                            ledger["open_wire_compound_helper_open_export_override_helper_shape_wire_info_count"],
                            0,
                        )
                        self.assertEqual(
                            ledger["open_wire_compound_helper_open_export_override_helper_shape_wire_info_count"],
                            ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"],
                        )
                        self.assertEqual(
                            ledger["open_wire_compound_legacy_helper_shape_wire_info_count"],
                            0,
                        )
                        self.assertEqual(
                            ledger["open_wire_compound_legacy_helper_shape_wire_info_count"],
                            ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"],
                        )
                        self.assertEqual(
                            ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"],
                            0,
                        )
                        self.assertEqual(ledger["result_wire_producer_unknown_invariant_count"], 0)
                        self.assertEqual(
                            ledger["result_wire_producer_exported_without_helper_wire_info_count"],
                            ledger["migrated_legacy_helper_slot_count"],
                        )

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

        self.assertEqual(named_shape["sketch_internal_history_status"], "history_partial:facemaker_buildface")
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertEqual(len(runtime_entries), runtime_history["open_export_edge_count"])
        self.assertEqual(len(internal_entries), internal_history["wire_joiner_open_export_edge_count"])
        self.assertEqual(len(internal_entries), len(runtime_entries))
        self.assertEqual(
            internal_history["wire_joiner_open_export_helper_override_edge_count"],
            runtime_history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            internal_history["wire_joiner_open_export_helper_override_source_edge_producer_output_edge_info_count"],
            runtime_history["open_export_helper_override_source_edge_producer_output_edge_info_count"],
        )
        self.assertEqual(internal_history["named_shape_history_missing_result_wire_identity_count"], 0)
        self.assertEqual(internal_history["element_map_result_wire_identity_mismatch_count"], 0)
        self.assertEqual(len(producer_entries), ledger["migrated_legacy_helper_slot_count"])

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
        self.assertEqual(len(internal_by_open_export), ledger["migrated_legacy_helper_slot_count"])
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
            self.assertEqual(ledger["open_wire_compound_helper_open_export_override_wire_info_count"], 0)
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_built_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_unowned_removal_child_wire_producer_ready_output_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_ready_wire_info_count"
                ],
                0,
            )
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_full_ahistory_evidence_wire_info_count"
                ],
                0,
            )
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
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_source_edge_export_shape_wire_info_count"],
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count"
            ],
            ledger["open_wire_compound_helper_open_export_override_source_edge_export_shape_wire_info_count"],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count"
            ]
            + ledger["open_wire_compound_helper_open_export_override_helper_shape_wire_info_count"]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
            ],
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
        )
        self.assertGreaterEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_built_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_info_count"
            ],
        )
        root_result_wire_producer_built = ledger[
            "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
        ]
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_unowned_removal_child_wire_producer_ready_output_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
            ],
            root_result_wire_producer_built,
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count"
            ],
            root_result_wire_producer_built,
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_built_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count"
            ],
            root_result_wire_producer_built,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_non_current_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_non_current_member_edge_info_count"
            ],
        )
        if (
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_wire_info_count"
            ]
            == 0
        ):
            self.assertEqual(
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_non_current_member_edge_info_count"
                ],
                ledger[
                    "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_non_current_member_edge_info_count"
                ],
            )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_current_member_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_non_current_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_covered_member_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_unique_covered_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_covered_member_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_unique_current_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_current_member_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_unique_current_member_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_unique_covered_member_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_full_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_full_ahistory_producer_evidence_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_source_edge_producer_output_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_unowned_removal_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_suppressed_pending_member_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_full_ahistory_producer_evidence_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_unowned_removal_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_primary_removal_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_secondary_removal_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_missing_removal_branch_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_pending_member_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_complete_child_ownership_root_edge_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_incomplete_child_ownership_root_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_root_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_wire_built_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_candidate_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_wire_built_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_ready_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_candidate_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_full_ahistory_evidence_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_ready_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_pending_member_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_candidate_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_full_ahistory_producer_evidence_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_missing_full_ahistory_producer_evidence_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_open_wire_compound_eligible_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_forced_open_wire_compound_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_root_producer_ready_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_current_member_child_wire_producer_ready_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_root_producer_ready_wire_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_current_member_child_wire_producer_full_ahistory_evidence_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_current_member_child_wire_producer_missing_full_ahistory_evidence_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_current_member_child_wire_producer_full_ahistory_evidence_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_blocked_by_source_shape_current_member_child_wire_producer_ready_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_requires_member_suppression_current_member_wire_info_count"
            ],
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
        self.assertEqual(len(entries), history["open_export_edge_count"])
        self.assertEqual(
            sum(1 for entry in entries if entry["source_edge_indices"]),
            history["open_export_source_lineage_edge_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if not entry["source_edge_indices"]),
            history["open_export_missing_source_lineage_edge_count"],
        )
        self.assertNotIn("open_export_generated_edge_count", history)
        for entry in entries:
            self.assertNotIn("generated_open_export", entry)
            self.assertNotIn("generated_open_export_reason", entry)
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override"]),
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_reason"]),
            history["open_export_helper_override_edge_count"],
        )
        self.assertTrue(
            all(
                bool(entry["helper_open_export_override_reason"]) == entry["helper_open_export_override"]
                for entry in entries
            )
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"] and not entry["source_edge_indices"]
            ),
            history["open_export_helper_override_missing_source_lineage_edge_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["purge_bridge"]),
            history["open_export_purge_bridge_edge_count"],
        )
        self.assertEqual(
            [entry["open_export_index"] for entry in entries],
            list(range(1, len(entries) + 1)),
        )
        return entries

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
            "repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count",
            "repeated_split_exhaust_rerun_live_transfer_wire_info_count",
            "repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count",
            "repeated_split_exhaust_rerun_live_done_wire_info_count",
            "repeated_split_exhaust_rerun_removal_scan_count",
            "repeated_split_exhaust_rerun_removal_edge_info_count",
            "repeated_split_exhaust_rerun_removal_unowned_edge_info_count",
            "repeated_split_exhaust_rerun_removal_secondary_edge_info_count",
            "repeated_split_exhaust_rerun_removal_primary_edge_info_count",
            "repeated_split_exhaust_rerun_loop_exit_no_removal_count",
            "repeated_split_exhaust_rerun_branch_search_candidate_count",
            "repeated_split_exhaust_rerun_branch_search_inside_candidate_count",
            "repeated_split_exhaust_rerun_branch_search_outside_candidate_count",
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
            ledger["repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count"]
            + ledger["repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count"],
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
            ledger["repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_outside_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_transfer_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_live_branch_search_inside_candidate_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_live_assigned_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_live_transfer_wire_info_count"]
            + ledger["repeated_split_exhaust_rerun_live_done_wire_info_count"],
            ledger["repeated_split_exhaust_rerun_live_closed_wire_info_count"],
        )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_removal_edge_info_count"],
            ledger["repeated_split_exhaust_rerun_removal_unowned_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_removal_secondary_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_removal_primary_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["repeated_split_exhaust_rerun_loop_exit_no_removal_count"],
            ledger["repeated_split_exhaust_rerun_removal_scan_count"],
        )
        if ledger["repeated_split_exhaust_rerun_removal_edge_info_count"] == 0:
            self.assertEqual(
                ledger["repeated_split_exhaust_rerun_loop_exit_no_removal_count"],
                ledger["repeated_split_exhaust_rerun_removal_scan_count"],
            )
        self.assertEqual(
            ledger["repeated_split_exhaust_rerun_branch_search_candidate_count"],
            ledger["repeated_split_exhaust_rerun_branch_search_inside_candidate_count"]
            + ledger["repeated_split_exhaust_rerun_branch_search_outside_candidate_count"],
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
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_branch_search_outside_candidate_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_transfer_wire_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_transferred_owner_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_live_done_wire_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_scan_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_unowned_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_secondary_edge_info_count"], 0)
            self.assertEqual(ledger["repeated_split_exhaust_rerun_removal_primary_edge_info_count"], 0)
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
        reason_counts = Counter(
            entry["helper_open_export_override_reason"]
            for entry in entries
            if entry["helper_open_export_override"]
        )

        self.assertEqual(reason_counts, Counter(expected_counts))
        self.assertEqual(sum(expected_counts.values()), ledger["helper_open_export_override_edge_info_count"])
        self.assertEqual(sum(expected_counts.values()), history["open_export_helper_override_edge_count"])
        self.assertEqual(
            sum(expected_counts.values()),
            ledger["helper_open_export_override_candidate_edge_count"],
        )
        self.assertEqual(ledger["helper_open_export_override_unbound_edge_count"], 0)
        self.assertEqual(ledger["helper_open_export_override_duplicate_source_edge_info_count"], 0)
        self.assert_exhaust_adjacent_search_ledger(ledger)
        self.assert_repeated_split_exhaust_removal_ledger(ledger)
        self.assertLessEqual(
            ledger["tight_bound_live_split_wire_info_count"],
            ledger["tight_bound_split_owner_wire_info_count"],
        )
        self.assertLessEqual(
            ledger["tight_bound_live_split_wire_edge_info_count"],
            ledger["tight_bound_split_owner_vertex_count"],
        )
        if ledger["tight_bound_live_split_wire_info_count"] == 0:
            self.assertEqual(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
        else:
            self.assertGreater(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
        if ledger["tight_bound_exhaust_discarded_purged_wire_info_count"] > 0:
            self.assertGreater(
                ledger["tight_bound_exhaust_primary_reset_edge_info_count"]
                + ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"],
                0,
            )
        self.assertEqual(
            ledger["owner_propagation_candidate_count"],
            ledger["owner_propagation_unassigned_candidate_count"]
            + ledger["owner_propagation_other_wire_candidate_count"],
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
            self.assertGreater(ledger["helper_open_export_override_edge_info_count"], 0)
            self.assertGreater(ledger["repeated_split_exhaust_cycle_count"], 0)
        self.assertGreaterEqual(
            history["deleted_history_count"],
            ledger["repeated_split_exhaust_removed_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_removal_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_info_consumed_count"],
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_source_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_source_edge_info_consumed_count"],
            history["open_export_helper_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_open_wire_compound_eligible_edge_info_count"]
            + ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_open_wire_compound_eligible_edge_info_count"]
            + history["open_export_helper_override_forced_open_wire_compound_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            history["open_export_helper_override_source_edge_export_shape_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count"
            ],
            history[
                "open_export_helper_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger["helper_open_export_override_full_ahistory_producer_evidence_edge_info_count"],
            history["open_export_helper_override_full_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger["helper_open_export_override_super_edge_member_edge_info_count"],
            history["open_export_helper_override_super_edge_member_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
            history["open_export_helper_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count"
            ],
        )
        self.assertGreaterEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ]
            + history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_current_member_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_built_wire_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count"
            ],
        )
        for ledger_key, history_key in (
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_primary_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_primary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_secondary_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_secondary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_removal_branch_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_removal_branch_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch_edge_info_count",
            ),
        ):
            self.assertEqual(ledger[ledger_key], history[history_key])
        self.assertEqual(
            ledger["helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
            history["open_export_helper_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            history["open_export_helper_override_source_edge_producer_output_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["helper_open_export_override_full_ahistory_producer_evidence_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
            ledger["helper_open_export_override_full_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            ledger["helper_open_export_override_full_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertLessEqual(
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
            ledger["helper_open_export_override_super_edge_member_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_primary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_secondary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_removal_branch_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        for ledger_key, history_key in (
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_covered_member_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_covered_member_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_current_member_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_current_member_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_member_root_result_wire_producer_non_current_member_edge_info_count",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_non_current_member_edge_info_count",
            ),
        ):
            self.assertEqual(ledger[ledger_key], history[history_key])
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_current_member_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_non_current_member_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_covered_member_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_wire_info_count"
            ]
            + ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_non_current_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_covered_member_edge_info_count"
            ],
        )
        self.assertGreaterEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_wire_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_wire_info_count"
            ],
            ledger[
                "helper_open_export_override_super_edge_member_with_root_edge_info_count"
            ],
        )
        self.assertGreaterEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_non_current_member_edge_info_count"
            ],
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_output_blocked_by_multi_member_super_edge_wire_info_count"
            ],
        )
        self.assertLessEqual(
            ledger["helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
            ledger["helper_open_export_override_super_edge_member_edge_info_count"],
        )
        self.assertLessEqual(
            ledger[
                "helper_open_export_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            ledger["helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_export_blocked_by_iteration_edge_info_count"]
            + ledger["helper_open_export_override_export_blocked_by_wire_info_edge_info_count"],
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_export_blocked_by_iteration_edge_info_count"]
            + history["open_export_helper_override_export_blocked_by_wire_info_edge_info_count"],
            history["open_export_helper_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_with_open_wire_compound_eligible_candidate_edge_info_count"]
            + ledger["helper_open_export_override_missing_open_wire_compound_eligible_candidate_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_with_open_wire_compound_eligible_candidate_edge_info_count"]
            + history["open_export_helper_override_missing_open_wire_compound_eligible_candidate_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_removed_source_edge_info_count"]
            + ledger["helper_open_export_override_missing_removed_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_removed_source_edge_info_count"]
            + history["open_export_helper_override_missing_removed_source_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_removed_target_edge_info_count"]
            + ledger["helper_open_export_override_missing_removed_target_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_removed_target_edge_info_count"]
            + history["open_export_helper_override_missing_removed_target_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"]
            + ledger["helper_open_export_override_missing_ahistory_remove_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_ahistory_remove_source_edge_info_count"]
            + history["open_export_helper_override_missing_ahistory_remove_source_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"]
            + ledger["helper_open_export_override_missing_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_ahistory_remove_source_lineage_edge_info_count"]
            + history["open_export_helper_override_missing_ahistory_remove_source_lineage_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_same_source_lineage_edge_info_count"]
            + ledger["helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_ahistory_remove_same_source_lineage_edge_info_count"]
            + history["open_export_helper_override_ahistory_remove_foreign_source_lineage_edge_info_count"],
            history["open_export_helper_override_ahistory_remove_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_safe_ahistory_producer_evidence_edge_info_count"]
            + ledger["helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_safe_ahistory_producer_evidence_edge_info_count"]
            + history["open_export_helper_override_missing_safe_ahistory_producer_evidence_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ]
            + history[
                "open_export_helper_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history["open_export_helper_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ]
            + ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ]
            + history[
                "open_export_helper_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"]
            + ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_source_lineage_removed_source_edge_info_count"]
            + history["open_export_helper_override_missing_source_lineage_removed_source_edge_info_count"],
            history["open_export_helper_override_edge_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_source_edge_info"]),
            history["open_export_helper_override_source_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_source_edge_info_consumed"]),
            history["open_export_helper_override_source_edge_info_consumed_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
            ),
            history["open_export_helper_override_open_wire_compound_eligible_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history["open_export_helper_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_source_edge_export_shape"]),
            history["open_export_helper_override_source_edge_export_shape_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
                and not entry["helper_open_export_override_source_edge_export_shape"]
            ),
            history[
                "open_export_helper_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_full_ahistory_producer_evidence"]),
            history["open_export_helper_override_full_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_full_ahistory_producer_evidence"]
                and not entry["helper_open_export_override_source_edge_export_shape"]
            ),
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_full_ahistory_producer_evidence"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                and not entry["helper_open_export_override_full_ahistory_producer_evidence"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_super_edge_member_edge_info"]),
            history["open_export_helper_override_super_edge_member_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_edge_info"]
            ),
            history["open_export_helper_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_open_lifecycle_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_closed_lifecycle_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_export_blocked_by_wire_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
        )
        self.assertTrue(
            all(
                (
                    entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                    + entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    + entry["helper_open_export_override_super_edge_root_export_blocked_by_wire_info"]
                )
                == 1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_edge_info"]
            )
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                and entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                and not entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                and not entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                and not entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence_iteration_blocked"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence_iteration_blocked"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_missing_safe_ahistory_producer_evidence_iteration_blocked"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence_iteration_blocked"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence_iteration_blocked"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_missing_safe_ahistory_producer_evidence_iteration_blocked"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and not entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_iteration_blocked_unowned_removal"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_iteration_blocked_primary_removal"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_iteration_blocked_secondary_removal"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_iteration_blocked_missing_removal_branch"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry[
                    "helper_open_export_override_super_edge_root_result_wire_producer_candidate_full_ahistory_producer_evidence"
                ]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry[
                    "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence"
                ]
            ),
            history[
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"]
                or (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_open_lifecycle_edge_info"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry[
                    "helper_open_export_override_super_edge_root_result_wire_producer_candidate_full_ahistory_producer_evidence"
                ]
                == (
                    entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"]
                    and entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry[
                    "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence"
                ]
                == (
                    entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"]
                    and not entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_full_ahistory_producer_evidence"
                    ]
                )
                + int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence"
                    ]
                )
                == int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"])
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        for entry_key, history_key in (
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_primary_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_primary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_secondary_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_secondary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_removal_branch",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_removal_branch_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal_edge_info_count",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch",
                "open_export_helper_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch_edge_info_count",
            ),
        ):
            self.assertEqual(
                sum(
                    1
                    for entry in entries
                    if entry["helper_open_export_override_super_edge_member_edge_info"] and entry[entry_key]
                ),
                history[history_key],
            )
        for candidate_branch_key, removal_branch_key in (
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal",
                "helper_open_export_override_super_edge_root_iteration_blocked_unowned_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_primary_removal",
                "helper_open_export_override_super_edge_root_iteration_blocked_primary_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_secondary_removal",
                "helper_open_export_override_super_edge_root_iteration_blocked_secondary_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_removal_branch",
                "helper_open_export_override_super_edge_root_iteration_blocked_missing_removal_branch",
            ),
        ):
            self.assertTrue(
                all(
                    entry[candidate_branch_key]
                    == (
                        entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"]
                        and entry[removal_branch_key]
                    )
                    for entry in entries
                    if entry["helper_open_export_override_super_edge_member_edge_info"]
                )
            )
        self.assertTrue(
            all(
                entry[
                    "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready"
                ]
                == (
                    entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal"]
                    and entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_full_ahistory_producer_evidence"
                    ]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal"])
                + int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate_primary_removal"])
                + int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate_secondary_removal"])
                + int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_removal_branch"])
                == int(entry["helper_open_export_override_super_edge_root_result_wire_producer_candidate"])
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        for missing_full_branch_key, candidate_branch_key in (
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal",
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal",
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_primary_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal",
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_secondary_removal",
            ),
            (
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch",
                "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_removal_branch",
            ),
        ):
            self.assertTrue(
                all(
                    entry[missing_full_branch_key]
                    == (
                        entry[
                            "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence"
                        ]
                        and entry[candidate_branch_key]
                    )
                    for entry in entries
                    if entry["helper_open_export_override_super_edge_member_edge_info"]
                )
            )
        self.assertTrue(
            all(
                int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal"
                    ]
                )
                + int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal"
                    ]
                )
                + int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal"
                    ]
                )
                + int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch"
                    ]
                )
                == int(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence"
                    ]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_iteration_blocked_unowned_removal"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_removed_by_unowned"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_iteration_blocked_primary_removal"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_removed_by_primary_owner"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_iteration_blocked_secondary_removal"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and entry["helper_open_export_override_super_edge_root_removed_by_secondary_owner"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_iteration_blocked_missing_removal_branch"]
                == (
                    entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                    and not entry["helper_open_export_override_super_edge_root_removed_by_unowned"]
                    and not entry["helper_open_export_override_super_edge_root_removed_by_primary_owner"]
                    and not entry["helper_open_export_override_super_edge_root_removed_by_secondary_owner"]
                )
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertTrue(
            all(
                int(entry["helper_open_export_override_super_edge_root_iteration_blocked_unowned_removal"])
                + int(entry["helper_open_export_override_super_edge_root_iteration_blocked_primary_removal"])
                + int(entry["helper_open_export_override_super_edge_root_iteration_blocked_secondary_removal"])
                + int(entry["helper_open_export_override_super_edge_root_iteration_blocked_missing_removal_branch"])
                == int(entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"])
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
            )
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history["open_export_helper_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_super_edge_member_edge_info"]
                and not entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history[
                "open_export_helper_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
                != entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
                for entry in entries
                if entry["helper_open_export_override"]
            )
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_export_blocked_by_iteration"]),
            history["open_export_helper_override_export_blocked_by_iteration_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_export_blocked_by_wire_info"]),
            history["open_export_helper_override_export_blocked_by_wire_info_edge_info_count"],
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_export_blocked_by_iteration"]
                != entry["helper_open_export_override_export_blocked_by_wire_info"]
                for entry in entries
                if entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            )
        )
        self.assertTrue(
            all(
                (entry["helper_open_export_override_selected_iteration"] < 0)
                == entry["helper_open_export_override_export_blocked_by_iteration"]
                for entry in entries
                if entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            )
        )
        self.assertTrue(
            all(
                (
                    entry["helper_open_export_override_selected_iteration"] >= 0
                    and entry["helper_open_export_override_selected_wire_info"] != 0
                )
                == entry["helper_open_export_override_export_blocked_by_wire_info"]
                for entry in entries
                if entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            )
        )
        self.assertEqual(
            sum(len(entry["helper_open_export_override_candidate_edge_info_indices"]) for entry in entries),
            history["open_export_helper_override_binding_candidate_edge_info_count"],
        )
        self.assertEqual(
            sum(
                len(entry["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"])
                for entry in entries
            ),
            history["open_export_helper_override_open_wire_compound_eligible_candidate_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"]
            ),
            history["open_export_helper_override_with_open_wire_compound_eligible_candidate_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"]
            ),
            history["open_export_helper_override_missing_open_wire_compound_eligible_candidate_edge_info_count"],
        )
        self.assertTrue(
            all(
                entry["edge_info_index"] in entry["helper_open_export_override_candidate_edge_info_indices"]
                for entry in entries
                if entry["helper_open_export_override"]
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
                == (
                    entry["edge_info_index"]
                    in entry[
                        "helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"
                    ]
                )
                for entry in entries
                if entry["helper_open_export_override"]
            )
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_removed_source_edge_info"]),
            history["open_export_helper_override_removed_source_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_removed_source_edge_info"]
            ),
            history["open_export_helper_override_missing_removed_source_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_removed_target_edge_info"]),
            history["open_export_helper_override_removed_target_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_removed_target_edge_info"]
            ),
            history["open_export_helper_override_missing_removed_target_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_ahistory_remove_source_edge_info"]),
            history["open_export_helper_override_ahistory_remove_source_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_ahistory_remove_source_edge_info"]
            ),
            history["open_export_helper_override_missing_ahistory_remove_source_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_ahistory_remove_source_lineage"]),
            history["open_export_helper_override_ahistory_remove_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_ahistory_remove_same_source_lineage"]),
            history["open_export_helper_override_ahistory_remove_same_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]),
            history["open_export_helper_override_ahistory_remove_foreign_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            sum(1 for entry in entries if entry["helper_open_export_override_safe_ahistory_producer_evidence"]),
            history["open_export_helper_override_safe_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                and entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
            ),
            history[
                "open_export_helper_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_safe_ahistory_producer_evidence"]
            ),
            history["open_export_helper_override_missing_safe_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_ahistory_remove_source_lineage"]
            ),
            history["open_export_helper_override_missing_ahistory_remove_source_lineage_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
            ),
            history["open_export_helper_override_source_lineage_removed_source_edge_info_count"],
        )
        self.assertEqual(
            sum(
                1
                for entry in entries
                if entry["helper_open_export_override"]
                and not entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
            ),
            history["open_export_helper_override_missing_source_lineage_removed_source_edge_info_count"],
        )
        self.assertTrue(
            all(
                bool(entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"])
                == entry["helper_open_export_override_ahistory_remove_source_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                bool(entry["helper_open_export_override_ahistory_remove_source_edge_indices"])
                == entry["helper_open_export_override_ahistory_remove_source_lineage"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_ahistory_remove_source_lineage"]
                or entry["helper_open_export_override_ahistory_remove_source_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                bool(
                    set(entry["source_edge_indices"]).intersection(
                        entry["helper_open_export_override_ahistory_remove_source_edge_indices"]
                    )
                )
                == entry["helper_open_export_override_ahistory_remove_same_source_lineage"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                (
                    entry["helper_open_export_override_ahistory_remove_source_lineage"]
                    and not entry["helper_open_export_override_ahistory_remove_same_source_lineage"]
                )
                == entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                (
                    (
                        entry["helper_open_export_override_removed_source_edge_info"]
                        and bool(entry["source_edge_indices"])
                    )
                    or entry["helper_open_export_override_ahistory_remove_same_source_lineage"]
                )
                == entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                or not entry["helper_open_export_override_safe_ahistory_producer_evidence"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                bool(entry["helper_open_export_override_source_lineage_removed_source_edge_info_indices"])
                == entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                or bool(entry["source_edge_indices"])
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_source_edge_info"] == entry["helper_open_export_override"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_source_edge_info_consumed"]
                == entry["helper_open_export_override_forced_open_wire_compound_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_super_edge_root_edge_info"]
                or entry["helper_open_export_override_super_edge_member_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                or entry["helper_open_export_override_super_edge_root_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                or entry["helper_open_export_override_super_edge_root_edge_info"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                not entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                or entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                for entry in entries
            )
        )
        self.assertTrue(
            all(
                entry["helper_open_export_override_super_edge_root_edge_info"]
                or entry["helper_open_export_override_super_edge_root_edge_info_index"] == 0
                for entry in entries
            )
        )
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_source_edge_info_wire_info_count"],
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_source_edge_info_consumed_wire_info_count"],
            ledger["open_wire_compound_helper_open_export_override_forced_open_wire_compound_wire_info_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_open_wire_compound_eligible_wire_info_count"]
            + ledger["open_wire_compound_helper_open_export_override_forced_open_wire_compound_wire_info_count"],
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
        )

        edge_reason_total = 0
        child_wire_reason_total = 0
        for reason, (edge_key, child_wire_key) in self.HELPER_OVERRIDE_REASON_LEDGER_KEYS.items():
            expected = expected_counts.get(reason, 0)
            self.assertEqual(ledger[edge_key], expected)
            self.assertEqual(ledger[child_wire_key], expected)
            edge_reason_total += ledger[edge_key]
            child_wire_reason_total += ledger[child_wire_key]

        self.assertEqual(edge_reason_total, ledger["helper_open_export_override_edge_info_count"])
        self.assertEqual(
            child_wire_reason_total,
            ledger["open_wire_compound_helper_open_export_override_wire_info_count"],
        )
        self.assert_result_wire_producer_ledger(sketch)
        return entries

    def assert_result_wire_producer_ledger(self, sketch: dict[str, object]) -> None:
        ledger = sketch["wire_joiner_ledger"]
        history = sketch["wire_joiner_history_detail"]
        legacy_slot_count = ledger["helper_open_export_override_edge_info_count"]
        producer_entries = ledger["result_wire_producer_ledger_entries"]

        self.assertEqual(ledger["result_wire_producer_ledger_entry_count"], legacy_slot_count)
        self.assertEqual(ledger["migrated_legacy_helper_slot_count"], legacy_slot_count)
        self.assertEqual(len(producer_entries), legacy_slot_count)
        self.assertEqual(ledger["result_wire_producer_unknown_invariant_count"], 0)
        self.assertEqual(ledger["source_shape_identity_unknown_count"], 0)
        self.assertEqual(ledger["result_wire_producer_none_without_blocker_count"], 0)
        self.assertEqual(ledger["multi_member_root_direct_output_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"], 0)
        self.assertEqual(
            ledger["open_wire_compound_helper_open_export_override_helper_shape_wire_info_count"],
            ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"],
        )
        self.assertEqual(
            ledger["result_wire_producer_exported_without_helper_wire_info_count"],
            ledger["migrated_legacy_helper_slot_count"],
        )
        self.assertEqual(
            ledger["open_wire_compound_legacy_helper_shape_wire_info_count"],
            ledger["result_wire_producer_blocker_legacy_helper_shape_still_used_count"],
        )
        self.assertLessEqual(
            ledger["result_wire_producer_blocker_source_shape_identity_not_ready_count"]
            + ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"]
            + ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ]
            + ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ]
            + ledger[
                "result_wire_producer_blocker_same_source_sidecar_source_shape_identity_not_ready_count"
            ]
            + ledger["result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count"]
            + ledger[
                "result_wire_producer_blocker_source_shape_member_vertex_identity_not_ready_count"
            ]
            + ledger[
                "result_wire_producer_blocker_current_member_child_wire_identity_not_ready_count"
            ]
            + ledger["result_wire_producer_blocker_current_member_missing_sidecar_evidence_count"]
            + ledger[
                "result_wire_producer_blocker_current_member_root_open_producer_not_ready_count"
            ]
            + ledger["result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count"],
            ledger["open_wire_compound_legacy_helper_shape_wire_info_count"],
        )
        self.assertEqual(
            ledger["result_wire_producer_source_shape_ready_count"]
            + ledger["result_wire_producer_source_shape_not_ready_count"],
            ledger["result_wire_producer_child_wire_ready_count"],
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
        self.assertEqual(len(helper_entries), legacy_slot_count)
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
            if not history_entry["helper_open_export_override_source_edge_export_shape"]:
                self.assertIn(producer_entry["kind"], {"CurrentMemberChildWire", "SuperEdgeRoot"})
                self.assertTrue(
                    history_entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_candidate"
                    ]
                    or history_entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_current_member_edge_info"
                    ]
                )
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
        live_reset_entries = [
            entry
            for entry in helper_entries
            if entry["result_wire_producer_kind"] == "LiveResetOpenEdge"
        ]
        self.assertEqual(
            len(live_reset_entries),
            ledger["result_wire_producer_live_reset_open_edge_count"],
        )
        source_shape_purge_blockers = {
            "SourceShapeWouldPurgeOriginal",
            "LiveResetSourceShapeWouldPurgeOriginal",
            "CurrentMemberSourceShapeWouldPurgeOriginal",
        }
        for entry in helper_entries:
            self.assertIn(entry["result_wire_producer_kind"], valid_kinds)
            self.assertIn(entry["result_wire_producer_state"], valid_states)
            self.assertIn(entry["result_wire_producer_blocker"], valid_blockers)
            if entry["result_wire_producer_kind"] == "None":
                self.assertNotEqual(entry["result_wire_producer_blocker"], "None")
            if entry["result_wire_producer_kind"] == "LiveResetOpenEdge":
                self.assertTrue(entry["helper_open_export_override_open_wire_compound_eligible_edge_info"])
                self.assertFalse(entry["helper_open_export_override_source_edge_info_consumed"])
                self.assertIn(
                    entry["result_wire_producer_blocker"],
                    {
                        "None",
                        "SourceShapeIdentityNotReady",
                        "SourceShapeWouldPurgeOriginal",
                        "LiveResetSourceShapeWouldPurgeOriginal",
                    },
                )
            if entry["result_wire_producer_blocker"] in source_shape_purge_blockers:
                self.assertIn(
                    entry["result_wire_producer_kind"],
                    {"LiveResetOpenEdge", "CurrentMemberChildWire"},
                )
                if (
                    entry["result_wire_producer_blocker"]
                    == "LiveResetSourceShapeWouldPurgeOriginal"
                ):
                    self.assertEqual(entry["result_wire_producer_kind"], "LiveResetOpenEdge")
                if (
                    entry["result_wire_producer_blocker"]
                    == "CurrentMemberSourceShapeWouldPurgeOriginal"
                ):
                    self.assertEqual(entry["result_wire_producer_kind"], "CurrentMemberChildWire")
                if entry["result_wire_producer_kind"] == "LiveResetOpenEdge":
                    self.assertTrue(entry["helper_open_export_override_open_wire_compound_eligible_edge_info"])
                    self.assertFalse(entry["helper_open_export_override_source_edge_info_consumed"])
                else:
                    self.assertTrue(entry["helper_open_export_override_super_edge_member_edge_info"])
                    root_open_member = entry[
                        "helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"
                    ]
                    branch_child_member = (
                        entry[
                            "helper_open_export_override_super_edge_root_result_wire_producer_candidate"
                        ]
                        and entry[
                            "helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready"
                        ]
                        and entry[
                            "helper_open_export_override_super_edge_root_result_wire_producer_current_member_edge_info"
                        ]
                    )
                    self.assertTrue(root_open_member or branch_child_member)
            if (
                entry["result_wire_producer_blocker"]
                == "ForeignAHistorySourceShapeReadyLineageMismatch"
            ):
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                )
                self.assertTrue(entry["helper_open_export_override_ahistory_remove_source_edge_info"])
                self.assertEqual(entry["result_wire_producer_state"], "AHistoryEvidenceReady")
                self.assertIn(
                    entry["result_wire_producer_source_edge_info_index"],
                    entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"],
                )
                self.assertFalse(entry["helper_open_export_override_source_edge_export_shape"])
            if entry["result_wire_producer_blocker"] == "ForeignAHistorySourceLineage":
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                )
                self.assertTrue(entry["helper_open_export_override_ahistory_remove_source_edge_info"])
                self.assertEqual(entry["result_wire_producer_state"], "AHistoryEvidenceReady")
                self.assertIn(
                    entry["result_wire_producer_source_edge_info_index"],
                    entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"],
                )
            if (
                entry["result_wire_producer_blocker"]
                == "ForeignAHistorySourceShapeIdentityNotReady"
            ):
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                )
                self.assertTrue(entry["helper_open_export_override_ahistory_remove_source_edge_info"])
                self.assertEqual(entry["result_wire_producer_state"], "AHistoryEvidenceReady")
                self.assertIn(
                    entry["result_wire_producer_source_edge_info_index"],
                    entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"],
                )
                self.assertFalse(entry["helper_open_export_override_source_edge_export_shape"])
            if (
                entry["result_wire_producer_blocker"]
                == "ForeignAHistorySourceGeometryMismatch"
            ):
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                )
                self.assertTrue(entry["helper_open_export_override_ahistory_remove_source_edge_info"])
                self.assertEqual(entry["result_wire_producer_state"], "AHistoryEvidenceReady")
                self.assertIn(
                    entry["result_wire_producer_source_edge_info_index"],
                    entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"],
                )
                self.assertFalse(entry["helper_open_export_override_source_edge_export_shape"])
            if (
                entry["result_wire_producer_blocker"]
                in {
                    "SameSourceSidecarSourceShapeIdentityNotReady",
                    "SameSourceSidecarGeometryMismatch",
                }
            ):
                self.assertEqual(entry["result_wire_producer_kind"], "ExistingSourceEdge")
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                )
                self.assertTrue(
                    entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                )
                self.assertIn(
                    entry["result_wire_producer_source_edge_info_index"],
                    entry[
                        "helper_open_export_override_source_lineage_removed_source_edge_info_indices"
                    ],
                )
                self.assertFalse(entry["helper_open_export_override_source_edge_export_shape"])
            if (
                entry["result_wire_producer_blocker"]
                in {
                    "CurrentMemberChildWireIdentityNotReady",
                    "CurrentMemberMissingSidecarEvidence",
                    "CurrentMemberRootOpenProducerNotReady",
                    "CurrentMemberSidecarGeometryMismatch",
                }
            ):
                self.assertEqual(entry["result_wire_producer_kind"], "CurrentMemberChildWire")
                self.assertTrue(entry["helper_open_export_override_super_edge_member_edge_info"])
                self.assertTrue(
                    entry[
                        "helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"
                    ]
                )
                self.assertFalse(
                    entry[
                        "helper_open_export_override_super_edge_root_result_wire_producer_current_member_edge_info"
                    ]
                )
                if entry["helper_open_export_override_source_lineage_removed_source_edge_info"]:
                    self.assertIn(
                        entry["result_wire_producer_source_edge_info_index"],
                        entry[
                            "helper_open_export_override_source_lineage_removed_source_edge_info_indices"
                        ],
                    )
                if (
                    entry["result_wire_producer_blocker"]
                    == "CurrentMemberSidecarGeometryMismatch"
                ):
                    self.assertTrue(
                        entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                    )
                    self.assertIn(
                        entry["result_wire_producer_source_edge_info_index"],
                        entry[
                            "helper_open_export_override_source_lineage_removed_source_edge_info_indices"
                        ],
                    )
                if (
                    entry["result_wire_producer_blocker"]
                    == "CurrentMemberMissingSidecarEvidence"
                ):
                    self.assertFalse(
                        entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                    )
                if (
                    entry["result_wire_producer_blocker"]
                    == "CurrentMemberRootOpenProducerNotReady"
                ):
                    self.assertFalse(
                        entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                    )
                    self.assertFalse(
                        entry[
                            "helper_open_export_override_super_edge_root_result_wire_producer_candidate"
                        ]
                    )
            if (
                entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                and not entry[
                    "helper_open_export_override_ahistory_remove_foreign_source_lineage"
                ]
            ):
                self.assertNotEqual(
                    entry["result_wire_producer_blocker"],
                    "MissingAHistoryRemoveSource",
                )
            if entry["result_wire_producer_blocker"] == "MissingRemovedTargetEvidence":
                self.assertFalse(entry["helper_open_export_override_removed_target_edge_info"])
                self.assertTrue(
                    entry["helper_open_export_override_ahistory_remove_source_edge_info"]
                    or entry["helper_open_export_override_removed_source_edge_info"]
                    or entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                )
        source_shape_purge_entries = [
            entry
            for entry in helper_entries
            if entry["result_wire_producer_blocker"] in source_shape_purge_blockers
        ]
        self.assertEqual(
            len(source_shape_purge_entries),
            ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"]
            + ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ]
            + ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ],
        )

        if ledger["unowned_removal_ready_slot_count"] > 0:
            self.assertEqual(ledger["unowned_removal_ready_legacy_helper_shape_output_count"], 0)
            self.assertEqual(
                ledger["unowned_removal_current_member_producer_output_count"],
                ledger["unowned_removal_ready_slot_count"],
            )

    def assert_helper_override_has_no_open_wire_compound_eligible_candidate(
        self,
        ledger: dict[str, int],
    ) -> None:
        self.assert_helper_override_open_wire_compound_eligible_candidate_count(ledger, expected=0)

    def assert_helper_override_open_wire_compound_eligible_candidate_count(
        self,
        ledger: dict[str, int],
        *,
        expected: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_open_wire_compound_eligible_edge_info_count"],
            expected,
        )
        self.assertEqual(
            ledger["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_count"],
            expected,
        )
        self.assertEqual(
            ledger["helper_open_export_override_with_open_wire_compound_eligible_candidate_edge_info_count"],
            expected,
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_open_wire_compound_eligible_candidate_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"] - expected,
        )

    def assert_helper_override_export_blocked_by_lifecycle(
        self,
        ledger: dict[str, int],
        *,
        by_iteration: int,
        by_wire_info: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_export_blocked_by_iteration_edge_info_count"],
            by_iteration,
        )
        self.assertEqual(
            ledger["helper_open_export_override_export_blocked_by_wire_info_edge_info_count"],
            by_wire_info,
        )
        self.assertEqual(
            by_iteration + by_wire_info,
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
        )

    def assert_helper_override_has_no_source_edge_export_shape(self, ledger: dict[str, int]) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            0,
        )

    def assert_helper_override_source_edge_export_shape_count(
        self,
        ledger: dict[str, int],
        expected: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            expected,
        )

    def assert_helper_override_eligible_without_source_edge_export_shape_count(
        self,
        ledger: dict[str, int],
        expected: int,
    ) -> None:
        self.assertEqual(
            ledger[
                "helper_open_export_override_open_wire_compound_eligible_without_source_edge_export_shape_edge_info_count"
            ],
            expected,
        )
        self.assertLessEqual(
            expected,
            ledger["helper_open_export_override_open_wire_compound_eligible_edge_info_count"],
        )

    def assert_helper_override_all_use_source_edge_export_shape(self, ledger: dict[str, int]) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_source_edge_export_shape_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )

    def assert_helper_override_full_ahistory_producer_evidence(
        self,
        ledger: dict[str, int],
        *,
        full: int,
        without_source_edge_export_shape: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_full_ahistory_producer_evidence_edge_info_count"],
            full,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
            without_source_edge_export_shape,
        )

    def assert_helper_override_ahistory_remove_lineage_split(
        self,
        ledger: dict[str, int],
        *,
        same: int,
        foreign: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_same_source_lineage_edge_info_count"],
            same,
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_foreign_source_lineage_edge_info_count"],
            foreign,
        )
        self.assertEqual(
            same + foreign,
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
        )

    def assert_helper_override_safe_ahistory_producer_evidence(
        self,
        ledger: dict[str, int],
        *,
        safe: int,
    ) -> None:
        self.assertEqual(
            ledger["helper_open_export_override_safe_ahistory_producer_evidence_edge_info_count"],
            safe,
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_safe_ahistory_producer_evidence_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"] - safe,
        )

    def assert_helper_override_safe_ahistory_forced_split(
        self,
        ledger: dict[str, int],
        *,
        safe_forced: int,
        missing_safe_forced: int,
        full_forced: int,
        safe_without_full_forced: int,
    ) -> None:
        self.assertEqual(
            ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            safe_forced,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            missing_safe_forced,
        )
        self.assertEqual(
            safe_forced + missing_safe_forced,
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            full_forced,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            safe_without_full_forced,
        )
        self.assertEqual(full_forced + safe_without_full_forced, safe_forced)

    def assert_helper_override_super_edge_member_split(
        self,
        ledger: dict[str, int],
        *,
        member: int,
        with_root: int,
        root_eligible: int,
        root_open: int,
        root_closed: int,
        root_blocked_by_iteration: int,
        root_unowned_iteration_blocked: int,
        root_primary_iteration_blocked: int,
        root_secondary_iteration_blocked: int,
        root_missing_branch_iteration_blocked: int,
        root_result_wire_candidate: int,
        root_result_wire_candidate_full: int,
        root_result_wire_candidate_missing_full: int,
        root_result_wire_candidate_unowned: int,
        root_result_wire_candidate_unowned_ready: int,
        root_result_wire_candidate_primary: int,
        root_result_wire_candidate_secondary: int,
        root_result_wire_candidate_missing_branch: int,
        root_result_wire_candidate_missing_full_unowned: int,
        root_result_wire_candidate_missing_full_primary: int,
        root_result_wire_candidate_missing_full_secondary: int,
        root_result_wire_candidate_missing_full_missing_branch: int,
        current_member_child_wire_ready: int,
        current_member_child_wire_full_ahistory: int,
        root_blocked_by_wire_info: int,
        root_safe: int,
        root_full: int,
        root_eligible_safe: int,
        root_eligible_missing_safe: int,
        root_safe_without_eligible: int,
        root_full_without_eligible: int,
        root_safe_iteration_blocked: int,
        root_full_iteration_blocked: int,
        root_missing_safe_iteration_blocked: int,
        forced: int,
        missing_safe_forced: int,
        root_result_wire_built: int | None = None,
    ) -> None:
        if root_result_wire_built is None:
            root_result_wire_built = root_result_wire_candidate
        self.assertEqual(ledger["helper_open_export_override_super_edge_member_edge_info_count"], member)
        self.assertEqual(
            ledger["helper_open_export_override_super_edge_member_with_root_edge_info_count"],
            with_root,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
            root_eligible,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
            root_open,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
            root_closed,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
            root_blocked_by_iteration,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_unowned_removal_edge_info_count"
            ],
            root_unowned_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_primary_removal_edge_info_count"
            ],
            root_primary_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_secondary_removal_edge_info_count"
            ],
            root_secondary_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_iteration_blocked_missing_removal_branch_edge_info_count"
            ],
            root_missing_branch_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_edge_info_count"
            ],
            root_result_wire_candidate,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_full_ahistory_producer_evidence_edge_info_count"
            ],
            root_result_wire_candidate_full,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_edge_info_count"
            ],
            root_result_wire_candidate_missing_full,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_edge_info_count"
            ],
            root_result_wire_candidate_unowned,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_edge_info_count"
            ],
            root_result_wire_candidate_unowned_ready,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_wire_info_count"
            ],
            root_result_wire_candidate,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_info_count"
            ],
            root_result_wire_candidate_unowned_ready,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_wire_built_wire_info_count"
            ],
            root_result_wire_built,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_candidate_unowned_removal_child_wire_producer_ready_wire_built_wire_info_count"
            ],
            root_result_wire_candidate_unowned_ready,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_ready_wire_info_count"
            ],
            current_member_child_wire_ready,
        )
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_current_member_child_wire_producer_full_ahistory_evidence_wire_info_count"
            ],
            current_member_child_wire_full_ahistory,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_primary_removal_edge_info_count"
            ],
            root_result_wire_candidate_primary,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_secondary_removal_edge_info_count"
            ],
            root_result_wire_candidate_secondary,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_removal_branch_edge_info_count"
            ],
            root_result_wire_candidate_missing_branch,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_unowned_removal_edge_info_count"
            ],
            root_result_wire_candidate_missing_full_unowned,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_primary_removal_edge_info_count"
            ],
            root_result_wire_candidate_missing_full_primary,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_secondary_removal_edge_info_count"
            ],
            root_result_wire_candidate_missing_full_secondary,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_result_wire_producer_candidate_missing_full_ahistory_producer_evidence_missing_removal_branch_edge_info_count"
            ],
            root_result_wire_candidate_missing_full_missing_branch,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
            root_blocked_by_wire_info,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
            root_safe,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
            root_full,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
            root_eligible_safe,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
            root_eligible_missing_safe,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            root_safe_without_eligible,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            root_full_without_eligible,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            root_safe_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            root_full_iteration_blocked,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            root_missing_safe_iteration_blocked,
        )
        self.assertEqual(
            ledger["helper_open_export_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
            forced,
        )
        self.assertEqual(
            ledger[
                "helper_open_export_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            missing_safe_forced,
        )
        self.assertLessEqual(root_eligible, with_root)
        self.assertLessEqual(root_open, with_root)
        self.assertLessEqual(root_closed, with_root)
        self.assertLessEqual(root_open + root_closed, with_root)
        self.assertEqual(root_eligible + root_blocked_by_iteration + root_blocked_by_wire_info, with_root)
        self.assertLessEqual(root_safe, with_root)
        self.assertLessEqual(root_full, root_safe)
        self.assertEqual(root_eligible_safe + root_eligible_missing_safe, root_eligible)
        self.assertEqual(root_eligible_safe + root_safe_without_eligible, root_safe)
        self.assertLessEqual(root_full_without_eligible, root_safe_without_eligible)
        self.assertEqual(
            root_safe_iteration_blocked + root_missing_safe_iteration_blocked,
            root_blocked_by_iteration,
        )
        self.assertEqual(
            root_unowned_iteration_blocked
            + root_primary_iteration_blocked
            + root_secondary_iteration_blocked
            + root_missing_branch_iteration_blocked,
            root_blocked_by_iteration,
        )
        self.assertEqual(
            root_result_wire_candidate_full + root_result_wire_candidate_missing_full,
            root_result_wire_candidate,
        )
        self.assertEqual(
            root_result_wire_candidate_unowned
            + root_result_wire_candidate_primary
            + root_result_wire_candidate_secondary
            + root_result_wire_candidate_missing_branch,
            root_result_wire_candidate,
        )
        self.assertEqual(
            root_result_wire_candidate_missing_full_unowned
            + root_result_wire_candidate_missing_full_primary
            + root_result_wire_candidate_missing_full_secondary
            + root_result_wire_candidate_missing_full_missing_branch,
            root_result_wire_candidate_missing_full,
        )
        self.assertEqual(
            root_result_wire_candidate_unowned_ready + root_result_wire_candidate_missing_full_unowned,
            root_result_wire_candidate_unowned,
        )
        self.assertLessEqual(root_result_wire_candidate, root_blocked_by_iteration)
        self.assertLessEqual(root_result_wire_candidate_unowned_ready, root_result_wire_candidate_full)
        self.assertLessEqual(current_member_child_wire_ready, root_result_wire_built)
        self.assertLessEqual(current_member_child_wire_full_ahistory, current_member_child_wire_ready)
        self.assertLessEqual(root_result_wire_candidate_full, root_full_iteration_blocked)
        self.assertLessEqual(root_full_iteration_blocked, root_safe_iteration_blocked)
        self.assertLessEqual(root_safe_iteration_blocked, root_safe)
        self.assertLessEqual(root_full_iteration_blocked, root_full)
        self.assertLessEqual(with_root, member)
        self.assertLessEqual(forced, member)
        self.assertLessEqual(missing_safe_forced, forced)

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
        self.assertEqual(sketch["facemaker_history_status"], "history_partial:facemaker_buildface")
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
        self.assertEqual(ledger["graph_fallback_assigned_edge_info_count"], 0)
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
        self.assertEqual(ledger["tight_bound_live_split_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
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
        self.assertEqual(ledger["temporary_result_wire_edge_info_count"], 0)
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
        self.assertEqual(history["open_export_edge_count"], ledger["open_export_edge_info_count"])
        self.assertEqual(
            history["open_export_source_lineage_edge_count"],
            ledger["source_lineage_open_export_edge_info_count"],
        )
        self.assertEqual(history["open_export_missing_source_lineage_edge_count"], 0)
        self.assertEqual(history["open_export_purge_bridge_edge_count"], 0)
        entries = self.assert_open_export_history_entries(history)
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
        self.assertEqual(ledger["branch_search_outside_candidate_count"], 0)
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertGreaterEqual(ledger["new_wire_seed_wire_info_count"], 1)
        self.assertEqual(ledger["split_wire_candidate_count"], 0)
        self.assertEqual(ledger["split_wire_edge_info_count"], 0)
        self.assertGreaterEqual(ledger["done_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_owned_edge_info_count"], ledger["primary_owned_edge_info_count"])
        self.assertEqual(ledger["owner_propagation_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_unassigned_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(ledger["tight_bound_exhaust_done_wire_info_count"], 0)
        self.assertEqual(
            ledger["tight_bound_exhaust_primary_reset_edge_info_count"],
            ledger["repeated_split_exhaust_removed_unowned_edge_info_count"],
        )
        self.assertEqual(ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(
            ledger["secondary_owned_edge_info_count"]
            + ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"],
            ledger["exhaust_secondary_owner_edge_info_count"],
        )
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["graph_secondary_owner_edge_info_count"], 0)
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
        self.assertEqual(named_shape["sketch_internal_history_status"], "history_partial:facemaker_buildface")
        self.assertIn("facemaker_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:modified", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:generated", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:deleted", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertEqual(internal_history["source_edge_count"], 5)
        self.assertEqual(internal_history["bounded_face_count"], 2)
        self.assertFalse(internal_history["pre_split_history"])
        self.assertTrue(internal_history["splitter_history"])
        self.assertEqual(internal_history["wire_joiner_source_edge_count"], 5)
        self.assertEqual(internal_history["wire_joiner_split_result_edge_count"], 9)
        self.assertGreater(internal_history["wire_joiner_modified_history_count"], 0)
        self.assertGreater(internal_history["wire_joiner_generated_history_count"], 0)
        self.assertGreater(internal_history["wire_joiner_deleted_history_count"], 0)
        self.assertGreater(internal_history["wire_joiner_open_export_edge_count"], 0)
        internal_entries = internal_history["wire_joiner_open_export_history_entries"]
        self.assertIsInstance(internal_entries, list)
        self.assertEqual(
            len(internal_entries),
            internal_history["wire_joiner_open_export_edge_count"],
        )
        self.assertTrue(all(entry["source_edge_indices"] for entry in internal_entries))
        self.assertTrue(internal_history["wire_joiner_splitter_history"])
        self.assertTrue(internal_history["wire_joiner_final_export_history"])
        self.assertNotIn("Edge5", named_shape["element_map"])
        self.assertGreaterEqual(len(split_entries), 2)
        for entry in split_entries:
            self.assertTrue(entry["element"].startswith("InternalEdge"))
            self.assertEqual(named_shape["elements"][entry["element"]]["status"], "split")

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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(ledger["temporary_result_wire_edge_info_count"], 0)
        self.assertGreater(ledger["helper_open_export_override_edge_info_count"], 0)
        self.assertEqual(
            ledger["helper_open_export_override_edge_info_count"],
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
        self.assertEqual(ledger["graph_secondary_owner_edge_info_count"], 0)
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
        self.assertEqual(ledger["tight_bound_live_split_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
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
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"],
        )
        self.assertGreater(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_removed_source_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            10,
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            2,
        )
        self.assertGreater(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 1)
        self.assert_helper_override_open_wire_compound_eligible_candidate_count(ledger, expected=1)
        self.assert_helper_override_export_blocked_by_lifecycle(
            ledger,
            by_iteration=11,
            by_wire_info=0,
        )
        self.assert_helper_override_full_ahistory_producer_evidence(
            ledger,
            full=6,
            without_source_edge_export_shape=0,
        )
        self.assert_helper_override_ahistory_remove_lineage_split(ledger, same=6, foreign=1)
        self.assert_helper_override_safe_ahistory_producer_evidence(ledger, safe=6)
        self.assert_helper_override_safe_ahistory_forced_split(
            ledger,
            safe_forced=6,
            missing_safe_forced=5,
            full_forced=6,
            safe_without_full_forced=0,
        )
        self.assert_helper_override_super_edge_member_split(
            ledger,
            member=4,
            with_root=4,
            root_eligible=0,
            root_open=4,
            root_closed=0,
            root_blocked_by_iteration=4,
            root_unowned_iteration_blocked=4,
            root_primary_iteration_blocked=0,
            root_secondary_iteration_blocked=0,
            root_missing_branch_iteration_blocked=0,
            root_result_wire_candidate=4,
            root_result_wire_candidate_full=4,
            root_result_wire_candidate_missing_full=0,
            root_result_wire_candidate_unowned=4,
            root_result_wire_candidate_unowned_ready=4,
            root_result_wire_candidate_primary=0,
            root_result_wire_candidate_secondary=0,
            root_result_wire_candidate_missing_branch=0,
            root_result_wire_candidate_missing_full_unowned=0,
            root_result_wire_candidate_missing_full_primary=0,
            root_result_wire_candidate_missing_full_secondary=0,
            root_result_wire_candidate_missing_full_missing_branch=0,
            current_member_child_wire_ready=4,
            current_member_child_wire_full_ahistory=4,
            root_blocked_by_wire_info=0,
            root_safe=4,
            root_full=4,
            root_eligible_safe=0,
            root_eligible_missing_safe=0,
            root_safe_without_eligible=4,
            root_full_without_eligible=4,
            root_safe_iteration_blocked=4,
            root_full_iteration_blocked=4,
            root_missing_safe_iteration_blocked=0,
            forced=4,
            missing_safe_forced=4,
            root_result_wire_built=4,
        )
        self.assertEqual(ledger["result_wire_producer_live_reset_open_edge_count"], 1)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_removed_target_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_foreign_ahistory_source_lineage_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_shape_ready_lineage_mismatch_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_shape_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_geometry_mismatch_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(
            ledger["result_wire_producer_blocker_current_member_root_open_producer_not_ready_count"],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(ledger["open_wire_compound_legacy_helper_shape_wire_info_count"], 0)
        self.assert_helper_override_source_edge_export_shape_count(ledger, 8)
        self.assert_helper_override_eligible_without_source_edge_export_shape_count(ledger, 0)
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
        self.assertEqual(ledger["graph_fallback_assigned_edge_info_count"], 0)
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
        self.assertEqual(history["open_export_edge_count"], ledger["open_export_edge_info_count"])
        self.assertEqual(
            history["open_export_helper_override_edge_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_helper_override_missing_source_lineage_edge_count"],
            ledger["source_lineage_missing_open_export_edge_info_count"],
        )
        self.assertGreater(history["open_export_source_lineage_edge_count"], 0)
        self.assertEqual(history["open_export_missing_source_lineage_edge_count"], 0)
        self.assertEqual(
            history["open_export_source_lineage_edge_count"],
            history["open_export_edge_count"],
        )
        entries = self.assert_helper_open_export_override_reason_ledger(
            sketch,
            {"partial_junction_open_cutter": 8},
        )
        self.assertTrue(all(entry["source_edge_indices"] for entry in entries))
        self.assertTrue(all(entry["helper_open_export_override"] for entry in entries))
        self.assertTrue(
            all(entry["helper_open_export_override_reason"] == "partial_junction_open_cutter" for entry in entries)
        )
        self.assertTrue(all(not entry["purge_bridge"] for entry in entries))
        internal_history = result["named_shapes"]["Sketch.InternalShape"]["sketch_internal_history"]
        internal_entries = internal_history["wire_joiner_open_export_history_entries"]
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_full_ahistory_producer_evidence_edge_info_count"
            ],
            history["open_export_helper_override_full_ahistory_producer_evidence_edge_info_count"],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_without_source_edge_export_shape_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_safe_ahistory_producer_evidence_without_full_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history["wire_joiner_open_export_helper_override_super_edge_member_edge_info_count"],
            history["open_export_helper_override_super_edge_member_edge_info_count"],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_with_root_edge_info_count"
            ],
            history["open_export_helper_override_super_edge_member_with_root_edge_info_count"],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_closed_lifecycle_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_iteration_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_export_blocked_by_wire_info_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_and_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_open_wire_compound_eligible_missing_safe_ahistory_producer_evidence_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_without_open_wire_compound_eligible_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_full_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_root_missing_safe_ahistory_producer_evidence_iteration_blocked_edge_info_count"
            ],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_forced_open_wire_compound_edge_info_count"
            ],
            history["open_export_helper_override_super_edge_member_forced_open_wire_compound_edge_info_count"],
        )
        self.assertEqual(
            internal_history[
                "wire_joiner_open_export_helper_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
            history[
                "open_export_helper_override_super_edge_member_missing_safe_ahistory_producer_evidence_forced_open_wire_compound_edge_info_count"
            ],
        )
        self.assertEqual(
            [entry["helper_open_export_override_reason"] for entry in internal_entries],
            [entry["helper_open_export_override_reason"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_source_edge_info"] for entry in internal_entries],
            [entry["helper_open_export_override_source_edge_info"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_source_edge_info_index"] for entry in internal_entries],
            [entry["helper_open_export_override_source_edge_info_index"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_source_edge_info_consumed"] for entry in internal_entries],
            [entry["helper_open_export_override_source_edge_info_consumed"] for entry in entries],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_open_wire_compound_eligible_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [entry["helper_open_export_override_forced_open_wire_compound_edge_info"] for entry in internal_entries],
            [entry["helper_open_export_override_forced_open_wire_compound_edge_info"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_source_edge_export_shape"] for entry in internal_entries],
            [entry["helper_open_export_override_source_edge_export_shape"] for entry in entries],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_full_ahistory_producer_evidence"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_full_ahistory_producer_evidence"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_member_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_member_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_edge_info_index"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_edge_info_index"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_open_wire_compound_eligible_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_open_lifecycle_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_open_lifecycle_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_closed_lifecycle_edge_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_closed_lifecycle_edge_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_selected_iteration"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_selected_iteration"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_selected_wire_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_selected_wire_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_selected_wire_info2"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_selected_wire_info2"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_export_blocked_by_iteration"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_export_blocked_by_wire_info"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_export_blocked_by_wire_info"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_super_edge_root_full_ahistory_producer_evidence"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry[
                    "helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in internal_entries
            ],
            [
                entry[
                    "helper_open_export_override_super_edge_root_safe_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry[
                    "helper_open_export_override_super_edge_root_full_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in internal_entries
            ],
            [
                entry[
                    "helper_open_export_override_super_edge_root_full_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in entries
            ],
        )
        self.assertEqual(
            [
                entry[
                    "helper_open_export_override_super_edge_root_missing_safe_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in internal_entries
            ],
            [
                entry[
                    "helper_open_export_override_super_edge_root_missing_safe_ahistory_producer_evidence_iteration_blocked"
                ]
                for entry in entries
            ],
        )
        self.assertEqual(
            [entry["helper_open_export_override_selected_iteration"] for entry in internal_entries],
            [entry["helper_open_export_override_selected_iteration"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_selected_wire_info"] for entry in internal_entries],
            [entry["helper_open_export_override_selected_wire_info"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_selected_wire_info2"] for entry in internal_entries],
            [entry["helper_open_export_override_selected_wire_info2"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_export_blocked_by_iteration"] for entry in internal_entries],
            [entry["helper_open_export_override_export_blocked_by_iteration"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_export_blocked_by_wire_info"] for entry in internal_entries],
            [entry["helper_open_export_override_export_blocked_by_wire_info"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_candidate_edge_info_indices"] for entry in internal_entries],
            [entry["helper_open_export_override_candidate_edge_info_indices"] for entry in entries],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_open_wire_compound_eligible_candidate_edge_info_indices"]
                for entry in entries
            ],
        )
        self.assertEqual(
            [entry["helper_open_export_override_source_lineage_removed_source_edge_info"] for entry in internal_entries],
            [entry["helper_open_export_override_source_lineage_removed_source_edge_info"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_ahistory_remove_same_source_lineage"] for entry in internal_entries],
            [entry["helper_open_export_override_ahistory_remove_same_source_lineage"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"] for entry in internal_entries],
            [entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"] for entry in entries],
        )
        self.assertEqual(
            [entry["helper_open_export_override_safe_ahistory_producer_evidence"] for entry in internal_entries],
            [entry["helper_open_export_override_safe_ahistory_producer_evidence"] for entry in entries],
        )
        self.assertEqual(
            [
                entry["helper_open_export_override_source_lineage_removed_source_edge_info_indices"]
                for entry in internal_entries
            ],
            [
                entry["helper_open_export_override_source_lineage_removed_source_edge_info_indices"]
                for entry in entries
            ],
        )
        self.assertGreater(history["modified_history_count"], 0)
        self.assertGreater(history["deleted_history_count"], 0)
        self.assertTrue(history["splitter_history"])
        self.assertTrue(history["final_export_history"])
        self.assertEqual(ledger["temporary_result_wire_edge_info_count"], 0)
        self.assertGreater(ledger["helper_open_export_override_edge_info_count"], 0)
        self.assertEqual(
            ledger["helper_open_export_override_edge_info_count"],
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
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            3,
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            5,
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 2)
        self.assert_helper_override_open_wire_compound_eligible_candidate_count(ledger, expected=2)
        self.assert_helper_override_export_blocked_by_lifecycle(
            ledger,
            by_iteration=6,
            by_wire_info=0,
        )
        self.assert_helper_override_full_ahistory_producer_evidence(
            ledger,
            full=2,
            without_source_edge_export_shape=0,
        )
        self.assert_helper_override_ahistory_remove_lineage_split(ledger, same=2, foreign=0)
        self.assert_helper_override_safe_ahistory_producer_evidence(ledger, safe=2)
        self.assert_helper_override_safe_ahistory_forced_split(
            ledger,
            safe_forced=2,
            missing_safe_forced=4,
            full_forced=2,
            safe_without_full_forced=0,
        )
        self.assert_helper_override_super_edge_member_split(
            ledger,
            member=4,
            with_root=4,
            root_eligible=2,
            root_open=4,
            root_closed=0,
            root_blocked_by_iteration=2,
            root_unowned_iteration_blocked=2,
            root_primary_iteration_blocked=0,
            root_secondary_iteration_blocked=0,
            root_missing_branch_iteration_blocked=0,
            root_result_wire_candidate=2,
            root_result_wire_candidate_full=2,
            root_result_wire_candidate_missing_full=0,
            root_result_wire_candidate_unowned=2,
            root_result_wire_candidate_unowned_ready=2,
            root_result_wire_candidate_primary=0,
            root_result_wire_candidate_secondary=0,
            root_result_wire_candidate_missing_branch=0,
            root_result_wire_candidate_missing_full_unowned=0,
            root_result_wire_candidate_missing_full_primary=0,
            root_result_wire_candidate_missing_full_secondary=0,
            root_result_wire_candidate_missing_full_missing_branch=0,
            current_member_child_wire_ready=4,
            current_member_child_wire_full_ahistory=2,
            root_blocked_by_wire_info=0,
            root_safe=2,
            root_full=2,
            root_eligible_safe=0,
            root_eligible_missing_safe=2,
            root_safe_without_eligible=2,
            root_full_without_eligible=2,
            root_safe_iteration_blocked=2,
            root_full_iteration_blocked=2,
            root_missing_safe_iteration_blocked=0,
            forced=4,
            missing_safe_forced=4,
            root_result_wire_built=4,
        )
        self.assertEqual(ledger["result_wire_producer_live_reset_open_edge_count"], 2)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_ahistory_remove_source_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_removed_target_evidence_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_source_shape_member_vertex_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_multi_member_root_pending_suppression_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_child_wire_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_missing_sidecar_evidence_count"], 0)
        self.assertEqual(
            ledger["result_wire_producer_blocker_current_member_root_open_producer_not_ready_count"],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(ledger["open_wire_compound_legacy_helper_shape_wire_info_count"], 0)
        self.assert_helper_override_source_edge_export_shape_count(ledger, 4)
        self.assert_helper_override_eligible_without_source_edge_export_shape_count(ledger, 0)
        self.assert_super_edge_lifecycle_ledger(ledger)
        self.assertGreaterEqual(ledger["ordered_wire_info_count"], 1)
        self.assertLessEqual(ledger["ordered_vertex_count"], ledger["edge_info_count"])
        self.assertLessEqual(ledger["iteration2_marked_edge_info_count"], ledger["edge_info_count"])
        self.assertLessEqual(ledger["helper_open_export_override_edge_info_count"], ledger["edge_info_count"])
        self.assertGreater(ledger["branch_search_candidate_count"], 0)
        self.assertGreaterEqual(ledger["branch_search_seed_wire_info_count"], 1)
        self.assertGreater(ledger["branch_search_inside_candidate_count"], 0)
        self.assertEqual(ledger["branch_search_outside_candidate_count"], 0)
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertGreaterEqual(ledger["new_wire_seed_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_wire_info_count"], 1)
        self.assertGreaterEqual(ledger["done_owned_edge_info_count"], ledger["primary_owned_edge_info_count"])
        self.assertGreaterEqual(ledger["split_wire_candidate_count"], 1)
        self.assertGreater(ledger["split_wire_edge_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
        self.assertEqual(
            ledger["owner_propagation_candidate_count"],
            ledger["owner_propagation_unassigned_candidate_count"]
            + ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertEqual(ledger["owner_propagation_unassigned_candidate_count"], 0)
        self.assertGreater(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(
            ledger["owner_propagation_other_wire_live_edge_info_count"],
            ledger["owner_propagation_other_wire_candidate_count"],
        )
        self.assertGreater(ledger["tight_bound_exhaust_done_wire_info_count"], 0)
        self.assertGreater(
            ledger["tight_bound_exhaust_primary_reset_edge_info_count"]
            + ledger["tight_bound_exhaust_primary_reset_blocked_edge_info_count"],
            0,
        )
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(ledger["secondary_owned_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], ledger["exhaust_secondary_owner_edge_info_count"])
        self.assertGreater(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["graph_secondary_owner_edge_info_count"], 0)
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
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            8,
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            2,
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 1)
        self.assert_helper_override_open_wire_compound_eligible_candidate_count(ledger, expected=1)
        self.assert_helper_override_export_blocked_by_lifecycle(
            ledger,
            by_iteration=9,
            by_wire_info=0,
        )
        self.assert_helper_override_full_ahistory_producer_evidence(
            ledger,
            full=4,
            without_source_edge_export_shape=0,
        )
        self.assert_helper_override_ahistory_remove_lineage_split(ledger, same=4, foreign=1)
        self.assert_helper_override_safe_ahistory_producer_evidence(ledger, safe=4)
        self.assert_helper_override_safe_ahistory_forced_split(
            ledger,
            safe_forced=4,
            missing_safe_forced=5,
            full_forced=4,
            safe_without_full_forced=0,
        )
        self.assert_helper_override_super_edge_member_split(
            ledger,
            member=4,
            with_root=4,
            root_eligible=0,
            root_open=4,
            root_closed=0,
            root_blocked_by_iteration=4,
            root_unowned_iteration_blocked=4,
            root_primary_iteration_blocked=0,
            root_secondary_iteration_blocked=0,
            root_missing_branch_iteration_blocked=0,
            root_result_wire_candidate=4,
            root_result_wire_candidate_full=4,
            root_result_wire_candidate_missing_full=0,
            root_result_wire_candidate_unowned=4,
            root_result_wire_candidate_unowned_ready=4,
            root_result_wire_candidate_primary=0,
            root_result_wire_candidate_secondary=0,
            root_result_wire_candidate_missing_branch=0,
            root_result_wire_candidate_missing_full_unowned=0,
            root_result_wire_candidate_missing_full_primary=0,
            root_result_wire_candidate_missing_full_secondary=0,
            root_result_wire_candidate_missing_full_missing_branch=0,
            current_member_child_wire_ready=4,
            current_member_child_wire_full_ahistory=4,
            root_blocked_by_wire_info=0,
            root_safe=4,
            root_full=4,
            root_eligible_safe=0,
            root_eligible_missing_safe=0,
            root_safe_without_eligible=4,
            root_full_without_eligible=4,
            root_safe_iteration_blocked=4,
            root_full_iteration_blocked=4,
            root_missing_safe_iteration_blocked=0,
            forced=4,
            missing_safe_forced=4,
        )
        self.assertEqual(ledger["result_wire_producer_live_reset_open_edge_count"], 1)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_removed_target_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_foreign_ahistory_source_lineage_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_shape_ready_lineage_mismatch_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_shape_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_foreign_ahistory_source_geometry_mismatch_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_missing_sidecar_evidence_count"], 0)
        self.assertEqual(
            ledger["result_wire_producer_blocker_current_member_root_open_producer_not_ready_count"],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_identity_not_ready_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(ledger["open_wire_compound_legacy_helper_shape_wire_info_count"], 0)
        self.assert_helper_override_source_edge_export_shape_count(ledger, 6)
        self.assert_helper_override_eligible_without_source_edge_export_shape_count(ledger, 0)
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
        self.assertGreater(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_removed_source_edge_info_count"],
        )
        self.assertGreater(
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"],
            ledger["helper_open_export_override_removed_source_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            0,
        )
        self.assertEqual(ledger["repeated_split_exhaust_rerun_miss_live_reset_edge_info_count"], 3)
        self.assert_helper_override_open_wire_compound_eligible_candidate_count(ledger, expected=3)
        self.assert_helper_override_export_blocked_by_lifecycle(
            ledger,
            by_iteration=12,
            by_wire_info=0,
        )
        self.assert_helper_override_full_ahistory_producer_evidence(
            ledger,
            full=5,
            without_source_edge_export_shape=0,
        )
        self.assert_helper_override_ahistory_remove_lineage_split(ledger, same=7, foreign=2)
        self.assert_helper_override_safe_ahistory_producer_evidence(ledger, safe=7)
        self.assert_helper_override_safe_ahistory_forced_split(
            ledger,
            safe_forced=7,
            missing_safe_forced=5,
            full_forced=5,
            safe_without_full_forced=2,
        )
        self.assert_helper_override_super_edge_member_split(
            ledger,
            member=3,
            with_root=3,
            root_eligible=1,
            root_open=3,
            root_closed=0,
            root_blocked_by_iteration=2,
            root_unowned_iteration_blocked=1,
            root_primary_iteration_blocked=1,
            root_secondary_iteration_blocked=0,
            root_missing_branch_iteration_blocked=0,
            root_result_wire_candidate=2,
            root_result_wire_candidate_full=2,
            root_result_wire_candidate_missing_full=0,
            root_result_wire_candidate_unowned=1,
            root_result_wire_candidate_unowned_ready=1,
            root_result_wire_candidate_primary=1,
            root_result_wire_candidate_secondary=0,
            root_result_wire_candidate_missing_branch=0,
            root_result_wire_candidate_missing_full_unowned=0,
            root_result_wire_candidate_missing_full_primary=0,
            root_result_wire_candidate_missing_full_secondary=0,
            root_result_wire_candidate_missing_full_missing_branch=0,
            current_member_child_wire_ready=3,
            current_member_child_wire_full_ahistory=2,
            root_blocked_by_wire_info=0,
            root_safe=2,
            root_full=2,
            root_eligible_safe=0,
            root_eligible_missing_safe=1,
            root_safe_without_eligible=2,
            root_full_without_eligible=2,
            root_safe_iteration_blocked=2,
            root_full_iteration_blocked=2,
            root_missing_safe_iteration_blocked=0,
            forced=3,
            missing_safe_forced=3,
            root_result_wire_built=3,
        )
        self.assertEqual(ledger["result_wire_producer_live_reset_open_edge_count"], 3)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_full_ahistory_producer_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_foreign_ahistory_source_lineage_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_missing_removed_target_evidence_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_identity_not_ready_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_same_source_sidecar_source_shape_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_same_source_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_source_shape_member_vertex_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_multi_member_root_pending_suppression_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_child_wire_identity_not_ready_count"
            ],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_missing_sidecar_evidence_count"], 0)
        self.assertEqual(
            ledger["result_wire_producer_blocker_current_member_root_open_producer_not_ready_count"],
            0,
        )
        self.assertEqual(ledger["result_wire_producer_blocker_current_member_sidecar_geometry_mismatch_count"], 0)
        self.assertEqual(ledger["result_wire_producer_blocker_source_shape_would_purge_original_count"], 0)
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_live_reset_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(
            ledger[
                "result_wire_producer_blocker_current_member_source_shape_would_purge_original_count"
            ],
            0,
        )
        self.assertEqual(ledger["open_wire_compound_legacy_helper_shape_wire_info_count"], 0)
        self.assert_helper_override_source_edge_export_shape_count(ledger, 12)
        self.assert_helper_override_eligible_without_source_edge_export_shape_count(ledger, 0)
        self.assertEqual(
            ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_member_suppressed_output_wire_info_count"
            ]
            - ledger[
                "open_wire_compound_helper_open_export_override_super_edge_root_result_wire_producer_unowned_removal_child_wire_producer_ready_output_wire_info_count"
            ],
            2,
        )
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
        self.assertEqual(
            ledger["helper_open_export_override_removed_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(ledger["helper_open_export_override_missing_removed_source_edge_info_count"], 0)
        self.assertEqual(
            ledger["helper_open_export_override_removed_target_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_ahistory_remove_source_lineage_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_source_lineage_removed_source_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assertEqual(
            ledger["helper_open_export_override_missing_source_lineage_removed_source_edge_info_count"],
            0,
        )
        self.assertEqual(ledger["helper_open_export_override_open_wire_compound_eligible_edge_info_count"], 0)
        self.assertEqual(
            ledger["helper_open_export_override_forced_open_wire_compound_edge_info_count"],
            ledger["helper_open_export_override_edge_info_count"],
        )
        self.assert_helper_override_has_no_open_wire_compound_eligible_candidate(ledger)
        self.assert_helper_override_export_blocked_by_lifecycle(
            ledger,
            by_iteration=1,
            by_wire_info=0,
        )
        self.assert_helper_override_full_ahistory_producer_evidence(
            ledger,
            full=1,
            without_source_edge_export_shape=0,
        )
        self.assert_helper_override_ahistory_remove_lineage_split(ledger, same=1, foreign=0)
        self.assert_helper_override_safe_ahistory_producer_evidence(ledger, safe=1)
        self.assert_helper_override_safe_ahistory_forced_split(
            ledger,
            safe_forced=1,
            missing_safe_forced=0,
            full_forced=1,
            safe_without_full_forced=0,
        )
        self.assert_helper_override_super_edge_member_split(
            ledger,
            member=0,
            with_root=0,
            root_eligible=0,
            root_open=0,
            root_closed=0,
            root_blocked_by_iteration=0,
            root_unowned_iteration_blocked=0,
            root_primary_iteration_blocked=0,
            root_secondary_iteration_blocked=0,
            root_missing_branch_iteration_blocked=0,
            root_result_wire_candidate=0,
            root_result_wire_candidate_full=0,
            root_result_wire_candidate_missing_full=0,
            root_result_wire_candidate_unowned=0,
            root_result_wire_candidate_unowned_ready=0,
            root_result_wire_candidate_primary=0,
            root_result_wire_candidate_secondary=0,
            root_result_wire_candidate_missing_branch=0,
            root_result_wire_candidate_missing_full_unowned=0,
            root_result_wire_candidate_missing_full_primary=0,
            root_result_wire_candidate_missing_full_secondary=0,
            root_result_wire_candidate_missing_full_missing_branch=0,
            current_member_child_wire_ready=0,
            current_member_child_wire_full_ahistory=0,
            root_blocked_by_wire_info=0,
            root_safe=0,
            root_full=0,
            root_eligible_safe=0,
            root_eligible_missing_safe=0,
            root_safe_without_eligible=0,
            root_full_without_eligible=0,
            root_safe_iteration_blocked=0,
            root_full_iteration_blocked=0,
            root_missing_safe_iteration_blocked=0,
            forced=0,
            missing_safe_forced=0,
        )
        self.assert_helper_override_all_use_source_edge_export_shape(ledger)
        self.assert_helper_override_eligible_without_source_edge_export_shape_count(ledger, 0)
        self.assertEqual(ledger["source_lineage_missing_open_export_edge_info_count"], 0)
        self.assertEqual(history["open_export_helper_override_missing_source_lineage_edge_count"], 0)
        self.assertTrue(
            all(
                entry["helper_open_export_override_removed_source_edge_info"]
                and bool(entry["source_edge_indices"])
                and entry["helper_open_export_override_removed_target_edge_info"]
                and entry["helper_open_export_override_ahistory_remove_source_edge_info"]
                and bool(entry["helper_open_export_override_ahistory_remove_source_edge_info_indices"])
                and entry["helper_open_export_override_ahistory_remove_source_lineage"]
                and bool(entry["helper_open_export_override_ahistory_remove_source_edge_indices"])
                and entry["helper_open_export_override_ahistory_remove_same_source_lineage"]
                and not entry["helper_open_export_override_ahistory_remove_foreign_source_lineage"]
                and entry["helper_open_export_override_source_lineage_removed_source_edge_info"]
                and bool(entry["helper_open_export_override_source_lineage_removed_source_edge_info_indices"])
                for entry in entries
                if entry["helper_open_export_override"]
            )
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
        self.assertEqual(sketch["facemaker_history_status"], "history_partial:facemaker_buildface")
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

        self.assertEqual(named_shape["sketch_internal_history_status"], "history_partial:facemaker_buildface")
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
        self.assertEqual(ledger["graph_fallback_assigned_edge_info_count"], 0)
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
        self.assertEqual(ledger["temporary_result_wire_edge_info_count"], 0)
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
        self.assertEqual(history["open_export_edge_count"], ledger["open_export_edge_info_count"])
        self.assertEqual(
            history["open_export_source_lineage_edge_count"],
            ledger["source_lineage_open_export_edge_info_count"],
        )
        self.assertEqual(
            history["open_export_purge_bridge_edge_count"],
            ledger["source_identity_purge_bridge_edge_info_count"],
        )
        self.assertEqual(history["open_export_missing_source_lineage_edge_count"], 0)
        entries = self.assert_open_export_history_entries(history)
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
        self.assertEqual(ledger["branch_search_outside_candidate_count"], 0)
        self.assertEqual(ledger["new_wire_seed_candidate_count"], ledger["branch_search_inside_candidate_count"])
        self.assertEqual(ledger["new_wire_seed_wire_info_count"], 0)
        self.assertEqual(ledger["split_wire_candidate_count"], 0)
        self.assertEqual(ledger["split_wire_edge_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_info_count"], 0)
        self.assertEqual(ledger["tight_bound_live_split_wire_edge_info_count"], 0)
        self.assertEqual(ledger["done_wire_info_count"], 0)
        self.assertEqual(ledger["done_owned_edge_info_count"], 0)
        self.assertEqual(ledger["owner_propagation_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_unassigned_candidate_count"], 0)
        self.assertEqual(ledger["owner_propagation_other_wire_candidate_count"], 0)
        self.assertEqual(ledger["exhaust_seed_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_shared_owner_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_done_secondary_edge_info_count"], 0)
        self.assertEqual(ledger["exhaust_secondary_owner_edge_info_count"], 0)
        self.assertEqual(ledger["graph_secondary_owner_edge_info_count"], 0)
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
        self.assertEqual(extrusion["topo_naming"], "known_gap:taper_history")
        self.assertEqual(extrusion["topo_naming_history"], "history_partial:taper")
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

    def test_p5_part_extrusion_supports_reverse_taper(self) -> None:
        result = self.run_recompute("part-extrusion-reverse-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertEqual(extrusion["topo_naming"], "known_gap:taper_history")
        self.assertEqual(extrusion["topo_naming_history"], "history_partial:taper")
        self.assertGreater(extrusion["volume"], 0.0)
        self.assertLess(extrusion["bbox"]["min"][2], -5.99)
        self.assertLess(extrusion["bbox"]["max"][2], 1.0)

    def test_p5_part_extrusion_supports_two_sided_taper(self) -> None:
        result = self.run_recompute("part-extrusion-two-sided-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertEqual(extrusion["topo_naming"], "known_gap:taper_history")
        self.assertEqual(extrusion["topo_naming_history"], "history_partial:taper")
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
