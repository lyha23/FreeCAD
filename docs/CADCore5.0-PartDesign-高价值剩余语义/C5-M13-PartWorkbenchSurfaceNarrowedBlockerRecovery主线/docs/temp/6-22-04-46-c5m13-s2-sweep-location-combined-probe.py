#!/usr/bin/env python3
"""FreeCADCmd-only probes for C5-M13 S2 Sweep location/combined blockers."""

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


def record_error(payload: dict, stage: str, exc: BaseException) -> dict:
    payload["failed_stage"] = stage
    payload["error"] = f"{type(exc).__name__}: {exc}"
    payload["traceback_tail"] = traceback.format_exc().splitlines()[-4:]
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


def auxiliary_wire():
    return Part.Wire(
        [Part.LineSegment(FreeCAD.Vector(0, 3, 0), FreeCAD.Vector(25, 3, 0)).toShape()]
    )


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


def open_wire_profile():
    return Part.Wire(
        [
            Part.LineSegment(FreeCAD.Vector(0, -1, 0), FreeCAD.Vector(0, 1, 0)).toShape(),
            Part.LineSegment(FreeCAD.Vector(0, 1, 0), FreeCAD.Vector(0, 1, 1)).toShape(),
        ]
    )


def edge_profile():
    return Part.LineSegment(FreeCAD.Vector(0, -1, 0), FreeCAD.Vector(0, 1, 0)).toShape()


def face_profile():
    return Part.Face(rectangle_profile())


def builder():
    direct = getattr(Part, "BRepOffsetAPI_MakePipeShell", None)
    if direct is None:
        direct = getattr(getattr(Part, "BRepOffsetAPI", None), "MakePipeShell", None)
    if direct is None:
        raise RuntimeError("Part module does not expose BRepOffsetAPI_MakePipeShell")
    return direct(spine_wire())


def lifecycle(maker, make_solid: bool = False) -> dict:
    payload: dict = {}
    try:
        payload["is_ready_before_build"] = bool(maker.isReady())
        payload["status_before_build"] = int(maker.getStatus())
    except Exception as exc:
        return record_error(payload, "status_before_build", exc)
    try:
        maker.build()
        payload["build_ok"] = True
    except Exception as exc:
        return record_error(payload, "build", exc)
    try:
        payload["status_after_build"] = int(maker.getStatus())
    except Exception as exc:
        return record_error(payload, "status_after_build", exc)
    if make_solid:
        try:
            payload["make_solid_ok"] = bool(maker.makeSolid())
        except Exception as exc:
            return record_error(payload, "make_solid", exc)
    try:
        result_shape = maker.shape()
        payload["shape_access_ok"] = result_shape is not None and not result_shape.isNull()
        payload["shape"] = shape_summary(result_shape)
    except Exception as exc:
        return record_error(payload, "shape", exc)
    return payload


def apply_step(maker, step: str, profile, location=None) -> None:
    if step == "transition":
        maker.setTransitionMode(1)
    elif step == "transition_round":
        maker.setTransitionMode(2)
    elif step == "frenet":
        maker.setFrenetMode(True)
    elif step == "tolerance":
        maker.setTolerance(0.0002, 0.0002, 0.01)
    elif step == "auxiliary":
        maker.setAuxiliarySpine(auxiliary_wire(), True, 0)
    elif step == "add_plain":
        maker.add(Profile=profile, WithContact=False, WithCorrection=False)
    elif step == "add_loc_false_false":
        maker.add(Profile=profile, Location=location, WithContact=False, WithCorrection=False)
    elif step == "add_loc_true_true":
        maker.add(Profile=profile, Location=location, WithContact=True, WithCorrection=True)
    elif step == "add_loc_false_true":
        maker.add(Profile=profile, Location=location, WithContact=False, WithCorrection=True)
    else:
        raise RuntimeError(f"unknown step {step}")


def run_steps(name: str, profile, location, steps: list[str]) -> dict:
    maker = builder()
    payload = {
        "case": name,
        "profile_shape_type": str(getattr(profile, "ShapeType", "")),
        "location_shape_type": str(getattr(location, "ShapeType", "")) if location is not None else "",
        "steps": steps,
    }
    for step in steps:
        try:
            apply_step(maker, step, profile, location)
        except Exception as exc:
            return record_error(payload, step, exc)
    payload["lifecycle"] = lifecycle(maker)
    return payload


