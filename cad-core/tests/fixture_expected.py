from __future__ import annotations

import json
from pathlib import Path

from .fixture_runner import ROOT


def discover_expected_cases(root: Path = ROOT) -> list[tuple[str, str, Path]]:
    cases: list[tuple[str, str, Path]] = []
    fixtures_root = root / "fixtures"
    for expected_path in sorted(fixtures_root.glob("*/expected/*.freecad.json")):
        group = expected_path.parent.parent.name
        fixture = expected_path.name.removesuffix(".freecad.json")
        cases.append((group, fixture, expected_path))
    return cases


class ExpectedFixtureAssertions:
    def assert_topology_counts(self, subshape_map: dict, expected: dict) -> None:
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

    def assert_bbox_close(self, actual: dict, expected_min: list[float], expected_max: list[float]) -> None:
        for actual_value, expected_value in zip(actual["min"], expected_min):
            self.assertAlmostEqual(actual_value, expected_value, delta=1e-6)
        for actual_value, expected_value in zip(actual["max"], expected_max):
            self.assertAlmostEqual(actual_value, expected_value, delta=1e-6)

    def assert_bbox_close_delta(
        self,
        actual: dict,
        expected_min: list[float],
        expected_max: list[float],
        delta: float,
    ) -> None:
        for actual_value, expected_value in zip(actual["min"], expected_min):
            self.assertAlmostEqual(actual_value, expected_value, delta=delta)
        for actual_value, expected_value in zip(actual["max"], expected_max):
            self.assertAlmostEqual(actual_value, expected_value, delta=delta)

    def expected_freecad(self, group: str, fixture: str) -> dict:
        return json.loads((ROOT / "fixtures" / group / "expected" / f"{fixture}.freecad.json").read_text())

    def assert_expected_object(
        self,
        result: dict,
        object_name: str,
        expected: dict,
        default_bbox_delta: float = 1e-6,
    ) -> None:
        obj = result["objects"][object_name]
        bbox_delta = expected.get("bbox_delta", default_bbox_delta)

        if "bbox" in expected:
            self.assert_bbox_close_delta(obj["bbox"], expected["bbox"]["min"], expected["bbox"]["max"], bbox_delta)
        if "volume" in expected:
            volume_delta = expected.get("volume_delta")
            if volume_delta is None:
                self.assertAlmostEqual(obj["volume"], expected["volume"])
            else:
                self.assertAlmostEqual(obj["volume"], expected["volume"], delta=volume_delta)
        if "topology_counts" in expected:
            self.assert_topology_counts(result["subshapes"][object_name], expected)
        if "mesh_summary" in expected:
            summary = result["mesh"][object_name]["summary"]
            expected_summary = expected["mesh_summary"]
            if "bbox" in expected_summary:
                self.assert_bbox_close_delta(
                    summary["bbox"],
                    expected_summary["bbox"]["min"],
                    expected_summary["bbox"]["max"],
                    expected_summary.get("bbox_delta", bbox_delta),
                )
            if "volume" in expected_summary:
                self.assertAlmostEqual(summary["volume"], expected_summary["volume"])
            for key in ("vertex_count", "triangle_count"):
                if key in expected_summary:
                    self.assertEqual(summary[key], expected_summary[key])

    def assert_result_matches_expected(self, result: dict, group: str, fixture: str) -> None:
        expected = self.expected_freecad(group, fixture)
        if "objects" in expected:
            default_bbox_delta = expected.get("bbox_delta", 1e-6)
            for object_name, object_expected in expected["objects"].items():
                self.assert_expected_object(result, object_name, object_expected, default_bbox_delta)
            return

        self.assert_expected_object(result, expected["object"], expected)

    def assert_object_matches_expected(self, result: dict, group: str, fixture: str) -> None:
        self.assert_result_matches_expected(result, group, fixture)
