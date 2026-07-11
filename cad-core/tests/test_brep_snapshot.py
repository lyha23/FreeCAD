from __future__ import annotations

import subprocess
import unittest

try:
    from .fixture_runner import BUILD_DIR, ROOT
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BUILD_DIR, ROOT


class BrepSnapshotProbeTest(unittest.TestCase):
    def test_restores_valid_single_subshape_and_rejects_invalid_payload(self) -> None:
        result = subprocess.run(
            [str(BUILD_DIR / "cad-core-brep-snapshot-probe")],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
