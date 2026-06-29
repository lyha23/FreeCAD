#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "c12m2.native-probe-artifact.v1"
PAYLOAD_PREFIX = "C12M2_PROBE_PAYLOAD="
BASELINE_SCRIPT_PATH = Path(__file__).with_name("6-29-20-12-c12m2-freecadcmd-baseline-probe.py")
CLASSIFICATIONS = (
    "expected_ready",
    "native_probe_blocked",
    "helper_blocked",
    "native_hidden",
    "sandbox_runtime_limit",
    "collector_bug",
    "product_boundary_rejected",
    "retained_no_expected",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Wrap C12-M2 native FreeCAD probes into the frozen artifact schema.",
    )
    parser.add_argument("--baseline", action="store_true", help="Run the FreeCADCmd version/OCCT baseline probe.")
    parser.add_argument("--freecad-script", help="Native probe Python script to execute inside FreeCADCmd.")
    parser.add_argument("--script-arg", action="append", default=[], help="Argument passed to --freecad-script.")
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD") or discover_freecadcmd())
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--out", required=True)
    parser.add_argument("--probe-id", required=True)
    parser.add_argument("--family", required=True)
    parser.add_argument("--case-id", required=True)
    parser.add_argument("--source-authority", required=True)
    parser.add_argument("--input-artifact", required=True)
    parser.add_argument("--request-local", default="not_evaluated")
    parser.add_argument("--request-local-notes", default="")
    parser.add_argument("--comparison-path", required=True)
    parser.add_argument("--expected-summary-json", help="JSON object for expected_summary.")
    parser.add_argument("--conclusion", choices=CLASSIFICATIONS)
    parser.add_argument("--note", action="append", default=[])
    return parser.parse_args()


def discover_freecadcmd() -> str:
    for name in ("freecadcmd", "FreeCADCmd", "freecadcmd-daily"):
        path = shutil.which(name)
        if path:
            return path
    return ""


def freecad_script_command(freecadcmd: str, script: Path, script_args: list[str]) -> list[str]:
    script_path = str(script.resolve())
    argv = json.dumps([script_path, *script_args], ensure_ascii=False)
    code = (
        "import sys; "
        f"sys.argv = {argv}; "
        f"exec(compile(open({script_path!r}, encoding='utf-8').read(), {script.name!r}, 'exec'))"
    )
    return [freecadcmd, "-c", code]


def script_command(args: argparse.Namespace) -> list[str]:
    if not args.freecadcmd:
        return []
    if args.baseline:
        return freecad_script_command(args.freecadcmd, BASELINE_SCRIPT_PATH, [])
    if not args.freecad_script:
        raise SystemExit("--freecad-script is required unless --baseline is used")
    return freecad_script_command(args.freecadcmd, Path(args.freecad_script), args.script_arg)


def run_command(command: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    if not command:
        return subprocess.CompletedProcess(command, 127, "", "FreeCADCmd not found")
    try:
        return subprocess.run(
            command,
            cwd=Path.cwd(),
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        return subprocess.CompletedProcess(
            command,
            124,
            exc.stdout or "",
            (exc.stderr or "") + f"\nTimeout after {timeout}s",
        )


def payload_from_stdout(stdout: str) -> dict[str, Any]:
    for line in reversed(stdout.splitlines()):
        if line.startswith(PAYLOAD_PREFIX):
            try:
                value = json.loads(line[len(PAYLOAD_PREFIX) :])
                if isinstance(value, dict):
                    return value
            except json.JSONDecodeError:
                return {}
    return {}


def classify(returncode: int, stdout: str, stderr: str, override: str | None) -> str:
    if override:
        return override
    combined = f"{stdout}\n{stderr}"
    if returncode == 0:
        return "expected_ready"
    if "Incompatible processor" in combined or "Application unexpectedly terminated" in combined:
        return "sandbox_runtime_limit"
    if "Timeout after" in combined:
        return "sandbox_runtime_limit"
    if "FreeCADCmd not found" in combined:
        return "native_probe_blocked"
    return "native_probe_blocked"


def expected_summary(args: argparse.Namespace, payload: dict[str, Any], conclusion: str) -> dict[str, Any]:
    if args.expected_summary_json:
        parsed = json.loads(args.expected_summary_json)
        if not isinstance(parsed, dict):
            raise SystemExit("--expected-summary-json must decode to an object")
        return parsed
    if args.baseline or args.family == "global_runtime_baseline":
        return {
            "kind": "runtime_baseline",
            "shape_kind": None,
            "subshape_counts": None,
            "bbox": None,
            "volume": None,
            "area": None,
            "diagnostics": [
                "S3 collected FreeCADCmd version/OCCT/LibPack only; no family native expected was collected.",
            ],
            "runtime_ready": conclusion == "expected_ready",
            "freecad_version": payload.get("freecad_version_string"),
            "occt_version": payload.get("occt_version") or payload.get("config_occt_version"),
        }
    return {
        "kind": "probe_output_unparsed",
        "shape_kind": None,
        "subshape_counts": None,
        "bbox": None,
        "volume": None,
        "area": None,
        "diagnostics": ["Native probe output must be summarized by the family probe before S6 comparison."],
    }


def build_artifact(args: argparse.Namespace, command: list[str], result: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    payload = payload_from_stdout(result.stdout)
    conclusion = classify(result.returncode, result.stdout, result.stderr, args.conclusion)
    return {
        "schema_version": SCHEMA_VERSION,
        "probe": {
            "id": args.probe_id,
            "family": args.family,
            "case_id": args.case_id,
        },
        "source_authority": args.source_authority,
        "input_artifact": args.input_artifact,
        "freecadcmd": {
            "path": args.freecadcmd or None,
            "version": payload.get("freecad_version_string"),
            "version_tuple": payload.get("freecad_version"),
            "occt_version": payload.get("occt_version") or payload.get("config_occt_version"),
            "libpack": payload.get("libpack"),
            "libpack_version": payload.get("libpack_version"),
            "freecad_libs": payload.get("freecad_libs"),
            "app_home_path": payload.get("app_home_path"),
            "raw_probe_payload": payload,
        },
        "command": command,
        "process": {
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "timeout_seconds": args.timeout,
        },
        "exception_classification": conclusion,
        "expected_summary": expected_summary(args, payload, conclusion),
        "request_local": {
            "judgement": args.request_local,
            "notes": args.request_local_notes,
        },
        "current_comparison_path": args.comparison_path,
        "conclusion": conclusion,
        "notes": args.note,
    }


def main() -> int:
    args = parse_args()
    command = script_command(args)
    result = run_command(command, args.timeout)
    artifact = build_artifact(args, command, result)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(artifact, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(artifact, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if result.returncode == 0 else result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
