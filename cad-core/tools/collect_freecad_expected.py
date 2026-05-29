#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FREECADCMD = "/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd"
SCHEMA_VERSION = "cad-core.freecad-expected.v1"
ENV_ARG_MARKER = "__cad_core_expected_args_env__"
ENV_ARG_NAME = "CAD_CORE_EXPECTED_ARGS_JSON"
SUPPORTED_NATIVE_TYPES = {
    "App::Link",
    "Part::Box",
    "Part::Common",
    "Part::Cone",
    "Part::Cut",
    "Part::Cylinder",
    "Part::Ellipse",
    "Part::Fuse",
    "Part::Helix",
    "Part::ImportIges",
    "Part::Line",
    "Part::MultiCommon",
    "Part::MultiFuse",
    "Part::Plane",
    "Part::Prism",
    "Part::RegularPolygon",
    "Part::Sphere",
    "Part::Spiral",
    "Part::Vertex",
    "Part::Wedge",
}


class UnsupportedFixture(RuntimeError):
    pass


def script_args(argv: list[str]) -> list[str]:
    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Application.cpp::parseProgramOptions(),
    # registers "--pass" as the option that passes remaining arguments through to a script.
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
    try:
        script_path = Path(__file__).resolve()
    except NameError:
        return False
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
        description="Collect CAD Core fixture expected JSON from native FreeCAD.",
    )
    parser.add_argument("fixture", nargs="?", help="Fixture JSON file. Omit when --phase is used.")
    parser.add_argument("--phase", help="Collect every supported fixture in fixtures/<phase>.")
    parser.add_argument("--fixtures-root", default=str(ROOT / "fixtures"), help="Fixture root directory.")
    parser.add_argument("--out", help="Output expected file for a single fixture.")
    parser.add_argument("--check", action="store_true", help="Compare generated output with existing expected files.")
    parser.add_argument("--pretty", action="store_true", help="Print generated JSON to stdout.")
    parser.add_argument("--skip-unsupported", action="store_true", help="Skip unsupported fixtures in --phase mode.")
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD", DEFAULT_FREECADCMD))
    return parser.parse_args(script_args(argv))


def freecad_version(FreeCAD: Any) -> str:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        return " ".join(str(item) for item in version if item)
    return str(version)


def load_fixture(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def atomic_write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def expected_path_for_fixture(fixtures_root: Path, fixture_path: Path) -> Path:
    phase = fixture_path.parent.name
    return fixtures_root / phase / "expected" / f"{fixture_path.stem}.freecad.json"


def fixture_paths(args: argparse.Namespace) -> list[Path]:
    fixtures_root = Path(args.fixtures_root)
    if args.phase:
        if args.fixture:
            raise ValueError("--phase and fixture path are mutually exclusive")
        return sorted(path for path in (fixtures_root / args.phase).glob("*.json") if path.is_file())
    if not args.fixture:
        raise ValueError("fixture path or --phase is required")
    return [Path(args.fixture)]


def set_placement(FreeCAD: Any, obj: Any, value: dict) -> None:
    base = value.get("Base", [0, 0, 0])
    rotation = value.get("Rotation", [0, 0, 0, 1])
    obj.Placement = FreeCAD.Placement(
        FreeCAD.Vector(float(base[0]), float(base[1]), float(base[2])),
        FreeCAD.Rotation(float(rotation[0]), float(rotation[1]), float(rotation[2]), float(rotation[3])),
    )


def fixture_file_name(value: str) -> str:
    path = Path(value)
    if path.is_absolute():
        return str(path)
    return str(ROOT / path)


def link_sub_value(created: dict[str, Any], value: dict) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"link target {target_name} was not created")
    target = created[target_name]
    sub_list = value.get("SubList", [])
    if sub_list:
        return target, list(sub_list)
    return target


def placement_value(FreeCAD: Any, value: dict) -> Any:
    base = value.get("Base", [0, 0, 0])
    rotation = value.get("Rotation", [0, 0, 0, 1])
    return FreeCAD.Placement(
        FreeCAD.Vector(float(base[0]), float(base[1]), float(base[2])),
        FreeCAD.Rotation(float(rotation[0]), float(rotation[1]), float(rotation[2]), float(rotation[3])),
    )


