from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreP7FeatureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["refine"], "applied")
        self.assert_object_matches_expected(result, "p7", "pad-refine-true")

    def test_p7_pocket_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("pocket-refine-true", "p7")
        pocket = result["objects"]["Pocket"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["status"], "ok")
        self.assertNotIn("refine", pocket)
        self.assertEqual(body["refined_features"], ["Pocket"])
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

    def test_p7_hole_refine_true_uses_body_final_result_refine(self) -> None:
        result = self.run_recompute("hole-refine-true", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(body["tip"], "Hole")
        self.assertEqual(body["refined_features"], ["Hole"])
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

    def test_p7_hole_without_base_and_threaded_gaps_are_explicit(self) -> None:
        result = self.run_recompute("hole-without-base", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "execution_failed")
        self.assertEqual(diagnostic["object"], "Hole")
        self.assertEqual(diagnostic["property"], "Profile")

        result = self.run_recompute("hole-threaded-known-gap", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "unsupported_property")
        self.assertEqual(diagnostic["object"], "Hole")
        self.assertEqual(diagnostic["property"], "ModelThread")
        self.assertIn("Hole::makeThread", diagnostic["message"])

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

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(dress_up["status"], "ok")
                self.assertEqual(dress_up["refine"], "applied")
                self.assertEqual(body["tip"], object_name)
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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Pad"])
        self.assertEqual(mirrored["body_mode"], "replace")
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_object_matches_expected(result, "p7", "mirrored-pad-datum-plane")

    def test_p7_transformed_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("mirrored-refine-true", "p7")
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["refine"], "applied")
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_object_matches_expected(result, "p7", "mirrored-refine-true")

    def test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache(self) -> None:
        result = self.run_recompute("mirrored-fillet-support-transform", "p7")
        fillet = result["objects"]["Fillet"]
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["support_transform"], True)
        self.assertEqual(fillet["add_sub_cache"], "support_transform")
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Fillet"])
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_object_matches_expected(result, "p7", "mirrored-fillet-support-transform")

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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
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

    def test_p7_polar_pattern_features_mode_rotates_additive_originals_by_extent(self) -> None:
        result = self.run_recompute("polar-pattern-pad-datum-line", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "PolarPattern")
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

    def test_p7_scaled_features_mode_scales_around_first_original_center_of_mass(self) -> None:
        result = self.run_recompute("scaled-pad-factor-two", "p7")
        scaled = result["objects"]["Scaled"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(scaled["status"], "ok")
        self.assertEqual(scaled["transformed"], "scaled")
        self.assertEqual(scaled["transform_mode"], "Features")
        self.assertEqual(scaled["originals"], ["Pad"])
        self.assertEqual(scaled["body_mode"], "replace")
        self.assertEqual(body["tip"], "Scaled")
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

    def test_p7_multi_transform_combines_linear_pattern_and_mirror(self) -> None:
        result = self.run_recompute("multi-transform-linear-mirror", "p7")
        multi = result["objects"]["MultiTransform"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["LinearPattern"]["transformation_template"], True)
        self.assertEqual(result["objects"]["Mirrored"]["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Features")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assertEqual(multi["body_mode"], "replace")
        self.assertEqual(body["tip"], "MultiTransform")
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

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(child["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Whole shape")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assert_object_matches_expected(result, "p7", "multi-transform-whole-shape")
