"""Independent closure validation for element-map producer trace v1 artifacts."""

from __future__ import annotations

import hashlib
import json
import re
import struct
from collections.abc import Mapping
from pathlib import Path
from typing import Any


SCHEMA = "freecad.element-map-producer-trace.v1"
SNAPSHOT_GROUPS = ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots")
NATIVE_CHECKPOINTED_PRODUCER_PREFIXES = (
    "hasher.",
    "element_map.",
    "mapper.",
    "maker.",
    "face_maker.",
    "wire_joiner.",
    "sketch.",
    "partdesign.",
    "property_shape.",
    "toposhape.",
)


class TraceValidationError(ValueError):
    """The trace is not a closed, self-consistent artifact."""


def canonical_sha256(value: Any) -> str:
    return hashlib.sha256(_canonical_value_bytes(value)).hexdigest()


def canonical_json_sha256(value: Any) -> str:
    """Hash the canonical JSON contract used by the CAD Core CLI sidecar."""

    encoded = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _canonical_value_bytes(value: Any) -> bytes:
    if value is None:
        return b"n;"
    if isinstance(value, bool):
        return b"b1;" if value else b"b0;"
    if isinstance(value, int):
        prefix = b"i" if value < 0 else b"u"
        return prefix + str(value).encode("ascii") + b";"
    if isinstance(value, float):
        if not (float("-inf") < value < float("inf")):
            raise ValueError("canonical producer trace values cannot contain NaN or infinity")
        return b"f" + struct.pack(">d", value).hex().encode("ascii") + b";"
    if isinstance(value, str):
        encoded = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        return f"s{len(encoded)}:".encode("ascii") + encoded
    if isinstance(value, list):
        return f"a{len(value)}[".encode("ascii") + b"".join(
            _canonical_value_bytes(item) for item in value
        ) + b"]"
    if isinstance(value, Mapping):
        items = sorted(value.items())
        return f"o{len(items)}{{".encode("ascii") + b"".join(
            _canonical_value_bytes(str(key)) + _canonical_value_bytes(item)
            for key, item in items
        ) + b"}"
    raise TypeError(f"unsupported canonical producer trace value {type(value).__name__}")


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise TraceValidationError(message)


def _snapshot_index(trace: Mapping[str, Any]) -> dict[str, Mapping[str, Any]]:
    snapshots: dict[str, Mapping[str, Any]] = {}
    for group in SNAPSHOT_GROUPS:
        values = trace.get(group)
        _require(isinstance(values, Mapping), f"{group} must be an object")
        for snapshot_id, snapshot in values.items():
            _require(snapshot_id not in snapshots, f"duplicate snapshot {snapshot_id}")
            _require(isinstance(snapshot, Mapping), f"snapshot {snapshot_id} must be an object")
            snapshots[snapshot_id] = {"_group": group, **snapshot}
    return snapshots


def _validate_snapshots(
    snapshots: Mapping[str, Mapping[str, Any]],
    *,
    strict_actual: bool,
    canonical_json_actual: bool,
    canonical_payload_required: bool,
) -> None:
    for snapshot_id, snapshot in snapshots.items():
        kind = snapshot.get("kind")
        if not isinstance(kind, str) or not kind:
            kind = {
                "stringTableSnapshots": "stringTable",
                "ledgerSnapshots": snapshot_id.split(":", 1)[0],
                "mapperSnapshots": "mapper",
            }.get(snapshot.get("_group"), "")
        digest = snapshot.get("sha256")
        _require(isinstance(kind, str) and kind, f"snapshot {snapshot_id} has no kind")
        _require(isinstance(digest, str) and len(digest) == 64, f"snapshot {snapshot_id} sha256 missing")
        _require(snapshot_id == f"{kind}:sha256:{digest}", f"snapshot hash mismatch {snapshot_id}")
        # CadRs snapshots use canonical JSON hashing. Native FreeCAD snapshots retain their
        # recorder's byte-level digest and are audited by the stored id/sha pair.
        if strict_actual and snapshot.get("payload") is not None:
            payload_digest = (
                canonical_json_sha256(snapshot.get("payload"))
                if canonical_json_actual
                else canonical_sha256(snapshot.get("payload"))
            )
            _require(
                payload_digest == digest,
                f"snapshot payload hash mismatch {snapshot_id}",
            )
        if canonical_payload_required:
            payload = (
                snapshot.get("payload")
                if "payload" in snapshot
                else snapshot.get("entries")
            )
            _require(payload is not None, f"snapshot {snapshot_id} canonical payload missing")
            canonical_digest = snapshot.get("canonicalPayloadSha256")
            _require(
                isinstance(canonical_digest, str) and len(canonical_digest) == 64,
                f"snapshot {snapshot_id} canonicalPayloadSha256 missing",
            )
            _require(
                canonical_json_sha256(payload) == canonical_digest,
                f"snapshot canonical payload hash mismatch {snapshot_id}",
            )
        nested = snapshot.get("nestedSnapshotRefs", [])
        _require(isinstance(nested, list), f"snapshot nested refs are not a list {snapshot_id}")
        for ref in nested:
            _require(ref in snapshots, f"snapshot {snapshot_id} references missing snapshot {ref}")


