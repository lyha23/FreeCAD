"""Ordered first-divergence comparison for independently validated producer traces."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any, Mapping, Sequence

from .model import ComparisonResult, ProjectedEvent, SemanticScope, ValidatedTrace
from .projection import _normalise_stage, _producer_kind, project_trace
from .validate import TraceValidationError


LOOK_AHEAD = 3


SLICE_OWNERS = {
    "hasher.": "app/string_hasher",
    "element_map.": "part/topo_shape",
    "mapper.": "part/topo_shape_mapper",
    "maker.": "part/topo_shape",
    "face_maker.": "part/face_maker",
    "wire_joiner.": "part/wire_joiner",
    "sketch.": "sketcher/sketch_object",
    "partdesign.extrude": "part_design/feature_extrude",
    "partdesign.dressup": "part_design/feature_dress_up",
    "partdesign.pattern": "part_design/feature_transformed",
    "partdesign.body_tip": "part_design/body",
    "property_shape.": "part/topo_shape",
    "toposhape.": "part/topo_shape",
    "reference.": "runtime/reference_resolution",
    "document.": "runtime/recompute",
    "scope.": "runtime/recompute",
    "trace.identity": "app/element_map_producer_trace",
}

PRODUCER_KIND_OWNERS = {
    "sketch.producer": "sketcher/sketch_object",
    "partdesign.extrude": "part_design/feature_extrude",
    "partdesign.dressup": "part_design/feature_dress_up",
    "partdesign.pattern": "part_design/feature_transformed",
    "partdesign.body_tip": "part_design/body",
    "wire_joiner.lifecycle": "part/wire_joiner",
    "face_maker.lifecycle": "part/face_maker",
    "maker.lifecycle": "part/topo_shape",
    "document.recompute": "runtime/recompute",
    "object.execute": "runtime/recompute",
}


SID_CONTEXT_KEYS = {
    "elementidrefs",
    "entrylocalrefs",
    "inputrelated",
    "orderedrelated",
    "related",
    "sidrefs",
    "tupleid",
    "prefixid",
    "prefixidindex",
}
NONDETERMINISTIC_RAW_KEYS = {"rawSuffix", "nondeterministicRaw"}


@dataclass
class _Bijection:
    label: str
    left_to_right: dict[Any, Any] = field(default_factory=dict)
    right_to_left: dict[Any, Any] = field(default_factory=dict)

    def bind(self, left: Any, right: Any) -> str | None:
        if left in self.left_to_right and self.left_to_right[left] != right:
            return f"{self.label} {left!r} maps to both {self.left_to_right[left]!r} and {right!r}"
        if right in self.right_to_left and self.right_to_left[right] != left:
            return f"{self.label} {right!r} maps from both {self.right_to_left[right]!r} and {left!r}"
        self.left_to_right[left] = right
        self.right_to_left[right] = left
        return None


@dataclass
class _Mappings:
    object_names: _Bijection = field(default_factory=lambda: _Bijection("object"))
    tags: _Bijection = field(default_factory=lambda: _Bijection("Tag"))
    sids: _Bijection = field(default_factory=lambda: _Bijection("SID"))
    identities: _Bijection = field(default_factory=lambda: _Bijection("trace identity"))


def _owner(event: ProjectedEvent | None) -> str:
    if event is None:
        return "unknown"
    slice_name = str(event.raw.get("slice", ""))
    for prefix, owner in SLICE_OWNERS.items():
        if slice_name.startswith(prefix):
            return owner
    return "runtime/recompute"


def _scope_owner(scope: SemanticScope) -> str:
    return PRODUCER_KIND_OWNERS.get(scope.producer_kind, "runtime/recompute")


def _result(
    classification: str,
    expected: ProjectedEvent | None,
    actual: ProjectedEvent | None,
    *,
    status: str = "different",
    pointer: str | None = None,
    expected_value: Any = None,
    actual_value: Any = None,
    downstream: int = 0,
    before_alignment: str = "not_compared",
    after_alignment: str = "not_compared",
    detail: str = "",
) -> ComparisonResult:
    event = expected or actual
    transaction = int(event.raw.get("transactionSequence", 0)) if event else None
    return ComparisonResult(
        status=status,
        classification=classification,
        transaction_ordinal=transaction,
        semantic_scope_path=event.scope_path if event else (),
        expected_event=expected.raw if expected else None,
        actual_event=actual.raw if actual else None,
        expected_event_identity=expected.identity if expected else None,
        actual_event_identity=actual.identity if actual else None,
        json_pointer=pointer,
        expected_value=expected_value,
        actual_value=actual_value,
        downstream_drift_count=downstream,
        before_alignment=before_alignment,
        after_alignment=after_alignment,
        owner=_owner(event),
        detail=detail,
    )


def _invalid(side: str, exc: Exception) -> ComparisonResult:
    detail = str(exc)
    classification = f"invalid_{side}_trace"
    if "final checkpoint" in detail or "no checkpoint" in detail:
        classification = "final_checkpoint_missing"
        detail = f"{side}: {detail}"
    return ComparisonResult(status="invalid", classification=classification, detail=detail)


def _pointer_token(value: Any) -> str:
    return str(value).replace("~", "~0").replace("/", "~1")


def _first_difference(expected: Any, actual: Any, pointer: str = "") -> tuple[str, Any, Any] | None:
    if type(expected) is not type(actual):
        return pointer or "/", expected, actual
    if isinstance(expected, Mapping):
        keys = sorted(set(expected) | set(actual))
        for key in keys:
            child = f"{pointer}/{_pointer_token(key)}"
            if key not in expected:
                return child, None, actual[key]
            if key not in actual:
                return child, expected[key], None
            difference = _first_difference(expected[key], actual[key], child)
            if difference:
                return difference
        return None
    if isinstance(expected, list):
        for index in range(max(len(expected), len(actual))):
            child = f"{pointer}/{index}"
            if index >= len(expected):
                return child, None, actual[index]
            if index >= len(actual):
                return child, expected[index], None
            difference = _first_difference(expected[index], actual[index], child)
            if difference:
                return difference
        return None
    if expected != actual:
        return pointer or "/", expected, actual
    return None


def _path_kind(path: tuple[str, ...], slice_name: str) -> str | None:
    lowered = [value.lower() for value in path]
    leaf = lowered[-1] if lowered else ""
    if leaf.endswith("tag") or leaf in {"objecttag", "mastertag", "inputtag", "outputtag"}:
        return "tag"
    if "identity" in leaf and leaf not in {"identitystatus"}:
        return "identity"
    if any(value in SID_CONTEXT_KEYS for value in lowered):
        return "sid"
    if slice_name.startswith("hasher.") and leaf in {"value", "prefixid", "prefixidindex"}:
        return "sid"
    return None


def _mapped_string(value: str, mappings: _Mappings, *, actual: bool) -> str:
    if not actual:
        return value

    def sid(match: re.Match[str]) -> str:
        raw = int(match.group(1))
        mapped = mappings.sids.right_to_left.get(raw)
        return f"#{mapped if mapped is not None else raw}"

    def tag(match: re.Match[str]) -> str:
        raw = int(match.group(1))
        mapped = mappings.tags.right_to_left.get(raw)
        return f"H{mapped if mapped is not None else raw}"

    return re.sub(r"#(-?\d+)", sid, re.sub(r"H(-?\d+)", tag, value))


def _project_pair(
    expected: Any,
    actual: Any,
    *,
    path: tuple[str, ...],
    slice_name: str,
    mappings: _Mappings,
    nondeterminism: tuple[Any, Any] | None = None,
) -> tuple[Any, Any, str | None]:
    if type(expected) is not type(actual):
        return expected, actual, None
    if isinstance(expected, Mapping):
        left: dict[str, Any] = {}
        right: dict[str, Any] = {}
        keys = sorted(set(expected) | set(actual))
        for key in keys:
            if key not in expected or key not in actual:
                if key in expected:
                    left[key] = expected[key]
                if key in actual:
                    right[key] = actual[key]
                continue
            if (
                key in NONDETERMINISTIC_RAW_KEYS
                and nondeterminism is not None
                and nondeterminism[0] == nondeterminism[1]
            ):
                left[key] = nondeterminism[0]
                right[key] = nondeterminism[0]
                continue
            left_value, right_value, error = _project_pair(
                expected[key],
                actual[key],
                path=path + (str(key),),
                slice_name=slice_name,
                mappings=mappings,
                nondeterminism=nondeterminism,
            )
            if error:
                return expected, actual, error
            if key == "stage" and isinstance(left_value, str) and isinstance(right_value, str):
                left_value = _normalise_stage(left_value)
                right_value = _normalise_stage(right_value)
            left[str(key)] = left_value
            right[str(key)] = right_value
        return left, right, None
    if isinstance(expected, list):
        left: list[Any] = []
        right: list[Any] = []
        for index in range(min(len(expected), len(actual))):
            left_value, right_value, error = _project_pair(
                expected[index],
                actual[index],
                path=path + (str(index),),
                slice_name=slice_name,
                mappings=mappings,
                nondeterminism=nondeterminism,
            )
            if error:
                return expected, actual, error
            left.append(left_value)
            right.append(right_value)
        left.extend(expected[len(left) :])
        right.extend(actual[len(right) :])
        return left, right, None

    kind = _path_kind(path, slice_name)
    if kind and expected is not None and actual is not None:
        mapping = {
            "tag": mappings.tags,
            "sid": mappings.sids,
            "identity": mappings.identities,
        }[kind]
        if expected in mapping.left_to_right or actual in mapping.right_to_left:
            projected_actual = mapping.right_to_left.get(actual, actual)
            return expected, projected_actual, None
        error = mapping.bind(expected, actual)
        if error:
            return expected, actual, error
        return expected, expected, None
    if isinstance(expected, str) and isinstance(actual, str):
        return expected, _mapped_string(actual, mappings, actual=True), None
    return expected, actual, None


def _expand_snapshot_refs(
    trace: ValidatedTrace,
    value: Any,
    *,
    active: frozenset[str] = frozenset(),
) -> Any:
    """Replace producer-local snapshot ids with their independently validated values."""

    if isinstance(value, str) and value in trace.snapshots:
        snapshot = trace.snapshots[value]
        kind = snapshot.get("kind", value.split(":", 1)[0])
        if value in active:
            return {"kind": kind, "recursiveRef": True}
        return {
            "kind": kind,
            "payload": _expand_snapshot_refs(
                trace,
                snapshot.get("payload"),
                active=active | {value},
            ),
        }
    if isinstance(value, Mapping):
        return {
            str(key): _expand_snapshot_refs(trace, item, active=active)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [_expand_snapshot_refs(trace, item, active=active) for item in value]
    return value


def _snapshot_payload(trace: ValidatedTrace, snapshot_id: str) -> Any:
    snapshot = trace.snapshots.get(snapshot_id)
    if snapshot is None:
        return None
    return _expand_snapshot_refs(trace, snapshot.get("payload"), active=frozenset({snapshot_id}))


def _classification_for_pointer(pointer: str, slice_name: str) -> str:
    lowered = pointer.lower()
    if "childrange" in lowered or "targetstart" in lowered or "targetend" in lowered:
        return "child_range_mismatch"
    if "mapper" in lowered or "/raw/" in lowered or any(
        token in lowered for token in ("/modified", "/generated", "/deleted", "candidate")
    ):
        return "mapper_relation_mismatch"
    if "inventory" in lowered or "/target" in lowered:
        return "target_inventory_mismatch"
    if any(token in lowered for token in ("related", "refs", "ordered")):
        return "ordered_refs_mismatch"
    if slice_name.startswith("hasher.") or "stringtable" in lowered or "/sid" in lowered:
        return "sid_allocation_mismatch"
    return "field_mismatch"


def _compare_value(
    expected: Any,
    actual: Any,
    *,
    pointer: str,
    slice_name: str,
    mappings: _Mappings,
    nondeterminism: tuple[Any, Any] | None = None,
) -> tuple[tuple[str, Any, Any] | None, str | None]:
    left, right, error = _project_pair(
        expected,
        actual,
        path=tuple(part for part in pointer.split("/") if part),
        slice_name=slice_name,
        mappings=mappings,
        nondeterminism=nondeterminism,
    )
    if error:
        return None, error
    return _first_difference(left, right, pointer), None


def _compare_matched_event(
    expected_trace: ValidatedTrace,
    actual_trace: ValidatedTrace,
    expected: ProjectedEvent,
    actual: ProjectedEvent,
    mappings: _Mappings,
    downstream: int,
) -> ComparisonResult | None:
    slice_name = str(expected.raw.get("slice", ""))
    expected_fields = _expand_snapshot_refs(expected_trace, expected.raw.get("fields"))
    actual_fields = _expand_snapshot_refs(actual_trace, actual.raw.get("fields"))
    nondeterminism: tuple[Any, Any] | None = None
    if isinstance(expected_fields, Mapping) and isinstance(actual_fields, Mapping):
        left_decl = (
            expected_fields.get("nondeterminismClass"),
            expected_fields.get("stableComparisonKey"),
        )
        right_decl = (
            actual_fields.get("nondeterminismClass"),
            actual_fields.get("stableComparisonKey"),
        )
        if all(value is not None for value in (*left_decl, *right_decl)):
            nondeterminism = (left_decl, right_decl)

    before_left = _snapshot_payload(expected_trace, str(expected.raw.get("beforeSnapshot")))
    before_right = _snapshot_payload(actual_trace, str(actual.raw.get("beforeSnapshot")))
    before_difference, mapping_error = _compare_value(
        before_left,
        before_right,
        pointer="/beforeSnapshot",
        slice_name=slice_name,
        mappings=mappings,
    )
    if mapping_error:
        return _result(
            "identity_mapping_failure",
            expected,
            actual,
            pointer="/beforeSnapshot",
            downstream=downstream,
            detail=mapping_error,
        )
    before_alignment = "aligned" if before_difference is None else "different"

    for field, classification in (("decision", "decision_mismatch"), ("reason", "reason_mismatch")):
        difference = _first_difference(
            expected.raw.get(field), actual.raw.get(field), f"/{field}"
        )
        if difference:
            pointer, left, right = difference
            return _result(
                classification,
                expected,
                actual,
                pointer=pointer,
                expected_value=left,
                actual_value=right,
                downstream=downstream,
                before_alignment=before_alignment,
            )

    if before_difference:
        pointer, left, right = before_difference
        return _result(
            "before_snapshot_mismatch",
            expected,
            actual,
            pointer=pointer,
            expected_value=left,
            actual_value=right,
            downstream=downstream,
            before_alignment="different",
        )

    difference, mapping_error = _compare_value(
        expected_fields,
        actual_fields,
        pointer="/fields",
        slice_name=slice_name,
        mappings=mappings,
        nondeterminism=nondeterminism,
    )
    if mapping_error:
        return _result(
            "identity_mapping_failure",
            expected,
            actual,
            pointer="/fields",
            downstream=downstream,
            before_alignment="aligned",
            detail=mapping_error,
        )
    if difference:
        pointer, left, right = difference
        return _result(
            _classification_for_pointer(pointer, slice_name),
            expected,
            actual,
            pointer=pointer,
            expected_value=left,
            actual_value=right,
            downstream=downstream,
            before_alignment="aligned",
        )

    after_left = _snapshot_payload(expected_trace, str(expected.raw.get("afterSnapshot")))
    after_right = _snapshot_payload(actual_trace, str(actual.raw.get("afterSnapshot")))
    after_difference, mapping_error = _compare_value(
        after_left,
        after_right,
        pointer="/afterSnapshot",
        slice_name=slice_name,
        mappings=mappings,
    )
    if mapping_error:
        return _result(
            "identity_mapping_failure",
            expected,
            actual,
            pointer="/afterSnapshot",
            downstream=downstream,
            before_alignment="aligned",
            detail=mapping_error,
        )
    if after_difference:
        pointer, left, right = after_difference
        classification = _classification_for_pointer(pointer, slice_name)
        if classification == "field_mismatch":
            classification = "after_snapshot_mismatch"
        return _result(
            classification,
            expected,
            actual,
            pointer=pointer,
            expected_value=left,
            actual_value=right,
            downstream=downstream,
            before_alignment="aligned",
            after_alignment="different",
        )

    expected_object = str(expected.raw.get("object", ""))
    actual_object = str(actual.raw.get("object", ""))
    if expected_object or actual_object:
        error = mappings.object_names.bind(expected_object, actual_object)
        if error:
            return _result(
                "identity_mapping_failure",
                expected,
                actual,
                pointer="/object",
                downstream=downstream,
                before_alignment="aligned",
                after_alignment="aligned",
                detail=error,
            )
    expected_tag = expected.raw.get("objectTag")
    actual_tag = actual.raw.get("objectTag")
    if expected_tag or actual_tag:
        error = mappings.tags.bind(expected_tag, actual_tag)
        if error:
            return _result(
                "identity_mapping_failure",
                expected,
                actual,
                pointer="/objectTag",
                downstream=downstream,
                before_alignment="aligned",
                after_alignment="aligned",
                detail=error,
            )
    return None


def _object_mappings(
    expected: ValidatedTrace, actual: ValidatedTrace, mappings: _Mappings
) -> ComparisonResult | None:
    left_by_key: dict[tuple[str, str, str], str] = {}
    right_by_key: dict[tuple[str, str, str], str] = {}
    for side, values, target in (
        ("expected", expected.object_keys, left_by_key),
        ("actual", actual.object_keys, right_by_key),
    ):
        for name, key in values.items():
            if key in target:
                return ComparisonResult(
                    status="invalid",
                    classification="object_bijection_invalid",
                    expected_value=target[key] if side == "expected" else None,
                    actual_value=name if side == "actual" else None,
                    owner="runtime/recompute",
                    detail=f"{side} object semantic identity is one-to-many: {key!r}",
                )
            target[key] = name
    if set(left_by_key) != set(right_by_key):
        missing = sorted(set(left_by_key) - set(right_by_key))
        extra = sorted(set(right_by_key) - set(left_by_key))
        if missing:
            pointer, left, right = "/objects/missing/0", missing[0], "missing"
        else:
            pointer, left, right = "/objects/extra/0", "missing", extra[0]
        return ComparisonResult(
            status="different",
            classification="target_inventory_mismatch",
            json_pointer=pointer,
            expected_value=left,
            actual_value=right,
            downstream_drift_count=max(len(expected.events), len(actual.events)),
            owner="runtime/recompute",
            detail=f"missing actual objects: {len(missing)}; extra actual objects: {len(extra)}",
        )
    for key in sorted(left_by_key):
        left_name = left_by_key[key]
        right_name = right_by_key[key]
        error = mappings.object_names.bind(left_name, right_name)
        if error:
            return ComparisonResult(
                status="invalid",
                classification="object_bijection_invalid",
                owner="runtime/recompute",
                detail=error,
            )
        left_tag = expected.object_tags.get(left_name)
        right_tag = actual.object_tags.get(right_name)
        if left_tag is not None and right_tag is not None:
            error = mappings.tags.bind(left_tag, right_tag)
            if error:
                return ComparisonResult(
                    status="different",
                    classification="identity_mapping_failure",
                    owner="runtime/recompute",
                    detail=error,
                )
    return None


def _effective_targets(
    trace: ValidatedTrace,
    transaction: Mapping[str, Any],
    ordinal: int,
    mappings: _Mappings,
    *,
    actual: bool,
    include_executed: bool,
) -> tuple[list[str] | None, str | None]:
    targets = transaction.get("targets")
    if not isinstance(targets, list):
        return None, "transaction targets must be an array"
    known = set(trace.object_keys)
    reported_effective = transaction.get("effectiveTargets")
    if reported_effective is not None and not isinstance(reported_effective, list):
        return None, "transaction effectiveTargets must be an array"
    declared = set(reported_effective if isinstance(reported_effective, list) else targets)
    unknown = sorted(declared - known)
    if unknown and not include_executed:
        return None, f"transaction targets are outside the object bijection: {unknown!r}"
    declared &= known
    if isinstance(reported_effective, list):
        effective = declared
    elif targets and include_executed:
        effective = declared | set(trace.executed_objects_by_transaction.get(ordinal, ()))
    elif targets:
        effective = declared
    else:
        # Empty native targets mean a document-wide recompute, not a wildcard.
        effective = known
    if actual:
        effective = {
            mappings.object_names.right_to_left.get(name, name) for name in effective
        }
    return sorted(effective), None


def _scope_path_signature(
    trace: ValidatedTrace,
    scope: SemanticScope,
    mappings: _Mappings,
    *,
    actual: bool,
) -> tuple[Any, ...]:
    segments: list[Any] = []
    current = scope
    while True:
        object_name = current.object_name
        if actual:
            object_name = mappings.object_names.right_to_left.get(object_name, object_name)
        segments.append(
            (object_name, current.producer_kind, current.stage, current.sibling_ordinal)
        )
        if not current.parent:
            break
        current = trace.scopes[current.parent]
    return tuple(reversed(segments))


def _scope_signature(
    trace: ValidatedTrace,
    sequence: int,
    mappings: _Mappings,
    *,
    actual: bool,
) -> tuple[Any, ...]:
    scope = trace.scopes[sequence]
    path = _scope_path_signature(trace, scope, mappings, actual=actual)
    return (scope.transaction, path[:-1], *path[-1])


def _scope_parentless(signature: tuple[Any, ...]) -> tuple[Any, ...]:
    return (signature[0], *signature[2:])


def _is_producer_replay(
    trace: ValidatedTrace, sequence: int, mappings: _Mappings
) -> bool:
    scope = trace.scopes[sequence]
    if scope.producer_kind not in {
        "partdesign.extrude",
        "partdesign.dressup",
        "partdesign.pattern",
    }:
        return False
    current = scope
    while current.parent:
        current = trace.scopes[current.parent]
        object_name = mappings.object_names.right_to_left.get(current.object_name, current.object_name)
        if current.producer_kind == "partdesign.body_tip" or "body" in object_name.lower():
            return True
    return False


def _align_scopes(
    expected: ValidatedTrace,
    actual: ValidatedTrace,
    mappings: _Mappings,
) -> tuple[dict[int, int] | None, ComparisonResult | None]:
    pairs: dict[int, int] = {}
    left_index = right_index = 0
    left = expected.scope_order
    right = actual.scope_order

    def signature(trace: ValidatedTrace, sequence: int, *, actual_side: bool) -> tuple[Any, ...]:
        return _scope_signature(
            trace,
            sequence,
            mappings,
            actual=actual_side,
        )

    while left_index < len(left) and right_index < len(right):
        left_sequence = left[left_index]
        right_sequence = right[right_index]
        left_signature = signature(expected, left_sequence, actual_side=False)
        right_signature = signature(actual, right_sequence, actual_side=True)
        if left_signature == right_signature:
            pairs[left_sequence] = right_sequence
            left_index += 1
            right_index += 1
            continue

        if _scope_parentless(left_signature) == _scope_parentless(right_signature):
            scope = expected.scopes[left_sequence]
            return None, ComparisonResult(
                status="different",
                classification="scope_parent_mismatch",
                transaction_ordinal=scope.transaction,
                semantic_scope_path=scope.path,
                json_pointer="/scope/parent",
                expected_value=left_signature[1],
                actual_value=right_signature[1],
                downstream_drift_count=max(len(left) - left_index, len(right) - right_index) - 1,
                owner=_scope_owner(scope),
            )

        right_match = next(
            (
                offset
                for offset in range(1, LOOK_AHEAD + 1)
                if right_index + offset < len(right)
                and signature(actual, right[right_index + offset], actual_side=True)
                == left_signature
            ),
            None,
        )
        left_match = next(
            (
                offset
                for offset in range(1, LOOK_AHEAD + 1)
                if left_index + offset < len(left)
                and signature(expected, left[left_index + offset], actual_side=False)
                == right_signature
            ),
            None,
        )
        if right_match is not None and (left_match is None or right_match <= left_match):
            scope = actual.scopes[right_sequence]
            classification = (
                "producer_replay"
                if _is_producer_replay(actual, right_sequence, mappings)
                else "scope_missing_or_extra"
            )
            return None, ComparisonResult(
                status="different",
                classification=classification,
                transaction_ordinal=scope.transaction,
                semantic_scope_path=scope.path,
                json_pointer="/scopes",
                expected_value="missing",
                actual_value=right_signature,
                downstream_drift_count=max(len(left) - left_index, len(right) - right_index) - 1,
                owner="part_design/body" if classification == "producer_replay" else _scope_owner(scope),
                detail="actual semantic scope appears before the next expected scope",
            )
        scope = expected.scopes[left_sequence]
        return None, ComparisonResult(
            status="different",
            classification="scope_missing_or_extra",
            transaction_ordinal=scope.transaction,
            semantic_scope_path=scope.path,
            json_pointer="/scopes",
            expected_value=left_signature,
            actual_value="missing" if left_match is not None else right_signature,
            downstream_drift_count=max(len(left) - left_index, len(right) - right_index) - 1,
            owner=_scope_owner(scope),
            detail=(
                "expected semantic scope is missing before the next actual scope"
                if left_match is not None
                else "scope identities differ outside finite look-ahead"
            ),
        )

    if left_index < len(left):
        sequence = left[left_index]
        scope = expected.scopes[sequence]
        return None, ComparisonResult(
            status="different",
            classification="scope_missing_or_extra",
            transaction_ordinal=scope.transaction,
            semantic_scope_path=scope.path,
            json_pointer="/scopes",
            expected_value=signature(expected, sequence, actual_side=False),
            actual_value="missing",
            downstream_drift_count=len(left) - left_index - 1,
            owner=_scope_owner(scope),
            detail="expected semantic scope is absent at end of actual scope stream",
        )
    if right_index < len(right):
        sequence = right[right_index]
        scope = actual.scopes[sequence]
        classification = (
            "producer_replay"
            if _is_producer_replay(actual, sequence, mappings)
            else "scope_missing_or_extra"
        )
        return None, ComparisonResult(
            status="different",
            classification=classification,
            transaction_ordinal=scope.transaction,
            semantic_scope_path=scope.path,
            json_pointer="/scopes",
            expected_value="missing",
            actual_value=signature(actual, sequence, actual_side=True),
            downstream_drift_count=len(right) - right_index - 1,
            owner="part_design/body" if classification == "producer_replay" else _scope_owner(scope),
            detail="actual semantic scope remains after expected scope stream ends",
        )
    return pairs, None


def _local_event_key(event: ProjectedEvent) -> tuple[Any, ...]:
    return event.identity[2:]


def _event_replay(event: ProjectedEvent) -> bool:
    fields = event.raw.get("fields")
    return bool(
        isinstance(fields, Mapping)
        and (fields.get("replayUpstreamProducers") or fields.get("replayedProducer"))
    )


def _compare_event_stream(
    expected_trace: ValidatedTrace,
    actual_trace: ValidatedTrace,
    expected_events: Sequence[ProjectedEvent],
    actual_events: Sequence[ProjectedEvent],
    mappings: _Mappings,
    actual_scope_to_expected: Mapping[int, int] | None = None,
) -> ComparisonResult | None:
    def key(event: ProjectedEvent, *, actual_side: bool) -> tuple[Any, ...]:
        scope = int(event.raw.get("scopeSequence", 0))
        if actual_side and actual_scope_to_expected is not None:
            scope = actual_scope_to_expected.get(scope, scope)
        return (int(event.raw.get("transactionSequence", 0)), scope, *_local_event_key(event))

    left_index = right_index = 0
    while left_index < len(expected_events) and right_index < len(actual_events):
        left = expected_events[left_index]
        right = actual_events[right_index]
        left_key = key(left, actual_side=False)
        right_key = key(right, actual_side=True)
        if left_key == right_key:
            difference = _compare_matched_event(
                expected_trace,
                actual_trace,
                left,
                right,
                mappings,
                max(len(expected_events) - left_index, len(actual_events) - right_index) - 1,
            )
            if difference:
                return difference
            left_index += 1
            right_index += 1
            continue
        right_match = next(
            (
                offset
                for offset in range(1, LOOK_AHEAD + 1)
                if right_index + offset < len(actual_events)
                and key(actual_events[right_index + offset], actual_side=True) == left_key
            ),
            None,
        )
        left_match = next(
            (
                offset
                for offset in range(1, LOOK_AHEAD + 1)
                if left_index + offset < len(expected_events)
                and key(expected_events[left_index + offset], actual_side=False) == right_key
            ),
            None,
        )
        classification = "producer_replay" if right_match is not None and _event_replay(right) else "event_missing_or_extra"
        return _result(
            classification,
            left,
            right,
            pointer="/events",
            expected_value=left_key,
            actual_value=right_key,
            downstream=max(len(expected_events) - left_index, len(actual_events) - right_index) - 1,
            detail=(
                "actual has an extra event"
                if right_match is not None and (left_match is None or right_match <= left_match)
                else "expected event is missing from actual"
                if left_match is not None
                else "event semantic identities differ outside finite look-ahead"
            ),
        )
    if left_index < len(expected_events):
        return _result(
            "event_missing_or_extra",
            expected_events[left_index],
            None,
            pointer="/events",
            expected_value="present",
            actual_value="missing",
            downstream=len(expected_events) - left_index - 1,
        )
    if right_index < len(actual_events):
        classification = "producer_replay" if _event_replay(actual_events[right_index]) else "event_missing_or_extra"
        return _result(
            classification,
            None,
            actual_events[right_index],
            pointer="/events",
            expected_value="missing",
            actual_value="present",
            downstream=len(actual_events) - right_index - 1,
        )
    return None


def _ordered_comparison_events(trace: ValidatedTrace) -> tuple[ProjectedEvent, ...]:
    """Recover semantic events in their original root/nested interleaving."""

    by_sequence: dict[int, ProjectedEvent] = {}
    for events in trace.events_by_scope.values():
        for event in events:
            # Scope zero belongs to the request/runtime envelope. Its closure is already
            # validated by the shared deep validator, but topo-state preflight, response
            # lookup, and document checkpoint diagnostics are not producer events and have
            # no native producer-scope counterpart.
            if int(event.raw.get("scopeSequence", 0)) == 0:
                continue
            if event.raw.get("slice") in {
                "initial",
                "document.recompute.begin",
                "document.recompute.end",
            }:
                continue
            by_sequence[event.sequence] = event
    return tuple(by_sequence[sequence] for sequence in sorted(by_sequence))


def compare_traces(
    expected: Mapping[str, Any] | str,
    actual: Mapping[str, Any] | str,
    *,
    document_graph: Mapping[str, Any] | None = None,
) -> ComparisonResult:
    """Return the earliest structural/state divergence without mutating either trace."""

    try:
        expected_trace = project_trace(expected, document_graph=document_graph)
    except (TraceValidationError, OSError, ValueError) as exc:
        if "document graph" in str(exc) or "trace object" in str(exc):
            return ComparisonResult(
                status="invalid",
                classification="object_bijection_invalid",
                owner="runtime/recompute",
                detail=f"expected: {exc}",
            )
        return _invalid("expected", exc)
    try:
        actual_trace = project_trace(actual, document_graph=document_graph)
    except (TraceValidationError, OSError, ValueError) as exc:
        if "document graph" in str(exc) or "trace object" in str(exc):
            return ComparisonResult(
                status="invalid",
                classification="object_bijection_invalid",
                owner="runtime/recompute",
                detail=f"actual: {exc}",
            )
        return _invalid("actual", exc)

    comparable_slices = {
        str(event.raw.get("slice"))
        for event in expected_trace.events
        if event.raw.get("slice") not in {"initial", "scope.begin", "scope.end", "scope.abort", "document.recompute.begin", "document.recompute.end"}
    } & {
        str(event.raw.get("slice"))
        for event in actual_trace.events
        if event.raw.get("slice") not in {"initial", "scope.begin", "scope.end", "scope.abort", "document.recompute.begin", "document.recompute.end"}
    }
    if not comparable_slices:
        return ComparisonResult(
            status="invalid",
            classification="invalid_trace_no_comparable_slice",
            detail="validated traces contain no common producer slice",
        )

    mappings = _Mappings()
    object_difference = _object_mappings(expected_trace, actual_trace, mappings)
    if object_difference:
        return object_difference

    if len(expected_trace.transactions) != len(actual_trace.transactions):
        return ComparisonResult(
            status="different",
            classification="transaction_missing_or_extra",
            json_pointer="/transactions",
            expected_value=len(expected_trace.transactions),
            actual_value=len(actual_trace.transactions),
            owner="runtime/recompute",
        )
    for ordinal, (left, right) in enumerate(
        zip(expected_trace.transactions, actual_trace.transactions, strict=True), 1
    ):
        left_targets, left_error = _effective_targets(
            expected_trace,
            left,
            ordinal,
            mappings,
            actual=False,
            include_executed=document_graph is not None,
        )
        right_targets, right_error = _effective_targets(
            actual_trace,
            right,
            ordinal,
            mappings,
            actual=True,
            include_executed=document_graph is not None,
        )
        if left_error or right_error:
            return ComparisonResult(
                status="invalid",
                classification="object_bijection_invalid",
                transaction_ordinal=ordinal,
                json_pointer="/transaction/targets",
                owner="runtime/recompute",
                detail=f"expected: {left_error or 'valid'}; actual: {right_error or 'valid'}",
            )
        difference = _first_difference(
            left_targets, right_targets, "/transaction/effectiveTargets"
        )
        if difference:
            pointer, expected_value, actual_value = difference
            return ComparisonResult(
                status="different",
                classification="target_inventory_mismatch",
                transaction_ordinal=ordinal,
                json_pointer=pointer,
                expected_value=expected_value,
                actual_value=actual_value,
                owner="runtime/recompute",
            )
        difference = _first_difference(
            left.get("outcome"), right.get("outcome"), "/transaction/outcome"
        )
        if difference:
            pointer, expected_value, actual_value = difference
            return ComparisonResult(
                status="different",
                classification="transaction_outcome_mismatch",
                transaction_ordinal=ordinal,
                json_pointer=pointer,
                expected_value=expected_value,
                actual_value=actual_value,
                owner="runtime/recompute",
            )

    scope_pairs, scope_difference = _align_scopes(expected_trace, actual_trace, mappings)
    if scope_difference:
        return scope_difference
    assert scope_pairs is not None

    actual_scope_to_expected = {
        actual_scope: expected_scope for expected_scope, actual_scope in scope_pairs.items()
    }
    difference = _compare_event_stream(
        expected_trace,
        actual_trace,
        _ordered_comparison_events(expected_trace),
        _ordered_comparison_events(actual_trace),
        mappings,
        actual_scope_to_expected=actual_scope_to_expected,
    )
    if difference:
        return difference

    return ComparisonResult(
        status="equal",
        classification="aligned",
        before_alignment="aligned",
        after_alignment="aligned",
    )
