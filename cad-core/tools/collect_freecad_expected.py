#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FREECADCMD = "/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd"
SCHEMA_VERSION = "cad-core.freecad-expected.v1"
ENV_ARG_MARKER = "__cad_core_expected_args_env__"
ENV_ARG_NAME = "CAD_CORE_EXPECTED_ARGS_JSON"
FREECAD_PRECISION_CONFUSION = 1e-7
SUPPORTED_NATIVE_TYPES = {
    "App::FeaturePython",
    "App::Link",
    "App::LinkElement",
    "App::LinkGroup",
    "Assembly::AssemblyLink",
    "Assembly::AssemblyObject",
    "Assembly::JointGroup",
    "Mesh::Import",
    "PartDesign::Body",
    "PartDesign::Chamfer",
    "PartDesign::Fillet",
    "PartDesign::Hole",
    "PartDesign::Line",
    "PartDesign::LinearPattern",
    "PartDesign::Mirrored",
    "PartDesign::MultiTransform",
    "PartDesign::Pad",
    "PartDesign::Plane",
    "PartDesign::PolarPattern",
    "PartDesign::Pocket",
    "PartDesign::Scaled",
    "Part::Box",
    "Part::BooleanFragments",
    "Part::Common",
    "Part::Cone",
    "Part::Cut",
    "Part::Cylinder",
    "Part::Ellipse",
    "Part::Ellipsoid",
    "Part::Fuse",
    "Part::Helix",
    "Part::ImportIges",
    "Part::ImportBrep",
    "Part::ImportStep",
    "Part::Line",
    "Part::Loft",
    "Part::MultiCommon",
    "Part::MultiFuse",
    "Part::Plane",
    "Part::Prism",
    "Part::RegularPolygon",
    "Part::RuledSurface",
    "Part::Section",
    "Part::Sphere",
    "Part::Spiral",
    "Part::Torus",
    "Part::Vertex",
    "Part::Wedge",
    "Part::XOR",
    "Sketcher::SketchObject",
}

HOLE_PRE_BODY_PROPERTIES = {
    "Profile",
}

SKETCH_SHAPE_DEPENDENT_PROPERTIES = {
    "AttachmentSupport",
    "Support",
    "MapMode",
}

DRESS_UP_TYPES = {
    "PartDesign::Chamfer",
    "PartDesign::Fillet",
}

TRANSFORMED_TYPES = {
    "PartDesign::LinearPattern",
    "PartDesign::Mirrored",
    "PartDesign::MultiTransform",
    "PartDesign::PolarPattern",
    "PartDesign::Scaled",
}

BODY_RESULT_TARGET_TYPES = DRESS_UP_TYPES | TRANSFORMED_TYPES
EXTERNAL_GEOMETRY_FLAG_NAMES = ("Defining", "Frozen", "Detached", "Missing", "Sync")


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
        if len(version) >= 4:
            revision = str(version[3]).split()[0]
            return f"{version[0]}.{version[1]}.{version[2]} revision {revision}"
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


def list_field(value: dict, *names: str) -> list:
    for name in names:
        item = value.get(name)
        if item is not None:
            return list(item)
    return []


def external_geometry_flags_from_item(item: dict) -> set[str]:
    flags: set[str] = set()

    def add_flag(name: str) -> None:
        if name in EXTERNAL_GEOMETRY_FLAG_NAMES:
            flags.add(name)

    for field in ("ExternalFlags", "Flags"):
        raw_flags = item.get(field)
        if isinstance(raw_flags, list):
            for raw_flag in raw_flags:
                if isinstance(raw_flag, str):
                    add_flag(raw_flag)
        elif isinstance(raw_flags, str):
            add_flag(raw_flags)
        elif isinstance(raw_flags, int) and raw_flags >= 0:
            for bit, name in enumerate(EXTERNAL_GEOMETRY_FLAG_NAMES):
                if raw_flags & (1 << bit):
                    flags.add(name)

    for name in EXTERNAL_GEOMETRY_FLAG_NAMES:
        if item.get(name) is True:
            flags.add(name)
    return flags


def resolve_external_subname(created: dict[str, Any], target_name: str, subname: str) -> tuple[str, str]:
    if "." in subname:
        target = created[target_name]
        try:
            _, _, old_name = target.resolveSubElement(subname, False, 2)
        except Exception as exc:
            raise UnsupportedFixture(
                f"source-prefixed stable subname {target_name}.{subname} cannot resolve: {exc}"
            ) from exc
        if not old_name or str(old_name).startswith(";"):
            raise UnsupportedFixture(
                f"source-prefixed stable subname {target_name}.{subname} resolved without current old-style name"
            )
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        # /SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry(), when a stored
        # external reference is missing, calls GeoFeature::resolveElement(obj, ref-subname)
        # and pushes the original target object plus elementName.oldName into
        # ExternalGeometry. The collector mirrors that recovery path for CAD Core
        # StableSubList source-prefixed oracle cases.
        return target_name, str(old_name)
    return target_name, subname


def link_sub_value(created: dict[str, Any], value: dict) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"link target {target_name} was not created")
    target = created[target_name]
    # FreeCAD oracle mode: CAD Core fixtures may keep a stale user-visible SubList and a
    # StableSubList that represents the stable element to be resolved through ElementMap.
    # Native FreeCAD has no JSON StableSubList property, so the collector feeds the stable
    # subname to PropertyLinkSub to collect the expected post-resolution geometry.
    sub_list = list_field(value, "StableSubList", "SubList")
    if sub_list:
        return target, [native_link_subname(target, subname) for subname in sub_list]
    return target


def assembly_joint_reference_value(created: dict[str, Any], value: dict) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"assembly joint reference target {target_name} was not created")
    target = created[target_name]
    sub_list = list_field(value, "StableSubList", "SubList")
    if sub_list:
        return target, [native_link_subname(target, subname) for subname in sub_list]
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py
    # ::getObject(), returns None when "len(subs) < 1"; an empty string sub-token is the
    # native representation of an object-level Assembly Joint reference.
    return target, [""]


def native_link_subname(target: Any, subname: str) -> str:
    if "." not in subname:
        return subname
    if getattr(target, "TypeId", "") == "App::Link":
        return subname

    token, rest = subname.split(".", 1)
    label = str(getattr(target, "Label", ""))
    name = str(getattr(target, "Name", ""))
    if token == name or (token.startswith("$") and token[1:] == label):
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        # ::LinkBaseExtension::extensionGetSubObject() accepts linked-object name/label
        # tokens while resolving a subobject chain, but Python PropertyXLinkSub assignment
        # may leave the link without Shape when the first token is the current target itself.
        # Strip that self token for native oracle collection; CAD Core still validates and
        # retags the original full SubList from the fixture.
        return rest
    return subname


def placement_value(FreeCAD: Any, value: dict) -> Any:
    base = value.get("Base", [0, 0, 0])
    rotation = value.get("Rotation", [0, 0, 0, 1])
    return FreeCAD.Placement(
        FreeCAD.Vector(float(base[0]), float(base[1]), float(base[2])),
        FreeCAD.Rotation(float(rotation[0]), float(rotation[1]), float(rotation[2]), float(rotation[3])),
    )


def vector_value(FreeCAD: Any, value: Any, field: str) -> Any:
    if not isinstance(value, list) or len(value) not in {2, 3}:
        raise UnsupportedFixture(f"{field} must be a two- or three-number vector")
    if not all(isinstance(item, (int, float)) for item in value):
        raise UnsupportedFixture(f"{field} must contain only numbers")
    z = value[2] if len(value) == 3 else 0.0
    return FreeCAD.Vector(float(value[0]), float(value[1]), float(z))


def number_field(value: dict, name: str) -> float:
    item = value.get(name)
    if not isinstance(item, (int, float)):
        raise UnsupportedFixture(f"Sketch Geometry {name} must be a number")
    return float(item)


def int_field(value: dict, name: str) -> int:
    item = value.get(name)
    if not isinstance(item, int):
        raise UnsupportedFixture(f"Sketch Geometry {name} must be an integer")
    return item


def sketch_geometry_value(FreeCAD: Any, item: dict) -> tuple[Any, bool]:
    import Part  # type: ignore

    kind = item.get("kind")
    construction = bool(item.get("construction", False))
    normal = FreeCAD.Vector(0.0, 0.0, 1.0)

    if kind in {"Point", "GeomPoint"}:
        return Part.Point(vector_value(FreeCAD, item.get("point"), "Point.point")), construction
    if kind == "LineSegment":
        return Part.LineSegment(
            vector_value(FreeCAD, item.get("start"), "LineSegment.start"),
            vector_value(FreeCAD, item.get("end"), "LineSegment.end"),
        ), construction
    if kind == "Circle":
        return Part.Circle(
            vector_value(FreeCAD, item.get("center"), "Circle.center"),
            normal,
            number_field(item, "radius"),
        ), construction
    if kind == "Ellipse":
        ellipse = Part.Ellipse(
            vector_value(FreeCAD, item.get("center"), "Ellipse.center"),
            number_field(item, "majorRadius"),
            number_field(item, "minorRadius"),
        )
        ellipse.AngleXU = float(item.get("angle", 0.0))
        return ellipse, construction
    if kind == "ArcOfCircle":
        circle = Part.Circle(
            vector_value(FreeCAD, item.get("center"), "ArcOfCircle.center"),
            normal,
            number_field(item, "radius"),
        )
        return Part.ArcOfCircle(circle, number_field(item, "startAngle"), number_field(item, "endAngle")), construction
    if kind == "ArcOfEllipse":
        ellipse = Part.Ellipse(
            vector_value(FreeCAD, item.get("center"), "ArcOfEllipse.center"),
            number_field(item, "majorRadius"),
            number_field(item, "minorRadius"),
        )
        ellipse.AngleXU = float(item.get("angle", 0.0))
        return Part.ArcOfEllipse(ellipse, number_field(item, "startAngle"), number_field(item, "endAngle")), construction
    if kind in {"ArcOfHyperbola", "Part::GeomArcOfHyperbola"}:
        hyperbola = Part.Hyperbola(
            vector_value(FreeCAD, item.get("center"), "ArcOfHyperbola.center"),
            number_field(item, "majorRadius"),
            number_field(item, "minorRadius"),
        )
        hyperbola.AngleXU = float(item.get("angle", 0.0))
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        # ::GeomArcOfHyperbola::Restore() reads "MajorRadius", "MinorRadius", "AngleXU",
        # "StartAngle" and "EndAngle", then calls GC_MakeArcOfHyperbola(..., Standard_True).
        return (
            Part.ArcOfHyperbola(
                hyperbola,
                number_field(item, "startAngle"),
                number_field(item, "endAngle"),
                True,
            ),
            construction,
        )
    if kind in {"ArcOfParabola", "Part::GeomArcOfParabola"}:
        center = vector_value(FreeCAD, item.get("center"), "ArcOfParabola.center")
        focal = number_field(item, "focal")
        angle = float(item.get("angle", 0.0))
        focus = FreeCAD.Vector(center.x + focal * math.cos(angle), center.y + focal * math.sin(angle), center.z)
        parabola = Part.Parabola(focus, center, normal)
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        # ::GeomArcOfParabola::Restore() reads "Focal", "AngleXU", "StartAngle" and
        # "EndAngle", then calls GC_MakeArcOfParabola(..., Standard_True).
        return (
            Part.ArcOfParabola(
                parabola,
                number_field(item, "startAngle"),
                number_field(item, "endAngle"),
                True,
            ),
            construction,
        )
    if kind in {"BSpline", "BSplineCurve", "GeomBSplineCurve"}:
        degree = int_field(item, "degree")
        raw_poles = item.get("poles")
        if not isinstance(raw_poles, list):
            raise UnsupportedFixture("BSpline.poles must be a list")
        poles = [vector_value(FreeCAD, pole, "BSpline.poles") for pole in raw_poles]
        knot_count = len(poles) - degree + 1
        if degree < 1 or knot_count < 2:
            raise UnsupportedFixture("BSpline requires at least degree + 1 poles")
        knots = [float(index) / float(knot_count - 1) for index in range(knot_count)]
        multiplicities = [1] * knot_count
        multiplicities[0] = degree + 1
        multiplicities[-1] = degree + 1
        curve = Part.BSplineCurve()
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Geometry.cpp
        # GeomBSplineCurve serializes "Poles", "Knots", "Multiplicity" and "Degree"; cad-core
        # fixture BSplines use the same non-periodic clamped knot construction.
        curve.buildFromPolesMultsKnots(poles, multiplicities, knots, False, degree)
        return curve, construction

    raise UnsupportedFixture(f"unsupported Sketch Geometry kind {kind}")


def set_sketch_geometry(FreeCAD: Any, obj: Any, value: Any) -> None:
    if not isinstance(value, list):
        raise UnsupportedFixture("Sketch Geometry must be a list")
    for item in value:
        if not isinstance(item, dict):
            raise UnsupportedFixture("Sketch Geometry items must be objects")
        geometry, construction = sketch_geometry_value(FreeCAD, item)
        obj.addGeometry(geometry, construction)


def external_geometry_facade(geometry: Any) -> Any:
    import Sketcher  # type: ignore

    return Sketcher.ExternalGeometryFacade(geometry)


def apply_external_geometry_metadata(geometry: Any, item: dict, index: int) -> Any:
    facade = external_geometry_facade(geometry)
    try:
        facade.Construction = bool(item.get("construction", True))
    except Exception as exc:
        raise UnsupportedFixture(f"failed to set ExternalGeo construction flag: {exc}") from exc
    try:
        facade.Id = int(item.get("Id", index + 1))
    except Exception as exc:
        raise UnsupportedFixture(f"failed to set ExternalGeo Id: {exc}") from exc
    ref = item.get("Ref")
    if isinstance(ref, str):
        try:
            facade.Ref = ref
        except Exception as exc:
            raise UnsupportedFixture(f"failed to set ExternalGeo Ref {ref}: {exc}") from exc
    for flag in external_geometry_flags_from_item(item):
        try:
            facade.setFlag(flag, True)
        except Exception as exc:
            raise UnsupportedFixture(f"failed to set ExternalGeo flag {flag}: {exc}") from exc
    return facade.Geometry


def set_sketch_external_geo(FreeCAD: Any, obj: Any, value: Any) -> None:
    if not isinstance(value, dict) or value.get("PropertyType") != "Part::PropertyGeometryList":
        raise UnsupportedFixture("Sketch ExternalGeo must be Part::PropertyGeometryList")
    items = list_field(value, "Geometry", "Values", "Items")
    if not items:
        raise UnsupportedFixture("Sketch ExternalGeo must contain Geometry items")

    geos = []
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            raise UnsupportedFixture("Sketch ExternalGeo Geometry items must be objects")
        geometry, _construction = sketch_geometry_value(FreeCAD, item)
        geos.append(apply_external_geometry_metadata(geometry, item, index))

    # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
    # ::SketchObject::fixMissingAxisInExternalGeo() expects the first two ExternalGeo entries to
    # be the root horizontal / vertical axes. CAD Core fixtures only declare persisted native
    # external entries, so preserve the SketchObject-created root axes and append the fixture geos.
    existing = list(getattr(obj, "ExternalGeo", []) or [])
    obj.ExternalGeo = existing[:2] + geos


