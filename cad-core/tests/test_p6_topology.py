from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreP6TopologyTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_p6_named_shape_exports_indexed_element_ledger(self) -> None:
        result = self.run_recompute("named-shape-indexed-pad", "p6")
        named_shape = result["named_shapes"]["Pad"]
        elements = named_shape["elements"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["owner"], "Pad")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Face1"], "Face1")
        self.assertEqual(elements[named_shape["element_map"]["Sketch.Edge1"]]["kind"], "edge")
        self.assertEqual(elements[named_shape["element_map"]["Sketch.Vertex1"]]["kind"], "vertex")
        self.assertIn("Face1", elements)
        self.assertIn("Edge1", elements)
        self.assertIn("Vertex1", elements)
        self.assertEqual(elements["Face1"]["kind"], "face")
        self.assertEqual(elements["Face1"]["status"], "generated")
        self.assertEqual(set(elements), set(result["subshapes"]["Pad"]))
        generated_sources = {
            source
            for item in named_shape["history"]
            if item["kind"] == "generated"
            for source in item["sources"]
        }
        self.assertIn("Sketch.Edge1", generated_sources)
        self.assertIn("Sketch.Vertex1", generated_sources)

    def test_p6_two_side_and_symmetric_fast_prisms_keep_profile_history(self) -> None:
        for fixture, owner, source in [
            ("pad-two-sides-length", "Pad", "Sketch"),
            ("pad-symmetric-length", "Pad", "Sketch"),
            ("pocket-two-sides-length", "Pocket", "SketchPocket"),
            ("pocket-symmetric-length", "Pocket", "SketchPocket"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"][owner]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Vertex1"]]["kind"], "vertex")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Face1"]]["kind"], "face")

    def test_p6_multi_prism_xor_propagates_profile_history(self) -> None:
        for fixture in [
            "pad-two-sides-up-to-face1",
            "pad-two-sides-up-to-shape1",
            "pad-two-sides-up-to-face2",
            "pad-two-sides-up-to-shape2",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"]["Pad"]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertTrue(any(key.startswith("Pad.XorUnion1.") for key in named_shape["element_map"]))
                self.assertEqual(elements[named_shape["element_map"]["Sketch.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"]["Sketch.Vertex1"]]["kind"], "vertex")

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
                named_shape = result["named_shapes"][owner]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(result["objects"][owner]["topo_naming"], "known_gap:taper_history")
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Vertex1"]]["kind"], "vertex")
                for edge in extra_edges:
                    self.assertEqual(elements[named_shape["element_map"][f"{source}.{edge}"]]["kind"], "edge")

    def test_p6_taper_records_thru_sections_generated_history(self) -> None:
        for fixture, owner, source_faces, source_edges, section_sources in [
            ("pad-length-taper", "Pad", ["Sketch.Face1"], ["Sketch.Edge1"], ["Pad.TaperSection2.Edge1"]),
            (
                "pocket-length-taper",
                "Pocket",
                ["SketchPocket.Face1"],
                ["SketchPocket.Edge1"],
                ["Pocket.TaperSection2.Edge1"],
            ),
            (
                "pad-two-sides-taper",
                "Pad",
                [],
                ["Sketch.Edge1"],
                ["Pad.Prism1.TaperSection2.Edge1", "Pad.Prism2.TaperSection2.Edge1"],
            ),
            (
                "pad-symmetric-taper",
                "Pad",
                [],
                ["Sketch.Edge1"],
                ["Pad.Prism1.TaperSection2.Edge1", "Pad.Prism2.TaperSection2.Edge1"],
            ),
            (
                "pad-length-taper-inner-wire",
                "Pad",
                ["Sketch.Face1"],
                ["Sketch.Edge1", "Sketch.Edge5"],
                ["Pad.Outer.TaperSection2.Edge1", "Pad.Inner1.TaperSection2.Edge1"],
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"][owner]
                history = named_shape["history"]

                self.assertEqual(result["diagnostics"], [])
                for source_face in source_faces:
                    self.assertIn(source_face, named_shape["element_map"])
                    self.assertEqual(named_shape["elements"][named_shape["element_map"][source_face]]["kind"], "face")
                    self.assertTrue(
                        any(item["kind"] == "generated" and item["sources"] == [source_face] for item in history)
                    )
                for source_edge in source_edges:
                    self.assertTrue(
                        any(item["kind"] == "generated" and item["sources"] == [source_edge] for item in history)
                    )
                for section_source in section_sources:
                    self.assertTrue(
                        any(
                            item["kind"] == "generated"
                            and item["sources"] == [section_source]
                            for item in history
                        )
                    )

    def test_p6_body_boolean_named_shape_records_maker_history(self) -> None:
        for fixture, required_sources in {
            "body-additive-fuse-history": ("BaseFeature.", "Pad."),
            "body-boolean-history": ("Pad.", "Pocket."),
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                named_shape = result["named_shapes"]["Body"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["owner"], "Body")
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(named_shape["element_map"]["Face1"], "Face1")
                non_indexed_sources = {
                    source
                    for item in named_shape["history"]
                    if item["kind"] != "indexed"
                    for source in item["sources"]
                }
                for required_source in required_sources:
                    self.assertTrue(any(source.startswith(required_source) for source in non_indexed_sources))
                self.assertTrue(any(key.startswith(required_sources[0]) for key in named_shape["element_map"]))
                self.assertTrue(any(key.startswith(required_sources[1]) for key in named_shape["element_map"]))

    def test_p6_body_boolean_propagates_nested_element_map_aliases(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-body-profile-source", "p6")
        named_shape = result["named_shapes"]["Body"]
        sketch = result["objects"]["ProbeSketch"]
        elements = named_shape["elements"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(elements[named_shape["element_map"]["SketchPad.Edge1"]]["kind"], "edge")
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)

    def test_p6_body_boolean_records_merge_history_for_shared_current_elements(self) -> None:
        for fixture, stable_sources in [
            ("body-additive-fuse-history", ("Pad.Edge3", "TopSketch.Edge1")),
            ("body-boolean-history", ("Pocket.Edge3", "SketchPocket.Edge1")),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                named_shape = result["named_shapes"]["Body"]
                target = named_shape["element_map"][stable_sources[1]]

                self.assertEqual(result["diagnostics"], [])
                self.assertTrue(
                    any(
                        item["kind"] == "merge"
                        and item["element"] == target
                        and all(source in item["sources"] for source in stable_sources)
                        for item in named_shape["history"]
                    )
                )

    def test_p6_body_split_history_does_not_guess_element_map(self) -> None:
        result = self.run_recompute("body-split-history", "p6")
        named_shape = result["named_shapes"]["Body"]
        split_entries = [item for item in named_shape["history"] if item["kind"] == "split"]
        split_sources = {source for item in split_entries for source in item["sources"]}
        deleted_elements = {item["element"] for item in named_shape["history"] if item["kind"] == "deleted"}

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Pad.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["Pad.Edge1"], "Edge1")
        self.assertEqual(named_shape["element_map"]["Pad.Vertex1"], "Vertex1")
        self.assertIn("Pad.Face5", split_sources)
        self.assertNotIn("Pad.Face5", named_shape["element_map"])
        self.assertTrue(any(element["status"] == "split" for element in named_shape["elements"].values()))
        self.assertIn("Pocket.Face5", deleted_elements)
        self.assertNotIn("Pocket.Face5", named_shape["element_map"])

    def test_p6_stable_subname_history_diagnostics(self) -> None:
        for fixture, code, object_name, property_name, stable_subname in [
            ("up-to-face-stable-body-split", "split_stable_subname", "ProbePad", "UpToFace", "Pad.Face5"),
            ("up-to-face-stable-body-deleted", "deleted_stable_subname", "ProbePad", "UpToFace", "Pocket.Face5"),
            ("sketch-external-edge-stable-body-split", "split_stable_subname", "ProbeSketch", "ExternalGeometry", "Pocket.Edge1"),
            ("sketch-external-edge-stable-body-deleted", "deleted_stable_subname", "ProbeSketch", "ExternalGeometry", "Pocket.Edge11"),
            (
                "sketch-external-edge-stable-body-split-after-add",
                "split_stable_subname",
                "ProbeSketch",
                "ExternalGeometry",
                "Pocket.Edge1",
            ),
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
