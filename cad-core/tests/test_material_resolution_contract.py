"""Focused checks for the A2 hermetic Material process-contract receipt."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import retained_coverage


class MaterialResolutionContractTests(unittest.TestCase):
    def test_checked_in_repeat2_receipt_is_fail_closed_and_complete(self) -> None:
        contract = (
            TOOLS / "freecad_expected_parity" / "retained_public_capabilities.v1.json"
        )
        receipt, receipt_path, errors = retained_coverage._native_process_contract_receipt(
            contract,
            "process-contract/material-resolution",
            process_contract_root=None,
        )

        self.assertEqual([], errors)
        self.assertTrue(receipt_path.is_file())
        assert receipt is not None
        self.assertEqual("passed", receipt["status"])
        self.assertEqual("passed", receipt["repeatStatus"])
        self.assertEqual(2, receipt["repeat"])
        self.assertEqual(8, receipt["caseCount"])
        self.assertEqual(
            {
                "local-card-model-resolution",
                "card-inheritance-and-child-override",
                "manager-refresh-second-resolution",
                "missing-card-lookup",
                "unknown-model-reference",
                "invalid-model-schema",
                "invalid-property-type",
                "inheritance-cycle-process-boundary",
            },
            {case["id"] for case in receipt["cases"]},
        )

    def test_missing_process_receipt_cannot_cover_the_api(self) -> None:
        contract = (
            TOOLS / "freecad_expected_parity" / "retained_public_api_surface.v1.json"
        )
        with tempfile.TemporaryDirectory() as temporary:
            receipt, _path, errors = retained_coverage._native_process_contract_receipt(
                contract,
                "process-contract/material-resolution",
                process_contract_root=Path(temporary),
            )

        self.assertIsNone(receipt)
        self.assertTrue(any("receipt is missing" in error for error in errors))

    def test_abnormal_cycle_is_structured_process_evidence(self) -> None:
        path = (
            TOOLS
            / "freecad_expected_parity"
            / "reports"
            / "process_contract"
            / "material-resolution.v1.json"
        )
        report = json.loads(path.read_text(encoding="utf-8"))
        cycle = next(
            case
            for case in report["cases"]
            if case["id"] == "inheritance-cycle-process-boundary"
        )

        self.assertTrue(all(run["status"] == "passed" for run in cycle["runs"]))
        for run in cycle["runs"]:
            process = run["process"]
            self.assertEqual("abnormal", run["result"]["actual"]["termination"])
            self.assertTrue(
                process["timedOut"]
                or process["signal"] is not None
                or process["exitCode"] not in (None, 0)
            )


if __name__ == "__main__":
    unittest.main()
