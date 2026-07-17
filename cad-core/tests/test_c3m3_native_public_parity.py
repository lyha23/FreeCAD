from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PHASE_ROOT = ROOT / "fixtures" / "sketcher-solve"
CASE = "sketch-underconstrained-no-constraints"
BINARY = ROOT / "build" / "cad-core"


class C3M3NativePublicParityTest(unittest.TestCase):
    """Protect the sole C3M3 native oracle through the official CLI response."""

    expected: dict[str, Any]
    actual: dict[str, Any]

    @classmethod
    def setUpClass(cls) -> None:
        super().setUpClass()
        input_path = PHASE_ROOT / f"{CASE}.json"
        expected_path = PHASE_ROOT / "expected" / f"{CASE}.freecad.json"
        ledger_path = PHASE_ROOT / "expected" / f"{CASE}.freecad.ledger.json"
        for path in (BINARY, input_path, expected_path, ledger_path):
            if not path.is_file():
                raise AssertionError(f"missing C3M3 native artifact: {path}")

        env = os.environ.copy()
        env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
        with tempfile.TemporaryDirectory(prefix="c3m3-native-public-parity-") as directory:
            output_path = Path(directory) / f"{CASE}.cad-core.json"
            completed = subprocess.run(
                [str(BINARY), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            if completed.returncode != 0 or not output_path.is_file():
                raise AssertionError(
                    "official CAD Core recompute failed for C3M3 native case: "
                    f"returncode={completed.returncode}\nstdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
                )
            cls.actual = json.loads(output_path.read_text(encoding="utf-8"))
        cls.expected = json.loads(expected_path.read_text(encoding="utf-8"))

    @staticmethod
    def result_for_object(payload: dict[str, Any], object_name: str) -> dict[str, Any]:
        results = payload.get("results")
        if not isinstance(results, list):
            raise AssertionError("public response results must be a list")
        for result in results:
            if isinstance(result, dict) and result.get("object") == object_name:
                return result
        raise AssertionError(f"public response has no result for {object_name}")

    def test_sketch_internal_counts_match_freecad_public_expected(self) -> None:
        self.assertNotIn("objects", self.actual, "legacy test output leaked into public parity")
        expected = self.result_for_object(self.expected, "Sketch")
        actual = self.result_for_object(self.actual, "Sketch")
        self.assertEqual(
            actual.get("sketch_internal", {}).get("internal_counts"),
            expected["sketch_internal"]["internal_counts"],
        )


if __name__ == "__main__":
    unittest.main()