def set_sketch_property(FreeCAD: Any, created: dict[str, Any], obj: Any, name: str, value: Any) -> None:
    if name == "Geometry":
        set_sketch_geometry(FreeCAD, obj, value)
        return
    if name == "Constraints":
        if value:
            raise UnsupportedFixture("Sketch Constraints are not enabled in this collector yet")
        return
    if name == "ExternalGeo":
        set_sketch_external_geo(FreeCAD, obj, value)
        return
    if name == "ExternalGeometry":
        set_sketch_external_geometry(created, obj, value)
        return
    set_property(FreeCAD, created, obj, name, value)


def set_sketch_external_geometry(created: dict[str, Any], obj: Any, value: Any) -> None:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyLinkSubList":
        raise UnsupportedFixture("Sketch ExternalGeometry must be App::PropertyLinkSubList")
    sub_set = value.get("SubSet")
    if not isinstance(sub_set, list):
        raise UnsupportedFixture("Sketch ExternalGeometry.SubSet must be a list")

    for item in sub_set:
        if not isinstance(item, dict):
            raise UnsupportedFixture("Sketch ExternalGeometry.SubSet items must be objects")
        target_name = item.get("value")
        if target_name not in created:
            raise UnsupportedFixture(f"external geometry target {target_name} was not created")

        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        # /SketchObjectExternal.cpp::SketchObject::addExternal(), the Python wrapper takes
        # "ObjectName" and "SubName", then stores ExternalGeometry.setValues(Objects,
        # SubElements). In oracle mode, feed StableSubList first to collect post-resolution
        # geometry for CAD Core stable-subname fixtures.
        flags = external_geometry_flags_from_item(item)
        for subname in list_field(item, "StableSubList", "SubList"):
            external_target_name, external_subname = resolve_external_subname(created, target_name, subname)
            try:
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectPyImp.cpp
                # ::SketchObjectPy::addExternal(), parses "ss|O!O!" and forwards "defining" to
                # /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                # ::SketchObject::addExternal(), which sets ExternalGeometryExtension::Defining
                # during rebuildExternalGeometry().
                obj.addExternal(external_target_name, external_subname, "Defining" in flags)
            except Exception as exc:
                raise UnsupportedFixture(
                    f"Sketch ExternalGeometry cannot add {external_target_name}.{external_subname}"
                    f" from {target_name}.{subname}: {exc}"
                ) from exc


def safe_setattr(obj: Any, name: str, value: Any) -> None:
    try:
        setattr(obj, name, value)
    except Exception as exc:
        raise UnsupportedFixture(f"failed to set property {name}: {exc}") from exc


def set_property(FreeCAD: Any, created: dict[str, Any], obj: Any, name: str, value: Any) -> None:
    if isinstance(value, dict):
        property_type = value.get("PropertyType")
        if value.get("__assembly_joint_reference"):
            safe_setattr(obj, name, assembly_joint_reference_value(created, value))
            return
        if property_type == "App::PropertyPlacement":
            if name == "Placement":
                set_placement(FreeCAD, obj, value)
            else:
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py
                # ::Joint.__init__() creates FeaturePython joint properties "Placement1" /
                # "Placement2"; these are property slots on the joint, not the object's own
                # App::GeoFeature "Placement".
                safe_setattr(obj, name, placement_value(FreeCAD, value))
            return
        if property_type in {
            "App::PropertyBool",
            "App::PropertyEnumeration",
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
        if property_type in {
            "App::PropertyLink",
            "App::PropertyLinkGlobal",
            "App::PropertyLinkHidden",
            "App::PropertyXLink",
        }:
            safe_setattr(obj, name, link_sub_value(created, value))
            return
        if property_type in {
            "App::PropertyLinkSub",
            "App::PropertyLinkSubHidden",
            "App::PropertyXLinkSub",
            "App::PropertyXLinkSubHidden",
        }:
            safe_setattr(obj, name, link_sub_value(created, value))
            return
        if property_type in {"App::PropertyLinkList", "App::PropertyLinkListHidden"}:
            safe_setattr(obj, name, [created[target] for target in list_field(value, "values", "value")])
            return
        if property_type == "App::PropertyLinkSubListHidden":
            safe_setattr(obj, name, [link_sub_value(created, item) for item in value.get("SubSet", [])])
            return
        raise UnsupportedFixture(f"unsupported structured property {name}: {property_type or sorted(value)}")
    if isinstance(value, list) and any(isinstance(item, dict) for item in value):
        raise UnsupportedFixture(f"unsupported link/list property {name}")
    if name == "FileName" and isinstance(value, str):
        value = fixture_file_name(value)
    safe_setattr(obj, name, value)


def set_link_visibility_list(obj: Any, value: Any) -> None:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyBoolList":
        raise UnsupportedFixture("App::Link VisibilityList must be an App::PropertyBoolList")

    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    # ::LinkBaseExtension::extensionSetElementVisible() writes the immutable
    # "VisibilityList" through setElementVisible(), using getElementIndex() for ShowElement
    # links and getArrayIndex() for collapsed ElementCount links.
    for index, visible in enumerate(value.get("value", [])):
        try:
            result = obj.setElementVisible(str(index), bool(visible))
        except Exception as exc:
            raise UnsupportedFixture(f"failed to set Link VisibilityList[{index}]: {exc}") from exc
        if int(result) < 0:
            raise UnsupportedFixture(f"failed to set Link VisibilityList[{index}]: setElementVisible returned {result}")


def link_group_element_names(fixture: dict) -> set[str]:
    names: set[str] = set()
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") != "App::LinkGroup":
            continue
        element_list = spec.get("Properties", {}).get("ElementList")
        if isinstance(element_list, dict) and element_list.get("PropertyType") == "App::PropertyLinkList":
            names.update(str(item) for item in list_field(element_list, "values", "value"))
    return names


def create_native_object_for_fixture(
    FreeCAD: Any,
    doc: Any,
    type_id: str,
    name: str,
    link_group_elements: set[str],
) -> Any:
    if type_id == "App::LinkElement" and name in link_group_elements:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
        # ::LinkBaseExtension::setLink() accepts normal App::Link children for LinkGroup
        # "ElementList". Standalone ownerless LinkElement objects can be created for CAD Core
        # fixtures, but inserting them into native LinkGroup.ElementList crashes FreeCADCmd;
        # use App::Link as the native geometry proxy while keeping the fixture schema unchanged.
        return doc.addObject("App::Link", name)
    return create_native_object(FreeCAD, doc, type_id, name)


def set_link_group_element_list(created: dict[str, Any], obj: Any, value: Any) -> None:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyLinkList":
        raise UnsupportedFixture("App::LinkGroup ElementList must be an App::PropertyLinkList")
    elements = []
    for target in list_field(value, "values", "value"):
        if target not in created:
            raise UnsupportedFixture(f"LinkGroup element {target} was not created")
        elements.append(created[target])

    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    # ::LinkBaseExtension::setLink(), in the LinkGroup assignment branch, updates
    # "ElementList" through the extension API instead of writing the property directly.
    obj.setLink(elements)


def link_owner_fixture_id(value: Any) -> int | None:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyInteger":
        return None
    try:
        return int(value.get("value", 0))
    except (TypeError, ValueError):
        return None


def try_set_link_owner(
    obj: Any,
    value: Any,
    fixture_id_to_actual_id: dict[int, int],
) -> bool:
    owner_fixture_id = link_owner_fixture_id(value)
    if owner_fixture_id is None:
        raise UnsupportedFixture("App::LinkElement _LinkOwner must be an App::PropertyInteger")
    if owner_fixture_id == 0:
        safe_setattr(obj, "_LinkOwner", 0)
        return True
    actual_owner_id = fixture_id_to_actual_id.get(owner_fixture_id)
    if actual_owner_id is None:
        return False

    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
    # ::LinkElement::canDelete() and ::LinkBaseExtension::update() compare "_LinkOwner"
    # against DocumentObject::getID() via "getObjectByID(_LinkOwner.getValue())"; fixture
    # IDs must therefore be mapped onto the native FreeCAD document IDs during collection.
    safe_setattr(obj, "_LinkOwner", actual_owner_id)
    return True


def flush_deferred_link_owners(
    deferred_link_owners: list[tuple[Any, Any]],
    fixture_id_to_actual_id: dict[int, int],
) -> None:
    unresolved: list[tuple[Any, Any]] = []
    for obj, value in deferred_link_owners:
        if not try_set_link_owner(obj, value, fixture_id_to_actual_id):
            unresolved.append((obj, value))
    deferred_link_owners[:] = unresolved


def set_body_property(created: dict[str, Any], obj: Any, name: str, value: Any) -> bool:
    if not isinstance(value, dict):
        return False
    property_type = value.get("PropertyType")
    if name == "Group" and property_type == "App::PropertyLinkList":
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BodyBase.cpp
        # ::BodyBase::addObject() is the supported membership path; writing Group directly skips
        # the same ownership bookkeeping that PartDesign recompute depends on.
        for target in list_field(value, "values", "value"):
            if target not in created:
                raise UnsupportedFixture(f"body member {target} was not created")
            obj.addObject(created[target])
        return True
    return False


def link_sub_value_with_empty_datum_subname(
    created: dict[str, Any],
    value: dict,
    empty_subname_type_ids: set[str],
) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"link target {target_name} was not created")
    target = created[target_name]
    sub_list = list_field(value, "StableSubList", "SubList")
    if sub_list:
        return target, sub_list
    if getattr(target, "TypeId", "") in empty_subname_type_ids:
        return target, [""]
    return link_sub_value(created, value)


def set_linear_pattern_property(created: dict[str, Any], obj: Any, name: str, value: Any) -> bool:
    if name in {"Direction", "Direction2"} and isinstance(value, dict):
        property_type = value.get("PropertyType")
        if property_type in {"App::PropertyLinkSub", "App::PropertyXLinkSub"}:
            # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App
            # /FeatureLinearPattern.cpp::LinearPattern::getDirectionFromProperty() rejects an
            # empty PropertyLinkSub sub-value list before it checks whether the target is a
            # DatumLine/DatumPlane. cad-core fixtures model datum directions with an empty SubList,
            # so the collector feeds a single empty subname to preserve fixture schema while using
            # FreeCAD's native direction path.
            safe_setattr(
                obj,
                name,
                link_sub_value_with_empty_datum_subname(
                    created,
                    value,
                    {"PartDesign::Line", "PartDesign::Plane"},
                ),
            )
            return True
    return False


def set_polar_pattern_property(created: dict[str, Any], obj: Any, name: str, value: Any) -> bool:
    if name == "Axis" and isinstance(value, dict):
        property_type = value.get("PropertyType")
        if property_type in {"App::PropertyLinkSub", "App::PropertyXLinkSub"}:
            # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App
            # /FeaturePolarPattern.cpp::PolarPattern::getRotation() returns the default axis when
            # Axis.getSubValues() is empty before it checks whether the target is a DatumLine.
            # cad-core fixtures model DatumLine axes with an empty SubList, so native collection
            # supplies one empty subname while keeping the fixture schema unchanged.
            safe_setattr(
                obj,
                name,
                link_sub_value_with_empty_datum_subname(created, value, {"PartDesign::Line"}),
            )
            return True
    return False


def create_native_object(FreeCAD: Any, doc: Any, type_id: str, name: str) -> Any:
    if type_id == "Part::BooleanFragments":
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools
        # /SplitFeatures.py::makeBooleanFragments(), creates "Part::FeaturePython" and
        # attaches FeatureBooleanFragments with Objects / Mode / Tolerance properties.
        FreeCAD.setActiveDocument(doc.Name)
        from BOPTools import SplitFeatures  # type: ignore

        return SplitFeatures.makeBooleanFragments(name)
    if type_id == "Part::XOR":
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/BOPTools
        # /SplitFeatures.py::makeXOR(), creates "Part::FeaturePython" and attaches
        # FeatureXOR; execute() delegates to SplitAPI.xor(shapes, Tolerance).
        FreeCAD.setActiveDocument(doc.Name)
        from BOPTools import SplitFeatures  # type: ignore

        return SplitFeatures.makeXOR(name)
    return doc.addObject(type_id, name)


def scalar_property_value(value: Any) -> Any:
    if isinstance(value, dict) and "value" in value:
        return value.get("value")
    return value


def initialize_assembly_feature_python(created: dict[str, Any], obj: Any, properties: dict[str, Any]) -> None:
    if "ObjectToGround" in properties:
        target_name = scalar_property_value(properties["ObjectToGround"])
        if target_name not in created:
            raise UnsupportedFixture(f"grounded joint target {target_name} was not created")
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py
        # ::GroundedJoint.__init__() adds "ObjectToGround" to App::FeaturePython joint objects.
        import JointObject  # type: ignore

        JointObject.GroundedJoint(obj, created[target_name])
        return

    if "JointType" in properties:
        joint_type = scalar_property_value(properties["JointType"])
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/JointObject.py
        # ::Joint.__init__() adds "JointType", "Reference1" and "Reference2"; JointTypes keeps
        # the user-facing enum names used by Assembly fixtures.
        import JointObject  # type: ignore

        try:
            type_index = list(JointObject.JointTypes).index(joint_type)
        except ValueError as exc:
            raise UnsupportedFixture(f"unsupported Assembly JointType {joint_type}") from exc
        JointObject.Joint(obj, type_index)
        return

    raise UnsupportedFixture("App::FeaturePython is only enabled for Assembly Joint/GroundedJoint fixtures")


def has_assembly_objects(fixture: dict) -> bool:
    assembly_types = {"Assembly::AssemblyObject", "Assembly::AssemblyLink", "Assembly::JointGroup"}
    return any(spec.get("TypeId") in assembly_types for spec in fixture.get("Objects", []))


def fixture_parent_by_child(fixture: dict) -> dict[str, str]:
    parent_by_child: dict[str, str] = {}
    for spec in fixture.get("Objects", []):
        name = spec.get("Name")
        group = spec.get("Properties", {}).get("Group")
        if not name or not isinstance(group, dict) or group.get("PropertyType") != "App::PropertyLinkList":
            continue
        for child_name in list_field(group, "values", "value"):
            parent_by_child[str(child_name)] = str(name)
    return parent_by_child


def create_parented_object(doc: Any, parent: Any, type_id: str, name: str) -> Any:
    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/CommandCreateAssembly.py
    # creates JointGroup via "assembly.newObject", and
    # /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/AssemblyTests/TestCore.py
    # creates Joint App::FeaturePython objects via "jointgroup.newObject".
    try:
        return parent.newObject(type_id, name)
    except Exception as exc:
        raise UnsupportedFixture(f"FreeCAD cannot add {type_id} under {parent.Name}: {exc}") from exc


