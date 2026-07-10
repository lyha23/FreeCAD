from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"


class FreecadExpectedReleaseGateEndToEndTest(unittest.TestCase):
    def run_compare_cli(
        self,
        temp_root: Path,
        binary: Path,
        *arguments: str,
        expected_returncode: int = 0,
    ) -> dict:
        completed = subprocess.run(
            [
                sys.executable,
                str(temp_root / "tools" / "compare_freecad_expected.py"),
                "--phase",
                "c4m6",
                "--bin",
                str(binary),
                *arguments,
            ],
            cwd=temp_root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(
            expected_returncode,
            completed.returncode,
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
        )
        return json.loads(completed.stdout)

    def test_real_binary_materializes_c4m6_phase_and_enforces_live_release_gate(self) -> None:
        """Exercise the documented c4m6 CLI gate without touching checked-in currents."""

        phase = "c4m6"
        binary = ROOT / "build" / "cad-core"

        self.assertTrue(binary.is_file(), f"missing CAD Core binary; build it first: {binary}")

        with tempfile.TemporaryDirectory(prefix="freecad-expected-release-gate-e2e-") as directory:
            temp_root = Path(directory)
            source_phase = ROOT / "fixtures" / phase
            temp_phase = temp_root / "fixtures" / phase
            temp_tools = temp_root / "tools"
            shutil.copytree(
                source_phase,
                temp_phase,
                ignore=shutil.ignore_patterns("cad-core-res", "cad-rs-res"),
            )
            temp_tools.mkdir()
            shutil.copy2(TOOLS / "compare_freecad_expected.py", temp_tools)
            shutil.copy2(TOOLS / "validate_freecad_expected_ledger.py", temp_tools)
            shutil.copytree(
                TOOLS / "freecad_expected_parity",
                temp_tools / "freecad_expected_parity",
                ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
            )

            current_dir = temp_phase / "cad-core-res"
            self.assertFalse(current_dir.exists())

            generation = self.run_compare_cli(temp_root, binary, "--write-current")

            self.assertEqual("ok", generation["status"], generation["preflight"]["errors"])
            generated_cases = generation["cases"]
            self.assertGreater(len(generated_cases), 0)
            self.assertEqual(
                {"cases": len(generated_cases), "written": len(generated_cases), "failed": 0},
                generation["summary"],
            )
            expected_current_names = {Path(item["current"]).name for item in generated_cases}
            self.assertEqual(
                expected_current_names,
                {path.name for path in current_dir.glob("*.cad-core.json")},
            )
            for current_path in current_dir.glob("*.cad-core.json"):
                self.assertIsInstance(json.loads(current_path.read_text(encoding="utf-8")), dict)

            release_summary = self.run_compare_cli(temp_root, binary, "--release-gate")
            release_path = temp_root / "out" / "freecad-expected-parity" / f"{phase}.json"
            release = json.loads(release_path.read_text(encoding="utf-8"))

            self.assertTrue(release["preflight"]["valid"], release["preflight"]["errors"])
            self.assertEqual(len(generated_cases), release["summary"]["cases"])
            self.assertEqual("green", release["semanticStatus"])
            self.assertIn(release["releaseStatus"], {"green", "protocol_divergence"})
            self.assertTrue(release["releaseGatePassed"])
            self.assertTrue(all(item["artifactEvidence"]["currentFresh"] for item in release["cases"]))
            self.assertEqual(release["releaseStatus"], release_summary["releaseStatus"])

            stale_path = current_dir / sorted(expected_current_names)[0]
            stale_path.write_text('{"stale": true}\n', encoding="utf-8")
            rejected = self.run_compare_cli(
                temp_root,
                binary,
                "--release-gate",
                expected_returncode=1,
            )
            self.assertEqual("invalid", rejected["releaseStatus"])
            self.assertFalse(rejected["releaseGatePassed"])


if __name__ == "__main__":
    unittest.main()
