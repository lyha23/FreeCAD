from __future__ import annotations

import unittest

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreFeatureFlowTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_rect_pad_pocket_outputs_cut_body(self) -> None:
        result = self.run_recompute("rect-pad-pocket", "p2")
        expected = self.expected_freecad("p2", "rect-pad-pocket")
        expected_object = next(iter(self.expected_result_objects(expected)))
        mesh = result["mesh"][expected_object]

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p2", "rect-pad-pocket")
        self.assertGreater(mesh["summary"]["triangle_count"], 0)
        self.assertNotIn("shapes", result)

    def test_body_basefeature_pad_uses_base_solid(self) -> None:
        result = self.run_recompute("body-basefeature-pad", "p2")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p2", "body-basefeature-pad")

    def test_p3a_pocket_through_all_outputs_cut_body(self) -> None:
        result = self.run_recompute("pocket-through-all", "p3a")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3a", "pocket-through-all")
        self.assertEqual(result["objects"]["Pocket"]["method"], "ThroughAll")

    def test_p3a_pocket_through_all_without_base_does_not_fake_body(self) -> None:
        result = self.run_recompute("pocket-through-all-without-base", "p3a")

        self.assertEqual(self.diagnostic_codes("pocket-through-all-without-base", "p3a"), ["execution_failed"])
        self.assertEqual(result["objects"]["Pocket"]["status"], "error")
        self.assertEqual(result["objects"]["Body"]["status"], "skipped")
        self.assertNotIn("Body", result["mesh"])

    def test_p3a_pocket_up_to_face_outputs_cut_body(self) -> None:
        result = self.run_recompute("pocket-up-to-face", "p3a")
        repeat = self.run_recompute("pocket-up-to-face", "p3a")
        expected = self.expected_freecad("p3a", "pocket-up-to-face")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(repeat["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3a", "pocket-up-to-face")
        self.assert_expected_object(repeat, expected["object"], expected)
        self.assertEqual(result["objects"]["Pocket"]["method"], "UpToFace")
        self.assertEqual(sorted(result["subshapes"]["Body"]), sorted(repeat["subshapes"]["Body"]))

    def test_p3a_pocket_up_to_shape_outputs_cut_body(self) -> None:
        for fixture in [
            "pocket-up-to-shape-face",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3a")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3a", fixture)
                self.assertEqual(result["objects"]["Pocket"]["method"], "UpToShape")

    def test_p3a_pad_up_to_face_outputs_solid(self) -> None:
        result = self.run_recompute("pad-up-to-face", "p3a")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["method"], "UpToFace")
        self.assert_object_matches_expected(result, "p3a", "pad-up-to-face")

    def test_p3a_up_to_shape_multi_face_failure_boundaries_are_structured(self) -> None:
        offset = self.run_recompute("pocket-up-to-shape-multiple-faces-offset", "p3a")
        invalid = self.run_recompute("pocket-up-to-shape-edge-subshape", "p3a")

        self.assertEqual([item["code"] for item in offset["diagnostics"]], ["unsupported_property"])
        self.assertEqual(offset["diagnostics"][0]["message"], "Extrude: Can only offset one face")
        self.assertEqual(offset["diagnostics"][0]["property"], "Offset")
        self.assertEqual(offset["objects"]["Pocket"]["status"], "error")
        self.assertEqual([item["code"] for item in invalid["diagnostics"]], ["unsupported_subshape_kind"])
        self.assertEqual(invalid["diagnostics"][0]["property"], "UpToShape")
        self.assertEqual(invalid["diagnostics"][0]["subname"], "Edge1")
        self.assertEqual(invalid["objects"]["Pocket"]["status"], "error")

    def test_p3b_two_sides_length_outputs_expected_extents(self) -> None:
        result = self.run_recompute("pad-two-sides-length", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pad-two-sides-length")

    def test_p3b_two_sides_up_to_targets(self) -> None:
        for fixture in [
            "pad-two-sides-up-to-face1",
            "pad-two-sides-up-to-shape1",
            "pad-two-sides-up-to-face2",
            "pad-two-sides-up-to-shape2",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(pad["method"], "Two sides")
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_up_to_first_last_selects_nearest_or_furthest_body_face(self) -> None:
        for fixture, method in [
            ("pad-up-to-first", "UpToFirst"),
            ("pad-up-to-last", "UpToLast"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(result["objects"]["Pad"]["method"], method)
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_pocket_two_sides_length_cuts_body(self) -> None:
        result = self.run_recompute("pocket-two-sides-length", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pocket-two-sides-length")

    def test_p3b_symmetric_length_outputs_expected_extents(self) -> None:
        for fixture in ["pad-symmetric-length", "pocket-symmetric-length"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_start_offset_moves_profile_before_side_logic(self) -> None:
        for fixture, expected_min, expected_max, method in [
            ("pad-start-offset", [0.0, 0.0, 5.0], [10.0, 5.0, 15.0], "Length"),
            ("pad-start-offset-reversed", [0.0, 0.0, -15.0], [10.0, 5.0, -5.0], "Length"),
            ("pad-symmetric-start-offset", [0.0, 0.0, -5.0], [10.0, 5.0, 15.0], "Symmetric"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(pad["method"], method)
                self.assert_bbox_close(pad["bbox"], expected_min, expected_max)

    def test_p3b_custom_vector_uses_along_sketch_normal_length(self) -> None:
        result = self.run_recompute("pad-custom-vector", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pad-custom-vector")

    def test_p3b_pocket_custom_vector_cuts_body(self) -> None:
        result = self.run_recompute("pocket-custom-vector", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["method"], "Length")
        self.assert_object_matches_expected(result, "p3b", "pocket-custom-vector")

    def test_p3b_reference_axis_uses_sketch_normal_axis(self) -> None:
        for fixture in ["pad-reference-axis", "pad-reference-axis-edge"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_sketch_placement_transforms_profile(self) -> None:
        result = self.run_recompute("pad-sketch-placement", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pad-sketch-placement")

    def test_p3b_custom_direction_with_placement(self) -> None:
        for fixture in ["pad-custom-direction-placement", "pad-custom-direction-sketch-rotation"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_body_and_featurebase_placement(self) -> None:
        result = self.run_recompute("pocket-body-placement", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pocket-body-placement")

        result = self.run_recompute("body-basefeature-placement", "p3b")
        body = result["objects"]["Body"]
        feature_base = result["objects"]["FeatureBase"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "body-basefeature-placement")
        self.assertEqual(feature_base["bbox"], body["bbox"])
        self.assertAlmostEqual(feature_base["volume"], body["volume"])

    def test_p3b_taper_outputs_geometry_and_consumes_thru_sections_history(self) -> None:
        result = self.run_recompute("pad-length-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertNotIn("topo_naming", pad)
        self.assertEqual(pad["topo_naming_history"], "maker_history:taper_thru_sections")
        self.assert_object_matches_expected(result, "p3b", "pad-length-taper")

        result = self.run_recompute("pad-two-sides-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertNotIn("topo_naming", pad)
        self.assertEqual(pad["topo_naming_history"], "maker_history:taper_thru_sections")
        self.assertEqual(pad["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pad-two-sides-taper")

        result = self.run_recompute("pad-symmetric-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertNotIn("topo_naming", pad)
        self.assertEqual(pad["topo_naming_history"], "maker_history:taper_thru_sections")
        self.assertEqual(pad["method"], "Symmetric")
        self.assert_object_matches_expected(result, "p3b", "pad-symmetric-taper")

    def test_p3b_taper_supports_inner_wire_profile(self) -> None:
        result = self.run_recompute("pad-length-taper-inner-wire", "p3b")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["profile_ready"], True)
        self.assertEqual(sketch["edge_count"], 8)
        self.assertNotIn("topo_naming", pad)
        self.assertEqual(pad["topo_naming_history"], "maker_history:taper_thru_sections")
        self.assert_object_matches_expected(result, "p3b", "pad-length-taper-inner-wire")

    def test_p3b_pocket_taper_cuts_body(self) -> None:
        result = self.run_recompute("pocket-length-taper", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertNotIn("topo_naming", pocket)
        self.assertEqual(pocket["topo_naming_history"], "maker_history:taper_thru_sections")
        self.assert_object_matches_expected(result, "p3b", "pocket-length-taper")

    def test_p4_normalized_links_drive_graph_and_executors(self) -> None:
        result = self.run_recompute("body-link-list", "p4")
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["status"], "ok")
        self.assertEqual(body["group"], ["Sketch", "Pad"])
        self.assertEqual(body["tip"], "Pad")
        self.assert_object_matches_expected(result, "p4", "body-link-list")

        result = self.run_recompute("feature-link-sub-list", "p4")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["method"], "UpToShape")
        self.assertEqual(pad["source_profile"], "Sketch")
        self.assert_object_matches_expected(result, "p4", "feature-link-sub-list")

    def test_p4_part_local_placement_transforms_body_output(self) -> None:
        result = self.run_recompute("part-placement-body", "p4")
        body = result["objects"]["Body"]
        part = result["objects"]["Part"]
        mesh_summary = result["mesh"]["Body"]["summary"]
        part_mesh_summary = result["mesh"]["Part"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p4", "part-placement-body")
        self.assertEqual(part["display_object"], "Body")
        self.assertEqual(part["bbox"], body["bbox"])
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertEqual(part_mesh_summary["bbox"], part["bbox"])
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])
        self.assertAlmostEqual(part["volume"], body["volume"])

    def test_p4_sketch_placement_pocket_uses_same_body_coordinates(self) -> None:
        result = self.run_recompute("sketch-placement-pocket", "p4")
        body = result["objects"]["Body"]
        mesh_summary = result["mesh"]["Body"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p4", "sketch-placement-pocket")
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])

    def test_p4_typed_scalar_properties_feed_feature_extrude(self) -> None:
        result = self.run_recompute("typed-property-pad", "p4")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["method"], "Length")
        self.assert_object_matches_expected(result, "p4", "typed-property-pad")

    def test_p4_datum_plane_support_places_sketch_profile(self) -> None:
        result = self.run_recompute("datum-plane-support", "p4")
        body = result["objects"]["Body"]
        pad = result["objects"]["Pad"]
        mesh_summary = result["mesh"]["Body"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["DatumPlane"]["status"], "ok")
        self.assert_object_matches_expected(result, "p4", "datum-plane-support")
        self.assertEqual(pad["bbox"], body["bbox"])
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])

    def test_p4_datum_line_reference_axis_drives_pad_direction(self) -> None:
        result = self.run_recompute("datum-line-reference-axis", "p4")
        datum_line = result["objects"]["DatumLine"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(datum_line["status"], "ok")
        self.assertEqual(datum_line["datum"], "line")
        self.assertAlmostEqual(datum_line["direction"][0], 0.7071067811865476)
        self.assertAlmostEqual(datum_line["direction"][1], 0.0)
        self.assertAlmostEqual(datum_line["direction"][2], 0.7071067811865475)
        self.assertEqual(pad["method"], "Length")
        self.assert_object_matches_expected(result, "p4", "datum-line-reference-axis")

    def test_p4_datum_point_uses_parent_part_placement(self) -> None:
        result = self.run_recompute("datum-point-part-placement", "p4")
        datum_point = result["objects"]["DatumPoint"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(datum_point["status"], "ok")
        self.assertEqual(datum_point["datum"], "point")
        self.assertEqual(datum_point["point"], [13.0, 4.0, 5.0])


if __name__ == "__main__":
    unittest.main()
