#!/usr/bin/env python3
"""FreeCADCmd-only GeomPlate probes for C5-M13 S4."""

from __future__ import annotations

import json
import os
import traceback

import FreeCAD  # type: ignore
import Part  # type: ignore


def version() -> str:
    items = FreeCAD.Version()
    if len(items) >= 4:
        return f"{items[0]}.{items[1]}.{items[2]} revision {str(items[3]).split()[0]}"
    return " ".join(str(item) for item in items)


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
    if shape is None or shape.isNull():
        return {"is_null": True}
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
    }


def line_curve(x2: float = 4.0, y2: float = 0.0, z: float = 0.0):
    return Part.LineSegment(FreeCAD.Vector(0, 0, z), FreeCAD.Vector(x2, y2, z))


def plane_surface():
    wire = Part.makePolygon(
        [
            FreeCAD.Vector(0, 0, 0),
            FreeCAD.Vector(4, 0, 0),
            FreeCAD.Vector(4, 4, 0),
            FreeCAD.Vector(0, 4, 0),
            FreeCAD.Vector(0, 0, 0),
        ]
    )
    return Part.Face(wire).Surface


def line2d_segment(first: float = 0.0, last: float = 4.0):
    return Part.Geom2d.Line2dSegment(Part.Geom2d.Line2d(), first, last)


def run_builder(constraints, *, surface=None) -> dict:
    builder = Part.GeomPlate.BuildPlateSurface(Surface=surface) if surface is not None else Part.GeomPlate.BuildPlateSurface()
    for constraint in constraints:
        builder.add(constraint)
    builder.perform()
    result = {"is_done": bool(builder.isDone())}
    if bool(builder.isDone()):
        result["shape"] = shape_summary(builder.surface().makeApprox().toShape())
    return result


def case_g1_curve_on_surface_variants() -> dict:
    records = []
    variants = [
        ("line_z0_surface_range_0_4", line_curve(), line2d_segment(0.0, 4.0), plane_surface()),
        ("line_z0_no_initial_range_0_4", line_curve(), line2d_segment(0.0, 4.0), None),
        ("line_z1_surface_range_0_4", line_curve(z=1.0), line2d_segment(0.0, 4.0), plane_surface()),
        ("line_z0_surface_range_0_1", line_curve(), line2d_segment(0.0, 1.0), plane_surface()),
    ]
    for name, curve, curve2d, surface in variants:
        record: dict[str, object] = {"variant": name}
        try:
            constraint = Part.GeomPlate.CurveConstraint(curve, 1)
            record["constraint_order"] = int(constraint.order())
            try:
                constraint.setG1Criterion()
            except Exception as exc:
                record["set_g1_criterion_error"] = f"{type(exc).__name__}: {exc}"
            constraint.setCurve2dOnSurf(curve2d)
            record["set_curve2d_on_surf"] = "ok"
            record["builder"] = run_builder([constraint], surface=surface)
        except Exception as exc:
            record["error_type"] = type(exc).__name__
            record["error"] = str(exc)
        records.append(record)
    return {
        "classification": "native_hidden_diagnostic_only",
        "source_boundary": "Tools.cpp consumes Adaptor3d_CurveOnSurface; Python setCurve2dOnSurf exposes only a 2D curve and builder still rejects the constraint as not on a surface.",
        "variants": records,
    }


def case_projected_curve2d_variants() -> dict:
    records = []
    variants = [
        ("range_0_1_tol_0p001", line2d_segment(0.0, 1.0), 0.001, 0.001, None),
        ("range_0_4_tol_0p01", line2d_segment(0.0, 4.0), 0.01, 0.01, None),
        ("range_0_4_tol_0p1", line2d_segment(0.0, 4.0), 0.1, 0.1, None),
        ("range_0_4_tol_0p01_initial_surface", line2d_segment(0.0, 4.0), 0.01, 0.01, plane_surface()),
    ]
    for name, curve2d, tol_u, tol_v, surface in variants:
        record: dict[str, object] = {"variant": name, "tol_u": tol_u, "tol_v": tol_v}
        try:
            constraint = Part.GeomPlate.CurveConstraint(line_curve(), 0)
            constraint.setProjectedCurve(curve2d, tol_u, tol_v)
            record["set_projected_curve"] = "ok"
            record["builder"] = run_builder([constraint], surface=surface)
        except Exception as exc:
            record["error_type"] = type(exc).__name__
            record["error"] = str(exc)
        records.append(record)
    return {
        "classification": "native_runtime_blocker",
        "variants": records,
    }


def case_curve_criteria_setters() -> dict:
    constraint = Part.GeomPlate.CurveConstraint(line_curve(), 0)
    result: dict[str, str] = {}
    for name in ("setG0Criterion", "setG1Criterion", "setG2Criterion"):
        try:
            getattr(constraint, name)()
            result[name] = "ok"
        except Exception as exc:
            result[name] = f"{type(exc).__name__}: {exc}"
    return result


def case_plate_surface_curves() -> dict:
    surface = Part.PlateSurface(Curves=[line_curve()])
    return {
        "constructor": "ok",
        "surface_type": type(surface).__name__,
        "approx": shape_summary(surface.makeApprox().toShape()),
    }


CASES = {
    "geomplate_g1_curve_on_surface_variants": case_g1_curve_on_surface_variants,
    "geomplate_projected_curve2d_variants": case_projected_curve2d_variants,
    "geomplate_curve_criteria_setters": case_curve_criteria_setters,
    "geomplate_plate_surface_curves": case_plate_surface_curves,
}


def run_case(case: str) -> dict:
    payload = {
        "case": case,
        "owner": "GeomPlate",
        "freecad_version": version(),
    }
    try:
        payload["status"] = "ok"
        payload["result"] = CASES[case]()
    except BaseException as exc:
        payload["status"] = "error"
        payload["error_type"] = type(exc).__name__
        payload["error"] = str(exc)
        payload["traceback_tail"] = traceback.format_exc().splitlines()[-8:]
    return payload


def main() -> None:
    selected = os.environ.get("C5M13_S4_PROBE_CASE", "all")
    cases = list(CASES) if selected == "all" else [selected]
    records = []
    for case in cases:
        if case not in CASES:
            raise SystemExit(f"unknown C5M13_S4_PROBE_CASE={case}")
        records.append(run_case(case))
    print(json.dumps(records if len(records) > 1 else records[0], ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
