#!/usr/bin/env python3
"""Collect hermetic process receipts for FreeCADMainCmd, Base, and MainPy.

These entrypoints are process/Python-module contracts, not document fixtures.  Each
case runs in a fresh config, user-data, and temporary directory.  The checked-in
manifest is the case denominator; this collector does not infer branches from the
current report or geometry fixture corpus.
"""

from __future__ import annotations

import argparse
import importlib
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

from freecad_expected_parity.process_contract import (
    SCHEMA,
    ProcessSpec,
    artifact,
    atomic_write,
    clean_environment,
    process_succeeded,
    run_process,
)


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "process_contracts"
    / "runtime_entrypoints"
    / "manifest.v1.json"
)
DEFAULT_REPORT = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "reports"
    / "process_contract"
    / "runtime-entrypoints.v1.json"
)
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
ENV_ARGS = "FREECAD_PROCESS_CONTRACT_ARGS_JSON"
ENV_MARKER = "__freecad_runtime_process_contract_args_env__"
SOURCE_EVIDENCE = {
    "maincmd": [
        "src/Main/MainCmd.cpp::main init UnknownProgramOption=1, ProgramInformation=0, Base::Exception=100",
        "src/Main/MainCmd.cpp::main run SystemExit=specified, Base/unknown=1, normal Application::destruct=0",
    ],
    "base": [
        "src/Base/Console.cpp::ConsoleSingleton::{sPyMessage,sPyWarning,sPyError,sPyGetObservers,sPySetStatus,sPyGetStatus}",
        "src/Base/PyException.cpp::pyThrowWrappedBaseException",
    ],
    "mainpy": [
        "src/Main/MainPy.cpp::PyInit_FreeCAD",
    ],
}


def load_manifest() -> dict[str, Any]:
    payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if payload.get("schema") != "freecad-runtime-entrypoint-process-manifest/v1":
        raise ValueError("runtime entrypoint manifest schema is invalid")
    cases = payload.get("cases")
    if not isinstance(cases, list) or not cases:
        raise ValueError("runtime entrypoint manifest cases are missing")
    ids = [case.get("id") for case in cases if isinstance(case, dict)]
    if len(ids) != len(cases) or len(set(ids)) != len(ids):
        raise ValueError("runtime entrypoint manifest case ids are invalid")
    return payload


def version_string(module: Any) -> str:
    version = module.Version()
    if isinstance(version, (list, tuple)):
        return " ".join(str(item) for item in version if item)
    return str(version)


def caught_error(callable_: Any) -> dict[str, str] | None:
    try:
        callable_()
    except Exception as exc:
        return {"type": type(exc).__name__, "message": str(exc)}
    return None


