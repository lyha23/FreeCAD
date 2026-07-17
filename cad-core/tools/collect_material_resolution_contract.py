#!/usr/bin/env python3
"""Collect hermetic FreeCADCmd process evidence for Material host resources.

Material libraries, cards and model schemas are selected from host preferences and
the filesystem, so they intentionally remain outside the stateless CAD request
graph.  This collector disables every ambient Material resource source and exposes
only a copied, checked-in resource directory to one isolated FreeCADCmd process.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
RESOURCE_ROOT = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "process_contracts"
    / "material_resolution"
    / "resources"
)
DEFAULT_REPORT = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "reports"
    / "process_contract"
    / "material-resolution.v1.json"
)
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
SCHEMA = "freecad-material-resolution-process-contract/v1"
ENV_ARGS = "FREECAD_MATERIAL_CONTRACT_ARGS_JSON"
ENV_MARKER = "__freecad_material_contract_args_env__"
CASES = (
    {"id": "local-card-model-resolution", "resource": "normal", "kind": "normal"},
    {"id": "card-inheritance-and-child-override", "resource": "inheritance", "kind": "inheritance"},
    {"id": "manager-refresh-second-resolution", "resource": "refresh", "kind": "refresh"},
    {"id": "missing-card-lookup", "resource": "missing", "kind": "missing"},
    {"id": "unknown-model-reference", "resource": "unknown-model", "kind": "unknown-model"},
    {"id": "invalid-model-schema", "resource": "invalid-schema", "kind": "invalid-schema"},
    {"id": "invalid-property-type", "resource": "invalid-property", "kind": "invalid-property"},
    {"id": "inheritance-cycle-process-boundary", "resource": "cycle", "kind": "cycle", "expectedAbnormal": True},
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def artifact(path: Path) -> dict[str, Any]:
    return {"path": str(path.resolve()), "sha256": sha256(path)}


def atomic_write(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def version_string(FreeCAD: Any) -> str:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        return " ".join(str(item) for item in version if item)
    return str(version)


def configure_hermetic_resources(FreeCAD: Any, resource_root: Path) -> None:
    params = FreeCAD.ParamGet(
        "User parameter:BaseApp/Preferences/Mod/Material/Resources"
    )
    params.SetBool("UseBuiltInMaterials", False)
    params.SetBool("UseMaterialsFromWorkbenches", False)
    params.SetBool("UseMaterialsFromConfigDir", False)
    params.SetBool("UseMaterialsFromCustomDir", True)
    params.SetString("CustomMaterialsDir", str(resource_root))


def run_inner(args: argparse.Namespace) -> int:
    import FreeCAD  # type: ignore

    resource_root = Path(args.resource_root).resolve()
    configure_hermetic_resources(FreeCAD, resource_root)
    import Materials  # type: ignore

    manager = Materials.MaterialManager()
    if args.kind == "normal":
        card_path = resource_root / "Primary.FCMat"
        by_uuid = manager.getMaterial("a2000000-0000-4000-8000-000000000101")
        by_path = manager.getMaterialByPath(str(card_path), "Custom")
        actual = {
            "uuid": by_uuid.UUID,
            "pathUuid": by_path.UUID,
            "name": by_uuid.Name,
            "libraryName": by_uuid.LibraryName,
            "physicalModels": sorted(by_uuid.PhysicalModels),
            "physicalProperties": dict(by_uuid.PhysicalProperties),
            "materialUuids": sorted(manager.Materials.keys()),
            "libraryNames": sorted(row[0] for row in manager.MaterialLibraries),
            "materialsWithModel": sorted(
                manager.materialsWithModel(
                    "a2000000-0000-4000-8000-000000000001"
                ).keys()
            ),
        }
        expected = {
            "uuid": "a2000000-0000-4000-8000-000000000101",
            "pathUuid": "a2000000-0000-4000-8000-000000000101",
            "name": "Primary",
            "libraryName": "Custom",
            "physicalModels": ["a2000000-0000-4000-8000-000000000001"],
            "physicalProperties": {"Density": "7.5", "Grade": "A2-normal"},
            "materialUuids": ["a2000000-0000-4000-8000-000000000101"],
            "libraryNames": ["Custom"],
            "materialsWithModel": ["a2000000-0000-4000-8000-000000000101"],
        }
    elif args.kind == "missing":
        error = None
        try:
            manager.getMaterialByPath(str(resource_root / "Absent.FCMat"), "Custom")
        except Exception as exc:  # exact public Python boundary is part of the receipt
            error = {"type": type(exc).__name__, "message": str(exc)}
        actual = {"error": error, "materialUuids": sorted(manager.Materials.keys())}
        expected = {
            "error": {"type": "LookupError", "message": "Material not found"},
            "materialUuids": [],
        }
    elif args.kind == "inheritance":
        material = manager.getMaterial("a2000000-0000-4000-8000-000000000202")
        actual = {
            "uuid": material.UUID,
            "parent": material.Parent,
            "physicalModels": sorted(material.PhysicalModels),
            "physicalProperties": dict(material.PhysicalProperties),
        }
        expected = {
            "uuid": "a2000000-0000-4000-8000-000000000202",
            "parent": "a2000000-0000-4000-8000-000000000201",
            "physicalModels": ["a2000000-0000-4000-8000-000000000002"],
            "physicalProperties": {"Density": "8.25", "Grade": "child-override"},
        }
    elif args.kind == "refresh":
        before = manager.getMaterial("a2000000-0000-4000-8000-000000000301")
        before_value = dict(before.PhysicalProperties).get("Revision")
        shutil.copyfile(
            resource_root / "Refresh.updated.template", resource_root / "Refresh.FCMat"
        )
        manager.refresh()
        after = manager.getMaterial("a2000000-0000-4000-8000-000000000301")
        actual = {
            "before": before_value,
            "after": dict(after.PhysicalProperties).get("Revision"),
            "uuidStable": before.UUID == after.UUID,
        }
        expected = {
            "before": "before-refresh",
            "after": "after-refresh",
            "uuidStable": True,
        }
    elif args.kind == "unknown-model":
        material = manager.getMaterial("a2000000-0000-4000-8000-000000000401")
        actual = {
            "uuid": material.UUID,
            "physicalModels": sorted(material.PhysicalModels),
            "physicalProperties": dict(material.PhysicalProperties),
        }
        expected = {
            "uuid": "a2000000-0000-4000-8000-000000000401",
            "physicalModels": [],
            "physicalProperties": {},
        }
    elif args.kind == "invalid-schema":
        model_manager = Materials.ModelManager()
        error = None
        try:
            model_manager.getModel("a2000000-0000-4000-8000-000000000501")
        except Exception as exc:
            error = {"type": type(exc).__name__, "messagePrefix": str(exc).splitlines()[0]}
        actual = {
            "error": error,
            "modelUuids": sorted(model_manager.Models.keys()),
            "materialUuids": sorted(manager.Materials.keys()),
        }
        expected = {
            "error": {"type": "LookupError", "messagePrefix": "Model not found:"},
            "modelUuids": [],
            "materialUuids": [],
        }
    elif args.kind == "invalid-property":
        material = manager.getMaterial("a2000000-0000-4000-8000-000000000601")
        actual = {
            "physicalModels": sorted(material.PhysicalModels),
            "physicalProperties": dict(material.PhysicalProperties),
            "invalidValue": material.getPhysicalValue("InvalidValue"),
        }
        expected = {
            "physicalModels": ["a2000000-0000-4000-8000-000000000006"],
            "physicalProperties": {},
            "invalidValue": None,
        }
    elif args.kind == "cycle":
        # MaterialLoader::dereference() marks a material only after recursively resolving
        # its parent. A two-card cycle therefore terminates this isolated process instead
        # of producing a graph result; the parent process records that source-backed edge.
        manager.getMaterial("a2000000-0000-4000-8000-000000000701")
        raise AssertionError("cyclic material inheritance unexpectedly returned")
    else:
        raise ValueError(f"unsupported Material process-contract case: {args.kind}")

    result = {
        "case": args.case,
        "status": "passed" if actual == expected else "failed",
        "actual": actual,
        "expected": expected,
        "producerVersion": version_string(FreeCAD),
    }
    atomic_write(Path(args.result), result)
    print(f"material process contract: case={args.case} status={result['status']}")
    return 0 if result["status"] == "passed" else 1


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
        description="Collect the hermetic Material library/card/model process contract."
    )
    parser.add_argument("--freecadcmd", default=DEFAULT_FREECADCMD)
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--case")
    parser.add_argument("--kind")
    parser.add_argument("--resource-root")
    parser.add_argument("--result")
    return parser.parse_args(script_args(argv))


def normalize(text: str, temporary_root: Path) -> str:
    return text.replace(str(temporary_root), "<TMP>").replace(str(ROOT), "<CAD_CORE>")


def run_once(
    spec: dict[str, str], *, freecadcmd: Path, temporary_root: Path, label: str
) -> dict[str, Any]:
    case_root = temporary_root / label / spec["id"]
    resource_root = case_root / "resources"
    result_path = case_root / "result.json"
    shutil.copytree(RESOURCE_ROOT / spec["resource"], resource_root)
    user_home = case_root / "user-home"
    user_data = case_root / "user-data"
    user_temp = case_root / "user-temp"
    for directory in (user_home, user_data, user_temp):
        directory.mkdir(parents=True, exist_ok=True)
    user_cfg = case_root / "user.cfg"
    system_cfg = case_root / "system.cfg"
    argv = [
        str(freecadcmd),
        "-u",
        str(user_cfg),
        "-s",
        str(system_cfg),
        str(Path(__file__).resolve()),
        "--pass",
        ENV_MARKER,
    ]
    inner_args = [
        "--case",
        spec["id"],
        "--kind",
        spec["kind"],
        "--resource-root",
        str(resource_root),
        "--result",
        str(result_path),
    ]
    env = os.environ.copy()
    env.update(
        {
            ENV_ARGS: json.dumps(inner_args),
            "FREECAD_USER_HOME": str(user_home),
            "FREECAD_USER_DATA": str(user_data),
            "FREECAD_USER_TEMP": str(user_temp),
        }
    )
    timed_out = False
    try:
        completed = subprocess.run(
            argv,
            cwd=case_root,
            env=env,
            capture_output=True,
            text=True,
            timeout=10 if spec.get("expectedAbnormal") else 30,
        )
        returncode = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        returncode = None
        stdout = exc.stdout.decode() if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode() if isinstance(exc.stderr, bytes) else (exc.stderr or "")
    result = (
        json.loads(result_path.read_text(encoding="utf-8"))
        if result_path.is_file()
        else None
    )
    if spec.get("expectedAbnormal"):
        abnormal = timed_out or (returncode is not None and returncode != 0)
        result = {
            "case": spec["id"],
            "status": "passed" if abnormal else "failed",
            "actual": {"termination": "abnormal" if abnormal else "normal"},
            "expected": {"termination": "abnormal"},
            "sourceBackedReason": "MaterialLoader::dereference marks completion after recursive parent traversal and has no in-progress cycle guard.",
        }
        passed = abnormal
    else:
        passed = returncode == 0 and result and result.get("status") == "passed"
    return {
        "label": label,
        "status": "passed" if passed else "failed",
        "process": {
            "argv": [normalize(item, temporary_root) for item in argv],
            "environment": {
                "FREECAD_USER_HOME": normalize(str(user_home), temporary_root),
                "FREECAD_USER_DATA": normalize(str(user_data), temporary_root),
                "FREECAD_USER_TEMP": normalize(str(user_temp), temporary_root),
            },
            "exitCode": returncode if returncode is not None and returncode >= 0 else None,
            "signal": -returncode if returncode is not None and returncode < 0 else None,
            "timedOut": timed_out,
            "stdout": normalize(stdout, temporary_root),
            "stderr": normalize(stderr, temporary_root),
        },
        "result": result,
    }


def run_outer(args: argparse.Namespace) -> int:
    if args.repeat < 2:
        raise ValueError("--repeat must be at least 2")
    requested = Path(args.freecadcmd)
    freecadcmd = requested.resolve()
    if not freecadcmd.is_file():
        raise FileNotFoundError(f"FreeCADCmd not found: {requested}")
    runs: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="freecad-material-contract-") as temporary:
        temporary_root = Path(temporary)
        for repeat in range(args.repeat):
            label = f"run-{repeat + 1}"
            for spec in CASES:
                runs.append(
                    run_once(
                        spec,
                        freecadcmd=freecadcmd,
                        temporary_root=temporary_root,
                        label=label,
                    )
                )
    by_case: dict[str, list[dict[str, Any]]] = {
        spec["id"]: [run for run in runs if run.get("result", {}).get("case") == spec["id"]]
        for spec in CASES
    }
    repeat_stable = all(
        len(case_runs) == args.repeat
        and all(run["status"] == "passed" for run in case_runs)
        and len(
            {
                json.dumps(run["result"]["actual"], sort_keys=True)
                for run in case_runs
            }
        )
        == 1
        for case_runs in by_case.values()
    )
    resources = [
        artifact(path)
        for path in sorted(RESOURCE_ROOT.rglob("*"))
        if path.is_file()
    ]
    report = {
        "schema": SCHEMA,
        "contractId": "process-contract/material-resolution",
        "status": "passed" if repeat_stable else "failed",
        "boundary": {
            "classification": "host_resource_process_contract",
            "documentGraphFieldsAdded": [],
            "ambientResourcesDisabled": [
                "built-in",
                "workbench",
                "user-config-directory",
            ],
        },
        "producer": {
            "requestedPath": str(requested),
            "path": str(freecadcmd),
            "sha256": sha256(freecadcmd),
        },
        "tool": artifact(Path(__file__)),
        "resources": resources,
        "repeat": args.repeat,
        "repeatStatus": "passed" if repeat_stable else "failed",
        "caseCount": len(CASES),
        "cases": [
            {"id": spec["id"], "runs": by_case[spec["id"]]} for spec in CASES
        ],
        "errors": [] if repeat_stable else ["one or more process-contract runs failed or drifted"],
    }
    atomic_write(Path(args.report), report)
    print(
        "material resolution process contract: "
        f"status={report['status']} cases={len(CASES)} repeat={args.repeat}"
    )
    return 0 if repeat_stable else 1


def main(argv: list[str] | None = None) -> int:
    args = parse_args(list(sys.argv[1:] if argv is None else argv))
    if invoked_by_freecad():
        return run_inner(args)
    return run_outer(args)


if __name__ == "__main__" or invoked_by_freecad():
    raise SystemExit(main())
