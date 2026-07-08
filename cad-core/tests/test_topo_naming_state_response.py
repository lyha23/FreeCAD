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


class TopoNamingStateResponseTest(unittest.TestCase):
    def fixture_payload(self, group: str, fixture: str) -> dict:
        path = ROOT / "fixtures" / group / f"{fixture}.json"
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


if __name__ == "__main__":
    unittest.main()
