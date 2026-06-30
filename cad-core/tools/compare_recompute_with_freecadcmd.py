#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
ENV_ARG_NAME = "CAD_CORE_COMPARE_FREECAD_ARGS_JSON"
ENV_ARG_MARKER = "__cad_core_compare_freecad_args_env__"
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
DEFAULT_CAD_CORE_BIN = str(ROOT / "build" / "cad-core")
CAD_CORE_SHAPE_SUMMARY_SCHEMA_VERSION = "cad-core.shape-summary.v1"
NATIVE_PROFILE_LINK_SUB_TYPES = {
    "PartDesign::Groove",
    "PartDesign::Pad",
    "PartDesign::Pocket",
    "PartDesign::Revolution",
}
RAW_VECTOR_PROPERTIES = {
    "Direction",
    "Direction2",
}


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
            "Run a cad-core recompute input through FreeCADCmd and compare it with an existing "
            "cad-core /cad/recompute output JSON."
        )
    )
    parser.add_argument("input", type=Path, help="cad-core recompute input JSON, for example temp/input/20-52-20-01.json")
    parser.add_argument(
        "--output",
        type=Path,
        help="cad-core recompute output JSON. Defaults to the matching temp/output/<name>.json for temp/input paths.",
    )
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD", DEFAULT_FREECADCMD))
    parser.add_argument("--cad-core-bin", default=os.environ.get("CAD_CORE_BIN", DEFAULT_CAD_CORE_BIN))
    parser.add_argument("--target", action="append", dest="targets", help="Target object to compare. Defaults to output results[].object.")
    parser.add_argument("--report", type=Path, help="Write the comparison report JSON to this path.")
    parser.add_argument("--native-out", type=Path, help="Write the raw FreeCADCmd summary JSON to this path.")
    parser.add_argument("--bbox-tol", type=float, default=1e-4)
    parser.add_argument("--volume-abs-tol", type=float, default=1e-3)
    parser.add_argument("--volume-rel-tol", type=float, default=2e-2)
    parser.add_argument("--skip-volume", action="store_true", help="Do not fail on volume mismatches.")
    parser.add_argument(
        "--skip-cad-core-shape-summary",
        action="store_true",
        help="Compare only the existing recompute output JSON instead of exporting cad-core BREP shapes.",
    )
    parser.add_argument(
        "--compare-mesh-volume",
        action="store_true",
        help="Also compare cad-core recompute mesh volume when no BREP Shape volume is available.",
    )
    parser.add_argument("--inside-freecad", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--summarize-brep", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--brep-map-json", help=argparse.SUPPRESS)
    parser.add_argument("--shape-summary-out", type=Path, help=argparse.SUPPRESS)
    return parser.parse_args(script_args(argv))


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def default_output_path(input_path: Path) -> Path:
    parts = input_path.parts
    for index in range(len(parts) - 1):
        if parts[index] == "temp" and parts[index + 1] == "input":
            return Path(*parts[: index + 1], "output", *parts[index + 2 :])
    return input_path.with_name(f"{input_path.stem}.output.json")


def freecadcmd_path(configured: str) -> str:
    if configured and Path(configured).exists():
        return configured
    found = shutil.which(configured) if configured else None
    if found:
        return found
    found = shutil.which("FreeCADCmd") or shutil.which("freecadcmd")
    if found:
        return found
    raise RuntimeError("FreeCADCmd not found; pass --freecadcmd or set FREECADCMD")


def cad_core_bin_path(configured: str) -> str:
    if configured and Path(configured).exists():
        return configured
    found = shutil.which(configured) if configured else None
    if found:
        return found
    wrapper = ROOT / "cad-core"
    if wrapper.exists():
        return str(wrapper)
    found = shutil.which("cad-core")
    if found:
        return found
    raise RuntimeError("cad-core binary not found; pass --cad-core-bin or set CAD_CORE_BIN")


def result_targets(output_payload: dict[str, Any], input_payload: dict[str, Any]) -> list[str]:
    results = output_payload.get("results")
    if isinstance(results, list) and results:
        return [str(item["object"]) for item in results if isinstance(item, dict) and item.get("object")]
    recompute_targets = input_payload.get("recompute", {}).get("objs")
    if isinstance(recompute_targets, list) and recompute_targets:
        return [str(item) for item in recompute_targets]
    return []