def create_assembly_objects(FreeCAD: Any, doc: Any, fixture: dict) -> dict[str, Any]:
    created: dict[str, Any] = {}
    specs = {spec["Name"]: spec for spec in fixture.get("Objects", [])}
    parent_by_child = fixture_parent_by_child(fixture)
    parented_types = {"Assembly::AssemblyLink", "Assembly::JointGroup", "App::FeaturePython"}

    def create_spec(name: str) -> Any:
        if name in created:
            return created[name]
        if name not in specs:
            raise UnsupportedFixture(f"assembly child {name} has no object spec")
        spec = specs[name]
        type_id = spec["TypeId"]
        if type_id not in SUPPORTED_NATIVE_TYPES:
            raise UnsupportedFixture(f"{type_id} is not enabled in this collector yet")

        parent_name = parent_by_child.get(name)
        if parent_name and type_id in parented_types:
            parent = create_spec(parent_name)
            obj = create_parented_object(doc, parent, type_id, name)
        else:
            try:
                obj = create_native_object(FreeCAD, doc, type_id, name)
            except Exception as exc:
                raise UnsupportedFixture(f"FreeCAD cannot add {type_id}: {exc}") from exc
        created[name] = obj
        return obj

    for spec in fixture.get("Objects", []):
        create_spec(spec["Name"])

    for spec in fixture.get("Objects", []):
        if spec["TypeId"] == "App::FeaturePython":
            initialize_assembly_feature_python(created, created[spec["Name"]], spec.get("Properties", {}))

    for spec in fixture.get("Objects", []):
        obj = created[spec["Name"]]
        for prop_name, prop_value in spec.get("Properties", {}).items():
            if spec["TypeId"] == "Assembly::JointGroup" and prop_name == "Group":
                # Parent-created Joint objects are already owned by JointGroup. Reassigning the
                # same child list through the raw Group property produces native out-of-scope
                # warnings in FreeCADCmd and is not needed for the oracle document shape.
                continue
            if spec["TypeId"] == "App::FeaturePython" and prop_name in {"Reference1", "Reference2"}:
                set_property(FreeCAD, created, obj, prop_name, {
                    **prop_value,
                    "__assembly_joint_reference": True,
                })
                continue
            if spec["TypeId"] == "Sketcher::SketchObject":
                set_sketch_property(FreeCAD, created, obj, prop_name, prop_value)
            else:
                set_property(FreeCAD, created, obj, prop_name, prop_value)
    return created


def create_objects(FreeCAD: Any, doc: Any, fixture: dict) -> dict[str, Any]:
    if has_assembly_objects(fixture):
        return create_assembly_objects(FreeCAD, doc, fixture)

    created: dict[str, Any] = {}
    link_group_elements = link_group_element_names(fixture)
    fixture_id_to_actual_id: dict[int, int] = {}
    deferred_link_owners: list[tuple[Any, Any]] = []
    deferred_after_shape: list[tuple[Any, str, Any]] = []
    deferred_external_geo: list[tuple[Any, Any]] = []
    deferred_external_geometry: list[tuple[Any, Any]] = []
    deferred_after_body: list[tuple[str, Any, str, Any]] = []
    for spec in fixture.get("Objects", []):
        name = spec["Name"]
        type_id = spec["TypeId"]
        if type_id not in SUPPORTED_NATIVE_TYPES:
            raise UnsupportedFixture(f"{type_id} is not enabled in this collector yet")
        try:
            obj = create_native_object_for_fixture(FreeCAD, doc, type_id, name, link_group_elements)
        except Exception as exc:
            raise UnsupportedFixture(f"FreeCAD cannot add {type_id}: {exc}") from exc
        created[name] = obj
        if "ID" in spec:
            fixture_id_to_actual_id[int(spec["ID"])] = int(getattr(obj, "ID"))
            flush_deferred_link_owners(deferred_link_owners, fixture_id_to_actual_id)
        properties = spec.get("Properties", {})
        if type_id == "App::FeaturePython":
            initialize_assembly_feature_python(created, obj, properties)
        deferred_link_element_count: tuple[str, Any] | None = None
        deferred_link_visibility: Any | None = None
        link_show_element = True
        if type_id == "App::Link":
            show_element = properties.get("ShowElement")
            if isinstance(show_element, dict):
                link_show_element = bool(show_element.get("value", True))
            elif show_element is not None:
                link_show_element = bool(show_element)

        for prop_name, prop_value in properties.items():
            if type_id == "PartDesign::Body" and set_body_property(created, obj, prop_name, prop_value):
                continue
            if type_id == "PartDesign::LinearPattern" and set_linear_pattern_property(created, obj, prop_name, prop_value):
                continue
            if type_id == "PartDesign::PolarPattern" and set_polar_pattern_property(created, obj, prop_name, prop_value):
                continue
            if type_id == "PartDesign::Hole" and prop_name not in HOLE_PRE_BODY_PROPERTIES:
                deferred_after_body.append((type_id, obj, prop_name, prop_value))
                continue
            if type_id == "Sketcher::SketchObject" and prop_name in SKETCH_SHAPE_DEPENDENT_PROPERTIES:
                deferred_after_shape.append((obj, prop_name, prop_value))
                continue
            if type_id == "Sketcher::SketchObject" and prop_name == "ExternalGeo":
                deferred_external_geo.append((obj, prop_value))
                continue
            if type_id == "Sketcher::SketchObject" and prop_name == "ExternalGeometry":
                deferred_external_geometry.append((obj, prop_value))
                continue
            if type_id == "App::LinkElement" and prop_name == "_LinkOwner":
                if not try_set_link_owner(obj, prop_value, fixture_id_to_actual_id):
                    deferred_link_owners.append((obj, prop_value))
                continue
            if type_id == "App::LinkGroup" and prop_name == "ElementList":
                set_link_group_element_list(created, obj, prop_value)
                continue
            if type_id == "App::Link" and prop_name == "ElementCount" and link_show_element:
                # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp
                # ::LinkBaseExtension::update(), in the ShowElement ElementCount branch,
                # creates/re-claims owner "_iN" children and consumes current
                # "PlacementList" / "ScaleList" values before clearing those lists.
                # Restore declarative fixtures by setting owner lists first.
                deferred_link_element_count = (prop_name, prop_value)
                continue
            if type_id in {"App::Link", "App::LinkGroup"} and prop_name == "VisibilityList":
                deferred_link_visibility = prop_value
                continue
            if type_id == "Sketcher::SketchObject":
                set_sketch_property(FreeCAD, created, obj, prop_name, prop_value)
            else:
                set_property(FreeCAD, created, obj, prop_name, prop_value)

        if deferred_link_element_count is not None:
            prop_name, prop_value = deferred_link_element_count
            set_property(FreeCAD, created, obj, prop_name, prop_value)
        if deferred_link_visibility is not None:
            set_link_visibility_list(obj, deferred_link_visibility)

    flush_deferred_link_owners(deferred_link_owners, fixture_id_to_actual_id)
    if deferred_link_owners:
        unresolved = ", ".join(str(link_owner_fixture_id(value)) for _, value in deferred_link_owners)
        raise UnsupportedFixture(f"unresolved App::LinkElement _LinkOwner fixture IDs: {unresolved}")

    if deferred_after_shape or deferred_after_body:
        doc.recompute()

    if deferred_after_shape:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/AttachExtension.cpp
        # ::AttachExtension::positionBySupport() resolves AttachmentSupport subshapes during
        # MapMode updates. Defer support/map restoration until referenced Pad/Body shapes exist.
        for obj, prop_name, prop_value in deferred_after_shape:
            set_sketch_property(FreeCAD, created, obj, prop_name, prop_value)
        doc.recompute()

    if deferred_after_body:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
        # ::Body::setBaseProperty() wires a PartDesign solid feature's BaseFeature when
        # Body::addObject() consumes Group. Hole::onChanged(Depth) then calls
        # getThroughAllLength(), so Hole depth/thread/head-cut properties must be restored
        # only after Body membership and a recompute have made the base shape available.
        for type_id, obj, prop_name, prop_value in deferred_after_body:
            if type_id == "Sketcher::SketchObject":
                set_sketch_property(FreeCAD, created, obj, prop_name, prop_value)
            else:
                set_property(FreeCAD, created, obj, prop_name, prop_value)

    if deferred_external_geometry:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        # /SketchObjectExternal.cpp::SketchObject::addExternal() resolves the target object's
        # current TopoShape before storing ExternalGeometry. Recompute the fixture graph once so
        # sketch/body/pad targets have Shape data before external references are added.
        doc.recompute()
        for obj, prop_value in deferred_external_geometry:
            set_sketch_external_geometry(created, obj, prop_value)

    if deferred_external_geo:
        # Restore persisted old ExternalGeo only after addExternal() has established the native
        # ExternalGeometry link list. The final collect_one() recompute then exercises FreeCAD's
        # Frozen / Sync / Detached / Missing state machine from a real document state.
        for obj, prop_value in deferred_external_geo:
            set_sketch_external_geo(FreeCAD, obj, prop_value)
    return created


def shape_summary(shape: Any) -> dict:
    try:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App
        # /TopoShapePyImp.cpp::TopoShapePy::optimalBoundingBox(), exposes
        # "BRepBndLib::AddOptimal" for Python. CAD Core serializes the same tighter bbox
        # class for object metadata, while FreeCAD's Shape.BoundBox is the looser display box.
        bbox = shape.optimalBoundingBox()
    except Exception:
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


def part_geometry_curve_items(fixture: dict) -> list[dict[str, Any]]:
    payload = fixture.get("partGeometryCurve")
    if payload is None:
        return []
    if isinstance(payload, list):
        return [item for item in payload if isinstance(item, dict)]
    if isinstance(payload, dict):
        return [payload]
    raise UnsupportedFixture("partGeometryCurve must be an object or a list of objects")


def part_geometry_curve_consumer_items(fixture: dict) -> list[dict[str, Any]]:
    payload = fixture.get("partGeometryCurveConsumers")
    if payload is None:
        return []
    if isinstance(payload, list):
        return [item for item in payload if isinstance(item, dict)]
    if isinstance(payload, dict):
        return [payload]
    raise UnsupportedFixture("partGeometryCurveConsumers must be an object or a list of objects")


def set_part_conic_curve_common(FreeCAD: Any, curve: Any, dto: dict[str, Any]) -> None:
    curve.Center = FreeCAD.Vector(*(float(item) for item in dto["center"]))
    curve.Axis = FreeCAD.Vector(*(float(item) for item in dto["normal"]))
    curve.AngleXU = float(dto["angleXU"])


def build_part_geometry_curve_shape(FreeCAD: Any, Part: Any, dto: dict[str, Any]) -> Any:
    curve_kind = str(dto.get("curveKind", "")).lower()
    if curve_kind == "hyperbola":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        # ::GeomHyperbola::Save/Restore() persists Center/Normal/MajorRadius/MinorRadius/AngleXU;
        # ::GeomArcOfHyperbola::Restore() adds StartAngle/EndAngle and calls
        # GC_MakeArcOfHyperbola(..., Standard_True). The collector mirrors that wrapper path.
        curve = Part.Hyperbola()
        set_part_conic_curve_common(FreeCAD, curve, dto)
        curve.MajorRadius = float(dto["majorRadius"])
        curve.MinorRadius = float(dto["minorRadius"])
        return Part.ArcOfHyperbola(curve, float(dto["startAngle"]), float(dto["endAngle"]), True).toShape()
    if curve_kind == "parabola":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp
        # ::GeomParabola::Save/Restore() persists Center/Normal/Focal/AngleXU;
        # ::GeomArcOfParabola::Restore() adds StartAngle/EndAngle and calls
        # GC_MakeArcOfParabola(..., Standard_True). The collector mirrors that wrapper path.
        curve = Part.Parabola()
        set_part_conic_curve_common(FreeCAD, curve, dto)
        curve.Focal = float(dto["focal"])
        return Part.ArcOfParabola(curve, float(dto["startAngle"]), float(dto["endAngle"]), True).toShape()
    raise UnsupportedFixture(f"unsupported partGeometryCurve curveKind {dto.get('curveKind')}")


def part_geometry_curve_metadata(dto: dict[str, Any]) -> dict[str, str]:
    curve_kind = str(dto.get("curveKind", "")).lower()
    curve_type = "GeomAbs_Hyperbola" if curve_kind == "hyperbola" else "GeomAbs_Parabola"
    part_geometry_type = "Part.Hyperbola" if curve_kind == "hyperbola" else "Part.Parabola"
    return {
        "feature": "part_geometry_curve",
        "dto": "PartConicCurveDTO",
        "curve_kind": curve_kind,
        "curve_type": curve_type,
        "part_geometry_type": part_geometry_type,
    }


def part_geometry_curve_object_expected(shape: Any, dto: dict[str, Any]) -> dict:
    summary = shape_summary(shape)
    edge = shape.Edges[0]
    metadata = part_geometry_curve_metadata(dto)
    summary["length"] = float(edge.Length)
    summary["length_delta"] = 1e-5
    summary["object_fields"] = {
        "status": "ok",
        "shape": "occt_edge",
        "feature": "part_geometry_curve",
        "dto": "PartConicCurveDTO",
        **metadata,
    }
    return summary


def property_payload_value(value: Any) -> Any:
    if isinstance(value, dict) and "PropertyType" in value and "value" in value:
        return value.get("value")
    return value


def consumer_property(properties: dict[str, Any], name: str, fallback: Any = None) -> Any:
    if name not in properties:
        return fallback
    return property_payload_value(properties[name])


def consumer_number_property(properties: dict[str, Any], name: str, fallback: float = 0.0) -> float:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, (int, float)):
        raise UnsupportedFixture(f"partGeometryCurve consumer {name} must be numeric")
    return float(value)


def consumer_bool_property(properties: dict[str, Any], name: str, fallback: bool = False) -> bool:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, bool):
        raise UnsupportedFixture(f"partGeometryCurve consumer {name} must be boolean")
    return value


def consumer_vector_property(properties: dict[str, Any], name: str, fallback: list[float]) -> list[float]:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, list) or len(value) != 3 or not all(isinstance(item, (int, float)) for item in value):
        raise UnsupportedFixture(f"partGeometryCurve consumer {name} must be a three-number vector")
    return [float(item) for item in value]


def consumer_link_property(properties: dict[str, Any], name: str) -> str:
    value = consumer_property(properties, name)
    if not isinstance(value, str) or not value:
        raise UnsupportedFixture(f"partGeometryCurve consumer {name} must be an object link")
    return value