def _validate_transactions(
    events: list[Mapping[str, Any]],
    transactions: list[Mapping[str, Any]],
    *,
    strict_actual: bool,
) -> None:
    previous_end = 0
    for expected, transaction in enumerate(transactions, 1):
        _require(transaction.get("sequence") == expected, f"transaction sequence {expected} missing")
        event_range = transaction.get("eventRange")
        _require(
            isinstance(event_range, list) and len(event_range) == 2,
            f"transaction {expected} has invalid eventRange",
        )
        begin, end = event_range
        if begin > previous_end + 1:
            gap_owner = 0 if expected == 1 else expected - 1
            gap = events[previous_end : begin - 1]
            _require(
                all(event.get("transactionSequence") == gap_owner for event in gap),
                f"transaction {expected} range gap contains foreign events",
            )
            previous_end = begin - 1
        _require(begin == previous_end + 1 and end >= begin, f"transaction {expected} range gap")
        _require(end <= len(events), f"transaction {expected} range exceeds events")
        selected = events[begin - 1 : end]
        _require(
            all(event.get("transactionSequence") == expected for event in selected),
            f"transaction {expected} contains foreign event",
        )
        _require(selected[0].get("slice") == "document.recompute.begin", "transaction begin missing")
        _require(selected[-1].get("slice") == "document.recompute.end", "transaction end missing")
        if strict_actual:
            semantic_events = [
                event
                for event in selected[:-1]
                if event.get("slice") not in {"scope.end", "scope.abort"}
            ]
            _require(
                bool(semantic_events)
                and semantic_events[-1].get("slice")
                in {"maker.final_checkpoint", "document.recompute.checkpoint"},
                f"transaction {expected} has no final checkpoint",
            )
            end_fields = selected[-1].get("fields")
            _require(isinstance(end_fields, Mapping), f"transaction {expected} end fields missing")
            partial_write = end_fields.get("partialWrite")
            _require(isinstance(partial_write, bool), f"transaction {expected} partialWrite missing")
            _require(
                partial_write == (transaction.get("outcome") != "success"),
                f"transaction {expected} partialWrite/outcome mismatch",
            )
        elif any(
            isinstance(event.get("slice"), str)
            and event["slice"].startswith(NATIVE_CHECKPOINTED_PRODUCER_PREFIXES)
            for event in selected[:-1]
        ):
            _require(
                any(
                    isinstance(event.get("slice"), str)
                    and event.get("slice").endswith("checkpoint")
                    for event in selected[:-1]
                ),
                f"transaction {expected} has no checkpoint",
            )
        previous_end = end
    # Native drain includes post-recompute read/projection evidence after document.recompute.end.
    # Those events retain the last transactionSequence for audit but are deliberately outside its
    # eventRange. They must still pass scope/snapshot/SID closure below.
    _require(previous_end <= len(events), "transaction range exceeds events")


