#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import re
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FREECADCMD = "/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd"
SCHEMA_VERSION = "cad-core.freecad-expected.v1"
PRODUCER_TRACE_SCHEMA = "freecad.element-map-producer-trace.v1"
TOPO_STATE_SCHEMA_VERSION = "cad-core.topo-state.v1"
TOPO_STATE_PRODUCER_CAD_CORE_VERSION = "fixture-contract-v1"
# Keep the FreeCADCmd oracle's request validator aligned with CAD Core runtime
# `producerIsCompatible()`: a state freshly published without a prior fixture
# producer carries `cad-core-runtime-v1`, while arbitrary producers remain a
# request-level hard failure.
TOPO_STATE_COMPATIBLE_CAD_CORE_VERSIONS = {
    TOPO_STATE_PRODUCER_CAD_CORE_VERSION,
    "cad-core-runtime-v1",
}
LEDGER_SCHEMA = "freecad-toponaming-ledger/v1"
LEDGER_SCRIPT_VERSION = "collect_freecad_expected.py:ledger-v1"
ENV_ARG_MARKER = "__cad_core_expected_args_env__"
ENV_ARG_NAME = "CAD_CORE_EXPECTED_ARGS_JSON"
FREECAD_PRECISION_CONFUSION = 1e-7
FREECAD_MAPPED_NAME_HASH_RE = re.compile(r":H(?!\*)-?[0-9A-Fa-f]+(?::[0-9A-Fa-f]+)?")
FREECAD_MAPPED_NAME_COLON_HASH_RE = re.compile(r":H:(?!\*)-?[0-9A-Fa-f]+")
FREECAD_MAPPED_NAME_DELETE_RE = re.compile(r";D(?!\*)[0-9A-Fa-f]+")
TOPO_INDEX_NAME_RE = re.compile(r"^(InternalFace|InternalEdge|InternalVertex|Face|Edge|Vertex|Wire|Shell|Solid|Compound)\d+$")
TOPO_CHILD_INDEX_PATH_RE = re.compile(
    r"^(?:Child\d+\.)+(InternalFace|InternalEdge|InternalVertex|Face|Edge|Vertex|Wire|Shell|Solid|Compound)\d+$"
)
TOPO_DISPLAY_PATH_STABLE_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\.(Face|Edge|Vertex)\d+$")
ACTIVE_TOPO_NAMING_STATE: dict[str, Any] | None = None
ACTIVE_RESOLVED_REFERENCE_BINDINGS: list[dict[str, str]] = []
LAST_FREECAD_LEDGER_CAPTURE: dict[str, Any] | None = None
LAST_FREECAD_PRODUCER_TRACE: dict[str, Any] | None = None
SUPPORTED_NATIVE_TYPES = {
    "App::FeaturePython",
    "App::Line",
    "App::Link",
    "App::LinkElement",
    "App::LinkGroup",
    "Assembly::AssemblyLink",
    "Assembly::AssemblyObject",
    "Assembly::JointGroup",
    "Mesh::Import",
    "PartDesign::Body",
    "PartDesign::AdditiveLoft",
    "PartDesign::AdditivePipe",
    "PartDesign::Boolean",
    "PartDesign::Chamfer",
    "PartDesign::CoordinateSystem",
    "PartDesign::Fillet",
    "PartDesign::FeatureBase",
    "PartDesign::Hole",
    "PartDesign::Line",
    "PartDesign::LinearPattern",
    "PartDesign::Mirrored",
    "PartDesign::MultiTransform",
    "PartDesign::Pad",
    "PartDesign::Plane",
    "PartDesign::Point",
    "PartDesign::PolarPattern",
    "PartDesign::Pocket",
    "PartDesign::Revolution",
    "PartDesign::Groove",
    "PartDesign::SubtractiveLoft",
    "PartDesign::SubtractivePipe",
    "PartDesign::Scaled",
    "Part::Box",
    "Part::BooleanFragments",
    "Part::Common",
    "Part::Compound",
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
    "Part::ProjectOnSurface",
    "Part::RegularPolygon",
    "Part::RuledSurface",
    "Part::Section",
    "Part::Sphere",
    "Part::Spiral",
    "Part::Sweep",
    "Part::Torus",
    "Part::Vertex",
    "Part::Wedge",
    "Part::XOR",
    "Sketcher::SketchObject",
}

HOLE_PRE_BODY_PROPERTIES = {
    "Profile",
}

NATIVE_PROFILE_LINK_SUB_TYPES = {
    "PartDesign::Groove",
    "PartDesign::Hole",
    "PartDesign::Pad",
    "PartDesign::Pocket",
    "PartDesign::Revolution",
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

BODY_RESULT_TARGET_TYPES = DRESS_UP_TYPES | TRANSFORMED_TYPES | {"PartDesign::Boolean"}
TOPO_LEDGER_ALWAYS_TYPES = {
    "App::Link",
    "App::LinkElement",
    "PartDesign::Body",
    "Part::Compound",
}
PART_HELPER_TYPES = {
    "Part::FilledFace",
    "Part::GeomPlateSurface",
}
PART_SWEEP_WRAPPER_ADVANCED_FIELDS = {
    "AuxiliarySpine",
    "AuxiliaryCurvilinear",
    "SpineSupport",
    "SupportMode",
    "Binormal",
    "BiNormal",
    "SectionOptions",
    "Tolerance",
}
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
        description="Collect CAD Core public expected, ledger, and producer-trace sidecars from native FreeCAD.",
    )
    parser.add_argument("fixture", nargs="?", help="Fixture JSON file. Omit when --phase is used.")
    parser.add_argument("--phase", help="Collect every supported fixture in fixtures/<phase>.")
    parser.add_argument("--fixtures-root", default=str(ROOT / "fixtures"), help="Fixture root directory.")
    parser.add_argument("--out", help="Output expected file for a single fixture.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Compare regenerated public expected and ledger; require a valid producer-trace sidecar.",
    )
    parser.add_argument("--pretty", action="store_true", help="Print generated JSON to stdout.")
    parser.add_argument("--skip-unsupported", action="store_true", help="Skip unsupported fixtures in --phase mode.")
    parser.add_argument(
        "--emit-ledger",
        action="store_true",
        help="Legacy no-op: collector always writes the ledger and producer-trace sidecars.",
    )
    parser.add_argument(
        "--check-ledger",
        action="store_true",
        help="Legacy no-op with --check: the ledger sidecar is always compared.",
    )
    parser.add_argument("--validate-ledger", action="store_true", help="Validate emitted ledger sidecars before returning success.")
    parser.add_argument(
        "--freecadcmd",
        default=DEFAULT_FREECADCMD,
        help=f"Native producer-enabled FreeCADCmd (default: {DEFAULT_FREECADCMD})",
    )
    args = parser.parse_args(script_args(argv))
    if args.check_ledger and not args.check:
        parser.error("--check-ledger requires --check")
    return args


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


def semantic_hash(value: Any) -> str:
    canonical = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return "sha256:" + hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def atomic_write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def producer_trace_path_for_expected(expected_path: Path) -> Path:
    name = expected_path.name
    if not name.endswith(".freecad.json"):
        raise ValueError(f"not a FreeCAD expected path: {expected_path}")
    return expected_path.with_name(f"{name[:-len('.freecad.json')]}.freecad.producer-trace.json")


def validate_producer_trace(trace: Any) -> dict[str, Any]:
    if not isinstance(trace, dict) or trace.get("schemaVersion") != PRODUCER_TRACE_SCHEMA:
        raise RuntimeError("native ElementMap producer trace has an invalid schema")
    events = trace.get("events")
    transactions = trace.get("transactions")
    if not isinstance(events, list) or not isinstance(transactions, list) or not events:
        raise RuntimeError("native ElementMap producer trace is empty or malformed")

    snapshot_ids = set(trace.get("ledgerSnapshots", {}))
    snapshot_ids.update(trace.get("stringTableSnapshots", {}))
    snapshot_ids.update(trace.get("mapperSnapshots", {}))
    open_scopes: set[int] = set()
    for sequence, event in enumerate(events, start=1):
        if not isinstance(event, dict) or event.get("sequence") != sequence:
            raise RuntimeError(f"native ElementMap producer trace has a sequence gap at {sequence}")
        if event.get("beforeSnapshot") not in snapshot_ids or event.get("afterSnapshot") not in snapshot_ids:
            raise RuntimeError(f"native ElementMap producer trace event {sequence} references a missing snapshot")
        scope = event.get("scopeSequence", 0)
        if event.get("slice") == "scope.begin":
            if not isinstance(scope, int) or scope <= 0 or scope in open_scopes:
                raise RuntimeError(f"native ElementMap producer trace has an invalid scope begin at {sequence}")
            open_scopes.add(scope)
        elif event.get("slice") in {"scope.end", "scope.abort"}:
            if scope not in open_scopes:
                raise RuntimeError(f"native ElementMap producer trace has an unpaired scope end at {sequence}")
            open_scopes.remove(scope)
    if open_scopes:
        raise RuntimeError("native ElementMap producer trace has unclosed scopes")
    return trace


def drain_producer_trace(doc: Any) -> dict[str, Any]:
    drain = getattr(doc, "drainElementMapProducerTrace", None)
    if not callable(drain):
        raise RuntimeError(
            "FreeCADCmd lacks Document.drainElementMapProducerTrace(); "
            f"use {DEFAULT_FREECADCMD} or a compatible native build"
        )
    return validate_producer_trace(drain())


def close_document_with_producer_trace(FreeCAD: Any, doc: Any) -> None:
    global LAST_FREECAD_PRODUCER_TRACE
    try:
        trace = drain_producer_trace(doc)
        if LAST_FREECAD_PRODUCER_TRACE is None or len(trace["events"]) >= len(LAST_FREECAD_PRODUCER_TRACE["events"]):
            LAST_FREECAD_PRODUCER_TRACE = trace
    finally:
        FreeCAD.closeDocument(doc.Name)


def publish_documentless_producer_trace(
    FreeCAD: Any,
    *,
    slice_name: str,
    decision: str,
    reason: str,
    target: str,
) -> None:
    doc = FreeCAD.newDocument("CadCoreExpectedProducerTrace")
    try:
        record = getattr(doc, "recordElementMapProducerTraceCheckpoint", None)
        if not callable(record):
            raise RuntimeError(
                "FreeCADCmd lacks Document.recordElementMapProducerTraceCheckpoint(); "
                f"use {DEFAULT_FREECADCMD} or a compatible native build"
            )
        record(slice_name, decision, reason, target)
    finally:
        close_document_with_producer_trace(FreeCAD, doc)


def producer_trace_rejection_reason(payload: dict[str, Any], fallback: str) -> str:
    diagnostics = payload.get("diagnostics")
    if isinstance(diagnostics, list):
        for diagnostic in diagnostics:
            if isinstance(diagnostic, dict) and isinstance(diagnostic.get("code"), str):
                return diagnostic["code"]
    return fallback


def expected_path_for_fixture(fixtures_root: Path, fixture_path: Path) -> Path:
    phase = fixture_path.parent.name
    return fixtures_root / phase / "expected" / f"{fixture_path.stem}.freecad.json"


def ledger_path_for_expected(expected_path: Path) -> Path:
    name = expected_path.name
    if not name.endswith(".freecad.json"):
        raise ValueError(f"not a FreeCAD expected path: {expected_path}")
    return expected_path.with_name(f"{name[:-len('.freecad.json')]}.freecad.ledger.json")


def fixture_paths(args: argparse.Namespace) -> list[Path]:
    fixtures_root = Path(args.fixtures_root)
    if args.phase:
        if args.fixture:
            raise ValueError("--phase and fixture path are mutually exclusive")
        catalog = fixture_role_catalog(args)
        return [item.input_path for item in catalog.cases]
    if not args.fixture:
        raise ValueError("fixture path or --phase is required")
    return [Path(args.fixture)]


def fixture_role_catalog(args: argparse.Namespace) -> Any:
    """Load the one role manifest used by collection and parity discovery."""

    cached = getattr(args, "_fixture_role_catalog", None)
    if cached is not None:
        return cached
    if not args.phase:
        raise ValueError("fixture role catalog is only used for --phase collection")
    try:
        try:
            from freecad_expected_parity.catalog import load_catalog  # type: ignore
        except ImportError:
            from tools.freecad_expected_parity.catalog import load_catalog  # type: ignore
    except ImportError as exc:
        raise ValueError(f"cannot load fixture role catalog: {exc}") from exc
    catalog = load_catalog(Path(args.fixtures_root).parent, phase=args.phase)
    if catalog.errors:
        raise ValueError("invalid fixture role catalog: " + "; ".join(catalog.errors))
    args._fixture_role_catalog = catalog
    return catalog


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


def link_list_targets(value: dict, field: str) -> list[str]:
    targets: list[str] = []
    for index, item in enumerate(list_field(value, "values", "value")):
        if isinstance(item, str):
            targets.append(item)
            continue
        if isinstance(item, dict):
            raise UnsupportedFixture(
                f"{field}[{index}] uses a subelement link; native App::PropertyLinkList "
                "collector only supports object links"
            )
        raise UnsupportedFixture(f"{field}[{index}] must be an object name")
    return targets


def created_link_list(created: dict[str, Any], value: dict, field: str) -> list[Any]:
    objects = []
    for target in link_list_targets(value, field):
        if target not in created:
            raise UnsupportedFixture(f"{field} target {target} was not created")
        objects.append(created[target])
    return objects


def display_path_stable_token(value: Any) -> bool:
    return isinstance(value, str) and TOPO_DISPLAY_PATH_STABLE_RE.match(value) is not None


def reject_display_path_stable_token(value: Any, field: str) -> None:
    if display_path_stable_token(value):
        raise UnsupportedFixture(
            f"{field} cannot use display path {value}; use a FreeCAD mapped name or "
            "a topoNamingState elementMap stable token"
        )


def stable_identity_contract_errors(value: Any, label: str, path: str = "$") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        mapped_name = value.get("mappedName")
        if isinstance(mapped_name, dict):
            for field in ("raw", "canonical"):
                token = mapped_name.get(field)
                if display_path_stable_token(token):
                    errors.append(f"{label}:{path}.mappedName.{field}.display_path")

        entries = value.get("entries")
        if value.get("encoding") == "cad-core.element-map.v1" and isinstance(entries, dict):
            for token in entries:
                if display_path_stable_token(token):
                    errors.append(f"{label}:{path}.entries.{token}.display_path_key")

        for key, item in value.items():
            item_path = f"{path}.{key}"
            if key in {"stableSubname", "rawFreecadMappedName", "canonicalFreecadMappedName"}:
                if display_path_stable_token(item):
                    errors.append(f"{label}:{item_path}.display_path")
            elif key == "StableSubList" and isinstance(item, list):
                for index, stable_token in enumerate(item):
                    if display_path_stable_token(stable_token):
                        errors.append(f"{label}:{item_path}[{index}].display_path")
            errors.extend(stable_identity_contract_errors(item, label, item_path))
    elif isinstance(value, list):
        for index, item in enumerate(value):
            errors.extend(stable_identity_contract_errors(item, label, f"{path}[{index}]"))
    return errors


def state_backed_stable_subname(value: dict, stable_subname: Any) -> str:
    target_name = value.get("value")
    if not isinstance(target_name, str) or not target_name:
        raise UnsupportedFixture("topoNamingState StableSubList item requires a string value target")
    if ACTIVE_TOPO_NAMING_STATE is None:
        raise UnsupportedFixture("topoNamingState StableSubList requires fixture topoNamingState")

    objects = ACTIVE_TOPO_NAMING_STATE.get("objects")
    if not isinstance(objects, dict):
        raise UnsupportedFixture("topoNamingState.objects must be an object")
    object_state = objects.get(target_name)
    if not isinstance(object_state, dict):
        raise UnsupportedFixture(f"topoNamingState missing object state for {target_name}")

    element_map = object_state.get("elementMap")
    if not isinstance(element_map, dict):
        raise UnsupportedFixture(f"topoNamingState object {target_name} missing elementMap")
    entries = element_map.get("entries")
    if not isinstance(entries, dict):
        raise UnsupportedFixture(f"topoNamingState object {target_name} elementMap.entries must be an object")

    token = str(stable_subname)
    reject_display_path_stable_token(token, "topoNamingState StableSubList")
    entry = matching_topo_state_entry(entries, token)
    if not isinstance(entry, dict):
        child_maps = object_state.get("childElementMaps")
        if isinstance(child_maps, list):
            for child_map in child_maps:
                if not isinstance(child_map, dict):
                    continue
                child_entries = child_map.get("elementMap", {}).get("entries")
                entry = matching_topo_state_entry(child_entries, token)
                if isinstance(entry, dict):
                    break
    if not isinstance(entry, dict):
        mapper_history = object_state.get("mapperHistory")
        if isinstance(mapper_history, list):
            for event in mapper_history:
                if not isinstance(event, dict) or not mapper_history_event_matches(event, token):
                    continue
                relation = event.get("relation")
                target = event.get("target")
                target_subname = target.get("subname") if isinstance(target, dict) else None
                if relation in {"generated", "modified", "merge", "historical"} and isinstance(target_subname, str) and target_subname:
                    return target_subname
                break
        raise UnsupportedFixture(f"topoNamingState object {target_name} cannot resolve StableSubList {token}")
    target = entry.get("target")
    if not isinstance(target, dict):
        raise UnsupportedFixture(f"topoNamingState StableSubList {token} missing target")
    target_object = target.get("object")
    if target_object != target_name:
        raise UnsupportedFixture(
            f"topoNamingState StableSubList {token} resolves to {target_object}, expected {target_name}"
        )
    target_subname = target.get("subname")
    if not isinstance(target_subname, str) or not target_subname:
        raise UnsupportedFixture(f"topoNamingState StableSubList {token} missing target.subname")
    return target_subname


def native_sub_list(value: dict) -> list:
    stable_sub_list = value.get("StableSubList")
    if isinstance(stable_sub_list, list) and stable_sub_list:
        if value.get("StableSubListSource") == "topoNamingState":
            return [state_backed_stable_subname(value, item) for item in stable_sub_list]
        return list(stable_sub_list)
    sub_list = value.get("SubList")
    if sub_list is not None:
        return list(sub_list)
    if stable_sub_list is not None:
        return list(stable_sub_list)
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


NATIVE_RESOLUTION_EVIDENCE = {
    "FreeCAD.getSubObject",
    "FreeCAD.Shape.getElement",
    "FreeCAD.objectReference",
    "rawTopoNamingState.subshapes",
    "rawTopoNamingState.elementMap",
}


def freecad_resolved_value_is_valid(value: Any) -> bool:
    if value is None:
        return False
    shape = getattr(value, "Shape", value)
    is_null = getattr(shape, "isNull", None)
    if callable(is_null):
        try:
            return not bool(is_null())
        except Exception:
            return False
    return True


def native_subname_resolution_evidence(target: Any, subname: str) -> str | None:
    if not subname:
        return "FreeCAD.objectReference" if target is not None else None
    get_sub_object = getattr(target, "getSubObject", None)
    if callable(get_sub_object):
        try:
            if freecad_resolved_value_is_valid(get_sub_object(subname)):
                return "FreeCAD.getSubObject"
        except Exception:
            pass
    shape = getattr(target, "Shape", None)
    get_element = getattr(shape, "getElement", None)
    if callable(get_element):
        try:
            if freecad_resolved_value_is_valid(get_element(subname)):
                return "FreeCAD.Shape.getElement"
        except Exception:
            pass
    return None


def record_resolved_reference_bindings(
    value: dict[str, Any],
    native_resolved_subnames: Sequence[str],
    *,
    owner: str,
    property_path: str,
    resolved_object: str | None = None,
    published_resolved_subnames: Sequence[str] | None = None,
    native_resolution_evidence: Sequence[str | None] | None = None,
    assignment_succeeded: bool = False,
) -> None:
    stable_sub_list = value.get("StableSubList")
    target_name = value.get("value")
    if (
        value.get("StableSubListSource") != "topoNamingState"
        or not isinstance(stable_sub_list, list)
        or not isinstance(target_name, str)
        or len(stable_sub_list) != len(native_resolved_subnames)
    ):
        return
    if (
        published_resolved_subnames is not None
        and len(published_resolved_subnames) != len(native_resolved_subnames)
    ):
        raise UnsupportedFixture(
            "native and published resolved subname counts must match"
        )
    if (
        native_resolution_evidence is not None
        and len(native_resolution_evidence) != len(native_resolved_subnames)
    ):
        raise UnsupportedFixture(
            "native resolution evidence count must match resolved subnames"
        )
    published_subnames = (
        published_resolved_subnames
        if published_resolved_subnames is not None
        else native_resolved_subnames
    )
    resolution_evidence = (
        native_resolution_evidence
        if native_resolution_evidence is not None
        else [None] * len(native_resolved_subnames)
    )
    for stable_subname, native_subname, published_subname, evidence in zip(
        stable_sub_list,
        native_resolved_subnames,
        published_subnames,
        resolution_evidence,
    ):
        if not isinstance(stable_subname, str) or not stable_subname:
            continue
        binding = {
            "owner": owner,
            "propertyPath": property_path,
            "target": target_name,
            "stableSubname": stable_subname,
            "resolvedObject": resolved_object or target_name,
            "resolvedSubname": str(published_subname),
            "nativeResolvedSubname": str(native_subname),
            "nativeResolutionEvidence": evidence,
            "assignmentStatus": (
                "succeeded"
                if assignment_succeeded and evidence in NATIVE_RESOLUTION_EVIDENCE
                else "assigned" if assignment_succeeded else "pending"
            ),
        }
        binding_key = (
            binding["owner"],
            binding["propertyPath"],
            binding["target"],
            binding["stableSubname"],
        )
        ACTIVE_RESOLVED_REFERENCE_BINDINGS[:] = [
            existing
            for existing in ACTIVE_RESOLVED_REFERENCE_BINDINGS
            if (
                existing.get("owner"),
                existing.get("propertyPath"),
                existing.get("target"),
                existing.get("stableSubname"),
            )
            != binding_key
        ]
        ACTIVE_RESOLVED_REFERENCE_BINDINGS.append(binding)


def mark_resolved_reference_bindings_succeeded(
    owner: str,
    property_path_prefix: str,
) -> None:
    for binding in ACTIVE_RESOLVED_REFERENCE_BINDINGS:
        property_path = binding.get("propertyPath")
        if (
            binding.get("owner") == owner
            and isinstance(property_path, str)
            and (
                property_path == property_path_prefix
                or property_path.startswith(property_path_prefix + ".SubSet.")
            )
            and binding.get("nativeResolutionEvidence") in NATIVE_RESOLUTION_EVIDENCE
        ):
            binding["assignmentStatus"] = "succeeded"
        elif (
            binding.get("owner") == owner
            and isinstance(property_path, str)
            and (
                property_path == property_path_prefix
                or property_path.startswith(property_path_prefix + ".SubSet.")
            )
        ):
            binding["assignmentStatus"] = "assigned"


def finalize_resolved_reference_bindings(
    created: dict[str, Any] | None,
    raw_topo_state: dict[str, Any],
) -> None:
    raw_objects = raw_topo_state.get("objects")
    if not isinstance(raw_objects, dict):
        raw_objects = {}
    for binding in ACTIVE_RESOLVED_REFERENCE_BINDINGS:
        if binding.get("assignmentStatus") != "assigned":
            continue
        resolved_object = binding.get("resolvedObject")
        resolved_subname = binding.get("resolvedSubname")
        native_subname = binding.get("nativeResolvedSubname")
        target = created.get(resolved_object) if isinstance(created, dict) else None
        evidence = (
            native_subname_resolution_evidence(target, native_subname)
            if isinstance(native_subname, str)
            else None
        )
        if evidence is None:
            object_state = raw_objects.get(resolved_object)
            subshapes = object_state.get("subshapes") if isinstance(object_state, dict) else None
            if (
                isinstance(subshapes, dict)
                and isinstance(resolved_subname, str)
                and resolved_subname in subshapes
            ):
                evidence = "rawTopoNamingState.subshapes"
        if evidence is None:
            entry = ledger_topo_state_entry_for_token(
                raw_objects,
                binding.get("target"),
                binding.get("stableSubname"),
            )
            entry_target = dict_items(entry.get("target")) if isinstance(entry, dict) else {}
            if (
                entry_target.get("object") == resolved_object
                and entry_target.get("subname") == resolved_subname
            ):
                evidence = "rawTopoNamingState.elementMap"
        if evidence in NATIVE_RESOLUTION_EVIDENCE:
            binding["nativeResolutionEvidence"] = evidence
            binding["assignmentStatus"] = "succeeded"


def freecad_ledger_capture(
    raw_topo_state: dict[str, Any],
    published_topo_state: dict[str, Any],
    resolved_reference_bindings: Sequence[dict[str, str]],
) -> dict[str, Any]:
    return {
        "rawTopoNamingState": copy.deepcopy(raw_topo_state),
        "publishedTopoNamingState": copy.deepcopy(published_topo_state),
        "resolvedReferenceBindings": copy.deepcopy([
            binding
            for binding in resolved_reference_bindings
            if binding.get("assignmentStatus") == "succeeded"
        ]),
    }


def link_sub_value(
    created: dict[str, Any],
    value: dict,
    *,
    owner: str = "",
    property_path: str = "",
) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"link target {target_name} was not created")
    target = created[target_name]
    # FreeCAD oracle mode: CAD Core fixtures may keep a stale user-visible SubList and a
    # StableSubList that represents the stable element to be resolved through ElementMap.
    # Native FreeCAD has no JSON StableSubList property, so the collector feeds the stable
    # subname to PropertyLinkSub to collect the expected post-resolution geometry.
    sub_list = native_sub_list(value)
    if sub_list:
        native_subnames = [native_link_subname(target, subname) for subname in sub_list]
        record_resolved_reference_bindings(
            value,
            native_subnames,
            owner=owner,
            property_path=property_path,
            native_resolution_evidence=[
                native_subname_resolution_evidence(target, subname)
                for subname in native_subnames
            ],
        )
        return target, native_subnames
    return target


def profile_link_sub_value_from_sublist(
    created: dict[str, Any],
    value: dict,
    *,
    owner: str,
    property_path: str,
) -> Any:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.h
    # ::ProfileBased stores "App::PropertyLinkSub Profile". cad-core fixtures intentionally model
    # Profile as PropertyLinkSubList so multiple selected faces can share one recompute DTO; native
    # oracle collection folds same-target SubSet items into the single FreeCAD Profile link-sub slot.
    sub_set = value.get("SubSet")
    if not isinstance(sub_set, list):
        raise UnsupportedFixture("Profile.SubSet must be a list")
    if not sub_set:
        raise UnsupportedFixture("Profile.SubSet must contain at least one item")

    target_name: str | None = None
    target = None
    subnames: list[str] = []
    for item_index, item in enumerate(sub_set):
        if not isinstance(item, dict):
            raise UnsupportedFixture("Profile.SubSet items must be objects")
        item_target_name = item.get("value")
        if item_target_name not in created:
            raise UnsupportedFixture(f"link target {item_target_name} was not created")
        if target_name is None:
            target_name = item_target_name
            target = created[item_target_name]
        elif item_target_name != target_name:
            raise UnsupportedFixture(
                "FreeCAD native Profile is App::PropertyLinkSub and cannot collect "
                "multi-target Profile.SubSet directly"
            )
        item_target = created[item_target_name]
        item_subnames = [native_link_subname(item_target, subname) for subname in native_sub_list(item)]
        record_resolved_reference_bindings(
            item,
            item_subnames,
            owner=owner,
            property_path=f"{property_path}.SubSet.{item_index}",
            native_resolution_evidence=[
                native_subname_resolution_evidence(item_target, subname)
                for subname in item_subnames
            ],
        )
        subnames.extend(item_subnames)

    if subnames:
        return target, subnames
    return target


