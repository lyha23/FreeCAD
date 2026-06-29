#!/usr/bin/env python3
"""C11-M2 S3 native Part.makeFilledFace helper probe.

Run from the repository root with normal Python. The host process launches one
FreeCADCmd process per case using the documented exec(compile(open(...))) style
so native crashes are recorded per case instead of aborting the whole probe.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
import traceback
from pathlib import Path
from typing import Any


OUTPUT_JSON = Path("docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json")
VERSION_TXT = Path("docs/temp/6-29-12-27-c11m2-s3-freecadcmd-version.txt")
MARKER = b"__C11M2_S3_JSON__"
TIMEOUT_SECONDS = 45


CASE_ORDER = [
    "helper_surface_initial_face",
    "helper_support_order_boundary_g1",
    "helper_support_order_boundary_g2",
    "helper_params_pts_on_curve",
    "helper_params_anisotropy",
    "helper_params_tol_g1_g2",
    "helper_params_max_segments",
    "helper_params_all",
    "helper_nonboundary_support_order_g1",
    "helper_nonboundary_support_order_g2",
    "control_default_boundary",
    "control_params_degree",
    "control_params_num_iter",
    "control_params_tol2d_tol3d",
    "control_params_max_degree",
    "control_nonboundary_no_support_order",
    "wrapper_load_init_surface",
    "wrapper_add_support_order_g1",
    "wrapper_add_support_order_g2",
    "wrapper_nonboundary_support_order_g1",
    "wrapper_set_params_all",
    "wrapper_constructor_all_params",
]


CASE_META = {
    "helper_surface_initial_face": ("C11M2-SCOPE-101", "helper", "Surface initial face"),
    "helper_support_order_boundary_g1": ("C11M2-SCOPE-102", "helper", "boundary support/order G1"),
    "helper_support_order_boundary_g2": ("C11M2-SCOPE-102", "helper", "boundary support/order G2"),
    "helper_params_pts_on_curve": ("C11M2-SCOPE-201", "helper", "PtsOnCurve"),
    "helper_params_anisotropy": ("C11M2-SCOPE-201", "helper", "Anisotropy"),
    "helper_params_tol_g1_g2": ("C11M2-SCOPE-201", "helper", "TolG1/TolG2"),
    "helper_params_max_segments": ("C11M2-SCOPE-201", "helper", "MaxSegments"),
    "helper_params_all": ("C11M2-SCOPE-201", "helper", "all explicit params"),
    "helper_nonboundary_support_order_g1": ("C11M2-SCOPE-202", "helper", "non-boundary support/order G1"),
    "helper_nonboundary_support_order_g2": ("C11M2-SCOPE-202", "helper", "non-boundary support/order G2"),
    "control_default_boundary": ("C11M2-SCOPE-203", "control", "default helper boundary control"),
    "control_params_degree": ("C11M2-SCOPE-203", "control", "Degree helper control"),
    "control_params_num_iter": ("C11M2-SCOPE-203", "control", "NumIter helper control"),
    "control_params_tol2d_tol3d": ("C11M2-SCOPE-203", "control", "Tol2d/Tol3d helper control"),
    "control_params_max_degree": ("C11M2-SCOPE-203", "control", "MaxDegree helper control"),
    "control_nonboundary_no_support_order": (
        "C11M2-SCOPE-203",
        "control",
        "non-boundary no support/order helper control",
    ),
    "wrapper_load_init_surface": ("C11M2-SCOPE-203", "direct_wrapper_control", "LoadInitSurface"),
    "wrapper_add_support_order_g1": ("C11M2-SCOPE-203", "direct_wrapper_control", "Add edge/support/order G1"),
    "wrapper_add_support_order_g2": ("C11M2-SCOPE-203", "direct_wrapper_control", "Add edge/support/order G2"),
    "wrapper_nonboundary_support_order_g1": (
        "C11M2-SCOPE-203",
        "direct_wrapper_control",
        "Add non-boundary edge/support/order G1",
    ),
    "wrapper_set_params_all": ("C11M2-SCOPE-203", "direct_wrapper_control", "Set*Param controls"),
    "wrapper_constructor_all_params": (
        "C11M2-SCOPE-203",
        "direct_wrapper_control",
        "constructor explicit params control",
    ),
}


REQUIRED_HELPER_CASES = {
    "C11M2-SCOPE-101": ["helper_surface_initial_face"],
    "C11M2-SCOPE-102": ["helper_support_order_boundary_g1", "helper_support_order_boundary_g2"],
    "C11M2-SCOPE-201": [
        "helper_params_pts_on_curve",
        "helper_params_anisotropy",
        "helper_params_tol_g1_g2",
        "helper_params_max_segments",
        "helper_params_all",
    ],
    "C11M2-SCOPE-202": [
        "helper_nonboundary_support_order_g1",
        "helper_nonboundary_support_order_g2",
    ],
}


def _decode(blob: bytes) -> str:
    return blob.decode("utf-8", errors="replace")


def _tail(blob: bytes, max_chars: int = 4000) -> str:
    text = _decode(blob)
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


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
    return (
        shutil.which("FreeCADCmd")
        or shutil.which("freecadcmd")
        or shutil.which("freecadcmd-daily")
    )


def _command(script_path: Path) -> str:
    return "exec(compile(open({!r}, encoding='utf-8').read(), 'c11m2_s3_filling_native_helper_probe.py', 'exec'))".format(
        str(script_path)
    )


def _parse_payload(stdout: bytes) -> dict[str, Any] | None:
    for line in stdout.splitlines():
        if line.startswith(MARKER):
            return json.loads(_decode(line[len(MARKER) :]))
    return None


def _process_failure(case_id: str, completed: subprocess.CompletedProcess[bytes] | None, reason: str) -> dict[str, Any]:
    scope_id, case_kind, description = CASE_META[case_id]
    returncode = completed.returncode if completed is not None else None
    stdout = completed.stdout if completed is not None else b""
    stderr = completed.stderr if completed is not None else b""
    return {
        "case_id": case_id,
        "scope_id": scope_id,
        "case_kind": case_kind,
        "description": description,
        "status": "notCollected",
        "can_enter_s4": False,
        "failure": {
            "stage": "freecadcmd_process",
            "error_type": reason,
            "error_message": f"FreeCADCmd did not return a stable payload for {case_id}",
            "returncode": returncode,
            "shell_exit": _shell_exit(returncode),
            "stdout_tail": _tail(stdout),
            "stderr_tail": _tail(stderr),
        },
    }


def _run_freecad_case(
    freecadcmd: str,
    script_path: Path,
    case_id: str,
    version_fallback: dict[str, Any] | None,
) -> dict[str, Any]:
    env = os.environ.copy()
    env["C11M2_S3_INSIDE_FREECAD"] = "1"
    env["C11M2_S3_CASE"] = case_id
    cmd = [freecadcmd, "-c", _command(script_path)]
    try:
        completed = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as exc:
        scope_id, case_kind, description = CASE_META[case_id]
        return {
            "case_id": case_id,
            "scope_id": scope_id,
            "case_kind": case_kind,
            "description": description,
            "status": "notCollected",
            "can_enter_s4": False,
            "failure": {
                "stage": "freecadcmd_process",
                "error_type": "timeout",
                "error_message": f"FreeCADCmd timed out after {TIMEOUT_SECONDS}s",
                "returncode": None,
                "shell_exit": None,
                "timeout_seconds": TIMEOUT_SECONDS,
                "stdout_tail": _tail(exc.stdout or b""),
                "stderr_tail": _tail(exc.stderr or b""),
            },
            "freecad_version": (version_fallback or {}).get("freecad_version"),
            "occt_version": (version_fallback or {}).get("occt_version"),
        }

    payload = _parse_payload(completed.stdout)
    if payload is None:
        payload = _process_failure(case_id, completed, "missing_probe_payload")
    payload["freecadcmd_command"] = " ".join(cmd)
    payload["returncode"] = completed.returncode
    payload["shell_exit"] = _shell_exit(completed.returncode)
    if completed.returncode != 0 and payload.get("status") not in {"notCollected", "blocked_by_environment"}:
        payload["status"] = "notCollected"
        payload["can_enter_s4"] = False
        payload["failure"] = {
            "stage": "freecadcmd_process",
            "error_type": "nonzero_exit_after_payload",
            "error_message": f"FreeCADCmd exited with {completed.returncode}",
            "returncode": completed.returncode,
            "shell_exit": _shell_exit(completed.returncode),
            "stdout_tail": _tail(completed.stdout),
            "stderr_tail": _tail(completed.stderr),
        }
    if "freecad_version" not in payload:
        payload["freecad_version"] = (version_fallback or {}).get("freecad_version")
    if "occt_version" not in payload:
        payload["occt_version"] = (version_fallback or {}).get("occt_version")
    return payload


def _scope_status(cases: dict[str, dict[str, Any]], scope_id: str) -> str:
    if scope_id == "C11M2-SCOPE-203":
        if all(cases[c].get("status") == "blocked_by_environment" for c in cases if CASE_META[c][0] == scope_id):
            return "blocked_by_environment"
        return "dependency_retained"
    required = REQUIRED_HELPER_CASES[scope_id]
    statuses = [cases[case_id].get("status") for case_id in required]
    if any(status == "blocked_by_environment" for status in statuses):
        return "blocked_by_environment"
    if all(status == "stable_native_expected" for status in statuses):
        return "stable_native_expected"
    return "notCollected"


def _scope_summary(cases: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    summary: dict[str, dict[str, Any]] = {}
    for scope_id, required in REQUIRED_HELPER_CASES.items():
        status = _scope_status(cases, scope_id)
        stable = [case_id for case_id in required if cases[case_id].get("status") == "stable_native_expected"]
        failed = [case_id for case_id in required if cases[case_id].get("status") != "stable_native_expected"]
        summary[scope_id] = {
            "status": status,
            "stable_cases": stable,
            "not_collected_cases": failed,
            "can_enter_s4": status == "stable_native_expected",
            "s4_rule": "Only stable_native_expected helper cases may enter S4; failures remain diagnostic and cannot become backend_gap.",
        }
    control_cases = [case_id for case_id in CASE_ORDER if CASE_META[case_id][0] == "C11M2-SCOPE-203"]
    summary["C11M2-SCOPE-203"] = {
        "status": _scope_status(cases, "C11M2-SCOPE-203"),
        "stable_cases": [case_id for case_id in control_cases if cases[case_id].get("shape_summary")],
        "not_collected_cases": [
            case_id for case_id in control_cases if cases[case_id].get("status") not in {"diagnostic_control_stable", "stable_control"}
        ],
        "can_enter_s4": False,
        "s4_rule": "Direct wrapper controls are diagnostic only and are not request-local Part.makeFilledFace expected.",
    }
    return summary


def _write_version_txt(freecadcmd: str, version_payload: dict[str, Any] | None, version_case: dict[str, Any]) -> None:
    lines = [
        f"FreeCADCmd={freecadcmd}",
        f"command={version_case.get('freecadcmd_command', '')}",
        f"returncode={version_case.get('returncode')}",
        f"shell_exit={version_case.get('shell_exit')}",
    ]
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
            "env_FREECAD_LIBPACK",
            "app_home_path",
        ]:
            lines.append(f"{key}={version_payload.get(key)}")
    else:
        lines.append("version_probe_status=notCollected")
        lines.append(json.dumps(version_case.get("failure", {}), ensure_ascii=False, sort_keys=True))
    VERSION_TXT.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _run_host() -> int:
    script_path = Path(__file__).resolve()
    freecadcmd = _freecadcmd()
    if freecadcmd is None:
        cases = {
            case_id: {
                "case_id": case_id,
                "scope_id": CASE_META[case_id][0],
                "case_kind": CASE_META[case_id][1],
                "description": CASE_META[case_id][2],
                "status": "blocked_by_environment",
                "can_enter_s4": False,
                "failure": {
                    "stage": "freecadcmd_lookup",
                    "error_type": "command_not_found",
                    "error_message": "FreeCADCmd/freecadcmd/freecadcmd-daily was not found in PATH",
                },
            }
            for case_id in CASE_ORDER
        }
        payload = {
            "schema_version": 1,
            "generated_at_unix": int(time.time()),
            "freecadcmd": None,
            "execution_model": "blocked before FreeCADCmd execution",
            "scope_classification": _scope_summary(cases),
            "cases": cases,
        }
        OUTPUT_JSON.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        VERSION_TXT.write_text("FreeCADCmd=not found\n", encoding="utf-8")
        return 2

    version_case = _run_freecad_case(freecadcmd, script_path, "__version__", None)
    version_payload = version_case.get("version") if isinstance(version_case.get("version"), dict) else None
    _write_version_txt(freecadcmd, version_payload, version_case)

    cases: dict[str, dict[str, Any]] = {}
    for case_id in CASE_ORDER:
        cases[case_id] = _run_freecad_case(freecadcmd, script_path, case_id, version_payload)

    payload = {
        "schema_version": 1,
        "generated_at_unix": int(time.time()),
        "freecadcmd": freecadcmd,
        "execution_model": "one FreeCADCmd process per case using -c exec(compile(open(...).read(), ...))",
        "version_probe": version_case,
        "scope_classification": _scope_summary(cases),
        "cases": cases,
    }
    OUTPUT_JSON.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


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
        "env_FREECAD_LIBPACK": os.environ.get("FREECAD_LIBPACK"),
        "app_home_path": FreeCAD.getHomePath(),
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
    if shape is None or shape.isNull():
        return {"is_null": True}
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


def _boundary_wire(FreeCAD: Any, Part: Any) -> Any:
    return Part.makePolygon(
        [
            FreeCAD.Vector(0, 0, 0),
            FreeCAD.Vector(8, 0, 0),
            FreeCAD.Vector(8, 8, 0),
            FreeCAD.Vector(0, 8, 0),
            FreeCAD.Vector(0, 0, 0),
        ]
    )


def _nonboundary_edge(FreeCAD: Any, Part: Any) -> Any:
    return Part.LineSegment(FreeCAD.Vector(2, 2, 1), FreeCAD.Vector(6, 2, 1)).toShape()


def _builder(Part: Any) -> Any:
    direct = getattr(getattr(Part, "BRepOffsetAPI", None), "MakeFilling", None)
    if direct is None:
        direct = getattr(Part, "BRepOffsetAPI_MakeFilling", None)
    if direct is None:
        raise RuntimeError("Part module does not expose BRepOffsetAPI.MakeFilling")
    return direct()


def _add_boundary(builder: Any, wire: Any, face: Any | None = None, order: int = 0) -> None:
    for index, edge in enumerate(list(wire.Edges)):
        if index == 0 and face is not None:
            builder.add(edge, face, order, True)
        else:
            builder.add(edge, 0, True)


def _finish_builder(builder: Any) -> dict[str, Any]:
    result: dict[str, Any] = {}
    builder.build()
    try:
        result["is_done"] = bool(builder.isDone())
    except Exception as exc:
        result["is_done_error"] = f"{type(exc).__name__}: {exc}"
    for name in ("G0Error", "G1Error", "G2Error"):
        try:
            result[name] = float(getattr(builder, name)())
        except Exception as exc:
            result[name + "_error"] = f"{type(exc).__name__}: {exc}"
    result["shape_summary"] = _shape_summary(builder.shape())
    return result


def _case_result(case_id: str, FreeCAD: Any, Part: Any) -> dict[str, Any]:
    wire = _boundary_wire(FreeCAD, Part)
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]

    if case_id == "helper_surface_initial_face":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], surface=face))}
    if case_id == "helper_support_order_boundary_g1":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], surface=face, supports=[(edge, face)], orders=[(edge, 1)]))}
    if case_id == "helper_support_order_boundary_g2":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], surface=face, supports=[(edge, face)], orders=[(edge, 2)]))}
    if case_id == "helper_params_pts_on_curve":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], ptsOnCurve=16))}
    if case_id == "helper_params_anisotropy":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], anisotropy=True))}
    if case_id == "helper_params_tol_g1_g2":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], tolG1=0.02, tolG2=0.2))}
    if case_id == "helper_params_max_segments":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], maxSegments=10))}
    if case_id == "helper_params_all":
        return {
            "shape_summary": _shape_summary(
                Part.makeFilledFace(
                    [wire],
                    degree=4,
                    ptsOnCurve=16,
                    numIter=4,
                    anisotropy=True,
                    tol2d=0.00001,
                    tol3d=0.0001,
                    tolG1=0.02,
                    tolG2=0.2,
                    maxDegree=9,
                    maxSegments=10,
                )
            )
        }
    if case_id == "helper_nonboundary_support_order_g1":
        nb_edge = _nonboundary_edge(FreeCAD, Part)
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire, nb_edge], surface=face, supports=[(nb_edge, face)], orders=[(nb_edge, 1)]))}
    if case_id == "helper_nonboundary_support_order_g2":
        nb_edge = _nonboundary_edge(FreeCAD, Part)
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire, nb_edge], surface=face, supports=[(nb_edge, face)], orders=[(nb_edge, 2)]))}
    if case_id == "control_default_boundary":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire]))}
    if case_id == "control_params_degree":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], degree=4))}
    if case_id == "control_params_num_iter":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], numIter=4))}
    if case_id == "control_params_tol2d_tol3d":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], tol2d=0.00001, tol3d=0.0001))}
    if case_id == "control_params_max_degree":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire], maxDegree=9))}
    if case_id == "control_nonboundary_no_support_order":
        return {"shape_summary": _shape_summary(Part.makeFilledFace([wire, _nonboundary_edge(FreeCAD, Part)]))}
    if case_id == "wrapper_load_init_surface":
        builder = _builder(Part)
        builder.loadInitSurface(face)
        _add_boundary(builder, wire)
        return _finish_builder(builder)
    if case_id == "wrapper_add_support_order_g1":
        builder = _builder(Part)
        _add_boundary(builder, wire, face, 1)
        return _finish_builder(builder)
    if case_id == "wrapper_add_support_order_g2":
        builder = _builder(Part)
        _add_boundary(builder, wire, face, 2)
        return _finish_builder(builder)
    if case_id == "wrapper_nonboundary_support_order_g1":
        builder = _builder(Part)
        _add_boundary(builder, wire)
        builder.add(_nonboundary_edge(FreeCAD, Part), face, 1, False)
        return _finish_builder(builder)
    if case_id == "wrapper_set_params_all":
        builder = _builder(Part)
        builder.setResolParam(Degree=4, NbPtsOnCur=16, NbIter=4, Anisotropy=True)
        builder.setConstrParam(Tol2d=0.00001, Tol3d=0.0001, TolAng=0.02, TolCurv=0.2)
        builder.setApproxParam(MaxDegree=9, MaxSegments=10)
        _add_boundary(builder, wire)
        return _finish_builder(builder)
    if case_id == "wrapper_constructor_all_params":
        builder_factory = getattr(getattr(Part, "BRepOffsetAPI", None), "MakeFilling", None)
        if builder_factory is None:
            raise RuntimeError("Part.BRepOffsetAPI.MakeFilling unavailable")
        builder = builder_factory(
            Degree=4,
            NbPtsOnCur=16,
            NbIter=4,
            Anisotropy=True,
            Tol2d=0.00001,
            Tol3d=0.0001,
            TolAng=0.02,
            TolCurv=0.2,
            MaxDegree=9,
            MaxSegments=10,
        )
        _add_boundary(builder, wire)
        return _finish_builder(builder)
    raise ValueError(f"unknown C11M2_S3_CASE={case_id}")


def _run_inside() -> int:
    FreeCAD, Part = _inside_imports()
    case_id = os.environ.get("C11M2_S3_CASE", "")
    version_payload = _inside_version(FreeCAD, Part)
    if case_id == "__version__":
        payload = {
            "case_id": case_id,
            "status": "stable_native_expected",
            "version": version_payload,
            "freecad_version": version_payload["freecad_version"],
            "occt_version": version_payload["occt_version"],
        }
        print(MARKER.decode("ascii") + json.dumps(payload, ensure_ascii=False, sort_keys=True), flush=True)
        return 0

    scope_id, case_kind, description = CASE_META[case_id]
    try:
        result = _case_result(case_id, FreeCAD, Part)
        if case_kind == "helper":
            status = "stable_native_expected"
            can_enter_s4 = True
        else:
            status = "diagnostic_control_stable"
            can_enter_s4 = False
        payload = {
            "case_id": case_id,
            "scope_id": scope_id,
            "case_kind": case_kind,
            "description": description,
            "status": status,
            "can_enter_s4": can_enter_s4,
            "freecad_version": version_payload["freecad_version"],
            "occt_version": version_payload["occt_version"],
            **result,
        }
    except BaseException as exc:
        payload = {
            "case_id": case_id,
            "scope_id": scope_id,
            "case_kind": case_kind,
            "description": description,
            "status": "notCollected" if case_kind == "helper" else "diagnostic_control_notCollected",
            "can_enter_s4": False,
            "freecad_version": version_payload["freecad_version"],
            "occt_version": version_payload["occt_version"],
            "failure": {
                "stage": "helper_call" if case_kind == "helper" else "direct_control_call",
                "error_type": type(exc).__name__,
                "error_message": str(exc),
                "traceback_tail": traceback.format_exc().splitlines()[-8:],
            },
        }
    print(MARKER.decode("ascii") + json.dumps(payload, ensure_ascii=False, sort_keys=True), flush=True)
    return 0


if os.environ.get("C11M2_S3_INSIDE_FREECAD") == "1":
    raise SystemExit(_run_inside())

if __name__ == "__main__":
    raise SystemExit(_run_host())
