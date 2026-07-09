from __future__ import annotations

import copy
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

try:
    from .fixture_runner import BIN, ROOT
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BIN, ROOT


C13M2_MAPPED_NAME_FOCUSED_CASES = (
    ("p2", "rect-pad-pocket", "Body"),
    ("c4m6", "topo-state-body-tip-stable-recovery", "Body"),
    ("p6", "up-to-face-stable-body-history", "ProbePad"),
)

C13M2_INDEXED_ONLY_BOUNDARY_CASES = (
    ("p5", "sketch-internal-face", "Sketch"),
    ("p8", "app-link-box-face", "BoxLink"),
)

C13M3_PRODUCER_EVIDENCE_CASES = (
    ("p2", "rect-pad-pocket", "Body"),
    ("c4m6", "topo-state-body-tip-stable-recovery", "Body"),
    ("p6", "up-to-face-stable-body-history", "Body"),
)

C4M6_EVIDENCE_PARITY_CASES = (
    ("topo-state-body-tip-stable-recovery", "Body"),
    ("topo-state-mapper-history-events", "HistoryProbe"),
    ("topo-state-reference-shadow-brep", "Body"),
)

C4M6_TOPO_STATE_PARITY_FIXTURES = (
    "topo-state-first-recompute-empty",
    "topo-state-body-tip-stable-recovery",
    "topo-state-link-compound-child-maps",
    "topo-state-mapper-history-events",
    "topo-state-reference-shadow-brep",
)

C4M6_EXPECTED_HARD_FAIL_FIXTURES = (
    ("topo-state-schema-incompatible", "topo_state_schema_incompatible"),
    ("topo-state-producer-incompatible", "topo_state_producer_incompatible"),
)


