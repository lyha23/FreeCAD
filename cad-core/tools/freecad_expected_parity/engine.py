"""The deep release-gate module: catalog, exact diff, registry and sources."""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

from .catalog import CatalogResult, FixtureCase, load_catalog, relative, sha256_bytes
from .family_metadata import metadata_for_unaccepted_diff
from .model import (
    ArtifactEvidence,
    CaseReport,
    EvaluationRequest,
    GenerationReport,
    MaterializeRequest,
    ParityReport,
)
from .registry import Registry, apply_registry, load_registry
from .public_semantics import compare_public_semantics
from .sources import ActualPayload, atomic_write_json_group, make_actual_source, parse_payload
try:
    from tools.element_map_producer_trace import TraceValidationError, compare_traces, validate_trace
except ModuleNotFoundError:
    from element_map_producer_trace import TraceValidationError, compare_traces, validate_trace


REPORT_SCHEMA = "cad-core.freecad-expected-parity.v2"
GENERATION_SCHEMA = "cad-core.freecad-expected-generation.v2"
FLOAT_TOLERANCE = 1e-6
REPORT_CATEGORIES = (
    "diagnostics",
    "results",
    "results.subshapes",
    "topoNamingState.objects",
    "topoNamingState.subshapes",
    "topoNamingState.elementMap",
    "topoNamingState.childElementMaps",
    "topoNamingState.mapperHistory",
    "geometry.numeric",
    "json",
)


def _trace_observation(
    item: FixtureCase,
    root: Path,
    expected_response: dict[str, Any] | None,
    actual: ActualPayload,
) -> tuple[str, dict[str, Any] | None, dict[str, str]]:
    expected_trace_path = item.expected_trace_path()
    links = {
        "comparator": "tools/compare_element_map_producer_trace.py",
        "expected": relative(expected_trace_path, root),
        "actual": actual.producer_trace_path or relative(item.current_trace_path(), root),
    }
    if not expected_trace_path.exists():
        return "missing", {"detail": f"missing expected producer trace: {links['expected']}"}, links
    if expected_response is None:
        return "invalid", {"detail": "native response unavailable for trace binding"}, links
    try:
        expected_trace = json.loads(expected_trace_path.read_text(encoding="utf-8"))
        request_payload = json.loads(item.input_path.read_text(encoding="utf-8"))
        validate_trace(
            expected_trace,
            input_document=request_payload,
            response_document=expected_response,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, TraceValidationError) as exc:
        return "invalid", {"detail": f"invalid expected producer trace: {exc}"}, links
    if actual.producer_trace_error or actual.producer_trace is None:
        detail = actual.producer_trace_error or "missing actual producer trace"
        status = "missing" if "missing" in detail or "no producer trace" in detail else "invalid"
        return status, {"detail": detail}, links
    result = compare_traces(
        expected_trace,
        actual.producer_trace,
        document_graph=(
            request_payload
            if isinstance(request_payload.get("Objects"), list)
            else None
        ),
    )
    diagnostic = {
        "classification": result.classification,
        "semanticScopePath": list(result.semantic_scope_path),
        "eventIdentity": {
            "expected": list(result.expected_event_identity) if result.expected_event_identity else None,
            "actual": list(result.actual_event_identity) if result.actual_event_identity else None,
        },
        "firstJsonPointer": result.json_pointer,
        "expectedValue": result.expected_value,
        "actualValue": result.actual_value,
        "beforeAlignment": result.before_alignment,
        "afterAlignment": result.after_alignment,
        "downstreamDriftCount": result.downstream_drift_count,
        "owner": result.owner,
        "detail": result.detail,
    }
    return ("aligned" if result.status == "equal" else result.status), diagnostic, links
