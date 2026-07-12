"""Focused contract tests for native producer-trace collection helpers."""

from __future__ import annotations

import copy
import importlib.util
import unittest
from pathlib import Path

from tests.producer_trace_fixture import event, insert_events, producer_trace, resequence


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


def trace() -> dict:
    return producer_trace()


def trace_with_access_summary() -> dict:
    value = trace()
    value["producer"] = {"name": "FreeCAD", "document": "Test"}
    initial_id = next(iter(value["stringTableSnapshots"]))
    ledger_id = next(iter(value["ledgerSnapshots"]))
    access_set = {
        "propertyReads": ["property:object:2:Shape"],
        "objectReads": ["object:2"],
        "documentReads": ["document"],
        "propertyWrites": ["property:object:2:Shape"],
        "objectWrites": ["object:2"],
        "documentWrites": [],
    }
    access_hash = collector.canonical_json_sha256(access_set)
    access_ref = f"accessSet:sha256:{access_hash}"
    value["accessSets"] = {
        access_ref: {**access_set, "canonicalHash": access_hash},
    }
    value["traceIdentities"] = {
        "element-map:property:object:2:Shape": {
            "kind": "elementMap",
            "ownerIdentity": "object:2",
            "propertyIdentity": "property:object:2:Shape",
        },
        "string-hasher:document": {
            "kind": "stringHasher",
            "ownerIdentity": "document",
        },
    }
    summary = {
        "scopeSequence": 1,
        "transactionSequence": 1,
        "producer": "PartDesign::Pad",
        "stage": "property_shape.set_value",
        "ownerIdentity": "object:2",
        "ownerObjectTag": 2,
        "owner": "Pad",
        "ownerTypeId": "PartDesign::Pad",
        "ownerGraphRole": "PartDesign::Pad",
        "propertyName": "Shape",
        "propertyIdentity": "property:object:2:Shape",
        "accessSet": access_ref,
        "elementMapReads": ["element-map:property:object:2:Shape"],
        "elementMapWrites": ["element-map:property:object:2:Shape"],
        "stringHasherReads": ["string-hasher:document"],
        "stringHasherWrites": [],
        "beforeLedgerSnapshot": initial_id,
        "afterLedgerSnapshot": ledger_id,
        "sidAllocationDelta": 0,
        "touchesDocumentState": True,
        "touchesSharedMutableState": False,
        "outcome": "complete",
    }
    summary_hash = collector.canonical_json_sha256(summary)
    summary_ref = f"accessSummary:sha256:{summary_hash}"
    value["accessSummaries"] = {
        summary_ref: {**summary, "canonicalHash": summary_hash},
    }
    insert_events(
        value,
        4,
        [
            event(
                0,
                1,
                0,
                "property_shape.access_summary",
                "published",
                "scope_closed",
                ledger_id,
                fields={"summaryRef": summary_ref},
            )
        ],
    )
    return value


def rehash_access_summary(value: dict) -> None:
    old_ref, summary = next(iter(value["accessSummaries"].items()))
    canonical = {key: item for key, item in summary.items() if key != "canonicalHash"}
    digest = collector.canonical_json_sha256(canonical)
    new_ref = f"accessSummary:sha256:{digest}"
    summary["canonicalHash"] = digest
    value["accessSummaries"] = {new_ref: summary}
    for item in value["events"]:
        if item.get("fields", {}).get("summaryRef") == old_ref:
            item["fields"]["summaryRef"] = new_ref


def rehash_access_set(value: dict) -> None:
    old_ref, access_set = next(iter(value["accessSets"].items()))
    canonical = {key: item for key, item in access_set.items() if key != "canonicalHash"}
    digest = collector.canonical_json_sha256(canonical)
    new_ref = f"accessSet:sha256:{digest}"
    access_set["canonicalHash"] = digest
    value["accessSets"] = {new_ref: access_set}
    for summary in value["accessSummaries"].values():
        if summary["accessSet"] == old_ref:
            summary["accessSet"] = new_ref
    rehash_access_summary(value)


