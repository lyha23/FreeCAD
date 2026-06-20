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
        edge_segments = mesh["edgeSegments"]
        expected_edges = {
            f"Edge{index}" for index in range(1, expected["topology_counts"]["edges"] + 1)
        }
        self.assertEqual({segment["id"] for segment in edge_segments}, expected_edges)
        self.assertEqual({segment["indexed"] for segment in edge_segments}, expected_edges)
        for segment in edge_segments:
            self.assertIn(segment["id"], subshape_map)
            self.assertEqual(subshape_map[segment["id"]]["kind"], "edge")
            self.assertGreaterEqual(len(segment["points"]), 2)


if __name__ == "__main__":
    unittest.main()