IGNORED_ACTUAL_TOP_LEVEL_KEYS = {"binaryPayloads", "documentObjectUpdates"}
RAW_MAPPED_NAME_FIELDS = {"rawFreecadMappedName", "raw_mapped_name"}
GEOMETRY_NUMERIC_FIELDS = {
    "area",
    "bbox",
    "center",
    "centerOfMass",
    "edgeSegments",
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
FREECAD_MAPPED_NAME_HASH_RE = re.compile(r":H(?!\*)-?[0-9A-Fa-f]+(?::[0-9A-Fa-f]+)?")
FREECAD_MAPPED_NAME_COLON_HASH_RE = re.compile(r":H:(?!\*)-?[0-9A-Fa-f]+")
FREECAD_MAPPED_NAME_DELETE_RE = re.compile(r";D(?!\*)[0-9A-Fa-f]+")

COMPARISON_PROFILE: dict[str, Any] = {
    "schemaVersion": "cad-core.freecad-expected-comparison-profile.v2",
    "artifactRawMappedNameNormalization": "FreeCAD :H/:D fragments",
    "publicSemanticMappedName": "canonical-only; producer-local raw/stable tokens excluded",
    "publicSemanticProductExtensions": ["mesh", "binaryPayloads", "documentObjectUpdates", "result_fields"],
    "publicSemanticMapperHistory": "relation/recoverability/candidate-shape outcome",
    "keyedLists": ["diagnostics", "results", "subshapes", "childElementMaps", "mapperHistory"],
    "geometryNumericTolerance": FLOAT_TOLERANCE,
    "ignoredActualTopLevelKeys": sorted(IGNORED_ACTUAL_TOP_LEVEL_KEYS),
}


def _canonical_json(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _request_path(value: Path | str | None, root: Path) -> Path | None:
    if value is None:
        return None
    path = Path(value)
    return path if path.is_absolute() else root / path


def _profile_sha256() -> str:
    return sha256_bytes(_canonical_json(COMPARISON_PROFILE))


def canonical_freecad_mapped_name(mapped_name: str) -> str:
    def replace_hash(match: re.Match[str]) -> str:
        return ":H*:*" if match.group(0).count(":") > 1 else ":H*"

    normalized = FREECAD_MAPPED_NAME_HASH_RE.sub(replace_hash, mapped_name)
    normalized = FREECAD_MAPPED_NAME_COLON_HASH_RE.sub(":H*", normalized)
    return FREECAD_MAPPED_NAME_DELETE_RE.sub(";D*", normalized)


def _mapped_key(item: dict[str, Any], candidates: tuple[str, ...], fallback: str) -> str:
    for candidate in candidates:
        value = item.get(candidate)
        if isinstance(value, str) and value:
            return value
        if isinstance(value, int):
            return str(value)
    return fallback


def _source_target_key(item: dict[str, Any], index: int) -> str:
    parts = [
        str(item.get("id") or ""),
        str(item.get("relation") or ""),
        str(item.get("maker_stage") or ""),
        json.dumps(item.get("source", {}), sort_keys=True, ensure_ascii=False),
        json.dumps(item.get("target", {}), sort_keys=True, ensure_ascii=False),
    ]
    key = "|".join(part for part in parts if part)
    return key or f"index:{index}"


def _unique_index(items: list[Any], path: tuple[str, ...]) -> dict[str, Any]:
    counts: Counter[str] = Counter()
    indexed: dict[str, Any] = {}
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            base = f"index:{index}"
        elif path and path[-1] == "diagnostics":
            base = _mapped_key(item, ("code",), f"index:{index}")
        elif path and path[-1] == "results":
            base = _mapped_key(item, ("object",), f"index:{index}")
        elif path and path[-1] == "subshapes":
            base = _mapped_key(item, ("indexed", "id", "subname"), f"index:{index}")
        elif path and path[-1] == "childElementMaps":
            key = item.get("key")
            if isinstance(key, str) and key:
                base = key
            else:
                base = f"{item.get('childObject', 'unknown')}:{item.get('childIndex', index)}:{item.get('pathPrefix', '')}"
        elif path and path[-1] == "mapperHistory":
            base = _source_target_key(item, index)
        else:
            base = f"index:{index}"
        suffix = counts[base]
        counts[base] += 1
        indexed[base if suffix == 0 else f"{base}#{suffix + 1}"] = item
    return indexed


def _is_raw_mapped_name_path(path: tuple[str, ...]) -> bool:
    return bool(path) and (path[-1] in RAW_MAPPED_NAME_FIELDS or path[-2:] == ("mappedName", "raw"))


def canonical_payload(value: Any, path: tuple[str, ...] = ()) -> Any:
    """Normalize only comparison-profile variability, never semantic identities."""

    if isinstance(value, str):
        return canonical_freecad_mapped_name(value) if _is_raw_mapped_name_path(path) else value
    if isinstance(value, list):
        if path and path[-1] in {"diagnostics", "results", "subshapes", "childElementMaps", "mapperHistory"}:
            return {key: canonical_payload(item, path + (key,)) for key, item in _unique_index(value, path).items()}
        return [canonical_payload(item, path + (str(index),)) for index, item in enumerate(value)]
    if isinstance(value, dict):
        return {
            str(key): canonical_payload(item, path + (str(key),))
            for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))
        }
    return value


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _path_string(path: tuple[str, ...]) -> str:
    return ".".join(path) if path else "$"


