#!/usr/bin/env python3
"""FreeCADCmd probes for C5-M20 Filling precise blockers.

Run one case per FreeCADCmd process. Several cases intentionally exercise
native crash paths, so "all" is not used by the validation command.
"""

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


def boundary_wire():
    return Part.makePolygon(
        [
            FreeCAD.Vector(0, 0, 0),
            FreeCAD.Vector(8, 0, 0),
            FreeCAD.Vector(8, 8, 0),
            FreeCAD.Vector(0, 8, 0),
            FreeCAD.Vector(0, 0, 0),
        ]
    )


def support_face():
    return Part.Face(boundary_wire())


def nonboundary_edge():
    return Part.LineSegment(FreeCAD.Vector(2, 2, 1), FreeCAD.Vector(6, 2, 1)).toShape()


def probe_default_control() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()]))


def probe_surface_only() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()], surface=support_face()))


def probe_boundary_support_only() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], supports=[(edge, face)]))


def probe_boundary_order_g1_only() -> dict:
    wire = boundary_wire()
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], orders=[(edge, 1)]))


def probe_boundary_support_order_g1() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], supports=[(edge, face)], orders=[(edge, 1)]))


def probe_boundary_support_order_g2() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], supports=[(edge, face)], orders=[(edge, 2)]))


def probe_pts_on_curve_only() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()], ptsOnCurve=16))


def probe_anisotropy_only() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()], anisotropy=True))


def probe_tol_g1_g2_only() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()], tolG1=0.02, tolG2=0.2))


def probe_max_segments_only() -> dict:
    return shape_summary(Part.makeFilledFace([boundary_wire()], maxSegments=10))


def probe_all_params() -> dict:
    return shape_summary(
        Part.makeFilledFace(
            [boundary_wire()],
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


def probe_nonboundary_support_order_g1() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    edge = nonboundary_edge()
    return shape_summary(Part.makeFilledFace([wire, edge], supports=[(edge, face)], orders=[(edge, 1)]))


def probe_nonboundary_support_order_g2() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    edge = nonboundary_edge()
    return shape_summary(Part.makeFilledFace([wire, edge], supports=[(edge, face)], orders=[(edge, 2)]))


def direct_builder_result(builder) -> dict:
    builder.build()
    return {
        "is_done": bool(builder.isDone()),
        "shape": shape_summary(builder.shape()) if bool(builder.isDone()) else None,
    }


def probe_wrapper_surface_control() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    builder = Part.BRepOffsetAPI.MakeFilling()
    builder.loadInitSurface(face)
    for edge in wire.Edges:
        builder.add(edge, 0, True)
    return direct_builder_result(builder)


def probe_wrapper_support_order_g1_control() -> dict:
    wire = boundary_wire()
    face = Part.Face(wire)
    builder = Part.BRepOffsetAPI.MakeFilling()
    for index, edge in enumerate(wire.Edges):
        if index == 0:
            builder.add(edge, face, 1, True)
        else:
            builder.add(edge, 0, True)
    return direct_builder_result(builder)


CASES = {
    "default_control": ("Part.makeFilledFace default boundary", probe_default_control),
    "surface_only": ("Part.makeFilledFace surface kwarg", probe_surface_only),
    "boundary_support_only": ("Part.makeFilledFace supports kwarg", probe_boundary_support_only),
    "boundary_order_g1_only": ("Part.makeFilledFace orders G1 kwarg", probe_boundary_order_g1_only),
    "boundary_support_order_g1": ("Part.makeFilledFace supports/orders G1 kwargs", probe_boundary_support_order_g1),
    "boundary_support_order_g2": ("Part.makeFilledFace supports/orders G2 kwargs", probe_boundary_support_order_g2),
    "pts_on_curve_only": ("Part.makeFilledFace ptsOnCurve kwarg", probe_pts_on_curve_only),
    "anisotropy_only": ("Part.makeFilledFace anisotropy kwarg", probe_anisotropy_only),
    "tol_g1_g2_only": ("Part.makeFilledFace tolG1/tolG2 kwargs", probe_tol_g1_g2_only),
    "max_segments_only": ("Part.makeFilledFace maxSegments kwarg", probe_max_segments_only),
    "all_params": ("Part.makeFilledFace all explicit constructor kwargs", probe_all_params),
    "nonboundary_support_order_g1": (
        "Part.makeFilledFace non-boundary edge supports/orders G1",
        probe_nonboundary_support_order_g1,
    ),
    "nonboundary_support_order_g2": (
        "Part.makeFilledFace non-boundary edge supports/orders G2",
        probe_nonboundary_support_order_g2,
    ),
    "wrapper_surface_control": ("Part.BRepOffsetAPI.MakeFilling loadInitSurface control", probe_wrapper_surface_control),
    "wrapper_support_order_g1_control": (
        "Part.BRepOffsetAPI.MakeFilling support/order G1 control",
        probe_wrapper_support_order_g1_control,
    ),
}


def run_case(case: str) -> dict:
    representative, func = CASES[case]
    payload = {
        "case": case,
        "owner": "Filling",
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
    selected = os.environ.get("C5M20_PROBE_CASE", "default_control")
    if selected not in CASES:
        raise SystemExit(f"unknown C5M20_PROBE_CASE={selected}")
    print(json.dumps(run_case(selected), ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
