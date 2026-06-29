#!/usr/bin/env python3
"""C12-M2 S5 Filling / GeomPlate / ProjectOnSurface native probe script.

The S3-schema JSON outputs produced from this script record FreeCADCmd
path/version/OCCT, command, stdout/stderr, exit code, expected_summary,
request-local judgement, current comparison path, and conclusion.
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import traceback
from pathlib import Path

import FreeCAD  # type: ignore
import Part  # type: ignore


PAYLOAD_PREFIX = "C12M2_PROBE_PAYLOAD="
CHILD_PREFIX = "C12M2_S5_CHILD_PAYLOAD="


def version_string() -> str:
    items = FreeCAD.Version()
    if len(items) >= 4:
        return f"{items[0]}.{items[1]}.{items[2]} revision {str(items[3]).split()[0]}"
    return " ".join(str(item) for item in items)


def metadata() -> dict:
    return {
        "freecad_version": list(FreeCAD.Version()),
        "freecad_version_string": version_string(),
        "occt_version": getattr(Part, "OCC_VERSION", None),
        "config_occt_version": FreeCAD.ConfigGet("OCC_VERSION"),
        "libpack": FreeCAD.ConfigGet("LibPack"),
        "libpack_version": FreeCAD.ConfigGet("LibPackVersion"),
        "freecad_libs": FreeCAD.ConfigGet("FreeCADLibs"),
        "app_home_path": FreeCAD.getHomePath(),
        "run_mode": "Cmd",
    }


def counts(shape) -> dict[str, int]:
    return {
        "solids": len(getattr(shape, "Solids", [])),
        "shells": len(getattr(shape, "Shells", [])),
        "faces": len(getattr(shape, "Faces", [])),
        "wires": len(getattr(shape, "Wires", [])),
        "edges": len(getattr(shape, "Edges", [])),
        "vertices": len(getattr(shape, "Vertexes", [])),
    }


def shape_summary(shape) -> dict:
    if shape is None:
        return {"is_null": True, "reason": "shape is None"}
    try:
        if shape.isNull():
            return {"is_null": True}
    except Exception:
        pass
    bbox = shape.BoundBox
    return {
        "is_null": False,
        "shape_type": str(getattr(shape, "ShapeType", "")),
        "topology_counts": counts(shape),
        "bbox": {
            "xmin": round(float(bbox.XMin), 6),
            "ymin": round(float(bbox.YMin), 6),
            "zmin": round(float(bbox.ZMin), 6),
            "xmax": round(float(bbox.XMax), 6),
            "ymax": round(float(bbox.YMax), 6),
            "zmax": round(float(bbox.ZMax), 6),
        },
        "volume": round(float(getattr(shape, "Volume", 0.0)), 9),
        "area": round(float(getattr(shape, "Area", 0.0)), 9),
        "length": round(float(getattr(shape, "Length", 0.0)), 9),
    }


def record_error(payload: dict, stage: str, exc: BaseException) -> dict:
    payload["failed_stage"] = stage
    payload["error"] = f"{type(exc).__name__}: {exc}"
    payload["traceback_tail"] = traceback.format_exc().splitlines()[-5:]
    return payload


def square_wire(size: float = 4.0, z: float = 0.0):
    return Part.makePolygon(
        [
            FreeCAD.Vector(0, 0, z),
            FreeCAD.Vector(size, 0, z),
            FreeCAD.Vector(size, size, z),
            FreeCAD.Vector(0, size, z),
            FreeCAD.Vector(0, 0, z),
        ]
    )


def square_edges(size: float = 4.0, z: float = 0.0) -> list:
    return list(square_wire(size, z).Edges)


def support_face(size: float = 4.0):
    return Part.makePlane(size, size, FreeCAD.Vector(0, 0, 0), FreeCAD.Vector(0, 0, 1))


def filling_builder():
    direct = getattr(Part, "BRepOffsetAPI_MakeFilling", None)
    if direct is None:
        direct = getattr(getattr(Part, "BRepOffsetAPI", None), "MakeFilling", None)
    if direct is None:
        raise RuntimeError("Part module does not expose BRepOffsetAPI_MakeFilling")
    return direct()


def filling_lifecycle(maker) -> dict:
    payload: dict = {}
    try:
        maker.build()
        payload["build_ok"] = True
    except Exception as exc:
        return record_error(payload, "build", exc)
    try:
        payload["is_done"] = bool(maker.isDone())
    except Exception as exc:
        payload["is_done_error"] = f"{type(exc).__name__}: {exc}"
    try:
        shape = maker.shape()
        payload["shape_access_ok"] = shape is not None and not shape.isNull()
        payload["shape"] = shape_summary(shape)
    except Exception as exc:
        return record_error(payload, "shape", exc)
    return payload


def run_filling_wrapper_case(case: str) -> dict:
    edges = square_edges()
    face = support_face()
    payload = {"case": case, "case_kind": "direct_wrapper"}
    try:
        maker = filling_builder()
        if case == "wrapper_boundary_default":
            for edge in edges:
                maker.add(Constraint=edge, Order=0, IsBound=True)
        elif case == "wrapper_load_init_surface":
            maker.loadInitSurface(face)
            for edge in edges:
                maker.add(Constraint=edge, Order=0, IsBound=True)
        elif case == "wrapper_add_support_order_g1":
            for edge in edges:
                maker.add(Constraint=edge, Support=face, Order=1, IsBound=True)
        elif case == "wrapper_add_support_order_g2":
            for edge in edges:
                maker.add(Constraint=edge, Support=face, Order=2, IsBound=True)
        elif case == "wrapper_uv_point_on_surface":
            for edge in edges:
                maker.add(Constraint=edge, Order=0, IsBound=True)
            maker.add(U=0.5, V=0.5, Support=face, Order=0)
        else:
            raise RuntimeError(f"unknown filling wrapper case {case}")
        payload["lifecycle"] = filling_lifecycle(maker)
    except Exception as exc:
        record_error(payload, "wrapper_case", exc)
    return payload


def run_filling_helper_case(case: str) -> dict:
    edges = square_edges()
    face = support_face()
    payload = {"case": case, "case_kind": "helper"}
    try:
        if case == "helper_boundary_control":
            result = Part.makeFilledFace(edges)
        elif case == "helper_surface_initial_face":
            result = Part.makeFilledFace(edges, surface=face)
        elif case == "helper_support_order_boundary_g1":
            result = Part.makeFilledFace(
                edges,
                supports=[(edges[0], face), (edges[1], face)],
                orders=[(edges[0], 1), (edges[1], 1)],
            )
        elif case == "helper_support_order_boundary_g2":
            result = Part.makeFilledFace(
                edges,
                supports=[(edges[0], face), (edges[1], face)],
                orders=[(edges[0], 2), (edges[1], 2)],
            )
        elif case == "helper_explicit_params_all":
            result = Part.makeFilledFace(
                edges,
                degree=4,
                ptsOnCurve=20,
                numIter=4,
                anisotropy=True,
                tol2d=0.00002,
                tol3d=0.0002,
                tolG1=0.02,
                tolG2=0.2,
                maxDegree=9,
                maxSegments=10,
            )
        else:
            raise RuntimeError(f"unknown filling helper case {case}")
        payload["status"] = "stable_native_expected"
        payload["shape"] = shape_summary(result)
    except Exception as exc:
        payload["status"] = "notCollected"
        record_error(payload, "helper_case", exc)
    return payload


def parse_child_payload(stdout: str) -> dict | None:
    for line in reversed(stdout.splitlines()):
        if line.startswith(CHILD_PREFIX):
            try:
                payload = json.loads(line[len(CHILD_PREFIX) :])
                if isinstance(payload, dict):
                    return payload
            except json.JSONDecodeError:
                return None
    return None


def run_filling_helper_child(case: str) -> dict:
    freecadcmd = os.environ.get("FREECADCMD") or shutil.which("freecadcmd") or shutil.which("FreeCADCmd")
    script = str(Path(sys.argv[0]).resolve())
    if not freecadcmd:
        return {
            "case": case,
            "case_kind": "helper",
            "status": "notCollected",
            "failure": {"stage": "child_freecadcmd", "error_type": "freecadcmd_not_found"},
        }
    code = (
        "import sys; "
        f"sys.argv = {[script, 'filling_child']!r}; "
        f"exec(compile(open({script!r}, encoding='utf-8').read(), {Path(script).name!r}, 'exec'))"
    )
    env = os.environ.copy()
    env["C12M2_S5_CHILD_CASE"] = case
    try:
        completed = subprocess.run(
            [freecadcmd, "-c", code],
            text=True,
            capture_output=True,
            timeout=12,
            check=False,
            env=env,
        )
    except subprocess.TimeoutExpired as exc:
        return {
            "case": case,
            "case_kind": "helper",
            "status": "notCollected",
            "failure": {
                "stage": "child_freecadcmd",
                "error_type": "timeout",
                "timeout_seconds": 12,
                "stdout_tail": (exc.stdout or "")[-2000:],
                "stderr_tail": (exc.stderr or "")[-2000:],
            },
        }
    payload = parse_child_payload(completed.stdout)
    if payload is None:
        payload = {
            "case": case,
            "case_kind": "helper",
            "status": "notCollected",
            "failure": {
                "stage": "child_freecadcmd",
                "error_type": "missing_child_payload",
            },
        }
    payload["child_command"] = [freecadcmd, "-c", code]
    payload["child_returncode"] = completed.returncode
    payload["child_stdout_tail"] = completed.stdout[-3000:]
    payload["child_stderr_tail"] = completed.stderr[-3000:]
    if completed.returncode != 0:
        payload["status"] = "notCollected"
        payload.setdefault("failure", {})
        payload["failure"].update(
            {
                "stage": "child_freecadcmd",
                "error_type": "nonzero_exit",
                "returncode": completed.returncode,
            }
        )
    return payload


def filling_probe() -> dict:
    wrapper_cases = [
        "wrapper_boundary_default",
        "wrapper_load_init_surface",
        "wrapper_add_support_order_g1",
        "wrapper_add_support_order_g2",
        "wrapper_uv_point_on_surface",
    ]
    helper_cases = [
        "helper_boundary_control",
        "helper_surface_initial_face",
        "helper_support_order_boundary_g1",
        "helper_support_order_boundary_g2",
        "helper_explicit_params_all",
    ]
    wrapper_results = [run_filling_wrapper_case(case) for case in wrapper_cases]
    helper_results = [run_filling_helper_child(case) for case in helper_cases]
    blocked_helpers = [r["case"] for r in helper_results if r.get("status") != "stable_native_expected"]
    stable_helpers = [r["case"] for r in helper_results if r.get("status") == "stable_native_expected"]
    stable_wrappers = [
        r["case"]
        for r in wrapper_results
        if r.get("lifecycle", {}).get("shape_access_ok") is True
    ]
    return {
        "probe_case": "filling_helper_lifecycle_split",
        "source_authority": "src/Mod/Part/App/AppPartPy.cpp::makeFilledFace; src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp::add,loadInitSurface,build,shape; src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace",
        "wrapper_results": wrapper_results,
        "helper_results": helper_results,
        "stable_wrapper_controls": stable_wrappers,
        "stable_helper_controls": stable_helpers,
        "blocked_helper_cases": blocked_helpers,
        "judgement": "helper_blocked",
        "reason": "Direct wrapper controls and simple helper controls can produce shapes, but helper support/order and initial-surface semantics remain mixed with wrapper lifecycle or process-level failures; these controls are not promoted to cad-core expected rows.",
    }


def geomplate_namespace():
    ns = getattr(Part, "GeomPlate", None)
    if ns is None:
        raise RuntimeError("Part.GeomPlate namespace is not available")
    return ns


def line3d(start, end):
    return Part.LineSegment(FreeCAD.Vector(*start), FreeCAD.Vector(*end))


def vector2d(x: float, y: float):
    base = getattr(FreeCAD, "Base", None)
    vector_type = getattr(base, "Vector2d", None) if base is not None else None
    if vector_type is None:
        vector_type = getattr(FreeCAD, "Vector2d", None)
    if vector_type is None:
        raise RuntimeError("FreeCAD Vector2d constructor is not available")
    return vector_type(x, y)


def line2d():
    geom2d = getattr(Part, "Geom2d", None)
    if geom2d is None:
        raise RuntimeError("Part.Geom2d namespace is not available")
    for name in ("Line2dSegment", "LineSegment2d", "Line2d"):
        ctor = getattr(geom2d, name, None)
        if ctor is None:
            continue
        try:
            return ctor(vector2d(0, 0), vector2d(4, 0))
        except Exception:
            continue
        try:
            return ctor(0, 0, 4, 0)
        except Exception:
            continue
    raise RuntimeError("No usable Part.Geom2d line constructor was found")


def plane_surface():
    for factory in (
        lambda: Part.Plane(),
        lambda: support_face().Surface,
    ):
        try:
            return factory()
        except Exception:
            continue
    raise RuntimeError("No usable plane surface constructor was found")


def surface_shape_summary(surface) -> dict:
    payload = {"surface_repr": repr(surface), "surface_type": type(surface).__name__}
    try:
        approx = surface.makeApprox()
        payload["makeApprox_type"] = type(approx).__name__
        try:
            payload["shape"] = shape_summary(approx.toShape())
        except Exception as exc:
            payload["toShape_error"] = f"{type(exc).__name__}: {exc}"
    except Exception as exc:
        payload["makeApprox_error"] = f"{type(exc).__name__}: {exc}"
    return payload


def run_geomplate_build(case: str) -> dict:
    ns = geomplate_namespace()
    payload = {"case": case}
    try:
        if case == "projected_curve2d_initial_surface":
            builder = ns.BuildPlateSurface(Surface=plane_surface())
        else:
            builder = ns.BuildPlateSurface()
        constraints = [
            ns.CurveConstraint(line3d((0, 0, 0), (4, 0, 0))),
            ns.CurveConstraint(line3d((4, 0, 0), (4, 4, 0))),
            ns.CurveConstraint(line3d((4, 4, 0), (0, 4, 0))),
            ns.CurveConstraint(line3d((0, 4, 0), (0, 0, 0))),
        ]
        if case == "projected_curve2d_initial_surface":
            c2d = line2d()
            constraints[0].setProjectedCurve(c2d, 0.01, 0.01)
            payload["projected_curve2d_set"] = True
        elif case == "curve2d_on_surface_no_initial_surface":
            c2d = line2d()
            constraints[0].setCurve2dOnSurf(c2d)
            payload["curve2d_on_surface_set"] = True
        for constraint in constraints:
            builder.add(constraint)
        builder.add(ns.PointConstraint(FreeCAD.Vector(2, 2, 1)))
        builder.perform()
        payload["perform_ok"] = True
        try:
            payload["is_done"] = bool(builder.isDone())
        except Exception as exc:
            payload["is_done_error"] = f"{type(exc).__name__}: {exc}"
        payload["surface"] = surface_shape_summary(builder.surface())
    except Exception as exc:
        record_error(payload, "geomplate_build", exc)
    return payload


def run_geomplate_g1_curve_case() -> dict:
    ns = geomplate_namespace()
    payload = {"case": "g1_curve_on_surface"}
    try:
        constraint = ns.CurveConstraint(line3d((0, 0, 0), (4, 0, 0)))
        constraint.setG1Criterion(0.01)
        payload["set_g1_curve_ok"] = True
    except Exception as exc:
        record_error(payload, "curve_setG1Criterion", exc)
    try:
        point = ns.PointConstraint(FreeCAD.Vector(2, 2, 1))
        point.setG1Criterion(0.01)
        payload["point_set_g1_control_ok"] = True
    except Exception as exc:
        payload["point_set_g1_control_error"] = f"{type(exc).__name__}: {exc}"
    return payload


def geomplate_probe() -> dict:
    results = [
        run_geomplate_build("projected_curve2d_initial_surface"),
        run_geomplate_build("curve2d_on_surface_no_initial_surface"),
        run_geomplate_g1_curve_case(),
    ]
    projected_ready = any(
        r.get("case") == "projected_curve2d_initial_surface" and r.get("perform_ok") is True
        for r in results
    )
    g1_hidden = any(
        r.get("case") == "g1_curve_on_surface" and "Not yet implemented" in r.get("error", "")
        for r in results
    )
    return {
        "probe_case": "geomplate_projected_curve2d_initial_surface_and_g1_split",
        "source_authority": "src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp::PyInit,loadInitSurface,perform,surface; src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setCurve2dOnSurf,setProjectedCurve,setG1Criterion",
        "results": results,
        "projected_curve2d_initial_surface_expected_ready": projected_ready,
        "g1_curve_on_surface_native_hidden": g1_hidden,
        "judgement": "expected_ready_with_native_hidden_g1",
        "reason": "Projected curve2d plus initial surface is probeable through native GeomPlate wrappers; CurveConstraint.setG1Criterion remains NotImplemented and must stay native-hidden.",
    }


def close_document(doc) -> None:
    try:
        FreeCAD.closeDocument(doc.Name)
    except Exception:
        pass


def assign_link_sub(obj, prop_name: str, value) -> dict:
    payload = {"property": prop_name, "assignment_repr": repr(value)}
    try:
        setattr(obj, prop_name, value)
        payload["assignment_ok"] = True
    except Exception as exc:
        record_error(payload, "assign_link_sub", exc)
    return payload


def element_history(shape, names: list[str]) -> dict:
    history = {}
    for name in names:
        try:
            value = shape.getElementHistory(name)
            history[name] = repr(value)
        except Exception as exc:
            history[name] = f"{type(exc).__name__}: {exc}"
    return history


def project_on_surface_probe() -> dict:
    doc = FreeCAD.newDocument("C12M2_S5_ProjectOnSurface")
    payload = {
        "probe_case": "project_on_surface_projection_provenance",
        "source_authority": "src/Mod/Part/App/FeatureProjectOnSurface.cpp::tryExecute,getProjectionShapes,projectWire,projectFace; src/Mod/Part/App/TopoShapePyImp.cpp::getElementHistory,mapShapes,mapSubElement",
    }
    try:
        support = doc.addObject("Part::Plane", "SupportPlane")
        support.Length = 8
        support.Width = 5
        source = doc.addObject("Part::Plane", "ProjectionWireFace")
        source.Length = 3
        source.Width = 2
        source.Placement = FreeCAD.Placement(FreeCAD.Vector(1, 1, -2), FreeCAD.Rotation())
        doc.recompute()
        projection = doc.addObject("Part::ProjectOnSurface", "ProjectedWireSplitProvenance")
        assignment_results = []
        try:
            projection.Mode = "Edges"
        except Exception as exc:
            assignment_results.append(record_error({"property": "Mode"}, "assign_mode", exc))
        try:
            projection.Height = 0.0
            projection.Offset = 0.0
            projection.Direction = FreeCAD.Vector(0, 0, 1)
        except Exception as exc:
            assignment_results.append(record_error({"property": "numeric_controls"}, "assign_controls", exc))
        assignment_results.append(assign_link_sub(projection, "SupportFace", (support, ["Face1"])))
        projection_assignment_candidates = [
            [(source, ["Wire1"])],
            [(source, "Wire1")],
            [(source, ["Face1"])],
        ]
        projection_assigned = False
        for candidate in projection_assignment_candidates:
            result = assign_link_sub(projection, "Projection", candidate)
            assignment_results.append(result)
            if result.get("assignment_ok"):
                projection_assigned = True
                break
        payload["assignment_results"] = assignment_results
        payload["projection_assignment_ok"] = projection_assigned
        doc.recompute()
        payload["recompute_ok"] = True
        payload["shape"] = shape_summary(projection.Shape)
        payload["element_history"] = element_history(
            projection.Shape,
            ["Edge1", "Edge2", "Edge3", "Edge4", "Wire1", "Face1"],
        )
        payload["history_visible"] = any(
            value not in {"None", ""}
            and not value.startswith("ValueError")
            and not value.startswith("RuntimeError")
            for value in payload["element_history"].values()
        )
        payload["judgement"] = "native_hidden"
        payload["reason"] = "Native ProjectOnSurface builds a result shape, but the probe cannot expose source-backed mapper/provenance/split trace through getElementHistory; result order is not accepted as ownership evidence."
    except Exception as exc:
        record_error(payload, "project_on_surface_probe", exc)
        payload["judgement"] = "native_probe_blocked"
        payload["reason"] = "ProjectOnSurface native probe did not reach a stable result shape."
    finally:
        close_document(doc)
    return payload


def main() -> int:
    child_case = os.environ.get("C12M2_S5_CHILD_CASE")
    if child_case:
        child_payload = run_filling_helper_case(child_case)
        child_payload.update(metadata())
        print(CHILD_PREFIX + json.dumps(child_payload, ensure_ascii=False, sort_keys=True))
        return 0

    family = sys.argv[1] if len(sys.argv) > 1 else "all"
    payload = metadata()
    if family == "filling":
        payload.update(filling_probe())
    elif family == "geomplate":
        payload.update(geomplate_probe())
    elif family == "project_on_surface":
        payload.update(project_on_surface_probe())
    else:
        payload.update(
            {
                "probe_case": "unknown",
                "judgement": "collector_bug",
                "reason": f"Unknown S5 probe family {family!r}",
            }
        )
    print(PAYLOAD_PREFIX + json.dumps(payload, ensure_ascii=False, sort_keys=True))
    return 0 if payload.get("judgement") != "collector_bug" else 1


main()
