#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = "cad-core.c8m2-subshapebinder-copyonchange-native-probe.v1"
ENV_ARG_MARKER = "__cad_core_c8m2_subshapebinder_copyonchange_probe_args_env__"
ENV_ARG_NAME = "CAD_CORE_C8M2_SUBSHAPEBINDER_COPYONCHANGE_PROBE_ARGS_JSON"
PROBE_PROP = "C8M2CopyOnChangeValue"


def default_freecadcmd() -> str:
    return (
        os.environ.get("FREECADCMD")
        or shutil.which("freecadcmd")
        or shutil.which("FreeCADCmd")
        or shutil.which("freecadcmd-daily")
        or "freecadcmd"
    )


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
        description="Probe FreeCAD native SubShapeBinder BindCopyOnChange lifecycle for C8-M2.",
    )
    parser.add_argument(
        "fixture",
        nargs="?",
        default=str(ROOT / "fixtures/c8m2/subshape-binder-copyonchange-lifecycle-probe.json"),
    )
    parser.add_argument(
        "--out",
        default=str(ROOT / "fixtures/c8m2/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json"),
    )
    parser.add_argument("--freecadcmd", default=default_freecadcmd())
    parser.add_argument("--workdir", help="Keep intermediate FreeCAD files in this directory.")
    parser.add_argument("--keep-workdir", action="store_true")
    parser.add_argument("--pretty", action="store_true")
    return parser.parse_args(script_args(argv))


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def load_fixture(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def display_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(Path.cwd().resolve()))
    except ValueError:
        return str(path)


def freecad_version_string(FreeCAD: Any) -> tuple[str, str]:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        text_parts = [str(item) for item in version if item]
        revision = str(version[3]).split()[0] if len(version) >= 4 else ""
        version_text = ".".join(str(item) for item in version[:3])
        return f"{version_text} revision {revision}".strip(), revision
    return str(version), ""


def jsonable(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, (list, tuple)):
        return [jsonable(item) for item in value]
    if isinstance(value, dict):
        return {str(key): jsonable(item) for key, item in value.items()}
    name = getattr(value, "Name", None) or getattr(value, "Label", None)
    if name:
        return {
            "Name": str(name),
            "TypeId": str(getattr(value, "TypeId", "")),
        }
    return repr(value)


def shape_summary(shape: Any) -> dict[str, Any]:
    if shape is None:
        return {"status": "missing_shape"}
    try:
        if shape.isNull():
            return {"status": "null_shape"}
    except Exception:
        pass
    try:
        bbox = shape.optimalBoundingBox()
    except Exception:
        bbox = getattr(shape, "BoundBox", None)
    payload: dict[str, Any] = {
        "status": "ok",
        "shape_type": str(getattr(shape, "ShapeType", "")),
        "area": float(getattr(shape, "Area", 0.0)),
        "volume": float(getattr(shape, "Volume", 0.0)),
        "topology_counts": {
            "solids": len(getattr(shape, "Solids", [])),
            "faces": len(getattr(shape, "Faces", [])),
            "edges": len(getattr(shape, "Edges", [])),
            "vertices": len(getattr(shape, "Vertexes", [])),
        },
    }
    if bbox is not None:
        payload["bbox"] = {
            "min": [float(bbox.XMin), float(bbox.YMin), float(bbox.ZMin)],
            "max": [float(bbox.XMax), float(bbox.YMax), float(bbox.ZMax)],
        }
    return payload


def property_names(obj: Any) -> list[str]:
    try:
        return [str(name) for name in getattr(obj, "PropertiesList")]
    except Exception:
        return []


def property_status(obj: Any, name: str) -> dict[str, Any]:
    payload: dict[str, Any] = {}
    for method_name, call in (
        ("getPropertyStatus", lambda: obj.getPropertyStatus(name)),
        ("getTypeIdOfProperty", lambda: obj.getTypeIdOfProperty(name)),
        ("getGroupOfProperty", lambda: obj.getGroupOfProperty(name)),
    ):
        if not hasattr(obj, method_name):
            payload[method_name] = {"status": "unavailable"}
            continue
        try:
            payload[method_name] = {"status": "ok", "value": jsonable(call())}
        except Exception as exc:
            payload[method_name] = {"status": "error", "error": str(exc)}
    try:
        payload["value"] = jsonable(getattr(obj, name))
    except Exception as exc:
        payload["value"] = {"status": "error", "error": str(exc)}
    return payload


