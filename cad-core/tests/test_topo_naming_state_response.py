from __future__ import annotations

import copy
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

try:
    from .fixture_runner import BIN, ROOT
    from .topo_naming_state_test_helpers import (
        canonicalize_freecad_mapped_names_and_keys,
        response_subshape_identity_index,
    )
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BIN, ROOT
    from topo_naming_state_test_helpers import (
        canonicalize_freecad_mapped_names_and_keys,
        response_subshape_identity_index,
    )


C13M3_PRODUCER_EVIDENCE_CASES = (
    ("p2", "rect-pad-pocket", "Body"),
    ("c4m6", "topo-state-body-tip-stable-recovery", "Body"),
)

C4M6_TOPO_STATE_PARITY_FIXTURES = (
    "topo-state-first-recompute-empty",
    "topo-state-body-tip-stable-recovery",
    "topo-state-document-hash-mismatch",
    "topo-state-link-compound-child-maps",
    "topo-state-mapper-history-events",
    "topo-state-object-hash-mismatch",
    "topo-state-reference-shadow-brep",
)

C4M6_EXPECTED_HARD_FAIL_FIXTURES = (
    ("topo-state-schema-incompatible", "topo_state_schema_incompatible"),
    ("topo-state-producer-incompatible", "topo_state_producer_incompatible"),
)


def expected_topo_state_frontend_contract_difference(actual: dict, expected: dict) -> str | None:
    if "topoNamingState" not in actual:
        return "topoNamingState: missing from actual response"

    actual_state = canonicalize_freecad_mapped_names_and_keys(actual["topoNamingState"])
    expected_state = canonicalize_freecad_mapped_names_and_keys(expected["topoNamingState"])
    actual_objects = actual_state.get("objects", {})
    expected_objects = expected_state.get("objects", {})
    if not isinstance(actual_objects, dict) or not isinstance(expected_objects, dict):
        return "topoNamingState.objects: must be JSON objects"

    required_objects = expected_frontend_required_topo_objects(expected)
    for object_name, expected_object in expected_objects.items():
        if object_name not in actual_objects:
            if object_name not in required_objects:
                continue
            return f"topoNamingState.objects.{object_name}: missing from actual response"
        actual_object = actual_objects[object_name]
        if not isinstance(actual_object, dict) or not isinstance(expected_object, dict):
            return f"topoNamingState.objects.{object_name}: must be JSON objects"

        diff = expected_subshape_state_difference(actual_object, expected_object, object_name)
        if diff is not None:
            return diff
        diff = expected_element_map_entries_difference(actual_object, expected_object, object_name)
        if diff is not None:
            return diff

    return expected_response_subshape_identity_difference(actual, expected)


def expected_frontend_required_topo_objects(expected: dict) -> set[str]:
    required: set[str] = set()
    state = expected.get("topoNamingState")
    objects = state.get("objects") if isinstance(state, dict) else {}
    object_names = set(objects) if isinstance(objects, dict) else set()

    for result in expected.get("results") or []:
        object_name = result.get("object") if isinstance(result, dict) else None
        if isinstance(object_name, str) and object_name in object_names:
            required.add(object_name)

    for update in expected.get("elementReferenceUpdates") or []:
        for item in reference_update_items(update):
            stable_sub_list = item.get("StableSubList")
            object_name = item.get("value")
            if (
                isinstance(stable_sub_list, list)
                and stable_sub_list
                and isinstance(object_name, str)
                and object_name in object_names
            ):
                required.add(object_name)

    return required


def reference_update_items(update: object) -> list[dict]:
    if not isinstance(update, dict):
        return []
    property_type = update.get("PropertyType")
    if property_type in {
        "App::PropertyLinkSub",
        "App::PropertyLinkSubHidden",
        "App::PropertyXLinkSub",
        "App::PropertyXLinkSubHidden",
    }:
        return [update]
    if property_type in {
        "App::PropertyLinkSubList",
        "App::PropertyLinkSubListHidden",
        "App::PropertyXLinkSubList",
    }:
        sub_set = update.get("SubSet")
        if isinstance(sub_set, list):
            return [item for item in sub_set if isinstance(item, dict)]
    return []