def safe_setattr(obj: Any, name: str, value: Any) -> None:
    try:
        setattr(obj, name, value)
    except Exception as exc:
        raise UnsupportedFixture(f"failed to set property {name}: {exc}") from exc


def set_property(FreeCAD: Any, created: dict[str, Any], obj: Any, name: str, value: Any) -> None:
    if isinstance(value, dict):
        property_type = value.get("PropertyType")
        if property_type == "App::PropertyPlacement":
            set_placement(FreeCAD, obj, value)
            return
        if property_type in {
            "App::PropertyBool",
            "App::PropertyInteger",
            "App::PropertyFloat",
            "App::PropertyLength",
            "App::PropertyString",
        }:
            safe_setattr(obj, name, value.get("value"))
            return
        if property_type == "App::PropertyPlacementList":
            safe_setattr(obj, name, [placement_value(FreeCAD, item) for item in value.get("value", [])])
            return
        if property_type == "App::PropertyVectorList":
            safe_setattr(
                obj,
                name,
                [FreeCAD.Vector(float(item[0]), float(item[1]), float(item[2])) for item in value.get("value", [])],
            )
            return
        if property_type == "App::PropertyBoolList":
            safe_setattr(obj, name, [bool(item) for item in value.get("value", [])])
            return
        if property_type in {"App::PropertyLink", "App::PropertyXLink"}:
            safe_setattr(obj, name, link_sub_value(created, value))
            return
        if property_type in {"App::PropertyLinkSub", "App::PropertyXLinkSub"}:
            safe_setattr(obj, name, link_sub_value(created, value))
            return
        if property_type == "App::PropertyLinkList":
            safe_setattr(obj, name, [created[target] for target in value.get("value", [])])
            return
        raise UnsupportedFixture(f"unsupported structured property {name}: {property_type or sorted(value)}")
    if isinstance(value, list) and any(isinstance(item, dict) for item in value):
        raise UnsupportedFixture(f"unsupported link/list property {name}")
    if name == "FileName" and isinstance(value, str):
        value = fixture_file_name(value)
    safe_setattr(obj, name, value)


def create_objects(FreeCAD: Any, doc: Any, fixture: dict) -> dict[str, Any]:
    created: dict[str, Any] = {}
    for spec in fixture.get("Objects", []):
        name = spec["Name"]
        type_id = spec["TypeId"]
        if type_id not in SUPPORTED_NATIVE_TYPES:
            raise UnsupportedFixture(f"{type_id} is not enabled in this collector yet")
        try:
            obj = doc.addObject(type_id, name)
        except Exception as exc:
            raise UnsupportedFixture(f"FreeCAD cannot add {type_id}: {exc}") from exc
        for prop_name, prop_value in spec.get("Properties", {}).items():
            set_property(FreeCAD, created, obj, prop_name, prop_value)
        created[name] = obj
    return created


def shape_summary(shape: Any) -> dict:
    bbox = shape.BoundBox
    summary: dict[str, Any] = {
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
    }
    return summary


def target_names(fixture: dict) -> list[str]:
    names = fixture.get("recompute", {}).get("objs")
    if names:
        return list(names)
    objects = fixture.get("Objects", [])
    if not objects:
        raise UnsupportedFixture("fixture has no Objects")
    return [objects[-1]["Name"]]


def collect_one(fixture_path: Path) -> dict:
    import FreeCAD  # type: ignore

    fixture = load_fixture(fixture_path)
    doc = FreeCAD.newDocument("CadCoreExpected")
    try:
        created = create_objects(FreeCAD, doc, fixture)
        doc.recompute()

        targets = target_names(fixture)
        object_payloads: dict[str, dict] = {}
        for name in targets:
            obj = created.get(name)
            if obj is None:
                raise UnsupportedFixture(f"target object {name} was not created")
            shape = getattr(obj, "Shape", None)
            if shape is None or shape.isNull():
                raise UnsupportedFixture(f"target object {name} has no shape")
            object_payloads[name] = shape_summary(shape)

        reference_types = ", ".join(spec["TypeId"] for spec in fixture.get("Objects", []))
        payload: dict[str, Any] = {
            "schema_version": SCHEMA_VERSION,
            "reference": f"FreeCADCmd oracle from {fixture_path.name}; objects: {reference_types}",
            "freecad_version": freecad_version(FreeCAD),
        }
        if len(object_payloads) == 1:
            object_name, summary = next(iter(object_payloads.items()))
            payload["object"] = object_name
            payload.update(summary)
        else:
            payload["objects"] = object_payloads
        return payload
    finally:
        FreeCAD.closeDocument(doc.Name)


