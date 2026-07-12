from __future__ import annotations

import argparse
import copy
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from tests.producer_trace_fixture import (
    add_snapshot,
    bind_trace,
    event,
    insert_events,
    producer_trace,
    resequence,
    retag,
)
from tools.compare_element_map_producer_trace import paths_from_args
from tools.element_map_producer_trace import compare_traces
from tools.element_map_producer_trace.compare import _Mappings, _project_pair
from tools.element_map_producer_trace.projection import _canonical_ordered_entries
from tools.collect_freecad_expected import canonical_json_sha256
from tests.test_collect_freecad_expected_producer_trace import (
    access_audit_trace,
    rehash_access_set,
    trace_with_access_summary,
)


ROOT = Path(__file__).resolve().parents[1]
PAD_GRAPH = {
    "Objects": [
        {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}
    ]
}


def _snapshot_id(trace: dict) -> str:
    return next(iter(trace["stringTableSnapshots"]))


def _scope_id(trace: dict) -> int:
    return next(value["scopeSequence"] for value in trace["events"] if value["slice"] == "scope.begin")


def _object_tag(trace: dict) -> int:
    return int(trace["objects"]["Pad"]["tag"])


def _normal_event(trace: dict, slice_name: str, **fields: object) -> dict:
    value = event(
        0,
        _scope_id(trace),
        0,
        slice_name,
        "observed",
        "test_event",
        _snapshot_id(trace),
        fields=dict(fields),
    )
    value["objectTag"] = _object_tag(trace)
    return value


def _allocation(trace: dict, value: int) -> dict:
    item = event(
        0,
        _scope_id(trace),
        0,
        "hasher.insert",
        "allocation",
        "string_id_allocated",
        _snapshot_id(trace),
        fields={"related": [], "result": {"value": value, "index": 0}},
    )
    item["objectTag"] = _object_tag(trace)
    return item


def _identity(
    trace: dict,
    value: str,
    *,
    kind: str = "ledger",
    role: str = "Pad:Shape",
) -> dict:
    item = event(
        0,
        _scope_id(trace),
        0,
        "trace.identity",
        "create",
        "first_seen_value_identity",
        _snapshot_id(trace),
        fields={
            "identity": value,
            "kind": kind,
            "role": role,
            "relatedIdentity": "",
        },
    )
    item["objectTag"] = _object_tag(trace)
    return item


def _extra_scope(trace: dict, *, producer: str = "PartDesign::Pad", stage: str = "child") -> dict:
    begin = event(
        0,
        2,
        1,
        "scope.begin",
        "begin",
        "scope_entered",
        _snapshot_id(trace),
        stage=stage,
    )
    begin["producer"] = producer
    checkpoint = event(
        0,
        2,
        1,
        "maker.final_checkpoint",
        "published",
        "value_snapshot",
        _snapshot_id(trace),
    )
    checkpoint["producer"] = producer
    end = event(
        0,
        2,
        1,
        "scope.end",
        "success",
        "scope_finished",
        _snapshot_id(trace),
    )
    end["producer"] = producer
    insert_events(trace, 3, [begin, checkpoint, end])
    return trace


def _wrapped_trace() -> dict:
    trace = producer_trace(scope_sequence=3)
    snapshot_id = _snapshot_id(trace)
    outer = event(
        0,
        1,
        0,
        "scope.begin",
        "begin",
        "scope_entered",
        snapshot_id,
        stage="document.object.execute",
    )
    inner = event(
        0,
        2,
        1,
        "scope.begin",
        "begin",
        "scope_entered",
        snapshot_id,
        stage="document.object.recompute",
    )
    for value in trace["events"]:
        if value.get("scopeSequence") == 3:
            value["parentScopeSequence"] = 2
    inner_end = event(
        0,
        2,
        1,
        "scope.end",
        "success",
        "scope_finished",
        trace["events"][-1]["beforeSnapshot"],
    )
    outer_end = event(
        0,
        1,
        0,
        "scope.end",
        "success",
        "scope_finished",
        trace["events"][-1]["beforeSnapshot"],
    )
    insert_events(trace, 1, [outer, inner])
    insert_events(trace, len(trace["events"]) - 1, [inner_end, outer_end])
    return trace


