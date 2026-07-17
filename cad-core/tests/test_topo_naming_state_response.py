from __future__ import annotations

import copy
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

try:
    from .fixture_runner import BIN, ROOT, semantic_fixture_path
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BIN, ROOT, semantic_fixture_path


TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import EvaluationRequest, evaluate


C13M3_PRODUCER_EVIDENCE_CASES = (
    ("partdesign-extrude", "rect-pad-pocket", "Body"),
    ("topology-state", "topo-state-body-tip-stable-recovery", "Body"),
)

C4M6_TOPO_STATE_PARITY_FIXTURES = (
    "topo-state-first-recompute-empty",
    "topo-state-body-tip-stable-recovery",
    "topo-state-link-compound-child-maps",
    "topo-state-reference-shadow-brep",
)

C4M6_EXPECTED_HARD_FAIL_FIXTURES = (
    ("topo-state-schema-incompatible", "topo_state_schema_incompatible"),
    ("topo-state-producer-incompatible", "topo_state_producer_incompatible"),
    ("topo-state-document-hash-mismatch", "topo_state_document_hash_mismatch"),
    ("topo-state-object-hash-mismatch", "topo_state_object_hash_mismatch"),
    ("topo-state-foreign-object-owner", "topo_state_object_owner_incompatible"),
)

C4M6_TRANSPORT_MESH_CASES = (
    ("topo-state-body-tip-stable-recovery", "Body"),
    ("topo-state-first-recompute-empty", "Box"),
    ("topo-state-link-compound-child-maps", "CompoundLink"),
    ("topo-state-reference-shadow-brep", "ProbeSketch"),
)

C4M6_MESH_KEYS = {
    "vertices",
    "normals",
    "indices",
    "faceIds",
    "edgeSegments",
    "vertexPoints",
}


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


