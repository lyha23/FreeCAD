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

    # C13-M2 S3 redline: remove expectedFailure in S4 when runtime publishes
    # FreeCAD-compatible raw/canonical mapped names instead of stable tokens.
    @unittest.expectedFailure
    def test_c13m2_p2_body_mapped_name_raw_canonical_matches_freecad_expected(self) -> None:
        self.assert_c13m2_focused_mapped_name_case("p2", "rect-pad-pocket", "Body")

    # C13-M2 S3 redline: remove expectedFailure in S4 when runtime publishes
    # FreeCAD-compatible raw/canonical mapped names instead of stable tokens.
    @unittest.expectedFailure
    def test_c13m2_c4m6_body_mapped_name_raw_canonical_matches_freecad_expected(self) -> None:
        self.assert_c13m2_focused_mapped_name_case(
            "c4m6",
            "topo-state-body-tip-stable-recovery",
            "Body",
        )

    # C13-M2 S3 redline: remove expectedFailure in S4 when runtime publishes
    # FreeCAD-compatible raw/canonical mapped names instead of stable tokens.
    @unittest.expectedFailure
    def test_c13m2_p6_probe_pad_mapped_name_raw_canonical_matches_freecad_expected(self) -> None:
        self.assert_c13m2_focused_mapped_name_case(
            "p6",
            "up-to-face-stable-body-history",
            "ProbePad",
        )

    # C13-M2 S3 redline: remove expectedFailure when indexed-only objects stop
    # publishing display/stable-token entries as fake raw mapped names.
    @unittest.expectedFailure
    def test_c13m2_p5_sketch_indexed_only_boundary_does_not_publish_fake_raw(self) -> None:
        self.assert_c13m2_indexed_only_boundary_case("p5", "sketch-internal-face", "Sketch")

    # C13-M2 S3 redline: remove expectedFailure when indexed-only objects stop
    # publishing display/stable-token entries as fake raw mapped names.
    @unittest.expectedFailure
    def test_c13m2_p8_app_link_indexed_only_boundary_does_not_publish_fake_raw(self) -> None:
        self.assert_c13m2_indexed_only_boundary_case("p8", "app-link-box-face", "BoxLink")

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

    def test_c13m2_focused_expected_has_no_non_empty_s5_key_id_evidence_yet(self) -> None:
        non_empty_evidence = []
        for group, fixture, object_name in (
            C13M2_MAPPED_NAME_FOCUSED_CASES + C13M2_INDEXED_ONLY_BOUNDARY_CASES
        ):
            expected = self.expected_payload(group, fixture)
            expected_entries = self.topo_state_object(expected, object_name)["elementMap"][
                "entries"
            ]
            for entry_key, entry in expected_entries.items():
                evidence = entry["evidence"]
                self.assertIn("childElementMapKey", evidence)
                self.assertIn("mapperHistoryIds", evidence)
                self.assertIsInstance(evidence["mapperHistoryIds"], list)
                if evidence["childElementMapKey"] is not None or evidence["mapperHistoryIds"]:
                    non_empty_evidence.append(
                        {
                            "fixture": f"{group}/{fixture}",
                            "object": object_name,
                            "entry": entry_key,
                            "childElementMapKey": evidence["childElementMapKey"],
                            "mapperHistoryIds": evidence["mapperHistoryIds"],
                        }
                    )

        self.assertEqual(
            non_empty_evidence,
            [],
            "C13-M2 S5 key/id evidence appeared; replace this guard with explicit "
            "childElementMapKey and mapperHistoryIds parity tests before marking it green.",
        )


if __name__ == "__main__":
    unittest.main()