def _summarize(value: Any) -> Any:
    if isinstance(value, dict):
        return {"type": "object", "size": len(value), "keys": sorted(map(str, value.keys()))[:12]}
    if isinstance(value, list):
        return {"type": "array", "size": len(value)}
    return value


def _is_geometry_numeric_path(path: tuple[str, ...]) -> bool:
    return any(part in GEOMETRY_NUMERIC_FIELDS for part in path)


def category_for_path(path: tuple[str, ...], expected: Any = None, actual: Any = None) -> str:
    if (_is_number(expected) or _is_number(actual)) and _is_geometry_numeric_path(path):
        return "geometry.numeric"
    if path and path[0] == "diagnostics":
        return "diagnostics"
    if path and path[0] == "results":
        return "results.subshapes" if "subshapes" in path else "results"
    if path and path[0] == "topoNamingState":
        if "childElementMaps" in path:
            return "topoNamingState.childElementMaps"
        if "mapperHistory" in path:
            return "topoNamingState.mapperHistory"
        if "elementMap" in path:
            return "topoNamingState.elementMap"
        if "subshapes" in path:
            return "topoNamingState.subshapes"
        return "topoNamingState.objects"
    return "json"


def _make_diff(kind: str, path: tuple[str, ...], expected: Any, actual: Any) -> dict[str, Any]:
    if _is_raw_mapped_name_path(path):
        comparison_class = "representation_difference"
    elif kind == "extra" and len(path) >= 3 and path[0] == "results":
        comparison_class = "product_extension"
    else:
        comparison_class = "public_semantic"
    diff: dict[str, Any] = {
        "category": category_for_path(path, expected, actual),
        "comparisonClass": comparison_class,
        "kind": kind,
        "path": _path_string(path),
        "_actualValue": actual,
    }
    if expected is not None:
        diff["expected"] = _summarize(expected)
    if actual is not None:
        diff["actual"] = _summarize(actual)
    return diff


def diff_values(
    expected: Any,
    actual: Any,
    path: tuple[str, ...] = (),
    diffs: list[dict[str, Any]] | None = None,
) -> list[dict[str, Any]]:
    if diffs is None:
        diffs = []
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            if not path and key in IGNORED_ACTUAL_TOP_LEVEL_KEYS and key not in expected:
                continue
            child = path + (str(key),)
            if key not in expected:
                diffs.append(_make_diff("extra", child, None, actual[key]))
            elif key not in actual:
                diffs.append(_make_diff("missing", child, expected[key], None))
            else:
                diff_values(expected[key], actual[key], child, diffs)
        return diffs
    if isinstance(expected, list) and isinstance(actual, list):
        for index in range(max(len(expected), len(actual))):
            child = path + (str(index),)
            if index >= len(expected):
                diffs.append(_make_diff("extra", child, None, actual[index]))
            elif index >= len(actual):
                diffs.append(_make_diff("missing", child, expected[index], None))
            else:
                diff_values(expected[index], actual[index], child, diffs)
        return diffs
    if _is_number(expected) and _is_number(actual):
        if _is_geometry_numeric_path(path):
            if abs(float(expected) - float(actual)) > FLOAT_TOLERANCE:
                diffs.append(_make_diff("numeric", path, expected, actual))
        elif expected != actual:
            diffs.append(_make_diff("value", path, expected, actual))
        return diffs
    if type(expected) is not type(actual):
        diffs.append(_make_diff("type", path, expected, actual))
    elif expected != actual:
        diffs.append(_make_diff("value", path, expected, actual))
    return diffs


