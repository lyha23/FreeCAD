from __future__ import annotations

import copy
from typing import Any

from tools.element_map_producer_trace.validate import canonical_sha256


def producer_trace(*, scope_sequence: int = 1, field_value: str = "same") -> dict[str, Any]:
    initial_payload: dict[str, Any] = {}
    initial_digest = canonical_sha256(initial_payload)
    initial_id = f"stringTable:sha256:{initial_digest}"
    ledger_payload = {"entries": [], "value": field_value}
    ledger_digest = canonical_sha256(ledger_payload)
    ledger_id = f"ledger:sha256:{ledger_digest}"
    events = [
        event(1, 0, 0, "document.recompute.begin", "begin", "request_parsed", initial_id),
        event(2, scope_sequence, 0, "scope.begin", "begin", "scope_entered", initial_id, stage="build"),
        event(3, scope_sequence, 0, "maker.select", "selected", "terminal_element_map_written", initial_id, fields={"value": field_value}),
        event(4, scope_sequence, 0, "maker.final_checkpoint", "published", "value_snapshot", initial_id, ledger_id),
        event(5, scope_sequence, 0, "scope.end", "success", "scope_finished", ledger_id),
        event(6, 0, 0, "document.recompute.end", "success", "recompute_finished", ledger_id, fields={"partialWrite": False}),
    ]
    return {
        "schemaVersion": "freecad.element-map-producer-trace.v1",
        "producer": {
            "name": "CadRs",
            "build": "test",
            "inputSha256": "0" * 64,
            "responseSha256": "1" * 64,
        },
        "transactions": [{
            "sequence": 1,
            "targets": ["Pad"],
            "eventRange": [1, 6],
            "outcome": "success",
            "detail": "recompute_finished",
        }],
        "objectTagIndex": {"2": {"object": "Pad", "typeId": "PartDesign::Pad"}},
        "objects": {"Pad": {"tag": 2, "typeId": "PartDesign::Pad", "slices": [2, 3, 4, 5]}},
        "events": events,
        "stringTableSnapshots": {
            initial_id: snapshot("stringTable", initial_digest, initial_payload, "initial")
        },
        "ledgerSnapshots": {
            ledger_id: snapshot("ledger", ledger_digest, ledger_payload, "maker.final_checkpoint")
        },
        "mapperSnapshots": {},
    }


def bind_trace(trace: dict[str, Any], request: Any, response: Any) -> dict[str, Any]:
    trace["producer"]["inputSha256"] = canonical_sha256(request)
    trace["producer"]["responseSha256"] = canonical_sha256(response)
    return trace


def retag(trace: dict[str, Any], tag: int) -> dict[str, Any]:
    old = trace["objects"]["Pad"]["tag"]
    trace["objects"]["Pad"]["tag"] = tag
    trace["objectTagIndex"] = {
        str(tag): {"object": "Pad", "typeId": "PartDesign::Pad"}
    }
    for item in trace["events"]:
        if item.get("objectTag") == old:
            item["objectTag"] = tag
    return trace


def resequence(trace: dict[str, Any]) -> dict[str, Any]:
    for sequence, item in enumerate(trace["events"], 1):
        item["sequence"] = sequence
    trace["transactions"][0]["eventRange"] = [1, len(trace["events"])]
    by_tag = {
        value["tag"]: name
        for name, value in trace["objects"].items()
        if isinstance(value, dict) and isinstance(value.get("tag"), int)
    }
    for value in trace["objects"].values():
        value["slices"] = []
    for item in trace["events"]:
        name = by_tag.get(item.get("objectTag"))
        if name is not None:
            trace["objects"][name]["slices"].append(item["sequence"])
    return trace


def insert_events(
    trace: dict[str, Any], index: int, values: list[dict[str, Any]]
) -> dict[str, Any]:
    trace["events"][index:index] = copy.deepcopy(values)
    return resequence(trace)


def add_snapshot(
    trace: dict[str, Any], payload: Any, *, kind: str = "ledger", label: str = "test"
) -> str:
    digest = canonical_sha256(payload)
    snapshot_id = f"{kind}:sha256:{digest}"
    group = {
        "ledger": "ledgerSnapshots",
        "stringTable": "stringTableSnapshots",
        "mapper": "mapperSnapshots",
    }[kind]
    trace[group][snapshot_id] = snapshot(kind, digest, copy.deepcopy(payload), label)
    return snapshot_id


def event(
    sequence: int,
    scope: int,
    parent: int,
    slice_name: str,
    decision: str,
    reason: str,
    before: str,
    after: str | None = None,
    *,
    stage: str | None = None,
    fields: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload = copy.deepcopy(fields or {})
    if stage is not None:
        payload["stage"] = stage
    return {
        "sequence": sequence,
        "transactionSequence": 1,
        "scopeSequence": scope,
        "parentScopeSequence": parent,
        "object": "Pad" if scope else "",
        "objectTag": 2 if scope else 0,
        "producer": "PartDesign::Pad" if scope else "",
        "slice": slice_name,
        "decision": decision,
        "reason": reason,
        "fields": payload,
        "beforeSnapshot": before,
        "afterSnapshot": after or before,
    }


def snapshot(kind: str, digest: str, payload: Any, label: str) -> dict[str, Any]:
    return {
        "kind": kind,
        "sha256": digest,
        "payload": payload,
        "label": label,
        "sidRefs": [],
        "definedSids": [],
        "nestedSnapshotRefs": [],
    }
