from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from topo_naming_state_test_helpers import canonical_freecad_mapped_name


REPORT_SCHEMA = "cad-core.freecad-expected-parity.v1"
FLOAT_TOLERANCE = 1e-6
IGNORED_ACTUAL_TOP_LEVEL_KEYS = {"binaryPayloads", "documentObjectUpdates"}
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
CLASSIFICATION_FIELDS = (
    "owner",
    "owner_step",
    "decision",
    "freecad_authority",
    "next_action",
    "close_condition",
)
HASH_MISMATCH_CASES = {
    "topo-state-document-hash-mismatch",
    "topo-state-object-hash-mismatch",
}
LINK_COMPOUND_CASES = {
    "topo-state-link-compound-child-maps",
}
RAW_MAPPED_NAME_FIELDS = {
    "rawFreecadMappedName",
    "raw_mapped_name",
}
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


@dataclass(frozen=True)
class ExpectedCase:
    phase: str
    case: str
    expected_path: Path
    input_path: Path
    current_path: Path

    def label(self) -> str:
        return f"{self.phase}/{self.case}"


def discover_expected_cases(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
) -> list[ExpectedCase]:
    fixtures_root = root / "fixtures"
    pattern = "*/expected/*.freecad.json"
    cases: list[ExpectedCase] = []
    for expected_path in sorted(fixtures_root.glob(pattern)):
        case_phase = expected_path.parent.parent.name
        case_name = expected_path.name.removesuffix(".freecad.json")
        if phase is not None and case_phase != phase:
            continue
        if case is not None and case_name != case:
            continue
        cases.append(
            ExpectedCase(
                phase=case_phase,
                case=case_name,
                expected_path=expected_path,
                input_path=fixtures_root / case_phase / f"{case_name}.json",
                current_path=fixtures_root
                / case_phase
                / "cad-core-res"
                / f"{case_name}.cad-core.json",
            )
        )
    return cases


def report_output_path(
    phase: str | None,
    case: str | None,
    output: Path | None,
    root: Path = ROOT,
) -> Path:
    if output is not None:
        return output
    name = phase or "all"
    if case is not None:
        name = f"{name}-{case}"
    return root / "out" / "freecad-expected-parity" / f"{name}.json"


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def mapped_key_for_item(item: dict[str, Any], candidates: tuple[str, ...], fallback: str) -> str:
    for candidate in candidates:
        value = item.get(candidate)
        if isinstance(value, str) and value:
            return value
        if isinstance(value, int):
            return str(value)
    return fallback


def source_target_key(item: dict[str, Any], index: int) -> str:
    parts = [
        str(item.get("id") or ""),
        str(item.get("relation") or ""),
        str(item.get("maker_stage") or ""),
        json.dumps(item.get("source", {}), sort_keys=True, ensure_ascii=False),
        json.dumps(item.get("target", {}), sort_keys=True, ensure_ascii=False),
    ]
    key = "|".join(part for part in parts if part)
    return key or f"index:{index}"


def unique_index(items: list[Any], path: tuple[str, ...]) -> dict[str, Any]:
    counts: Counter[str] = Counter()
    indexed: dict[str, Any] = {}
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            base = f"index:{index}"
        elif path and path[-1] == "diagnostics":
            base = mapped_key_for_item(item, ("code",), f"index:{index}")
        elif path and path[-1] == "results":
            base = mapped_key_for_item(item, ("object",), f"index:{index}")
        elif path and path[-1] == "subshapes":
            base = mapped_key_for_item(item, ("indexed", "id", "subname"), f"index:{index}")
        elif path and path[-1] == "childElementMaps":
            key = item.get("key")
            if isinstance(key, str) and key:
                base = key
            else:
                child_object = item.get("childObject", "unknown")
                child_index = item.get("childIndex", index)
                path_prefix = item.get("pathPrefix", "")
                base = f"{child_object}:{child_index}:{path_prefix}"
        elif path and path[-1] == "mapperHistory":
            base = source_target_key(item, index)
        else:
            base = f"index:{index}"

        suffix = counts[base]
        counts[base] += 1
        indexed[base if suffix == 0 else f"{base}#{suffix + 1}"] = item
    return indexed


