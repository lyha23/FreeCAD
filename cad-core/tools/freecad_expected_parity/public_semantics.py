"""Compare the public semantics shared by FreeCAD and CAD Core.

The native expected payload and the CAD Core response are different producer
formats.  This module is the seam between them: it keeps product-only fields
and producer-local tokens out of native parity while retaining strict checks
for diagnostics, geometry and reference semantics.
"""

from __future__ import annotations

import json
from collections import Counter
from typing import Any


RAW_FIELDS = {"rawFreecadMappedName", "raw_mapped_name"}
LOCAL_SUBSHAPE_FIELDS = {"id", "indexed", "subname", "fullSubname", "stableSubname", "resolvedIndexed"}
PRODUCT_FIELDS = {
    "binaryPayloads",
    "documentObjectUpdates",
    "mesh",
}
GEOMETRY_NUMERIC_FIELDS = {
    "area",
    "bbox",
    "center",
    "centerOfMass",
    "length",
    "matrix",
    "max",
    "mesh",
    "min",
    "normal",
    "placement",
    "points",
    "position",
    "rotation",
    "summary",
    "volume",
}
STRICT_COLLECTIONS = {"diagnostics", "results", "elementReferenceUpdates"}
KEYED_COLLECTIONS = {"diagnostics", "results", "subshapes", "childElementMaps", "mapperHistory"}


def _path_has(path: tuple[str, ...], value: str) -> bool:
    return value in path


def _is_raw_path(path: tuple[str, ...]) -> bool:
    return bool(path) and (path[-1] in RAW_FIELDS or path[-2:] == ("mappedName", "raw"))


def _is_local_subshape_field(path: tuple[str, ...]) -> bool:
    return bool(path) and path[-1] in LOCAL_SUBSHAPE_FIELDS and _path_has(path, "subshapes")


def _canonical_identity(item: Any, fallback: str) -> str:
    if not isinstance(item, dict):
        return fallback
    for key in ("canonicalFreecadMappedName", "canonicalMappedName"):
        value = item.get(key)
        if isinstance(value, str) and value:
            return value
    mapped_name = item.get("mappedName")
    if isinstance(mapped_name, dict):
        value = mapped_name.get("canonical")
        if isinstance(value, str) and value:
            return value
    return fallback


def _mapper_history_semantics(item: Any) -> dict[str, Any]:
    """Keep the public outcome of history, not producer-local evidence IDs."""

    if not isinstance(item, dict):
        return {"value": item}
    candidates = item.get("candidates")
    if not isinstance(candidates, list):
        candidates = []
    shape_kinds = sorted(
        str(candidate.get("shapeKind") or candidate.get("shape_kind"))
        for candidate in candidates
        if isinstance(candidate, dict) and (candidate.get("shapeKind") or candidate.get("shape_kind"))
    )
    source = item.get("source") if isinstance(item.get("source"), dict) else {}
    target = item.get("target") if isinstance(item.get("target"), dict) else {}
    return {
        "relation": item.get("relation"),
        "recoverability": item.get("recoverability"),
        "diagnosticStatus": item.get("diagnostic_status"),
        "sourceObject": source.get("object"),
        "targetObject": target.get("object"),
        "candidateCount": len(candidates),
        "shapeKinds": shape_kinds,
    }


def _collection_key(item: Any, collection: str, index: int) -> str:
    if not isinstance(item, dict):
        return f"index:{index}"
    if collection == "diagnostics":
        return str(item.get("code") or f"index:{index}")
    if collection == "results":
        return str(item.get("object") or f"index:{index}")
    if collection == "subshapes":
        return _canonical_identity(item, str(item.get("id") or item.get("indexed") or index))
    if collection == "childElementMaps":
        return str(item.get("key") or item.get("pathPrefix") or f"index:{index}")
    if collection == "mapperHistory":
        return "|".join(
            (
                str(item.get("relation") or ""),
                str(item.get("recoverability") or ""),
                str(item.get("sourceObject") or ""),
                str(item.get("targetObject") or ""),
                str(item.get("candidateCount") or 0),
                ",".join(item.get("shapeKinds") or []),
            )
        )
    return f"index:{index}"


def _normalize_element_entries(value: Any) -> Any:
    if not isinstance(value, dict):
        return value
    normalized: dict[str, Any] = {}
    for raw_key, entry in value.items():
        key = _canonical_identity(entry.get("value") if isinstance(entry, dict) else None, str(raw_key))
        normalized[key] = entry
    return normalized


def _normalize_keyed_list(value: list[Any], collection: str) -> dict[str, Any]:
    counts: Counter[str] = Counter()
    result: dict[str, Any] = {}
    for index, item in enumerate(value):
        base = _collection_key(item, collection, index)
        suffix = counts[base]
        counts[base] += 1
        result[base if suffix == 0 else f"{base}#{suffix + 1}"] = item
    return result


