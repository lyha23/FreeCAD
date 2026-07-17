"""Focused contract tests for Sketcher operation native authority."""

from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path
from types import SimpleNamespace


CAD_CORE = Path(__file__).resolve().parents[1]
COLLECTOR_PATH = CAD_CORE / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("sketcher_operation_collector", COLLECTOR_PATH)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)

PHASE = CAD_CORE / "fixtures" / "sketcher-operations"

TRIM_EXTEND_CASES = {
    "sketch-trim-line-between-cutters",
    "sketch-trim-arc-between-cutters",
    "sketch-trim-conic-between-cutters",
    "sketch-trim-no-intersection-deletes-geometry",
    "sketch-trim-invalid-geometry-diagnostic",
    "sketch-extend-line-recompute-update",
    "sketch-extend-arc",
    "sketch-extend-conic-native-diagnostic",
    "sketch-extend-line-to-degenerate-boundary",
}
FILLET_CASES = {
    "sketch-fillet-trim-closed-profile",
    "sketch-fillet-create-corner",
    "sketch-fillet-chamfer",
    "sketch-fillet-without-trim",
    "sketch-fillet-radius-recompute-update",
    "sketch-fillet-invalid-geometry-pair",
}
CLONE_CONIC_BSPLINE_CASES = {
    "sketch-clone-line-dimensional-constraint",
    "sketch-block-after-copy-recompute-update",
    "sketch-clone-conic-arc",
    "sketch-conic-expose-internal-geometry",
    "sketch-conic-convert-to-bspline",
    "sketch-bspline-degree-knot-recompute-update",
    "sketch-bspline-knot-multiplicity",
    "sketch-bspline-invalid-knot-native-diagnostic",
}
ALL_CASES = TRIM_EXTEND_CASES | FILLET_CASES | CLONE_CONIC_BSPLINE_CASES
NATIVE_DIAGNOSTIC_CASES = {
    "sketch-trim-invalid-geometry-diagnostic",
    "sketch-extend-conic-native-diagnostic",
    "sketch-fillet-invalid-geometry-pair",
    "sketch-bspline-invalid-knot-native-diagnostic",
}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SketchOperationCollectorContractTests(unittest.TestCase):
    def setUp(self) -> None:
        collector.ACTIVE_SKETCH_OPERATION_RECEIPTS = {}

    def test_sketch_properties_apply_geometry_constraints_before_operations(self) -> None:
        properties = {
            "Operations": [{"op": "trim"}],
            "Label": "Sketch",
            "Constraints": [],
            "Geometry": [],
        }

        ordered = collector.ordered_native_property_items("Sketcher::SketchObject", properties)

        self.assertEqual(
            ["Geometry", "Constraints", "Label", "Operations"],
            [name for name, _ in ordered],
        )

    def test_operation_receipts_keep_target_result_and_actual_native_diagnostic(self) -> None:
        class Sketch:
            Name = "Sketch"

            def trim(self, geometry_id, point):
                if geometry_id == 99:
                    raise ValueError("native trim boundary")
                return geometry_id + int(point.x)

        freecad = SimpleNamespace(
            Vector=lambda x, y, z: SimpleNamespace(x=x, y=y, z=z)
        )
        actions = [
            {
                "op": "trim",
                "geometryId": 2,
                "point": [3, 0, 0],
                "expectedOutcome": "target_result",
            },
            {
                "op": "trim",
                "geometryId": 99,
                "point": [0, 0, 0],
                "expectedOutcome": "native_diagnostic",
            },
        ]

        collector.apply_sketch_operations(freecad, Sketch(), actions, stage="initial")

        receipts = collector.ACTIVE_SKETCH_OPERATION_RECEIPTS["Sketch"]
        self.assertEqual(["target_result", "native_diagnostic"], [r["evidenceLevel"] for r in receipts])
        self.assertEqual(5, receipts[0]["result"])
        self.assertEqual(
            "ValueError", receipts[1]["nativeDiagnostic"]["exceptionType"]
        )
        self.assertEqual(
            "native trim boundary", receipts[1]["nativeDiagnostic"]["message"]
        )

    def test_operation_schema_fails_closed_on_unknown_fields(self) -> None:
        action = {
            "op": "trim",
            "geometryId": 0,
            "point": [0, 0, 0],
            "fixtureSpecificShortcut": True,
        }
        with self.assertRaisesRegex(collector.UnsupportedFixture, "unsupported fields"):
            collector.apply_sketch_operations(
                SimpleNamespace(),
                SimpleNamespace(Name="Sketch"),
                [action],
                stage="initial",
            )

    def test_native_diagnostic_cannot_relabel_collector_argument_errors(self) -> None:
        action = {
            "op": "trim",
            "point": [0, 0, 0],
            "expectedOutcome": "native_diagnostic",
        }
        with self.assertRaisesRegex(collector.UnsupportedFixture, "geometryId must be an integer"):
            collector.apply_sketch_operations(
                SimpleNamespace(
                    Vector=lambda x, y, z: SimpleNamespace(x=x, y=y, z=z)
                ),
                SimpleNamespace(Name="Sketch", trim=lambda *_: None),
                [action],
                stage="initial",
            )
        self.assertEqual([], collector.ACTIVE_SKETCH_OPERATION_RECEIPTS["Sketch"])