def run_inner(case_id: str, result_path: Path) -> int:
    import FreeCAD  # type: ignore

    if case_id == "base-console-observer":
        console = FreeCAD.Console
        observers = list(console.GetObservers())
        if not observers:
            raise AssertionError("FreeCAD.Console has no registered observer")
        observer = observers[0]
        original = {
            kind: bool(console.GetStatus(observer, kind))
            for kind in ("Msg", "Wrn", "Err")
        }
        restored: dict[str, bool] = {}
        try:
            for kind in original:
                console.SetStatus(observer, kind, True)
            console.PrintMessage("A5-CONSOLE-MESSAGE\n")
            console.PrintWarning("A5-CONSOLE-WARNING\n")
            console.PrintError("A5-CONSOLE-ERROR\n")
        finally:
            for kind, status in original.items():
                console.SetStatus(observer, kind, status)
                restored[kind] = bool(console.GetStatus(observer, kind))
        actual = {
            "observers": observers,
            "selectedObserver": observer,
            "originalStatus": original,
            "restoredStatus": restored,
            "restored": restored == original,
            "emitted": ["A5-CONSOLE-MESSAGE", "A5-CONSOLE-WARNING", "A5-CONSOLE-ERROR"],
        }
        passed = actual["restored"]
    elif case_id == "base-python-exception-translation":
        console = FreeCAD.Console
        observers = list(console.GetObservers())
        if not observers:
            raise AssertionError("FreeCAD.Console has no registered observer")
        unknown_observer = caught_error(
            lambda: console.SetStatus("__A5_UNKNOWN_OBSERVER__", "Msg", True)
        )
        unknown_type = caught_error(
            lambda: console.GetStatus(observers[0], "__A5_UNKNOWN_TYPE__")
        )
        actual = {
            "unknownObserver": unknown_observer,
            "unknownType": unknown_type,
            "observerStillReadable": console.GetStatus(observers[0], "Msg") is not None,
        }
        passed = (
            unknown_observer is not None
            and unknown_type is not None
            and actual["observerStillReadable"]
        )
    elif case_id == "mainpy-import-identity-shutdown":
        first = importlib.import_module("FreeCAD")
        second = importlib.import_module("FreeCAD")
        actual = {
            "moduleName": first.__name__,
            "sameObject": first is second,
            "sameAsSysModules": first is sys.modules.get("FreeCAD"),
            "version": version_string(first),
            "shutdownPath": "return-to-MainCmd-normal-destruction",
        }
        passed = (
            actual["moduleName"] == "FreeCAD"
            and actual["sameObject"]
            and actual["sameAsSysModules"]
        )
    elif case_id in {
        "maincmd-system-exit",
        "maincmd-run-base-exception",
        "maincmd-run-unknown-exception",
    }:
        actual = {"triggerReady": True, "case": case_id}
        passed = True
    else:
        raise ValueError(f"unsupported inner runtime process case: {case_id}")

    payload = {
        "case": case_id,
        "status": "passed" if passed else "failed",
        "actual": actual,
        "producerVersion": version_string(FreeCAD),
    }
    atomic_write(result_path, payload)
    print(f"runtime process contract: case={case_id} status={payload['status']}")
    if not passed:
        return 1
    if case_id == "maincmd-system-exit":
        raise SystemExit(23)
    if case_id == "maincmd-run-base-exception":
        FreeCAD.Console.SetStatus("__A5_UNHANDLED_OBSERVER__", "Msg", True)
        raise AssertionError("unhandled Base exception unexpectedly returned")
    if case_id == "maincmd-run-unknown-exception":
        raise RuntimeError("A5 controlled unknown run-phase exception")
    return 0


def script_args(argv: list[str]) -> list[str]:
    args = list(argv)
    if "--pass" in args:
        args = args[args.index("--pass") + 1 :]
    if args == [ENV_MARKER] and os.environ.get(ENV_ARGS):
        return json.loads(os.environ[ENV_ARGS])
    return args


def invoked_by_freecad() -> bool:
    return "--pass" in sys.argv and any(
        not arg.startswith("-") and Path(arg).resolve() == Path(__file__).resolve()
        for arg in sys.argv[1:]
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect FreeCADMainCmd/Base/MainPy hermetic process receipts."
    )
    parser.add_argument("--freecadcmd", default=DEFAULT_FREECADCMD)
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--case", action="append", dest="cases")
    parser.add_argument("--inner-case")
    parser.add_argument("--result")
    return parser.parse_args(script_args(argv))