def access_audit_trace(*, shared_write: bool = False, cross_owner_write: bool = False) -> dict:
    summaries = {}
    access_sets = {}
    identities = {}
    events = []
    for ordinal, (tag, owner, type_id) in enumerate(
        ((2, "Point", "PartDesign::Point"), (3, "Line", "PartDesign::Line")), 1
    ):
        owner_identity = f"object:{tag}"
        property_identity = f"property:{owner_identity}:Shape"
        write_identity = "property:shared:Shape" if shared_write else property_identity
        if cross_owner_write and owner == "Point":
            write_identity = "property:object:3:Shape"
        access_set = {
            "propertyReads": [property_identity],
            "objectReads": [owner_identity],
            "documentReads": [],
            "propertyWrites": [write_identity],
            "objectWrites": [owner_identity],
            "documentWrites": [],
        }
        access_hash = collector.canonical_json_sha256(access_set)
        access_ref = f"accessSet:sha256:{access_hash}"
        access_sets[access_ref] = {**access_set, "canonicalHash": access_hash}
        map_identity = f"element-map:property:{owner_identity}:Shape"
        identities[map_identity] = {
            "kind": "elementMap",
            "ownerIdentity": owner_identity,
            "propertyIdentity": property_identity,
        }
        summary = {
            "scopeSequence": ordinal,
            "transactionSequence": 1,
            "producer": type_id,
            "stage": "property_shape.set_value",
            "ownerIdentity": owner_identity,
            "ownerObjectTag": tag,
            "owner": owner,
            "ownerTypeId": type_id,
            "ownerGraphRole": type_id,
            "propertyName": "Shape",
            "propertyIdentity": property_identity,
            "accessSet": access_ref,
            "elementMapReads": [map_identity],
            "elementMapWrites": [map_identity],
            "stringHasherReads": [],
            "stringHasherWrites": [],
            "beforeLedgerSnapshot": "ledger:before",
            "afterLedgerSnapshot": "ledger:after",
            "sidAllocationDelta": 0,
            "touchesDocumentState": False,
            "touchesSharedMutableState": shared_write or (cross_owner_write and owner == "Point"),
            "outcome": "complete",
        }
        digest = collector.canonical_json_sha256(summary)
        summary_ref = f"accessSummary:sha256:{digest}"
        summaries[summary_ref] = {**summary, "canonicalHash": digest}
        events.append({
            "sequence": ordinal,
            "slice": "property_shape.access_summary",
            "fields": {"summaryRef": summary_ref},
        })
    return {
        "events": events,
        "accessSummaries": summaries,
        "accessSets": access_sets,
        "traceIdentities": identities,
    }


