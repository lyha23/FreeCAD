#!/usr/bin/env python3
"""FreeCADCmd-only probe for C5-M12 S1 surface native oracle collectability."""

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


def rectangle_wire(z: float = 0.0, size: float = 4.0, xoff: float = 0.0):
    half = size / 2.0
    return Part.makePolygon(
        [
            FreeCAD.Vector(xoff - half, -half, z),
            FreeCAD.Vector(xoff + half, -half, z),
            FreeCAD.Vector(xoff + half, half, z),
            FreeCAD.Vector(xoff - half, half, z),
            FreeCAD.Vector(xoff - half, -half, z),
        ]
    )


def support_face():
    return Part.Face(
        Part.makePolygon(
            [
                FreeCAD.Vector(0, -5, 0),
                FreeCAD.Vector(30, -5, 0),
                FreeCAD.Vector(30, 5, 0),
                FreeCAD.Vector(0, 5, 0),
                FreeCAD.Vector(0, -5, 0),
            ]
        )
    )


def pipeshell_builder():
    spine = Part.Wire(
        [Part.LineSegment(FreeCAD.Vector(0, 0, 0), FreeCAD.Vector(25, 0, 0)).toShape()]
    )
    profile = Part.Wire([Part.makeCircle(1.0, FreeCAD.Vector(0, 0, 0), FreeCAD.Vector(1, 0, 0))])
    direct = getattr(Part, "BRepOffsetAPI_MakePipeShell", None)
    if direct is None:
        direct = getattr(getattr(Part, "BRepOffsetAPI", None), "MakePipeShell", None)
    if direct is None:
        raise RuntimeError("Part module does not expose BRepOffsetAPI_MakePipeShell")
    builder = direct(spine)
    builder.setTransitionMode(1)
    return builder, profile


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
    result = builder.shape()
    return {"builder_status": status, "shape": shape_summary(result)}


def probe_sweep_support() -> dict:
    builder, profile = pipeshell_builder()
    ok = bool(builder.setSpineSupport(support_face()))
    builder.add(Profile=profile, WithContact=False, WithCorrection=False)
    result = finish_pipeshell(builder)
    result["set_spine_support"] = ok
    return result


def probe_sweep_located() -> dict:
    builder, profile = pipeshell_builder()
    builder.setFrenetMode(True)
    builder.add(
        Profile=profile,
        Location=vertex(FreeCAD.Vector(0, 0, 0)),
        WithContact=False,
        WithCorrection=False,
    )
    return finish_pipeshell(builder)


def probe_sweep_combined() -> dict:
    builder, profile = pipeshell_builder()
    auxiliary = Part.Wire(
        [Part.LineSegment(FreeCAD.Vector(0, 3, 0), FreeCAD.Vector(25, 3, 0)).toShape()]
    )
    builder.setAuxiliarySpine(auxiliary, True, 0)
    builder.setTolerance(0.0002, 0.0002, 0.01)
    builder.add(
        Profile=profile,
        Location=vertex(FreeCAD.Vector(0, 0, 0)),
        WithContact=True,
        WithCorrection=True,
    )
    return finish_pipeshell(builder)


def add_shape(doc, name: str, shape):
    obj = doc.addObject("Part::Feature", name)
    obj.Shape = shape
    return obj


def add_sketch(doc, name: str, z: float):
    sketch = doc.addObject("Sketcher::SketchObject", name)
    sketch.Placement = FreeCAD.Placement(FreeCAD.Vector(0, 0, z), FreeCAD.Rotation())
    pts = [
        FreeCAD.Vector(-2, -2, 0),
        FreeCAD.Vector(2, -2, 0),
        FreeCAD.Vector(2, 2, 0),
        FreeCAD.Vector(-2, 2, 0),
    ]
    for start, end in zip(pts, pts[1:] + pts[:1]):
        sketch.addGeometry(Part.LineSegment(start, end), False)
    return sketch


def loft_for_sections(doc, sections: list) -> dict:
    loft = doc.addObject("Part::Loft", "Loft")
    loft.Sections = sections
    loft.Solid = False
    loft.Ruled = False
    loft.Closed = False
    loft.MaxDegree = 5
    doc.recompute()
    return shape_summary(loft.Shape)


def probe_loft_profiles() -> dict:
    wire_doc = FreeCAD.newDocument("C5M12LoftWire")
    try:
        wire_result = loft_for_sections(
            wire_doc,
            [
                add_shape(wire_doc, "Wire0", rectangle_wire(0)),
                add_shape(wire_doc, "Wire1", rectangle_wire(8)),
            ],
        )
    finally:
        FreeCAD.closeDocument(wire_doc.Name)

    face_doc = FreeCAD.newDocument("C5M12LoftFace")
    try:
        face_result = loft_for_sections(
            face_doc,
            [
                add_shape(face_doc, "Face0", Part.Face(rectangle_wire(0))),
                add_shape(face_doc, "Face1", Part.Face(rectangle_wire(8))),
            ],
        )
    finally:
        FreeCAD.closeDocument(face_doc.Name)

    vertex_doc = FreeCAD.newDocument("C5M12LoftVertex")
    try:
        vertex_result = loft_for_sections(
            vertex_doc,
            [
                add_shape(vertex_doc, "Vertex0", vertex(FreeCAD.Vector(0, 0, 0))),
                add_shape(vertex_doc, "Wire1", rectangle_wire(8)),
            ],
        )
    finally:
        FreeCAD.closeDocument(vertex_doc.Name)

    sketch_doc = FreeCAD.newDocument("C5M12LoftSketch")
    try:
        sk0 = add_sketch(sketch_doc, "Sketch0", 0)
        sk1 = add_sketch(sketch_doc, "Sketch1", 8)
        sketch_result = loft_for_sections(sketch_doc, [sk0, sk1])
        subelement_error = None
        try:
            loft = sketch_doc.addObject("Part::Loft", "LoftSubelement")
            loft.Sections = [(sk0, ["Edge1"]), (sk1, ["Edge1"])]
        except Exception as exc:
            subelement_error = f"{type(exc).__name__}: {exc}"
    finally:
        FreeCAD.closeDocument(sketch_doc.Name)

    return {
        "wire_profile": wire_result,
        "face_profile": face_result,
        "vertex_profile": vertex_result,
        "sketch_object_profile": sketch_result,
        "sketch_subelement_assignment": {
            "status": "native_hidden" if subelement_error else "accepted",
            "error": subelement_error,
            "note": "Part::Loft.Sections is a PropertyLinkList in PartFeatures.cpp and has no subname storage path.",
        },
    }


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


