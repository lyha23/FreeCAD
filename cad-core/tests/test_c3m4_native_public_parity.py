from __future__ import annotations

import json
import math
import os
import subprocess
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "cad-core"

NATIVE_CASES = (
    "part-filling-boundary-edges-default",
    "part-filling-closed-wire-default",
    "part-filling-invalid-inputs",
    "part-geomplate-curve-point-default",
    "part-geomplate-invalid-inputs",
    "part-loft-closed",
    "part-loft-ruled",
    "part-loft-solid",
    "part-loft-two-section-surface",
    "part-section-stable-history",
    "part-sweep-frenet-off",
    "part-sweep-open-profile-surface",
    "part-sweep-right-corner-surface",
    "part-sweep-solid",
    "part-sweep-spine-subedges",
    "part-sweep-transition-round-corner",
    "part-sweep-transition-transformed",
)


class C3M4NativePublicParityTest(unittest.TestCase):
    """Direct public-response checks for the 17 c3m4 native fixtures.

    These checks intentionally do not import the expected-parity projector.  They run
    the supported CLI response and compare selected FreeCAD public fields directly so
    a projector or registry rule cannot turn a missing native field into green.
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
        env = os.environ.copy()
        env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
        with tempfile.TemporaryDirectory(prefix="c3m4-native-public-parity-") as directory:
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
                        raise AssertionError(f"missing native c3m4 artifact: {path}")

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
    def _results_by_object(
        payload: dict[str, Any],
        case: str,
        producer: str,
    ) -> dict[str, dict[str, Any]]:
        results = payload.get("results")
        if not isinstance(results, list):
            raise AssertionError(f"{case}: {producer}.results must be a list")
        by_object: dict[str, dict[str, Any]] = {}
        for index, item in enumerate(results):
            if not isinstance(item, dict) or not isinstance(item.get("object"), str):
                raise AssertionError(f"{case}: {producer}.results[{index}] has no object name")
            object_name = item["object"]
            if object_name in by_object:
                raise AssertionError(f"{case}: duplicate {producer} result object {object_name}")
            by_object[object_name] = item
        return by_object

    @classmethod
    def _public_subset_mismatches(
        cls,
        expected: Any,
        actual: Any,
        path: str,
    ) -> list[str]:
        if isinstance(expected, dict):
            if not isinstance(actual, dict):
                return [f"{path}: expected object, actual={type(actual).__name__}"]
            mismatches: list[str] = []
            for key, expected_value in expected.items():
                child = f"{path}.{key}"
                if key not in actual:
                    mismatches.append(f"{child}: missing")
                    continue
                mismatches.extend(cls._public_subset_mismatches(expected_value, actual[key], child))
            return mismatches
        if isinstance(expected, list):
            if not isinstance(actual, list):
                return [f"{path}: expected list, actual={type(actual).__name__}"]
            if len(expected) != len(actual):
                return [f"{path}: expected length {len(expected)}, actual length {len(actual)}"]
            mismatches: list[str] = []
            for index, expected_value in enumerate(expected):
                mismatches.extend(
                    cls._public_subset_mismatches(expected_value, actual[index], f"{path}[{index}]")
                )
            return mismatches
        if (
            isinstance(expected, (int, float))
            and not isinstance(expected, bool)
            and isinstance(actual, (int, float))
            and not isinstance(actual, bool)
        ):
            if not math.isclose(float(expected), float(actual), rel_tol=0.0, abs_tol=1e-6):
                return [f"{path}: expected {expected!r}, actual {actual!r}"]
            return []
        if expected != actual:
            return [f"{path}: expected {expected!r}, actual {actual!r}"]
        return []

    def _assert_no_mismatches(self, category: str, mismatches: list[str]) -> None:
        self.assertEqual([], mismatches, f"{category} mismatches:\n" + "\n".join(mismatches))

    def test_official_output_covers_exactly_the_17_native_cases(self) -> None:
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

    def test_expected_result_object_fields_are_published_as_public_subsets(self) -> None:
        mismatches: list[str] = []
        for case in NATIVE_CASES:
            expected_results = self._results_by_object(
                self.expected_by_case[case], case, "expected"
            )
            actual_results = self._results_by_object(self.actual_by_case[case], case, "actual")
            for object_name, expected_result in expected_results.items():
                if "object_fields" not in expected_result:
                    continue
                actual_result = actual_results.get(object_name)
                if actual_result is None:
                    mismatches.append(f"{case}.results.{object_name}: missing result")
                    continue
                mismatches.extend(
                    self._public_subset_mismatches(
                        expected_result["object_fields"],
                        actual_result.get("object_fields"),
                        f"{case}.results.{object_name}.object_fields",
                    )
                )
        self._assert_no_mismatches("result object_fields", mismatches)

    def test_expected_native_errors_and_codes_are_published_exactly(self) -> None:
        mismatches: list[str] = []
        for case in NATIVE_CASES:
            expected_results = self._results_by_object(
                self.expected_by_case[case], case, "expected"
            )
            actual_results = self._results_by_object(self.actual_by_case[case], case, "actual")
            for object_name, expected_result in expected_results.items():
                for field in ("native_error", "native_error_code"):
                    if field not in expected_result:
                        continue
                    actual_result = actual_results.get(object_name)
                    if actual_result is None:
                        mismatches.append(f"{case}.results.{object_name}: missing result")
                        continue
                    if field not in actual_result:
                        mismatches.append(f"{case}.results.{object_name}.{field}: missing")
                    elif actual_result[field] != expected_result[field]:
                        mismatches.append(
                            f"{case}.results.{object_name}.{field}: "
                            f"expected {expected_result[field]!r}, actual {actual_result[field]!r}"
                        )
        self._assert_no_mismatches("native failure publication", mismatches)

    def test_expected_filling_shape_summaries_are_published_as_public_subsets(self) -> None:
        mismatches: list[str] = []
        for case in NATIVE_CASES:
            expected_results = self._results_by_object(
                self.expected_by_case[case], case, "expected"
            )
            actual_results = self._results_by_object(self.actual_by_case[case], case, "actual")
            for object_name, expected_result in expected_results.items():
                if "shape_summary" not in expected_result:
                    continue
                actual_result = actual_results.get(object_name)
                if actual_result is None:
                    mismatches.append(f"{case}.results.{object_name}: missing result")
                    continue
                mismatches.extend(
                    self._public_subset_mismatches(
                        expected_result["shape_summary"],
                        actual_result.get("shape_summary"),
                        f"{case}.results.{object_name}.shape_summary",
                    )
                )
        self._assert_no_mismatches("filling shape_summary", mismatches)

    def test_expected_diagnostic_code_and_source_counts_match_exactly(self) -> None:
        mismatches: list[str] = []
        for case in NATIVE_CASES:
            expected_diagnostics = self.expected_by_case[case].get("diagnostics")
            actual_diagnostics = self.actual_by_case[case].get("diagnostics")
            if not isinstance(expected_diagnostics, list) or not isinstance(
                actual_diagnostics, list
            ):
                mismatches.append(f"{case}.diagnostics: expected and actual must both be lists")
                continue
            expected_counts = Counter(
                (item.get("code"), item.get("source"))
                for item in expected_diagnostics
                if isinstance(item, dict)
            )
            actual_counts = Counter(
                (item.get("code"), item.get("source"))
                for item in actual_diagnostics
                if isinstance(item, dict)
            )
            if expected_counts != actual_counts:
                mismatches.append(
                    f"{case}.diagnostics code+source counts: "
                    f"expected={dict(expected_counts)!r}, actual={dict(actual_counts)!r}"
                )
        self._assert_no_mismatches("diagnostic code+source", mismatches)

    def test_section_topo_state_publishes_expected_canonical_element_map_and_subshapes(
        self,
    ) -> None:
        case = "part-section-stable-history"
        expected_state = self.expected_by_case[case]["topoNamingState"]["objects"]["Section"]
        actual_topo_state = self.actual_by_case[case].get("topoNamingState")
        self.assertIsInstance(actual_topo_state, dict)
        actual_objects = actual_topo_state.get("objects")
        self.assertIsInstance(actual_objects, dict)
        self.assertIn("Section", actual_objects)
        actual_state = actual_objects["Section"]

        mismatches: list[str] = []
        expected_element_map = expected_state["elementMap"]
        actual_element_map = actual_state.get("elementMap")
        if not isinstance(actual_element_map, dict):
            mismatches.append("Section.elementMap: missing or not an object")
        else:
            if actual_element_map.get("status") != expected_element_map.get("status"):
                mismatches.append(
                    "Section.elementMap.status: "
                    f"expected {expected_element_map.get('status')!r}, "
                    f"actual {actual_element_map.get('status')!r}"
                )
            actual_entries = actual_element_map.get("entries")
            if not isinstance(actual_entries, dict):
                mismatches.append("Section.elementMap.entries: missing or not an object")
            else:
                for canonical_name, expected_entry in expected_element_map["entries"].items():
                    actual_entry = actual_entries.get(canonical_name)
                    if actual_entry is None:
                        mismatches.append(f"Section.elementMap.entries.{canonical_name}: missing")
                        continue
                    expected_public_entry = {
                        "mappedName": {"canonical": expected_entry["mappedName"]["canonical"]},
                        "recoverability": expected_entry["recoverability"],
                        "shapeKind": expected_entry["shapeKind"],
                        "source": expected_entry["source"],
                        "target": expected_entry["target"],
                    }
                    mismatches.extend(
                        self._public_subset_mismatches(
                            expected_public_entry,
                            actual_entry,
                            f"Section.elementMap.entries.{canonical_name}",
                        )
                    )

        actual_subshapes = actual_state.get("subshapes")
        if not isinstance(actual_subshapes, dict):
            mismatches.append("Section.subshapes: missing or not an object")
        else:
            for indexed, expected_subshape in expected_state["subshapes"].items():
                actual_subshape = actual_subshapes.get(indexed)
                if actual_subshape is None:
                    mismatches.append(f"Section.subshapes.{indexed}: missing")
                    continue
                expected_public_subshape = {
                    key: expected_subshape[key]
                    for key in (
                        "canonicalFreecadMappedName",
                        "identityStatus",
                        "resolvedIndexed",
                        "subname",
                    )
                }
                mismatches.extend(
                    self._public_subset_mismatches(
                        expected_public_subshape,
                        actual_subshape,
                        f"Section.subshapes.{indexed}",
                    )
                )

        self._assert_no_mismatches("Section canonical topo state", mismatches)

    def test_section_mapper_history_has_no_actual_only_public_entries(self) -> None:
        case = "part-section-stable-history"
        expected_state = self.expected_by_case[case]["topoNamingState"]["objects"]["Section"]
        actual_state = self.actual_by_case[case]["topoNamingState"]["objects"]["Section"]
        self.assertEqual(
            expected_state["mapperHistory"],
            actual_state.get("mapperHistory"),
            "actual-only Section MapperHistory must not replace the expected canonical ElementMap",
        )

    def test_public_topo_state_covers_exactly_the_native_shape_objects(self) -> None:
        mismatches: list[str] = []
        for case in NATIVE_CASES:
            expected_state = self.expected_by_case[case].get("topoNamingState")
            actual_state = self.actual_by_case[case].get("topoNamingState")
            if not isinstance(expected_state, dict) or not isinstance(actual_state, dict):
                mismatches.append(f"{case}.topoNamingState: expected and actual must be objects")
                continue
            expected_objects = expected_state.get("objects")
            actual_objects = actual_state.get("objects")
            if not isinstance(expected_objects, dict) or not isinstance(actual_objects, dict):
                mismatches.append(
                    f"{case}.topoNamingState.objects: expected and actual must be objects"
                )
                continue
            if set(expected_objects) != set(actual_objects):
                mismatches.append(
                    f"{case}.topoNamingState.objects: "
                    f"expected={sorted(expected_objects)!r}, actual={sorted(actual_objects)!r}"
                )
        self._assert_no_mismatches("topo state object publication", mismatches)


if __name__ == "__main__":
    unittest.main()
