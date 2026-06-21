#!/usr/bin/env python3
"""FreeCADCmd-only probes for C5-M13 S1 narrowed surface blockers."""

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
    payload = {
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
    try:
        payload["volume"] = round(float(shape.Volume), 6)
    except Exception:
        payload["volume"] = None
    return payload


def vertex(point):
    try:
        return Part.Vertex(point)
    except Exception:
        return Part.Point(point).toShape()


def spine_wire():
    return Part.Wire(
        [Part.LineSegment(FreeCAD.Vector(0, 0, 0), FreeCAD.Vector(25, 0, 0)).toShape()]
    )


def circle_profile():
    return Part.Wire([Part.makeCircle(1.0, FreeCAD.Vector(0, 0, 0), FreeCAD.Vector(1, 0, 0))])


def rectangle_profile():
    return Part.makePolygon(
        [
            FreeCAD.Vector(0, -1, -1),
            FreeCAD.Vector(0, 1, -1),
            FreeCAD.Vector(0, 1, 1),
            FreeCAD.Vector(0, -1, 1),
            FreeCAD.Vector(0, -1, -1),
        ]
    )


def support_face():
    return Part.Face(
        Part.makePolygon(
            [
                FreeCAD.Vector(0, -5, -5),
                FreeCAD.Vector(25, -5, -5),
                FreeCAD.Vector(25, 5, 5),
                FreeCAD.Vector(0, 5, 5),
                FreeCAD.Vector(0, -5, -5),
            ]
        )
    )


def pipeshell_builder():
    direct = getattr(Part, "BRepOffsetAPI_MakePipeShell", None)
    if direct is None:
        direct = getattr(getattr(Part, "BRepOffsetAPI", None), "MakePipeShell", None)
    if direct is None:
        raise RuntimeError("Part module does not expose BRepOffsetAPI_MakePipeShell")
    builder = direct(spine_wire())
    builder.setTransitionMode(1)
    return builder


def finish_pipeshell(builder) -> dict:
    status = {
        "is_ready": bool(builder.isReady()),
        "status_before_build": int(builder.getStatus()),
    }
    builder.build()
    status["status_after_build"] = int(builder.getStatus())
    try:
        status["make_solid"] = bool(builder.makeSolid())
    except Exception as exc:
        status["make_solid_error"] = f"{type(exc).__name__}: {exc}"
    return {"builder_status": status, "shape": shape_summary(builder.shape())}


def probe_sweep_located_free_vertex() -> dict:
    builder = pipeshell_builder()
    builder.setFrenetMode(True)
    builder.add(
        Profile=circle_profile(),
        Location=vertex(FreeCAD.Vector(0, 0, 0)),
        WithContact=False,
        WithCorrection=False,
    )
    return finish_pipeshell(builder)


def probe_sweep_located_profile_vertex() -> dict:
    profile = rectangle_profile()
    builder = pipeshell_builder()
    builder.setFrenetMode(True)
    builder.add(
        Profile=profile,
        Location=list(profile.Vertexes)[0],
        WithContact=False,
        WithCorrection=False,
    )
    return finish_pipeshell(builder)


def probe_sweep_combined_profile_vertex() -> dict:
    profile = rectangle_profile()
    builder = pipeshell_builder()
    auxiliary = Part.Wire(
        [Part.LineSegment(FreeCAD.Vector(0, 3, 0), FreeCAD.Vector(25, 3, 0)).toShape()]
    )
    builder.setAuxiliarySpine(auxiliary, True, 0)
    builder.setTolerance(0.0002, 0.0002, 0.01)
    builder.add(
        Profile=profile,
        Location=list(profile.Vertexes)[0],
        WithContact=True,
        WithCorrection=True,
    )
    return finish_pipeshell(builder)


def filling_boundary_wire():
    return Part.makePolygon(
        [
            FreeCAD.Vector(0, 0, 0),
            FreeCAD.Vector(8, 0, 0),
            FreeCAD.Vector(8, 8, 0),
            FreeCAD.Vector(0, 8, 0),
            FreeCAD.Vector(0, 0, 0),
        ]
    )


def filling_nonboundary_edge():
    return Part.LineSegment(FreeCAD.Vector(2, 2, 1), FreeCAD.Vector(6, 2, 1)).toShape()


def probe_filling_default() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()]))