def collect_part_geometry_curve_extrusion_expected(
    FreeCAD: Any,
    consumer: dict[str, Any],
    source_shapes: dict[str, Any],
    source_metadata: dict[str, dict[str, str]],
) -> dict:
    if consumer.get("TypeId") != "Part::Extrusion":
        raise UnsupportedFixture("partGeometryCurveConsumers expected collection supports Part::Extrusion")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("partGeometryCurve consumer Properties must be an object")

    dir_mode = consumer_property(properties, "DirMode", "Custom")
    if dir_mode not in {"Custom", 0}:
        raise UnsupportedFixture("partGeometryCurve consumer oracle only supports Part::Extrusion DirMode=Custom")
    if consumer_bool_property(properties, "Solid", False):
        raise UnsupportedFixture("partGeometryCurve consumer oracle only supports Part::Extrusion Solid=false")
    if abs(consumer_number_property(properties, "TaperAngle", 0.0)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("partGeometryCurve consumer oracle does not publish tapered extrusion")
    if abs(consumer_number_property(properties, "TaperAngleRev", 0.0)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("partGeometryCurve consumer oracle does not publish tapered reverse extrusion")

    base_name = consumer_link_property(properties, "Base")
    if base_name not in source_shapes:
        raise UnsupportedFixture(f"partGeometryCurve consumer Base {base_name} was not created")

    direction = consumer_vector_property(properties, "Dir", [0.0, 0.0, 1.0])
    magnitude = math.sqrt(sum(component * component for component in direction))
    if magnitude <= FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("partGeometryCurve consumer Dir must not be zero-length")
    unit = [component / magnitude for component in direction]
    if consumer_bool_property(properties, "Reversed", False):
        unit = [-component for component in unit]

    length_fwd = consumer_number_property(properties, "LengthFwd", 0.0)
    length_rev = consumer_number_property(properties, "LengthRev", 0.0)
    if abs(length_fwd) <= FREECAD_PRECISION_CONFUSION and abs(length_rev) <= FREECAD_PRECISION_CONFUSION:
        length_fwd = magnitude
    if consumer_bool_property(properties, "Symmetric", False):
        length_rev = length_fwd * 0.5
        length_fwd = length_fwd * 0.5
    if abs(length_fwd + length_rev) <= FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("partGeometryCurve consumer total extrusion length must not be zero")

    source_shape = source_shapes[base_name].copy()
    if abs(length_rev) > FREECAD_PRECISION_CONFUSION:
        source_shape.translate(FreeCAD.Vector(*(component * -length_rev for component in unit)))
    result_shape = source_shape.extrude(FreeCAD.Vector(*(component * (length_fwd + length_rev) for component in unit)))
    summary = shape_summary(result_shape)
    source_info = source_metadata[base_name]
    summary["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(result_shape),
        "feature": "part_extrusion",
        "source_base": base_name,
        "solid": False,
        "length_fwd": length_fwd,
        "length_rev": length_rev,
        "reversed": consumer_bool_property(properties, "Reversed", False),
        "symmetric": consumer_bool_property(properties, "Symmetric", False),
        "source_feature": "part_geometry_curve",
        "source_dto": "PartConicCurveDTO",
        "source_curve_kind": source_info["curve_kind"],
        "source_curve_type": source_info["curve_type"],
        "source_part_geometry_type": source_info["part_geometry_type"],
    }
    return summary


def collect_part_geometry_curve_line_expected(
    FreeCAD: Any,
    Part: Any,
    consumer: dict[str, Any],
    source_shapes: dict[str, Any],
    source_metadata: dict[str, dict[str, str]],
) -> dict:
    if consumer.get("TypeId") != "Part::Line":
        raise UnsupportedFixture("partGeometryCurveConsumers line source must be Part::Line")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("partGeometryCurve line consumer Properties must be an object")
    start = FreeCAD.Vector(
        consumer_number_property(properties, "X1", 0.0),
        consumer_number_property(properties, "Y1", 0.0),
        consumer_number_property(properties, "Z1", 0.0),
    )
    end = FreeCAD.Vector(
        consumer_number_property(properties, "X2", 0.0),
        consumer_number_property(properties, "Y2", 0.0),
        consumer_number_property(properties, "Z2", 0.0),
    )
    shape = Part.makeLine(start, end)
    name = str(consumer.get("Name") or "Line")
    source_shapes[name] = shape
    source_metadata[name] = {"feature": "part_line"}
    summary = shape_summary(shape)
    summary["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "primitive": "line",
    }
    return summary


def consumer_enum_property(properties: dict[str, Any], name: str, labels: list[str], fallback: str) -> str:
    value = consumer_property(properties, name, fallback)
    if isinstance(value, str) and value in labels:
        return value
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return labels[index]
    raise UnsupportedFixture(f"partGeometryCurve consumer {name} must be one of {', '.join(labels)}")


def prefixed_source_metadata(prefix: str, metadata: dict[str, str]) -> dict[str, str]:
    return {f"{prefix}_{key}": value for key, value in metadata.items()}


def collect_part_geometry_curve_ruled_surface_expected(
    Part: Any,
    consumer: dict[str, Any],
    source_shapes: dict[str, Any],
    source_metadata: dict[str, dict[str, str]],
) -> dict:
    if consumer.get("TypeId") != "Part::RuledSurface":
        raise UnsupportedFixture("partGeometryCurveConsumers ruled surface source must be Part::RuledSurface")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("partGeometryCurve ruled surface consumer Properties must be an object")

    curve1 = consumer_link_property(properties, "Curve1")
    curve2 = consumer_link_property(properties, "Curve2")
    for curve in (curve1, curve2):
        if curve not in source_shapes:
            raise UnsupportedFixture(f"partGeometryCurve consumer Curve source {curve} was not created")

    orientation = consumer_enum_property(
        properties,
        "Orientation",
        ["Automatic", "Forward", "Reversed"],
        "Automatic",
    )
    if orientation == "Reversed":
        second = source_shapes[curve2].copy()
        second.reverse()
    else:
        second = source_shapes[curve2]

    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp
    # ::RuledSurface::execute() resolves Curve1/Curve2 to two edges before calling
    # makeElementRuledSurface(); this fallback only freezes that post-resolution edge/edge geometry.
    result_shape = Part.makeRuledSurface(source_shapes[curve1], second)
    summary = shape_summary(result_shape)
    summary["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(result_shape),
        "feature": "part_ruled_surface",
        "source_curve1": curve1,
        "source_curve2": curve2,
        "orientation": orientation,
        **prefixed_source_metadata("source_curve1", source_metadata.get(curve1, {})),
        **prefixed_source_metadata("source_curve2", source_metadata.get(curve2, {})),
    }
    return summary


def collect_part_geometry_curve_expected(fixture_path: Path, fixture: dict) -> dict:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    items = part_geometry_curve_items(fixture)
    consumers = part_geometry_curve_consumer_items(fixture)
    if not items:
        raise UnsupportedFixture("partGeometryCurve expected collection requires at least one DTO")
    if not consumers and len(items) != 1:
        raise UnsupportedFixture("partGeometryCurve edge expected collection supports one valid DTO per fixture")

    source_shapes: dict[str, Any] = {}
    source_metadata: dict[str, dict[str, str]] = {}
    object_payloads: dict[str, dict] = {}
    for dto in items:
        object_name = str(dto.get("name") or "PartConicCurve")
        shape = build_part_geometry_curve_shape(FreeCAD, Part, dto)
        source_shapes[object_name] = shape
        source_metadata[object_name] = part_geometry_curve_metadata(dto)
        object_payloads[object_name] = part_geometry_curve_object_expected(shape, dto)

    for consumer in consumers:
        object_name = str(consumer.get("Name") or "PartConicCurveConsumer")
        type_id = consumer.get("TypeId")
        if type_id == "Part::Line":
            object_payloads[object_name] = collect_part_geometry_curve_line_expected(
                FreeCAD,
                Part,
                consumer,
                source_shapes,
                source_metadata,
            )
        elif type_id == "Part::Extrusion":
            object_payloads[object_name] = collect_part_geometry_curve_extrusion_expected(
                FreeCAD,
                consumer,
                source_shapes,
                source_metadata,
            )
        elif type_id == "Part::RuledSurface":
            object_payloads[object_name] = collect_part_geometry_curve_ruled_surface_expected(
                Part,
                consumer,
                source_shapes,
                source_metadata,
            )
        else:
            raise UnsupportedFixture(
                "partGeometryCurveConsumers expected collection supports Part::Line, "
                "Part::Extrusion and Part::RuledSurface"
            )

    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "reference": (
            f"FreeCADCmd PartConicCurveDTO oracle from {fixture_path.name}; "
            "Part::RuledSurface consumers use Part.makeRuledSurface after link-resolved edge inputs"
        ),
        "freecad_version": freecad_version(FreeCAD),
        "bbox_delta": 0.2,
        "named_shapes": {name: {"owner": name} for name in object_payloads},
    }
    if len(object_payloads) == 1:
        object_name, summary = next(iter(object_payloads.items()))
        payload["object"] = object_name
        payload.update(summary)
    else:
        payload["objects"] = object_payloads
    return payload


def shape_with_placement(shape: Any, placement: Any) -> Any:
    copied = shape.copy()
    copied.Placement = placement.multiply(copied.Placement)
    return copied


def native_display_shape(obj: Any, seen: set[str] | None = None) -> Any | None:
    seen = set() if seen is None else seen
    name = str(getattr(obj, "Name", ""))
    if name in seen:
        return None
    seen.add(name)

    type_id = getattr(obj, "TypeId", "")
    if type_id == "Assembly::AssemblyLink":
        return assembly_link_display_shape(obj, seen)
    if type_id == "Assembly::AssemblyObject":
        return assembly_object_display_shape(obj, seen)

    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        return None
    return shape


def assembly_link_display_shape(obj: Any, seen: set[str]) -> Any | None:
    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyLink.cpp
    # ::AssemblyLink::execute() calls "updateContents()" then "App::Part::execute()". FreeCAD
    # CLI does not expose a Part "Shape" for AssemblyLink, so expected collection summarizes
    # the linked display target with the link Placement while keeping solver migration separate.
    target = getattr(obj, "LinkedObject", None)
    if target is None:
        return None
    target_shape = native_display_shape(target, set(seen))
    if target_shape is None:
        return None
    return shape_with_placement(target_shape, obj.Placement)


def assembly_object_display_shape(obj: Any, seen: set[str]) -> Any | None:
    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    # ::AssemblyObject::execute() calls "App::Part::execute()" before "solve(false)"; cad-core
    # P8 keeps the grouped display shape but marks the solver path as "not_migrated".
    import Part  # type: ignore

    shapes = []
    for child in list(getattr(obj, "Group", [])):
        child_type = getattr(child, "TypeId", "")
        if child_type in {"Assembly::JointGroup", "App::FeaturePython"}:
            continue
        child_shape = native_display_shape(child, set(seen))
        if child_shape is not None:
            shapes.append(child_shape)
    if not shapes:
        return None
    if len(shapes) == 1:
        return shapes[0]
    return Part.makeCompound(shapes)


def link_name(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    return str(getattr(value, "Name", ""))


def link_target_name(value: Any) -> str:
    if isinstance(value, tuple) and value:
        return link_name(value[0])
    return link_name(value)


def link_names(values: Any) -> list[str]:
    return [link_name(item) for item in list(values or []) if link_name(item)]


def shape_kind(shape: Any) -> str:
    shape_type = str(getattr(shape, "ShapeType", ""))
    return {
        "Compound": "occt_compound",
        "CompSolid": "occt_compsolid",
        "Solid": "occt_solid",
        "Shell": "occt_shell",
        "Face": "occt_face",
        "Wire": "occt_wire",
        "Edge": "occt_edge",
        "Vertex": "occt_vertex",
    }.get(shape_type, "occt_shape")


def app_link_payload(obj: Any, role: str) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "link": role,
        "linked_object": link_target_name(getattr(obj, "LinkedObject", None)),
        "shape": shape_kind(shape),
    }
    return payload


def assembly_joint_feature_python_payload(obj: Any) -> dict:
    if hasattr(obj, "ObjectToGround"):
        return {
            "object_fields": {
                "assembly": "grounded_joint",
                "object_to_ground": link_name(getattr(obj, "ObjectToGround", None)),
                "solve": "not_migrated",
            }
        }

    if hasattr(obj, "JointType"):
        reference1 = getattr(obj, "Reference1", None)
        reference2 = getattr(obj, "Reference2", None)
        return {
            "object_fields": {
                "assembly": "joint",
                "joint_type": str(getattr(obj, "JointType", "")),
                "reference1": link_sub_payload(reference1),
                "reference2": link_sub_payload(reference2),
                "suppressed": bool(getattr(obj, "Suppressed", False)),
                "solve": "not_migrated",
            }
        }

    raise UnsupportedFixture(f"App::FeaturePython object {obj.Name} is not an Assembly joint")


def link_sub_payload(value: Any) -> dict:
    if isinstance(value, tuple) and value:
        target = value[0]
        subnames = list(value[1]) if len(value) > 1 else []
        return {"object": link_name(target), "subnames": [str(item) for item in subnames]}
    return {"object": link_name(value), "subnames": []}


def assembly_joint_group_payload(obj: Any) -> dict:
    group = list(getattr(obj, "Group", []))
    joints = [
        child
        for child in group
        if getattr(child, "TypeId", "") == "App::FeaturePython"
        and (hasattr(child, "JointType") or hasattr(child, "ObjectToGround"))
    ]
    return {
        "object_fields": {
            "assembly": "joint_group",
            "group": link_names(group),
            "joints": link_names(joints),
            "solve": "not_migrated",
        }
    }


def placement_payload(placement: Any) -> dict:
    base = placement.Base
    rotation = placement.Rotation
    q = list(rotation.Q)
    return {
        "PropertyType": "App::PropertyPlacement",
        "Base": [float(base.x), float(base.y), float(base.z)],
        "Rotation": [float(q[0]), float(q[1]), float(q[2]), float(q[3])],
    }


def same_placement_payload(left: dict, right: dict, tolerance: float = 1e-9) -> bool:
    for field in ("Base", "Rotation"):
        for left_value, right_value in zip(left[field], right[field]):
            if abs(float(left_value) - float(right_value)) > tolerance:
                return False
    return True


def fixture_placement_payload(spec: dict) -> dict:
    placement = spec.get("Properties", {}).get("Placement")
    if isinstance(placement, dict) and placement.get("PropertyType") == "App::PropertyPlacement":
        return {
            "PropertyType": "App::PropertyPlacement",
            "Base": [float(value) for value in placement.get("Base", [0, 0, 0])],
            "Rotation": [float(value) for value in placement.get("Rotation", [0, 0, 0, 1])],
        }
    return {
        "PropertyType": "App::PropertyPlacement",
        "Base": [0.0, 0.0, 0.0],
        "Rotation": [0.0, 0.0, 0.0, 1.0],
    }


def assembly_link_specs_for_object(fixture: dict, assembly_name: str) -> list[dict]:
    specs = {spec.get("Name"): spec for spec in fixture.get("Objects", []) if isinstance(spec, dict)}
    assembly_spec = specs.get(assembly_name)
    if not isinstance(assembly_spec, dict):
        return []
    group = assembly_spec.get("Properties", {}).get("Group")
    if not isinstance(group, dict):
        return []
    result = []
    for child_name in list_field(group, "values", "value"):
        child_spec = specs.get(child_name)
        if isinstance(child_spec, dict) and child_spec.get("TypeId") == "Assembly::AssemblyLink":
            result.append(child_spec)
    return result


def fixture_placement_for_property(value: Any) -> dict:
    if isinstance(value, dict) and value.get("PropertyType") == "App::PropertyPlacement":
        return {
            "Base": [float(item) for item in value.get("Base", [0, 0, 0])],
            "Rotation": [float(item) for item in value.get("Rotation", [0, 0, 0, 1])],
        }
    return {
        "Base": [0.0, 0.0, 0.0],
        "Rotation": [0.0, 0.0, 0.0, 1.0],
    }


def yaw_pitch_roll_from_fixture_placement(placement: dict) -> tuple[float, float, float]:
    # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Base/Rotation.cpp
    # ::Rotation::getYawPitchRoll(), returns yaw/pitch/roll in degrees. The collector mirrors the
    # pitch/roll comparison used by AssemblyObject::slidingPartIndex() for native expected JSON.
    x, y, z, w = [float(item) for item in placement["Rotation"]]
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if length <= 0.0:
        return 0.0, 0.0, 0.0
    x /= length
    y /= length
    z /= length
    w /= length
    q00 = x * x
    q11 = y * y
    q22 = z * z
    q33 = w * w
    q01 = x * y
    q02 = x * z
    q03 = x * w
    q12 = y * z
    q13 = y * w
    q23 = z * w
    qd2 = 2.0 * (q13 - q02)
    tolerance = 16.0 * sys.float_info.epsilon
    if abs(qd2 - 1.0) <= tolerance:
        return 0.0, math.degrees(math.pi / 2.0), math.degrees(2.0 * math.atan2(x, w))
    if abs(qd2 + 1.0) <= tolerance:
        return 0.0, math.degrees(-math.pi / 2.0), math.degrees(2.0 * math.atan2(x, w))
    yaw = math.degrees(math.atan2(2.0 * (q01 + q23), (q00 + q33) - (q11 + q22)))
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, qd2))))
    roll = math.degrees(math.atan2(2.0 * (q12 + q03), (q22 + q33) - (q00 + q11)))
    return yaw, pitch, roll