def compare_payloads(expected: dict[str, Any], actual: dict[str, Any]) -> list[dict[str, Any]]:
    return diff_values(canonical_payload(expected), canonical_payload(actual))


def _load_file_payload(path: Path, label: str) -> ActualPayload:
    if not path.exists():
        return ActualPayload(None, None, f"missing {label}: {path}")
    try:
        raw = path.read_bytes()
    except OSError as exc:
        return ActualPayload(None, None, f"cannot read {label} {path}: {exc}")
    return parse_payload(raw, label=str(path))


def _strict_ledger_errors(expected_path: Path) -> list[str]:
    try:
        try:
            from validate_freecad_expected_ledger import validate_expected_file  # type: ignore
        except ImportError:
            from tools.validate_freecad_expected_ledger import validate_expected_file  # type: ignore
        return list(validate_expected_file(expected_path, strict=True))
    except Exception as exc:  # The preflight must fail closed even if the validator cannot load.
        return [f"ledger validator failed: {exc}"]


def _normalized_sha256(payload: dict[str, Any]) -> str:
    return sha256_bytes(_canonical_json(canonical_payload(payload)))


def _visible_diff(diff: dict[str, Any]) -> dict[str, Any]:
    return {key: value for key, value in diff.items() if not key.startswith("_")}


def _annotate_non_c4m6_unaccepted_family_metadata(cases: list[CaseReport]) -> None:
    """Restore S4 ownership metadata without widening protocol acceptance.

    ``apply_registry`` is intentionally the only code that can set
    ``accepted=True``.  Family metadata is attached after that decision and
    only makes unresolved non-c4m6 work actionable in snapshot reports.
    """

    for item in cases:
        if item.phase == "c4m6":
            continue
        for diff in item.diffs:
            if diff.get("accepted") is True:
                continue
            metadata = metadata_for_unaccepted_diff(item.phase, item.case, diff)
            if diff.get("decision") == "unaccepted_diff":
                diff.update(metadata)
                continue
            # Preserve a registry failure's explicit reason.  It remains red,
            # while the family fields still point the implementation work at
            # the same S4 known-gap surface.
            diff["familyDecision"] = metadata["decision"]
            for field, value in metadata.items():
                if field != "decision":
                    diff.setdefault(field, value)


