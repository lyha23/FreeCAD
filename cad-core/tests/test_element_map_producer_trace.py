"""Focused closure, mutation, determinism, and isolation tests for CAD Core producer traces."""

from __future__ import annotations

import copy
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.element_map_producer_trace import (
    TraceValidationError,
    canonical_json_sha256,
    validate_trace,
)


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "build" / "cad-core"
FIXTURE = ROOT / "fixtures" / "p2" / "rect-pad-pocket.json"


class ElementMapProducerTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not CLI.exists():
            raise unittest.SkipTest("build/cad-core is required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        cls.output = cls.directory / "case.cad-core.json"
        subprocess.run(
            [str(CLI), "recompute", str(FIXTURE), "--output", str(cls.output)],
            check=True,
            cwd=ROOT,
        )
        cls.trace_path = cls.directory / "case.cad-core.producer-trace.json"
        cls.request = json.loads(FIXTURE.read_text(encoding="utf-8"))
        cls.response = json.loads(cls.output.read_text(encoding="utf-8"))
        cls.trace = validate_trace(
            cls.trace_path,
            input_document=cls.request,
            response_document=cls.response,
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def assert_mutation_fails(self, mutate) -> None:
        malformed = copy.deepcopy(self.trace)
        mutate(malformed)
        with self.assertRaises(TraceValidationError):
            validate_trace(malformed)

    def rehash_snapshot(self, trace: dict, group: str, old_id: str) -> str:
        snapshot = trace[group].pop(old_id)
        digest = canonical_json_sha256(snapshot["payload"])
        new_id = f"{snapshot['kind']}:sha256:{digest}"
        snapshot["sha256"] = digest
        trace[group][new_id] = snapshot
        for event in trace["events"]:
            if event["beforeSnapshot"] == old_id:
                event["beforeSnapshot"] = new_id
            if event["afterSnapshot"] == old_id:
                event["afterSnapshot"] = new_id
            for key, value in list(event["fields"].items()):
                if value == old_id:
                    event["fields"][key] = new_id
        for snapshot_group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for nested in trace[snapshot_group].values():
                nested["nestedSnapshotRefs"] = [
                    new_id if value == old_id else value
                    for value in nested.get("nestedSnapshotRefs", [])
                ]
                payload = nested.get("payload")
                if isinstance(payload, dict):
                    for key, value in list(payload.items()):
                        if value == old_id:
                            payload[key] = new_id
        return new_id

    def test_sequence_parent_ranges_and_snapshots_close(self) -> None:
        self.assertGreater(len(self.trace["events"]), 100)
        self.assertEqual(self.trace["transactions"][0]["eventRange"], [1, len(self.trace["events"])])
        slices = {event["slice"] for event in self.trace["events"]}
        self.assertTrue(
            {
                "mapper.snapshot",
                "maker.preserve.checkpoint",
                "maker.final_checkpoint",
                "property_shape.set_value",
                "partdesign.body_tip",
            }.issubset(slices)
        )

    def test_sid_table_entry_refs_child_ranges_and_mapper_order(self) -> None:
        final_table = max(
            self.trace["stringTableSnapshots"].values(),
            key=lambda snapshot: snapshot["payload"].get("lastId", 0),
        )["payload"]
        entries = final_table["entries"]
        self.assertEqual(list(range(1, final_table["lastId"] + 1)), [entry["value"] for entry in entries])
        for entry in entries:
            for related in entry["related"]:
                self.assertLessEqual(related["value"], entry["value"])

        find_all = [event for event in self.trace["events"] if event["slice"] == "element_map.find_all"]
        self.assertTrue(find_all)
        for event in find_all:
            for entry in event["fields"]["orderedEntries"]:
                self.assertIn("rawMappedName", entry)
                self.assertIsInstance(entry["elementIdRefs"], list)

        child_ranges = [
            child
            for snapshot in self.trace["ledgerSnapshots"].values()
            for child in snapshot["payload"].get("childRanges", [])
        ]
        self.assertTrue(child_ranges)
        for child in child_ranges:
            self.assertGreaterEqual(child["count"], 0)
            self.assertGreaterEqual(child["offset"], 0)
            self.assertIn(child["kind"], {"vertex", "edge", "face"})

        maker_scopes = {
            event["scopeSequence"]
            for event in self.trace["events"]
            if event["slice"] == "maker.begin"
        }
        for scope in maker_scopes:
            scoped = [
                event for event in self.trace["events"] if event["scopeSequence"] == scope
            ]
            mapper_sequence = min(
                event["sequence"] for event in scoped if event["slice"] == "mapper.snapshot"
            )
            consumption = [
                event["sequence"]
                for event in scoped
                if event["slice"] in {"maker.modified", "maker.generated"}
            ]
            self.assertTrue(all(mapper_sequence < sequence for sequence in consumption))

    def test_body_tip_inherits_without_replaying_upstream_producers(self) -> None:
        body_begin = next(
            event
            for event in self.trace["events"]
            if event["slice"] == "scope.begin"
            and event["fields"].get("stage") == "Body::execute"
        )
        descendants = {body_begin["scopeSequence"]}
        changed = True
        while changed:
            changed = False
            for event in self.trace["events"]:
                if event["slice"] == "scope.begin" and event["parentScopeSequence"] in descendants:
                    if event["scopeSequence"] not in descendants:
                        descendants.add(event["scopeSequence"])
                        changed = True
        body_events = [
            event for event in self.trace["events"] if event["scopeSequence"] in descendants
        ]
        inherit = next(event for event in body_events if event["slice"] == "partdesign.body_tip")
        self.assertTrue(inherit["fields"]["inheritOnly"])
        self.assertFalse(inherit["fields"]["replayUpstreamProducers"])
        self.assertFalse(
            {"maker.begin", "partdesign.extrude", "partdesign.dressup", "partdesign.pattern"}
            & {event["slice"] for event in body_events}
        )

    def test_delete_event_hard_fails(self) -> None:
        self.assert_mutation_fails(lambda trace: trace["events"].pop(len(trace["events"]) // 2))

    def test_delete_snapshot_hard_fails(self) -> None:
        def mutate(trace):
            event = next(event for event in trace["events"] if event["afterSnapshot"] != event["beforeSnapshot"])
            snapshot_id = event["afterSnapshot"]
            for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
                if snapshot_id in trace[group]:
                    del trace[group][snapshot_id]
                    return
            self.fail("no referenced snapshot found")

        self.assert_mutation_fails(mutate)

    def test_delete_sid_hard_fails(self) -> None:
        def mutate(trace):
            referenced = None
            for group in ("ledgerSnapshots", "mapperSnapshots"):
                for snapshot in trace[group].values():
                    if snapshot["sidRefs"]:
                        referenced = snapshot["sidRefs"][0]["value"]
                        break
                if referenced:
                    break
            self.assertIsNotNone(referenced)
            for snapshot in trace["stringTableSnapshots"].values():
                snapshot["definedSids"] = [value for value in snapshot["definedSids"] if value != referenced]

        self.assert_mutation_fails(mutate)

    def test_delete_checkpoint_hard_fails(self) -> None:
        def mutate(trace):
            index = max(
                index
                for index, event in enumerate(trace["events"])
                if event["slice"] in {"maker.final_checkpoint", "document.recompute.checkpoint"}
            )
            trace["events"].pop(index)

        self.assert_mutation_fails(mutate)

    def test_table_checkpoint_cannot_substitute_for_final_checkpoint(self) -> None:
        def mutate(trace):
            for event in trace["events"]:
                if event["slice"] in {"maker.final_checkpoint", "document.recompute.checkpoint"}:
                    event["slice"] = "table_checkpoint"

        self.assert_mutation_fails(mutate)

    def test_unknown_event_sid_hard_fails(self) -> None:
        def mutate(trace):
            event = next(
                event
                for event in trace["events"]
                if event["fields"].get("orderedRelated")
            )
            event["fields"]["orderedRelated"][0]["value"] = 999999

        self.assert_mutation_fails(mutate)

    def test_unknown_element_map_write_sid_hard_fails(self) -> None:
        def mutate(trace):
            event = next(
                event
                for event in trace["events"]
                if event["slice"] == "element_map.write"
                and event["fields"].get("elementIdRefs")
            )
            event["fields"]["elementIdRefs"][0]["value"] = 999999

        self.assert_mutation_fails(mutate)

    def test_required_producer_scope_without_final_checkpoint_hard_fails(self) -> None:
        def mutate(trace):
            required_scopes = {
                event["scopeSequence"]
                for event in trace["events"]
                if event["slice"] == "scope.begin"
                and event["fields"].get("descriptor", {}).get("requiresFinalCheckpoint") is True
            }
            checkpoint = next(
                event
                for event in trace["events"]
                if event["scopeSequence"] in required_scopes
                and event["slice"] == "maker.final_checkpoint"
                and sum(
                    candidate["scopeSequence"] == event["scopeSequence"]
                    and candidate["slice"] == "maker.final_checkpoint"
                    for candidate in trace["events"]
                )
                == 1
            )
            checkpoint["slice"] = "checkpoint.removed"

        self.assert_mutation_fails(mutate)

    def test_child_range_inventory_overflow_hard_fails(self) -> None:
        def mutate(trace):
            snapshot_id, snapshot = next(
                (snapshot_id, snapshot)
                for snapshot_id, snapshot in trace["ledgerSnapshots"].items()
                if snapshot["payload"].get("childRanges")
            )
            snapshot["payload"]["childRanges"][0]["count"] = 999999
            self.rehash_snapshot(trace, "ledgerSnapshots", snapshot_id)

        self.assert_mutation_fails(mutate)

    def test_mapper_source_without_input_inventory_hard_fails(self) -> None:
        def mutate(trace):
            snapshot_id, snapshot = next(
                (snapshot_id, snapshot)
                for snapshot_id, snapshot in trace["mapperSnapshots"].items()
                if snapshot["payload"].get("raw", {}).get("inputs")
            )
            snapshot["payload"]["raw"]["inputs"].clear()
            self.rehash_snapshot(trace, "mapperSnapshots", snapshot_id)

        self.assert_mutation_fails(mutate)

    def test_child_range_without_nested_ledger_snapshot_hard_fails(self) -> None:
        def mutate(trace):
            snapshot_id, snapshot = next(
                (snapshot_id, snapshot)
                for snapshot_id, snapshot in trace["ledgerSnapshots"].items()
                if snapshot["payload"].get("childRanges")
            )
            snapshot["payload"]["childRanges"][0]["nestedSnapshot"] = None
            snapshot["payload"]["childRanges"][0]["nestedSnapshotStatus"] = "missing"
            self.rehash_snapshot(trace, "ledgerSnapshots", snapshot_id)

        self.assert_mutation_fails(mutate)

    def test_face_maker_names_used_and_combo_refs_are_published(self) -> None:
        success = next(
            event
            for event in self.trace["events"]
            if event["slice"] == "face_maker.lifecycle"
            and event["decision"] == "success"
        )
        self.assertTrue(success["fields"]["namesUsed"])
        self.assertTrue(success["fields"]["comboNames"])
        for combo in success["fields"]["comboNames"]:
            self.assertIn("rawName", combo)
            self.assertIsInstance(combo["orderedSourceRefs"], list)

    def test_upper_pass_guards_publish_stable_reasons(self) -> None:
        rejected = [
            event
            for event in self.trace["events"]
            if event["slice"] == "maker.upper" and event["decision"] == "rejected"
        ]
        self.assertTrue(rejected)
        self.assertTrue(all(event["reason"].startswith("upper_") for event in rejected))

    def test_delete_scope_end_hard_fails(self) -> None:
        def mutate(trace):
            index = next(index for index, event in enumerate(trace["events"]) if event["slice"] == "scope.end")
            trace["events"].pop(index)

        self.assert_mutation_fails(mutate)

    def test_artifact_binding_mismatch_hard_fails(self) -> None:
        malformed_response = copy.deepcopy(self.response)
        malformed_response["diagnostics"].append({"code": "mutation"})
        with self.assertRaises(TraceValidationError):
            validate_trace(
                self.trace,
                input_document=self.request,
                response_document=malformed_response,
            )

    def test_two_runs_are_deterministic_and_public_response_is_isolated(self) -> None:
        second_output = self.directory / "second.cad-core.json"
        subprocess.run(
            [str(CLI), "recompute", str(FIXTURE), "--output", str(second_output)],
            check=True,
            cwd=ROOT,
        )
        second_trace = json.loads(
            (self.directory / "second.cad-core.producer-trace.json").read_text(encoding="utf-8")
        )
        self.assertEqual(self.response, json.loads(second_output.read_text(encoding="utf-8")))
        first_projection = copy.deepcopy(self.trace)
        second_projection = copy.deepcopy(second_trace)
        first_projection["producer"]["document"] = "<input>"
        second_projection["producer"]["document"] = "<input>"
        self.assertEqual(first_projection, second_projection)
        serialized_response = json.dumps(self.response)
        self.assertNotIn("producerTrace", serialized_response)
        self.assertNotIn("producer-trace", serialized_response)


if __name__ == "__main__":
    unittest.main()