def same_fixture_pitch_and_roll(slider_placement: dict, target_placement: dict) -> bool:
    _, slider_pitch, slider_roll = yaw_pitch_roll_from_fixture_placement(slider_placement)
    _, target_pitch, target_roll = yaw_pitch_roll_from_fixture_placement(target_placement)
    return abs(slider_pitch - target_pitch) < 1e-7 and abs(slider_roll - target_roll) < 1e-7


def apply_screw_rackpinion_fixture_sliding(
    solver_joints: list[dict],
    joint_placements: dict[str, tuple[dict, dict]],
) -> None:
    for target in solver_joints:
        if target["joint_type"] not in {"Screw", "RackPinion"}:
            continue
        target_placements = joint_placements[target["object"]]
        sliding_found = 0
        for slider in solver_joints:
            if slider["joint_type"] != "Slider":
                continue
            slider_placements = joint_placements[slider["object"]]
            found = 0
            slider_placement = None
            target_placement = None
            slider_reference1 = slider["reference1"]["object"]
            slider_reference2 = slider["reference2"]["object"]
            target_reference1 = target["reference1"]["object"]
            target_reference2 = target["reference2"]["object"]
            if slider_reference1 and slider_reference1 in {target_reference1, target_reference2}:
                found = 1 if slider_reference1 == target_reference1 else 2
                slider_placement = slider_placements[0]
                target_placement = target_placements[0 if found == 1 else 1]
            elif slider_reference2 and slider_reference2 in {target_reference1, target_reference2}:
                found = 1 if slider_reference2 == target_reference1 else 2
                slider_placement = slider_placements[1]
                target_placement = target_placements[0 if found == 1 else 1]
            if (
                found != 0
                and slider_placement is not None
                and target_placement is not None
                and same_fixture_pitch_and_roll(slider_placement, target_placement)
            ):
                sliding_found = found
        target["sliding_part_index"] = sliding_found
        target["jcs_swapped_for_solver"] = False
        if sliding_found == 2:
            target["reference1"], target["reference2"] = target["reference2"], target["reference1"]
            target["jcs_swapped_for_solver"] = True


def joint_payloads_from_fixture(fixture: dict, assembly_name: str) -> tuple[list[str], list[str], list[dict]]:
    specs = {spec.get("Name"): spec for spec in fixture.get("Objects", []) if isinstance(spec, dict)}
    assembly_spec = specs.get(assembly_name)
    if not isinstance(assembly_spec, dict):
        return [], [], []
    group = assembly_spec.get("Properties", {}).get("Group")
    joint_group_names = []
    if isinstance(group, dict):
        for child_name in list_field(group, "values", "value"):
            child_spec = specs.get(child_name)
            if isinstance(child_spec, dict) and child_spec.get("TypeId") == "Assembly::JointGroup":
                joint_group_names.append(str(child_name))

    grounded_joints: list[str] = []
    joints: list[str] = []
    solver_joints: list[dict] = []
    joint_placements: dict[str, tuple[dict, dict]] = {}
    for joint_group_name in joint_group_names:
        joint_group_spec = specs.get(joint_group_name)
        joint_group = joint_group_spec.get("Properties", {}).get("Group") if isinstance(joint_group_spec, dict) else None
        if not isinstance(joint_group, dict):
            continue
        for joint_name in list_field(joint_group, "values", "value"):
            joint_spec = specs.get(joint_name)
            if not isinstance(joint_spec, dict) or joint_spec.get("TypeId") != "App::FeaturePython":
                continue
            properties = joint_spec.get("Properties", {})
            if "ObjectToGround" in properties:
                grounded_joints.append(str(joint_name))
                continue
            joint_type = scalar_property_value(properties.get("JointType"))
            if joint_type is None:
                continue
            joints.append(str(joint_name))
            distance = float(scalar_property_value(properties.get("Distance", 0.0)) or 0.0)
            solver_joint = {
                "object": str(joint_name),
                "joint_type": str(joint_type),
                "reference1": link_sub_payload_from_fixture(properties.get("Reference1")),
                "reference2": link_sub_payload_from_fixture(properties.get("Reference2")),
                "suppressed": bool(scalar_property_value(properties.get("Suppressed", False))),
            }
            joint_placements[str(joint_name)] = (
                fixture_placement_for_property(properties.get("Placement1")),
                fixture_placement_for_property(properties.get("Placement2")),
            )
            if joint_type in {"Distance", "Slider", "Gears", "Belt", "RackPinion", "Screw"}:
                solver_joint["distance"] = distance
            if joint_type == "Distance":
                resolve_fixture_distance_type(solver_joint, specs)
            if joint_type == "Screw":
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
                # ::AssemblyObject::makeMbdJointOfType(), Screw writes "pitch=getJointDistance".
                solver_joint["pitch"] = distance
            if joint_type == "RackPinion":
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
                # ::AssemblyObject::makeMbdJointOfType(), RackPinion writes
                # "pitchRadius=getJointDistance".
                solver_joint["pitch_radius"] = distance
            if joint_type in {"Gears", "Belt"}:
                distance2 = float(scalar_property_value(properties.get("Distance2", 0.0)) or 0.0)
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
                # ::AssemblyObject::makeMbdJointOfType(), Gears sets "radiusJ = getJointDistance2(joint)"
                # while Belt sets "radiusJ = -getJointDistance2(joint)".
                solver_joint["distance2"] = distance2
                solver_joint["radius_i"] = solver_joint["distance"]
                solver_joint["radius_j"] = -distance2 if joint_type == "Belt" else distance2
            if joint_type == "Angle":
                solver_joint["angle"] = float(scalar_property_value(properties.get("Angle", 0.0)) or 0.0)
            solver_joints.append(solver_joint)
    apply_screw_rackpinion_fixture_sliding(solver_joints, joint_placements)
    return grounded_joints, joints, solver_joints


def link_sub_payload_from_fixture(value: Any) -> dict:
    if not isinstance(value, dict):
        return {"object": "", "subnames": []}
    return {
        "object": str(value.get("value", "")),
        "subnames": [str(item) for item in list_field(value, "SubList", "StableSubList")],
    }


def scalar_fixture_property(properties: dict, name: str, default: float = 0.0) -> float:
    value = properties.get(name)
    if isinstance(value, dict):
        value = value.get("value", default)
    if isinstance(value, (int, float)):
        return float(value)
    return float(default)


def linked_fixture_spec(reference: dict, specs: dict[str, dict]) -> dict:
    target = specs.get(str(reference.get("object", "")), {})
    properties = target.get("Properties", {}) if isinstance(target, dict) else {}
    if target.get("TypeId") in {"Assembly::AssemblyLink", "App::Link"}:
        linked_object = properties.get("LinkedObject")
        if isinstance(linked_object, dict):
            linked_name = str(linked_object.get("value", ""))
            return specs.get(linked_name, target)
    return target


def fixture_subshape_token(reference: dict) -> str:
    subnames = reference.get("subnames", [])
    subname = str(subnames[0]) if subnames else ""
    if "." in subname:
        subname = subname.split(".")[-1]
    return subname


def fixture_reference_element(reference: dict, specs: dict[str, dict]) -> tuple[str, str, float | None, str | None]:
    subname = fixture_subshape_token(reference)
    target = linked_fixture_spec(reference, specs)
    type_id = str(target.get("TypeId", ""))
    properties = target.get("Properties", {}) if isinstance(target, dict) else {}

    if subname.startswith("Vertex"):
        return "Vertex", "point", None, None
    if subname.startswith("Edge"):
        primitive = "curve"
        radius = 0.0
        if type_id in {"Part::Line", "Part::Box"}:
            primitive = "line"
        elif type_id in {"Part::Cylinder", "Part::Circle"}:
            primitive = "circle"
            radius = scalar_fixture_property(properties, "Radius")
        elif type_id == "Part::Cone" and subname in {"Edge1", "Edge2"}:
            primitive = "circle"
            radius = scalar_fixture_property(properties, "Radius1")
        return "Edge", primitive, radius, "getEdgeRadius"
    if subname.startswith("Face"):
        primitive = "surface"
        radius = 0.0
        if type_id in {"Part::Plane", "Part::Box"}:
            primitive = "plane"
        elif type_id == "Part::Cylinder":
            primitive = "cylinder" if subname == "Face1" else "plane"
            radius = scalar_fixture_property(properties, "Radius") if primitive == "cylinder" else 0.0
        elif type_id == "Part::Sphere":
            primitive = "sphere"
            radius = scalar_fixture_property(properties, "Radius")
        elif type_id == "Part::Cone":
            primitive = "cone" if subname == "Face1" else "plane"
        elif type_id == "Part::Torus":
            primitive = "torus"
        return "Face", primitive, radius, "getFaceRadius"

    if type_id == "Part::Vertex":
        return "Vertex", "point", None, None
    if type_id == "Part::Line":
        return "Edge", "line", 0.0, "getEdgeRadius"
    if type_id == "Part::Plane":
        return "Face", "plane", 0.0, "getFaceRadius"
    return "", "", None, None


def is_basic_distance_reference(reference: dict, element_kind: str, primitive: str) -> bool:
    return (
        reference.get("element_kind") == element_kind
        and reference.get("primitive") == primitive
    )


def is_distance_reference(reference: dict, element_kind: str, primitive: str) -> bool:
    return is_basic_distance_reference(reference, element_kind, primitive)


def swap_solver_joint_references(solver_joint: dict) -> None:
    solver_joint["reference1"], solver_joint["reference2"] = (
        solver_joint["reference2"],
        solver_joint["reference1"],
    )
    solver_joint["jcs_swapped_for_solver"] = True


def reference_radius(solver_joint: dict, key: str) -> float:
    value = solver_joint.get(key, {}).get("radius")
    if value is None:
        value = solver_joint.get(f"{key}_radius")
    return float(value) if isinstance(value, (int, float)) else 0.0


def set_extended_scalar(
    solver_joint: dict,
    solver_joint_class: str,
    scalar_field: str,
    scalar_correction: float,
    scalar_correction_source: str,
    radius_source_side: str,
) -> None:
    distance = float(solver_joint.get("distance", 0.0) or 0.0)
    solver_joint["solver_joint_class"] = solver_joint_class
    solver_joint[scalar_field] = distance + scalar_correction
    solver_joint["scalar_correction"] = scalar_correction
    solver_joint["scalar_correction_source"] = scalar_correction_source
    solver_joint["radius_source_side"] = radius_source_side
    solver_joint["distance_type_mapping_status"] = "mapped_s4_extended"
    solver_joint["distance_type_boundary"] = "extended_mapping_pending_s5_oracle"


def mark_default_distance_boundary(solver_joint: dict) -> None:
    solver_joint["scalar_correction"] = 0.0
    solver_joint["scalar_correction_source"] = "none"
    solver_joint["radius_source_side"] = "none"
    solver_joint["distance_type_mapping_status"] = "default_boundary_not_mapped"
    solver_joint["distance_type_boundary"] = "default_or_todo_boundary"