def _validate_scopes(events: list[Mapping[str, Any]]) -> None:
    stack: list[int] = []
    parents: dict[int, int] = {}
    closed: set[int] = set()
    checkpointed: set[int] = set()
    requires_checkpoint: set[int] = set()
    for event in events:
        sequence = event.get("scopeSequence")
        parent = event.get("parentScopeSequence")
        if event.get("slice") == "scope.begin":
            _require(isinstance(sequence, int) and sequence > 0, "scope.begin has no scope sequence")
            _require(sequence not in parents, f"scope {sequence} begins twice")
            expected_parent = stack[-1] if stack else 0
            _require(parent == expected_parent, f"scope {sequence} parent mismatch")
            parents[sequence] = parent
            fields = event.get("fields")
            descriptor = fields.get("descriptor") if isinstance(fields, Mapping) else None
            if isinstance(descriptor, Mapping) and descriptor.get("requiresFinalCheckpoint") is True:
                requires_checkpoint.add(sequence)
            stack.append(sequence)
        elif event.get("slice") in {"scope.end", "scope.abort"}:
            _require(stack and stack[-1] == sequence, f"scope {sequence} closes non-LIFO")
            _require(
                sequence not in requires_checkpoint or sequence in checkpointed,
                f"producer scope {sequence} has no final checkpoint",
            )
            stack.pop()
            closed.add(sequence)
        elif sequence:
            _require(sequence in parents, f"event references unknown scope {sequence}")
            _require(parent == parents[sequence], f"event scope {sequence} parent drift")
            if (
                isinstance(event.get("slice"), str)
                and "checkpoint" in event["slice"]
                and event.get("decision") == "published"
            ):
                checkpointed.add(sequence)
    _require(not stack, f"unclosed scopes {stack}")
    _require(set(parents) == closed, "scope begin/end sets differ")


def _validate_object_indexes(trace: Mapping[str, Any], events: list[Mapping[str, Any]]) -> None:
    tag_index = trace.get("objectTagIndex")
    objects = trace.get("objects")
    _require(isinstance(tag_index, Mapping), "objectTagIndex must be an object")
    _require(isinstance(objects, Mapping), "objects must be an object")
    by_sequence = {event.get("sequence"): event for event in events}
    seen_slices: set[int] = set()
    object_tags: set[str] = set()
    for object_name, value in objects.items():
        _require(isinstance(value, Mapping), f"object index {object_name} is invalid")
        tag = value.get("tag")
        object_tags.add(str(tag))
        _require(str(tag) in tag_index, f"object {object_name} tag is not indexed")
        indexed = tag_index[str(tag)]
        _require(isinstance(indexed, Mapping), f"object {object_name} tag index is invalid")
        _require(indexed.get("object") == object_name, f"object {object_name} tag owner mismatch")
        object_type = value.get("typeId")
        indexed_type = indexed.get("typeId")
        _require(
            isinstance(indexed_type, str) and bool(indexed_type),
            f"object {object_name} tag typeId missing",
        )
        if object_type is not None:
            _require(
                isinstance(object_type, str)
                and object_type == indexed_type,
                f"object {object_name} tag typeId mismatch",
            )
        for sequence in value.get("slices", []):
            _require(sequence not in seen_slices, f"event {sequence} appears in two object slice lists")
            seen_slices.add(sequence)
            event = by_sequence.get(sequence)
            _require(event is not None, f"object {object_name} references missing event {sequence}")
            _require(event.get("objectTag") == tag, f"event {sequence} object tag mismatch")
    _require(
        set(str(key) for key in tag_index) == object_tags,
        "objectTagIndex and objects tag sets differ",
    )
    for event in events:
        tag = event.get("objectTag")
        if tag:
            _require(str(tag) in tag_index, f"event {event.get('sequence')} objectTag is unresolved")
            _require(event.get("sequence") in seen_slices, f"event {event.get('sequence')} missing object slice")