def probe_filling_surface_only() -> dict:
    wire = filling_boundary_wire()
    return shape_summary(Part.makeFilledFace([wire], surface=Part.Face(wire)))


def probe_filling_support_order_g1() -> dict:
    wire = filling_boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], surface=face, supports=[(edge, face)], orders=[(edge, 1)]))


def probe_filling_support_order_g2() -> dict:
    wire = filling_boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], surface=face, supports=[(edge, face)], orders=[(edge, 2)]))


def probe_filling_params_degree_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], degree=4))


def probe_filling_params_tolerance_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], tol2d=0.00001, tol3d=0.0001))


def probe_filling_params_pts_on_curve_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], ptsOnCurve=16))


def probe_filling_params_num_iter_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], numIter=4))


def probe_filling_params_anisotropy_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], anisotropy=True))


def probe_filling_params_g1_g2_tol_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], tolG1=0.02, tolG2=0.2))


def probe_filling_params_max_degree_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], maxDegree=9))


def probe_filling_params_max_segments_only() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()], maxSegments=10))


def probe_filling_params_all() -> dict:
    return shape_summary(
        Part.makeFilledFace(
            [filling_boundary_wire()],
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


def probe_filling_nonboundary_no_support_order() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire(), filling_nonboundary_edge()]))


def probe_filling_nonboundary_support_order_g1() -> dict:
    wire = filling_boundary_wire()
    face = Part.Face(wire)
    edge = filling_nonboundary_edge()
    return shape_summary(
        Part.makeFilledFace([wire, edge], surface=face, supports=[(edge, face)], orders=[(edge, 1)])
    )


def line_curve(z: float = 0.0):
    return Part.LineSegment(FreeCAD.Vector(0, 0, z), FreeCAD.Vector(5, 0, z))


def line2d():
    return Part.Geom2d.Line2dSegment(Part.Geom2d.Line2d(), 0.0, 1.0)


def probe_geomplate_g1_curve_on_surface() -> dict:
    cc = Part.GeomPlate.CurveConstraint(line_curve(), 1)
    result: dict[str, object] = {"constraint_order": int(cc.order())}
    try:
        cc.setG1Criterion()
    except Exception as exc:
        result["set_g1_criterion_error"] = f"{type(exc).__name__}: {exc}"
    try:
        cc.setCurve2dOnSurf(line2d())
        result["set_curve2d_on_surf"] = "ok"
    except Exception as exc:
        result["set_curve2d_on_surf_error"] = f"{type(exc).__name__}: {exc}"
    try:
        builder = Part.GeomPlate.BuildPlateSurface(Surface=Part.Face(rectangle_profile()).Surface)
        builder.add(cc)
        builder.perform()
        result["is_done"] = bool(builder.isDone())
        if bool(builder.isDone()):
            result["shape"] = shape_summary(builder.surface().makeApprox().toShape())
    except Exception as exc:
        result["build_error"] = f"{type(exc).__name__}: {exc}"
    result["native_hidden_note"] = (
        "Python exposes setCurve2dOnSurf(curve2d) but not the Tools.cpp "
        "Adaptor3d_CurveOnSurface construction path used by C++ helpers."
    )
    return result


def probe_geomplate_projected_curve2d() -> dict:
    cc = Part.GeomPlate.CurveConstraint(line_curve(), 0)
    result: dict[str, object] = {}
    cc.setProjectedCurve(line2d(), 0.001, 0.001)
    result["set_projected_curve"] = "ok"
    builder = Part.GeomPlate.BuildPlateSurface()
    builder.add(cc)
    builder.perform()
    result["is_done"] = bool(builder.isDone())
    if bool(builder.isDone()):
        result["shape"] = shape_summary(builder.surface().makeApprox().toShape())
    return result


def probe_geomplate_curve_criteria_boundary() -> dict:
    cc = Part.GeomPlate.CurveConstraint(line_curve(), 0)
    result: dict[str, object] = {}
    for name in ("setG0Criterion", "setG1Criterion", "setG2Criterion"):
        try:
            getattr(cc, name)()
            result[name] = "ok"
        except Exception as exc:
            result[name] = f"{type(exc).__name__}: {exc}"
    return result