def close_enough(left: float, right: float, delta: float) -> bool:
    return abs(float(left) - float(right)) <= delta


def compare_bbox(existing: dict, generated: dict, delta: float) -> bool:
    for side in ("min", "max"):
        for expected_value, generated_value in zip(existing[side], generated[side]):
            if not close_enough(expected_value, generated_value, delta):
                return False
    return True


def compare_object_expected(existing: dict, generated: dict) -> list[str]:
    errors: list[str] = []
    bbox_delta = existing.get("bbox_delta", 1e-6)
    if "bbox" in existing and not compare_bbox(existing["bbox"], generated["bbox"], bbox_delta):
        errors.append("bbox")
    if "volume" in existing and not close_enough(existing["volume"], generated["volume"], existing.get("volume_delta", 1e-6)):
        errors.append("volume")
    if "topology_counts" in existing and existing["topology_counts"] != generated["topology_counts"]:
        errors.append("topology_counts")
    return errors


def compare_json(path: Path, payload: dict) -> bool:
    if not path.exists():
        print(f"missing expected: {path}", file=sys.stderr)
        return False
    existing = json.loads(path.read_text(encoding="utf-8"))
    errors: list[str] = []
    if "objects" in existing:
        generated_objects = payload.get("objects", {})
        for object_name, object_expected in existing["objects"].items():
            generated = generated_objects.get(object_name)
            if generated is None:
                errors.append(f"{object_name}:missing")
                continue
            errors.extend(f"{object_name}:{field}" for field in compare_object_expected(object_expected, generated))
    else:
        if existing.get("object") != payload.get("object"):
            errors.append("object")
        errors.extend(compare_object_expected(existing, payload))
    if errors:
        print(f"expected differs: {path}: {', '.join(errors)}", file=sys.stderr)
        return False
    return True


def run_inside_freecad(args: argparse.Namespace) -> int:
    fixtures_root = Path(args.fixtures_root)
    failures = 0
    skipped = 0
    for fixture_path in fixture_paths(args):
        try:
            payload = collect_one(fixture_path)
        except UnsupportedFixture as exc:
            if args.phase and args.skip_unsupported:
                skipped += 1
                print(f"skip unsupported {fixture_path}: {exc}", file=sys.stderr)
                continue
            print(f"unsupported {fixture_path}: {exc}", file=sys.stderr)
            failures += 1
            continue
        except Exception as exc:
            print(f"failed {fixture_path}: {exc}", file=sys.stderr)
            print(traceback.format_exc(), file=sys.stderr)
            failures += 1
            continue

        out_path = Path(args.out) if args.out else expected_path_for_fixture(fixtures_root, fixture_path)
        if args.check:
            if args.phase and args.skip_unsupported and not out_path.exists():
                skipped += 1
                print(f"skip missing expected {fixture_path}", file=sys.stderr)
                continue
            failures += 0 if compare_json(out_path, payload) else 1
        else:
            atomic_write_json(out_path, payload)
        if args.pretty or (not args.out and not args.phase):
            print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))

    if args.phase:
        print(f"processed={len(fixture_paths(args)) - skipped - failures} skipped={skipped} failed={failures}", file=sys.stderr)
    return 1 if failures else 0


def run_via_freecadcmd(argv: list[str], args: argparse.Namespace) -> int:
    # FreeCAD's --pass is multitoken but option-looking values can still be parsed
    # by the outer CLI. Keep wrapper invocations lossless by passing real args via env.
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