def is_raw_mapped_name_path(path: tuple[str, ...]) -> bool:
    if not path:
        return False
    if path[-1] in RAW_MAPPED_NAME_FIELDS:
        return True
    return len(path) >= 2 and path[-2:] == ("mappedName", "raw")


def canonical_payload(value: Any, path: tuple[str, ...] = ()) -> Any:
    if isinstance(value, str):
        if is_raw_mapped_name_path(path):
            return canonical_freecad_mapped_name(value)
        return value
    if isinstance(value, list):
        if path and path[-1] in {
            "diagnostics",
            "results",
            "subshapes",
            "childElementMaps",
            "mapperHistory",
        }:
            return {
                key: canonical_payload(item, path + (key,))
                for key, item in unique_index(value, path).items()
            }
        return [canonical_payload(item, path + (str(index),)) for index, item in enumerate(value)]
    if isinstance(value, dict):
        return {
            str(key): canonical_payload(item, path + (str(key),))
            for key, item in sorted(value.items(), key=lambda kv: str(kv[0]))
        }
    return value


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def path_string(path: tuple[str, ...]) -> str:
    return ".".join(path) if path else "$"


def summarize_value(value: Any) -> Any:
    if isinstance(value, dict):
        return {
            "type": "object",
            "size": len(value),
            "keys": sorted(map(str, value.keys()))[:12],
        }
    if isinstance(value, list):
        return {"type": "array", "size": len(value)}
    return value


def is_geometry_numeric_path(path: tuple[str, ...]) -> bool:
    return any(part in GEOMETRY_NUMERIC_FIELDS for part in path)


def category_for_path(path: tuple[str, ...], expected: Any = None, actual: Any = None) -> str:
    if (is_number(expected) or is_number(actual)) and is_geometry_numeric_path(path):
        return "geometry.numeric"
    if path and path[0] == "diagnostics":
        return "diagnostics"
    if path and path[0] == "results":
        if "subshapes" in path:
            return "results.subshapes"
        return "results"
    if path and path[0] == "topoNamingState":
        if "childElementMaps" in path:
            return "topoNamingState.childElementMaps"
        if "mapperHistory" in path:
            return "topoNamingState.mapperHistory"
        if "elementMap" in path:
            return "topoNamingState.elementMap"
        if "subshapes" in path:
            return "topoNamingState.subshapes"
        if len(path) >= 2 and path[1] == "objects":
            return "topoNamingState.objects"
        return "topoNamingState.objects"
    return "json"


def diff_values(
    expected: Any,
    actual: Any,
    path: tuple[str, ...] = (),
    diffs: list[dict[str, Any]] | None = None,
) -> list[dict[str, Any]]:
    if diffs is None:
        diffs = []

    if isinstance(expected, dict) and isinstance(actual, dict):
        keys = sorted(set(expected) | set(actual))
        for key in keys:
            if not path and key in IGNORED_ACTUAL_TOP_LEVEL_KEYS and key not in expected:
                continue
            child_path = path + (str(key),)
            if key not in expected:
                diffs.append(
                    make_diff("extra", child_path, None, actual[key])
                )
                continue
            if key not in actual:
                diffs.append(
                    make_diff("missing", child_path, expected[key], None)
                )
                continue
            diff_values(expected[key], actual[key], child_path, diffs)
        return diffs

    if isinstance(expected, list) and isinstance(actual, list):
        max_len = max(len(expected), len(actual))
        for index in range(max_len):
            child_path = path + (str(index),)
            if index >= len(expected):
                diffs.append(make_diff("extra", child_path, None, actual[index]))
                continue
            if index >= len(actual):
                diffs.append(make_diff("missing", child_path, expected[index], None))
                continue
            diff_values(expected[index], actual[index], child_path, diffs)
        return diffs

    if is_number(expected) and is_number(actual):
        if is_geometry_numeric_path(path):
            if abs(float(expected) - float(actual)) > FLOAT_TOLERANCE:
                diffs.append(make_diff("numeric", path, expected, actual))
        elif expected != actual:
            diffs.append(make_diff("value", path, expected, actual))
        return diffs

    if type(expected) is not type(actual):
        diffs.append(make_diff("type", path, expected, actual))
        return diffs

    if expected != actual:
        diffs.append(make_diff("value", path, expected, actual))
    return diffs


