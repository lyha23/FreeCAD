#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FREECADCMD = os.environ.get("FREECADCMD") or shutil.which("freecadcmd") or "freecadcmd"
SCHEMA_VERSION = "cad-core.freecad-expected.v1"
C8M1_SCHEMA_VERSION = "cad-core.c8m1-shapebinder-native-oracle.v1"
ENV_ARG_MARKER = "__cad_core_c8m1_shapebinder_expected_args_env__"
ENV_ARG_NAME = "CAD_CORE_C8M1_SHAPEBINDER_EXPECTED_ARGS_JSON"


class OracleBlocked(RuntimeError):
    def __init__(self, kind: str, reason: str, evidence: dict[str, Any] | None = None) -> None:
        super().__init__(reason)
        self.kind = kind
        self.reason = reason
        self.evidence = evidence or {}


def script_args(argv: list[str]) -> list[str]:
    args = list(argv)
    if "--pass" in args:
        args = args[args.index("--pass") + 1 :]
    if args == [ENV_ARG_MARKER] and os.environ.get(ENV_ARG_NAME):
        return json.loads(os.environ[ENV_ARG_NAME])
    if args and args[0] == "--":
        return args[1:]
    return args


def invoked_by_freecad_cli_import() -> bool:
    if "--pass" not in sys.argv:
        return False
    script_path = Path(__file__).resolve()
    for arg in sys.argv[1:]:
        if arg.startswith("-"):
            continue
        try:
            if Path(arg).resolve() == script_path:
                return True
        except OSError:
            continue
    return False


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect C8-M1 ShapeBinder/SubShapeBinder expected JSON from native FreeCAD.",
    )
    parser.add_argument("fixture", nargs="?", help="Fixture JSON file. Omit when --phase is used.")
    parser.add_argument("--phase", default=None, help="Collect fixtures from cad-core/fixtures/<phase>.")
    parser.add_argument("--fixtures-root", default=str(ROOT / "fixtures"))
    parser.add_argument("--out", help="Output expected file for a single fixture.")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--freecadcmd", default=DEFAULT_FREECADCMD)
    return parser.parse_args(script_args(argv))


def freecad_version(FreeCAD: Any) -> str:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        if len(version) >= 4:
            revision = str(version[3]).split()[0]
            return f"{version[0]}.{version[1]}.{version[2]} revision {revision}"
        return " ".join(str(item) for item in version if item)
    return str(version)


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def fixture_paths(args: argparse.Namespace) -> list[Path]:
    fixtures_root = Path(args.fixtures_root)
    if args.phase:
        if args.fixture:
            raise ValueError("--phase and fixture path are mutually exclusive")
        return sorted(path for path in (fixtures_root / args.phase).glob("*.json") if path.is_file())
    if not args.fixture:
        raise ValueError("fixture path or --phase is required")
    return [Path(args.fixture)]


def expected_path_for_fixture(fixtures_root: Path, fixture_path: Path) -> Path:
    phase = fixture_path.parent.name
    return fixtures_root / phase / "expected" / f"{fixture_path.stem}.freecad.json"


