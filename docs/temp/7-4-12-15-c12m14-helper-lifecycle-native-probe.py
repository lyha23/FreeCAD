#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import time
import traceback
from pathlib import Path
from typing import Any, Callable


SCHEMA_VERSION = "c12m14.helper-lifecycle-native-probe.v1"
MARKER = b"C12M14_HELPER_LIFECYCLE_PAYLOAD="
INSIDE_ENV = "C12M14_HELPER_LIFECYCLE_INSIDE_FREECAD"
CASE_ENV = "C12M14_HELPER_LIFECYCLE_CASE"
TIMEOUT_SECONDS = 25

try:
    SCRIPT_PATH = Path(__file__).resolve()
except NameError:
    SCRIPT_PATH = Path(sys.argv[0]).resolve()
OUTPUT_JSON = SCRIPT_PATH.with_name("7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json")
VERSION_TXT = SCRIPT_PATH.with_name("7-4-12-15-c12m14-helper-lifecycle-freecadcmd-version.txt")

SOURCE_AUTHORITY = (
    "src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::"
    "add/remove/isReady/getStatus/build/shape/firstShape/lastShape/generated/simulate/makeSolid"
)
INPUT_ARTIFACT = (
    "docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe.py plus "
    "cad-core/fixtures/c12m13/expected/part-sweep-helper-mutable-sequence.freecad.json"
)


CASE_META: dict[str, dict[str, Any]] = {
    "__version__": {
        "scope_ids": [],
        "methods": ["runtime_baseline"],
        "description": "FreeCAD/OCCT/LibPack runtime baseline.",
    },
    "baseline_subset": {
        "scope_ids": ["C12M14-ORACLE-001"],
        "methods": ["add", "isReady", "getStatus", "build", "shape", "makeSolid"],
        "description": "Collected helper subset baseline inherited from C12-M13.",
    },
    "remove_before_add": {
        "scope_ids": ["C12M14-ORACLE-101"],
        "methods": ["remove", "isReady", "getStatus", "build"],
        "description": "Delete a profile before it was added, then inspect readiness/build diagnostics.",
    },
    "remove_after_add_before_build": {
        "scope_ids": ["C12M14-ORACLE-101"],
        "methods": ["add", "remove", "isReady", "getStatus", "build"],
        "description": "Add then remove before build.",
    },
    "remove_after_build": {
        "scope_ids": ["C12M14-ORACLE-101"],
        "methods": ["add", "build", "shape", "remove", "isReady", "getStatus"],
        "description": "Remove a profile after successful build.",
    },
    "remove_readd_ordering": {
        "scope_ids": ["C12M14-ORACLE-101"],
        "methods": ["add", "remove", "add", "isReady", "build", "shape"],
        "description": "Add, remove and re-add the same profile before build.",
    },
    "first_last_unbuilt": {
        "scope_ids": ["C12M14-ORACLE-102"],
        "methods": ["add", "firstShape", "lastShape"],
        "description": "firstShape/lastShape after add but before build.",
    },
    "first_last_build_fail": {
        "scope_ids": ["C12M14-ORACLE-102"],
        "methods": ["isReady", "build", "firstShape", "lastShape"],
        "description": "firstShape/lastShape after an intentionally unready build attempt.",
    },
    "first_last_build_success": {
        "scope_ids": ["C12M14-ORACLE-102"],
        "methods": ["add", "build", "firstShape", "lastShape"],
        "description": "firstShape/lastShape after successful build.",
    },
    "generated_before_build": {
        "scope_ids": ["C12M14-ORACLE-103"],
        "methods": ["add", "generated"],
        "description": "generated(profile) before build.",
    },
    "generated_after_build": {
        "scope_ids": ["C12M14-ORACLE-103"],
        "methods": ["add", "build", "generated"],
        "description": "generated(profile) after successful build.",
    },
    "generated_unknown_profile": {
        "scope_ids": ["C12M14-ORACLE-103"],
        "methods": ["add", "build", "generated"],
        "description": "generated(profile) for a shape that was not added.",
    },
    "simulate_pre_build": {
        "scope_ids": ["C12M14-ORACLE-104"],
        "methods": ["add", "simulate"],
        "description": "simulate(count=2) before build.",
    },
    "simulate_post_build": {
        "scope_ids": ["C12M14-ORACLE-104"],
        "methods": ["add", "build", "simulate"],
        "description": "simulate(count=2) after successful build.",
    },
    "simulate_unready": {
        "scope_ids": ["C12M14-ORACLE-104"],
        "methods": ["simulate"],
        "description": "simulate(count=2) without any added profile.",
    },
    "simulate_count_zero": {
        "scope_ids": ["C12M14-ORACLE-104"],
        "methods": ["add", "simulate"],
        "description": "simulate(count=0) parameter diagnostic.",
    },
    "combination_remove_readd_simulate_build": {
        "scope_ids": ["C12M14-ORACLE-105"],
        "methods": ["add", "remove", "add", "simulate", "build", "shape"],
        "description": "remove/readd/simulate/build combination instability check.",
    },
}