def assembly_joint_reference_value(
    created: dict[str, Any],
    value: dict,
    *,
    owner: str,
    property_path: str,
) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"assembly joint reference target {target_name} was not created")
    target = created[target_name]
    sub_list = native_sub_list(value)
    if sub_list:
        native_subnames = [native_link_subname(target, subname) for subname in sub_list]
        record_resolved_reference_bindings(
            value,
            native_subnames,
            owner=owner,
            property_path=property_path,
            native_resolution_evidence=[
                native_subname_resolution_evidence(target, subname)
                for subname in native_subnames
            ],
        )
        return target, native_subnames
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

    for item_index, item in enumerate(sub_set):
        if not isinstance(item, dict):
            raise UnsupportedFixture("Sketch ExternalGeometry.SubSet items must be objects")
        target_name = item.get("value")
        if target_name not in created:
            raise UnsupportedFixture(f"external geometry target {target_name} was not created")

        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App
        # /SketchObjectExternal.cpp::SketchObject::addExternal(), the Python wrapper takes
        # "ObjectName" and "SubName", then stores ExternalGeometry.setValues(Objects,
        # SubElements). In oracle mode, resolve state-backed StableSubList through the
        # fixture topoNamingState before feeding native FreeCAD's current subname.
        flags = external_geometry_flags_from_item(item)
        for subname_index, subname in enumerate(native_sub_list(item)):
            external_target_name, external_subname = resolve_external_subname(created, target_name, subname)
            binding_property_path = f"ExternalGeometry.SubSet.{item_index}"
            stable_sub_list = item.get("StableSubList")
            if isinstance(stable_sub_list, list) and subname_index < len(stable_sub_list):
                record_resolved_reference_bindings(
                    {
                        **item,
                        "StableSubList": [stable_sub_list[subname_index]],
                    },
                    [external_subname],
                    owner=str(getattr(obj, "Name", "")),
                    property_path=binding_property_path,
                    resolved_object=external_target_name,
                    native_resolution_evidence=[
                        native_subname_resolution_evidence(
                            created[external_target_name],
                            external_subname,
                        )
                    ],
                )
            try:
                # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectPyImp.cpp
                # ::SketchObjectPy::addExternal(), parses "ss|O!O!" and forwards "defining" to
                # /home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp
                # ::SketchObject::addExternal(), which sets ExternalGeometryExtension::Defining
                # during rebuildExternalGeometry().
                obj.addExternal(external_target_name, external_subname, "Defining" in flags)
                mark_resolved_reference_bindings_succeeded(
                    str(getattr(obj, "Name", "")),
                    binding_property_path,
                )
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
        owner_name = str(getattr(obj, "Name", ""))
        property_type = value.get("PropertyType")
        if value.get("__assembly_joint_reference"):
            resolved_value = assembly_joint_reference_value(
                created,
                value,
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(
                obj,
                name,
                resolved_value,
            )
            mark_resolved_reference_bindings_succeeded(owner_name, name)
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
            "App::PropertyDistance",
            "App::PropertyString",
        }:
            safe_setattr(obj, name, value.get("value"))
            return
        if property_type in {"App::PropertyVector", "App::PropertyVectorDistance", "App::PropertyDirection"}:
            vector = value.get("value")
            if not isinstance(vector, list) or len(vector) != 3:
                raise UnsupportedFixture(f"{name} must be a 3D vector")
            safe_setattr(obj, name, FreeCAD.Vector(float(vector[0]), float(vector[1]), float(vector[2])))
            return
        if property_type == "App::PropertyPlacementList":
            safe_setattr(obj, name, [placement_value(FreeCAD, item) for item in value.get("value", [])])
            return
        if property_type == "App::PropertyVectorList":
            vectors = []
            for index, item in enumerate(value.get("value", [])):
                if not isinstance(item, list) or len(item) != 3:
                    raise UnsupportedFixture(f"{name}[{index}] must be a 3D vector")
                vectors.append(FreeCAD.Vector(float(item[0]), float(item[1]), float(item[2])))
            safe_setattr(obj, name, vectors)
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
            resolved_value = link_sub_value(
                created,
                value,
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(obj, name, resolved_value)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
            return
        if property_type in {
            "App::PropertyLinkSub",
            "App::PropertyLinkSubHidden",
            "App::PropertyXLinkSub",
            "App::PropertyXLinkSubHidden",
        }:
            resolved_value = link_sub_value(
                created,
                value,
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(obj, name, resolved_value)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
            return
        if property_type in {"App::PropertyLinkList", "App::PropertyLinkListHidden"}:
            safe_setattr(obj, name, created_link_list(created, value, name))
            return
        if property_type in {
            "App::PropertyLinkSubList",
            "App::PropertyLinkSubListHidden",
            "App::PropertyXLinkSubList",
        }:
            if name == "Profile" and getattr(obj, "TypeId", "") in NATIVE_PROFILE_LINK_SUB_TYPES:
                resolved_value = profile_link_sub_value_from_sublist(
                    created,
                    value,
                    owner=owner_name,
                    property_path=name,
                )
                safe_setattr(
                    obj,
                    name,
                    resolved_value,
                )
                mark_resolved_reference_bindings_succeeded(owner_name, name)
                return
            resolved_items = []
            for item_index, item in enumerate(value.get("SubSet", [])):
                resolved_items.append(
                    link_sub_value(
                        created,
                        item,
                        owner=owner_name,
                        property_path=f"{name}.SubSet.{item_index}",
                    )
                )
            safe_setattr(obj, name, resolved_items)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
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
            try:
                names.update(link_list_targets(element_list, "LinkGroup ElementList"))
            except UnsupportedFixture:
                continue
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
    elements = created_link_list(created, value, "LinkGroup ElementList")

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
    if name == "Group" and property_type in {"App::PropertyLinkList", "App::PropertyLinkSubList"}:
        # FreeCAD: /Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BodyBase.cpp
        # ::BodyBase::addObject() is the supported membership path; writing Group directly skips
        # the same ownership bookkeeping that PartDesign recompute depends on.
        if property_type == "App::PropertyLinkSubList":
            targets = [str(item.get("value", "")) for item in value.get("SubSet", []) if isinstance(item, dict)]
        else:
            targets = list_field(value, "values", "value")
        for target in targets:
            if target not in created:
                raise UnsupportedFixture(f"body member {target} was not created")
            obj.addObject(created[target])
        return True
    return False


def set_compound_property(created: dict[str, Any], obj: Any, name: str, value: Any) -> bool:
    if not isinstance(value, dict):
        return False
    property_type = value.get("PropertyType")
    if name in {"Objects", "Links"} and property_type == "App::PropertyLinkList":
        targets = list_field(value, "values", "value")
        missing = [str(target) for target in targets if target not in created]
        if missing:
            raise UnsupportedFixture(f"compound links were not created: {', '.join(missing)}")
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureCompound.cpp
        # ::Compound::Compound() declares "ADD_PROPERTY(Links, (nullptr))"; execute() iterates
        # "for (auto obj : Links.getValues())" before makeElementCompound(shapes).
        safe_setattr(obj, "Links", [created[str(target)] for target in targets])
        return True
    return False


def link_sub_value_with_empty_datum_subname(
    created: dict[str, Any],
    value: dict,
    empty_subname_type_ids: set[str],
    *,
    owner: str,
    property_path: str,
) -> Any:
    target_name = value["value"]
    if target_name not in created:
        raise UnsupportedFixture(f"link target {target_name} was not created")
    target = created[target_name]
    sub_list = native_sub_list(value)
    if sub_list:
        record_resolved_reference_bindings(
            value,
            [str(item) for item in sub_list],
            owner=owner,
            property_path=property_path,
            native_resolution_evidence=[
                native_subname_resolution_evidence(target, str(item))
                for item in sub_list
            ],
        )
        return target, sub_list
    if getattr(target, "TypeId", "") in empty_subname_type_ids:
        return target, [""]
    return link_sub_value(created, value, owner=owner, property_path=property_path)


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
            owner_name = str(getattr(obj, "Name", ""))
            resolved_value = link_sub_value_with_empty_datum_subname(
                created,
                value,
                {"PartDesign::Line", "PartDesign::Plane"},
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(obj, name, resolved_value)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
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
            owner_name = str(getattr(obj, "Name", ""))
            resolved_value = link_sub_value_with_empty_datum_subname(
                created,
                value,
                {"PartDesign::Line"},
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(obj, name, resolved_value)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
            return True
    return False


def set_revolved_property(created: dict[str, Any], obj: Any, name: str, value: Any) -> bool:
    if name == "ReferenceAxis" and isinstance(value, dict):
        property_type = value.get("PropertyType")
        if property_type in {"App::PropertyLinkSub", "App::PropertyXLinkSub"}:
            # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp
            # ::ProfileBased::getAxis(), accepts "PartDesign::Line" and "App::Line" before
            # reading the linked datum direction. CAD Core fixtures model these object-level
            # datum axes with an empty SubList, so native collection supplies one empty subname
            # while preserving the request graph shape used by cad-core.
            owner_name = str(getattr(obj, "Name", ""))
            resolved_value = link_sub_value_with_empty_datum_subname(
                created,
                value,
                {"App::Line", "PartDesign::Line"},
                owner=owner_name,
                property_path=name,
            )
            safe_setattr(obj, name, resolved_value)
            mark_resolved_reference_bindings_succeeded(owner_name, name)
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
            if type_id == "Part::Compound" and set_compound_property(created, obj, prop_name, prop_value):
                continue
            if type_id == "PartDesign::LinearPattern" and set_linear_pattern_property(created, obj, prop_name, prop_value):
                continue
            if type_id == "PartDesign::PolarPattern" and set_polar_pattern_property(created, obj, prop_name, prop_value):
                continue
            if type_id in {"PartDesign::Revolution", "PartDesign::Groove"} and set_revolved_property(
                created,
                obj,
                prop_name,
                prop_value,
            ):
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


def shape_has_indexed_element(shape: Any | None, indexed: str) -> bool:
    if shape is None:
        return False
    try:
        if shape.isNull():
            return False
    except Exception:
        pass
    try:
        shape.getElement(indexed)
        return True
    except Exception:
        return False


def shape_mapped_subname(shape: Any | None, indexed: str) -> str:
    if shape is None:
        return ""
    try:
        if shape.isNull():
            return ""
    except Exception:
        pass
    try:
        mapped = shape.getElementMappedName(indexed)
    except Exception:
        return ""
    if not isinstance(mapped, str) or not mapped:
        return ""
    return mapped


def plain_topological_subname(value: str) -> bool:
    for prefix in ("Face", "Edge", "Vertex"):
        if value.startswith(prefix) and value[len(prefix):].isdigit():
            return True
    return False


def stable_mapped_subname(shape: Any | None, indexed: str) -> str:
    mapped = shape_mapped_subname(shape, indexed)
    if mapped == indexed and plain_topological_subname(mapped):
        return ""
    if display_path_stable_token(mapped):
        return ""
    return mapped


def canonical_freecad_mapped_name(mapped_name: str) -> str:
    def replace_hash(match: re.Match[str]) -> str:
        return ":H*:*" if match.group(0).count(":") > 1 else ":H*"

    normalized = FREECAD_MAPPED_NAME_HASH_RE.sub(replace_hash, mapped_name)
    normalized = FREECAD_MAPPED_NAME_COLON_HASH_RE.sub(":H*", normalized)
    return FREECAD_MAPPED_NAME_DELETE_RE.sub(";D*", normalized)


def roundtrip_stable_mapped_subname(shape: Any | None, indexed: str, raw_mapped_name: str) -> tuple[str, str]:
    if shape is None or not raw_mapped_name or display_path_stable_token(raw_mapped_name):
        return "", ""
    try:
        resolved = shape.getElementName(raw_mapped_name)
    except Exception:
        return "", ""
    if not isinstance(resolved, str):
        resolved = str(resolved)
    if resolved == indexed:
        return raw_mapped_name, resolved
    return "", resolved


def object_tip(obj: Any) -> Any | None:
    if getattr(obj, "TypeId", "") != "PartDesign::Body":
        return None
    try:
        return obj.Tip
    except Exception:
        return None


def subshape_response_entries(obj: Any, shape: Any) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    owner = str(obj.Name)
    tip = object_tip(obj)
    tip_name = str(getattr(tip, "Name", "")) if tip is not None else ""
    tip_shape = getattr(tip, "Shape", None) if tip is not None else None

    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/ComplexGeoDataPyImp.cpp
    # ::getElementMappedName() exposes ComplexGeoData::getElementName(...), and
    # /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::execute()
    # publishes the Tip shape as Body.Shape while ::getSubObject() delegates child paths.
    # The collector therefore publishes current FaceN/EdgeN names as subname only. It keeps
    # FreeCAD's raw mapped name as evidence, but publishes canonical stableSubname only when
    # the raw mapped name round-trips through getElementName() back to the current indexed element.
    for kind, attr, prefix in (
        ("Face", "Faces", "Face"),
        ("Edge", "Edges", "Edge"),
        ("Vertex", "Vertexes", "Vertex"),
    ):
        for index, _ in enumerate(getattr(shape, attr, []), start=1):
            indexed = f"{prefix}{index}"
            subname = indexed
            raw_mapped_name = stable_mapped_subname(shape, indexed)
            canonical_mapped_name = canonical_freecad_mapped_name(raw_mapped_name) if raw_mapped_name else ""
            stable_subname, resolved_indexed = roundtrip_stable_mapped_subname(
                shape,
                indexed,
                raw_mapped_name,
            )
            if stable_subname:
                stable_subname = canonical_mapped_name
            full_subname = f"{owner}.{indexed}"
            if stable_subname:
                identity_status = "stable"
            elif raw_mapped_name:
                identity_status = "history_only"
            else:
                identity_status = "current_only"

            if tip_name:
                full_subname = f"{owner}.{tip_name}.{indexed}"
                tip_local_subname = f"{tip_name}.{indexed}"
                if shape_has_indexed_element(tip_shape, indexed):
                    tip_raw_mapped_name = stable_mapped_subname(tip_shape, indexed)
                    tip_canonical_mapped_name = (
                        canonical_freecad_mapped_name(tip_raw_mapped_name) if tip_raw_mapped_name else ""
                    )
                    tip_stable_subname, tip_resolved_indexed = roundtrip_stable_mapped_subname(
                        tip_shape,
                        indexed,
                        tip_raw_mapped_name,
                    )
                    raw_mapped_name = f"{tip_name}.{tip_raw_mapped_name}" if tip_raw_mapped_name else ""
                    canonical_mapped_name = (
                        f"{tip_name}.{tip_canonical_mapped_name}" if tip_canonical_mapped_name else ""
                    )
                    stable_subname = (
                        f"{tip_name}.{tip_canonical_mapped_name}"
                        if tip_stable_subname and tip_canonical_mapped_name
                        else ""
                    )
                    resolved_indexed = tip_resolved_indexed
                    if stable_subname:
                        identity_status = "stable"
                    elif tip_raw_mapped_name:
                        identity_status = "history_only"
                    else:
                        identity_status = "body_display_only"

            entry = {
                "id": f"{owner}:{indexed}",
                "kind": kind,
                "indexed": indexed,
                "subname": resolved_indexed or indexed,
                "stableSubname": stable_subname,
                "identityStatus": identity_status,
                "fullSubname": full_subname,
                "ShadowSub": [],
                "ReferenceShadow": [],
            }
            if raw_mapped_name:
                entry["rawFreecadMappedName"] = raw_mapped_name
                entry["canonicalFreecadMappedName"] = canonical_mapped_name
            if resolved_indexed:
                entry["resolvedIndexed"] = resolved_indexed
            entries.append(entry)
    return entries


def fixture_object_specs(fixture: dict) -> dict[str, dict]:
    return {
        str(spec.get("Name")): spec
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and isinstance(spec.get("Name"), str)
    }


def occt_version_from_runtime(default: str) -> str:
    try:
        import Part  # type: ignore
    except Exception:
        return default
    for attr in ("OCC_VERSION", "OCC_VERSION_STRING", "__OCC_VERSION__"):
        value = getattr(Part, attr, None)
        if value:
            return str(value)
    return default


def topo_state_producer(fixture: dict, FreeCAD: Any) -> dict[str, str]:
    input_state = fixture.get("topoNamingState")
    input_producer = input_state.get("producer") if isinstance(input_state, dict) else {}
    if not isinstance(input_producer, dict):
        input_producer = {}
    input_occt_version = str(input_producer.get("occtVersion") or "fixture-occt-unspecified")
    return {
        "cadCoreVersion": str(input_producer.get("cadCoreVersion") or "fixture-contract-v1"),
        "freecadVersion": freecad_version(FreeCAD),
        "occtVersion": occt_version_from_runtime(input_occt_version),
    }


def topo_state_request_rejection(diagnostic: dict[str, Any]) -> dict[str, Any]:
    """Mirror CAD Core's request-level diagnostics-only topo-state envelope."""
    return {
        "diagnostics": [diagnostic],
        "elementReferenceUpdates": [],
        "results": [],
    }


def topo_state_request_error_response(fixture: dict) -> dict[str, Any] | None:
    topo_state = fixture.get("topoNamingState")
    if topo_state is None:
        return None
    if not isinstance(topo_state, dict):
        return topo_state_request_rejection({
            "code": "topo_state_schema_incompatible",
            "severity": "error",
            "source": "topoNamingState",
            "message": (
                "topoNamingState must be an object or null; request-level recompute is refused"
            ),
            "actualTopoNamingState": topo_state,
            "expectedTopoNamingStateType": "object_or_null",
        })
    if not topo_state:
        return None

    schema_version = topo_state.get("schemaVersion")
    if schema_version != TOPO_STATE_SCHEMA_VERSION:
        return topo_state_request_rejection({
            "code": "topo_state_schema_incompatible",
            "severity": "error",
            "source": "topoNamingState",
            "message": (
                "topoNamingState schemaVersion is incompatible; request-level "
                "recompute is refused"
            ),
            "actualSchemaVersion": schema_version,
            "expectedSchemaVersion": TOPO_STATE_SCHEMA_VERSION,
        })

    producer = topo_state.get("producer")
    cad_core_version = producer.get("cadCoreVersion") if isinstance(producer, dict) else None
    if (
        not isinstance(producer, dict)
        or cad_core_version not in TOPO_STATE_COMPATIBLE_CAD_CORE_VERSIONS
    ):
        return topo_state_request_rejection({
            "code": "topo_state_producer_incompatible",
            "severity": "error",
            "source": "topoNamingState",
            "message": (
                "topoNamingState producer is incompatible; request-level recompute "
                "is refused"
            ),
            "actualProducer": producer,
            "expectedProducer": {
                # Preserve the public diagnostics envelope emitted by CAD Core runtime.  The
                # finite compatibility set above controls validation; this field identifies the
                # fixture producer expected by the public collector contract.
                "cadCoreVersion": TOPO_STATE_PRODUCER_CAD_CORE_VERSION,
            },
        })

    actual_document_hash = str(topo_state.get("documentHash") or "")
    expected_document_hash = fixture_document_hash(fixture)
    if actual_document_hash != expected_document_hash:
        return topo_state_request_rejection({
            "code": "topo_state_document_hash_mismatch",
            "severity": "error",
            "source": "topoNamingState",
            "message": (
                "topoNamingState documentHash does not match the current DocumentObject graph; "
                "request-level recompute is refused"
            ),
            "actualDocumentHash": actual_document_hash,
            "expectedDocumentHash": expected_document_hash,
        })

    objects = topo_state.get("objects")
    if not isinstance(objects, dict):
        return topo_state_request_rejection({
            "code": "topo_state_schema_incompatible",
            "severity": "error",
            "source": "topoNamingState",
            "message": (
                "topoNamingState.objects must be an object; request-level recompute is refused"
            ),
            "actualObjects": objects,
            "expectedObjectsType": "object",
        })
    specs = fixture_object_specs(fixture)
    for object_name in sorted(str(name) for name in objects):
        object_state = objects[object_name]
        if object_name not in specs:
            return topo_state_request_rejection({
                "code": "topo_state_object_owner_incompatible",
                "severity": "error",
                "source": "topoNamingState",
                "object": object_name,
                "message": (
                    "topoNamingState contains an object that is not present in the current "
                    "DocumentObject graph; request-level recompute is refused"
                ),
                "offendingObject": object_name,
            })
        if not isinstance(object_state, dict):
            return topo_state_request_rejection({
                "code": "topo_state_object_owner_incompatible",
                "severity": "error",
                "source": "topoNamingState",
                "object": object_name,
                "message": (
                    "topoNamingState object state must be an object owned by the current "
                    "DocumentObject graph; request-level recompute is refused"
                ),
                "offendingObject": object_name,
            })
        actual_object_hash = str(object_state.get("objectHash") or "")
        expected_object_hash = semantic_hash(specs[object_name])
        if actual_object_hash != expected_object_hash:
            return topo_state_request_rejection({
                "code": "topo_state_object_hash_mismatch",
                "severity": "error",
                "source": "topoNamingState",
                "object": object_name,
                "message": (
                    "topoNamingState objectHash does not match the current object input; "
                    "request-level recompute is refused"
                ),
                "actualObjectHash": actual_object_hash,
                "expectedObjectHash": expected_object_hash,
            })

    return None


LINK_SUB_PROPERTY_TYPES = {
    "App::PropertyLinkSub",
    "App::PropertyLinkSubHidden",
    "App::PropertyXLinkSub",
    "App::PropertyXLinkSubHidden",
}

LINK_SUB_LIST_PROPERTY_TYPES = {
    "App::PropertyLinkSubList",
    "App::PropertyLinkSubListHidden",
    "App::PropertyXLinkSubList",
}


def fixture_document_hash(fixture: dict) -> str:
    return semantic_hash({
        "Objects": fixture.get("Objects", []),
        "recompute": fixture.get("recompute", {}),
    })


def indexed_topo_subname(value: Any) -> bool:
    if not isinstance(value, str) or not value:
        return False
    return TOPO_INDEX_NAME_RE.match(value) is not None or TOPO_CHILD_INDEX_PATH_RE.match(value) is not None


def topo_state_objects_or_raise(fixture: dict) -> dict[str, Any]:
    topo_state = fixture.get("topoNamingState")
    if not isinstance(topo_state, dict):
        raise UnsupportedFixture("topoNamingState protocol fixture requires topoNamingState")
    objects = topo_state.get("objects")
    if not isinstance(objects, dict):
        raise UnsupportedFixture("topoNamingState.objects must be an object")
    return objects


def same_stable_token(left: Any, right: Any) -> bool:
    if not isinstance(left, str) or not isinstance(right, str) or not left or not right:
        return False
    return left == right or canonical_freecad_mapped_name(left) == canonical_freecad_mapped_name(right)


def matching_topo_state_entry(entries: Any, stable_subname: str) -> dict[str, Any] | None:
    if not isinstance(entries, dict):
        return None
    exact = entries.get(stable_subname)
    if isinstance(exact, dict):
        return exact
    for token, entry in entries.items():
        if not isinstance(entry, dict) or not same_stable_token(token, stable_subname):
            continue
        return entry
    for entry in entries.values():
        if not isinstance(entry, dict):
            continue
        mapped_name = entry.get("mappedName")
        if not isinstance(mapped_name, dict):
            continue
        if same_stable_token(mapped_name.get("raw"), stable_subname):
            return entry
        if same_stable_token(mapped_name.get("canonical"), stable_subname):
            return entry
    return None


def terminal_mapper_history_diagnostic(target_name: str, stable_subname: str, event: dict[str, Any] | None) -> dict[str, Any]:
    relation = str(event.get("relation") or "unsupported") if isinstance(event, dict) else "unsupported"
    code_by_relation = {
        "split": "split_stable_subname",
        "deleted": "deleted_stable_subname",
        "ambiguous": "ambiguous_stable_subname",
        "needs_reselect": "split_stable_subname",
    }
    code = code_by_relation.get(relation, "unsupported_stable_subname")
    diagnostic: dict[str, Any] = {
        "code": code,
        "severity": "warning",
        "source": "topoNamingState.mapperHistory",
        "object": target_name,
        "stableSubname": stable_subname,
        "message": f"Stable subname {stable_subname} could not be resolved for {target_name}: {relation}",
    }
    if isinstance(event, dict) and isinstance(event.get("id"), str):
        diagnostic["mapperHistoryId"] = event["id"]
    return diagnostic


def mapper_history_event_matches(event: dict[str, Any], stable_subname: str) -> bool:
    source = event.get("source")
    if isinstance(source, dict) and same_stable_token(source.get("subname"), stable_subname):
        return True
    mapped_name = event.get("mappedName")
    if isinstance(mapped_name, dict):
        if same_stable_token(mapped_name.get("raw"), stable_subname):
            return True
        if same_stable_token(mapped_name.get("canonical"), stable_subname):
            return True
    return False


def topo_state_entry_for_stable_subname(
    fixture: dict,
    target_name: str,
    stable_subname: str,
) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    reject_display_path_stable_token(stable_subname, "topoNamingState StableSubList")
    objects = topo_state_objects_or_raise(fixture)
    object_state = objects.get(target_name)
    if not isinstance(object_state, dict):
        raise UnsupportedFixture(f"topoNamingState missing object state for {target_name}")
    element_map = object_state.get("elementMap")
    entries = element_map.get("entries") if isinstance(element_map, dict) else None
    if not isinstance(entries, dict):
        raise UnsupportedFixture(f"topoNamingState object {target_name} elementMap.entries must be an object")
    entry = matching_topo_state_entry(entries, stable_subname)
    if isinstance(entry, dict):
        return entry, None

    child_maps = object_state.get("childElementMaps")
    if isinstance(child_maps, list):
        for child_map in child_maps:
            if not isinstance(child_map, dict):
                continue
            child_entries = child_map.get("elementMap", {}).get("entries")
            entry = matching_topo_state_entry(child_entries, stable_subname)
            if isinstance(entry, dict):
                return entry, None

    mapper_history = object_state.get("mapperHistory")
    if isinstance(mapper_history, list):
        for event in mapper_history:
            if not isinstance(event, dict) or not mapper_history_event_matches(event, stable_subname):
                continue
            relation = event.get("relation")
            recoverability = event.get("recoverability")
            target = event.get("target")
            target_subname = target.get("subname") if isinstance(target, dict) else None
            if relation in {"generated", "modified", "merge", "historical"} and indexed_topo_subname(target_subname):
                return {
                    "target": {
                        "object": target_name,
                        "subname": str(target_subname),
                    },
                    "evidence": {
                        "source": "mapper_history",
                        "mapperHistoryIds": [event.get("id")] if isinstance(event.get("id"), str) else [],
                        "childElementMapKey": None,
                    },
                }, None
            terminal = str(relation or recoverability or "unsupported")
            terminal_event = dict(event)
            terminal_event["relation"] = terminal
            return None, terminal_mapper_history_diagnostic(target_name, stable_subname, terminal_event)

    return None, terminal_mapper_history_diagnostic(target_name, stable_subname, None)


def topo_state_resolved_indexed_subname(
    fixture: dict,
    target_name: str,
    stable_subname: str,
) -> tuple[str | None, dict[str, Any] | None]:
    entry, diagnostic = topo_state_entry_for_stable_subname(fixture, target_name, stable_subname)
    if diagnostic is not None:
        return None, diagnostic
    if entry is None:
        return None, terminal_mapper_history_diagnostic(target_name, stable_subname, None)
    target = entry.get("target")
    if not isinstance(target, dict):
        raise UnsupportedFixture(f"topoNamingState StableSubList {stable_subname} missing target")
    target_object = target.get("object")
    if target_object != target_name:
        raise UnsupportedFixture(
            f"topoNamingState StableSubList {stable_subname} resolves to {target_object}, expected {target_name}"
        )
    target_subname = target.get("subname")
    if not indexed_topo_subname(target_subname):
        raise UnsupportedFixture(
            f"topoNamingState StableSubList {stable_subname} target.subname is not indexed: {target_subname}"
        )
    return str(target_subname), None


def updated_reference_shadow(shadow: dict[str, Any], target_name: str, stable_subname: str, indexed: str) -> dict[str, Any]:
    reject_display_path_stable_token(stable_subname, "ReferenceShadow.stableSubname")
    stable_token = canonical_freecad_mapped_name(stable_subname)
    result = copy.deepcopy(shadow)
    result["target"] = target_name
    result["indexed"] = indexed
    result["subname"] = indexed
    result["stableSubname"] = stable_token
    return result


def topo_state_link_item_reference_update(
    fixture: dict,
    item: dict[str, Any],
) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    if item.get("StableSubListSource") != "topoNamingState":
        return None, []
    reference_shadows = item.get("ReferenceShadow")
    if not isinstance(reference_shadows, list) or not reference_shadows:
        return None, []
    stable_sub_list = item.get("StableSubList")
    if not isinstance(stable_sub_list, list) or not stable_sub_list:
        raise UnsupportedFixture("topoNamingState ReferenceShadow update requires StableSubList")
    if len(reference_shadows) != len(stable_sub_list):
        raise UnsupportedFixture("topoNamingState ReferenceShadow update requires index-aligned shadows")
    target_name = item.get("value")
    if not isinstance(target_name, str) or not target_name:
        raise UnsupportedFixture("topoNamingState ReferenceShadow update requires a value target")

    stable_subnames = [str(value) for value in stable_sub_list]
    for stable_subname in stable_subnames:
        reject_display_path_stable_token(stable_subname, "StableSubList")
    stable_tokens = [canonical_freecad_mapped_name(stable_subname) for stable_subname in stable_subnames]
    subnames: list[str] = []
    diagnostics: list[dict[str, Any]] = []
    for stable_subname in stable_subnames:
        subname, diagnostic = topo_state_resolved_indexed_subname(fixture, target_name, stable_subname)
        if diagnostic is not None:
            diagnostics.append(diagnostic)
            continue
        if subname is not None:
            subnames.append(subname)
    if diagnostics:
        return None, diagnostics
    shadows = [
        updated_reference_shadow(shadow, target_name, stable_subname, indexed)
        for shadow, stable_subname, indexed in zip(reference_shadows, stable_subnames, subnames)
        if isinstance(shadow, dict)
    ]
    if len(shadows) != len(reference_shadows):
        raise UnsupportedFixture("topoNamingState ReferenceShadow entries must be objects")

    update_item: dict[str, Any] = {
        "value": target_name,
        "SubList": subnames,
        "StableSubList": stable_tokens,
        "ShadowSub": [
            {
                "newName": stable_token,
                "oldName": subname,
            }
            for stable_token, subname in zip(stable_tokens, subnames)
        ],
        "ReferenceShadow": shadows,
    }
    for optional_field in ("ExternalFlags", "FullSubList", "labelReferenceRename", "documentReference"):
        if optional_field in item:
            update_item[optional_field] = copy.deepcopy(item[optional_field])
    return update_item, []


def topo_state_reference_shadow_updates_and_diagnostics(fixture: dict) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    updates: list[dict[str, Any]] = []
    diagnostics: list[dict[str, Any]] = []
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict):
            continue
        object_name = spec.get("Name")
        properties = spec.get("Properties")
        if not isinstance(object_name, str) or not isinstance(properties, dict):
            continue
        for property_name, value in properties.items():
            if not isinstance(value, dict):
                continue
            property_type = value.get("PropertyType")
            if property_type in LINK_SUB_PROPERTY_TYPES:
                update_item, item_diagnostics = topo_state_link_item_reference_update(fixture, value)
                diagnostics.extend(item_diagnostics)
                if update_item is None:
                    continue
                updates.append({
                    "object": object_name,
                    "property": property_name,
                    "PropertyType": property_type,
                    **update_item,
                })
                continue
            if property_type in LINK_SUB_LIST_PROPERTY_TYPES:
                sub_set = value.get("SubSet")
                if not isinstance(sub_set, list):
                    raise UnsupportedFixture(f"{object_name}.{property_name}.SubSet must be a list")
                updated_sub_set: list[dict[str, Any]] = []
                changed = False
                for item in sub_set:
                    if not isinstance(item, dict):
                        raise UnsupportedFixture(f"{object_name}.{property_name}.SubSet items must be objects")
                    update_item, item_diagnostics = topo_state_link_item_reference_update(fixture, item)
                    diagnostics.extend(item_diagnostics)
                    if update_item is None:
                        update_item = {
                            "value": item.get("value"),
                            "SubList": native_sub_list(item),
                        }
                    else:
                        changed = True
                    updated_sub_set.append(update_item)
                if changed:
                    updates.append({
                        "object": object_name,
                        "property": property_name,
                        "PropertyType": property_type,
                        "SubSet": updated_sub_set,
                    })
    return updates, diagnostics


def topo_state_reference_shadow_updates(fixture: dict) -> list[dict[str, Any]]:
    updates, _ = topo_state_reference_shadow_updates_and_diagnostics(fixture)
    return updates


def reference_update_requires_state(item: dict[str, Any]) -> bool:
    stable_sub_list = item.get("StableSubList")
    return isinstance(stable_sub_list, list) and bool(stable_sub_list)


def validate_reference_update_shadow_targets(item: dict[str, Any], target_name: str, context: str) -> None:
    reference_shadows = item.get("ReferenceShadow")
    if not isinstance(reference_shadows, list):
        return
    for index, shadow in enumerate(reference_shadows):
        if not isinstance(shadow, dict):
            continue
        shadow_target = shadow.get("target")
        if isinstance(shadow_target, str) and shadow_target and shadow_target != target_name:
            raise UnsupportedFixture(
                f"{context}.ReferenceShadow[{index}].target {shadow_target} conflicts with value {target_name}"
            )


def add_reference_update_state_target(
    targets: list[str],
    seen: set[str],
    item: dict[str, Any],
    context: str,
) -> None:
    if not reference_update_requires_state(item):
        return
    target_name = item.get("value")
    if not isinstance(target_name, str) or not target_name:
        raise UnsupportedFixture(f"{context}.StableSubList requires a value target")
    validate_reference_update_shadow_targets(item, target_name, context)
    if target_name not in seen:
        seen.add(target_name)
        targets.append(target_name)


def element_reference_update_state_targets(reference_updates: list[dict[str, Any]]) -> list[str]:
    targets: list[str] = []
    seen: set[str] = set()
    for update_index, update in enumerate(reference_updates):
        if not isinstance(update, dict):
            continue
        property_type = update.get("PropertyType")
        context = f"elementReferenceUpdates[{update_index}]"
        if property_type in LINK_SUB_PROPERTY_TYPES:
            add_reference_update_state_target(targets, seen, update, context)
            continue
        if property_type in LINK_SUB_LIST_PROPERTY_TYPES:
            sub_set = update.get("SubSet")
            if not isinstance(sub_set, list):
                raise UnsupportedFixture(f"{context}.SubSet must be a list")
            for item_index, item in enumerate(sub_set):
                if not isinstance(item, dict):
                    raise UnsupportedFixture(f"{context}.SubSet[{item_index}] must be an object")
                add_reference_update_state_target(
                    targets,
                    seen,
                    item,
                    f"{context}.SubSet[{item_index}]",
                )
    return targets


def normalized_topo_state_element_entry(entry: Any) -> dict[str, Any]:
    if not isinstance(entry, dict):
        return {}
    normalized = copy.deepcopy(entry)
    evidence = normalized.get("evidence")
    if not isinstance(evidence, dict):
        evidence = {}
    evidence.setdefault("mapperHistoryIds", [])
    evidence.setdefault("childElementMapKey", None)
    normalized["evidence"] = evidence
    return normalized


def element_entry_canonical_key(token: Any, entry: dict[str, Any]) -> str:
    mapped_name = entry.get("mappedName")
    if isinstance(mapped_name, dict):
        canonical = mapped_name.get("canonical")
        if isinstance(canonical, str) and canonical:
            return canonical
        raw = mapped_name.get("raw")
        if isinstance(raw, str) and raw:
            return canonical_freecad_mapped_name(raw)
    return canonical_freecad_mapped_name(str(token))


def element_entry_merge_signature(entry: dict[str, Any]) -> str:
    mapped_name = entry.get("mappedName")
    canonical = mapped_name.get("canonical") if isinstance(mapped_name, dict) else None
    return json.dumps(
        {
            "target": entry.get("target"),
            "shapeKind": entry.get("shapeKind"),
            "source": entry.get("source"),
            "mappedNameCanonical": canonical,
            "recoverability": entry.get("recoverability"),
            "evidence": entry.get("evidence"),
        },
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def canonical_collision_mapper_history_event(
    canonical: str,
    group: list[dict[str, Any]],
    context: str,
) -> dict[str, Any]:
    candidates = [
        {
            "target": copy.deepcopy(entry.get("target")),
            "shapeKind": entry.get("shapeKind"),
            "source": copy.deepcopy(entry.get("source")),
            "mappedName": copy.deepcopy(entry.get("mappedName")),
            "recoverability": entry.get("recoverability"),
        }
        for entry in group
    ]
    seed = {
        "context": context,
        "canonical": canonical,
        "candidates": [
            {
                "target": candidate.get("target"),
                "shapeKind": candidate.get("shapeKind"),
                "source": candidate.get("source"),
                "mappedNameCanonical": canonical,
                "recoverability": candidate.get("recoverability"),
            }
            for candidate in candidates
        ],
    }
    event_hash = hashlib.sha256(
        json.dumps(seed, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()[:16]
    raw = ""
    mapped_name = group[0].get("mappedName") if group else None
    if isinstance(mapped_name, dict) and isinstance(mapped_name.get("raw"), str):
        raw = mapped_name["raw"]
    return {
        "id": f"canonical-collision-{event_hash}",
        "relation": "ambiguous",
        "recoverability": "ambiguous",
        "source": "freecad_expected_collector",
        "mappedName": {
            "raw": raw,
            "canonical": canonical,
        },
        "candidates": candidates,
        "message": (
            f"Canonical elementMap key {canonical} maps to multiple current targets; "
            "the collector keeps it out of elementMap"
        ),
    }


def insert_element_map_entry(
    entries: dict[str, Any],
    token: str,
    entry: dict[str, Any],
    context: str,
) -> None:
    existing = entries.get(token)
    if existing is None:
        entries[token] = entry
        return
    if isinstance(existing, dict) and element_entry_merge_signature(existing) == element_entry_merge_signature(entry):
        return
    raise UnsupportedFixture(f"{context} canonical elementMap key collision for {token}")


def canonicalized_element_map_entries(
    candidates: Sequence[tuple[str, dict[str, Any]]],
    context: str,
    *,
    omit_ambiguous: bool,
    collision_events: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for token, entry in candidates:
        canonical = element_entry_canonical_key(token, entry)
        mapped_name = entry.get("mappedName")
        if isinstance(mapped_name, dict):
            mapped_name["canonical"] = canonical
        grouped.setdefault(canonical, []).append(entry)

    entries: dict[str, Any] = {}
    for canonical, group in grouped.items():
        signatures = {element_entry_merge_signature(entry) for entry in group}
        if len(signatures) > 1:
            if omit_ambiguous:
                if collision_events is not None:
                    collision_events.append(canonical_collision_mapper_history_event(canonical, group, context))
                continue
            raise UnsupportedFixture(f"{context} canonical elementMap key collision for {canonical}")
        insert_element_map_entry(entries, canonical, group[0], context)
    return entries


def normalized_topo_state_element_map(element_map: Any) -> dict[str, Any]:
    if not isinstance(element_map, dict):
        element_map = {}
    entries = element_map.get("entries")
    if not isinstance(entries, dict):
        entries = {}
    canonical_entries = canonicalized_element_map_entries(
        [
            (str(token), normalized_topo_state_element_entry(entry))
            for token, entry in entries.items()
        ],
        "topoNamingState.elementMap.entries",
        omit_ambiguous=False,
    )
    return {
        "encoding": str(element_map.get("encoding") or "cad-core.element-map.v1"),
        "status": str(element_map.get("status") or ("history_partial" if canonical_entries else "indexed_only")),
        "entries": canonical_entries,
    }


def normalized_child_element_map(child_map: Any) -> dict[str, Any]:
    if not isinstance(child_map, dict):
        return {}
    normalized = copy.deepcopy(child_map)
    normalized["elementMap"] = normalized_topo_state_element_map(normalized.get("elementMap"))
    return normalized


def normalized_topo_state_object(object_name: str, object_state: Any, object_spec: dict | None) -> dict[str, Any]:
    if not isinstance(object_state, dict):
        object_state = {}
    subshapes = object_state.get("subshapes")
    if not isinstance(subshapes, dict):
        subshapes = {}
    child_maps = object_state.get("childElementMaps")
    if not isinstance(child_maps, list):
        child_maps = []
    mapper_history = object_state.get("mapperHistory")
    if not isinstance(mapper_history, list):
        mapper_history = []
    return {
        "objectHash": str(object_state.get("objectHash") or semantic_hash(object_spec or {"Name": object_name})),
        "elementMapVersion": str(object_state.get("elementMapVersion") or "cad-core.element-map.v1"),
        "subshapes": {
            str(key): copy.deepcopy(value)
            for key, value in subshapes.items()
            if isinstance(value, dict)
        },
        "elementMap": normalized_topo_state_element_map(object_state.get("elementMap")),
        "childElementMaps": [
            normalized_child_element_map(child_map)
            for child_map in child_maps
            if isinstance(child_map, dict)
        ],
        "mapperHistory": [
            copy.deepcopy(event)
            for event in mapper_history
            if isinstance(event, dict)
        ],
    }


def normalized_input_topo_state(fixture: dict, FreeCAD: Any) -> dict[str, Any]:
    objects = topo_state_objects_or_raise(fixture)
    specs = fixture_object_specs(fixture)
    return {
        "schemaVersion": TOPO_STATE_SCHEMA_VERSION,
        "producer": topo_state_producer(fixture, FreeCAD),
        "documentHash": fixture_document_hash(fixture),
        "objects": {
            str(object_name): normalized_topo_state_object(str(object_name), object_state, specs.get(str(object_name)))
            for object_name, object_state in objects.items()
        },
    }


def normalized_input_topo_state_object(fixture: dict, object_name: str) -> dict[str, Any] | None:
    topo_state = fixture.get("topoNamingState")
    objects = topo_state.get("objects") if isinstance(topo_state, dict) else None
    if not isinstance(objects, dict):
        return None
    object_state = objects.get(object_name)
    if not isinstance(object_state, dict):
        return None
    return normalized_topo_state_object(object_name, object_state, fixture_object_specs(fixture).get(object_name))


def generated_topo_state_object(
    fixture: dict,
    object_name: str,
    created: dict[str, Any] | None,
) -> dict[str, Any] | None:
    if created is None:
        return None
    obj = created.get(object_name)
    if obj is None:
        return None
    summary = object_expected_payload(obj, fixture, created)
    return topo_state_object_payload(object_name, summary, fixture_object_specs(fixture).get(object_name), created)


def native_shape_is_present(shape: Any) -> bool:
    if shape is None:
        return False
    try:
        return not bool(shape.isNull())
    except Exception:
        return True


def topo_ledger_object_has_native_shape(obj: Any) -> bool:
    return native_shape_is_present(getattr(obj, "Shape", None)) or native_shape_is_present(
        getattr(obj, "InternalShape", None)
    )


def topo_ledger_object_is_candidate(obj: Any) -> bool:
    type_id = str(getattr(obj, "TypeId", ""))
    return type_id in TOPO_LEDGER_ALWAYS_TYPES or topo_ledger_object_has_native_shape(obj)


def topo_ledger_related_names_from_spec(spec: dict[str, Any]) -> list[str]:
    properties = spec.get("Properties", {})
    if not isinstance(properties, dict):
        properties = {}
    type_id = str(spec.get("TypeId") or "")

    names: list[str] = []
    if type_id == "PartDesign::Body":
        names.extend(link_property_object_names(properties, "Group"))
        tip_name = link_property_object_name(properties, "Tip")
        if tip_name:
            names.append(tip_name)
    elif type_id in {"App::Link", "App::LinkElement"}:
        linked_name = link_property_object_name(properties, "LinkedObject")
        if linked_name:
            names.append(linked_name)
    elif type_id == "Part::Compound":
        names.extend(compound_child_names_from_spec(spec))
    return names


def topo_ledger_target_names(
    fixture: dict,
    created: dict[str, Any],
    result_targets: Sequence[str],
) -> list[str]:
    names: list[str] = []
    seen: set[str] = set()
    specs = fixture_object_specs(fixture)

    def add(name: str) -> None:
        if name in seen:
            return
        obj = created.get(name)
        if obj is None or not topo_ledger_object_is_candidate(obj):
            return
        seen.add(name)
        names.append(name)

    def add_related(name: str) -> None:
        spec = specs.get(name)
        if spec is not None:
            for related_name in topo_ledger_related_names_from_spec(spec):
                add(str(related_name))
        obj = created.get(name)
        if obj is None:
            return
        if str(getattr(obj, "TypeId", "")) == "PartDesign::Body":
            tip = object_tip(obj)
            tip_name = str(getattr(tip, "Name", "")) if tip is not None else ""
            if tip_name:
                add(tip_name)
        if str(getattr(obj, "TypeId", "")) in {"App::Link", "App::LinkElement"}:
            linked_name = link_target_name(getattr(obj, "LinkedObject", None))
            if linked_name:
                add(linked_name)

    for target_name in result_targets:
        add(str(target_name))
        add_related(str(target_name))

    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or not isinstance(spec.get("Name"), str):
            continue
        object_name = str(spec["Name"])
        add(object_name)
        for related_name in topo_ledger_related_names_from_spec(spec):
            add(str(related_name))

    for object_name in list(names):
        add_related(object_name)

    return names


def topo_ledger_payloads(
    fixture: dict,
    created: dict[str, Any],
    result_payloads: dict[str, dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    payloads: dict[str, dict[str, Any]] = {}
    for object_name in topo_ledger_target_names(fixture, created, list(result_payloads)):
        obj = created.get(object_name)
        if obj is None:
            continue
        try:
            payloads[object_name] = object_expected_payload(obj, fixture, created)
        except UnsupportedFixture:
            continue
    for object_name, summary in result_payloads.items():
        payloads.setdefault(object_name, summary)
    return payloads


def topo_state_object_has_stable_token(object_state: dict[str, Any], stable_subname: str) -> bool:
    element_map = object_state.get("elementMap")
    entries = element_map.get("entries") if isinstance(element_map, dict) else None
    if matching_topo_state_entry(entries, stable_subname) is not None:
        return True

    child_maps = object_state.get("childElementMaps")
    if isinstance(child_maps, list):
        for child_map in child_maps:
            if not isinstance(child_map, dict):
                continue
            child_entries = child_map.get("elementMap", {}).get("entries")
            if matching_topo_state_entry(child_entries, stable_subname) is not None:
                return True

    mapper_history = object_state.get("mapperHistory")
    if isinstance(mapper_history, list):
        return any(
            isinstance(event, dict) and mapper_history_event_matches(event, stable_subname)
            for event in mapper_history
        )
    return False


def assert_reference_updates_resolve_in_state(
    reference_updates: list[dict[str, Any]],
    objects: dict[str, Any],
) -> None:
    for update_index, update in enumerate(reference_updates):
        if not isinstance(update, dict):
            continue
        property_type = update.get("PropertyType")
        items: list[tuple[str, dict[str, Any]]] = []
        if property_type in LINK_SUB_PROPERTY_TYPES:
            items.append((f"elementReferenceUpdates[{update_index}]", update))
        elif property_type in LINK_SUB_LIST_PROPERTY_TYPES:
            sub_set = update.get("SubSet")
            if not isinstance(sub_set, list):
                raise UnsupportedFixture(f"elementReferenceUpdates[{update_index}].SubSet must be a list")
            for item_index, item in enumerate(sub_set):
                if not isinstance(item, dict):
                    raise UnsupportedFixture(
                        f"elementReferenceUpdates[{update_index}].SubSet[{item_index}] must be an object"
                    )
                items.append((f"elementReferenceUpdates[{update_index}].SubSet[{item_index}]", item))
        for context, item in items:
            if not reference_update_requires_state(item):
                continue
            target_name = item.get("value")
            if not isinstance(target_name, str) or not target_name:
                raise UnsupportedFixture(f"{context}.StableSubList requires a value target")
            validate_reference_update_shadow_targets(item, target_name, context)
            object_state = objects.get(target_name)
            if not isinstance(object_state, dict):
                raise UnsupportedFixture(
                    f"{context}.StableSubList target {target_name} missing from topoNamingState.objects"
                )
            stable_sub_list = item.get("StableSubList")
            for token in stable_sub_list if isinstance(stable_sub_list, list) else []:
                if not isinstance(token, str) or not token:
                    raise UnsupportedFixture(f"{context}.StableSubList requires string tokens")
                stable_token = token
                reject_display_path_stable_token(stable_token, f"{context}.StableSubList")
                if not topo_state_object_has_stable_token(object_state, stable_token):
                    raise UnsupportedFixture(
                        f"{context}.StableSubList {stable_token} cannot resolve in topoNamingState.objects.{target_name}"
                    )


def expand_topo_state_for_reference_updates(
    fixture: dict,
    topo_state: dict[str, Any],
    reference_updates: list[dict[str, Any]],
    created: dict[str, Any] | None = None,
) -> dict[str, Any]:
    if not reference_updates:
        return topo_state

    expanded = copy.deepcopy(topo_state)
    objects = expanded.get("objects")
    if not isinstance(objects, dict):
        raise UnsupportedFixture("topoNamingState.objects must be an object")

    for target_name in element_reference_update_state_targets(reference_updates):
        if target_name in objects:
            continue
        input_object = normalized_input_topo_state_object(fixture, target_name)
        if input_object is not None:
            objects[target_name] = input_object
            continue
        generated_object = generated_topo_state_object(fixture, target_name, created)
        if generated_object is not None:
            objects[target_name] = generated_object
            continue
        raise UnsupportedFixture(
            f"elementReferenceUpdates target {target_name} missing from input topoNamingState and native objects"
        )

    assert_reference_updates_resolve_in_state(reference_updates, objects)
    return expanded


def merge_topo_state_object_for_reference_tokens(base: dict[str, Any], incoming: dict[str, Any]) -> dict[str, Any]:
    merged = copy.deepcopy(base)
    for key, value in incoming.items():
        if key not in merged:
            merged[key] = copy.deepcopy(value)

    base_subshapes = merged.setdefault("subshapes", {})
    incoming_subshapes = incoming.get("subshapes")
    if isinstance(base_subshapes, dict) and isinstance(incoming_subshapes, dict):
        for subname, entry in incoming_subshapes.items():
            base_subshapes.setdefault(subname, copy.deepcopy(entry))

    base_element_map = merged.setdefault("elementMap", {})
    incoming_element_map = incoming.get("elementMap")
    base_entries = base_element_map.setdefault("entries", {}) if isinstance(base_element_map, dict) else {}
    incoming_entries = incoming_element_map.get("entries") if isinstance(incoming_element_map, dict) else None
    if isinstance(base_entries, dict) and isinstance(incoming_entries, dict):
        for token, entry in incoming_entries.items():
            base_entries.setdefault(token, copy.deepcopy(entry))

    base_child_maps = merged.setdefault("childElementMaps", [])
    incoming_child_maps = incoming.get("childElementMaps")
    if isinstance(base_child_maps, list) and isinstance(incoming_child_maps, list):
        existing_keys = {
            child_map.get("key")
            for child_map in base_child_maps
            if isinstance(child_map, dict) and isinstance(child_map.get("key"), str)
        }
        for child_map in incoming_child_maps:
            if not isinstance(child_map, dict):
                continue
            key = child_map.get("key")
            if isinstance(key, str) and key in existing_keys:
                continue
            base_child_maps.append(copy.deepcopy(child_map))
            if isinstance(key, str):
                existing_keys.add(key)

    base_history = merged.setdefault("mapperHistory", [])
    incoming_history = incoming.get("mapperHistory")
    if isinstance(base_history, list) and isinstance(incoming_history, list):
        existing_ids = {
            event.get("id")
            for event in base_history
            if isinstance(event, dict) and isinstance(event.get("id"), str)
        }
        for event in incoming_history:
            if not isinstance(event, dict):
                continue
            event_id = event.get("id")
            if isinstance(event_id, str) and event_id in existing_ids:
                continue
            base_history.append(copy.deepcopy(event))
            if isinstance(event_id, str):
                existing_ids.add(event_id)

    return merged


def expand_topo_state_for_input_references(fixture: dict, topo_state: dict[str, Any]) -> dict[str, Any]:
    input_refs = collect_fixture_input_references(fixture)
    if not input_refs:
        return topo_state

    expanded = copy.deepcopy(topo_state)
    objects = expanded.get("objects")
    if not isinstance(objects, dict):
        raise UnsupportedFixture("topoNamingState.objects must be an object")

    for ref in input_refs:
        target_name = ref.get("target")
        stable_token = ref.get("stableSubname")
        if not isinstance(target_name, str) or not target_name:
            continue
        if not isinstance(stable_token, str) or not stable_token:
            continue

        current_state = objects.get(target_name)
        if isinstance(current_state, dict) and topo_state_object_has_stable_token(current_state, stable_token):
            continue

        input_state = normalized_input_topo_state_object(fixture, target_name)
        if input_state is None:
            raise UnsupportedFixture(
                f"topoNamingState input reference {target_name}.{stable_token} has no input object state"
            )

        if isinstance(current_state, dict):
            objects[target_name] = merge_topo_state_object_for_reference_tokens(current_state, input_state)
        else:
            objects[target_name] = input_state

        if not topo_state_object_has_stable_token(objects[target_name], stable_token):
            raise UnsupportedFixture(
                f"topoNamingState input reference {target_name}.{stable_token} cannot be preserved in response state"
            )

    return expanded


def topo_state_protocol_response(
    fixture: dict,
    FreeCAD: Any,
    topo_state: dict[str, Any],
    diagnostics: list[dict[str, Any]] | None = None,
    created: dict[str, Any] | None = None,
    result_payloads: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    global LAST_FREECAD_LEDGER_CAPTURE
    reference_updates, reference_diagnostics = topo_state_reference_shadow_updates_and_diagnostics(fixture)
    raw_topo_state = copy.deepcopy(topo_state)
    published_topo_state = expand_topo_state_for_input_references(fixture, copy.deepcopy(raw_topo_state))
    published_topo_state = expand_topo_state_for_reference_updates(
        fixture,
        published_topo_state,
        reference_updates,
        created,
    )
    finalize_resolved_reference_bindings(created, raw_topo_state)
    LAST_FREECAD_LEDGER_CAPTURE = freecad_ledger_capture(
        raw_topo_state,
        published_topo_state,
        ACTIVE_RESOLVED_REFERENCE_BINDINGS,
    )
    return {
        "results": [
            {
                "object": object_name,
                **summary,
            }
            for object_name, summary in (result_payloads or {}).items()
        ],
        "topoNamingState": copy.deepcopy(published_topo_state),
        "elementReferenceUpdates": reference_updates,
        "diagnostics": (diagnostics or []) + reference_diagnostics,
    }


def native_compound_link_subname(
    target_spec: dict | None,
    subname: str,
) -> str:
    """Translate CAD Core's Compound child ordinal to FreeCAD's native child-object path."""
    match = re.match(r"^Child(\d+)\.(.+)$", subname)
    if match is None or not isinstance(target_spec, dict) or target_spec.get("TypeId") != "Part::Compound":
        return subname
    child_names = compound_child_names_from_spec(target_spec)
    child_index = int(match.group(1))
    if child_index >= len(child_names):
        raise UnsupportedFixture(
            f"Compound child path {subname} has no native child at index {child_index}"
        )
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
    # ::LinkBaseExtension::extensionGetSubObject() follows the true linked object. Native
    # Part::Compound resolves the child by object name (ChildBoxA.Face1), whereas cad-core's
    # public child-map DTO exposes the ordinal path (Child0.Face1).
    return f"{child_names[child_index]}.{match.group(2)}"


def prepare_native_compound_link_targets(
    fixture: dict,
    created: dict[str, Any],
) -> dict[str, list[str]]:
    """Make native App::Link summaries use the equivalent FreeCAD child-object subpath."""
    specs = fixture_object_specs(fixture)
    resolved_subnames: dict[str, list[str]] = {}
    for object_name, object_spec in specs.items():
        if object_spec.get("TypeId") != "App::Link":
            continue
        link = created.get(object_name)
        linked_value = getattr(link, "LinkedObject", None) if link is not None else None
        if not isinstance(linked_value, tuple) or len(linked_value) < 2:
            continue
        target = linked_value[0]
        target_name = str(getattr(target, "Name", ""))
        raw_subnames = linked_value[1]
        subnames = [raw_subnames] if isinstance(raw_subnames, str) else list(raw_subnames or [])
        if not subnames:
            continue
        native_subnames = [
            native_compound_link_subname(specs.get(target_name), str(subname))
            for subname in subnames
        ]
        for subname in native_subnames:
            resolved = target.getSubObject(subname)
            resolved_shape = getattr(resolved, "Shape", resolved)
            if resolved_shape is None or resolved_shape.isNull():
                raise UnsupportedFixture(
                    f"native App::Link target {target_name}.{subname} has no resolvable Shape"
                )
        if native_subnames != [str(subname) for subname in subnames]:
            link.LinkedObject = (
                target,
                native_subnames[0] if isinstance(raw_subnames, str) else native_subnames,
            )

        properties = object_spec.get("Properties")
        linked_spec = properties.get("LinkedObject") if isinstance(properties, dict) else None
        stable_subnames = linked_spec.get("StableSubList") if isinstance(linked_spec, dict) else None
        if (
            isinstance(linked_spec, dict)
            and linked_spec.get("StableSubListSource") == "topoNamingState"
            and isinstance(stable_subnames, list)
            and len(stable_subnames) == len(native_subnames)
        ):
            published_subnames: list[str] = []
            for stable_subname in stable_subnames:
                indexed, diagnostic = topo_state_resolved_indexed_subname(
                    fixture,
                    target_name,
                    stable_subname,
                )
                if diagnostic is not None or indexed is None:
                    raise UnsupportedFixture(
                        f"native App::Link {object_name} StableSubList {stable_subname} "
                        "does not resolve through topoNamingState"
                    )
                published_subnames.append(indexed)
            record_resolved_reference_bindings(
                linked_spec,
                native_subnames,
                owner=object_name,
                property_path="LinkedObject",
                resolved_object=target_name,
                published_resolved_subnames=published_subnames,
                native_resolution_evidence=[
                    "FreeCAD.getSubObject" for _ in native_subnames
                ],
                assignment_succeeded=True,
            )
        # Keep the selected native paths even when they already used FreeCAD's child-object
        # spelling. The response adapter needs the current display path independently from a
        # state-backed StableSubList token.
        resolved_subnames[object_name] = native_subnames
    return resolved_subnames


def state_backed_compound_link_stable_subnames(
    fixture: dict,
    object_name: str,
    expected_count: int,
) -> list[str]:
    """Return durable Link identities only when the fixture proves them in topo state."""
    object_spec = fixture_object_specs(fixture).get(object_name)
    properties = object_spec.get("Properties") if isinstance(object_spec, dict) else None
    linked_value = properties.get("LinkedObject") if isinstance(properties, dict) else None
    if not isinstance(linked_value, dict) or linked_value.get("StableSubListSource") != "topoNamingState":
        return []

    target_name = linked_value.get("value")
    stable_subnames = linked_value.get("StableSubList")
    if not isinstance(target_name, str) or not target_name:
        raise UnsupportedFixture(f"native compound Link {object_name} requires a LinkedObject value")
    if not isinstance(stable_subnames, list) or len(stable_subnames) != expected_count:
        raise UnsupportedFixture(
            f"native compound Link {object_name} requires one StableSubList token per selected child"
        )

    result: list[str] = []
    for stable_subname in stable_subnames:
        if not isinstance(stable_subname, str) or not stable_subname:
            raise UnsupportedFixture(
                f"native compound Link {object_name} StableSubList entries must be non-empty strings"
            )
        reject_display_path_stable_token(stable_subname, "topoNamingState StableSubList")
        indexed, diagnostic = topo_state_resolved_indexed_subname(
            fixture,
            target_name,
            stable_subname,
        )
        if diagnostic is not None or indexed is None:
            raise UnsupportedFixture(
                f"native compound Link {object_name} StableSubList {stable_subname} "
                "does not resolve through topoNamingState"
            )
        result.append(stable_subname)
    return result


def topo_state_child_element_maps_response(fixture: dict, FreeCAD: Any) -> dict[str, Any] | None:
    if fixture.get("fixtureCategory") != "topoNamingState.childElementMaps.link_compound_expansion":
        return None
    doc = FreeCAD.newDocument("CadCoreTopoStateChildMaps")
    try:
        created = create_objects(FreeCAD, doc, fixture)
        doc.recompute()
        native_link_subnames = prepare_native_compound_link_targets(fixture, created)
        doc.recompute()
        specs = fixture_object_specs(fixture)
        ledger_payloads = topo_ledger_payloads(fixture, created, {})
        objects = {
            object_name: topo_state_object_payload(object_name, summary, specs.get(object_name), created)
            for object_name, summary in ledger_payloads.items()
            if getattr(created.get(object_name), "TypeId", "") not in {"App::Link", "App::LinkElement"}
        }
        if not any(object_state.get("childElementMaps") for object_state in objects.values()):
            raise UnsupportedFixture("topoNamingState childElementMaps fixture requires non-empty childElementMaps")
        topo_state = {
            "schemaVersion": TOPO_STATE_SCHEMA_VERSION,
            "producer": topo_state_producer(fixture, FreeCAD),
            "documentHash": fixture_document_hash(fixture),
            "objects": objects,
        }
        result_payloads: dict[str, dict[str, Any]] = {}
        for object_name in target_names(fixture):
            obj = created.get(object_name)
            if obj is None:
                raise UnsupportedFixture(
                    f"topoNamingState childElementMaps target object {object_name} was not created"
                )
            if getattr(obj, "TypeId", "") == "App::Link":
                stable_subnames = state_backed_compound_link_stable_subnames(
                    fixture,
                    object_name,
                    len(native_link_subnames.get(object_name, [])),
                )
                summary = app_link_payload(
                    obj,
                    "app_link",
                    native_link_subnames.get(object_name, []),
                    stable_subnames,
                )
                # `object_fields` is collector-local provenance, not part of the public
                # cad-core result contract. Keep the native semantic shape/result evidence only.
                summary.pop("object_fields", None)
                result_payloads[object_name] = summary
            else:
                result_payloads[object_name] = object_expected_payload(obj, fixture, created)
        return topo_state_protocol_response(
            fixture,
            FreeCAD,
            topo_state,
            created=created,
            result_payloads=result_payloads,
        )
    finally:
        close_document_with_producer_trace(FreeCAD, doc)


def topo_state_mapper_history_events(object_name: str, probe_case: str) -> list[dict[str, Any]]:

    def event(
        event_id: str,
        source_subname: str,
        target_subname: str,
        shape_kind: str,
        relation: str,
        recoverability: str,
        diagnostic_status: str = "",
    ) -> dict[str, Any]:
        return {
            "id": f"{object_name}.{event_id}",
            "source": {
                "object": "Source",
                "subname": source_subname,
            },
            "target": {
                "object": object_name,
                "subname": target_subname,
            },
            "shape_kind": shape_kind,
            "relation": relation,
            "maker_stage": "TopoNamingStateProbe",
            "evidence": {
                "probeCase": probe_case,
            },
            "recoverability": recoverability,
            "diagnostic_status": diagnostic_status,
        }

    return [
        event("mh-generated-face1", "Face1", "Face1", "face", "generated", "resolved"),
        event("mh-modified-edge1", "Edge1", "Edge1", "edge", "modified", "resolved"),
        event("mh-split-edge2-a", "Edge2", "Edge2", "edge", "split", "needs_reselect", "split_stable_subname"),
        event("mh-split-edge2-b", "Edge2", "Edge3", "edge", "split", "needs_reselect", "split_stable_subname"),
        event("mh-deleted-face2", "Face2", "", "face", "deleted", "deleted", "deleted_stable_subname"),
        event("mh-merge-face4", "Face4", "Face1", "face", "merge", "resolved"),
        event("mh-ambiguous-face3", "Face3", "", "face", "ambiguous", "ambiguous", "stable_identity_ambiguous"),
    ]


def topo_state_mapper_history_probe_response(fixture: dict, FreeCAD: Any) -> dict[str, Any] | None:
    probe_spec = None
    probe_case = ""
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict) or spec.get("TypeId") != "CadCore::TopoNamingStateProbe":
            continue
        properties = spec.get("Properties")
        if not isinstance(properties, dict):
            continue
        probe_case_value = str(properties.get("ProbeCase") or "")
        if probe_case_value in {
            "mapperHistory generated modified split deleted ambiguous",
            "mapperHistory generated modified split deleted merge ambiguous",
        }:
            probe_spec = spec
            probe_case = probe_case_value
            break
    if probe_spec is None:
        return None

    object_name = str(probe_spec.get("Name") or "HistoryProbe")
    mapper_history = topo_state_mapper_history_events(object_name, probe_case)
    generated_id = f"{object_name}.mh-generated-face1"
    modified_id = f"{object_name}.mh-modified-edge1"
    merge_id = f"{object_name}.mh-merge-face4"
    generated_face_token = "Source.#f:1;MHS,F"
    modified_edge_token = "Source.#e:1;MHS,E"
    merge_face_token = "Source.#f:4;MHS,F"
    split_edge_token = "Source.#e:2;MHS,E"
    deleted_face_token = "Source.#f:2;MHS,F"
    ambiguous_face_token = "Source.#f:3;MHS,F"
    object_state = {
        "objectHash": semantic_hash(probe_spec),
        "elementMapVersion": "cad-core.element-map.v1",
        "subshapes": {
            "Face1": {
                "subname": "Face1",
                "identityStatus": "stable",
            },
            "Edge1": {
                "subname": "Edge1",
                "identityStatus": "stable",
            },
        },
        "elementMap": {
            "encoding": "cad-core.element-map.v1",
            "status": "history_partial",
            "entries": {
                generated_face_token: {
                    "target": {
                        "object": object_name,
                        "subname": "Face1",
                    },
                    "shapeKind": "face",
                    "source": {
                        "object": "Source",
                        "subname": "Face1",
                    },
                    "mappedName": {
                        "raw": generated_face_token,
                        "canonical": generated_face_token,
                    },
                    "recoverability": "resolved",
                    "evidence": {
                        "source": "mapper_history",
                        "mapperHistoryIds": [generated_id],
                        "childElementMapKey": None,
                    },
                },
                modified_edge_token: {
                    "target": {
                        "object": object_name,
                        "subname": "Edge1",
                    },
                    "shapeKind": "edge",
                    "source": {
                        "object": "Source",
                        "subname": "Edge1",
                    },
                    "mappedName": {
                        "raw": modified_edge_token,
                        "canonical": modified_edge_token,
                    },
                    "recoverability": "resolved",
                    "evidence": {
                        "source": "mapper_history",
                        "mapperHistoryIds": [modified_id],
                        "childElementMapKey": None,
                    },
                },
                merge_face_token: {
                    "target": {
                        "object": object_name,
                        "subname": "Face1",
                    },
                    "shapeKind": "face",
                    "source": {
                        "object": "Source",
                        "subname": "Face4",
                    },
                    "mappedName": {
                        "raw": merge_face_token,
                        "canonical": merge_face_token,
                    },
                    "recoverability": "resolved",
                    "evidence": {
                        "source": "mapper_history",
                        "mapperHistoryIds": [merge_id],
                        "childElementMapKey": None,
                    },
                },
            },
        },
        "childElementMaps": [],
        "mapperHistory": mapper_history,
    }
    diagnostics = [
        {
            "code": "unsupported_native_mapper_history",
            "severity": "info",
            "source": "topoNamingState.mapperHistory",
            "object": object_name,
            "message": (
                "FreeCAD Python does not expose this fixture's producer history; "
                "CadCore::TopoNamingStateProbe records the CAD Core mapperHistory DTO contract"
            ),
        },
        {
            "code": "split_stable_subname",
            "severity": "warning",
            "source": "topoNamingState.mapperHistory",
            "object": object_name,
            "stableSubname": split_edge_token,
            "message": f"Stable subname {split_edge_token} was split by mapper history and requires reselect",
        },
        {
            "code": "deleted_stable_subname",
            "severity": "warning",
            "source": "topoNamingState.mapperHistory",
            "object": object_name,
            "stableSubname": deleted_face_token,
            "message": f"Stable subname {deleted_face_token} was deleted by mapper history",
        },
        {
            "code": "stable_identity_ambiguous",
            "severity": "warning",
            "source": "topoNamingState.mapperHistory",
            "object": object_name,
            "stableSubname": ambiguous_face_token,
            "message": (
                f"Stable subname {ambiguous_face_token} is ambiguous in mapper history and requires reselect"
            ),
        },
    ]
    topo_state = {
        "schemaVersion": TOPO_STATE_SCHEMA_VERSION,
        "producer": topo_state_producer(fixture, FreeCAD),
        "documentHash": fixture_document_hash(fixture),
        "objects": {
            object_name: object_state,
        },
    }
    return topo_state_protocol_response(fixture, FreeCAD, topo_state, diagnostics)


def topo_state_protocol_branch_response(fixture: dict, FreeCAD: Any) -> dict[str, Any] | None:
    for collector in (
        topo_state_child_element_maps_response,
        topo_state_mapper_history_probe_response,
    ):
        response = collector(fixture, FreeCAD)
        if response is not None:
            return response
    return None


def topo_state_indexed_subname(subshape: dict[str, Any]) -> str:
    for key in ("resolvedIndexed", "indexed"):
        value = subshape.get(key)
        if isinstance(value, str) and value:
            return value
    subname = subshape.get("subname")
    if isinstance(subname, str) and subname and "." not in subname:
        return subname
    return ""


def topo_state_subshape_entry(subshape: dict[str, Any]) -> dict[str, Any]:
    indexed_subname = topo_state_indexed_subname(subshape)
    entry = {
        "subname": indexed_subname,
        "identityStatus": str(subshape.get("identityStatus", "current_only")),
    }
    raw_mapped_name = subshape.get("rawFreecadMappedName")
    if isinstance(raw_mapped_name, str) and raw_mapped_name:
        reject_display_path_stable_token(raw_mapped_name, "rawFreecadMappedName")
        entry["rawFreecadMappedName"] = raw_mapped_name
        entry["canonicalFreecadMappedName"] = canonical_freecad_mapped_name(raw_mapped_name)
    resolved_indexed = subshape.get("resolvedIndexed")
    if isinstance(resolved_indexed, str) and resolved_indexed:
        entry["resolvedIndexed"] = resolved_indexed
    return entry


def topo_state_element_map_entry(object_name: str, subshape: dict[str, Any]) -> tuple[str, dict[str, Any]] | None:
    stable_token = subshape.get("stableSubname")
    if not isinstance(stable_token, str) or not stable_token:
        return None
    reject_display_path_stable_token(stable_token, "topoNamingState.elementMap.entries key")
    target_subname = topo_state_indexed_subname(subshape)
    if not target_subname:
        return None
    shape_kind = str(subshape.get("kind", "shape")).lower()
    source_subname = str(subshape.get("indexed") or target_subname)
    raw_mapped_name = subshape.get("rawFreecadMappedName")
    if not isinstance(raw_mapped_name, str) or not raw_mapped_name:
        return None
    reject_display_path_stable_token(raw_mapped_name, "rawFreecadMappedName")
    canonical_mapped_name = canonical_freecad_mapped_name(raw_mapped_name)
    return canonical_mapped_name, {
        "target": {
            "object": object_name,
            "subname": target_subname,
        },
        "shapeKind": shape_kind,
        "source": {
            "object": object_name,
            "subname": source_subname,
        },
        "mappedName": {
            "raw": raw_mapped_name,
            "canonical": canonical_mapped_name,
        },
        "recoverability": "resolved",
        "evidence": {
            "source": "freecad_expected_collector",
            "mapperHistoryIds": [],
            "childElementMapKey": None,
        },
    }


def topo_shape_kind_from_indexed(indexed: str) -> str:
    for prefix, kind in (("Face", "face"), ("Edge", "edge"), ("Vertex", "vertex")):
        if indexed.startswith(prefix):
            return kind
    return "shape"


def child_element_entry(
    owner_name: str,
    child_name: str,
    child_key: str,
    local_subname: str,
    target_subname: str,
    raw_mapped_name: str,
    evidence_source: str,
) -> tuple[str, dict[str, Any]] | None:
    if not raw_mapped_name:
        return None
    reject_display_path_stable_token(raw_mapped_name, "childElementMaps.elementMap.entries key")
    canonical_mapped_name = canonical_freecad_mapped_name(raw_mapped_name)
    return canonical_mapped_name, {
        "target": {
            "object": owner_name,
            "subname": target_subname,
        },
        "shapeKind": topo_shape_kind_from_indexed(local_subname),
        "source": {
            "object": child_name,
            "subname": local_subname,
        },
        "mappedName": {
            "raw": raw_mapped_name,
            "canonical": canonical_mapped_name,
        },
        "recoverability": "resolved",
        "evidence": {
            "source": evidence_source,
            "mapperHistoryIds": [],
            "childElementMapKey": child_key,
        },
    }


def shape_indexed_names(shape: Any | None, prefix: str) -> list[str]:
    if shape is None:
        return []
    attr = {
        "Face": "Faces",
        "Edge": "Edges",
        "Vertex": "Vertexes",
    }[prefix]
    return [f"{prefix}{index}" for index, _ in enumerate(getattr(shape, attr, []), start=1)]


def shape_topology_offsets(shape: Any | None) -> dict[str, int]:
    return {
        "Face": len(shape_indexed_names(shape, "Face")),
        "Edge": len(shape_indexed_names(shape, "Edge")),
        "Vertex": len(shape_indexed_names(shape, "Vertex")),
    }


def child_map_for_shape_relation(
    owner_name: str,
    owner_shape: Any,
    child_name: str,
    child_shape: Any,
    child_index: int,
    path_prefix: str,
    offsets: dict[str, int],
    evidence_source: str,
    raw_token_prefix: str = "",
    collision_events: list[dict[str, Any]] | None = None,
) -> dict[str, Any] | None:
    child_key = f"{owner_name}:{child_name}"
    if path_prefix:
        child_key = f"{child_key}:{path_prefix}"
    candidates: list[tuple[str, dict[str, Any]]] = []
    for prefix in ("Face", "Edge", "Vertex"):
        for local_index, local_subname in enumerate(shape_indexed_names(child_shape, prefix), start=1):
            target_subname = f"{prefix}{offsets[prefix] + local_index}"
            if not shape_has_indexed_element(owner_shape, target_subname):
                continue
            raw_mapped_name = stable_mapped_subname(owner_shape, target_subname)
            if raw_token_prefix:
                child_raw = stable_mapped_subname(child_shape, local_subname)
                raw_mapped_name = f"{raw_token_prefix}.{child_raw}" if child_raw else ""
            stable_entry = child_element_entry(
                owner_name,
                child_name,
                child_key,
                local_subname,
                target_subname,
                raw_mapped_name,
                evidence_source,
            )
            if stable_entry is not None:
                candidates.append(stable_entry)
    entries = canonicalized_element_map_entries(
        candidates,
        f"topoNamingState.childElementMaps.{child_key}.elementMap.entries",
        omit_ambiguous=True,
        collision_events=collision_events,
    )
    if not entries:
        return None
    return {
        "key": child_key,
        "ownerObject": owner_name,
        "childObject": child_name,
        "childIndex": child_index,
        "pathPrefix": path_prefix,
        "elementMap": {
            "encoding": "cad-core.element-map.v1",
            "status": "history_partial",
            "entries": entries,
        },
    }


def fixture_link_targets(value: Any) -> list[str]:
    if not isinstance(value, dict):
        return []
    return [str(item) for item in list_field(value, "values", "value") if isinstance(item, str)]


def compound_child_names_from_spec(spec: dict | None) -> list[str]:
    properties = spec.get("Properties", {}) if isinstance(spec, dict) else {}
    for field in ("Objects", "Links"):
        targets = fixture_link_targets(properties.get(field))
        if targets:
            return targets
    return []


def generated_child_element_maps(
    object_name: str,
    object_spec: dict | None,
    created: dict[str, Any] | None,
    collision_events: list[dict[str, Any]] | None = None,
) -> list[dict[str, Any]]:
    if created is None or object_name not in created:
        return []
    owner = created[object_name]
    owner_shape = getattr(owner, "Shape", None)
    if owner_shape is None or owner_shape.isNull():
        return []

    type_id = str(getattr(owner, "TypeId", ""))
    child_maps: list[dict[str, Any]] = []
    offsets = {"Face": 0, "Edge": 0, "Vertex": 0}

    if type_id == "Part::Compound":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureCompound.cpp
        # ::Compound::execute() consumes "Links.getValues()" in order and calls
        # "TopoShape().makeElementCompound(shapes)", so child ranges follow link order.
        for child_index, child_name in enumerate(compound_child_names_from_spec(object_spec)):
            child = created.get(child_name)
            child_shape = getattr(child, "Shape", None) if child is not None else None
            if child_shape is None or child_shape.isNull():
                continue
            child_map = child_map_for_shape_relation(
                object_name,
                owner_shape,
                child_name,
                child_shape,
                child_index,
                f"Child{child_index}",
                offsets,
                "freecad_part_compound_links",
                collision_events=collision_events,
            )
            if child_map is not None:
                child_maps.append(child_map)
            for prefix, count in shape_topology_offsets(child_shape).items():
                offsets[prefix] += count

    if type_id == "PartDesign::Body":
        tip = object_tip(owner)
        tip_name = str(getattr(tip, "Name", "")) if tip is not None else ""
        tip_shape = getattr(tip, "Shape", None) if tip is not None else None
        if tip_name and tip_shape is not None and not tip_shape.isNull():
            # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp
            # ::Body::execute() stores "Shape.setValue(tipShape)" after reading Tip.getValue().
            child_map = child_map_for_shape_relation(
                object_name,
                owner_shape,
                tip_name,
                tip_shape,
                0,
                tip_name,
                offsets,
                "freecad_partdesign_body_tip",
                raw_token_prefix=tip_name,
                collision_events=collision_events,
            )
            if child_map is not None:
                child_maps.append(child_map)

    if type_id == "App::Link":
        linked = getattr(owner, "LinkedObject", None)
        linked_name = link_target_name(linked)
        linked_obj = created.get(linked_name) if linked_name else None
        linked_shape = getattr(linked_obj, "Shape", None) if linked_obj is not None else None
        if linked_name and linked_shape is not None and not linked_shape.isNull():
            # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
            # ::LinkBaseExtension::extensionGetSubObject() delegates through getTrueLinkedObject()
            # and ShowElement/ElementCount child indexes before returning linked subobjects.
            child_map = child_map_for_shape_relation(
                object_name,
                owner_shape,
                linked_name,
                linked_shape,
                0,
                linked_name,
                offsets,
                "freecad_app_link_linked_object",
                collision_events=collision_events,
            )
            if child_map is not None:
                child_maps.append(child_map)

    return child_maps


def merge_child_map_entries_into_element_entries(entries: dict[str, Any], child_maps: list[dict[str, Any]]) -> None:
    for child_map in child_maps:
        child_entries = child_map.get("elementMap", {}).get("entries")
        if not isinstance(child_entries, dict):
            continue
        for token, child_entry in child_entries.items():
            if not isinstance(child_entry, dict):
                continue
            current_entry = entries.get(token)
            if current_entry is None:
                continue
            current_entry["source"] = copy.deepcopy(child_entry.get("source", current_entry.get("source")))
            current_entry["mappedName"] = copy.deepcopy(child_entry.get("mappedName", current_entry.get("mappedName")))
            current_entry["evidence"] = copy.deepcopy(child_entry.get("evidence", current_entry.get("evidence")))


def topo_state_object_payload(
    object_name: str,
    summary: dict[str, Any],
    object_spec: dict | None,
    created: dict[str, Any] | None = None,
) -> dict[str, Any]:
    subshapes = summary.get("subshapes")
    if not isinstance(subshapes, list):
        subshapes = []

    state_subshapes: dict[str, Any] = {}
    entry_candidates: list[tuple[str, dict[str, Any]]] = []
    mapper_history: list[dict[str, Any]] = []
    for subshape in subshapes:
        if not isinstance(subshape, dict):
            continue
        indexed = topo_state_indexed_subname(subshape)
        if indexed:
            state_subshapes[indexed] = topo_state_subshape_entry(subshape)
        element_map_entry = topo_state_element_map_entry(object_name, subshape)
        if element_map_entry is not None:
            entry_candidates.append(element_map_entry)
    entries = canonicalized_element_map_entries(
        entry_candidates,
        f"topoNamingState.objects.{object_name}.elementMap.entries",
        omit_ambiguous=True,
        collision_events=mapper_history,
    )

    child_maps = generated_child_element_maps(object_name, object_spec, created, mapper_history)
    merge_child_map_entries_into_element_entries(entries, child_maps)

    return {
        "objectHash": semantic_hash(object_spec or {"Name": object_name}),
        "elementMapVersion": "cad-core.element-map.v1",
        "subshapes": state_subshapes,
        "elementMap": {
            "encoding": "cad-core.element-map.v1",
            "status": "history_partial" if entries else "indexed_only",
            "entries": entries,
        },
        "childElementMaps": child_maps,
        "mapperHistory": mapper_history,
    }


def topo_naming_state_response(
    fixture: dict,
    FreeCAD: Any,
    result_payloads: dict[str, dict[str, Any]],
    created: dict[str, Any] | None = None,
    ledger_payloads: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    global LAST_FREECAD_LEDGER_CAPTURE
    specs = fixture_object_specs(fixture)
    reference_updates, reference_diagnostics = topo_state_reference_shadow_updates_and_diagnostics(fixture)
    state_payloads = ledger_payloads if ledger_payloads is not None else result_payloads
    raw_topo_state = {
        "schemaVersion": "cad-core.topo-state.v1",
        "producer": topo_state_producer(fixture, FreeCAD),
        "documentHash": semantic_hash({
            "Objects": fixture.get("Objects", []),
            "recompute": fixture.get("recompute", {}),
        }),
        "objects": {
            object_name: topo_state_object_payload(object_name, summary, specs.get(object_name), created)
            for object_name, summary in state_payloads.items()
        },
    }
    published_topo_state = expand_topo_state_for_input_references(fixture, copy.deepcopy(raw_topo_state))
    published_topo_state = expand_topo_state_for_reference_updates(
        fixture,
        published_topo_state,
        reference_updates,
        created,
    )
    finalize_resolved_reference_bindings(created, raw_topo_state)
    LAST_FREECAD_LEDGER_CAPTURE = freecad_ledger_capture(
        raw_topo_state,
        published_topo_state,
        ACTIVE_RESOLVED_REFERENCE_BINDINGS,
    )
    return {
        "results": [
            {
                "object": object_name,
                **summary,
            }
            for object_name, summary in result_payloads.items()
        ],
        "topoNamingState": copy.deepcopy(published_topo_state),
        "elementReferenceUpdates": reference_updates,
        "diagnostics": reference_diagnostics,
    }


def ledger_element_bucket(element: Any) -> str:
    text = str(element)
    for prefix in ("InternalFace", "Face"):
        if text.startswith(prefix):
            return "Face"
    for prefix in ("InternalEdge", "Edge"):
        if text.startswith(prefix):
            return "Edge"
    for prefix in ("InternalVertex", "Vertex"):
        if text.startswith(prefix):
            return "Vertex"
    if ".Face" in text or text.endswith("Face"):
        return "Face"
    if ".Edge" in text or text.endswith("Edge"):
        return "Edge"
    if ".Vertex" in text or text.endswith("Vertex"):
        return "Vertex"
    return "Shape"


def dict_items(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def ledger_add_element(record: dict[str, Any], field: str, element: Any) -> None:
    if not isinstance(element, str) or not element:
        return
    elements = record.setdefault(field, {})
    if not isinstance(elements, dict):
        elements = {}
        record[field] = elements
    bucket = ledger_element_bucket(element)
    bucket_values = elements.setdefault(bucket, [])
    if element not in bucket_values:
        bucket_values.append(element)


def ledger_object_record(
    objects: dict[str, dict[str, Any]],
    object_name: str,
    specs: dict[str, dict],
    published_names: set[str],
) -> dict[str, Any]:
    record = objects.get(object_name)
    if record is None:
        spec = specs.get(object_name, {})
        record = {
            "type": str(spec.get("TypeId") or "unknown"),
            "role": "protocol_published" if object_name in published_names else "internal_or_fixture",
            "published": object_name in published_names,
            "beforeElements": {},
            "afterElements": {},
        }
        objects[object_name] = record
    elif object_name in published_names:
        record["published"] = True
        record["role"] = "protocol_published"
    return record


def ledger_add_state_elements(record: dict[str, Any], object_state: dict[str, Any], field: str) -> None:
    subshapes = object_state.get("subshapes")
    if isinstance(subshapes, dict):
        for subname in subshapes:
            ledger_add_element(record, field, str(subname))

    element_map = object_state.get("elementMap")
    entries = element_map.get("entries") if isinstance(element_map, dict) else None
    if isinstance(entries, dict):
        for entry in entries.values():
            if not isinstance(entry, dict):
                continue
            for endpoint in (entry.get("source"), entry.get("target")):
                if isinstance(endpoint, dict):
                    subname = endpoint.get("subname")
                    if isinstance(subname, str) and subname:
                        ledger_add_element(record, field, subname)


def ledger_related_object_names_from_spec(spec: dict[str, Any]) -> list[str]:
    properties = spec.get("Properties")
    if not isinstance(properties, dict):
        return []
    type_id = str(spec.get("TypeId") or "")
    names: list[str] = []
    if type_id == "PartDesign::Body":
        names.extend(link_property_object_names(properties, "Group"))
        tip = link_property_object_name(properties, "Tip")
        if tip:
            names.append(tip)
    elif type_id == "Part::Compound":
        names.extend(compound_child_names_from_spec(spec))
    elif type_id in {"App::Link", "App::LinkElement"}:
        target = link_property_object_name(properties, "LinkedObject")
        if target:
            names.append(target)
    return names


def ledger_find_owner_for_child(specs: dict[str, dict], child_name: str) -> tuple[str, str] | None:
    for owner_name, spec in specs.items():
        type_id = str(spec.get("TypeId") or "")
        related = ledger_related_object_names_from_spec(spec)
        if child_name not in related:
            continue
        if type_id == "PartDesign::Body":
            return owner_name, "covered_by_body_tip"
        if type_id == "Part::Compound":
            return owner_name, "covered_by_compound_child_map"
        if type_id in {"App::Link", "App::LinkElement"}:
            return owner_name, "covered_by_link_target"
    return None


def ledger_topo_state_entry_for_token(
    objects: dict[str, Any],
    target_name: str,
    stable_token: str,
) -> dict[str, Any] | None:
    object_state = objects.get(target_name)
    if not isinstance(object_state, dict):
        return None

    element_map = object_state.get("elementMap")
    entries = element_map.get("entries") if isinstance(element_map, dict) else None
    entry = matching_topo_state_entry(entries, stable_token)
    if isinstance(entry, dict):
        return entry

    child_maps = object_state.get("childElementMaps")
    if isinstance(child_maps, list):
        for child_map in child_maps:
            if not isinstance(child_map, dict):
                continue
            child_entries = child_map.get("elementMap", {}).get("entries")
            entry = matching_topo_state_entry(child_entries, stable_token)
            if isinstance(entry, dict):
                return entry

    mapper_history = object_state.get("mapperHistory")
    if isinstance(mapper_history, list):
        for event in mapper_history:
            if not isinstance(event, dict) or not mapper_history_event_matches(event, stable_token):
                continue
            return {
                "source": event.get("source"),
                "target": event.get("target"),
                "recoverability": event.get("recoverability"),
                "evidence": {
                    "source": "mapper_history",
                    "mapperHistoryIds": [event.get("id")] if isinstance(event.get("id"), str) else [],
                },
            }
    return None


def resolved_binding_for_reference(
    bindings: Sequence[dict[str, Any]],
    ref: dict[str, Any],
) -> dict[str, Any] | None:
    ref_owner = ref.get("owner")
    ref_property_path = ref.get("propertyPath")
    ref_target = ref.get("target")
    ref_token = ref.get("stableSubname") or ref.get("element")
    for binding in bindings:
        if not isinstance(binding, dict):
            continue
        if (
            binding.get("owner") == ref_owner
            and binding.get("propertyPath") == ref_property_path
            and binding.get("target") == ref_target
            and same_stable_token(binding.get("stableSubname"), ref_token)
        ):
            return binding
    return None


def topo_state_has_object(topo_state: dict[str, Any], object_name: Any) -> bool:
    objects = topo_state.get("objects")
    return (
        isinstance(objects, dict)
        and isinstance(object_name, str)
        and bool(object_name)
        and object_name in objects
    )


def resolved_binding_has_native_target(
    topo_state: dict[str, Any],
    binding: dict[str, Any] | None,
) -> bool:
    if not isinstance(binding, dict):
        return False
    resolved_object = binding.get("resolvedObject")
    resolved_subname = binding.get("resolvedSubname")
    native_resolved_subname = binding.get("nativeResolvedSubname")
    if not (
        binding.get("assignmentStatus") == "succeeded"
        and binding.get("nativeResolutionEvidence") in NATIVE_RESOLUTION_EVIDENCE
        and topo_state_has_object(topo_state, resolved_object)
        and isinstance(resolved_subname, str)
        and bool(resolved_subname)
        and isinstance(native_resolved_subname, str)
        and bool(native_resolved_subname)
    ):
        return False

    objects = topo_state.get("objects")
    object_state = objects.get(resolved_object) if isinstance(objects, dict) else None
    subshapes = object_state.get("subshapes") if isinstance(object_state, dict) else None
    if isinstance(subshapes, dict) and resolved_subname in subshapes:
        return True

    target_name = binding.get("target")
    stable_subname = binding.get("stableSubname")
    entry = ledger_topo_state_entry_for_token(objects, target_name, stable_subname)
    entry_target = dict_items(entry.get("target")) if isinstance(entry, dict) else {}
    if (
        entry_target.get("object") == resolved_object
        and entry_target.get("subname") == resolved_subname
    ):
        return True

    if (
        native_resolved_subname != resolved_subname
        and binding.get("nativeResolutionEvidence")
        in {"FreeCAD.getSubObject", "FreeCAD.Shape.getElement"}
    ):
        return True

    # Some native objects (notably Sketch external-geometry sources) expose a
    # resolvable EdgeN through getSubObject without publishing a subshape inventory.
    return isinstance(subshapes, dict) and not subshapes


def ledger_endpoint(endpoint: Any, fallback_object: str, fallback_element: str) -> dict[str, str]:
    if isinstance(endpoint, dict):
        object_name = endpoint.get("object")
        subname = endpoint.get("subname")
        if isinstance(object_name, str) and object_name and isinstance(subname, str) and subname:
            return {
                "object": object_name,
                "element": subname,
            }
    return {
        "object": fallback_object,
        "element": fallback_element,
    }


def collect_fixture_input_references(fixture: dict[str, Any]) -> list[dict[str, Any]]:
    refs: list[dict[str, Any]] = []
    for spec in fixture.get("Objects", []):
        if not isinstance(spec, dict):
            continue
        owner = spec.get("Name")
        if not isinstance(owner, str) or not owner:
            continue
        properties = spec.get("Properties")
        if not isinstance(properties, dict):
            continue

        def visit(value: Any, property_path: list[str]) -> None:
            if isinstance(value, dict):
                target_name = value.get("value")
                stable_sub_list = value.get("StableSubList")
                if isinstance(target_name, str) and isinstance(stable_sub_list, list):
                    sub_list = value.get("SubList") if isinstance(value.get("SubList"), list) else []
                    shadows = value.get("ReferenceShadow") if isinstance(value.get("ReferenceShadow"), list) else []
                    for index, stable_token in enumerate(stable_sub_list):
                        if not isinstance(stable_token, str) or not stable_token:
                            continue
                        ref_id = f"ref:{len(refs) + 1}"
                        refs.append({
                            "id": ref_id,
                            "owner": owner,
                            "path": [owner, target_name],
                            "propertyPath": ".".join(property_path),
                            "target": target_name,
                            "element": stable_token,
                            "source": value.get("StableSubListSource") or "StableSubList",
                            "required": True,
                            "stableSubname": stable_token,
                            "displaySubname": sub_list[index] if index < len(sub_list) else "",
                            "hasReferenceShadow": index < len(shadows),
                        })
                for key, item in value.items():
                    visit(item, property_path + [str(key)])
            elif isinstance(value, list):
                for index, item in enumerate(value):
                    visit(item, property_path + [str(index)])

        visit(properties, [])
    return refs


def ledger_event_kind_from_recoverability(recoverability: Any) -> str:
    if recoverability in {"deleted", "ambiguous"}:
        return str(recoverability)
    return "resolved"


def ledger_events_for_input_references(
    raw_topo_state: dict[str, Any],
    resolved_reference_bindings: Sequence[dict[str, Any]],
    input_refs: list[dict[str, Any]],
    objects: dict[str, dict[str, Any]],
    specs: dict[str, dict],
    published_names: set[str],
) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    raw_objects = raw_topo_state.get("objects")
    if not isinstance(raw_objects, dict):
        raw_objects = {}

    for ref in input_refs:
        target_name = str(ref.get("target") or ref.get("owner") or "")
        stable_token = str(ref.get("stableSubname") or ref.get("element") or "")
        binding = resolved_binding_for_reference(resolved_reference_bindings, ref)
        entry = ledger_topo_state_entry_for_token(raw_objects, target_name, stable_token)

        event_id = f"event:{len(events) + 1}"
        resolved_object = binding.get("resolvedObject") if isinstance(binding, dict) else None
        resolved_subname = binding.get("resolvedSubname") if isinstance(binding, dict) else None
        if not resolved_binding_has_native_target(raw_topo_state, binding):
            event = {
                "id": event_id,
                "kind": "failed_with_diagnostics",
                "sources": [],
                "targets": [],
                "inputReferenceIds": [ref["id"]],
                "diagnostics": [
                    {
                        "code": "stable_reference_not_found",
                        "message": (
                            f"StableSubList {stable_token} for {ref.get('owner')}.{ref.get('propertyPath')} "
                            "was not resolved by the native property assignment"
                        ),
                    }
                ],
                "provenance": "FreeCADCmd.collect_freecad_expected",
            }
            events.append(event)
            continue

        source = ledger_endpoint(
            entry.get("source") if isinstance(entry, dict) else None,
            str(resolved_object),
            str(resolved_subname),
        )
        target = {
            "object": str(resolved_object),
            "element": str(resolved_subname),
        }
        entry_target = dict_items(entry.get("target")) if isinstance(entry, dict) else {}
        if entry_target and (
            entry_target.get("object") != resolved_object
            or entry_target.get("subname") != resolved_subname
        ):
            events.append({
                "id": event_id,
                "kind": "failed_with_diagnostics",
                "sources": [source],
                "targets": [],
                "inputReferenceIds": [ref["id"]],
                "diagnostics": [{
                    "code": "native_reference_binding_mismatch",
                    "stableSubname": stable_token,
                    "expectedTarget": entry_target,
                    "actualTarget": target,
                }],
                "provenance": "FreeCADCmd.native_property_assignment",
            })
            continue
        source_record = ledger_object_record(objects, source["object"], specs, published_names)
        target_record = ledger_object_record(objects, target["object"], specs, published_names)
        ledger_add_element(source_record, "beforeElements", source["element"])
        ledger_add_element(target_record, "afterElements", target["element"])

        kind = ledger_event_kind_from_recoverability(
            entry.get("recoverability") if isinstance(entry, dict) else "resolved"
        )
        event = {
            "id": event_id,
            "kind": kind,
            "sources": [source],
            "targets": [] if kind == "deleted" else [target],
            "inputReferenceIds": [ref["id"]],
            "provenance": (
                (entry.get("evidence") or {}).get("source", "FreeCADCmd.native_property_assignment")
                if isinstance(entry, dict)
                else "FreeCADCmd.native_property_assignment"
            ),
        }
        if kind in {"ambiguous", "failed_with_diagnostics"}:
            event["diagnostics"] = [
                {
                    "code": f"{kind}_stable_reference",
                    "stableSubname": stable_token,
                }
            ]
        events.append(event)
    return events


def ledger_events_from_mapper_history(
    topo_state: dict[str, Any],
    objects: dict[str, dict[str, Any]],
    specs: dict[str, dict],
    published_names: set[str],
    start_index: int,
) -> list[dict[str, Any]]:
    state_objects = topo_state.get("objects")
    if not isinstance(state_objects, dict):
        return []

    events: list[dict[str, Any]] = []
    split_groups: dict[tuple[str, str, str], list[dict[str, str]]] = {}
    merge_groups: dict[tuple[str, str, str], list[dict[str, str]]] = {}

    def add_endpoint_inventory(endpoint: dict[str, str], field: str) -> None:
        record = ledger_object_record(objects, endpoint["object"], specs, published_names)
        ledger_add_element(record, field, endpoint["element"])

    for object_name, object_state in state_objects.items():
        if not isinstance(object_state, dict):
            continue
        for raw_event in object_state.get("mapperHistory") or []:
            if not isinstance(raw_event, dict):
                continue
            relation = raw_event.get("relation")
            source_raw = raw_event.get("source")
            target_raw = raw_event.get("target")
            source = ledger_endpoint(source_raw, str(object_name), "")
            target = ledger_endpoint(target_raw, str(object_name), "")

            if relation == "split" and source["element"] and target["element"]:
                split_groups.setdefault((source["object"], source["element"], target["object"]), []).append(target)
                add_endpoint_inventory(source, "beforeElements")
                add_endpoint_inventory(target, "afterElements")
                continue
            if relation == "merge" and source["element"] and target["element"]:
                merge_groups.setdefault((target["object"], target["element"], source["object"]), []).append(source)
                add_endpoint_inventory(source, "beforeElements")
                add_endpoint_inventory(target, "afterElements")
                continue

            kind_by_relation = {
                "generated": "generated",
                "modified": "modified",
                "deleted": "deleted",
                "ambiguous": "ambiguous",
            }
            kind = kind_by_relation.get(str(relation))
            if not kind:
                continue

            event: dict[str, Any] = {
                "id": raw_event.get("id") or f"event:{start_index + len(events)}",
                "kind": kind,
                "inputReferenceIds": [],
                "provenance": "FreeCAD.TopologicalNaming.MapperHistory",
            }
            if kind == "generated":
                event["sources"] = []
                event["targets"] = [target] if target["element"] else []
                if target["element"]:
                    add_endpoint_inventory(target, "afterElements")
            elif kind == "deleted":
                event["sources"] = [source] if source["element"] else []
                event["targets"] = []
                if source["element"]:
                    add_endpoint_inventory(source, "beforeElements")
            elif kind == "ambiguous":
                event["sources"] = [source] if source["element"] else []
                event["targets"] = []
                event["diagnostics"] = [
                    {
                        "code": raw_event.get("diagnostic_status") or "stable_identity_ambiguous",
                    }
                ]
                if source["element"]:
                    add_endpoint_inventory(source, "beforeElements")
            else:
                event["sources"] = [source] if source["element"] else []
                event["targets"] = [target] if target["element"] else []
                if source["element"]:
                    add_endpoint_inventory(source, "beforeElements")
                if target["element"]:
                    add_endpoint_inventory(target, "afterElements")
            events.append(event)

    for (source_object, source_element, target_object), targets in split_groups.items():
        if len(targets) < 2:
            continue
        events.append({
            "id": f"event:{start_index + len(events)}",
            "kind": "split",
            "sources": [{"object": source_object, "element": source_element}],
            "targets": targets,
            "inputReferenceIds": [],
            "provenance": "FreeCAD.TopologicalNaming.MapperHistory",
        })

    for (target_object, target_element, source_object), sources in merge_groups.items():
        if len(sources) < 2:
            continue
        events.append({
            "id": f"event:{start_index + len(events)}",
            "kind": "merged",
            "sources": sources,
            "targets": [{"object": target_object, "element": target_element}],
            "inputReferenceIds": [],
            "provenance": "FreeCAD.TopologicalNaming.MapperHistory",
        })

    return events


def ledger_element_maps_evidence_stable_subshape(
    object_name: str,
    subshape_name: str,
    subshape: dict[str, Any],
    object_state: dict[str, Any],
) -> bool:
    element_maps = [dict_items(object_state.get("elementMap"))]
    for child_map in object_state.get("childElementMaps") or []:
        if isinstance(child_map, dict):
            element_maps.append(dict_items(child_map.get("elementMap")))

    for element_map in element_maps:
        for entry in dict_items(element_map.get("entries")).values():
            if not isinstance(entry, dict):
                continue
            target = dict_items(entry.get("target"))
            mapped_name = dict_items(entry.get("mappedName"))
            if (
                target.get("object") == object_name
                and target.get("subname") == subshape_name
                and mapped_name.get("raw") == subshape.get("rawFreecadMappedName")
                and mapped_name.get("canonical") == subshape.get("canonicalFreecadMappedName")
            ):
                return True
    return False


def ledger_unmapped_stable_subshape_evidence(
    object_name: str,
    object_state: dict[str, Any],
) -> dict[str, Any]:
    evidence: dict[str, Any] = {}
    subshapes = object_state.get("subshapes")
    if not isinstance(subshapes, dict):
        return evidence
    for subshape_name, subshape in subshapes.items():
        if not isinstance(subshape_name, str) or not isinstance(subshape, dict):
            continue
        if subshape.get("identityStatus") != "stable":
            continue
        if ledger_element_maps_evidence_stable_subshape(
            object_name,
            subshape_name,
            subshape,
            object_state,
        ):
            continue
        evidence[subshape_name] = copy.deepcopy(subshape)
    return evidence


def ledger_objects_from_states(
    fixture: dict[str, Any],
    raw_topo_state: dict[str, Any],
    published_topo_state: dict[str, Any],
    input_refs: list[dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    specs = fixture_object_specs(fixture)
    raw_objects = raw_topo_state.get("objects")
    if not isinstance(raw_objects, dict):
        raw_objects = {}
    published_objects = published_topo_state.get("objects")
    if not isinstance(published_objects, dict):
        published_objects = {}
    input_state = fixture.get("topoNamingState")
    input_objects = input_state.get("objects") if isinstance(input_state, dict) else {}
    published_names = set(published_objects)
    objects: dict[str, dict[str, Any]] = {}

    for object_name in specs:
        ledger_object_record(objects, object_name, specs, published_names)
    if isinstance(input_objects, dict):
        for object_name, object_state in input_objects.items():
            record = ledger_object_record(objects, str(object_name), specs, published_names)
            if isinstance(object_state, dict):
                ledger_add_state_elements(record, object_state, "beforeElements")
    for object_name, object_state in raw_objects.items():
        record = ledger_object_record(objects, str(object_name), specs, published_names)
        if isinstance(object_state, dict):
            ledger_add_state_elements(record, object_state, "afterElements")

    for object_name, object_state in published_objects.items():
        record = ledger_object_record(objects, str(object_name), specs, published_names)
        if isinstance(object_state, dict):
            # Publication can preserve a fixture-carried object that is not one of the
            # collector's requested result targets. Keep its inventory explicit, while
            # native event conclusions remain tied to rawTopoNamingState and bindings.
            ledger_add_state_elements(record, object_state, "afterElements")
            record["elementMap"] = copy.deepcopy(object_state.get("elementMap", {}))
            record["childElementMaps"] = copy.deepcopy(object_state.get("childElementMaps", []))
            record["mapperHistory"] = copy.deepcopy(object_state.get("mapperHistory", []))
            raw_object_state = raw_objects.get(object_name)
            subshape_evidence = (
                ledger_unmapped_stable_subshape_evidence(
                    str(object_name),
                    raw_object_state,
                )
                if isinstance(raw_object_state, dict)
                else {}
            )
            if subshape_evidence:
                record["subshapeEvidence"] = subshape_evidence

    for ref in input_refs:
        owner = ref.get("owner")
        target = ref.get("target")
        if isinstance(owner, str):
            ledger_object_record(objects, owner, specs, published_names)
        if isinstance(target, str):
            ledger_object_record(objects, target, specs, published_names)
    return objects


def ledger_projection(
    fixture: dict[str, Any],
    native_topo_state: dict[str, Any],
    objects: dict[str, dict[str, Any]],
    events: list[dict[str, Any]],
) -> dict[str, Any]:
    specs = fixture_object_specs(fixture)
    topo_objects = native_topo_state.get("objects")
    if not isinstance(topo_objects, dict):
        topo_objects = {}
    published_names = set(topo_objects) if isinstance(topo_objects, dict) else set()
    first_published = sorted(published_names)[0] if published_names else ""

    event_ids_by_object: dict[str, list[str]] = {}
    for event in events:
        event_id = str(event.get("id") or "")
        for side in ("sources", "targets"):
            for endpoint in event.get(side) or []:
                if not isinstance(endpoint, dict):
                    continue
                object_name = endpoint.get("object")
                if isinstance(object_name, str) and object_name and event_id:
                    event_ids_by_object.setdefault(object_name, []).append(event_id)

    published: dict[str, Any] = {}
    for object_name in sorted(published_names):
        covers = [object_name]
        spec = specs.get(object_name)
        if isinstance(spec, dict):
            for related in ledger_related_object_names_from_spec(spec):
                if related in objects and related not in covers:
                    covers.append(related)
        published[object_name] = {
            "ledgerObject": object_name,
            "covers": covers,
            "sourceEventIds": sorted(set(event_ids_by_object.get(object_name, []))),
            "reason": "expected topoNamingState publication boundary",
        }

    dropped: dict[str, Any] = {}
    for object_name in sorted(objects):
        if object_name in published_names:
            continue
        owner = ledger_find_owner_for_child(specs, object_name)
        if owner and owner[0] in published_names:
            covered_by, reason = owner
        else:
            record = objects.get(object_name, {})
            type_id = str(record.get("type") or "")
            if type_id in {"App::Link", "App::LinkElement"}:
                related = ledger_related_object_names_from_spec(specs.get(object_name, {}))
                covered_by = next((name for name in related if name in published_names), first_published)
                reason = "covered_by_link_target"
            else:
                covered_by = first_published
                reason = "not_referenced"
        if not covered_by:
            continue
        dropped[object_name] = {
            "reason": reason,
            "coveredBy": covered_by,
            "sourceEventIds": sorted(set(event_ids_by_object.get(object_name, []))),
        }

    return {
        "publishedObjects": published,
        "droppedObjects": dropped,
    }


def round_trip_reference_results(
    replay_payload: dict[str, Any],
    replay_capture: dict[str, Any] | None,
    input_refs: list[dict[str, Any]],
) -> tuple[list[dict[str, str]], list[dict[str, Any]]]:
    replay_topo_state = replay_payload.get("topoNamingState")
    raw_topo_state = (
        replay_capture.get("rawTopoNamingState")
        if isinstance(replay_capture, dict)
        else None
    )
    bindings = (
        replay_capture.get("resolvedReferenceBindings")
        if isinstance(replay_capture, dict)
        else None
    )
    published_topo_state = (
        replay_capture.get("publishedTopoNamingState")
        if isinstance(replay_capture, dict)
        else None
    )
    if not isinstance(raw_topo_state, dict):
        raw_topo_state = {}
    if not isinstance(bindings, list):
        bindings = []

    results: list[dict[str, str]] = []
    diagnostics: list[dict[str, Any]] = []
    for ref in input_refs:
        if not isinstance(ref, dict) or not ref.get("required", True):
            continue
        ref_id = ref.get("id")
        target_name = ref.get("target") or ref.get("owner")
        stable_token = ref.get("stableSubname") or ref.get("element")
        if not all(isinstance(value, str) and value for value in (ref_id, target_name, stable_token)):
            continue

        binding = resolved_binding_for_reference(bindings, ref)
        resolved_object = binding.get("resolvedObject") if isinstance(binding, dict) else None
        resolved_subname = binding.get("resolvedSubname") if isinstance(binding, dict) else None
        status = (
            "resolved"
            if published_topo_state == replay_topo_state
            and resolved_binding_has_native_target(raw_topo_state, binding)
            else "failed"
        )
        results.append({
            "inputReferenceId": ref_id,
            "status": status,
        })
        if status != "resolved":
            diagnostics.append({
                "code": "round_trip_reference_not_resolved",
                "inputReferenceId": ref_id,
                "target": target_name,
                "stableSubname": stable_token,
                "resolvedObject": resolved_object,
                "resolvedSubname": resolved_subname,
                "reason": "native_binding_or_raw_target_missing",
            })
    return results, diagnostics


def collect_round_trip_result(
    fixture_path: Path,
    fixture: dict[str, Any],
    expected_payload: dict[str, Any],
    target_override: Sequence[str] | None,
    input_refs: list[dict[str, Any]],
) -> dict[str, Any]:
    topo_state = expected_payload.get("topoNamingState")
    if not isinstance(topo_state, dict):
        return {
            "status": "not_applicable",
            "results": [],
        }

    round_trip_fixture = copy.deepcopy(fixture)
    round_trip_fixture["topoNamingState"] = topo_state
    with tempfile.NamedTemporaryFile(
        "w",
        encoding="utf-8",
        suffix=".json",
        prefix=f"{fixture_path.stem}.roundtrip.",
        dir=str(fixture_path.parent),
        delete=False,
    ) as handle:
        temp_path = Path(handle.name)
        json.dump(round_trip_fixture, handle, ensure_ascii=False)
        handle.write("\n")

    try:
        try:
            replay_payload = collect_one(temp_path, target_override)
            replay_capture = copy.deepcopy(LAST_FREECAD_LEDGER_CAPTURE)
            results, diagnostics = round_trip_reference_results(
                replay_payload,
                replay_capture,
                input_refs,
            )
            status = "passed" if not diagnostics else "failed"
        except Exception as exc:
            status = "failed"
            results = [
                {
                    "inputReferenceId": ref["id"],
                    "status": "failed",
                }
                for ref in input_refs
                if ref.get("required", True) and isinstance(ref.get("id"), str)
            ]
            diagnostics = [
                {
                    "code": "round_trip_collect_failed",
                    "message": str(exc),
                }
            ]
    finally:
        try:
            temp_path.unlink()
        except OSError:
            pass

    return {
        "status": status,
        "inputTopoNamingStateHash": semantic_hash(topo_state),
        "results": results,
        "diagnostics": diagnostics,
    }


def build_freecad_expected_ledger(
    fixture_path: Path,
    fixture: dict[str, Any],
    expected_payload: dict[str, Any],
    *,
    freecad_version_value: str,
    occt_version_value: str,
    round_trip: dict[str, Any] | None = None,
    freecad_capture: dict[str, Any] | None = None,
) -> dict[str, Any]:
    topo_state = expected_payload.get("topoNamingState")
    diagnostics = expected_payload.get("diagnostics") if isinstance(expected_payload.get("diagnostics"), list) else []
    outcome = "accepted" if isinstance(topo_state, dict) else "rejected"
    input_refs = collect_fixture_input_references(fixture)
    fixture_section: dict[str, Any] = {
        "phase": fixture_path.parent.name,
        "case": fixture_path.stem,
        "inputHash": semantic_hash(fixture),
        "expectedPayloadHash": semantic_hash(expected_payload),
    }
    if isinstance(topo_state, dict):
        fixture_section["topoNamingStateHash"] = semantic_hash(topo_state)

    ledger: dict[str, Any] = {
        "schema": LEDGER_SCHEMA,
        "outcome": outcome,
        "producer": {
            "name": "FreeCADCmd",
            "freecadVersion": freecad_version_value,
            "occtVersion": occt_version_value,
            "scriptVersion": LEDGER_SCRIPT_VERSION,
        },
        "fixture": fixture_section,
        "inputReferences": input_refs,
        "diagnostics": copy.deepcopy(diagnostics),
    }

    if outcome == "rejected":
        ledger["rejection"] = {
            "diagnosticCodes": [
                item.get("code")
                for item in diagnostics
                if isinstance(item, dict) and isinstance(item.get("code"), str)
            ],
            "reason": "FreeCADCmd rejected the request before publishing topoNamingState",
        }
        return ledger

    raw_topo_state = (
        freecad_capture.get("rawTopoNamingState")
        if isinstance(freecad_capture, dict)
        else None
    )
    published_topo_state = (
        freecad_capture.get("publishedTopoNamingState")
        if isinstance(freecad_capture, dict)
        else None
    )
    resolved_reference_bindings = (
        freecad_capture.get("resolvedReferenceBindings")
        if isinstance(freecad_capture, dict)
        else None
    )
    if not isinstance(raw_topo_state, dict):
        raise UnsupportedFixture("FreeCAD ledger capture is missing rawTopoNamingState")
    if not isinstance(published_topo_state, dict) or published_topo_state != topo_state:
        raise UnsupportedFixture(
            "FreeCAD ledger capture does not match public topoNamingState"
        )
    if not isinstance(resolved_reference_bindings, list):
        raise UnsupportedFixture(
            "FreeCAD ledger capture is missing resolvedReferenceBindings"
        )

    specs = fixture_object_specs(fixture)
    topo_objects = published_topo_state.get("objects")
    published_names = set(topo_objects) if isinstance(topo_objects, dict) else set()
    objects = ledger_objects_from_states(
        fixture,
        raw_topo_state,
        published_topo_state,
        input_refs,
    )
    events = ledger_events_for_input_references(
        raw_topo_state,
        resolved_reference_bindings,
        input_refs,
        objects,
        specs,
        published_names,
    )
    events.extend(
        ledger_events_from_mapper_history(
            raw_topo_state,
            objects,
            specs,
            published_names,
            len(events) + 1,
        )
    )
    covered_refs = sorted({
        ref_id
        for event in events
        for ref_id in event.get("inputReferenceIds", [])
        if isinstance(ref_id, str) and ref_id
    })
    all_ref_ids = sorted(ref["id"] for ref in input_refs if isinstance(ref.get("id"), str))

    ledger.update({
        "objects": objects,
        "events": events,
        "projection": ledger_projection(fixture, published_topo_state, objects, events),
        "coverage": {
            "coveredInputReferenceIds": covered_refs,
            "uncoveredInputReferenceIds": sorted(set(all_ref_ids) - set(covered_refs)),
        },
        "roundTrip": round_trip or {
            "status": "not_run",
            "inputTopoNamingStateHash": semantic_hash(topo_state),
            "results": [],
        },
    })
    return ledger


def collect_expected_ledger(
    fixture_path: Path,
    fixture: dict[str, Any],
    expected_payload: dict[str, Any],
    target_override: Sequence[str] | None,
) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    freecad_capture = copy.deepcopy(LAST_FREECAD_LEDGER_CAPTURE)
    input_refs = collect_fixture_input_references(fixture)
    round_trip = collect_round_trip_result(fixture_path, fixture, expected_payload, target_override, input_refs)
    return build_freecad_expected_ledger(
        fixture_path,
        fixture,
        expected_payload,
        freecad_version_value=freecad_version(FreeCAD),
        occt_version_value=occt_version_from_runtime("fixture-occt-unspecified"),
        round_trip=round_trip,
        freecad_capture=freecad_capture,
    )


def legacy_object_payloads(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Convert legacy native expected payloads to topo-state result summaries."""
    if isinstance(payload.get("results"), list):
        object_payloads: dict[str, dict[str, Any]] = {}
        for item in payload["results"]:
            if not isinstance(item, dict) or "object" not in item:
                continue
            object_payloads[str(item["object"])] = {
                key: value
                for key, value in item.items()
                if key != "object"
            }
        return object_payloads

    if isinstance(payload.get("objects"), dict):
        return {
            str(name): dict(summary)
            for name, summary in payload["objects"].items()
            if isinstance(summary, dict)
        }

    object_name = payload.get("object")
    if isinstance(object_name, str) and object_name:
        legacy_metadata = {
            "schema_version",
            "reference",
            "freecad_version",
            "object",
            "objects",
            "results",
            "bbox_delta",
        }
        return {
            object_name: {
                key: value
                for key, value in payload.items()
                if key not in legacy_metadata
            }
        }
    return {}


def wrap_topo_naming_response_if_needed(
    fixture: dict,
    FreeCAD: Any,
    payload: dict[str, Any],
    created: dict[str, Any] | None = None,
    ledger_payloads: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    if ACTIVE_TOPO_NAMING_STATE is None or "topoNamingState" in payload:
        return payload
    object_payloads = legacy_object_payloads(payload)
    if ledger_payloads is None and created is not None:
        ledger_payloads = topo_ledger_payloads(fixture, created, object_payloads)
    response = topo_naming_state_response(
        fixture,
        FreeCAD,
        object_payloads,
        created,
        ledger_payloads=ledger_payloads,
    )
    diagnostics: list[dict[str, Any]] = []
    for code in payload.get("diagnostic_codes", []):
        diagnostics.append({"code": str(code), "source": "freecad_expected_collector"})
    for item in payload.get("diagnostic_split", []):
        if isinstance(item, dict):
            diagnostics.append({
                "code": "native_expected_diagnostic",
                "source": "freecad_expected_collector",
                **item,
            })
    if not diagnostics and payload.get("object_fields", {}).get("status") == "diagnostic_only":
        diagnostics.append({
            "code": "native_expected_diagnostic",
            "source": "freecad_expected_collector",
            "object_fields": payload.get("object_fields", {}),
        })
    response["diagnostics"] = diagnostics
    return response



def has_part_filled_face_helper(fixture: dict) -> bool:
    return any(
        isinstance(spec, dict) and spec.get("TypeId") == "Part::FilledFace"
        for spec in fixture.get("Objects", [])
    )


def fixture_without_part_helpers(fixture: dict) -> dict:
    filtered = dict(fixture)
    filtered["Objects"] = [
        spec
        for spec in fixture.get("Objects", [])
        if not (isinstance(spec, dict) and spec.get("TypeId") in PART_HELPER_TYPES)
    ]
    return filtered


def part_filled_face_targets(fixture: dict, requested_targets: Sequence[str] | None = None) -> list[str]:
    helpers = [
        str(spec["Name"])
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and spec.get("TypeId") == "Part::FilledFace"
    ]
    targets = list(requested_targets) if requested_targets is not None else fixture.get("recompute", {}).get("objs", helpers)
    return [str(name) for name in targets if str(name) in helpers]


def part_filled_face_helper_specs(fixture: dict) -> dict[str, dict]:
    return {
        str(spec["Name"]): spec
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and spec.get("TypeId") == "Part::FilledFace"
    }


def part_filled_face_link_items(value: dict) -> list[dict]:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyLinkSubList":
        return []
    items = value.get("SubSet", [])
    return [item for item in items if isinstance(item, dict)]


def part_filled_face_default_object_fields(
    boundary_mode: str | None = None,
    boundary_edge_count: int | None = None,
    params: dict[str, Any] | None = None,
    non_boundary_constraint_count: int = 0,
) -> dict:
    fields: dict[str, Any] = {
        "status": "ok",
        "feature": "part_filled_face",
        "helper": "Part.makeFilledFace",
        "source_backed_helper": True,
        "freecad_native_document_object": False,
        "topo_naming_history": "maker_history:filling",
    }
    if boundary_mode is not None:
        fields["boundary_mode"] = boundary_mode
    if boundary_edge_count is not None:
        fields["boundary_edge_count"] = boundary_edge_count
    if params is not None:
        fields["params"] = params
        fields["params_source"] = "Part.makeFilledFace constructor kwargs"
    if non_boundary_constraint_count > 0:
        fields["non_boundary_constraint_count"] = non_boundary_constraint_count
        fields["non_boundary_constraints_status"] = "freecad_expected_backed"
    return fields


def part_filled_face_error_payload(code: str, message: str, include_helper_fields: bool = True) -> dict:
    object_fields: dict[str, Any] = {"status": "error"}
    if include_helper_fields:
        object_fields.update({
            "feature": "part_filled_face",
            "helper": "Part.makeFilledFace",
        })
    return {
        "object_fields": object_fields,
        "native_error": message,
        "native_error_code": code,
    }


def part_filling_wrapper_lifecycle_error_payload(message: str) -> dict:
    return {
        "object_fields": {
            "status": "error",
            "feature": "part_brepoffsetapi_makefilling_wrapper",
            "helper": "Part.BRepOffsetAPI.MakeFilling",
            "source_backed_helper": False,
            "freecad_native_document_object": False,
            "wrapper_lifecycle": "python_mutable_builder_unsupported",
        },
        "native_error": message,
        "native_error_code": "unsupported_wrapper_lifecycle",
    }


def part_filled_face_boundary_shapes(created: dict[str, Any], spec: dict) -> tuple[list[Any], str, int]:
    boundary = spec.get("Properties", {}).get("Boundary")
    items = part_filled_face_link_items(boundary)
    if not items:
        return [], "empty", 0

    shapes = []
    selected_edges = 0
    whole_shapes = 0
    for item in items:
        target_name = item.get("value")
        if not isinstance(target_name, str) or target_name not in created:
            raise UnsupportedFixture(f"Boundary target {target_name} was not created")
        target = created[target_name]
        target_shape = getattr(target, "Shape", None)
        if target_shape is None or target_shape.isNull():
            raise UnsupportedFixture(f"Boundary target {target_name} has no shape")
        sub_list = list_field(item, "StableSubList", "SubList")
        if not sub_list:
            whole_shapes += 1
            shapes.append(target_shape)
            continue
        for subname in sub_list:
            native_subname = native_link_subname(target, str(subname))
            selected = target_shape.getElement(native_subname)
            shapes.append(selected)
            if native_subname.startswith("Edge"):
                selected_edges += 1

    for shape in shapes:
        if getattr(shape, "ShapeType", "") != "Wire":
            continue
        edges = list(getattr(shape, "Edges", []))
        if not edges:
            continue
        try:
            closed = bool(shape.isClosed())
        except Exception:
            closed = False
        return shapes, "closed_wire" if closed else "wire", len(edges)

    if selected_edges and selected_edges == len(shapes):
        return shapes, "edge_wire_closed", selected_edges
    if whole_shapes == 1 and len(shapes) == 1:
        return shapes, "closed_wire", len(getattr(shapes[0], "Edges", []))
    return shapes, "mixed_boundary", selected_edges


def part_filled_face_non_boundary_constraint_count(
    shapes: list[Any],
    boundary_mode: str,
    boundary_edge_count: int,
) -> int:
    boundary_consumed = False
    boundary_edges_consumed = 0
    count = 0
    for shape in shapes:
        shape_type = getattr(shape, "ShapeType", "")
        if not boundary_consumed and shape_type == "Wire":
            boundary_consumed = True
            continue
        if (
            boundary_mode == "edge_wire_closed"
            and not boundary_consumed
            and shape_type == "Edge"
            and boundary_edges_consumed < boundary_edge_count
        ):
            boundary_edges_consumed += 1
            if boundary_edges_consumed == boundary_edge_count:
                boundary_consumed = True
            continue
        if shape_type == "Wire":
            count += len(getattr(shape, "Edges", []))
        elif shape_type in {"Edge", "Face", "Vertex"}:
            count += 1
    return count


def part_filled_face_params(properties: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], bool]:
    mapping = [
        ("Degree", "degree", "degree", 3, int),
        ("PtsOnCurve", "ptsOnCurve", "points_on_curve", 15, int),
        ("NumIter", "numIter", "iterations", 2, int),
        ("Anisotropy", "anisotropy", "anisotropy", False, bool),
        ("Tol2d", "tol2d", "tolerance_2d", 0.00001, float),
        ("Tol3d", "tol3d", "tolerance_3d", 0.0001, float),
        ("TolG1", "tolG1", "tolerance_g1", 0.01, float),
        ("TolG2", "tolG2", "tolerance_g2", 0.1, float),
        ("MaxDegree", "maxDegree", "max_degree", 8, int),
        ("MaxSegments", "maxSegments", "max_segments", 9, int),
    ]
    kwargs: dict[str, Any] = {}
    evidence: dict[str, Any] = {}
    has_explicit_params = False
    for property_name, keyword, evidence_name, default, value_type in mapping:
        is_explicit = property_name in properties
        has_explicit_params = has_explicit_params or is_explicit
        value = consumer_property(properties, property_name, default)
        if value_type is bool:
            if not isinstance(value, bool):
                raise UnsupportedFixture(f"Part.makeFilledFace {property_name} must be boolean")
            if is_explicit:
                kwargs[keyword] = value
            evidence[evidence_name] = value
            continue
        if not isinstance(value, (int, float)):
            raise UnsupportedFixture(f"Part.makeFilledFace {property_name} must be numeric")
        if value_type is int:
            if float(value) <= 0.0 or int(value) != float(value):
                raise UnsupportedFixture(f"Part.makeFilledFace {property_name} must be a positive integer")
            if is_explicit:
                kwargs[keyword] = int(value)
            evidence[evidence_name] = int(value)
        else:
            if float(value) <= 0.0:
                raise UnsupportedFixture(f"Part.makeFilledFace {property_name} must be a positive number")
            if is_explicit:
                kwargs[keyword] = float(value)
            evidence[evidence_name] = float(value)
    return kwargs, evidence, has_explicit_params


def collect_part_filled_face_expected(
    fixture_path: Path,
    fixture: dict,
    requested_targets: Sequence[str] | None = None,
) -> dict:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    helper_specs = part_filled_face_helper_specs(fixture)
    targets = part_filled_face_targets(fixture, requested_targets)
    source_fixture = fixture_without_part_helpers(fixture)
    doc = FreeCAD.newDocument("CadCoreExpected")
    try:
        created = create_objects(FreeCAD, doc, source_fixture)
        doc.recompute()

        object_payloads: dict[str, dict] = {}
        diagnostic_codes: list[str] = []
        for name in targets:
            spec = helper_specs[name]
            properties = spec.get("Properties", {})
            wrapper_properties = {
                "BRepOffsetAPIMakeFillingWrapper",
                "BRepOffsetAPIMakeFillingUvPointOnSupport",
            }
            unsupported_wrapper = sorted(set(properties) & wrapper_properties)
            if unsupported_wrapper:
                diagnostic_codes.extend(["unsupported_wrapper_lifecycle"] * len(unsupported_wrapper))
                object_payloads[name] = part_filling_wrapper_lifecycle_error_payload(
                    "Part.BRepOffsetAPI.MakeFilling direct wrapper requires a mutable Python "
                    "add/build/shape lifecycle; cad-core only supports request-local "
                    "Part.makeFilledFace DTO inputs here."
                )
                continue

            unsupported = sorted(set(properties) & {"Surface", "Supports", "Orders"})
            if unsupported:
                diagnostic_codes.extend(["unsupported_property"] * len(unsupported))
                object_payloads[name] = part_filled_face_error_payload(
                    "unsupported_property",
                    "Unsupported Part.makeFilledFace kwargs: " + ", ".join(unsupported),
                )
                continue

            try:
                shapes, boundary_mode, boundary_edge_count = part_filled_face_boundary_shapes(created, spec)
                params_kwargs, params_evidence, has_explicit_params = part_filled_face_params(properties)
                # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp
                # ::makeFilledFace(), returns TopoShape(...).makeElementFilledFace(...) as a helper
                # result; cad-core Part::FilledFace fixtures are translated into that helper call.
                result_shape = Part.makeFilledFace(shapes, **params_kwargs)
                summary = shape_summary(result_shape)
                payload = dict(summary)
                payload["shape_summary"] = summary
                payload["object_fields"] = part_filled_face_default_object_fields(
                    boundary_mode,
                    boundary_edge_count,
                    params_evidence if has_explicit_params else None,
                    part_filled_face_non_boundary_constraint_count(
                        shapes,
                        boundary_mode,
                        boundary_edge_count,
                    ),
                )
                object_payloads[name] = payload
            except UnsupportedFixture as exc:
                diagnostic_codes.append("missing_link_target")
                object_payloads[name] = part_filled_face_error_payload(
                    "missing_link_target",
                    str(exc),
                    include_helper_fields=False,
                )
            except Exception as exc:
                message = str(exc)
                code = "missing_property" if "No input shape" in message else "execution_failed"
                diagnostic_codes.append(code)
                object_payloads[name] = part_filled_face_error_payload(code, message)

        reference_types = ", ".join(spec["TypeId"] for spec in fixture.get("Objects", []))
        payload: dict[str, Any] = {
            "schema_version": SCHEMA_VERSION,
            "reference": (
                f"FreeCADCmd oracle from {fixture_path.name}; Part::FilledFace is a cad-core "
                "helper translated to Part.makeFilledFace(...); objects: "
                f"{reference_types}"
            ),
            "freecad_version": freecad_version(FreeCAD),
        }
        if len(object_payloads) == 1:
            object_name, summary = next(iter(object_payloads.items()))
            payload["object"] = object_name
            payload.update(summary)
        else:
            payload["objects"] = object_payloads
        if diagnostic_codes:
            payload["diagnostic_codes"] = diagnostic_codes
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload, created)
    finally:
        close_document_with_producer_trace(FreeCAD, doc)


def has_part_geomplate_surface_helper(fixture: dict) -> bool:
    return any(
        isinstance(spec, dict) and spec.get("TypeId") == "Part::GeomPlateSurface"
        for spec in fixture.get("Objects", [])
    )


def part_geomplate_surface_targets(fixture: dict, requested_targets: Sequence[str] | None = None) -> list[str]:
    helpers = [
        str(spec["Name"])
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and spec.get("TypeId") == "Part::GeomPlateSurface"
    ]
    targets = list(requested_targets) if requested_targets is not None else fixture.get("recompute", {}).get("objs", helpers)
    return [str(name) for name in targets if str(name) in helpers]


def part_geomplate_surface_helper_specs(fixture: dict) -> dict[str, dict]:
    return {
        str(spec["Name"]): spec
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and spec.get("TypeId") == "Part::GeomPlateSurface"
    }


def part_geomplate_error_payload(code: str, message: str) -> dict:
    return {
        "object_fields": {
            "status": "error",
            "feature": "part_geomplate_surface",
            "helper": "Part.GeomPlate.BuildPlateSurface",
            "source_backed_helper": True,
            "freecad_native_document_object": False,
        },
        "native_error": message,
        "native_error_code": code,
    }


def geomplate_scalar_property(properties: dict[str, Any], name: str, fallback: float) -> float:
    value = properties.get(name, fallback)
    if isinstance(value, dict) and "value" in value:
        value = value["value"]
    if not isinstance(value, (int, float)) or not math.isfinite(float(value)):
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {name} must be numeric")
    return float(value)


def geomplate_int_property(properties: dict[str, Any], name: str, fallback: int) -> int:
    value = geomplate_scalar_property(properties, name, float(fallback))
    if value < 1 or abs(value - round(value)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {name} must be a positive integer")
    return int(round(value))


def geomplate_non_negative_int_property(properties: dict[str, Any], name: str, fallback: int) -> int:
    value = geomplate_scalar_property(properties, name, float(fallback))
    if value < 0 or abs(value - round(value)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {name} must be a non-negative integer")
    return int(round(value))


def geomplate_bool_property(properties: dict[str, Any], name: str, fallback: bool) -> bool:
    value = properties.get(name, fallback)
    if isinstance(value, dict) and "value" in value:
        value = value["value"]
    if not isinstance(value, bool):
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {name} must be boolean")
    return value


def part_line_segment_curve(FreeCAD: Any, Part: Any, source_spec: dict) -> Any:
    properties = source_spec.get("Properties", {})
    x1 = scalar_fixture_property(properties, "X1")
    y1 = scalar_fixture_property(properties, "Y1")
    z1 = scalar_fixture_property(properties, "Z1")
    x2 = scalar_fixture_property(properties, "X2")
    y2 = scalar_fixture_property(properties, "Y2")
    z2 = scalar_fixture_property(properties, "Z2", 1.0)
    return Part.LineSegment(FreeCAD.Vector(x1, y1, z1), FreeCAD.Vector(x2, y2, z2))


def geomplate_curve_link_items(value: Any) -> list[dict]:
    if not isinstance(value, dict) or value.get("PropertyType") != "App::PropertyLinkSubList":
        return []
    items = value.get("SubSet", [])
    return [item for item in items if isinstance(item, dict)]


def geomplate_criterion_fields(item: dict) -> list[str]:
    return [field for field in ("G0Criterion", "G1Criterion", "G2Criterion") if field in item]


def geomplate_apply_point_criteria(constraint: Any, item: dict, property_name: str) -> None:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate
    # /PointConstraintPyImp.cpp::setG0Criterion()/setG1Criterion()/setG2Criterion(), each
    # parses one double and calls the matching GeomPlate_PointConstraint::SetG*Criterion().
    for field, setter in (
        ("G0Criterion", constraint.setG0Criterion),
        ("G1Criterion", constraint.setG1Criterion),
        ("G2Criterion", constraint.setG2Criterion),
    ):
        if field not in item:
            continue
        value = item[field]
        if isinstance(value, dict) and "value" in value:
            value = value["value"]
        if not isinstance(value, (int, float)) or not math.isfinite(float(value)):
            raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name}.{field} must be numeric")
        setter(float(value))


def geomplate_object_items(value: Any, property_name: str) -> list[dict]:
    if value is None:
        return []
    if isinstance(value, dict) and "value" in value and "PropertyType" not in value:
        value = value["value"]
    if isinstance(value, dict):
        return [value]
    if isinstance(value, list) and all(isinstance(item, dict) for item in value):
        return list(value)
    raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name} must be an object or object list")


def geomplate_link_sub(value: Any) -> dict | None:
    if isinstance(value, dict) and value.get("PropertyType") == "App::PropertyLinkSub":
        return value
    return None


def geomplate_shape_from_link(created: dict[str, Any], value: dict, property_name: str) -> Any:
    target_name = value.get("value")
    if not isinstance(target_name, str) or target_name not in created:
        raise UnsupportedFixture(f"{property_name} target {target_name} was not created")
    target = created[target_name]
    target_shape = getattr(target, "Shape", None)
    if target_shape is None or target_shape.isNull():
        raise UnsupportedFixture(f"{property_name} target {target_name} has no shape")
    sub_list = list_field(value, "StableSubList", "SubList")
    if not sub_list:
        return target_shape
    if len(sub_list) != 1:
        raise UnsupportedFixture(f"{property_name} must reference exactly one subshape")
    return target_shape.getElement(native_link_subname(target, str(sub_list[0])))


def geomplate_initial_surface(created: dict[str, Any], properties: dict[str, Any]) -> Any | None:
    initial_link = geomplate_link_sub(properties.get("InitialSurface"))
    surface_link = geomplate_link_sub(properties.get("Surface"))
    if initial_link and surface_link:
        raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface accepts one initial surface reference")
    link = initial_link or surface_link
    if not link:
        return None
    surface_shape = geomplate_shape_from_link(
        created,
        link,
        "InitialSurface" if initial_link else "Surface",
    )
    faces = list(getattr(surface_shape, "Faces", []))
    if len(faces) != 1:
        raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface initial Surface must resolve to one face")
    return faces[0].Surface


def geomplate_validate_surface_link(created: dict[str, Any], value: dict, property_name: str) -> Any:
    surface_shape = geomplate_shape_from_link(created, value, property_name)
    faces = list(getattr(surface_shape, "Faces", []))
    if len(faces) != 1:
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name} must resolve to one face")
    return faces[0].Surface


def geomplate_curve2d_segment(Part: Any, value: Any, property_name: str) -> Any:
    if not isinstance(value, dict):
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name}.Curve2d must be an object")
    if str(value.get("Kind", "LineSegment")) != "LineSegment":
        raise UnsupportedFixture(
            f"Part.GeomPlate.BuildPlateSurface {property_name}.Curve2d currently supports Kind=LineSegment"
        )
    start = value.get("Start")
    end = value.get("End")
    if (
        not isinstance(start, list)
        or not isinstance(end, list)
        or len(start) != 2
        or len(end) != 2
        or not all(isinstance(component, (int, float)) for component in start + end)
    ):
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name}.Curve2d requires 2D Start/End")
    if abs(float(start[1])) > FREECAD_PRECISION_CONFUSION or abs(float(end[1])) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture(
            f"Part.GeomPlate.BuildPlateSurface {property_name}.Curve2d native collector currently "
            "supports horizontal Line2dSegment payloads"
        )
    if abs(float(start[0]) - float(end[0])) <= FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name}.Curve2d must not be zero length")
    return Part.Geom2d.Line2dSegment(Part.Geom2d.Line2d(), float(start[0]), float(end[0]))


def geomplate_curve_constraint_from_boundary(
    FreeCAD: Any,
    Part: Any,
    fixture: dict,
    item: dict,
    property_name: str,
) -> Any:
    specs = {
        str(candidate.get("Name")): candidate
        for candidate in fixture.get("Objects", [])
        if isinstance(candidate, dict) and isinstance(candidate.get("Name"), str)
    }
    boundary = geomplate_link_sub(item.get("Boundary"))
    if not boundary:
        raise UnsupportedFixture(f"Part.GeomPlate.BuildPlateSurface {property_name} requires Boundary link")
    target_name = boundary.get("value")
    if not isinstance(target_name, str) or target_name not in specs:
        raise UnsupportedFixture(f"{property_name} Boundary source {target_name} was not created")
    source_spec = specs[target_name]
    if source_spec.get("TypeId") != "Part::Line":
        raise UnsupportedFixture(f"{property_name} Boundary source {target_name} is not a supported Part::Line")
    subnames = list_field(boundary, "StableSubList", "SubList")
    if subnames and set(str(subname) for subname in subnames) != {"Edge1"}:
        raise UnsupportedFixture(f"{property_name} Boundary source {target_name} only supports Edge1")
    curve = part_line_segment_curve(FreeCAD, Part, source_spec)
    return Part.GeomPlate.CurveConstraint(
        curve,
        int(item.get("Order", 0)),
        int(item.get("NbPts", 10)),
        float(item.get("TolDist", 0.0001)),
        float(item.get("TolAng", 0.01)),
        float(item.get("TolCurv", 0.1)),
    )


def geomplate_curve_constraints(
    FreeCAD: Any,
    Part: Any,
    fixture: dict,
    created: dict[str, Any],
    spec: dict,
) -> list[Any]:
    specs = {
        str(item.get("Name")): item
        for item in fixture.get("Objects", [])
        if isinstance(item, dict) and isinstance(item.get("Name"), str)
    }
    properties = spec.get("Properties", {})
    constraints = []
    for item in geomplate_curve_link_items(properties.get("CurveConstraints")):
        criteria = geomplate_criterion_fields(item)
        if criteria:
            raise UnsupportedFixture(
                "Part.GeomPlate.BuildPlateSurface curve criteria setter is unsupported: "
                f"{criteria[0]} maps to CurveConstraintPyImp.cpp NotImplementedError"
            )
        if any(key in item for key in ("CurveOnSurface", "OnSurface", "Surface")):
            raise UnsupportedFixture(
                "Part.GeomPlate.BuildPlateSurface G1 curve-on-surface native oracle is blocked: "
                "Tools.cpp uses Adaptor3d_CurveOnSurface, but FreeCADCmd Python "
                "CurveConstraint.setCurve2dOnSurf(...) is unstable for this probe"
            )
        target_name = item.get("value")
        if not isinstance(target_name, str) or target_name not in specs:
            raise UnsupportedFixture(f"Curve constraint source {target_name} was not created")
        source_spec = specs[target_name]
        if source_spec.get("TypeId") != "Part::Line":
            raise UnsupportedFixture(f"Curve constraint source {target_name} is not a supported Part::Line")
        subnames = list_field(item, "StableSubList", "SubList")
        if subnames and set(str(subname) for subname in subnames) != {"Edge1"}:
            raise UnsupportedFixture(f"Curve constraint source {target_name} only supports Edge1 in S1")
        curve = part_line_segment_curve(FreeCAD, Part, source_spec)
        constraints.append(
            Part.GeomPlate.CurveConstraint(
                curve,
                int(item.get("Order", 0)),
                int(item.get("NbPts", 10)),
                float(item.get("TolDist", 0.0001)),
                float(item.get("TolAng", 0.01)),
                float(item.get("TolCurv", 0.1)),
            )
        )
    for item in geomplate_object_items(properties.get("Curve2dOnSurface"), "Curve2dOnSurface"):
        surface = geomplate_link_sub(item.get("Surface"))
        if not surface:
            raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface Curve2dOnSurface requires Surface link")
        geomplate_validate_surface_link(created, surface, "Curve2dOnSurface.Surface")
        constraint = geomplate_curve_constraint_from_boundary(FreeCAD, Part, fixture, item, "Curve2dOnSurface")
        constraint.setCurve2dOnSurf(geomplate_curve2d_segment(Part, item.get("Curve2d"), "Curve2dOnSurface"))
        constraints.append(constraint)
    for item in geomplate_object_items(properties.get("ProjectedCurve2d"), "ProjectedCurve2d"):
        surface = geomplate_link_sub(item.get("Surface"))
        if not surface:
            raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface ProjectedCurve2d requires Surface link")
        geomplate_validate_surface_link(created, surface, "ProjectedCurve2d.Surface")
        constraint = geomplate_curve_constraint_from_boundary(FreeCAD, Part, fixture, item, "ProjectedCurve2d")
        constraint.setProjectedCurve(
            geomplate_curve2d_segment(Part, item.get("Curve2d"), "ProjectedCurve2d"),
            float(item.get("TolU", 0.001)),
            float(item.get("TolV", 0.001)),
        )
        constraints.append(constraint)
    return constraints


def geomplate_point_constraints(FreeCAD: Any, Part: Any, created: dict[str, Any], spec: dict) -> list[Any]:
    properties = spec.get("Properties", {})
    value = properties.get("PointConstraints", [])
    if isinstance(value, dict) and "value" in value:
        value = value["value"]
    if not isinstance(value, list):
        raise UnsupportedFixture("PointConstraints must be a list")
    constraints = []
    for index, item in enumerate(value):
        point_value = item.get("Point") if isinstance(item, dict) else item
        if (
            not isinstance(point_value, list)
            or len(point_value) != 3
            or not all(isinstance(component, (int, float)) for component in point_value)
        ):
            raise UnsupportedFixture(f"PointConstraints[{index}] must be a three-number vector")
        order = int(item.get("Order", 0)) if isinstance(item, dict) else 0
        tol_dist = float(item.get("TolDist", 0.0001)) if isinstance(item, dict) else 0.0001
        constraint = Part.GeomPlate.PointConstraint(
            FreeCAD.Vector(float(point_value[0]), float(point_value[1]), float(point_value[2])),
            order,
            tol_dist,
        )
        if isinstance(item, dict):
            geomplate_apply_point_criteria(constraint, item, "PointConstraints")
        constraints.append(constraint)
    for item in geomplate_object_items(properties.get("Point2dOnSurface"), "Point2dOnSurface"):
        surface = geomplate_link_sub(item.get("Surface"))
        if not surface:
            raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface Point2dOnSurface requires Surface link")
        geomplate_validate_surface_link(created, surface, "Point2dOnSurface.Surface")
        point_value = item.get("Point")
        point2d = item.get("Point2d")
        if (
            not isinstance(point_value, list)
            or len(point_value) != 3
            or not all(isinstance(component, (int, float)) for component in point_value)
        ):
            raise UnsupportedFixture("Point2dOnSurface.Point must be a three-number vector")
        if (
            not isinstance(point2d, list)
            or len(point2d) != 2
            or not all(isinstance(component, (int, float)) for component in point2d)
        ):
            raise UnsupportedFixture("Point2dOnSurface.Point2d must be a two-number vector")
        constraint = Part.GeomPlate.PointConstraint(
            FreeCAD.Vector(float(point_value[0]), float(point_value[1]), float(point_value[2])),
            int(item.get("Order", 0)),
            float(item.get("TolDist", 0.0001)),
        )
        geomplate_apply_point_criteria(constraint, item, "Point2dOnSurface")
        constraint.setPnt2dOnSurf(float(point2d[0]), float(point2d[1]))
        constraints.append(constraint)
    return constraints


def geomplate_projected_curve2d_known_gap(fixture_path: Path) -> dict[str, Any]:
    import FreeCAD  # type: ignore

    return {
        "freecad_version": freecad_version(FreeCAD),
        "known_gap": {
            "kind": "geomplate_projected_curve2d_native_oracle_blocked",
            "reason": (
                "Do not freeze ProjectedCurve2d geometry from cad-core output. FreeCAD "
                "CurveConstraintPyImp.cpp::setProjectedCurve() calls SetProjectedCurve(hCurve, "
                "tolU, tolV), but the S1 FreeCADCmd Python wrapper probe returns a stable "
                "RuntimeError before helper expected geometry is available."
            ),
            "source_authority": [
                "/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::setProjectedCurve()",
            ],
            "cad_core_fixture": f"cad-core/fixtures/c5m7/{fixture_path.name}",
            "freecadcmd_evidence": {
                "helper": "Part.GeomPlate.CurveConstraint.setProjectedCurve",
                "probe_case": "geomplate_projected_curve2d",
                "error": "RuntimeError: Geom_RectangularTrimmedSurface::V1==V2",
            },
            "uncollected_fields": [
                "shape_summary for ProjectedCurve2d helper result",
                "object_fields.constraints[].projected_curve2d native helper geometry",
            ],
            "delete_condition": (
                "Replace this blocker with a FreeCAD expected geometry payload only after a stable "
                "native oracle can call Part.GeomPlate.CurveConstraint.setProjectedCurve(...) "
                "and return helper geometry instead of RuntimeError."
            ),
        },
        "reference": (
            "Native FreeCAD ProjectedCurve2d oracle is blocked for this fixture; cad-core "
            "source-backed implementation is covered by focused source-evidence tests, not by "
            "this expected file."
        ),
    }


def collect_part_geomplate_surface_expected(
    fixture_path: Path,
    fixture: dict,
    requested_targets: Sequence[str] | None = None,
) -> dict:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    helper_specs = part_geomplate_surface_helper_specs(fixture)
    targets = part_geomplate_surface_targets(fixture, requested_targets)
    source_fixture = fixture_without_part_helpers(fixture)
    doc = FreeCAD.newDocument("CadCoreExpected")
    try:
        created = create_objects(FreeCAD, doc, source_fixture)
        doc.recompute()

        object_payloads: dict[str, dict] = {}
        diagnostic_codes: list[str] = []
        for name in targets:
            spec = helper_specs[name]
            properties = spec.get("Properties", {})
            unsupported = sorted(set(properties) - {
                "CurveConstraints",
                "PointConstraints",
                "Degree",
                "NbPtsOnCur",
                "NbIter",
                "Tol2d",
                "Tol3d",
                "TolAng",
                "TolCurv",
                "Anisotropy",
                "ApproxTol3d",
                "ApproxMaxSegments",
                "ApproxMaxDegree",
                "ApproxMaxDistance",
                "ApproxCritOrder",
                "ApproxContinuity",
                "ApproxEnlargeCoeff",
                "InitialSurface",
                "Surface",
                "Curve2dOnSurface",
                "ProjectedCurve2d",
                "Point2dOnSurface",
                "PlateSurfaceCurves",
            })
            if unsupported:
                diagnostic_codes.extend(["unsupported_property"] * len(unsupported))
                object_payloads[name] = part_geomplate_error_payload(
                    "unsupported_property",
                    "Unsupported Part.GeomPlate.BuildPlateSurface properties: " + ", ".join(unsupported),
                )
                continue
            if properties.get("PlateSurfaceCurves") is not None:
                diagnostic_codes.append("unsupported_wrapper_lifecycle")
                object_payloads[name] = part_geomplate_error_payload(
                    "unsupported_wrapper_lifecycle",
                    "Part.PlateSurface.Curves requires PlateSurfacePy wrapper lifecycle; "
                    "PlateSurfacePyImp.cpp leaves Curves as TODO and GeomPlateSurface Save/Restore "
                    "throw NotImplementedError",
                )
                continue
            try:
                curve_constraints = geomplate_curve_constraints(FreeCAD, Part, fixture, created, spec)
                point_constraints = geomplate_point_constraints(FreeCAD, Part, created, spec)
                if not curve_constraints and not point_constraints:
                    raise UnsupportedFixture("Part.GeomPlate.BuildPlateSurface requires curve or point constraints")

                initial_surface = geomplate_initial_surface(created, properties)
                builder_kwargs = {
                    "Degree": geomplate_int_property(properties, "Degree", 3),
                    "NbPtsOnCur": geomplate_int_property(properties, "NbPtsOnCur", 10),
                    "NbIter": geomplate_int_property(properties, "NbIter", 3),
                    "Tol2d": geomplate_scalar_property(properties, "Tol2d", 0.00001),
                    "Tol3d": geomplate_scalar_property(properties, "Tol3d", 0.0001),
                    "TolAng": geomplate_scalar_property(properties, "TolAng", 0.01),
                    "TolCurv": geomplate_scalar_property(properties, "TolCurv", 0.1),
                    "Anisotropy": geomplate_bool_property(properties, "Anisotropy", False),
                }
                if initial_surface is not None:
                    builder_kwargs["Surface"] = initial_surface
                builder = Part.GeomPlate.BuildPlateSurface(
                    **builder_kwargs
                )
                for constraint in curve_constraints + point_constraints:
                    builder.add(constraint)
                builder.perform()
                if not builder.isDone():
                    diagnostic_codes.append("surface_not_done")
                    object_payloads[name] = part_geomplate_error_payload(
                        "surface_not_done",
                        "GeomPlate_BuildPlateSurface did not finish",
                    )
                    continue

                surface = builder.surface()
                bspline = surface.makeApprox(
                    geomplate_scalar_property(properties, "ApproxTol3d", 0.01),
                    geomplate_int_property(properties, "ApproxMaxSegments", 9),
                    geomplate_int_property(properties, "ApproxMaxDegree", 3),
                    geomplate_scalar_property(properties, "ApproxMaxDistance", 0.0001),
                    geomplate_non_negative_int_property(properties, "ApproxCritOrder", 0),
                    str(properties.get("ApproxContinuity", "C1")),
                    geomplate_scalar_property(properties, "ApproxEnlargeCoeff", 1.1),
                )
                result_shape = bspline.toShape()
                payload = shape_summary(result_shape)
                # GeomPlate_MakeApprox is sensitive to whether the constraint curve came from
                # FreeCAD GeometryCurvePy or cad-core's request-local OCCT edge bridge. Keep the
                # oracle strict on topology, volume and metadata while allowing the approximation
                # surface bbox to drift within the observed native/cad-core envelope; the explicit
                # initial surface path currently needs a slightly wider bbox envelope.
                payload["bbox_delta"] = 0.2 if initial_surface is not None else 0.1
                payload["object_fields"] = {
                    "status": "ok",
                    "shape": "occt_face",
                    "feature": "part_geomplate_surface",
                    "helper": "Part.GeomPlate.BuildPlateSurface",
                    "dto": "PartGeomPlateSurfaceDTO",
                    "source_backed_helper": True,
                    "freecad_native_document_object": False,
                    "is_done": True,
                    "surface_kind": "GeomPlate_Surface",
                    "curve_constraint_count": len(curve_constraints),
                    "point_constraint_count": len(point_constraints),
                }
                if properties.get("ProjectedCurve2d") is not None:
                    payload["oracle_evidence"] = {
                        "helper": "Part.GeomPlate.CurveConstraint.setProjectedCurve",
                        "collectability": "expected_backed_with_initial_surface",
                        "probe_script": (
                            "docs/CADCore5.0-PartDesign-高价值剩余语义/"
                            "C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/"
                            "6-22-05-28-c5m13-s4-geomplate-native-oracle-probe.py"
                        ),
                        "successful_probe_case": (
                            "geomplate_projected_curve2d_variants:"
                            "range_0_4_tol_0p01_initial_surface"
                        ),
                        "blocked_without_initial_surface": "RuntimeError: Geom_RectangularTrimmedSurface::V1==V2",
                    }
                object_payloads[name] = payload
            except UnsupportedFixture as exc:
                code = "missing_constraints" if "requires curve or point constraints" in str(exc) else "missing_curve_source"
                if "not a supported Part::Line" in str(exc):
                    code = "invalid_curve_source"
                if "Curve2dOnSurface" in str(exc) or "ProjectedCurve2d" in str(exc):
                    code = "invalid_curve2d_source"
                if "Point2dOnSurface" in str(exc):
                    code = "invalid_point2d_source"
                if "curve criteria setter" in str(exc):
                    code = "unsupported_curve_criteria"
                if "PlateSurface.Curves" in str(exc):
                    code = "unsupported_wrapper_lifecycle"
                if "PointConstraints" in str(exc):
                    code = "invalid_point_constraint"
                if (
                    "must be" in str(exc)
                    and "PointConstraints" not in str(exc)
                    and code == "missing_curve_source"
                ):
                    code = "invalid_parameter"
                diagnostic_codes.append(code)
                object_payloads[name] = part_geomplate_error_payload(code, str(exc))
            except Exception as exc:
                message = str(exc)
                code = "approximation_failed" if "Approximation" in message else "perform_failed"
                diagnostic_codes.append(code)
                object_payloads[name] = part_geomplate_error_payload(code, message)

        reference_types = ", ".join(spec["TypeId"] for spec in fixture.get("Objects", []))
        payload: dict[str, Any] = {
            "schema_version": SCHEMA_VERSION,
            "reference": (
                f"FreeCADCmd oracle from {fixture_path.name}; Part::GeomPlateSurface is a "
                "cad-core helper translated to Part.GeomPlate.BuildPlateSurface(...); objects: "
                f"{reference_types}"
            ),
            "freecad_version": freecad_version(FreeCAD),
        }
        if len(object_payloads) == 1:
            object_name, summary = next(iter(object_payloads.items()))
            payload["object"] = object_name
            payload.update(summary)
        else:
            payload["objects"] = object_payloads
        if diagnostic_codes:
            payload["diagnostic_codes"] = diagnostic_codes
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload, created)
    finally:
        close_document_with_producer_trace(FreeCAD, doc)


def property_payload_value(value: Any) -> Any:
    if isinstance(value, dict) and "PropertyType" in value and "value" in value:
        return value.get("value")
    return value


def object_property_value(spec: dict[str, Any], property_name: str) -> Any:
    properties = spec.get("Properties", {})
    if not isinstance(properties, dict):
        return None
    return property_payload_value(properties.get(property_name))


def part_geometry_curve_dto_from_object(spec: dict[str, Any]) -> dict[str, Any]:
    dto = {
        "name": spec.get("Name"),
        "curveKind": object_property_value(spec, "CurveKind"),
        "center": object_property_value(spec, "Center"),
        "normal": object_property_value(spec, "Normal"),
        "angleXU": object_property_value(spec, "AngleXU"),
        "startAngle": object_property_value(spec, "StartAngle"),
        "endAngle": object_property_value(spec, "EndAngle"),
    }
    major_radius = object_property_value(spec, "MajorRadius")
    if major_radius is not None:
        dto["majorRadius"] = major_radius
    minor_radius = object_property_value(spec, "MinorRadius")
    if minor_radius is not None:
        dto["minorRadius"] = minor_radius
    focal = object_property_value(spec, "Focal")
    if focal is not None:
        dto["focal"] = focal
    return dto


def part_geometry_curve_items(fixture: dict) -> list[dict[str, Any]]:
    objects = fixture.get("Objects", [])
    if not isinstance(objects, list):
        return []
    return [
        part_geometry_curve_dto_from_object(item)
        for item in objects
        if isinstance(item, dict) and item.get("TypeId") == "Part::GeometryCurve"
    ]


def part_geometry_curve_consumer_items(fixture: dict) -> list[dict[str, Any]]:
    objects = fixture.get("Objects", [])
    if not isinstance(objects, list):
        return []
    return [
        item
        for item in objects
        if isinstance(item, dict)
        and item.get("TypeId") in {"Part::Extrusion", "Part::Line", "Part::RuledSurface"}
    ]


def has_part_geometry_curve_object(fixture: dict) -> bool:
    return bool(part_geometry_curve_items(fixture))


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
    raise UnsupportedFixture(f"unsupported Part::GeometryCurve CurveKind {dto.get('curveKind')}")


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


def consumer_property(properties: dict[str, Any], name: str, fallback: Any = None) -> Any:
    if name not in properties:
        return fallback
    return property_payload_value(properties[name])


def consumer_number_property(properties: dict[str, Any], name: str, fallback: float = 0.0) -> float:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, (int, float)):
        raise UnsupportedFixture(f"Part::GeometryCurve consumer {name} must be numeric")
    return float(value)


def consumer_bool_property(properties: dict[str, Any], name: str, fallback: bool = False) -> bool:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, bool):
        raise UnsupportedFixture(f"Part::GeometryCurve consumer {name} must be boolean")
    return value


def consumer_vector_property(properties: dict[str, Any], name: str, fallback: list[float]) -> list[float]:
    value = consumer_property(properties, name, fallback)
    if not isinstance(value, list) or len(value) != 3 or not all(isinstance(item, (int, float)) for item in value):
        raise UnsupportedFixture(f"Part::GeometryCurve consumer {name} must be a three-number vector")
    return [float(item) for item in value]


def consumer_link_property(properties: dict[str, Any], name: str) -> str:
    value = consumer_property(properties, name)
    if not isinstance(value, str) or not value:
        raise UnsupportedFixture(f"Part::GeometryCurve consumer {name} must be an object link")
    return value


def collect_part_geometry_curve_extrusion_expected(
    FreeCAD: Any,
    consumer: dict[str, Any],
    source_shapes: dict[str, Any],
    source_metadata: dict[str, dict[str, str]],
) -> dict:
    if consumer.get("TypeId") != "Part::Extrusion":
        raise UnsupportedFixture("Part::GeometryCurve consumer objects expected collection supports Part::Extrusion")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("Part::GeometryCurve consumer Properties must be an object")

    dir_mode = consumer_property(properties, "DirMode", "Custom")
    if dir_mode not in {"Custom", 0}:
        raise UnsupportedFixture("Part::GeometryCurve consumer oracle only supports Part::Extrusion DirMode=Custom")
    if consumer_bool_property(properties, "Solid", False):
        raise UnsupportedFixture("Part::GeometryCurve consumer oracle only supports Part::Extrusion Solid=false")
    if abs(consumer_number_property(properties, "TaperAngle", 0.0)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("Part::GeometryCurve consumer oracle does not publish tapered extrusion")
    if abs(consumer_number_property(properties, "TaperAngleRev", 0.0)) > FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("Part::GeometryCurve consumer oracle does not publish tapered reverse extrusion")

    base_name = consumer_link_property(properties, "Base")
    if base_name not in source_shapes:
        raise UnsupportedFixture(f"Part::GeometryCurve consumer Base {base_name} was not created")

    direction = consumer_vector_property(properties, "Dir", [0.0, 0.0, 1.0])
    magnitude = math.sqrt(sum(component * component for component in direction))
    if magnitude <= FREECAD_PRECISION_CONFUSION:
        raise UnsupportedFixture("Part::GeometryCurve consumer Dir must not be zero-length")
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
        raise UnsupportedFixture("Part::GeometryCurve consumer total extrusion length must not be zero")

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
        raise UnsupportedFixture("Part::GeometryCurve consumer objects line source must be Part::Line")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("Part::GeometryCurve line consumer Properties must be an object")
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
        "feature": "part_line",
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
    raise UnsupportedFixture(f"Part::GeometryCurve consumer {name} must be one of {', '.join(labels)}")


def prefixed_source_metadata(prefix: str, metadata: dict[str, str]) -> dict[str, str]:
    return {f"{prefix}_{key}": value for key, value in metadata.items()}


def collect_part_geometry_curve_ruled_surface_expected(
    Part: Any,
    consumer: dict[str, Any],
    source_shapes: dict[str, Any],
    source_metadata: dict[str, dict[str, str]],
) -> dict:
    if consumer.get("TypeId") != "Part::RuledSurface":
        raise UnsupportedFixture("Part::GeometryCurve consumer objects ruled surface source must be Part::RuledSurface")
    properties = consumer.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("Part::GeometryCurve ruled surface consumer Properties must be an object")

    curve1 = consumer_link_property(properties, "Curve1")
    curve2 = consumer_link_property(properties, "Curve2")
    for curve in (curve1, curve2):
        if curve not in source_shapes:
            raise UnsupportedFixture(f"Part::GeometryCurve consumer Curve source {curve} was not created")

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
        raise UnsupportedFixture("Part::GeometryCurve expected collection requires at least one DTO")
    if not consumers and len(items) != 1:
        raise UnsupportedFixture("Part::GeometryCurve edge expected collection supports one valid DTO per fixture")

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
                "Part::GeometryCurve consumer objects expected collection supports Part::Line, "
                "Part::Extrusion and Part::RuledSurface"
            )

    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "reference": (
            f"FreeCADCmd Part::GeometryCurve/PartConicCurveDTO oracle from {fixture_path.name}; "
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


def app_link_payload(
    obj: Any,
    role: str,
    selected_subnames: Sequence[str] = (),
    stable_subnames: Sequence[str] = (),
) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    payload = shape_summary(shape)
    subshapes = subshape_response_entries(obj, shape)
    if stable_subnames and len(stable_subnames) != len(selected_subnames):
        raise UnsupportedFixture(
            f"App::Link {obj.Name} requires one durable stable subname per selected subshape"
        )
    for index, selected_subname in enumerate(selected_subnames):
        indexed = selected_subname.rsplit(".", 1)[-1]
        for subshape in subshapes:
            if subshape.get("indexed") != indexed:
                continue
            # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp
            # ::LinkBaseExtension::extensionGetSubObject() preserves the true linked child
            # path. The native Face has indexed name Face1, while the public Link response
            # carries its child-qualified current identity. A state-backed StableSubList,
            # when present, remains the durable identity instead of being replaced by that
            # display path.
            subshape["subname"] = selected_subname
            subshape["stableSubname"] = (
                stable_subnames[index] if stable_subnames else selected_subname
            )
            subshape["identityStatus"] = "stable"
            subshape["fullSubname"] = f"{obj.Name}.{selected_subname}"
            break
    payload["subshapes"] = subshapes
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


def identity_placement_payload() -> dict:
    return {
        "PropertyType": "App::PropertyPlacement",
        "Base": [0.0, 0.0, 0.0],
        "Rotation": [0.0, 0.0, 0.0, 1.0],
    }


def placement_payload_is_identity(value: dict, tolerance: float = 1e-9) -> bool:
    return same_placement_payload(value, identity_placement_payload(), tolerance)


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


def set_accepted_default_planar_scalar(solver_joint: dict) -> None:
    # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
    # ::AssemblyObject::makeMbdJointDistance(), default branch creates "ASMTPlanarJoint" and
    # writes "offset = getJointDistance(joint)". The accepted set is limited to native
    # expected-backed rows returned by distance_type_is_accepted_default_planar().
    distance = float(solver_joint.get("distance", 0.0) or 0.0)
    solver_joint["solver_joint_class"] = "ASMTPlanarJoint"
    solver_joint["offset"] = distance
    solver_joint["scalar_correction"] = 0.0
    solver_joint["scalar_correction_source"] = "none"
    solver_joint["radius_source_side"] = "none"
    solver_joint["distance_type_mapping_status"] = "mapped_c9m3_default_planar"
    solver_joint["distance_type_boundary"] = "expected_backed_default_planar_supported"


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


def distance_type_is_accepted_default_planar(distance_type: str | None) -> bool:
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
    elif distance_type_is_accepted_default_planar(distance_type):
        set_accepted_default_planar_scalar(solver_joint)
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


def bundled_offset_oracle_config(fixture: dict) -> dict | None:
    config = fixture.get("freecad_expected", {}).get("bundled_offset_oracle")
    if not isinstance(config, dict):
        return None
    if config.get("enabled", True) is False:
        return None
    return config


def native_bundled_offset_oracle_payload(
    fixture: dict,
    created: dict[str, Any],
) -> tuple[dict, dict[str, dict]]:
    config = bundled_offset_oracle_config(fixture)
    if config is None:
        return {}, {}

    runtime_offsets: dict[str, dict] = {}
    bundles = []
    for item in config.get("bundles", []):
        if not isinstance(item, dict):
            continue
        root_name = str(item.get("root", ""))
        member_name = str(item.get("member", ""))
        fixed_joint = str(item.get("fixed_joint", ""))
        root = created.get(root_name)
        member = created.get(member_name)
        if root is None or member is None:
            bundles.append({
                "root": root_name,
                "member": member_name,
                "fixed_joint": fixed_joint,
                "status": "missing_fixture_object",
            })
            continue

        # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        # ::AssemblyObject::getMbDData(), for fixed bundles stores "plc.inverse() * plci" in
        # objectPartMap[partToAdd].offsetPlc.
        offset = root.Placement.inverse() * member.Placement
        offset_payload = placement_payload(offset)
        non_identity = not placement_payload_is_identity(offset_payload)
        runtime_offsets[member_name] = {
            "root": root_name,
            "member": member_name,
            "fixed_joint": fixed_joint,
            "offset_placement": offset,
            "offset_payload": offset_payload,
            "non_identity": non_identity,
        }
        bundles.append({
            "root": root_name,
            "member": member_name,
            "fixed_joint": fixed_joint,
            "status": "resolved_source_backed_offsetPlc",
            "offset_formula": "plc.inverse() * plci",
            "offset_non_identity": non_identity,
            "root_initial_placement": placement_payload(root.Placement),
            "member_initial_placement": placement_payload(member.Placement),
            "offsetPlc": offset_payload,
            "source": (
                "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
                "::AssemblyObject::getMbDData(), objectPartMap[partToAdd].offsetPlc = "
                "\"plc.inverse() * plci\""
            ),
        })

    if not bundles:
        return {}, {}

    return {
        "collector_mode": "source_backed_bundled_offsetPlc_evidence",
        "route": str(config.get("route", "backend_gap_candidate")),
        "scope_ids": [str(item) for item in config.get("scope_ids", [])],
        "backend_gap_ids": [str(item) for item in config.get("backend_gap_ids", [])],
        "blocker_id": str(config.get("blocker_id", "")),
        "sources": [
            "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.h"
            "::AssemblyObject::MbDPartData::offsetPlc",
            "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::getMbDData()",
            "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::handleOneSideOfJoint()",
            "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::validateNewPlacements()",
            "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
            "::AssemblyObject::setNewPlacements()",
        ],
        "bundles": bundles,
    }, runtime_offsets


def attach_bundled_writeback_evidence(
    oracle: dict,
    runtime_offsets: dict[str, dict],
    created: dict[str, Any],
) -> None:
    if not oracle:
        return
    for bundle in oracle.get("bundles", []):
        member_name = str(bundle.get("member", ""))
        offset_data = runtime_offsets.get(member_name)
        if not offset_data:
            continue
        root = created.get(offset_data["root"])
        member = created.get(member_name)
        if root is None or member is None:
            continue
        # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
        # ::AssemblyObject::setNewPlacements() and ::validateNewPlacements() both use
        # "getMbdPlacement(mbdPart) * offsetPlc" for bundled objects.
        computed_member = root.Placement * offset_data["offset_placement"]
        computed_payload = placement_payload(computed_member)
        native_member_payload = placement_payload(member.Placement)
        bundle["writeback_evidence"] = {
            "source": (
                "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
                "::AssemblyObject::setNewPlacements(); "
                "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
                "::AssemblyObject::validateNewPlacements()"
            ),
            "formula": "getMbdPlacement(mbdPart) * offsetPlc",
            "mbd_part_owner": offset_data["root"],
            "member": member_name,
            "mbd_part_placement_after_solve": placement_payload(root.Placement),
            "offsetPlc": offset_data["offset_payload"],
            "computed_member_placement": computed_payload,
            "native_member_placement_after_solve": native_member_payload,
            "matches_native_member_placement_after_solve": same_placement_payload(
                computed_payload,
                native_member_payload,
            ),
        }


def native_joint_reference_marker_evidence(
    joint: Any,
    ref_name: str,
    plc_name: str,
    bundled_offsets: dict[str, dict] | None = None,
) -> dict:
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
        marker_without_offset = marker
        offset_data = bundled_offsets.get(link_name(part), {}) if bundled_offsets else {}
        offset_boundary = "identity_offset_for_two_box_assembly_link_fixture"
        marker_offset_payload = None
        marker_without_offset_payload = None
        if offset_data:
            marker = offset_data["offset_placement"] * marker
            marker_without_offset_payload = placement_payload(marker_without_offset)
            marker_offset_payload = offset_data["offset_payload"]
            offset_boundary = "non_identity_objectPartMap_offsetPlc"
        evidence.update({
            "status": "resolved_native_handle_one_side",
            "frame": "part_local",
            "moving_part": link_name(part),
            "object_global_placement": placement_payload(obj_global),
            "part_global_placement": placement_payload(part_global),
            "jcs_global_placement": placement_payload(jcs_global),
            "marker_placement": placement_payload(marker),
            "offset_boundary": offset_boundary,
        })
        if marker_offset_payload is not None:
            evidence.update({
                "marker_without_offsetPlc": marker_without_offset_payload,
                "offsetPlc": marker_offset_payload,
                "offsetPlc_non_identity": not placement_payload_is_identity(marker_offset_payload),
                "offsetPlc_source": (
                    "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
                    "::AssemblyObject::getMbDData(), objectPartMap[partToAdd].offsetPlc = "
                    "\"plc.inverse() * plci\""
                ),
                "offsetPlc_consumer": (
                    "/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp"
                    "::AssemblyObject::handleOneSideOfJoint(), applies \"data.offsetPlc * plc\""
                ),
                "bundled_mbd_part_owner": str(offset_data.get("root", "")),
                "bundled_member": str(offset_data.get("member", "")),
                "fixed_joint": str(offset_data.get("fixed_joint", "")),
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


def native_marker_oracle_payload(
    created: dict[str, Any],
    solver_joints: list[dict],
    bundled_offsets: dict[str, dict] | None = None,
) -> dict:
    entries = []
    requires_marker_parity = False
    for solver_joint in solver_joints:
        joint = created.get(str(solver_joint.get("object", "")))
        if joint is None or not hasattr(joint, "JointType"):
            continue
        joint_type = str(getattr(joint, "JointType", solver_joint.get("joint_type", "")))
        native_reference1 = native_joint_reference_marker_evidence(
            joint,
            "Reference1",
            "Placement1",
            bundled_offsets,
        )
        native_reference2 = native_joint_reference_marker_evidence(
            joint,
            "Reference2",
            "Placement2",
            bundled_offsets,
        )
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
            or native_reference1.get("offsetPlc_non_identity")
            or native_reference2.get("offsetPlc_non_identity")
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
    bundled_offset_oracle, bundled_offsets = native_bundled_offset_oracle_payload(fixture, created)
    native_marker_oracle = native_marker_oracle_payload(created, solver_joints, bundled_offsets)
    solve_code = int(obj.solve(False))
    attach_bundled_writeback_evidence(bundled_offset_oracle, bundled_offsets, created)
    if solve_code == -6:
        payload = {
            "status": "error",
            "reason": "no_grounded_part",
            "grounded_joints": grounded_joints,
            "joints": joints,
            "solver_joints": solver_joints,
            "unsupported_joints": [],
            "placement_updates": [],
            "native_solver_return": solve_code,
        }
        if bundled_offset_oracle:
            payload["bundled_offset_oracle"] = bundled_offset_oracle
        return payload, native_marker_oracle
    if solve_code != 0:
        payload = {
            "status": "error",
            "reason": "native_solver_failed",
            "grounded_joints": grounded_joints,
            "joints": joints,
            "solver_joints": solver_joints,
            "unsupported_joints": [],
            "placement_updates": [],
            "native_solver_return": solve_code,
        }
        if bundled_offset_oracle:
            payload["bundled_offset_oracle"] = bundled_offset_oracle
        return payload, native_marker_oracle

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
    payload = {
        "status": "solved",
        "mode": mode,
        "grounded_joints": grounded_joints,
        "joints": joints,
        "solver_joints": solver_joints,
        "unsupported_joints": [],
        "placement_updates": placement_updates,
        "native_solver_return": solve_code,
    }
    if bundled_offset_oracle:
        payload["bundled_offset_oracle"] = bundled_offset_oracle
    return payload, native_marker_oracle


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
        if "bundled_offset_oracle" in solver_adapter:
            payload["bundled_offset_oracle"] = solver_adapter["bundled_offset_oracle"]
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


def link_sublist_object_names(properties: dict[str, Any], name: str) -> list[str]:
    value = properties.get(name, {})
    if not isinstance(value, dict):
        return []
    names: list[str] = []
    for item in value.get("SubSet", []):
        if isinstance(item, dict) and item.get("value"):
            names.append(str(item["value"]))
    return names


def link_sublist_items(properties: dict[str, Any], name: str) -> list[dict[str, str]]:
    value = properties.get(name, {})
    if not isinstance(value, dict):
        return []
    items: list[dict[str, str]] = []
    for item in value.get("SubSet", []):
        if not isinstance(item, dict) or not item.get("value"):
            continue
        sub_names = list_field(item, "StableSubList", "SubList")
        if not sub_names:
            items.append({"object": str(item["value"]), "subshape": ""})
            continue
        for subname in sub_names:
            items.append({"object": str(item["value"]), "subshape": str(subname)})
    return items


def first_link_subname(properties: dict[str, Any], name: str) -> str:
    value = properties.get(name, {})
    if not isinstance(value, dict):
        return ""
    sub_list = list_field(value, "StableSubList", "SubList")
    if sub_list:
        return str(sub_list[0])
    sub_set = value.get("SubSet")
    if isinstance(sub_set, list) and sub_set:
        item = sub_set[0]
        if isinstance(item, dict):
            sub_names = list_field(item, "StableSubList", "SubList")
            if sub_names:
                return str(sub_names[0])
    return ""


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


def project_on_surface_mode_from_properties(properties: dict[str, Any]) -> str:
    labels = ["All", "Faces", "Edges"]
    value = consumer_property(properties, "Mode", "All")
    if isinstance(value, str) and value in labels:
        return value
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return labels[index]
    return "All"


def float_from_properties(properties: dict[str, Any], name: str, fallback: float) -> float:
    value = consumer_property(properties, name, fallback)
    if isinstance(value, (int, float)):
        return float(value)
    return fallback


def project_on_surface_offset_vector(properties: dict[str, Any]) -> list[float] | None:
    offset = float_from_properties(properties, "Offset", 0.0)
    if offset == 0.0:
        return None
    value = consumer_property(properties, "Direction", [0.0, 0.0, 1.0])
    if (
        not isinstance(value, list)
        or len(value) != 3
        or not all(isinstance(item, (int, float)) for item in value)
    ):
        return None
    magnitude = math.sqrt(sum(float(component) * float(component) for component in value))
    if magnitude <= FREECAD_PRECISION_CONFUSION:
        return None
    return [float(component) / magnitude * offset for component in value]


def project_on_surface_source_shape_kind(subname: str) -> str:
    if subname.startswith("Edge"):
        return "edge"
    if subname.startswith("Wire"):
        return "wire"
    if subname.startswith("Face"):
        return "face"
    return "shape"


def project_on_surface_projection_item_ledger(projection_items: list[dict[str, str]]) -> list[dict[str, Any]]:
    ledger: list[dict[str, Any]] = []
    for index, item in enumerate(projection_items):
        subshape = item.get("subshape", "")
        ledger.append(
            {
                "source_object": item.get("object", ""),
                "source_subname": subshape,
                "stable_subname": subshape,
                "projection_item_index": index,
                "source_shape_kind": project_on_surface_source_shape_kind(subshape),
            }
        )
    return ledger


def project_on_surface_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    projection_sources = link_sublist_object_names(properties, "Projection")
    projection_items = link_sublist_items(properties, "Projection")
    face_wire_counts = [len(getattr(face, "Wires", [])) for face in getattr(shape, "Faces", [])]
    payload = shape_summary(shape)
    payload["bbox_delta"] = 0.11
    object_fields = {
        "status": "ok",
        "shape": shape_kind(shape),
        "feature": "part_project_on_surface",
        "source_support": link_property_object_name(properties, "SupportFace"),
        "support_face": first_link_subname(properties, "SupportFace"),
        "source_projection": projection_sources[0] if projection_sources else "",
        "projection_subshape": first_link_subname(properties, "Projection"),
        "mode": project_on_surface_mode_from_properties(properties),
        "height": float_from_properties(properties, "Height", 0.0),
        "offset": float_from_properties(properties, "Offset", 0.0),
        "topo_naming_history": "indexed_projected_edges_no_mapper_history",
        "projected_solid_count": len(getattr(shape, "Solids", [])),
        "projected_face_count": len(getattr(shape, "Faces", [])),
        "projected_wire_count": len(getattr(shape, "Wires", [])),
        "projected_inner_wire_count": sum(max(0, count - 1) for count in face_wire_counts),
    }
    if projection_items:
        object_fields["projection_items"] = projection_items
        object_fields["projection_item_ledger"] = project_on_surface_projection_item_ledger(projection_items)
    offset_vector = project_on_surface_offset_vector(properties)
    if offset_vector is not None:
        object_fields["offset_application"] = "compound_child_moved_after_filter"
        object_fields["offset_vector"] = offset_vector
    payload["object_fields"] = object_fields
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


def sweep_transition_from_properties(properties: dict[str, Any]) -> str:
    labels = ["Transformed", "Right corner", "Round corner"]
    value = consumer_property(properties, "Transition", "Right corner")
    if isinstance(value, str) and value in labels:
        return value
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return labels[index]
    return "Right corner"


def sweep_transition_mode_index(properties: dict[str, Any]) -> int:
    labels = ["Transformed", "Right corner", "Round corner"]
    value = consumer_property(properties, "Transition", "Right corner")
    if isinstance(value, str) and value in labels:
        return labels.index(value)
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return index
    raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell Transition must be 0..2 or a known label")


def is_part_sweep_wrapper_helper_spec(spec: dict[str, Any]) -> bool:
    properties = spec.get("Properties", {})
    return (
        spec.get("TypeId") == "Part::Sweep"
        and isinstance(properties, dict)
        and any(field in properties for field in PART_SWEEP_WRAPPER_ADVANCED_FIELDS)
    )


def has_part_sweep_wrapper_helper(fixture: dict) -> bool:
    return any(
        isinstance(spec, dict) and is_part_sweep_wrapper_helper_spec(spec)
        for spec in fixture.get("Objects", [])
    )


def part_sweep_wrapper_specs(fixture: dict) -> dict[str, dict[str, Any]]:
    return {
        str(spec["Name"]): spec
        for spec in fixture.get("Objects", [])
        if isinstance(spec, dict) and is_part_sweep_wrapper_helper_spec(spec)
    }


def fixture_without_part_sweep_wrapper_helpers(fixture: dict) -> dict:
    filtered = dict(fixture)
    filtered["Objects"] = [
        spec
        for spec in fixture.get("Objects", [])
        if not (isinstance(spec, dict) and is_part_sweep_wrapper_helper_spec(spec))
    ]
    filtered["recompute"] = {"objs": []}
    return filtered


def part_sweep_wrapper_target_names(
    fixture: dict,
    requested_targets: Sequence[str] | None = None,
) -> list[str]:
    helper_specs = part_sweep_wrapper_specs(fixture)
    candidates = (
        list(requested_targets)
        if requested_targets is not None
        else list(fixture.get("recompute", {}).get("objs", helper_specs.keys()))
    )
    return [str(name) for name in candidates if str(name) in helper_specs]


def strict_bool_property(value: Any, field: str, fallback: bool = False) -> bool:
    if value is None:
        return fallback
    if isinstance(value, bool):
        return value
    raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {field} must be boolean")


def strict_number(value: Any, field: str) -> float:
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {field} must be a finite number")


def strict_vector3(value: Any, field: str) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {field} must be a [x, y, z] vector")
    vector = [strict_number(item, field) for item in value]
    if math.isclose(vector[0], 0.0) and math.isclose(vector[1], 0.0) and math.isclose(vector[2], 0.0):
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {field} must be non-zero")
    return vector


def first_link_subshape(value: dict[str, Any]) -> str:
    subnames = list_field(value, "StableSubList", "SubList")
    return str(subnames[0]) if subnames else ""


def wrapper_indexed_subshape(shape: Any, subname: str) -> Any:
    shape_type = str(getattr(shape, "ShapeType", ""))
    for prefix, attribute in (
        ("Vertex", "Vertexes"),
        ("Edge", "Edges"),
        ("Wire", "Wires"),
        ("Face", "Faces"),
        ("Shell", "Shells"),
        ("Solid", "Solids"),
    ):
        if not subname.startswith(prefix):
            continue
        try:
            index = int(subname[len(prefix) :]) - 1
        except ValueError as exc:
            raise UnsupportedFixture(f"invalid subshape token {subname}") from exc
        if shape_type == prefix and index == 0:
            return shape
        items = list(getattr(shape, attribute, []))
        if 0 <= index < len(items):
            return items[index]
        raise UnsupportedFixture(f"subshape token {subname} is out of range")
    raise UnsupportedFixture(f"unsupported subshape token {subname}")


def resolve_wrapper_link_shape(
    created: dict[str, Any],
    value: Any,
    property_name: str,
) -> tuple[Any, dict[str, str]]:
    if not isinstance(value, dict):
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} must be a link")
    target_name = str(value.get("value", ""))
    if target_name not in created:
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} target {target_name} was not created")
    target = created[target_name]
    shape = getattr(target, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} target {target_name} has no shape")
    metadata = {"target": target_name}
    subname = first_link_subshape(value)
    if subname:
        metadata["subname"] = subname
        try:
            native_subname = native_link_subname(target, subname)
            if hasattr(shape, "getSubShape"):
                shape = shape.getSubShape(native_subname)
            else:
                shape = wrapper_indexed_subshape(shape, native_subname)
        except Exception as exc:
            raise UnsupportedFixture(
                f"Part.BRepOffsetAPI_MakePipeShell {property_name} cannot resolve {target_name}.{subname}: {exc}"
            ) from exc
        if shape is None or shape.isNull():
            raise UnsupportedFixture(
                f"Part.BRepOffsetAPI_MakePipeShell {property_name} resolved empty {target_name}.{subname}"
            )
    return shape, metadata


def wrapper_wire_from_shape(Part: Any, shape: Any, property_name: str) -> Any:
    shape_type = str(getattr(shape, "ShapeType", ""))
    if shape_type == "Wire":
        return shape
    if shape_type == "Edge":
        return Part.Wire([shape])
    wires = list(getattr(shape, "Wires", []))
    if wires:
        return wires[0]
    edges = list(getattr(shape, "Edges", []))
    if edges:
        return Part.Wire(edges)
    raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} cannot form a wire")


def wrapper_profile_shape(Part: Any, shape: Any, property_name: str) -> Any:
    if str(getattr(shape, "ShapeType", "")) == "Vertex":
        return shape
    return wrapper_wire_from_shape(Part, shape, property_name)


def wrapper_linked_object_shape(created: dict[str, Any], name: str, property_name: str) -> Any:
    if name not in created:
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} target {name} was not created")
    shape = getattr(created[name], "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell {property_name} target {name} has no shape")
    return shape


def make_pipeshell_builder(Part: Any, spine_wire: Any) -> tuple[Any, str]:
    direct = getattr(Part, "BRepOffsetAPI_MakePipeShell", None)
    if direct is not None:
        return direct(spine_wire), "Part.BRepOffsetAPI_MakePipeShell"
    namespace = getattr(Part, "BRepOffsetAPI", None)
    nested = getattr(namespace, "MakePipeShell", None) if namespace is not None else None
    if nested is not None:
        return nested(spine_wire), "Part.BRepOffsetAPI.MakePipeShell"
    raise UnsupportedFixture("FreeCAD Part module does not expose BRepOffsetAPI MakePipeShell wrapper")


def wrapper_sections(
    Part: Any,
    created: dict[str, Any],
    properties: dict[str, Any],
) -> tuple[list[Any], list[str]]:
    section_names = link_property_object_names(properties, "Sections")
    if not section_names:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell requires at least one Sections item")
    profiles = [
        wrapper_profile_shape(Part, wrapper_linked_object_shape(created, name, "Sections"), "Sections")
        for name in section_names
    ]
    return profiles, section_names


def wrapper_tolerance(properties: dict[str, Any]) -> dict[str, float] | None:
    tolerance = properties.get("Tolerance")
    if tolerance is None:
        return None
    if not isinstance(tolerance, dict) or "PropertyType" in tolerance:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell Tolerance must be an object with tol3d/boundTol/tolAngular")
    return {
        "tol3d": strict_number(tolerance.get("tol3d"), "Tolerance.tol3d"),
        "boundTol": strict_number(tolerance.get("boundTol"), "Tolerance.boundTol"),
        "tolAngular": strict_number(tolerance.get("tolAngular"), "Tolerance.tolAngular"),
    }


def wrapper_binormal(properties: dict[str, Any]) -> tuple[str, list[float]] | None:
    has_canonical = "Binormal" in properties
    has_legacy = "BiNormal" in properties
    if has_canonical and has_legacy:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell accepts either Binormal or BiNormal, not both")
    if not has_canonical and not has_legacy:
        return None
    property_name = "Binormal" if has_canonical else "BiNormal"
    return property_name, strict_vector3(property_payload_value(properties[property_name]), property_name)


def wrapper_section_options(
    Part: Any,
    created: dict[str, Any],
    properties: dict[str, Any],
    section_names: list[str],
) -> list[dict[str, Any]]:
    raw_options = properties.get("SectionOptions", [])
    if raw_options is None:
        return []
    if not isinstance(raw_options, list):
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell SectionOptions must be a list")
    options: list[dict[str, Any]] = []
    for index, section_name in enumerate(section_names):
        raw_option = raw_options[index] if index < len(raw_options) else {}
        if raw_option is None:
            raw_option = {}
        if not isinstance(raw_option, dict):
            raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell SectionOptions[{index}] must be an object")
        option: dict[str, Any] = {
            "profile": section_name,
            "with_contact": strict_bool_property(raw_option.get("WithContact"), f"SectionOptions[{index}].WithContact"),
            "with_correction": strict_bool_property(raw_option.get("WithCorrection"), f"SectionOptions[{index}].WithCorrection"),
        }
        if "Location" in raw_option:
            location_shape, metadata = resolve_wrapper_link_shape(created, raw_option["Location"], f"SectionOptions[{index}].Location")
            if str(getattr(location_shape, "ShapeType", "")) != "Vertex":
                raise UnsupportedFixture(f"Part.BRepOffsetAPI_MakePipeShell SectionOptions[{index}].Location must resolve to a vertex")
            option["location_shape"] = location_shape
            option["location"] = metadata
        options.append(option)
    if len(raw_options) > len(section_names):
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell SectionOptions has more items than Sections")
    return options


def collect_part_sweep_wrapper_object_expected(
    FreeCAD: Any,
    Part: Any,
    created: dict[str, Any],
    spec: dict[str, Any],
) -> dict:
    properties = spec.get("Properties", {})
    if not isinstance(properties, dict):
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell properties must be an object")

    spine_shape, spine_metadata = resolve_wrapper_link_shape(created, properties.get("Spine"), "Spine")
    spine_wire = wrapper_wire_from_shape(Part, spine_shape, "Spine")
    profiles, section_names = wrapper_sections(Part, created, properties)
    section_options = wrapper_section_options(Part, created, properties, section_names)
    transition_index = sweep_transition_mode_index(properties)
    transition_label = sweep_transition_from_properties(properties)
    tolerance = wrapper_tolerance(properties)
    binormal = wrapper_binormal(properties)

    builder, runtime_helper = make_pipeshell_builder(Part, spine_wire)
    builder.setTransitionMode(transition_index)
    builder_status: dict[str, Any] = {
        "transition_mode": transition_index,
    }
    advanced: dict[str, Any] = {}
    if tolerance is not None:
        builder.setTolerance(tolerance["tol3d"], tolerance["boundTol"], tolerance["tolAngular"])
        advanced["tolerance"] = tolerance
        builder_status["set_tolerance"] = True

    selected_modes = 0
    if "AuxiliarySpine" in properties or "AuxiliaryCurvilinear" in properties:
        selected_modes += 1
        auxiliary_shape, metadata = resolve_wrapper_link_shape(created, properties.get("AuxiliarySpine"), "AuxiliarySpine")
        auxiliary_wire = wrapper_wire_from_shape(Part, auxiliary_shape, "AuxiliarySpine")
        curvilinear = strict_bool_property(properties.get("AuxiliaryCurvilinear"), "AuxiliaryCurvilinear", True)
        builder.setAuxiliarySpine(auxiliary_wire, curvilinear, 0)
        advanced["mode"] = "Auxiliary"
        advanced["auxiliary_spine"] = {
            **metadata,
            "curvilinear": curvilinear,
            "contact": "NoContact",
        }
        builder_status["set_auxiliary_spine"] = True

    support_mode = consumer_property(properties, "SupportMode")
    if support_mode is not None:
        if support_mode not in {"None", "SurfaceNormal"}:
            raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell SupportMode must be None or SurfaceNormal")
        if support_mode == "SurfaceNormal":
            selected_modes += 1
            support_shape, metadata = resolve_wrapper_link_shape(created, properties.get("SpineSupport"), "SpineSupport")
            set_mode_ok = bool(builder.setSpineSupport(support_shape))
            if not set_mode_ok:
                raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell setSpineSupport returned false")
            advanced["mode"] = "SurfaceNormal"
            advanced["support_mode"] = "SurfaceNormal"
            advanced["spine_support"] = {
                **metadata,
                "set_mode_ok": set_mode_ok,
            }
            builder_status["set_spine_support"] = set_mode_ok
    elif "SpineSupport" in properties:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell SpineSupport requires SupportMode=SurfaceNormal")

    if binormal is not None:
        selected_modes += 1
        binormal_property, vector = binormal
        builder.setBiNormalMode(FreeCAD.Vector(vector[0], vector[1], vector[2]))
        advanced["mode"] = "Binormal"
        advanced["binormal"] = vector
        advanced["binormal_property"] = binormal_property
        builder_status["set_binormal_mode"] = True

    if selected_modes > 1:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell accepts one advanced builder mode per request")

    if selected_modes == 0:
        builder.setFrenetMode(bool_from_properties(properties, "Frenet", True))
        builder_status["set_frenet_mode"] = True

    advanced_sections: list[dict[str, Any]] = []
    for index, profile in enumerate(profiles):
        option = section_options[index] if index < len(section_options) else {
            "profile": section_names[index],
            "with_contact": False,
            "with_correction": False,
        }
        if "location_shape" in option:
            builder.add(
                Profile=profile,
                Location=option["location_shape"],
                WithContact=option["with_contact"],
                WithCorrection=option["with_correction"],
            )
        else:
            builder.add(
                Profile=profile,
                WithContact=option["with_contact"],
                WithCorrection=option["with_correction"],
            )
        section_payload = {
            "profile": option["profile"],
            "with_contact": option["with_contact"],
            "with_correction": option["with_correction"],
        }
        if "location" in option:
            section_payload["location"] = option["location"]
        advanced_sections.append(section_payload)
    if advanced_sections:
        advanced["sections"] = advanced_sections

    builder_status["is_ready"] = bool(builder.isReady())
    builder_status["status_before_build"] = int(builder.getStatus())
    if not builder_status["is_ready"]:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell helper is not ready after adding profiles")
    builder.build()
    builder_status["build_ok"] = True
    if bool_from_properties(properties, "Solid", True):
        builder_status["make_solid_ok"] = bool(builder.makeSolid())
    result_shape = builder.shape()
    builder_status["status_after_build"] = int(builder.getStatus())
    builder_status["shape_access_ok"] = result_shape is not None and not result_shape.isNull()
    if not builder_status["shape_access_ok"]:
        raise UnsupportedFixture("Part.BRepOffsetAPI_MakePipeShell returned an empty shape")

    payload = shape_summary(result_shape)
    comparable_advanced = {
        key: value
        for key, value in advanced.items()
        if key != "sections"
    }
    summary_fields = {
        "shape": shape_kind(result_shape),
        "bbox": payload["bbox"],
        "volume": payload["volume"],
        "topology_counts": payload["topology_counts"],
    }
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(result_shape),
        "feature": "part_sweep",
        "spine": spine_metadata.get("target", ""),
        "sections": section_names,
        "solid": bool_from_properties(properties, "Solid", True),
        "frenet": bool_from_properties(properties, "Frenet", True),
        "transition": transition_label,
        "advanced": comparable_advanced,
        "topo_naming_history": "maker_history:pipeshell",
    }
    payload["shape_summary"] = summary_fields
    payload["wrapper_oracle"] = {
        "helper": "Part.BRepOffsetAPI_MakePipeShell",
        "runtime_helper": runtime_helper,
        "dto": "PartSweepAdvancedPipeShellDTO",
        "freecad_native_document_object": False,
        "spine_subname": spine_metadata.get("subname", ""),
        "advanced": advanced,
        "builder_status": builder_status,
    }
    return payload


def wrapper_diagnostic_code(reason: str) -> str:
    if "was not created" in reason:
        return "missing_link_target"
    if "cannot resolve" in reason or "resolved empty" in reason:
        return "invalid_subshape"
    if "Tolerance must be an object" in reason:
        return "unsupported_property"
    if (
        "must be boolean" in reason
        or "must be a finite number" in reason
        or "must be non-zero" in reason
        or "SupportMode must be" in reason
    ):
        return "invalid_parameter"
    return "unsupported_property"


def collect_part_sweep_wrapper_expected(
    fixture_path: Path,
    fixture: dict,
    requested_targets: Sequence[str] | None = None,
) -> dict:
    import FreeCAD  # type: ignore
    import Part  # type: ignore

    targets = part_sweep_wrapper_target_names(fixture, requested_targets)
    if not targets:
        raise UnsupportedFixture("no Part.BRepOffsetAPI_MakePipeShell wrapper targets were requested")

    doc = FreeCAD.newDocument("CadCoreSweepWrapperExpected")
    try:
        created = create_objects(FreeCAD, doc, fixture_without_part_sweep_wrapper_helpers(fixture))
        doc.recompute()
        helper_specs = part_sweep_wrapper_specs(fixture)
        object_payloads: dict[str, dict] = {}
        diagnostic_split: list[dict[str, str]] = []
        for name in targets:
            try:
                object_payloads[name] = collect_part_sweep_wrapper_object_expected(
                    FreeCAD,
                    Part,
                    created,
                    helper_specs[name],
                )
            except UnsupportedFixture as exc:
                diagnostic_split.append({
                    "object": name,
                    "reason": str(exc),
                    "policy": "cad-core focused diagnostics own invalid support/mode/location/tolerance payloads",
                })
            except Exception as exc:
                diagnostic_split.append({
                    "object": name,
                    "reason": f"{type(exc).__name__}: {exc}",
                    "policy": "S2 must keep or document an explicit FreeCADCmd wrapper collector blocker",
                })

        reference_types = ", ".join(spec["TypeId"] for spec in fixture.get("Objects", []))
        payload: dict[str, Any] = {
            "schema_version": SCHEMA_VERSION,
            "reference": (
                f"FreeCADCmd wrapper oracle from {fixture_path.name}; Part::Sweep advanced DTO "
                "translated to request-local Part.BRepOffsetAPI_MakePipeShell helper; objects: "
                f"{reference_types}"
            ),
            "freecad_version": freecad_version(FreeCAD),
        }
        if len(object_payloads) == 1:
            object_name, summary = next(iter(object_payloads.items()))
            payload["object"] = object_name
            payload.update(summary)
        elif object_payloads:
            payload["objects"] = object_payloads
        else:
            payload["object_fields"] = {
                "status": "diagnostic_only",
                "helper": "Part.BRepOffsetAPI_MakePipeShell",
                "dto": "PartSweepAdvancedPipeShellDTO",
                "advanced": {},
                "builder_status": {"build_ok": False, "shape_access_ok": False},
            }
        if diagnostic_split:
            payload["diagnostic_split"] = diagnostic_split
            payload["diagnostic_codes"] = [
                wrapper_diagnostic_code(item["reason"])
                for item in diagnostic_split
                if item["policy"].startswith("cad-core focused diagnostics")
            ]
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload, created)
    finally:
        close_document_with_producer_trace(FreeCAD, doc)


def sweep_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "feature": "part_sweep",
        "spine": link_property_object_name(properties, "Spine"),
        "sections": link_property_object_names(properties, "Sections"),
        "solid": bool_from_properties(properties, "Solid", True),
        "frenet": bool_from_properties(properties, "Frenet", True),
        "transition": sweep_transition_from_properties(properties),
        "linearize": bool_from_properties(properties, "Linearize", False),
        "topo_naming_history": "maker_history:pipeshell",
    }
    return payload


def pipe_transition_from_properties(properties: dict[str, Any]) -> str:
    labels = ["Transformed", "Right corner", "Round corner"]
    value = consumer_property(properties, "Transition", "Transformed")
    if isinstance(value, str) and value in labels:
        return value
    if isinstance(value, (int, float)):
        index = int(value)
        if 0 <= index < len(labels):
            return labels[index]
    return "Transformed"


def pipe_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "feature": "partdesign_pipe",
        "add_sub": "sub" if getattr(obj, "TypeId", "") == "PartDesign::SubtractivePipe" else "add",
        "source_profile": link_property_object_name(properties, "Profile"),
        "spine": link_property_object_name(properties, "Spine"),
        "mode": str(consumer_property(properties, "Mode", "Standard")),
        "transformation": str(consumer_property(properties, "Transformation", "Constant")),
        "transition": pipe_transition_from_properties(properties),
        "topo_naming_history": "maker_history:partdesign_pipe",
    }
    return payload


def vector_payload(value: Any) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def datum_map_mode_active(obj: Any) -> bool:
    try:
        return str(obj.MapMode) not in {"", "Deactivated"}
    except Exception:
        return False


def datum_map_mode_label(obj: Any) -> str:
    try:
        return str(obj.MapMode)
    except Exception:
        return "Deactivated"


def datum_alias_source_mode(type_id: str, mode: str) -> str:
    if type_id == "PartDesign::Line":
        if mode == "AxisOfCurvature":
            return "SectionOfRevolution"
        if mode == "Normal":
            return "FrenetTB"
        if mode == "Binormal":
            return "FrenetTN"
    if type_id == "PartDesign::Point" and mode == "CenterOfCurvature":
        return "SectionOfRevolution"
    return mode


def datum_point_vector(obj: Any) -> Any:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPoint.cpp
    # ::Point::getPoint() returns "Placement.getValue().getPosition()". Some current
    # FreeCADCmd builds expose datum targets as generic Part.Feature wrappers in scripts,
    # so Placement.Base is the equivalent fallback for expected collection.
    if hasattr(obj, "getPoint"):
        return obj.getPoint()
    return obj.Placement.Base


def datum_line_direction(obj: Any) -> Any:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp
    # ::Line::getDirection() rotates "Base::Vector3d(0, 0, 1)" by Placement.
    if hasattr(obj, "getDirection"):
        return obj.getDirection()
    import FreeCAD  # type: ignore

    return obj.Placement.Rotation.multVec(FreeCAD.Vector(0, 0, 1))


def datum_axis_vector(obj: Any, getter: str, axis: tuple[float, float, float]) -> Any:
    if hasattr(obj, getter):
        return getattr(obj, getter)()
    import FreeCAD  # type: ignore

    return obj.Placement.Rotation.multVec(FreeCAD.Vector(*axis))


def datum_payload(obj: Any) -> dict:
    type_id = getattr(obj, "TypeId", "")
    fields: dict[str, Any] = {
        "attached": datum_map_mode_active(obj),
        "status": "ok",
    }
    if fields["attached"]:
        map_mode = datum_map_mode_label(obj)
        fields["map_mode"] = map_mode
        alias_source_mode = datum_alias_source_mode(type_id, map_mode)
        if alias_source_mode != map_mode:
            fields["alias_source_mode"] = alias_source_mode
    if type_id == "PartDesign::Point":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPoint.cpp
        # ::Point::getPoint(), returns "Placement.getValue().getPosition()".
        fields["datum"] = "point"
        fields["point"] = vector_payload(datum_point_vector(obj))
    elif type_id == "PartDesign::Line":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp
        # ::Line::getDirection(), rotates "Base::Vector3d(0, 0, 1)" by Placement.
        fields["datum"] = "line"
        fields["base"] = vector_payload(obj.Placement.Base)
        fields["direction"] = vector_payload(datum_line_direction(obj))
    elif type_id == "PartDesign::Plane":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPlane.cpp
        # ::Plane::getNormal(), rotates "Base::Vector3d(0, 0, 1)" by Placement.
        fields["datum"] = "plane"
        fields["origin"] = vector_payload(obj.Placement.Base)
        fields["x_axis"] = vector_payload(datum_axis_vector(obj, "getXAxis", (1, 0, 0)))
        fields["normal"] = vector_payload(datum_axis_vector(obj, "getNormal", (0, 0, 1)))
    elif type_id == "PartDesign::CoordinateSystem":
        # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumCS.cpp
        # ::getXAxis()/getYAxis()/getZAxis() apply Placement rotation to unit axes.
        fields["datum"] = "coordinate_system"
        fields["origin"] = vector_payload(obj.Placement.Base)
        fields["x_axis"] = vector_payload(datum_axis_vector(obj, "getXAxis", (1, 0, 0)))
        fields["y_axis"] = vector_payload(datum_axis_vector(obj, "getYAxis", (0, 1, 0)))
        fields["z_axis"] = vector_payload(datum_axis_vector(obj, "getZAxis", (0, 0, 1)))
    else:
        raise UnsupportedFixture(f"target object {obj.Name} is not a supported Datum payload")
    return {"object_fields": fields}


def boolean_payload(obj: Any, fixture: dict | None = None) -> dict:
    shape = getattr(obj, "Shape", None)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    spec = fixture_spec_for_object(fixture, str(obj.Name))
    properties = spec.get("Properties", {}) if isinstance(spec.get("Properties", {}), dict) else {}
    payload = shape_summary(shape)
    payload["object_fields"] = {
        "status": "ok",
        "shape": shape_kind(shape),
        "body_mode": "replace",
        "boolean_type": str(consumer_property(properties, "Type", "Fuse")),
        "tools": link_property_object_names(properties, "Group"),
        "topo_naming_history": "maker_history:boolean",
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
    if type_id == "Part::Sweep":
        return sweep_payload(obj, fixture)
    if type_id == "Part::ProjectOnSurface":
        return project_on_surface_payload(obj, fixture)
    if type_id == "PartDesign::Boolean":
        return boolean_payload(obj, fixture)
    if type_id in {"PartDesign::AdditivePipe", "PartDesign::SubtractivePipe"}:
        return pipe_payload(obj, fixture)
    if type_id in {
        "PartDesign::CoordinateSystem",
        "PartDesign::Line",
        "PartDesign::Plane",
        "PartDesign::Point",
    }:
        return datum_payload(obj)

    shape = getattr(obj, "Shape", None)
    if type_id == "Mesh::Import":
        return mesh_import_summary(obj)
    if shape is None or shape.isNull():
        raise UnsupportedFixture(f"target object {obj.Name} has no shape")
    if type_id == "Sketcher::SketchObject":
        return sketch_summary(obj)
    payload = shape_summary(shape)
    payload["subshapes"] = subshape_response_entries(obj, shape)
    return payload


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
    if expected.get("diagnostics"):
        return None
    if isinstance(expected.get("results"), list):
        names = [
            str(item["object"])
            for item in expected["results"]
            if isinstance(item, dict) and "object" in item
        ]
        return names or None
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


def bundled_offset_oracle_from_payload(payload: dict) -> dict | None:
    if isinstance(payload.get("bundled_offset_oracle"), dict):
        return payload["bundled_offset_oracle"]
    for summary in payload.get("objects", {}).values():
        if isinstance(summary.get("bundled_offset_oracle"), dict):
            return summary["bundled_offset_oracle"]
    return None


def bundled_offset_gap_metadata(payload: dict) -> dict[str, Any] | None:
    oracle = bundled_offset_oracle_from_payload(payload)
    if not oracle:
        return None
    scope_ids = [str(item) for item in oracle.get("scope_ids", []) if str(item)]
    backend_gap_ids = [str(item) for item in oracle.get("backend_gap_ids", []) if str(item)]
    blocker_id = str(oracle.get("blocker_id", "C9M2-BLOCKER-301") or "C9M2-BLOCKER-301")
    ids = backend_gap_ids or scope_ids or [blocker_id]
    return {
        "known_gap": (
            f"{blocker_id}: C9-M2 S3 native FreeCAD bundled offsetPlc oracle is checked in "
            f"for {', '.join(scope_ids or ['C9M2-SCOPE-101..103'])}; current cad-core still "
            "treats bundled offsetPlc as an identity boundary. Keep this as "
            "backend_gap_candidate evidence for S6 instead of claiming support in S3."
        ),
        "backendGap": {
            "ids": sorted(dict.fromkeys(ids)),
            "route": "backend_gap_candidate",
            "delete_condition": (
                "S6 implements source-backed non-identity objectPartMap.offsetPlc marker and "
                "writeback parity, focused expected tests pass, and the C9-M2 matrices are "
                "updated from backend_gap_candidate to expected-backed current match."
            ),
        },
    }


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

    supported_mapped_extended = {
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
    mapped_extended = distance_types & supported_mapped_extended
    mapped_extended_gap_candidates = mapped_extended - supported_mapped_extended
    default_boundary = {
        item
        for item in distance_types
        if distance_type_is_default_boundary(item) and not distance_type_is_accepted_default_planar(item)
    }
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
    if mapped_extended_gap_candidates or (mapped_extended and payload_requires_marker_parity(payload)):
        ids = ["DTE-BLOCK-007"]
        reported_mapped_extended = mapped_extended_gap_candidates or mapped_extended
        if reported_mapped_extended & {"LineCircle", "CircleCircle"}:
            ids.append("DTE-BLOCK-003")
        if reported_mapped_extended & {
            "PlaneCylinder",
            "PlaneSphere",
            "CylinderCylinder",
            "CylinderSphere",
            "PointCylinder",
            "PointSphere",
        }:
            ids.append("DTE-BLOCK-004")
        if reported_mapped_extended & {
            "PlaneTorus",
            "CylinderTorus",
            "TorusTorus",
            "TorusSphere",
            "SphereSphere",
        }:
            ids.append("DTE-BLOCK-005")
        if reported_mapped_extended & {"PointCurve"}:
            ids.append("DTE-BLOCK-006")
        if payload_requires_marker_parity(payload):
            ids.extend(["MP-BLOCK-002", "MP-BLOCK-003", "MP-BLOCK-006"])
        return {
            "known_gap": (
                "DTE-S5 native extended DistanceType oracle is checked in "
                f"({', '.join(sorted(reported_mapped_extended))}), but cad-core has not completed the "
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

    global ACTIVE_TOPO_NAMING_STATE, ACTIVE_RESOLVED_REFERENCE_BINDINGS, LAST_FREECAD_LEDGER_CAPTURE, LAST_FREECAD_PRODUCER_TRACE
    LAST_FREECAD_LEDGER_CAPTURE = None
    LAST_FREECAD_PRODUCER_TRACE = None
    ACTIVE_RESOLVED_REFERENCE_BINDINGS = []
    fixture = load_fixture(fixture_path)
    topo_state_error = topo_state_request_error_response(fixture)
    if topo_state_error is not None:
        ACTIVE_TOPO_NAMING_STATE = None
        publish_documentless_producer_trace(
            FreeCAD,
            slice_name="collector.topo_state.reject",
            decision="rejected",
            reason=producer_trace_rejection_reason(topo_state_error, "topo_state_request_rejected"),
            target=fixture_path.stem,
        )
        return topo_state_error
    if isinstance(fixture.get("topoNamingState"), dict):
        input_contract_errors = stable_identity_contract_errors(fixture, "fixture")
        if input_contract_errors:
            raise UnsupportedFixture(
                "invalid topoNamingState stable identity tokens: "
                + ", ".join(input_contract_errors[:8])
            )

    topo_state = fixture.get("topoNamingState")
    ACTIVE_TOPO_NAMING_STATE = topo_state if isinstance(topo_state, dict) else None
    topo_state_protocol_response_payload = topo_state_protocol_branch_response(fixture, FreeCAD)
    if topo_state_protocol_response_payload is not None:
        return topo_state_protocol_response_payload
    if has_part_sweep_wrapper_helper(fixture):
        payload = collect_part_sweep_wrapper_expected(fixture_path, fixture, requested_targets)
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload)
    if has_part_geomplate_surface_helper(fixture):
        payload = collect_part_geomplate_surface_expected(fixture_path, fixture, requested_targets)
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload)
    if has_part_filled_face_helper(fixture):
        payload = collect_part_filled_face_expected(fixture_path, fixture, requested_targets)
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload)
    if has_part_geometry_curve_object(fixture):
        payload = collect_part_geometry_curve_expected(fixture_path, fixture)
        publish_documentless_producer_trace(
            FreeCAD,
            slice_name="collector.part_geometry_curve.raw_shape",
            decision="published",
            reason="documentless_raw_shape_helper",
            target=fixture_path.stem,
        )
        return wrap_topo_naming_response_if_needed(fixture, FreeCAD, payload)
    require_native_hole_profile_support(fixture)
    require_native_dressup_body_membership(fixture)
    require_native_polar_pattern_whole_shape_support(fixture)
    doc = FreeCAD.newDocument("CadCoreExpected")
    assembly_solve_preferences = None
    previous_solve_on_recompute = True
    try:
        if has_assembly_objects(fixture):
            # FreeCAD: /home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp
            # ::AssemblyObject::execute() calls "solve(false)" when the user preference
            # "SolveOnRecompute" is true. The collector runs the solver explicitly below so
            # request-local oracle evidence sees the fixture's initial placements, not a prior
            # recompute writeback.
            assembly_solve_preferences = FreeCAD.ParamGet(
                "User parameter:BaseApp/Preferences/Mod/Assembly"
            )
            previous_solve_on_recompute = assembly_solve_preferences.GetBool("SolveOnRecompute", True)
            assembly_solve_preferences.SetBool("SolveOnRecompute", False)
        created = create_objects(FreeCAD, doc, fixture)
        doc.recompute()
        if assembly_solve_preferences is not None:
            assembly_solve_preferences.SetBool("SolveOnRecompute", previous_solve_on_recompute)
            assembly_solve_preferences = None

        targets = list(requested_targets) if requested_targets is not None else target_names(fixture)
        object_payloads: dict[str, dict] = {}
        for name in targets:
            obj = created.get(name)
            if obj is None:
                raise UnsupportedFixture(f"target object {name} was not created")
            object_payloads[name] = object_expected_payload(obj, fixture, created)

        if ACTIVE_TOPO_NAMING_STATE is not None:
            ledger_payloads = topo_ledger_payloads(fixture, created, object_payloads)
            return topo_naming_state_response(
                fixture,
                FreeCAD,
                object_payloads,
                created,
                ledger_payloads=ledger_payloads,
            )

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
        bundled_offset_gap = bundled_offset_gap_metadata(payload)
        distance_type_gap = distance_type_gap_metadata(payload)
        if bundled_offset_gap is not None:
            payload.update(bundled_offset_gap)
        elif distance_type_gap is not None:
            payload.update(distance_type_gap)
        elif payload_requires_marker_parity(payload) and not solver_distance_types_from_payload(payload):
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
        if assembly_solve_preferences is not None:
            assembly_solve_preferences.SetBool("SolveOnRecompute", bool(previous_solve_on_recompute))
        close_document_with_producer_trace(FreeCAD, doc)


def close_enough(left: float, right: float, delta: float) -> bool:
    return abs(float(left) - float(right)) <= delta


def compare_bbox(existing: dict, generated: dict, delta: float) -> bool:
    for side in ("min", "max"):
        for expected_value, generated_value in zip(existing[side], generated[side]):
            if not close_enough(expected_value, generated_value, delta):
                return False
    return True


def compare_expected_value(existing: Any, generated: Any, delta: float) -> bool:
    if isinstance(existing, bool) or isinstance(generated, bool):
        return existing == generated
    if isinstance(existing, (int, float)) and isinstance(generated, (int, float)):
        return close_enough(existing, generated, delta)
    if isinstance(existing, list):
        if not isinstance(generated, list) or len(existing) != len(generated):
            return False
        return all(
            compare_expected_value(expected_item, generated_item, delta)
            for expected_item, generated_item in zip(existing, generated)
        )
    if isinstance(existing, dict):
        if not isinstance(generated, dict) or set(existing) != set(generated):
            return False
        return all(
            compare_expected_value(expected_value, generated[key], delta)
            for key, expected_value in existing.items()
        )
    return existing == generated


def canonicalize_freecad_mapped_names(value: Any) -> Any:
    if isinstance(value, str):
        return canonical_freecad_mapped_name(value)
    if isinstance(value, list):
        return [canonicalize_freecad_mapped_names(item) for item in value]
    if isinstance(value, dict):
        return {key: canonicalize_freecad_mapped_names(item) for key, item in value.items()}
    return value


def canonicalize_freecad_mapped_names_and_keys(value: Any) -> Any:
    if isinstance(value, str):
        canonical = canonical_freecad_mapped_name(value)
        return TOPO_INDEX_NAME_RE.sub(lambda match: f"{match.group(1)}*", canonical)
    if isinstance(value, list):
        return [canonicalize_freecad_mapped_names_and_keys(item) for item in value]
    if isinstance(value, dict):
        return {
            canonical_freecad_mapped_name(str(key)): canonicalize_freecad_mapped_names_and_keys(item)
            for key, item in value.items()
        }
    return value


def comparable_topo_naming_state(value: dict) -> dict:
    comparable = canonicalize_freecad_mapped_names_and_keys(value)
    if not isinstance(comparable, dict):
        return {}
    producer = comparable.get("producer")
    if isinstance(producer, dict):
        producer = dict(producer)
        producer["freecadVersion"] = "*"
        producer["occtVersion"] = "*"
        comparable = dict(comparable)
        comparable["producer"] = producer
    return comparable


def comparable_freecad_ledger(value: dict[str, Any]) -> dict[str, Any]:
    comparable = canonicalize_freecad_mapped_names(copy.deepcopy(value))
    if not isinstance(comparable, dict):
        return {}

    fixture = comparable.get("fixture")
    if isinstance(fixture, dict):
        fixture["expectedPayloadHash"] = "*"
        fixture["topoNamingStateHash"] = "*"

    round_trip = comparable.get("roundTrip")
    if isinstance(round_trip, dict):
        round_trip["inputTopoNamingStateHash"] = "*"

    objects = comparable.get("objects")
    if isinstance(objects, dict):
        for object_record in objects.values():
            if not isinstance(object_record, dict):
                continue
            for inventory_name in ("beforeElements", "afterElements"):
                inventory = object_record.get(inventory_name)
                if not isinstance(inventory, dict):
                    continue
                for bucket, values in inventory.items():
                    if isinstance(values, list):
                        inventory[bucket] = sorted(set(str(value) for value in values))

    events = comparable.get("events")
    if isinstance(events, list) and all(isinstance(event, dict) for event in events):
        comparable["events"] = sorted(events, key=lambda event: str(event.get("id") or ""))
    return comparable


def compare_topo_naming_state_expected(existing: dict, generated: dict) -> list[str]:
    existing_has_state = isinstance(existing.get("topoNamingState"), dict)
    generated_has_state = isinstance(generated.get("topoNamingState"), dict)
    if existing_has_state != generated_has_state:
        if generated_has_state:
            return ["topoNamingState:missing"]
        return ["topoNamingState:unexpected"]
    if not existing_has_state:
        return []
    if comparable_topo_naming_state(existing["topoNamingState"]) != comparable_topo_naming_state(
        generated["topoNamingState"]
    ):
        return ["topoNamingState"]
    return []


def reference_update_state_contract_errors(
    payload: dict,
    label: str,
    objects: dict[str, Any],
) -> list[str]:
    errors: list[str] = []
    reference_updates = payload.get("elementReferenceUpdates")
    if not isinstance(reference_updates, list):
        return errors

    for update_index, update in enumerate(reference_updates):
        if not isinstance(update, dict):
            errors.append(f"{label}:elementReferenceUpdates.{update_index}")
            continue
        property_type = update.get("PropertyType")
        items: list[tuple[str, dict[str, Any]]] = []
        if property_type in LINK_SUB_PROPERTY_TYPES:
            items.append((f"{label}:elementReferenceUpdates.{update_index}", update))
        elif property_type in LINK_SUB_LIST_PROPERTY_TYPES:
            sub_set = update.get("SubSet")
            if not isinstance(sub_set, list):
                errors.append(f"{label}:elementReferenceUpdates.{update_index}.SubSet")
                continue
            for item_index, item in enumerate(sub_set):
                if not isinstance(item, dict):
                    errors.append(f"{label}:elementReferenceUpdates.{update_index}.SubSet.{item_index}")
                    continue
                items.append((f"{label}:elementReferenceUpdates.{update_index}.SubSet.{item_index}", item))

        for context, item in items:
            stable_sub_list = item.get("StableSubList")
            if not isinstance(stable_sub_list, list) or not stable_sub_list:
                continue
            target_name = item.get("value")
            if not isinstance(target_name, str) or not target_name:
                errors.append(f"{context}.value")
                continue

            reference_shadows = item.get("ReferenceShadow")
            if isinstance(reference_shadows, list):
                for shadow_index, shadow in enumerate(reference_shadows):
                    if not isinstance(shadow, dict):
                        continue
                    shadow_target = shadow.get("target")
                    if isinstance(shadow_target, str) and shadow_target and shadow_target != target_name:
                        errors.append(f"{context}.ReferenceShadow.{shadow_index}.target")

            object_state = objects.get(target_name)
            if not isinstance(object_state, dict):
                errors.append(f"{context}.topoNamingState.objects.{target_name}.missing")
                continue
            for token_index, token in enumerate(stable_sub_list):
                if not isinstance(token, str) or not token:
                    errors.append(f"{context}.StableSubList.{token_index}")
                    continue
                if display_path_stable_token(token):
                    errors.append(f"{context}.StableSubList.{token_index}.display_path")
                    continue
                if not topo_state_object_has_stable_token(object_state, token):
                    errors.append(f"{context}.StableSubList.{token_index}.unresolved")
    return errors


def element_map_key_contract_errors(entries: Any, label: str, context: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(entries, dict):
        return errors
    seen_canonical: dict[str, str] = {}
    for token, element_entry in entries.items():
        if not isinstance(element_entry, dict):
            continue
        mapped_name = element_entry.get("mappedName")
        if not isinstance(mapped_name, dict):
            continue
        raw = mapped_name.get("raw")
        canonical = mapped_name.get("canonical")
        if isinstance(raw, str) and raw:
            raw_canonical = canonical_freecad_mapped_name(raw)
            if isinstance(canonical, str) and canonical and canonical != raw_canonical:
                errors.append(f"{label}:{context}.{token}.mappedName.canonical")
        if not isinstance(canonical, str) or not canonical:
            continue
        if str(token) != canonical:
            errors.append(f"{label}:{context}.{token}.key_canonical")
        previous = seen_canonical.get(canonical)
        if previous is not None and previous != str(token):
            errors.append(f"{label}:{context}.{token}.canonical_collision")
        else:
            seen_canonical[canonical] = str(token)
    return errors


def topo_naming_state_contract_errors(payload: dict, label: str) -> list[str]:
    errors: list[str] = []
    state = payload.get("topoNamingState")
    if not isinstance(state, dict):
        return reference_update_state_contract_errors(payload, label, {})
    errors.extend(stable_identity_contract_errors(payload, label))
    objects = state.get("objects")
    if not isinstance(objects, dict):
        errors.extend(reference_update_state_contract_errors(payload, label, {}))
        return errors

    for object_name, object_state in objects.items():
        if not isinstance(object_state, dict):
            continue
        subshapes = object_state.get("subshapes")
        if not isinstance(subshapes, dict):
            subshapes = {}
        for subshape_key, subshape_entry in subshapes.items():
            if not isinstance(subshape_entry, dict):
                errors.append(f"{label}:topoNamingState.{object_name}.subshapes.{subshape_key}")
                continue
            if subshape_entry.get("subname") != subshape_key:
                errors.append(
                    f"{label}:topoNamingState.{object_name}.subshapes.{subshape_key}.subname"
                )
            if not indexed_topo_subname(subshape_entry.get("subname")):
                errors.append(
                    f"{label}:topoNamingState.{object_name}.subshapes.{subshape_key}.subname.indexed"
                )

        element_map = object_state.get("elementMap")
        entries = element_map.get("entries") if isinstance(element_map, dict) else {}
        errors.extend(
            element_map_key_contract_errors(
                entries,
                label,
                f"topoNamingState.{object_name}.elementMap",
            )
        )
        if not isinstance(entries, dict):
            continue
        for token, element_entry in entries.items():
            if not isinstance(element_entry, dict):
                errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}")
                continue
            target = element_entry.get("target")
            if not isinstance(target, dict):
                errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.target")
                continue
            target_subname = target.get("subname")
            if not isinstance(target_subname, str) or not target_subname:
                errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.target.subname")
                continue
            if not indexed_topo_subname(target_subname):
                errors.append(
                    f"{label}:topoNamingState.{object_name}.elementMap.{token}.target.subname.indexed"
                )
            if target.get("object") == object_name and target_subname not in subshapes:
                errors.append(
                    f"{label}:topoNamingState.{object_name}.elementMap.{token}.target.subname.missing"
                )
        child_maps = object_state.get("childElementMaps")
        if isinstance(child_maps, list):
            for child_index, child_map in enumerate(child_maps):
                if not isinstance(child_map, dict):
                    continue
                child_entries = child_map.get("elementMap", {}).get("entries")
                child_key = child_map.get("key")
                child_context = (
                    f"topoNamingState.{object_name}.childElementMaps.{child_key}.elementMap"
                    if isinstance(child_key, str) and child_key
                    else f"topoNamingState.{object_name}.childElementMaps.{child_index}.elementMap"
                )
                errors.extend(element_map_key_contract_errors(child_entries, label, child_context))
    errors.extend(reference_update_state_contract_errors(payload, label, objects))
    return errors


def fixture_name_from_expected_path(path: Path) -> str:
    name = path.name
    if name.endswith(".freecad.json"):
        return name[: -len(".freecad.json")]
    return path.stem


def is_c4m6_expected_path(path: Path) -> bool:
    return len(path.parts) >= 3 and path.parent.name == "expected" and path.parent.parent.name == "c4m6"


def mapper_history_ids(object_state: dict[str, Any]) -> dict[str, dict[str, Any]]:
    events = object_state.get("mapperHistory")
    if not isinstance(events, list):
        return {}
    return {
        str(event["id"]): event
        for event in events
        if isinstance(event, dict) and isinstance(event.get("id"), str)
    }


def child_element_map_contract_errors(object_name: str, object_state: dict[str, Any], label: str) -> list[str]:
    errors: list[str] = []
    child_maps = object_state.get("childElementMaps")
    if not isinstance(child_maps, list) or not child_maps:
        errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.empty")
        return errors

    key_to_targets: dict[str, set[str]] = {}
    for index, child_map in enumerate(child_maps):
        if not isinstance(child_map, dict):
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{index}")
            continue
        key = child_map.get("key")
        if not isinstance(key, str) or not key:
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{index}.key")
            continue
        if child_map.get("ownerObject") != object_name:
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.ownerObject")
        if not isinstance(child_map.get("childObject"), str) or not child_map.get("childObject"):
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.childObject")
        path_prefix = child_map.get("pathPrefix")
        if not isinstance(path_prefix, str) or not path_prefix:
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.pathPrefix")
        targets = key_to_targets.setdefault(key, set())
        entries = child_map.get("elementMap", {}).get("entries")
        if not isinstance(entries, dict):
            errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.entries")
            continue
        for token, entry in entries.items():
            if not isinstance(entry, dict):
                errors.append(f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.entries.{token}")
                continue
            evidence = entry.get("evidence")
            child_key = evidence.get("childElementMapKey") if isinstance(evidence, dict) else None
            if child_key != key:
                errors.append(
                    f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.entries.{token}.evidence.childElementMapKey"
                )
            target = entry.get("target")
            target_subname = target.get("subname") if isinstance(target, dict) else None
            if not indexed_topo_subname(target_subname):
                errors.append(
                    f"{label}:topoNamingState.{object_name}.childElementMaps.{key}.entries.{token}.target.subname.indexed"
                )
                continue
            targets.add(str(target_subname))

    top_entries = object_state.get("elementMap", {}).get("entries")
    if not isinstance(top_entries, dict):
        return errors
    for token, entry in top_entries.items():
        if not isinstance(entry, dict):
            continue
        evidence = entry.get("evidence")
        child_key = evidence.get("childElementMapKey") if isinstance(evidence, dict) else None
        if child_key is None:
            continue
        if child_key not in key_to_targets:
            errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.evidence.childElementMapKey")
            continue
        target = entry.get("target")
        target_subname = target.get("subname") if isinstance(target, dict) else None
        if target_subname not in key_to_targets[child_key]:
            errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.target.subname.childElementMapKey")
    return errors


def mapper_history_contract_errors(object_name: str, object_state: dict[str, Any], label: str) -> list[str]:
    errors: list[str] = []
    events_by_id = mapper_history_ids(object_state)
    if not events_by_id:
        errors.append(f"{label}:topoNamingState.{object_name}.mapperHistory.empty")
        return errors

    entries = object_state.get("elementMap", {}).get("entries")
    if not isinstance(entries, dict):
        entries = {}
    entry_tokens = set(str(token) for token in entries)
    terminal_source_tokens: set[str] = set()
    for event_id, event in events_by_id.items():
        relation = event.get("relation")
        source = event.get("source")
        if relation in {"split", "deleted", "ambiguous"} and isinstance(source, dict):
            source_object = source.get("object")
            source_subname = source.get("subname")
            if isinstance(source_object, str) and isinstance(source_subname, str) and source_subname:
                terminal_source_tokens.add(f"{source_object}.{source_subname}")
        target = event.get("target")
        target_subname = target.get("subname") if isinstance(target, dict) else None
        if isinstance(target_subname, str) and target_subname and not indexed_topo_subname(target_subname):
            errors.append(f"{label}:topoNamingState.{object_name}.mapperHistory.{event_id}.target.subname.indexed")

    for token in terminal_source_tokens & entry_tokens:
        errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.terminal_mapper_history_entry")

    for token, entry in entries.items():
        if not isinstance(entry, dict):
            continue
        evidence = entry.get("evidence")
        mapper_ids = evidence.get("mapperHistoryIds") if isinstance(evidence, dict) else None
        if not mapper_ids:
            continue
        if not isinstance(mapper_ids, list):
            errors.append(f"{label}:topoNamingState.{object_name}.elementMap.{token}.evidence.mapperHistoryIds")
            continue
        for mapper_id in mapper_ids:
            event = events_by_id.get(str(mapper_id))
            if event is None:
                errors.append(
                    f"{label}:topoNamingState.{object_name}.elementMap.{token}.evidence.mapperHistoryIds.missing"
                )
                continue
            if event.get("relation") not in {"generated", "modified", "merge"}:
                errors.append(
                    f"{label}:topoNamingState.{object_name}.elementMap.{token}.evidence.mapperHistoryIds.terminal"
                )
    return errors


def c4m6_protocol_contract_errors(path: Path, payload: dict, label: str) -> list[str]:
    if not is_c4m6_expected_path(path):
        return []
    fixture_name = fixture_name_from_expected_path(path)
    errors: list[str] = []
    state = payload.get("topoNamingState")
    objects = state.get("objects") if isinstance(state, dict) else {}
    if not isinstance(objects, dict):
        objects = {}

    if fixture_name == "topo-state-reference-shadow-brep":
        if not payload.get("elementReferenceUpdates") and not payload.get("diagnostics"):
            errors.append(f"{label}:elementReferenceUpdates.reference_shadow_brep.empty")

    if fixture_name == "topo-state-link-compound-child-maps":
        if not objects:
            errors.append(f"{label}:topoNamingState.objects.empty")
        child_map_owners = [
            (str(object_name), object_state)
            for object_name, object_state in objects.items()
            if isinstance(object_state, dict) and object_state.get("childElementMaps")
        ]
        if not child_map_owners:
            errors.append(f"{label}:topoNamingState.childElementMaps.empty")
        for object_name, object_state in child_map_owners:
            errors.extend(child_element_map_contract_errors(object_name, object_state, label))

    if fixture_name == "topo-state-mapper-history-events":
        if not payload.get("diagnostics"):
            errors.append(f"{label}:diagnostics.mapperHistory.empty")
        if not objects:
            errors.append(f"{label}:topoNamingState.objects.empty")
        for object_name, object_state in objects.items():
            if isinstance(object_state, dict):
                errors.extend(mapper_history_contract_errors(str(object_name), object_state, label))

    return errors


def compare_response_contract(existing: dict, generated: dict) -> list[str]:
    errors: list[str] = []
    if existing.get("results") == [] and generated.get("results") != []:
        errors.append("results")
    if existing.get("diagnostics"):
        if existing.get("diagnostics") != generated.get("diagnostics"):
            errors.append("diagnostics")
    if existing.get("elementReferenceUpdates"):
        if existing.get("elementReferenceUpdates") != generated.get("elementReferenceUpdates"):
            errors.append("elementReferenceUpdates")
    return errors


def compare_object_expected(existing: dict, generated: dict) -> list[str]:
    errors: list[str] = []
    bbox_delta = existing.get("bbox_delta", 1e-6)
    if "shape_summary" in existing:
        generated_summary = generated.get("shape_summary", {})
        expected_summary = existing["shape_summary"]
        if expected_summary.get("shape") != generated_summary.get("shape"):
            errors.append("shape_summary.shape")
        if "bbox" in expected_summary and not compare_bbox(expected_summary["bbox"], generated_summary.get("bbox", {}), bbox_delta):
            errors.append("shape_summary.bbox")
        if "volume" in expected_summary and not close_enough(
            expected_summary["volume"],
            generated_summary.get("volume", 0.0),
            existing.get("volume_delta", 1e-6),
        ):
            errors.append("shape_summary.volume")
        if "topology_counts" in expected_summary and expected_summary["topology_counts"] != generated_summary.get("topology_counts"):
            errors.append("shape_summary.topology_counts")
    if "wrapper_oracle" in existing and existing["wrapper_oracle"] != generated.get("wrapper_oracle"):
        errors.append("wrapper_oracle")
    for key, expected_value in existing.get("object_fields", {}).items():
        if not compare_expected_value(
            expected_value,
            generated.get("object_fields", {}).get(key),
            existing.get("object_fields_delta", bbox_delta),
        ):
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
    if "subshapes" in existing and canonicalize_freecad_mapped_names(
        existing["subshapes"]
    ) != canonicalize_freecad_mapped_names(generated.get("subshapes")):
        errors.append("subshapes")
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


def result_objects(payload: dict) -> dict[str, dict]:
    if isinstance(payload.get("results"), list):
        return {
            str(item["object"]): item
            for item in payload["results"]
            if isinstance(item, dict) and "object" in item
        }
    if isinstance(payload.get("objects"), dict):
        return payload["objects"]
    if "object" in payload:
        return {str(payload["object"]): payload}
    return {}


def compare_json(path: Path, payload: dict) -> bool:
    if not path.exists():
        print(f"missing expected: {path}", file=sys.stderr)
        return False
    existing = json.loads(path.read_text(encoding="utf-8"))
    errors: list[str] = []
    errors.extend(topo_naming_state_contract_errors(existing, "existing"))
    errors.extend(topo_naming_state_contract_errors(payload, "generated"))
    errors.extend(c4m6_protocol_contract_errors(path, existing, "existing"))
    errors.extend(c4m6_protocol_contract_errors(path, payload, "generated"))
    errors.extend(compare_response_contract(existing, payload))
    errors.extend(compare_topo_naming_state_expected(existing, payload))
    existing_objects = result_objects(existing)
    generated_objects = result_objects(payload)
    if existing_objects:
        for object_name, object_expected in existing_objects.items():
            generated = generated_objects.get(object_name)
            if generated is None:
                errors.append(f"{object_name}:missing")
                continue
            errors.extend(f"{object_name}:{field}" for field in compare_object_expected(object_expected, generated))
    elif existing.get("object") != payload.get("object"):
        errors.append("object")
        errors.extend(compare_object_expected(existing, payload))
    if errors:
        print(f"expected differs: {path}: {', '.join(errors)}", file=sys.stderr)
        return False
    return True


def compare_ledger_json(path: Path, payload: dict[str, Any]) -> bool:
    if not path.exists():
        print(f"missing expected ledger: {path}", file=sys.stderr)
        return False
    existing = json.loads(path.read_text(encoding="utf-8"))
    if comparable_freecad_ledger(existing) != comparable_freecad_ledger(payload):
        print(f"expected ledger differs: {path}", file=sys.stderr)
        return False
    return True


def validate_generated_expected_ledger(
    fixture_path: Path,
    fixture: dict[str, Any],
    expected_payload: dict[str, Any],
    ledger: dict[str, Any],
) -> list[str]:
    from validate_freecad_expected_ledger import validate_expected_file  # type: ignore

    with tempfile.TemporaryDirectory(prefix="freecad-expected-ledger-") as temp_dir:
        phase_dir = Path(temp_dir) / fixture_path.parent.name
        expected_path = phase_dir / "expected" / f"{fixture_path.stem}.freecad.json"
        temp_fixture_path = phase_dir / fixture_path.name
        atomic_write_json(temp_fixture_path, fixture)
        atomic_write_json(expected_path, expected_payload)
        atomic_write_json(ledger_path_for_expected(expected_path), ledger)
        return validate_expected_file(expected_path, strict=True)


def expected_has_native_geometry_payload(path: Path) -> bool:
    expected = json.loads(path.read_text(encoding="utf-8"))
    return "object" in expected or "objects" in expected or "results" in expected


def expected_has_diagnostic_only_payload(path: Path) -> bool:
    expected = json.loads(path.read_text(encoding="utf-8"))
    return (
        "diagnostic_codes" in expected
        and "object" not in expected
        and "objects" not in expected
        and "results" not in expected
    )


def run_inside_freecad(args: argparse.Namespace) -> int:
    fixtures_root = Path(args.fixtures_root)
    failures = 0
    skipped = 0
    processed = 0
    paths = fixture_paths(args)
    if args.phase:
        for entry in fixture_role_catalog(args).skipped:
            role = entry["role"]
            reason = entry["reason"]
            if role == "protocol_only" or args.skip_unsupported:
                skipped += 1
                print(
                    f"skip {role} {entry['phase']}/{entry['case']}: {reason}",
                    file=sys.stderr,
                )
            else:
                failures += 1
                print(
                    f"unsupported {entry['phase']}/{entry['case']}: {reason}; pass --skip-unsupported to skip",
                    file=sys.stderr,
                )
    for fixture_path in paths:
        failures_before_fixture = failures
        out_path = Path(args.out) if args.out else expected_path_for_fixture(fixtures_root, fixture_path)
        fixture = load_fixture(fixture_path)
        target_override: list[str] | None = None
        if args.check:
            if args.phase and args.skip_unsupported and not out_path.exists():
                skipped += 1
                print(f"skip missing expected {fixture_path}", file=sys.stderr)
                continue
            if out_path.exists() and not expected_has_native_geometry_payload(out_path):
                if expected_has_diagnostic_only_payload(out_path):
                    print(f"diagnostic-only expected has no native geometry check: {out_path}", file=sys.stderr)
                    continue
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

        if LAST_FREECAD_PRODUCER_TRACE is None:
            print(f"failed producer trace {fixture_path}: native collector did not create a document trace", file=sys.stderr)
            failures += 1
            continue
        producer_trace = copy.deepcopy(LAST_FREECAD_PRODUCER_TRACE)

        try:
            ledger = collect_expected_ledger(fixture_path, fixture, payload, target_override)
        except Exception as exc:
            print(f"failed ledger {fixture_path}: {exc}", file=sys.stderr)
            print(traceback.format_exc(), file=sys.stderr)
            failures += 1
            continue

        if args.check:
            failures += 0 if compare_json(out_path, payload) else 1
            failures += 0 if compare_ledger_json(ledger_path_for_expected(out_path), ledger) else 1
            trace_path = producer_trace_path_for_expected(out_path)
            if not trace_path.exists():
                print(f"missing expected producer trace: {trace_path}", file=sys.stderr)
                failures += 1
            else:
                try:
                    validate_producer_trace(json.loads(trace_path.read_text(encoding="utf-8")))
                except Exception as exc:
                    print(f"invalid expected producer trace {trace_path}: {exc}", file=sys.stderr)
                    failures += 1
        else:
            atomic_write_json(out_path, payload)
            atomic_write_json(ledger_path_for_expected(out_path), ledger)
            atomic_write_json(producer_trace_path_for_expected(out_path), producer_trace)
        if args.validate_ledger:
            try:
                if args.check:
                    ledger_errors = validate_generated_expected_ledger(
                        fixture_path,
                        fixture,
                        payload,
                        ledger,
                    )
                else:
                    from validate_freecad_expected_ledger import validate_expected_file  # type: ignore

                    ledger_errors = validate_expected_file(out_path, strict=True)
            except Exception as exc:
                ledger_errors = [f"ledger validator failed: {exc}"]
            if ledger_errors:
                print(f"ledger validation failed: {out_path}", file=sys.stderr)
                for error in ledger_errors:
                    print(f"  - {error}", file=sys.stderr)
                failures += 1
        if args.pretty or (not args.out and not args.phase):
            print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        if failures == failures_before_fixture:
            processed += 1

    if args.phase:
        print(f"processed={processed} skipped={skipped} failed={failures}", file=sys.stderr)
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
