"""Focused fail-closed checks for the A5 runtime-entrypoint process contract."""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
PARITY = TOOLS / "freecad_expected_parity"
REPORT = PARITY / "reports" / "process_contract" / "runtime-entrypoints.v1.json"
MANIFEST = PARITY / "process_contracts" / "runtime_entrypoints" / "manifest.v1.json"
sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import retained_coverage


class RuntimeEntrypointProcessContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.receipt = json.loads(REPORT.read_text(encoding="utf-8"))

    def validate(self, receipt: dict, required: list[str]) -> list[str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = root / "runtime-entrypoints.v1.json"
            path.write_text(
                json.dumps(receipt, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            contract_path = root / "capabilities.json"
            contract_path.write_text("{}\n", encoding="utf-8")
            _receipt, _path, errors = (
                retained_coverage._native_process_contract_receipt(
                    contract_path,
                    "process-contract/runtime-entrypoints",
                    process_contract_root=root,
                    required_case_ids=required,
                )
            )
        return errors

    def test_checked_in_repeat2_receipt_and_manifest_cover_required_branches(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual("freecad-native-process-contract/v1", self.receipt["schema"])
        self.assertEqual("passed", self.receipt["status"])
        self.assertEqual(2, self.receipt["repeat"])
        self.assertEqual(
            {case["id"] for case in manifest["cases"]},
            {case["id"] for case in self.receipt["cases"]},
        )
        required = [
            "maincmd-unknown-option",
            "base-console-observer",
            "mainpy-import-identity-shutdown",
        ]
        self.assertEqual([], self.validate(self.receipt, required))

    def test_normal_shutdown_has_direct_log_evidence(self) -> None:
        case = next(
            case
            for case in self.receipt["cases"]
            if case["id"] == "mainpy-import-identity-shutdown"
        )
        for run in case["runs"]:
            self.assertEqual(0, run["process"]["exitCode"])
            self.assertIn("Log: FreeCAD terminating...", run["normalizedLog"])
            self.assertIn("Log: Saving user parameter...done", run["normalizedLog"])
            self.assertTrue(run["result"]["actual"]["sameObject"])

    def test_receipt_fails_closed_when_required_fields_are_removed(self) -> None:
        mutations = {
            "schema": lambda receipt: receipt.pop("schema"),
            "repeat": lambda receipt: receipt.pop("repeat"),
            "producer": lambda receipt: receipt["producer"].pop("sha256"),
            "case": lambda receipt: receipt["cases"][0].pop("id"),
            "run": lambda receipt: receipt["cases"][0].pop("runs"),
            "argv": lambda receipt: receipt["cases"][0]["runs"][0]["process"].pop("argv"),
            "environment": lambda receipt: receipt["cases"][0]["runs"][0]["process"].pop("environment"),
            "exit": lambda receipt: receipt["cases"][0]["runs"][0]["process"].pop("exitCode"),
            "stdout": lambda receipt: receipt["cases"][0]["runs"][0]["process"].pop("stdout"),
            "stderr": lambda receipt: receipt["cases"][0]["runs"][0]["process"].pop("stderr"),
        }
        for field, mutate in mutations.items():
            with self.subTest(field=field):
                receipt = copy.deepcopy(self.receipt)
                mutate(receipt)
                self.assertTrue(
                    self.validate(receipt, ["maincmd-unknown-option"]),
                    f"missing {field} unexpectedly passed",
                )

    def test_source_backed_return100_probe_cannot_be_claimed_as_native_case(self) -> None:
        errors = self.validate(
            self.receipt,
            ["maincmd-init-base-exception-probe"],
        )
        self.assertTrue(any("not covered" in error for error in errors))

    def test_item_local_process_cases_are_mandatory(self) -> None:
        errors = self.validate(self.receipt, [])
        self.assertTrue(any("item-local processCases" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