def _normalize(value: Any, path: tuple[str, ...] = ()) -> Any:
    if _is_raw_path(path) or _is_local_subshape_field(path):
        return None
    if isinstance(value, list):
        if path and path[-1] in KEYED_COLLECTIONS:
            if path[-1] == "mapperHistory":
                value = [_mapper_history_semantics(item) for item in value]
            value = _normalize_keyed_list(value, path[-1])
        else:
            return [_normalize(item, path + (str(index),)) for index, item in enumerate(value)]
    if isinstance(value, dict):
        result: dict[str, Any] = {}
        for key, item in value.items():
            key = str(key)
            if key in RAW_FIELDS or key in PRODUCT_FIELDS:
                continue
            if key == "mappedName" and isinstance(item, dict):
                result[key] = {
                    child_key: _normalize(child, path + (key, child_key))
                    for child_key, child in item.items()
                    if child_key != "raw"
                }
                continue
            if key == "entries" and path and path[-1] == "elementMap":
                item = _normalize_element_entries(item)
            result[key] = _normalize(item, path + (key,))
        return result
    return value


def _strict_extra_keys(path: tuple[str, ...]) -> bool:
    return bool(path) and path[-1] in STRICT_COLLECTIONS


def _make_diff(kind: str, path: tuple[str, ...], expected: Any, actual: Any) -> dict[str, Any]:
    diff: dict[str, Any] = {
        "category": _category(path, expected, actual),
        "comparisonClass": "public_semantic",
        "kind": kind,
        "path": ".".join(path) if path else "$",
        "_actualValue": actual,
    }
    if expected is not None:
        diff["expected"] = _summary(expected)
    if actual is not None:
        diff["actual"] = _summary(actual)
    return diff


def _summary(value: Any) -> Any:
    if isinstance(value, dict):
        return {"type": "object", "size": len(value), "keys": sorted(value)[:12]}
    if isinstance(value, list):
        return {"type": "array", "size": len(value)}
    return value


def _category(path: tuple[str, ...], expected: Any, actual: Any) -> str:
    if (
        isinstance(expected, (int, float))
        and not isinstance(expected, bool)
        or isinstance(actual, (int, float))
        and not isinstance(actual, bool)
    ) and any(part in GEOMETRY_NUMERIC_FIELDS for part in path):
        return "geometry.numeric"
    if path and path[0] == "diagnostics":
        return "diagnostics"
    if path and path[0] == "results":
        return "results.subshapes" if "subshapes" in path else "results"
    if path and path[0] == "topoNamingState":
        for name in ("childElementMaps", "mapperHistory", "elementMap", "subshapes"):
            if name in path:
                return f"topoNamingState.{name}"
        return "topoNamingState.objects"
    if path and path[0] == "elementReferenceUpdates":
        return "json"
    return "json"


def _diff(expected: Any, actual: Any, path: tuple[str, ...], diffs: list[dict[str, Any]]) -> None:
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            child = path + (str(key),)
            if key not in expected:
                if _strict_extra_keys(path):
                    diffs.append(_make_diff("extra", child, None, actual[key]))
                continue
            if key not in actual:
                diffs.append(_make_diff("missing", child, expected[key], None))
                continue
            _diff(expected[key], actual[key], child, diffs)
        return
    if isinstance(expected, list) and isinstance(actual, list):
        for index in range(max(len(expected), len(actual))):
            child = path + (str(index),)
            if index >= len(expected):
                diffs.append(_make_diff("extra", child, None, actual[index]))
            elif index >= len(actual):
                diffs.append(_make_diff("missing", child, expected[index], None))
            else:
                _diff(expected[index], actual[index], child, diffs)
        return
    if isinstance(expected, (int, float)) and isinstance(actual, (int, float)):
        if any(part in GEOMETRY_NUMERIC_FIELDS for part in path):
            different = abs(float(expected) - float(actual)) > 1e-6
        else:
            different = expected != actual
        if different:
            diffs.append(_make_diff("numeric", path, expected, actual))
        return
    if type(expected) is not type(actual) or expected != actual:
        diffs.append(_make_diff("value", path, expected, actual))


def compare_public_semantics(expected: dict[str, Any], actual: dict[str, Any]) -> list[dict[str, Any]]:
    """Compare the public semantic projection, not the raw producer payloads."""

    expected_view = _normalize(expected)
    actual_view = _normalize(actual)
    diffs: list[dict[str, Any]] = []
    _diff(expected_view, actual_view, (), diffs)
    return diffs