def expected_subshape_state_difference(
    actual_object: dict,
    expected_object: dict,
    object_name: str,
) -> str | None:
    actual_subshapes = actual_object.get("subshapes", {})
    expected_subshapes = expected_object.get("subshapes", {})
    if not isinstance(actual_subshapes, dict) or not isinstance(expected_subshapes, dict):
        return f"topoNamingState.objects.{object_name}.subshapes: must be JSON objects"

    for indexed, expected_subshape in expected_subshapes.items():
        actual_subshape = actual_subshapes.get(indexed)
        if actual_subshape is None:
            return f"topoNamingState.objects.{object_name}.subshapes.{indexed}: missing from actual response"
        if not isinstance(actual_subshape, dict) or not isinstance(expected_subshape, dict):
            return f"topoNamingState.objects.{object_name}.subshapes.{indexed}: must be JSON objects"
    return None


def expected_element_map_entries_difference(
    actual_object: dict,
    expected_object: dict,
    object_name: str,
) -> str | None:
    actual_entries = ((actual_object.get("elementMap") or {}).get("entries") or {})
    expected_entries = ((expected_object.get("elementMap") or {}).get("entries") or {})
    if not isinstance(actual_entries, dict) or not isinstance(expected_entries, dict):
        return f"topoNamingState.objects.{object_name}.elementMap.entries: must be JSON objects"

    for token, expected_entry in expected_entries.items():
        actual_entry = actual_entries.get(token)
        if actual_entry is None:
            return (
                f"topoNamingState.objects.{object_name}.elementMap.entries.{token}: "
                "missing from actual response"
            )
        if not isinstance(actual_entry, dict) or not isinstance(expected_entry, dict):
            return (
                f"topoNamingState.objects.{object_name}.elementMap.entries.{token}: "
                "must be JSON objects"
            )
        for field in ("target", "source", "shapeKind", "mappedName"):
            if field in expected_entry and actual_entry.get(field) != expected_entry[field]:
                return (
                    f"topoNamingState.objects.{object_name}.elementMap.entries.{token}.{field}: "
                    f"value differs: actual={actual_entry.get(field)!r} expected={expected_entry[field]!r}"
                )
    return None


def expected_response_subshape_identity_difference(actual: dict, expected: dict) -> str | None:
    actual_index = response_subshape_identity_index(actual)
    expected_index = response_subshape_identity_index(expected)
    for subshape_key, expected_identity in expected_index.items():
        actual_identity = actual_index.get(subshape_key)
        if actual_identity is None:
            return f"results.subshapes.identity.{subshape_key}: missing from actual response"
        for field, expected_value in expected_identity.items():
            actual_value = actual_identity.get(field)
            if actual_value != expected_value:
                return (
                    f"results.subshapes.identity.{subshape_key}.{field}: "
                    f"value differs: actual={actual_value!r} expected={expected_value!r}"
                )
    return None