def _case_report(
    item: FixtureCase,
    root: Path,
    source: Any,
    *,
    validate_ledger: bool,
) -> CaseReport:
    evidence = ArtifactEvidence()
    report = CaseReport(
        phase=item.phase,
        case=item.case,
        role=item.role,
        status="invalid",
        input=relative(item.input_path, root),
        expected=relative(item.expected_path, root),
        current=relative(item.current_path, root),
        artifact_evidence=evidence,
    )
    try:
        evidence.input_sha256 = sha256_bytes(item.input_path.read_bytes())
    except OSError as exc:
        report.preflight_errors.append(f"cannot read input: {exc}")
    expected = _load_file_payload(item.expected_path, "native expected")
    if expected.raw_bytes is not None:
        evidence.expected_sha256 = sha256_bytes(expected.raw_bytes)
    if expected.error:
        report.preflight_errors.append(expected.error)
    try:
        ledger_raw = item.ledger_path.read_bytes()
        evidence.ledger_sha256 = sha256_bytes(ledger_raw)
    except OSError as exc:
        report.preflight_errors.append(f"cannot read ledger: {exc}")
    if validate_ledger:
        report.preflight_errors.extend(_strict_ledger_errors(item.expected_path))

    current = _load_file_payload(item.current_path, "current artifact")
    if current.raw_bytes is not None:
        evidence.current_raw_sha256 = sha256_bytes(current.raw_bytes)
    if current.error:
        report.preflight_errors.append(current.error)
    elif current.payload is not None:
        evidence.current_normalized_sha256 = _normalized_sha256(current.payload)

    actual = source.load(item)
    if actual.raw_bytes is not None:
        evidence.actual_raw_sha256 = sha256_bytes(actual.raw_bytes)
    if actual.error:
        report.source_error = actual.error
        report.preflight_errors.append(actual.error)
    elif actual.payload is not None:
        evidence.actual_normalized_sha256 = _normalized_sha256(actual.payload)
    if evidence.actual_normalized_sha256 is not None and evidence.current_normalized_sha256 is not None:
        evidence.current_fresh = evidence.actual_normalized_sha256 == evidence.current_normalized_sha256

    report.trace_status, report.trace_diagnostic, report.trace_links = _trace_observation(
        item,
        root,
        expected.payload,
        actual,
    )

    if report.preflight_errors or expected.payload is None or actual.payload is None:
        report.preflight_errors = list(dict.fromkeys(report.preflight_errors))
        return report
    artifact_diffs = compare_payloads(expected.payload, actual.payload)
    semantic_diffs = compare_public_semantics(expected.payload, actual.payload)
    semantic_paths = {str(diff.get("path")) for diff in semantic_diffs}
    observations: list[dict[str, Any]] = []
    for artifact_diff in artifact_diffs:
        if artifact_diff.get("comparisonClass") != "public_semantic":
            observations.append(artifact_diff)
            continue
        if str(artifact_diff.get("path")) in semantic_paths:
            continue
        representation = dict(artifact_diff)
        representation["comparisonClass"] = "representation_difference"
        observations.append(representation)
    diffs = [*semantic_diffs, *observations]
    for diff in diffs:
        diff["phase"] = item.phase
        diff["case"] = item.case
    report.artifact_diff_count = len(artifact_diffs)
    report.diffs = diffs
    report.status = "green" if not artifact_diffs else "red"
    return report


def _summary(cases: list[CaseReport]) -> dict[str, Any]:
    categories = {category: 0 for category in REPORT_CATEGORIES}
    decisions: Counter[str] = Counter()
    accepted = 0
    unaccepted = 0
    comparison_classes: Counter[str] = Counter()
    for item in cases:
        for diff in item.diffs:
            comparison_classes[str(diff.get("comparisonClass", "public_semantic"))] += 1
            category = str(diff.get("category", "json"))
            categories[category] = categories.get(category, 0) + 1
            decision = diff.get("decision")
            if isinstance(decision, str) and decision:
                decisions[decision] += 1
            if diff.get("accepted") is True:
                accepted += 1
            else:
                unaccepted += 1
    return {
        "cases": len(cases),
        "passed": sum(item.status == "green" for item in cases),
        "red": sum(item.status == "red" for item in cases),
        "invalid": sum(item.status == "invalid" for item in cases),
        "diffs": accepted + unaccepted,
        "artifactDiffs": sum(item.artifact_diff_count for item in cases),
        "semanticDiffs": comparison_classes["public_semantic"],
        "productExtensions": comparison_classes["product_extension"],
        "representationDifferences": comparison_classes["representation_difference"],
        "accepted": accepted,
        "unaccepted": unaccepted,
        "categories": categories,
        "decisions": dict(sorted(decisions.items())),
    }


def _release_status(
    *,
    source_kind: str,
    invalid: bool,
    exact: str,
    semantic: str,
    cases: list[CaseReport],
) -> tuple[str, bool]:
    if invalid:
        return "invalid", False
    if source_kind not in {"live", "rust-ffi", "freecad-kernel-v2"}:
        return "not_evaluated", False
    if source_kind == "live" and any(item.artifact_evidence.current_fresh is not True for item in cases):
        return "invalid", False
    if semantic == "red":
        return "red", False
    if exact == "green":
        return "green", True
    return "protocol_divergence", True