def copy_on_change_property_names(obj: Any) -> list[str]:
    names: list[str] = []
    for name in property_names(obj):
        status = property_status(obj, name).get("getPropertyStatus", {}).get("value", [])
        group = property_status(obj, name).get("getGroupOfProperty", {}).get("value", "")
        if "CopyOnChange" in status or str(group).startswith("_CopyOnChange("):
            names.append(name)
    return names


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


def object_state(obj: Any, role: str) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "role": role,
        "name": str(getattr(obj, "Name", "")),
        "label": str(getattr(obj, "Label", "")),
        "type_id": str(getattr(obj, "TypeId", "")),
        "properties": {},
        "copy_on_change_properties": {},
        "hidden_cache_fields": {},
    }
    for name in ("BindCopyOnChange", "BindMode", "PartialLoad", "Support"):
        if hasattr(obj, name):
            value = support_snapshot(obj) if name == "Support" else getattr(obj, name)
            payload["properties"][name] = jsonable(value)
    for name in copy_on_change_property_names(obj):
        payload["copy_on_change_properties"][name] = property_status(obj, name)
    for name in ("_CopiedLink", "_CopiedObjs"):
        hidden: dict[str, Any] = {"hasattr": hasattr(obj, name)}
        try:
            hidden["value"] = jsonable(getattr(obj, name))
        except Exception as exc:
            hidden["value"] = {"status": "error", "error": str(exc)}
        try:
            hidden["property"] = property_status(obj, name)
        except Exception as exc:
            hidden["property"] = {"status": "error", "error": str(exc)}
        payload["hidden_cache_fields"][name] = hidden
    if hasattr(obj, "Shape"):
        try:
            payload["shape_summary"] = shape_summary(getattr(obj, "Shape"))
        except Exception as exc:
            payload["shape_summary"] = {"status": "error", "error": str(exc)}
    return payload


def try_call(label: str, call: Callable[[], Any]) -> dict[str, Any]:
    try:
        value = call()
        return {"label": label, "status": "ok", "value": jsonable(value)}
    except Exception as exc:
        return {
            "label": label,
            "status": "error",
            "error_type": type(exc).__name__,
            "error": str(exc),
        }


def set_copy_on_change_status(obj: Any, prop_name: str) -> dict[str, Any]:
    attempts = [
        try_call("setPropertyStatus(name, CopyOnChange, True)", lambda: obj.setPropertyStatus(prop_name, "CopyOnChange", True)),
        try_call("setPropertyStatus(name, CopyOnChange)", lambda: obj.setPropertyStatus(prop_name, "CopyOnChange")),
        try_call("setPropertyStatus(name, [CopyOnChange])", lambda: obj.setPropertyStatus(prop_name, ["CopyOnChange"])),
    ]
    return {
        "attempts": attempts,
        "final_status": property_status(obj, prop_name),
    }


def add_source_copy_on_change_property(source: Any) -> dict[str, Any]:
    evidence: dict[str, Any] = {}
    evidence["add_property"] = try_call(
        "addProperty(App::PropertyFloat)",
        lambda: source.addProperty("App::PropertyFloat", PROBE_PROP, "C8M2", "C8-M2 CopyOnChange probe property"),
    )
    try:
        setattr(source, PROBE_PROP, 11.0)
        evidence["initial_value"] = 11.0
    except Exception as exc:
        evidence["initial_value"] = {"status": "error", "error": str(exc)}
    evidence["copy_on_change_status"] = set_copy_on_change_status(source, PROBE_PROP)
    evidence["source_copy_on_change_properties"] = {
        name: property_status(source, name) for name in copy_on_change_property_names(source)
    }
    return evidence


def recompute_payload(doc: Any) -> dict[str, Any]:
    try:
        doc.recompute()
        return {"status": "ok"}
    except Exception as exc:
        return {
            "status": "error",
            "error_type": type(exc).__name__,
            "error": str(exc),
            "traceback": traceback.format_exc(),
        }