def _validate_transaction_targets(trace: Mapping[str, Any]) -> None:
    objects = trace.get("objects")
    transactions = trace.get("transactions")
    events = trace.get("events")
    _require(isinstance(objects, Mapping), "objects must be an object")
    _require(isinstance(transactions, list), "transactions must be an array")
    _require(isinstance(events, list), "events must be an array")
    for ordinal, transaction in enumerate(transactions, 1):
        _require(isinstance(transaction, Mapping), f"transaction {ordinal} is invalid")
        targets = transaction.get("targets")
        _require(isinstance(targets, list), f"transaction {ordinal} targets must be an array")
        event_range = transaction.get("eventRange")
        selected = (
            events[event_range[0] - 1 : event_range[1]]
            if isinstance(event_range, list) and len(event_range) == 2
            else []
        )
        for target in targets:
            documentless_target = any(
                isinstance(event, Mapping)
                and event.get("slice") == "scope.begin"
                and event.get("object") == target
                and event.get("objectTag") == 0
                and event.get("producer") == "FreeCADCmd.collector"
                and isinstance(event.get("fields"), Mapping)
                and event["fields"].get("stage") == "documentless.producer"
                for event in selected
            )
            _require(
                isinstance(target, str) and (target in objects or documentless_target),
                f"transaction {ordinal} target {target!r} is absent from object index",
            )


def _validate_snapshot_timeline(
    events: list[Mapping[str, Any]],
    snapshots: Mapping[str, Mapping[str, Any]],
    *,
    reset_sid_on_transaction: bool,
) -> None:
    known_sids: set[int] = set()
    transaction_sequence: int | None = None
    for event in events:
        current_transaction = event.get("transactionSequence")
        if reset_sid_on_transaction and current_transaction != transaction_sequence:
            known_sids.clear()
        transaction_sequence = current_transaction
        before = event.get("beforeSnapshot")
        after = event.get("afterSnapshot")
        _require(before in snapshots and after in snapshots, f"event {event.get('sequence')} snapshot missing")
        if event.get("slice") == "document.recompute.begin":
            continue
        if before == after and event.get("decision") != "published":
            continue
        snapshot = snapshots[after]
        defined_sids = list(snapshot.get("definedSids", []))
        if snapshot.get("_group") == "stringTableSnapshots":
            entries = snapshot.get("entries")
            if isinstance(entries, Mapping):
                defined_sids.extend(
                    int(value) for value in entries if str(value).isdigit()
                )
        for value in defined_sids:
            _require(isinstance(value, int) and value > 0, f"snapshot {after} defines invalid SID")
            known_sids.add(value)
        for ref in snapshot.get("sidRefs", []):
            value = ref.get("value", 0) if isinstance(ref, Mapping) else 0
            _require(value <= 0 or value in known_sids, f"snapshot {after} references unknown SID {value}")


def _sid_ref_values(value: Any) -> list[int]:
    """Return SID values from a value that is explicitly a SID reference field."""

    if isinstance(value, int):
        return [value]
    if isinstance(value, Mapping):
        result: list[int] = []
        if isinstance(value.get("value"), int):
            result.append(value["value"])
        for nested in value.values():
            if isinstance(nested, (Mapping, list)):
                result.extend(_sid_ref_values(nested))
        return result
    if isinstance(value, list):
        result: list[int] = []
        for nested in value:
            result.extend(_sid_ref_values(nested))
        return result
    return []


def _validate_event_sid_timeline(events: list[Mapping[str, Any]]) -> None:
    known_sids: set[int] = set()
    ref_keys = {
        "elementIdRefs",
        "entryLocalRefs",
        "orderedEntries",
        "inputRelated",
        "orderedRelated",
        "related",
        "tupleId",
    }
    for event in events:
        if event.get("slice") == "document.recompute.begin":
            known_sids.clear()
        fields = event.get("fields")
        _require(isinstance(fields, Mapping), f"event {event.get('sequence')} fields must be an object")

        for key in ref_keys:
            if key not in fields:
                continue
            for value in _sid_ref_values(fields[key]):
                _require(value <= 0 or value in known_sids, f"event {event.get('sequence')} references unknown SID {value}")
        prefix_id = fields.get("prefixId")
        if isinstance(prefix_id, int):
            _require(prefix_id <= 0 or prefix_id in known_sids, f"event {event.get('sequence')} references unknown SID {prefix_id}")

        if event.get("slice") == "hasher.insert":
            result = fields.get("result")
            value = result.get("value") if isinstance(result, Mapping) else None
            if value is None:
                raw_id = fields.get("id")
                value = int(raw_id) if isinstance(raw_id, str) and raw_id.isdigit() else None
            _require(isinstance(value, int) and value > 0, f"event {event.get('sequence')} has invalid SID result")
            if event.get("decision") in {"allocation", "allocated"}:
                _require(value not in known_sids, f"event {event.get('sequence')} reallocates SID {value}")
                known_sids.add(value)
            elif event.get("decision") == "hit":
                _require(value in known_sids, f"event {event.get('sequence')} hits unknown SID {value}")


