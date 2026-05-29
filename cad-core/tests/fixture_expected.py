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

    def assert_sketch_internal_matches_expected(
        self,
        result: dict,
        object_name: str,
        expected: dict,
    ) -> None:
        obj = result["objects"][object_name]
        subshape_map = result["subshapes"].get(object_name, {})
        internal_expected = expected["sketch_internal"]

        for key in ("shape", "profile", "profile_ready", "raw_edge_count"):
            if key in internal_expected:
                actual_key = "internal_shape" if key == "shape" else key
                self.assertEqual(obj[actual_key], internal_expected[key])

        counts = internal_expected.get("internal_counts", {})
        for key, actual_key in [
            ("faces", "internal_face_count"),
            ("edges", "internal_edge_count"),
            ("vertices", "internal_vertex_count"),
        ]:
            if key in counts:
                self.assertEqual(obj[actual_key], counts[key])

        public_counts = internal_expected.get("public_counts", {})
        for prefix, key in [("Face", "faces"), ("Edge", "edges"), ("Vertex", "vertices")]:
            if key in public_counts:
                self.assertEqual(sum(name.startswith(prefix) for name in subshape_map), public_counts[key])

        for subshape_name in internal_expected.get("contains", []):
            self.assertIn(subshape_name, subshape_map)

        if "element_map" in internal_expected:
            self.assertEqual(obj["internal_element_map"], internal_expected["element_map"])

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
        if "sketch_internal" in expected:
            self.assert_sketch_internal_matches_expected(result, object_name, expected)

    def assert_named_shape_matches_expected(
        self,
        result: dict,
        object_name: str,
        expected: dict,
    ) -> None:
        named_shape = result["named_shapes"][object_name]
        element_map = named_shape.get("element_map", {})
        elements = named_shape.get("elements", {})

        for key in ("owner", "element_map_status"):
            if key in expected:
                self.assertEqual(named_shape[key], expected[key])

        for source, element_name in expected.get("element_map", {}).items():
            self.assertIn(source, element_map)
            self.assertEqual(element_map[source], element_name)

        for source in expected.get("element_map_contains", []):
            self.assertIn(source, element_map)

        for source in expected.get("element_map_absent", []):
            self.assertNotIn(source, element_map)

        for prefix in expected.get("element_map_prefixes", []):
            self.assertTrue(any(source.startswith(prefix) for source in element_map))

        for element_name in expected.get("elements_include", []):
            self.assertIn(element_name, elements)

        if expected.get("elements_equal_subshapes"):
            self.assertEqual(set(elements), set(result["subshapes"][object_name]))

        for status in expected.get("element_statuses_include", []):
            self.assertTrue(any(element.get("status") == status for element in elements.values()))

        for element_name, element_expected in expected.get("elements", {}).items():
            self.assertIn(element_name, elements)
            for key, value in element_expected.items():
                self.assertEqual(elements[element_name][key], value)

        for source, kind in expected.get("element_kind_by_source", {}).items():
            self.assertIn(source, element_map)
            element_name = element_map[source]
            self.assertIn(element_name, elements)
            self.assertEqual(elements[element_name]["kind"], kind)

        for kind, required_sources in expected.get("history_sources", {}).items():
            actual_sources = {
                source
                for item in named_shape.get("history", [])
                if item["kind"] == kind
                for source in item.get("sources", [])
            }
            for source in required_sources:
                self.assertIn(source, actual_sources)

        history = named_shape.get("history", [])
        for kind in expected.get("history_kinds_include", []):
            self.assertTrue(any(item["kind"] == kind for item in history))

        all_history_sources = {
            source
            for item in history
            for source in item.get("sources", [])
        }
        for source in expected.get("history_sources_any", []):
            self.assertIn(source, all_history_sources)

        for prefix in expected.get("history_source_prefixes", []):
            self.assertTrue(any(source.startswith(prefix) for source in all_history_sources))

        for prefix in expected.get("history_non_indexed_source_prefixes", []):
            self.assertTrue(
                any(
                    source.startswith(prefix)
                    for item in history
                    if item["kind"] != "indexed"
                    for source in item.get("sources", [])
                )
            )

        for kind, element_names in expected.get("history_elements", {}).items():
            actual_elements = {
                item.get("element")
                for item in history
                if item["kind"] == kind
            }
            for element_name in element_names:
                self.assertIn(element_name, actual_elements)

        for entry_expected in expected.get("history_entries", []):
            element_name = entry_expected.get("element")
            element_source = entry_expected.get("element_for_source")
            if element_source is not None:
                self.assertIn(element_source, element_map)
                element_name = element_map[element_source]
            self.assertTrue(
                any(
                    item["kind"] == entry_expected["kind"]
                    and (element_name is None or item.get("element") == element_name)
                    and all(source in item.get("sources", []) for source in entry_expected.get("sources_include", []))
                    and item.get("sources") == entry_expected.get("sources", item.get("sources"))
                    for item in history
                )
            )

    def assert_result_matches_expected(self, result: dict, group: str, fixture: str) -> None:
        expected = self.expected_freecad(group, fixture)
        if "known_gap" in expected:
            self.skipTest(f"{group}/{fixture}: {expected['known_gap']}")
        if "objects" in expected:
            default_bbox_delta = expected.get("bbox_delta", 1e-6)
            for object_name, object_expected in expected["objects"].items():
                self.assert_expected_object(result, object_name, object_expected, default_bbox_delta)
        elif "object" in expected:
            self.assert_expected_object(result, expected["object"], expected)

        for object_name, named_shape_expected in expected.get("named_shapes", {}).items():
            self.assert_named_shape_matches_expected(result, object_name, named_shape_expected)

    def assert_object_matches_expected(self, result: dict, group: str, fixture: str) -> None:
        self.assert_result_matches_expected(result, group, fixture)