class CompareProducerTraceTests(unittest.TestCase):
    def test_access_summary_identity_is_projected_after_raw_summary_ref_changes(self) -> None:
        expected = trace_with_access_summary()
        actual = retag(trace_with_access_summary(), 99)
        summary = next(iter(actual["accessSummaries"].values()))
        summary["ownerObjectTag"] = 99
        summary["ownerIdentity"] = "object:99"
        summary["propertyIdentity"] = "property:object:99:Shape"
        for identity in actual["accessSets"].values():
            for field in ("propertyReads", "propertyWrites"):
                identity[field] = [value.replace("object:2", "object:99") for value in identity[field]]
            for field in ("objectReads", "objectWrites"):
                identity[field] = [value.replace("object:2", "object:99") for value in identity[field]]
        for metadata in actual["traceIdentities"].values():
            metadata["ownerIdentity"] = metadata["ownerIdentity"].replace("object:2", "object:99")
            if "propertyIdentity" in metadata:
                metadata["propertyIdentity"] = metadata["propertyIdentity"].replace("object:2", "object:99")
        renamed_identities = {
            key.replace("object:2", "object:99"): value
            for key, value in actual["traceIdentities"].items()
        }
        actual["traceIdentities"] = renamed_identities
        summary["elementMapReads"] = [value.replace("object:2", "object:99") for value in summary["elementMapReads"]]
        summary["elementMapWrites"] = [value.replace("object:2", "object:99") for value in summary["elementMapWrites"]]
        rehash_access_set(actual)

        result = compare_traces(expected, actual)

        self.assertEqual("equal", result.status, f"{result.classification}: {result.detail}")
        self.assertEqual("projected", result.equivalence)
        self.assertIn(
            "derived_access_summary_identity",
            {record.reason_code for record in result.normalizations},
        )

    def test_dangling_summary_ref_is_invalid_before_projection(self) -> None:
        expected = trace_with_access_summary()
        actual = trace_with_access_summary()
        actual["events"][4]["fields"]["summaryRef"] = "accessSummary:sha256:missing"

        result = compare_traces(expected, actual)

        self.assertEqual("invalid", result.status)
        self.assertEqual("invalid_actual_trace", result.classification)

    def test_tampered_summary_hash_is_invalid_before_projection(self) -> None:
        expected = trace_with_access_summary()
        actual = trace_with_access_summary()
        next(iter(actual["accessSummaries"].values()))["propertyName"] = "Other"

        result = compare_traces(expected, actual)

        self.assertEqual("invalid", result.status)
        self.assertEqual("invalid_actual_trace", result.classification)

    def test_independent_owner_summary_blocks_may_reorder(self) -> None:
        def two_owner_trace() -> dict:
            value = trace_with_access_summary()
            value["objects"]["Pad"]["typeId"] = "PartDesign::Point"
            value["objectTagIndex"]["2"] = {"object": "Point", "typeId": "PartDesign::Point"}
            value["objects"]["Point"] = value["objects"].pop("Pad")
            value["transactions"][0]["targets"] = ["Point"]
            for item in value["events"]:
                if item.get("object") == "Pad":
                    item["object"] = "Point"
            value["objects"]["Line"] = {"tag": 3, "typeId": "PartDesign::Line", "slices": []}
            value["objectTagIndex"]["3"] = {"object": "Line", "typeId": "PartDesign::Line"}
            summary_ref, summary = next(iter(value["accessSummaries"].items()))
            summary.update(
                owner="Point",
                ownerIdentity="object:2",
                ownerTypeId="PartDesign::Point",
                ownerGraphRole="PartDesign::Point",
                propertyIdentity="property:object:2:Shape",
                touchesDocumentState=False,
                stringHasherReads=[],
            )
            access_ref = summary["accessSet"]
            access_set = value["accessSets"][access_ref]
            access_set["documentReads"] = []
            access_set["documentWrites"] = []
            access_ref = "accessSet:sha256:" + canonical_json_sha256(
                {key: access_set[key] for key in access_set if key != "canonicalHash"}
            )
            access_set["canonicalHash"] = access_ref.rsplit(":", 1)[-1]
            value["accessSets"] = {access_ref: access_set}
            summary["accessSet"] = access_ref

            line_access = copy.deepcopy(access_set)
            for field in ("propertyReads", "propertyWrites", "objectReads", "objectWrites"):
                line_access[field] = [item.replace("object:2", "object:3") for item in line_access[field]]
            line_access_ref = "accessSet:sha256:" + canonical_json_sha256(
                {key: line_access[key] for key in line_access if key != "canonicalHash"}
            )
            line_access["canonicalHash"] = line_access_ref.rsplit(":", 1)[-1]
            value["accessSets"][line_access_ref] = line_access
            value["traceIdentities"]["element-map:property:object:3:Shape"] = {
                "kind": "elementMap",
                "ownerIdentity": "object:3",
                "propertyIdentity": "property:object:3:Shape",
            }
            line_summary = copy.deepcopy(summary)
            line_summary.update(
                owner="Line",
                ownerIdentity="object:3",
                ownerObjectTag=3,
                ownerTypeId="PartDesign::Line",
                ownerGraphRole="PartDesign::Line",
                propertyIdentity="property:object:3:Shape",
                accessSet=line_access_ref,
                elementMapReads=["element-map:property:object:3:Shape"],
                elementMapWrites=["element-map:property:object:3:Shape"],
            )
            summary_ref = "accessSummary:sha256:" + canonical_json_sha256(
                {key: summary[key] for key in summary if key != "canonicalHash"}
            )
            summary["canonicalHash"] = summary_ref.rsplit(":", 1)[-1]
            line_summary_ref = "accessSummary:sha256:" + canonical_json_sha256(
                {key: line_summary[key] for key in line_summary if key != "canonicalHash"}
            )
            line_summary["canonicalHash"] = line_summary_ref.rsplit(":", 1)[-1]
            value["accessSummaries"] = {summary_ref: summary, line_summary_ref: line_summary}
            summary_event = next(item for item in value["events"] if item["slice"] == "property_shape.access_summary")
            summary_event["fields"]["summaryRef"] = summary_ref
            line_event = copy.deepcopy(summary_event)
            line_event["fields"]["summaryRef"] = line_summary_ref
            value["events"].insert(5, line_event)
            resequence(value)
            return value

        expected = two_owner_trace()
        actual = two_owner_trace()
        summary_positions = [index for index, item in enumerate(actual["events"]) if item["slice"] == "property_shape.access_summary"]
        actual["events"][summary_positions[0]], actual["events"][summary_positions[1]] = (
            actual["events"][summary_positions[1]], actual["events"][summary_positions[0]]
        )
        resequence(actual)

        result = compare_traces(expected, actual)

        self.assertEqual("equal", result.status, f"{result.classification}: {result.detail}")
        self.assertEqual("projected", result.equivalence)
        self.assertIn(
            "independent_owner_block_reorder",
            {record.reason_code for record in result.normalizations},
            f"{result.equivalence} {result.normalizations}",
        )

    def test_comparison_result_distinguishes_raw_and_projected_equivalence(self) -> None:
        raw = compare_traces(producer_trace(), producer_trace())
        self.assertEqual("equal", raw.status)
        self.assertEqual("raw", raw.equivalence)
        self.assertEqual(0, raw.raw_difference_count)
        self.assertEqual(0, raw.semantic_difference_count)
        self.assertEqual((), raw.normalizations)

        expected = producer_trace()
        actual = retag(producer_trace(), 0x5A)
        projected = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("equal", projected.status)
        self.assertEqual("projected", projected.equivalence)
        self.assertGreater(projected.raw_difference_count, 0)
        self.assertEqual(0, projected.semantic_difference_count)
        self.assertIn(
            "runtime_tag_bijection",
            {record.reason_code for record in projected.normalizations},
        )

    def test_boolean_string_table_flags_never_enter_sid_bijection(self) -> None:
        mappings = _Mappings()
        self.assertIsNone(mappings.sids.bind(1, 1))
        expected, actual, error = _project_pair(
            True,
            True,
            path=("entries", "12", "prefixIDIndex"),
            slice_name="table_checkpoint",
            mappings=mappings,
        )
        self.assertIsNone(error)
        self.assertIs(expected, True)
        self.assertIs(actual, True)

    def test_ordered_entries_resolve_and_collapse_only_equivalent_transport_aliases(self) -> None:
        table = {
            8: {"data": "g3v2", "postfix": ";SKT", "mapped": True},
            9: {"data": "g4v1", "postfix": ";SKT", "mapped": True},
        }
        native = (
            "#8:2;:H9f,V[#8:2]|g4v1;SKT;:H9f,V[]|"
            "g3v2;SKT;:H9f,V[]|#9:1;:H9f,V[#9:1]"
        )
        actual = "#8:2;:H9f,V[#8:2]|#9:1;:H9f,V[#9:1]"
        self.assertEqual(
            _canonical_ordered_entries(native, table),
            _canonical_ordered_entries(actual, table),
        )

    def test_equal_and_first_field_pointer(self) -> None:
        self.assertEqual(compare_traces(producer_trace(), producer_trace()).status, "equal")
        different = producer_trace(field_value="different")
        result = compare_traces(producer_trace(), different)
        self.assertEqual(result.status, "different")
        self.assertEqual(result.classification, "field_mismatch")
        self.assertEqual(result.json_pointer, "/fields/value")

    def test_all_document_and_explicit_target_use_effective_execution_set(self) -> None:
        expected = producer_trace()
        expected["transactions"][0]["targets"] = []
        actual = producer_trace()
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("equal", result.status)

    def test_transaction_missing_or_extra_and_outcome_are_classified_before_scopes(self) -> None:
        actual = producer_trace()
        second = copy.deepcopy(actual["events"])
        offset = len(actual["events"])
        for value in second:
            value["sequence"] += offset
            value["transactionSequence"] = 2
            if value["scopeSequence"]:
                value["scopeSequence"] = 2
        actual["events"].extend(second)
        actual["transactions"].append(
            {
                "sequence": 2,
                "targets": ["Pad"],
                "eventRange": [offset + 1, offset * 2],
                "outcome": "success",
                "detail": "recompute_finished",
            }
        )
        actual["objects"]["Pad"]["slices"].extend(
            value["sequence"] for value in second if value["objectTag"] == 2
        )
        result = compare_traces(producer_trace(), actual, document_graph=PAD_GRAPH)
        self.assertEqual("transaction_missing_or_extra", result.classification)
        self.assertEqual("/transactions", result.json_pointer)

        outcome = producer_trace()
        outcome["transactions"][0]["outcome"] = "exception"
        outcome["events"][-1]["fields"]["partialWrite"] = True
        result = compare_traces(producer_trace(), outcome, document_graph=PAD_GRAPH)
        self.assertEqual("transaction_outcome_mismatch", result.classification)
        self.assertEqual("/transaction/outcome", result.json_pointer)

    def test_sid_allocation_field_drift_has_its_own_classification(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        left = _allocation(expected, 1)
        right = _allocation(actual, 17)
        left["fields"]["data"] = "Face1"
        right["fields"]["data"] = "Face2"
        insert_events(expected, 2, [left])
        insert_events(actual, 2, [right])
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("sid_allocation_mismatch", result.classification)
        self.assertEqual("/fields/data", result.json_pointer)

    def test_native_and_cad_rs_hasher_insert_envelopes_share_one_semantic_event(self) -> None:
        expected = producer_trace()
        expected["producer"] = {"name": "FreeCAD", "document": "Native"}
        native = event(
            0,
            _scope_id(expected),
            0,
            "hasher.insert",
            "allocated",
            "table_insert",
            _snapshot_id(expected),
            fields={"data": "Face", "id": "1", "postfix": ";OP"},
        )
        native["objectTag"] = _object_tag(expected)

        actual = producer_trace()
        cad_rs = _allocation(actual, 1)
        cad_rs["fields"].update({"data": "Face", "postfix": ";OP"})
        insert_events(expected, 2, [native])
        insert_events(actual, 2, [cad_rs])

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.expected_value!r} {result.actual_value!r}",
        )

    def test_preserved_encode_expands_validated_cad_rs_sid_and_drops_transport_fields(self) -> None:
        expected = retag(producer_trace(), 159)
        expected["producer"] = {"name": "FreeCAD", "document": "Native"}
        native = _normal_event(
            expected,
            "element_map.encode",
            before="g1;SKT;:H9f,V",
        )
        native["decision"] = "preserved"
        native["reason"] = "same_or_empty_tag"

        actual = producer_trace()
        table_id = add_snapshot(
            actual,
            {
                "entries": [
                    {
                        "token": "#2",
                        "data": "g",
                        "postfix": ";SKT",
                        "mapped": True,
                        "related": [],
                    }
                ]
            },
            kind="stringTable",
            label="table_checkpoint",
        )
        actual["stringTableSnapshots"][table_id]["definedSids"] = [2]
        cad_rs = event(
            0,
            _scope_id(actual),
            0,
            "element_map.encode",
            "preserved",
            "same_or_empty_tag",
            _snapshot_id(actual),
            fields={
                "before": "#2:1;:H2,V",
                "after": "#2:1;:H2,V",
                "elementType": "V",
                "masterTag": "0",
                "inputTag": "0",
                "entryLocalRefs": [],
            },
        )
        cad_rs["objectTag"] = _object_tag(actual)
        insert_events(expected, 2, [native])
        insert_events(actual, 2, [cad_rs])

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.expected_value!r} {result.actual_value!r}",
        )

    def test_document_object_wrapper_scopes_are_transparent(self) -> None:
        result = compare_traces(_wrapped_trace(), producer_trace(scope_sequence=19))
        self.assertEqual("equal", result.status)

    def test_request_runtime_root_events_do_not_enter_producer_event_bijection(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        runtime = event(
            0,
            0,
            0,
            "topo_state.preflight",
            "selected",
            "topo_state_admitted",
            _snapshot_id(actual),
        )
        insert_events(actual, 1, [runtime])
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("equal", result.status)

    def test_unknown_producer_names_are_not_fuzzily_canonicalized(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        for value in expected["events"]:
            if value["scopeSequence"]:
                value["producer"] = "Custom::PadLike"
        for value in actual["events"]:
            if value["scopeSequence"]:
                value["producer"] = "Custom::PocketLike"
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("scope_missing_or_extra", result.classification)

    def test_effective_target_mismatch_precedes_scope_comparison(self) -> None:
        graph = {
            "Objects": [
                {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}},
                {"Name": "Sketch", "TypeId": "Sketcher::SketchObject", "Properties": {}},
            ]
        }
        expected = producer_trace()
        actual = producer_trace()
        for trace in (expected, actual):
            trace["objects"]["Sketch"] = {
                "tag": 3,
                "typeId": "Sketcher::SketchObject",
                "slices": [],
            }
            trace["objectTagIndex"]["3"] = {
                "object": "Sketch",
                "typeId": "Sketcher::SketchObject",
            }
        expected["transactions"][0]["targets"] = []
        result = compare_traces(expected, actual, document_graph=graph)
        self.assertEqual("target_inventory_mismatch", result.classification)
        self.assertEqual("/transaction/effectiveTargets/1", result.json_pointer)

    def test_native_effective_targets_are_authoritative_over_wrapper_scopes(self) -> None:
        graph = {
            "Objects": [
                {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}},
                {"Name": "Sketch", "TypeId": "Sketcher::SketchObject", "Properties": {}},
            ]
        }
        expected = producer_trace()
        actual = _extra_scope(
            producer_trace(),
            producer="Sketcher::SketchObject",
            stage="sketch.producer",
        )
        for trace in (expected, actual):
            trace["objects"]["Sketch"] = {
                "tag": 3,
                "typeId": "Sketcher::SketchObject",
                "slices": [],
            }
            trace["objectTagIndex"]["3"] = {
                "object": "Sketch",
                "typeId": "Sketcher::SketchObject",
            }
        expected["transactions"][0]["effectiveTargets"] = ["Pad", "Sketch"]
        for value in actual["events"]:
            if value.get("scopeSequence") == 2:
                value["object"] = "Sketch"
                value["objectTag"] = 3
        resequence(actual)

        result = compare_traces(expected, actual, document_graph=graph)
        self.assertEqual("scope_missing_or_extra", result.classification)

    def test_object_contract_requires_cross_trace_bijection(self) -> None:
        actual = producer_trace()
        actual["objects"]["Pad"]["typeId"] = "PartDesign::Pocket"
        actual["objectTagIndex"]["2"]["typeId"] = "PartDesign::Pocket"
        result = compare_traces(producer_trace(), actual, document_graph=PAD_GRAPH)
        self.assertEqual("invalid", result.status)
        self.assertEqual("object_bijection_invalid", result.classification)

    def test_object_contract_validates_graph_role_on_each_trace(self) -> None:
        graph = {
            "Objects": [
                {
                    "Name": "Pad",
                    "TypeId": "PartDesign::Pad",
                    "GraphRole": "feature.target",
                }
            ]
        }
        expected = producer_trace()
        actual = producer_trace()
        expected["objects"]["Pad"]["graphRole"] = "feature.target"
        actual["objects"]["Pad"]["graphRole"] = "feature.source"
        result = compare_traces(expected, actual, document_graph=graph)
        self.assertEqual("invalid", result.status)
        self.assertEqual("object_bijection_invalid", result.classification)
        self.assertIn("graphRole", result.detail)

    def test_graph_external_wrapper_only_object_is_auxiliary(self) -> None:
        expected = producer_trace()
        expected["objects"]["Origin"] = {
            "tag": 3,
            "typeId": "App::Origin",
            "slices": [],
        }
        expected["objectTagIndex"]["3"] = {
            "object": "Origin",
            "typeId": "App::Origin",
        }
        expected["transactions"][0]["targets"].append("Origin")
        result = compare_traces(expected, producer_trace(), document_graph=PAD_GRAPH)
        self.assertEqual("equal", result.status)

    def test_parent_first_scope_bijection_reports_missing_child(self) -> None:
        expected = _extra_scope(
            producer_trace(),
            producer="Part::FaceMaker",
            stage="face_maker.lifecycle",
        )
        result = compare_traces(expected, producer_trace(), document_graph=PAD_GRAPH)
        self.assertEqual("scope_missing_or_extra", result.classification)
        self.assertEqual("/scopes", result.json_pointer)
        self.assertEqual("part/face_maker", result.owner)
        self.assertTrue(result.semantic_scope_path[-1].endswith("#1"))

    def test_local_scope_tag_sid_identity_and_snapshot_hash_drift_is_isomorphic(self) -> None:
        expected = producer_trace(scope_sequence=7)
        expected["producer"] = {"document": "Native"}
        actual = retag(producer_trace(scope_sequence=41), 90)
        expected_identities = [
            _identity(expected, f"{kind}:{index}", kind=kind, role=f"Pad:{kind}")
            for index, kind in enumerate(("shape", "elementMap", "hasher", "mapper"), 1)
        ]
        actual_identities = [
            _identity(actual, f"{kind}:{index + 90}", kind=kind, role=f"Pad:{kind}")
            for index, kind in enumerate(("shape", "elementMap", "hasher", "mapper"), 1)
        ]
        insert_events(expected, 2, [*expected_identities, _allocation(expected, 1)])
        insert_events(actual, 2, [*actual_identities, _allocation(actual, 71)])

        old = _snapshot_id(expected)
        item = expected["stringTableSnapshots"].pop(old)
        item["sha256"] = "a" * 64
        new = "stringTable:sha256:" + "a" * 64
        expected["stringTableSnapshots"][new] = item
        for value in expected["events"]:
            if value["beforeSnapshot"] == old:
                value["beforeSnapshot"] = new
            if value["afterSnapshot"] == old:
                value["afterSnapshot"] = new

        self.assertEqual("equal", compare_traces(expected, actual).status)

    def test_snapshot_field_joins_by_validated_payload_not_hash_text(self) -> None:
        expected = producer_trace()
        actual = copy.deepcopy(expected)
        actual_id = actual["events"][3]["afterSnapshot"]
        actual["events"][3]["beforeSnapshot"] = actual_id
        actual["events"][3]["fields"]["snapshot"] = actual_id

        expected["producer"] = {"name": "FreeCAD"}
        item = expected["ledgerSnapshots"].pop(actual_id)
        expected_id = "ledger:sha256:" + "b" * 64
        item["sha256"] = "b" * 64
        expected["ledgerSnapshots"][expected_id] = item
        for value in expected["events"]:
            if value["beforeSnapshot"] == actual_id:
                value["beforeSnapshot"] = expected_id
            if value["afterSnapshot"] == actual_id:
                value["afterSnapshot"] = expected_id
        expected["events"][3]["beforeSnapshot"] = expected_id
        expected["events"][3]["fields"]["snapshot"] = expected_id

        self.assertEqual("equal", compare_traces(expected, actual).status)

    def test_raw_mapped_name_uses_established_tag_sid_bijections_but_remains_strict(self) -> None:
        expected = producer_trace()
        actual = retag(producer_trace(), 90)
        insert_events(expected, 2, [_allocation(expected, 1)])
        insert_events(actual, 2, [_allocation(actual, 71)])
        expected["events"][3]["fields"]["rawMappedName"] = "Face1:H2:#1"
        actual["events"][3]["fields"]["rawMappedName"] = "Face1:H90:#71"
        self.assertEqual(
            "equal",
            compare_traces(expected, actual, document_graph=PAD_GRAPH).status,
        )

        actual["events"][3]["fields"]["rawMappedName"] = "Face2:H90:#71"
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("field_mismatch", result.classification)
        self.assertEqual("/fields/rawMappedName", result.json_pointer)

    def test_event_identity_projects_tag_before_alignment_but_keeps_source_strict(self) -> None:
        expected = producer_trace()
        actual = retag(producer_trace(), 0x5A)
        insert_events(
            expected,
            2,
            [
                _normal_event(
                    expected,
                    "maker.generated",
                    source="#8;:H2,V",
                    sourceTag=2,
                    target="Edge1",
                )
            ],
        )
        insert_events(
            actual,
            2,
            [
                _normal_event(
                    actual,
                    "maker.generated",
                    source="#8;:H5a,V",
                    sourceTag=0x5A,
                    target="Edge1",
                )
            ],
        )

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        actual_generated = next(
            item for item in actual["events"] if item["slice"] == "maker.generated"
        )
        actual_generated["fields"]["source"] = "#8;:H5a:2,V"
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("different", result.status)

    def test_event_fields_bind_explicit_tag_before_embedded_mapped_name(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        insert_events(
            expected,
            2,
            [
                _normal_event(
                    expected,
                    "maker.generated",
                    source="#8;:H-64,V",
                    sourceTag=-100,
                    target="Edge1",
                )
            ],
        )
        insert_events(
            actual,
            2,
            [
                _normal_event(
                    actual,
                    "maker.generated",
                    source="#8;:H-384,V",
                    sourceTag=-900,
                    target="Edge1",
                )
            ],
        )

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        actual_generated = next(
            item for item in actual["events"] if item["slice"] == "maker.generated"
        )
        actual_generated["fields"]["source"] = "#8;:H-384:2,V"
        self.assertEqual(
            "different",
            compare_traces(expected, actual, document_graph=PAD_GRAPH).status,
        )

    def test_snapshot_derived_hash_does_not_override_projected_payload(self) -> None:
        expected = producer_trace()
        actual = retag(producer_trace(), 0x5A)
        expected_snapshot = add_snapshot(
            expected,
            {
                "value": "same",
                "canonicalPayloadSha256": "a" * 64,
            },
        )
        actual_snapshot = add_snapshot(
            actual,
            {
                "value": "same",
                "canonicalPayloadSha256": "b" * 64,
            },
        )
        expected["events"][2]["fields"] = {"snapshot": expected_snapshot}
        actual["events"][2]["fields"] = {"snapshot": actual_snapshot}

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        different_snapshot = add_snapshot(
            actual,
            {
                "value": "different",
                "canonicalPayloadSha256": "b" * 64,
            },
        )
        actual["events"][2]["fields"] = {"snapshot": different_snapshot}
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("different", result.status)
        self.assertIn("value", result.json_pointer or "")

    def test_deleted_tag_in_mapped_name_uses_the_object_tag_bijection(self) -> None:
        expected = producer_trace()
        actual = retag(producer_trace(), 0x5A)
        expected["events"][2]["fields"] = {
            "rawMappedName": "Edge1;D2;:H2:5,E"
        }
        actual["events"][2]["fields"] = {
            "rawMappedName": "Edge1;D5a;:H5a:5,E"
        }

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        actual["events"][2]["fields"]["rawMappedName"] = "Edge1;D5a;:H5a:6,E"
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("field_mismatch", result.classification)

    def test_embedded_unknown_tags_bind_only_when_string_structure_matches(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        expected["events"][2]["fields"] = {
            "rawMappedName": "Vertex1;Db44;:H:5,V"
        }
        actual["events"][2]["fields"] = {
            "rawMappedName": "Vertex1;D574;:H:5,V"
        }

        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        actual["events"][2]["fields"]["rawMappedName"] = "Vertex1;D575;:H:6,V"
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("field_mismatch", result.classification)

        expected = producer_trace()
        actual = producer_trace()
        expected["events"][2]["fields"] = {"owner": "OwnerHabc"}
        actual["events"][2]["fields"] = {"owner": "OwnerHdef"}
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("field_mismatch", result.classification)

    def test_transient_owner_uses_first_seen_semantics_not_random_tag_name(self) -> None:
        def add_transient(trace: dict, name: str, tag: int, producer: str) -> None:
            trace["objects"][name] = {
                "tag": tag,
                "typeId": "Part::TopoShape",
                "slices": [],
            }
            trace["objectTagIndex"][str(tag)] = {
                "object": name,
                "typeId": "Part::TopoShape",
            }
            observed = _normal_event(
                trace,
                "refine.lifecycle",
                stage="makeElementRefine",
            )
            observed["object"] = name
            observed["objectTag"] = tag
            observed["producer"] = producer
            insert_events(trace, 2, [observed])

        expected = producer_trace()
        actual = producer_trace()
        add_transient(expected, "@transient:-100", -100, "Part::TopoShape")
        add_transient(actual, "@transient:-900", -900, "Part::TopoShape")

        result = compare_traces(expected, actual)
        self.assertEqual(
            "equal",
            result.status,
            f"{result.classification} {result.json_pointer} {result.detail}",
        )

        changed = producer_trace()
        add_transient(changed, "@transient:-901", -901, "Part::FaceMaker")
        result = compare_traces(expected, changed)
        self.assertEqual("target_inventory_mismatch", result.classification)

    def test_identity_one_to_many_is_invalid_actual(self) -> None:
        actual = producer_trace()
        insert_events(actual, 2, [_identity(actual, "ledger:1"), _identity(actual, "ledger:2")])
        result = compare_traces(producer_trace(), actual)
        self.assertEqual("invalid", result.status)
        self.assertEqual("invalid_actual_trace", result.classification)

    def test_valid_missing_or_extra_event_uses_finite_lookahead(self) -> None:
        actual = producer_trace()
        insert_events(actual, 3, [_normal_event(actual, "maker.generated", target="Face2")])
        result = compare_traces(producer_trace(), actual)
        self.assertEqual("event_missing_or_extra", result.classification)
        self.assertIn("extra", result.detail)

        reverse = compare_traces(actual, producer_trace())
        self.assertEqual("event_missing_or_extra", reverse.classification)

        far = producer_trace()
        insert_events(
            far,
            2,
            [
                _normal_event(far, "maker.generated", target=f"Face{index}")
                for index in range(1, 5)
            ],
        )
        result = compare_traces(producer_trace(), far, document_graph=PAD_GRAPH)
        self.assertEqual("event_missing_or_extra", result.classification)
        self.assertIn("outside finite look-ahead", result.detail)

    def test_missing_or_extra_scope_and_parent_mismatch(self) -> None:
        extra = _extra_scope(
            producer_trace(),
            producer="Part::FaceMaker",
            stage="face_maker.lifecycle",
        )
        result = compare_traces(producer_trace(), extra)
        self.assertEqual("scope_missing_or_extra", result.classification)
        self.assertEqual("part/face_maker", result.owner)

        parent = copy.deepcopy(extra)
        for value in parent["events"]:
            if value.get("scopeSequence") == 2:
                value["parentScopeSequence"] = 0
        # Rebuild a valid sibling scope so only the semantic parent differs.
        parent["events"] = [
            value
            for value in parent["events"]
            if value.get("scopeSequence") != 2
        ]
        resequence(parent)
        sibling = _extra_scope(
            parent,
            producer="Part::FaceMaker",
            stage="face_maker.lifecycle",
        )
        for value in sibling["events"]:
            if value.get("scopeSequence") == 2:
                value["parentScopeSequence"] = 0
        # Move the sibling after the parent closes to keep LIFO closure valid.
        child = [value for value in sibling["events"] if value.get("scopeSequence") == 2]
        sibling["events"] = [value for value in sibling["events"] if value.get("scopeSequence") != 2]
        sibling["events"][-1:-1] = child
        resequence(sibling)
        result = compare_traces(extra, sibling)
        self.assertEqual("scope_parent_mismatch", result.classification)
        self.assertEqual("part/face_maker", result.owner)

    def test_scope_alignment_does_not_search_past_finite_lookahead(self) -> None:
        actual = producer_trace()
        values: list[dict] = []
        for ordinal in range(4):
            sequence = 10 + ordinal
            producer = f"Extra::Producer{ordinal}"
            begin = event(
                0,
                sequence,
                0,
                "scope.begin",
                "begin",
                "scope_entered",
                _snapshot_id(actual),
                stage=f"extra{ordinal}",
            )
            checkpoint = event(
                0,
                sequence,
                0,
                "maker.final_checkpoint",
                "published",
                "value_snapshot",
                _snapshot_id(actual),
            )
            end = event(
                0,
                sequence,
                0,
                "scope.end",
                "success",
                "scope_finished",
                _snapshot_id(actual),
            )
            for item in (begin, checkpoint, end):
                item["producer"] = producer
            values.extend((begin, checkpoint, end))
        insert_events(actual, 1, values)
        result = compare_traces(producer_trace(), actual, document_graph=PAD_GRAPH)
        self.assertEqual("scope_missing_or_extra", result.classification)
        self.assertIn("outside finite look-ahead", result.detail)
        self.assertIn("Extra::Producer0", repr(result.actual_value))

    def test_body_upstream_producer_replay_is_classified(self) -> None:
        expected = producer_trace()
        for trace in (expected,):
            trace["objects"] = {
                "Body": {"tag": 3, "typeId": "PartDesign::Body", "slices": []},
                "Pad": {"tag": 2, "typeId": "PartDesign::Pad", "slices": []},
            }
            trace["objectTagIndex"] = {
                "2": {"object": "Pad", "typeId": "PartDesign::Pad"},
                "3": {"object": "Body", "typeId": "PartDesign::Body"},
            }
            trace["transactions"][0]["targets"] = ["Body"]
            for value in trace["events"]:
                if value.get("scopeSequence"):
                    value["object"] = "Body"
                    value["objectTag"] = 3
                    value["producer"] = "PartDesign::Body"
            resequence(trace)
        actual = copy.deepcopy(expected)
        child = _extra_scope(actual, producer="PartDesign::Pad", stage="partdesign.extrude")
        for value in child["events"]:
            if value.get("scopeSequence") == 2:
                value["object"] = "Pad"
                value["objectTag"] = 2
        resequence(child)
        result = compare_traces(expected, child)
        self.assertEqual("producer_replay", result.classification)
        self.assertEqual("part_design/body", result.owner)

    def test_decision_and_reason_precede_field_or_sid_cascade(self) -> None:
        actual = producer_trace(field_value="cascade")
        actual["events"][2]["decision"] = "rejected"
        result = compare_traces(producer_trace(), actual)
        self.assertEqual("decision_mismatch", result.classification)
        self.assertEqual("/decision", result.json_pointer)

        actual = producer_trace()
        actual["events"][2]["reason"] = "different_reason"
        self.assertEqual("reason_mismatch", compare_traces(producer_trace(), actual).classification)

    def test_nested_event_interleaving_beats_parent_final_snapshot_drift(self) -> None:
        expected = _extra_scope(
            producer_trace(),
            producer="Part::FaceMaker",
            stage="face_maker.lifecycle",
        )
        actual = copy.deepcopy(expected)
        child_checkpoint = next(
            value
            for value in actual["events"]
            if value["scopeSequence"] == 2 and value["slice"] == "maker.final_checkpoint"
        )
        child_checkpoint["decision"] = "changed"
        parent_checkpoint = next(
            value
            for value in actual["events"]
            if value["scopeSequence"] == 1 and value["slice"] == "maker.final_checkpoint"
        )
        parent_checkpoint["afterSnapshot"] = add_snapshot(
            actual, {"parent": "downstream-drift"}
        )
        result = compare_traces(expected, actual, document_graph=PAD_GRAPH)
        self.assertEqual("decision_mismatch", result.classification)
        self.assertEqual(2, result.actual_event["scopeSequence"])

    def test_before_already_diverged_vs_event_first_changes_state(self) -> None:
        before = producer_trace()
        changed = add_snapshot(before, {"state": "already-different"})
        before["events"][2]["beforeSnapshot"] = changed
        before["events"][2]["afterSnapshot"] = changed
        result = compare_traces(producer_trace(), before)
        self.assertEqual("before_snapshot_mismatch", result.classification)
        self.assertEqual("different", result.before_alignment)

        after = producer_trace()
        changed = add_snapshot(after, {"state": "changed-by-event"})
        after["events"][2]["afterSnapshot"] = changed
        result = compare_traces(producer_trace(), after)
        self.assertEqual("after_snapshot_mismatch", result.classification)
        self.assertEqual("aligned", result.before_alignment)
        self.assertEqual("different", result.after_alignment)

    def test_ordered_refs_mapper_candidates_targets_and_child_ranges_are_strict(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        insert_events(expected, 2, [_allocation(expected, 1), _allocation(expected, 2)])
        insert_events(actual, 2, [_allocation(actual, 10), _allocation(actual, 20)])
        expected["events"][4]["fields"]["orderedRelated"] = [1, 2]
        actual["events"][4]["fields"]["orderedRelated"] = [20, 10]
        self.assertEqual("ordered_refs_mismatch", compare_traces(expected, actual).classification)

        for key, left, right, classification in (
            ("orderedCandidates", ["Face1", "Face2"], ["Face2", "Face1"], "mapper_relation_mismatch"),
            ("targets", ["Face1", "Face2"], ["Face2", "Face1"], "target_inventory_mismatch"),
            (
                "childRanges",
                [{"kind": "edge", "offset": 0, "count": 1}, {"kind": "face", "offset": 0, "count": 1}],
                [{"kind": "face", "offset": 0, "count": 1}, {"kind": "edge", "offset": 0, "count": 1}],
                "child_range_mismatch",
            ),
        ):
            with self.subTest(key=key):
                expected = producer_trace()
                actual = producer_trace()
                expected["events"][2]["fields"][key] = left
                actual["events"][2]["fields"][key] = right
                self.assertEqual(classification, compare_traces(expected, actual).classification)

    def test_nondeterminism_requires_source_declaration(self) -> None:
        expected = producer_trace()
        actual = producer_trace()
        expected["events"][2]["fields"]["rawSuffix"] = "left"
        actual["events"][2]["fields"]["rawSuffix"] = "right"
        self.assertEqual("field_mismatch", compare_traces(expected, actual).classification)

        for trace in (expected, actual):
            trace["events"][2]["fields"].update(
                {"nondeterminismClass": "uuid-suffix", "stableComparisonKey": "same-input"}
            )
        self.assertEqual("equal", compare_traces(expected, actual).status)

    def test_invalid_sides_and_checkpoint_mutation_are_separate(self) -> None:
        self.assertEqual("invalid_expected_trace", compare_traces({}, producer_trace()).classification)
        self.assertEqual("invalid_actual_trace", compare_traces(producer_trace(), {}).classification)
        checkpoint = producer_trace()
        checkpoint["events"][3]["slice"] = "maker.after"
        result = compare_traces(producer_trace(), checkpoint)
        self.assertEqual("invalid", result.status)
        self.assertEqual("final_checkpoint_missing", result.classification)

    def test_closed_traces_without_a_comparable_producer_slice_are_invalid(self) -> None:
        actual = producer_trace()
        actual["events"][2]["slice"] = "mapper.query"
        actual["events"][3]["slice"] = "document.recompute.checkpoint"
        result = compare_traces(producer_trace(), actual, document_graph=PAD_GRAPH)
        self.assertEqual("invalid", result.status)
        self.assertEqual("invalid_trace_no_comparable_slice", result.classification)

    def test_phase_case_defaults_to_cad_core_actual(self) -> None:
        args = argparse.Namespace(
            expected=None,
            actual=None,
            phase="c4m6",
            case="demo",
            actual_kind="cad-core",
        )
        expected, actual = paths_from_args(args)
        self.assertEqual("demo.freecad.producer-trace.json", expected.name)
        self.assertEqual(ROOT / "fixtures/c4m6/cad-core-res/demo.cad-core.producer-trace.json", actual)

    def test_cli_exit_codes_binding_and_report_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            input_path = directory / "input.json"
            expected_response_path = directory / "expected-response.json"
            actual_response_path = directory / "actual-response.json"
            expected = directory / "expected.json"
            actual = directory / "actual.json"
            report = directory / "report.json"
            request = copy.deepcopy(PAD_GRAPH)
            response = {"response": True}
            input_path.write_text(json.dumps(request), encoding="utf-8")
            expected_response_path.write_text(json.dumps(response), encoding="utf-8")
            actual_response_path.write_text(json.dumps(response), encoding="utf-8")
            expected.write_text(json.dumps(bind_trace(producer_trace(), request, response)), encoding="utf-8")
            actual.write_text(json.dumps(bind_trace(producer_trace(), request, response)), encoding="utf-8")
            command = [
                "python3",
                str(ROOT / "tools/compare_element_map_producer_trace.py"),
                "--expected",
                str(expected),
                "--actual",
                str(actual),
                "--input",
                str(input_path),
                "--expected-response",
                str(expected_response_path),
                "--actual-response",
                str(actual_response_path),
                "--report",
                str(report),
            ]
            completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(0, completed.returncode, completed.stderr)
            self.assertIn("FIRST_DIVERGENCE aligned", completed.stdout)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertIn("traceHashes", payload)
            self.assertEqual("aligned", payload["beforeAlignment"])

            different = bind_trace(producer_trace(field_value="different"), request, response)
            actual.write_text(json.dumps(different), encoding="utf-8")
            self.assertEqual(1, subprocess.run(command, cwd=ROOT).returncode)
            actual.write_text("{}", encoding="utf-8")
            self.assertEqual(2, subprocess.run(command, cwd=ROOT).returncode)
            invalid_payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual("invalid_actual_trace", invalid_payload["classification"])
            self.assertEqual("not_compared", invalid_payload["beforeAlignment"])
            self.assertEqual(0, invalid_payload["downstreamDriftCount"])
            self.assertEqual(str(expected), invalid_payload["expected"]["path"])
            self.assertIsNotNone(invalid_payload["traceHashes"]["expected"])
            self.assertIsNotNone(invalid_payload["traceHashes"]["actual"])

            checkpoint = bind_trace(producer_trace(), request, response)
            checkpoint["events"][3]["slice"] = "maker.after"
            actual.write_text(json.dumps(checkpoint), encoding="utf-8")
            self.assertEqual(2, subprocess.run(command, cwd=ROOT).returncode)
            self.assertEqual(
                "final_checkpoint_missing",
                json.loads(report.read_text(encoding="utf-8"))["classification"],
            )

            actual.unlink()
            self.assertEqual(2, subprocess.run(command, cwd=ROOT).returncode)
            self.assertEqual(
                "missing_actual_trace",
                json.loads(report.read_text(encoding="utf-8"))["classification"],
            )


if __name__ == "__main__":
    unittest.main()
