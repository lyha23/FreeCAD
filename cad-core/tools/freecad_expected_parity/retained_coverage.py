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
MATERIAL_PROCESS_CONTRACT_SCHEMA = "freecad-material-resolution-process-contract/v1"
NATIVE_PROCESS_CONTRACT_SCHEMA = "freecad-native-process-contract/v1"
NATIVE_PROCESS_CONTRACT_SCHEMAS = frozenset(
    {MATERIAL_PROCESS_CONTRACT_SCHEMA, NATIVE_PROCESS_CONTRACT_SCHEMA}
)
ASSEMBLY_SUPPORT_MATRIX_RECEIPT_ID = "assembly-solver-support-matrix/v1"
ASSEMBLY_SUPPORT_MATRIX_REPORT_SCHEMA = (
    "freecad-assembly-solver-support-matrix-report/v1"
)
PUBLIC_CAPABILITY_CONTRACT_SCHEMA = "freecad-retained-public-capabilities/v1"
PUBLIC_CAPABILITY_REPORT_SCHEMA = "freecad-retained-public-capability-coverage/v1"
PUBLIC_API_SURFACE_SCHEMA = "freecad-retained-public-api-surface/v1"
PUBLIC_API_REPORT_SCHEMA = "freecad-retained-public-api-coverage/v1"
EXECUTION_EVIDENCE_LEVELS = frozenset(
    {"target_result", "dependency_result", "native_diagnostic"}
)
THIN_EVIDENCE_LEVELS = frozenset({"entrypoint_smoke_only", "typeid_property_only"})
NON_NATIVE_EVIDENCE_LEVELS = frozenset(
    {"protocol_contract", "unsupported_contract", "native_process_test"}
)
CAPABILITY_EVIDENCE_LEVELS = (
    EXECUTION_EVIDENCE_LEVELS
    | THIN_EVIDENCE_LEVELS
    | NON_NATIVE_EVIDENCE_LEVELS
)
NON_NATIVE_CAPABILITY_DISPOSITIONS = frozenset(
    {"protocol_only", "unsupported", "non_cad_smoke", "native_process_test"}
)
PUBLIC_API_EXPOSURES = frozenset(
    {
        "CxxPublicApi",
        "DocumentObjectTypeId",
        "ProcessEntry",
        "PythonModule",
        "PythonWrapper",
        "RetainedTargetDependency",
    }
)
PUBLIC_API_NATIVE_EXPRESSIBILITY = frozenset(
    {"native_fixture", "native_process_test", "protocol_only", "unsupported"}
)
PUBLIC_API_DISPOSITIONS = frozenset(
    {
        "native_fixture",
        "native_process_test",
        "protocol_only",
        "unsupported",
        "uncovered",
        "non_cad_smoke",
    }
)