def run_inside_freecad(args: argparse.Namespace) -> int:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from collect_freecad_expected import collect_one  # type: ignore

    input_path = normalized_freecadcmd_input(args.input.resolve())
    payload = collect_one(input_path, args.targets)
    if args.native_out:
        write_json(args.native_out, payload)
    else:
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


def part_shape_summary(shape: Any) -> dict[str, Any]:
    try:
        bbox = shape.optimalBoundingBox()
    except Exception:
        bbox = shape.BoundBox
    return {
        "bbox": {
            "min": [float(bbox.XMin), float(bbox.YMin), float(bbox.ZMin)],
            "max": [float(bbox.XMax), float(bbox.YMax), float(bbox.ZMax)],
        },
        "volume": float(getattr(shape, "Volume", 0.0)),
        "topology_counts": {
            "faces": len(getattr(shape, "Faces", [])),
            "edges": len(getattr(shape, "Edges", [])),
            "vertices": len(getattr(shape, "Vertexes", [])),
        },
        "shape_type": str(getattr(shape, "ShapeType", "")),
        "solids": len(getattr(shape, "Solids", [])),
        "compounds": len(getattr(shape, "Compounds", [])),
        "summary_source": "cad_core_brep_shape",
    }


def run_brep_summary_inside_freecad(args: argparse.Namespace) -> int:
    import Part  # type: ignore

    if not args.brep_map_json:
        raise RuntimeError("--brep-map-json is required")

    brep_map = json.loads(args.brep_map_json)
    payload: dict[str, Any] = {
        "schema_version": CAD_CORE_SHAPE_SUMMARY_SCHEMA_VERSION,
        "reference": "cad-core Shape summary from temporary --export-format brep files",
        "objects": {},
    }
    for target, brep_path in brep_map.items():
        shape = Part.Shape()
        shape.read(str(brep_path))
        payload["objects"][str(target)] = part_shape_summary(shape)

    if args.shape_summary_out:
        write_json(args.shape_summary_out, payload)
    else:
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


def normalized_freecadcmd_input(input_path: Path) -> Path:
    payload = load_json(input_path)
    normalized = normalize_fixture_for_freecadcmd(payload)
    if normalized == payload:
        return input_path
    output_path = Path(tempfile.gettempdir()) / f"{input_path.stem}.freecadcmd-normalized.json"
    write_json(output_path, normalized)
    return output_path


def normalize_fixture_for_freecadcmd(payload: dict[str, Any]) -> dict[str, Any]:
    normalized = copy.deepcopy(payload)
    for spec in normalized.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") not in NATIVE_PROFILE_LINK_SUB_TYPES:
            continue
        properties = spec.get("Properties")
        if not isinstance(properties, dict):
            continue
        for property_name in RAW_VECTOR_PROPERTIES:
            value = properties.get(property_name)
            if is_3d_number_list(value):
                properties[property_name] = {
                    "PropertyType": "App::PropertyVector",
                    "value": value,
                }
        profile = properties.get("Profile")
        if not isinstance(profile, dict) or profile.get("PropertyType") != "App::PropertyLinkSubList":
            continue
        sub_set = profile.get("SubSet")
        if not isinstance(sub_set, list) or len(sub_set) != 1 or not isinstance(sub_set[0], dict):
            continue
        item = copy.deepcopy(sub_set[0])
        item["PropertyType"] = "App::PropertyLinkSub"
        properties["Profile"] = item
    return normalized


def is_3d_number_list(value: Any) -> bool:
    return (
        isinstance(value, list)
        and len(value) == 3
        and all(isinstance(item, (int, float)) and not isinstance(item, bool) for item in value)
    )


