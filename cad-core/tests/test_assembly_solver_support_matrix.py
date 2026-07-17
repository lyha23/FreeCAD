"""Focused fail-closed checks for the A4 Assembly/OndselSolver matrix."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
PARITY = TOOLS / "freecad_expected_parity"
sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import retained_coverage
from validate_assembly_solver_support_matrix import validate


class AssemblySolverSupportMatrixTests(unittest.TestCase):
    def test_live_source_denominator_and_authority_receipts_pass(self) -> None:
        report = validate(
            PARITY / "assembly_solver_support_matrix.v1.json",
            fixtures_root=CAD_CORE_ROOT / "fixtures",
            roles_path=PARITY / "fixture_roles.v1.json",
        )

        self.assertEqual("passed", report["status"])
        self.assertEqual(13, report["summary"]["jointTypeCount"])
        self.assertEqual(10, report["summary"]["markerGeometryFamilyCount"])
        self.assertEqual(37, report["summary"]["distanceTypeCount"])
        self.assertEqual(0, report["summary"]["missingEquivalenceClassCount"])
        self.assertEqual(0, report["summary"]["invalidEvidenceClassCount"])

    def test_missing_native_evidence_fails_closed(self) -> None:
        matrix = json.loads(
            (PARITY / "assembly_solver_support_matrix.v1.json").read_text(
                encoding="utf-8"
            )
        )
        evidence = next(
            item
            for item in matrix["evidenceClasses"]
            if item["id"] == "solver.failure.shared"
        )
        evidence["fixture"] = None
        with tempfile.TemporaryDirectory() as temp_dir:
            matrix_path = Path(temp_dir) / "matrix.json"
            matrix_path.write_text(
                json.dumps(matrix, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            report = validate(
                matrix_path,
                fixtures_root=CAD_CORE_ROOT / "fixtures",
                roles_path=PARITY / "fixture_roles.v1.json",
            )

        self.assertEqual("failed", report["status"])
        self.assertIn("solver.failure.shared", report["missingEquivalenceClasses"])

    def test_capability_receipt_rejects_a_stale_matrix(self) -> None:
        receipt = json.loads(
            (PARITY / "reports" / "assembly_solver_support_matrix.v1.json").read_text(
                encoding="utf-8"
            )
        )
        matrix_text = (PARITY / "assembly_solver_support_matrix.v1.json").read_text(
            encoding="utf-8"
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            contract_path = root / "capabilities.json"
            contract_path.write_text("{}\n", encoding="utf-8")
            (root / "assembly_solver_support_matrix.v1.json").write_text(
                matrix_text + "\n",
                encoding="utf-8",
            )
            report_path = root / "reports" / "assembly_solver_support_matrix.v1.json"
            report_path.parent.mkdir(parents=True)
            report_path.write_text(
                json.dumps(receipt, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            _, _, _, errors = retained_coverage._assembly_support_matrix_receipt(
                contract_path,
                retained_coverage.ASSEMBLY_SUPPORT_MATRIX_RECEIPT_ID,
            )

        self.assertTrue(any("stale" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
