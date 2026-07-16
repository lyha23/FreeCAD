#!/usr/bin/env python3
"""Build the deterministic fixture-role inventory and non-native triage report."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

from collect_freecad_expected import (
    SUPPORTED_NATIVE_TYPES,
    file_sha256,
    topo_state_request_error_response,
)
from freecad_expected_parity.catalog import load_catalog


ROOT = Path(__file__).resolve().parents[1]
ROLES_PATH = Path(__file__).with_name("freecad_expected_parity") / "fixture_roles.v1.json"
SCHEMA = "freecad-fixture-authority-inventory/v1"

COLLECTOR_STRUCTURED_PROPERTY_TYPES = {
    "App::PropertyBool",
    "App::PropertyBoolList",
    "App::PropertyDirection",
    "App::PropertyDistance",
    "App::PropertyEnumeration",
    "App::PropertyFloat",
    "App::PropertyInteger",
    "App::PropertyLength",
    "App::PropertyLink",
    "App::PropertyLinkGlobal",
    "App::PropertyLinkHidden",
    "App::PropertyLinkList",
    "App::PropertyLinkListHidden",
    "App::PropertyLinkSub",
    "App::PropertyLinkSubHidden",
    "App::PropertyLinkSubList",
    "App::PropertyLinkSubListHidden",
    "App::PropertyPlacement",
    "App::PropertyPlacementList",
    "App::PropertyString",
    "App::PropertyVector",
    "App::PropertyVectorDistance",
    "App::PropertyVectorList",
    "App::PropertyXLink",
    "App::PropertyXLinkSub",
    "App::PropertyXLinkSubHidden",
    "App::PropertyXLinkSubList",
}

FAMILY_PRIORITY = {
    "part_primitives": 1,
    "sketch": 2,
    "body_pad_pocket": 3,
    "link": 4,
    "topology_state": 5,
    "part_boolean": 6,
    "revolution_groove": 7,
    "dressup_transformed": 8,
    "loft_pipe_sweep": 9,
    "assembly_mesh": 10,
    "other": 99,
}


def canonical_sha256(value: Any) -> str:
    payload = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def relative(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return str(path.resolve())


def input_paths(fixtures_root: Path) -> list[Path]:
    return sorted(path for path in fixtures_root.glob("*/*.json") if path.is_file())


def nested_property_types(value: Any) -> set[str]:
    result: set[str] = set()
    if isinstance(value, dict):
        property_type = value.get("PropertyType")
        if isinstance(property_type, str) and property_type:
            result.add(property_type)
        for item in value.values():
            result.update(nested_property_types(item))
    elif isinstance(value, list):
        for item in value:
            result.update(nested_property_types(item))
    return result


def object_types(payload: dict[str, Any]) -> set[str]:
    return {
        str(item.get("TypeId"))
        for item in payload.get("Objects", [])
        if isinstance(item, dict) and isinstance(item.get("TypeId"), str)
    }


def has_non_empty_sketch_constraints(payload: dict[str, Any]) -> bool:
    return any(
        item.get("TypeId") == "Sketcher::SketchObject"
        and bool((item.get("Properties") or {}).get("Constraints"))
        for item in payload.get("Objects", [])
        if isinstance(item, dict)
    )


def business_family(payload: dict[str, Any]) -> str:
    types = object_types(payload)
    if types & {"Assembly::AssemblyObject", "Assembly::AssemblyLink", "Assembly::JointGroup", "Mesh::Import"}:
        return "assembly_mesh"
    if types & {"PartDesign::AdditiveLoft", "PartDesign::SubtractiveLoft", "PartDesign::AdditivePipe", "PartDesign::SubtractivePipe", "Part::Loft", "Part::Sweep"}:
        return "loft_pipe_sweep"
    if types & {"PartDesign::Fillet", "PartDesign::Chamfer", "PartDesign::LinearPattern", "PartDesign::PolarPattern", "PartDesign::Mirrored", "PartDesign::MultiTransform", "PartDesign::Scaled"}:
        return "dressup_transformed"
    if types & {"PartDesign::Revolution", "PartDesign::Groove"}:
        return "revolution_groove"
    if types & {"Part::Cut", "Part::Fuse", "Part::Common", "Part::MultiFuse", "Part::MultiCommon", "Part::BooleanFragments", "Part::XOR", "PartDesign::Boolean"}:
        return "part_boolean"
    if types & {"App::Link", "App::LinkElement", "App::LinkGroup"}:
        return "link"
    if types & {"PartDesign::Body", "PartDesign::Pad", "PartDesign::Pocket", "PartDesign::Hole"}:
        return "body_pad_pocket"
    if types == {"Sketcher::SketchObject"}:
        return "sketch"
    if types and all(item.startswith("Part::") for item in types):
        return "part_primitives"
    state = payload.get("topoNamingState")
    if isinstance(state, dict) and bool(state.get("objects")):
        return "topology_state"
    return "other"


def unsupported_classification(payload: dict[str, Any]) -> dict[str, Any]:
    types = object_types(payload)
    property_types = nested_property_types(payload)
    internal_types = sorted(item for item in types if item.startswith("CadCore::"))
    disabled_types = sorted(
        item
        for item in types
        if item not in SUPPORTED_NATIVE_TYPES and item not in internal_types
    )
    unsupported_property_types = sorted(
        item for item in property_types if item not in COLLECTOR_STRUCTURED_PROPERTY_TYPES
    )
    constraints = has_non_empty_sketch_constraints(payload)
    request_rejection = topo_state_request_error_response(payload)
    rejection_diagnostics = (
        request_rejection.get("diagnostics")
        if isinstance(request_rejection, dict)
        else None
    )
    rejection_code = (
        rejection_diagnostics[0].get("code")
        if isinstance(rejection_diagnostics, list)
        and rejection_diagnostics
        and isinstance(rejection_diagnostics[0], dict)
        else None
    )

    evidence: list[str] = []
    if rejection_code:
        category = "freecad_native_not_expressible"
        evidence.append(f"request preflight rejects before native recompute: {rejection_code}")
        next_action = (
            "Repair or reclassify the input topoNamingState contract before any native authority probe."
        )
        close_condition = (
            "The intended public-root semantics execute, or a source-backed protocol-only decision replaces the case."
        )
    elif internal_types:
        category = "non_native_fixture"
        evidence.append("internal probe TypeId: " + ", ".join(internal_types))
        next_action = "Keep outside native authority; replace with a public FreeCAD object graph before promotion."
        close_condition = "A public-root fixture replaces the internal probe and passes independent FreeCADCmd replay."
    elif disabled_types or unsupported_property_types or constraints:
        category = "collector_general_gap"
        if disabled_types:
            evidence.append("collector-disabled TypeId: " + ", ".join(disabled_types))
        if unsupported_property_types:
            evidence.append(
                "collector-disabled structured property: "
                + ", ".join(unsupported_property_types)
            )
        if constraints:
            evidence.append("non-empty Sketch Constraints are rejected by the collector")
        next_action = "Add source-backed generic TypeId/property support and focused tests, then collect in staging."
        close_condition = "The generic collector path produces a strict-valid expected/ledger pair and repeat 2 passes."
    else:
        category = "not_investigated"
        evidence.append("static collector surface has no known TypeId/property blocker")
        next_action = "Run an isolated staging collection with the controlled FreeCADCmd and classify the observed result."
        close_condition = "A staging probe records either a native promotion receipt or a precise oracle/collector blocker."

    return {
        "category": category,
        "businessFamily": business_family(payload),
        "objectTypes": sorted(types),
        "propertyTypes": sorted(property_types),
        "evidence": evidence,
        "nextAction": next_action,
        "closeCondition": close_condition,
        "candidateEligible": category == "not_investigated",
    }


def probe_failure_category(detail: str) -> str:
    non_native_markers = (
        "link target None was not created",
        "not part of the enumeration",
        "object has no attribute",
        "requires the Profile sketch AttachmentSupport/Support",
        "requires Body Group membership",
        "external geometry target Missing",
    )
    if any(marker in detail for marker in non_native_markers):
        return "non_native_fixture"
    freecad_boundary_markers = (
        "target object",
        "Not able to add external shape element",
    )
    if any(marker in detail for marker in freecad_boundary_markers):
        return "freecad_native_not_expressible"
    return "collector_general_gap"


def probe_receipts(root: Path) -> dict[tuple[str, str], dict[str, Any]]:
    reports_dir = root / "tools" / "freecad_expected_parity" / "reports" / "probes"
    receipts: dict[tuple[str, str], dict[str, Any]] = {}
    for collect_path in sorted(reports_dir.glob("*-collect.json")):
        try:
            report = json.loads(collect_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        cases = report.get("cases")
        if not isinstance(cases, list) or len(cases) != 1 or not isinstance(cases[0], dict):
            continue
        case_report = cases[0]
        phase = case_report.get("phase")
        case = case_report.get("case")
        if not isinstance(phase, str) or not isinstance(case, str):
            continue
        first_error = next(
            (
                item
                for item in case_report.get("errors", [])
                if isinstance(item, dict) and isinstance(item.get("detail"), str)
            ),
            None,
        )
        repeat_path = collect_path.with_name(
            collect_path.name.removesuffix("-collect.json") + "-repeat2.json"
        )
        repeat_report: dict[str, Any] = {}
        if repeat_path.is_file():
            try:
                repeat_report = json.loads(repeat_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                repeat_report = {}
        receipts[(phase, case)] = {
            "collectReport": artifact_record(collect_path, root),
            "status": report.get("status"),
            "ledgerOutcome": case_report.get("ledgerOutcome"),
            "failureStage": first_error.get("stage") if first_error else None,
            "failureDetail": first_error.get("detail") if first_error else None,
            "repeat2Report": artifact_record(repeat_path, root) if repeat_path.is_file() else None,
            "repeat2Status": repeat_report.get("status") if repeat_report else None,
        }
    return receipts


def apply_probe_receipt(
    classification: dict[str, Any],
    receipt: dict[str, Any] | None,
) -> dict[str, Any]:
    if receipt is None:
        return classification
    updated = dict(classification)
    updated["probe"] = receipt
    detail = str(receipt.get("failureDetail") or "")
    if receipt.get("status") == "failed":
        category = probe_failure_category(detail)
        updated["category"] = category
        updated["candidateEligible"] = False
        updated["evidence"] = [*updated.get("evidence", []), f"isolated FreeCADCmd probe: {detail}"]
        updated["nextAction"] = (
            "Resolve the recorded collector/oracle boundary as a generic semantic batch before retrying."
        )
        updated["closeCondition"] = (
            "A new isolated probe reaches a strict-valid authority pair and independent repeat 2."
        )
    elif receipt.get("ledgerOutcome") == "rejected":
        updated["category"] = "freecad_native_not_expressible"
        updated["candidateEligible"] = False
        updated["evidence"] = [
            *updated.get("evidence", []),
            "isolated FreeCADCmd probe produced only a rejected request ledger",
        ]
        updated["nextAction"] = (
            "Repair or reclassify the input contract; do not promote a rejection that bypasses the intended feature semantics."
        )
        updated["closeCondition"] = (
            "The intended public-root semantics execute, or a source-backed protocol-only decision replaces the case."
        )
    elif receipt.get("ledgerOutcome") == "accepted" and receipt.get("repeat2Status") == "passed":
        updated["candidateEligible"] = True
        updated["evidence"] = [
            *updated.get("evidence", []),
            "accepted staging authority passed independent repeat 2",
        ]
        updated["nextAction"] = "Promote input, public expected, ledger, role, and producer receipt atomically."
    return updated


def role_entries(path: Path) -> tuple[list[dict[str, Any]], list[str]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    raw = payload.get("roles")
    if not isinstance(raw, list):
        return [], ["fixture roles must be a list"]
    entries: list[dict[str, Any]] = []
    errors: list[str] = []
    seen: set[tuple[str, str]] = set()
    for index, entry in enumerate(raw):
        if not isinstance(entry, dict):
            errors.append(f"fixture role {index} must be an object")
            continue
        phase = entry.get("phase")
        case = entry.get("case")
        if not isinstance(phase, str) or not isinstance(case, str):
            errors.append(f"fixture role {index} is missing phase/case")
            continue
        key = (phase, case)
        if key in seen:
            errors.append(f"duplicate fixture role: {phase}/{case}")
            continue
        seen.add(key)
        entries.append(entry)
    return entries, errors


def artifact_record(path: Path, root: Path) -> dict[str, Any]:
    return {
        "path": relative(path, root),
        "sha256": file_sha256(path) if path.is_file() else None,
        "exists": path.is_file(),
    }


def next_candidates(rows: Iterable[dict[str, Any]], per_family: int = 4) -> list[dict[str, Any]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        classification = row.get("classification", {})
        if classification.get("candidateEligible"):
            grouped[str(classification.get("businessFamily", "other"))].append(row)
    selected: list[dict[str, Any]] = []
    for family in sorted(grouped, key=lambda item: (FAMILY_PRIORITY.get(item, 99), item)):
        ordered = sorted(
            grouped[family],
            key=lambda item: (
                item.get("objectCount", 0),
                item["phase"],
                item["case"],
            ),
        )
        for row in ordered[:per_family]:
            selected.append(
                {
                    "phase": row["phase"],
                    "case": row["case"],
                    "businessFamily": family,
                    "inputSha256": row["input"]["sha256"],
                    "reason": "No static collector blocker; requires isolated FreeCADCmd staging proof.",
                }
            )
    return selected


def build_report(root: Path = ROOT, roles_path: Path = ROLES_PATH) -> dict[str, Any]:
    root = root.resolve()
    fixtures_root = root / "fixtures"
    roles_path = roles_path.resolve()
    collector_path = Path(__file__).with_name("collect_freecad_expected.py").resolve()
    entries, manifest_errors = role_entries(roles_path)
    role_by = {(str(item["phase"]), str(item["case"])): item for item in entries}
    catalog = load_catalog(root, roles_path=roles_path)
    anomalies = [*manifest_errors, *catalog.errors]
    inputs = input_paths(fixtures_root)
    rows: list[dict[str, Any]] = []
    unsupported_rows: list[dict[str, Any]] = []
    protocol_rows: list[dict[str, Any]] = []
    probes = probe_receipts(root)

    for input_path in inputs:
        phase = input_path.parent.name
        case = input_path.stem
        entry = role_by.get((phase, case))
        payload = json.loads(input_path.read_text(encoding="utf-8"))
        if not isinstance(payload, dict):
            anomalies.append(f"fixture input must be an object: {relative(input_path, root)}")
            payload = {}
        row: dict[str, Any] = {
            "phase": phase,
            "case": case,
            "role": entry.get("role") if entry else None,
            "objectCount": len(payload.get("Objects", [])) if isinstance(payload.get("Objects"), list) else 0,
            "input": artifact_record(input_path, root),
        }
        expected_dir = input_path.parent / "expected"
        if row["role"] == "native":
            public_path = expected_dir / f"{case}.freecad.json"
            ledger_path = expected_dir / f"{case}.freecad.ledger.json"
            trace_path = expected_dir / f"{case}.freecad.producer-trace.json"
            row["publicExpected"] = artifact_record(public_path, root)
            row["ledger"] = artifact_record(ledger_path, root)
            row["producerTrace"] = artifact_record(trace_path, root)
        elif row["role"] == "protocol_only":
            protocol_path = expected_dir / f"{case}.expeted.json"
            row["protocolExpected"] = artifact_record(protocol_path, root)
            row["classification"] = {
                "category": "freecad_native_not_expressible",
                "evidence": [str(entry.get("reason"))],
                "authority": str(entry.get("authority")),
                "nextAction": str(entry.get("nextAction")),
                "closeCondition": str(entry.get("closeCondition")),
            }
            protocol_rows.append(row)
        elif row["role"] == "unsupported":
            row["classification"] = apply_probe_receipt(
                unsupported_classification(payload),
                probes.get((phase, case)),
            )
            unsupported_rows.append(row)
        rows.append(row)

    role_counts = Counter(str(item.get("role")) for item in entries)
    classification_counts = Counter(
        str(row["classification"]["category"]) for row in unsupported_rows
    )
    artifact_counts = {
        "publicExpected": len(list(fixtures_root.glob("*/expected/*.freecad.json"))),
        "ledger": len(list(fixtures_root.glob("*/expected/*.freecad.ledger.json"))),
        "producerTrace": len(list(fixtures_root.glob("*/expected/*.freecad.producer-trace.json"))),
        "protocolExpected": len(list(fixtures_root.glob("*/expected/*.expeted.json"))),
    }
    if not inputs:
        anomalies.append("fixture inventory is empty")
    if len(entries) != len(inputs):
        anomalies.append(f"role/input count mismatch: roles={len(entries)} inputs={len(inputs)}")
    if artifact_counts["publicExpected"] != role_counts.get("native", 0):
        anomalies.append("native public expected count does not match native role count")
    if artifact_counts["ledger"] != role_counts.get("native", 0):
        anomalies.append("native ledger count does not match native role count")
    if artifact_counts["protocolExpected"] != role_counts.get("protocol_only", 0):
        anomalies.append("protocol expected count does not match protocol_only role count")

    report = {
        "schema": SCHEMA,
        "status": "passed" if not anomalies else "failed",
        "fixturesRoot": str(fixtures_root),
        "roles": {
            "path": relative(roles_path, root),
            "sha256": file_sha256(roles_path),
        },
        "collector": {
            "path": relative(collector_path, root),
            "sha256": file_sha256(collector_path),
        },
        "counts": {
            "inputs": len(inputs),
            "roles": dict(sorted(role_counts.items())),
            "artifacts": artifact_counts,
            "unsupportedClassifications": dict(sorted(classification_counts.items())),
        },
        "inventoryHash": canonical_sha256(rows),
        "anomalies": sorted(set(anomalies)),
        "cases": rows,
        "unsupported": [
            {
                key: row[key]
                for key in ("phase", "case", "objectCount", "input", "classification")
            }
            for row in unsupported_rows
        ],
        "protocolOnly": [
            {
                key: row[key]
                for key in ("phase", "case", "input", "protocolExpected", "classification")
            }
            for row in protocol_rows
        ],
        "probeReceipts": [
            {
                "phase": phase,
                "case": case,
                **receipt,
            }
            for (phase, case), receipt in sorted(probes.items())
        ],
        "nextNativeCandidates": next_candidates(unsupported_rows),
        "classificationPolicy": {
            "categories": [
                "collector_general_gap",
                "freecad_native_not_expressible",
                "non_native_fixture",
                "not_investigated",
            ],
            "note": (
                "Static classification never promotes a case. not_investigated candidates "
                "must pass isolated FreeCADCmd collection, strict ledger validation, and repeat 2."
            ),
        },
    }
    return report


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Audit fixture roles/artifacts and emit deterministic non-native triage."
    )
    parser.add_argument("--root", default=str(ROOT), help="cad-core root")
    parser.add_argument("--roles", default=str(ROLES_PATH), help="fixture role manifest")
    parser.add_argument("--report", help="machine-readable output path outside fixtures/")
    parser.add_argument("--pretty", action="store_true", help="print the complete report")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    report_path = Path(args.report).resolve() if args.report else None
    if report_path is not None and report_path.is_relative_to((root / "fixtures").resolve()):
        parser.error("--report must be outside the checked-in fixture tree")
    report = build_report(root, Path(args.roles))
    if report_path is not None:
        write_json(report_path, report)
    if args.pretty:
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    else:
        counts = report["counts"]
        print(
            "fixture authority inventory: "
            f"status={report['status']} inputs={counts['inputs']} "
            f"roles={counts['roles']} classifications={counts['unsupportedClassifications']}"
        )
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
