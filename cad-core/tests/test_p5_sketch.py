from __future__ import annotations

import ctypes
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import ROOT, CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import ROOT, CadCoreFixtureTestCase


class CadCoreP5SketchTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_c3m3_full_rank_solver(
        self,
        sketch: dict,
        degrees_of_freedom: int,
        constraint_rank: int,
        blocked_groups: int = 0,
    ) -> None:
        self.assertEqual(sketch["solver_state"], "underconstrained")
        self.assertEqual(sketch["solver_degrees_of_freedom"], degrees_of_freedom)
        self.assertEqual(sketch["solver_dof_status"], "request_local_full_rank")
        self.assertEqual(sketch["solver_constraint_rank"], constraint_rank)
        self.assertEqual(sketch["solver_dependent_parameter_groups"], 1)
        self.assertEqual(sketch["solver_blocked_dependent_parameter_groups"], blocked_groups)
        self.assertEqual(sketch["solver_dependent_parameters"], degrees_of_freedom)

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

    def assert_internal_history_publication_surface(self, result: dict[str, object]) -> dict:
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        self.assertNotIn("wire_joiner_ledger", sketch)
        self.assertNotIn("wire_joiner_history_detail", sketch)
        self.assertNotIn("facemaker_history", sketch)
        self.assertNotIn("internal_shape_history", sketch)
        self.assertNotIn("sketch_internal_history", named_shape)
        self.assertNotIn("sketch_internal_history_status", named_shape)
        diagnostics = sketch["internal_shape_history_diagnostics"]
        self.assertIn("facemaker", diagnostics)
        self.assertIn("wire_joiner", diagnostics)
        self.assertIn("element_map", named_shape)
        self.assertIn("history", named_shape)
        self.assertIn("mapper_history", named_shape)
        self.assertIn("element_history_status", named_shape)
        return diagnostics

    def assert_wire_joiner_mapper_events_do_not_expose_producer_anatomy(
        self,
        named_shape: dict[str, object],
    ) -> list[dict[str, object]]:
        events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "wire_joiner:open_export"
        ]
        for event in events:
            evidence = event["evidence"]
            for field_prefix in (
                "result_wire_producer_",
                "open_wire_compound_",
                "wire_joiner_history_event_",
            ):
                self.assertFalse(
                    any(key.startswith(field_prefix) for key in evidence),
                    evidence,
                )
        return events

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

    def external_geometry_pad_payload(self, flags: list[str]) -> dict:
        payload = self.external_geometry_state_payload(flags)
        payload["Objects"].append(
            {
                "Name": "Pad",
                "ID": 3,
                "TypeId": "PartDesign::Pad",
                "Properties": {
                    "Profile": {
                        "PropertyType": "App::PropertyLinkSubList",
                        "SubSet": [
                            {
                                "value": "Sketch",
                                "SubList": ["InternalFace1"],
                                "StableSubList": [""],
                            }
                        ],
                    },
                    "Type": "Length",
                    "Length": 4,
                    "Reversed": False,
                    "SideType": "One side",
                },
            }
        )
        payload["recompute"] = {"objs": ["Sketch", "Pad"]}
        return payload

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
                diagnostics = self.assert_internal_history_publication_surface(result)

                self.assertEqual(result["diagnostics"], [])
                self.assertGreaterEqual(diagnostics["event_count"], 1)
                self.assertIn("facemaker", diagnostics)

    def test_p5_all_fixtures_omit_generated_and_disable_legacy_helper_output(self) -> None:
        p5_fixture_dir = ROOT / "fixtures" / "p5"
        for fixture_path in sorted(p5_fixture_dir.glob("*.json")):
            with self.subTest(fixture=fixture_path.stem):
                result = self.run_recompute_file(fixture_path)
                for object_name, obj in result["objects"].items():
                    with self.subTest(fixture=fixture_path.stem, object=object_name):
                        self.assertNotIn("wire_joiner_ledger", obj)
                        self.assertNotIn("wire_joiner_history_detail", obj)
                        self.assertNotIn("facemaker_history", obj)

    def test_p7_sketch_internal_history_consumes_result_wire_producer_identity(self) -> None:
        for fixture in self.P7_RESULT_WIRE_IDENTITY_FIXTURES:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                self.assert_sketch_internal_history_publication(result)

    def assert_sketch_internal_history_publication(
        self,
        result: dict[str, object],
    ) -> None:
        diagnostics = self.assert_internal_history_publication_surface(result)
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        self.assertIn("WireJoinerOpenWires", diagnostics["producer_tags"])
        self.assertGreater(diagnostics["wire_joiner"]["open_export_count"], 0)
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        events = self.assert_wire_joiner_mapper_events_do_not_expose_producer_anatomy(named_shape)
        self.assertTrue(events)
        self.assertTrue(
            any(event["relation"] in {"generated", "split", "deleted", "preserved"} for event in events)
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

    def test_c4m3_sketch_constraint_supported_state_fixture(self) -> None:
        result = self.run_recompute("sketch-constraint-supported-state", "c4m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "underconstrained")
        self.assertEqual(sketch["solver_geometry_updates"], 0)
        self.assertEqual(sketch["solver_geometry_update_status"], "none")
        self.assertEqual(sketch["orientation_constraints_applied"], 4)

    def test_c4m3_sketch_constraint_geometry_update_fixture(self) -> None:
        result = self.run_recompute("sketch-constraint-geometry-update", "c4m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "underconstrained")
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_geometry_update_status"], "whole_line_orientation_first_slice")

    def test_c4m3_sketch_constraint_conflict_diagnostic_fixture(self) -> None:
        result = self.run_recompute("sketch-constraint-conflict-diagnostic", "c4m3")
        diagnostic = result["diagnostics"][0]
        sketch = result["objects"]["Sketch"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["sketch_solver_conflict"])
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver")
        self.assertEqual(diagnostic["target"], "Constraints[1,2]")
        self.assertEqual(sketch["status"], "error")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["solver_state"], "conflict")

    def test_c4m3_sketch_constraint_unsupported_relation_has_locator(self) -> None:
        result = self.run_recompute("sketch-constraint-unsupported-relation", "c4m3")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(
            [item["code"] for item in result["diagnostics"]],
            ["unsupported_sketch_constraint_relation"],
        )
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver_constraint")
        self.assertEqual(
            diagnostic["target"],
            "Constraints[1].Type=SnellsLaw;First=0;FirstPos=start;Second=1;SecondPos=end;Third=2",
        )
        self.assertEqual(result["objects"]["Sketch"]["status"], "error")

    def test_c3m3_sketch_horizontal_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-horizontal-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 3, 1)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["orientation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "whole_line_orientation_first_slice",
        )

    def test_c3m3_sketch_coordinate_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-coordinate-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 2, 2)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["dimension_constraints_applied"], 2)
        self.assertEqual(sketch["solver_geometry_updates"], 2)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 2)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "endpoint_coordinate_first_slice",
        )

    def test_c3m3_sketch_circle_radius_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-circle-radius-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 2, 1)
        self.assertEqual(sketch["dimension_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 1)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "circle_radius_diameter_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [-3.0, -3.0, 0.0], "max": [3.0, 3.0, 4.0]})

    def test_c3m3_sketch_line_length_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-line-length-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 15, 1)
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["dimension_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 1)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "line_length_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [0.0, 0.0, 0.0], "max": [5.0, 2.0, 3.0]})

    def test_c3m3_sketch_arc_length_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-arc-length-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 8, 1)
        self.assertEqual(sketch["edge_count"], 2)
        self.assertEqual(sketch["dimension_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "arc_length_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [-1.0, 0.0, 0.0], "max": [1.0, 1.0, 3.0]})

    def test_c3m3_sketch_point_on_line_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-point-on-line-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 7, 1)
        self.assertEqual(sketch["edge_count"], 2)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "point_on_object_line_first_slice",
        )
        self.assertEqual(extrude["bbox"], {"min": [-4.0, 0.0, 0.0], "max": [0.0, 5.0, 1.0]})

    def test_c3m3_sketch_parallel_line_pair_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-parallel-line-pair-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 7, 1)
        self.assertEqual(sketch["edge_count"], 2)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "line_pair_relation_first_slice",
        )
        self.assertEqual(extrude["bbox"], {"min": [0.0, 0.0, 0.0], "max": [4.0, 2.0, 1.0]})

    def test_c3m3_sketch_perpendicular_line_pair_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-perpendicular-line-pair-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 7, 1)
        self.assertEqual(sketch["edge_count"], 2)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "line_pair_relation_first_slice",
        )
        self.assertEqual(extrude["bbox"], {"min": [0.0, 0.0, 0.0], "max": [4.0, 6.0, 1.0]})

    def test_c3m3_sketch_perpendicular_line_circle_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-perpendicular-line-circle-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 6, 1)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "perpendicular_curve_midpoint_relation_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [1.0, -1.0, 0.0], "max": [3.0, 1.0, 2.0]})

    def test_c3m3_sketch_equal_circle_radius_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-equal-circle-radius-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 5, 1)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_equal_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "equal_relation_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [3.0, -2.0, 0.0], "max": [7.0, 2.0, 2.0]})

    def test_c3m3_sketch_tangent_line_circle_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-tangent-line-circle-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 6, 1)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_equal_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_tangent_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "tangent_line_round_relation_first_slice",
        )
        self.assertEqual(pad["bbox"], {"min": [1.0, 0.0, 0.0], "max": [3.0, 2.0, 2.0]})

    def test_c3m3_sketch_symmetric_line_axis_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-symmetric-line-axis-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 6, 2)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_equal_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_tangent_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_symmetric_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_line_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_center_relation_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "symmetric_line_axis_relation_first_slice",
        )
        self.assertEqual(extrude["bbox"], {"min": [-1.0, 1.0, 0.0], "max": [1.0, 1.0, 1.0]})

    def test_c3m3_sketch_symmetric_center_point_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-symmetric-center-point-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 4, 2)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_equal_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_tangent_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_symmetric_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_line_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_symmetric_center_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "symmetric_center_point_relation_first_slice",
        )
        self.assertEqual(extrude["bbox"], {"min": [-1.0, 1.0, 0.0], "max": [1.0, 1.0, 1.0]})

    def test_c3m3_sketch_symmetric_arc_endpoint_constraint_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-symmetric-arc-endpoint-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 7, 2)
        self.assertEqual(sketch["edge_count"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 1)
        self.assertEqual(sketch["solver_orientation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_coordinate_geometry_updates"], 0)
        self.assertEqual(sketch["solver_radius_geometry_updates"], 0)
        self.assertEqual(sketch["solver_length_geometry_updates"], 0)
        self.assertEqual(sketch["solver_arc_geometry_updates"], 0)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_line_pair_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_curve_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_equal_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_tangent_relation_geometry_updates"], 0)
        self.assertEqual(sketch["solver_symmetric_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_line_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_center_relation_geometry_updates"], 0)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "symmetric_line_axis_relation_first_slice",
        )
        self.assertLess(extrude["bbox"]["min"][0], -0.9)
        self.assertGreater(extrude["bbox"]["max"][0], 0.9)
        self.assertGreater(extrude["bbox"]["max"][1], 0.9)

    def test_c3m3_sketch_symmetric_coupled_curve_relation_updates_solver_geometry(self) -> None:
        result = self.run_recompute("sketch-symmetric-coupled-curve-relation-solver-geometry-update", "c3m3")
        sketch = result["objects"]["Sketch"]
        extrude = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 8, 3)
        self.assertEqual(sketch["edge_count"], 2)
        self.assertEqual(sketch["relation_constraints_applied"], 2)
        self.assertEqual(sketch["solver_geometry_updates"], 2)
        self.assertEqual(sketch["solver_relation_geometry_updates"], 2)
        self.assertEqual(sketch["solver_tangent_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_relation_geometry_updates"], 1)
        self.assertEqual(sketch["solver_symmetric_line_relation_geometry_updates"], 1)
        self.assertEqual(
            sketch["solver_geometry_update_status"],
            "request_local_solver_geometry_first_slices",
        )
        self.assertLessEqual(extrude["bbox"]["min"][0], -1.0)
        self.assertGreaterEqual(extrude["bbox"]["max"][0], 3.0)

    def test_c3m3_sketch_dof_underconstrained_after_satisfied_constraint(self) -> None:
        result = self.run_recompute("sketch-dof-underconstrained-after-constraint", "c3m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assert_c3m3_full_rank_solver(sketch, 3, 1)
        self.assertEqual(sketch["solver_geometry_updates"], 0)
        self.assertEqual(sketch["solver_geometry_update_status"], "none")
        self.assertEqual(sketch["orientation_constraints_applied"], 1)
        self.assertEqual(sketch["dimension_constraints_applied"], 0)
        self.assertEqual(sketch["relation_constraints_applied"], 0)

    def test_c3m3_sketch_full_solver_dof_reports_dependent_groups(self) -> None:
        result = self.run_recompute("sketch-full-solver-dof-dependent-group", "c3m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_c3m3_full_rank_solver(sketch, 3, 6, blocked_groups=1)
        self.assertEqual(sketch["block_constraints_applied"], 1)
        self.assertEqual(sketch["relation_constraints_applied"], 1)
        self.assertEqual(sketch["dimension_constraints_applied"], 1)
        self.assertEqual(sketch["solver_geometry_updates"], 0)
        self.assertEqual(sketch["solver_geometry_update_status"], "none")

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

    def test_c3m3_sketch_partially_redundant_constraints_warn_without_blocking_profile(self) -> None:
        result = self.run_recompute("sketch-partially-redundant-block-horizontal", "c3m3")
        diagnostic = result["diagnostics"][0]
        sketch = result["objects"]["Sketch"]

        self.assertEqual(
            [item["code"] for item in result["diagnostics"]],
            ["sketch_solver_partially_redundant"],
        )
        self.assertEqual(diagnostic["severity"], "warning")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "Constraints")
        self.assertEqual(diagnostic["stage"], "solver")
        self.assertEqual(diagnostic["target"], "Constraints[1,2]")
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 4)
        self.assert_c3m3_full_rank_solver(sketch, 11, 5, blocked_groups=1)
        self.assertEqual(sketch["solver_conflicting_constraints"], [])
        self.assertEqual(sketch["solver_redundant_constraints"], [])
        self.assertEqual(sketch["solver_partially_redundant_constraints"], [1, 2])
        self.assertEqual(sketch["orientation_constraints_applied"], 1)
        self.assertEqual(sketch["block_constraints_applied"], 1)

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

    def test_c3m3_sketch_without_constraints_reports_underconstrained_state(self) -> None:
        result = self.run_recompute("sketch-underconstrained-no-constraints", "c3m3")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assert_c3m3_full_rank_solver(sketch, 16, 0)
        self.assertEqual(sketch["solver_malformed_constraints"], [])
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

    def test_p5_conic_arc_profiles_are_supported_open_edges(self) -> None:
        for fixture in ["sketch-hyperbola-arc-profile", "sketch-parabola-arc-profile"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 1)
                self.assertEqual(sketch["raw_edge_count"], 1)
                self.assertFalse(sketch["profile_ready"])
                self.assert_object_matches_expected(result, "p5", fixture)

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

    def test_p5_conic_construction_arcs_are_ignored_for_profile(self) -> None:
        result = self.run_recompute("sketch-conic-arcs-construction-filter", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["raw_edge_count"], 4)
        self.assert_object_matches_expected(result, "p5", "sketch-conic-arcs-construction-filter")

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

    def test_p5_conic_external_geometry_projects_as_construction_curves(self) -> None:
        result = self.run_recompute("sketch-conic-arcs-external-geometry-projected", "p5")
        source = result["objects"]["SourceSketch"]
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(source["status"], "ok")
        self.assertEqual(source["edge_count"], 2)
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 2)
        self.assertEqual(sketch["external_curve_count"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-conic-arcs-external-geometry-projected")

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
        result = self.run_recompute("sketch-external-defining-profile", "p5")
        sketch = result["objects"]["Sketch"]
        expected = self.expected_freecad("p5", "sketch-external-defining-profile")
        sketch_expected = expected["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["profile"], "occt_face")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["raw_edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["defining"], 1)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Defining"], 4)
        self.assertEqual(sketch_expected["sketch_external"]["construction_count"], 4)
        self.assert_object_matches_expected(result, "p5", "sketch-external-defining-profile")

    def test_p5_external_geometry_frozen_source_change_matches_freecad_oracle(self) -> None:
        result = self.run_recompute("sketch-external-frozen-source-changed", "p5")
        expected = self.expected_freecad("p5", "sketch-external-frozen-source-changed")
        sketch_expected = expected["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(result["objects"]["Sketch"]["external_geometry_state_counts"]["frozen"], 1)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Frozen"], 4)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Sync"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-external-frozen-source-changed")

    def test_p5_external_geometry_frozen_sync_source_change_matches_freecad_oracle(self) -> None:
        result = self.run_recompute("sketch-external-frozen-sync-source-changed", "p5")
        expected = self.expected_freecad("p5", "sketch-external-frozen-sync-source-changed")
        sketch_expected = expected["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_flags_sync"])
        sub_set = updates[0]["properties"]["ExternalGeometry"]["SubSet"]
        self.assertEqual(sub_set[0]["ExternalFlags"], ["Defining", "Frozen"])
        self.assertEqual(result["objects"]["Sketch"]["external_geometry_state_counts"]["sync"], 1)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Frozen"], 4)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Sync"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-external-frozen-sync-source-changed")

    def test_p5_external_geometry_detached_source_change_matches_freecad_oracle(self) -> None:
        result = self.run_recompute("sketch-external-detached-source-changed", "p5")
        expected = self.expected_freecad("p5", "sketch-external-detached-source-changed")
        sketch_expected = expected["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_detach"])
        properties = updates[0]["properties"]
        self.assertEqual(properties["ExternalGeometry"]["SubSet"], [])
        for item in properties["ExternalGeo"]["Geometry"]:
            self.assertEqual(item["ExternalFlags"], ["Defining"])
            self.assertNotIn("Ref", item)
        self.assertEqual(result["objects"]["Sketch"]["external_geometry_state_counts"]["detached"], 1)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Detached"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-external-detached-source-changed")

    def test_p5_external_geometry_missing_source_recovery_matches_freecad_oracle(self) -> None:
        result = self.run_recompute("sketch-external-missing-source-recovered", "p5")
        expected = self.expected_freecad("p5", "sketch-external-missing-source-recovered")
        sketch_expected = expected["objects"]["Sketch"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["reason"] for item in updates], ["external_geometry_flags_sync"])
        sub_set = updates[0]["properties"]["ExternalGeometry"]["SubSet"]
        self.assertEqual(sub_set[0]["ExternalFlags"], ["Defining"])
        self.assertEqual(result["objects"]["Sketch"]["external_geometry_state_counts"]["missing"], 1)
        self.assertEqual(result["objects"]["Sketch"]["external_geometry_state_counts"]["recovered_missing"], 1)
        self.assertEqual(sketch_expected["sketch_external"]["flag_counts"]["Missing"], 0)
        self.assert_object_matches_expected(result, "p5", "sketch-external-missing-source-recovered")

    def test_p5_reference_only_external_geometry_does_not_build_profile(self) -> None:
        result = self.run_payload(self.external_geometry_pad_payload([]))
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["open_profile"])
        self.assertEqual(diagnostics[0]["object"], "Pad")
        self.assertEqual(diagnostics[0]["property"], "Profile")
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 4)
        self.assertEqual(sketch["external_geometry_state_counts"]["defining"], 0)
        self.assertEqual(pad["status"], "error")

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

    def test_c4m3_external_geometry_lifecycle_pressure_package(self) -> None:
        for fixture, state_counts, update_reason, expected_backed in [
            ("sketch-external-internal-frozen-native-pool", {"frozen": 1}, None, True),
            ("sketch-external-internal-detached-native-pool", {"detached": 1}, "external_geometry_detach", True),
            (
                "sketch-external-internal-missing-recovered",
                {"missing": 1, "recovered_missing": 1},
                "external_geometry_flags_sync",
                True,
            ),
            ("sketch-external-internal-frozen-brep-snapshot", {"frozen": 1}, None, False),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "c4m3")
                sketch = result["objects"]["Sketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                for state, count in state_counts.items():
                    self.assertEqual(sketch["external_geometry_state_counts"][state], count)
                if update_reason is None:
                    self.assertEqual(result["documentObjectUpdates"], [])
                else:
                    self.assertEqual([item["reason"] for item in result["documentObjectUpdates"]], [update_reason])
                if expected_backed:
                    self.assert_result_matches_expected(result, "c4m3", fixture)

    def test_c4m3_external_geometry_missing_snapshot_is_locatable_diagnostic(self) -> None:
        result = self.run_recompute("sketch-external-internal-frozen-missing-snapshot-diagnostic", "c4m3")
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["missing_external_geometry_snapshot"])
        self.assertEqual(diagnostic["stage"], "graph")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "ExternalGeometry")
        self.assertEqual(diagnostic["target"], "MissingBox")
        self.assertEqual(diagnostic["subname"], "Face5")

    def test_p5_conic_native_external_geo_reuses_old_geometry(self) -> None:
        result = self.run_recompute("sketch-conic-arcs-external-geometry-native", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 0)
        self.assertEqual(sketch["external_geometry_count"], 2)
        self.assertEqual(sketch["external_curve_count"], 2)
        self.assertEqual(sketch["external_geometry_state_counts"]["missing"], 2)

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

    def test_p5_invalid_conic_arc_params_report_unsupported_geometry(self) -> None:
        cases = [
            (
                "hyperbola-major-radius",
                {
                    "kind": "ArcOfHyperbola",
                    "center": [0, 0],
                    "majorRadius": 0,
                    "minorRadius": 2,
                    "startAngle": -0.5,
                    "endAngle": 0.5,
                },
            ),
            (
                "parabola-focal",
                {
                    "kind": "ArcOfParabola",
                    "center": [0, 0],
                    "focal": 0,
                    "startAngle": -1,
                    "endAngle": 1,
                },
            ),
            (
                "equal-trim",
                {
                    "kind": "ArcOfHyperbola",
                    "center": [0, 0],
                    "majorRadius": 4,
                    "minorRadius": 2,
                    "startAngle": 0.5,
                    "endAngle": 0.5,
                },
            ),
        ]
        for case_id, geometry in cases:
            with self.subTest(case_id=case_id):
                result = self.run_payload(
                    {
                        "Objects": [
                            {
                                "Name": "Sketch",
                                "ID": 1,
                                "TypeId": "Sketcher::SketchObject",
                                "Properties": {"Geometry": [geometry], "Constraints": []},
                            }
                        ],
                        "recompute": {"objs": ["Sketch"]},
                    }
                )
                diagnostic = result["diagnostics"][0]

                self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_geometry"])
                self.assertEqual(diagnostic["object"], "Sketch")
                self.assertEqual(diagnostic["property"], "Geometry")

    def test_p5_closed_sketch_exports_internal_subshapes(self) -> None:
        result = self.run_recompute("sketch-internal-face", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face")

    def test_p5_sketch_internal_result_package_keeps_publication_fields(self) -> None:
        result = self.run_recompute("sketch-internal-face", "p5")
        sketch = result["objects"]["Sketch"]
        subshapes = result["subshapes"]["Sketch"]
        mesh = result["mesh"]["Sketch"]

        self.assertEqual(sketch["profile"], "occt_face")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["internal_shape"], "occt_internal_shape")
        self.assertEqual(sketch["internal_face_count"], 1)
        self.assertEqual(sketch["internal_edge_count"], 4)
        self.assertEqual(sketch["internal_vertex_count"], 4)
        self.assertEqual(sketch["internal_element_map"]["Edge1"], "InternalEdge1")
        self.assertEqual(sketch["internal_element_map"]["InternalEdge1"], "Edge1")
        self.assertIn("Edge1", subshapes)
        self.assertIn("InternalEdge1", subshapes)
        self.assertIn("InternalFace1", mesh["faceIds"])
        diagnostics = self.assert_internal_history_publication_surface(result)
        self.assertIn("FaceMakerBuildFace", diagnostics["producer_tags"])
        self.assertGreaterEqual(diagnostics["event_count"], 1)
        self.assertGreaterEqual(
            diagnostics["relation_summary"].get("generated", 0),
            1,
        )
        self.assertEqual(diagnostics["wire_joiner"]["diagnostics"]["status"], "ok")
        self.assertIn("has_open_wires", diagnostics["wire_joiner"]["diagnostics"]["summary"])
        self.assertFalse(
            diagnostics["wire_joiner"]["diagnostics"]["summary"]["missing_child_wire_invariant"]
        )

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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        diagnostics = self.assert_internal_history_publication_surface(result)
        self.assertGreaterEqual(diagnostics["facemaker"]["bounded_face_count"], 2)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-split-line")

    def test_p5_through_open_cutter_publishes_named_shape_history(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        diagnostics = self.assert_internal_history_publication_surface(result)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(diagnostics["facemaker"]["source_edge_count"], 5)
        self.assertEqual(diagnostics["facemaker"]["bounded_face_count"], 2)
        self.assertTrue(diagnostics["facemaker"]["splitter_history"])
        self.assertGreater(diagnostics["wire_joiner"]["open_export_count"], 0)
        self.assertIn("facemaker_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assert_wire_joiner_mapper_events_do_not_expose_producer_anatomy(named_shape)
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-through-open-cutter")

    def test_p5_internal_shape_records_split_history_for_open_cutter_fragments(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        split_entries = [
            item
            for item in named_shape["history"]
            if item["kind"] == "split" and item["sources"] == ["Edge5"]
        ]
        self.assertNotIn("Edge5", named_shape["element_map"])
        self.assertGreaterEqual(len(split_entries), 2)
        for entry in split_entries:
            self.assertTrue(entry["element"].startswith("InternalEdge"))
            self.assertEqual(named_shape["elements"][entry["element"]]["status"], "split")

    def test_p5_facemaker_bounded_faces_emit_publication_events(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        diagnostics = self.assert_internal_history_publication_surface(result)

        self.assertEqual(diagnostics["facemaker"]["bounded_face_count"], sketch["internal_face_count"])
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

    def test_p5_summary_history_is_diagnostic_only(self) -> None:
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
        self.assertFalse(
            any(
                event["diagnostic_status"] == "summary_only:wire_joiner_history:open_export"
                for event in summary_events
            )
        )

    def test_p5_source_edge_one_to_many_split_uses_publication(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        self.assertNotIn("Edge5", sketch["internal_element_map"])
        self.assertNotIn("Edge5", named_shape["element_map"])
        edge5_events = [
            event
            for event in named_shape["mapper_history"]
            if event["source"] == {"object": "Sketch", "subname": "Edge5"}
            and event["relation"] == "split"
            and event["shape_kind"] == "edge"
        ]
        self.assertGreaterEqual(len(edge5_events), 2)
        self.assertTrue(
            all(event["evidence"].get("producer") in {"FaceMakerBuildFace", "WireJoiner"} for event in edge5_events)
        )

    def test_p5_deleted_no_original_purge_is_diagnostic_not_unique_map(self) -> None:
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
        for event in purge_events:
            self.assertNotIn("open_wire_compound_no_original_purged_by_ledger", event["evidence"])
            self.assertEqual(event["evidence"].get("diagnostic_code"), "no_original_purge")

    def test_p5_wire_joiner_open_export_uses_publication_events(self) -> None:
        result = self.run_recompute("sketch-internal-face-t-cutter", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]
        diagnostics = self.assert_internal_history_publication_surface(result)

        self.assertGreater(diagnostics["wire_joiner"]["open_export_count"], 0)
        events = self.assert_wire_joiner_mapper_events_do_not_expose_producer_anatomy(named_shape)
        self.assertTrue(events)
        self.assertTrue(any(event["relation"] == "generated" for event in events))
        self.assertTrue(any(event["relation"] == "split" for event in events))
        for event in events:
            self.assertEqual(event["evidence"]["producer"], "WireJoiner")
            self.assertEqual(event["relation"], event["evidence"]["relation"])

    def test_p5_internal_shape_publication_fixture_set(self) -> None:
        fixtures = [
            "sketch-internal-face-branch-open-cutter",
            "sketch-internal-face-cross-cutters",
            "sketch-internal-face-t-cutter",
            "sketch-internal-face-internal-branch-cutter",
            "sketch-internal-face-segmented-cross-cutter",
            "sketch-internal-face-overlap-rectangles",
            "sketch-internal-face-adjacent-rectangles",
            "sketch-internal-face-overlap-circles",
            "sketch-internal-face-three-overlap-circles",
            "sketch-internal-face-arc-lens",
            "sketch-internal-face-line-arc-same-endpoints",
            "sketch-internal-face-cross-pattern",
            "sketch-internal-face-dangling-line",
        ]
        for fixture in fixtures:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]
                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assert_internal_history_publication_surface(result)
                self.assert_object_matches_expected(result, "p5", fixture)
                self.assertIn("Sketch", result["mesh"])

    def test_p5_self_intersecting_cubic_bspline_splits_into_bounded_regions(self) -> None:
        result = self.run_recompute("sketch-internal-face-cubic-figure8-bspline", "p5")
        sketch = result["objects"]["Sketch"]
        diagnostics = self.assert_internal_history_publication_surface(result)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(diagnostics["facemaker"]["source_edge_count"], 1)
        self.assertTrue(diagnostics["facemaker"]["pre_split_history"])
        self.assertFalse(diagnostics["facemaker"]["splitter_history"])
        self.assertEqual(diagnostics["facemaker"]["bounded_face_count"], sketch["internal_face_count"])
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cubic-figure8-bspline")

    def test_p5_self_intersecting_cubic_bspline_records_terminal_split_history(self) -> None:
        result = self.run_recompute("sketch-internal-face-cubic-figure8-bspline", "p5")
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        split_entries = [
            item
            for item in named_shape["history"]
            if item["kind"] == "split" and item["sources"] == ["Edge1"]
        ]

        self.assertIn("facemaker_history:pre_split", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
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

    def test_c4m3_internal_shape_expected_backed_pressure_package(self) -> None:
        for fixture in [
            "sketch-external-internal-open-profile-empty",
            "sketch-external-internal-bounded-cross-cutters",
            "sketch-external-internal-self-intersection-bowtie",
            "sketch-external-internal-split-dangling-mixed",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "c4m3")
                sketch = result["objects"]["Sketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assert_result_matches_expected(result, "c4m3", fixture)

        mixed = self.run_recompute("sketch-external-internal-split-dangling-mixed", "c4m3")
        named_shape = mixed["named_shapes"]["Sketch.InternalShape"]
        self.assertIn("wire_joiner_history:splitter", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:modified", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:deleted", named_shape["element_history_status"])
        self.assertIn("wire_joiner_history:open_export", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertIn("InternalEdge", "".join(named_shape["elements"]))

    def test_p5_pad_uses_selected_internal_face_sublist(self) -> None:
        result = self.run_recompute("pad-internal-face-sublist", "p5")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "pad-internal-face-sublist")

    def test_p5_pad_accepts_internal_face_stable_sublist_with_internal_named_shape(self) -> None:
        result = self.run_recompute("pad-internal-face-stable-sublist", "p5")
        pad = result["objects"]["Pad"]
        named_shape = result["named_shapes"]["Sketch.InternalShape"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["shape"], "occt_solid")
        self.assertAlmostEqual(pad["volume"], 250.0)
        self.assertEqual(named_shape["element_map"]["InternalFace1"], "InternalFace1")
        self.assertIn("InternalFace1", named_shape["elements"])
        self.assert_object_matches_expected(result, "p5", "pad-internal-face-stable-sublist")

    def test_p5_pad_rejects_missing_internal_face_stable_sublist(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-stable-sublist.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        profile = payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]
        profile["SubList"] = ["InternalFace99"]
        profile["StableSubList"] = ["InternalFace99"]

        result = self.run_payload(payload)
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_stable_subname"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace99")
        self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_p5_pad_rejects_internal_face_stable_sublist_without_internal_shape_evidence(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-open-wire-profile.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        profile = payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]
        profile["SubList"] = ["InternalFace1"]
        profile["StableSubList"] = ["InternalFace1"]

        result = self.run_payload(payload)
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_stable_subname"])
        self.assertEqual(diagnostic["object"], "Pad")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Sketch")
        self.assertEqual(diagnostic["subname"], "InternalFace1")
        self.assertIn("Sketch.InternalShape NamedShape/ElementMap evidence", diagnostic["message"])
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
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["StableSubList"], ["g305:split1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])
        self.assertEqual(update["ReferenceShadow"][0]["stableSubname"], "g305:split1")

    def test_c4m3_reference_shadow_single_subshape_pressure_package(self) -> None:
        ffi_result = self.run_recompute_ffi("sketch-external-internal-reference-shadow-edge-stable", "c4m3")
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
        shadow = update["ReferenceShadow"][0]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(update["SubList"], ["InternalEdge1"])
        self.assertEqual(update["StableSubList"], ["Edge1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "Edge1", "oldName": "InternalEdge1"}])
        self.assertEqual(len(update["ReferenceShadow"]), 1)
        self.assertEqual(shadow["target"], "BaseSketch")
        self.assertEqual(shadow["property"], "InternalShape")
        self.assertEqual(shadow["shapeType"], "Edge")
        self.assertEqual(shadow["subname"], "InternalEdge1")
        self.assertEqual(shadow["stableSubname"], "Edge1")

        payload = json.loads(
            (ROOT / "fixtures" / "c4m3" / "sketch-external-internal-frozen-brep-snapshot.json").read_text(
                encoding="utf-8"
            )
        )
        snapshot_shadow = payload["Objects"][0]["Properties"]["ExternalGeometry"]["SubSet"][0]["ReferenceShadow"][0]
        self.assertEqual(snapshot_shadow["target"], "MissingBox")
        self.assertEqual(snapshot_shadow["property"], "Shape")
        self.assertEqual(snapshot_shadow["shapeType"], "Face")
        self.assertEqual(snapshot_shadow["subname"], "Face5")
        self.assertIn("brep", snapshot_shadow)
        self.assertEqual(len(payload["Objects"][0]["Properties"]["ExternalGeometry"]["SubSet"][0]["ReferenceShadow"]), 1)

    def test_p5_pad_uses_shadow_sub_before_global_reference_shadow_recovery(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]["SubList"] = ["InternalFace2"]

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

        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["StableSubList"], ["g305:split1"])
        self.assertEqual(update["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])

    def test_p5_pad_recovers_empty_sublist_from_shadow_sub_reference_shadow(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json"
        for sublist_mode in ["omitted", "empty"]:
            with self.subTest(sublist_mode=sublist_mode):
                payload = json.loads(fixture_path.read_text(encoding="utf-8"))
                profile = payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]
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

                self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_profile"])
                self.assertEqual(result["objects"]["Pad"]["status"], "error")

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
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
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
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
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
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
        self.assertEqual(update["SubList"], ["InternalFace1"])
        self.assertEqual(update["ReferenceShadow"][0]["subname"], "InternalFace1")
        brep = update["ReferenceShadow"][0]["brep"]
        self.assertEqual(brep["format"], "brep-text")
        self.assertEqual(brep["byteLength"], len(brep["data"]))
        self.assertEqual(brep["sha256"], hashlib.sha256(brep["data"].encode()).hexdigest())

    def test_p5_pad_rejects_reference_shadow_brep_zstd_base64_decode_error(self) -> None:
        fixture_path = ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow-brep-bin-recover-sublist.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]["ReferenceShadow"][0]["brep"]["data"] = "not-base64"

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
        payload["Objects"][1]["Properties"]["Profile"]["SubSet"][0]["ReferenceShadow"][0]["brep"]["sha256"] = "0" * 64

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
        update = ffi_result["elementReferenceUpdates"][0]["SubSet"][0]
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

    def test_c4m3_reference_shadow_deferred_diagnostics_are_locatable(self) -> None:
        for fixture, code in [
            (
                "sketch-external-internal-reference-shadow-brep-split-diagnostic",
                "subname_split_requires_reselect",
            ),
            ("sketch-external-internal-reference-shadow-brep-deleted-diagnostic", "subname_deleted"),
            ("sketch-external-internal-reference-shadow-ambiguous-diagnostic", "subname_resolve_ambiguous"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "c4m3")
                diagnostic = result["diagnostics"][0]

                self.assertEqual([item["code"] for item in result["diagnostics"]], [code])
                self.assertEqual(diagnostic["object"], "Pad")
                self.assertEqual(diagnostic["property"], "Profile")
                self.assertEqual(diagnostic["target"], "Sketch")
                self.assertEqual(diagnostic["subname"], "InternalFace99")
                self.assertEqual(result["objects"]["Pad"]["status"], "error")

    def test_c4m3_external_geometry_unsupported_reference_shadow_brep_is_locatable(self) -> None:
        result = self.run_recompute(
            "sketch-external-internal-unsupported-reference-shadow-brep-diagnostic",
            "c4m3",
        )
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_reference_shadow_brep"])
        self.assertEqual(diagnostic["stage"], "runtime")
        self.assertEqual(diagnostic["object"], "Sketch")
        self.assertEqual(diagnostic["property"], "ExternalGeometry")
        self.assertEqual(diagnostic["target"], "MissingBox")
        self.assertEqual(diagnostic["subname"], "Face5")

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

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_profile"])
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
        self.assertEqual(extrusion["topo_naming_history"], "maker_history:taper_thru_sections")
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
        self.assertEqual(extrusion["topo_naming_history"], "maker_history:taper_thru_sections")
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
        self.assertEqual(extrusion["topo_naming_history"], "maker_history:taper_thru_sections")
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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertAlmostEqual(extrusion["volume"], 340.0)
        self.assertEqual(extrusion["bbox"], {"min": [0.0, 0.0, 0.0], "max": [10.0, 10.0, 5.0]})
        diagnostics = self.assert_internal_history_publication_surface(result)
        self.assertGreaterEqual(diagnostics["facemaker"]["bounded_face_count"], 1)

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
