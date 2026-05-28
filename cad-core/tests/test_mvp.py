from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "cad-core"


class CadCoreOcctMvpTest(unittest.TestCase):
    def run_recompute(self, fixture: str) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / f"{fixture}.result.json"
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(ROOT / "fixtures" / "mvp" / f"{fixture}.json"),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
            )
            return json.loads(output.read_text(encoding="utf-8"))

    def diagnostic_codes(self, fixture: str) -> list[str]:
        return [item["code"] for item in self.run_recompute(fixture)["diagnostics"]]

    def test_fixture_diagnostics(self) -> None:
        expected = {
            "empty": [],
            "unknown-type": ["unsupported_type"],
            "duplicate-name": ["duplicate_object_name"],
            "duplicate-id": ["duplicate_object_id"],
            "legacy-lowercase": ["parse_error"],
            "missing-profile": ["missing_property"],
            "missing-link": ["missing_link_target"],
            "missing-target": ["missing_object"],
            "cycle-dependency": ["cycle_dependency"],
            "unsupported-geometry": ["unsupported_geometry"],
            "invalid-length": ["invalid_length"],
            "unsupported-property": ["unsupported_property"],
            "open-sketch": ["open_profile"],
            "rect-pad": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture), codes)

    def test_rect_pad_outputs_occt_mesh_and_subshape_map(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "rect-pad.result.json"
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(ROOT / "fixtures" / "mvp" / "rect-pad.json"),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
            )
            result = json.loads(output.read_text(encoding="utf-8"))
            pad = result["objects"]["Pad"]
            mesh = result["mesh"]["Pad"]
            subshape_map = result["subshapes"]["Pad"]

            self.assertEqual(result["diagnostics"], [])
            self.assertIn("OCCT", pad["kernel"])
            self.assertEqual(pad["bbox"]["min"], [0.0, 0.0, 0.0])
            self.assertEqual(pad["bbox"]["max"], [10.0, 5.0, 10.0])
            self.assertAlmostEqual(pad["volume"], 500.0)
            self.assertTrue(subshape_map)

            expected = json.loads((ROOT / "fixtures" / "mvp" / "expected" / "rect-pad.freecad.json").read_text())
            self.assertGreater(mesh["summary"]["vertex_count"], 0)
            self.assertGreater(mesh["summary"]["triangle_count"], 0)
            self.assertEqual(mesh["summary"]["triangle_count"], expected["mesh_summary"]["triangle_count"])
            self.assertEqual(
                sum(key.startswith("Face") for key in subshape_map),
                expected["topology_counts"]["faces"],
            )
            self.assertEqual(
                sum(key.startswith("Edge") for key in subshape_map),
                expected["topology_counts"]["edges"],
            )
            self.assertEqual(
                sum(key.startswith("Vertex") for key in subshape_map),
                expected["topology_counts"]["vertices"],
            )


if __name__ == "__main__":
    unittest.main()