def load_fixture(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def placement_payload(placement: Any) -> dict[str, Any]:
    base = placement.Base
    rotation = placement.Rotation
    return {
        "Base": [float(base.x), float(base.y), float(base.z)],
        "Rotation": [float(rotation.Q[0]), float(rotation.Q[1]), float(rotation.Q[2]), float(rotation.Q[3])],
    }


def bbox_payload(shape: Any) -> dict[str, list[float]]:
    try:
        bbox = shape.optimalBoundingBox()
    except Exception:
        bbox = shape.BoundBox
    return {
        "min": [float(bbox.XMin), float(bbox.YMin), float(bbox.ZMin)],
        "max": [float(bbox.XMax), float(bbox.YMax), float(bbox.ZMax)],
    }


def element_map_payload(shape: Any) -> dict[str, Any]:
    size = int(getattr(shape, "ElementMapSize", 0))
    payload: dict[str, Any] = {
        "ElementMapSize": size,
        "ElementMapVersion": str(getattr(shape, "ElementMapVersion", "")),
    }
    try:
        element_map = dict(getattr(shape, "ElementMap", {}))
    except Exception as exc:
        payload["ElementMap"] = {"status": "unavailable", "error": str(exc)}
        return payload
    payload["ElementMap"] = {str(key): str(value) for key, value in sorted(element_map.items())}
    return payload


def shape_summary(shape: Any, include_children: bool = True) -> dict[str, Any]:
    if shape is None:
        return {"status": "missing_shape"}
    try:
        if shape.isNull():
            return {"status": "null_shape"}
    except Exception:
        pass
    summary: dict[str, Any] = {
        "status": "ok",
        "shape_type": str(getattr(shape, "ShapeType", "")),
        "bbox": bbox_payload(shape),
        "volume": float(getattr(shape, "Volume", 0.0)),
        "area": float(getattr(shape, "Area", 0.0)),
        "length": float(getattr(shape, "Length", 0.0)),
        "topology_counts": {
            "solids": len(getattr(shape, "Solids", [])),
            "shells": len(getattr(shape, "Shells", [])),
            "faces": len(getattr(shape, "Faces", [])),
            "wires": len(getattr(shape, "Wires", [])),
            "edges": len(getattr(shape, "Edges", [])),
            "vertices": len(getattr(shape, "Vertexes", [])),
        },
        "element_map": element_map_payload(shape),
    }
    if include_children:
        try:
            children = list(shape.childShapes())
        except Exception:
            children = []
        summary["childShapes"] = [
            shape_summary(child, include_children=False) for child in children
        ]
    return summary


def support_snapshot(obj: Any) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    try:
        raw_entries = list(getattr(obj, "Support", []))
    except Exception:
        raw_entries = []
    for item in raw_entries:
        if isinstance(item, tuple) and len(item) == 2:
            target, subs = item
            entries.append({
                "object": str(getattr(target, "Name", target)),
                "label": str(getattr(target, "Label", "")),
                "subnames": [str(sub) for sub in subs],
            })
        else:
            entries.append({
                "object": str(getattr(item, "Name", item)),
                "label": str(getattr(item, "Label", "")),
                "subnames": [],
            })
    return entries


def object_payload(obj: Any, role: str | None = None, include_shape: bool = True) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "name": str(getattr(obj, "Name", "")),
        "label": str(getattr(obj, "Label", "")),
        "type_id": str(getattr(obj, "TypeId", "")),
        "placement": placement_payload(getattr(obj, "Placement")),
        "InList": [str(getattr(item, "Name", "")) for item in getattr(obj, "InList", [])],
        "OutList": [str(getattr(item, "Name", "")) for item in getattr(obj, "OutList", [])],
    }
    if role:
        payload["role"] = role
    if hasattr(obj, "Support"):
        payload["Support"] = support_snapshot(obj)
    for name in ("TraceSupport", "Relative", "MakeFace", "Fuse", "Refine", "Offset", "BindMode", "BindCopyOnChange", "PartialLoad"):
        if hasattr(obj, name):
            try:
                payload[name] = str(getattr(obj, name)) if name in {"BindMode", "BindCopyOnChange"} else getattr(obj, name)
            except Exception:
                pass
    if include_shape and hasattr(obj, "Shape"):
        payload["shape_summary"] = shape_summary(getattr(obj, "Shape"))
    return payload


def base_payload(fixture_path: Path, fixture: dict[str, Any], FreeCAD: Any, reference: str) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "case_schema_version": C8M1_SCHEMA_VERSION,
        "reference": reference,
        "freecad_version": freecad_version(FreeCAD),
        "source_fixture": str(fixture_path),
        "source_fixture_name": fixture_path.name,
        "oracle_ids": fixture.get("oracle_ids", []),
        "native_case": fixture.get("c8m1_case"),
        "fixture_group": "c8m1",
        "freecad_authority": fixture.get("freecad_authority", []),
        "route": "native_oracle_collected",
    }


def make_box(doc: Any, name: str, length: float = 10, width: float = 10, height: float = 10, placement: Any = None) -> Any:
    box = doc.addObject("Part::Box", name)
    box.Length = length
    box.Width = width
    box.Height = height
    if placement is not None:
        box.Placement = placement
    return box


