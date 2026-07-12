"""Canonical semantic paths and event identities for producer-trace comparison."""

from __future__ import annotations

from collections import Counter, defaultdict
from dataclasses import dataclass
import re
from typing import Any, Mapping

from .model import ProjectedEvent, SemanticScope, ValidatedTrace
from .validate import validate_trace


@dataclass(frozen=True)
class ProjectionPolicy:
    commutative_event_slices: frozenset[str] = frozenset(
        {"element_map.find", "element_map.find_all"}
    )
    derived_hash_fields: frozenset[str] = frozenset(
        {"canonicalPayloadSha256", "rawCanonicalSha256"}
    )
    mapper_relation_fields: tuple[str, ...] = ("modified", "generated")
    access_summary_event_slice: str = "property_shape.access_summary"
    derived_access_summary_reason: str = "derived_access_summary_identity"
    independent_owner_reorder_reason: str = "independent_owner_block_reorder"


PROJECTION_POLICY = ProjectionPolicy()


def _stage(event: Mapping[str, Any]) -> str:
    fields = event.get("fields")
    if isinstance(fields, Mapping):
        stage = fields.get("stage")
        if isinstance(stage, str):
            return _normalise_stage(stage)
    return ""


_STAGE_ALIASES = {
    "document.object.execute": "object.execute",
    "document.object.recompute": "object.execute",
    "object.execute": "object.execute",
    "recompute": "document.recompute",
    "SketchObject::buildShape/buildInternals": "sketch.producer",
    "sketch.producer": "sketch.producer",
}


def _normalise_stage(stage: str) -> str:
    return _STAGE_ALIASES.get(stage, stage)


_PRODUCER_KIND_BY_NAME = {
    "Sketcher::SketchObject": "sketch.producer",
    "PartDesign::Body": "partdesign.body_tip",
    "PartDesign::Pad": "partdesign.extrude",
    "PartDesign::Pocket": "partdesign.extrude",
    "PartDesign::Chamfer": "partdesign.dressup",
    "PartDesign::Fillet": "partdesign.dressup",
    "PartDesign::LinearPattern": "partdesign.pattern",
    "PartDesign::PolarPattern": "partdesign.pattern",
    "PartDesign::Mirrored": "partdesign.pattern",
    "PartDesign::Scaled": "partdesign.pattern",
    "PartDesign::MultiTransform": "partdesign.pattern",
    "PartDesign::Transformed": "partdesign.pattern",
    "Part::WireJoiner": "wire_joiner.lifecycle",
    "Part::FaceMaker": "face_maker.lifecycle",
    "Part::FaceMakerBuildFace": "face_maker.lifecycle",
    "Part::TopoShape": "maker.lifecycle",
}


def _producer_kind(producer: str, stage: str) -> str:
    if stage in {"document.recompute", "object.execute"}:
        return stage
    return _PRODUCER_KIND_BY_NAME.get(producer, producer)


def _semantic_stage(producer_kind: str, stage: str) -> str:
    if producer_kind in {
        "sketch.producer",
        "partdesign.extrude",
        "partdesign.dressup",
        "partdesign.pattern",
        "partdesign.body_tip",
        "wire_joiner.lifecycle",
        "face_maker.lifecycle",
        "maker.lifecycle",
    }:
        return producer_kind
    return stage


def _event_semantic_fields(event: Mapping[str, Any]) -> tuple[Any, ...]:
    fields = event.get("fields")
    if not isinstance(fields, Mapping):
        return ()
    source = tuple(
        (key, _freeze(fields[key]))
        for key in ("sourceRole", "sourceShapeRole", "sourceIndexed", "source")
        if key in fields
    )
    target = tuple(
        (key, _freeze(fields[key]))
        for key in (
            "targetRole",
            "targetShapeRole",
            "targetIndexed",
            "target",
            "indexedName",
            "IndexedName",
            "indexed",
        )
        if key in fields
    )
    ordinal = tuple(
        (key, _freeze(fields[key]))
        for key in ("candidateOrdinal", "targetOrdinal", "inputOrdinal", "sourceOrdinal")
        if key in fields
    )
    stable_random = ()
    if fields.get("nondeterminismClass") and "stableComparisonKey" in fields:
        stable_random = (
            ("nondeterminismClass", _freeze(fields["nondeterminismClass"])),
            ("stableComparisonKey", _freeze(fields["stableComparisonKey"])),
        )
    return source + target + ordinal + stable_random