def make_diff(kind: str, path: tuple[str, ...], expected: Any, actual: Any) -> dict[str, Any]:
    category = category_for_path(path, expected, actual)
    diff: dict[str, Any] = {
        "category": category,
        "kind": kind,
        "path": path_string(path),
    }
    if expected is not None:
        diff["expected"] = summarize_value(expected)
    if actual is not None:
        diff["actual"] = summarize_value(actual)
    return diff


def compare_payloads(expected: dict[str, Any], actual: dict[str, Any]) -> list[dict[str, Any]]:
    expected_canonical = canonical_payload(expected)
    actual_canonical = canonical_payload(actual)
    return diff_values(expected_canonical, actual_canonical)


def classification(
    owner: str,
    owner_step: str,
    decision: str,
    freecad_authority: str,
    next_action: str,
    close_condition: str,
) -> dict[str, str]:
    return {
        "owner": owner,
        "owner_step": owner_step,
        "decision": decision,
        "freecad_authority": freecad_authority,
        "next_action": next_action,
        "close_condition": close_condition,
    }


def classify_diff(phase: str, case_name: str, diff: dict[str, Any]) -> dict[str, Any]:
    classified = dict(diff)
    classified.update(classification_for_diff(phase, case_name, diff))
    return classified


def classification_for_diff(
    phase: str,
    case_name: str,
    diff: dict[str, Any],
) -> dict[str, str]:
    category = str(diff.get("category", "json"))
    path = str(diff.get("path", ""))

    if case_name in HASH_MISMATCH_CASES:
        return classification(
            "cad-core/src/runtime/topo_naming_state.cpp; cad-core/src/runtime/recompute.cpp",
            "S3",
            "intentional_protocol_divergence",
            "c4m6 native expected hash-mismatch fixtures and topoNamingState protocol boundary",
            "Document/object hash mismatch follows native expected recompute semantics; cad-core still returns mesh transport metadata for frontend consumers.",
            "The only remaining hash-mismatch diffs are registered transport metadata divergences.",
        )

    if category == "topoNamingState.mapperHistory":
        return classification(
            "cad-core/src/runtime/topo_naming_state.cpp; cad-core/src/part/topo_shape.cpp",
            "S3",
            "mapper_history_publication_gap",
            "src/App/ElementMap.cpp; src/Mod/Part/App/TopoShapeMapper.cpp",
            "Publish expected-facing mapperHistory events from the topo history ledger without leaking unrelated indexed history.",
            "c4m6 mapperHistory diffs are green, or each remaining event is documented as an intentional divergence.",
        )

    if category in {
        "topoNamingState.objects",
        "topoNamingState.subshapes",
        "topoNamingState.elementMap",
        "topoNamingState.childElementMaps",
    }:
        return classification(
            "cad-core/src/runtime/topo_naming_state.cpp",
            "S3",
            "runtime_publication_gap",
            "src/App/ElementMap.cpp; src/Mod/Part/App/TopoShapeExpansion.cpp",
            "Publish the full native expected public object set, subshapes, elementMap and childElementMaps for c4m6.",
            "c4m6 topoNamingState object/subshape/elementMap publication diffs are strict green.",
        )

    if case_name in LINK_COMPOUND_CASES and category == "results":
        return classification(
            "cad-core/src/runtime/recompute.cpp; cad-core/src/app/link.cpp",
            "S3",
            "intentional_protocol_divergence",
            "src/App/PropertyLinks.cpp; src/Mod/Part/App/TopoShapeExpansion.cpp",
            "Link child-path stableSubname diagnostics are resolved; cad-core keeps the Link result payload as frontend transport metadata while native expected records owner projection only.",
            "The Link compound case has no diagnostics/topoNamingState diffs; the only remaining result diff is documented as transport metadata divergence.",
        )

    if category == "diagnostics":
        return classification(
            "cad-core/src/runtime/recompute.cpp",
            "S3",
            "stable_subname_diagnostic_policy",
            "src/App/PropertyLinks.cpp; src/Mod/Part/App/TopoShapeExpansion.cpp",
            "Align stableSubname diagnostics and Link compound result publication with the native expected policy.",
            "c4m6 stableSubname diagnostic/result diffs are strict green, or the protocol divergence is recorded.",
        )

    if category in {"results", "geometry.numeric"}:
        if path.endswith(".mesh") or path.endswith(".subshapes") or path in {
            "results.CompoundLink",
            "results.HistoryProbe",
        }:
            return classification(
                "cad-core/src/runtime/recompute.cpp",
                "S2/S3",
                "intentional_protocol_divergence",
                "c4m6 native expected result payload and release-output protocol boundary",
                "Keep mesh/helper result payloads as cad-core frontend transport metadata while native expected remains a public semantic oracle.",
                "Every remaining result transport diff is documented in the C13-M5 matrix as intentional divergence.",
            )
        return classification(
            "cad-core/src/runtime/recompute.cpp",
            "S3",
            "runtime_publication_gap",
            "c4m6 native expected result payload",
            "Publish native expected result fields such as bbox, topology_counts, volume and sketch metadata.",
            "c4m6 result-field diffs are strict green.",
        )

    if category == "results.subshapes":
        return classification(
            "cad-core/src/runtime/recompute.cpp",
            "S2/S3",
            "intentional_protocol_divergence",
            "c4m6 native expected result payload and release-output protocol boundary",
            "Keep response subshape maps as cad-core frontend transport metadata while native expected records the semantic summary only.",
            "Every remaining result subshape transport diff is documented in the C13-M5 matrix as intentional divergence.",
        )

    if phase == "c4m6":
        return classification(
            "cad-core/src/runtime/recompute.cpp",
            "S2/S3",
            "protocol_decision_required",
            "c4m6 native expected payload and release-output protocol boundary",
            "Classify this remaining c4m6 strict diff before treating the phase as release comparable.",
            "Every c4m6 strict diff has a concrete implementation owner or documented protocol decision.",
        )

    return classification(
        "cad-core/tools/compare_freecad_expected.py",
        "S4",
        "unclassified_phase_gap",
        "phase-family expected payload",
        "Classify this phase-family diff before closing its strict release gate.",
        "The selected phase-family report has complete owner and decision metadata.",
    )