def make_rect_sketch(doc: Any, name: str, x0: float = 0, y0: float = 0, x1: float = 10, y1: float = 5) -> Any:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    sketch = doc.addObject("Sketcher::SketchObject", name)
    geometry = [
        Part.LineSegment(FreeCAD.Vector(x0, y0, 0), FreeCAD.Vector(x1, y0, 0)),
        Part.LineSegment(FreeCAD.Vector(x1, y0, 0), FreeCAD.Vector(x1, y1, 0)),
        Part.LineSegment(FreeCAD.Vector(x1, y1, 0), FreeCAD.Vector(x0, y1, 0)),
        Part.LineSegment(FreeCAD.Vector(x0, y1, 0), FreeCAD.Vector(x0, y0, 0)),
    ]
    sketch.addGeometry(geometry, False)
    return sketch


def collect_shape_binder_whole(doc: Any) -> dict[str, Any]:
    source_part = doc.addObject("App::Part", "SourcePart")
    box = make_box(doc, "SupportBox", 2, 3, 4)
    source_part.addObject(box)
    target_body = doc.addObject("PartDesign::Body", "TargetBody")
    binder = doc.addObject("PartDesign::ShapeBinder", "ShapeBinder")
    binder.Support = [box]
    target_body.addObject(binder)
    doc.recompute()
    return {
        "object": "ShapeBinder",
        "objects": {
            "SupportBox": object_payload(box, "source_support"),
            "ShapeBinder": object_payload(binder, "shape_binder_whole"),
            "TargetBody": object_payload(target_body, "body_consumer"),
        },
    }


def collect_shape_binder_subshapes(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "SupportBox", 3, 4, 5)
    face = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderFace")
    face.Support = [(box, "Face1")]
    edge = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderEdge")
    edge.Support = [(box, "Edge1")]
    vertex = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderVertex")
    vertex.Support = [(box, "Vertex1")]
    multi = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderMulti")
    multi.Support = [(box, "Face1"), (box, "Edge1"), (box, "Vertex1")]
    doc.recompute()
    return {
        "objects": {
            "ShapeBinderFace": object_payload(face, "selected_face"),
            "ShapeBinderEdge": object_payload(edge, "selected_edge"),
            "ShapeBinderVertex": object_payload(vertex, "selected_vertex"),
            "ShapeBinderMulti": object_payload(multi, "multi_subshape_compound"),
        },
        "compound_child_order": ["Face1", "Edge1", "Vertex1"],
    }


def collect_shape_binder_trace_support(doc: Any) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    source_part = doc.addObject("App::Part", "SourcePart")
    source_part.Placement.Base = FreeCAD.Vector(10, 0, 0)
    box = make_box(doc, "SupportBox", 2, 3, 4)
    source_part.addObject(box)
    target_part = doc.addObject("App::Part", "TargetPart")
    target_part.Placement.Base = FreeCAD.Vector(0, 20, 0)
    no_trace = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderTraceFalse")
    no_trace.Support = [box]
    no_trace.TraceSupport = False
    trace = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderTraceTrue")
    trace.Support = [box]
    trace.TraceSupport = True
    target_part.addObject(no_trace)
    target_part.addObject(trace)
    doc.recompute()
    return {
        "objects": {
            "SourcePart": object_payload(source_part, "source_container", include_shape=False),
            "TargetPart": object_payload(target_part, "target_container", include_shape=False),
            "ShapeBinderTraceFalse": object_payload(no_trace, "trace_support_false"),
            "ShapeBinderTraceTrue": object_payload(trace, "trace_support_true"),
        },
        "transform_evidence": {
            "source_container_base": [10.0, 0.0, 0.0],
            "target_container_base": [0.0, 20.0, 0.0],
            "freecad_formula": "targetCS.inverse() * sourceCS",
        },
    }