def _freeze(value: Any) -> Any:
    if isinstance(value, Mapping):
        return tuple((key, _freeze(item)) for key, item in sorted(value.items()))
    if isinstance(value, list):
        return tuple(_freeze(item) for item in value)
    return value


_SID_TOKEN = re.compile(r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?")


def _sid_entries(payload: Mapping[str, Any]) -> dict[int, Mapping[str, Any]]:
    best: Any = []
    snapshots = payload.get("stringTableSnapshots")
    if not isinstance(snapshots, Mapping):
        return {}
    for snapshot in snapshots.values():
        if not isinstance(snapshot, Mapping):
            continue
        body = snapshot.get("payload") if isinstance(snapshot.get("payload"), Mapping) else snapshot
        entries = body.get("entries") if isinstance(body, Mapping) else None
        if isinstance(entries, (list, Mapping)) and len(entries) > len(best):
            best = entries
    result: dict[int, Mapping[str, Any]] = {}
    iterable = best.items() if isinstance(best, Mapping) else enumerate(best)
    for key, entry in iterable:
        if not isinstance(entry, Mapping):
            continue
        token = entry.get("token") if isinstance(best, list) else f"#{int(key):x}"
        match = _SID_TOKEN.fullmatch(token) if isinstance(token, str) else None
        if match is None:
            continue
        normalized = dict(entry)
        flags = _flag_map(entry.get("flags"))
        if flags:
            normalized["mapped"] = bool(entry.get("postfix"))
            normalized["appendIndex"] = flags.get("indexed", False) or flags.get(
                "prefixIDIndex", False
            )
        result[int(match.group(1), 16)] = normalized
    return result


def _expand_cad_rs_sids(value: Any, entries: Mapping[int, Mapping[str, Any]]) -> Any:
    if not isinstance(value, str) or not entries:
        return value

    def expand(text: str, active: frozenset[int] = frozenset()) -> str:
        def replace(match: re.Match[str]) -> str:
            sid = int(match.group(1), 16)
            if sid in active:
                return match.group(0)
            entry = entries.get(sid)
            if entry is None:
                return match.group(0)
            data = str(entry.get("data", ""))
            postfix = str(entry.get("postfix", ""))
            raw_index = match.group(2)
            if raw_index and bool(entry.get("mapped")):
                index = int(raw_index, 16)
                split = re.match(r"^(.*?)([0-9]+)$", data)
                base_has_digit = bool(split and any(character.isdigit() for character in split.group(1)))
                append_index = entry.get("appendIndex")
                if append_index is True or (
                    append_index is None and (split is None or not base_has_digit)
                ):
                    data += str(index)
            return expand(data + postfix, active | {sid})

        return _SID_TOKEN.sub(replace, text)

    return expand(value)


def _sid_ref_token(reference: Any) -> str | None:
    if not isinstance(reference, Mapping):
        return None
    value = reference.get("value")
    index = reference.get("index", 0)
    if not isinstance(value, int) or value <= 0 or not isinstance(index, int):
        return None
    return f"#{value:x}" + (f":{index:x}" if index else "")


def _canonical_element_entry(
    fields: Mapping[str, Any], sid_entries: Mapping[int, Mapping[str, Any]]
) -> dict[str, Any]:
    canonical = dict(fields)
    raw = fields.get("raw")
    refs = fields.get("entryLocalRefs", fields.get("elementIdRefs"))
    expanded = _expand_cad_rs_sids(raw, sid_entries)
    if isinstance(refs, list):
        tokens = [_sid_ref_token(reference) for reference in refs]
    elif isinstance(refs, str):
        tokens = [token for token in refs.split(",") if token]
    else:
        tokens = []
    if isinstance(raw, str) and isinstance(expanded, str) and expanded != raw:
        if tokens and all(token is not None and token in raw for token in tokens):
            canonical["raw"] = expanded
            if "entryLocalRefs" in fields:
                canonical["entryLocalRefs"] = ""
            if "elementIdRefs" in fields:
                canonical["elementIdRefs"] = []
    return canonical


def _dedupe_element_entries(entries: list[Any]) -> list[Any]:
    result: list[Any] = []
    seen: set[Any] = set()
    for entry in entries:
        identity = _freeze(entry)
        if identity in seen:
            continue
        seen.add(identity)
        result.append(entry)
    return result


def _canonical_grouped_ledger(
    ledger: Mapping[str, Any], sid_entries: Mapping[int, Mapping[str, Any]]
) -> dict[str, Any]:
    raw_entries = ledger.get("entries")
    if not isinstance(raw_entries, Mapping):
        return dict(ledger)
    grouped: dict[str, Any] = {}
    for kind_name, values in raw_entries.items():
        if not isinstance(values, list):
            grouped[str(kind_name)] = values
            continue
        grouped[str(kind_name)] = [
            {
                **value,
                "entries": _dedupe_element_entries(
                    [
                        _canonical_element_entry(entry, sid_entries)
                        if isinstance(entry, Mapping)
                        else entry
                        for entry in value.get("entries", [])
                    ]
                ),
            }
            if isinstance(value, Mapping)
            else value
            for value in values
        ]
    return {**ledger, "entries": grouped}


def _canonical_ordered_entries(
    value: Any, sid_entries: Mapping[int, Mapping[str, Any]]
) -> Any:
    if not isinstance(value, str):
        return value
    canonical: list[dict[str, Any]] = []
    seen: set[Any] = set()
    for item in value.split("|") if value else []:
        raw, separator, suffix = item.rpartition("[")
        if not separator or not suffix.endswith("]"):
            return value
        refs = [token for token in suffix[:-1].split(",") if token]
        expanded = _expand_cad_rs_sids(raw, sid_entries)
        if not isinstance(expanded, str):
            return value
        remaining_refs = refs
        if expanded != raw and refs and all(token in raw for token in refs):
            remaining_refs = []
        entry = {"raw": expanded, "refs": remaining_refs}
        identity = _freeze(entry)
        if identity in seen:
            continue
        seen.add(identity)
        canonical.append(entry)
    return canonical


def _collapse_equivalent_entry_write_cycles(
    events: list[Mapping[str, Any]],
) -> list[Mapping[str, Any]]:
    result: list[Mapping[str, Any]] = []
    seen_by_scope: dict[int, set[Any]] = {}
    index = 0
    while index < len(events):
        event = events[index]
        scope = int(event.get("scopeSequence", 0))
        if event.get("slice") == "element_map.find_all":
            seen_by_scope[scope] = set()
            result.append(event)
            index += 1
            continue
        if (
            event.get("slice") == "element_map.encode"
            and scope in seen_by_scope
            and index + 2 < len(events)
        ):
            write = events[index + 1]
            checkpoint = events[index + 2]
            if (
                int(write.get("scopeSequence", 0)) == scope
                and int(checkpoint.get("scopeSequence", 0)) == scope
                and write.get("slice") == "element_map.write"
                and checkpoint.get("slice") == "element_map.write_checkpoint"
            ):
                identity = _freeze(write.get("fields", {}))
                if identity in seen_by_scope[scope]:
                    index += 3
                    continue
                seen_by_scope[scope].add(identity)
                result.extend((event, write, checkpoint))
                index += 3
                continue
        result.append(event)
        index += 1
    return result


def _canonical_event(
    event: Mapping[str, Any],
    producer_name: str,
    sid_entries: Mapping[int, Mapping[str, Any]],
) -> Mapping[str, Any]:
    """Join validated producer-specific envelopes onto the native semantic event contract."""

    fields = event.get("fields")
    if not isinstance(fields, Mapping):
        return event
    if event.get("slice") == "hasher.insert" and isinstance(fields.get("result"), Mapping):
        result = fields.get("result")
        value = result.get("value") if isinstance(result, Mapping) else None
        if not isinstance(value, int) or value <= 0:
            return event
        canonical = dict(event)
        canonical["decision"] = {
            "allocation": "allocated",
            "hit": "existing",
        }.get(str(event.get("decision", "")), event.get("decision"))
        canonical["reason"] = {
            "string_id_allocated": "table_insert",
            "string_id_hit": "duplicate_insert",
        }.get(str(event.get("reason", "")), event.get("reason"))
        canonical["fields"] = {
            "data": fields.get("data", ""),
            "id": value,
            "postfix": fields.get("postfix", ""),
        }
        return canonical
    if event.get("slice") == "hasher.insert" and isinstance(fields.get("id"), str):
        try:
            value = int(fields["id"])
        except ValueError:
            return event
        canonical = dict(event)
        canonical["fields"] = {**fields, "id": value}
        return canonical

    if event.get("slice") == "element_map.encode" and event.get("decision") == "preserved":
        canonical = dict(event)
        canonical["fields"] = {
            "before": _expand_cad_rs_sids(fields.get("before", ""), sid_entries)
        }
        return canonical

    if event.get("slice") == "element_map.write":
        fields = _canonical_element_entry(fields, sid_entries)
        canonical = dict(event)
        canonical["fields"] = fields
        event = canonical

    if event.get("slice") == "element_map.find_all":
        canonical = dict(event)
        canonical["fields"] = {
            **fields,
            "orderedEntries": _canonical_ordered_entries(
                fields.get("orderedEntries", ""), sid_entries
            ),
        }
        event = canonical

    if producer_name != "CadRs":
        return event

    if event.get("slice") in {"element_map.encode", "element_map.write", "element_map.find"}:
        refs = fields.get("entryLocalRefs")
        if isinstance(refs, list):
            encoded = []
            for reference in refs:
                if not isinstance(reference, Mapping):
                    return event
                value = reference.get("value")
                index = reference.get("index", 0)
                if not isinstance(value, int) or not isinstance(index, int):
                    return event
                encoded.append(f"#{value:x}" + (f":{index:x}" if index else ""))
            canonical = dict(event)
            canonical["fields"] = {**fields, "entryLocalRefs": ",".join(encoded)}
            return canonical
    return event


def _flag_map(value: Any) -> dict[str, bool]:
    if not isinstance(value, str):
        return {}
    result: dict[str, bool] = {}
    for item in value.split(";"):
        key, separator, raw = item.partition("=")
        if separator and raw in {"0", "1"}:
            result[key] = raw == "1"
    return result


def _string_table_entries(snapshot: Mapping[str, Any]) -> list[dict[str, Any]] | None:
    raw_entries: Any
    payload = snapshot.get("payload")
    if isinstance(payload, Mapping):
        raw_entries = payload.get("entries")
    else:
        raw_entries = snapshot.get("entries")

    if isinstance(raw_entries, Mapping):
        ordered = [raw_entries[key] for key in sorted(raw_entries, key=lambda key: int(key))]
        ids = [int(key) for key in sorted(raw_entries, key=lambda key: int(key))]
        values = {
            value: str(entry.get("data", "")) + str(entry.get("postfix", ""))
            for value, entry in zip(ids, ordered)
            if isinstance(entry, Mapping)
        }
        result: list[dict[str, Any]] = []
        for entry in ordered:
            if not isinstance(entry, Mapping):
                return None
            flags = _flag_map(entry.get("flags"))
            result.append(
                {
                    "data": entry.get("data", ""),
                    "postfix": entry.get("postfix", ""),
                    "postfixed": flags.get("postfixed", False),
                    "postfixEncoded": flags.get("postfixEncoded", False),
                    "indexed": flags.get("indexed", False),
                    "prefixID": flags.get("prefixID", False),
                    "prefixIDIndex": flags.get("prefixIDIndex", False),
                    "related": entry.get("related", []),
                }
            )
        return result

    if not isinstance(raw_entries, list):
        return None
    values: dict[int, str] = {}
    for entry in raw_entries:
        if not isinstance(entry, Mapping):
            return None
        token = entry.get("token")
        if isinstance(token, str) and token.startswith("#"):
            try:
                values[int(token[1:].split(":", 1)[0], 16)] = str(entry.get("data", "")) + str(
                    entry.get("postfix", "")
                )
            except ValueError:
                return None
    result = []
    for entry in raw_entries:
        related = []
        for reference in entry.get("related", []):
            if not isinstance(reference, Mapping):
                return None
            token = reference.get("token")
            if not isinstance(token, str) or not token.startswith("#"):
                return None
            try:
                value = int(token[1:].split(":", 1)[0], 16)
            except ValueError:
                return None
            related.append({"value": value, "index": int(reference.get("index", 0))})
        data = str(entry.get("data", ""))
        postfix = str(entry.get("postfix", ""))
        related_values = {values.get(reference["value"], "") for reference in related}
        mapped = bool(entry.get("mapped"))
        result.append(
            {
                "data": data,
                "postfix": postfix,
                "postfixed": mapped and bool(postfix),
                "postfixEncoded": mapped and bool(postfix) and postfix in related_values,
                "indexed": mapped and bool(postfix) and data in related_values,
                "prefixID": mapped and data.startswith("#") and not data.endswith(":"),
                "prefixIDIndex": mapped and data.startswith("#") and data.endswith(":"),
                "related": related,
            }
        )
    return result


def _canonical_snapshot(
    snapshot_id: str,
    snapshot: Mapping[str, Any],
    group: str,
    *,
    producer_name: str,
    sid_entries: Mapping[int, Mapping[str, Any]],
) -> Mapping[str, Any]:
    kind = snapshot.get("kind")
    if not isinstance(kind, str) or not kind:
        kind = "stringTable" if group == "stringTableSnapshots" else snapshot_id.split(":", 1)[0]
    canonical = dict(snapshot)
    canonical["kind"] = kind
    if kind == "stringTable":
        entries = _string_table_entries(snapshot)
        if entries is not None:
            canonical["payload"] = {"entries": entries}
        elif "payload" in snapshot:
            canonical["payload"] = snapshot.get("payload")
        else:
            canonical["payload"] = None
    elif kind == "ledger" and isinstance(snapshot.get("payload"), Mapping):
        payload = snapshot["payload"]
        shape = payload.get("shape")
        raw_entries = payload.get("entries")
        indexed = shape.get("indexed") if isinstance(shape, Mapping) else None
        if isinstance(indexed, Mapping) and isinstance(raw_entries, Mapping):
            grouped: dict[str, list[dict[str, Any]]] = {}
            for kind_name in ("Vertex", "Edge", "Face"):
                inventory = indexed.get(kind_name, indexed.get(kind_name.lower(), []))
                if not isinstance(inventory, list):
                    continue
                values = []
                for item in inventory:
                    name = item.get("indexed") if isinstance(item, Mapping) else None
                    entries = raw_entries.get(name) if isinstance(name, str) else None
                    if isinstance(name, str) and isinstance(entries, list):
                        canonical_entries = _dedupe_element_entries(
                            [
                                _canonical_element_entry(entry, sid_entries)
                                if isinstance(entry, Mapping)
                                else entry
                                for entry in entries
                            ]
                        )
                        values.append({"indexed": name, "entries": canonical_entries})
                if values:
                    grouped[kind_name] = values
            canonical["payload"] = {"childMaps": [], "entries": grouped}
        elif isinstance(raw_entries, Mapping):
            canonical["payload"] = _canonical_grouped_ledger(payload, sid_entries)
        if isinstance(payload.get("ledger"), Mapping):
            canonical["payload"] = {
                **canonical.get("payload", payload),
                "ledger": _canonical_grouped_ledger(payload["ledger"], sid_entries),
            }
    elif kind == "mapper" and isinstance(snapshot.get("payload"), Mapping):
        canonical["payload"] = _canonical_mapper_payload(snapshot["payload"])
    elif "payload" not in canonical and "entries" in canonical:
        canonical["payload"] = canonical.get("entries")
    return canonical


def _canonical_mapper_payload(payload: Mapping[str, Any]) -> Mapping[str, Any]:
    raw = payload.get("raw")
    if not isinstance(raw, Mapping) or not isinstance(raw.get("sources"), list):
        return payload
    inputs = raw.get("inputs")
    inventory_by_ordinal: dict[int, Any] = {}
    if isinstance(inputs, list):
        for item in inputs:
            if isinstance(item, Mapping) and isinstance(item.get("sourceOrdinal"), int):
                inventory_by_ordinal[item["sourceOrdinal"]] = item.get("inventory")

    groups: dict[Any, dict[str, Any]] = {}
    for source in raw["sources"]:
        if not isinstance(source, Mapping):
            return payload
        ordinal = source.get("sourceOrdinal")
        if not isinstance(ordinal, int):
            return payload
        descriptor = {
            "sourceShapeType": source.get("sourceShapeType"),
            "sourceIndexed": source.get("sourceIndexed"),
            "sourceInventory": inventory_by_ordinal.get(ordinal),
            "queryErrors": source.get("queryErrors"),
        }
        key = _freeze(descriptor)
        group = groups.setdefault(
            key,
            {
                "descriptor": descriptor,
                "sourceCount": 0,
                "modified": Counter(),
                "generated": Counter(),
                "relationValues": {},
                "deletedCount": 0,
            },
        )
        group["sourceCount"] += 1
        group["deletedCount"] += int(source.get("deleted") is True)
        for relation_kind in PROJECTION_POLICY.mapper_relation_fields:
            relations = source.get(relation_kind)
            if not isinstance(relations, list):
                return payload
            for relation in relations:
                frozen = _freeze(relation)
                group[relation_kind][frozen] += 1
                group["relationValues"][(relation_kind, frozen)] = relation

    projected_groups = []
    for key in sorted(groups, key=repr):
        group = groups[key]
        projected_groups.append(
            {
                "descriptor": group["descriptor"],
                "sourceCount": group["sourceCount"],
                "deletedCount": group["deletedCount"],
                "modified": [
                    {
                        "relation": group["relationValues"][("modified", frozen)],
                        "count": count,
                    }
                    for frozen, count in sorted(group["modified"].items(), key=repr)
                ],
                "generated": [
                    {
                        "relation": group["relationValues"][("generated", frozen)],
                        "count": count,
                    }
                    for frozen, count in sorted(group["generated"].items(), key=repr)
                ],
            }
        )
    return {
        **payload,
        "raw": {
            **raw,
            "sources": projected_groups,
        },
    }


def _document_object_contract(
    document_graph: Mapping[str, Any] | None,
) -> dict[str, tuple[str, str, str]] | None:
    if document_graph is None:
        return None
    objects = document_graph.get("Objects")
    if not isinstance(objects, list):
        raise ValueError("document graph /Objects must be an array")
    contract: dict[str, tuple[str, str, str]] = {}
    for index, item in enumerate(objects):
        if not isinstance(item, Mapping):
            raise ValueError(f"document graph /Objects/{index} must be an object")
        name = item.get("Name")
        type_id = item.get("TypeId")
        if not isinstance(name, str) or not name:
            raise ValueError(f"document graph /Objects/{index}/Name must be a non-empty string")
        if not isinstance(type_id, str) or not type_id:
            raise ValueError(f"document graph /Objects/{index}/TypeId must be a non-empty string")
        if name in contract:
            raise ValueError(f"document graph object name is not unique: {name!r}")
        graph_role = item.get("GraphRole", item.get("graphRole", type_id))
        if not isinstance(graph_role, str) or not graph_role:
            raise ValueError(f"document graph /Objects/{index}/GraphRole must be a non-empty string")
        contract[name] = (name, type_id, graph_role)
    return contract


def project_trace(
    trace_or_path: Mapping[str, Any] | str,
    *,
    document_graph: Mapping[str, Any] | None = None,
) -> ValidatedTrace:
    payload = validate_trace(trace_or_path)
    producer = payload.get("producer")
    producer_name = str(producer.get("name", "")) if isinstance(producer, Mapping) else ""
    sid_entries = _sid_entries(payload)
    raw_events = _collapse_equivalent_entry_write_cycles(
        [_canonical_event(event, producer_name, sid_entries) for event in payload["events"]]
    )
    object_keys: dict[str, tuple[str, str, str]] = {}
    object_tags: dict[str, int] = {}
    tag_index = payload.get("objectTagIndex", {})
    trace_object_keys: dict[str, tuple[str, str, str]] = {}
    object_metadata: dict[str, tuple[str, str, int | None]] = {}
    for name, value in payload["objects"].items():
        tag = value.get("tag") if isinstance(value, Mapping) else None
        indexed = tag_index.get(str(tag), {}) if isinstance(tag_index, Mapping) else {}
        type_id = (
            str(value.get("typeId") or indexed.get("typeId") or "")
            if isinstance(value, Mapping) and isinstance(indexed, Mapping)
            else ""
        )
        graph_role = str(value.get("graphRole", type_id)) if isinstance(value, Mapping) else type_id
        object_metadata[name] = (type_id, graph_role, tag if isinstance(tag, int) else None)
        if isinstance(tag, int):
            object_tags[name] = tag

    # Native TopoShape producer scopes use run-local names such as
    # `@transient:-4198`. Their Tag is raw evidence, not cross-run identity.
    # Build a semantic owner from the first observed producer/stage and an
    # ordinal among equivalent transient owners. Unobserved transients remain
    # strict because there is no evidence with which to establish a bijection.
    transient_names = {
        name for name in object_metadata if name.startswith("@transient:")
    }
    transient_semantic_names: dict[str, str] = {}
    transient_counts: Counter[tuple[str, str, str, str]] = Counter()
    tag_to_name = {
        tag: name
        for name, (_type_id, _graph_role, tag) in object_metadata.items()
        if tag is not None
    }
    for event in raw_events:
        owner = str(event.get("object", ""))
        if not owner:
            event_tag = event.get("objectTag")
            owner = tag_to_name.get(event_tag, "") if isinstance(event_tag, int) else ""
        if owner not in transient_names or owner in transient_semantic_names:
            continue
        type_id, graph_role, _tag = object_metadata[owner]
        producer = str(event.get("producer", ""))
        stage = _normalise_stage(_stage(event))
        identity = (producer, stage, type_id, graph_role)
        transient_counts[identity] += 1
        transient_semantic_names[owner] = (
            f"@transient:{producer}|{stage}#{transient_counts[identity]}"
        )

    for name, (type_id, graph_role, _tag) in object_metadata.items():
        semantic_name = transient_semantic_names.get(name, name)
        trace_object_keys[name] = (semantic_name, type_id, graph_role)

    contract = _document_object_contract(document_graph)
    if contract is None:
        object_keys = trace_object_keys
    else:
        for name, key in contract.items():
            trace_key = trace_object_keys.get(name)
            if trace_key is None:
                raise ValueError(f"trace omits document graph object {name!r}")
            if trace_key[1] != key[1]:
                raise ValueError(
                    f"trace object {name!r} typeId {trace_key[1]!r} does not match graph {key[1]!r}"
                )
            if trace_key[2] != key[2]:
                raise ValueError(
                    f"trace object {name!r} graphRole {trace_key[2]!r} does not match graph {key[2]!r}"
                )
        # The graph contract owns the primary object bijection. Producer-local owners and
        # FreeCAD's implicit Origin tree remain available through event fields/lifecycles,
        # but must not masquerade as request graph objects and pre-empt their first event.
        object_keys = contract
        object_tags = {name: tag for name, tag in object_tags.items() if name in contract}

    scopes: dict[int, SemanticScope] = {}
    scope_order: list[int] = []
    sibling_counts: Counter[tuple[int, tuple[str, ...], str, str, str]] = Counter()
    raw_scope_to_semantic: dict[int, int] = {}
    ignored_raw_scopes: set[int] = set()
    auxiliary_raw_scopes: set[int] = set()
    executed_objects: defaultdict[int, set[str]] = defaultdict(set)
    for event in raw_events:
        if event.get("slice") != "scope.begin":
            continue
        sequence = int(event["scopeSequence"])
        raw_parent = int(event.get("parentScopeSequence", 0))
        raw_object_name = str(event.get("object", ""))
        object_name = raw_object_name
        producer = str(event.get("producer", ""))
        stage = _stage(event)
        kind = _producer_kind(producer, stage)
        parent_is_auxiliary = raw_parent in auxiliary_raw_scopes
        is_auxiliary = bool(
            (object_name and object_name not in object_keys)
            or (parent_is_auxiliary and not object_name)
        )
        is_wrapper = kind in {"document.recompute", "object.execute"}
        if is_auxiliary or is_wrapper:
            ignored_raw_scopes.add(sequence)
            if is_auxiliary:
                auxiliary_raw_scopes.add(sequence)
            raw_scope_to_semantic[sequence] = raw_scope_to_semantic.get(raw_parent, 0)
            continue
        parent = raw_scope_to_semantic.get(raw_parent, 0)
        if not object_name and parent:
            object_name = scopes[parent].object_name
        elif object_name in object_keys:
            object_name = object_keys[object_name][0]
        stage = _semantic_stage(kind, stage)
        parent_path = scopes[parent].path if parent else ()
        sibling_key = (
            int(event.get("transactionSequence", 0)),
            parent_path,
            object_name,
            kind,
            stage,
        )
        sibling_counts[sibling_key] += 1
        ordinal = sibling_counts[sibling_key]
        segment = f"{object_name}|{kind}|{stage}#{ordinal}"
        scopes[sequence] = SemanticScope(
            sequence=sequence,
            parent=parent,
            transaction=int(event.get("transactionSequence", 0)),
            object_name=object_name,
            producer=producer,
            producer_kind=kind,
            stage=stage,
            sibling_ordinal=ordinal,
            path=parent_path + (segment,),
        )
        scope_order.append(sequence)
        raw_scope_to_semantic[sequence] = sequence
        if raw_object_name in object_keys:
            executed_objects[int(event.get("transactionSequence", 0))].add(raw_object_name)

    occurrences: Counter[tuple[Any, ...]] = Counter()
    events: list[ProjectedEvent] = []
    events_by_scope: defaultdict[tuple[int, int], list[ProjectedEvent]] = defaultdict(list)
    for event in raw_events:
        raw_scope_sequence = int(event.get("scopeSequence", 0))
        scope_sequence = raw_scope_to_semantic.get(raw_scope_sequence, 0)
        scope_path = scopes[scope_sequence].path if scope_sequence else ()
        base = (
            int(event.get("transactionSequence", 0)),
            scope_path,
            event.get("slice"),
            _event_semantic_fields(event),
        )
        occurrences[base] += 1
        projected = ProjectedEvent(
            raw=event,
            scope_path=scope_path,
            identity=base + (occurrences[base],),
        )
        events.append(projected)
        if (
            raw_scope_sequence not in ignored_raw_scopes
            and event.get("slice") not in {"scope.begin", "scope.end", "scope.abort"}
        ):
            events_by_scope[(int(event.get("transactionSequence", 0)), scope_sequence)].append(projected)

    snapshots: dict[str, Mapping[str, Any]] = {}
    for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
        snapshots.update(
            {
                snapshot_id: _canonical_snapshot(
                    snapshot_id,
                    snapshot,
                    group,
                    producer_name=producer_name,
                    sid_entries=sid_entries,
                )
                for snapshot_id, snapshot in payload[group].items()
            }
        )
    return ValidatedTrace(
        payload=payload,
        transactions=tuple(payload["transactions"]),
        events=tuple(events),
        scopes=scopes,
        scope_order=tuple(scope_order),
        events_by_scope={key: tuple(value) for key, value in events_by_scope.items()},
        snapshots=snapshots,
        object_keys=object_keys,
        object_tags=object_tags,
        executed_objects_by_transaction={
            transaction: frozenset(names) for transaction, names in executed_objects.items()
        },
    )
