#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FIXTURE = ROOT / "fixtures" / "p6" / "body-pad3body-duplicate-stable-subname.json"
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
DEFAULT_CAD_CORE_BIN = ROOT / "build" / "cad-core"
ENV_ARG_MARKER = "__cad_core_pad3body_oracle_bridge_args_env__"
ENV_ARG_NAME = "CAD_CORE_PAD3BODY_ORACLE_BRIDGE_ARGS_JSON"
SCHEMA_VERSION = "cad-core.pad3body-freecad-oracle-bridge.v1"
EXPORT_OBJECTS = ("Pad", "Revolution", "Pad2", "Fillet", "Pad3", "Pad3Body")
GEOMETRY_TOLERANCE = 1e-5
AREA_TOLERANCE = 1e-4


class BridgeBlocked(RuntimeError):
    def __init__(self, mapping: dict[str, Any]) -> None:
        self.mapping = mapping
        super().__init__(str(mapping.get("reason", "bridge blocked")))


def script_args(argv: list[str]) -> list[str]:
    args = list(argv)
    if "--pass" in args:
        args = args[args.index("--pass") + 1 :]
    if args == [ENV_ARG_MARKER] and os.environ.get(ENV_ARG_NAME):
        return json.loads(os.environ[ENV_ARG_NAME])
    if args and args[0] == "--":
        args = args[1:]
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
        description=(
            "Collect Pad3Body FreeCAD native-selection oracle bridge evidence without "
            "changing cad-core production topo naming."
        )
    )
    parser.add_argument("fixture", nargs="?", default=str(DEFAULT_FIXTURE))
    parser.add_argument("--out", default="/tmp/body-pad3body-freecad-oracle-bridge.json")
    parser.add_argument("--workdir", default="/tmp/body-pad3body-freecad-oracle-bridge")
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD", DEFAULT_FREECADCMD))
    parser.add_argument("--cad-core-bin", default=os.environ.get("CAD_CORE_BIN", str(DEFAULT_CAD_CORE_BIN)))
    parser.add_argument("--inside-freecad", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--brep-dir", help=argparse.SUPPRESS)
    parser.add_argument("--export-manifest", help=argparse.SUPPRESS)
    return parser.parse_args(script_args(argv))


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def resolve_executable(configured: str, fallback_name: str) -> str:
    path = Path(configured)
    if configured and path.exists():
        return str(path)
    found = shutil.which(configured) if configured else None
    if found:
        return found
    found = shutil.which(fallback_name)
    if found:
        return found
    raise RuntimeError(f"{fallback_name} not found: {configured}")


def expected_revolution_body_display_diagnostic(output_path: Path) -> dict[str, Any] | None:
    if not output_path.exists():
        return None
    try:
        output = load_json(output_path)
    except Exception:
        return None
    for diagnostic in output.get("diagnostics", []):
        if not isinstance(diagnostic, dict):
            continue
        if (
            diagnostic.get("code") == "body_display_subname_not_feature_local"
            and diagnostic.get("object") == "Pad2"
            and diagnostic.get("property") == "Profile"
            and diagnostic.get("target") == "Revolution"
            and diagnostic.get("subname") == "Face9"
        ):
            return diagnostic
    return None


def probe_expected_cad_core_diagnostic(executable: str,
                                       fixture: Path,
                                       workdir: Path) -> dict[str, Any]:
    output_path = workdir / "cad-core-diagnostic-probe.output.json"
    if output_path.exists():
        output_path.unlink()
    command = [
        executable,
        "recompute",
        str(fixture),
        "--output",
        str(output_path),
    ]
    completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    diagnostic = expected_revolution_body_display_diagnostic(output_path)
    return {
        "argv": command,
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "output": str(output_path),
        "diagnostic": diagnostic,
    }


def export_cad_core_breps(args: argparse.Namespace, workdir: Path) -> dict[str, Any]:
    executable = resolve_executable(args.cad_core_bin, "cad-core")
    fixture = Path(args.fixture).resolve()
    brep_dir = workdir / "cad-core-breps"
    brep_dir.mkdir(parents=True, exist_ok=True)
    exports: dict[str, Any] = {}
    for object_name in EXPORT_OBJECTS:
        output_path = brep_dir / f"{object_name}.output.json"
        brep_path = brep_dir / f"{object_name}.brep"
        for path in (output_path, brep_path):
            if path.exists():
                path.unlink()
        command = [
            executable,
            "recompute",
            str(fixture),
            "--output",
            str(output_path),
            "--export-object",
            object_name,
            "--export-format",
            "brep",
            "--export-file",
            str(brep_path),
        ]
        completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        exports[object_name] = {
            "argv": command,
            "returncode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "output": str(output_path),
            "brep": str(brep_path),
            "brep_exists": brep_path.exists(),
        }
        if completed.returncode != 0 or not brep_path.exists():
            diagnostic = expected_revolution_body_display_diagnostic(output_path)
            diagnostic_probe = None
            if diagnostic is None:
                diagnostic_probe = probe_expected_cad_core_diagnostic(executable, fixture, brep_dir)
                diagnostic = diagnostic_probe.get("diagnostic")
            if diagnostic is not None:
                exports[object_name]["expected_diagnostic"] = diagnostic
                if diagnostic_probe is not None:
                    exports[object_name]["diagnostic_probe"] = diagnostic_probe
                return {
                    "status": "expected_cad_core_diagnostic",
                    "brep_dir": str(brep_dir),
                    "exports": exports,
                    "diagnostic": diagnostic,
                    "expected_stop": (
                        "cad-core rejected Pad2.Profile value=Revolution Face9 before "
                        "FreeCAD bridge mapping because the only evidence is a Body display path"
                    ),
                }
            raise RuntimeError(f"cad-core BREP export failed for {object_name}")
    return {"status": "exports_ok", "brep_dir": str(brep_dir), "exports": exports}


def run_via_freecadcmd(argv: list[str], args: argparse.Namespace) -> int:
    workdir = Path(args.workdir)
    workdir.mkdir(parents=True, exist_ok=True)
    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "fixture": str(Path(args.fixture).resolve()),
    }
    try:
        export_payload = export_cad_core_breps(args, workdir)
        payload["cad_core_exports"] = export_payload
        if export_payload.get("status") == "expected_cad_core_diagnostic":
            payload["status"] = "expected_cad_core_diagnostic"
            payload["diagnostic"] = export_payload.get("diagnostic")
            payload["expected_stop"] = export_payload.get("expected_stop")
            atomic_write_json(Path(args.out), payload)
            return 0
        export_manifest = workdir / "cad-core-export-manifest.json"
        atomic_write_json(export_manifest, export_payload)
    except Exception as exc:
        payload["status"] = "cad_core_export_failed"
        payload["error"] = str(exc)
        payload["traceback"] = traceback.format_exc()
        atomic_write_json(Path(args.out), payload)
        return 1

    child_args = [
        "--inside-freecad",
        str(Path(args.fixture).resolve()),
        "--out",
        str(Path(args.out).resolve()),
        "--workdir",
        str(workdir),
        "--brep-dir",
        str(export_payload["brep_dir"]),
        "--export-manifest",
        str(export_manifest),
    ]
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(child_args, ensure_ascii=False)
    command = [
        resolve_executable(args.freecadcmd, "FreeCADCmd"),
        str(Path(__file__).resolve()),
        "--pass",
        ENV_ARG_MARKER,
    ]
    completed = subprocess.run(command, cwd=ROOT.parent, env=env, text=True, capture_output=True)
    if completed.returncode != 0:
        payload["status"] = "freecadcmd_failed"
        payload["freecadcmd"] = {
            "argv": command,
            "returncode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
        if Path(args.out).exists():
            try:
                existing = load_json(Path(args.out))
                existing["wrapper_failure"] = payload["freecadcmd"]
                atomic_write_json(Path(args.out), existing)
            except Exception:
                atomic_write_json(Path(args.out), payload)
        else:
            atomic_write_json(Path(args.out), payload)
    return completed.returncode


def freecad_version(FreeCAD: Any) -> str:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        if len(version) >= 4:
            revision = str(version[3]).split()[0]
            return f"{version[0]}.{version[1]}.{version[2]} revision {revision}"
        return " ".join(str(item) for item in version if item)
    return str(version)


def object_specs(fixture: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(spec["Name"]): spec for spec in fixture.get("Objects", []) if isinstance(spec, dict)}


def shape_is_present(shape: Any) -> bool:
    if shape is None:
        return False
    try:
        return not bool(shape.isNull())
    except Exception:
        return True


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
    payload: dict[str, Any] = {
        "ElementMapSize": int(getattr(shape, "ElementMapSize", 0)),
        "ElementMapVersion": str(getattr(shape, "ElementMapVersion", "")),
    }
    try:
        element_map = dict(getattr(shape, "ElementMap", {}))
        payload["ElementMap"] = {str(key): str(value) for key, value in sorted(element_map.items())}
    except Exception as exc:
        payload["ElementMap"] = {"status": "unavailable", "error": str(exc)}
    return payload


def shape_summary(shape: Any) -> dict[str, Any]:
    if not shape_is_present(shape):
        return {"status": "missing_shape"}
    return {
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


def subshape_sequence(shape: Any, kind: str) -> list[Any]:
    if kind == "Face":
        return list(getattr(shape, "Faces", []))
    if kind == "Edge":
        return list(getattr(shape, "Edges", []))
    if kind == "Vertex":
        return list(getattr(shape, "Vertexes", []))
    raise ValueError(f"unsupported subshape kind {kind}")


def subshape_by_token(shape: Any, token: str) -> Any:
    for kind in ("Face", "Edge", "Vertex"):
        if token.startswith(kind):
            index = int(token[len(kind) :]) - 1
            items = subshape_sequence(shape, kind)
            if index < 0 or index >= len(items):
                raise ValueError(f"{token} out of range")
            return items[index]
    raise ValueError(f"unsupported subshape token {token}")


def shape_center(shape: Any) -> list[float]:
    center = getattr(shape, "CenterOfMass", None)
    if center is None:
        return [0.0, 0.0, 0.0]
    return [float(center.x), float(center.y), float(center.z)]


def bbox_delta(left: dict[str, list[float]], right: dict[str, list[float]]) -> float:
    return max(
        abs(float(a) - float(b))
        for side in ("min", "max")
        for a, b in zip(left[side], right[side])
    )


def vector_delta(left: list[float], right: list[float]) -> float:
    return math.sqrt(sum((float(a) - float(b)) ** 2 for a, b in zip(left, right)))


def distance_between(left: Any, right: Any) -> float | None:
    try:
        result = left.distToShape(right)
        if isinstance(result, tuple) and result:
            return float(result[0])
        return float(result)
    except Exception:
        return None


def candidate_metrics(source: Any, candidate: Any) -> dict[str, Any]:
    source_bbox = bbox_payload(source)
    candidate_bbox = bbox_payload(candidate)
    source_center = shape_center(source)
    candidate_center = shape_center(candidate)
    distance = distance_between(source, candidate)
    return {
        "distance": distance,
        "bbox_delta": bbox_delta(source_bbox, candidate_bbox),
        "center_delta": vector_delta(source_center, candidate_center),
        "area_delta": abs(float(getattr(source, "Area", 0.0)) - float(getattr(candidate, "Area", 0.0))),
        "length_delta": abs(float(getattr(source, "Length", 0.0)) - float(getattr(candidate, "Length", 0.0))),
    }


def metrics_are_strong(metrics: dict[str, Any], kind: str) -> bool:
    distance = metrics.get("distance")
    if distance is None or float(distance) > GEOMETRY_TOLERANCE:
        return False
    if float(metrics["bbox_delta"]) > GEOMETRY_TOLERANCE:
        return False
    if float(metrics["center_delta"]) > GEOMETRY_TOLERANCE:
        return False
    if kind == "Face":
        return float(metrics["area_delta"]) <= AREA_TOLERANCE
    if kind == "Edge":
        return float(metrics["length_delta"]) <= GEOMETRY_TOLERANCE
    return True


def read_brep_shape(Part: Any, brep_dir: Path, object_name: str) -> Any:
    brep_path = brep_dir / f"{object_name}.brep"
    shape = Part.Shape()
    shape.read(str(brep_path))
    return shape


def resolve_probe(target: Any, subname: str) -> dict[str, Any]:
    if not hasattr(target, "resolveSubElement"):
        return {"status": "unsupported"}
    try:
        resolved = target.resolveSubElement(subname, False, 2)
        old_name = resolved[2] if isinstance(resolved, tuple) and len(resolved) > 2 else None
        return {"status": "ok", "old_name": str(old_name)}
    except Exception as exc:
        return {"status": "error", "error": str(exc)}


def map_by_brep_geometry(
    Part: Any,
    brep_dir: Path,
    object_name: str,
    cad_core_token: str,
    native_target: Any,
    property_path: str,
) -> dict[str, Any]:
    kind = "".join(ch for ch in cad_core_token if not ch.isdigit())
    cad_core_shape = read_brep_shape(Part, brep_dir, object_name)
    native_shape = getattr(native_target, "Shape", None)
    if not shape_is_present(native_shape):
        return {
            "property": property_path,
            "status": "blocked",
            "reason": f"{object_name} native target has no shape",
            "cad_core_reference": f"{object_name}.{cad_core_token}",
        }
    try:
        source_subshape = subshape_by_token(cad_core_shape, cad_core_token)
    except Exception as exc:
        candidates = []
        if kind in {"Face", "Edge", "Vertex"}:
            for index, candidate in enumerate(subshape_sequence(native_shape, kind), start=1):
                native_subname = f"{kind}{index}"
                candidates.append({
                    "native_subname": native_subname,
                    "signature": shape_summary(candidate),
                    "resolveSubElement": resolve_probe(native_target, native_subname),
                })
        return {
            "property": property_path,
            "status": "blocked",
            "reason": (
                "cad-core exported BREP for the target object cannot resolve the requested token; "
                "this usually means the fixture reference is Body-display scoped or stale rather "
                "than an object-local native subname"
            ),
            "cad_core_reference": f"{object_name}.{cad_core_token}",
            "method": "cad_core_brep_subshape_to_freecad_native_geometry",
            "source_error": str(exc),
            "source_shape_summary": shape_summary(cad_core_shape),
            "candidate_count": len(candidates),
            "candidates": candidates,
        }
    candidates = []
    for index, candidate in enumerate(subshape_sequence(native_shape, kind), start=1):
        native_subname = f"{kind}{index}"
        metrics = candidate_metrics(source_subshape, candidate)
        candidates.append({
            "native_subname": native_subname,
            "strong_match": metrics_are_strong(metrics, kind),
            "metrics": metrics,
            "signature": shape_summary(candidate),
            "resolveSubElement": resolve_probe(native_target, native_subname),
        })
    strong = [candidate for candidate in candidates if candidate["strong_match"]]
    mapping: dict[str, Any] = {
        "property": property_path,
        "cad_core_reference": f"{object_name}.{cad_core_token}",
        "method": "cad_core_brep_subshape_to_freecad_native_geometry",
        "source_signature": shape_summary(source_subshape),
        "candidate_count": len(candidates),
        "candidates": candidates,
    }
    if len(strong) == 1:
        mapping.update({
            "status": "mapped",
            "native_reference": f"{object_name}.{strong[0]['native_subname']}",
            "native_subname": strong[0]["native_subname"],
            "selected_candidate": strong[0],
        })
    elif not strong:
        mapping.update({
            "status": "blocked",
            "reason": "no FreeCAD native candidate matched the cad-core BREP subshape within tolerance",
        })
    else:
        mapping.update({
            "status": "ambiguous",
            "reason": "multiple FreeCAD native candidates matched the cad-core BREP subshape",
            "strong_matches": strong,
        })
    return mapping


def direct_mapping(property_path: str, cad_core_reference: str, native_reference: str, reason: str) -> dict[str, Any]:
    native_subname = native_reference.split(".", 1)[1] if "." in native_reference else ""
    return {
        "property": property_path,
        "status": "mapped",
        "method": "direct_native_fixture_rule",
        "cad_core_reference": cad_core_reference,
        "native_reference": native_reference,
        "native_subname": native_subname,
        "reason": reason,
    }


def require_mapped(mapping: dict[str, Any]) -> str:
    if mapping.get("status") != "mapped":
        raise BridgeBlocked(mapping)
    return str(mapping["native_subname"])


def spec_properties(specs: dict[str, dict[str, Any]], name: str) -> dict[str, Any]:
    return dict(specs[name].get("Properties", {}))


def set_non_link_properties(FreeCAD: Any, collector: Any, created: dict[str, Any], obj: Any, props: dict[str, Any], skip: set[str]) -> None:
    for prop_name, prop_value in props.items():
        if prop_name in skip:
            continue
        collector.set_property(FreeCAD, created, obj, prop_name, prop_value)


def set_profile(obj: Any, target: Any, subname: str | None) -> None:
    if subname:
        obj.Profile = (target, [subname])
    else:
        obj.Profile = target


def object_state(obj: Any) -> dict[str, Any]:
    payload = {
        "Name": str(getattr(obj, "Name", "")),
        "Label": str(getattr(obj, "Label", "")),
        "TypeId": str(getattr(obj, "TypeId", "")),
        "State": [str(item) for item in getattr(obj, "State", [])],
    }
    if hasattr(obj, "Shape"):
        payload["shape_summary"] = shape_summary(getattr(obj, "Shape"))
    return payload


def collect_inside_freecad(args: argparse.Namespace) -> int:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import collect_freecad_expected as collector  # type: ignore

    fixture_path = Path(args.fixture).resolve()
    fixture = load_json(fixture_path)
    specs = object_specs(fixture)
    brep_dir = Path(args.brep_dir or Path(args.workdir) / "cad-core-breps")
    doc = FreeCAD.newDocument("Pad3BodyOracleBridge")
    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "status": "running",
        "fixture": str(fixture_path),
        "freecad_version": freecad_version(FreeCAD),
        "brep_dir": str(brep_dir),
        "mappings": [],
        "objects": {},
        "blocked": [],
    }
    if args.export_manifest:
        try:
            payload["cad_core_exports"] = load_json(Path(args.export_manifest))
        except Exception as exc:
            payload["cad_core_exports"] = {"status": "unavailable", "error": str(exc)}
    created: dict[str, Any] = {}
    try:
        sketch_spec = specs["草图 1:57:56 PM"]
        sketch = collector.create_native_object(FreeCAD, doc, sketch_spec["TypeId"], sketch_spec["Name"])
        created[sketch_spec["Name"]] = sketch
        for prop_name, prop_value in sketch_spec.get("Properties", {}).items():
            collector.set_sketch_property(FreeCAD, created, sketch, prop_name, prop_value)
        doc.recompute()
        payload["objects"][sketch_spec["Name"]] = object_state(sketch)

        pad = collector.create_native_object(FreeCAD, doc, "PartDesign::Pad", "Pad")
        created["Pad"] = pad
        pad_props = spec_properties(specs, "Pad")
        set_profile(pad, sketch, None)
        mapping = direct_mapping(
            "Pad.Profile",
            "草图 1:57:56 PM.g100001;SKT;FAC",
            "草图 1:57:56 PM.<whole-sketch-profile>",
            "FreeCAD native PartDesign Profile consumes the sketch object for this closed profile, not cad-core SKT/FAC token text.",
        )
        payload["mappings"].append(mapping)
        set_non_link_properties(FreeCAD, collector, created, pad, pad_props, {"Profile"})
        doc.recompute()
        payload["objects"]["Pad"] = object_state(pad)

        revolution = collector.create_native_object(FreeCAD, doc, "PartDesign::Revolution", "Revolution")
        created["Revolution"] = revolution
        revolution_props = spec_properties(specs, "Revolution")
        mapping = map_by_brep_geometry(Part, brep_dir, "Pad", "Face6", pad, "Revolution.Profile")
        payload["mappings"].append(mapping)
        set_profile(revolution, pad, require_mapped(mapping))
        axis_mapping = direct_mapping(
            "Revolution.ReferenceAxis",
            "草图 1:57:56 PM.g100004",
            "草图 1:57:56 PM.Edge4",
            "Sketch geometry id 100004 is the fourth line segment in the fixture Geometry list, which FreeCAD exposes as Edge4.",
        )
        axis_mapping["resolveSubElement"] = resolve_probe(sketch, "Edge4")
        payload["mappings"].append(axis_mapping)
        revolution.ReferenceAxis = (sketch, ["Edge4"])
        set_non_link_properties(FreeCAD, collector, created, revolution, revolution_props, {"Profile", "ReferenceAxis"})
        doc.recompute()
        payload["objects"]["Revolution"] = object_state(revolution)

        pad2 = collector.create_native_object(FreeCAD, doc, "PartDesign::Pad", "Pad2")
        created["Pad2"] = pad2
        pad2_props = spec_properties(specs, "Pad2")
        mapping = map_by_brep_geometry(Part, brep_dir, "Revolution", "Face9", revolution, "Pad2.Profile")
        payload["mappings"].append(mapping)
        set_profile(pad2, revolution, require_mapped(mapping))
        set_non_link_properties(FreeCAD, collector, created, pad2, pad2_props, {"Profile"})
        doc.recompute()
        payload["objects"]["Pad2"] = object_state(pad2)

        fillet = collector.create_native_object(FreeCAD, doc, "PartDesign::Fillet", "Fillet")
        created["Fillet"] = fillet
        fillet_props = spec_properties(specs, "Fillet")
        mapping = map_by_brep_geometry(Part, brep_dir, "Pad2", "Edge7", pad2, "Fillet.Base")
        payload["mappings"].append(mapping)
        fillet.Base = (pad2, [require_mapped(mapping)])
        set_non_link_properties(FreeCAD, collector, created, fillet, fillet_props, {"Base"})
        doc.recompute()
        payload["objects"]["Fillet"] = object_state(fillet)

        pad3 = collector.create_native_object(FreeCAD, doc, "PartDesign::Pad", "Pad3")
        created["Pad3"] = pad3
        pad3_props = spec_properties(specs, "Pad3")
        mapping = map_by_brep_geometry(Part, brep_dir, "Fillet", "Face6", fillet, "Pad3.Profile")
        payload["mappings"].append(mapping)
        set_profile(pad3, fillet, require_mapped(mapping))
        set_non_link_properties(FreeCAD, collector, created, pad3, pad3_props, {"Profile"})
        doc.recompute()
        payload["objects"]["Pad3"] = object_state(pad3)

        body = collector.create_native_object(FreeCAD, doc, "PartDesign::Body", "Pad3Body")
        created["Pad3Body"] = body
        for member_name in specs["Pad3Body"]["Properties"]["Group"]["values"]:
            body.addObject(created[member_name])
        body.Tip = pad3
        doc.recompute()
        payload["objects"]["Pad3Body"] = object_state(body)

        final_shape = getattr(body, "Shape", None)
        if shape_is_present(final_shape):
            payload["status"] = "ok"
            payload["final_object"] = "Pad3Body"
            payload["final_summary"] = shape_summary(final_shape)
        else:
            payload["status"] = "blocked"
            payload["blocked"].append({
                "stage": "Pad3Body",
                "reason": "FreeCAD Body final shape is missing after mapped selections",
                "object_state": object_state(body),
            })
    except BridgeBlocked as exc:
        payload["status"] = "blocked"
        payload["blocked"].append(exc.mapping)
    except Exception as exc:
        payload["status"] = "error"
        payload["error"] = str(exc)
        payload["traceback"] = traceback.format_exc()
    finally:
        try:
            FreeCAD.closeDocument(doc.Name)
        finally:
            atomic_write_json(Path(args.out), payload)
    return 0 if payload.get("status") == "ok" else 1


def main(argv: list[str] | None = None) -> int:
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args = parse_args(raw_argv)
    if not args.inside_freecad:
        try:
            import FreeCAD  # type: ignore # noqa: F401
        except ImportError:
            return run_via_freecadcmd(raw_argv, args)
    return collect_inside_freecad(args)


if __name__ == "__main__" or invoked_by_freecad_cli_import():
    raise SystemExit(main())
