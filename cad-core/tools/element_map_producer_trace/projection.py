"""Canonical semantic paths and event identities for producer-trace comparison."""

from __future__ import annotations

from collections import Counter, defaultdict
from typing import Any, Mapping

from .model import ProjectedEvent, SemanticScope, ValidatedTrace
from .validate import validate_trace


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
    raw_events = payload["events"]
    object_keys: dict[str, tuple[str, str, str]] = {}
    object_tags: dict[str, int] = {}
    tag_index = payload.get("objectTagIndex", {})
    trace_object_keys: dict[str, tuple[str, str, str]] = {}
    for name, value in payload["objects"].items():
        tag = value.get("tag") if isinstance(value, Mapping) else None
        indexed = tag_index.get(str(tag), {}) if isinstance(tag_index, Mapping) else {}
        type_id = (
            str(value.get("typeId") or indexed.get("typeId") or "")
            if isinstance(value, Mapping) and isinstance(indexed, Mapping)
            else ""
        )
        graph_role = str(value.get("graphRole", type_id)) if isinstance(value, Mapping) else type_id
        trace_object_keys[name] = (name, type_id, graph_role)
        if isinstance(tag, int):
            object_tags[name] = tag

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
        object_name = str(event.get("object", ""))
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
        if object_name in object_keys:
            executed_objects[int(event.get("transactionSequence", 0))].add(object_name)

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
        snapshots.update(payload[group])
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