_NATIVE_SID_TOKEN = re.compile(r"#([0-9a-fA-F]+)(?::[-]?\d+)?")


def _native_event_sid_refs(value: Any) -> list[int]:
    if isinstance(value, str):
        return [int(match.group(1), 16) for match in _NATIVE_SID_TOKEN.finditer(value)]
    if isinstance(value, Mapping):
        result: list[int] = []
        for nested in value.values():
            result.extend(_native_event_sid_refs(nested))
        return result
    if isinstance(value, list):
        result: list[int] = []
        for nested in value:
            result.extend(_native_event_sid_refs(nested))
        return result
    return []


def _validate_native_event_sid_timeline(events: list[Mapping[str, Any]]) -> None:
    known_sids: set[int] = set()
    for event in events:
        fields = event.get("fields")
        _require(isinstance(fields, Mapping), f"event {event.get('sequence')} fields must be an object")
        for value in _native_event_sid_refs(fields):
            _require(
                value <= 0 or value in known_sids,
                f"event {event.get('sequence')} references unknown native SID {value}",
            )
        if event.get("slice") != "hasher.insert":
            continue
        raw_id = fields.get("id")
        _require(
            isinstance(raw_id, str) and raw_id.isdigit() and int(raw_id) > 0,
            f"event {event.get('sequence')} has invalid native SID result",
        )
        value = int(raw_id)
        decision = event.get("decision")
        if decision == "allocated":
            _require(value not in known_sids, f"event {event.get('sequence')} reallocates native SID {value}")
            known_sids.add(value)
        elif decision == "existing":
            _require(value in known_sids, f"event {event.get('sequence')} hits unknown native SID {value}")
        else:
            raise TraceValidationError(
                f"event {event.get('sequence')} has invalid native SID decision {decision!r}"
            )


def _validate_identity_lifecycle(events: list[Mapping[str, Any]], *, strict_actual: bool) -> None:
    if not strict_actual:
        return
    identities: dict[str, tuple[str, str]] = {}
    semantic_keys: set[tuple[str, str, str]] = set()
    for event in events:
        if event.get("slice") != "trace.identity":
            continue
        fields = event.get("fields")
        _require(isinstance(fields, Mapping), f"event {event.get('sequence')} identity fields missing")
        identity = fields.get("identity")
        kind = fields.get("kind")
        role = fields.get("role")
        related = fields.get("relatedIdentity", "")
        lifecycle = event.get("decision")
        _require(isinstance(identity, str) and identity, f"event {event.get('sequence')} identity missing")
        _require(isinstance(kind, str) and kind, f"event {event.get('sequence')} identity kind missing")
        _require(isinstance(role, str) and role, f"event {event.get('sequence')} identity role missing")
        _require(lifecycle in {"create", "copy", "share", "reset", "drop"}, f"event {event.get('sequence')} identity lifecycle invalid")
        _require(identity not in identities, f"trace identity {identity} is allocated twice")
        semantic_key = (kind, role, str(lifecycle))
        _require(semantic_key not in semantic_keys, f"trace identity semantic lifecycle is one-to-many {semantic_key}")
        if lifecycle == "create":
            _require(not related, f"created trace identity {identity} unexpectedly has a source")
        else:
            _require(
                isinstance(related, str) and related in identities,
                f"trace identity {identity} references unknown source {related}",
            )
        identities[identity] = (kind, role)
        semantic_keys.add(semantic_key)