def probe_filling() -> dict:
    result: dict[str, dict] = {}
    for key, func in (
        ("default_boundary", probe_filling_default),
        ("non_boundary_edge_no_support", probe_filling_nonboundary),
    ):
        result[key] = func()
    result["hazardous_cases"] = (
        "Run filling_params, filling_surface_only, filling_support_order and "
        "filling_nonboundary_support_order as separate FreeCADCmd processes; "
        "these can terminate the native process before Python can emit JSON."
    )
    return result


def probe_filling_default() -> dict:
    return shape_summary(Part.makeFilledFace([filling_boundary_wire()]))


def probe_filling_params() -> dict:
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


def probe_filling_nonboundary() -> dict:
    wire = filling_boundary_wire()
    non_boundary = Part.LineSegment(FreeCAD.Vector(2, 2, 1), FreeCAD.Vector(6, 2, 1)).toShape()
    return shape_summary(Part.makeFilledFace([wire, non_boundary]))


def probe_filling_surface_only() -> dict:
    wire = filling_boundary_wire()
    return shape_summary(Part.makeFilledFace([wire], surface=Part.Face(wire)))


def probe_filling_support_order() -> dict:
    wire = filling_boundary_wire()
    face = Part.Face(wire)
    edge = list(wire.Edges)[0]
    return shape_summary(Part.makeFilledFace([wire], surface=face, supports=[(edge, face)], orders=[(edge, 1)]))


def probe_filling_nonboundary_support_order() -> dict:
    wire = filling_boundary_wire()
    face = Part.Face(wire)
    non_boundary = Part.LineSegment(FreeCAD.Vector(2, 2, 1), FreeCAD.Vector(6, 2, 1)).toShape()
    return shape_summary(
        Part.makeFilledFace(
            [wire, non_boundary],
            surface=face,
            supports=[(non_boundary, face)],
            orders=[(non_boundary, 1)],
        )
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
        builder = Part.GeomPlate.BuildPlateSurface(Surface=Part.Face(rectangle_wire(0)).Surface)
        builder.add(cc)
        builder.perform()
        result["is_done"] = bool(builder.isDone())
        if bool(builder.isDone()):
            result["shape"] = shape_summary(builder.surface().makeApprox().toShape())
    except Exception as exc:
        result["build_error"] = f"{type(exc).__name__}: {exc}"
    result["native_hidden_note"] = (
        "Python exposes setCurve2dOnSurf(curve2d) but not the Tools.cpp "
        "Adaptor3d_CurveOnSurface construction path needed by the native oracle."
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


CASES = {
    "sweep_support": ("Sweep", "SpineSupport/SupportMode", probe_sweep_support),
    "sweep_located": ("Sweep", "add(Profile, Location, WithContact, WithCorrection)", probe_sweep_located),
    "sweep_combined": ("Sweep", "AuxiliarySpine + tolerance + located profile", probe_sweep_combined),
    "loft_profiles": ("Loft", "wire/face/vertex/sketch object and sketch subelement", probe_loft_profiles),
    "filling_helper": ("Filling", "safe surface helper subset", probe_filling),
    "filling_default": ("Filling", "default boundary helper", probe_filling_default),
    "filling_params": ("Filling", "non-default params helper", probe_filling_params),
    "filling_nonboundary": ("Filling", "non-boundary edge helper without support/order", probe_filling_nonboundary),
    "filling_surface_only": ("Filling", "initial surface helper", probe_filling_surface_only),
    "filling_support_order": ("Filling", "support/order helper", probe_filling_support_order),
    "filling_nonboundary_support_order": (
        "Filling",
        "non-boundary support/order helper",
        probe_filling_nonboundary_support_order,
    ),
    "geomplate_g1_curve_on_surface": ("GeomPlate", "G1 curve-on-surface wrapper path", probe_geomplate_g1_curve_on_surface),
    "geomplate_projected_curve2d": ("GeomPlate", "ProjectedCurve2d wrapper path", probe_geomplate_projected_curve2d),
}

CRASH_PRONE_CASES = {
    "filling_params",
    "filling_surface_only",
    "filling_nonboundary_support_order",
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
    selected = os.environ.get("C5M12_PROBE_CASE", "all")
    if selected == "all":
        cases = [case for case in CASES if case not in CRASH_PRONE_CASES]
    else:
        cases = [selected]
    records = []
    for case in cases:
        if case not in CASES:
            raise SystemExit(f"unknown C5M12_PROBE_CASE={case}")
        records.append(run_case(case))
    print(json.dumps(records if len(records) > 1 else records[0], ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