def collect_shape_binder_datum_fallback(doc: Any) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    line = doc.addObject("App::Line", "AppLine")
    line.Placement.Base = FreeCAD.Vector(1, 2, 3)
    plane = doc.addObject("App::Plane", "AppPlane")
    plane.Placement.Base = FreeCAD.Vector(4, 5, 6)
    point = doc.addObject("App::Point", "AppPoint")
    point.Placement.Base = FreeCAD.Vector(7, 8, 9)
    line_binder = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderLine")
    line_binder.Support = [line]
    plane_binder = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderPlane")
    plane_binder.Support = [plane]
    point_binder = doc.addObject("PartDesign::ShapeBinder", "ShapeBinderPoint")
    point_binder.Support = [point]
    doc.recompute()
    return {
        "objects": {
            "ShapeBinderLine": object_payload(line_binder, "app_line_edge_fallback"),
            "ShapeBinderPlane": object_payload(plane_binder, "app_plane_face_fallback"),
            "ShapeBinderPoint": object_payload(point_binder, "app_point_vertex_fallback"),
        },
    }


def collect_subshape_binder_basic(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "SupportBox", 3, 4, 5)
    whole = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderWhole")
    whole.Support = [box]
    face = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderFace")
    face.Support = [(box, ["Face1"])]
    edges = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderEdgeList")
    edges.Support = [(box, ["Edge1", "Edge2", "Edge3", "Edge4"])]
    doc.recompute()
    return {
        "objects": {
            "SubShapeBinderWhole": object_payload(whole, "whole_support"),
            "SubShapeBinderFace": object_payload(face, "face_support"),
            "SubShapeBinderEdgeList": object_payload(edges, "edge_list_support"),
        },
    }


def collect_subshape_binder_geometry_ops(doc: Any) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    sketch = make_rect_sketch(doc, "RectangleSketch", 0, 0, 10, 5)
    makeface = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderMakeFace")
    makeface.Support = [sketch]
    makeface.MakeFace = True
    offset = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderOffset")
    offset.Support = [(sketch, ["Edge1", "Edge2", "Edge3", "Edge4"])]
    offset.Offset = 1.0
    box_a = make_box(doc, "FuseBoxA", 4, 4, 4)
    box_b = make_box(
        doc,
        "FuseBoxB",
        4,
        4,
        4,
        FreeCAD.Placement(FreeCAD.Vector(2, 0, 0), FreeCAD.Rotation()),
    )
    fuse = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderFuse")
    fuse.Support = [box_a, box_b]
    fuse.Fuse = True
    refine = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderRefine")
    refine.Support = [box_a, box_b]
    refine.Fuse = True
    refine.Refine = True
    doc.recompute()
    return {
        "objects": {
            "SubShapeBinderMakeFace": object_payload(makeface, "make_face"),
            "SubShapeBinderOffset": object_payload(offset, "offset_2d"),
            "SubShapeBinderFuse": object_payload(fuse, "fuse_solids"),
            "SubShapeBinderRefine": object_payload(refine, "fuse_then_refine"),
        },
    }


def collect_subshape_binder_setlinks(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "SupportBox", 2, 2, 2)
    empty = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderEmptySubList")
    empty.Support = [(box, [])]
    diagnostics: dict[str, Any] = {}
    try:
        cycle = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderCycle")
        cycle.Support = [cycle]
        diagnostics["cycle_rejection"] = {"status": "unexpected_ok", "support": support_snapshot(cycle)}
    except Exception as exc:
        diagnostics["cycle_rejection"] = {
            "status": "error",
            "error_type": type(exc).__name__,
            "message": str(exc),
        }
    doc.recompute()
    return {
        "objects": {
            "SubShapeBinderEmptySubList": object_payload(empty, "empty_sublist_whole_selection"),
        },
        "diagnostics": diagnostics,
        "diagnostic_codes": ["cycle_rejected_by_property_link"] if diagnostics.get("cycle_rejection", {}).get("status") == "error" else [],
    }