def _indexed_inventory(shape: Any) -> dict[str, set[str]]:
    if not isinstance(shape, Mapping):
        return {}
    indexed = shape.get("indexed")
    if not isinstance(indexed, Mapping):
        return {}
    result: dict[str, set[str]] = {}
    for kind, entries in indexed.items():
        if not isinstance(entries, list):
            continue
        result[str(kind).lower()] = {
            str(entry.get("indexed", "")).lower()
            for entry in entries
            if isinstance(entry, Mapping) and entry.get("indexed")
        }
    return result


def _snapshot_defined_sids(snapshot: Mapping[str, Any]) -> set[int]:
    result = {
        value for value in snapshot.get("definedSids", []) if isinstance(value, int) and value > 0
    }
    payload = snapshot.get("payload")
    if isinstance(payload, Mapping):
        entries = payload.get("entries")
        if isinstance(entries, list):
            result.update(
                entry.get("value")
                for entry in entries
                if isinstance(entry, Mapping)
                and isinstance(entry.get("value"), int)
                and entry["value"] > 0
            )
    return result


def _validate_child_ranges_and_mapper(
    snapshots: Mapping[str, Mapping[str, Any]], *, strict_actual: bool
) -> None:
    for snapshot_id, snapshot in snapshots.items():
        payload = snapshot.get("payload")
        if not isinstance(payload, Mapping):
            continue
        inventory = _indexed_inventory(payload.get("shape"))
        child_ranges = payload.get("childRanges", [])
        if strict_actual and "childRanges" in payload:
            _require(
                isinstance(payload.get("canonicalCollisions"), list),
                f"snapshot {snapshot_id} collision inventory missing",
            )
        for child in child_ranges:
            _require(isinstance(child, Mapping), f"snapshot {snapshot_id} child range invalid")
            offset = child.get("offset")
            count = child.get("count")
            _require(isinstance(offset, int) and offset >= 0, f"snapshot {snapshot_id} child offset invalid")
            _require(isinstance(count, int) and count >= 0, f"snapshot {snapshot_id} child count invalid")
            kind = str(child.get("kind", "")).lower()
            _require(bool(kind), f"snapshot {snapshot_id} child kind missing")
            if strict_actual:
                _require(offset + count <= len(inventory.get(kind, set())), f"snapshot {snapshot_id} child range exceeds {kind} inventory")
            if strict_actual and count > 0:
                prefix = {"vertex": "Vertex", "edge": "Edge", "face": "Face"}.get(kind, kind.title())
                _require(child.get("targetStart") == f"{prefix}{offset + 1}", f"snapshot {snapshot_id} child targetStart mismatch")
                _require(child.get("targetEnd") == f"{prefix}{offset + count}", f"snapshot {snapshot_id} child targetEnd mismatch")
            if strict_actual:
                nested_snapshot = child.get("nestedSnapshot")
                _require(
                    isinstance(nested_snapshot, str)
                    and bool(nested_snapshot)
                    and child.get("nestedSnapshotStatus") == "published"
                    and nested_snapshot in snapshots
                    and nested_snapshot in snapshot.get("nestedSnapshotRefs", []),
                    f"snapshot {snapshot_id} child nested snapshot missing",
                )
                nested_defined_sids: set[int] = set()
                nested_ledger = snapshots[nested_snapshot]
                for table_ref in nested_ledger.get("nestedSnapshotRefs", []):
                    table_snapshot = snapshots.get(table_ref)
                    if table_snapshot is not None:
                        nested_defined_sids.update(_snapshot_defined_sids(table_snapshot))
                for value in _sid_ref_values(child.get("sourceElementIdRefs", [])):
                    _require(
                        value <= 0 or value in nested_defined_sids,
                        f"snapshot {snapshot_id} child references unknown nested SID {value}",
                    )

        entries = payload.get("entries")
        if strict_actual and isinstance(entries, Mapping):
            nested_sids: set[int] = set()
            for nested in snapshot.get("nestedSnapshotRefs", []):
                nested_snapshot = snapshots.get(nested)
                if nested_snapshot is not None:
                    nested_sids.update(_snapshot_defined_sids(nested_snapshot))
            for indexed_name, values in entries.items():
                name = str(indexed_name).lower()
                kind = next((candidate for candidate in ("vertex", "edge", "face") if name.startswith(candidate)), "")
                _require(bool(kind) and name in inventory.get(kind, set()), f"snapshot {snapshot_id} ledger entry {indexed_name} is absent from shape inventory")
                _require(isinstance(values, list), f"snapshot {snapshot_id} ledger entry {indexed_name} must be a list")
                for entry in values:
                    _require(isinstance(entry, Mapping), f"snapshot {snapshot_id} ledger entry {indexed_name} is invalid")
                    for value in _sid_ref_values(entry.get("elementIdRefs", [])):
                        _require(value <= 0 or value in nested_sids, f"snapshot {snapshot_id} ledger entry {indexed_name} references unknown SID {value}")
        raw = payload.get("raw")
        if not isinstance(raw, Mapping):
            continue
        output_inventory = _indexed_inventory(raw.get("output"))
        output_names = {name for values in output_inventory.values() for name in values}
        inputs = raw.get("inputs")
        if not strict_actual:
            for source in raw.get("sources", []):
                for relation in ("modified", "generated"):
                    for target in source.get(relation, []):
                        indexed = str(target.get("indexed", "")).lower()
                        if indexed:
                            _require(indexed in output_names, f"mapper target {indexed} is absent")
                            continue
                        status = target.get("relationStatus")
                        members = target.get("outputMembers")
                        _require(
                            (status == "not_in_output" and isinstance(members, list) and not members)
                            or (status == "expanded_to_output" and isinstance(members, list) and members),
                            "unknown mapper relation",
                        )
            continue
        _require(isinstance(inputs, list), f"mapper snapshot {snapshot_id} inputs missing")
        input_inventories: dict[int, dict[str, set[str]]] = {}
        for input_value in inputs:
            _require(isinstance(input_value, Mapping), f"mapper snapshot {snapshot_id} input invalid")
            ordinal = input_value.get("sourceOrdinal")
            _require(isinstance(ordinal, int) and ordinal >= 0, f"mapper snapshot {snapshot_id} input ordinal invalid")
            input_inventories[ordinal] = _indexed_inventory(input_value.get("inventory"))
        for source in raw.get("sources", []):
            _require(isinstance(source, Mapping), f"mapper snapshot {snapshot_id} source invalid")
            source_ordinal = source.get("sourceOrdinal")
            source_kind = str(source.get("sourceShapeType", "")).lower()
            source_indexed = str(source.get("sourceIndexed", "")).lower()
            _require(source_ordinal in input_inventories, f"mapper source ordinal {source_ordinal} is absent")
            _require(source_indexed in input_inventories[source_ordinal].get(source_kind, set()), f"mapper source {source_indexed} is absent")
            for relation in ("modified", "generated"):
                for target in source.get(relation, []):
                    indexed = str(target.get("indexed", "")).lower()
                    if indexed:
                        _require(indexed in output_names, f"mapper target {indexed} is absent")
                        _require(target.get("relationStatus") == "resolved", f"mapper target {indexed} is unresolved")
                        continue
                    status = target.get("relationStatus")
                    members = target.get("outputMembers")
                    if status == "not_in_output":
                        _require(isinstance(members, list) and not members, "mapper not_in_output relation has members")
                        continue
                    _require(status == "expanded_to_output", "unknown mapper relation")
                    _require(isinstance(members, list) and members, "mapper expanded relation has no members")
                    for member in members:
                        _require(isinstance(member, Mapping), "mapper output member is invalid")
                        member_indexed = str(member.get("indexed", "")).lower()
                        _require(member_indexed in output_names, f"mapper output member {member_indexed} is absent")


