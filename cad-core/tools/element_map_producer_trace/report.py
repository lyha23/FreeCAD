"""Stable JSON report projection for first-divergence results."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

from .model import ComparisonResult


REPORT_SCHEMA = "freecad.element-map-producer-trace-comparison.v1"


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _artifact_reference(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {"path": None, "sha256": None}
    return {
        "path": str(path),
        "sha256": file_sha256(path) if path.is_file() else None,
    }


def invalid_report_payload(
    *,
    classification: str,
    detail: str,
    phase: str | None,
    case: str | None,
    expected_path: Path | None,
    actual_path: Path | None,
) -> dict[str, Any]:
    """Project an input/closure failure into the same stable report envelope."""

    expected = _artifact_reference(expected_path)
    actual = _artifact_reference(actual_path)
    return {
        "schema": REPORT_SCHEMA,
        "status": "invalid",
        "classification": classification,
        "source": {"phase": phase, "case": case},
        "expected": expected,
        "actual": actual,
        "traceHashes": {
            "expected": expected["sha256"],
            "actual": actual["sha256"],
        },
        "transactionOrdinal": None,
        "semanticScopePath": [],
        "expectedEvent": None,
        "actualEvent": None,
        "eventIdentity": {"expected": None, "actual": None},
        "firstJsonPointer": None,
        "expectedValue": None,
        "actualValue": None,
        "downstreamDriftCount": 0,
        "beforeAlignment": "not_compared",
        "afterAlignment": "not_compared",
        "owner": "unknown",
        "detail": detail,
    }


def report_payload(
    result: ComparisonResult,
    *,
    phase: str | None,
    case: str | None,
    expected_path: Path,
    actual_path: Path,
) -> dict[str, Any]:
    expected_hash = file_sha256(expected_path)
    actual_hash = file_sha256(actual_path)
    return {
        "schema": REPORT_SCHEMA,
        "status": result.status,
        "classification": result.classification,
        "source": {"phase": phase, "case": case},
        "expected": {"path": str(expected_path), "sha256": expected_hash},
        "actual": {"path": str(actual_path), "sha256": actual_hash},
        "traceHashes": {"expected": expected_hash, "actual": actual_hash},
        "transactionOrdinal": result.transaction_ordinal,
        "semanticScopePath": list(result.semantic_scope_path),
        "expectedEvent": result.expected_event,
        "actualEvent": result.actual_event,
        "eventIdentity": {
            "expected": list(result.expected_event_identity) if result.expected_event_identity else None,
            "actual": list(result.actual_event_identity) if result.actual_event_identity else None,
        },
        "firstJsonPointer": result.json_pointer,
        "expectedValue": result.expected_value,
        "actualValue": result.actual_value,
        "downstreamDriftCount": result.downstream_drift_count,
        "beforeAlignment": result.before_alignment,
        "afterAlignment": result.after_alignment,
        "owner": result.owner,
        "detail": result.detail,
    }


def human_report(payload: dict[str, Any]) -> str:
    lines = [
        f"FIRST_DIVERGENCE {payload.get('classification', 'invalid_trace')}",
        f"status: {payload.get('status', 'invalid')}",
    ]
    scope = payload.get("semanticScopePath")
    if scope:
        lines.append("scope: " + " > ".join(str(value) for value in scope))
    event_identity = payload.get("eventIdentity")
    if event_identity:
        lines.append("event identity: " + json.dumps(event_identity, ensure_ascii=False, sort_keys=True))
    if payload.get("firstJsonPointer") is not None:
        lines.append(f"first JSON path: {payload['firstJsonPointer']}")
        lines.append("expected: " + json.dumps(payload.get("expectedValue"), ensure_ascii=False, sort_keys=True))
        lines.append("actual: " + json.dumps(payload.get("actualValue"), ensure_ascii=False, sort_keys=True))
    lines.append(f"before alignment: {payload.get('beforeAlignment', 'not_compared')}")
    lines.append(f"after alignment: {payload.get('afterAlignment', 'not_compared')}")
    if payload.get("traceHashes"):
        lines.append("trace hashes: " + json.dumps(payload["traceHashes"], sort_keys=True))
    lines.append(f"downstream drift: {payload.get('downstreamDriftCount', 0)} events collapsed")
    lines.append(f"owner: {payload.get('owner', 'unknown')}")
    if payload.get("detail"):
        lines.append(f"detail: {payload['detail']}")
    return "\n".join(lines)


def write_report(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