def resolve_extended_fixture_distance_mapping(solver_joint: dict) -> bool:
    distance_type = solver_joint.get("distance_type")
    if distance_type == "LineCircle":
        set_extended_scalar(
            solver_joint,
            "ASMTRevCylJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference2"),
            "getEdgeRadius(reference2)",
            "reference2",
        )
    elif distance_type == "CircleCircle":
        set_extended_scalar(
            solver_joint,
            "ASMTRevCylJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getEdgeRadius(reference1)+getEdgeRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "PlaneCylinder":
        set_extended_scalar(
            solver_joint,
            "ASMTLineInPlaneJoint",
            "offset",
            reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference2)",
            "reference2",
        )
    elif distance_type == "PlaneSphere":
        set_extended_scalar(
            solver_joint,
            "ASMTPointInPlaneJoint",
            "offset",
            reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference2)",
            "reference2",
        )
    elif distance_type == "PlaneTorus":
        set_extended_scalar(solver_joint, "ASMTPlanarJoint", "offset", 0.0, "none", "none")
    elif distance_type == "CylinderCylinder":
        set_extended_scalar(
            solver_joint,
            "ASMTRevCylJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference1)+getFaceRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "CylinderSphere":
        set_extended_scalar(
            solver_joint,
            "ASMTCylSphJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference1)+getFaceRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "CylinderTorus":
        set_extended_scalar(
            solver_joint,
            "ASMTRevCylJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference1)+getFaceRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "TorusTorus":
        set_extended_scalar(solver_joint, "ASMTPlanarJoint", "offset", 0.0, "none", "none")
    elif distance_type == "TorusSphere":
        set_extended_scalar(
            solver_joint,
            "ASMTCylSphJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference1)+getFaceRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "SphereSphere":
        set_extended_scalar(
            solver_joint,
            "ASMTSphSphJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1") + reference_radius(solver_joint, "reference2"),
            "getFaceRadius(reference1)+getFaceRadius(reference2)",
            "reference1+reference2",
        )
    elif distance_type == "PointCylinder":
        set_extended_scalar(
            solver_joint,
            "ASMTCylSphJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1"),
            "getFaceRadius(reference1)",
            "reference1",
        )
    elif distance_type == "PointSphere":
        set_extended_scalar(
            solver_joint,
            "ASMTSphSphJoint",
            "distance_ij",
            reference_radius(solver_joint, "reference1"),
            "getFaceRadius(reference1)",
            "reference1",
        )
    elif distance_type == "PointCurve":
        set_extended_scalar(solver_joint, "ASMTPointInPlaneJoint", "offset", 0.0, "none", "none")
    else:
        return False
    return True


def distance_type_is_basic(distance_type: str | None) -> bool:
    return distance_type in {
        "PointPoint",
        "LineLine",
        "PlanePlane",
        "PointPlane",
        "LinePlane",
        "PointLine",
    }


def distance_type_is_default_boundary(distance_type: str | None) -> bool:
    return distance_type in {
        "PlaneCone",
        "CylinderCone",
        "ConeCone",
        "ConeTorus",
        "ConeSphere",
        "PointCone",
        "PointTorus",
        "LineCylinder",
        "LineSphere",
        "LineCone",
        "LineTorus",
        "CurvePlane",
        "CurveCylinder",
        "CurveSphere",
        "CurveCone",
        "CurveTorus",
        "Other",
    }


def resolve_fixture_distance_mapping(solver_joint: dict) -> None:
    distance = float(solver_joint.get("distance", 0.0) or 0.0)
    distance_type = solver_joint.get("distance_type")

    # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    # ::makeMbdJointDistance(), maps basic DistanceTypes to resolved ASMT classes and stores
    # the scalar as "distanceIJ" or "offset".
    if distance_type == "PointPoint":
        if distance < FREECAD_PRECISION_CONFUSION:
            solver_joint["solver_joint_class"] = "ASMTSphericalJoint"
            return
        solver_joint["solver_joint_class"] = "ASMTSphSphJoint"
        solver_joint["distance_ij"] = distance
    elif distance_type == "LineLine":
        solver_joint["solver_joint_class"] = "ASMTRevCylJoint"
        solver_joint["distance_ij"] = distance
    elif distance_type == "PointLine":
        # FreeCADCmd 1.2.0 revision 20260519 exports this native path as
        # "tInPlaneJointE" with "offset", even though the nearby C++ source switch still names
        # ASMTCylSphJoint for DistanceType::PointLine.
        solver_joint["solver_joint_class"] = "ASMTLineInPlaneJoint"
        solver_joint["offset"] = distance
    elif distance_type == "PlanePlane":
        solver_joint["solver_joint_class"] = "ASMTPlanarJoint"
        solver_joint["offset"] = distance
    elif distance_type == "PointPlane":
        solver_joint["solver_joint_class"] = "ASMTPointInPlaneJoint"
        solver_joint["offset"] = distance
    elif distance_type == "LinePlane":
        solver_joint["solver_joint_class"] = "ASMTLineInPlaneJoint"
        solver_joint["offset"] = distance
    elif resolve_extended_fixture_distance_mapping(solver_joint):
        return
    elif distance_type_is_default_boundary(distance_type):
        mark_default_distance_boundary(solver_joint)


def resolve_fixture_distance_type(solver_joint: dict, specs: dict[str, dict]) -> None:
    if solver_joint.get("joint_type") != "Distance":
        return

    # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp
    # ::getDistanceType(), reads Reference1/Reference2 element kind and calls "swapJCS(joint)"
    # so line or face references are first for basic point / line / plane DistanceTypes.
    for key in ("reference1", "reference2"):
        element_kind, primitive, radius, radius_source = fixture_reference_element(solver_joint[key], specs)
        solver_joint[key]["element_kind"] = element_kind
        solver_joint[key]["primitive"] = primitive
        if radius is not None:
            solver_joint[key]["radius"] = radius
        if radius_source is not None:
            solver_joint[key]["radius_source"] = radius_source

    solver_joint["jcs_swapped_for_solver"] = False
    if (
        is_basic_distance_reference(solver_joint["reference1"], "Vertex", "point")
        and is_basic_distance_reference(solver_joint["reference2"], "Vertex", "point")
    ):
        solver_joint["distance_type"] = "PointPoint"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Edge", "line")
        and is_basic_distance_reference(solver_joint["reference2"], "Edge", "line")
    ):
        solver_joint["distance_type"] = "LineLine"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_basic_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        solver_joint["distance_type"] = "PlanePlane"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Vertex", "point")
        and is_basic_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PointPlane"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_basic_distance_reference(solver_joint["reference2"], "Vertex", "point")
    ):
        solver_joint["distance_type"] = "PointPlane"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Edge", "line")
        and is_basic_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "LinePlane"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_basic_distance_reference(solver_joint["reference2"], "Edge", "line")
    ):
        solver_joint["distance_type"] = "LinePlane"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Vertex", "point")
        and is_basic_distance_reference(solver_joint["reference2"], "Edge", "line")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PointLine"
    elif (
        is_basic_distance_reference(solver_joint["reference1"], "Edge", "line")
        and is_basic_distance_reference(solver_joint["reference2"], "Vertex", "point")
    ):
        solver_joint["distance_type"] = "PointLine"
    elif (
        is_distance_reference(solver_joint["reference1"], "Edge", "line")
        and is_distance_reference(solver_joint["reference2"], "Edge", "circle")
    ):
        solver_joint["distance_type"] = "LineCircle"
    elif (
        is_distance_reference(solver_joint["reference1"], "Edge", "circle")
        and is_distance_reference(solver_joint["reference2"], "Edge", "line")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "LineCircle"
    elif (
        is_distance_reference(solver_joint["reference1"], "Edge", "circle")
        and is_distance_reference(solver_joint["reference2"], "Edge", "circle")
    ):
        solver_joint["distance_type"] = "CircleCircle"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_distance_reference(solver_joint["reference2"], "Face", "cylinder")
    ):
        solver_joint["distance_type"] = "PlaneCylinder"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cylinder")
        and is_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PlaneCylinder"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_distance_reference(solver_joint["reference2"], "Face", "sphere")
    ):
        solver_joint["distance_type"] = "PlaneSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "sphere")
        and is_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PlaneSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_distance_reference(solver_joint["reference2"], "Face", "cone")
    ):
        solver_joint["distance_type"] = "PlaneCone"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cone")
        and is_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PlaneCone"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "plane")
        and is_distance_reference(solver_joint["reference2"], "Face", "torus")
    ):
        solver_joint["distance_type"] = "PlaneTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "torus")
        and is_distance_reference(solver_joint["reference2"], "Face", "plane")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "PlaneTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cylinder")
        and is_distance_reference(solver_joint["reference2"], "Face", "cylinder")
    ):
        solver_joint["distance_type"] = "CylinderCylinder"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cylinder")
        and is_distance_reference(solver_joint["reference2"], "Face", "sphere")
    ):
        solver_joint["distance_type"] = "CylinderSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "sphere")
        and is_distance_reference(solver_joint["reference2"], "Face", "cylinder")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "CylinderSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cylinder")
        and is_distance_reference(solver_joint["reference2"], "Face", "cone")
    ):
        solver_joint["distance_type"] = "CylinderCone"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cone")
        and is_distance_reference(solver_joint["reference2"], "Face", "cylinder")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "CylinderCone"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cylinder")
        and is_distance_reference(solver_joint["reference2"], "Face", "torus")
    ):
        solver_joint["distance_type"] = "CylinderTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "torus")
        and is_distance_reference(solver_joint["reference2"], "Face", "cylinder")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "CylinderTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cone")
        and is_distance_reference(solver_joint["reference2"], "Face", "cone")
    ):
        solver_joint["distance_type"] = "ConeCone"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cone")
        and is_distance_reference(solver_joint["reference2"], "Face", "torus")
    ):
        solver_joint["distance_type"] = "ConeTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "torus")
        and is_distance_reference(solver_joint["reference2"], "Face", "cone")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "ConeTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "cone")
        and is_distance_reference(solver_joint["reference2"], "Face", "sphere")
    ):
        solver_joint["distance_type"] = "ConeSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "sphere")
        and is_distance_reference(solver_joint["reference2"], "Face", "cone")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "ConeSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "torus")
        and is_distance_reference(solver_joint["reference2"], "Face", "torus")
    ):
        solver_joint["distance_type"] = "TorusTorus"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "torus")
        and is_distance_reference(solver_joint["reference2"], "Face", "sphere")
    ):
        solver_joint["distance_type"] = "TorusSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "sphere")
        and is_distance_reference(solver_joint["reference2"], "Face", "torus")
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "TorusSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Face", "sphere")
        and is_distance_reference(solver_joint["reference2"], "Face", "sphere")
    ):
        solver_joint["distance_type"] = "SphereSphere"
    elif (
        is_distance_reference(solver_joint["reference1"], "Vertex", "point")
        and solver_joint["reference2"].get("element_kind") == "Face"
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = {
            "cylinder": "PointCylinder",
            "sphere": "PointSphere",
            "cone": "PointCone",
            "torus": "PointTorus",
        }.get(solver_joint["reference1"].get("primitive"), "Other")
    elif (
        solver_joint["reference1"].get("element_kind") == "Face"
        and is_distance_reference(solver_joint["reference2"], "Vertex", "point")
    ):
        solver_joint["distance_type"] = {
            "cylinder": "PointCylinder",
            "sphere": "PointSphere",
            "cone": "PointCone",
            "torus": "PointTorus",
        }.get(solver_joint["reference1"].get("primitive"), "Other")
    elif (
        solver_joint["reference1"].get("element_kind") == "Edge"
        and solver_joint["reference2"].get("element_kind") == "Face"
    ):
        swap_solver_joint_references(solver_joint)
        primitive = solver_joint["reference1"].get("primitive")
        edge_primitive = solver_joint["reference2"].get("primitive")
        solver_joint["distance_type"] = {
            (True, "cylinder"): "LineCylinder",
            (True, "sphere"): "LineSphere",
            (True, "cone"): "LineCone",
            (True, "torus"): "LineTorus",
            (False, "plane"): "CurvePlane",
            (False, "cylinder"): "CurveCylinder",
            (False, "sphere"): "CurveSphere",
            (False, "cone"): "CurveCone",
            (False, "torus"): "CurveTorus",
        }.get((edge_primitive == "line", primitive), "Other")
    elif (
        solver_joint["reference1"].get("element_kind") == "Face"
        and solver_joint["reference2"].get("element_kind") == "Edge"
    ):
        primitive = solver_joint["reference1"].get("primitive")
        edge_primitive = solver_joint["reference2"].get("primitive")
        solver_joint["distance_type"] = {
            (True, "cylinder"): "LineCylinder",
            (True, "sphere"): "LineSphere",
            (True, "cone"): "LineCone",
            (True, "torus"): "LineTorus",
            (False, "plane"): "CurvePlane",
            (False, "cylinder"): "CurveCylinder",
            (False, "sphere"): "CurveSphere",
            (False, "cone"): "CurveCone",
            (False, "torus"): "CurveTorus",
        }.get((edge_primitive == "line", primitive), "Other")
    elif (
        is_distance_reference(solver_joint["reference1"], "Vertex", "point")
        and solver_joint["reference2"].get("element_kind") == "Edge"
    ):
        swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = (
            "PointLine" if solver_joint["reference1"].get("primitive") == "line" else "PointCurve"
        )
    elif (
        solver_joint["reference1"].get("element_kind") == "Edge"
        and is_distance_reference(solver_joint["reference2"], "Vertex", "point")
    ):
        solver_joint["distance_type"] = (
            "PointLine" if solver_joint["reference1"].get("primitive") == "line" else "PointCurve"
        )
    elif (
        solver_joint["reference1"].get("element_kind") == "Edge"
        and solver_joint["reference2"].get("element_kind") == "Edge"
    ):
        if solver_joint["reference1"].get("primitive") != "line" and solver_joint["reference2"].get("primitive") == "line":
            swap_solver_joint_references(solver_joint)
        solver_joint["distance_type"] = "Other"

    if "distance_type" not in solver_joint:
        for key in ("reference1", "reference2"):
            solver_joint[key].pop("element_kind", None)
            solver_joint[key].pop("primitive", None)
        solver_joint.pop("jcs_swapped_for_solver", None)
        return

    include_radius_evidence = not distance_type_is_basic(solver_joint.get("distance_type"))
    for side in ("reference1", "reference2"):
        solver_joint[f"{side}_element_kind"] = solver_joint[side].pop("element_kind")
        solver_joint[f"{side}_primitive"] = solver_joint[side].pop("primitive")
        radius = solver_joint[side].pop("radius", None)
        radius_source = solver_joint[side].pop("radius_source", None)
        if include_radius_evidence and radius is not None:
            solver_joint[f"{side}_radius"] = float(radius)
        if include_radius_evidence and radius_source is not None:
            solver_joint[f"{side}_radius_source"] = radius_source
    resolve_fixture_distance_mapping(solver_joint)


def normalized_link_sub_payload(value: Any) -> dict:
    payload = link_sub_payload(value)
    payload["subnames"] = [subname for subname in payload["subnames"] if subname]
    return payload


def reference_payload_key(reference: dict) -> tuple[str, tuple[str, ...]]:
    return str(reference.get("object", "")), tuple(str(item) for item in reference.get("subnames", []))


def native_joint_reference_marker_evidence(joint: Any, ref_name: str, plc_name: str) -> dict:
    import FreeCAD  # type: ignore
    import UtilsAssembly  # type: ignore

    ref = getattr(joint, ref_name, None)
    connector = getattr(joint, plc_name, FreeCAD.Placement())
    reference = normalized_link_sub_payload(ref)
    evidence: dict[str, Any] = {
        "source": (
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::handleOneSideOfJoint()"
        ),
        "reference": reference,
        "connector_placement": placement_payload(connector),
        "subshape_reference": bool(reference["subnames"]),
        "status": "unresolved",
    }
    if ref is None or not isinstance(ref, tuple) or len(ref) != 2 or ref[0] is None:
        evidence["status"] = "missing_reference"
        evidence["diagnostic"] = f"{ref_name} is not a native Assembly Joint reference"
        return evidence

    try:
        part = UtilsAssembly.getMovingPart(ref)
        if part is None:
            evidence["status"] = "missing_moving_part"
            evidence["diagnostic"] = f"{ref_name} has no moving part"
            return evidence
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        # ::AssemblyObject::handleOneSideOfJoint(), computes "obj_global_plc =
        # getGlobalPlacement(nullptr, ref)", then "plc = obj_global_plc * plc", then
        # "part_global_plc.inverse() * plc" before marker creation.
        obj_global = UtilsAssembly.getGlobalPlacement(ref)
        part_global = UtilsAssembly.getGlobalPlacement(ref, part)
        jcs_global = obj_global * connector
        marker = part_global.inverse() * jcs_global
        evidence.update({
            "status": "resolved_native_handle_one_side",
            "frame": "part_local",
            "moving_part": link_name(part),
            "object_global_placement": placement_payload(obj_global),
            "part_global_placement": placement_payload(part_global),
            "jcs_global_placement": placement_payload(jcs_global),
            "marker_placement": placement_payload(marker),
            "offset_boundary": "identity_offset_for_two_box_assembly_link_fixture",
        })
    except Exception as exc:
        evidence["status"] = "collector_error"
        evidence["diagnostic"] = str(exc)
    return evidence


def native_current_value_evidence(joint: Any, joint_type: str) -> dict | None:
    import UtilsAssembly  # type: ignore

    if joint_type == "Distance":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py
        # ::getJointDistance(), calls "getJcsGlobalPlc(joint.PlacementN, joint.ReferenceN)"
        # for both sides and signs the scalar from "plc3.Base.z".
        return {
            "source": (
                "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py"
                "::getJointDistance()"
            ),
            "linear_distance_from_jcs_placements": float(UtilsAssembly.getJointDistance(joint)),
        }
    if joint_type == "Angle":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py
        # ::getJointXYAngle(), multiplies Placement1/2 by reference global placements and
        # returns atan2() of the relative X axis.
        return {
            "source": (
                "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py"
                "::getJointXYAngle()"
            ),
            "xy_angle_radians_from_jcs_placements": float(UtilsAssembly.getJointXYAngle(joint)),
        }
    return None


def native_marker_oracle_payload(created: dict[str, Any], solver_joints: list[dict]) -> dict:
    entries = []
    requires_marker_parity = False
    for solver_joint in solver_joints:
        joint = created.get(str(solver_joint.get("object", "")))
        if joint is None or not hasattr(joint, "JointType"):
            continue
        joint_type = str(getattr(joint, "JointType", solver_joint.get("joint_type", "")))
        native_reference1 = native_joint_reference_marker_evidence(joint, "Reference1", "Placement1")
        native_reference2 = native_joint_reference_marker_evidence(joint, "Reference2", "Placement2")
        by_reference = {
            reference_payload_key(native_reference1["reference"]): native_reference1,
            reference_payload_key(native_reference2["reference"]): native_reference2,
        }
        solver_reference1 = by_reference.get(
            reference_payload_key(solver_joint.get("reference1", {})),
            native_reference1,
        )
        solver_reference2 = by_reference.get(
            reference_payload_key(solver_joint.get("reference2", {})),
            native_reference2,
        )
        current_value = native_current_value_evidence(joint, joint_type)
        requires_marker_parity = requires_marker_parity or bool(
            native_reference1.get("subshape_reference")
            or native_reference2.get("subshape_reference")
        )
        entry = {
            "object": solver_joint.get("object", ""),
            "joint_type": joint_type,
            "jcs_swapped_for_solver": bool(solver_joint.get("jcs_swapped_for_solver", False)),
            "native_reference1": native_reference1,
            "native_reference2": native_reference2,
            "solver_reference1": solver_reference1,
            "solver_reference2": solver_reference2,
        }
        if current_value is not None:
            entry["current_value"] = current_value
        entries.append(entry)
    if not entries:
        return {}
    return {
        "source": (
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::handleOneSideOfJoint(); "
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp"
            "::getJointCurrentValue()"
        ),
        "requires_cad_core_marker_parity": requires_marker_parity,
        "solver_joints": entries,
    }


def native_assembly_solver_payload(obj: Any, fixture: dict, created: dict[str, Any]) -> tuple[dict, dict]:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    # ::AssemblyObject::solve(), calls "fixGroundedParts()", returns "-6" when no grounded
    # part exists, then runs "mbdAssembly->runPreDrag()" and "setNewPlacements()". CAD Core
    # expected fixtures use this native writeback as the Assembly placement oracle.
    grounded_joints, joints, solver_joints = joint_payloads_from_fixture(fixture, str(obj.Name))
    native_marker_oracle = native_marker_oracle_payload(created, solver_joints)
    solve_code = int(obj.solve(False))
    if solve_code == -6:
        return {
            "status": "error",
            "reason": "no_grounded_part",
            "grounded_joints": grounded_joints,
            "joints": joints,
            "solver_joints": solver_joints,
            "unsupported_joints": [],
            "placement_updates": [],
            "native_solver_return": solve_code,
        }, native_marker_oracle
    if solve_code != 0:
        return {
            "status": "error",
            "reason": "native_solver_failed",
            "grounded_joints": grounded_joints,
            "joints": joints,
            "solver_joints": solver_joints,
            "unsupported_joints": [],
            "placement_updates": [],
            "native_solver_return": solve_code,
        }, native_marker_oracle

    placement_updates = []
    for component_spec in assembly_link_specs_for_object(fixture, str(obj.Name)):
        component_name = str(component_spec["Name"])
        component = created.get(component_name)
        if component is None:
            continue
        old_placement = fixture_placement_payload(component_spec)
        new_placement = placement_payload(component.Placement)
        if same_placement_payload(old_placement, new_placement):
            continue
        placement_updates.append({
            "action": "assembly_set_placement",
            "assembly": str(obj.Name),
            "joint": "OndselSolver",
            "joint_type": "solver_result",
            "object": component_name,
            "objectId": int(component_spec.get("ID", 0)),
            "typeId": str(component_spec.get("TypeId", "")),
            "reason": "assembly_solver_placement_writeback",
            "properties": {
                "Placement": new_placement,
            },
        })

    if not joints:
        mode = "grounded_only_noop"
    else:
        mode = "real_ondsel_solver"
    return {
        "status": "solved",
        "mode": mode,
        "grounded_joints": grounded_joints,
        "joints": joints,
        "solver_joints": solver_joints,
        "unsupported_joints": [],
        "placement_updates": placement_updates,
        "native_solver_return": solve_code,
    }, native_marker_oracle


def assembly_object_payload(obj: Any, fixture: dict | None = None, created: dict[str, Any] | None = None) -> dict:
    group = list(getattr(obj, "Group", []))
    joint_groups = [child for child in group if getattr(child, "TypeId", "") == "Assembly::JointGroup"]
    joints = []
    for joint_group in joint_groups:
        for child in list(getattr(joint_group, "Group", [])):
            if getattr(child, "TypeId", "") == "App::FeaturePython":
                joints.append(child)

    payload = {
        "object_fields": {
            "assembly": "object",
            "group": link_names(group),
            "joint_groups": link_names(joint_groups),
            "joints": link_names(joints),
            "solve": "not_migrated",
        }
    }
    if fixture is not None and created is not None:
        solver_adapter, native_marker_oracle = native_assembly_solver_payload(obj, fixture, created)
        native_solver_return = solver_adapter.pop("native_solver_return")
        payload["native_solver"] = {"return_code": native_solver_return}
        payload["solver_adapter"] = solver_adapter
        if native_marker_oracle:
            payload["native_marker_oracle"] = native_marker_oracle
        if solver_adapter["status"] == "solved":
            payload["object_fields"]["solve"] = "solved_noop" if solver_adapter.get("mode") == "grounded_only_noop" else "solved"
        elif solver_adapter.get("reason") == "no_grounded_part":
            payload["object_fields"]["solve"] = "error"
    display_shape = assembly_object_display_shape(obj, set())
    if display_shape is not None:
        payload.update(shape_summary(display_shape))
    return payload


def assembly_link_payload(obj: Any) -> dict:
    payload = {
        "object_fields": {
            "status": "ok",
            "link": "assembly_link",
            "linked_object": link_name(getattr(obj, "LinkedObject", None)),
            "rigid": bool(getattr(obj, "Rigid", True)),
        }
    }
    display_shape = assembly_link_display_shape(obj, set())
    if display_shape is not None:
        payload.update(shape_summary(display_shape))
    return payload


def fixture_spec_for_object(fixture: dict | None, name: str) -> dict[str, Any]:
    if fixture is None:
        return {}
    for spec in fixture.get("Objects", []):
        if isinstance(spec, dict) and spec.get("Name") == name:
            return spec
    return {}


def link_property_object_name(properties: dict[str, Any], name: str) -> str:
    value = properties.get(name, {})
    if isinstance(value, dict):
        item = value.get("value")
        return str(item) if item is not None else ""
    return ""


def link_property_object_names(properties: dict[str, Any], name: str) -> list[str]:
    value = properties.get(name, {})
    if isinstance(value, dict):
        return link_names(value.get("values", value.get("value", [])))
    if isinstance(value, list):
        return link_names(value)
    return []


def ruled_surface_orientation_from_properties(properties: dict[str, Any]) -> str:
    labels = ["Automatic", "Forward", "Reversed"]
    value = consumer_property(properties, "Orientation", "Automatic")
    if isinstance(value, str) and value in labels:
        return value
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return labels[index]
    return "Automatic"


def ruled_surface_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "feature": "part_ruled_surface",
        "source_curve1": link_property_object_name(properties, "Curve1"),
        "source_curve2": link_property_object_name(properties, "Curve2"),
        "orientation": ruled_surface_orientation_from_properties(properties),
    }
    return payload