def validate_trace(
    trace_or_path: Mapping[str, Any] | str | Path,
    *,
    input_document: Any | None = None,
    response_document: Any | None = None,
    input_bytes: bytes | None = None,
    response_bytes: bytes | None = None,
) -> dict[str, Any]:
    """Validate closure and optional same-run artifact bindings, returning a plain dict."""

    if isinstance(trace_or_path, (str, Path)):
        trace = json.loads(Path(trace_or_path).read_text(encoding="utf-8"))
    else:
        trace = dict(trace_or_path)
    _require(trace.get("schemaVersion") == SCHEMA, "unsupported trace schema")
    producer = trace.get("producer")
    _require(isinstance(producer, Mapping), "producer metadata missing")
    producer_name = producer.get("name")
    native_producer = (
        producer_name == "FreeCAD"
        or (producer_name is None and isinstance(producer.get("document"), str))
    )
    bound_native = (
        producer_name == "FreeCAD"
        and isinstance(producer.get("inputSha256"), str)
        and isinstance(producer.get("responseSha256"), str)
    )
    canonical_native_snapshots = (
        bound_native
        and producer.get("snapshotPayloadHashAlgorithm")
        == "canonical-json-sha256-v1"
    )
    _require(
        native_producer or producer_name in {"CADCore", "CadRs", "FreeCAD"},
        "producer name missing",
    )
    if not native_producer:
        for field in ("inputSha256", "responseSha256"):
            _require(isinstance(producer.get(field), str), f"producer {field} missing")

    events = trace.get("events")
    transactions = trace.get("transactions")
    _require(isinstance(events, list), "events must be an array")
    _require(isinstance(transactions, list), "transactions must be an array")
    for expected, event in enumerate(events, 1):
        _require(isinstance(event, Mapping), f"event {expected} is invalid")
        _require(event.get("sequence") == expected, f"event sequence {expected} missing")
        _require(bool(event.get("slice")), f"event {expected} slice missing")
        _require(
            isinstance(event.get("decision"), str)
            and isinstance(event.get("reason"), str)
            and (native_producer or bool(event.get("decision"))),
            f"event {expected} decision missing",
        )

    snapshots = _snapshot_index(trace)
    _validate_snapshots(
        snapshots,
        strict_actual=not native_producer,
        canonical_json_actual=producer_name == "CADCore",
        canonical_payload_required=canonical_native_snapshots,
    )
    _validate_transactions(events, transactions, strict_actual=not native_producer)
    _validate_scopes(events)
    _validate_object_indexes(trace, events)
    _validate_transaction_targets(trace)
    _validate_snapshot_timeline(
        events,
        snapshots,
        reset_sid_on_transaction=not native_producer,
    )
    if canonical_native_snapshots:
        _validate_native_event_sid_timeline(events)
    elif not native_producer:
        _validate_event_sid_timeline(events)
    _validate_identity_lifecycle(events, strict_actual=not native_producer)
    _validate_child_ranges_and_mapper(snapshots, strict_actual=not native_producer)

    if input_bytes is not None:
        _require(
            isinstance(producer.get("inputSha256"), str),
            "producer trace does not carry inputSha256 artifact binding",
        )
        _require(
            producer.get("inputSha256") == hashlib.sha256(input_bytes).hexdigest(),
            "inputSha256 does not bind the supplied request bytes",
        )
    elif input_document is not None:
        input_digest = (
            canonical_json_sha256(input_document)
            if producer_name in {"CADCore", "FreeCAD"} or native_producer
            else canonical_sha256(input_document)
        )
        _require(
            producer.get("inputSha256") == input_digest,
            "inputSha256 does not bind the supplied request",
        )
    if response_bytes is not None:
        _require(
            isinstance(producer.get("responseSha256"), str),
            "producer trace does not carry responseSha256 artifact binding",
        )
        _require(
            producer.get("responseSha256") == hashlib.sha256(response_bytes).hexdigest(),
            "responseSha256 does not bind the supplied response bytes",
        )
    elif response_document is not None:
        response_digest = (
            canonical_json_sha256(response_document)
            if producer_name in {"CADCore", "FreeCAD"} or native_producer
            else canonical_sha256(response_document)
        )
        _require(
            producer.get("responseSha256") == response_digest,
            "responseSha256 does not bind the supplied response",
        )
    return trace


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--response", type=Path)
    args = parser.parse_args()
    input_document = json.loads(args.input.read_text(encoding="utf-8")) if args.input else None
    response_document = json.loads(args.response.read_text(encoding="utf-8")) if args.response else None
    validate_trace(args.trace, input_document=input_document, response_document=response_document)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
