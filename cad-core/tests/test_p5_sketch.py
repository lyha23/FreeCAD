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

    def test_p5_sketch_exports_internal_edge_vertex_stable_subnames(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face", "p5")
        subshapes = {
            item["indexed"]: item
            for item in result["results"][0]["subshapes"]
        }

        self.assertEqual(subshapes["InternalEdge1"]["stableSubname"], "Edge1")
        self.assertEqual(subshapes["InternalVertex1"]["stableSubname"], "Vertex1")
        self.assertEqual(subshapes["InternalFace1"]["stableSubname"], "")

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
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-split-line")

    def test_p5_through_open_cutter_keeps_non_original_split_fragments(self) -> None:
        result = self.run_recompute("sketch-internal-face-through-open-cutter", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-through-open-cutter")

    def test_p5_cross_cutters_build_four_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-cross-cutters", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cross-cutters")

    def test_p5_overlapping_closed_profiles_split_into_disjoint_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-overlap-rectangles", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-overlap-rectangles")

    def test_p5_overlapping_circles_split_into_lens_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-overlap-circles", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-overlap-circles")

    def test_p5_three_overlapping_circles_split_into_venn_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-three-overlap-circles", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-three-overlap-circles")

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

    def test_p5_cross_pattern_closed_profiles_split_into_five_internal_faces(self) -> None:
        result = self.run_recompute("sketch-internal-face-cross-pattern", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-cross-pattern")

    def test_p5_dangling_open_line_keeps_bounded_internal_face(self) -> None:
        result = self.run_recompute("sketch-internal-face-dangling-line", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assert_object_matches_expected(result, "p5", "sketch-internal-face-dangling-line")
        self.assertIn("Sketch", result["mesh"])

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
        self.assertNotEqual(brep["byteLength"], 662)

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
        self.assertGreater(extrusion["volume"], 0.0)
        self.assertIn("Extrude", result["mesh"])

    def test_p5_part_extrusion_supports_reverse_taper(self) -> None:
        result = self.run_recompute("part-extrusion-reverse-taper", "p5")
        extrusion = result["objects"]["Extrude"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(extrusion["status"], "ok")
        self.assertEqual(extrusion["shape"], "occt_solid")
        self.assertEqual(extrusion["topo_naming"], "known_gap:taper_history")
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