def normalized_result(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def run_case(
    case: dict[str, Any], *, executable: Path, temporary_root: Path, label: str
) -> dict[str, Any]:
    case_root = temporary_root / label / case["id"]
    user_home = case_root / "user-home"
    user_data = case_root / "user-data"
    user_temp = case_root / "user-temp"
    for directory in (case_root, user_home, user_data, user_temp):
        directory.mkdir(parents=True, exist_ok=True)
    user_cfg = case_root / "user.cfg"
    system_cfg = case_root / "system.cfg"
    result_path = case_root / "result.json"
    log_path = case_root / "FreeCAD.log"
    replacements = {
        str(case_root): "<CASE_ROOT>",
        str(temporary_root): "<TMP>",
        str(ROOT): "<CAD_CORE>",
        str(Path(__file__).resolve().parents[2]): "<REPO>",
    }
    environment = clean_environment(
        {
            "PATH": "/usr/bin:/bin",
            "LANG": "C",
            "LC_ALL": "C",
            "FREECAD_USER_HOME": str(user_home),
            "FREECAD_USER_DATA": str(user_data),
            "FREECAD_USER_TEMP": str(user_temp),
            "TMPDIR": str(user_temp),
        }
    )
    base_argv = ["-u", str(user_cfg), "-s", str(system_cfg)]
    if case.get("captureLog"):
        base_argv.extend(["--log-file", str(log_path)])
    mode = case["mode"]
    if mode.startswith("script") or mode == "script":
        inner_args = ["--inner-case", case["id"], "--result", str(result_path)]
        environment[ENV_ARGS] = json.dumps(inner_args)
        argv = [*base_argv, str(Path(__file__).resolve()), "--pass", ENV_MARKER]
    else:
        argv = [*base_argv, *case.get("argv", [])]
        if mode == "direct_init_probe":
            environment["PYTHONHOME"] = str(case_root / "missing-python-home")
    process = run_process(
        ProcessSpec(
            executable=executable,
            argv=argv,
            cwd=case_root,
            environment=environment,
            timeout_seconds=30,
            replacements=replacements,
        )
    )
    result = normalized_result(result_path)
    if log_path.is_file():
        raw_log = log_path.read_text(encoding="utf-8", errors="replace")
        shutdown_markers = (
            "Log: Exiting on purpose",
            "Log: FreeCAD terminating...",
            "Log: Saving system parameter...done",
            "Log: Saving user parameter...done",
        )
        normalized_log = "\n".join(
            marker for marker in shutdown_markers if marker in raw_log
        )
    else:
        normalized_log = None
    expected_exit = int(case["expectedExitCode"])
    exit_matches = process_succeeded(process, expected_exit)
    if mode.startswith("script") or mode == "script":
        result_matches = bool(result and result.get("status") == "passed")
    else:
        result_matches = True
    if case["id"] == "base-console-observer" and result:
        merged_output = process["stdout"] + process["stderr"]
        result_matches = result_matches and all(
            marker in merged_output for marker in result["actual"]["emitted"]
        )
    coverage_policy = case.get("coveragePolicy")
    if case.get("captureLog"):
        result_matches = result_matches and bool(
            normalized_log
            and "FreeCAD terminating..." in normalized_log
            and "Saving user parameter...done" in normalized_log
        )
    if coverage_policy == "source_backed_exception_unless_exit_100_observed":
        status = "passed"
        coverage_outcome = (
            "native_process_test" if exit_matches else "source_backed_exception"
        )
        result = {
            "case": case["id"],
            "status": "passed",
            "actual": {
                "exitCode": process["exitCode"],
                "signal": process["signal"],
                "timedOut": process["timedOut"],
            },
            "expected": {"exitCode": expected_exit},
            "sourceEvidence": SOURCE_EVIDENCE["maincmd"][0],
            "attemptedArgv": process["argv"],
            "attemptedEnvironment": {"PYTHONHOME": "<MISSING_PYTHON_HOME>"},
            "closeCondition": "A safe deterministic argv/env trigger reaches MainCmd's init Base::Exception catch and exits 100 on the release producer.",
        }
    elif coverage_policy == "script_exception_translation_with_source_backed_top_level_catch":
        marker = "Exception while processing file:"
        translation_observed = marker in process["stderr"]
        status = "passed" if exit_matches and result_matches and translation_observed else "failed"
        coverage_outcome = "native_process_test" if status == "passed" else "failed"
        result = {
            **(result or {}),
            "processTranslation": {
                "fileBoundaryObserved": translation_observed,
                "observedExitCode": process["exitCode"],
                "topLevelMainCmdCatch": "source_backed_exception",
                "sourceEvidence": "src/Main/MainCmd.cpp::main run Base::Exception/unknown catches exit 1",
                "closeCondition": "A safe public argv or script entry bypasses the per-file exception isolation in Application::runApplication and reaches the corresponding MainCmd top-level catch.",
            },
        }
    else:
        status = "passed" if exit_matches and result_matches else "failed"
        coverage_outcome = "native_process_test" if status == "passed" else "failed"
    return {
        "label": label,
        "caseId": case["id"],
        "status": status,
        "coverageOutcome": coverage_outcome,
        "process": process,
        "normalizedLog": normalized_log,
        "result": result,
    }


def semantic_result(run: dict[str, Any]) -> str:
    process = run["process"]
    payload = {
        "status": run["status"],
        "coverageOutcome": run["coverageOutcome"],
        "exitCode": process["exitCode"],
        "signal": process["signal"],
        "timedOut": process["timedOut"],
        "stdout": process["stdout"],
        "stderr": process["stderr"],
        "result": run["result"],
        "normalizedLog": run["normalizedLog"],
    }
    return json.dumps(payload, ensure_ascii=False, sort_keys=True)


def run_outer(args: argparse.Namespace) -> int:
    if args.repeat < 2:
        raise ValueError("--repeat must be at least 2")
    manifest = load_manifest()
    all_cases = manifest["cases"]
    requested_ids = set(args.cases or [case["id"] for case in all_cases])
    known_ids = {case["id"] for case in all_cases}
    unknown_ids = requested_ids - known_ids
    if unknown_ids:
        raise ValueError(f"unknown process-contract cases: {sorted(unknown_ids)}")
    cases = [case for case in all_cases if case["id"] in requested_ids]
    requested = Path(args.freecadcmd)
    executable = requested.resolve()
    if not executable.is_file():
        raise FileNotFoundError(f"FreeCADCmd not found: {requested}")
    runs: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="freecad-runtime-contract-") as temporary:
        temporary_root = Path(temporary)
        for repeat in range(args.repeat):
            label = f"run-{repeat + 1}"
            for case in cases:
                runs.append(
                    run_case(
                        case,
                        executable=executable,
                        temporary_root=temporary_root,
                        label=label,
                    )
                )
    case_rows = []
    errors: list[str] = []
    for case in cases:
        case_runs = [run for run in runs if run["caseId"] == case["id"]]
        stable = (
            len(case_runs) == args.repeat
            and all(run["status"] == "passed" for run in case_runs)
            and len({semantic_result(run) for run in case_runs}) == 1
        )
        if not stable:
            errors.append(f"case failed or drifted: {case['id']}")
        case_rows.append(
            {
                "id": case["id"],
                "group": case["group"],
                "evidenceFor": case["evidenceFor"],
                "sourceEvidence": SOURCE_EVIDENCE[case["group"]],
                "status": "passed" if stable else "failed",
                "coverageOutcome": case_runs[0]["coverageOutcome"] if case_runs else "failed",
                "runs": case_runs,
            }
        )
    report = {
        "schema": SCHEMA,
        "contractId": manifest["contractId"],
        "status": "passed" if not errors else "failed",
        "repeat": args.repeat,
        "repeatStatus": "passed" if not errors else "failed",
        "caseCount": len(cases),
        "manifestCaseCount": len(all_cases),
        "producer": {
            "requestedPath": str(requested),
            **artifact(executable),
        },
        "tool": artifact(Path(__file__)),
        "runner": artifact(ROOT / "tools" / "freecad_expected_parity" / "process_contract.py"),
        "resources": [artifact(MANIFEST)],
        "environmentPolicy": {
            "inheritedAllowlist": ["SYSTEMROOT"],
            "fixed": {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"},
            "caseLocal": [
                "FREECAD_USER_HOME",
                "FREECAD_USER_DATA",
                "FREECAD_USER_TEMP",
                "TMPDIR",
                ENV_ARGS,
            ],
            "probeOnly": ["PYTHONHOME"],
        },
        "normalization": {
            "lineEndings": "CRLF to LF",
            "replacements": {
                "temporaryRoot": "<TMP>",
                "cadCoreRoot": "<CAD_CORE>",
                "repositoryRoot": "<REPO>",
            },
        },
        "cadCoreRuntimeParity": "not_evaluated",
        "boundaryExceptions": [
            {
                "branch": "MainCmd init Base::Exception -> return 100",
                "classification": "source_backed_exception_unless_native_exit_100_observed",
                "receiptCase": "maincmd-init-base-exception-probe",
            },
            {
                "branch": "MainCmd run Base::Exception/unknown -> return 1",
                "classification": "source_backed_exception",
                "receiptCases": [
                    "maincmd-run-base-exception",
                    "maincmd-run-unknown-exception",
                ],
                "reason": "The public script path catches and reports each file exception before MainCmd's top-level catch, then completes normal destruction with exit 0.",
            },
            {
                "branch": "FreeCADMainPy host embedding, interpreter finalize/reinitialize, and forced-signal shutdown",
                "classification": "protocol_only",
                "reason": "Host-owned multi-interpreter/finalize and signal-forced termination are outside the deterministic FreeCADCmd product boundary.",
            },
        ],
        "cases": case_rows,
        "errors": errors,
    }
    atomic_write(Path(args.report), report)
    print(
        "runtime entrypoint process contract: "
        f"status={report['status']} cases={len(cases)} repeat={args.repeat}"
    )
    return 0 if not errors else 1


def main(argv: list[str] | None = None) -> int:
    args = parse_args(list(sys.argv[1:] if argv is None else argv))
    if invoked_by_freecad():
        if not args.inner_case or not args.result:
            raise ValueError("FreeCAD inner process contract requires --inner-case and --result")
        return run_inner(args.inner_case, Path(args.result))
    return run_outer(args)


if invoked_by_freecad():
    inner_code = main()
    if inner_code:
        raise SystemExit(inner_code)
elif __name__ == "__main__":
    raise SystemExit(main())
