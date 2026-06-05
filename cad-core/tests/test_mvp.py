from __future__ import annotations

import unittest

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreOcctMvpTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_rect_pad_outputs_occt_mesh_and_subshape_map(self) -> None:
        result = self.run_recompute("rect-pad", "mvp")
        expected = self.expected_freecad("mvp", "rect-pad")
        object_name = expected["object"]
        pad = result["objects"][object_name]
        mesh = result["mesh"][object_name]
        subshape_map = result["subshapes"][object_name]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("OCCT", pad["kernel"])
        self.assert_object_matches_expected(result, "mvp", "rect-pad")
        self.assertTrue(subshape_map)
        self.assertGreater(mesh["summary"]["vertex_count"], 0)
        self.assertGreater(mesh["summary"]["triangle_count"], 0)


if __name__ == "__main__":
    unittest.main()
