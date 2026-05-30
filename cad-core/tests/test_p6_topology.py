from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreP6TopologyTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_p6_named_shape_exports_indexed_element_ledger(self) -> None:
        result = self.run_recompute("named-shape-indexed-pad", "p6")

        self.assertEqual(result["diagnostics"], [])
        self.assert_result_matches_expected(result, "p6", "named-shape-indexed-pad")

    def test_p6_two_side_and_symmetric_fast_prisms_keep_profile_history(self) -> None:
        for fixture, owner, source in [
            ("pad-two-sides-length", "Pad", "Sketch"),
            ("pad-symmetric-length", "Pad", "Sketch"),
            ("pocket-two-sides-length", "Pocket", "SketchPocket"),
            ("pocket-symmetric-length", "Pocket", "SketchPocket"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p6_multi_prism_xor_propagates_profile_history(self) -> None:
        for fixture in [
            "pad-two-sides-up-to-face1",
            "pad-two-sides-up-to-shape1",
            "pad-two-sides-up-to-face2",
            "pad-two-sides-up-to-shape2",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p6_taper_preserved_sources_are_partial_history(self) -> None:
        for fixture, owner, source, extra_edges in [
            ("pad-length-taper", "Pad", "Sketch", []),
            ("pad-two-sides-taper", "Pad", "Sketch", []),
            ("pad-symmetric-taper", "Pad", "Sketch", []),
            ("pad-length-taper-inner-wire", "Pad", "Sketch", ["Edge5"]),
            ("pocket-length-taper", "Pocket", "SketchPocket", []),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(result["objects"][owner]["topo_naming"], "known_gap:taper_history")
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p6_body_boolean_named_shape_records_maker_history(self) -> None:
        for fixture, required_sources in {
            "body-additive-fuse-history": ("BaseFeature.", "Pad."),
            "body-boolean-history": ("Pad.", "Pocket."),
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")

                self.assertEqual(result["diagnostics"], [])
                self.assert_result_matches_expected(result, "p6", fixture)

    def test_p6_body_boolean_propagates_nested_element_map_aliases(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-body-profile-source", "p6")
        sketch = result["objects"]["ProbeSketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_result_matches_expected(result, "p6", "sketch-external-edge-stable-body-profile-source")
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)

    def test_p6_body_split_history_promotes_unique_same_kind_targets(self) -> None:
        result = self.run_recompute("body-split-history", "p6")
        body_named_shape = result["named_shapes"]["Body"]
        history_kinds = {item["kind"] for item in body_named_shape["history"]}

        self.assertEqual(result["diagnostics"], [])
        self.assert_result_matches_expected(result, "p6", "body-split-history")
        self.assertEqual(body_named_shape["element_map"]["Pad.Face5"], "Face4")
        self.assertEqual(body_named_shape["element_map"]["Pocket.Edge1"], "Edge22")
        self.assertNotIn("split", history_kinds)

    def test_p6_stable_subname_history_diagnostics(self) -> None:
        for fixture, code, object_name, property_name, stable_subname in [
            ("up-to-face-stable-body-deleted", "deleted_stable_subname", "ProbePad", "UpToFace", "Pocket.Face5"),
            ("sketch-external-edge-stable-body-deleted", "deleted_stable_subname", "ProbeSketch", "ExternalGeometry", "Pocket.Edge11"),
            (
                "sketch-external-edge-stable-body-deleted-after-add",
                "deleted_stable_subname",
                "ProbeSketch",
                "ExternalGeometry",
                "Pocket.Edge11",
            ),
        ]:
            with self.subTest(fixture=fixture):
                diagnostic = self.run_recompute(fixture, "p6")["diagnostics"][0]

                self.assertEqual(diagnostic["code"], code)
                self.assertEqual(diagnostic["object"], object_name)
                self.assertEqual(diagnostic["property"], property_name)
                self.assertEqual(diagnostic["target"], "Body")
                self.assertEqual(diagnostic["subname"], stable_subname)

    def test_p6_split_stable_subname_reaches_downstream_geometry_after_recovery(self) -> None:
        for fixture, code in [
            ("up-to-face-stable-body-split", "execution_failed"),
            ("sketch-external-edge-stable-body-split", "unsupported_geometry"),
            ("sketch-external-edge-stable-body-split-after-add", "unsupported_geometry"),
            ("sketch-external-edge-stable-body-split-current-sublist", "unsupported_geometry"),
        ]:
            with self.subTest(fixture=fixture):
                diagnostic = self.run_recompute(fixture, "p6")["diagnostics"][0]

                self.assertEqual(diagnostic["code"], code)
                self.assertNotEqual(diagnostic["code"], "split_stable_subname")

    def test_p6_up_to_face_uses_element_map_before_stale_sublist(self) -> None:
        for fixture in [
            "up-to-face-stable-indexed-reference",
            "up-to-face-stable-indexed-opaque-sublist",
            "up-to-face-stable-body-history",
            "up-to-face-stable-body-preserved",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                feature = result["objects"].get("ProbePad", result["objects"].get("Pocket"))

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(feature["status"], "ok")
                self.assertEqual(feature["method"], "UpToFace")

    def test_p6_external_geometry_link_sub_list_uses_element_map(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-indexed-opaque-sublist", "p6")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)
        self.assertEqual(pad["status"], "ok")

        for fixture in [
            "sketch-external-edge-stable-body-preserved",
            "sketch-external-edge-stable-body-profile-source",
            "sketch-external-edge-stable-multi-prism",
            "sketch-external-edge-stable-taper-preserved",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                sketch = result["objects"]["ProbeSketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], 0)
                self.assertEqual(sketch["external_point_count"], 0)