def support_property_observability(obj: Any) -> dict[str, Any]:
    support_value = getattr(obj, "Support", None)
    payload: dict[str, Any] = {
        "python_value": jsonable(support_value),
        "property_api": property_status(obj, "Support"),
        "allow_partial_methods": {},
    }
    candidate = None
    try:
        candidate = obj.getPropertyByName("Support")
    except Exception as exc:
        payload["getPropertyByName"] = {"status": "error", "error": str(exc)}
    if candidate is not None:
        payload["getPropertyByName"] = {
            "status": "ok",
            "type": type(candidate).__name__,
            "value": jsonable(candidate),
        }
        for method in ("getAllowPartial", "allowPartial", "isAllowedPartial"):
            if not hasattr(candidate, method):
                payload["allow_partial_methods"][method] = {"status": "unavailable"}
                continue
            payload["allow_partial_methods"][method] = try_call(method, lambda method=method: getattr(candidate, method)())
    return payload


def document_names(FreeCAD: Any) -> list[str]:
    try:
        return sorted(str(name) for name in FreeCAD.listDocuments().keys())
    except Exception:
        return []


def make_box(doc: Any, name: str) -> Any:
    box = doc.addObject("Part::Box", name)
    box.Length = 10
    box.Width = 10
    box.Height = 10
    return box