CASE_ORDER = [case_id for case_id in CASE_META if case_id != "__version__"]

ORACLE_CASES = {
    "C12M14-ORACLE-001": ["baseline_subset"],
    "C12M14-ORACLE-101": [
        "remove_before_add",
        "remove_after_add_before_build",
        "remove_after_build",
        "remove_readd_ordering",
    ],
    "C12M14-ORACLE-102": [
        "first_last_unbuilt",
        "first_last_build_fail",
        "first_last_build_success",
    ],
    "C12M14-ORACLE-103": [
        "generated_before_build",
        "generated_after_build",
        "generated_unknown_profile",
    ],
    "C12M14-ORACLE-104": [
        "simulate_pre_build",
        "simulate_post_build",
        "simulate_unready",
        "simulate_count_zero",
    ],
    "C12M14-ORACLE-105": ["combination_remove_readd_simulate_build"],
}


def _decode(data: bytes | str | None) -> str:
    if data is None:
        return ""
    if isinstance(data, str):
        return data
    return data.decode("utf-8", errors="replace")


def _tail(data: bytes | str | None, limit: int = 4000) -> str:
    text = _decode(data)
    return text[-limit:]


def _shell_exit(returncode: int | None) -> int | None:
    if returncode is None:
        return None
    if returncode < 0:
        return 128 + abs(returncode)
    return returncode


def _freecadcmd() -> str | None:
    override = os.environ.get("FREECADCMD")
    if override:
        return override
    return shutil.which("freecadcmd") or shutil.which("FreeCADCmd") or shutil.which("freecadcmd-daily")


def _command(script_path: Path) -> str:
    script = str(script_path)
    return (
        "import sys; "
        "sys.argv = [{!r}]; "
        "exec(compile(open({!r}, encoding='utf-8').read(), "
        "'c12m14_helper_lifecycle_native_probe.py', 'exec'))"
    ).format(script, script)


def _parse_payload(stdout: bytes) -> dict[str, Any] | None:
    for line in reversed(stdout.splitlines()):
        if line.startswith(MARKER):
            return json.loads(_decode(line[len(MARKER) :]))
    return None


def _process_classification(
    case_id: str,
    completed: subprocess.CompletedProcess[bytes] | None,
    error_type: str,
) -> str:
    stdout = completed.stdout if completed is not None else b""
    stderr = completed.stderr if completed is not None else b""
    combined = f"{_decode(stdout)}\n{_decode(stderr)}"
    if "Incompatible processor" in combined or "neon" in combined or "Application unexpectedly terminated" in combined:
        return "sandbox_runtime_limit"
    if "NCollection_Sequence::ChangeValue" in combined:
        return "native_instability_blocker"
    if error_type == "timeout":
        return "sandbox_runtime_limit"
    if completed is not None and completed.returncode is not None and completed.returncode < 0:
        return "native_instability_blocker"
    if case_id == "combination_remove_readd_simulate_build":
        return "native_instability_blocker"
    return "native_probe_blocked"


def _process_failure(
    case_id: str,
    completed: subprocess.CompletedProcess[bytes] | None,
    error_type: str,
    timeout_seconds: int | None = None,
) -> dict[str, Any]:
    meta = CASE_META[case_id]
    returncode = completed.returncode if completed is not None else None
    stdout = completed.stdout if completed is not None else b""
    stderr = completed.stderr if completed is not None else b""
    classification = _process_classification(case_id, completed, error_type)
    failure: dict[str, Any] = {
        "stage": "freecadcmd_process",
        "error_type": error_type,
        "error_message": f"FreeCADCmd did not return a stable payload for {case_id}",
        "returncode": returncode,
        "shell_exit": _shell_exit(returncode),
        "stdout_tail": _tail(stdout),
        "stderr_tail": _tail(stderr),
        "classification": classification,
    }
    if timeout_seconds is not None:
        failure["timeout_seconds"] = timeout_seconds
    return {
        "case_id": case_id,
        "scope_ids": meta["scope_ids"],
        "methods": meta["methods"],
        "description": meta["description"],
        "status": "blocked_by_environment" if classification == "sandbox_runtime_limit" else "not_collected",
        "classification": classification,
        "can_enter_s3": False,
        "can_enter_s4": False,
        "method_sequence": meta["methods"],
        "operations": [],
        "failure": failure,
    }


