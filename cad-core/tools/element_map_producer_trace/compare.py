"""Ordered first-divergence comparison for independently validated producer traces."""

from __future__ import annotations

import re
import hashlib
from collections import Counter
from dataclasses import dataclass, field, replace
from typing import Any, Mapping, Sequence

from .model import (
    ComparisonResult,
    NormalizationRecord,
    ProjectedEvent,
    SemanticScope,
    ValidatedTrace,
)
from .projection import PROJECTION_POLICY, _normalise_stage, _producer_kind, project_trace
from .validate import TraceValidationError, canonical_json_sha256


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
DERIVED_PAYLOAD_HASH_KEYS = {"canonicalPayloadSha256", "rawCanonicalSha256"}


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
    mapped_indices: _Bijection = field(default_factory=lambda: _Bijection("mapped-name index"))
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


def _raw_difference_count(expected: Any, actual: Any) -> int:
    # The public field is a raw-difference indicator used for case-level
    # summaries, not an expensive count of every downstream JSON leaf drift.
    return int(expected != actual)


def _normalization_records(mappings: _Mappings) -> tuple[NormalizationRecord, ...]:
    records: list[NormalizationRecord] = []
    for reason_code, mapping, pointer in (
        ("runtime_tag_bijection", mappings.tags, "/runtimeTag"),
        ("opaque_sid_bijection", mappings.sids, "/stringId"),
        ("opaque_mapped_index_bijection", mappings.mapped_indices, "/mappedNameIndex"),
        ("transient_owner_projection", mappings.identities, "/traceIdentity"),
    ):
        for expected_raw, actual_raw in mapping.left_to_right.items():
            if expected_raw == actual_raw:
                continue
            records.append(
                NormalizationRecord(
                    reason_code=reason_code,
                    expected_json_pointer=pointer,
                    actual_json_pointer=pointer,
                    expected_raw=expected_raw,
                    actual_raw=actual_raw,
                    semantic_key=expected_raw,
                )
            )
    return tuple(records)


def _raw_sid_graph(
    payload: Mapping[str, Any], mappings: _Mappings, *, actual: bool
) -> dict[int, tuple[Any, list[tuple[int, int]]]]:
    snapshots = payload.get("stringTableSnapshots")
    if not isinstance(snapshots, Mapping):
        return {}
    best: Any = None
    for snapshot in snapshots.values():
        if not isinstance(snapshot, Mapping):
            continue
        body = snapshot.get("payload") if isinstance(snapshot.get("payload"), Mapping) else snapshot
        entries = body.get("entries") if isinstance(body, Mapping) else None
        if isinstance(entries, (list, Mapping)) and (best is None or len(entries) > len(best)):
            best = entries
    if best is None:
        return {}
    graph: dict[int, tuple[Any, list[tuple[int, int]]]] = {}
    iterable = best.items() if isinstance(best, Mapping) else enumerate(best)
    for key, entry in iterable:
        if not isinstance(entry, Mapping):
            continue
        if isinstance(best, Mapping):
            try:
                sid = int(key)
            except (TypeError, ValueError):
                continue
        else:
            token = entry.get("token")
            match = re.fullmatch(r"#([0-9a-fA-F]+)", token) if isinstance(token, str) else None
            if match is None:
                continue
            sid = int(match.group(1), 16)
        related: list[tuple[int, int]] = []
        raw_related = entry.get("related", [])
        if isinstance(raw_related, list):
            for reference in raw_related:
                if not isinstance(reference, Mapping):
                    continue
                value = reference.get("value")
                if not isinstance(value, int):
                    token = reference.get("token")
                    match = re.fullmatch(r"#([0-9a-fA-F]+)", token) if isinstance(token, str) else None
                    value = int(match.group(1), 16) if match else None
                index = reference.get("index", 0)
                if isinstance(value, int) and isinstance(index, int):
                    related.append((value, index))
        base = tuple(
            (
                str(name),
                _stable_sort_value(
                    _mapped_string(value, mappings, actual=True)
                    if actual and isinstance(value, str)
                    else value
                ),
            )
            for name, value in sorted(entry.items())
            if name not in {"token", "related"}
        )
        graph[sid] = (base, related)
    return graph


def _bind_sid_graph(
    expected: ValidatedTrace, actual: ValidatedTrace, mappings: _Mappings
) -> str | None:
    left = _raw_sid_graph(expected.payload, mappings, actual=False)
    right = _raw_sid_graph(actual.payload, mappings, actual=True)
    if not left or not right:
        return None
    digest = lambda value: hashlib.sha256(repr(value).encode("utf-8")).hexdigest()
    left_labels = {sid: digest(base) for sid, (base, _related) in left.items()}
    right_labels = {sid: digest(base) for sid, (base, _related) in right.items()}
    # Related SID chains are shallow in the validated schema; eight rounds
    # cover nested references without making cost proportional to table size.
    for _ in range(8):
        next_left = {
            sid: digest((left_labels[sid], tuple((left_labels.get(ref), index) for ref, index in related)))
            for sid, (_base, related) in left.items()
        }
        next_right = {
            sid: digest((right_labels[sid], tuple((right_labels.get(ref), index) for ref, index in related)))
            for sid, (_base, related) in right.items()
        }
        if next_left == left_labels and next_right == right_labels:
            break
        left_labels, right_labels = next_left, next_right
    left_groups: dict[Any, list[int]] = {}
    right_groups: dict[Any, list[int]] = {}
    for sid, label in left_labels.items():
        left_groups.setdefault(label, []).append(sid)
    for sid, label in right_labels.items():
        right_groups.setdefault(label, []).append(sid)
    for label in set(left_groups) & set(right_groups):
        left_ids, right_ids = left_groups[label], right_groups[label]
        if len(left_ids) == len(right_ids) == 1:
            error = mappings.sids.bind(left_ids[0], right_ids[0])
            if error:
                return error
    return None


