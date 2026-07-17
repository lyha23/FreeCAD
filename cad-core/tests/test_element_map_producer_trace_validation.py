from __future__ import annotations

import copy
import unittest
from pathlib import Path

from tests.producer_trace_fixture import bind_trace, event, insert_events, producer_trace
from tools.element_map_producer_trace import (
    TraceValidationError,
    canonical_json_sha256,
    validate_trace,
)
from tools.element_map_producer_trace.validate import _validate_child_ranges_and_mapper


ROOT = Path(__file__).resolve().parents[1]


class ProducerTraceValidationTests(unittest.TestCase):
    def test_validates_cad_rs_and_checked_in_native_closure(self) -> None:
        validate_trace(producer_trace())
        validate_trace(
            ROOT
            / "fixtures/topology-state/expected/"
            "topo-state-body-tip-stable-recovery.freecad.producer-trace.json"
        )
        validate_trace(
            ROOT
            / "fixtures/sketcher-external-geometry/expected/"
            "sketch-external-circle-edge.freecad.producer-trace.json"
        )
        validate_trace(
            ROOT
            / "fixtures/topology-state/expected/"
            "topo-state-document-hash-mismatch.freecad.producer-trace.json"
        )

    def test_missing_scope_end_and_snapshot_hard_fail(self) -> None:
        missing_end = producer_trace()
        missing_end["events"].pop(4)
        with self.assertRaises(TraceValidationError):
            validate_trace(missing_end)

        missing_snapshot = producer_trace()
        missing_snapshot["ledgerSnapshots"].clear()
        with self.assertRaises(TraceValidationError):
            validate_trace(missing_snapshot)

    def test_response_binding_is_strict(self) -> None:
        request = {"request": True}
        response = {"response": True}
        trace = bind_trace(producer_trace(), request, response)
        validate_trace(trace, input_document=request, response_document=response)
        with self.assertRaises(TraceValidationError):
            validate_trace(trace, response_document={"different": True})

    def test_native_binding_requires_explicit_provenance_hashes(self) -> None:
        trace = producer_trace()
        trace["producer"] = {"document": "Native"}
        with self.assertRaisesRegex(TraceValidationError, "inputSha256"):
            validate_trace(trace, input_document={"request": True})

    def test_bound_native_snapshot_canonical_payload_hash_is_strict(self) -> None:
        request = {"request": True}
        response = {"response": True}
        trace = producer_trace()
        trace["producer"] = {
            "name": "FreeCAD",
            "document": "Native",
            "inputSha256": canonical_json_sha256(request),
            "responseSha256": canonical_json_sha256(response),
            "snapshotPayloadHashAlgorithm": "canonical-json-sha256-v1",
        }
        for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for snapshot in trace[group].values():
                payload = snapshot.get("payload") if "payload" in snapshot else snapshot.get("entries")
                snapshot["canonicalPayloadSha256"] = canonical_json_sha256(payload)
        validate_trace(trace, input_document=request, response_document=response)

        snapshot = next(iter(trace["ledgerSnapshots"].values()))
        snapshot["canonicalPayloadSha256"] = "0" * 64
        with self.assertRaisesRegex(TraceValidationError, "canonical payload hash mismatch"):
            validate_trace(trace, input_document=request, response_document=response)

    def test_bound_native_event_sid_timeline_is_strict(self) -> None:
        request = {"request": True}
        response = {"response": True}
        trace = producer_trace()
        trace["producer"] = {
            "name": "FreeCAD",
            "document": "Native",
            "inputSha256": canonical_json_sha256(request),
            "responseSha256": canonical_json_sha256(response),
            "snapshotPayloadHashAlgorithm": "canonical-json-sha256-v1",
        }
        for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for snapshot in trace[group].values():
                payload = snapshot.get("payload") if "payload" in snapshot else snapshot.get("entries")
                snapshot["canonicalPayloadSha256"] = canonical_json_sha256(payload)
        allocation = event(
            0,
            1,
            0,
            "hasher.insert",
            "allocated",
            "table_insert",
            trace["events"][0]["afterSnapshot"],
            fields={"id": "1", "data": "Face1", "postfix": ""},
        )
        insert_events(trace, 2, [allocation])
        trace["events"][3]["fields"]["entryLocalRefs"] = "#2:1"
        with self.assertRaisesRegex(TraceValidationError, "unknown native SID 2"):
            validate_trace(trace, input_document=request, response_document=response)

    def test_bound_native_event_sid_persists_across_recompute_boundary(self) -> None:
        request = {"request": True}
        response = {"response": True}
        trace = producer_trace()
        trace["producer"] = {
            "name": "FreeCAD",
            "document": "Native",
            "inputSha256": canonical_json_sha256(request),
            "responseSha256": canonical_json_sha256(response),
            "snapshotPayloadHashAlgorithm": "canonical-json-sha256-v1",
        }
        for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for snapshot in trace[group].values():
                payload = snapshot.get("payload") if "payload" in snapshot else snapshot.get("entries")
                snapshot["canonicalPayloadSha256"] = canonical_json_sha256(payload)
        allocation = event(
            0,
            0,
            0,
            "hasher.insert",
            "allocated",
            "fixture_setup",
            trace["events"][0]["afterSnapshot"],
            fields={"id": "1", "data": "Face1", "postfix": ""},
        )
        insert_events(trace, 1, [allocation])
        trace["events"][3]["fields"]["entryLocalRefs"] = "#1:1"
        validate_trace(trace, input_document=request, response_document=response)

    def test_mapper_not_in_output_is_closed_without_fabricated_target(self) -> None:
        snapshots = {
            "mapper:fixture": {
                "payload": {
                    "raw": {
                        "inputs": [
                            {
                                "sourceOrdinal": 0,
                                "inventory": {
                                    "indexed": {"Edge": [{"indexed": "Edge1"}]}
                                },
                            }
                        ],
                        "output": {"indexed": {"Edge": []}},
                        "sources": [
                            {
                                "sourceOrdinal": 0,
                                "sourceShapeType": "Edge",
                                "sourceIndexed": "Edge1",
                                "modified": [
                                    {
                                        "indexed": "",
                                        "relationStatus": "not_in_output",
                                        "outputMembers": [],
                                    }
                                ],
                                "generated": [],
                            }
                        ],
                    }
                }
            }
        }
        _validate_child_ranges_and_mapper(snapshots, strict_actual=True)

        snapshots["mapper:fixture"]["payload"]["raw"]["sources"][0]["modified"][0][
            "outputMembers"
        ] = [{"indexed": "Edge1"}]
        with self.assertRaisesRegex(TraceValidationError, "not_in_output relation has members"):
            _validate_child_ranges_and_mapper(snapshots, strict_actual=True)

    def test_native_allows_closed_empty_recompute_after_projection_gap(self) -> None:
        trace = producer_trace()
        trace["producer"] = {"document": "Native"}
        terminal_snapshot = trace["events"][-1]["afterSnapshot"]
        gap = event(
            7,
            0,
            0,
            "property_shape.get_value",
            "read",
            "post_recompute_projection",
            terminal_snapshot,
        )
        begin = event(
            8,
            0,
            0,
            "document.recompute.begin",
            "begin",
            "request_parsed",
            terminal_snapshot,
        )
        end = event(
            9,
            0,
            0,
            "document.recompute.end",
            "success",
            "recompute_finished",
            terminal_snapshot,
        )
        begin["transactionSequence"] = 2
        end["transactionSequence"] = 2
        trace["events"].extend([gap, begin, end])
        trace["transactions"].append(
            {
                "sequence": 2,
                "targets": [],
                "eventRange": [8, 9],
                "outcome": "success",
            }
        )

        validate_trace(trace)

        gap["transactionSequence"] = 0
        with self.assertRaisesRegex(TraceValidationError, "gap"):
            validate_trace(trace)

    def test_native_element_map_producer_still_requires_a_checkpoint(self) -> None:
        trace = producer_trace()
        trace["producer"] = {"document": "Native"}
        trace["events"][3]["slice"] = "maker.after"
        with self.assertRaisesRegex(TraceValidationError, "checkpoint"):
            validate_trace(trace)

    def test_event_sid_checkpoint_scope_and_decision_mutations_hard_fail(self) -> None:
        mutations = []

        sequence = producer_trace()
        sequence["events"][2]["sequence"] = 99
        mutations.append(sequence)

        sid = producer_trace()
        sid["events"][2]["fields"]["entryLocalRefs"] = [1]
        mutations.append(sid)

        checkpoint = producer_trace()
        checkpoint["events"][3]["slice"] = "maker.after"
        mutations.append(checkpoint)

        scope = producer_trace()
        scope["events"][2]["parentScopeSequence"] = 99
        mutations.append(scope)

        decision = producer_trace()
        decision["events"][2]["decision"] = None
        mutations.append(decision)

        for mutation in mutations:
            with self.subTest(slice=mutation["events"][2].get("slice")):
                with self.assertRaises(TraceValidationError):
                    validate_trace(mutation)

    def test_identity_lifecycle_one_to_many_hard_fails(self) -> None:
        trace = producer_trace()
        before = trace["events"][0]["afterSnapshot"]
        identities = []
        for identity in ("ledger:1", "ledger:2"):
            item = event(
                0,
                1,
                0,
                "trace.identity",
                "create",
                "first_seen_value_identity",
                before,
                fields={
                    "identity": identity,
                    "kind": "ledger",
                    "role": "Pad:Shape",
                    "relatedIdentity": "",
                },
            )
            identities.append(item)
        insert_events(trace, 2, identities)
        with self.assertRaisesRegex(TraceValidationError, "one-to-many"):
            validate_trace(trace)

    def test_object_tag_index_is_bidirectional_and_type_closed(self) -> None:
        wrong_type = producer_trace()
        wrong_type["objectTagIndex"]["2"]["typeId"] = "PartDesign::Pocket"
        with self.assertRaisesRegex(TraceValidationError, "typeId mismatch"):
            validate_trace(wrong_type)

        extra = producer_trace()
        extra["objectTagIndex"]["999"] = {
            "object": "Ghost",
            "typeId": "App::Feature",
        }
        with self.assertRaisesRegex(TraceValidationError, "tag sets differ"):
            validate_trace(extra)

    def test_transaction_targets_close_against_object_index(self) -> None:
        trace = producer_trace()
        trace["transactions"][0]["targets"] = ["Ghost"]
        with self.assertRaisesRegex(TraceValidationError, "absent from object index"):
            validate_trace(trace)

    def test_sid_namespace_restarts_for_each_transaction(self) -> None:
        trace = producer_trace()
        first_allocation = trace["events"][2]
        first_allocation.update(
            {
                "slice": "hasher.insert",
                "decision": "allocation",
                "reason": "string_id_allocated",
                "fields": {"related": [], "result": {"value": 1, "index": 0}},
            }
        )
        second_events = copy.deepcopy(trace["events"])
        for event in second_events:
            event["sequence"] += 6
            event["transactionSequence"] = 2
            if event["scopeSequence"]:
                event["scopeSequence"] = 2
            if event["parentScopeSequence"]:
                event["parentScopeSequence"] = 2
        trace["events"].extend(second_events)
        trace["transactions"].append(
            {
                "sequence": 2,
                "targets": ["Pad"],
                "eventRange": [7, 12],
                "outcome": "success",
                "detail": "recompute_finished",
            }
        )
        trace["objects"]["Pad"]["slices"].extend([8, 9, 10, 11])
        validate_trace(trace)


if __name__ == "__main__":
    unittest.main()