def _run_freecad_case(
    freecadcmd: str,
    script_path: Path,
    case_id: str,
    version_fallback: dict[str, Any] | None,
) -> dict[str, Any]:
    env = os.environ.copy()
    env[INSIDE_ENV] = "1"
    env[CASE_ENV] = case_id
    cmd = [freecadcmd, "-c", _command(script_path)]
    try:
        completed = subprocess.run(
            cmd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        completed_timeout = subprocess.CompletedProcess(cmd, None, exc.stdout or b"", exc.stderr or b"")
        payload = _process_failure(case_id, completed_timeout, "timeout", TIMEOUT_SECONDS)
        payload["process"] = {
            "command": cmd,
            "returncode": None,
            "shell_exit": None,
            "stdout": _decode(exc.stdout or b""),
            "stderr": _decode(exc.stderr or b""),
            "stdout_tail": _tail(exc.stdout or b""),
            "stderr_tail": _tail(exc.stderr or b""),
            "timeout_seconds": TIMEOUT_SECONDS,
        }
        return payload

    payload = _parse_payload(completed.stdout)
    if payload is None:
        payload = _process_failure(case_id, completed, "missing_probe_payload")
    elif completed.returncode != 0:
        payload["status"] = "not_collected"
        payload["classification"] = _process_classification(case_id, completed, "nonzero_exit_after_payload")
        payload["can_enter_s3"] = False
        payload["can_enter_s4"] = False
        payload["failure"] = {
            "stage": "freecadcmd_process",
            "error_type": "nonzero_exit_after_payload",
            "error_message": f"FreeCADCmd exited with {completed.returncode}",
            "returncode": completed.returncode,
            "shell_exit": _shell_exit(completed.returncode),
            "stdout_tail": _tail(completed.stdout),
            "stderr_tail": _tail(completed.stderr),
            "classification": payload["classification"],
        }

    payload["process"] = {
        "command": cmd,
        "returncode": completed.returncode,
        "shell_exit": _shell_exit(completed.returncode),
        "stdout": _decode(completed.stdout),
        "stderr": _decode(completed.stderr),
        "stdout_tail": _tail(completed.stdout),
        "stderr_tail": _tail(completed.stderr),
        "timeout_seconds": TIMEOUT_SECONDS,
    }
    if version_fallback and "freecad_version" not in payload:
        payload["freecad_version"] = version_fallback.get("freecad_version")
    if version_fallback and "occt_version" not in payload:
        payload["occt_version"] = version_fallback.get("occt_version")
    return payload


def _status_for_cases(cases: dict[str, dict[str, Any]], case_ids: list[str]) -> str:
    statuses = [cases[case_id].get("status") for case_id in case_ids]
    classifications = [cases[case_id].get("classification") for case_id in case_ids]
    if any(cls == "sandbox_runtime_limit" for cls in classifications):
        return "sandbox_runtime_limit"
    if any(cls == "native_instability_blocker" for cls in classifications):
        return "native_instability_blocker"
    if any(status in {"not_collected", "blocked_by_environment"} for status in statuses):
        return "native_probe_blocked"
    if all(status == "stable_native_payload" for status in statuses):
        return "stable_native_payload"
    return "stable_native_diagnostic"


def _oracle_classification(cases: dict[str, dict[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for oracle_id, case_ids in ORACLE_CASES.items():
        status = _status_for_cases(cases, case_ids)
        result[oracle_id] = {
            "case_ids": case_ids,
            "status": status,
            "stable_cases": [case_id for case_id in case_ids if cases[case_id].get("can_enter_s3")],
            "blocked_cases": [case_id for case_id in case_ids if not cases[case_id].get("can_enter_s3")],
            "can_enter_s4": False,
            "s4_rule": "S2 evidence does not unlock implementation; S3 product-contract/current-mismatch gate is required.",
        }
    return result


def _blockers(cases: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    blockers: list[dict[str, Any]] = []
    for case_id, payload in cases.items():
        if payload.get("can_enter_s3") and payload.get("classification") != "native_instability_blocker":
            continue
        blockers.append(
            {
                "case_id": case_id,
                "scope_ids": payload.get("scope_ids", []),
                "classification": payload.get("classification"),
                "failure": payload.get("failure")
                or {
                    "stage": "helper_operation",
                    "error_type": "native_instability",
                    "diagnostics": payload.get("diagnostics", []),
                },
                "can_enter_s4": False,
            }
        )
    return blockers


def _overall_conclusion(
    cases: dict[str, dict[str, Any]],
    freecadcmd: str | None,
) -> str:
    if not freecadcmd:
        return "native_probe_blocked"
    classifications = {str(case.get("classification")) for case in cases.values()}
    if "sandbox_runtime_limit" in classifications:
        return "sandbox_runtime_limit"
    if "native_instability_blocker" in classifications:
        return "native_instability_blocker"
    if any(not case.get("can_enter_s3") for case in cases.values()):
        return "native_probe_blocked"
    return "stable_native_probe_payload_s2_only"


def _can_enter_s3(cases: dict[str, dict[str, Any]]) -> bool:
    if not cases:
        return False
    hard_blockers = {"native_probe_blocked", "sandbox_runtime_limit"}
    return all(str(case.get("classification")) not in hard_blockers for case in cases.values())


def _write_version_txt(
    freecadcmd: str | None,
    version_payload: dict[str, Any] | None,
    version_case: dict[str, Any] | None,
) -> None:
    lines = [f"FreeCADCmd={freecadcmd or 'not found'}"]
    if version_case:
        proc = version_case.get("process", {})
        lines.extend(
            [
                "command=" + " ".join(proc.get("command", [])),
                f"returncode={proc.get('returncode')}",
                f"shell_exit={proc.get('shell_exit')}",
                f"classification={version_case.get('classification')}",
                f"status={version_case.get('status')}",
            ]
        )
    if version_payload:
        for key in [
            "freecad_version",
            "freecad_version_tuple",
            "build_revision",
            "build_revision_date",
            "occt_version",
            "config_occt_version",
            "libpack",
            "libpack_version",
            "freecad_libs",
            "app_home_path",
            "run_mode",
        ]:
            lines.append(f"{key}={version_payload.get(key)}")
    else:
        lines.append("version_probe_status=notCollected")
    VERSION_TXT.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _run_host() -> int:
    freecadcmd = _freecadcmd()
    if not freecadcmd:
        cases = {
            case_id: {
                "case_id": case_id,
                "scope_ids": CASE_META[case_id]["scope_ids"],
                "methods": CASE_META[case_id]["methods"],
                "description": CASE_META[case_id]["description"],
                "status": "blocked_by_environment",
                "classification": "native_probe_blocked",
                "can_enter_s3": False,
                "can_enter_s4": False,
                "method_sequence": CASE_META[case_id]["methods"],
                "operations": [],
                "failure": {
                    "stage": "freecadcmd_lookup",
                    "error_type": "command_not_found",
                    "error_message": "FreeCADCmd/freecadcmd/freecadcmd-daily was not found in PATH",
                    "classification": "native_probe_blocked",
                },
            }
            for case_id in CASE_ORDER
        }
        payload = _top_level_payload(freecadcmd, None, None, cases)
        OUTPUT_JSON.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        _write_version_txt(freecadcmd, None, None)
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        return 2

    version_case = _run_freecad_case(freecadcmd, SCRIPT_PATH, "__version__", None)
    version_payload = version_case.get("version") if isinstance(version_case.get("version"), dict) else None
    _write_version_txt(freecadcmd, version_payload, version_case)

    cases: dict[str, dict[str, Any]] = {}
    for case_id in CASE_ORDER:
        cases[case_id] = _run_freecad_case(freecadcmd, SCRIPT_PATH, case_id, version_payload)

    payload = _top_level_payload(freecadcmd, version_case, version_payload, cases)
    OUTPUT_JSON.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


def _top_level_payload(
    freecadcmd: str | None,
    version_case: dict[str, Any] | None,
    version_payload: dict[str, Any] | None,
    cases: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    blockers = _blockers(cases)
    conclusion = _overall_conclusion(cases, freecadcmd)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_at_unix": int(time.time()),
        "source_authority": SOURCE_AUTHORITY,
        "input_artifact": INPUT_ARTIFACT,
        "execution_model": "one FreeCADCmd process per case using -c exec(compile(open(...).read(), ...))",
        "freecadcmd": {
            "path": freecadcmd,
            "version": (version_payload or {}).get("freecad_version"),
            "version_tuple": (version_payload or {}).get("freecad_version_tuple"),
            "occt_version": (version_payload or {}).get("occt_version")
            or (version_payload or {}).get("config_occt_version"),
            "libpack": (version_payload or {}).get("libpack"),
            "libpack_version": (version_payload or {}).get("libpack_version"),
            "app_home_path": (version_payload or {}).get("app_home_path"),
            "raw_probe_payload": version_payload,
            "version_case": version_case,
        },
        "cases": cases,
        "oracle_classification": _oracle_classification(cases),
        "process_failures": blockers,
        "blockers": blockers,
        "can_enter_s3": _can_enter_s3(cases),
        "can_enter_s4": False,
        "s4_rule": "S2 writes temporary evidence only; S3 must decide product-contract/current-mismatch before any C++ work.",
        "conclusion": conclusion,
    }


def _inside_imports() -> tuple[Any, Any]:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    return FreeCAD, Part


def _inside_version(FreeCAD: Any, Part: Any) -> dict[str, Any]:
    items = FreeCAD.Version()
    if len(items) >= 4:
        freecad_version = f"{items[0]}.{items[1]}.{items[2]} revision {str(items[3]).split()[0]}"
    else:
        freecad_version = " ".join(str(item) for item in items)
    return {
        "freecad_version": freecad_version,
        "freecad_version_tuple": [str(item) for item in items],
        "build_revision": FreeCAD.ConfigGet("BuildRevision"),
        "build_revision_date": FreeCAD.ConfigGet("BuildRevisionDate"),
        "occt_version": getattr(Part, "OCC_VERSION", None),
        "config_occt_version": FreeCAD.ConfigGet("OCC_VERSION"),
        "libpack": FreeCAD.ConfigGet("LibPack"),
        "libpack_version": FreeCAD.ConfigGet("LibPackVersion"),
        "freecad_libs": FreeCAD.ConfigGet("FreeCADLibs"),
        "app_home_path": FreeCAD.getHomePath(),
        "run_mode": "Cmd",
    }


def _counts(shape: Any) -> dict[str, int]:
    return {
        "solids": len(getattr(shape, "Solids", [])),
        "shells": len(getattr(shape, "Shells", [])),
        "faces": len(getattr(shape, "Faces", [])),
        "wires": len(getattr(shape, "Wires", [])),
        "edges": len(getattr(shape, "Edges", [])),
        "vertices": len(getattr(shape, "Vertexes", [])),
    }


def _shape_summary(shape: Any) -> dict[str, Any]:
    if shape is None:
        return {"is_null": True, "none": True}
    try:
        if shape.isNull():
            return {"is_null": True}
    except Exception:
        return {"summary_error": "shape has no isNull()", "python_type": type(shape).__name__}
    bbox = shape.BoundBox
    result: dict[str, Any] = {
        "is_null": False,
        "shape_type": str(getattr(shape, "ShapeType", "")),
        "topology_counts": _counts(shape),
        "bbox": {
            "xmin": round(float(bbox.XMin), 6),
            "ymin": round(float(bbox.YMin), 6),
            "zmin": round(float(bbox.ZMin), 6),
            "xmax": round(float(bbox.XMax), 6),
            "ymax": round(float(bbox.YMax), 6),
            "zmax": round(float(bbox.ZMax), 6),
        },
    }
    for attr in ("Area", "Volume", "Length"):
        try:
            result[attr.lower()] = round(float(getattr(shape, attr)), 6)
        except Exception:
            result[attr.lower()] = None
    return result


def _value_summary(value: Any) -> dict[str, Any]:
    if value is None:
        return {"return_kind": "none", "value": None}
    if isinstance(value, bool):
        return {"return_kind": "bool", "value": value}
    if isinstance(value, int):
        return {"return_kind": "int", "value": value}
    if isinstance(value, float):
        return {"return_kind": "float", "value": value}
    if isinstance(value, (list, tuple)):
        return {
            "return_kind": "list",
            "length": len(value),
            "items": [_value_summary(item) for item in value],
        }
    if hasattr(value, "ShapeType") or hasattr(value, "isNull"):
        return {"return_kind": "shape", "shape": _shape_summary(value)}
    return {"return_kind": type(value).__name__, "repr": repr(value)}


def _make_context(FreeCAD: Any, Part: Any) -> dict[str, Any]:
    v = FreeCAD.Vector
    spine = Part.makePolygon([v(0, 0, 0), v(0, 0, 8)])
    profile = Part.Wire([Part.makeCircle(1.0, v(0, 0, 0), v(0, 0, 1))])
    profile_alt = Part.Wire([Part.makeCircle(0.65, v(0, 0, 4), v(0, 0, 1))])
    profile_unknown = Part.Wire([Part.makeCircle(0.35, v(5, 0, 0), v(0, 0, 1))])
    brep_offset_api = getattr(Part, "BRepOffsetAPI", None)
    candidates = [
        ("Part.BRepOffsetAPI.MakePipeShell", getattr(brep_offset_api, "MakePipeShell", None)),
        ("Part.BRepOffsetAPI_MakePipeShell", getattr(Part, "BRepOffsetAPI_MakePipeShell", None)),
    ]
    for helper_name, factory in candidates:
        if factory is not None:
            helper = factory(spine)
            return {
                "helper": helper,
                "helper_name": helper_name,
                "spine": spine,
                "profile": profile,
                "profile_alt": profile_alt,
                "profile_unknown": profile_unknown,
                "input_summary": {
                    "helper_name": helper_name,
                    "spine": _shape_summary(spine),
                    "profile": _shape_summary(profile),
                    "profile_alt": _shape_summary(profile_alt),
                    "profile_unknown": _shape_summary(profile_unknown),
                },
            }
    raise RuntimeError("No Part.BRepOffsetAPI.MakePipeShell or Part.BRepOffsetAPI_MakePipeShell factory found")


def _record(
    operations: list[dict[str, Any]],
    label: str,
    func: Callable[[], Any],
    args: dict[str, Any] | None = None,
) -> Any:
    entry: dict[str, Any] = {"label": label}
    if args is not None:
        entry["args"] = args
    try:
        value = func()
        entry["ok"] = True
        entry["return"] = _value_summary(value)
        operations.append(entry)
        return value
    except BaseException as exc:
        entry["ok"] = False
        entry["exception"] = {
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback_tail": "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))[-2000:],
        }
        operations.append(entry)
        return None


def _case_status(operations: list[dict[str, Any]]) -> str:
    if all(op.get("ok") for op in operations):
        return "stable_native_payload"
    return "stable_native_diagnostic"


def _case_payload(
    case_id: str,
    version: dict[str, Any],
    ctx: dict[str, Any],
    operations: list[dict[str, Any]],
) -> dict[str, Any]:
    meta = CASE_META[case_id]
    status = _case_status(operations)
    classification = status
    if any(
        "NCollection_Sequence::ChangeValue" in str((op.get("exception") or {}).get("message", ""))
        for op in operations
    ):
        classification = "native_instability_blocker"
    return {
        "case_id": case_id,
        "scope_ids": meta["scope_ids"],
        "methods": meta["methods"],
        "description": meta["description"],
        "status": status,
        "classification": classification,
        "can_enter_s3": True,
        "can_enter_s4": False,
        "method_sequence": [op["label"] for op in operations],
        "input_summary": ctx.get("input_summary", {}),
        "operations": operations,
        "diagnostics": [op["exception"] for op in operations if not op.get("ok")],
        "freecad_version": version.get("freecad_version"),
        "occt_version": version.get("occt_version") or version.get("config_occt_version"),
        "s4_rule": "S2 stable payload is evidence only; S3 gate is still required.",
    }


def _inside_case(case_id: str, FreeCAD: Any, Part: Any) -> dict[str, Any]:
    version = _inside_version(FreeCAD, Part)
    if case_id == "__version__":
        return {
            "case_id": case_id,
            "status": "stable_native_payload",
            "classification": "expected_ready",
            "can_enter_s3": True,
            "can_enter_s4": False,
            "version": version,
        }

    ctx = _make_context(FreeCAD, Part)
    helper = ctx["helper"]
    profile = ctx["profile"]
    profile_alt = ctx["profile_alt"]
    profile_unknown = ctx["profile_unknown"]
    operations: list[dict[str, Any]] = []

    if case_id == "baseline_subset":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "isReady()", helper.isReady)
        _record(operations, "getStatus() before build", helper.getStatus)
        _record(operations, "build()", helper.build)
        _record(operations, "getStatus() after build", helper.getStatus)
        _record(operations, "shape()", helper.shape)
        _record(operations, "makeSolid()", helper.makeSolid)
    elif case_id == "remove_before_add":
        _record(operations, "remove(profile) before add", lambda: helper.remove(profile), {"profile": "profile"})
        _record(operations, "isReady()", helper.isReady)
        _record(operations, "getStatus()", helper.getStatus)
        _record(operations, "build()", helper.build)
    elif case_id == "remove_after_add_before_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "remove(profile) before build", lambda: helper.remove(profile), {"profile": "profile"})
        _record(operations, "isReady()", helper.isReady)
        _record(operations, "getStatus()", helper.getStatus)
        _record(operations, "build()", helper.build)
    elif case_id == "remove_after_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "build()", helper.build)
        _record(operations, "shape()", helper.shape)
        _record(operations, "remove(profile) after build", lambda: helper.remove(profile), {"profile": "profile"})
        _record(operations, "isReady()", helper.isReady)
        _record(operations, "getStatus()", helper.getStatus)
    elif case_id == "remove_readd_ordering":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "remove(profile)", lambda: helper.remove(profile), {"profile": "profile"})
        _record(operations, "add(profile) again", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "isReady()", helper.isReady)
        _record(operations, "build()", helper.build)
        _record(operations, "shape()", helper.shape)
    elif case_id == "first_last_unbuilt":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "firstShape() unbuilt", helper.firstShape)
        _record(operations, "lastShape() unbuilt", helper.lastShape)
    elif case_id == "first_last_build_fail":
        _record(operations, "isReady() without add", helper.isReady)
        _record(operations, "build() without add", helper.build)
        _record(operations, "firstShape() after failed build", helper.firstShape)
        _record(operations, "lastShape() after failed build", helper.lastShape)
    elif case_id == "first_last_build_success":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "build()", helper.build)
        _record(operations, "firstShape() after build", helper.firstShape)
        _record(operations, "lastShape() after build", helper.lastShape)
    elif case_id == "generated_before_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "generated(profile) before build", lambda: helper.generated(profile), {"profile": "profile"})
    elif case_id == "generated_after_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "build()", helper.build)
        _record(operations, "generated(profile) after build", lambda: helper.generated(profile), {"profile": "profile"})
    elif case_id == "generated_unknown_profile":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "build()", helper.build)
        _record(
            operations,
            "generated(profile_unknown) after build",
            lambda: helper.generated(profile_unknown),
            {"profile": "profile_unknown"},
        )
    elif case_id == "simulate_pre_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "simulate(2) before build", lambda: helper.simulate(2), {"count": 2})
    elif case_id == "simulate_post_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "build()", helper.build)
        _record(operations, "simulate(2) after build", lambda: helper.simulate(2), {"count": 2})
    elif case_id == "simulate_unready":
        _record(operations, "simulate(2) without add", lambda: helper.simulate(2), {"count": 2})
    elif case_id == "simulate_count_zero":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "simulate(0)", lambda: helper.simulate(0), {"count": 0})
    elif case_id == "combination_remove_readd_simulate_build":
        _record(operations, "add(profile)", lambda: helper.add(profile), {"profile": "profile"})
        _record(operations, "remove(profile)", lambda: helper.remove(profile), {"profile": "profile"})
        _record(operations, "add(profile_alt)", lambda: helper.add(profile_alt), {"profile": "profile_alt"})
        _record(operations, "simulate(2) after remove/readd before build", lambda: helper.simulate(2), {"count": 2})
        _record(operations, "build()", helper.build)
        _record(operations, "shape()", helper.shape)
    else:
        raise RuntimeError(f"Unknown case id: {case_id}")

    return _case_payload(case_id, version, ctx, operations)


def _run_inside() -> int:
    case_id = os.environ.get(CASE_ENV, "__version__")
    FreeCAD, Part = _inside_imports()
    payload = _inside_case(case_id, FreeCAD, Part)
    print(MARKER.decode("ascii") + json.dumps(payload, ensure_ascii=False, sort_keys=True))
    return 0


def main() -> int:
    if os.environ.get(INSIDE_ENV) == "1":
        return _run_inside()
    return _run_host()


if os.environ.get(INSIDE_ENV) == "1":
    main()
elif globals().get("__name__") == "__main__":
    raise SystemExit(main())
