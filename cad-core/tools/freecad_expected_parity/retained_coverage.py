"""Map fixture contracts to the retained FreeCAD module closure and work queues."""

from __future__ import annotations

import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable


OWNER_MODULE_ORDER = (
    "FreeCADApp",
    "Material",
    "Part",
    "Sketcher",
    "PartDesign",
    "Mesh",
    "Spreadsheet",
    "Assembly",
    "OndselSolver",
)
RETAINED_OWNER_MODULES = frozenset(OWNER_MODULE_ORDER)
CORE_TARGET_ORDER = (
    "FreeCADBase",
    "FreeCADApp",
    "FreeCADMainCmd",
    "FreeCADMainPy",
)
NON_CAD_ENTRY_ORDER = ("Help", "AddonManager")
COVERAGE_SCHEMA = "freecad-retained-module-fixture-coverage/v1"
NON_CAD_SMOKE_SCHEMA = "freecad-non-cad-smoke/v1"


def fixture_owner_modules(payload: dict[str, Any]) -> list[str]:
    """Return every retained semantic owner exercised by one fixture graph."""

    type_ids = {
        str(item.get("TypeId"))
        for item in payload.get("Objects", [])
        if isinstance(item, dict) and isinstance(item.get("TypeId"), str)
    }
    owners: set[str] = set()
    for type_id in type_ids:
        prefix = type_id.split("::", 1)[0]
        if prefix == "App":
            owners.add("FreeCADApp")
        elif prefix in RETAINED_OWNER_MODULES:
            owners.add(prefix)
    property_types: set[str] = set()

    def collect_property_types(value: Any) -> None:
        if isinstance(value, dict):
            property_type = value.get("PropertyType")
            if isinstance(property_type, str):
                property_types.add(property_type)
            for item in value.values():
                collect_property_types(item)
        elif isinstance(value, list):
            for item in value:
                collect_property_types(item)

    collect_property_types(payload)
    if any(property_type.startswith("Materials::") for property_type in property_types):
        owners.add("Material")
    if "Assembly::JointGroup" in type_ids and "App::FeaturePython" in type_ids:
        owners.add("OndselSolver")
    return [module for module in OWNER_MODULE_ORDER if module in owners]


def annotate_fixture_row(row: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any]:
    owners = fixture_owner_modules(payload)
    return {
        **row,
        "ownerModules": owners,
        "inRetainedClosure": bool(owners),
    }


def _queue_sort_key(row: dict[str, Any]) -> tuple[Any, ...]:
    classification = row.get("classification", {})
    owners = row.get("ownerModules", [])
    first_owner = min(
        (OWNER_MODULE_ORDER.index(owner) for owner in owners if owner in RETAINED_OWNER_MODULES),
        default=len(OWNER_MODULE_ORDER),
    )
    return (
        first_owner,
        str(classification.get("businessFamily", "other")),
        int(row.get("objectCount", 0)),
        str(row.get("phase", "")),
        str(row.get("case", "")),
    )


def _queue_item(row: dict[str, Any]) -> dict[str, Any]:
    classification = row.get("classification", {})
    return {
        "phase": row["phase"],
        "case": row["case"],
        "ownerModules": list(row.get("ownerModules", [])),
        "inRetainedClosure": bool(row.get("inRetainedClosure")),
        "businessFamily": classification.get("businessFamily", "other"),
        "classificationCategory": classification.get("category"),
        "inputSha256": row["input"]["sha256"],
        "evidence": classification.get("evidence", []),
        "nextAction": classification.get("nextAction"),
        "closeCondition": classification.get("closeCondition"),
    }


