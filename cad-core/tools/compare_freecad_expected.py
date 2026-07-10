"""Thin command-line adapter for :mod:`freecad_expected_parity`."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

try:  # Direct ``python tools/compare...`` puts tools/ on sys.path.
    from freecad_expected_parity import EvaluationRequest, MaterializeRequest, evaluate, materialize_current
    from freecad_expected_parity.catalog import FixtureCase, load_catalog, relative
    from freecad_expected_parity.engine import REPORT_CATEGORIES, REPORT_SCHEMA, compare_payloads
except ImportError:  # ``python -m`` and package-oriented test runners.
    from tools.freecad_expected_parity import EvaluationRequest, MaterializeRequest, evaluate, materialize_current
    from tools.freecad_expected_parity.catalog import FixtureCase, load_catalog, relative
    from tools.freecad_expected_parity.engine import REPORT_CATEGORIES, REPORT_SCHEMA, compare_payloads


ROOT = Path(__file__).resolve().parents[1]
ExpectedCase = FixtureCase  # Compatibility name; ownership lives in the catalogue.


def discover_expected_cases(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
) -> list[FixtureCase]:
    """Compatibility helper backed by the single role catalogue."""

    return load_catalog(Path(root), phase=phase, case=case).cases


def report_output_path(
    phase: str | None,
    case: str | None,
    output: Path | None,
    root: Path = ROOT,
) -> Path:
    if output is not None:
        return output
    name = phase or "all"
    if case:
        name = f"{name}-{case}"
    return Path(root) / "out" / "freecad-expected-parity" / f"{name}.json"


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_strict_compare(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
    output: Path | None = None,
    *,
    live: bool = False,
    binary: Path | None = None,
    actual_source: str | None = None,
    ffi_library: Path | None = None,
    roles_path: Path | None = None,
    registry_path: Path | None = None,
) -> dict[str, Any]:
    report = evaluate(
        EvaluationRequest(
            root=root,
            phase=phase,
            case=case,
            source_kind=actual_source or ("live" if live else "snapshot"),
            binary=binary,
            ffi_library=ffi_library,
            roles_path=roles_path,
            registry_path=registry_path,
        )
    ).to_dict()
    output_path = report_output_path(phase, case, output, Path(root))
    report["report"] = relative(output_path, Path(root))
    _write_json(output_path, report)
    return report


def run_write_current(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
    *,
    binary: Path | None = None,
    roles_path: Path | None = None,
) -> dict[str, Any]:
    return materialize_current(
        MaterializeRequest(root=root, phase=phase, case=case, binary=binary, roles_path=roles_path)
    ).to_dict()


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate native FreeCAD expected output against CAD Core.")
    parser.add_argument("--phase", help="Only select one fixture phase.")
    parser.add_argument("--case", help="Only select one fixture case.")
    parser.add_argument("--strict", action="store_true", help="Write a report-only v2 parity evaluation.")
    parser.add_argument("--write-current", action="store_true", help="Materialize current outputs through the live source.")
    parser.add_argument("--live", action="store_true", help="Evaluate one fresh live CAD Core result per case.")
    parser.add_argument(
        "--actual-source",
        choices=("snapshot", "live", "rust-ffi"),
        help="Actual-payload adapter; rust-ffi calls cad_rs_recompute_json once per fixture.",
    )
    parser.add_argument("--release-gate", action="store_true", help="Run the live release gate and fail on non-passing verdicts.")
    parser.add_argument("--run-contract-tests", action="store_true", help="Run registry-selected dotted unittest ids.")
    parser.add_argument("--bin", type=Path, help="CAD Core binary for live/materialization modes.")
    parser.add_argument("--ffi-lib", type=Path, help="Rust cad-core-ffi cdylib for --actual-source rust-ffi.")
    parser.add_argument("--roles", type=Path, help="Fixture-role manifest path.")
    parser.add_argument("--registry", type=Path, help="Protocol-divergence registry path.")
    parser.add_argument("--output", type=Path, help="Parity report output path.")
    return parser.parse_args(argv)


def _stdout_payload(payload: dict[str, Any]) -> dict[str, Any]:
    if payload.get("schemaVersion") == REPORT_SCHEMA:
        return {
            "schemaVersion": payload["schemaVersion"],
            "selection": payload["selection"],
            "exactStatus": payload["exactStatus"],
            "semanticStatus": payload["semanticStatus"],
            "releaseStatus": payload["releaseStatus"],
            "releaseGatePassed": payload["releaseGatePassed"],
            "status": payload["status"],
            "summary": payload["summary"],
            "report": payload.get("report"),
        }
    return payload


def _run_contract_tests(report: dict[str, Any], root: Path) -> tuple[dict[str, Any], int]:
    tests = report.get("registryAudit", {}).get("contractTests", [])
    if not isinstance(tests, list):
        return {"contractTests": [], "status": "invalid"}, 1
    if report.get("releaseStatus") == "not_applicable":
        return {"contractTests": tests, "status": "not_run_not_applicable"}, 0
    if report.get("releaseStatus") == "invalid":
        return {"contractTests": tests, "status": "not_run_invalid_parity"}, 1
    if report.get("semanticStatus") != "green":
        return {"contractTests": tests, "status": "not_run_unaccepted_parity"}, 1
    if not tests:
        return {"contractTests": [], "status": "ok", "returncode": 0}, 0
    completed = subprocess.run([sys.executable, "-m", "unittest", *tests], cwd=root, check=False)
    return {"contractTests": tests, "status": "ok" if completed.returncode == 0 else "failed", "returncode": completed.returncode}, completed.returncode


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.actual_source == "rust-ffi" and args.ffi_lib is None:
        raise SystemExit("--ffi-lib is required for --actual-source rust-ffi")
    root = ROOT
    payloads: list[dict[str, Any]] = []
    exit_code = 0
    if args.write_current:
        generation = run_write_current(root, args.phase, args.case, binary=args.bin, roles_path=args.roles)
        payloads.append(generation)
        if generation["status"] != "ok":
            exit_code = 1
    needs_report = args.strict or args.live or args.release_gate or args.run_contract_tests or not args.write_current
    report: dict[str, Any] | None = None
    if needs_report:
        report = run_strict_compare(
            root,
            args.phase,
            args.case,
            args.output,
            live=args.live or (args.release_gate and args.actual_source is None),
            binary=args.bin,
            actual_source=args.actual_source,
            ffi_library=args.ffi_lib,
            roles_path=args.roles,
            registry_path=args.registry,
        )
        payloads.append(report)
        if report["releaseStatus"] == "invalid":
            exit_code = 1
        if args.release_gate and report["releaseStatus"] != "not_applicable" and not report["releaseGatePassed"]:
            exit_code = 1
        if args.run_contract_tests:
            contract_payload, contract_code = _run_contract_tests(report, root)
            payloads.append(contract_payload)
            if contract_code:
                exit_code = 1
    for payload in payloads:
        print(json.dumps(_stdout_payload(payload), ensure_ascii=False, indent=2, sort_keys=True))
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