class ProducerTraceCollectorTests(unittest.TestCase):
    def test_trace_path_tracks_expected_stem(self) -> None:
        expected = Path("/tmp/case.freecad.json")
        self.assertEqual(
            collector.producer_trace_path_for_expected(expected),
            Path("/tmp/case.freecad.producer-trace.json"),
        )

    def test_valid_trace_is_accepted(self) -> None:
        self.assertEqual(collector.validate_producer_trace(trace())["schemaVersion"], collector.PRODUCER_TRACE_SCHEMA)

    def test_complete_property_shape_access_summary_is_accepted(self) -> None:
        validated = collector.validate_producer_trace(trace_with_access_summary())
        self.assertEqual(1, len(validated["accessSummaries"]))

    def test_access_summary_without_owner_type_is_rejected(self) -> None:
        malformed = trace_with_access_summary()
        summary = next(iter(malformed["accessSummaries"].values()))
        summary.pop("ownerTypeId")
        with self.assertRaisesRegex(RuntimeError, "ownerTypeId"):
            collector.validate_producer_trace(malformed)

    def test_exception_scope_cannot_publish_complete_access_summary(self) -> None:
        malformed = trace_with_access_summary()
        scope_end = next(item for item in malformed["events"] if item["slice"] == "scope.end")
        scope_end["decision"] = "exception"
        scope_end["reason"] = "synthetic_failure"
        rehash_access_summary(malformed)
        with self.assertRaisesRegex(RuntimeError, "outcome"):
            collector.validate_producer_trace(malformed)

    def test_document_write_requires_shared_mutable_state_flag(self) -> None:
        malformed = trace_with_access_summary()
        access_set = next(iter(malformed["accessSets"].values()))
        access_set["documentWrites"] = ["document"]
        rehash_access_set(malformed)
        with self.assertRaisesRegex(RuntimeError, "shared mutable"):
            collector.validate_producer_trace(malformed)

    def test_owner_block_audit_accepts_disjoint_local_shape_writes(self) -> None:
        audit = collector.audit_property_shape_owner_blocks(access_audit_trace())
        self.assertEqual("independent_owner_block_reorder", audit["verdict"])
        self.assertEqual(["Point", "Line"], audit["rawOrder"])

    def test_owner_block_audit_reports_shared_write_conflict(self) -> None:
        audit = collector.audit_property_shape_owner_blocks(access_audit_trace(shared_write=True))
        self.assertEqual("shared_state_blocker", audit["verdict"])
        self.assertTrue(audit["writeConflicts"])

    def test_owner_block_audit_reports_cross_owner_property_write(self) -> None:
        audit = collector.audit_property_shape_owner_blocks(access_audit_trace(cross_owner_write=True))
        self.assertEqual("shared_state_blocker", audit["verdict"])
        self.assertTrue(audit["crossOwnerDependencies"])

    def test_access_summary_required_identity_fields_are_strict(self) -> None:
        for field in ("ownerTypeId", "ownerGraphRole", "propertyName"):
            with self.subTest(field=field):
                malformed = trace_with_access_summary()
                next(iter(malformed["accessSummaries"].values())).pop(field)
                with self.assertRaisesRegex(RuntimeError, field):
                    collector.validate_producer_trace(malformed)

    def test_access_summary_rejects_dangling_access_set(self) -> None:
        malformed = trace_with_access_summary()
        malformed["accessSets"].clear()
        with self.assertRaisesRegex(RuntimeError, "accessSet is missing"):
            collector.validate_producer_trace(malformed)

    def test_access_set_rejects_unresolved_property_identity(self) -> None:
        malformed = trace_with_access_summary()
        access_set = next(iter(malformed["accessSets"].values()))
        access_set["propertyReads"] = ["property:object:999:Shape"]
        rehash_access_set(malformed)
        with self.assertRaisesRegex(RuntimeError, "property identity"):
            collector.validate_producer_trace(malformed)

    def test_cross_owner_property_write_is_valid_but_audited_as_dependency(self) -> None:
        value = trace_with_access_summary()
        value["objectTagIndex"]["3"] = {"object": "Line", "typeId": "PartDesign::Line"}
        value["objects"]["Line"] = {"tag": 3, "typeId": "PartDesign::Line", "slices": []}
        access_set = next(iter(value["accessSets"].values()))
        access_set["propertyWrites"] = ["property:object:3:Shape"]
        summary = next(iter(value["accessSummaries"].values()))
        summary["touchesSharedMutableState"] = True
        rehash_access_set(value)
        validated = collector.validate_producer_trace(value)
        audit = collector.audit_property_shape_owner_blocks(validated)
        self.assertEqual("shared_state_blocker", audit["verdict"])
        self.assertTrue(audit["crossOwnerDependencies"])

    def test_access_summary_rejects_dangling_element_map_identity(self) -> None:
        malformed = trace_with_access_summary()
        summary = next(iter(malformed["accessSummaries"].values()))
        summary["elementMapReads"] = ["element-map:missing"]
        rehash_access_summary(malformed)
        with self.assertRaisesRegex(RuntimeError, "unresolved"):
            collector.validate_producer_trace(malformed)

    def test_access_summary_rejects_dangling_string_hasher_identity(self) -> None:
        malformed = trace_with_access_summary()
        summary = next(iter(malformed["accessSummaries"].values()))
        summary["stringHasherReads"] = ["string-hasher:missing"]
        rehash_access_summary(malformed)
        with self.assertRaisesRegex(RuntimeError, "unresolved"):
            collector.validate_producer_trace(malformed)

    def test_access_summary_rejects_dangling_snapshot(self) -> None:
        malformed = trace_with_access_summary()
        summary = next(iter(malformed["accessSummaries"].values()))
        summary["afterLedgerSnapshot"] = "ledger:sha256:missing"
        rehash_access_summary(malformed)
        with self.assertRaisesRegex(RuntimeError, "snapshot is missing"):
            collector.validate_producer_trace(malformed)

    def test_access_summary_rejects_canonical_hash_tampering(self) -> None:
        malformed = trace_with_access_summary()
        next(iter(malformed["accessSummaries"].values()))["propertyName"] = "Other"
        with self.assertRaisesRegex(RuntimeError, "propertyIdentity mismatch|canonical hash mismatch"):
            collector.validate_producer_trace(malformed)

    def test_access_set_rejects_non_deduplicated_repeated_access(self) -> None:
        malformed = trace_with_access_summary()
        access_set = next(iter(malformed["accessSets"].values()))
        access_set["propertyReads"].append(access_set["propertyReads"][0])
        with self.assertRaisesRegex(RuntimeError, "sorted unique"):
            collector.validate_producer_trace(malformed)

    def test_access_summary_rejects_dangling_sid_allocation_node(self) -> None:
        malformed = trace_with_access_summary()
        summary = next(iter(malformed["accessSummaries"].values()))
        summary["sidAllocationDelta"] = 1
        summary["relatedAllocationRefs"] = ["allocation:sha256:missing"]
        rehash_access_summary(malformed)
        with self.assertRaisesRegex(RuntimeError, "allocation"):
            collector.validate_producer_trace(malformed)

    def test_missing_checkpoint_is_rejected(self) -> None:
        malformed = copy.deepcopy(trace())
        malformed["ledgerSnapshots"].clear()
        with self.assertRaisesRegex(RuntimeError, "snapshot missing"):
            collector.validate_producer_trace(malformed)

    def test_unclosed_scope_is_rejected(self) -> None:
        malformed = trace()
        malformed["events"].pop(4)
        resequence(malformed)
        with self.assertRaisesRegex(RuntimeError, "unclosed scopes"):
            collector.validate_producer_trace(malformed)

    def test_binding_declares_and_checks_canonical_native_snapshot_hashes(self) -> None:
        request = {"request": True}
        response = {"response": True}
        bound = collector.bind_producer_trace_artifacts(
            producer_trace(),
            input_document=request,
            response_document=response,
        )
        self.assertEqual("FreeCAD", bound["producer"]["name"])
        self.assertEqual(
            "canonical-json-sha256-v1",
            bound["producer"]["snapshotPayloadHashAlgorithm"],
        )
        for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for snapshot in bound[group].values():
                self.assertRegex(snapshot["canonicalPayloadSha256"], r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