def collect_subshape_binder_relative_nested(doc: Any) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    source_part = doc.addObject("App::Part", "SourcePart")
    source_part.Placement.Base = FreeCAD.Vector(5, 0, 0)
    box = make_box(doc, "NestedBox", 2, 3, 4)
    source_part.addObject(box)
    target_part = doc.addObject("App::Part", "TargetPart")
    target_part.Placement.Base = FreeCAD.Vector(0, 7, 0)
    nested = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderNested")
    nested.Support = [(source_part, ["NestedBox.Face1"])]
    target_part.addObject(nested)
    relative = doc.addObject("PartDesign::SubShapeBinder", "SubShapeBinderRelative")
    relative.Relative = True
    relative.Support = [(source_part, ["NestedBox.Face1"])]
    target_part.addObject(relative)
    doc.recompute()
    route_evidence: dict[str, Any] = {}
    for subname in ("NestedBox.Face1", "NestedBox.Edge1", "$NestedBox.Face1"):
        try:
            sobj = nested.getSubObject(subname)
            route_evidence[subname] = {
                "status": "ok",
                "python_type": type(sobj).__name__,
                "repr": repr(sobj),
            }
        except Exception as exc:
            route_evidence[subname] = {
                "status": "error",
                "error_type": type(exc).__name__,
                "message": str(exc),
            }
    return {
        "objects": {
            "SourcePart": object_payload(source_part, "source_container", include_shape=False),
            "TargetPart": object_payload(target_part, "target_container", include_shape=False),
            "SubShapeBinderNested": object_payload(nested, "nested_get_subobject_route"),
            "SubShapeBinderRelative": object_payload(relative, "relative_context_route"),
        },
        "nested_route_evidence": route_evidence,
    }


def collect_subshape_binder_profile_consumer(doc: Any) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    body = doc.addObject("PartDesign::Body", "Body")
    sketch = make_rect_sketch(doc, "Sketch", 10, 10, 30, 15)
    body.addObject(sketch)
    binder = body.newObject("PartDesign::SubShapeBinder", "BinderProfile")
    binder.Support = [sketch]
    revolution = body.newObject("PartDesign::Revolution", "Revolution")
    revolution.Profile = (binder, [""])
    revolution.ReferenceAxis = (doc.getObject("Y_Axis"), [""])
    revolution.Angle = 360.0
    revolution.Reversed = True
    doc.recompute()
    revolution.Angle2 = 60.0
    doc.recompute()
    return {
        "objects": {
            "BinderProfile": object_payload(binder, "subshape_binder_as_profile"),
            "Revolution": object_payload(revolution, "downstream_profile_consumer"),
            "Body": object_payload(body, "body_replay_consumer"),
        },
        "profile_consumer_evidence": {
            "binder_area": float(binder.Shape.Area),
            "revolution_volume": float(revolution.Shape.Volume),
        },
    }


def collect_binder_element_map(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "Box", 10, 10, 10)
    box001 = make_box(doc, "Box001", 10, 10, 10)
    fusion = doc.addObject("Part::MultiFuse", "Fusion")
    fusion.Refine = False
    fusion.Shapes = [box, box001]
    doc.recompute()
    shape_binder = doc.addObject("PartDesign::ShapeBinder", "ShapeBinder")
    shape_binder.Support = [fusion]
    body = doc.addObject("PartDesign::Body", "Body")
    subshape_binder = body.newObject("PartDesign::SubShapeBinder", "SubShapeBinder")
    subshape_binder.Support = [(fusion, [""])]
    base_body = doc.addObject("PartDesign::Body", "BodyBaseFeature")
    base_body.BaseFeature = fusion
    doc.recompute()
    return {
        "objects": {
            "Fusion": object_payload(fusion, "source_element_map"),
            "ShapeBinder": object_payload(shape_binder, "shape_binder_element_map"),
            "SubShapeBinder": object_payload(subshape_binder, "subshape_binder_element_map"),
            "Body": object_payload(body, "body_with_subshape_binder"),
            "BodyBaseFeature": object_payload(base_body, "base_feature_control"),
        },
        "element_map_evidence": {
            "fusion_element_map_size": int(fusion.Shape.ElementMapSize),
            "shape_binder_child_element_map_sizes": [
                int(getattr(child, "ElementMapSize", 0)) for child in shape_binder.Shape.childShapes()
            ],
            "subshape_binder_child_element_map_sizes": [
                int(getattr(child, "ElementMapSize", 0)) for child in subshape_binder.Shape.childShapes()
            ],
            "body_shape_element_map_size": int(body.Shape.ElementMapSize),
            "base_feature_body_shape_element_map_size": int(base_body.Shape.ElementMapSize),
        },
    }


