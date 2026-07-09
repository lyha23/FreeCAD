from __future__ import annotations

import argparse
import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class ExpectedCase:
    phase: str
    case: str
    expected_path: Path
    input_path: Path
    current_path: Path


def discover_expected_cases(
    root: Path = ROOT,
    phase: str | None = None,
    case: str | None = None,
) -> list[ExpectedCase]:
    fixtures_root = root / "fixtures"
    cases: list[ExpectedCase] = []
    for expected_path in sorted(fixtures_root.glob("*/expected/*.freecad.json")):
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


def relative(path: Path, root: Path) -> str:
    return str(path.relative_to(root)) if path.is_relative_to(root) else str(path)


def regenerate_case(item: ExpectedCase, root: Path = ROOT, bin_path: Path | None = None) -> dict[str, Any]:
    binary = bin_path or root / "build" / "cad-core"
    record: dict[str, Any] = {
        "phase": item.phase,
        "case": item.case,
        "input": relative(item.input_path, root),
        "expected": relative(item.expected_path, root),
        "current": relative(item.current_path, root),
    }
    if not item.input_path.exists():
        record["status"] = "missing_input"
        return record
    if not binary.exists():
        record["status"] = "missing_binary"
        record["binary"] = relative(binary, root)
        return record

    item.current_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(binary),
        "recompute",
        str(item.input_path),
        "--output",
        str(item.current_path),
    ]
    env = os.environ.copy()
    env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
    completed = subprocess.run(
        command,
        cwd=root,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    record["returncode"] = completed.returncode
    if completed.returncode != 0:
        record["status"] = "cad_core_failed"
        record["stdout"] = completed.stdout
        record["stderr"] = completed.stderr
        return record
    record["status"] = "written"
    return record


def regenerate_cases(cases: Iterable[ExpectedCase], root: Path = ROOT, bin_path: Path | None = None) -> dict[str, Any]:
    records = [regenerate_case(item, root=root, bin_path=bin_path) for item in cases]
    failures = [item for item in records if item["status"] != "written"]
    return {
        "schemaVersion": "cad-core.freecad-expected-regenerate.v1",
        "status": "ok" if not failures else "red",
        "summary": {
            "cases": len(records),
            "written": sum(1 for item in records if item["status"] == "written"),
            "failed": len(failures),
        },
        "cases": records,
    }


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Regenerate cad-core-res outputs from expected-discovered fixture cases."
    )
    parser.add_argument("--phase", help="Only regenerate one fixture phase.")
    parser.add_argument("--case", help="Only regenerate one fixture case name.")
    parser.add_argument("--bin", type=Path, default=ROOT / "build" / "cad-core", help="cad-core binary path.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    report = regenerate_cases(
        discover_expected_cases(ROOT, args.phase, args.case),
        root=ROOT,
        bin_path=args.bin,
    )
    print(json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False))
    return 0 if report["status"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