def relative(path: Path, root: Path) -> str:
    return str(path.relative_to(root)) if path.is_relative_to(root) else str(path)


def missing_case_report(case: ExpectedCase, kind: str, path: Path, root: Path) -> dict[str, Any]:
    diffs = [
        classify_diff(
            case.phase,
            case.case,
            {
                "category": "results",
                "kind": kind,
                "path": relative(path, root),
            },
        )
    ]
    return {
        "phase": case.phase,
        "case": case.case,
        "expected": relative(case.expected_path, root),
        "current": relative(case.current_path, root),
        "status": "red",
        "diffCount": len(diffs),
        "categories": count_field(diffs, "category"),
        "decisions": count_field(diffs, "decision"),
        "diffs": diffs,
    }


def compare_case(case: ExpectedCase, root: Path = ROOT) -> dict[str, Any]:
    if not case.current_path.exists():
        return missing_case_report(case, "missing_current", case.current_path, root)
    expected = load_json(case.expected_path)
    actual = load_json(case.current_path)
    diffs = [
        classify_diff(case.phase, case.case, diff)
        for diff in compare_payloads(expected, actual)
    ]
    return {
        "phase": case.phase,
        "case": case.case,
        "expected": relative(case.expected_path, root),
        "current": relative(case.current_path, root),
        "status": "green" if not diffs else "red",
        "diffCount": len(diffs),
        "categories": count_field(diffs, "category"),
        "decisions": count_field(diffs, "decision"),
        "diffs": diffs,
    }