def nested_field_paths(
    value: object,
    field: str,
    path: tuple[str | int, ...] = (),
) -> list[tuple[str | int, ...]]:
    paths: list[tuple[str | int, ...]] = []
    if isinstance(value, dict):
        for key, child in value.items():
            child_path = path + (str(key),)
            if key == field:
                paths.append(child_path)
            paths.extend(nested_field_paths(child, field, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            paths.extend(nested_field_paths(child, field, path + (index,)))
    return paths


def canonical_collision_fingerprint(mapper_history: object) -> tuple[tuple[object, ...], ...]:
    if not isinstance(mapper_history, list):
        return ()
    fingerprint: list[tuple[object, ...]] = []
    for event in mapper_history:
        if not isinstance(event, dict) or event.get("relation") != "ambiguous":
            continue
        mapped_name = event.get("mappedName", {})
        candidates = event.get("candidates", [])
        candidate_fingerprint = []
        if isinstance(candidates, list):
            for candidate in candidates:
                if not isinstance(candidate, dict):
                    continue
                source = candidate.get("source", {})
                target = candidate.get("target", {})
                candidate_mapped_name = candidate.get("mappedName", {})
                candidate_fingerprint.append(
                    (
                        source.get("object", "") if isinstance(source, dict) else "",
                        source.get("subname", "") if isinstance(source, dict) else "",
                        target.get("object", "") if isinstance(target, dict) else "",
                        target.get("subname", "") if isinstance(target, dict) else "",
                        candidate.get("shapeKind", ""),
                        candidate_mapped_name.get("canonical", "")
                        if isinstance(candidate_mapped_name, dict)
                        else "",
                        candidate.get("recoverability", ""),
                    )
                )
        fingerprint.append(
            (
                event.get("id", ""),
                event.get("source", ""),
                mapped_name.get("canonical", "") if isinstance(mapped_name, dict) else "",
                event.get("recoverability", ""),
                tuple(sorted(candidate_fingerprint)),
            )
        )
    return tuple(sorted(fingerprint))


class TopoNamingStateResponseTest(unittest.TestCase):
    def fixture_payload(self, group: str | None, fixture: str) -> dict:
        path = semantic_fixture_path(fixture, group)
        return json.loads(path.read_text(encoding="utf-8"))

    def expected_payload(self, group: str | None, fixture: str) -> dict:
        path = semantic_fixture_path(fixture, group).parent / "expected" / f"{fixture}.freecad.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def protocol_expected_payload(self, group: str | None, fixture: str) -> dict:
        path = semantic_fixture_path(fixture, group).parent / "expected" / f"{fixture}.expeted.json"
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

    def run_official_recompute_fixture(self, group: str | None, fixture: str) -> dict:
        return self.run_official_recompute_payload(self.fixture_payload(group, fixture))

    def run_legacy_recompute_payload(self, payload: bytes | dict) -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode("utf-8")
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

    def run_legacy_recompute_fixture(self, group: str, fixture: str) -> dict:
        return self.run_legacy_recompute_payload(self.fixture_payload(group, fixture))

    def assert_c4m6_native_parity_gate(self, fixture: str) -> None:
        response = self.run_official_recompute_fixture("topology-state", fixture)
        report = evaluate(
            EvaluationRequest(
                root=ROOT,
                phase="topology-state",
                case=fixture,
                source_kind="in_memory",
                in_memory_actuals={("topology-state", fixture): response},
            )
        ).to_dict()
        self.assertTrue(report["preflight"]["valid"], report["preflight"]["errors"])
        self.assertEqual(1, report["summary"]["cases"])
        self.assertEqual("red", report["exactStatus"])
        self.assertEqual("green", report["semanticStatus"])
        self.assertEqual("not_evaluated", report["releaseStatus"])

    def assert_p2_consumer_topo_state_smoke(self) -> None:
        response = self.run_official_recompute_fixture("partdesign-extrude", "rect-pad-pocket")

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
        expected: dict | None = None,
    ) -> None:
        self.assertEqual(
            set(response),
            {"diagnostics", "elementReferenceUpdates", "results"},
        )
        self.assertEqual(response["results"], [])
        self.assertEqual(response["elementReferenceUpdates"], [])
        self.assertEqual(len(response["diagnostics"]), 1)
        self.assertEqual(response["diagnostics"][0]["severity"], "error")
        self.assertEqual(response["diagnostics"][0]["code"], code)
        if expected is not None:
            self.assertEqual(response, expected)

    @staticmethod
    def result_for_object(payload: dict, object_name: str) -> dict:
        return next(
            item
            for item in payload.get("results", [])
            if isinstance(item, dict) and item.get("object") == object_name
        )

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
        payload = self.fixture_payload("topology-state", "topo-state-body-tip-stable-recovery")

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
        self.assert_c4m6_native_parity_gate("topo-state-body-tip-stable-recovery")

    def test_c4m6_success_response_matches_freecad_expected_topo_state(self) -> None:
        for fixture in C4M6_TOPO_STATE_PARITY_FIXTURES:
            with self.subTest(fixture=fixture):
                self.assert_c4m6_native_parity_gate(fixture)

    def test_c4m6_reference_shadow_response_keeps_expected_update_contract(self) -> None:
        reference_response = self.run_official_recompute_fixture(
            "topology-state",
            "topo-state-reference-shadow-brep",
        )
        expected = self.expected_payload("topology-state", "topo-state-reference-shadow-brep")
        self.assertEqual(reference_response["diagnostics"], [])
        self.assertEqual(
            reference_response["elementReferenceUpdates"],
            expected["elementReferenceUpdates"],
        )
        self.assertEqual(len(reference_response["elementReferenceUpdates"]), 1)

        for update in reference_response["elementReferenceUpdates"]:
            for item in reference_update_items(update):
                sub_list = item.get("SubList", [])
                stable_sub_list = item.get("StableSubList", [])
                self.assertIsInstance(sub_list, list)
                self.assertIsInstance(stable_sub_list, list)
                cardinality = len(sub_list) if sub_list else len(stable_sub_list)
                self.assertGreater(cardinality, 0)
                for field in ("StableSubList", "FullSubList", "ShadowSub", "ReferenceShadow"):
                    if field in item:
                        self.assertEqual(len(item[field]), cardinality, field)
                for index in range(cardinality):
                    if sub_list:
                        self.assertEqual(item["ShadowSub"][index]["oldName"], sub_list[index])
                        self.assertEqual(item["ReferenceShadow"][index]["subname"], sub_list[index])
                    self.assertEqual(item["ShadowSub"][index]["newName"], stable_sub_list[index])
                    self.assertEqual(
                        item["ReferenceShadow"][index]["stableSubname"],
                        stable_sub_list[index],
                    )
                    self.assertEqual(item["ReferenceShadow"][index]["target"], item["value"])

        self.assertEqual(
            nested_field_paths(reference_response, "brep"),
            [("elementReferenceUpdates", 0, "SubSet", 0, "ReferenceShadow", 0, "brep")],
        )
        self.assertEqual(nested_field_paths(reference_response["topoNamingState"], "brep"), [])
        self.assertEqual(nested_field_paths(reference_response["results"], "brep"), [])

    def test_c4m6_expected_request_failures_have_exact_diagnostics_only_envelopes(self) -> None:
        for fixture, code in C4M6_EXPECTED_HARD_FAIL_FIXTURES:
            with self.subTest(fixture=fixture):
                response = self.run_official_recompute_fixture(None, fixture)

                self.assert_topo_state_hard_fail(
                    response,
                    code,
                    self.expected_payload(None, fixture),
                )

    def test_c4m6_compound_link_native_semantic_result_matches_freecad_expected(self) -> None:
        response = self.run_official_recompute_fixture(
            "topology-state",
            "topo-state-link-compound-child-maps",
        )
        expected = self.expected_payload(
            "topology-state",
            "topo-state-link-compound-child-maps",
        )
        actual_result = self.result_for_object(response, "CompoundLink")
        expected_result = self.result_for_object(expected, "CompoundLink")

        for field in ("bbox", "volume", "topology_counts"):
            with self.subTest(field=field):
                self.assertEqual(actual_result[field], expected_result[field])
        actual_subshapes = {item["indexed"]: item for item in actual_result["subshapes"]}
        expected_subshapes = {item["indexed"]: item for item in expected_result["subshapes"]}
        self.assertEqual(actual_subshapes, expected_subshapes)

    def test_c4m6_compound_child_map_part_ledger_materializes_collisions_without_reference(
        self,
    ) -> None:
        payload = self.fixture_payload("topology-state", "topo-state-link-compound-child-maps")
        payload["Objects"] = [
            object_payload
            for object_payload in payload["Objects"]
            if object_payload["Name"] != "CompoundLink"
        ]
        payload.pop("topoNamingState")
        payload["recompute"] = {"objs": ["Compound"]}

        first = self.run_legacy_recompute_payload(payload)
        second = self.run_legacy_recompute_payload(payload)
        first_events = [
            event
            for event in first["named_shapes"]["Compound"]["mapper_history"]
            if event.get("relation") == "ambiguous"
        ]
        second_events = [
            event
            for event in second["named_shapes"]["Compound"]["mapper_history"]
            if event.get("relation") == "ambiguous"
        ]

        self.assertEqual(first_events, second_events)
        self.assertEqual(len(first_events), 26)
        self.assertTrue(all(event.get("source") == "part_element_map" for event in first_events))
        self.assertFalse(
            any(event.get("source") == "freecad_expected_collector" for event in first_events)
        )

        legacy_fingerprint = canonical_collision_fingerprint(first_events)
        direct_response = self.run_official_recompute_payload(payload)
        direct_compound_state = direct_response["topoNamingState"]["objects"]["Compound"]
        self.assertEqual(
            canonical_collision_fingerprint(direct_compound_state["mapperHistory"]),
            legacy_fingerprint,
        )

        actual_targets_by_kind: dict[str, set[frozenset[str]]] = {
            "edge": set(),
            "face": set(),
            "vertex": set(),
        }
        for event in first_events:
            candidates = event["candidates"]
            shape_kinds = {candidate["shapeKind"] for candidate in candidates}
            self.assertEqual(len(shape_kinds), 1)
            shape_kind = shape_kinds.pop()
            targets = frozenset(candidate["target"]["subname"] for candidate in candidates)
            self.assertEqual(len(targets), 2)
            actual_targets_by_kind[shape_kind].add(targets)

        self.assertEqual(
            actual_targets_by_kind["edge"],
            {frozenset({f"Edge{index}", f"Edge{index + 12}"}) for index in range(1, 13)},
        )
        self.assertEqual(
            actual_targets_by_kind["face"],
            {frozenset({f"Face{index}", f"Face{index + 6}"}) for index in range(1, 7)},
        )
        self.assertEqual(
            actual_targets_by_kind["vertex"],
            {frozenset({f"Vertex{index}", f"Vertex{index + 8}"}) for index in range(1, 9)},
        )
        collision_keys = {
            event["mappedName"]["canonical"]
            for event in first_events
        }
        self.assertTrue(
            collision_keys.isdisjoint(first["named_shapes"]["Compound"]["element_map"])
        )
        self.assertTrue(
            collision_keys.isdisjoint(direct_compound_state["elementMap"]["entries"])
        )

        link_response = self.run_official_recompute_fixture(
            "topology-state",
            "topo-state-link-compound-child-maps",
        )
        link_compound_state = link_response["topoNamingState"]["objects"]["Compound"]
        self.assertEqual(
            canonical_collision_fingerprint(link_compound_state["mapperHistory"]),
            legacy_fingerprint,
        )
        self.assertEqual(
            link_compound_state["subshapes"]["Child0.Face1"]["canonicalFreecadMappedName"],
            "Compound/ChildBoxA.#f:1;BOX,F",
        )
        self.assertIn("Compound:ChildBoxA:Child0", {
            child_map["key"] for child_map in link_compound_state["childElementMaps"]
        })

        round_trip_payload = self.fixture_payload("topology-state", "topo-state-link-compound-child-maps")
        round_trip_payload["topoNamingState"] = link_response["topoNamingState"]
        round_trip_response = self.run_official_recompute_payload(round_trip_payload)
        self.assertEqual(round_trip_response["diagnostics"], [])
        self.assertEqual(
            canonical_collision_fingerprint(
                round_trip_response["topoNamingState"]["objects"]["Compound"]["mapperHistory"]
            ),
            legacy_fingerprint,
        )

    def test_c4m6_transport_metadata_references_current_result_subshapes(self) -> None:
        for fixture, object_name in C4M6_TRANSPORT_MESH_CASES:
            with self.subTest(fixture=fixture, object=object_name):
                response = self.run_official_recompute_fixture(None, fixture)
                result = self.result_for_object(response, object_name)
                self.assertEqual(set(result["mesh"]), C4M6_MESH_KEYS)
                subshapes = {
                    item["indexed"]
                    for item in result.get("subshapes", [])
                    if isinstance(item, dict) and isinstance(item.get("indexed"), str)
                }
                for edge in result["mesh"]["edgeSegments"]:
                    if isinstance(edge, dict) and isinstance(edge.get("indexed"), str):
                        self.assertIn(edge["indexed"], subshapes)
                for vertex in result["mesh"]["vertexPoints"]:
                    if isinstance(vertex, dict) and isinstance(vertex.get("indexed"), str):
                        self.assertIn(vertex["indexed"], subshapes)

        reference = self.run_official_recompute_fixture(
            "topology-state", "topo-state-reference-shadow-brep"
        )
        reference_result = self.result_for_object(reference, "ProbeSketch")
        self.assertEqual(
            {item["indexed"] for item in reference_result["subshapes"]},
            {"Edge1", "Vertex1", "Vertex2"},
        )

    def test_c4m6_history_probe_uses_protocol_only_contract_not_native_geometry_oracle(self) -> None:
        response = self.run_official_recompute_fixture(
            "topology-state",
            "topo-state-mapper-history-events",
        )
        expected = self.protocol_expected_payload(
            "topology-state",
            "topo-state-mapper-history-events",
        )

        evidence = expected.get("oracle_evidence")
        self.assertIsInstance(evidence, dict)
        self.assertFalse(evidence.get("freecad_native_parity"))
        self.assertEqual(response["diagnostics"], expected["diagnostics"])
        self.assertEqual(response["topoNamingState"], expected["topoNamingState"])
        self.assertEqual(
            [item["object"] for item in response["results"]],
            ["HistoryProbe"],
        )

    def test_c4m6_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("topology-state", "topo-state-first-recompute-empty")
        payload["topoNamingState"]["objects"] = {
            "Box": {
                "objectHash": self.expected_payload(
                    "topology-state", "topo-state-first-recompute-empty"
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

    def test_c4m6_malformed_topo_state_objects_hard_fails_before_recompute(self) -> None:
        for label, objects in (("array", []), ("null", None), ("missing", None)):
            with self.subTest(objects=label):
                payload = self.fixture_payload("topology-state", "topo-state-first-recompute-empty")
                if label == "missing":
                    payload["topoNamingState"].pop("objects")
                else:
                    payload["topoNamingState"]["objects"] = objects

                response = self.run_official_recompute_payload(payload)

                self.assert_topo_state_hard_fail(response, "topo_state_schema_incompatible")
                self.assertEqual(response["diagnostics"][0]["actualObjects"], objects)
                self.assertEqual(response["diagnostics"][0]["expectedObjectsType"], "object")

    def test_c4m6_non_object_topo_state_hard_fails_before_recompute(self) -> None:
        payload = self.fixture_payload("topology-state", "topo-state-first-recompute-empty")
        payload["topoNamingState"] = []

        response = self.run_official_recompute_payload(payload)

        self.assert_topo_state_hard_fail(response, "topo_state_schema_incompatible")
        self.assertEqual(response["diagnostics"][0]["actualTopoNamingState"], [])
        self.assertEqual(response["diagnostics"][0]["expectedTopoNamingStateType"], "object_or_null")

    def test_c4m6_null_topo_state_is_an_explicit_no_state_request(self) -> None:
        payload = self.fixture_payload("topology-state", "topo-state-first-recompute-empty")
        payload["topoNamingState"] = None

        response = self.run_official_recompute_payload(payload)

        self.assertEqual(response["diagnostics"], [])
        self.assertEqual([item["object"] for item in response["results"]], ["Box"])
        self.assertIn("topoNamingState", response)

    def test_c4m6_child_element_map_encoding_mismatch_hard_fails_without_topo_state(self) -> None:
        payload = self.fixture_payload("topology-state", "topo-state-body-tip-stable-recovery")
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
        ):
            with self.subTest(fixture=fixture):
                self.assert_c4m6_native_parity_gate(fixture)


if __name__ == "__main__":
    unittest.main()
