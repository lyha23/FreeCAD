from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreP5SketchTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
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

    def test_p5_split_line_builds_multiple_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-split-line", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-split-line")

    def test_p5_unsupported_splitter_does_not_fake_internal_region(self) -> None:
        result = self.run_recompute("sketch-internal-face-unsupported-splitter", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_profile_region"])
        self.assertEqual(sketch["status"], "ok")
        self.assertFalse(sketch["profile_ready"])
        self.assertNotIn("Sketch", result["mesh"])

    def test_p5_pad_uses_selected_internal_face_sublist(self) -> None:
        result = self.run_recompute("pad-internal-face-sublist", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["internal_face_count"], 2)
        self.assertEqual(pad["status"], "ok")
        self.assertAlmostEqual(pad["volume"], 250.0)

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

    def test_p5_external_geometry_resolves_internal_edge(self) -> None:
        result = self.run_recompute("sketch-external-internal-edge", "p5")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p5", "sketch-external-internal-edge")

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