def _native_process_contract_receipt(
    contract_path: Path,
    contract_id: str,
    *,
    process_contract_root: Path | None,
    required_case_ids: Iterable[str] | None = None,
) -> tuple[dict[str, Any] | None, Path, list[str]]:
    """Load and fail-close one checked-in hermetic native process receipt."""

    relative = contract_id.removeprefix("process-contract/") + ".v1.json"
    receipt_path = (
        process_contract_root / relative
        if process_contract_root is not None
        else contract_path.parent / "reports" / "process_contract" / relative
    )
    errors: list[str] = []
    receipt: dict[str, Any] | None = None
    if not receipt_path.is_file():
        return None, receipt_path, [f"native process contract receipt is missing: {contract_id}"]
    try:
        candidate = json.loads(receipt_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return None, receipt_path, [
            f"native process contract receipt is invalid JSON: {contract_id}: {exc}"
        ]
    if not isinstance(candidate, dict):
        return None, receipt_path, [
            f"native process contract receipt must be an object: {contract_id}"
        ]
    receipt = candidate
    schema = receipt.get("schema")
    if schema not in NATIVE_PROCESS_CONTRACT_SCHEMAS:
        errors.append(f"native process contract schema is invalid: {contract_id}")
    if receipt.get("contractId") != contract_id:
        errors.append(f"native process contract id is invalid: {contract_id}")
    if receipt.get("status") != "passed" or receipt.get("repeatStatus") != "passed":
        errors.append(f"native process contract did not pass repeat validation: {contract_id}")
    repeat = receipt.get("repeat")
    if not isinstance(repeat, int) or repeat < 2:
        errors.append(f"native process contract repeat must be at least 2: {contract_id}")
        repeat = 0
    producer = receipt.get("producer")
    tool = receipt.get("tool")
    if not isinstance(producer, dict) or not producer.get("path") or not producer.get("sha256"):
        errors.append(f"native process contract producer identity is incomplete: {contract_id}")
    if not isinstance(tool, dict) or not tool.get("sha256"):
        errors.append(f"native process contract tool identity is incomplete: {contract_id}")
    if schema == NATIVE_PROCESS_CONTRACT_SCHEMA:
        runner = receipt.get("runner")
        resources = receipt.get("resources")
        if not isinstance(runner, dict) or not runner.get("path") or not runner.get("sha256"):
            errors.append(f"native process contract runner identity is incomplete: {contract_id}")
        if (
            not isinstance(resources, list)
            or not resources
            or any(
                not isinstance(resource, dict)
                or not resource.get("path")
                or not resource.get("sha256")
                for resource in resources
            )
        ):
            errors.append(f"native process contract resources are incomplete: {contract_id}")
    cases = receipt.get("cases")
    if not isinstance(cases, list) or not cases:
        errors.append(f"native process contract cases are missing: {contract_id}")
        cases = []
    seen_cases: set[str] = set()
    for case in cases:
        case_id = case.get("id") if isinstance(case, dict) else None
        runs = case.get("runs") if isinstance(case, dict) else None
        if not isinstance(case_id, str) or not case_id or case_id in seen_cases:
            errors.append(f"native process contract has invalid case ids: {contract_id}")
            continue
        seen_cases.add(case_id)
        if not isinstance(runs, list) or len(runs) != repeat:
            errors.append(
                f"native process contract case repeat count is invalid: {contract_id}/{case_id}"
            )
            continue
        for run in runs:
            result = run.get("result") if isinstance(run, dict) else None
            process = run.get("process") if isinstance(run, dict) else None
            if (
                not isinstance(run, dict)
                or run.get("status") != "passed"
            ):
                errors.append(
                    f"native process contract case run failed: {contract_id}/{case_id}"
                )
                break
            if result is not None and (
                not isinstance(result, dict) or result.get("status") != "passed"
            ):
                errors.append(
                    f"native process contract case result failed: {contract_id}/{case_id}"
                )
                break
            if (
                not isinstance(process, dict)
                or not isinstance(process.get("argv"), list)
                or not process.get("argv")
                or not isinstance(process.get("environment"), dict)
                or not isinstance(process.get("stdout"), str)
                or not isinstance(process.get("stderr"), str)
                or not all(
                    field in process for field in ("exitCode", "signal", "timedOut")
                )
            ):
                errors.append(
                    f"native process contract process receipt is incomplete: {contract_id}/{case_id}"
                )
                break
            if schema == NATIVE_PROCESS_CONTRACT_SCHEMA and (
                run.get("caseId") != case_id
                or not isinstance(run.get("label"), str)
                or not run.get("label")
                or run.get("coverageOutcome") not in {
                    "native_process_test",
                    "source_backed_exception",
                }
                or not isinstance(process.get("cwd"), str)
                or not isinstance(process.get("timeoutSeconds"), int)
                or not isinstance(process.get("executable"), dict)
                or not process["executable"].get("path")
                or not process["executable"].get("sha256")
            ):
                errors.append(
                    f"native process contract common-schema run is incomplete: {contract_id}/{case_id}"
                )
                break
    required_cases = set(required_case_ids or ())
    if schema == NATIVE_PROCESS_CONTRACT_SCHEMA and not required_cases:
        errors.append(
            f"native process contract evidence requires item-local processCases: {contract_id}"
        )
    missing_cases = sorted(required_cases - seen_cases)
    if missing_cases:
        errors.append(
            f"native process contract required cases are missing: {contract_id}: {', '.join(missing_cases)}"
        )
    if required_cases and schema == NATIVE_PROCESS_CONTRACT_SCHEMA:
        case_by_id = {
            case.get("id"): case for case in cases if isinstance(case, dict)
        }
        invalid_required = sorted(
            case_id
            for case_id in required_cases
            if case_id in case_by_id
            and (
                case_by_id[case_id].get("coverageOutcome") != "native_process_test"
                or case_by_id[case_id].get("status") != "passed"
            )
        )
        if invalid_required:
            errors.append(
                "native process contract required cases are not covered: "
                f"{contract_id}: {', '.join(invalid_required)}"
            )
    return receipt, receipt_path, errors


def _assembly_support_matrix_receipt(
    contract_path: Path,
    receipt_id: str,
) -> tuple[dict[str, Any] | None, Path, Path, list[str]]:
    """Load the A4 matrix receipt and reject stale or incomplete equivalence coverage."""

    receipt_path = contract_path.parent / "reports" / "assembly_solver_support_matrix.v1.json"
    matrix_path = contract_path.parent / "assembly_solver_support_matrix.v1.json"
    errors: list[str] = []
    if receipt_id != ASSEMBLY_SUPPORT_MATRIX_RECEIPT_ID:
        return None, receipt_path, matrix_path, [
            f"unknown Assembly support matrix receipt: {receipt_id}"
        ]
    if not receipt_path.is_file():
        return None, receipt_path, matrix_path, [
            f"Assembly support matrix receipt is missing: {receipt_id}"
        ]
    if not matrix_path.is_file():
        return None, receipt_path, matrix_path, [
            f"Assembly support matrix contract is missing: {receipt_id}"
        ]
    try:
        candidate = json.loads(receipt_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return None, receipt_path, matrix_path, [
            f"Assembly support matrix receipt is invalid JSON: {receipt_id}: {exc}"
        ]
    if not isinstance(candidate, dict):
        return None, receipt_path, matrix_path, [
            f"Assembly support matrix receipt must be an object: {receipt_id}"
        ]
    receipt = candidate
    if receipt.get("schema") != ASSEMBLY_SUPPORT_MATRIX_REPORT_SCHEMA:
        errors.append(f"Assembly support matrix receipt schema is invalid: {receipt_id}")
    if receipt.get("status") != "passed":
        errors.append(f"Assembly support matrix receipt did not pass: {receipt_id}")
    summary = receipt.get("summary")
    expected_summary = {
        "jointTypeCount": 13,
        "markerGeometryFamilyCount": 10,
        "distanceTypeCount": 37,
        "rowCount": 50,
        "evidenceClassCount": 51,
        "missingEquivalenceClassCount": 0,
        "invalidEvidenceClassCount": 0,
    }
    if not isinstance(summary, dict):
        errors.append(f"Assembly support matrix summary is missing: {receipt_id}")
        summary = {}
    for field, expected in expected_summary.items():
        if summary.get(field) != expected:
            errors.append(
                f"Assembly support matrix summary {field} is invalid: {receipt_id}"
            )
    if summary.get("evidenceStatusCounts") != {"passed": 51}:
        errors.append(f"Assembly support matrix evidence counts are invalid: {receipt_id}")
    evidence_receipts = receipt.get("evidenceReceipts")
    if (
        not isinstance(evidence_receipts, list)
        or len(evidence_receipts) != 51
        or any(
            not isinstance(item, dict) or item.get("status") != "passed"
            for item in evidence_receipts
        )
    ):
        errors.append(f"Assembly support matrix evidence receipts are incomplete: {receipt_id}")
    if receipt.get("missingEquivalenceClasses") != []:
        errors.append(f"Assembly support matrix has missing evidence: {receipt_id}")
    if receipt.get("invalidEvidenceClasses") != [] or receipt.get("errors") != []:
        errors.append(f"Assembly support matrix has invalid evidence: {receipt_id}")
    if receipt.get("cadCoreRuntimeParity") != "not_evaluated":
        errors.append(f"Assembly support matrix overclaims CAD Core parity: {receipt_id}")
    matrix = receipt.get("matrix")
    matrix_sha = hashlib.sha256(matrix_path.read_bytes()).hexdigest()
    if not isinstance(matrix, dict) or matrix.get("sha256") != matrix_sha:
        errors.append(f"Assembly support matrix receipt is stale: {receipt_id}")
    return receipt, receipt_path, matrix_path, errors


def build_public_api_coverage_report(
    authority_report: dict[str, Any],
    *,
    api_surface_path: Path,
    capability_contract_path: Path,
    fixture_corpus_closure_status: str,
    process_contract_root: Path | None = None,
) -> dict[str, Any]:
    """Validate the source-side API denominator and map item-local execution evidence.

    Unlike fixture capability discovery, this denominator is an explicit source/target-closure
    contract. A fixture TypeId or property can satisfy an already-declared item, but it can
    never create the denominator. Every API item must also map back to at least one maintained
    capability row so the older capability report remains traceable during migration.
    """

    surface = json.loads(api_surface_path.read_text(encoding="utf-8"))
    capabilities = json.loads(capability_contract_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    if surface.get("schema") != PUBLIC_API_SURFACE_SCHEMA:
        errors.append("API surface schema must be " + PUBLIC_API_SURFACE_SCHEMA)
    if capabilities.get("schema") != PUBLIC_CAPABILITY_CONTRACT_SCHEMA:
        errors.append(
            "capability contract schema must be " + PUBLIC_CAPABILITY_CONTRACT_SCHEMA
        )
    required_modules = surface.get("requiredModules")
    capability_modules = capabilities.get("requiredModules")
    if (
        not isinstance(required_modules, list)
        or not required_modules
        or not all(isinstance(item, str) and item for item in required_modules)
        or len(required_modules) != len(set(required_modules))
    ):
        errors.append("API surface requiredModules must be a non-empty unique string list")
        required_modules = []
    if required_modules != capability_modules:
        errors.append("API surface requiredModules must match the retained capability contract")

    closure = surface.get("retainedClosureSource")
    if not isinstance(closure, dict):
        errors.append("API surface retainedClosureSource must be an object")
        closure = {}
    for field in ("targetClosure", "pruningPlan"):
        value = closure.get(field)
        if not isinstance(value, str) or not value:
            errors.append(f"API surface retainedClosureSource.{field} is required")

    known_capabilities = {
        (str(module.get("name")), str(capability.get("id")))
        for module in capabilities.get("modules", [])
        if isinstance(module, dict)
        for capability in module.get("capabilities", [])
        if isinstance(capability, dict)
    }
    mapped_capabilities: set[tuple[str, str]] = set()
    role_by_fixture = {
        f"{row.get('phase')}/{row.get('case')}": str(row.get("role"))
        for row in authority_report.get("cases", [])
        if isinstance(row, dict)
    }
    seen_ids: set[str] = set()
    module_rows: dict[str, list[dict[str, Any]]] = defaultdict(list)
    coverage_counts: Counter[str] = Counter()

    api_items = surface.get("apis")
    if not isinstance(api_items, list) or not api_items:
        errors.append("API surface apis must be a non-empty list")
        api_items = []
    for index, item in enumerate(api_items):
        if not isinstance(item, dict):
            errors.append(f"apis[{index}] must be an object")
            continue
        api_id = item.get("id")
        module = item.get("module")
        symbol = item.get("symbol")
        if not isinstance(api_id, str) or not api_id:
            errors.append(f"apis[{index}].id must be a non-empty string")
            continue
        if api_id in seen_ids:
            errors.append(f"duplicate API surface id {api_id}")
        seen_ids.add(api_id)
        if not isinstance(module, str) or module not in required_modules:
            errors.append(f"{api_id}.module is not in requiredModules")
            continue
        if not isinstance(symbol, str) or not symbol:
            errors.append(f"{api_id}.symbol must be a non-empty string")
            symbol = ""
        # One public symbol may intentionally have several denominator rows when its major
        # runtime branches have different expressibility/disposition (for example MainCmd
        # startup versus exit-code failures). API id, not symbol text, is the unique key.
        for field in ("publicSurface",):
            if not isinstance(item.get(field), str) or not item.get(field):
                errors.append(f"{api_id}.{field} must be a non-empty string")
        exposure = item.get("exposure")
        if exposure not in PUBLIC_API_EXPOSURES:
            errors.append(f"{api_id}.exposure is invalid: {exposure!r}")
        native_expressibility = item.get("nativeExpressibility")
        if native_expressibility not in PUBLIC_API_NATIVE_EXPRESSIBILITY:
            errors.append(
                f"{api_id}.nativeExpressibility is invalid: {native_expressibility!r}"
            )
        disposition = item.get("disposition")
        if disposition not in PUBLIC_API_DISPOSITIONS:
            errors.append(f"{api_id}.disposition is invalid: {disposition!r}")
        source_evidence = item.get("sourceEvidence")
        if not isinstance(source_evidence, list) or not source_evidence or not all(
            isinstance(source, str) and source for source in source_evidence
        ):
            errors.append(f"{api_id}.sourceEvidence must be a non-empty string list")
            source_evidence = []
        runtime_branches = item.get("runtimeBranches")
        if not isinstance(runtime_branches, list) or not runtime_branches or not all(
            isinstance(branch, str) and branch for branch in runtime_branches
        ):
            errors.append(f"{api_id}.runtimeBranches must be a non-empty string list")
            runtime_branches = []
        capability_ids = item.get("capabilityIds")
        if not isinstance(capability_ids, list) or not capability_ids or not all(
            isinstance(capability_id, str) and capability_id
            for capability_id in capability_ids
        ):
            errors.append(f"{api_id}.capabilityIds must be a non-empty string list")
            capability_ids = []
        for capability_id in capability_ids:
            key = (module, capability_id)
            if key not in known_capabilities:
                errors.append(f"{api_id} maps unknown capability {module}/{capability_id}")
            mapped_capabilities.add(key)

        fixture_evidence = item.get("fixtureEvidence")
        if not isinstance(fixture_evidence, list):
            errors.append(f"{api_id}.fixtureEvidence must be a list")
            fixture_evidence = []
        evidence_rows: list[dict[str, Any]] = []
        for evidence_index, evidence in enumerate(fixture_evidence):
            if not isinstance(evidence, dict):
                errors.append(f"{api_id}.fixtureEvidence[{evidence_index}] must be an object")
                continue
            fixture = evidence.get("fixture")
            level = evidence.get("level")
            branch = evidence.get("branch")
            if not isinstance(fixture, str) or "/" not in fixture:
                errors.append(f"{api_id}.fixtureEvidence[{evidence_index}].fixture is invalid")
                continue
            if level not in CAPABILITY_EVIDENCE_LEVELS:
                errors.append(f"{api_id}.fixtureEvidence[{evidence_index}].level is invalid")
            if not isinstance(branch, str) or branch not in runtime_branches:
                errors.append(
                    f"{api_id}.fixtureEvidence[{evidence_index}].branch must name an item runtime branch"
                )
            evidence_role = role_by_fixture.get(fixture, "missing")
            receipt_artifact: dict[str, Any] | None = None
            process_cases: list[str] | None = None
            if level == "native_process_test":
                raw_process_cases = evidence.get("processCases")
                process_cases = (
                    list(raw_process_cases)
                    if isinstance(raw_process_cases, list)
                    and all(isinstance(case_id, str) and case_id for case_id in raw_process_cases)
                    else []
                )
                _receipt, receipt_path, receipt_errors = _native_process_contract_receipt(
                    api_surface_path,
                    fixture,
                    process_contract_root=process_contract_root,
                    required_case_ids=process_cases,
                )
                evidence_role = "native_process_test" if not receipt_errors else "missing"
                receipt_artifact = _artifact(receipt_path)
                errors.extend(f"{api_id}: {error}" for error in receipt_errors)
            evidence_rows.append(
                {
                    "fixture": fixture,
                    "level": level,
                    "branch": branch,
                    "role": evidence_role,
                    **(
                        {"processContractReceipt": receipt_artifact}
                        if receipt_artifact is not None
                        else {}
                    ),
                    **(
                        {"processCases": process_cases}
                        if process_cases is not None
                        else {}
                    ),
                }
            )

        support_matrix_row: dict[str, Any] | None = None
        support_matrix_valid = True
        support_matrix_id = item.get("supportMatrixReceipt")
        requires_support_matrix = (
            api_id == "ondselsolver.full_joint_type_and_degenerate_matrix"
        )
        if requires_support_matrix and support_matrix_id is None:
            errors.append(f"{api_id}.supportMatrixReceipt is required")
            support_matrix_valid = False
        if support_matrix_id is not None:
            if not isinstance(support_matrix_id, str) or not support_matrix_id:
                errors.append(f"{api_id}.supportMatrixReceipt is invalid")
                support_matrix_valid = False
            else:
                _, receipt_path, matrix_path, receipt_errors = (
                    _assembly_support_matrix_receipt(api_surface_path, support_matrix_id)
                )
                support_matrix_valid = not receipt_errors
                errors.extend(f"{api_id}: {error}" for error in receipt_errors)
                support_matrix_row = {
                    "id": support_matrix_id,
                    "status": "passed" if support_matrix_valid else "failed",
                    "report": _artifact(receipt_path),
                    "matrix": _artifact(matrix_path),
                }

        coverage_status = "uncovered"
        if disposition == "native_fixture":
            if (
                not evidence_rows
                or any(row["role"] != "native" for row in evidence_rows)
                or not support_matrix_valid
            ):
                coverage_status = "missing_authority"
            else:
                executed_branches = {
                    str(row["branch"])
                    for row in evidence_rows
                    if row["level"] in EXECUTION_EVIDENCE_LEVELS
                }
                if not executed_branches:
                    coverage_status = "thin"
                elif set(runtime_branches) <= executed_branches:
                    coverage_status = "covered"
                else:
                    coverage_status = "branch_gap"
        elif disposition == "native_process_test":
            executed_branches = {
                str(row["branch"])
                for row in evidence_rows
                if row["level"] == "native_process_test"
                and row["role"] == "native_process_test"
            }
            coverage_status = (
                "thin"
                if not executed_branches
                else "covered"
                if set(runtime_branches) <= executed_branches
                else "branch_gap"
            )
            if not isinstance(item.get("rationale"), str) or not item.get("rationale"):
                errors.append(f"{api_id} native process test requires boundary rationale")
        elif disposition == "uncovered":
            if not isinstance(item.get("rationale"), str) or not item.get("rationale"):
                errors.append(f"{api_id} uncovered item requires rationale")
        else:
            coverage_status = "non_native_exception"
            if not isinstance(item.get("rationale"), str) or not item.get("rationale"):
                errors.append(f"{api_id} non-native item requires rationale")
        coverage_counts[coverage_status] += 1
        module_rows[module].append(
            {
                **item,
                "sourceEvidence": source_evidence,
                "runtimeBranches": runtime_branches,
                "fixtureEvidence": evidence_rows,
                "coverageStatus": coverage_status,
                **(
                    {"supportMatrixReceipt": support_matrix_row}
                    if support_matrix_row is not None
                    else {}
                ),
            }
        )

    missing_capabilities = sorted(known_capabilities - mapped_capabilities)
    if missing_capabilities:
        errors.append(
            "capabilities missing API surface mapping: "
            + ", ".join(f"{module}/{capability}" for module, capability in missing_capabilities)
        )
    modules: list[dict[str, Any]] = []
    for module in required_modules:
        rows = module_rows.get(module, [])
        if not rows:
            errors.append(f"API surface module has no entries: {module}")
        status = (
            "failed"
            if any(row["coverageStatus"] in {"missing_authority", "invalid"} for row in rows)
            else "partial"
            if any(
                row["coverageStatus"] in {"thin", "uncovered", "branch_gap"}
                for row in rows
            )
            else "covered"
        )
        modules.append(
            {
                "name": module,
                "coverageStatus": status,
                "apiCount": len(rows),
                "apis": rows,
            }
        )

    api_closure_status = "passed" if not errors else "failed"
    module_status = (
        "failed"
        if errors or any(module["coverageStatus"] == "failed" for module in modules)
        else "partial"
        if any(module["coverageStatus"] == "partial" for module in modules)
        else "passed"
    )
    return {
        "schema": PUBLIC_API_REPORT_SCHEMA,
        "status": "passed" if api_closure_status == "passed" else "failed",
        "contract": _artifact(api_surface_path),
        "capabilityContract": _artifact(capability_contract_path),
        "apiSurfaceClosure": {
            "status": api_closure_status,
            "apiCount": len(seen_ids),
            "classifiedApiCount": len(seen_ids) if not errors else len(seen_ids),
            "unclassifiedApiCount": 0 if not errors else len(errors),
            "meaning": "source-side retained public API denominator has a disposition",
        },
        "moduleApiCoverage": {
            "status": module_status,
            "counts": dict(sorted(coverage_counts.items())),
            "meaning": "API items and major runtime branches have item-local execution evidence",
        },
        "fixtureCorpusClosure": {
            "status": fixture_corpus_closure_status,
            "meaning": "registered fixture roles and native authority artifacts are closed",
        },
        "cadCoreRuntimeParity": {
            "status": "not_evaluated",
            "meaning": "native fixture authority does not prove CAD Core runtime parity",
        },
        "modules": modules,
        "errors": errors,
    }


def build_public_capability_coverage_report(
    authority_report: dict[str, Any],
    *,
    capability_contract_path: Path,
    fixture_corpus_closure_status: str,
    process_contract_root: Path | None = None,
) -> dict[str, Any]:
    """Map an explicit retained capability inventory back to fixture evidence.

    The inventory is intentionally independent from the fixture corpus. Inferring it from
    existing fixtures would make a closed corpus look like complete module API coverage even
    when a whole public branch has no fixture expression.
    """

    contract = json.loads(capability_contract_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    if contract.get("schema") != PUBLIC_CAPABILITY_CONTRACT_SCHEMA:
        errors.append(
            "capability contract schema must be " + PUBLIC_CAPABILITY_CONTRACT_SCHEMA
        )
    scope = contract.get("scope")
    if not isinstance(scope, str) or not scope:
        errors.append("capability contract scope must be a non-empty string")
    required_modules = contract.get("requiredModules")
    if not isinstance(required_modules, list) or not required_modules or not all(
        isinstance(item, str) and item for item in required_modules
    ):
        errors.append("capability contract requiredModules must be a non-empty string list")
        required_modules = []
    elif len(required_modules) != len(set(required_modules)):
        errors.append("capability contract requiredModules must be unique")

    role_by_fixture = {
        f"{row.get('phase')}/{row.get('case')}": str(row.get("role"))
        for row in authority_report.get("cases", [])
        if isinstance(row, dict)
    }
    seen_modules: set[str] = set()
    seen_capabilities: set[tuple[str, str]] = set()
    modules: list[dict[str, Any]] = []
    coverage_counts: Counter[str] = Counter()

    for module_index, module in enumerate(contract.get("modules", [])):
        if not isinstance(module, dict):
            errors.append(f"modules[{module_index}] must be an object")
            continue
        module_name = module.get("name")
        if not isinstance(module_name, str) or not module_name:
            errors.append(f"modules[{module_index}].name must be a non-empty string")
            continue
        if module_name in seen_modules:
            errors.append(f"duplicate capability module {module_name}")
        seen_modules.add(module_name)
        capability_rows: list[dict[str, Any]] = []
        capabilities = module.get("capabilities")
        if not isinstance(capabilities, list) or not capabilities:
            errors.append(f"{module_name}.capabilities must be a non-empty list")
            capabilities = []
        for capability_index, capability in enumerate(capabilities):
            if not isinstance(capability, dict):
                errors.append(
                    f"{module_name}.capabilities[{capability_index}] must be an object"
                )
                continue
            capability_id = capability.get("id")
            disposition = capability.get("disposition")
            source_evidence = capability.get("sourceEvidence")
            fixture_evidence = capability.get("fixtureEvidence")
            rationale = capability.get("rationale")
            if not isinstance(capability_id, str) or not capability_id:
                errors.append(
                    f"{module_name}.capabilities[{capability_index}].id must be a non-empty string"
                )
                continue
            identity = (module_name, capability_id)
            if identity in seen_capabilities:
                errors.append(f"duplicate capability {module_name}/{capability_id}")
            seen_capabilities.add(identity)
            for field in ("publicSurface", "runtimeBranch"):
                value = capability.get(field)
                if not isinstance(value, str) or not value:
                    errors.append(
                        f"{module_name}/{capability_id}.{field} must be a non-empty string"
                    )
            if not isinstance(source_evidence, list) or not source_evidence or not all(
                isinstance(item, str) and item for item in source_evidence
            ):
                errors.append(
                    f"{module_name}/{capability_id} requires FreeCAD sourceEvidence"
                )
                source_evidence = []
            if not isinstance(fixture_evidence, list):
                errors.append(
                    f"{module_name}/{capability_id}.fixtureEvidence must be a list"
                )
                fixture_evidence = []

            evidence_rows: list[dict[str, Any]] = []
            for evidence_index, evidence in enumerate(fixture_evidence):
                if not isinstance(evidence, dict):
                    errors.append(
                        f"{module_name}/{capability_id}.fixtureEvidence[{evidence_index}] must be an object"
                    )
                    continue
                fixture = evidence.get("fixture")
                level = evidence.get("level")
                if not isinstance(fixture, str) or "/" not in fixture:
                    errors.append(
                        f"{module_name}/{capability_id}.fixtureEvidence[{evidence_index}].fixture is invalid"
                    )
                    continue
                if not isinstance(level, str) or not level:
                    errors.append(
                        f"{module_name}/{capability_id}.fixtureEvidence[{evidence_index}].level is invalid"
                    )
                    continue
                if level not in CAPABILITY_EVIDENCE_LEVELS:
                    errors.append(
                        f"{module_name}/{capability_id}.fixtureEvidence[{evidence_index}].level is unsupported: {level}"
                    )
                evidence_role = role_by_fixture.get(fixture, "missing")
                receipt_artifact: dict[str, Any] | None = None
                process_cases: list[str] | None = None
                if level == "native_process_test":
                    raw_process_cases = evidence.get("processCases")
                    process_cases = (
                        list(raw_process_cases)
                        if isinstance(raw_process_cases, list)
                        and all(
                            isinstance(case_id, str) and case_id
                            for case_id in raw_process_cases
                        )
                        else []
                    )
                    _receipt, receipt_path, receipt_errors = _native_process_contract_receipt(
                        capability_contract_path,
                        fixture,
                        process_contract_root=process_contract_root,
                        required_case_ids=process_cases,
                    )
                    evidence_role = (
                        "native_process_test" if not receipt_errors else "missing"
                    )
                    receipt_artifact = _artifact(receipt_path)
                    errors.extend(
                        f"{module_name}/{capability_id}: {error}"
                        for error in receipt_errors
                    )
                evidence_rows.append(
                    {
                        "fixture": fixture,
                        "level": level,
                        "role": evidence_role,
                        **(
                            {"processContractReceipt": receipt_artifact}
                            if receipt_artifact is not None
                            else {}
                        ),
                        **(
                            {"processCases": process_cases}
                            if process_cases is not None
                            else {}
                        ),
                    }
                )

            support_matrix_row: dict[str, Any] | None = None
            support_matrix_valid = True
            support_matrix_id = capability.get("supportMatrixReceipt")
            requires_support_matrix = (
                module_name == "OndselSolver"
                and capability_id == "full_joint_type_and_degenerate_matrix"
            )
            if requires_support_matrix and support_matrix_id is None:
                errors.append(
                    f"{module_name}/{capability_id}.supportMatrixReceipt is required"
                )
                support_matrix_valid = False
            if support_matrix_id is not None:
                if not isinstance(support_matrix_id, str) or not support_matrix_id:
                    errors.append(
                        f"{module_name}/{capability_id}.supportMatrixReceipt is invalid"
                    )
                    support_matrix_valid = False
                else:
                    _, receipt_path, matrix_path, receipt_errors = (
                        _assembly_support_matrix_receipt(
                            capability_contract_path,
                            support_matrix_id,
                        )
                    )
                    support_matrix_valid = not receipt_errors
                    errors.extend(
                        f"{module_name}/{capability_id}: {error}"
                        for error in receipt_errors
                    )
                    support_matrix_row = {
                        "id": support_matrix_id,
                        "status": "passed" if support_matrix_valid else "failed",
                        "report": _artifact(receipt_path),
                        "matrix": _artifact(matrix_path),
                    }

            coverage_status = "uncovered"
            if disposition == "native_fixture":
                if not evidence_rows or any(
                    item["role"] != "native" for item in evidence_rows
                ) or not support_matrix_valid:
                    coverage_status = "missing_authority"
                elif any(
                    item["level"] in EXECUTION_EVIDENCE_LEVELS
                    for item in evidence_rows
                ):
                    coverage_status = "covered"
                else:
                    coverage_status = "thin"
            elif disposition == "native_process_test":
                coverage_status = (
                    "covered"
                    if any(
                        item["level"] == "native_process_test"
                        and item["role"] == "native_process_test"
                        for item in evidence_rows
                    )
                    else "missing_authority"
                )
                if not isinstance(rationale, str) or not rationale:
                    errors.append(
                        f"{module_name}/{capability_id} native process test requires boundary rationale"
                    )
            elif disposition == "uncovered":
                if not isinstance(rationale, str) or not rationale:
                    errors.append(
                        f"{module_name}/{capability_id} uncovered capability requires rationale"
                    )
            elif disposition in NON_NATIVE_CAPABILITY_DISPOSITIONS:
                coverage_status = "non_native_exception"
                if not isinstance(rationale, str) or not rationale:
                    errors.append(
                        f"{module_name}/{capability_id} non-native exception requires rationale"
                    )
            else:
                errors.append(
                    f"{module_name}/{capability_id} has invalid disposition {disposition!r}"
                )
                coverage_status = "invalid"

            coverage_counts[coverage_status] += 1
            capability_rows.append(
                {
                    "id": capability_id,
                    "publicSurface": capability.get("publicSurface"),
                    "runtimeBranch": capability.get("runtimeBranch"),
                    "disposition": disposition,
                    "coverageStatus": coverage_status,
                    "sourceEvidence": source_evidence,
                    "fixtureEvidence": evidence_rows,
                    "rationale": rationale,
                    **(
                        {"supportMatrixReceipt": support_matrix_row}
                        if support_matrix_row is not None
                        else {}
                    ),
                }
            )
        module_status = (
            "failed"
            if any(
                item["coverageStatus"] in {"missing_authority", "invalid"}
                for item in capability_rows
            )
            else "partial"
            if any(
                item["coverageStatus"] in {"thin", "uncovered"}
                for item in capability_rows
            )
            else "covered"
        )
        modules.append(
            {
                "name": module_name,
                "coverageStatus": module_status,
                "capabilityCount": len(capability_rows),
                "capabilities": capability_rows,
            }
        )

    actual_module_order = [item["name"] for item in modules]
    if required_modules and actual_module_order != required_modules:
        errors.append(
            "capability contract modules must exactly match requiredModules order"
        )

    api_status = (
        "failed"
        if errors or any(item["coverageStatus"] == "failed" for item in modules)
        else "partial"
        if any(item["coverageStatus"] == "partial" for item in modules)
        else "covered"
    )
    return {
        "schema": PUBLIC_CAPABILITY_REPORT_SCHEMA,
        "status": "failed" if errors or api_status == "failed" else "passed",
        "scope": scope,
        "scopeLimitations": contract.get("scopeLimitations", []),
        "contract": _artifact(capability_contract_path),
        "fixtureCorpusClosure": {
            "status": fixture_corpus_closure_status,
            "meaning": "registered fixture roles and native authority artifacts are closed",
        },
        "moduleApiCoverage": {
            "status": api_status,
            "counts": dict(sorted(coverage_counts.items())),
            "meaning": "explicit retained public capabilities and major branches mapped to fixtures",
        },
        "cadCoreRuntimeParity": {
            "status": "not_evaluated",
            "meaning": "native fixture authority does not prove CAD Core runtime parity",
        },
        "modules": modules,
        "errors": errors,
    }


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
    if report is not None and report.get("schema") == "freecad-producer-reproduction-bundle/v1":
        bundle_errors: list[str] = []
        base_ref = report.get("baseReport")
        incremental_refs = report.get("incrementalReports")
        if not isinstance(base_ref, str) or not base_ref:
            bundle_errors.append("producer bundle baseReport is missing")
            base_ref = ""
        if (
            not isinstance(incremental_refs, list)
            or not incremental_refs
            or not all(isinstance(item, str) and item for item in incremental_refs)
        ):
            bundle_errors.append("producer bundle incrementalReports are missing")
            incremental_refs = []
        references = [base_ref, *incremental_refs] if base_ref else list(incremental_refs)
        if len(references) != len(set(references)):
            bundle_errors.append("producer bundle report references must be unique")

        receipt_rows: list[dict[str, Any]] = []
        covered_keys: set[tuple[str, str]] = set()
        base_keys: set[tuple[str, str]] = set()
        producer_sha256: set[str] = set()
        collector_sha256: set[str] = set()
        bundle_root = producer_report_path.parent.resolve()
        for receipt_index, relative in enumerate(references):
            receipt_path = (producer_report_path.parent / relative).resolve()
            receipt_errors: list[str] = []
            if bundle_root not in receipt_path.parents:
                receipt_errors.append("receipt path escapes producer report root")
                receipt_doc: dict[str, Any] | None = None
            elif not receipt_path.is_file():
                receipt_errors.append("receipt file is missing")
                receipt_doc = None
            else:
                try:
                    candidate = json.loads(receipt_path.read_text(encoding="utf-8"))
                except json.JSONDecodeError:
                    candidate = None
                receipt_doc = candidate if isinstance(candidate, dict) else None
                if receipt_doc is None:
                    receipt_errors.append("receipt is not a JSON object")

            entry_by_key: dict[tuple[str, str], dict[str, Any]] = {}
            if receipt_doc is not None:
                required_top_level = {
                    "schema": "freecad-fixture-regression-report/v1",
                    "status": "passed",
                    "mode": "repeated-native-check",
                    "publicExpectedStatus": "passed",
                    "ledgerValidationStatus": "passed",
                    "producerTraceStatus": "not_evaluated",
                }
                for key, expected in required_top_level.items():
                    if receipt_doc.get(key) != expected:
                        receipt_errors.append(f"{key} must be {expected!r}")
                producer = receipt_doc.get("candidate")
                collector = receipt_doc.get("collector")
                if (
                    not isinstance(producer, dict)
                    or not producer.get("path")
                    or not producer.get("sha256")
                ):
                    receipt_errors.append("FreeCADCmd identity is incomplete")
                else:
                    producer_sha256.add(str(producer["sha256"]))
                if not isinstance(collector, dict) or not collector.get("sha256"):
                    receipt_errors.append("collector identity is incomplete")
                else:
                    collector_sha256.add(str(collector["sha256"]))
                for key in (
                    "candidateRunDifferences",
                    "candidateRunLedgerDrifts",
                    "candidateRunVariations",
                ):
                    if receipt_doc.get(key) != []:
                        receipt_errors.append(f"{key} must be empty")

                manifest = receipt_doc.get("manifest")
                entries = manifest.get("entries") if isinstance(manifest, dict) else None
                if not isinstance(entries, list) or not entries:
                    receipt_errors.append("manifest entries are missing")
                    entries = []
                for entry in entries:
                    if not isinstance(entry, dict):
                        receipt_errors.append("manifest contains a non-object entry")
                        continue
                    key = (str(entry.get("phase")), str(entry.get("case")))
                    if key in entry_by_key:
                        receipt_errors.append(
                            f"manifest duplicates {key[0]}/{key[1]}"
                        )
                    entry_by_key[key] = entry
                if isinstance(manifest, dict):
                    if manifest.get("cases") != len(entry_by_key):
                        receipt_errors.append("manifest case count is stale")
                    if manifest.get("phases") != len(
                        {phase for phase, _case in entry_by_key}
                    ):
                        receipt_errors.append("manifest phase count is stale")
                for key, entry in entry_by_key.items():
                    expected_hashes = native_by_key.get(key)
                    if expected_hashes is None:
                        receipt_errors.append(
                            f"manifest contains non-native authority {key[0]}/{key[1]}"
                        )
                    elif any(
                        entry.get(name) != value
                        for name, value in expected_hashes.items()
                    ):
                        receipt_errors.append(
                            f"manifest hashes are stale for {key[0]}/{key[1]}"
                        )
                runs = receipt_doc.get("runs")
                receipt_count = len(entry_by_key)
                if not isinstance(runs, list) or len(runs) != 2:
                    receipt_errors.append("receipt must contain exactly two independent runs")
                else:
                    labels = {
                        run.get("label") for run in runs if isinstance(run, dict)
                    }
                    if labels != {"run-a", "run-b"}:
                        receipt_errors.append("receipt run labels must be run-a/run-b")
                    for run in runs:
                        run_report = run.get("report") if isinstance(run, dict) else None
                        if (
                            not isinstance(run, dict)
                            or run.get("returncode") != 0
                            or not isinstance(run_report, dict)
                            or run_report.get("status") != "passed"
                            or run_report.get("discovered") != receipt_count
                            or run_report.get("processed") != receipt_count
                            or run_report.get("failed") != 0
                            or run_report.get("skipped") != 0
                            or run_report.get("publicExpectedStatus") != "passed"
                            or run_report.get("ledgerValidationStatus") != "passed"
                            or run_report.get("producerTraceStatus") != "not_evaluated"
                        ):
                            receipt_errors.append(
                                f"run {run.get('label') if isinstance(run, dict) else None!r} "
                                f"did not execute {receipt_count}/{receipt_count} cleanly"
                            )

            receipt_keys = set(entry_by_key)
            if receipt_index == 0:
                base_keys = receipt_keys
            covered_keys.update(receipt_keys)
            bundle_errors.extend(
                f"producer bundle {relative}: {error}" for error in receipt_errors
            )
            receipt_rows.append(
                {
                    "kind": "base_all_native" if receipt_index == 0 else "incremental_phase",
                    "report": _artifact(receipt_path),
                    "caseCount": len(receipt_keys),
                    "status": "passed" if not receipt_errors else "failed",
                    "errors": receipt_errors,
                }
            )

        if len(producer_sha256) > 1:
            bundle_errors.append("producer bundle FreeCADCmd identities differ")
        if len(collector_sha256) > 1:
            bundle_errors.append("producer bundle collector identities differ")
        missing_keys = set(native_by_key) - covered_keys
        extra_keys = covered_keys - set(native_by_key)
        if missing_keys:
            bundle_errors.append("producer bundle does not cover the current native inventory")
        if extra_keys:
            bundle_errors.append("producer bundle contains stale native authority")
        ignored_by_base = sorted(set(native_by_key) - base_keys)
        return {
            "status": "passed" if not bundle_errors else "failed",
            "report": report_artifact,
            "nativeCaseCount": native_count,
            "baseAllNativeCaseCount": len(base_keys),
            "ignoredByBaseAllNative": [
                {"phase": phase, "case": case} for phase, case in ignored_by_base
            ],
            "incrementalReceiptCount": max(0, len(receipt_rows) - 1),
            "receipts": receipt_rows,
            "errors": bundle_errors,
        }
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