def collect_bindmode_lifecycle(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "SupportBox", 10, 10, 10)
    sync = doc.addObject("PartDesign::SubShapeBinder", "BindModeSynchronized")
    sync.Support = [box]
    sync.BindMode = "Synchronized"
    frozen = doc.addObject("PartDesign::SubShapeBinder", "BindModeFrozen")
    frozen.Support = [box]
    frozen.BindMode = "Frozen"
    detached = doc.addObject("PartDesign::SubShapeBinder", "BindModeDetached")
    detached.BindMode = "Detached"
    detached.Support = [box]
    doc.recompute()
    before = {
        "Synchronized": shape_summary(sync.Shape),
        "Frozen": shape_summary(frozen.Shape),
        "Detached": shape_summary(detached.Shape),
        "DetachedSupport": support_snapshot(detached),
    }
    box.Length = 20
    doc.recompute()
    after = {
        "Synchronized": shape_summary(sync.Shape),
        "Frozen": shape_summary(frozen.Shape),
        "Detached": shape_summary(detached.Shape),
        "DetachedSupport": support_snapshot(detached),
    }
    return {
        "objects": {
            "BindModeSynchronized": object_payload(sync, "bindmode_synchronized"),
            "BindModeFrozen": object_payload(frozen, "bindmode_frozen"),
            "BindModeDetached": object_payload(detached, "bindmode_detached"),
        },
        "lifecycle_evidence": {
            "before_source_change": before,
            "after_source_length_change": after,
            "source_change": "SupportBox.Length 10 -> 20",
        },
    }


def collect_copy_on_change_lifecycle(doc: Any) -> dict[str, Any]:
    box = make_box(doc, "SupportBox", 10, 10, 10)
    disabled = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeDisabled")
    disabled.Support = [box]
    disabled.BindCopyOnChange = "Disabled"
    enabled = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeEnabled")
    enabled.Support = [box]
    enabled.BindCopyOnChange = "Enabled"
    partial = doc.addObject("PartDesign::SubShapeBinder", "PartialLoadEnabled")
    partial.Support = [box]
    partial.PartialLoad = True
    mutated = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeMutated")
    mutated.Support = [box]
    mutated.BindCopyOnChange = "Mutated"
    doc.recompute()
    return {
        "objects": {
            "CopyOnChangeDisabled": object_payload(disabled, "copy_on_change_disabled"),
            "CopyOnChangeEnabled": object_payload(enabled, "copy_on_change_enabled"),
            "CopyOnChangeMutated": object_payload(mutated, "copy_on_change_mutated"),
            "PartialLoadEnabled": object_payload(partial, "partial_load_enabled"),
        },
        "diagnostic_codes": ["copy_on_change_full_temporary_document_cache_not_collected"],
        "known_gap": {
            "kind": "c8m1_copy_on_change_full_temporary_document_native_lifecycle_blocked",
            "route": "native_oracle_blocked",
            "reason": (
                "FreeCADCmd exposes BindCopyOnChange and PartialLoad property states, but the full "
                "temporary copied-object document cache and mutation trigger are session-local "
                "lifecycle state, not a stable stateless cad-core expected geometry payload."
            ),
            "source_authority": [
                "/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()",
                "/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::checkCopyOnChange()",
                "/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::checkPropertyStatus()",
            ],
            "freecadcmd_evidence": {
                "property_states_collected": ["Disabled", "Enabled", "Mutated", "PartialLoad=True"],
                "support_property_type": "App::PropertyXLinkSubList",
            },
            "uncollected_fields": [
                "_CopiedObjs temporary document cache contents",
                "LinkBaseExtension copy-on-change property divergence trigger",
                "Support.setAllowPartial internal flag beyond Python-visible PartialLoad value",
            ],
            "delete_condition": (
                "Replace this lifecycle blocker only after FreeCADCmd exposes a stable request-local "
                "CopyOnChange mutation payload with copied-object evidence that does not require a "
                "persistent backend session."
            ),
            "reopen_condition": (
                "Reopen for S4 only if product chooses a request-local CopyOnChange DTO subset backed "
                "by this or a stronger native oracle."
            ),
        },
    }