def probe_geomplate_plate_surface_curves_boundary() -> dict:
    try:
        surface = Part.PlateSurface(Curves=[line_curve()])
        return {
            "constructor": "ok",
            "surface_type": type(surface).__name__,
            "approx": shape_summary(surface.makeApprox().toShape()),
        }
    except Exception as exc:
        return {"constructor_error": f"{type(exc).__name__}: {exc}"}


CASES = {
    "sweep_located_free_vertex": ("Sweep", "located profile with independent vertex", probe_sweep_located_free_vertex),
    "sweep_located_profile_vertex": ("Sweep", "located profile with vertex from profile", probe_sweep_located_profile_vertex),
    "sweep_combined_profile_vertex": (
        "Sweep",
        "auxiliary + tolerance + located profile vertex",
        probe_sweep_combined_profile_vertex,
    ),
    "filling_default": ("Filling", "default boundary control", probe_filling_default),
    "filling_surface_only": ("Filling", "initial surface", probe_filling_surface_only),
    "filling_support_order_g1": ("Filling", "surface + support/order G1", probe_filling_support_order_g1),
    "filling_support_order_g2": ("Filling", "surface + support/order G2", probe_filling_support_order_g2),
    "filling_params_degree_only": ("Filling", "non-default degree only", probe_filling_params_degree_only),
    "filling_params_tolerance_only": ("Filling", "non-default tolerances only", probe_filling_params_tolerance_only),
    "filling_params_pts_on_curve_only": (
        "Filling",
        "non-default ptsOnCurve only",
        probe_filling_params_pts_on_curve_only,
    ),
    "filling_params_num_iter_only": ("Filling", "non-default numIter only", probe_filling_params_num_iter_only),
    "filling_params_anisotropy_only": (
        "Filling",
        "non-default anisotropy only",
        probe_filling_params_anisotropy_only,
    ),
    "filling_params_g1_g2_tol_only": (
        "Filling",
        "non-default tolG1/tolG2 only",
        probe_filling_params_g1_g2_tol_only,
    ),
    "filling_params_max_degree_only": (
        "Filling",
        "non-default maxDegree only",
        probe_filling_params_max_degree_only,
    ),
    "filling_params_max_segments_only": (
        "Filling",
        "non-default maxSegments only",
        probe_filling_params_max_segments_only,
    ),
    "filling_params_all": ("Filling", "all non-default constructor params", probe_filling_params_all),
    "filling_nonboundary_no_support_order": (
        "Filling",
        "non-boundary edge without support/order control",
        probe_filling_nonboundary_no_support_order,
    ),
    "filling_nonboundary_support_order_g1": (
        "Filling",
        "non-boundary edge with support/order G1",
        probe_filling_nonboundary_support_order_g1,
    ),
    "geomplate_g1_curve_on_surface": (
        "GeomPlate",
        "G1 curve-on-surface wrapper path",
        probe_geomplate_g1_curve_on_surface,
    ),
    "geomplate_projected_curve2d": ("GeomPlate", "ProjectedCurve2d wrapper path", probe_geomplate_projected_curve2d),
    "geomplate_curve_criteria_boundary": (
        "GeomPlate",
        "curve criteria setter diagnostic boundary",
        probe_geomplate_curve_criteria_boundary,
    ),
    "geomplate_plate_surface_curves_boundary": (
        "GeomPlate",
        "PlateSurface.Curves wrapper boundary",
        probe_geomplate_plate_surface_curves_boundary,
    ),
}


def run_case(case: str) -> dict:
    owner, representative, func = CASES[case]
    payload = {
        "case": case,
        "owner": owner,
        "representative": representative,
        "freecad_version": version(),
    }
    try:
        payload["status"] = "ok"
        payload["result"] = func()
    except BaseException as exc:
        payload["status"] = "error"
        payload["error_type"] = type(exc).__name__
        payload["error"] = str(exc)
        payload["traceback_tail"] = traceback.format_exc().splitlines()[-8:]
    return payload


def main() -> None:
    selected = os.environ.get("C5M13_PROBE_CASE", "all")
    cases = list(CASES) if selected == "all" else [selected]
    records = []
    for case in cases:
        if case not in CASES:
            raise SystemExit(f"unknown C5M13_PROBE_CASE={case}")
        records.append(run_case(case))
    print(json.dumps(records if len(records) > 1 else records[0], ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