def cases() -> dict[str, callable]:
    rect = rectangle_profile
    return {
        "located_free_vertex": lambda: run_steps(
            "located_free_vertex",
            rect(),
            vertex(FreeCAD.Vector(0, 0, 0)),
            ["transition", "frenet", "add_loc_false_false"],
        ),
        "located_profile_owned_vertex": lambda: (lambda p: run_steps(
            "located_profile_owned_vertex",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(rect()),
        "located_profile_coordinate_free_vertex": lambda: run_steps(
            "located_profile_coordinate_free_vertex",
            rect(),
            vertex(FreeCAD.Vector(0, -1, -1)),
            ["transition", "frenet", "add_loc_false_false"],
        ),
        "located_spine_owned_vertex": lambda: (lambda s, p: run_steps(
            "located_spine_owned_vertex",
            p,
            list(s.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(spine_wire(), rect()),
        "located_add_before_frenet": lambda: (lambda p: run_steps(
            "located_add_before_frenet",
            p,
            list(p.Vertexes)[0],
            ["transition", "add_loc_false_false", "frenet"],
        ))(rect()),
        "located_add_before_transition": lambda: (lambda p: run_steps(
            "located_add_before_transition",
            p,
            list(p.Vertexes)[0],
            ["add_loc_false_false", "transition", "frenet"],
        ))(rect()),
        "located_no_frenet": lambda: (lambda p: run_steps(
            "located_no_frenet",
            p,
            list(p.Vertexes)[0],
            ["transition", "add_loc_false_false"],
        ))(rect()),
        "located_tolerance_before_add": lambda: (lambda p: run_steps(
            "located_tolerance_before_add",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "tolerance", "add_loc_false_false"],
        ))(rect()),
        "located_open_wire_profile": lambda: (lambda p: run_steps(
            "located_open_wire_profile",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(open_wire_profile()),
        "located_edge_profile": lambda: (lambda p: run_steps(
            "located_edge_profile",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(edge_profile()),
        "located_face_profile": lambda: (lambda p: run_steps(
            "located_face_profile",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(face_profile()),
        "plain_control": lambda: (lambda p: run_steps(
            "plain_control",
            p,
            None,
            ["transition", "frenet", "add_plain"],
        ))(rect()),
        "combined_aux_tolerance_add": lambda: (lambda p: run_steps(
            "combined_aux_tolerance_add",
            p,
            list(p.Vertexes)[0],
            ["transition_round", "auxiliary", "tolerance", "add_loc_false_true"],
        ))(rect()),
        "combined_tolerance_aux_add": lambda: (lambda p: run_steps(
            "combined_tolerance_aux_add",
            p,
            list(p.Vertexes)[0],
            ["transition_round", "tolerance", "auxiliary", "add_loc_false_true"],
        ))(rect()),
        "combined_add_aux_tolerance": lambda: (lambda p: run_steps(
            "combined_add_aux_tolerance",
            p,
            list(p.Vertexes)[0],
            ["transition_round", "add_loc_false_true", "auxiliary", "tolerance"],
        ))(rect()),
        "combined_aux_add_tolerance": lambda: (lambda p: run_steps(
            "combined_aux_add_tolerance",
            p,
            list(p.Vertexes)[0],
            ["transition_round", "auxiliary", "add_loc_false_true", "tolerance"],
        ))(rect()),
        "combined_no_location_control": lambda: (lambda p: run_steps(
            "combined_no_location_control",
            p,
            None,
            ["transition_round", "auxiliary", "tolerance", "add_plain"],
        ))(rect()),
    }


def main() -> None:
    selected = os.environ.get("C5M13_S2_CASE", "all")
    registry = cases()
    names = list(registry) if selected == "all" else [selected]
    output = {
        "freecad_version": version(),
        "selected": selected,
        "results": [],
    }
    for name in names:
        try:
            output["results"].append(registry[name]())
        except Exception as exc:
            output["results"].append(record_error({"case": name}, "case", exc))
    print(json.dumps(output, indent=2, sort_keys=True))


main()