CASE_COLLECTORS: dict[str, Callable[[Any], dict[str, Any]]] = {
    "shape-binder-whole-box-cross-body": collect_shape_binder_whole,
    "shape-binder-face-edge-vertex-multi-subshape": collect_shape_binder_subshapes,
    "shape-binder-trace-support-placement": collect_shape_binder_trace_support,
    "shape-binder-datum-fallback-line-plane-point": collect_shape_binder_datum_fallback,
    "subshape-binder-basic-support-whole-face-edge-list": collect_subshape_binder_basic,
    "subshape-binder-makeface-offset-fuse-refine": collect_subshape_binder_geometry_ops,
    "subshape-binder-setlinks-normalization-diagnostics": collect_subshape_binder_setlinks,
    "subshape-binder-relative-context-nested-route": collect_subshape_binder_relative_nested,
    "subshape-binder-profile-consumer-before-after-pad": collect_subshape_binder_profile_consumer,
    "shape-binder-subshape-binder-element-map-namedshape-body-replay": collect_binder_element_map,
    "subshape-binder-bindmode-synchronized-frozen-detached": collect_bindmode_lifecycle,
    "subshape-binder-copy-on-change-disabled-enabled-mutated-partialload": collect_copy_on_change_lifecycle,
}


def blocked_payload(
    fixture_path: Path,
    fixture: dict[str, Any],
    FreeCAD: Any,
    kind: str,
    reason: str,
    evidence: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload = base_payload(
        fixture_path,
        fixture,
        FreeCAD,
        f"FreeCADCmd C8-M1 native oracle blocker from {fixture_path.name}",
    )
    payload["route"] = "native_oracle_blocked"
    payload["known_gap"] = {
        "kind": kind,
        "route": "native_oracle_blocked",
        "reason": reason,
        "freecadcmd_evidence": evidence or {},
        "source_fixture": str(fixture_path),
        "delete_condition": (
            "Replace this blocker with native expected geometry only after FreeCADCmd can "
            "observe the case stably without using current cad-core output."
        ),
        "reopen_condition": "Reopen S4 implementation gate only after source-backed native expected exists.",
    }
    return payload


def collect_one(fixture_path: Path) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    fixture = load_fixture(fixture_path)
    native_case = fixture.get("c8m1_case")
    if native_case not in CASE_COLLECTORS:
        raise ValueError(f"unsupported C8-M1 native case {native_case!r}")
    doc = FreeCAD.newDocument("C8M1ShapeBinderExpected")
    try:
        try:
            case_payload = CASE_COLLECTORS[str(native_case)](doc)
        except OracleBlocked as exc:
            return blocked_payload(fixture_path, fixture, FreeCAD, exc.kind, exc.reason, exc.evidence)
        reference = (
            f"FreeCADCmd C8-M1 ShapeBinder/SubShapeBinder oracle from {fixture_path.name}; "
            f"case={native_case}; oracle_ids={','.join(fixture.get('oracle_ids', []))}"
        )
        payload = base_payload(fixture_path, fixture, FreeCAD, reference)
        payload.update(case_payload)
        return payload
    finally:
        FreeCAD.closeDocument(doc.Name)


def run_inside_freecad(args: argparse.Namespace) -> int:
    fixtures_root = Path(args.fixtures_root)
    failures = 0
    for fixture_path in fixture_paths(args):
        out_path = Path(args.out) if args.out else expected_path_for_fixture(fixtures_root, fixture_path)
        try:
            payload = collect_one(fixture_path)
            atomic_write_json(out_path, payload)
            if args.pretty or (not args.out and not args.phase):
                print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        except Exception as exc:
            print(f"failed {fixture_path}: {exc}", file=sys.stderr)
            print(traceback.format_exc(), file=sys.stderr)
            failures += 1
    return 1 if failures else 0


def run_via_freecadcmd(argv: list[str], args: argparse.Namespace) -> int:
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(script_args(argv), ensure_ascii=False)
    command = [args.freecadcmd, str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    return subprocess.run(command, cwd=Path.cwd(), env=env).returncode


def main(argv: list[str] | None = None) -> int:
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args = parse_args(raw_argv)
    try:
        import FreeCAD  # type: ignore # noqa: F401
    except ImportError:
        return run_via_freecadcmd(raw_argv, args)
    return run_inside_freecad(args)


if __name__ == "__main__" or invoked_by_freecad_cli_import():
    raise SystemExit(main())