def build_work_queues(rows: Iterable[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    """Partition unsupported fixtures into implementation, staging, promotion and evidence queues."""

    ordered = sorted(rows, key=_queue_sort_key)
    collector = [
        row
        for row in ordered
        if row.get("classification", {}).get("category") == "collector_general_gap"
    ]
    promotion = [
        row
        for row in ordered
        if row.get("classification", {}).get("probe", {}).get("ledgerOutcome") == "accepted"
        and row.get("classification", {}).get("probe", {}).get("repeat2Status") == "passed"
    ]
    promotion_keys = {(row["phase"], row["case"]) for row in promotion}
    staging = [
        row
        for row in ordered
        if row.get("classification", {}).get("candidateEligible")
        and (row["phase"], row["case"]) not in promotion_keys
    ]
    blocked = [
        row
        for row in ordered
        if row.get("classification", {}).get("category")
        in {"freecad_native_not_expressible", "non_native_fixture"}
    ]
    return {
        "collectorImplementationQueue": [_queue_item(row) for row in collector],
        "retainedModuleCollectorImplementationQueue": [
            _queue_item(row) for row in collector if row.get("inRetainedClosure")
        ],
        "stagingCandidateQueue": [_queue_item(row) for row in staging],
        "promotionQueue": [_queue_item(row) for row in promotion],
        "blockedOrReclassified": [_queue_item(row) for row in blocked],
    }


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _artifact(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.resolve()),
        "sha256": _file_sha256(path) if path.is_file() else None,
        "exists": path.is_file(),
    }


def _evidence_index(rows: Iterable[dict[str, Any]], contract_key: str) -> list[dict[str, Any]]:
    fixtures_by_value: dict[str, list[str]] = defaultdict(list)
    for row in rows:
        fixture = f"{row['phase']}/{row['case']}"
        for value in row.get("contract", {}).get(contract_key, []):
            fixtures_by_value[str(value)].append(fixture)
    return [
        {"name": value, "fixtures": sorted(set(fixtures))}
        for value, fixtures in sorted(fixtures_by_value.items())
    ]


def _module_row(
    name: str,
    rows: list[dict[str, Any]],
    *,
    evidence: list[str],
    association: str,
) -> dict[str, Any]:
    classifications = Counter(
        str(row.get("classification", {}).get("category"))
        for row in rows
        if row.get("role") == "unsupported"
    )
    native_eligible = sum(
        1
        for row in rows
        if row.get("role") == "unsupported"
        and row.get("classification", {}).get("category")
        in {"collector_general_gap", "not_investigated"}
    )
    fixture_count = len(rows)
    collector_gap_count = classifications.get("collector_general_gap", 0)
    not_investigated_count = classifications.get("not_investigated", 0)
    coverage_status = (
        "passed"
        if fixture_count > 0
        and collector_gap_count == 0
        and not_investigated_count == 0
        and native_eligible == 0
        else "failed"
    )
    return {
        "name": name,
        "coverageKind": "cad_native_authority",
        "fixtureAssociation": association,
        "typeIds": _evidence_index(rows, "typeIds"),
        "properties": _evidence_index(rows, "properties"),
        "propertyTypes": _evidence_index(rows, "propertyTypes"),
        "capabilityFamilies": _evidence_index(rows, "capabilityFamilies"),
        "fixtures": sorted(f"{row['phase']}/{row['case']}" for row in rows),
        "fixtureCount": fixture_count,
        "nativeAuthorityCount": sum(row.get("role") == "native" for row in rows),
        "protocolOnlyCount": sum(row.get("role") == "protocol_only" for row in rows),
        "evidenceExcludedCount": (
            classifications.get("freecad_native_not_expressible", 0)
            + classifications.get("non_native_fixture", 0)
        ),
        "collectorGeneralGapCount": collector_gap_count,
        "notInvestigatedCount": not_investigated_count,
        "nativeEligibleWithoutAuthorityCount": native_eligible,
        "coverageStatus": coverage_status,
        "evidence": evidence,
    }


def _non_cad_module_row(
    name: str,
    receipt_path: Path | None,
    *,
    evidence: list[str],
) -> dict[str, Any]:
    receipt: dict[str, Any] | None = None
    if receipt_path is not None and receipt_path.is_file():
        try:
            candidate = json.loads(receipt_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            candidate = None
        if isinstance(candidate, dict):
            receipt = candidate
    checks = receipt.get("checks") if isinstance(receipt, dict) else None
    producer = receipt.get("producer") if isinstance(receipt, dict) else None
    smoke_passed = bool(
        receipt
        and receipt.get("schema") == NON_CAD_SMOKE_SCHEMA
        and receipt.get("entry") == name
        and receipt.get("status") == "passed"
        and isinstance(producer, dict)
        and isinstance(producer.get("sha256"), str)
        and bool(producer.get("sha256"))
        and isinstance(checks, list)
        and bool(checks)
        and all(isinstance(check, dict) and check.get("status") == "passed" for check in checks)
    )
    return {
        "name": name,
        "coverageKind": "non_cad_smoke",
        "fixtureAssociation": "none",
        "typeIds": [],
        "properties": [],
        "propertyTypes": [],
        "capabilityFamilies": [],
        "fixtures": [],
        "fixtureCount": 0,
        "nativeAuthorityCount": 0,
        "protocolOnlyCount": 0,
        "evidenceExcludedCount": 0,
        "collectorGeneralGapCount": 0,
        "notInvestigatedCount": 0,
        "nativeEligibleWithoutAuthorityCount": 0,
        "coverageStatus": "non_cad_smoke" if smoke_passed else "failed",
        "evidence": evidence,
        "smokeReceipt": _artifact(receipt_path) if receipt_path is not None else None,
    }


def _producer_reproduction_validation(
    authority_report: dict[str, Any],
    producer_report_path: Path | None,
) -> dict[str, Any]:
    report_artifact = (
        _artifact(producer_report_path) if producer_report_path is not None else None
    )
    errors: list[str] = []
    report: dict[str, Any] | None = None
    if producer_report_path is None or not producer_report_path.is_file():
        errors.append("producer report is missing")
    else:
        try:
            candidate = json.loads(producer_report_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            candidate = None
        if isinstance(candidate, dict):
            report = candidate
        else:
            errors.append("producer report is not a JSON object")

    native_rows = [
        row
        for row in authority_report.get("cases", [])
        if isinstance(row, dict) and row.get("role") == "native"
    ]
    native_by_key = {
        (str(row.get("phase")), str(row.get("case"))): {
            "fixtureSha256": row.get("input", {}).get("sha256"),
            "publicSha256": row.get("publicExpected", {}).get("sha256"),
            "ledgerSha256": row.get("ledger", {}).get("sha256"),
        }
        for row in native_rows
    }
    native_count = len(native_by_key)
    if report is not None:
        required_top_level = {
            "schema": "freecad-fixture-regression-report/v1",
            "status": "passed",
            "mode": "repeated-native-check",
            "publicExpectedStatus": "passed",
            "ledgerValidationStatus": "passed",
            "producerTraceStatus": "not_evaluated",
        }
        for key, expected in required_top_level.items():
            if report.get(key) != expected:
                errors.append(
                    f"producer report {key} must be {expected!r}, got {report.get(key)!r}"
                )
        producer = report.get("candidate")
        collector = report.get("collector")
        if (
            not isinstance(producer, dict)
            or not producer.get("path")
            or not producer.get("sha256")
        ):
            errors.append("producer report is missing FreeCADCmd path/SHA256")
        if not isinstance(collector, dict) or not collector.get("sha256"):
            errors.append("producer report is missing collector SHA256")
        for key in (
            "candidateRunDifferences",
            "candidateRunLedgerDrifts",
            "candidateRunVariations",
        ):
            if report.get(key) != []:
                errors.append(f"producer report {key} must be empty")

        manifest = report.get("manifest")
        entries = manifest.get("entries") if isinstance(manifest, dict) else None
        entry_by_key: dict[tuple[str, str], dict[str, Any]] = {}
        if not isinstance(entries, list):
            errors.append("producer report manifest entries are missing")
        else:
            for entry in entries:
                if not isinstance(entry, dict):
                    errors.append("producer report manifest contains a non-object entry")
                    continue
                key = (str(entry.get("phase")), str(entry.get("case")))
                if key in entry_by_key:
                    errors.append(
                        f"producer report manifest duplicates {key[0]}/{key[1]}"
                    )
                entry_by_key[key] = entry
        if not isinstance(manifest, dict) or manifest.get("cases") != native_count:
            errors.append(
                f"producer report manifest case count must equal native inventory {native_count}"
            )
        if isinstance(manifest, dict) and manifest.get("phases") != len(
            {phase for phase, _case in native_by_key}
        ):
            errors.append("producer report manifest phase count is stale")
        if set(entry_by_key) != set(native_by_key):
            errors.append("producer report manifest case set does not match native inventory")
        else:
            for key, expected_hashes in native_by_key.items():
                entry = entry_by_key[key]
                if any(entry.get(name) != value for name, value in expected_hashes.items()):
                    errors.append(
                        f"producer report manifest hashes are stale for {key[0]}/{key[1]}"
                    )
                    break

        runs = report.get("runs")
        if not isinstance(runs, list) or len(runs) != 2:
            errors.append("producer report must contain exactly two independent runs")
        else:
            labels = {run.get("label") for run in runs if isinstance(run, dict)}
            if labels != {"run-a", "run-b"}:
                errors.append("producer report run labels must be run-a/run-b")
            for run in runs:
                run_report = run.get("report") if isinstance(run, dict) else None
                if (
                    not isinstance(run, dict)
                    or run.get("returncode") != 0
                    or not isinstance(run_report, dict)
                    or run_report.get("status") != "passed"
                    or run_report.get("discovered") != native_count
                    or run_report.get("processed") != native_count
                    or run_report.get("failed") != 0
                    or run_report.get("skipped") != 0
                    or run_report.get("publicExpectedStatus") != "passed"
                    or run_report.get("ledgerValidationStatus") != "passed"
                    or run_report.get("producerTraceStatus") != "not_evaluated"
                ):
                    label = run.get("label") if isinstance(run, dict) else None
                    errors.append(
                        f"producer report run {label!r} did not execute {native_count}/{native_count} cleanly"
                    )

    return {
        "status": "passed" if not errors else "failed",
        "report": report_artifact,
        "nativeCaseCount": native_count,
        "errors": errors,
    }


def build_module_coverage_report(
    authority_report: dict[str, Any],
    *,
    closure_contract_path: Path,
    pruning_plan_path: Path,
    producer_report_path: Path | None = None,
    non_cad_smoke_receipts: dict[str, Path] | None = None,
    native_baseline: int = 480,
) -> dict[str, Any]:
    """Build one deterministic retained-module coverage verdict from live contracts."""

    closure = json.loads(closure_contract_path.read_text(encoding="utf-8"))
    plan_text = pruning_plan_path.read_text(encoding="utf-8")
    module_contracts = [
        item
        for item in closure.get("modules", [])
        if isinstance(item, dict) and isinstance(item.get("name"), str)
    ]
    module_by_name = {str(item["name"]): item for item in module_contracts}
    app_modules = [str(item["name"]) for item in module_contracts]
    target_dependencies = [
        str(item) for item in closure.get("targetDependencies", []) if isinstance(item, str)
    ]
    release_targets = {
        str(item.get("target"))
        for item in closure.get("releaseArtifacts", [])
        if isinstance(item, dict) and isinstance(item.get("target"), str)
    }
    core_targets = [target for target in CORE_TARGET_ORDER if target in release_targets]
    non_cad_entries = [entry for entry in NON_CAD_ENTRY_ORDER if entry in plan_text]
    rows = [row for row in authority_report.get("cases", []) if isinstance(row, dict)]
    retained_rows = [row for row in rows if row.get("inRetainedClosure")]

    modules: list[dict[str, Any]] = []
    for target in core_targets:
        modules.append(
            _module_row(
                target,
                retained_rows,
                evidence=[str(closure_contract_path.resolve())],
                association="runtime_execution_path",
            )
        )
    for module in app_modules:
        module_rows = [row for row in rows if module in row.get("ownerModules", [])]
        contract = module_by_name[module]
        modules.append(
            _module_row(
                module,
                module_rows,
                evidence=[str(item) for item in contract.get("evidence", [])],
                association="semantic_owner",
            )
        )
    for dependency in target_dependencies:
        dependency_rows = [row for row in rows if dependency in row.get("ownerModules", [])]
        modules.append(
            _module_row(
                dependency,
                dependency_rows,
                evidence=[str(closure_contract_path.resolve())],
                association="retained_target_dependency",
            )
        )
    smoke_receipts = non_cad_smoke_receipts or {}
    for entry in non_cad_entries:
        modules.append(
            _non_cad_module_row(
                entry,
                smoke_receipts.get(entry),
                evidence=[str(pruning_plan_path.resolve())],
            )
        )

    producer_validation = _producer_reproduction_validation(
        authority_report,
        producer_report_path,
    )
    anomalies: list[str] = []
    missing_core = [target for target in CORE_TARGET_ORDER if target not in core_targets]
    if missing_core:
        anomalies.append("closure contract is missing core targets: " + ", ".join(missing_core))
    if not app_modules:
        anomalies.append("closure contract has no retained App modules")
    if not rows:
        anomalies.append("fixture authority inventory is empty")
    native_after = int(authority_report.get("counts", {}).get("roles", {}).get("native", 0))
    coverage_status = (
        "passed"
        if not anomalies
        and producer_validation["status"] == "passed"
        and modules
        and all(
            item["coverageStatus"] in {"passed", "non_cad_smoke"} for item in modules
        )
        and native_after > native_baseline
        else "failed"
    )
    coverage_hash_payload = {
        "closure": {
            "coreTargets": core_targets,
            "appModules": app_modules,
            "targetDependencies": target_dependencies,
            "nonCadEntries": non_cad_entries,
        },
        "modules": modules,
        "nativeAfter": native_after,
        "producerValidation": producer_validation,
    }
    coverage_hash = "sha256:" + hashlib.sha256(
        json.dumps(
            coverage_hash_payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
    ).hexdigest()
    return {
        "schema": COVERAGE_SCHEMA,
        "status": "passed" if not anomalies else "failed",
        "coverageStatus": coverage_status,
        "retainedClosureSource": {
            "pruningPlan": _artifact(pruning_plan_path),
            "closureContract": _artifact(closure_contract_path),
            "contractSchema": closure.get("schema"),
            "milestone": closure.get("milestone"),
            "coreTargets": core_targets,
            "appModules": app_modules,
            "targetDependencies": target_dependencies,
            "pythonDataEntries": non_cad_entries,
            "producerReport": _artifact(producer_report_path)
            if producer_report_path is not None
            else None,
        },
        "nativeBaseline": native_baseline,
        "nativeAfter": native_after,
        "nativeExpanded": native_after > native_baseline,
        "producerValidation": producer_validation,
        "globalFixtureCount": len(
            {(str(row.get("phase")), str(row.get("case"))) for row in retained_rows}
        ),
        "retainedModuleCollectorImplementationQueueCount": len(
            authority_report.get("retainedModuleCollectorImplementationQueue", [])
        ),
        "notInvestigatedCount": sum(
            row.get("classification", {}).get("category") == "not_investigated"
            for row in retained_rows
        ),
        "nativeEligibleWithoutAuthorityCount": sum(
            row.get("role") == "unsupported"
            and row.get("classification", {}).get("category")
            in {"collector_general_gap", "not_investigated"}
            for row in retained_rows
        ),
        "modules": modules,
        "coverageHash": coverage_hash,
        "anomalies": anomalies,
    }
