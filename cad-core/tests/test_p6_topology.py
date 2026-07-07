from __future__ import annotations

import json
import math
import subprocess
import tempfile
from pathlib import Path

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import BIN, CadCoreFixtureTestCase, ROOT
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import BIN, CadCoreFixtureTestCase, ROOT


class CadCoreP6TopologyTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def run_c3m1_probe(self, fixture: str) -> dict:
        probe = ROOT / "build" / "cad-core-c3m1-topology-probe"
        fixture_path = ROOT / "fixtures" / "c3m1" / f"{fixture}.json"
        completed = subprocess.run(
            [str(probe), str(fixture_path)],
            cwd=ROOT,
            check=True,
            text=True,
            capture_output=True,
        )
        return json.loads(completed.stdout)

    def c4m4_result(self, fixture: str) -> dict:
        return self.run_recompute(fixture, "c4m4")

    def single_reference_update_item(self, result: dict) -> dict:
        self.assertEqual(len(result["elementReferenceUpdates"]), 1)
        update = result["elementReferenceUpdates"][0]
        if "SubSet" in update:
            self.assertEqual(len(update["SubSet"]), 1)
            return update["SubSet"][0]
        return update

    def p6_payload(self, fixture: str) -> dict:
        path = ROOT / "fixtures" / "p6" / f"{fixture}.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def run_payload(self, payload: dict) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            input_path = Path(tmp) / "payload.json"
            input_path.write_text(json.dumps(payload), encoding="utf-8")
            return self.run_recompute_file(input_path)

    def run_response_payload(self, payload: dict) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            input_path = Path(tmp) / "payload.json"
            output_path = Path(tmp) / "payload.result.json"
            input_path.write_text(json.dumps(payload), encoding="utf-8")
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def assert_duplicate_stable_diagnostics(
        self,
        result: dict,
        object_name: str,
        expected_conflicts: dict[str, set[str]],
    ) -> None:
        diagnostics = [
            item
            for item in result["diagnostics"]
            if item["code"] == "duplicate_stable_subname"
        ]
        self.assertTrue(diagnostics)
        self.assertFalse(
            any(item["object"] == object_name for item in result["results"]),
            "response must not publish a target result with duplicate stableSubname",
        )
        actual_conflicts = {
            item["target"]: set(item["subname"].split(", "))
            for item in diagnostics
            if item["object"] == object_name
        }
        for target, indexed in expected_conflicts.items():
            self.assertEqual(actual_conflicts.get(target), indexed)
        for diagnostic in diagnostics:
            if diagnostic["object"] != object_name:
                continue
            self.assertEqual(diagnostic["severity"], "error")
            self.assertEqual(diagnostic["stage"], "response")

    def assert_edge_identity_contract(self, result: dict, object_name: str) -> None:
        body = next(item for item in result["results"] if item["object"] == object_name)
        edge_subshapes = {
            item["id"]: item
            for item in body["subshapes"]
            if item["kind"] == "Edge"
        }
        edge_segments = body["mesh"]["edgeSegments"]
        accepted_statuses = {"stable", "stable_split_fragment"}

        self.assertTrue(edge_subshapes)
        self.assertTrue(edge_segments)
        for subshape in edge_subshapes.values():
            self.assertIn(subshape.get("identityStatus"), accepted_statuses)
        self.assertEqual({segment["id"] for segment in edge_segments}, set(edge_subshapes))
        for segment in edge_segments:
            subshape = edge_subshapes[segment["id"]]
            self.assertEqual(segment["indexed"], subshape["indexed"])
            self.assertIn(segment.get("identityStatus"), accepted_statuses)
            self.assertEqual(segment["identityStatus"], subshape["identityStatus"])

    def nearest_edge_distance(self, result_item: dict, point: tuple[float, float, float]) -> float:
        def point_segment_distance(a: list[float], b: list[float]) -> float:
            ab = [b[index] - a[index] for index in range(3)]
            ap = [point[index] - a[index] for index in range(3)]
            denominator = sum(value * value for value in ab)
            if denominator == 0:
                return math.dist(point, a)
            t = max(0.0, min(1.0, sum(ap[index] * ab[index] for index in range(3)) / denominator))
            closest = [a[index] + t * ab[index] for index in range(3)]
            return math.dist(point, closest)

        distances: list[float] = []
        for edge in result_item["mesh"]["edgeSegments"]:
            points = edge["points"]
            distances.extend(
                point_segment_distance(points[index], points[index + 1])
                for index in range(len(points) - 1)
            )
        return min(distances)

    def assert_c4m4_update(
        self,
        fixture: str,
        stable_subname: str,
        current_subname: str,
        *,
        object_name: str,
        property_name: str,
    ) -> dict:
        result = self.c4m4_result(fixture)

        self.assertEqual(result["diagnostics"], [])
        item = self.single_reference_update_item(result)
        self.assertEqual(item["StableSubList"], [stable_subname])
        self.assertEqual(item["SubList"], [current_subname])
        self.assertEqual(
            item["ShadowSub"],
            [{"newName": stable_subname, "oldName": current_subname}],
        )
        self.assertEqual(item["ReferenceShadow"][0]["stableSubname"], stable_subname)
        update = result["elementReferenceUpdates"][0]
        self.assertEqual(update["object"], object_name)
        self.assertEqual(update["property"], property_name)
        return result

    def assert_c4m4_diagnostic(
        self,
        fixture: str,
        code: str,
        object_name: str,
        property_name: str,
        target: str,
        subname: str,
    ) -> dict:
        result = self.c4m4_result(fixture)

        self.assertEqual(result["elementReferenceUpdates"], [])
        diagnostic = result["diagnostics"][0]
        self.assertEqual(diagnostic["code"], code)
        self.assertEqual(diagnostic["object"], object_name)
        self.assertEqual(diagnostic["property"], property_name)
        self.assertEqual(diagnostic["target"], target)
        self.assertEqual(diagnostic["subname"], subname)
        return result

    def test_p6_body_tip_face_profile_replays_body_until_target_feature(self) -> None:
        result = self.run_recompute("body-tip-face-profile-pad-after-revolution", "p6")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["PadPreview"]["status"], "ok")
        self.assertEqual(result["objects"]["PadPreviewBody"]["status"], "ok")
        self.assertEqual(result["objects"]["PadPreviewBody"]["replay_stopped_at_tip"], "PadPreview")
        self.assertGreater(len(result["mesh"]["PadPreviewBody"]["triangles"]), 0)
        self.assertGreater(len(result["subshapes"]["PadPreviewBody"]), 0)

    def test_p6_pad_preview_edges_publish_identity_status_in_core_results(self) -> None:
        payload = self.p6_payload("body-tip-face-profile-pad-after-revolution")
        payload["recompute"]["objs"] = ["PadPreview", "PadPreviewBody"]

        cli_result = self.run_response_payload(payload)
        ffi_result = self.run_recompute_ffi_payload(payload)

        self.assertEqual(cli_result["diagnostics"], [])
        self.assertEqual(ffi_result["diagnostics"], [])
        for result in (cli_result, ffi_result):
            self.assert_edge_identity_contract(result, "PadPreview")
            self.assert_edge_identity_contract(result, "PadPreviewBody")

    def test_p6_revolution_body_display_face_is_not_feature_local_profile(self) -> None:
        result = self.run_recompute("body-pad3body-duplicate-stable-subname", "p6")

        diagnostic = next(
            item
            for item in result["diagnostics"]
            if item["code"] == "body_display_subname_not_feature_local"
        )
        self.assertEqual(diagnostic["object"], "Pad2")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Revolution")
        self.assertEqual(diagnostic["subname"], "Face9")
        self.assertIn("RevolutionBody.Revolution.Face9", diagnostic["message"])
        self.assertIn("Revolution.Shape has no local Face9", diagnostic["message"])

        revolution_faces = sorted(
            key
            for key in result["subshapes"]["Revolution"]
            if key.startswith("Face")
        )
        self.assertEqual(revolution_faces, ["Face1", "Face2", "Face3", "Face4"])
        self.assertNotIn("Face9", result["subshapes"]["Revolution"])

        self.assertEqual(result["objects"]["Pad2"]["status"], "error")
        self.assertEqual(result["objects"]["Fillet"]["status"], "skipped")
        self.assertEqual(result["objects"]["Pad3"]["status"], "skipped")
        self.assertEqual(result["objects"]["Pad3Body"]["status"], "skipped")
        self.assertNotIn("Pad2", result["mesh"])
        self.assertNotIn("Pad2", result["subshapes"])
        self.assertNotIn("Pad3Body", result["mesh"])
        self.assertNotIn("Pad3Body", result["subshapes"])

    def test_p6_body_face_profile_prefers_body_topo_shape_over_direct_feature_face(self) -> None:
        result = self.run_recompute("body-tip-face-profile-pad-after-sketch-axis-revolution", "p6")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["PadPreview"]["status"], "ok")
        self.assertEqual(result["objects"]["PadPreviewBody"]["status"], "ok")
        self.assertGreater(len(result["mesh"]["PadPreviewBody"]["triangles"]), 0)
        self.assertGreater(len(result["subshapes"]["PadPreviewBody"]), 0)

    def test_p6_same_body_fillet_resolves_target_local_stable_edge_on_body_replay(self) -> None:
        payload = self.p6_payload("body-dressup-fillet-target-local-stable-edge")
        result = self.run_response_payload(payload)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["object"] for item in result["results"]], ["Pad2Body"])
        selected_point = (-206.14529418945312, 382.5390319824219, 848.0122110459921)
        stable_distance = self.nearest_edge_distance(result["results"][0], selected_point)
        self.assertAlmostEqual(stable_distance, 20.0, delta=0.1)

        payload_with_qualified_stable = json.loads(json.dumps(payload))
        for item in payload_with_qualified_stable["Objects"]:
            if item["Name"] == "FilletPreview":
                item["Properties"]["Base"]["StableSubList"] = ["Pad2.Edge7"]

        result_with_qualified_stable = self.run_response_payload(payload_with_qualified_stable)

        self.assertEqual(result_with_qualified_stable["diagnostics"], [])
        qualified_stable_distance = self.nearest_edge_distance(
            result_with_qualified_stable["results"][0],
            selected_point,
        )
        self.assertAlmostEqual(qualified_stable_distance, 20.0, delta=0.1)

        payload_without_stable = json.loads(json.dumps(payload))
        for item in payload_without_stable["Objects"]:
            if item["Name"] == "FilletPreview":
                del item["Properties"]["Base"]["StableSubList"]

        result_without_stable = self.run_response_payload(payload_without_stable)

        self.assertEqual(result_without_stable["diagnostics"], [])
        no_stable_distance = self.nearest_edge_distance(
            result_without_stable["results"][0],
            selected_point,
        )
        self.assertAlmostEqual(no_stable_distance, 20.0, delta=0.1)
        self.assertAlmostEqual(stable_distance, no_stable_distance, delta=0.1)
        self.assertAlmostEqual(stable_distance, qualified_stable_distance, delta=0.1)

    def test_p6_same_body_fillet_accepts_revolution_body_tip_edge_reference(self) -> None:
        payload = self.p6_payload("body-revolution-filletpreview-tip-edge")
        body_only_payload = json.loads(json.dumps(payload))
        body_only_payload["Objects"] = [
            item for item in body_only_payload["Objects"] if item["Name"] != "FilletPreview"
        ]
        for item in body_only_payload["Objects"]:
            if item["Name"] == "RevolutionBody":
                item["Properties"]["Group"]["values"] = [
                    value
                    for value in item["Properties"]["Group"]["values"]
                    if value != "FilletPreview"
                ]
                item["Properties"]["Tip"]["value"] = "Revolution"

        body_only_result = self.run_response_payload(body_only_payload)
        self.assert_duplicate_stable_diagnostics(
            body_only_result,
            "RevolutionBody",
            {
                "Revolution.Edge2": {"Edge1", "Edge2"},
                "Revolution.Edge5": {"Edge5", "Edge6"},
            },
        )

        result = self.run_response_payload(payload)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["object"] for item in result["results"]], ["RevolutionBody"])
        self.assertIsNotNone(next(item for item in result["results"] if item["object"] == "RevolutionBody")["mesh"])

    def test_p6_same_body_fillet_ignores_stable_candidate_that_moves_current_edge(self) -> None:
        payload = self.p6_payload("body-revolution-filletpreview-stable-collision")
        body_only_payload = json.loads(json.dumps(payload))
        body_only_payload["Objects"] = [
            item for item in body_only_payload["Objects"] if item["Name"] != "FilletPreview"
        ]
        for item in body_only_payload["Objects"]:
            if item["Name"] == "RevolutionBody":
                item["Properties"]["Group"]["values"] = [
                    value
                    for value in item["Properties"]["Group"]["values"]
                    if value != "FilletPreview"
                ]
                item["Properties"]["Tip"]["value"] = "Revolution"

        body_only_result = self.run_response_payload(body_only_payload)
        self.assert_duplicate_stable_diagnostics(
            body_only_result,
            "RevolutionBody",
            {
                "Revolution.Edge3": {"Edge3", "Edge4"},
                "Revolution.Edge5": {"Edge19", "Edge5"},
            },
        )

        result = self.run_response_payload(payload)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual([item["object"] for item in result["results"]], ["RevolutionBody"])
        self.assertIsNotNone(next(item for item in result["results"] if item["object"] == "RevolutionBody")["mesh"])

    def test_p6_revolution_body_duplicate_face_stable_is_structured_diagnostic(self) -> None:
        payload = self.p6_payload("revolution-pad2-stable-sublist-pollution")
        payload["Objects"] = payload["Objects"][:4] + [
            {
                "Name": "RevolutionBody",
                "ID": 8,
                "TypeId": "PartDesign::Body",
                "Properties": {
                    "Group": {
                        "PropertyType": "App::PropertyLinkList",
                        "values": [
                            "草图 12:28:39 PM",
                            "Pad",
                            "Fillet",
                            "Revolution",
                        ],
                    },
                    "Tip": {
                        "PropertyType": "App::PropertyLink",
                        "value": "Revolution",
                    },
                },
            }
        ]
        payload["recompute"] = {"objs": ["RevolutionBody"]}

        result = self.run_response_payload(payload)

        self.assert_duplicate_stable_diagnostics(
            result,
            "RevolutionBody",
            {"Revolution.Vertex1": {"Vertex1", "Vertex2"}},
        )

    def test_p6_profile_resolver_rejects_revolution_full_path_pick_without_local_face(self) -> None:
        payload = self.p6_payload("revolution-pad2-stable-sublist-pollution")
        for item in payload["Objects"]:
            if item["Name"] == "Pad2":
                item["Properties"]["Profile"]["SubSet"][0]["StableSubList"] = []

        result = self.run_response_payload(payload)

        diagnostic = next(
            item
            for item in result["diagnostics"]
            if item["code"] == "body_display_subname_not_feature_local"
        )
        self.assertEqual(diagnostic["object"], "Pad2")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Revolution")
        self.assertEqual(diagnostic["subname"], "Face12")
        self.assertIn("RevolutionBody.Revolution.Face12", diagnostic["message"])
        self.assertEqual([item["object"] for item in result["results"]], ["Pad2Body"])
        self.assertIsNone(result["results"][0]["mesh"])
        self.assertEqual(result["results"][0]["subshapes"], [])

    def test_p6_body_result_publishes_revolution_tip_face_path(self) -> None:
        payload = self.p6_payload("body-tip-face-profile-pad-after-revolution")
        payload["Objects"] = payload["Objects"][:3] + [
            {
                "Name": "RevolutionBody",
                "ID": 6,
                "TypeId": "PartDesign::Body",
                "Properties": {
                    "Group": {
                        "PropertyType": "App::PropertyLinkList",
                        "values": ["SketchSource", "Pad", "Revolution"],
                    },
                    "Tip": {
                        "PropertyType": "App::PropertyLink",
                        "value": "Revolution",
                    },
                },
            }
        ]
        payload["recompute"] = {"objs": ["RevolutionBody"]}

        result = self.run_response_payload(payload)

        self.assertEqual(result["diagnostics"], [])
        face4 = next(item for item in result["results"][0]["subshapes"] if item["indexed"] == "Face4")
        self.assertEqual(face4["id"], "RevolutionBody:Face4")
        self.assertIn(face4["subname"], {"Face4", "Revolution.Face4"})
        if face4["subname"] == "Revolution.Face4":
            self.assertTrue(face4["stableSubname"].startswith("Revolution."))

    def test_p6_body_face_profile_does_not_replay_across_bodies(self) -> None:
        payload = self.p6_payload("body-tip-face-profile-pad-after-revolution")
        payload["Objects"].append(
            {
                "Name": "RevolutionBody",
                "ID": 6,
                "TypeId": "PartDesign::Body",
                "Properties": {
                    "Group": {
                        "PropertyType": "App::PropertyLinkList",
                        "values": ["SketchSource", "Pad", "Revolution"],
                    },
                    "Tip": {
                        "PropertyType": "App::PropertyLink",
                        "value": "Revolution",
                    },
                },
            }
        )
        payload["Objects"][4]["Properties"]["Group"]["values"] = ["PadPreview"]

        result = self.run_payload(payload)

        diagnostic = result["diagnostics"][0]
        self.assertEqual(diagnostic["code"], "invalid_subshape")
        self.assertEqual(diagnostic["object"], "PadPreview")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Revolution")
        self.assertEqual(diagnostic["subname"], "Face4")

    def test_p6_body_face_profile_rejects_forward_group_reference(self) -> None:
        payload = self.p6_payload("body-tip-face-profile-pad-after-revolution")
        payload["Objects"][4]["Properties"]["Group"]["values"] = [
            "SketchSource",
            "Pad",
            "PadPreview",
            "Revolution",
        ]

        result = self.run_payload(payload)

        diagnostic = result["diagnostics"][0]
        self.assertEqual(diagnostic["code"], "invalid_body_profile_reference")
        self.assertEqual(diagnostic["object"], "PadPreview")
        self.assertEqual(diagnostic["property"], "Profile")
        self.assertEqual(diagnostic["target"], "Revolution")

    def test_c4m4_topo_reference_pressure_updated_rows_publish_reference_updates(self) -> None:
        result = self.assert_c4m4_update(
            "topo-reference-pressure-multi-producer-updated",
            "Pad.Face6",
            "Face5",
            object_name="ProbePad",
            property_name="UpToFace",
        )
        self.assertEqual(result["objects"]["ProbePad"]["status"], "ok")
        self.assertIn("history_consumed:merge", result["named_shapes"]["Body"]["element_history_status"])

        result = self.c4m4_result("topo-reference-pressure-rename-label-updated")
        self.assertEqual(result["diagnostics"], [])
        update = result["elementReferenceUpdates"][0]
        self.assertEqual(update["SubList"], ["$PrettyBox.Face1"])
        self.assertEqual(
            update["labelReferenceRename"][0],
            {
                "index": 0,
                "oldLabel": "OldPrettyBox",
                "newLabel": "PrettyBox",
                "oldSubname": "$OldPrettyBox.Face1",
                "newSubname": "$PrettyBox.Face1",
                "method": "PropertyLinkBase.updateLabelReference",
            },
        )

        result = self.assert_c4m4_update(
            "topo-reference-pressure-link-retag-updated",
            "Body.SketchPocket.Edge1",
            "Edge12",
            object_name="ProbeSketch",
            property_name="ExternalGeometry",
        )
        self.assertIn(
            "Body.SketchPocket.Edge1",
            result["named_shapes"]["BodyLink"]["element_map"],
        )

        result = self.assert_c4m4_update(
            "topo-reference-pressure-shapefix-refine-updated",
            "Sketch.Face1",
            "Face5",
            object_name="ProbeSketch",
            property_name="ExternalGeometry",
        )
        self.assertEqual(result["objects"]["ProbeSketch"]["external_geometry_count"], 1)
        self.assertIn("history_consumed:merge", result["named_shapes"]["Pad"]["element_history_status"])

        result = self.assert_c4m4_update(
            "topo-reference-pressure-dressup-transformed-updated",
            "Mirrored.Transform1.Face1",
            "Face5",
            object_name="ProbeSketch",
            property_name="ExternalGeometry",
        )
        mirrored_history = result["named_shapes"]["Mirrored"]["element_history_status"]
        self.assertIn("terminal_history:split_deleted", mirrored_history)
        self.assertIn("subname_split_requires_reselect", mirrored_history)

    def test_c4m4_topo_reference_pressure_copy_on_change_asserts_both_update_channels(self) -> None:
        result = self.assert_c4m4_update(
            "topo-reference-pressure-copy-on-change-owned-child",
            "OwnedCopyBox.Face1",
            "Face1",
            object_name="ProbeSketch",
            property_name="ExternalGeometry",
        )

        self.assertEqual(
            [(update["action"], update["reason"], update["object"]) for update in result["documentObjectUpdates"]],
            [("update", "show_element_element_list_child_sync", "ArrayLink_i0")],
        )
        self.assertEqual(result["objects"]["ArrayLink_i0"]["linked_object"], "OwnedCopyBox")
        self.assertIn("OwnedCopyBox.Face1", result["named_shapes"]["ArrayLink_i0"]["element_map"])

    def test_c4m4_topo_reference_pressure_unchanged_import_keeps_update_channel_quiet(self) -> None:
        result = self.c4m4_result("topo-reference-pressure-import-unchanged")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        named_shape = result["named_shapes"]["ImportedStep"]
        self.assertIn("import_shape_element_map", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["ImportedStep.Face1"], "Face1")

    def test_c4m4_topo_reference_pressure_needs_reselect_and_diagnostic_rows_are_locatable(self) -> None:
        self.assert_c4m4_diagnostic(
            "topo-reference-pressure-rename-label-ambiguous-diagnostic",
            "label_reference_ambiguous",
            "BoxLink",
            "LinkedObject",
            "Box",
            "$OldPrettyBox.Face1",
        )

        result = self.assert_c4m4_diagnostic(
            "topo-reference-pressure-import-change-deleted",
            "deleted_stable_subname",
            "ProbePad",
            "UpToFace",
            "ImportedStep",
            "ImportedStep.Face999",
        )
        self.assertIn("import_shape_element_map", result["named_shapes"]["ImportedStep"]["element_history_status"])

        self.assert_c4m4_diagnostic(
            "topo-reference-pressure-import-change-ambiguous-diagnostic",
            "subname_resolve_ambiguous",
            "ProbePad",
            "UpToFace",
            "ImportedStep",
            "OldImportFace",
        )

        result = self.assert_c4m4_diagnostic(
            "topo-reference-pressure-shapefix-refine-deleted",
            "deleted_stable_subname",
            "ProbePad",
            "UpToFace",
            "Body",
            "Pocket.Face5",
        )
        self.assertTrue(
            any(
                item["kind"] == "deleted" and item["element"] == "Pocket.Face5"
                for item in result["named_shapes"]["Body"]["history"]
            )
        )

        result = self.assert_c4m4_diagnostic(
            "topo-reference-pressure-boolean-split-needs-reselect",
            "split_stable_subname",
            "ProbePad",
            "UpToFace",
            "BooleanFragments",
            "BoxA.Face3",
        )
        named_shape = result["named_shapes"]["BooleanFragments"]
        self.assertIn("subname_split_requires_reselect", named_shape["element_history_status"])
        self.assertTrue(
            any(
                item["kind"] == "split" and "BoxA.Face3" in item["sources"]
                for item in named_shape["history"]
            )
        )

    def test_c3m1_shapefix_delete_small_edge_records_deleted_mapper_history(self) -> None:
        result = self.run_c3m1_probe("shapefix-delete-small-edge")
        named_shape = result["named_shapes"]["ShapeFix"]
        mapper_history = named_shape["mapper_history"]
        result_edges = [
            element
            for element in named_shape["elements"].values()
            if element["kind"] == "edge"
        ]

        self.assertLess(len(result_edges), 5)
        deleted_events = [
            event
            for event in mapper_history
            if event["relation"] == "deleted"
            and event["maker_stage"] == "terminal_history"
            and event["source"]["object"] == "Source"
            and event["source"]["subname"].startswith("Edge")
        ]

        self.assertGreater(len(deleted_events), 0)
        self.assertTrue(
            all(event["target"] == {"object": "ShapeFix", "subname": ""} for event in deleted_events)
        )
        self.assertTrue(all(event["recoverability"] == "deleted" for event in deleted_events))
        self.assertTrue(
            all(event["diagnostic_status"] == "deleted_stable_subname" for event in deleted_events)
        )
        deleted_source_keys = {
            f'{event["source"]["object"]}.{event["source"]["subname"]}'
            for event in deleted_events
        }
        self.assertFalse(
            any(key in named_shape["element_map"] for key in deleted_source_keys)
        )

    def test_c3m1_shapefix_wireframe_records_modified_mapper_history(self) -> None:
        result = self.run_c3m1_probe("shapefix-modify-face-wire")
        named_shape = result["named_shapes"]["ShapeFix"]
        mapper_history = named_shape["mapper_history"]

        self.assertIn("shapefix_root_history:modified", named_shape["element_history_status"])
        self.assertNotIn("shapefix_root_history:generated", named_shape["element_history_status"])
        self.assertTrue(any(key.startswith("Source.Edge") for key in named_shape["element_map"]))

        modified_edge_events = [
            event
            for event in mapper_history
            if event["relation"] == "modified"
            and event["maker_stage"] == "maker_history"
            and event["shape_kind"] == "edge"
            and event["source"]["object"] == "Source"
        ]
        generated_events = [
            event
            for event in mapper_history
            if event["relation"] == "generated" and event["maker_stage"] == "maker_history"
        ]

        self.assertGreater(len(modified_edge_events), 0)
        self.assertEqual(generated_events, [])
        self.assertTrue(all(event["recoverability"] == "resolved" for event in modified_edge_events))
        self.assertTrue(all(event["diagnostic_status"] == "" for event in modified_edge_events))
        self.assertTrue(
            all(event["target"]["object"] == "ShapeFix" for event in modified_edge_events)
        )
        self.assertTrue(
            all(event["target"]["subname"].startswith("Edge") for event in modified_edge_events)
        )

    def test_c3m1_element_map_policy_drop_records_dropped_history_without_stale_alias(self) -> None:
        result = self.run_c3m1_probe("element-map-policy-drop")
        named_shape = result["named_shapes"]["DropResult"]

        self.assertIn("element_map_policy:drop", named_shape["element_history_status"])
        self.assertFalse(any(key.startswith("Source.") for key in named_shape["element_map"]))
        self.assertEqual(
            {key for key, value in named_shape["element_map"].items() if key == value},
            set(named_shape["element_map"]),
        )

        drop_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "element_map_policy_drop"
        ]
        self.assertGreater(len(drop_events), 0)
        self.assertTrue(all(event["source"]["object"] == "Source" for event in drop_events))
        self.assertTrue(all(event["target"] == {"object": "DropResult", "subname": ""} for event in drop_events))
        self.assertTrue(all(event["recoverability"] == "diagnostic" for event in drop_events))
        self.assertTrue(all(event["diagnostic_status"] == "element_map_policy_drop" for event in drop_events))

    def test_c3m1_element_map_policy_propagate_wire_preserves_makewire_edges(self) -> None:
        result = self.run_c3m1_probe("element-map-propagate-wire")
        named_shape = result["named_shapes"]["Wire"]

        self.assertIn(
            "element_map_policy_propagate:make_element_wires",
            named_shape["element_history_status"],
        )
        self.assertEqual(named_shape["element_map"]["EdgeA.Edge1"], "Edge1")
        self.assertEqual(named_shape["element_map"]["EdgeB.Edge1"], "Edge2")
        self.assertIn(named_shape["element_map"]["EdgeA.Vertex2"], {"Vertex1", "Vertex2", "Vertex3"})
        self.assertIn(named_shape["element_map"]["EdgeB.Vertex1"], {"Vertex1", "Vertex2", "Vertex3"})
        self.assertEqual(
            named_shape["element_map"]["EdgeA.Vertex2"],
            named_shape["element_map"]["EdgeB.Vertex1"],
        )

    def test_c3m1_element_map_policy_propagate_shell_preserves_source_faces(self) -> None:
        result = self.run_c3m1_probe("element-map-propagate-shell")
        named_shape = result["named_shapes"]["Shell"]

        self.assertIn(
            "element_map_policy_propagate:make_element_shell",
            named_shape["element_history_status"],
        )
        self.assertEqual(named_shape["element_map"]["FaceCompound.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["FaceCompound.Face6"], "Face6")
        self.assertTrue(any(key.startswith("FaceCompound.Edge") for key in named_shape["element_map"]))
        self.assertTrue(any(key.startswith("FaceCompound.Vertex") for key in named_shape["element_map"]))

    def test_c3m1_mapper_history_ambiguous_split_requires_reselect(self) -> None:
        result = self.run_c3m1_probe("mapper-history-ambiguous-split")
        named_shape = result["named_shapes"]["Split"]

        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertIn("subname_split_requires_reselect", named_shape["element_history_status"])
        self.assertNotIn("Source.Edge1", named_shape["element_map"])

        split_events = [
            event
            for event in named_shape["mapper_history"]
            if event["relation"] == "split"
            and event["source"] == {"object": "Source", "subname": "Edge1"}
        ]
        self.assertEqual(
            {event["target"]["subname"] for event in split_events},
            {"Edge1", "Edge2"},
        )
        self.assertTrue(all(event["recoverability"] == "needs_reselect" for event in split_events))
        self.assertTrue(
            all(event["diagnostic_status"] == "split_stable_subname" for event in split_events)
        )

        generated_events = [
            event
            for event in named_shape["mapper_history"]
            if event["relation"] == "generated"
            and event["source"] == {"object": "Source", "subname": "Edge1"}
        ]
        modified_events = [
            event
            for event in named_shape["mapper_history"]
            if event["relation"] == "modified"
            and event["source"] == {"object": "Source", "subname": "Edge1"}
        ]
        self.assertGreaterEqual(len(generated_events), 1)
        self.assertGreaterEqual(len(modified_events), 2)

    def test_c3m1_make_element_solid_from_shell_records_maker_history(self) -> None:
        result = self.run_c3m1_probe("make-element-solid-from-shell")
        named_shape = result["named_shapes"]["Solid"]

        self.assertIn("part_make_solid:make_element_solid", named_shape["element_history_status"])
        self.assertTrue(any(key.startswith("SourceShell.Face") for key in named_shape["element_map"]))
        self.assertTrue(
            any(
                event["maker_stage"] == "element_map_preserved"
                and event["source"]["object"] == "SourceShell"
                for event in named_shape["mapper_history"]
            )
        )

    def test_c3m1_element_map_child_map_recurses_nested_compound_ranges(self) -> None:
        result = self.run_recompute("element-map-child-map-recursive-compound", "c3m1")
        compound_ab = result["named_shapes"]["CompoundAB"]
        compound_nested = result["named_shapes"]["CompoundNested"]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn(
            "element_map_child_map:preserve_source_ranges",
            compound_ab["element_history_status"],
        )
        self.assertIn(
            "element_map_child_map:preserve_source_ranges",
            compound_nested["element_history_status"],
        )
        self.assertIn(
            "element_map_child_map:recursive_source_ranges",
            compound_nested["element_history_status"],
        )
        self.assertEqual(compound_nested["element_map"]["SketchA.Edge1"], "Edge1")
        self.assertEqual(compound_nested["element_map"]["SketchB.Edge1"], "Edge3")
        self.assertEqual(compound_nested["element_map"]["SketchC.Edge1"], "Edge5")

        edge_child_maps = [
            item for item in compound_nested["child_element_maps"] if item["kind"] == "edge"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["target_start"],
                    item["target_end"],
                    item["source_child_map_count"],
                )
                for item in edge_child_maps
            ],
            [
                ("CompoundAB", 0, 4, "Edge1", "Edge4", 4),
                ("SketchA", 0, 2, "Edge1", "Edge2", 0),
                ("SketchB", 2, 2, "Edge3", "Edge4", 0),
                ("SketchC", 4, 1, "Edge5", "Edge5", 0),
            ],
        )

        vertex_child_maps = [
            item for item in compound_nested["child_element_maps"] if item["kind"] == "vertex"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["target_start"],
                    item["target_end"],
                    item["source_child_map_count"],
                )
                for item in vertex_child_maps
            ],
            [
                ("CompoundAB", 0, 6, "Vertex1", "Vertex6", 4),
                ("SketchA", 0, 3, "Vertex1", "Vertex3", 0),
                ("SketchB", 3, 3, "Vertex4", "Vertex6", 0),
                ("SketchC", 6, 2, "Vertex7", "Vertex8", 0),
            ],
        )

    def test_c3m1_element_map_child_map_preserves_and_composes_postfix(self) -> None:
        result = self.run_c3m1_probe("element-map-child-map-postfix-compound")
        compound_ab = result["named_shapes"]["CompoundAB"]
        compound_nested = result["named_shapes"]["CompoundNested"]

        self.assertIn(
            "element_map_child_map:postfix_source_ranges",
            compound_ab["element_history_status"],
        )
        self.assertIn(
            "element_map_child_map:postfix_source_ranges",
            compound_nested["element_history_status"],
        )

        compound_ab_edge_maps = [
            item for item in compound_ab["child_element_maps"] if item["kind"] == "edge"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["postfix"],
                )
                for item in compound_ab_edge_maps
            ],
            [
                ("SketchA", 0, 2, ";:SOURCE"),
                ("SketchB", 2, 2, ";:SOURCE"),
            ],
        )

        compound_nested_edge_maps = [
            item for item in compound_nested["child_element_maps"] if item["kind"] == "edge"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["postfix"],
                )
                for item in compound_nested_edge_maps
            ],
            [
                ("CompoundAB", 0, 4, ";:PARENT"),
                ("SketchA", 0, 2, ";:SOURCE;:PARENT"),
                ("SketchB", 2, 2, ";:SOURCE;:PARENT"),
                ("SketchC", 4, 1, ";:PARENT"),
            ],
        )

    def test_c3m1_element_map_child_map_records_hash_keys(self) -> None:
        result = self.run_c3m1_probe("element-map-child-map-hash-key-compound")
        compound_ab = result["named_shapes"]["CompoundAB"]
        compound_nested = result["named_shapes"]["CompoundNested"]

        self.assertIn(
            "element_map_child_map:hashed_child_map_keys",
            compound_ab["element_history_status"],
        )
        self.assertIn(
            "element_map_child_map:hashed_child_map_keys",
            compound_nested["element_history_status"],
        )

        compound_ab_edge_maps = [
            item for item in compound_ab["child_element_maps"] if item["kind"] == "edge"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["postfix"],
                    item["encoded_child_map_key"].startswith(";:R"),
                )
                for item in compound_ab_edge_maps
            ],
            [
                ("SketchA", 0, 6, ";:SOURCE", True),
                ("SketchB", 6, 6, ";:SOURCE", True),
            ],
        )

        compound_nested_edge_maps = [
            item for item in compound_nested["child_element_maps"] if item["kind"] == "edge"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["offset"],
                    item["count"],
                    item["postfix"],
                    item["encoded_child_map_key"].startswith(";:R"),
                )
                for item in compound_nested_edge_maps
            ],
            [
                ("CompoundAB", 0, 12, ";:PARENT", True),
                ("SketchA", 0, 6, ";:SOURCE;:PARENT", True),
                ("SketchB", 6, 6, ";:SOURCE;:PARENT", True),
            ],
        )
        encoded_keys = [item["encoded_child_map_key"] for item in compound_nested_edge_maps]
        self.assertEqual(len(encoded_keys), len(set(encoded_keys)))

    def test_c3m1_import_step_records_face_stable_element_map(self) -> None:
        result = self.run_recompute("part-import-step-face-stable", "c3m1")
        named_shape = result["named_shapes"]["ImportedStep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("import_shape_element_map", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["ImportedStep.Face1"], "Face1")
        self.assertIn("ImportedStep.Face1", named_shape["elements"]["Face1"]["sources"])

        import_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "import_shape_element_map"
            and event["target"] == {"object": "ImportedStep", "subname": "Face1"}
        ]
        self.assertGreater(len(import_events), 0)
        self.assertTrue(all(event["relation"] == "preserved" for event in import_events))
        self.assertTrue(all(event["recoverability"] == "resolved" for event in import_events))
        self.assertTrue(all(event["evidence"]["format"] == "step" for event in import_events))

    def test_c3m1_import_brep_records_edge_stable_element_map(self) -> None:
        result = self.run_recompute("part-import-brep-edge-stable", "c3m1")
        named_shape = result["named_shapes"]["ImportedCylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("import_shape_element_map", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["ImportedCylinder.Edge1"], "Edge1")
        self.assertIn("ImportedCylinder.Edge1", named_shape["elements"]["Edge1"]["sources"])

        import_events = [
            event
            for event in named_shape["mapper_history"]
            if event["maker_stage"] == "import_shape_element_map"
            and event["target"] == {"object": "ImportedCylinder", "subname": "Edge1"}
        ]
        self.assertGreater(len(import_events), 0)
        self.assertTrue(all(event["relation"] == "preserved" for event in import_events))
        self.assertTrue(all(event["recoverability"] == "resolved" for event in import_events))
        self.assertTrue(all(event["evidence"]["format"] == "brep" for event in import_events))

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
                self.assertEqual(owner_object["topo_naming_history"], "maker_history:taper_thru_sections")
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
            (
                event
                for event in mapper_history
                if event["relation"] == "preserved"
                and event["source"] == {"object": "Pad", "subname": "Edge1"}
                and event["target"] == {"object": "Body", "subname": "Edge1"}
            ),
            None,
        )
        if preserved is not None:
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

    def test_c4m3_stable_subname_loss_is_locatable_diagnostic(self) -> None:
        diagnostic = self.run_recompute(
            "sketch-external-internal-stable-deleted-diagnostic",
            "c4m3",
        )["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "deleted_stable_subname")
        self.assertEqual(diagnostic["object"], "ProbeSketch")
        self.assertEqual(diagnostic["property"], "ExternalGeometry")
        self.assertEqual(diagnostic["target"], "Body")
        self.assertEqual(diagnostic["subname"], "Pocket.Edge11")

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

    def test_c3m2_source_object_rename_recovery_rewrites_link_target(self) -> None:
        result = self.run_recompute("source-object-rename-recovery", "c3m2")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["ProbePad"]["status"], "ok")
        update = result["elementReferenceUpdates"][0]
        shadow = update["ReferenceShadow"][0]

        self.assertEqual(update["object"], "ProbePad")
        self.assertEqual(update["property"], "UpToFace")
        self.assertEqual(update["value"], "RenamedBody")
        self.assertEqual(update["SubList"], ["Face5"])
        self.assertEqual(update["StableSubList"], ["Pad.Face6"])
        self.assertEqual(update["sourceObjectRename"], {
            "oldName": "Body",
            "newName": "RenamedBody",
            "method": "ReferenceShadow.targetId",
        })
        self.assertEqual(shadow["target"], "RenamedBody")
        self.assertEqual(shadow["reference_recovery"], "source_object_rename")

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

    def test_p6_complex_reference_recovery_oracles_match_freecad_expected(self) -> None:
        for fixture in [
            "sketch-external-edge-stable-chamfer-refine",
            "sketch-external-edge-stable-two-sides-taper",
        ]:
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("p6", fixture)
                result = self.run_recompute(fixture, "p6")
                sketch = result["objects"][expected["object"]]
                flag_counts = expected["sketch_external"]["flag_counts"]

                self.assertEqual(result["diagnostics"], [])
                self.assert_result_matches_expected(result, "p6", fixture)
                self.assertEqual(
                    sketch["external_geometry_count"],
                    expected["sketch_external"]["construction_count"],
                )
                self.assertEqual(sketch["external_geometry_state_counts"]["missing"], flag_counts["Missing"])
                self.assertEqual(sketch["external_geometry_state_counts"]["detached"], flag_counts["Detached"])
                self.assertEqual(sketch["external_geometry_state_counts"]["frozen"], flag_counts["Frozen"])
                self.assertEqual(sketch["external_geometry_state_counts"]["sync"], flag_counts["Sync"])
                self.assertEqual(sketch["external_geometry_state_counts"]["defining"], flag_counts["Defining"])