def collect_probe_payload(FreeCAD: Any, args: argparse.Namespace, command: list[str]) -> dict[str, Any]:
    fixture_path = Path(args.fixture)
    fixture = load_fixture(fixture_path)
    version, revision = freecad_version_string(FreeCAD)
    work_root = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="c8m2-copyonchange-probe-"))
    work_root.mkdir(parents=True, exist_ok=True)
    cleanup_workdir = not args.workdir and not args.keep_workdir
    doc = None
    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "route": "native_evidence_collected_with_known_gap_blocker",
        "freecad_version": version,
        "freecad_revision": revision,
        "source_fixture": display_path(fixture_path),
        "source_fixture_name": fixture_path.name,
        "fixture_group": fixture.get("fixture_group", "c8m2"),
        "native_case": fixture.get("c8m2_case"),
        "oracle_ids": fixture.get("oracle_ids", []),
        "freecad_authority": fixture.get("freecad_authority", []),
        "command": {
            "argv": command,
            "returncode": 0,
        },
        "observed_fields": [],
        "unobservable_fields": [],
        "oracle_results": {},
    }
    if not cleanup_workdir:
        payload["workdir"] = str(work_root)
    try:
        doc = FreeCAD.newDocument("C8M2CopyOnChangeProbe")
        source = make_box(doc, "SupportBox")
        payload["source_property_setup"] = add_source_copy_on_change_property(source)

        disabled = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeDisabled")
        disabled.Support = [source]
        disabled.BindCopyOnChange = "Disabled"

        enabled = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeEnabled")
        enabled.Support = [source]
        enabled.BindCopyOnChange = "Enabled"

        partial = doc.addObject("PartDesign::SubShapeBinder", "PartialLoadEnabled")
        partial.Support = [source]
        partial.PartialLoad = True

        mutating = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeMutationTrigger")
        mutating.Support = [source]
        mutating.BindCopyOnChange = "Enabled"

        explicit_mutated = doc.addObject("PartDesign::SubShapeBinder", "CopyOnChangeExplicitMutated")
        explicit_mutated.Support = [source]
        explicit_mutated.BindCopyOnChange = "Mutated"

        save_path = work_root / "c8m2-copyonchange-probe.FCStd"
        payload["save_before_recompute"] = try_call("doc.saveAs", lambda: doc.saveAs(str(save_path)))
        if not cleanup_workdir:
            payload["saved_fcstd"] = str(save_path)

        payload["documents_before_recompute"] = document_names(FreeCAD)
        payload["initial_recompute"] = recompute_payload(doc)
        payload["documents_after_initial_recompute"] = document_names(FreeCAD)
        payload["initial_objects"] = {
            "SupportBox": object_state(source, "source_support"),
            "CopyOnChangeDisabled": object_state(disabled, "copy_on_change_disabled"),
            "CopyOnChangeEnabled": object_state(enabled, "copy_on_change_enabled"),
            "PartialLoadEnabled": object_state(partial, "partial_load_enabled"),
            "CopyOnChangeMutationTrigger": object_state(mutating, "copy_on_change_enabled_before_mutation"),
            "CopyOnChangeExplicitMutated": object_state(explicit_mutated, "copy_on_change_explicit_mutated"),
        }

        mutation: dict[str, Any] = {
            "before": object_state(mutating, "copy_on_change_enabled_before_assignment"),
        }
        if hasattr(mutating, PROBE_PROP):
            mutation["assignment"] = try_call(f"set {PROBE_PROP}=17.0", lambda: setattr(mutating, PROBE_PROP, 17.0))
            mutation["after_assignment"] = object_state(mutating, "copy_on_change_after_assignment")
            mutation["recompute"] = recompute_payload(doc)
            mutation["after_recompute"] = object_state(mutating, "copy_on_change_after_recompute")
            mutation["documents_after_recompute"] = document_names(FreeCAD)
        else:
            mutation["assignment"] = {
                "status": "unavailable",
                "reason": f"Binder did not expose dynamic {PROBE_PROP}; CopyOnChange property divergence cannot be triggered from Python.",
            }
        payload["mutation_trigger"] = mutation

        payload["partialload_observability"] = {
            "object": object_state(partial, "partial_load_enabled"),
            "support_property": support_property_observability(partial),
        }

        payload["oracle_results"]["C8M2-ORACLE-101"] = {
            "route": "native_property_state_collected",
            "observed_fields": [
                "BindCopyOnChange Disabled Python-visible state",
                "BindCopyOnChange Enabled Python-visible state",
                "BindCopyOnChange Mutated Python-visible state",
                "dynamic CopyOnChange property names when source exposes CopyOnChange property status",
                "mutation-triggered BindCopyOnChange state when Python-visible dynamic property is writable",
            ],
            "unobservable_fields": [
                "_CopiedObjs private vector contents",
                "copyObject dependency mapping behind temporary document cache",
            ],
        }
        payload["oracle_results"]["C8M2-ORACLE-102"] = {
            "route": "native_partialload_property_state_collected",
            "observed_fields": [
                "PartialLoad=True Python-visible property state",
                "Support PropertyXLinkSubList Python-visible value",
            ],
            "unobservable_fields": [
                "Support.setAllowPartial internal flag when no Python getAllowPartial-style method is exposed",
            ],
        }
        payload["oracle_results"]["C8M2-ORACLE-103"] = {
            "route": "oracle_blocked",
            "observed_fields": [
                "BindCopyOnChange Mutated Python-visible state",
                "_CopiedLink hidden property value if exposed by Python",
                "temporary document names visible through FreeCAD.listDocuments() during this probe",
            ],
            "unobservable_fields": [
                "_CopiedObjs private vector payload",
                "_tmp_binder copyObject object graph and dependency order",
                "recomputeFeature(true) internal copied-object ElementMap lifecycle",
            ],
            "reason": (
                "FreeCAD native probing can expose property state and some hidden-link/document names, "
                "but it still does not produce a stable stateless request-local payload for the full "
                "temporary-document copied-object cache."
            ),
        }
        payload["observed_fields"] = sorted({
            field
            for result in payload["oracle_results"].values()
            for field in result.get("observed_fields", [])
        })
        payload["unobservable_fields"] = sorted({
            field
            for result in payload["oracle_results"].values()
            for field in result.get("unobservable_fields", [])
        })
        payload["known_gap"] = {
            "kind": "c8m2_copy_on_change_full_temporary_document_cache_blocker",
            "route": "oracle_blocked",
            "reason": payload["oracle_results"]["C8M2-ORACLE-103"]["reason"],
            "source_fixture": display_path(fixture_path),
            "freecadcmd_evidence": {
                "freecad_version": version,
                "freecad_revision": revision,
                "observed_fields": payload["observed_fields"],
                "unobservable_fields": payload["unobservable_fields"],
                "documents_after_initial_recompute": payload.get("documents_after_initial_recompute", []),
                "documents_after_mutation_recompute": mutation.get("documents_after_recompute", []),
            },
            "delete_condition": (
                "Delete this blocker only after a native probe exposes a stable request-local DTO for "
                "_CopiedObjs/_tmp_binder/copyObject/recomputeFeature that does not require a backend "
                "session or persistent temporary document cache, and S6 explicitly opens implementation."
            ),
            "reopen_condition": (
                "Reopen S3 if FreeCADCmd or a native C++ probe can export copied-object cache contents "
                "and mutation lifecycle as deterministic request-local evidence."
            ),
        }
        return payload
    except Exception as exc:
        payload.update({
            "route": "native_oracle_blocked",
            "blocker_layer": "probe_exception",
            "reason": str(exc),
            "traceback": traceback.format_exc(),
            "delete_condition": "Fix the C8-M2 native probe or FreeCAD runtime and rerun S3.",
            "reopen_condition": "Reopen when FreeCADCmd can execute the native probe and write evidence JSON.",
        })
        return payload
    finally:
        if doc is not None:
            try:
                FreeCAD.closeDocument(doc.Name)
            except Exception:
                pass
        if cleanup_workdir:
            shutil.rmtree(work_root, ignore_errors=True)


