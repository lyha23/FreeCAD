"""Focused tests for machine-readable strict ledger validation receipts."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


VALIDATOR = Path(__file__).resolve().parents[1] / "tools" / "validate_freecad_expected_ledger.py"
SPEC = importlib.util.spec_from_file_location("validate_freecad_expected_ledger_report", VALIDATOR)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class LedgerValidationReportTests(unittest.TestCase):
    def test_passed_validation_writes_case_hashes_and_split_counts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expected = root / "fixtures" / "p1" / "expected" / "case.freecad.json"
            ledger = validator.ledger_path_for_expected(expected)
            expected.parent.mkdir(parents=True)
            expected.write_text("{}\n", encoding="utf-8")
            ledger.write_text("{}\n", encoding="utf-8")
            report = root / "report.json"

            with (
                mock.patch.object(validator, "validate_expected_file", return_value=[]),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                result = validator.main(
                    [str(expected), "--strict", "--report", str(report)]
                )

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertTrue(payload["strict"])
            self.assertEqual(1, payload["selected"])
            self.assertEqual(0, payload["failed"])
            self.assertEqual(validator.file_sha256(expected), payload["cases"][0]["publicSha256"])
            self.assertEqual(validator.file_sha256(ledger), payload["cases"][0]["ledgerSha256"])
            self.assertIsNone(payload["firstFailure"])

    def test_zero_selection_writes_fail_closed_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            report = root / "report.json"

            with contextlib.redirect_stderr(io.StringIO()):
                result = validator.main(
                    [str(root / "missing" / "*.freecad.json"), "--strict", "--report", str(report)]
                )

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(2, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual("selection", payload["stage"])
            self.assertEqual(0, payload["selected"])


if __name__ == "__main__":
    unittest.main()
