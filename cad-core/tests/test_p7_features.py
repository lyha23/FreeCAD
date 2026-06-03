from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import BIN, ROOT, CadCoreFixtureTestCase


class CadCoreP7FeatureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_dressup_slot_history(
        self,
        named_shape: dict,
        source_objects: set[str],
        transformed_source: str,
    ) -> None:
        maker_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "maker_history"
        ]

        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertIn("history_consumed:merge", named_shape["element_history_status"])
        for source_object in source_objects:
            self.assertTrue(
                any(key.startswith(f"{source_object}.") for key in named_shape["element_map"]),
                f"{source_object} aliases should survive DressUp AddSubShape slot propagation",
            )
            self.assertTrue(
                any(event["source"]["object"] == source_object for event in maker_events),
                f"{source_object} should remain a maker-history source",
            )
        self.assertTrue(
            any(key.startswith(f"{transformed_source}.") for key in named_shape["element_map"]),
            "transformed DressUp slot aliases should survive into ElementMap",
        )
        self.assertTrue(
            any(
                event["relation"] == "modified"
                and event["source"]["object"] == transformed_source
                for event in maker_events
            ),
            "transformed DressUp slot should record modified maker history",
        )

    def assert_refine_model_history(
        self,
        named_shape: dict,
        generated_sources: set[str],
        modified_sources: set[str],
        deleted_sources: set[tuple[str, str]],
    ) -> None:
        maker_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "maker_history"
        ]
        terminal_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "terminal_history"
        ]

        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        for source_object in generated_sources:
            self.assertTrue(
                any(
                    event["relation"] == "generated"
                    and event["source"]["object"] == source_object
                    for event in maker_events
                ),
                f"{source_object} should contribute generated RefineModel history",
            )
        for source_object in modified_sources:
            self.assertTrue(
                any(
                    event["relation"] == "modified"
                    and event["source"]["object"] == source_object
                    for event in maker_events
                ),
                f"{source_object} should contribute modified RefineModel history",
            )
        if deleted_sources:
            self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        for source_object, source_subname in deleted_sources:
            self.assertTrue(
                any(
                    event["relation"] == "deleted"
                    and event["source"] == {"object": source_object, "subname": source_subname}
                    and event["recoverability"] == "deleted"
                    and event["diagnostic_status"] == "deleted_stable_subname"
                    for event in terminal_events
                ),
                f"{source_object}.{source_subname} should remain terminal deleted RefineModel history",
            )

    def assert_transformed_pattern_ownership(
        self,
        named_shape: dict,
        transformed_sources: set[str],
        source_objects: set[str],
        *,
        terminal: bool,
    ) -> None:
        events = named_shape["mapper_history"]

        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("history_consumed:merge", named_shape["element_history_status"])
        if terminal:
            self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
            self.assertTrue(
                any(event["maker_stage"] == "terminal_history" for event in events),
                "transformed/pattern ownership should keep terminal split/deleted history",
            )
        for transformed_source in transformed_sources:
            self.assertTrue(
                any(key.startswith(f"{transformed_source}.") for key in named_shape["element_map"]),
                f"{transformed_source} aliases should survive transformed/pattern ownership propagation",
            )
            self.assertTrue(
                any(event["source"]["object"] == transformed_source for event in events),
                f"{transformed_source} should remain visible in mapper history",
            )
        for source_object in source_objects:
            self.assertTrue(
                any(event["source"]["object"] == source_object for event in events),
                f"{source_object} should remain visible in transformed/pattern mapper history",
            )

    def test_p7_refine_false_is_feature_refine_noop(self) -> None:
        result = self.run_recompute("pad-refine-false", "p7")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertNotIn("topo_naming", pad)
        self.assert_object_matches_expected(result, "p7", "pad-refine-false")

    def test_p7_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("pad-refine-true", "p7")
        pad = result["objects"]["Pad"]
        pad_named_shape = result["named_shapes"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["refine"], "applied")
        self.assert_refine_model_history(pad_named_shape, {"Sketch"}, set(), set())
        self.assert_object_matches_expected(result, "p7", "pad-refine-true")

    def test_p7_pocket_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("pocket-refine-true", "p7")
        pocket = result["objects"]["Pocket"]
        body = result["objects"]["Body"]
        body_named_shape = result["named_shapes"]["Body"]
        body_history = result["named_shapes"]["Body"]["history"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["status"], "ok")
        self.assertNotIn("refine", pocket)
        self.assertEqual(body["refined_features"], ["Pocket"])
        for source in ["Pocket.Face5", "Pocket.Face6", "SketchPocket.Face1"]:
            self.assertTrue(
                any(
                    item["kind"] == "deleted"
                    and item["element"] == source
                    and item["sources"] == [source]
                    for item in body_history
                )
            )
        self.assert_refine_model_history(
            body_named_shape,
            {"SketchPad", "SketchPocket"},
            {"Pad", "Pocket", "SketchPad", "SketchPocket"},
            {("Pocket", "Face5"), ("Pocket", "Face6"), ("SketchPocket", "Face1")},
        )
        self.assert_object_matches_expected(result, "p7", "pocket-refine-true")

    def test_p7_coordinate_system_exposes_axes_for_reference_axis(self) -> None:
        result = self.run_recompute("datum-coordinate-system-reference-axis", "p7")
        coordinate_system = result["objects"]["CoordinateSystem"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(coordinate_system["datum"], "coordinate_system")
        self.assertAlmostEqual(coordinate_system["z_axis"][0], 0.7071067811865476)
        self.assertAlmostEqual(coordinate_system["z_axis"][2], 0.7071067811865475)
        self.assertEqual(pad["method"], "Length")
        self.assert_object_matches_expected(result, "p7", "datum-coordinate-system-reference-axis")

    def test_p7_coordinate_system_can_place_sketch_support(self) -> None:
        result = self.run_recompute("datum-coordinate-system-sketch-support", "p7")
        coordinate_system = result["objects"]["CoordinateSystem"]
        body = result["objects"]["Body"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(coordinate_system["origin"], [0.0, 0.0, 2.0])
        self.assert_object_matches_expected(result, "p7", "datum-coordinate-system-sketch-support")
        self.assertEqual(pad["bbox"], body["bbox"])

    def test_p7_origin_ignores_local_placement_but_keeps_parent_group_placement(self) -> None:
        result = self.run_recompute("origin-identity-placement", "p7")
        origin = result["objects"]["Origin"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(origin["datum"], "origin")
        self.assertEqual(origin["origin"], [10.0, 0.0, 0.0])
        self.assertEqual(origin["x_axis"], [1.0, 0.0, 0.0])

    def test_p7_hole_blind_depth_cuts_body(self) -> None:
        result = self.run_recompute("hole-blind-depth", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(hole["add_sub"], "sub")
        self.assert_object_matches_expected(result, "p7", "hole-blind-depth")

    def test_p7_hole_supported_profile_matches_native_oracle(self) -> None:
        result = self.run_recompute("hole-supported-blind-depth", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["source_profile"], "SketchHole")
        self.assert_object_matches_expected(result, "p7", "hole-supported-blind-depth")

    def test_p7_hole_supported_common_variants_match_native_oracle(self) -> None:
        for fixture, expected_fields in [
            ("hole-supported-through-all", {"method": "ThroughAll"}),
            ("hole-supported-counterbore", {"hole_cut_type": "Counterbore", "hole_cut_diameter": 4.0}),
            ("hole-supported-countersink", {"hole_cut_type": "Countersink", "hole_cut_countersink_angle": 90.0}),
            ("hole-supported-counterdrill", {"hole_cut_type": "Counterdrill", "hole_cut_depth": 1.0}),
            ("hole-supported-angled-drill-point", {"drill_point": "Angled", "drill_for_depth": False}),
            ("hole-supported-tapered", {"tapered": True, "tapered_angle": 80.0}),
            ("hole-supported-point-profile", {"method": "Dimension"}),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["status"], "ok")
                self.assertEqual(hole["source_profile"], "SketchHole")
                for field, value in expected_fields.items():
                    self.assertEqual(hole[field], value)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_supported_threaded_heads_match_native_oracle(self) -> None:
        for fixture, expected_fields in [
            (
                "hole-supported-threaded-standard-counterbore",
                {"hole_cut_definition_source": "iso4762.json", "hole_cut_diameter": 8.0, "hole_cut_depth": 4.4},
            ),
            (
                "hole-supported-threaded-standard-countersink",
                {"hole_cut_definition_source": "iso10642.json", "hole_cut_diameter": 9.0},
            ),
            (
                "hole-supported-threaded-dynamic-din7984",
                {
                    "hole_cut_definition_source": "din7984.json",
                    "hole_cut_standard": "DIN 7984",
                    "hole_cut_diameter": 8.0,
                    "hole_cut_depth": 3.2,
                },
            ),
            (
                "hole-supported-threaded-dynamic-iso2009",
                {
                    "hole_cut_definition_source": "iso2009.json",
                    "hole_cut_standard": "ISO 2009",
                    "hole_cut_diameter": 9.5,
                },
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["status"], "ok")
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["diameter_source"], "thread_tap_drill")
                self.assertEqual(hole["source_profile"], "SketchHole")
                for field, value in expected_fields.items():
                    if isinstance(value, float):
                        self.assertAlmostEqual(hole[field], value, delta=1e-9)
                    else:
                        self.assertEqual(hole[field], value)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_supported_thread_options_match_native_oracle(self) -> None:
        for fixture, expected_fields in [
            (
                "hole-supported-thread-depth-din76",
                {
                    "thread_depth_type": "Tapped (DIN76)",
                    "thread_depth": 6.2,
                    "thread_runout": 3.8,
                },
            ),
            (
                "hole-supported-thread-depth-dimension-clamped",
                {
                    "thread_depth_type": "Dimension",
                    "thread_depth": 6.0,
                    "thread_runout": 3.8,
                },
            ),
            (
                "hole-supported-thread-class-clearance",
                {
                    "thread_class": "4G",
                    "thread_direction": "Left",
                    "thread_clearance": 0.022,
                    "thread_radius_clearance": 0.011,
                },
            ),
            (
                "hole-supported-thread-custom-clearance",
                {
                    "thread_class": "6H",
                    "thread_direction": "Right",
                    "use_custom_thread_clearance": True,
                    "custom_thread_clearance": 0.08,
                    "thread_clearance": 0.08,
                    "thread_radius_clearance": 0.04,
                },
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["status"], "ok")
                self.assertEqual(hole["source_profile"], "SketchHole")
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["diameter_source"], "thread_tap_drill")
                for field, value in expected_fields.items():
                    if isinstance(value, float):
                        self.assertAlmostEqual(hole[field], value, delta=1e-9)
                    else:
                        self.assertEqual(hole[field], value)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_refine_true_uses_body_final_result_refine(self) -> None:
        result = self.run_recompute("hole-refine-true", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        body_named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(body["tip"], "Hole")
        self.assertEqual(body["refined_features"], ["Hole"])
        self.assert_refine_model_history(
            body_named_shape,
            {"SketchPad"},
            {"Pad", "Hole"},
            {("Hole", "Face3")},
        )
        self.assert_object_matches_expected(result, "p7", "hole-refine-true")

    def test_p7_hole_through_all_cuts_body(self) -> None:
        result = self.run_recompute("hole-through-all", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["method"], "ThroughAll")
        self.assertGreater(hole["depth"], 10.0)
        self.assert_object_matches_expected(result, "p7", "hole-through-all")

    def test_p7_hole_counterbore_cuts_head_cylinder_and_shaft(self) -> None:
        result = self.run_recompute("hole-counterbore", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Counterbore")
        self.assertEqual(hole["hole_cut_diameter"], 4.0)
        self.assertEqual(hole["hole_cut_depth"], 2.0)
        self.assert_object_matches_expected(result, "p7", "hole-counterbore")

    def test_p7_hole_countersink_cuts_conical_head_and_shaft(self) -> None:
        result = self.run_recompute("hole-countersink", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Countersink")
        self.assertEqual(hole["hole_cut_countersink_angle"], 90.0)
        self.assert_object_matches_expected(result, "p7", "hole-countersink")

    def test_p7_hole_counterdrill_cuts_head_cone_between_two_cylinders(self) -> None:
        result = self.run_recompute("hole-counterdrill", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Counterdrill")
        self.assertEqual(hole["hole_cut_depth"], 1.0)
        self.assert_object_matches_expected(result, "p7", "hole-counterdrill")

    def test_p7_hole_threaded_metric_head_cut_uses_freecad_standard_tables(self) -> None:
        for fixture, cut_type, standard, source, cut_diameter, cut_depth, angle in [
            ("hole-threaded-standard-counterbore", "Counterbore", "", "iso4762.json", 8.0, 4.4, 90.0),
            ("hole-threaded-standard-countersink", "Countersink", "", "iso10642.json", 9.0, 0.0, 90.0),
            ("hole-threaded-dynamic-din7984", "Counterbore", "DIN 7984", "din7984.json", 8.0, 3.2, 90.0),
            ("hole-threaded-dynamic-iso2009", "Countersink", "ISO 2009", "iso2009.json", 9.5, 0.0, 90.0),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], "ISOMetricProfile")
                self.assertEqual(hole["thread_size"], "M4x0.7")
                self.assertEqual(hole["diameter_source"], "thread_tap_drill")
                self.assertAlmostEqual(hole["diameter"], 3.3, delta=1e-9)
                self.assertEqual(hole["hole_cut_type"], cut_type)
                self.assertEqual(hole["hole_cut_standard"], standard)
                self.assertEqual(hole["hole_cut_definition_source"], source)
                self.assertAlmostEqual(hole["hole_cut_diameter"], cut_diameter, delta=1e-9)
                self.assertAlmostEqual(hole["hole_cut_depth"], cut_depth, delta=1e-9)
                self.assertAlmostEqual(hole["hole_cut_countersink_angle"], angle, delta=1e-9)

    def test_p7_hole_cut_type_loads_external_resource_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            resource_dir = Path(tmpdir)
            (resource_dir / "cad-core-custom-head.json").write_text(
                json.dumps(
                    {
                        "name": "CAD Core Custom Head",
                        "cut_type": "counterbore",
                        "thread_type": "metric",
                        "data": [
                            {
                                "thread": "M4x0.7",
                                "diameter": 12.3,
                                "depth": 4.5,
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            fixture = json.loads((ROOT / "fixtures" / "p7" / "hole-threaded-dynamic-din7984.json").read_text())
            for obj in fixture["Objects"]:
                if obj["Name"] == "Hole":
                    obj["Properties"]["HoleCutType"] = "CAD Core Custom Head"
                    obj["Properties"]["HoleCutDiameter"] = 0
                    obj["Properties"]["HoleCutDepth"] = 0
                    break

            input_path = resource_dir / "hole-custom-resource.json"
            output_path = resource_dir / "hole-custom-resource.result.json"
            input_path.write_text(json.dumps(fixture), encoding="utf-8")

            env = os.environ.copy()
            env["CAD_CORE_HOLE_RESOURCE_DIR"] = str(resource_dir)
            env["CAD_CORE_TEST_LEGACY_OUTPUT"] = "1"
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                env=env,
                check=True,
            )

            result = json.loads(output_path.read_text(encoding="utf-8"))
            hole = result["objects"]["Hole"]
            self.assertEqual(result["diagnostics"], [])
            self.assertEqual(hole["hole_cut_type"], "Counterbore")
            self.assertEqual(hole["hole_cut_standard"], "CAD Core Custom Head")
            self.assertEqual(hole["hole_cut_definition_source"], "cad-core-custom-head.json")
            self.assertAlmostEqual(hole["hole_cut_diameter"], 12.3, delta=1e-9)
            self.assertAlmostEqual(hole["hole_cut_depth"], 4.5, delta=1e-9)

    def test_p7_hole_thread_depth_follows_freecad_thread_depth_param(self) -> None:
        for fixture, depth_type, thread_depth, runout in [
            ("hole-thread-depth-din76", "Tapped (DIN76)", 6.2, 3.8),
            ("hole-thread-depth-dimension-clamped", "Dimension", 6.0, 3.8),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_depth_type"], depth_type)
                self.assertAlmostEqual(hole["thread_depth"], thread_depth, delta=1e-9)
                self.assertAlmostEqual(hole["thread_runout"], runout, delta=1e-9)

    def test_p7_hole_model_thread_parameter_state_matches_freecad_inputs(self) -> None:
        for fixture, thread_class, direction, use_custom, custom, clearance, radius in [
            ("hole-thread-class-clearance", "4G", "Left", False, 0.0, 0.022, 0.011),
            ("hole-thread-custom-clearance", "6H", "Right", True, 0.08, 0.08, 0.04),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], "ISOMetricProfile")
                self.assertEqual(hole["thread_size"], "M4x0.7")
                self.assertEqual(hole["thread_class"], thread_class)
                self.assertEqual(hole["thread_direction"], direction)
                self.assertEqual(hole["use_custom_thread_clearance"], use_custom)
                self.assertAlmostEqual(hole["custom_thread_clearance"], custom, delta=1e-9)
                self.assertAlmostEqual(hole["thread_clearance"], clearance, delta=1e-9)
                self.assertAlmostEqual(hole["thread_radius_clearance"], radius, delta=1e-9)

    def test_p7_hole_model_thread_builds_freecad_pipe_shell_tool(self) -> None:
        plain = self.run_recompute("hole-supported-thread-class-clearance", "p7")
        result = self.run_recompute("hole-supported-model-thread-metric", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], True)
        self.assertEqual(hole["model_thread_geometry"], "pipe_shell")
        self.assertEqual(hole["thread_type"], "ISOMetricProfile")
        self.assertEqual(hole["thread_size"], "M4x0.7")
        self.assertEqual(hole["thread_class"], "4G")
        self.assertEqual(hole["thread_direction"], "Left")
        self.assertAlmostEqual(hole["thread_pitch"], 0.7, delta=1e-9)
        self.assertAlmostEqual(hole["thread_radius_clearance"], 0.011, delta=1e-9)
        self.assertGreater(hole["volume"], plain["objects"]["Hole"]["volume"])
        self.assertLess(result["objects"]["Body"]["volume"], plain["objects"]["Body"]["volume"])
        self.assert_object_matches_expected(result, "p7", "hole-supported-model-thread-metric")

    def test_p7_hole_angled_drill_point_extends_blind_hole_tip(self) -> None:
        result = self.run_recompute("hole-angled-drill-point", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "None")
        self.assertEqual(hole["drill_point"], "Angled")
        self.assertEqual(hole["drill_for_depth"], False)
        self.assert_object_matches_expected(result, "p7", "hole-angled-drill-point")

    def test_p7_hole_uses_sketch_points_as_hole_centers(self) -> None:
        result = self.run_recompute("hole-point-profile", "p7")
        sketch = result["objects"]["SketchHole"]
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["raw_point_count"], 1)
        self.assertEqual(sketch["profile_ready"], False)
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(hole["source_profile"], "SketchHole")
        self.assert_object_matches_expected(result, "p7", "hole-point-profile")

    def test_p7_hole_tapered_profile_uses_tapered_angle_bottom_radius(self) -> None:
        result = self.run_recompute("hole-tapered", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["tapered"], True)
        self.assertEqual(hole["tapered_angle"], 80.0)
        self.assert_object_matches_expected(result, "p7", "hole-tapered")

    def test_p7_hole_threaded_without_model_thread_uses_tap_drill_plain_tool(self) -> None:
        result = self.run_recompute("hole-threaded-cosmetic", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricProfile")
        self.assertEqual(hole["thread_size"], "M1x0.25")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 1.0)
        self.assertEqual(hole["thread_pitch"], 0.25)
        self.assertEqual(hole["diameter"], 0.75)
        self.assertEqual(hole["drill_point"], "Flat")
        self.assert_object_matches_expected(result, "p7", "hole-threaded-cosmetic")

    def test_p7_hole_threaded_fine_profile_uses_fine_tap_drill_table(self) -> None:
        result = self.run_recompute("hole-threaded-fine-cosmetic", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricFineProfile")
        self.assertEqual(hole["thread_size"], "M4x0.5")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 4.0)
        self.assertEqual(hole["thread_pitch"], 0.5)
        self.assertEqual(hole["diameter"], 3.5)
        self.assert_object_matches_expected(result, "p7", "hole-threaded-fine-cosmetic")

    def test_p7_hole_threaded_unc_profile_uses_unc_tap_drill_table(self) -> None:
        result = self.run_recompute("hole-threaded-unc-cosmetic", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "UNC")
        self.assertEqual(hole["thread_size"], "#4")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 2.845)
        self.assertEqual(hole["thread_pitch"], 0.635)
        self.assertEqual(hole["diameter"], 2.35)
        self.assert_object_matches_expected(result, "p7", "hole-threaded-unc-cosmetic")

    def test_p7_hole_threaded_unf_and_unef_profiles_use_tap_drill_tables(self) -> None:
        for fixture, thread_type, thread_size, thread_diameter, thread_pitch, diameter in [
            ("hole-threaded-unf-cosmetic", "UNF", "#4", 2.845, 0.529, 2.40),
            ("hole-threaded-unef-cosmetic", "UNEF", "#12", 5.486, 0.794, 4.80),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["diameter_source"], "thread_tap_drill")
                self.assertEqual(hole["thread_diameter"], thread_diameter)
                self.assertEqual(hole["thread_pitch"], thread_pitch)
                self.assertEqual(hole["diameter"], diameter)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_threaded_pipe_and_british_profiles_use_freecad_tables(self) -> None:
        for fixture, thread_type, thread_size, thread_diameter, thread_pitch, diameter, source in [
            (
                "hole-threaded-npt-cosmetic",
                "NPT",
                "1/16",
                7.938,
                0.941,
                7.938 - (2.0 * (0.8 * 0.941)) * 0.75,
                "thread_npt_fallback",
            ),
            (
                "hole-threaded-bsp-fallback-cosmetic",
                "BSP",
                "1 1/8",
                37.897,
                2.309,
                37.897 - (2.0 * (0.640327 * 2.309)) * 0.75,
                "thread_whitworth_fallback",
            ),
            (
                "hole-threaded-bsw-cosmetic",
                "BSW",
                "1/8",
                3.175,
                0.635,
                2.55,
                "thread_tap_drill",
            ),
            (
                "hole-threaded-bsf-cosmetic",
                "BSF",
                "3/16",
                4.763,
                0.794,
                4.00,
                "thread_tap_drill",
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["diameter_source"], source)
                self.assertEqual(hole["thread_diameter"], thread_diameter)
                self.assertEqual(hole["thread_pitch"], thread_pitch)
                self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_threaded_isotyre_profile_uses_pitch_fallback(self) -> None:
        result = self.run_recompute("hole-threaded-isotyre-cosmetic", "p7")
        hole = result["objects"]["Hole"]
        diameter = 5.334 - 0.705

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOTyre")
        self.assertEqual(hole["thread_size"], "5v1")
        self.assertEqual(hole["diameter_source"], "thread_pitch_fallback")
        self.assertEqual(hole["thread_diameter"], 5.334)
        self.assertEqual(hole["thread_pitch"], 0.705)
        self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
        self.assert_object_matches_expected(result, "p7", "hole-threaded-isotyre-cosmetic")

    def test_p7_hole_thread_clearance_uses_iso_metric_fit_table(self) -> None:
        result = self.run_recompute("hole-thread-clearance", "p7")
        hole = result["objects"]["Hole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricProfile")
        self.assertEqual(hole["thread_size"], "M4x0.7")
        self.assertEqual(hole["thread_fit"], "Coarse")
        self.assertEqual(hole["diameter_source"], "thread_clearance")
        self.assertEqual(hole["thread_diameter"], 4.0)
        self.assertEqual(hole["thread_pitch"], 0.7)
        self.assertEqual(hole["diameter"], 4.8)
        self.assert_object_matches_expected(result, "p7", "hole-thread-clearance")

    def test_p7_hole_thread_clearance_uses_uts_table_and_generic_fallback(self) -> None:
        for fixture, thread_type, thread_size, diameter, source in [
            ("hole-unc-clearance", "UNC", "#4", 3.3, "thread_uts_clearance"),
            (
                "hole-isotyre-clearance-fallback",
                "ISOTyre",
                "5v1",
                5.334 * 1.10,
                "thread_clearance_fallback",
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["thread_fit"], "Normal")
                self.assertEqual(hole["diameter_source"], source)
                self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_hole_without_base_diagnostics_are_explicit(self) -> None:
        for fixture in ["hole-without-base", "hole-threaded-known-gap"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                diagnostic = result["diagnostics"][0]

                self.assertEqual(diagnostic["code"], "execution_failed")
                self.assertEqual(diagnostic["object"], "Hole")
                self.assertEqual(diagnostic["property"], "Profile")

    def test_p7_fillet_replaces_body_tip_shape(self) -> None:
        result = self.run_recompute("fillet-pad-edge", "p7")
        fillet = result["objects"]["Fillet"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["dress_up"], "fillet")
        self.assertEqual(fillet["body_mode"], "replace")
        self.assertEqual(body["tip"], "Fillet")
        self.assert_object_matches_expected(result, "p7", "fillet-pad-edge")

    def test_p7_chamfer_replaces_body_tip_shape(self) -> None:
        result = self.run_recompute("chamfer-pad-edge", "p7")
        chamfer = result["objects"]["Chamfer"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(chamfer["status"], "ok")
        self.assertEqual(chamfer["dress_up"], "chamfer")
        self.assertEqual(chamfer["body_mode"], "replace")
        self.assertEqual(body["tip"], "Chamfer")
        self.assert_object_matches_expected(result, "p7", "chamfer-pad-edge")

    def test_p7_dressup_refine_true_uses_refinemodel_path(self) -> None:
        for fixture, object_name in [
            ("fillet-refine-true", "Fillet"),
            ("chamfer-refine-true", "Chamfer"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                dress_up = result["objects"][object_name]
                body = result["objects"]["Body"]
                dress_up_named_shape = result["named_shapes"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(dress_up["status"], "ok")
                self.assertEqual(dress_up["refine"], "applied")
                self.assertEqual(body["tip"], object_name)
                self.assert_refine_model_history(
                    dress_up_named_shape,
                    {"Pad", "SketchPad"},
                    {"Pad", "SketchPad"},
                    {("Pad", "Vertex1"), ("Pad", "Vertex2"), ("SketchPad", "Vertex1")},
                )
                self.assert_object_matches_expected(result, "p7", fixture)

    def test_p7_dressup_base_diagnostics_are_structured(self) -> None:
        result = self.run_recompute("fillet-missing-edge", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_subshape")
        self.assertEqual(diagnostic["object"], "Fillet")
        self.assertEqual(diagnostic["property"], "Base")
        self.assertEqual(diagnostic["target"], "Pad")
        self.assertEqual(diagnostic["subname"], "Edge99")

        result = self.run_recompute("chamfer-invalid-size", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "Chamfer")
        self.assertEqual(diagnostic["property"], "Size")

    def test_p7_mirrored_features_mode_fuses_transformed_additive_original(self) -> None:
        result = self.run_recompute("mirrored-pad-datum-plane", "p7")
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        mirrored_named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Pad"])
        self.assertEqual(mirrored["body_mode"], "replace")
        self.assertEqual(body["tip"], "Mirrored")
        self.assertTrue(
            any(key.startswith("Mirrored.Transform1.") for key in mirrored_named_shape["element_map"]),
            "transformed copy source aliases should survive into ElementMap",
        )
        self.assertTrue(
            any(key.startswith("Pad.") for key in mirrored_named_shape["element_map"]),
            "source feature stable aliases should survive transformed copy propagation",
        )
        self.assertTrue(
            any(
                item["kind"] == "modified"
                and any(source.startswith("Mirrored.Transform1.") for source in item["sources"])
                for item in mirrored_named_shape["history"]
            ),
            "transformed copy should record non-indexed history for copied source aliases",
        )
        self.assert_object_matches_expected(result, "p7", "mirrored-pad-datum-plane")

    def test_p7_transformed_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("mirrored-refine-true", "p7")
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        mirrored_named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["refine"], "applied")
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_refine_model_history(
            mirrored_named_shape,
            {"SketchPad"},
            {"Mirrored", "Mirrored.Transform1", "Pad", "SketchPad"},
            {
                ("Mirrored.Transform1", "Face2"),
                ("Pad", "Face2"),
                ("SketchPad", "Edge2"),
            },
        )
        self.assert_object_matches_expected(result, "p7", "mirrored-refine-true")

    def test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache(self) -> None:
        result = self.run_recompute("mirrored-fillet-support-transform", "p7")
        fillet = result["objects"]["Fillet"]
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        mirrored_named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["support_transform"], True)
        self.assertEqual(fillet["add_sub_cache"], "support_transform")
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Fillet"])
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_dressup_slot_history(mirrored_named_shape, {"Fillet", "Pad"}, "Mirrored.Transform1")
        self.assert_object_matches_expected(result, "p7", "mirrored-fillet-support-transform")

    def test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache(self) -> None:
        result = self.run_recompute("mirrored-dressup-chain-support-transform", "p7")
        fillet = result["objects"]["Fillet"]
        chamfer = result["objects"]["Chamfer"]
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        mirrored_named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["support_transform_source"], "Pad")
        self.assertEqual(chamfer["status"], "ok")
        self.assertEqual(chamfer["support_transform"], True)
        self.assertEqual(chamfer["support_transform_source"], "Pad")
        self.assertEqual(chamfer["source_base"], "Fillet")
        self.assertEqual(chamfer["add_sub_cache"], "support_transform")
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Chamfer"])
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_dressup_slot_history(
            mirrored_named_shape,
            {"Chamfer", "Fillet", "Pad"},
            "Mirrored.Transform1",
        )
        self.assert_object_matches_expected(result, "p7", "mirrored-dressup-chain-support-transform")

    def test_p7_mirrored_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("mirrored-whole-shape", "p7")
        mirrored = result["objects"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Whole shape")
        self.assertEqual(mirrored["originals"], ["Pad"])
        self.assert_object_matches_expected(result, "p7", "mirrored-whole-shape")

    def test_p7_linear_pattern_features_mode_fuses_additive_originals_by_extent(self) -> None:
        result = self.run_recompute("linear-pattern-pad-datum-line", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]
        pattern_named_shape = result["named_shapes"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_transformed_pattern_ownership(
            pattern_named_shape,
            {"LinearPattern.Transform1", "LinearPattern.Transform2"},
            {"Pad", "SketchPad"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "linear-pattern-pad-datum-line")

    def test_p7_linear_pattern_uses_sketch_construction_axis(self) -> None:
        result = self.run_recompute("linear-pattern-pad-sketch-axis", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_object_matches_expected(result, "p7", "linear-pattern-pad-sketch-axis")

    def test_p7_linear_pattern_combines_two_directions(self) -> None:
        result = self.run_recompute("linear-pattern-pad-two-directions", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_object_matches_expected(result, "p7", "linear-pattern-pad-two-directions")

    def test_p7_linear_pattern_replays_multi_original_add_and_sub_slots(self) -> None:
        result = self.run_recompute("linear-pattern-pad-pocket-multi-original", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]
        pattern_named_shape = result["named_shapes"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad", "Pocket"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_transformed_pattern_ownership(
            pattern_named_shape,
            {"LinearPattern.Transform1", "LinearPattern.Transform2"},
            {"Pad", "Pocket", "SketchPad", "SketchPocket"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "linear-pattern-pad-pocket-multi-original")

    def test_p7_linear_pattern_can_transform_subtractive_original_only(self) -> None:
        result = self.run_recompute("linear-pattern-pocket-subtractive-original", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]
        pattern_named_shape = result["named_shapes"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pocket"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_transformed_pattern_ownership(
            pattern_named_shape,
            {"LinearPattern.Transform1"},
            {"Pad", "Pocket", "SketchPad", "SketchPocket"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "linear-pattern-pocket-subtractive-original")

    def test_p7_linear_pattern_custom_spacing_list_controls_steps(self) -> None:
        result = self.run_recompute("linear-pattern-custom-spacings", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_object_matches_expected(result, "p7", "linear-pattern-custom-spacings")

    def test_p7_linear_pattern_spacing_pattern_controls_steps(self) -> None:
        result = self.run_recompute("linear-pattern-spacing-pattern", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_object_matches_expected(result, "p7", "linear-pattern-spacing-pattern")

    def test_p7_linear_pattern_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("linear-pattern-whole-shape", "p7")
        pattern = result["objects"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assert_object_matches_expected(result, "p7", "linear-pattern-whole-shape")

    def test_p7_linear_pattern_whole_shape_uses_body_prefix_support(self) -> None:
        result = self.run_recompute("linear-pattern-whole-shape-body-prefix-support", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_object_matches_expected(result, "p7", "linear-pattern-whole-shape-body-prefix-support")

    def test_p7_linear_pattern_whole_shape_consumes_refined_prefix_support(self) -> None:
        result = self.run_recompute("linear-pattern-whole-shape-refined-prefix-support", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]
        pattern_named_shape = result["named_shapes"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pocket"])
        self.assertEqual(pattern["support_refined_features"], ["Pocket"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_transformed_pattern_ownership(
            pattern_named_shape,
            {"LinearPattern.Transform1"},
            {"Pad", "Pocket", "SketchPad", "SketchPocket"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "linear-pattern-whole-shape-refined-prefix-support")

    def test_p7_polar_pattern_features_mode_rotates_additive_originals_by_extent(self) -> None:
        result = self.run_recompute("polar-pattern-pad-datum-line", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]
        pattern_named_shape = result["named_shapes"]["PolarPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_transformed_pattern_ownership(
            pattern_named_shape,
            {"PolarPattern.Transform1", "PolarPattern.Transform2", "PolarPattern.Transform3"},
            {"Pad", "SketchPad"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "polar-pattern-pad-datum-line")

    def test_p7_polar_pattern_uses_sketch_normal_axis(self) -> None:
        result = self.run_recompute("polar-pattern-pad-sketch-axis", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_object_matches_expected(result, "p7", "polar-pattern-pad-sketch-axis")

    def test_p7_polar_pattern_spacing_pattern_controls_angles(self) -> None:
        result = self.run_recompute("polar-pattern-spacing-pattern", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_object_matches_expected(result, "p7", "polar-pattern-spacing-pattern")

    def test_p7_polar_pattern_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("polar-pattern-whole-shape", "p7")
        pattern = result["objects"]["PolarPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assert_object_matches_expected(result, "p7", "polar-pattern-whole-shape")

    def test_p7_polar_pattern_whole_shape_uses_body_prefix_support(self) -> None:
        result = self.run_recompute("polar-pattern-whole-shape-body-prefix-support", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_object_matches_expected(result, "p7", "polar-pattern-whole-shape-body-prefix-support")

    def test_p7_scaled_features_mode_scales_around_first_original_center_of_mass(self) -> None:
        result = self.run_recompute("scaled-pad-factor-two", "p7")
        scaled = result["objects"]["Scaled"]
        body = result["objects"]["Body"]
        scaled_named_shape = result["named_shapes"]["Scaled"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(scaled["status"], "ok")
        self.assertEqual(scaled["transformed"], "scaled")
        self.assertEqual(scaled["transform_mode"], "Features")
        self.assertEqual(scaled["originals"], ["Pad"])
        self.assertEqual(scaled["body_mode"], "replace")
        self.assertEqual(body["tip"], "Scaled")
        self.assert_transformed_pattern_ownership(
            scaled_named_shape,
            {"Scaled.Transform1"},
            {"Pad", "SketchPad"},
            terminal=False,
        )
        self.assert_object_matches_expected(result, "p7", "scaled-pad-factor-two")

    def test_p7_scaled_diagnostics_are_structured(self) -> None:
        result = self.run_recompute("scaled-invalid-factor", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "Scaled")
        self.assertEqual(diagnostic["property"], "Factor")

    def test_p7_scaled_whole_shape_scales_support_around_origin(self) -> None:
        result = self.run_recompute("scaled-whole-shape", "p7")
        scaled = result["objects"]["Scaled"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(scaled["status"], "ok")
        self.assertEqual(scaled["transformed"], "scaled")
        self.assertEqual(scaled["transform_mode"], "Whole shape")
        self.assertEqual(scaled["originals"], ["Pad"])
        self.assert_object_matches_expected(result, "p7", "scaled-whole-shape")

    def test_p7_transformed_copy_preserves_terminal_stable_history(self) -> None:
        for fixture, code, stable_subname in [
            ("mirrored-stable-history-split", "split_stable_subname", "Pad.Face5"),
            ("mirrored-stable-history-deleted", "deleted_stable_subname", "Pocket.Face5"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                diagnostic = result["diagnostics"][0]
                history_kinds = {
                    item["kind"]
                    for item in result["named_shapes"]["Mirrored"]["history"]
                }
                history_status = result["named_shapes"]["Mirrored"]["element_history_status"]

                self.assertEqual(diagnostic["code"], code)
                self.assertEqual(diagnostic["object"], "ProbePad")
                self.assertEqual(diagnostic["property"], "UpToFace")
                self.assertEqual(diagnostic["target"], "Mirrored")
                self.assertEqual(diagnostic["subname"], stable_subname)
                self.assertIn(code.removesuffix("_stable_subname"), history_kinds)
                self.assertIn("history_consumed:generated_modified", history_status)
                self.assertIn("terminal_history:split_deleted", history_status)
                self.assertIn("history_consumed:merge", history_status)
                if code == "split_stable_subname":
                    named_shape = result["named_shapes"]["Mirrored"]
                    split_events = [
                        event
                        for event in named_shape["mapper_history"]
                        if event["relation"] == "split"
                        and event["source"] == {"object": "Body", "subname": "Edge1"}
                    ]

                    self.assertGreaterEqual(len(split_events), 2)
                    self.assertNotIn("Body.Edge1", named_shape["element_map"])
                    self.assertTrue(
                        all(event["recoverability"] == "needs_reselect" for event in split_events)
                    )
                    self.assertTrue(
                        all(event["diagnostic_status"] == "split_stable_subname" for event in split_events)
                    )

    def test_p7_multi_transform_combines_linear_pattern_and_mirror(self) -> None:
        result = self.run_recompute("multi-transform-linear-mirror", "p7")
        multi = result["objects"]["MultiTransform"]
        body = result["objects"]["Body"]
        multi_named_shape = result["named_shapes"]["MultiTransform"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["LinearPattern"]["transformation_template"], True)
        self.assertEqual(result["objects"]["Mirrored"]["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Features")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assertEqual(multi["body_mode"], "replace")
        self.assertEqual(body["tip"], "MultiTransform")
        self.assert_transformed_pattern_ownership(
            multi_named_shape,
            {
                "MultiTransform.Transform1",
                "MultiTransform.Transform2",
                "MultiTransform.Transform3",
                "MultiTransform.Transform4",
                "MultiTransform.Transform5",
            },
            {"Pad", "SketchPad"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "multi-transform-linear-mirror")

    def test_p7_multi_transform_scaled_child_uses_diagonal_composition(self) -> None:
        result = self.run_recompute("multi-transform-scaled-diagonal", "p7")
        multi = result["objects"]["MultiTransform"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["LinearPattern"]["transformation_template"], True)
        self.assertEqual(result["objects"]["Scaled"]["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(body["tip"], "MultiTransform")
        self.assert_object_matches_expected(result, "p7", "multi-transform-scaled-diagonal")

    def test_p7_multi_transform_scaled_divisor_gap_is_explicit(self) -> None:
        result = self.run_recompute("multi-transform-scaled-divisor-known-gap", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "MultiTransform")
        self.assertEqual(diagnostic["property"], "Transformations")
        self.assertIn("divisor", diagnostic["message"])

    def test_p7_multi_transform_whole_shape_uses_support_and_child_transforms(self) -> None:
        result = self.run_recompute("multi-transform-whole-shape", "p7")
        multi = result["objects"]["MultiTransform"]
        child = result["objects"]["LinearPattern"]
        multi_named_shape = result["named_shapes"]["MultiTransform"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(child["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Whole shape")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assert_transformed_pattern_ownership(
            multi_named_shape,
            {"MultiTransform.Transform1", "MultiTransform.Transform2"},
            {"Pad", "SketchPad"},
            terminal=True,
        )
        self.assert_object_matches_expected(result, "p7", "multi-transform-whole-shape")
