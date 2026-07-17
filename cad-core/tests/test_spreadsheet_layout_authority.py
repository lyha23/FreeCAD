"""Focused fail-closed and checked-in authority tests for Spreadsheet layout APIs."""

from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path


CAD_CORE = Path(__file__).resolve().parents[1]
COLLECTOR_PATH = CAD_CORE / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("spreadsheet_layout_collector", COLLECTOR_PATH)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)

PHASE = CAD_CORE / "fixtures" / "spreadsheet-layout"
ALL_CASES = {
    "spreadsheet-layout-style",
    "spreadsheet-layout-alignment",
    "spreadsheet-layout-row-column-size",
    "spreadsheet-layout-merge-clears-covered-cells",
    "spreadsheet-layout-recompute-update",
    "spreadsheet-layout-unmerge-recompute-update",
    "spreadsheet-layout-overlap-replaces-prior-merge",
    "spreadsheet-layout-invalid-range-native-diagnostic",
    "spreadsheet-layout-invalid-style-option-native-diagnostic",
}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SpreadsheetLayoutCollectorContractTests(unittest.TestCase):
    def setUp(self) -> None:
        collector.ACTIVE_SPREADSHEET_OPERATION_RECEIPTS = {}

    def test_native_diagnostic_records_actual_public_api_exception(self) -> None:
        class Sheet:
            Name = "Sheet"

            def mergeCells(self, cell_range: str) -> None:
                raise RuntimeError(f"invalid native range: {cell_range}")

        collector.apply_spreadsheet_operations(
            Sheet(),
            [{
                "op": "mergeCells",
                "range": "invalid",
                "expectedOutcome": "native_diagnostic",
            }],
            stage="initial",
        )

        receipt = collector.ACTIVE_SPREADSHEET_OPERATION_RECEIPTS["Sheet"][0]
        self.assertEqual("native_diagnostic", receipt["evidenceLevel"])
        self.assertEqual("RuntimeError", receipt["nativeDiagnostic"]["exceptionType"])
        self.assertEqual("invalid native range: invalid", receipt["nativeDiagnostic"]["message"])

    def test_unknown_operation_field_fails_closed(self) -> None:
        with self.assertRaisesRegex(collector.UnsupportedFixture, "unsupported fields"):
            collector.apply_spreadsheet_operations(
                object(),
                [{"op": "mergeCells", "range": "A1:B2", "caseShortcut": True}],
                stage="initial",
            )

    def test_expected_diagnostic_cannot_relabel_collector_schema_error(self) -> None:
        with self.assertRaisesRegex(collector.UnsupportedFixture, "range must be"):
            collector.apply_spreadsheet_operations(
                object(),
                [{"op": "mergeCells", "range": 3, "expectedOutcome": "native_diagnostic"}],
                stage="initial",
            )


class SpreadsheetLayoutCheckedInAuthorityTests(unittest.TestCase):
    def test_phase_is_the_minimum_complete_layout_batch(self) -> None:
        self.assertEqual(ALL_CASES, {path.stem for path in PHASE.glob("*.json")})

    def test_every_input_has_native_public_and_ledger_authority(self) -> None:
        roles = load_json(
            CAD_CORE
            / "tools"
            / "freecad_expected_parity"
            / "fixture_roles.v1.json"
        )["roles"]
        role_by_case = {
            (row["phase"], row["case"]): row["role"]
            for row in roles
        }
        for case in sorted(ALL_CASES):
            with self.subTest(case=case):
                expected = PHASE / "expected" / f"{case}.freecad.json"
                ledger = PHASE / "expected" / f"{case}.freecad.ledger.json"
                self.assertTrue(expected.is_file())
                self.assertTrue(ledger.is_file())
                self.assertEqual("native", role_by_case[("spreadsheet-layout", case)])
                self.assertEqual("accepted", load_json(ledger)["outcome"])

    def test_public_expected_covers_operations_mutation_and_diagnostics(self) -> None:
        receipts = []
        for case in sorted(ALL_CASES):
            payload = load_json(PHASE / "expected" / f"{case}.freecad.json")
            self.assertEqual("not_evaluated", payload.get("cadCoreRuntimeParity", "not_evaluated"))
            receipts.extend(payload["results"][0]["spreadsheet"]["layoutOperations"])

        operations = {row["operation"] for row in receipts}
        self.assertEqual(
            {"setStyle", "setAlignment", "setColumnWidth", "setRowHeight", "mergeCells", "splitCell"},
            operations,
        )
        self.assertTrue(any(row["stage"] == "recompute_mutation" for row in receipts))
        self.assertGreaterEqual(
            sum(row["evidenceLevel"] == "native_diagnostic" for row in receipts),
            2,
        )


if __name__ == "__main__":
    unittest.main()