def bool_from_properties(properties: dict[str, Any], name: str, fallback: bool) -> bool:
    value = consumer_property(properties, name, fallback)
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        return value.lower() in {"true", "1", "yes"}
    return fallback


def int_from_properties(properties: dict[str, Any], name: str, fallback: int) -> int:
    value = consumer_property(properties, name, fallback)
    if isinstance(value, (int, float)):
        return int(value)
    return fallback


def loft_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "feature": "part_loft",
        "sections": link_property_object_names(properties, "Sections"),
        "solid": bool_from_properties(properties, "Solid", True),
        "ruled": bool_from_properties(properties, "Ruled", False),
        "closed": bool_from_properties(properties, "Closed", False),
        "linearize": bool_from_properties(properties, "Linearize", False),
        "max_degree": int_from_properties(properties, "MaxDegree", 5),
        "topo_naming_history": "maker_history:loft_thru_sections",
    }
    return payload


def object_expected_payload(obj: Any, fixture: dict | None = None, created: dict[str, Any] | None = None) -> dict:
    type_id = getattr(obj, "TypeId", "")
    if type_id == "Assembly::AssemblyLink":
        return assembly_link_payload(obj)
    if type_id == "Assembly::AssemblyObject":
        return assembly_object_payload(obj, fixture, created)
    if type_id == "Assembly::JointGroup":
        return assembly_joint_group_payload(obj)
    if type_id == "App::FeaturePython":
        return assembly_joint_feature_python_payload(obj)
    if type_id == "App::Link":
        return app_link_payload(obj, "app_link")
    if type_id == "App::LinkElement":
        return app_link_payload(obj, "app_link_element")
    if type_id == "Part::RuledSurface":
        return ruled_surface_payload(obj, fixture)
    if type_id == "Part::Loft":
        return loft_payload(obj, fixture)

    shape = getattr(obj, "Shape", None)
    if type_id == "Mesh::Import":
        return mesh_import_summary(obj)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    if type_id == "Sketcher::SketchObject":
        return sketch_summary(obj)
    return shape_summary(shape)


def mesh_import_summary(obj: Any) -> dict:
    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Mesh/App
    # /FeatureMeshImport.cpp::Mesh::Import::execute(), reads PropertyFile "FileName",
    # calls "apcKernel->load(...)" and stores the result with "Mesh.setValuePtr(...)".
    # Mesh objects publish Mesh.CountPoints / CountEdges / CountFacets / Volume and
    # BoundBox rather than a Part "Shape".
    mesh = getattr(obj, "Mesh", None)
    if mesh is None:
        raise UnsupportedFixture(f"target object {obj.Name} has no Mesh")
    bbox = mesh.BoundBox
    vertex_count = int(mesh.CountPoints)
    triangle_count = int(mesh.CountFacets)
    return {
        "bbox": {
            "min": [float(bbox.XMin), float(bbox.YMin), float(bbox.ZMin)],
            "max": [float(bbox.XMax), float(bbox.YMax), float(bbox.ZMax)],
        },
        "volume": float(mesh.Volume),
        "topology_counts": {
            "faces": triangle_count,
            "edges": int(mesh.CountEdges),
            "vertices": vertex_count,
        },
        "mesh_summary": {
            "vertex_count": vertex_count,
            "triangle_count": triangle_count,
        },
    }


def sketch_external_geometry(obj: Any) -> list[Any]:
    try:
        return list(obj.getExternalGeometry())
    except Exception:
        pass
    try:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        # /SketchObject.cpp::SketchObject::init(), ExternalGeo starts with the H/V axis
        # construction lines. Python builds without getExternalGeometry() still expose the
        # transient projected externals through ExternalGeo after those two axis entries.
        return list(obj.ExternalGeo)[2:]
    except Exception:
        return []


def sketch_external_geometry_counts(obj: Any) -> dict[str, int]:
    geometry = sketch_external_geometry(obj)
    try:
        external_count = int(obj.getExternalGeometryCount())
    except Exception:
        external_count = len(geometry)

    point_count = 0
    curve_count = 0
    for item in geometry:
        type_name = type(item).__name__
        if "Point" in type_name:
            point_count += 1
        elif any(kind in type_name for kind in ("Circle", "Arc", "Ellipse", "BSpline", "Bezier")):
            curve_count += 1

    return {
        "external_geometry_count": external_count,
        "external_point_count": point_count,
        "external_curve_count": curve_count,
    }


def sketch_external_geometry_flag_payload(obj: Any) -> dict[str, Any]:
    try:
        import Sketcher  # type: ignore
    except Exception:
        return {"flags": [], "flag_counts": {name: 0 for name in EXTERNAL_GEOMETRY_FLAG_NAMES}}

    flags_by_geometry = []
    flag_counts = {name: 0 for name in EXTERNAL_GEOMETRY_FLAG_NAMES}
    construction_count = 0
    for geometry in sketch_external_geometry(obj):
        try:
            facade = Sketcher.ExternalGeometryFacade(geometry)
        except Exception:
            continue
        flags = []
        for name in EXTERNAL_GEOMETRY_FLAG_NAMES:
            try:
                if facade.testFlag(name):
                    flags.append(name)
                    flag_counts[name] += 1
            except Exception:
                continue
        try:
            if bool(facade.Construction):
                construction_count += 1
        except Exception:
            pass
        flags_by_geometry.append(flags)
    return {
        "flags": flags_by_geometry,
        "flag_counts": flag_counts,
        "construction_count": construction_count,
    }


def sketch_summary(obj: Any) -> dict:
    shape = getattr(obj, "Shape", None)
    internal_shape = getattr(obj, "InternalShape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no sketch shape")

    payload: dict[str, Any] = {
        "object_fields": {
            "status": "ok",
            "shape": "occt_sketch_shape",
            "raw_edge_count": len(getattr(shape, "Edges", [])),
            **sketch_external_geometry_counts(obj),
        }
    }
    if internal_shape is not None and not internal_shape.isNull():
        payload["sketch_internal"] = {
            "profile_ready": len(getattr(internal_shape, "Faces", [])) > 0,
            "shape": "occt_internal_shape",
            "raw_edge_count": len(getattr(shape, "Edges", [])),
            "internal_counts": {
                "faces": len(getattr(internal_shape, "Faces", [])),
                "edges": len(getattr(internal_shape, "Edges", [])),
                "vertices": len(getattr(internal_shape, "Vertexes", [])),
            },
        }
    else:
        payload["sketch_internal"] = {
            "profile_ready": False,
            "shape": "empty",
            "raw_edge_count": len(getattr(shape, "Edges", [])),
        }
    payload["sketch_external"] = sketch_external_geometry_flag_payload(obj)
    return payload