def count_field(diffs: list[dict[str, Any]], field: str) -> dict[str, int]:
    counts: Counter[str] = Counter()
    for diff in diffs:
        value = diff.get(field)
        if isinstance(value, str) and value:
            counts[value] += 1
    return dict(sorted(counts.items()))


def summarize_cases(cases: list[dict[str, Any]]) -> dict[str, Any]:
    categories: dict[str, int] = {category: 0 for category in REPORT_CATEGORIES}
    decisions: Counter[str] = Counter()
    for case in cases:
        for diff in case["diffs"]:
            categories[diff.get("category", "json")] = categories.get(diff.get("category", "json"), 0) + 1
            decision = diff.get("decision")
            if isinstance(decision, str) and decision:
                decisions[decision] += 1
    red_cases = [case for case in cases if case["status"] != "green"]
    return {
        "cases": len(cases),
        "passed": len(cases) - len(red_cases),
        "red": len(red_cases),
        "categories": categories,
        "decisions": dict(sorted(decisions.items())),
    }


def build_report(
    cases: list[ExpectedCase],
    phase: str | None = None,
    case: str | None = None,
    root: Path = ROOT,
) -> dict[str, Any]:
    case_reports = [compare_case(item, root=root) for item in cases]
    summary = summarize_cases(case_reports)
    return {
        "schemaVersion": REPORT_SCHEMA,
        "phase": phase,
        "case": case,
        "status": "green" if summary["red"] == 0 else "red",
        "summary": summary,
        "cases": case_reports,
    }


def run_strict_compare(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
    output: Path | None = None,
) -> dict[str, Any]:
    cases = discover_expected_cases(root, phase, case)
    report = build_report(cases, phase, case, root=root)
    output_path = report_output_path(phase, case, output, root)
    report["report"] = relative(output_path, root)
    write_json(output_path, report)
    return report


def run_write_current(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
) -> dict[str, Any]:
    import regenerate_cad_core_res

    cases = discover_expected_cases(root, phase, case)
    return regenerate_cad_core_res.regenerate_cases(cases, root=root)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare native FreeCAD expected JSON against generated cad-core-res output."
    )
    parser.add_argument("--phase", help="Only compare one fixture phase.")
    parser.add_argument("--case", help="Only compare one fixture case name.")
    parser.add_argument("--strict", action="store_true", help="Write a strict structured diff report.")
    parser.add_argument(
        "--write-current",
        action="store_true",
        help="Regenerate cad-core-res outputs for expected-discovered cases before comparing.",
    )
    parser.add_argument("--output", type=Path, help="Report output path for --strict.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    payloads: list[dict[str, Any]] = []
    if args.write_current:
        generation_report = run_write_current(ROOT, args.phase, args.case)
        payloads.append(generation_report)
        if generation_report["status"] != "ok":
            print(json.dumps(generation_report, indent=2, sort_keys=True, ensure_ascii=False))
            return 1

    if args.strict or not args.write_current:
        payloads.append(run_strict_compare(ROOT, args.phase, args.case, args.output))

    for payload in payloads:
        print(json.dumps(stdout_payload(payload), indent=2, sort_keys=True, ensure_ascii=False))
    return 0


def stdout_payload(payload: dict[str, Any]) -> dict[str, Any]:
    if payload.get("schemaVersion") == REPORT_SCHEMA:
        return {
            "schemaVersion": payload["schemaVersion"],
            "phase": payload.get("phase"),
            "case": payload.get("case"),
            "status": payload["status"],
            "summary": payload["summary"],
            "report": payload.get("report"),
        }
    return payload


if __name__ == "__main__":
    raise SystemExit(main())
