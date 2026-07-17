from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any

try:
    from .fixture_runner import BIN, ROOT, semantic_fixture_path
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BIN, ROOT, semantic_fixture_path


LEGACY_PHASE = "c3m2"
EVIDENCE_FIELDS = {
    "authority",
    "close_condition",
    "freecad_native_parity",
    "next_action",
    "reason",
}


class C3M2ProtocolContractTest(unittest.TestCase):
    """Exercise manual C3M2 contracts against the live runtime, never an artifact snapshot."""

    @staticmethod
    def response_for(case: str) -> dict[str, Any]:
        input_path = semantic_fixture_path(case)
        with tempfile.TemporaryDirectory(prefix="c3m2-protocol-contract-") as directory:
            output_path = Path(directory) / "response.json"
            environment = os.environ.copy()
            environment.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            completed = subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            if completed.returncode != 0:
                raise AssertionError(
                    f"{case} exited {completed.returncode}\n"
                    f"stdout:\n{completed.stdout}\n"
                    f"stderr:\n{completed.stderr}"
                )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def assert_subset(self, expected: Any, actual: Any, path: str) -> None:
        if isinstance(expected, dict):
            self.assertIsInstance(actual, dict, path)
            assert isinstance(actual, dict)
            for key, value in expected.items():
                self.assertIn(key, actual, f"{path}.{key}")
                self.assert_subset(value, actual[key], f"{path}.{key}")
            return
        if isinstance(expected, list):
            self.assertIsInstance(actual, list, path)
            assert isinstance(actual, list)
            self.assertEqual(len(expected), len(actual), path)
            for index, value in enumerate(expected):
                self.assert_subset(value, actual[index], f"{path}[{index}]")
            return
        self.assertEqual(expected, actual, path)

    def assert_forbidden_keys_absent(self, value: Any, forbidden: set[str], path: str = "$") -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                self.assertNotIn(key, forbidden, f"{path}.{key}")
                self.assert_forbidden_keys_absent(child, forbidden, f"{path}.{key}")
        elif isinstance(value, list):
            for index, child in enumerate(value):
                self.assert_forbidden_keys_absent(child, forbidden, f"{path}[{index}]")

    def assert_response_contract(self, response: dict[str, Any], contract: dict[str, Any]) -> None:
        self.assert_subset(contract["diagnostics"], response.get("diagnostics"), "diagnostics")
        self.assertEqual(
            contract["resultObjects"],
            [item.get("object") for item in response.get("results", [])],
            "result object publication",
        )
        self.assert_subset(
            contract["documentObjectUpdates"],
            response.get("documentObjectUpdates", []),
            "documentObjectUpdates",
        )
        self.assert_subset(
            contract["elementReferenceUpdates"],
            response.get("elementReferenceUpdates", []),
            "elementReferenceUpdates",
        )

        state_contract = contract["topoNamingState"]
        state = response.get("topoNamingState")
        if not state_contract["present"]:
            self.assertIsNone(state, "hard-fail responses must not publish a new topoNamingState")
            return

        self.assertIsInstance(state, dict, "accepted response must publish topoNamingState")
        assert isinstance(state, dict)
        self.assertEqual(
            state_contract["objectNames"],
            sorted(state.get("objects", {})),
            "topoNamingState owners",
        )
        self.assert_forbidden_keys_absent(state, set(state_contract.get("forbiddenKeys", [])))

    def test_c3m2_protocol_contracts_match_live_runtime(self) -> None:
        roles_path = ROOT / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"
        roles = json.loads(roles_path.read_text(encoding="utf-8"))["roles"]
        migration = json.loads(
            (
                ROOT
                / "tools"
                / "freecad_expected_parity"
                / "fixture_legacy_phase_map.v1.json"
            ).read_text(encoding="utf-8")
        )["cases"]
        legacy_cases = {
            entry["case"] for entry in migration if entry["legacyPhase"] == LEGACY_PHASE
        }
        c3m2_roles = [entry for entry in roles if entry.get("case") in legacy_cases]

        self.assertEqual(12, len(c3m2_roles))
        protocol_roles = [entry for entry in c3m2_roles if entry.get("role") == "protocol_only"]
        native_roles = [entry for entry in c3m2_roles if entry.get("role") == "native"]
        self.assertEqual(11, len(protocol_roles))
        self.assertEqual(["source-object-rename-recovery"], [entry["case"] for entry in native_roles])
        self.assertFalse([entry for entry in c3m2_roles if entry.get("role") == "unsupported"])

        for entry in sorted(protocol_roles, key=lambda item: item["case"]):
            case = entry["case"]
            artifact_path = semantic_fixture_path(case).parent / "expected" / f"{case}.expeted.json"
            with self.subTest(case=case):
                artifact = json.loads(artifact_path.read_text(encoding="utf-8"))
                evidence = artifact["oracle_evidence"]
                self.assertEqual(EVIDENCE_FIELDS, set(evidence))
                self.assertFalse(evidence["freecad_native_parity"])
                self.assertTrue(
                    all(
                        isinstance(evidence[field], str) and evidence[field]
                        for field in EVIDENCE_FIELDS - {"freecad_native_parity"}
                    )
                )

                contract = artifact["contract"]
                self.assertIsInstance(contract.get("runs", 1), int)
                self.assertGreaterEqual(contract.get("runs", 1), 1)
                for attempt in range(contract.get("runs", 1)):
                    with self.subTest(case=case, attempt=attempt):
                        self.assert_response_contract(self.response_for(case), contract)

    def test_runtime_producer_stays_inside_the_strict_collector_boundary(self) -> None:
        # CAD Core publishes this producer on a first response with no incoming state.  The
        # collector/ledger mirror must recognize that exact runtime producer, while preserving
        # diagnostics-only rejection for every other producer token.
        from tools import collect_freecad_expected as collector
        from tools import validate_freecad_expected_ledger as ledger

        fixture: dict[str, Any] = {
            "Objects": [],
            "recompute": {"objs": []},
        }
        fixture["topoNamingState"] = {
            "schemaVersion": "cad-core.topo-state.v1",
            "producer": {"cadCoreVersion": "cad-core-runtime-v1"},
            "documentHash": collector.fixture_document_hash(fixture),
            "objects": {},
        }

        self.assertIsNone(collector.topo_state_request_error_response(fixture))
        self.assertIsNone(ledger.input_topo_state_rejection_code(fixture))

        fixture["topoNamingState"]["producer"] = {"cadCoreVersion": "foreign-producer"}
        collector_rejection = collector.topo_state_request_error_response(fixture)
        self.assertIsNotNone(collector_rejection)
        assert collector_rejection is not None
        self.assertEqual(
            "topo_state_producer_incompatible",
            collector_rejection["diagnostics"][0]["code"],
        )
        self.assertEqual("topo_state_producer_incompatible", ledger.input_topo_state_rejection_code(fixture))


if __name__ == "__main__":
    unittest.main()
