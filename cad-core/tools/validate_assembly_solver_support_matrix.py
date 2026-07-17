#!/usr/bin/env python3
"""Fail-closed validator for the A4 Assembly/OndselSolver support matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
from typing import Any

from generate_assembly_solver_support_matrix import (
    DISTANCE_TYPES,
    JOINT_TYPES,
    MARKER_FAMILIES,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = (
    ROOT / "tools" / "freecad_expected_parity" / "assembly_solver_support_matrix.v1.json"
)
DEFAULT_REPORT = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "reports"
    / "assembly_solver_support_matrix.v1.json"
)
DEFAULT_ROLES = ROOT / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"


def canonical_hash(payload: Any) -> str:
    encoded = json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return "sha256:" + hashlib.sha256(encoded).hexdigest()


def assembly_payload(expected: dict[str, Any]) -> dict[str, Any] | None:
    for result in expected.get("results", []):
        if isinstance(result, dict) and result.get("object") == "Assembly":
            return result
    if expected.get("object") == "Assembly":
        return expected
    return None


def solver_joints(payload: dict[str, Any]) -> list[dict[str, Any]]:
    rows = payload.get("solver_adapter", {}).get("solver_joints", [])
    return [row for row in rows if isinstance(row, dict)]


def validate_checks(payload: dict[str, Any], checks: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    adapter = payload.get("solver_adapter", {})
    joints = solver_joints(payload)
    if payload.get("native_solver", {}).get("return_code") != checks.get("nativeSolverReturn"):
        errors.append("nativeSolverReturn")
    if adapter.get("status") != checks.get("solverStatus"):
        errors.append("solverStatus")
    if "solverReason" in checks and adapter.get("reason") != checks["solverReason"]:
        errors.append("solverReason")
    actual_joint_types = {str(row.get("joint_type", "")) for row in joints}
    for joint_type in checks.get("jointTypes", []):
        if joint_type not in actual_joint_types:
            errors.append(f"jointType:{joint_type}")
    actual_distance_types = {str(row.get("distance_type", "")) for row in joints}
    for distance_type in checks.get("distanceTypes", []):
        if distance_type not in actual_distance_types:
            errors.append(f"distanceType:{distance_type}")
    updates = adapter.get("placement_updates", [])
    if not isinstance(updates, list):
        updates = []
    if len(updates) < int(checks.get("minimumPlacementUpdates", 0)):
        errors.append("minimumPlacementUpdates")
    actual_update_objects = {
        str(row.get("object", "")) for row in updates if isinstance(row, dict)
    }
    for object_name in checks.get("placementUpdateObjects", []):
        if object_name not in actual_update_objects:
            errors.append(f"placementUpdateObject:{object_name}")
    if "maximumPlacementUpdates" in checks and len(updates) > int(checks["maximumPlacementUpdates"]):
        errors.append("maximumPlacementUpdates")
    if "slidingPartIndex" in checks and not any(
        row.get("sliding_part_index") == checks["slidingPartIndex"] for row in joints
    ):
        errors.append("slidingPartIndex")
    return errors


def validate(
    matrix_path: Path,
    *,
    fixtures_root: Path,
    roles_path: Path,
) -> dict[str, Any]:
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    roles_doc = json.loads(roles_path.read_text(encoding="utf-8"))
    native_roles = {
        f"{row.get('phase')}/{row.get('case')}"
        for row in roles_doc.get("roles", [])
        if isinstance(row, dict) and row.get("role") == "native"
    }
    errors: list[str] = []
    if matrix.get("schema") != "freecad-assembly-solver-support-matrix/v1":
        errors.append("invalid matrix schema")
    if matrix.get("jointTypes") != JOINT_TYPES:
        errors.append("jointTypes do not match source denominator")
    if matrix.get("markerGeometryFamilies") != MARKER_FAMILIES:
        errors.append("markerGeometryFamilies do not match source denominator")
    if matrix.get("distanceTypes") != DISTANCE_TYPES:
        errors.append("distanceTypes do not match source denominator")

    evidence_rows = matrix.get("evidenceClasses", [])
    rows = matrix.get("rows", [])
    evidence_by_id: dict[str, dict[str, Any]] = {}
    for item in evidence_rows if isinstance(evidence_rows, list) else []:
        evidence_id = item.get("id") if isinstance(item, dict) else None
        if not isinstance(evidence_id, str) or not evidence_id or evidence_id in evidence_by_id:
            errors.append("duplicate or invalid evidence class id")
            continue
        evidence_by_id[evidence_id] = item

    row_ids: set[str] = set()
    seen_joint_types: set[str] = set()
    seen_marker_families: set[str] = set()
    seen_distance_types: set[str] = set()
    referenced_evidence: set[str] = set()
    for row in rows if isinstance(rows, list) else []:
        if not isinstance(row, dict) or not isinstance(row.get("id"), str):
            errors.append("invalid matrix row")
            continue
        row_id = row["id"]
        if row_id in row_ids:
            errors.append(f"duplicate matrix row: {row_id}")
        row_ids.add(row_id)
        joint_type = row.get("jointType")
        marker_families = row.get("markerGeometryFamilies")
        evidence_ids = row.get("evidenceClassIds")
        if joint_type not in JOINT_TYPES:
            errors.append(f"{row_id}: invalid jointType")
        else:
            seen_joint_types.add(joint_type)
        if not isinstance(marker_families, list) or not marker_families:
            errors.append(f"{row_id}: markerGeometryFamilies are missing")
        else:
            for family in marker_families:
                if family not in MARKER_FAMILIES:
                    errors.append(f"{row_id}: unknown marker family {family}")
                else:
                    seen_marker_families.add(family)
        if joint_type == "Distance":
            distance_type = row.get("distanceType")
            if distance_type not in DISTANCE_TYPES:
                errors.append(f"{row_id}: invalid distanceType")
            else:
                seen_distance_types.add(distance_type)
        if not isinstance(evidence_ids, list) or not evidence_ids:
            errors.append(f"{row_id}: evidenceClassIds are missing")
        else:
            for evidence_id in evidence_ids:
                if evidence_id not in evidence_by_id:
                    errors.append(f"{row_id}: missing evidence class {evidence_id}")
                else:
                    referenced_evidence.add(evidence_id)

    for label, expected, actual in (
        ("joint", set(JOINT_TYPES), seen_joint_types),
        ("marker", set(MARKER_FAMILIES), seen_marker_families),
        ("distance", set(DISTANCE_TYPES), seen_distance_types),
    ):
        missing = sorted(expected - actual)
        if missing:
            errors.append(f"missing {label} equivalence classes: {', '.join(missing)}")

    unused = sorted(set(evidence_by_id) - referenced_evidence)
    if unused:
        errors.append("unreferenced evidence classes: " + ", ".join(unused))

    evidence_receipts: list[dict[str, Any]] = []
    missing_evidence: list[str] = []
    invalid_evidence: list[str] = []
    for evidence_id, item in sorted(evidence_by_id.items()):
        fixture = item.get("fixture")
        receipt: dict[str, Any] = {
            "id": evidence_id,
            "fixture": fixture,
            "level": item.get("level"),
            "status": "missing",
        }
        if not isinstance(fixture, str) or "/" not in fixture:
            missing_evidence.append(evidence_id)
            evidence_receipts.append(receipt)
            continue
        phase, case = fixture.split("/", 1)
        input_path = fixtures_root / phase / f"{case}.json"
        expected_path = fixtures_root / phase / "expected" / f"{case}.freecad.json"
        ledger_path = fixtures_root / phase / "expected" / f"{case}.freecad.ledger.json"
        receipt.update({
            "input": str(input_path),
            "expected": str(expected_path),
            "ledger": str(ledger_path),
        })
        receipt_errors: list[str] = []
        if fixture not in native_roles:
            receipt_errors.append("nativeRole")
        if not input_path.is_file():
            receipt_errors.append("input")
        if not expected_path.is_file():
            receipt_errors.append("expected")
        if not ledger_path.is_file():
            receipt_errors.append("ledger")
        if not receipt_errors:
            expected = json.loads(expected_path.read_text(encoding="utf-8"))
            ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
            payload = assembly_payload(expected)
            if payload is None:
                receipt_errors.append("assemblyPayload")
            else:
                receipt_errors.extend(validate_checks(payload, item.get("checks", {})))
            fixture_record = ledger.get("fixture", {})
            if ledger.get("outcome") != "accepted":
                receipt_errors.append("ledgerOutcome")
            if fixture_record.get("phase") != phase or fixture_record.get("case") != case:
                receipt_errors.append("ledgerFixtureIdentity")
            if fixture_record.get("expectedPayloadHash") != canonical_hash(expected):
                receipt_errors.append("ledgerExpectedPayloadHash")
            if ledger.get("roundTrip", {}).get("status") != "passed":
                receipt_errors.append("ledgerRoundTrip")
        if receipt_errors:
            receipt["status"] = "invalid"
            receipt["errors"] = sorted(set(receipt_errors))
            invalid_evidence.append(evidence_id)
        else:
            receipt["status"] = "passed"
        evidence_receipts.append(receipt)

    missing_unique = sorted(set(missing_evidence))
    invalid_unique = sorted(set(invalid_evidence))
    status = "passed" if not errors and not missing_unique and not invalid_unique else "failed"
    return {
        "schema": "freecad-assembly-solver-support-matrix-report/v1",
        "status": status,
        "matrix": {
            "path": str(matrix_path),
            "sha256": hashlib.sha256(matrix_path.read_bytes()).hexdigest(),
        },
        "summary": {
            "jointTypeCount": len(seen_joint_types),
            "markerGeometryFamilyCount": len(seen_marker_families),
            "distanceTypeCount": len(seen_distance_types),
            "rowCount": len(row_ids),
            "evidenceClassCount": len(evidence_by_id),
            "missingEquivalenceClassCount": len(missing_unique),
            "invalidEvidenceClassCount": len(invalid_unique),
            "evidenceStatusCounts": dict(sorted(Counter(row["status"] for row in evidence_receipts).items())),
        },
        "missingEquivalenceClasses": missing_unique,
        "invalidEvidenceClasses": invalid_unique,
        "errors": errors,
        "evidenceReceipts": evidence_receipts,
        "fixtureCorpusClosure": "authority_artifacts_and_roles_only",
        "cadCoreRuntimeParity": "not_evaluated",
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", default=str(DEFAULT_MATRIX))
    parser.add_argument("--fixtures-root", default=str(ROOT / "fixtures"))
    parser.add_argument("--roles", default=str(DEFAULT_ROLES))
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    args = parser.parse_args()
    report = validate(
        Path(args.matrix),
        fixtures_root=Path(args.fixtures_root),
        roles_path=Path(args.roles),
    )
    write_json(Path(args.report), report)
    summary = report["summary"]
    print(
        "assembly solver support matrix: "
        f"status={report['status']} rows={summary['rowCount']} "
        f"evidence={summary['evidenceClassCount']} "
        f"missing={summary['missingEquivalenceClassCount']} "
        f"invalid={summary['invalidEvidenceClassCount']}"
    )
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
