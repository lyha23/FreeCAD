from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from typing import Any


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]
TESTS_ROOT = CAD_CORE_ROOT / "tests"
if str(TESTS_ROOT) not in sys.path:
    sys.path.insert(0, str(TESTS_ROOT))

from fixture_expected import discover_expected_cases  # noqa: E402


HARD_FAIL_WITHOUT_TOPO_STATE = {
    "c4m6/topo-state-schema-incompatible",
    "c4m6/topo-state-producer-incompatible",
}

REQUIRED_MAPPER_RELATIONS = {
    "generated",
    "modified",
    "split",
    "deleted",
    "merge",
    "ambiguous",
}

REQUIRED_RECOVERY_DIAGNOSTICS = {
    "split_stable_subname",
    "deleted_stable_subname",
    "stable_identity_ambiguous",
}


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def case_name(group: str, fixture: str) -> str:
    return f"{group}/{fixture}"


def expected_payloads() -> list[tuple[str, str, Path, dict[str, Any]]]:
    return [
        (group, fixture, path, load_json(path))
        for group, fixture, path in discover_expected_cases(CAD_CORE_ROOT)
    ]


def topo_objects(payload: dict[str, Any]) -> dict[str, Any]:
    state = payload.get("topoNamingState")
    if not isinstance(state, dict):
        return {}
    objects = state.get("objects")
    return objects if isinstance(objects, dict) else {}


def walk_json(value: Any):
    if isinstance(value, dict):
        yield value
        for item in value.values():
            yield from walk_json(item)
    elif isinstance(value, list):
        for item in value:
            yield from walk_json(item)


class FreecadExpectedNativeOracleCoverageTest(unittest.TestCase):
    def test_freecad_expected_files_carry_native_oracle_producer_metadata(self) -> None:
        failures: list[str] = []
        for group, fixture, _path, payload in expected_payloads():
            name = case_name(group, fixture)
            if name in HARD_FAIL_WITHOUT_TOPO_STATE:
                self.assertNotIn("topoNamingState", payload, name)
                self.assertTrue(payload.get("diagnostics"), name)
                continue

            state = payload.get("topoNamingState")
            if not isinstance(state, dict):
                failures.append(f"{name}:missing_topoNamingState")
                continue
            producer = state.get("producer")
            if not isinstance(producer, dict):
                failures.append(f"{name}:missing_producer")
                continue
            freecad_version = producer.get("freecadVersion")
            occt_version = producer.get("occtVersion")
            if not isinstance(freecad_version, str) or "revision" not in freecad_version:
                failures.append(f"{name}:producer.freecadVersion={freecad_version!r}")
            if not isinstance(occt_version, str) or not occt_version:
                failures.append(f"{name}:producer.occtVersion={occt_version!r}")
            if producer.get("freecadVersion") == "cad-core-runtime":
                failures.append(f"{name}:producer.freecadVersion.cad_core_runtime")

        self.assertEqual(failures, [])

    def test_public_topo_state_oracle_covers_ledger_publication_shapes(self) -> None:
        coverage = {
            "elementMap.entries": 0,
            "childElementMaps": 0,
            "mapperHistory": 0,
        }
        examples: dict[str, str] = {}
        relations: set[str] = set()
        recovery_diagnostics: set[str] = set()

        for group, fixture, _path, payload in expected_payloads():
            name = case_name(group, fixture)
            for diagnostic in payload.get("diagnostics") or []:
                if isinstance(diagnostic, dict) and isinstance(diagnostic.get("code"), str):
                    recovery_diagnostics.add(diagnostic["code"])

            for object_state in topo_objects(payload).values():
                if not isinstance(object_state, dict):
                    continue
                entries = (object_state.get("elementMap") or {}).get("entries") or {}
                if isinstance(entries, dict) and entries:
                    coverage["elementMap.entries"] += len(entries)
                    examples.setdefault("elementMap.entries", name)

                child_maps = object_state.get("childElementMaps") or []
                if isinstance(child_maps, list) and child_maps:
                    coverage["childElementMaps"] += len(child_maps)
                    examples.setdefault("childElementMaps", name)

                mapper_history = object_state.get("mapperHistory") or []
                if isinstance(mapper_history, list) and mapper_history:
                    coverage["mapperHistory"] += len(mapper_history)
                    examples.setdefault("mapperHistory", name)
                    for event in mapper_history:
                        if isinstance(event, dict) and isinstance(event.get("relation"), str):
                            relations.add(event["relation"])

        self.assertGreater(coverage["elementMap.entries"], 0, examples)
        self.assertGreater(coverage["childElementMaps"], 0, examples)
        self.assertGreater(coverage["mapperHistory"], 0, examples)
        self.assertTrue(REQUIRED_MAPPER_RELATIONS <= relations, sorted(relations))
        self.assertTrue(
            REQUIRED_RECOVERY_DIAGNOSTICS <= recovery_diagnostics,
            sorted(recovery_diagnostics),
        )

    def test_mapper_history_native_gap_is_explicit_in_expected_oracle(self) -> None:
        payload = load_json(
            CAD_CORE_ROOT
            / "fixtures"
            / "c4m6"
            / "expected"
            / "topo-state-mapper-history-events.freecad.json"
        )
        diagnostics = payload.get("diagnostics") or []
        codes = {
            diagnostic.get("code")
            for diagnostic in diagnostics
            if isinstance(diagnostic, dict)
        }
        self.assertIn("unsupported_native_mapper_history", codes)

    def test_freecad_expected_is_publication_oracle_not_full_internal_named_shape_dump(self) -> None:
        """Guard the boundary that motivated this audit.

        The checked-in .freecad.json corpus is strong enough for public
        topoNamingState publication coverage, but it is not a full legacy
        cad-core named_shapes dump. Internal completeness must still be
        validated against cad-core runtime outputs or dedicated native probes.
        """

        named_shape_element_map_examples: list[str] = []
        mapped_name_provenance_examples: list[str] = []
        child_element_maps_examples: list[str] = []
        mapper_history_examples: list[str] = []

        for group, fixture, _path, payload in expected_payloads():
            name = case_name(group, fixture)
            for item in walk_json(payload):
                if "mapped_name_provenance" in item:
                    mapped_name_provenance_examples.append(name)
                if "child_element_maps" in item:
                    child_element_maps_examples.append(name)
                if "mapper_history" in item:
                    mapper_history_examples.append(name)

            for result in payload.get("results") or []:
                if not isinstance(result, dict):
                    continue
                named_shapes = result.get("named_shapes")
                if not isinstance(named_shapes, dict):
                    continue
                for named_shape in named_shapes.values():
                    if isinstance(named_shape, dict) and "element_map" in named_shape:
                        named_shape_element_map_examples.append(name)

        self.assertEqual(named_shape_element_map_examples, [])
        self.assertEqual(mapped_name_provenance_examples, [])
        self.assertEqual(child_element_maps_examples, [])
        self.assertEqual(mapper_history_examples, [])


if __name__ == "__main__":
    unittest.main()