def target_names(fixture: dict) -> list[str]:
    names = fixture.get("recompute", {}).get("objs")
    if names:
        targets = list(names)
        specs = {spec.get("Name"): spec for spec in fixture.get("Objects", []) if isinstance(spec, dict)}
        transformation_templates: set[str] = set()
        for spec in fixture.get("Objects", []):
            if not isinstance(spec, dict) or spec.get("TypeId") != "PartDesign::MultiTransform":
                continue
            transformations = spec.get("Properties", {}).get("Transformations")
            if isinstance(transformations, dict):
                transformation_templates.update(str(item) for item in list_field(transformations, "values", "value"))
        for name in list(targets):
            spec = specs.get(name)
            if not spec or spec.get("TypeId") != "PartDesign::Body":
                continue
            group = spec.get("Properties", {}).get("Group")
            body_members = list_field(group, "values", "value") if isinstance(group, dict) else []
            insert_at = targets.index(name)
            for member_name in body_members:
                member_spec = specs.get(member_name)
                if (
                    member_spec
                    and member_spec.get("TypeId") in BODY_RESULT_TARGET_TYPES
                    and member_name not in targets
                    and member_name not in transformation_templates
                ):
                    # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App
                    # /FeatureDressUp.cpp::Fillet::execute() and ::Chamfer::execute() store the
                    # replacement solid on the DressUp feature, and /FeatureTransformed.cpp
                    # ::Transformed::execute() stores the transformed replacement solid on the
                    # transformed feature; Body.Tip exposes the final Body result.
                    # /FeatureMultiTransform.cpp::MultiTransform::getTransformations() uses
                    # Transformations links as child templates, so those children are not expected
                    # to publish Shape during native collection. Collect replacement features as
                    # well as Body, but skip MultiTransform child templates.
                    targets.insert(insert_at, str(member_name))
                    insert_at += 1
        return targets
    objects = fixture.get("Objects", [])
    if not objects:
        raise UnsupportedFixture("fixture has no Objects")
    return [objects[-1]["Name"]]


def require_native_hole_profile_support(fixture: dict) -> None:
    specs = {spec.get("Name"): spec for spec in fixture.get("Objects", []) if isinstance(spec, dict)}
    for spec in fixture.get("Objects", []):
        if spec.get("TypeId") != "PartDesign::Hole":
            continue
        profile = spec.get("Properties", {}).get("Profile")
        if not isinstance(profile, dict):
            continue
        target = specs.get(profile.get("value"))
        if not target or target.get("TypeId") != "Sketcher::SketchObject":
            continue
        target_properties = target.get("Properties", {})
        if "AttachmentSupport" in target_properties or "Support" in target_properties:
            continue
        raise UnsupportedFixture(
            "PartDesign::Hole native oracle requires the Profile sketch AttachmentSupport/Support; "
            "detached placement-only Hole fixtures are geometry-equivalent CAD Core cases"
        )


def require_native_dressup_body_membership(fixture: dict) -> None:
    dressup_names = {
        spec.get("Name")
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and spec.get("TypeId") in DRESS_UP_TYPES
    }
    if not dressup_names:
        return

    body_members: set[str] = set()
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") != "PartDesign::Body":
            continue
        group = spec.get("Properties", {}).get("Group")
        if isinstance(group, dict) and group.get("PropertyType") == "App::PropertyLinkList":
            body_members.update(str(item) for item in list_field(group, "values", "value"))

    missing = sorted(name for name in dressup_names if name not in body_members)
    if missing:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp
        # ::Body::setBaseProperty() sets the previous solid feature as BaseFeature, and
        # /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp
        # ::DressUp::onChanged() keeps BaseFeature and Base aligned "as long as the feature
        # is inside a body". Native expected collection only freezes that Body-member path.
        raise UnsupportedFixture(
            "PartDesign::Fillet/Chamfer native oracle requires Body Group membership; "
            f"diagnostic-only or standalone DressUp fixtures are skipped: {', '.join(missing)}"
        )


def require_native_polar_pattern_whole_shape_support(fixture: dict) -> None:
    body_members: set[str] = set()
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") != "PartDesign::Body":
            continue
        group = spec.get("Properties", {}).get("Group")
        if isinstance(group, dict) and group.get("PropertyType") == "App::PropertyLinkList":
            body_members.update(str(item) for item in list_field(group, "values", "value"))

    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") != "PartDesign::PolarPattern":
            continue
        properties = spec.get("Properties", {})
        transform_mode = properties.get("TransformMode")
        if transform_mode != "Whole shape":
            continue
        name = str(spec.get("Name"))
        base_feature = properties.get("BaseFeature")
        has_base_feature = isinstance(base_feature, dict) and bool(base_feature.get("value"))
        if name in body_members or has_base_feature:
            continue
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App
        # /FeatureTransformed.cpp::Transformed::execute() fills BaseFeature through
        # Body::setBaseProperty(this) before getBaseObject() in normal Body usage. The existing
        # standalone cad-core fixture is a geometry-equivalent adapter case, not a native Body
        # lifecycle oracle for PolarPattern Whole shape.
        raise UnsupportedFixture(
            "PartDesign::PolarPattern Whole shape native oracle requires Body Group membership "
            "or BaseFeature support; standalone Whole shape fixtures remain CAD Core "
            "geometry-equivalent cases"
        )


def expected_target_names(path: Path) -> list[str] | None:
    if not path.exists():
        return None
    expected = json.loads(path.read_text(encoding="utf-8"))
    if "objects" in expected:
        return list(expected["objects"].keys())
    if "object" in expected:
        return [str(expected["object"])]
    return None


def payload_requires_marker_parity(payload: dict) -> bool:
    if payload.get("native_marker_oracle", {}).get("requires_cad_core_marker_parity"):
        return True
    for summary in payload.get("objects", {}).values():
        if summary.get("native_marker_oracle", {}).get("requires_cad_core_marker_parity"):
            return True
    return False


def solver_distance_types_from_payload(payload: dict) -> set[str]:
    distance_types: set[str] = set()
    summaries = list(payload.get("objects", {}).values())
    if "solver_adapter" in payload:
        summaries.append(payload)
    for summary in summaries:
        for solver_joint in summary.get("solver_adapter", {}).get("solver_joints", []):
            if solver_joint.get("joint_type") != "Distance":
                continue
            distance_type = solver_joint.get("distance_type")
            if isinstance(distance_type, str):
                distance_types.add(distance_type)
    return distance_types


def distance_type_gap_metadata(payload: dict) -> dict[str, Any] | None:
    distance_types = solver_distance_types_from_payload(payload)
    if not distance_types:
        return None

    mapped_extended = distance_types & {
        "LineCircle",
        "CircleCircle",
        "PlaneCylinder",
        "PlaneSphere",
        "PlaneTorus",
        "CylinderCylinder",
        "CylinderSphere",
        "CylinderTorus",
        "TorusTorus",
        "TorusSphere",
        "SphereSphere",
        "PointCylinder",
        "PointSphere",
        "PointCurve",
    }
    default_boundary = {item for item in distance_types if distance_type_is_default_boundary(item)}
    if default_boundary:
        return {
            "known_gap": (
                "DTE-BLOCK-006/DTE-NG-003: native FreeCAD default/TODO DistanceType "
                f"oracle collected for diagnostic review only ({', '.join(sorted(default_boundary))}); "
                "cad-core must keep default_boundary_not_mapped out of supported capability until "
                "S6 or a later product decision explicitly reopens this boundary."
            ),
            "nonGoal": {
                "ids": ["DTE-NG-003"],
                "delete_condition": (
                    "A later DistanceType scope update accepts the FreeCAD default/TODO behavior, "
                    "cad-core implements the reopened case with focused tests, and capability docs "
                    "publish it explicitly instead of inheriting default support."
                ),
            },
        }
    if mapped_extended:
        ids = ["DTE-BLOCK-007"]
        if mapped_extended & {"LineCircle", "CircleCircle"}:
            ids.append("DTE-BLOCK-003")
        if mapped_extended & {
            "PlaneCylinder",
            "PlaneSphere",
            "CylinderCylinder",
            "CylinderSphere",
            "PointCylinder",
            "PointSphere",
        }:
            ids.append("DTE-BLOCK-004")
        if mapped_extended & {"PlaneTorus", "CylinderTorus", "TorusTorus", "TorusSphere", "SphereSphere"}:
            ids.append("DTE-BLOCK-005")
        if mapped_extended & {"PointCurve"}:
            ids.append("DTE-BLOCK-006")
        if payload_requires_marker_parity(payload):
            ids.extend(["MP-BLOCK-002", "MP-BLOCK-003", "MP-BLOCK-006"])
        return {
            "known_gap": (
                "DTE-S5 native extended DistanceType oracle is checked in "
                f"({', '.join(sorted(mapped_extended))}), but cad-core has not completed the "
                "S6 publication/parity gate for this expected file; delete after the listed "
                "blockers are closed and focused expected parity passes."
            ),
            "backendGap": {
                "ids": sorted(dict.fromkeys(ids)),
                "delete_condition": (
                    "S6 keeps this case in the supported subset, cad-core resolves the same "
                    "DistanceType ASMT class/scalar and any required marker placement, and "
                    "CadCoreExpectedFixtureTest passes without skipping this expected file."
                ),
            },
        }
    return None


def collect_one(fixture_path: Path, requested_targets: Sequence[str] | None = None) -> dict:
    import FreeCAD  # type: ignore

    fixture = load_fixture(fixture_path)
    if "partGeometryCurve" in fixture:
        return collect_part_geometry_curve_expected(fixture_path, fixture)
    require_native_hole_profile_support(fixture)
    require_native_dressup_body_membership(fixture)
    require_native_polar_pattern_whole_shape_support(fixture)
    doc = FreeCAD.newDocument("CadCoreExpected")
    try:
        created = create_objects(FreeCAD, doc, fixture)
        doc.recompute()

        targets = list(requested_targets) if requested_targets is not None else target_names(fixture)
        object_payloads: dict[str, dict] = {}
        for name in targets:
            obj = created.get(name)
            if obj is None:
                raise UnsupportedFixture(f"target object {name} was not created")
            object_payloads[name] = object_expected_payload(obj, fixture, created)

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
        distance_type_gap = distance_type_gap_metadata(payload)
        if distance_type_gap is not None:
            payload.update(distance_type_gap)
        elif payload_requires_marker_parity(payload):
            payload["known_gap"] = (
                "MP-BLOCK-002/003/006: S4 native marker oracle is checked in, but current "
                "cad-core S3 resolver still withholds subshape markerPlacement and the real "
                "Ondsel path falls back to identity markers; delete this in S5 after resolver "
                "parity makes placement_updates match this native expected."
            )
            payload["backendGap"] = {
                "ids": ["MP-BLOCK-002", "MP-BLOCK-003", "MP-BLOCK-006"],
                "delete_condition": (
                    "S5 implements FreeCAD handleOneSideOfJoint object-global to part-local "
                    "marker resolution and focused expected parity passes for this fixture."
                ),
            }
        diagnostic_codes = []
        for summary in object_payloads.values():
            if summary.get("native_solver", {}).get("return_code") == -6:
                diagnostic_codes.append("missing_grounded_part")
        if diagnostic_codes:
            payload["diagnostic_codes"] = sorted(set(diagnostic_codes))
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
    for key, expected_value in existing.get("object_fields", {}).items():
        if generated.get("object_fields", {}).get(key) != expected_value:
            errors.append(f"object_fields.{key}")
    if "sketch_internal" in existing:
        expected_internal = existing["sketch_internal"]
        generated_internal = generated.get("sketch_internal", {})
        for key in ("shape", "profile_ready", "raw_edge_count"):
            if key in expected_internal and generated_internal.get(key) != expected_internal[key]:
                errors.append(f"sketch_internal.{key}")
        for key, expected_value in expected_internal.get("internal_counts", {}).items():
            if generated_internal.get("internal_counts", {}).get(key) != expected_value:
                errors.append(f"sketch_internal.internal_counts.{key}")
        for key, expected_value in expected_internal.get("min_internal_counts", {}).items():
            generated_value = generated_internal.get("internal_counts", {}).get(key)
            if generated_value is None or generated_value < expected_value:
                errors.append(f"sketch_internal.min_internal_counts.{key}")
    if "bbox" in existing and not compare_bbox(existing["bbox"], generated["bbox"], bbox_delta):
        errors.append("bbox")
    if "volume" in existing and not close_enough(existing["volume"], generated["volume"], existing.get("volume_delta", 1e-6)):
        errors.append("volume")
    if "length" in existing and not close_enough(existing["length"], generated["length"], existing.get("length_delta", 1e-6)):
        errors.append("length")
    if "topology_counts" in existing and existing["topology_counts"] != generated["topology_counts"]:
        errors.append("topology_counts")
    if "sketch_external" in existing:
        generated_external = generated.get("sketch_external", {})
        for key, expected_value in existing["sketch_external"].items():
            if generated_external.get(key) != expected_value:
                errors.append(f"sketch_external.{key}")
    if "solver_adapter" in existing and existing["solver_adapter"] != generated.get("solver_adapter"):
        errors.append("solver_adapter")
    if "native_solver" in existing and existing["native_solver"] != generated.get("native_solver"):
        errors.append("native_solver")
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


def expected_has_native_geometry_payload(path: Path) -> bool:
    expected = json.loads(path.read_text(encoding="utf-8"))
    return "object" in expected or "objects" in expected


def run_inside_freecad(args: argparse.Namespace) -> int:
    fixtures_root = Path(args.fixtures_root)
    failures = 0
    skipped = 0
    for fixture_path in fixture_paths(args):
        out_path = Path(args.out) if args.out else expected_path_for_fixture(fixtures_root, fixture_path)
        target_override: list[str] | None = None
        if args.check:
            if args.phase and args.skip_unsupported and not out_path.exists():
                skipped += 1
                print(f"skip missing expected {fixture_path}", file=sys.stderr)
                continue
            if out_path.exists() and not expected_has_native_geometry_payload(out_path):
                if args.phase and args.skip_unsupported:
                    skipped += 1
                    print(f"skip non-geometry expected {fixture_path}", file=sys.stderr)
                    continue
                print(f"unsupported expected for native geometry check: {out_path}", file=sys.stderr)
                failures += 1
                continue
            target_override = expected_target_names(out_path)

        try:
            payload = collect_one(fixture_path, target_override)
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

        if args.check:
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
