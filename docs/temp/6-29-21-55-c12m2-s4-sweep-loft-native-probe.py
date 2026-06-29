#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import traceback

import FreeCAD  # type: ignore
import Part  # type: ignore


PAYLOAD_PREFIX = "C12M2_PROBE_PAYLOAD="


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
        "volume": round(float(getattr(shape, "Volume", 0.0)), 9),
        "area": round(float(getattr(shape, "Area", 0.0)), 9),
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


def pipe_shell_builder():
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
    elif step == "binormal":
        maker.setBiNormalMode(FreeCAD.Vector(0, 0, 1))
    elif step == "tolerance":
        maker.setTolerance(0.0001, 0.0002, 0.01)
    elif step == "auxiliary":
        maker.setAuxiliarySpine(auxiliary_wire(), False, 0)
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
    maker = pipe_shell_builder()
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


def sweep_location_probe() -> dict:
    rect = rectangle_profile
    cases = [
        ("located_free_vertex", lambda: run_steps(
            "located_free_vertex",
            rect(),
            vertex(FreeCAD.Vector(0, 0, 0)),
            ["transition", "frenet", "add_loc_false_false"],
        )),
        ("located_profile_owned_vertex", lambda: (lambda p: run_steps(
            "located_profile_owned_vertex",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(rect())),
        ("located_open_wire_profile", lambda: (lambda p: run_steps(
            "located_open_wire_profile",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "add_loc_false_false"],
        ))(open_wire_profile())),
        ("located_add_before_transition", lambda: (lambda p: run_steps(
            "located_add_before_transition",
            p,
            list(p.Vertexes)[0],
            ["add_loc_false_false", "transition", "frenet"],
        ))(rect())),
        ("located_tolerance_before_add", lambda: (lambda p: run_steps(
            "located_tolerance_before_add",
            p,
            list(p.Vertexes)[0],
            ["transition", "frenet", "tolerance", "add_loc_false_false"],
        ))(rect())),
        ("plain_control", lambda: run_steps(
            "plain_control",
            rect(),
            None,
            ["transition", "frenet", "add_plain"],
        )),
    ]
    results = []
    for _, factory in cases:
        try:
            results.append(factory())
        except Exception as exc:
            results.append(record_error({"case": "case_factory"}, "case", exc))
    return {
        "probe_case": "sweep_location_overload",
        "source_authority": "src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add(Profile, Location, WithContact, WithCorrection)",
        "results": results,
        "judgement": "native_probe_blocked",
        "reason": "Location overload representatives reach ready/status 0 then fail at build; no-location control builds.",
    }


def sweep_options_probe() -> dict:
    rect = rectangle_profile
    cases = [
        ("auxiliary_no_location", lambda: run_steps(
            "auxiliary_no_location",
            rect(),
            None,
            ["transition", "frenet", "auxiliary", "add_plain"],
        )),
        ("binormal_no_location", lambda: run_steps(
            "binormal_no_location",
            rect(),
            None,
            ["transition", "binormal", "add_plain"],
        )),
        ("tolerance_no_location", lambda: run_steps(
            "tolerance_no_location",
            rect(),
            None,
            ["transition", "frenet", "tolerance", "add_plain"],
        )),
        ("combined_aux_tolerance_no_location", lambda: run_steps(
            "combined_aux_tolerance_no_location",
            rect(),
            None,
            ["transition_round", "auxiliary", "tolerance", "add_plain"],
        )),
        ("combined_aux_tolerance_with_location", lambda: (lambda p: run_steps(
            "combined_aux_tolerance_with_location",
            p,
            list(p.Vertexes)[0],
            ["transition_round", "auxiliary", "tolerance", "add_loc_false_true"],
        ))(rect())),
    ]
    results = []
    for _, factory in cases:
        try:
            results.append(factory())
        except Exception as exc:
            results.append(record_error({"case": "case_factory"}, "case", exc))
    return {
        "probe_case": "sweep_options_auxiliary_tolerance_binormal",
        "source_authority": "src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine,setBiNormalMode,setTolerance,add",
        "results": results,
        "judgement": "expected_ready_controls_current_covered",
        "reason": "No-location auxiliary/binormal/tolerance controls build and are already represented by checked-in c5m10 expected; the combined Location variant remains blocked by the Location overload.",
    }


def close_document(doc) -> None:
    try:
        FreeCAD.closeDocument(doc.Name)
    except Exception:
        pass


def make_loft_document(case_name: str):
    doc = FreeCAD.newDocument(case_name)
    lower = doc.addObject("Part::RegularPolygon", "LowerProfile")
    lower.Polygon = 4
    lower.Circumradius = 2.0
    upper = doc.addObject("Part::RegularPolygon", "UpperProfile")
    upper.Polygon = 4
    upper.Circumradius = 1.0
    upper.Placement = FreeCAD.Placement(FreeCAD.Vector(0, 0, 3.0), FreeCAD.Rotation())
    doc.recompute()
    loft = doc.addObject("Part::Loft", "Loft")
    loft.Solid = False
    loft.Ruled = True
    loft.Closed = False
    loft.MaxDegree = 2
    return doc, lower, upper, loft


def run_loft_assignment(name: str, assignment_factory) -> dict:
    doc, lower, upper, loft = make_loft_document(f"C12M2_{name}")
    payload = {"case": name}
    try:
        assignment = assignment_factory(lower, upper)
        payload["assignment_repr"] = repr(assignment)
        loft.Sections = assignment
        payload["sections_after_assignment"] = [obj.Name for obj in loft.Sections]
        doc.recompute()
        payload["recompute_ok"] = True
        payload["shape"] = shape_summary(loft.Shape)
    except Exception as exc:
        record_error(payload, "assign_or_recompute", exc)
    finally:
        close_document(doc)
    return payload


def loft_subelement_probe() -> dict:
    assignments = [
        ("object_level_control", lambda lower, upper: [lower, upper]),
        ("tuple_list_subname", lambda lower, upper: [(lower, ["Edge1"]), upper]),
        ("tuple_string_subname", lambda lower, upper: [(lower, "Edge1"), upper]),
        ("tuple_stable_subname", lambda lower, upper: [(lower, ["Edge1"], ["Edge1"]), upper]),
    ]
    results = []
    for name, factory in assignments:
        results.append(run_loft_assignment(name, factory))
    return {
        "probe_case": "loft_subelement_assignment",
        "source_authority": "src/Mod/Part/App/PartFeatures.cpp::Loft::execute reads Sections.getValues only; src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft",
        "results": results,
        "judgement": "native_hidden",
        "reason": "Object-level Sections assignment builds, but selected subelement tuple assignments are rejected by App::PropertyLinkList before recompute.",
    }


def selected_probe(name: str) -> dict:
    if name == "sweep_location":
        return sweep_location_probe()
    if name == "sweep_options":
        return sweep_options_probe()
    if name == "loft_subelement":
        return loft_subelement_probe()
    if name == "all":
        return {
            "probe_case": "all",
            "results": [
                sweep_location_probe(),
                sweep_options_probe(),
                loft_subelement_probe(),
            ],
        }
    raise RuntimeError(f"unknown probe selector {name}")


def main() -> None:
    selector = sys.argv[1] if len(sys.argv) > 1 else "all"
    payload = metadata()
    payload.update(selected_probe(selector))
    print(PAYLOAD_PREFIX + json.dumps(payload, ensure_ascii=False, sort_keys=True))


main()