def run_freecadcmd(input_path: Path, targets: list[str], args: argparse.Namespace, native_out: Path) -> subprocess.CompletedProcess[str]:
    child_args = [
        "--inside-freecad",
        str(input_path),
        "--native-out",
        str(native_out),
    ]
    for target in targets:
        child_args.extend(["--target", target])
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(child_args, ensure_ascii=False)
    command = [freecadcmd_path(args.freecadcmd), str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    return subprocess.run(command, cwd=ROOT.parent, env=env, text=True, capture_output=True)


def safe_target_stem(index: int, target: str) -> str:
    safe = "".join(ch if ch.isalnum() or ch in {"-", "_", "."} else "_" for ch in target)
    return f"{index:02d}-{safe or 'target'}"


def run_brep_summary_freecadcmd(
    input_path: Path,
    brep_files: dict[str, Path],
    args: argparse.Namespace,
    summary_out: Path,
) -> subprocess.CompletedProcess[str]:
    child_args = [
        "--summarize-brep",
        str(input_path),
        "--brep-map-json",
        json.dumps({target: str(path) for target, path in brep_files.items()}, ensure_ascii=False),
        "--shape-summary-out",
        str(summary_out),
    ]
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(child_args, ensure_ascii=False)
    command = [freecadcmd_path(args.freecadcmd), str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    return subprocess.run(command, cwd=ROOT.parent, env=env, text=True, capture_output=True)


def cad_core_shape_summary(
    input_path: Path,
    targets: list[str],
    args: argparse.Namespace,
    tmp_dir: Path,
) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    executable = cad_core_bin_path(args.cad_core_bin)
    brep_files: dict[str, Path] = {}
    exports: list[dict[str, str]] = []
    for index, target in enumerate(targets):
        stem = safe_target_stem(index, target)
        export_output = tmp_dir / f"{stem}.cad-core-output.json"
        brep_path = tmp_dir / f"{stem}.brep"
        command = [
            executable,
            "recompute",
            str(input_path),
            "--output",
            str(export_output),
            "--export-object",
            target,
            "--export-format",
            "brep",
            "--export-file",
            str(brep_path),
        ]
        completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if completed.returncode != 0:
            return None, {
                "status": "cad_core_shape_export_failed",
                "object": target,
                "argv": command,
                "command_returncode": completed.returncode,
                "stdout": completed.stdout,
                "stderr": completed.stderr,
            }
        brep_files[target] = brep_path
        exports.append({"object": target, "format": "brep"})

    summary_out = tmp_dir / f"{input_path.stem}.cad-core-shape-summary.json"
    completed = run_brep_summary_freecadcmd(input_path, brep_files, args, summary_out)
    if completed.returncode != 0:
        return None, {
            "status": "cad_core_shape_summary_failed",
            "command_returncode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
    payload = load_json(summary_out)
    payload["exports"] = exports
    return payload, None


def vertices_from_mesh(mesh: dict[str, Any]) -> list[list[float]]:
    vertices = mesh.get("vertices") or []
    if not vertices:
        return []
    if isinstance(vertices[0], list):
        return [[float(coord) for coord in vertex[:3]] for vertex in vertices]
    return [
        [float(vertices[index]), float(vertices[index + 1]), float(vertices[index + 2])]
        for index in range(0, len(vertices), 3)
    ]


def bbox_from_vertices(vertices: list[list[float]]) -> dict[str, list[float]] | None:
    if not vertices:
        return None
    return {
        "min": [min(vertex[axis] for vertex in vertices) for axis in range(3)],
        "max": [max(vertex[axis] for vertex in vertices) for axis in range(3)],
    }


def topology_counts(subshapes: list[dict[str, Any]]) -> dict[str, int]:
    counts = {"faces": 0, "edges": 0, "vertices": 0}
    for item in subshapes:
        kind = item.get("kind")
        if kind == "Face":
            counts["faces"] += 1
        elif kind == "Edge":
            counts["edges"] += 1
        elif kind == "Vertex":
            counts["vertices"] += 1
    return counts


def mesh_volume(vertices: list[list[float]], indices: list[int]) -> float | None:
    if not vertices or not indices or len(indices) % 3 != 0:
        return None

    def cross(left: list[float], right: list[float]) -> list[float]:
        return [
            left[1] * right[2] - left[2] * right[1],
            left[2] * right[0] - left[0] * right[2],
            left[0] * right[1] - left[1] * right[0],
        ]

    volume = 0.0
    for index in range(0, len(indices), 3):
        a = vertices[int(indices[index])]
        b = vertices[int(indices[index + 1])]
        c = vertices[int(indices[index + 2])]
        cross_bc = cross(b, c)
        volume += (a[0] * cross_bc[0] + a[1] * cross_bc[1] + a[2] * cross_bc[2]) / 6.0
    return abs(volume)


def cad_core_mesh_summaries(output_payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    summaries: dict[str, dict[str, Any]] = {}
    for result in output_payload.get("results", []):
        if not isinstance(result, dict) or not result.get("object"):
            continue
        mesh = result.get("mesh") or {}
        vertices = vertices_from_mesh(mesh) if isinstance(mesh, dict) else []
        summary: dict[str, Any] = {
            "bbox": bbox_from_vertices(vertices),
            "topology_counts": topology_counts(result.get("subshapes") or []),
            "summary_source": "cad_core_recompute_mesh",
        }
        volume = mesh_volume(vertices, mesh.get("indices") or []) if isinstance(mesh, dict) else None
        if volume is not None:
            summary["mesh_volume"] = volume
        summaries[str(result["object"])] = summary
    return summaries


def cad_core_shape_summaries(shape_payload: dict[str, Any] | None) -> dict[str, dict[str, Any]]:
    if not shape_payload:
        return {}
    objects = shape_payload.get("objects")
    if isinstance(objects, dict):
        return objects
    return {}


def native_summaries(native_payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    if isinstance(native_payload.get("objects"), dict):
        return native_payload["objects"]
    if native_payload.get("object"):
        ignored = {"schema_version", "reference", "freecad_version", "object"}
        return {str(native_payload["object"]): {key: value for key, value in native_payload.items() if key not in ignored}}
    return {}


def close_enough(left: float, right: float, abs_tol: float, rel_tol: float = 0.0) -> bool:
    return abs(left - right) <= max(abs_tol, rel_tol * max(abs(left), abs(right), 1.0))


def compare_bbox(
    cad_bbox: dict[str, list[float]] | None,
    native_bbox: dict[str, list[float]],
    tolerance: float,
    cad_label: str,
) -> list[str]:
    if cad_bbox is None:
        return [f"bbox:missing_{cad_label}"]
    errors: list[str] = []
    for side in ("min", "max"):
        for index, (cad_value, native_value) in enumerate(zip(cad_bbox[side], native_bbox[side])):
            if not close_enough(cad_value, native_value, tolerance):
                errors.append(f"bbox.{side}[{index}]: {cad_label}={cad_value} freecadcmd={native_value}")
    return errors


def cad_core_label(cad_summary: dict[str, Any]) -> str:
    return "cad_core_shape" if cad_summary.get("summary_source") == "cad_core_brep_shape" else "cad_core_mesh"


def compare_target(
    cad_summary: dict[str, Any] | None,
    native_summary: dict[str, Any] | None,
    args: argparse.Namespace,
    cad_mesh_summary: dict[str, Any] | None = None,
) -> dict[str, Any]:
    if cad_summary is None:
        return {"status": "missing_cad_core", "differences": ["missing cad-core result"]}
    if native_summary is None:
        return {"status": "missing_freecadcmd", "differences": ["missing FreeCADCmd result"]}

    differences: list[str] = []
    cad_label = cad_core_label(cad_summary)
    if "bbox" in native_summary:
        differences.extend(compare_bbox(cad_summary.get("bbox"), native_summary["bbox"], args.bbox_tol, cad_label))
    if native_summary.get("topology_counts") != cad_summary.get("topology_counts"):
        differences.append(
            "topology_counts: "
            f"{cad_label}={cad_summary.get('topology_counts')} freecadcmd={native_summary.get('topology_counts')}"
        )
    if not args.skip_volume and "volume" in native_summary and "volume" in cad_summary:
        if not close_enough(cad_summary["volume"], native_summary["volume"], args.volume_abs_tol, args.volume_rel_tol):
            differences.append(
                f"volume: {cad_label}={cad_summary['volume']} freecadcmd={native_summary['volume']}"
            )
    elif (
        not args.skip_volume
        and args.compare_mesh_volume
        and "volume" in native_summary
        and "mesh_volume" in cad_summary
    ):
        if not close_enough(cad_summary["mesh_volume"], native_summary["volume"], args.volume_abs_tol, args.volume_rel_tol):
            differences.append(
                f"volume: cad_core_mesh={cad_summary['mesh_volume']} freecadcmd={native_summary['volume']}"
            )
    report = {
        "status": "match" if not differences else "different",
        "differences": differences,
        "cad_core": cad_summary,
        "freecadcmd": native_summary,
    }
    if cad_mesh_summary is not None:
        report["cad_core_mesh"] = cad_mesh_summary
    return report


def compare(
    input_path: Path,
    output_path: Path,
    native_path: Path,
    args: argparse.Namespace,
    cad_core_shape_payload: dict[str, Any] | None = None,
) -> dict[str, Any]:
    input_payload = load_json(input_path)
    output_payload = load_json(output_path)
    native_payload = load_json(native_path)
    targets = args.targets or result_targets(output_payload, input_payload)
    cad_mesh_summaries = cad_core_mesh_summaries(output_payload)
    cad_shape_summaries = cad_core_shape_summaries(cad_core_shape_payload)
    cad_summaries = cad_shape_summaries or cad_mesh_summaries
    freecad_summaries = native_summaries(native_payload)
    target_reports = {
        target: compare_target(
            cad_summaries.get(target),
            freecad_summaries.get(target),
            args,
            cad_mesh_summaries.get(target),
        )
        for target in targets
    }
    report = {
        "input": str(input_path),
        "output": str(output_path),
        "native": str(native_path),
        "freecad_version": native_payload.get("freecad_version"),
        "diagnostics": output_payload.get("diagnostics", []),
        "cad_core_summary_source": "brep_shape" if cad_shape_summaries else "recompute_mesh",
        "targets": target_reports,
        "status": "match" if all(item["status"] == "match" for item in target_reports.values()) else "different",
    }
    if cad_core_shape_payload is not None:
        report["cad_core_shape_summary"] = {
            "schema_version": cad_core_shape_payload.get("schema_version"),
            "reference": cad_core_shape_payload.get("reference"),
            "exports": cad_core_shape_payload.get("exports", []),
        }
    return report


def run_parent(args: argparse.Namespace) -> int:
    input_path = args.input.resolve()
    output_path = (args.output or default_output_path(input_path)).resolve()
    input_payload = load_json(input_path)
    output_payload = load_json(output_path)
    targets = args.targets or result_targets(output_payload, input_payload)
    if not targets:
        raise RuntimeError("No targets found. Pass --target or include results[] / recompute.objs.")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        native_path = args.native_out.resolve() if args.native_out else Path(tmp) / f"{input_path.stem}.freecad.json"
        completed = run_freecadcmd(input_path, targets, args, native_path)
        if completed.returncode != 0:
            report = {
                "status": "freecadcmd_failed",
                "command_returncode": completed.returncode,
                "stdout": completed.stdout,
                "stderr": completed.stderr,
            }
            print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
            return 2
        cad_core_shape_payload = None
        cad_core_shape_error = None
        if not args.skip_cad_core_shape_summary:
            cad_core_shape_payload, cad_core_shape_error = cad_core_shape_summary(input_path, targets, args, tmp_dir)
            if cad_core_shape_error is not None:
                print(json.dumps(cad_core_shape_error, ensure_ascii=False, indent=2, sort_keys=True))
                return 2
        report = compare(input_path, output_path, native_path, args, cad_core_shape_payload)
        if completed.stderr.strip():
            report["freecadcmd_stderr"] = completed.stderr.strip()
        if args.report:
            write_json(args.report, report)
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return 0 if report["status"] == "match" else 1


def main(argv: list[str] | None = None) -> int:
    args = parse_args(list(sys.argv[1:] if argv is None else argv))
    try:
        if args.summarize_brep:
            return run_brep_summary_inside_freecad(args)
        if args.inside_freecad:
            return run_inside_freecad(args)
        return run_parent(args)
    except Exception as exc:
        print(json.dumps({"status": "error", "error": str(exc)}, ensure_ascii=False, indent=2), file=sys.stderr)
        return 2


if __name__ == "__main__" or invoked_by_freecad_cli_import():
    raise SystemExit(main())