def evaluate(request: EvaluationRequest) -> ParityReport:
    """Evaluate selected fixtures without persisting any current artifact."""

    root = request.root_path().resolve()
    roles_path = _request_path(request.roles_path, root)
    registry_path = _request_path(request.registry_path, root) if request.registry_path else Path(__file__).with_name(
        "protocol_divergences.v1.json"
    )
    catalog: CatalogResult = load_catalog(root, phase=request.phase, case=request.case, roles_path=roles_path)
    registry: Registry = load_registry(registry_path)
    global_errors = list(catalog.errors)
    source = make_actual_source(
        request.source_kind,
        root=root,
        binary=_request_path(request.binary, root),
        ffi_library=_request_path(request.ffi_library, root),
        in_memory_actuals=request.in_memory_actuals,
        timeout_seconds=request.timeout_seconds,
    )
    if source is None:
        global_errors.append(f"unsupported actual source kind: {request.source_kind}")
    # A selected phase/case may contain only protocol-only, unsupported, or
    # permanent internal-probe fixtures.  With no native expected artifact
    # there is nothing for this comparator to compare; report that scope as
    # not applicable.  Keep fail-closed behavior for an unknown selection or
    # any catalogue/preflight error (those have no skipped role evidence).
    not_applicable = not catalog.cases and bool(catalog.skipped) and not global_errors
    if not catalog.cases and not not_applicable:
        global_errors.append("zero native fixture cases selected")

    cases: list[CaseReport] = []
    if source is not None:
        for item in catalog.cases:
            cases.append(_case_report(item, root, source, validate_ledger=request.validate_ledger))

    all_diffs = [diff for item in cases for diff in item.diffs]
    registry_audit = apply_registry(registry, all_diffs, phase=request.phase, case=request.case)
    _annotate_non_c4m6_unaccepted_family_metadata(cases)
    if not registry_audit["valid"]:
        global_errors.extend(str(error) for error in registry_audit["validationErrors"])
    if request.source_kind == "live" and any(
        item.artifact_evidence.current_fresh is not True for item in cases
    ):
        global_errors.append("live actual payload does not match every checked-in current artifact")

    case_invalid = any(item.status == "invalid" for item in cases)
    invalid = bool(global_errors) or case_invalid
    if invalid:
        exact_status = "not_evaluated"
        semantic_status = "not_evaluated"
    elif not_applicable:
        exact_status = "not_evaluated"
        semantic_status = "not_evaluated"
    else:
        exact_status = "green" if not any(item.artifact_diff_count for item in cases) else "red"
        semantic_status = "green" if all(diff.get("accepted") is True for diff in all_diffs) else "red"
    if not_applicable:
        release_status, passed = "not_applicable", False
    else:
        release_status, passed = _release_status(
            source_kind=request.source_kind,
            invalid=invalid,
            exact=exact_status,
            semantic=semantic_status,
            cases=cases,
        )
    summary = _summary(cases)
    actual_source_evidence = (
        source.evidence()
        if source is not None and callable(getattr(source, "evidence", None))
        else None
    )
    for item in cases:
        item.diffs = [_visible_diff(diff) for diff in item.diffs]
    return ParityReport(
        schema_version=REPORT_SCHEMA,
        selection={"phase": request.phase, "case": request.case, "sourceKind": request.source_kind},
        run_evidence={
            "sourceKind": request.source_kind,
            "binarySha256": _binary_sha256(_request_path(request.binary, root), root)
            if request.source_kind == "live"
            else None,
            "ffiLibrarySha256": _binary_sha256(_request_path(request.ffi_library, root), root)
            if request.source_kind in {"rust-ffi", "freecad-kernel-v2"}
            else None,
            "actualSource": actual_source_evidence,
            "comparisonProfile": COMPARISON_PROFILE,
            "comparisonProfileSha256": _profile_sha256(),
            "registrySha256": registry.sha256,
            "fixtureRolesSha256": catalog.roles_sha256,
        },
        exact_status=exact_status,
        semantic_status=semantic_status,
        release_status=release_status,
        release_gate_passed=passed,
        summary=summary,
        cases=cases,
        registry_audit=registry_audit,
        preflight={
            "valid": not invalid,
            "errors": sorted(dict.fromkeys(global_errors)),
            "skipped": catalog.skipped,
            "rolesPath": str(catalog.roles_path),
            "registryPath": str(registry.path),
        },
    )


def _binary_sha256(binary: Path | str | None, root: Path) -> str | None:
    path = Path(binary) if binary else root / "build" / "cad-core"
    try:
        return sha256_bytes(path.read_bytes())
    except OSError:
        return None


