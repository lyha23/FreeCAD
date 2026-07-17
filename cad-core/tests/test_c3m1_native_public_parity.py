from __future__ import annotations

import json
import math
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "cad-core"

# Keep this aligned with the established public parity contract in
# tools/freecad_expected_parity/engine.py::FLOAT_TOLERANCE.  This is not an output-side
# tolerance or a fixture exception: native/CAD Core OCCT bounds retain the contract's fixed
# numeric representation precision while topology and identity assertions below stay exact.
# This test deliberately does not import the comparator or its projector.
SEMANTIC_NUMERIC_TOLERANCE = 1e-6
FREECAD_PRODUCER_TAG = re.compile(r":H-?[0-9a-fA-F]+(?::[0-9]+)?")

NATIVE_CASES = (
    "element-map-child-map-recursive-compound",
    "part-import-brep-edge-stable",
    "part-import-step-face-stable",
)


class C3M1NativePublicParityTest(unittest.TestCase):
    """Direct public-CLI checks for the three native C3M1 fixtures.

    The test intentionally owns its small public-semantic views rather than importing
    compare_freecad_expected.py or freecad_expected_parity.  It therefore proves that
    the supported CLI itself publishes the native result, identity, and topo-state
    evidence required by this focused C3M1 slice.
    """

    expected_by_case: dict[str, dict[str, Any]]
    actual_by_case: dict[str, dict[str, Any]]

    @classmethod
    def setUpClass(cls) -> None:
        super().setUpClass()
        if not BINARY.is_file():
            raise AssertionError(f"missing CAD Core binary; build it first: {BINARY}")

        cls.expected_by_case = {}
        cls.actual_by_case = {}
        environment = os.environ.copy()
        environment.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
        with tempfile.TemporaryDirectory(prefix="c3m1-native-public-parity-") as directory:
            output_root = Path(directory)
            for case in NATIVE_CASES:
                matches = sorted((ROOT / "fixtures").glob(f"*/{case}.json"))
                if len(matches) != 1:
                    raise AssertionError(f"{case}: expected one semantic phase, got {matches}")
                input_path = matches[0]
                expected_path = input_path.parent / "expected" / f"{case}.freecad.json"
                ledger_path = input_path.parent / "expected" / f"{case}.freecad.ledger.json"
                output_path = output_root / f"{case}.cad-core.json"
                for path in (input_path, expected_path, ledger_path):
                    if not path.is_file():
                        raise AssertionError(f"missing native C3M1 artifact: {path}")

                completed = subprocess.run(
                    [str(BINARY), "recompute", str(input_path), "--output", str(output_path)],
                    cwd=ROOT,
                    env=environment,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
                if completed.returncode != 0 or not output_path.is_file():
                    raise AssertionError(
                        f"official CAD Core recompute failed for {case}: "
                        f"returncode={completed.returncode}\n"
                        f"stdout:\n{completed.stdout}\n"
                        f"stderr:\n{completed.stderr}"
                    )

                expected = json.loads(expected_path.read_text(encoding="utf-8"))
                actual = json.loads(output_path.read_text(encoding="utf-8"))
                if not isinstance(expected, dict) or not isinstance(actual, dict):
                    raise AssertionError(f"{case}: public response must be a JSON object")
                cls.expected_by_case[case] = expected
                cls.actual_by_case[case] = actual

    @staticmethod
    def _keyed_objects(items: Any, key: str, label: str) -> dict[str, dict[str, Any]]:
        if not isinstance(items, list):
            raise AssertionError(f"{label}: expected list")
        result: dict[str, dict[str, Any]] = {}
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                raise AssertionError(f"{label}[{index}]: expected object")
            value = item.get(key)
            if not isinstance(value, str) or not value:
                raise AssertionError(f"{label}[{index}].{key}: expected non-empty string")
            if value in result:
                raise AssertionError(f"{label}: duplicate {key} {value!r}")
            result[value] = item
        return result

    @classmethod
    def _results_by_object(cls, payload: dict[str, Any], label: str) -> dict[str, dict[str, Any]]:
        return cls._keyed_objects(payload.get("results"), "object", f"{label}.results")

    @classmethod
    def _subshapes_by_index(
        cls,
        result: dict[str, Any],
        label: str,
    ) -> dict[str, dict[str, Any]]:
        return cls._keyed_objects(result.get("subshapes"), "indexed", f"{label}.subshapes")

    @staticmethod
    def _require_object(value: Any, label: str) -> dict[str, Any]:
        if not isinstance(value, dict):
            raise AssertionError(f"{label}: expected object")
        return value

    @staticmethod
    def _canonicalize_producer_tags(raw: str, label: str) -> str:
        """Validate the public mapped-name codec's producer-local tag projection."""

        def replace(match: re.Match[str]) -> str:
            value = match.group(0)
            return ":H*:*" if ":" in value[2:] else ":H*"

        canonical, replacement_count = FREECAD_PRODUCER_TAG.subn(replace, raw)
        if replacement_count == 0:
            raise AssertionError(f"{label}: expected at least one real FreeCAD :H<hex> producer tag")
        return canonical

    def _assert_mapped_name_equal(self, expected: Any, actual: Any, label: str) -> None:
        expected_name = self._require_object(expected, f"{label}.expected")
        actual_name = self._require_object(actual, f"{label}.actual")
        expected_raw = expected_name.get("raw")
        actual_raw = actual_name.get("raw")
        expected_canonical = expected_name.get("canonical")
        actual_canonical = actual_name.get("canonical")
        self.assertIsInstance(expected_raw, str, f"{label}.expected.raw: expected string")
        self.assertIsInstance(actual_raw, str, f"{label}.actual.raw: expected string")
        self.assertIsInstance(expected_canonical, str, f"{label}.expected.canonical: expected string")
        self.assertIsInstance(actual_canonical, str, f"{label}.actual.canonical: expected string")
        self.assertEqual(expected_canonical, actual_canonical, f"{label}.canonical")
        self.assertEqual(
            expected_canonical,
            self._canonicalize_producer_tags(expected_raw, f"{label}.expected.raw"),
            f"{label}.expected raw codec projection",
        )
        self.assertEqual(
            actual_canonical,
            self._canonicalize_producer_tags(actual_raw, f"{label}.actual.raw"),
            f"{label}.actual raw codec projection",
        )

    def _assert_optional_subshape_mapped_name_equal(
        self, expected: dict[str, Any], actual: dict[str, Any], label: str
    ) -> None:
        raw_key = "rawFreecadMappedName"
        canonical_key = "canonicalFreecadMappedName"
        self.assertEqual(raw_key in expected, raw_key in actual, f"{label}.{raw_key} presence")
        self.assertEqual(
            canonical_key in expected,
            canonical_key in actual,
            f"{label}.{canonical_key} presence",
        )
        if raw_key not in expected:
            return
        self._assert_mapped_name_equal(
            {"raw": expected[raw_key], "canonical": expected[canonical_key]},
            {"raw": actual[raw_key], "canonical": actual[canonical_key]},
            f"{label}.mappedName",
        )

    def _assert_public_subshape_equal(
        self, expected: dict[str, Any], actual: dict[str, Any], label: str
    ) -> None:
        self._assert_optional_subshape_mapped_name_equal(expected, actual, label)
        expected_rest = dict(expected)
        actual_rest = dict(actual)
        # Raw bytes were validated above against the canonical codec.  Compare every other
        # public field exactly; producer-local tag values are the only intentionally local bytes.
        expected_rest.pop("rawFreecadMappedName", None)
        actual_rest.pop("rawFreecadMappedName", None)
        self.assertEqual(expected_rest, actual_rest, label)

    def _assert_element_map_equal(self, expected: Any, actual: Any, label: str) -> None:
        expected_map = self._require_object(expected, f"{label}.expected")
        actual_map = self._require_object(actual, f"{label}.actual")
        for field in ("encoding", "status"):
            self.assertEqual(expected_map.get(field), actual_map.get(field), f"{label}.{field}")
        expected_entries = self._require_object(expected_map.get("entries"), f"{label}.expected.entries")
        actual_entries = self._require_object(actual_map.get("entries"), f"{label}.actual.entries")
        self.assertEqual(set(expected_entries), set(actual_entries), f"{label}.entries coverage")
        for name, expected_entry_value in expected_entries.items():
            expected_entry = self._require_object(expected_entry_value, f"{label}.expected.entries.{name}")
            actual_entry = self._require_object(actual_entries[name], f"{label}.actual.entries.{name}")
            self._assert_mapped_name_equal(
                expected_entry.get("mappedName"),
                actual_entry.get("mappedName"),
                f"{label}.entries.{name}.mappedName",
            )
            expected_rest = dict(expected_entry)
            actual_rest = dict(actual_entry)
            expected_rest.pop("mappedName", None)
            actual_rest.pop("mappedName", None)
            self.assertEqual(expected_rest, actual_rest, f"{label}.entries.{name}")

    def _assert_child_maps_equal(self, expected: Any, actual: Any, label: str) -> None:
        expected_maps = self._keyed_objects(expected, "key", f"{label}.expected")
        actual_maps = self._keyed_objects(actual, "key", f"{label}.actual")
        self.assertEqual(set(expected_maps), set(actual_maps), f"{label} coverage")
        for key, expected_child in expected_maps.items():
            actual_child = actual_maps[key]
            for field in ("childIndex", "childObject", "ownerObject", "pathPrefix"):
                self.assertEqual(
                    expected_child.get(field), actual_child.get(field), f"{label}.{key}.{field}"
                )
            self._assert_element_map_equal(
                expected_child.get("elementMap"),
                actual_child.get("elementMap"),
                f"{label}.{key}.elementMap",
            )

    def _assert_mapper_history_equal(self, expected: Any, actual: Any, label: str) -> None:
        self.assertIsInstance(expected, list, f"{label}.expected: expected list")
        self.assertIsInstance(actual, list, f"{label}.actual: expected list")
        self.assertEqual(len(expected), len(actual), f"{label} length")
        for index, (expected_item_value, actual_item_value) in enumerate(zip(expected, actual)):
            item_label = f"{label}[{index}]"
            expected_item = self._require_object(expected_item_value, f"{item_label}.expected")
            actual_item = self._require_object(actual_item_value, f"{item_label}.actual")
            self._assert_mapped_name_equal(
                expected_item.get("mappedName"),
                actual_item.get("mappedName"),
                f"{item_label}.mappedName",
            )
            expected_candidates = expected_item.get("candidates")
            actual_candidates = actual_item.get("candidates")
            self.assertIsInstance(expected_candidates, list, f"{item_label}.expected.candidates")
            self.assertIsInstance(actual_candidates, list, f"{item_label}.actual.candidates")
            self.assertEqual(
                len(expected_candidates), len(actual_candidates), f"{item_label}.candidates length"
            )
            for candidate_index, (expected_candidate_value, actual_candidate_value) in enumerate(
                zip(expected_candidates, actual_candidates)
            ):
                candidate_label = f"{item_label}.candidates[{candidate_index}]"
                expected_candidate = self._require_object(
                    expected_candidate_value, f"{candidate_label}.expected"
                )
                actual_candidate = self._require_object(
                    actual_candidate_value, f"{candidate_label}.actual"
                )
                self._assert_mapped_name_equal(
                    expected_candidate.get("mappedName"),
                    actual_candidate.get("mappedName"),
                    f"{candidate_label}.mappedName",
                )
                expected_candidate_rest = dict(expected_candidate)
                actual_candidate_rest = dict(actual_candidate)
                expected_candidate_rest.pop("mappedName", None)
                actual_candidate_rest.pop("mappedName", None)
                self.assertEqual(expected_candidate_rest, actual_candidate_rest, candidate_label)

            self.assertEqual(expected_item.get("relation"), actual_item.get("relation"), item_label)
            if expected_item.get("relation") == "ambiguous":
                # The native oracle records the observation layer, while CAD Core records the
                # actual Part ElementMap ledger that found the collision.  Both are explicitly
                # asserted; neither source label is silently normalized into the other.
                self.assertEqual("freecad_expected_collector", expected_item.get("source"), item_label)
                self.assertEqual("part_element_map", actual_item.get("source"), item_label)
                self.assertIn("collector keeps", expected_item.get("message", ""), item_label)
                self.assertIn("Part keeps", actual_item.get("message", ""), item_label)
                excluded = {"mappedName", "candidates", "source", "message"}
            else:
                excluded = {"mappedName", "candidates"}
            expected_rest = {key: value for key, value in expected_item.items() if key not in excluded}
            actual_rest = {key: value for key, value in actual_item.items() if key not in excluded}
            self.assertEqual(expected_rest, actual_rest, item_label)

    def _assert_close_number(self, expected: Any, actual: Any, path: str) -> None:
        self.assertIsInstance(expected, (int, float), f"{path}: expected fixture value is not numeric")
        self.assertIsInstance(actual, (int, float), f"{path}: CLI value is not numeric")
        self.assertTrue(
            math.isclose(
                float(expected),
                float(actual),
                rel_tol=0.0,
                abs_tol=SEMANTIC_NUMERIC_TOLERANCE,
            ),
            f"{path}: expected {expected!r}, actual {actual!r}, "
            f"tolerance={SEMANTIC_NUMERIC_TOLERANCE}",
        )

    def _assert_bbox(self, expected: Any, actual: Any, path: str) -> None:
        expected_bbox = self._require_object(expected, f"{path}.expected")
        actual_bbox = self._require_object(actual, f"{path}.actual")
        for bound in ("min", "max"):
            expected_values = expected_bbox.get(bound)
            actual_values = actual_bbox.get(bound)
            self.assertIsInstance(expected_values, list, f"{path}.{bound}: expected list")
            self.assertIsInstance(actual_values, list, f"{path}.{bound}: actual list")
            self.assertEqual(len(expected_values), 3, f"{path}.{bound}: expected 3 coordinates")
            self.assertEqual(len(actual_values), 3, f"{path}.{bound}: actual 3 coordinates")
            for index, (expected_value, actual_value) in enumerate(zip(expected_values, actual_values)):
                self._assert_close_number(expected_value, actual_value, f"{path}.{bound}[{index}]")

    def test_official_cli_public_mode_and_native_inventory(self) -> None:
        self.assertEqual(set(NATIVE_CASES), set(self.expected_by_case))
        self.assertEqual(set(NATIVE_CASES), set(self.actual_by_case))
        for case in NATIVE_CASES:
            with self.subTest(case=case):
                actual = self.actual_by_case[case]
                self.assertNotIn(
                    "objects",
                    actual,
                    "legacy test output leaked into the native public-parity test",
                )
                self.assertIsInstance(actual.get("results"), list)
                self.assertIsInstance(actual.get("diagnostics"), list)
                self.assertIsInstance(actual.get("topoNamingState"), dict)

    def test_native_results_match_bbox_topology_volume_and_subshape_identity(self) -> None:
        for case in NATIVE_CASES:
            with self.subTest(case=case):
                expected_results = self._results_by_object(self.expected_by_case[case], f"{case}.expected")
                actual_results = self._results_by_object(self.actual_by_case[case], f"{case}.actual")
                self.assertEqual(set(expected_results), set(actual_results), f"{case}: result objects")
                for object_name, expected_result in expected_results.items():
                    actual_result = actual_results[object_name]
                    self._assert_bbox(
                        expected_result.get("bbox"),
                        actual_result.get("bbox"),
                        f"{case}.results.{object_name}.bbox",
                    )
                    self.assertEqual(
                        expected_result.get("topology_counts"),
                        actual_result.get("topology_counts"),
                        f"{case}.results.{object_name}.topology_counts",
                    )
                    self._assert_close_number(
                        expected_result.get("volume"),
                        actual_result.get("volume"),
                        f"{case}.results.{object_name}.volume",
                    )

                    expected_subshapes = self._subshapes_by_index(
                        expected_result, f"{case}.expected.results.{object_name}"
                    )
                    actual_subshapes = self._subshapes_by_index(
                        actual_result, f"{case}.actual.results.{object_name}"
                    )
                    self.assertEqual(
                        set(expected_subshapes),
                        set(actual_subshapes),
                        f"{case}.results.{object_name}.subshapes indexed coverage",
                    )
                    for indexed, expected_subshape in expected_subshapes.items():
                        self._assert_public_subshape_equal(
                            expected_subshape,
                            actual_subshapes[indexed],
                            f"{case}.results.{object_name}.subshapes.{indexed}",
                        )

    def test_native_diagnostics_and_reference_updates_match(self) -> None:
        for case in NATIVE_CASES:
            with self.subTest(case=case):
                expected = self.expected_by_case[case]
                actual = self.actual_by_case[case]
                self.assertEqual(expected.get("diagnostics"), actual.get("diagnostics"))
                self.assertEqual(
                    expected.get("elementReferenceUpdates"),
                    actual.get("elementReferenceUpdates"),
                )

    def test_native_topo_state_matches_element_maps_child_maps_history_and_subshapes(self) -> None:
        for case in NATIVE_CASES:
            with self.subTest(case=case):
                expected_state = self._require_object(
                    self.expected_by_case[case].get("topoNamingState"),
                    f"{case}.expected.topoNamingState",
                )
                actual_state = self._require_object(
                    self.actual_by_case[case].get("topoNamingState"),
                    f"{case}.actual.topoNamingState",
                )
                for field in ("schemaVersion", "producer", "documentHash"):
                    self.assertEqual(
                        expected_state.get(field),
                        actual_state.get(field),
                        f"{case}.topoNamingState.{field}",
                    )

                expected_objects = self._require_object(
                    expected_state.get("objects"), f"{case}.expected.topoNamingState.objects"
                )
                actual_objects = self._require_object(
                    actual_state.get("objects"), f"{case}.actual.topoNamingState.objects"
                )
                self.assertEqual(
                    set(expected_objects),
                    set(actual_objects),
                    f"{case}.topoNamingState object coverage",
                )
                for object_name, expected_object_value in expected_objects.items():
                    expected_object = self._require_object(
                        expected_object_value,
                        f"{case}.expected.topoNamingState.objects.{object_name}",
                    )
                    actual_object = self._require_object(
                        actual_objects[object_name],
                        f"{case}.actual.topoNamingState.objects.{object_name}",
                    )
                    for field in ("objectHash", "elementMapVersion"):
                        self.assertEqual(
                            expected_object.get(field),
                            actual_object.get(field),
                            f"{case}.topoNamingState.objects.{object_name}.{field}",
                        )

                    expected_subshapes = self._require_object(
                        expected_object.get("subshapes"),
                        f"{case}.expected.topoNamingState.objects.{object_name}.subshapes",
                    )
                    actual_subshapes = self._require_object(
                        actual_object.get("subshapes"),
                        f"{case}.actual.topoNamingState.objects.{object_name}.subshapes",
                    )
                    self.assertEqual(
                        set(expected_subshapes),
                        set(actual_subshapes),
                        f"{case}.topoNamingState.objects.{object_name}.subshape coverage",
                    )
                    for indexed, expected_subshape_value in expected_subshapes.items():
                        expected_subshape = self._require_object(
                            expected_subshape_value,
                            f"{case}.expected.topoNamingState.objects.{object_name}.subshapes.{indexed}",
                        )
                        actual_subshape = self._require_object(
                            actual_subshapes[indexed],
                            f"{case}.actual.topoNamingState.objects.{object_name}.subshapes.{indexed}",
                        )
                        self._assert_public_subshape_equal(
                            expected_subshape,
                            actual_subshape,
                            f"{case}.topoNamingState.objects.{object_name}.subshapes.{indexed}",
                        )

                    self._assert_element_map_equal(
                        expected_object.get("elementMap"),
                        actual_object.get("elementMap"),
                        f"{case}.topoNamingState.objects.{object_name}.elementMap",
                    )
                    self._assert_child_maps_equal(
                        expected_object.get("childElementMaps"),
                        actual_object.get("childElementMaps"),
                        f"{case}.topoNamingState.objects.{object_name}.childElementMaps",
                    )
                    self._assert_mapper_history_equal(
                        expected_object.get("mapperHistory"),
                        actual_object.get("mapperHistory"),
                        f"{case}.topoNamingState.objects.{object_name}.mapperHistory",
                    )

    def test_imports_publish_current_only_without_synthetic_alias_or_history(self) -> None:
        for case, object_name in (
            ("part-import-brep-edge-stable", "ImportedCylinder"),
            ("part-import-step-face-stable", "ImportedStep"),
        ):
            with self.subTest(case=case):
                actual_result = self._results_by_object(self.actual_by_case[case], f"{case}.actual")[
                    object_name
                ]
                self.assertEqual(
                    {"current_only"},
                    {item["identityStatus"] for item in actual_result["subshapes"]},
                )
                self.assertEqual({""}, {item["stableSubname"] for item in actual_result["subshapes"]})
                actual_object = self.actual_by_case[case]["topoNamingState"]["objects"][object_name]
                self._assert_element_map_equal(
                    {
                        "encoding": "cad-core.element-map.v1",
                        "status": "indexed_only",
                        "entries": {},
                    },
                    actual_object["elementMap"],
                    f"{case}.{object_name}.elementMap",
                )
                self.assertEqual([], actual_object["childElementMaps"])
                self.assertEqual([], actual_object["mapperHistory"])


if __name__ == "__main__":
    unittest.main()