class TopoNamingStateResponseTest(unittest.TestCase):
    def fixture_payload(self, group: str, fixture: str) -> dict:
        path = ROOT / "fixtures" / group / f"{fixture}.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def expected_payload(self, group: str, fixture: str) -> dict:
        path = ROOT / "fixtures" / group / "expected" / f"{fixture}.freecad.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def run_official_recompute_payload(self, payload: bytes | dict) -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            input_path = tmp_path / "request.json"
            output_path = tmp_path / "result.json"
            input_path.write_bytes(payload)
            env = os.environ.copy()
            env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def run_official_recompute_fixture(self, group: str, fixture: str) -> dict:
        return self.run_official_recompute_payload(self.fixture_payload(group, fixture))

    def run_legacy_recompute_fixture(self, group: str, fixture: str) -> dict:
        payload = json.dumps(self.fixture_payload(group, fixture)).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            input_path = tmp_path / "request.json"
            output_path = tmp_path / "result.json"
            input_path.write_bytes(payload)
            env = os.environ.copy()
            env["CAD_CORE_TEST_LEGACY_OUTPUT"] = "1"
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def assert_topo_naming_state_matches_freecad_expected(self, group: str, fixture: str) -> None:
        response = self.run_official_recompute_fixture(group, fixture)
        expected = self.expected_payload(group, fixture)
        diff = expected_topo_state_frontend_contract_difference(response, expected)
        self.assertIsNone(diff, f"{group}/{fixture}: {diff}")

    def assert_p2_consumer_topo_state_smoke(self) -> None:
        response = self.run_official_recompute_fixture("p2", "rect-pad-pocket")

        self.assertEqual(response["diagnostics"], [])
        self.assertEqual([item["object"] for item in response["results"]], ["Body"])
        state = response["topoNamingState"]["objects"]
        for object_name in ("Body", "Pad", "Pocket", "SketchPad", "SketchPocket"):
            with self.subTest(object=object_name):
                self.assertIn(object_name, state)
                object_state = state[object_name]
                self.assertIsInstance(object_state.get("subshapes"), dict)
                self.assertGreater(len(object_state["subshapes"]), 0)
                self.assertEqual(
                    object_state.get("elementMap", {}).get("encoding"),
                    "cad-core.element-map.v1",
                )
        for object_name in ("Body", "Pad", "Pocket"):
            with self.subTest(element_map=object_name):
                entries = state[object_name]["elementMap"]["entries"]
                self.assertGreater(len(entries), 0)
                self.assertTrue(
                    any(
                        isinstance(entry, dict)
                        and entry.get("evidence", {}).get("source")
                        in {
                            "child_element_map",
                            "element_map",
                            "freecad_expected_collector",
                            "freecad_partdesign_body_tip",
                            "mapper_history",
                        }
                        for entry in entries.values()
                    )
                )

    def assert_topo_state_hard_fail(
        self,
        response: dict,
        code: str,
    ) -> None:
        self.assertNotIn("topoNamingState", response)
        self.assertEqual(response["results"], [])
        self.assertEqual(response["elementReferenceUpdates"], [])
        self.assertEqual(len(response["diagnostics"]), 1)
        self.assertEqual(response["diagnostics"][0]["severity"], "error")
        self.assertEqual(response["diagnostics"][0]["code"], code)

    def assert_source_backed_producer_evidence(self, response: dict, object_name: str) -> None:
        named_shape = response["named_shapes"][object_name]
        provenance = named_shape["mapped_name_provenance"]
        source_backed = {
            entry_key: entry
            for entry_key, entry in provenance.items()
            if entry["status"] == "source_backed"
        }

        self.assertGreater(len(source_backed), 0)
        for entry_key, entry in source_backed.items():
            with self.subTest(object=object_name, entry=entry_key):
                self.assertEqual(entry["entry_key"], entry_key)
                self.assertIn(entry_key, named_shape["element_map"])
                self.assertIn(entry["current_element"], named_shape["elements"])
                self.assertNotEqual(entry["source_element"], "")
                self.assertIn(entry["element_type"], {"Face", "Edge", "Vertex"})
                self.assertIsInstance(entry["producer_tag"], int)
                self.assertIsInstance(entry["master_tag"], int)
                self.assertIsInstance(entry["source_tag"], int)
                self.assertNotEqual(entry["raw_mapped_name"], "")
                self.assertNotEqual(entry["canonical_mapped_name"], "")
                self.assertIn(";:H", entry["raw_mapped_name"])
                self.assertIn(":H*", entry["canonical_mapped_name"])

    def test_c13m1_official_cli_response_keeps_p2_topo_state_consumer_smoke(self) -> None:
        self.assert_p2_consumer_topo_state_smoke()

    def test_c13m1_response_topo_state_round_trips_without_body_tip_recovery_regression(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-body-tip-stable-recovery")

        first_response = self.run_official_recompute_payload(payload)
        self.assertIn("topoNamingState", first_response)

        round_trip_payload = copy.deepcopy(payload)
        round_trip_payload["topoNamingState"] = first_response["topoNamingState"]
        second_response = self.run_official_recompute_payload(round_trip_payload)
        body = next(item for item in second_response["results"] if item["object"] == "Body")
        edge_subshapes = [item for item in body["subshapes"] if item["kind"] == "Edge"]

        self.assertEqual(second_response["diagnostics"], [])
        self.assertGreater(len(edge_subshapes), 0)
        for edge in edge_subshapes:
            self.assertEqual(edge["identityStatus"], "stable")
            self.assertNotEqual(edge["stableSubname"], "")

    def test_c13m3_s3_partdesign_producer_evidence_exists_for_focused_paths(self) -> None:
        for group, fixture, object_name in C13M3_PRODUCER_EVIDENCE_CASES:
            with self.subTest(fixture=f"{group}/{fixture}", object=object_name):
                response = self.run_legacy_recompute_fixture(group, fixture)
                self.assert_source_backed_producer_evidence(response, object_name)

    def test_c13m2_p2_topo_state_keeps_consumer_smoke(self) -> None:
        self.assert_p2_consumer_topo_state_smoke()

    def test_c13m2_c4m6_topo_state_matches_freecad_expected(self) -> None:
        self.assert_topo_naming_state_matches_freecad_expected(
            "c4m6",
            "topo-state-body-tip-stable-recovery",
        )

    def test_c13m2_p5_topo_state_matches_freecad_expected(self) -> None:
        self.assert_topo_naming_state_matches_freecad_expected("p5", "sketch-internal-face")

    def test_c13m2_p8_link_topo_state_keeps_indexed_only_expected_boundary(self) -> None:
        self.assert_topo_naming_state_matches_freecad_expected("p8", "app-link-box")

    def test_c4m6_success_response_matches_freecad_expected_topo_state(self) -> None:
        for fixture in C4M6_TOPO_STATE_PARITY_FIXTURES:
            with self.subTest(fixture=fixture):
                self.assert_topo_naming_state_matches_freecad_expected("c4m6", fixture)

    def test_c4m6_reference_shadow_response_keeps_expected_update_contract(self) -> None:
        reference_response = self.run_official_recompute_fixture(
            "c4m6",
            "topo-state-reference-shadow-brep",
        )
        self.assertEqual(reference_response["diagnostics"], [])
        self.assertEqual(len(reference_response["elementReferenceUpdates"]), 1)
        reference_item = reference_response["elementReferenceUpdates"][0]["SubSet"][0]
        self.assertEqual(reference_item["StableSubList"], ["Pad.#d:4;:G;XTR;:H*:*,F"])
        self.assertEqual(reference_item["ShadowSub"][0]["newName"], "Pad.#d:4;:G;XTR;:H*:*,F")
        self.assertEqual(
            reference_item["ReferenceShadow"][0]["stableSubname"],
            "Pad.#d:4;:G;XTR;:H*:*,F",
        )

    def test_c4m6_expected_schema_and_producer_failures_do_not_publish_topo_state(self) -> None:
        for fixture, code in C4M6_EXPECTED_HARD_FAIL_FIXTURES:
            with self.subTest(fixture=fixture):
                response = self.run_official_recompute_fixture("c4m6", fixture)

                self.assert_topo_state_hard_fail(response, code)

    def test_c4m6_document_and_object_hash_mismatch_recompute_with_topo_state(self) -> None:
        for fixture in (
            "topo-state-document-hash-mismatch",
            "topo-state-object-hash-mismatch",
        ):
            with self.subTest(fixture=fixture):
                response = self.run_official_recompute_fixture("c4m6", fixture)

                self.assertEqual(response["diagnostics"], [])
                self.assertIn("topoNamingState", response)
                self.assertGreater(len(response["results"]), 0)
                self.assert_topo_naming_state_matches_freecad_expected("c4m6", fixture)

    def test_c4m6_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-first-recompute-empty")
        payload["topoNamingState"]["objects"] = {
            "Box": {
                "objectHash": self.expected_payload(
                    "c4m6", "topo-state-first-recompute-empty"
                )["topoNamingState"]["objects"]["Box"]["objectHash"],
                "elementMapVersion": "cad-core.element-map.v1",
                "subshapes": {},
                "elementMap": {
                    "encoding": "cad-core.element-map.v0",
                    "status": "indexed_only",
                    "entries": {},
                },
                "childElementMaps": [],
                "mapperHistory": [],
            }
        }

        response = self.run_official_recompute_payload(payload)

        self.assert_topo_state_hard_fail(
            response,
            "topo_state_element_map_encoding_incompatible",
        )

    def test_c4m6_child_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("c4m6", "topo-state-body-tip-stable-recovery")
        first_response = self.run_official_recompute_payload(payload)
        payload["topoNamingState"] = first_response["topoNamingState"]
        body = payload["topoNamingState"]["objects"]["Body"]
        body["childElementMaps"][0]["elementMap"]["encoding"] = "cad-core.element-map.v0"

        response = self.run_official_recompute_payload(payload)

        self.assert_topo_state_hard_fail(
            response,
            "topo_state_element_map_encoding_incompatible",
        )

    def test_c4m6_child_map_and_mapper_history_match_freecad_expected(self) -> None:
        for fixture in (
            "topo-state-body-tip-stable-recovery",
            "topo-state-link-compound-child-maps",
            "topo-state-mapper-history-events",
        ):
            with self.subTest(fixture=fixture):
                self.assert_topo_naming_state_matches_freecad_expected("c4m6", fixture)


if __name__ == "__main__":
    unittest.main()