def blocked_payload(args: argparse.Namespace, layer: str, reason: str, command: list[str], returncode: int, stdout: str = "", stderr: str = "") -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "route": "native_oracle_blocked",
        "blocker_layer": layer,
        "reason": reason,
        "freecad_version": "unavailable",
        "freecad_revision": "unavailable",
        "source_fixture": display_path(Path(args.fixture)),
        "source_fixture_name": Path(args.fixture).name,
        "fixture_group": "c8m2",
        "native_case": "subshape-binder-copyonchange-lifecycle-probe",
        "oracle_ids": ["C8M2-ORACLE-101", "C8M2-ORACLE-102", "C8M2-ORACLE-103"],
        "command": {
            "argv": command,
            "returncode": returncode,
            "stdout_tail": stdout[-4000:],
            "stderr_tail": stderr[-4000:],
        },
        "observed_fields": [],
        "unobservable_fields": [
            "BindCopyOnChange Python-visible state",
            "PartialLoad allow-partial Python-visible state",
            "_CopiedObjs/_tmp_binder/copyObject/recomputeFeature lifecycle",
        ],
        "known_gap": {
            "kind": "c8m2_native_oracle_execution_blocked",
            "route": "native_oracle_blocked",
            "reason": reason,
            "freecadcmd_evidence": {
                "command": command,
                "returncode": returncode,
                "stdout_tail": stdout[-4000:],
                "stderr_tail": stderr[-4000:],
            },
            "delete_condition": "Rerun S3 after FreeCADCmd can execute this probe and write native evidence JSON.",
            "reopen_condition": "Reopen when the local FreeCADCmd/Qt runtime becomes available or manual native evidence is collected.",
        },
        "delete_condition": "Rerun S3 after FreeCADCmd can execute this probe and write native evidence JSON.",
        "reopen_condition": "Reopen when FreeCADCmd or manual native collection is available.",
    }


def patch_command_result(path: Path, returncode: int, command: list[str], stdout: str, stderr: str, args: argparse.Namespace) -> None:
    if path.exists():
        payload = json.loads(path.read_text(encoding="utf-8"))
    else:
        payload = blocked_payload(args, "freecadcmd", "FreeCADCmd did not produce native evidence JSON", command, returncode, stdout, stderr)
    payload.setdefault("command", {})
    payload["command"]["argv"] = command
    payload["command"]["returncode"] = returncode
    if returncode != 0 and stdout:
        payload["command"]["stdout_tail"] = stdout[-4000:]
    if returncode != 0 and stderr:
        payload["command"]["stderr_tail"] = stderr[-4000:]
    atomic_write_json(path, payload)


def run_inside_freecad(args: argparse.Namespace, command: list[str]) -> int:
    import FreeCAD  # type: ignore

    out_path = Path(args.out)
    payload = collect_probe_payload(FreeCAD, args, command)
    atomic_write_json(out_path, payload)
    if args.pretty:
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if payload.get("route") != "native_oracle_blocked" else 1


def run_via_freecadcmd(raw_argv: list[str], args: argparse.Namespace) -> int:
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(script_args(raw_argv), ensure_ascii=False)
    command = [args.freecadcmd, str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    try:
        result = subprocess.run(command, cwd=Path.cwd(), env=env, capture_output=True, text=True)
    except FileNotFoundError as exc:
        payload = blocked_payload(args, "freecadcmd_missing", str(exc), command, 127)
        atomic_write_json(Path(args.out), payload)
        return 127
    patch_command_result(Path(args.out), result.returncode, command, result.stdout, result.stderr, args)
    if args.pretty and Path(args.out).exists():
        print(Path(args.out).read_text(encoding="utf-8"))
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args = parse_args(raw_argv)
    try:
        import FreeCAD  # type: ignore # noqa: F401
    except ImportError:
        return run_via_freecadcmd(raw_argv, args)
    return run_inside_freecad(args, [sys.executable, str(Path(__file__).resolve()), *raw_argv])


if __name__ == "__main__" or invoked_by_freecad_cli_import():
    raise SystemExit(main())