def materialize_current(request: MaterializeRequest) -> GenerationReport:
    """Run live payload generation, validate every result, then atomically replace currents.

    Generation is all-or-nothing at the selected scope: any failed runner,
    malformed JSON, missing role/ledger, or zero selection leaves every current
    artifact untouched.
    """

    root = request.root_path().resolve()
    roles_path = _request_path(request.roles_path, root)
    catalog = load_catalog(root, phase=request.phase, case=request.case, roles_path=roles_path)
    errors = list(catalog.errors)
    if not catalog.cases:
        errors.append("zero native fixture cases selected")
    source = make_actual_source(
        "live",
        root=root,
        binary=_request_path(request.binary, root),
        ffi_library=None,
        in_memory_actuals=None,
        timeout_seconds=request.timeout_seconds,
    )
    assert source is not None
    staged: list[tuple[FixtureCase, dict[str, Any], ActualPayload, dict[str, Any]]] = []
    records: list[dict[str, Any]] = []
    for item in catalog.cases:
        record: dict[str, Any] = {
            "phase": item.phase,
            "case": item.case,
            "input": relative(item.input_path, root),
            "expected": relative(item.expected_path, root),
            "current": relative(item.current_path, root),
            "currentProducerTrace": relative(item.current_trace_path(), root),
            "status": "invalid",
        }
        if request.validate_ledger:
            ledger_errors = _strict_ledger_errors(item.expected_path)
            if ledger_errors:
                record["errors"] = ledger_errors
                errors.extend(ledger_errors)
                records.append(record)
                continue
        actual = source.load(item)
        if actual.error or actual.payload is None:
            record["errors"] = [actual.error or "missing live payload"]
            errors.extend(record["errors"])
            records.append(record)
            continue
        if (
            actual.producer_trace_error
            or actual.producer_trace is None
            or actual.producer_trace_raw is None
        ):
            record["errors"] = [
                actual.producer_trace_error
                or f"missing validated producer trace for {item.label()}"
            ]
            errors.extend(record["errors"])
            records.append(record)
            continue
        record["status"] = "staged"
        record["actualRawSha256"] = sha256_bytes(actual.raw_bytes) if actual.raw_bytes else None
        record["actualProducerTraceSha256"] = (
            sha256_bytes(actual.producer_trace_raw) if actual.producer_trace_raw else None
        )
        records.append(record)
        staged.append((item, actual.payload, actual, record))
    if errors:
        return GenerationReport(
            schema_version=GENERATION_SCHEMA,
            status="invalid",
            selection={"phase": request.phase, "case": request.case, "sourceKind": "live"},
            summary={"cases": len(records), "written": 0, "failed": len(errors)},
            cases=records,
            preflight={"valid": False, "errors": sorted(dict.fromkeys(errors))},
        )
    artifacts: list[tuple[Path, dict[str, Any]]] = []
    for item, payload, actual, _record in staged:
        assert actual.producer_trace is not None
        artifacts.extend(
            [
                (item.current_path, payload),
                (item.current_trace_path(), actual.producer_trace),
            ]
        )
    try:
        atomic_write_json_group(artifacts)
    except (OSError, RuntimeError, ValueError) as exc:
        message = f"atomic response/producer-trace materialization failed: {exc}"
        errors.append(message)
        for _item, _payload, _actual, record in staged:
            record["status"] = "invalid"
            record["errors"] = [message]

    if errors:
        return GenerationReport(
            schema_version=GENERATION_SCHEMA,
            status="invalid",
            selection={"phase": request.phase, "case": request.case, "sourceKind": "live"},
            summary={"cases": len(records), "written": 0, "failed": len(errors)},
            cases=records,
            preflight={"valid": False, "errors": sorted(dict.fromkeys(errors))},
        )
    for _item, _payload, _actual, record in staged:
        record["status"] = "written"
    return GenerationReport(
        schema_version=GENERATION_SCHEMA,
        status="ok",
        selection={"phase": request.phase, "case": request.case, "sourceKind": "live"},
        summary={"cases": len(records), "written": len(records), "failed": 0},
        cases=records,
        preflight={"valid": True, "errors": []},
    )
