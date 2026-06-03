from __future__ import annotations

import json
import tempfile
from pathlib import Path

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase, ROOT


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

    def test_p6_taper_thru_sections_history_is_mapper_backed(self) -> None:
        for fixture, owner, source in [
            ("pad-length-taper", "Pad", "Sketch"),
            ("pad-two-sides-taper", "Pad", "Sketch"),
            ("pad-symmetric-taper", "Pad", "Sketch"),
            ("pad-length-taper-inner-wire", "Pad", "Sketch"),
            ("pocket-length-taper", "Pocket", "SketchPocket"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                owner_object = result["objects"][owner]
                named_shape = result["named_shapes"][owner]
                history_kinds = {item["kind"] for item in named_shape["history"]}
                maker_events = [
                    event
                    for event in named_shape["mapper_history"]
                    if event["maker_stage"] == "maker_history"
                ]

                self.assertEqual(result["diagnostics"], [])
                self.assertNotIn("topo_naming", owner_object)
                self.assertNotIn("topo_naming_history", owner_object)
                self.assert_object_matches_expected(result, "p3b", fixture)
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertIn("generated", history_kinds)
                self.assertTrue(
                    any(
                        event["relation"] == "generated"
                        and event["source"]["object"] == source
                        for event in maker_events
                    )
                )
                self.assertTrue(
                    any(
                        event["relation"] == "generated"
                        and event["source"]["object"].startswith(f"{owner}.")
                        and ".TaperSection" in event["source"]["object"]
                        for event in maker_events
                    )
                )

    def test_p6_body_boolean_named_shape_records_maker_history(self) -> None:
        for fixture, required_sources in {
            "body-additive-fuse-history": ("BaseFeature.", "Pad."),
            "body-boolean-history": ("Pad.", "Pocket."),
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")

                self.assertEqual(result["diagnostics"], [])
                self.assert_result_matches_expected(result, "p6", fixture)

    def test_p6_mapper_history_core_serializes_legacy_history_and_preserved_aliases(self) -> None:
        result = self.run_recompute("body-boolean-history", "p6")
        named_shape = result["named_shapes"]["Body"]
        mapper_history = named_shape["mapper_history"]

        self.assertGreater(len(mapper_history), 0)
        for event in mapper_history[:20]:
            self.assertEqual(
                {
                    "source",
                    "target",
                    "shape_kind",
                    "relation",
                    "maker_stage",
                    "evidence",
                    "recoverability",
                    "diagnostic_status",
                },
                set(event),
            )
            self.assertIn("object", event["source"])
            self.assertIn("subname", event["source"])
            self.assertIn("object", event["target"])
            self.assertIn("subname", event["target"])

        modified = next(
            event
            for event in mapper_history
            if event["relation"] == "modified"
            and event["source"] == {"object": "Pad", "subname": "Face5"}
            and event["target"] == {"object": "Body", "subname": "Face4"}
        )
        self.assertEqual(modified["shape_kind"], "face")
        self.assertEqual(modified["maker_stage"], "maker_history")
        self.assertEqual(modified["recoverability"], "resolved")
        self.assertEqual(modified["evidence"]["legacy_history_kind"], "modified")

        preserved = next(
            event
            for event in mapper_history
            if event["relation"] == "preserved"
            and event["source"] == {"object": "Pad", "subname": "Edge1"}
            and event["target"] == {"object": "Body", "subname": "Edge1"}
        )
        self.assertEqual(preserved["maker_stage"], "element_map_preserved")
        self.assertEqual(preserved["evidence"]["element_map"], True)
        self.assertEqual(named_shape["element_map"]["Pad.Edge1"], "Edge1")

        deleted = next(
            event
            for event in mapper_history
            if event["relation"] == "deleted"
            and event["source"] == {"object": "Pocket", "subname": "Face5"}
        )
        self.assertEqual(deleted["target"], {"object": "Body", "subname": ""})
        self.assertEqual(deleted["recoverability"], "deleted")
        self.assertEqual(deleted["diagnostic_status"], "deleted_stable_subname")
        self.assertNotIn("Pocket.Face5", named_shape["element_map"])

    def test_p6_body_boolean_propagates_nested_element_map_aliases(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-body-profile-source", "p6")
        sketch = result["objects"]["ProbeSketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_result_matches_expected(result, "p6", "sketch-external-edge-stable-body-profile-source")
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 1)

    def test_p6_body_split_history_promotes_unique_same_kind_targets(self) -> None:
        result = self.run_recompute("body-split-history", "p6")

        self.assertEqual(result["diagnostics"], [])
        self.assert_result_matches_expected(result, "p6", "body-split-history")

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
        diagnostic = self.run_recompute("up-to-face-stable-body-split", "p6")["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "execution_failed")

        for fixture in [
            "sketch-external-edge-stable-body-split",
            "sketch-external-edge-stable-body-split-after-add",
            "sketch-external-edge-stable-body-split-current-sublist",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                sketch = result["objects"]["ProbeSketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_point_count"], 1)
                self.assert_result_matches_expected(result, "p6", fixture)

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

    def test_p6_reference_shadow_update_uses_stable_element_map_for_up_to_face(self) -> None:
        fixture_path = ROOT / "fixtures" / "p6" / "up-to-face-stable-body-history.json"
        payload = json.loads(fixture_path.read_text(encoding="utf-8"))
        up_to_face = next(item for item in payload["Objects"] if item["Name"] == "ProbePad")["Properties"]["UpToFace"]
        up_to_face["ReferenceShadow"] = [
            {
                "target": "Body",
                "targetId": 5,
                "property": "Shape",
                "shapeType": "Face",
                "indexed": "OldBodyFace",
                "subname": "OldBodyFace",
                "stableSubname": "Pad.Face6",
                "fingerprint": {},
            }
        ]

        temp_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile("w", suffix=".json", encoding="utf-8", delete=False) as temp:
                json.dump(payload, temp)
                temp_path = Path(temp.name)
            result = self.run_recompute_file(temp_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)

        update = result["elementReferenceUpdates"][0]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["ProbePad"]["status"], "ok")
        self.assertEqual(update["object"], "ProbePad")
        self.assertEqual(update["property"], "UpToFace")
        self.assertEqual(update["SubList"], ["Face5"])
        self.assertEqual(update["StableSubList"], ["Pad.Face6"])
        self.assertEqual(update["ShadowSub"], [{"newName": "Pad.Face6", "oldName": "Face5"}])
        self.assertEqual(update["ReferenceShadow"][0]["subname"], "Face5")

    def test_p6_external_geometry_link_sub_list_uses_element_map(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-indexed-opaque-sublist", "p6")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)
        self.assertEqual(pad["status"], "ok")

        for fixture, point_count in [
            ("sketch-external-edge-stable-body-preserved", 0),
            ("sketch-external-edge-stable-body-profile-source", 1),
            ("sketch-external-edge-stable-multi-prism", 0),
            ("sketch-external-edge-stable-taper-preserved", 0),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                sketch = result["objects"]["ProbeSketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], 0)
                self.assertEqual(sketch["external_point_count"], point_count)