class TopoNamingStateResponseTest(unittest.TestCase):
    def fixture_payload(self, group: str, fixture: str) -> dict:
        path = ROOT / "fixtures" / group / f"{fixture}.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def expected_payload(self, group: str, fixture: str) -> dict:
        path = ROOT / "fixtures" / group / "expected" / f"{fixture}.freecad.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def run_official_recompute_payload(self, payload: bytes | dict) -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            input_path = tmp_path / "request.json"
            output_path = tmp_path / "result.json"
            input_path.write_bytes(payload)
            env = os.environ.copy()
            env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def run_official_recompute_fixture(self, group: str, fixture: str) -> dict:
        return self.run_official_recompute_payload(self.fixture_payload(group, fixture))

    def run_legacy_recompute_fixture(self, group: str, fixture: str) -> dict:
        payload = json.dumps(self.fixture_payload(group, fixture)).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            input_path = tmp_path / "request.json"
            output_path = tmp_path / "result.json"
            input_path.write_bytes(payload)
            env = os.environ.copy()
            env["CAD_CORE_TEST_LEGACY_OUTPUT"] = "1"
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def topo_state_object(self, response: dict, object_name: str) -> dict:
        return response["topoNamingState"]["objects"][object_name]

    def assert_topo_state_schema_gap_only(self, state: dict) -> None:
        self.assertEqual(state["schemaVersion"], "cad-core.topo-state.v1")
        self.assertIsInstance(state["producer"], dict)
        self.assertIsInstance(state["documentHash"], str)
        self.assertNotEqual(state["documentHash"], "")
        self.assertIsInstance(state["objects"], dict)

    def assert_body_element_map_schema_gap_only(self, state: dict) -> None:
        self.assertIn("Body", state["objects"])
        body = state["objects"]["Body"]
        self.assertIsInstance(body["objectHash"], str)
        self.assertNotEqual(body["objectHash"], "")
        self.assertEqual(body["elementMapVersion"], "cad-core.element-map.v1")
        self.assertIsInstance(body["subshapes"], dict)
        self.assertIsInstance(body["childElementMaps"], list)
        self.assertIsInstance(body["mapperHistory"], list)

        element_map = body["elementMap"]
        self.assertEqual(element_map["encoding"], "cad-core.element-map.v1")
        self.assertIn(element_map["status"], {"indexed_only", "history_partial"})
        self.assertIsInstance(element_map["entries"], dict)

        for entry in element_map["entries"].values():
            mapped_name = entry.get("mappedName")
            if mapped_name is None:
                continue
            self.assertIsInstance(mapped_name.get("raw"), str)
            self.assertIsInstance(mapped_name.get("canonical"), str)

    def assert_mapped_names_match_expected(
        self,
        actual_object: dict,
        expected_object: dict,
    ) -> None:
        actual_element_map = actual_object["elementMap"]
        expected_element_map = expected_object["elementMap"]
        self.assertEqual(actual_element_map["encoding"], expected_element_map["encoding"])
        self.assertEqual(actual_element_map["status"], expected_element_map["status"])

        actual_entries = actual_element_map["entries"]
        expected_entries = expected_element_map["entries"]
        self.assertEqual(set(actual_entries), set(expected_entries))

        for entry_key, expected_entry in expected_entries.items():
            with self.subTest(entry=entry_key):
                actual_mapped_name = actual_entries[entry_key]["mappedName"]
                expected_mapped_name = expected_entry["mappedName"]
                self.assertEqual(actual_mapped_name["raw"], expected_mapped_name["raw"])
                self.assertEqual(
                    actual_mapped_name["canonical"],
                    expected_mapped_name["canonical"],
                )

    def assert_entry_evidence_matches_expected(
        self,
        actual_object: dict,
        expected_object: dict,
    ) -> None:
        actual_entries = actual_object["elementMap"]["entries"]
        expected_entries = expected_object["elementMap"]["entries"]
        self.assertEqual(set(actual_entries), set(expected_entries))

        for entry_key, expected_entry in expected_entries.items():
            with self.subTest(entry=entry_key):
                actual_evidence = actual_entries[entry_key]["evidence"]
                expected_evidence = expected_entry["evidence"]
                self.assertIn("childElementMapKey", actual_evidence)
                self.assertIn("mapperHistoryIds", actual_evidence)
                self.assertEqual(
                    actual_evidence["childElementMapKey"],
                    expected_evidence["childElementMapKey"],
                )
                self.assertEqual(
                    actual_evidence["mapperHistoryIds"],
                    expected_evidence["mapperHistoryIds"],
                )

    def assert_topo_state_entry_contract(self, entry_key: str, entry: dict) -> None:
        self.assertEqual(entry_key, entry["mappedName"]["canonical"])
        self.assertIsInstance(entry["mappedName"]["raw"], str)
        self.assertIsInstance(entry["mappedName"]["canonical"], str)
        self.assertEqual(entry["recoverability"], "resolved")
        evidence = entry["evidence"]
        self.assertIn("childElementMapKey", evidence)
        self.assertIn("mapperHistoryIds", evidence)
        self.assertNotIn("mapperHistoryIndexes", evidence)
        self.assertIsInstance(evidence["mapperHistoryIds"], list)

    def assert_topo_state_object_runtime_contract(self, object_state: dict) -> None:
        self.assertEqual(object_state["elementMapVersion"], "cad-core.element-map.v1")
        element_map = object_state["elementMap"]
        self.assertEqual(element_map["encoding"], "cad-core.element-map.v1")
        self.assertIn(element_map["status"], {"indexed_only", "history_partial"})
        self.assertIsInstance(element_map["entries"], dict)
        mapper_history_ids = {
            event["id"]
            for event in object_state["mapperHistory"]
            if isinstance(event, dict) and event.get("id")
        }
        for entry_key, entry in element_map["entries"].items():
            self.assert_topo_state_entry_contract(entry_key, entry)
            for event_id in entry["evidence"]["mapperHistoryIds"]:
                self.assertIn(event_id, mapper_history_ids)
        for child_map in object_state["childElementMaps"]:
            self.assertEqual(child_map["elementMap"]["encoding"], "cad-core.element-map.v1")
            self.assertEqual(child_map["elementMap"]["status"], "history_partial")
            for entry_key, entry in child_map["elementMap"]["entries"].items():
                self.assert_topo_state_entry_contract(entry_key, entry)
                self.assertEqual(entry["evidence"]["childElementMapKey"], child_map["key"])

    def assert_topo_state_hard_fail(
        self,
        response: dict,
        code: str,
    ) -> None:
        self.assertNotIn("topoNamingState", response)
        self.assertEqual(response["results"], [])
        self.assertEqual(response["elementReferenceUpdates"], [])
        self.assertEqual(len(response["diagnostics"]), 1)
        self.assertEqual(response["diagnostics"][0]["severity"], "error")
        self.assertEqual(response["diagnostics"][0]["code"], code)

    def assert_indexed_only_expected_boundary(
        self,
        actual_object: dict,
        expected_object: dict,
    ) -> None:
        expected_element_map = expected_object["elementMap"]
        actual_element_map = actual_object["elementMap"]
        self.assertEqual(expected_element_map["status"], "indexed_only")
        self.assertEqual(expected_element_map["entries"], {})

        self.assertEqual(actual_element_map["status"], "indexed_only")
        self.assertEqual(actual_element_map["entries"], {})

    def assert_source_backed_producer_evidence(self, response: dict, object_name: str) -> None:
        named_shape = response["named_shapes"][object_name]
        provenance = named_shape["mapped_name_provenance"]
        source_backed = {
            entry_key: entry
            for entry_key, entry in provenance.items()
            if entry["status"] == "source_backed"
        }

        self.assertGreater(len(source_backed), 0)
        for entry_key, entry in source_backed.items():
            with self.subTest(object=object_name, entry=entry_key):
                self.assertEqual(entry["entry_key"], entry_key)
                self.assertIn(entry_key, named_shape["element_map"])
                self.assertIn(entry["current_element"], named_shape["elements"])
                self.assertNotEqual(entry["source_element"], "")
                self.assertIn(entry["element_type"], {"Face", "Edge", "Vertex"})
                self.assertIsInstance(entry["producer_tag"], int)
                self.assertIsInstance(entry["master_tag"], int)
                self.assertIsInstance(entry["source_tag"], int)
                self.assertNotEqual(entry["raw_mapped_name"], "")
                self.assertNotEqual(entry["canonical_mapped_name"], "")
                self.assertIn(";:H", entry["raw_mapped_name"])
                self.assertIn(":H*", entry["canonical_mapped_name"])

    def test_c13m1_official_cli_response_publishes_body_topo_state_schema_gap_only(self) -> None:
        payload = self.fixture_payload("p2", "rect-pad-pocket")

        response = self.run_official_recompute_payload(payload)

        self.assertIn("topoNamingState", response)
        state = response["topoNamingState"]
        self.assert_topo_state_schema_gap_only(state)
        self.assert_body_element_map_schema_gap_only(state)

    def test_c13m1_response_topo_state_round_trips_without_body_tip_recovery_regression(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-body-tip-stable-recovery")

        first_response = self.run_official_recompute_payload(payload)
        self.assertIn("topoNamingState", first_response)

        round_trip_payload = copy.deepcopy(payload)
        round_trip_payload["topoNamingState"] = first_response["topoNamingState"]
        second_response = self.run_official_recompute_payload(round_trip_payload)
        body = next(item for item in second_response["results"] if item["object"] == "Body")
        edge_subshapes = [item for item in body["subshapes"] if item["kind"] == "Edge"]

        self.assertEqual(second_response["diagnostics"], [])
        self.assertGreater(len(edge_subshapes), 0)
        for edge in edge_subshapes:
            self.assertEqual(edge["identityStatus"], "stable")
            self.assertNotEqual(edge["stableSubname"], "")

    def test_c13m3_s3_partdesign_producer_evidence_exists_for_focused_paths(self) -> None:
        for group, fixture, object_name in C13M3_PRODUCER_EVIDENCE_CASES:
            with self.subTest(fixture=f"{group}/{fixture}", object=object_name):
                response = self.run_legacy_recompute_fixture(group, fixture)
                self.assert_source_backed_producer_evidence(response, object_name)

    # C13-M2 S3 redline: remove expectedFailure in S4 when runtime publishes
    # FreeCAD-compatible raw/canonical mapped names instead of stable tokens.
    @unittest.expectedFailure
    def test_c13m2_p2_body_mapped_name_raw_canonical_matches_freecad_expected(self) -> None:
        self.assert_c13m2_focused_mapped_name_case("p2", "rect-pad-pocket", "Body")

    def test_c13m2_c4m6_body_publishes_runtime_mapped_name_entries(self) -> None:
        response = self.run_official_recompute_fixture(
            "c4m6",
            "topo-state-body-tip-stable-recovery",
        )
        expected = self.expected_payload("c4m6", "topo-state-body-tip-stable-recovery")

        body = self.topo_state_object(response, "Body")
        expected_body = self.topo_state_object(expected, "Body")

        self.assertEqual(body["elementMap"]["status"], "history_partial")
        self.assertEqual(
            len(body["elementMap"]["entries"]),
            len(expected_body["elementMap"]["entries"]),
        )
        self.assertEqual(
            [child["key"] for child in body["childElementMaps"]],
            [child["key"] for child in expected_body["childElementMaps"]],
        )
        self.assert_topo_state_object_runtime_contract(body)

    # C13-M2 S3 redline: remove expectedFailure in S4 when runtime publishes
    # FreeCAD-compatible raw/canonical mapped names instead of stable tokens.
    @unittest.expectedFailure
    def test_c13m2_p6_probe_pad_mapped_name_raw_canonical_matches_freecad_expected(self) -> None:
        self.assert_c13m2_focused_mapped_name_case(
            "p6",
            "up-to-face-stable-body-history",
            "ProbePad",
        )

    # C13-M3 S4 partial close: indexed-only objects must not publish
    # display/stable-token entries as fake raw mapped names.
    def test_c13m2_p5_sketch_indexed_only_boundary_does_not_publish_fake_raw(self) -> None:
        self.assert_c13m2_indexed_only_boundary_case("p5", "sketch-internal-face", "Sketch")

    # C13-M3 S4 partial close: indexed-only objects must not publish
    # display/stable-token entries as fake raw mapped names.
    def test_c13m2_p8_app_link_indexed_only_boundary_does_not_publish_fake_raw(self) -> None:
        self.assert_c13m2_indexed_only_boundary_case("p8", "app-link-box-face", "BoxLink")

    def test_c4m6_success_response_topo_state_runtime_contract(self) -> None:
        for fixture in C4M6_TOPO_STATE_PARITY_FIXTURES:
            with self.subTest(fixture=fixture):
                response = self.run_official_recompute_fixture("c4m6", fixture)

                self.assertIn("topoNamingState", response)
                self.assert_topo_state_schema_gap_only(response["topoNamingState"])
                for object_state in response["topoNamingState"]["objects"].values():
                    self.assert_topo_state_object_runtime_contract(object_state)

    def test_c4m6_success_response_aligns_with_expected_runtime_shape(self) -> None:
        body = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-body-tip-stable-recovery"),
            "Body",
        )
        body_expected = self.topo_state_object(
            self.expected_payload("c4m6", "topo-state-body-tip-stable-recovery"),
            "Body",
        )
        self.assertEqual(len(body["elementMap"]["entries"]), len(body_expected["elementMap"]["entries"]))
        self.assertEqual(len(body["childElementMaps"]), 1)
        self.assertEqual(body["childElementMaps"][0]["key"], "Body:Pad:Pad")

        compound = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-link-compound-child-maps"),
            "Compound",
        )
        compound_expected = self.topo_state_object(
            self.expected_payload("c4m6", "topo-state-link-compound-child-maps"),
            "Compound",
        )
        self.assertEqual(compound["elementMap"]["entries"], {})
        self.assertEqual(
            [child["key"] for child in compound["childElementMaps"]],
            [child["key"] for child in compound_expected["childElementMaps"]],
        )
        self.assertEqual(
            [len(child["elementMap"]["entries"]) for child in compound["childElementMaps"]],
            [len(child["elementMap"]["entries"]) for child in compound_expected["childElementMaps"]],
        )

        history_probe = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-mapper-history-events"),
            "HistoryProbe",
        )
        expected_history_probe = self.topo_state_object(
            self.expected_payload("c4m6", "topo-state-mapper-history-events"),
            "HistoryProbe",
        )
        self.assertEqual(
            [event.get("id") for event in history_probe["mapperHistory"] if event.get("id")],
            [event.get("id") for event in expected_history_probe["mapperHistory"] if event.get("id")],
        )

        reference_response = self.run_official_recompute_fixture(
            "c4m6",
            "topo-state-reference-shadow-brep",
        )
        self.assertEqual(reference_response["diagnostics"], [])
        self.assertEqual(len(reference_response["elementReferenceUpdates"]), 1)
        reference_item = reference_response["elementReferenceUpdates"][0]["SubSet"][0]
        self.assertEqual(reference_item["StableSubList"], ["Pad.#d:4;:G;XTR;:H*:*,F"])
        self.assertEqual(reference_item["ShadowSub"][0]["newName"], "Pad.#d:4;:G;XTR;:H*:*,F")
        self.assertEqual(
            reference_item["ReferenceShadow"][0]["stableSubname"],
            "Pad.#d:4;:G;XTR;:H*:*,F",
        )

    def test_c4m6_expected_schema_and_producer_failures_do_not_publish_topo_state(self) -> None:
        for fixture, code in C4M6_EXPECTED_HARD_FAIL_FIXTURES:
            with self.subTest(fixture=fixture):
                response = self.run_official_recompute_fixture("c4m6", fixture)

                self.assert_topo_state_hard_fail(response, code)

    def test_c4m6_document_hash_mismatch_hard_fails_without_topo_state(self) -> None:
        response = self.run_official_recompute_fixture("c4m6", "topo-state-document-hash-mismatch")

        self.assert_topo_state_hard_fail(response, "topo_state_document_hash_mismatch")

    def test_c4m6_object_hash_mismatch_hard_fails_without_topo_state(self) -> None:
        response = self.run_official_recompute_fixture("c4m6", "topo-state-object-hash-mismatch")

        self.assert_topo_state_hard_fail(response, "topo_state_object_hash_mismatch")

    def test_c4m6_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-first-recompute-empty")
        payload["topoNamingState"]["objects"] = {
            "Box": {
                "objectHash": self.expected_payload(
                    "c4m6", "topo-state-first-recompute-empty"
                )["topoNamingState"]["objects"]["Box"]["objectHash"],
                "elementMapVersion": "cad-core.element-map.v1",
                "subshapes": {},
                "elementMap": {
                    "encoding": "cad-core.element-map.v0",
                    "status": "indexed_only",
                    "entries": {},
                },
                "childElementMaps": [],
                "mapperHistory": [],
            }
        }

        response = self.run_official_recompute_payload(payload)

        self.assert_topo_state_hard_fail(
            response,
            "topo_state_element_map_encoding_incompatible",
        )

    def test_c4m6_child_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-body-tip-stable-recovery")
        first_response = self.run_official_recompute_payload(payload)
        payload["topoNamingState"] = first_response["topoNamingState"]
        body = payload["topoNamingState"]["objects"]["Body"]
        body["childElementMaps"][0]["elementMap"]["encoding"] = "cad-core.element-map.v0"

        response = self.run_official_recompute_payload(payload)

        self.assert_topo_state_hard_fail(
            response,
            "topo_state_element_map_encoding_incompatible",
        )

    def assert_c13m2_focused_mapped_name_case(
        self,
        group: str,
        fixture: str,
        object_name: str,
    ) -> None:
        response = self.run_official_recompute_fixture(group, fixture)
        expected = self.expected_payload(group, fixture)

        self.assert_mapped_names_match_expected(
            self.topo_state_object(response, object_name),
            self.topo_state_object(expected, object_name),
        )

    def assert_c13m2_indexed_only_boundary_case(
        self,
        group: str,
        fixture: str,
        object_name: str,
    ) -> None:
        response = self.run_official_recompute_fixture(group, fixture)
        expected = self.expected_payload(group, fixture)

        self.assert_indexed_only_expected_boundary(
            self.topo_state_object(response, object_name),
            self.topo_state_object(expected, object_name),
        )

    def test_c4m6_child_map_and_mapper_history_evidence_runtime_contract(self) -> None:
        body = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-body-tip-stable-recovery"),
            "Body",
        )
        self.assertEqual(body["childElementMaps"][0]["key"], "Body:Pad:Pad")
        self.assertEqual(
            len(body["childElementMaps"][0]["elementMap"]["entries"]),
            len(body["elementMap"]["entries"]),
        )
        for entry in body["childElementMaps"][0]["elementMap"]["entries"].values():
            self.assertEqual(entry["evidence"]["source"], "freecad_partdesign_body_tip")

        compound = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-link-compound-child-maps"),
            "Compound",
        )
        self.assertEqual(
            [child["key"] for child in compound["childElementMaps"]],
            ["Compound:ChildBoxA:Child0", "Compound:ChildBoxB:Child1"],
        )
        for child in compound["childElementMaps"]:
            self.assertEqual(len(child["elementMap"]["entries"]), 26)
            for entry in child["elementMap"]["entries"].values():
                self.assertEqual(entry["evidence"]["source"], "freecad_part_compound_links")

        history_probe = self.topo_state_object(
            self.run_official_recompute_fixture("c4m6", "topo-state-mapper-history-events"),
            "HistoryProbe",
        )
        expected_history_probe = self.topo_state_object(
            self.expected_payload("c4m6", "topo-state-mapper-history-events"),
            "HistoryProbe",
        )
        self.assertEqual(
            {
                entry["evidence"]["mapperHistoryIds"][0]
                for entry in history_probe["elementMap"]["entries"].values()
                if entry["evidence"]["mapperHistoryIds"]
            },
            {
                entry["evidence"]["mapperHistoryIds"][0]
                for entry in expected_history_probe["elementMap"]["entries"].values()
            },
        )


if __name__ == "__main__":
    unittest.main()