class SketchOperationCheckedInAuthorityTests(unittest.TestCase):
    def test_phase_is_exactly_three_minimal_semantic_batches(self) -> None:
        inputs = {path.stem for path in PHASE.glob("*.json")}

        self.assertEqual(23, len(inputs))
        self.assertEqual(ALL_CASES, inputs)
        self.assertTrue(TRIM_EXTEND_CASES.isdisjoint(FILLET_CASES))
        self.assertTrue(TRIM_EXTEND_CASES.isdisjoint(CLONE_CONIC_BSPLINE_CASES))
        self.assertTrue(FILLET_CASES.isdisjoint(CLONE_CONIC_BSPLINE_CASES))

    def test_every_input_has_item_local_operation_outcome_contract(self) -> None:
        for case in sorted(ALL_CASES):
            with self.subTest(case=case):
                fixture = load_json(PHASE / f"{case}.json")
                operations = []
                for spec in fixture["Objects"]:
                    operations.extend(spec.get("Properties", {}).get("Operations", []))
                for mutation in fixture.get("recompute", {}).get("mutations", []):
                    operations.extend(
                        mutation.get("Properties", {}).get("Operations", [])
                    )
                self.assertTrue(operations)
                self.assertTrue(
                    all(
                        item.get("expectedOutcome", "target_result")
                        in {"target_result", "native_diagnostic"}
                        for item in operations
                    )
                )
                has_native_diagnostic = any(
                    item.get("expectedOutcome") == "native_diagnostic"
                    for item in operations
                )
                self.assertEqual(case in NATIVE_DIAGNOSTIC_CASES, has_native_diagnostic)

    def test_every_expected_projects_operations_geometry_constraints_solver_and_internal_shape(self) -> None:
        for case in sorted(ALL_CASES):
            with self.subTest(case=case):
                expected = load_json(PHASE / "expected" / f"{case}.freecad.json")
                self.assertEqual([], expected["diagnostics"])
                result = expected["results"][0]
                self.assertEqual("Sketch", result["object"])
                for field in (
                    "sketch_operations",
                    "sketch_geometry",
                    "sketch_constraints",
                    "sketch_solver",
                    "sketch_internal",
                ):
                    self.assertIn(field, result)
                levels = {
                    receipt["evidenceLevel"]
                    for receipt in result["sketch_operations"]
                }
                expected_level = (
                    "native_diagnostic"
                    if case in NATIVE_DIAGNOSTIC_CASES
                    else "target_result"
                )
                self.assertIn(expected_level, levels)
                self.assertGreaterEqual(result["sketch_solver"]["geometryCount"], 0)
                self.assertGreaterEqual(result["sketch_solver"]["constraintCount"], 0)

    def test_every_expected_has_same_run_accepted_ledger_closure(self) -> None:
        for case in sorted(ALL_CASES):
            with self.subTest(case=case):
                ledger = load_json(
                    PHASE / "expected" / f"{case}.freecad.ledger.json"
                )
                self.assertEqual(
                    "freecad-toponaming-ledger/v1", ledger["schema"]
                )
                self.assertEqual("accepted", ledger["outcome"])
                self.assertEqual("sketcher-operations", ledger["fixture"]["phase"])
                self.assertEqual(case, ledger["fixture"]["case"])
                self.assertEqual("passed", ledger["roundTrip"]["status"])
                self.assertEqual("FreeCADCmd", ledger["producer"]["name"])
                self.assertTrue(ledger["producer"]["freecadVersion"])
                self.assertTrue(ledger["producer"]["occtVersion"])


if __name__ == "__main__":
    unittest.main()