def _tag_reference_signatures(payload: Mapping[str, Any]) -> dict[int, Counter[str]]:
    signatures: dict[int, Counter[str]] = {}
    tag_pattern = re.compile(
        r"(?P<prefix>(?<![A-Za-z0-9])H|;D)(?P<tag>-?[0-9a-fA-F]+)"
    )
    sid_pattern = re.compile(r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?")

    def visit(value: Any) -> None:
        if isinstance(value, str):
            skeleton = tag_pattern.sub(
                lambda match: f"{match.group('prefix')}<TAG>",
                sid_pattern.sub("#<SID>", value),
            )
            skeleton = re.sub(
                r"H<TAG>:[0-9a-fA-F]+(?=,[VEF](?:\b|$))",
                "H<TAG>:<OPAQUE_INDEX>",
                skeleton,
            )
            for match in tag_pattern.finditer(value):
                raw = match.group("tag")
                tag = int(raw[1:], 16) * -1 if raw.startswith("-") else int(raw, 16)
                signatures.setdefault(tag, Counter())[skeleton] += 1
        elif isinstance(value, Mapping):
            for item in value.values():
                visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(payload)
    return signatures


def _bind_tag_reference_graph(
    expected: ValidatedTrace, actual: ValidatedTrace, mappings: _Mappings
) -> str | None:
    left = _tag_reference_signatures(expected.payload)
    right = _tag_reference_signatures(actual.payload)
    left_groups: dict[Any, list[int]] = {}
    right_groups: dict[Any, list[int]] = {}
    for tag, signature in left.items():
        left_groups.setdefault(tuple(sorted(signature.items())), []).append(tag)
    for tag, signature in right.items():
        right_groups.setdefault(tuple(sorted(signature.items())), []).append(tag)
    for signature in set(left_groups) & set(right_groups):
        if len(left_groups[signature]) != 1 or len(right_groups[signature]) != 1:
            continue
        left_tag, right_tag = left_groups[signature][0], right_groups[signature][0]
        if (
            left_tag in mappings.tags.left_to_right
            and mappings.tags.left_to_right[left_tag] != right_tag
        ) or (
            right_tag in mappings.tags.right_to_left
            and mappings.tags.right_to_left[right_tag] != left_tag
        ):
            continue
        error = mappings.tags.bind(left_tag, right_tag)
        if error:
            return error
    return None


def _sid_reference_signatures(
    payload: Mapping[str, Any], mappings: _Mappings, *, actual: bool
) -> dict[int, Counter[str]]:
    signatures: dict[int, Counter[str]] = {}
    known_sids = set(_raw_sid_graph(payload, mappings, actual=actual))
    events = payload.get("events")
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, Mapping) or event.get("slice") != "hasher.insert":
                continue
            fields = event.get("fields")
            if not isinstance(fields, Mapping):
                continue
            result = fields.get("result")
            value = result.get("value") if isinstance(result, Mapping) else fields.get("id")
            if isinstance(value, int):
                known_sids.add(value)
            elif isinstance(value, str) and value.isdigit():
                known_sids.add(int(value))
    sid_pattern = re.compile(r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?")
    opaque_pattern = re.compile(
        r"H([0-9a-fA-F]+):([0-9a-fA-F]+)(?=,[VEF](?:\b|$))"
    )

    def visit(value: Any) -> None:
        if isinstance(value, str):
            projected = _mapped_string(value, mappings, actual=actual)
            skeleton = sid_pattern.sub(
                lambda match: "#<SID>" + (f":{match.group(2)}" if match.group(2) else ""),
                opaque_pattern.sub(
                    lambda match: f"H{match.group(1)}:<OPAQUE_INDEX>", projected
                ),
            )
            for match in sid_pattern.finditer(projected):
                digits = match.group(1)
                sid = int(digits, 16)
                if sid not in known_sids and digits.isdigit() and int(digits, 10) in known_sids:
                    sid = int(digits, 10)
                signatures.setdefault(sid, Counter())[skeleton] += 1
        elif isinstance(value, Mapping):
            for item in value.values():
                visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(payload)
    return signatures


def _bind_sid_reference_graph(
    expected: ValidatedTrace, actual: ValidatedTrace, mappings: _Mappings
) -> str | None:
    left = _sid_reference_signatures(expected.payload, mappings, actual=False)
    right = _sid_reference_signatures(actual.payload, mappings, actual=True)
    left_groups: dict[Any, list[int]] = {}
    right_groups: dict[Any, list[int]] = {}
    for sid, signature in left.items():
        left_groups.setdefault(tuple(sorted(signature.items())), []).append(sid)
    for sid, signature in right.items():
        right_groups.setdefault(tuple(sorted(signature.items())), []).append(sid)
    for signature in set(left_groups) & set(right_groups):
        if len(left_groups[signature]) == len(right_groups[signature]) == 1:
            left_sid, right_sid = left_groups[signature][0], right_groups[signature][0]
            if (
                left_sid in mappings.sids.left_to_right
                and mappings.sids.left_to_right[left_sid] != right_sid
            ) or (
                right_sid in mappings.sids.right_to_left
                and mappings.sids.right_to_left[right_sid] != left_sid
            ):
                continue
            error = mappings.sids.bind(left_sid, right_sid)
            if error:
                return error
    return None


def _opaque_index_signatures(
    payload: Mapping[str, Any], mappings: _Mappings, *, actual: bool
) -> Counter[tuple[str, int, int]]:
    signatures: Counter[tuple[str, int, int]] = Counter()
    pattern = re.compile(
        r"H(?P<tag>[0-9a-fA-F]+):(?P<index>[0-9a-fA-F]+),(?P<kind>[VEF])"
    )

    def visit(value: Any) -> None:
        if isinstance(value, str):
            projected = _mapped_string(value, mappings, actual=actual)
            skeleton = pattern.sub(
                lambda item: re.sub(
                    r":[0-9a-fA-F]+,", ":<OPAQUE_INDEX>,", item.group(0)
                ),
                projected,
            )
            for ordinal, match in enumerate(pattern.finditer(projected)):
                signatures[(skeleton, ordinal, int(match.group("index"), 16))] += 1
        elif isinstance(value, Mapping):
            for item in value.values():
                visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(payload)
    return signatures


def _bind_opaque_index_graph(
    expected: ValidatedTrace, actual: ValidatedTrace, mappings: _Mappings
) -> str | None:
    left = _opaque_index_signatures(expected.payload, mappings, actual=False)
    right = _opaque_index_signatures(actual.payload, mappings, actual=True)
    left_groups: dict[tuple[str, int, int], list[int]] = {}
    right_groups: dict[tuple[str, int, int], list[int]] = {}
    for (skeleton, ordinal, index), count in left.items():
        if count >= 2:
            left_groups.setdefault((skeleton, ordinal, count), []).append(index)
    for (skeleton, ordinal, index), count in right.items():
        if count >= 2:
            right_groups.setdefault((skeleton, ordinal, count), []).append(index)
    for namespace in set(left_groups) & set(right_groups):
        if len(left_groups[namespace]) == len(right_groups[namespace]) == 1:
            skeleton, ordinal, _count = namespace
            error = mappings.mapped_indices.bind(
                (skeleton, ordinal, left_groups[namespace][0]),
                (skeleton, ordinal, right_groups[namespace][0]),
            )
            if error:
                return error
    return None


def _path_kind(path: tuple[str, ...], slice_name: str) -> str | None:
    lowered = [value.lower() for value in path]
    leaf = lowered[-1] if lowered else ""
    if leaf.endswith("tag") or leaf in {"objecttag", "mastertag", "inputtag", "outputtag"}:
        return "tag"
    if "identity" in leaf and leaf not in {"identitystatus"}:
        return "identity"
    # StringIDRef is a `(value, index)` pair. Only `value` belongs to the SID
    # bijection; treating index 0 as an SID aliases it with Python's `False`.
    if any(value in SID_CONTEXT_KEYS for value in lowered) and leaf != "index":
        return "sid"
    if slice_name.startswith("hasher.") and leaf in {"id", "value", "prefixid", "prefixidindex"}:
        return "sid"
    return None


def _mapped_string(value: str, mappings: _Mappings, *, actual: bool) -> str:
    if not actual:
        return value

    def sid(match: re.Match[str]) -> str:
        digits = match.group(1)
        raw = int(digits, 16)
        if raw not in mappings.sids.right_to_left and digits.isdigit():
            decimal = int(digits, 10)
            if decimal in mappings.sids.right_to_left:
                raw = decimal
        mapped = mappings.sids.right_to_left.get(raw)
        projected_sid = mapped if mapped is not None else raw
        raw_index = match.group(2)
        if raw_index is None:
            return f"#{projected_sid:x}"
        index = int(raw_index, 16)
        projected_index = mappings.mapped_indices.right_to_left.get(
            (raw, index), (projected_sid, index)
        )[1]
        return f"#{projected_sid:x}:{projected_index:x}"

    def projected_tag(raw_text: str) -> int:
        negative = raw_text.startswith("-")
        digits = raw_text[1:] if negative else raw_text
        raw = int(digits, 16) * (-1 if negative else 1)
        if digits.isdigit():
            decimal = int(digits, 10) * (-1 if negative else 1)
            if raw not in mappings.tags.right_to_left and decimal in mappings.tags.right_to_left:
                raw = decimal
        mapped = mappings.tags.right_to_left.get(raw)
        if mapped is None and raw < 0:
            positive = mappings.tags.right_to_left.get(-raw)
            if positive is not None:
                mapped = -positive
        return mapped if mapped is not None else raw

    def tag(match: re.Match[str]) -> str:
        projected = projected_tag(match.group(1))
        return f"H{'-' if projected < 0 else ''}{abs(projected):x}"

    def deleted_tag(match: re.Match[str]) -> str:
        projected = projected_tag(match.group(1))
        return f";D{'-' if projected < 0 else ''}{abs(projected):x}"

    projected = re.sub(
        r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?",
        sid,
        re.sub(
            r";D(-?[0-9a-fA-F]+)",
            deleted_tag,
            re.sub(r"(?<![A-Za-z0-9])H(-?[0-9a-fA-F]+)", tag, value),
        ),
    )
    opaque_pattern = re.compile(
        r"H([0-9a-fA-F]+):([0-9a-fA-F]+)(?=,[VEF](?:\b|$))"
    )
    skeleton = opaque_pattern.sub(
        lambda match: f"H{match.group(1)}:<OPAQUE_INDEX>", projected
    )
    ordinal = -1

    def opaque_index(match: re.Match[str]) -> str:
        nonlocal ordinal
        ordinal += 1
        raw = int(match.group(2), 16)
        projected_node = mappings.mapped_indices.right_to_left.get(
            (skeleton, ordinal, raw)
        )
        if isinstance(projected_node, tuple):
            return f"H{match.group(1)}:{projected_node[2]:x}"
        projected_sid = mappings.sids.right_to_left.get(raw)
        return (
            f"H{match.group(1)}:{projected_sid:x}"
            if projected_sid is not None
            else match.group(0)
        )

    return opaque_pattern.sub(opaque_index, projected)


_EMBEDDED_TAG_TOKEN = re.compile(
    r"(?P<prefix>(?<![A-Za-z0-9])H|;D)(?P<tag>-?[0-9a-fA-F]+)"
)


def _project_string_pair(
    expected: str,
    actual: str,
    mappings: _Mappings,
) -> tuple[str, str | None]:
    """Project embedded Tags when both mapped-name structures agree.

    Unknown run-local Tags may first appear only inside a raw mapped name. Bind
    them pairwise only when removing the Tag tokens leaves identical strings;
    source/index/SID/postfix changes therefore remain strict mismatches.
    """

    expected_sid_tokens = list(re.finditer(r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?", expected))
    actual_sid_tokens = list(re.finditer(r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?", actual))
    sid_skeleton = lambda value: re.sub(
        r"#([0-9a-fA-F]+)(?::([0-9a-fA-F]+))?", "#<SID>", value
    )
    if len(expected_sid_tokens) == len(actual_sid_tokens) and sid_skeleton(expected) == sid_skeleton(actual):
        for left, right in zip(expected_sid_tokens, actual_sid_tokens, strict=True):
            left_sid, right_sid = int(left.group(1), 16), int(right.group(1), 16)
            sid_error = mappings.sids.bind(left_sid, right_sid)
            if sid_error:
                return actual, sid_error
            left_index, right_index = left.group(2), right.group(2)
            if (left_index is None) != (right_index is None):
                return actual, None
            if left_index is not None and right_index is not None:
                index_error = mappings.mapped_indices.bind(
                    (left_sid, int(left_index, 16)),
                    (right_sid, int(right_index, 16)),
                )
                if index_error:
                    return actual, index_error
    projected_actual = _mapped_string(actual, mappings, actual=True)
    if not any(marker in expected for marker in (";", ":", "#")) or not any(
        marker in actual for marker in (";", ":", "#")
    ):
        return projected_actual, None
    expected_matches = list(_EMBEDDED_TAG_TOKEN.finditer(expected))
    actual_matches = list(_EMBEDDED_TAG_TOKEN.finditer(actual))
    if len(expected_matches) != len(actual_matches):
        return projected_actual, None

    def skeleton(value: str) -> str:
        return _EMBEDDED_TAG_TOKEN.sub(
            lambda match: f"{match.group('prefix')}<TAG>",
            value,
        )

    if skeleton(expected) != skeleton(projected_actual):
        return projected_actual, None
    for left, right in zip(expected_matches, actual_matches, strict=True):
        left_text = left.group("tag")
        right_text = right.group("tag")
        left_tag = int(left_text[1:], 16) * -1 if left_text.startswith("-") else int(left_text, 16)
        right_tag = int(right_text[1:], 16) * -1 if right_text.startswith("-") else int(right_text, 16)
        if right_tag not in mappings.tags.right_to_left and right_text.lstrip("-").isdigit():
            decimal = int(right_text, 10)
            if decimal in mappings.tags.right_to_left:
                right_tag = decimal
        error = mappings.tags.bind(left_tag, right_tag)
        if error:
            return projected_actual, error
    return _mapped_string(actual, mappings, actual=True), None


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
        # Bind explicit sibling Tag fields before walking embedded mapped-name
        # strings. Alphabetical traversal otherwise compares `source` before
        # `sourceTag` and reports the run-local H/D token as a field mismatch.
        for key in keys:
            if key not in expected or key not in actual:
                continue
            child_path = path + (str(key),)
            if _path_kind(child_path, slice_name) != "tag":
                continue
            left_tag = expected[key]
            right_tag = actual[key]
            if (
                not isinstance(left_tag, int)
                or isinstance(left_tag, bool)
                or not isinstance(right_tag, int)
                or isinstance(right_tag, bool)
            ):
                continue
            error = mappings.tags.bind(left_tag, right_tag)
            if error:
                return expected, actual, error
        for key in keys:
            # Each trace is independently validated before comparison. These
            # hashes cover the producer-local raw payload, including run-local
            # Tags, so comparing their text after semantic Tag projection
            # would override the payload comparison with a false mismatch.
            if key in DERIVED_PAYLOAD_HASH_KEYS:
                continue
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
    if kind == "sid" and isinstance(expected, str) and isinstance(actual, str):
        projected_actual, error = _project_string_pair(expected, actual, mappings)
        return expected, projected_actual, error
    if (
        kind
        and expected is not None
        and actual is not None
        and not isinstance(expected, bool)
        and not isinstance(actual, bool)
    ):
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
        projected_actual, error = _project_string_pair(expected, actual, mappings)
        return expected, projected_actual, error
    return expected, actual, None


def _expand_snapshot_refs(
    trace: ValidatedTrace,
    value: Any,
    *,
    active: frozenset[str] = frozenset(),
) -> Any:
    """Replace producer-local snapshot ids with their independently validated values."""

    if isinstance(value, str) and value in trace.snapshots:
        if not active and value in trace.projected_snapshot_cache:
            return trace.projected_snapshot_cache[value]
        snapshot = trace.snapshots[value]
        kind = snapshot.get("kind", value.split(":", 1)[0])
        if value in active:
            return {"kind": kind, "recursiveRef": True}
        expanded = {
            "kind": kind,
            "payload": _expand_snapshot_refs(
                trace,
                snapshot.get("payload"),
                active=active | {value},
            ),
        }
        if not active:
            trace.projected_snapshot_cache[value] = expanded
        return expanded
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
    cache_key = f"payload:{snapshot_id}"
    if cache_key not in trace.projected_snapshot_cache:
        trace.projected_snapshot_cache[cache_key] = _expand_snapshot_refs(
            trace, snapshot.get("payload"), active=frozenset({snapshot_id})
        )
    return trace.projected_snapshot_cache[cache_key]


def _mask_snapshot_values(trace: ValidatedTrace, value: Any) -> Any:
    if isinstance(value, str) and value in trace.snapshots:
        return "<INTERMEDIATE_SNAPSHOT>"
    if isinstance(value, Mapping):
        return {str(key): _mask_snapshot_values(trace, item) for key, item in value.items()}
    if isinstance(value, list):
        return [_mask_snapshot_values(trace, item) for item in value]
    return value


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


def _has_access_summary_extension(trace: Any) -> bool:
    return isinstance(trace, Mapping) and (
        any(
            isinstance(event, Mapping)
            and event.get("slice") == PROJECTION_POLICY.access_summary_event_slice
            for event in trace.get("events", [])
        )
        or any(key in trace for key in ("accessSummaries", "accessSets", "traceIdentities"))
    )


def _validate_access_summary_extension(trace: Any) -> None:
    """Run the collector's strict single-artifact validator before projection.

    This import is deliberately lazy: the collector uses compare_traces for its
    two-run gate, so importing it at module load time would create a cycle.
    """

    if not _has_access_summary_extension(trace):
        return
    try:
        from tools.collect_freecad_expected import validate_producer_trace
    except ModuleNotFoundError:
        from collect_freecad_expected import validate_producer_trace
    validate_producer_trace(trace)


def _access_identity_value(value: Any) -> Any:
    if not isinstance(value, str):
        return value
    match = re.fullmatch(r"object:(-?\d+)", value)
    if match:
        return {"kind": "object", "tag": int(match.group(1))}
    match = re.fullmatch(r"property:object:(-?\d+):(.+)", value)
    if match:
        return {
            "kind": "property",
            "ownerTag": int(match.group(1)),
            "propertyName": match.group(2),
        }
    return value


def _access_identity_records(
    trace: ValidatedTrace,
    summary: Mapping[str, Any],
    field: str,
    identity_indexes: dict[str, int] | None = None,
) -> list[Any]:
    identities = trace.payload.get("traceIdentities", {})
    records: list[Any] = []
    local_ids = identity_indexes if identity_indexes is not None else {}
    for ref in summary.get(field, []):
        if not isinstance(ref, str):
            records.append(ref)
            continue
        if ref not in local_ids:
            local_ids[ref] = len(local_ids)
        metadata = identities.get(ref, {}) if isinstance(identities, Mapping) else {}
        records.append(
            {
                "identityIndex": local_ids[ref],
                "metadata": metadata,
            }
        )
    return records


def _access_set_view(trace: ValidatedTrace, summary: Mapping[str, Any]) -> Mapping[str, Any]:
    access_sets = trace.payload.get("accessSets", {})
    access_set = access_sets.get(summary.get("accessSet"), {}) if isinstance(access_sets, Mapping) else {}
    if not isinstance(access_set, Mapping):
        return access_set
    return {
        field: [_access_identity_value(value) for value in access_set.get(field, [])]
        for field in (
            "propertyReads",
            "objectReads",
            "documentReads",
            "propertyWrites",
            "objectWrites",
            "documentWrites",
        )
    }


def _access_summary_view(trace: ValidatedTrace, summary_ref: str) -> Mapping[str, Any] | None:
    cache_key = f"access-summary:{summary_ref}"
    if cache_key in trace.projected_summary_cache:
        return trace.projected_summary_cache[cache_key]
    summaries = trace.payload.get("accessSummaries", {})
    if not isinstance(summaries, Mapping):
        return None
    summary = summaries.get(summary_ref)
    if not isinstance(summary, Mapping):
        return None
    view = {
        key: value
        for key, value in summary.items()
        if key not in {"canonicalHash", "accessSet", "beforeLedgerSnapshot", "afterLedgerSnapshot"}
    }
    view["accessSet"] = _access_set_view(trace, summary)
    identity_indexes: dict[str, int] = {}
    for field in ("elementMapReads", "elementMapWrites", "stringHasherReads", "stringHasherWrites"):
        view[field] = _access_identity_records(trace, summary, field, identity_indexes)
    for field in ("beforeLedgerSnapshot", "afterLedgerSnapshot"):
        view[field] = _snapshot_payload(trace, str(summary[field]))
    if "relatedAllocationRefs" in summary:
        nodes = trace.payload.get("allocationNodes", {})
        view["relatedAllocationRefs"] = [
            nodes.get(ref, ref) if isinstance(nodes, Mapping) else ref
            for ref in summary["relatedAllocationRefs"]
        ]
    trace.projected_summary_cache[cache_key] = view
    return view


def _summary_ref(event: ProjectedEvent) -> str | None:
    if event.raw.get("slice") != PROJECTION_POLICY.access_summary_event_slice:
        return None
    fields = event.raw.get("fields")
    ref = fields.get("summaryRef") if isinstance(fields, Mapping) else None
    return ref if isinstance(ref, str) else None


def _summary_owner_key(trace: Mapping[str, Any], event: Mapping[str, Any]) -> tuple[Any, ...] | None:
    raw_event = event.raw if isinstance(event, ProjectedEvent) else event
    fields = raw_event.get("fields")
    ref = fields.get("summaryRef") if isinstance(fields, Mapping) else None
    summaries = trace.get("accessSummaries", {})
    summary = summaries.get(ref) if isinstance(summaries, Mapping) else None
    if not isinstance(summary, Mapping):
        return None
    return (
        summary.get("owner"),
        summary.get("ownerTypeId"),
        summary.get("ownerGraphRole"),
        summary.get("propertyName"),
    )


def _independent_owner_summary_reorder(
    expected: ValidatedTrace,
    actual: ValidatedTrace,
) -> tuple[bool, list[ProjectedEvent], list[ProjectedEvent]]:
    """Normalize only a proven, complete independent owner-summary window."""

    left_events = list(_ordered_comparison_events(expected))
    right_events = list(_ordered_comparison_events(actual))
    if not _has_access_summary_extension(expected.payload) or not _has_access_summary_extension(actual.payload):
        return False, left_events, right_events
    try:
        from tools.collect_freecad_expected import audit_property_shape_owner_blocks
    except ModuleNotFoundError:
        from collect_freecad_expected import audit_property_shape_owner_blocks
    left_audit = audit_property_shape_owner_blocks(dict(expected.payload))
    right_audit = audit_property_shape_owner_blocks(dict(actual.payload))
    if left_audit.get("verdict") != "independent_owner_block_reorder" or right_audit.get("verdict") != "independent_owner_block_reorder":
        return False, left_events, right_events

    left_summary = [event for event in left_events if _summary_ref(event) is not None]
    right_summary = [event for event in right_events if _summary_ref(event) is not None]
    left_keys = sorted(_summary_owner_key(expected.payload, event) for event in left_summary)
    right_keys = sorted(_summary_owner_key(actual.payload, event) for event in right_summary)
    if not left_summary or left_keys != right_keys:
        return False, left_events, right_events
    def reorder(events: list[ProjectedEvent], trace: ValidatedTrace) -> list[ProjectedEvent]:
        slots = [index for index, event in enumerate(events) if _summary_ref(event) is not None]
        ordered = sorted(
            (events[index] for index in slots),
            key=lambda event: _summary_owner_key(trace.payload, event.raw) or (),
        )
        for ordinal, (index, event) in enumerate(zip(slots, ordered, strict=True), 1):
            events[index] = replace(event, identity=event.identity[:-1] + (ordinal,))
        return events

    return True, reorder(left_events, expected), reorder(right_events, actual)


def _compare_matched_event(
    expected_trace: ValidatedTrace,
    actual_trace: ValidatedTrace,
    expected: ProjectedEvent,
    actual: ProjectedEvent,
    mappings: _Mappings,
    downstream: int,
    *,
    ignore_intermediate_snapshots: bool = False,
    snapshot_comparison_cache: set[tuple[str, str, str]] | None = None,
) -> ComparisonResult | None:
    slice_name = str(expected.raw.get("slice", ""))
    projected_summary_pair: tuple[Any, Any, str | None] | None = None
    if ignore_intermediate_snapshots:
        expected_fields = _mask_snapshot_values(expected_trace, expected.raw.get("fields"))
        actual_fields = _mask_snapshot_values(actual_trace, actual.raw.get("fields"))
    else:
        expected_fields = _expand_snapshot_refs(expected_trace, expected.raw.get("fields"))
        actual_fields = _expand_snapshot_refs(actual_trace, actual.raw.get("fields"))
    expected_summary_ref = _summary_ref(expected)
    actual_summary_ref = _summary_ref(actual)
    if expected_summary_ref is not None and actual_summary_ref is not None:
        expected_summary = _access_summary_view(expected_trace, expected_summary_ref)
        actual_summary = _access_summary_view(actual_trace, actual_summary_ref)
        if expected_summary is not None and actual_summary is not None:
            expected_fields = expected_summary
            actual_fields = actual_summary
            projected_summary_pair = _project_pair(
                expected_summary,
                actual_summary,
                path=("fields",),
                slice_name=slice_name,
                mappings=mappings,
                nondeterminism=None,
            )
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

    before_left = (
        None
        if ignore_intermediate_snapshots
        else _snapshot_payload(expected_trace, str(expected.raw.get("beforeSnapshot")))
    )
    before_right = (
        None
        if ignore_intermediate_snapshots
        else _snapshot_payload(actual_trace, str(actual.raw.get("beforeSnapshot")))
    )
    before_ids = (
        str(expected.raw.get("beforeSnapshot")),
        str(actual.raw.get("beforeSnapshot")),
        slice_name,
    )
    if snapshot_comparison_cache is not None and before_ids in snapshot_comparison_cache:
        before_difference, mapping_error = None, None
    else:
        before_difference, mapping_error = _compare_value(
            before_left,
            before_right,
            pointer="/beforeSnapshot",
            slice_name=slice_name,
            mappings=mappings,
        )
        if (
            snapshot_comparison_cache is not None
            and before_difference is None
            and mapping_error is None
        ):
            snapshot_comparison_cache.add(before_ids)
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

    if projected_summary_pair is not None:
        projected_left, projected_right, mapping_error = projected_summary_pair
        difference = (
            None
            if mapping_error is not None
            else (
                None
                if canonical_json_sha256(projected_left)
                == canonical_json_sha256(projected_right)
                else _first_difference(projected_left, projected_right, "/fields")
            )
        )
    else:
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

    after_left = (
        None
        if ignore_intermediate_snapshots
        else _snapshot_payload(expected_trace, str(expected.raw.get("afterSnapshot")))
    )
    after_right = (
        None
        if ignore_intermediate_snapshots
        else _snapshot_payload(actual_trace, str(actual.raw.get("afterSnapshot")))
    )
    after_ids = (
        str(expected.raw.get("afterSnapshot")),
        str(actual.raw.get("afterSnapshot")),
        slice_name,
    )
    if snapshot_comparison_cache is not None and after_ids in snapshot_comparison_cache:
        after_difference, mapping_error = None, None
    else:
        after_difference, mapping_error = _compare_value(
            after_left,
            after_right,
            pointer="/afterSnapshot",
            slice_name=slice_name,
            mappings=mappings,
        )
        if (
            snapshot_comparison_cache is not None
            and after_difference is None
            and mapping_error is None
        ):
            snapshot_comparison_cache.add(after_ids)
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
    if event.raw.get("slice") in {"element_map.find", "element_map.find_all"} or (
        event.raw.get("slice") == "toposhape.can_map"
        and event.raw.get("decision") == "accepted"
        and event.raw.get("reason") == "cache_ready"
    ):
        return event.identity[2:-1]
    return event.identity[2:]


def _event_join_value(value: Any) -> Any:
    """Remove run-local Tags only from the event alignment key.

    The matched event still goes through the strict field/snapshot comparator,
    where the Tag bijection is checked and all non-Tag mapped-name content
    remains significant.
    """

    if isinstance(value, tuple):
        return tuple(_event_join_value(item) for item in value)
    if isinstance(value, str):
        value = re.sub(r"(?<![A-Za-z0-9])H-?[0-9a-fA-F]+", "H<TAG>", value)
        return re.sub(r";D[0-9a-fA-F]+", ";D<TAG>", value)
    return value


def _event_join_key(event: ProjectedEvent) -> tuple[Any, ...]:
    return tuple(_event_join_value(item) for item in _local_event_key(event))


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
    expected_mapper_scope_keys: Mapping[int, tuple[Any, ...]] | None = None,
    actual_mapper_scope_keys: Mapping[int, tuple[Any, ...]] | None = None,
    expected_projected_mapper_scopes: frozenset[int] = frozenset(),
    actual_projected_mapper_scopes: frozenset[int] = frozenset(),
) -> ComparisonResult | None:
    snapshot_comparison_cache: set[tuple[str, str, str]] = set()
    def key(event: ProjectedEvent, *, actual_side: bool) -> tuple[Any, ...]:
        scope = int(event.raw.get("scopeSequence", 0))
        mapper_keys = actual_mapper_scope_keys if actual_side else expected_mapper_scope_keys
        if mapper_keys is not None and scope in mapper_keys:
            return (
                int(event.raw.get("transactionSequence", 0)),
                mapper_keys[scope],
                *_event_join_key(event),
            )
        if actual_side and actual_scope_to_expected is not None:
            scope = actual_scope_to_expected.get(scope, scope)
        return (int(event.raw.get("transactionSequence", 0)), scope, *_event_join_key(event))

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
                ignore_intermediate_snapshots=(
                    int(left.raw.get("scopeSequence", 0)) in expected_projected_mapper_scopes
                    and int(right.raw.get("scopeSequence", 0)) in actual_projected_mapper_scopes
                ),
                snapshot_comparison_cache=snapshot_comparison_cache,
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


def _mapper_scope_projection(
    trace: ValidatedTrace,
    events: Sequence[ProjectedEvent],
) -> tuple[tuple[ProjectedEvent, ...], dict[int, tuple[Any, ...]], frozenset[int]]:
    """Canonicalize only proven equivalent mapper-input child scope blocks."""

    raw_mapper_snapshots = trace.payload.get("mapperSnapshots")
    if not isinstance(raw_mapper_snapshots, Mapping):
        return tuple(events), {}, frozenset()
    events_by_scope: dict[int, list[ProjectedEvent]] = {}
    for event in events:
        events_by_scope.setdefault(int(event.raw.get("scopeSequence", 0)), []).append(event)

    replacements: dict[int, list[ProjectedEvent]] = {}
    scope_keys: dict[int, tuple[Any, ...]] = {}
    projected_scopes: set[int] = set()
    for parent_scope, parent_events in events_by_scope.items():
        mapper_event = next(
            (event for event in parent_events if event.raw.get("slice") == "mapper.snapshot"),
            None,
        )
        if mapper_event is None:
            continue
        fields = mapper_event.raw.get("fields")
        snapshot_id = fields.get("snapshot") if isinstance(fields, Mapping) else None
        snapshot = raw_mapper_snapshots.get(snapshot_id) if isinstance(snapshot_id, str) else None
        payload = snapshot.get("payload") if isinstance(snapshot, Mapping) else None
        raw = payload.get("raw") if isinstance(payload, Mapping) else None
        inputs = raw.get("inputs") if isinstance(raw, Mapping) else None
        if not isinstance(inputs, list):
            continue
        child_scopes = sorted(
            (
                sequence
                for sequence, scope_events in events_by_scope.items()
                if sequence in trace.scopes
                and trace.scopes[sequence].parent == parent_scope
                and any(
                    event.raw.get("slice") == "toposhape.map_sub_element"
                    for event in scope_events
                )
            ),
            key=lambda sequence: min(event.sequence for event in events_by_scope[sequence]),
        )
        if len(child_scopes) != len(inputs):
            continue

        descriptors: list[Any] = []
        for item in inputs:
            if not isinstance(item, Mapping) or not isinstance(item.get("sourceOrdinal"), int):
                descriptors = []
                break
            descriptors.append(_stable_sort_value(item.get("inventory")))
        if not descriptors:
            continue

        grouped: dict[Any, list[int]] = {}
        for scope_sequence, descriptor in zip(child_scopes, descriptors, strict=True):
            grouped.setdefault(descriptor, []).append(scope_sequence)
        for descriptor, group_scopes in grouped.items():
            if len(group_scopes) < 2:
                continue
            sorted_scopes = sorted(
                group_scopes,
                key=lambda sequence: _mapper_scope_block_key(events_by_scope[sequence]),
            )
            positions = sorted(
                index
                for scope_sequence in group_scopes
                for index, event in enumerate(events)
                if int(event.raw.get("scopeSequence", 0)) == scope_sequence
            )
            sorted_events = [
                event
                for scope_sequence in sorted_scopes
                for event in events_by_scope[scope_sequence]
            ]
            if len(positions) != len(sorted_events):
                continue
            for position, event in zip(positions, sorted_events, strict=True):
                replacements[position] = [event]
            for rank, scope_sequence in enumerate(sorted_scopes):
                scope_keys[scope_sequence] = ("mapper-source-group", parent_scope, descriptor, rank)
                projected_scopes.add(scope_sequence)

        # The same mapper-input loop also emits parent-scope read/read/can-map
        # blocks. Canonicalize those blocks with the identical descriptor
        # grouping; no other parent events are reorderable.
        parent_positions = [
            index
            for index, event in enumerate(events)
            if int(event.raw.get("scopeSequence", 0)) == parent_scope
        ]
        candidate_ends: list[int] = []
        for local_index, position in enumerate(parent_positions):
            event = events[position]
            if not (
                event.raw.get("slice") == "toposhape.can_map"
                and event.raw.get("decision") == "accepted"
                and event.raw.get("reason") == "cache_ready"
            ):
                continue
            candidate_ends.append(local_index)
        query_descriptors: list[Any] = [
            (operation_role, descriptor)
            for operation_role in ("Input", "Vertex", "Edge", "Face")
            for descriptor in descriptors
        ]
        if len(candidate_ends) == len(query_descriptors):
            candidate_blocks: list[list[int]] = []
            previous_end = -1
            movable_slices = {
                "element_map.find",
                "element_map.find_all",
                "maker.candidate.reject",
                "maker.modified",
                "maker.generated",
                "toposhape.can_map",
            }
            for end in candidate_ends:
                block = parent_positions[previous_end + 1 : end + 1]
                last_unrelated = max(
                    (
                        index
                        for index, position in enumerate(block)
                        if events[position].raw.get("slice") not in movable_slices
                    ),
                    default=-1,
                )
                candidate_blocks.append(block[last_unrelated + 1 :])
                previous_end = end
            query_groups: dict[Any, list[int]] = {}
            for block_index, descriptor in enumerate(query_descriptors):
                query_groups.setdefault(descriptor, []).append(block_index)
            for descriptor, block_indices in query_groups.items():
                if len(block_indices) < 2:
                    continue
                # Only the mapper query operation slice is movable. A block
                # containing unrelated lifecycle events is not proven safe.
                if any(
                    events[position].raw.get("slice")
                    not in movable_slices
                    for index in block_indices
                    for position in candidate_blocks[index]
                ):
                    continue
                ordered_blocks = sorted(
                    (candidate_blocks[index] for index in block_indices),
                    key=lambda block: _mapper_scope_block_key([events[index] for index in block]),
                )
                target_positions = sorted(
                    position for index in block_indices for position in candidate_blocks[index]
                )
                ordered_events = [
                    events[position] for block in ordered_blocks for position in block
                ]
                if len(target_positions) == len(ordered_events):
                    for position, event in zip(target_positions, ordered_events, strict=True):
                        replacements[position] = [event]

    projected: list[ProjectedEvent] = []
    for index, event in enumerate(events):
        projected.extend(replacements.get(index, [event]))
    return tuple(projected), scope_keys, frozenset(projected_scopes)


def _stable_sort_value(value: Any) -> Any:
    if isinstance(value, Mapping):
        return tuple(
            (str(key), _stable_sort_value(item))
            for key, item in sorted(value.items())
            if str(key) not in {"sourceOrdinal", "sourceTag"}
        )
    if isinstance(value, list):
        return tuple(_stable_sort_value(item) for item in value)
    if isinstance(value, str):
        value = re.sub(r"#(?:[0-9a-fA-F]+)", "#<SID>", value)
        value = re.sub(r"(?<![A-Za-z0-9])H-?[0-9a-fA-F]+", "H<TAG>", value)
        return re.sub(r";D-?[0-9a-fA-F]+", ";D<TAG>", value)
    return value


def _mapper_scope_block_key(events: Sequence[ProjectedEvent]) -> Any:
    return tuple(
        (
            event.raw.get("slice"),
            event.raw.get("decision"),
            event.raw.get("reason"),
            _stable_sort_value(event.raw.get("fields")),
        )
        for event in events
    )


def _commutative_read_projection(
    events: Sequence[ProjectedEvent], mappings: _Mappings, *, actual: bool
) -> tuple[ProjectedEvent, ...]:
    def projected(value: Any) -> Any:
        if isinstance(value, Mapping):
            return tuple((str(key), projected(item)) for key, item in sorted(value.items()))
        if isinstance(value, list):
            return tuple(projected(item) for item in value)
        if isinstance(value, str):
            return _mapped_string(value, mappings, actual=actual)
        return value

    result: list[ProjectedEvent] = []
    index = 0
    while index < len(events):
        event = events[index]
        anchor = (
            event.raw.get("transactionSequence"),
            event.raw.get("scopeSequence"),
            event.raw.get("producer"),
            event.raw.get("beforeSnapshot"),
            event.raw.get("afterSnapshot"),
        )
        end = index + 1
        while end < len(events):
            candidate = events[end]
            candidate_anchor = (
                candidate.raw.get("transactionSequence"),
                candidate.raw.get("scopeSequence"),
                candidate.raw.get("producer"),
                candidate.raw.get("beforeSnapshot"),
                candidate.raw.get("afterSnapshot"),
            )
            if candidate_anchor != anchor:
                break
            end += 1
        window = list(events[index:end])
        read_positions = [
            offset
            for offset, item in enumerate(window)
            if item.raw.get("slice") in {"element_map.find", "element_map.find_all"}
        ]
        reads = [window[offset] for offset in read_positions]
        reads.sort(
            key=lambda item: repr(
                (
                    item.raw.get("slice"),
                    item.raw.get("decision"),
                    item.raw.get("reason"),
                    projected(item.raw.get("fields")),
                )
            )
        )
        for offset, item in zip(read_positions, reads, strict=True):
            window[offset] = item
        result.extend(window)
        index = end
    return tuple(result)


def compare_traces(
    expected: Mapping[str, Any] | str,
    actual: Mapping[str, Any] | str,
    *,
    document_graph: Mapping[str, Any] | None = None,
) -> ComparisonResult:
    """Return the earliest structural/state divergence without mutating either trace."""

    for side, trace in (("expected", expected), ("actual", actual)):
        try:
            _validate_access_summary_extension(trace)
        except (TraceValidationError, RuntimeError, OSError, ValueError) as exc:
            return _invalid(side, exc)

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
    tag_reference_error = _bind_tag_reference_graph(
        expected_trace, actual_trace, mappings
    )
    if tag_reference_error:
        return ComparisonResult(
            status="different",
            classification="identity_mapping_failure",
            json_pointer="/tagReferences",
            owner="runtime/recompute",
            detail=tag_reference_error,
            semantic_difference_count=1,
        )
    sid_graph_error = _bind_sid_graph(expected_trace, actual_trace, mappings)
    if sid_graph_error:
        return ComparisonResult(
            status="different",
            classification="sid_allocation_mismatch",
            json_pointer="/stringTableSnapshots",
            owner="app/string_hasher",
            detail=sid_graph_error,
            semantic_difference_count=1,
        )
    sid_reference_error = _bind_sid_reference_graph(
        expected_trace, actual_trace, mappings
    )
    if sid_reference_error:
        return ComparisonResult(
            status="different",
            classification="sid_allocation_mismatch",
            json_pointer="/sidReferences",
            owner="app/string_hasher",
            detail=sid_reference_error,
            semantic_difference_count=1,
        )
    opaque_index_error = _bind_opaque_index_graph(expected_trace, actual_trace, mappings)
    if opaque_index_error:
        return ComparisonResult(
            status="different",
            classification="opaque_mapped_index_mismatch",
            json_pointer="/mappedNameIndex",
            owner="part/topo_shape",
            detail=opaque_index_error,
            semantic_difference_count=1,
        )

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
    owner_reordered, expected_ordered, actual_ordered = _independent_owner_summary_reorder(
        expected_trace, actual_trace
    )
    expected_events, expected_mapper_keys, expected_mapper_scopes = _mapper_scope_projection(
        expected_trace, expected_ordered
    )
    actual_events, actual_mapper_keys, actual_mapper_scopes = _mapper_scope_projection(
        actual_trace, actual_ordered
    )
    expected_events = _commutative_read_projection(
        expected_events, mappings, actual=False
    )
    actual_events = _commutative_read_projection(
        actual_events, mappings, actual=True
    )
    difference = _compare_event_stream(
        expected_trace,
        actual_trace,
        expected_events,
        actual_events,
        mappings,
        actual_scope_to_expected=actual_scope_to_expected,
        expected_mapper_scope_keys=expected_mapper_keys,
        actual_mapper_scope_keys=actual_mapper_keys,
        expected_projected_mapper_scopes=expected_mapper_scopes,
        actual_projected_mapper_scopes=actual_mapper_scopes,
    )
    if difference:
        return difference

    raw_difference_count = _raw_difference_count(
        expected_trace.payload, actual_trace.payload
    )
    normalizations = list(_normalization_records(mappings))
    if raw_difference_count and any(_summary_ref(event) is not None for event in expected_events + actual_events):
        normalizations.append(
            NormalizationRecord(
                reason_code=PROJECTION_POLICY.derived_access_summary_reason,
                expected_json_pointer="/fields/summaryRef",
                actual_json_pointer="/fields/summaryRef",
                semantic_key="projected-access-summary",
            )
        )
    if owner_reordered:
        normalizations.append(
            NormalizationRecord(
                reason_code=PROJECTION_POLICY.independent_owner_reorder_reason,
                expected_json_pointer="/events",
                actual_json_pointer="/events",
                semantic_key="access-summary-owner-window",
            )
        )
    if raw_difference_count and expected_mapper_scopes and actual_mapper_scopes:
        normalizations.append(
            NormalizationRecord(
                reason_code="equivalent_mapper_source_permutation",
                expected_json_pointer="/mapperSnapshots",
                actual_json_pointer="/mapperSnapshots",
                semantic_key="descriptor-grouped-relation-multiset",
            )
        )
    return ComparisonResult(
        status="equal",
        classification="aligned",
        before_alignment="aligned",
        after_alignment="aligned",
        equivalence=(
            "raw"
            if raw_difference_count == 0
            else "projected"
        ),
        normalizations=tuple(normalizations),
        raw_difference_count=raw_difference_count,
        semantic_difference_count=0,
    )
