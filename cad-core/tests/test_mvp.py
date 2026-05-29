from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "cad-core"


class CadCoreOcctMvpTest(unittest.TestCase):
    def run_recompute(self, fixture: str, group: str = "mvp") -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / f"{fixture}.result.json"
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(ROOT / "fixtures" / group / f"{fixture}.json"),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
            )
            return json.loads(output.read_text(encoding="utf-8"))

    def diagnostic_codes(self, fixture: str, group: str = "mvp") -> list[str]:
        return [item["code"] for item in self.run_recompute(fixture, group)["diagnostics"]]

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

    def test_p2_fixture_diagnostics(self) -> None:
        expected = {
            "body-basefeature-pad": [],
            "rect-pad-pocket": [],
            "missing-basefeature": ["missing_link_target"],
            "pocket-without-base": ["execution_failed"],
            "pocket-open-sketch": ["open_profile"],
            "unsupported-pocket-type": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p2"), codes)

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

    def test_rect_pad_pocket_outputs_cut_body(self) -> None:
        result = self.run_recompute("rect-pad-pocket", "p2")
        body = result["objects"]["Body"]
        mesh = result["mesh"]["Body"]
        subshape_map = result["subshapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(body["volume"], 320.0)
        self.assertGreater(mesh["summary"]["triangle_count"], 0)
        self.assertNotIn("shapes", result)

        expected = json.loads((ROOT / "fixtures" / "p2" / "expected" / "rect-pad-pocket.freecad.json").read_text())
        self.assertEqual(mesh["summary"]["bbox"], expected["bbox"])
        self.assertAlmostEqual(mesh["summary"]["volume"], expected["volume"])
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

    def test_body_basefeature_pad_uses_base_solid(self) -> None:
        result = self.run_recompute("body-basefeature-pad", "p2")
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 5.0])
        self.assertAlmostEqual(body["volume"], 250.0)


if __name__ == "__main__":
    unittest.main()
