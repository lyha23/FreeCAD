#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import traceback
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_VERSION = "c12m10.copy-on-change-native-copied-graph.v1"
ENV_ARG_MARKER = "__c12m10_subshapebinder_copy_on_change_probe_args_env__"
ENV_ARG_NAME = "C12M10_SUBSHAPEBINDER_COPY_ON_CHANGE_PROBE_ARGS_JSON"
PROBE_PROP = "C12M10CopyOnChangeValue"


def default_freecadcmd() -> str:
    return (
        os.environ.get("FREECADCMD")
        or "/Users/li/.cargo/bin/freecadcmd"
        or shutil.which("freecadcmd")
        or shutil.which("FreeCADCmd")
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
        description="Collect C12-M10 SubShapeBinder CopyOnChange native copied graph evidence.",
    )
    parser.add_argument(
        "--raw-out",
        default=str(ROOT / "docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph.raw.freecad.json"),
    )
    parser.add_argument(
        "--gate-out",
        default=str(ROOT / "docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json"),
    )
    parser.add_argument("--freecadcmd", default=default_freecadcmd())
    parser.add_argument("--workdir", help="Keep intermediate FreeCAD files in this directory.")
    parser.add_argument("--keep-workdir", action="store_true")
    parser.add_argument("--pretty", action="store_true")
    return parser.parse_args(script_args(argv))


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def display_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(Path.cwd().resolve()))
    except ValueError:
        return str(path)


def freecad_version_string(FreeCAD: Any) -> tuple[str, str]:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        revision = str(version[3]).split()[0] if len(version) >= 4 else ""
        version_text = ".".join(str(item) for item in version[:3])
        return f"{version_text} revision {revision}".strip(), revision
    return str(version), ""


def config_get(FreeCAD: Any, name: str) -> str:
    try:
        return str(FreeCAD.ConfigGet(name))
    except Exception:
        return ""


def runtime_baseline(FreeCAD: Any, command: list[str], args: argparse.Namespace) -> dict[str, Any]:
    version, revision = freecad_version_string(FreeCAD)
    return {
        "freecadcmd": args.freecadcmd,
        "freecad_version": version,
        "freecad_revision": revision,
        "occt_version": config_get(FreeCAD, "OCC_VERSION"),
        "libpack_version": config_get(FreeCAD, "LibPackVersion"),
        "app_home_path": config_get(FreeCAD, "AppHomePath"),
        "platform": platform.platform(),
        "python_executable": sys.executable,
        "script_path": str(Path(__file__).resolve()),
        "cwd": str(Path.cwd()),
        "timestamp_utc": now_iso(),
        "command": {
            "argv": command,
            "returncode": 0,
        },
    }


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
            "Label": str(getattr(value, "Label", "")),
            "TypeId": str(getattr(value, "TypeId", "")),
            "ID": safe_object_id(value),
            "Document": str(getattr(getattr(value, "Document", None), "Name", "")),
        }
    return repr(value)


def safe_call(label: str, call: Callable[[], Any]) -> dict[str, Any]:
    try:
        return {"label": label, "status": "ok", "value": jsonable(call())}
    except Exception as exc:
        return {
            "label": label,
            "status": "error",
            "error_type": type(exc).__name__,
            "error": str(exc),
        }


def safe_object_id(obj: Any) -> Any:
    for attr in ("ID", "Id"):
        try:
            value = getattr(obj, attr)
            if value is not None:
                return int(value)
        except Exception:
            pass
    for method in ("getID",):
        try:
            return int(getattr(obj, method)())
        except Exception:
            pass
    return None


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
            "shells": len(getattr(shape, "Shells", [])),
            "faces": len(getattr(shape, "Faces", [])),
            "wires": len(getattr(shape, "Wires", [])),
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


def dynamic_property_names(obj: Any) -> list[str]:
    if not hasattr(obj, "getDynamicPropertyNames"):
        return []
    try:
        return [str(name) for name in obj.getDynamicPropertyNames()]
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
                "id": safe_object_id(target),
                "document": str(getattr(getattr(target, "Document", None), "Name", "")),
                "subnames": [str(sub) for sub in subs],
            })
        else:
            entries.append({
                "object": str(getattr(item, "Name", item)),
                "label": str(getattr(item, "Label", "")),
                "id": safe_object_id(item),
                "document": str(getattr(getattr(item, "Document", None), "Name", "")),
                "subnames": [],
            })
    return entries


def cache_properties(obj: Any) -> dict[str, Any]:
    payload: dict[str, Any] = {}
    names = sorted(set(dynamic_property_names(obj)) | set(property_names(obj)))
    for name in names:
        if name.startswith("Cache_"):
            payload[name] = property_status(obj, name)
    return payload


def copied_link_snapshot(obj: Any) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "hasattr": hasattr(obj, "_CopiedLink"),
        "property": property_status(obj, "_CopiedLink"),
    }
    try:
        value = getattr(obj, "_CopiedLink")
        payload["value"] = jsonable(value)
        if isinstance(value, tuple) and len(value) == 2:
            target, subs = value
            payload["target"] = jsonable(target)
            payload["subvalues"] = [str(sub) for sub in subs]
    except Exception as exc:
        payload["value"] = {"status": "error", "error": str(exc)}
    return payload


def hidden_state_snapshot(obj: Any) -> dict[str, Any]:
    payload: dict[str, Any] = {}
    for name in ("_CopiedLink", "_CopiedObjs"):
        if name == "_CopiedLink":
            payload[name] = copied_link_snapshot(obj)
            continue
        hidden: dict[str, Any] = {"hasattr": hasattr(obj, name), "property": property_status(obj, name)}
        try:
            hidden["value"] = jsonable(getattr(obj, name))
        except Exception as exc:
            hidden["value"] = {"status": "error", "error": str(exc)}
        payload[name] = hidden
    return payload


def shape_element_map_probe(shape: Any) -> dict[str, Any]:
    if shape is None:
        return {"status": "missing_shape"}
    payload: dict[str, Any] = {
        "python_type": type(shape).__name__,
        "attributes": {},
        "method_attempts": {},
    }
    for attr in ("Hasher", "Tag", "ElementMap", "ElementMapSize", "NamedShape"):
        payload["attributes"][attr] = safe_call(f"shape.{attr}", lambda attr=attr: getattr(shape, attr))
    for method in ("getElementMapSize", "getElementName", "getElement", "countElement"):
        if not hasattr(shape, method):
            payload["method_attempts"][method] = {"status": "unavailable"}
            continue
        if method == "getElementName":
            payload["method_attempts"][method] = safe_call(method, lambda method=method: getattr(shape, method)("Face1"))
        elif method == "getElement":
            payload["method_attempts"][method] = safe_call(method, lambda method=method: getattr(shape, method)("Face1"))
        elif method == "countElement":
            payload["method_attempts"][method] = safe_call(method, lambda method=method: getattr(shape, method)("Face"))
        else:
            payload["method_attempts"][method] = safe_call(method, lambda method=method: getattr(shape, method)())
    return payload


def object_links(obj: Any) -> dict[str, Any]:
    def names(items: Any) -> list[str]:
        out: list[str] = []
        try:
            for item in list(items):
                out.append(str(getattr(item, "Name", item)))
        except Exception:
            pass
        return out

    return {
        "in_list": names(getattr(obj, "InList", [])),
        "out_list": names(getattr(obj, "OutList", [])),
    }


def object_state(obj: Any, role: str) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "role": role,
        "name": str(getattr(obj, "Name", "")),
        "label": str(getattr(obj, "Label", "")),
        "id": safe_object_id(obj),
        "type_id": str(getattr(obj, "TypeId", "")),
        "document": str(getattr(getattr(obj, "Document", None), "Name", "")),
        "links": object_links(obj),
        "properties": {},
        "dynamic_properties": dynamic_property_names(obj),
        "copy_on_change_properties": {},
        "cache_properties": cache_properties(obj),
        "hidden_state": hidden_state_snapshot(obj),
    }
    for name in ("BindCopyOnChange", "BindMode", "PartialLoad", "Support"):
        if hasattr(obj, name):
            value = support_snapshot(obj) if name == "Support" else getattr(obj, name)
            payload["properties"][name] = jsonable(value)
    for name in copy_on_change_property_names(obj):
        payload["copy_on_change_properties"][name] = property_status(obj, name)
    if hasattr(obj, "Shape"):
        try:
            shape = getattr(obj, "Shape")
            payload["shape_summary"] = shape_summary(shape)
            payload["shape_element_map_probe"] = shape_element_map_probe(shape)
        except Exception as exc:
            payload["shape_summary"] = {"status": "error", "error": str(exc)}
    return payload


def document_names(FreeCAD: Any) -> list[str]:
    try:
        return sorted(str(name) for name in FreeCAD.listDocuments().keys())
    except Exception:
        return []


def document_snapshot(FreeCAD: Any) -> dict[str, Any]:
    payload: dict[str, Any] = {}
    try:
        documents = FreeCAD.listDocuments()
    except Exception as exc:
        return {"status": "error", "error": str(exc)}
    for name, doc in documents.items():
        objects = []
        try:
            for obj in doc.Objects:
                objects.append({
                    "name": str(getattr(obj, "Name", "")),
                    "label": str(getattr(obj, "Label", "")),
                    "id": safe_object_id(obj),
                    "type_id": str(getattr(obj, "TypeId", "")),
                    "links": object_links(obj),
                    "dynamic_properties": dynamic_property_names(obj),
                    "copy_on_change_properties": copy_on_change_property_names(obj),
                    "cache_properties": sorted(cache_properties(obj).keys()),
                    "shape_summary": shape_summary(getattr(obj, "Shape", None)) if hasattr(obj, "Shape") else None,
                })
        except Exception as exc:
            objects.append({"status": "error", "error": str(exc)})
        payload[str(name)] = {
            "object_count": len(objects),
            "object_order": [item.get("name") for item in objects],
            "objects": objects,
        }
    return payload


def set_copy_on_change_status(obj: Any, prop_name: str) -> dict[str, Any]:
    attempts = [
        safe_call("setPropertyStatus(name, CopyOnChange, True)", lambda: obj.setPropertyStatus(prop_name, "CopyOnChange", True)),
        safe_call("setPropertyStatus(name, CopyOnChange)", lambda: obj.setPropertyStatus(prop_name, "CopyOnChange")),
        safe_call("setPropertyStatus(name, [CopyOnChange])", lambda: obj.setPropertyStatus(prop_name, ["CopyOnChange"])),
    ]
    return {
        "attempts": attempts,
        "final_status": property_status(obj, prop_name),
    }


def add_source_copy_on_change_property(source: Any) -> dict[str, Any]:
    evidence: dict[str, Any] = {}
    evidence["add_property"] = safe_call(
        "addProperty(App::PropertyFloat)",
        lambda: source.addProperty("App::PropertyFloat", PROBE_PROP, "C12M10", "C12-M10 CopyOnChange probe property"),
    )
    evidence["initial_value"] = safe_call(f"set {PROBE_PROP}=11.0", lambda: setattr(source, PROBE_PROP, 11.0))
    evidence["copy_on_change_status"] = set_copy_on_change_status(source, PROBE_PROP)
    evidence["source_copy_on_change_properties"] = {
        name: property_status(source, name) for name in copy_on_change_property_names(source)
    }
    return evidence


def recompute_payload(doc: Any, label: str = "doc.recompute") -> dict[str, Any]:
    try:
        value = doc.recompute()
        return {"label": label, "status": "ok", "value": jsonable(value)}
    except Exception as exc:
        return {
            "label": label,
            "status": "error",
            "error_type": type(exc).__name__,
            "error": str(exc),
            "traceback": traceback.format_exc(),
        }


def assign_support(obj: Any, target: Any, subnames: list[str] | None = None) -> dict[str, Any]:
    if subnames:
        attempts = [
            safe_call("Support=[(target, subnames)]", lambda: setattr(obj, "Support", [(target, subnames)])),
            safe_call("Support=[(target, tuple(subnames))]", lambda: setattr(obj, "Support", [(target, tuple(subnames))])),
        ]
    else:
        attempts = [
            safe_call("Support=[target]", lambda: setattr(obj, "Support", [target])),
            safe_call("Support=[(target, [])]", lambda: setattr(obj, "Support", [(target, [])])),
        ]
    return {"attempts": attempts, "result": support_snapshot(obj)}


def make_box(FreeCAD: Any, doc: Any, name: str, placement: tuple[float, float, float] = (0, 0, 0)) -> Any:
    box = doc.addObject("Part::Box", name)
    box.Length = 10
    box.Width = 10
    box.Height = 10
    try:
        box.Placement.Base = FreeCAD.Vector(*placement)
    except Exception:
        pass
    return box


def make_dependency_source(FreeCAD: Any, doc: Any) -> dict[str, Any]:
    base = make_box(FreeCAD, doc, "SourceBase", (0, 0, 0))
    tool = make_box(FreeCAD, doc, "SourceTool", (3, 3, -1))
    cut = doc.addObject("Part::Cut", "SourceCut")
    cut.Base = base
    cut.Tool = tool
    second = make_box(FreeCAD, doc, "SecondSupport", (15, 0, 0))
    setup = {
        "source_property_setup": add_source_copy_on_change_property(cut),
        "initial_recompute": recompute_payload(doc, "source graph recompute"),
    }
    return {"base": base, "tool": tool, "source": cut, "second": second, "setup": setup}


def add_binder(doc: Any, name: str, support: Any | None, mode: str | None, subnames: list[str] | None = None) -> dict[str, Any]:
    binder = doc.addObject("PartDesign::SubShapeBinder", name)
    setup: dict[str, Any] = {"name": name}
    if support is not None:
        setup["support_assignment"] = assign_support(binder, support, subnames)
    if mode is not None:
        setup["bind_copy_on_change_assignment"] = safe_call(f"BindCopyOnChange={mode}", lambda: setattr(binder, "BindCopyOnChange", mode))
    return {"object": binder, "setup": setup}


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
            payload["allow_partial_methods"][method] = safe_call(method, lambda method=method: getattr(candidate, method)())
    return payload


def dependency_order_observability(doc: Any, source: Any, tmp_snapshot: dict[str, Any]) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "source_out_list": object_links(source).get("out_list", []),
        "source_in_list": object_links(source).get("in_list", []),
        "method_attempts": {},
        "tmp_document_object_order": tmp_snapshot.get("_tmp_binder", {}).get("object_order", []),
        "note": "Document.cpp::copyObject uses getDependencyList(... DepNoXLinked | DepSort) internally; Python probe records public doc order only.",
    }
    for method in ("getDependencyList", "copyObject"):
        if not hasattr(doc, method):
            payload["method_attempts"][method] = {"status": "unavailable"}
            continue
        if method == "getDependencyList":
            payload["method_attempts"][method] = safe_call(method, lambda method=method: getattr(doc, method)([source]))
        else:
            payload["method_attempts"][method] = {"status": "skipped", "reason": "Do not call doc.copyObject from the probe outside the FreeCAD SubShapeBinder lifecycle."}
    return payload


def copied_object_method_attempts(copied: Any | None) -> dict[str, Any]:
    if copied is None:
        return {"status": "missing_copied_object"}
    payload: dict[str, Any] = {}
    for method in ("recomputeFeature", "recompute", "touch", "enforceRecompute"):
        if not hasattr(copied, method):
            payload[method] = {"status": "unavailable"}
            continue
        if method == "recomputeFeature":
            payload[method] = safe_call("copied.recomputeFeature(True)", lambda method=method: getattr(copied, method)(True))
        else:
            payload[method] = safe_call(f"copied.{method}()", lambda method=method: getattr(copied, method)())
    return payload


def find_copied_link_target(obj: Any) -> Any | None:
    try:
        value = getattr(obj, "_CopiedLink")
        if isinstance(value, tuple) and value:
            return value[0]
        name = getattr(value, "Name", None)
        if name:
            return value
    except Exception:
        pass
    return None


def collect_probe_payload(FreeCAD: Any, args: argparse.Namespace, command: list[str]) -> dict[str, Any]:
    work_root = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="c12m10-copyonchange-probe-"))
    work_root.mkdir(parents=True, exist_ok=True)
    cleanup_workdir = not args.workdir and not args.keep_workdir
    doc = None
    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "artifact_kind": "raw_native_probe",
        "route": "native_probe_executed",
        "decision": "raw_evidence_collected_gate_required",
        "runtime_baseline": runtime_baseline(FreeCAD, command, args),
        "freecad_source_authority": [
            "src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()",
            "src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()",
            "src/Mod/PartDesign/App/ShapeBinder.h::_CopiedObjs/_CopiedLink/PartialLoad",
            "src/App/Document.cpp::Document::copyObject()",
            "src/App/Document.cpp::Document::recomputeFeature()",
        ],
        "probe_ids": [f"C12M10-PROBE-{idx:03d}" for idx in range(1, 12)],
        "observations": {},
    }
    if not cleanup_workdir:
        payload["workdir"] = str(work_root)
    try:
        doc = FreeCAD.newDocument("C12M10CopyOnChangeProbe")
        source_graph = make_dependency_source(FreeCAD, doc)
        source = source_graph["source"]
        second = source_graph["second"]
        payload["observations"]["source_graph"] = {
            "setup": source_graph["setup"],
            "objects": {
                "SourceBase": object_state(source_graph["base"], "source_dependency_base"),
                "SourceTool": object_state(source_graph["tool"], "source_dependency_tool"),
                "SourceCut": object_state(source, "source_dependency_result"),
                "SecondSupport": object_state(second, "second_support"),
            },
        }

        binders: dict[str, Any] = {}
        setups: dict[str, Any] = {}
        for name, mode in (
            ("ModeDisabled", "Disabled"),
            ("ModeEnabled", "Enabled"),
            ("ModeMutationTrigger", "Enabled"),
            ("ModeExplicitMutated", "Mutated"),
        ):
            entry = add_binder(doc, name, source, mode, ["Face1"])
            binders[name] = entry["object"]
            setups[name] = entry["setup"]
        partial_entry = add_binder(doc, "PartialLoadTrue", source, "Disabled", ["Face1"])
        partial = partial_entry["object"]
        partial.PartialLoad = True
        binders["PartialLoadTrue"] = partial
        setups["PartialLoadTrue"] = partial_entry["setup"]

        zero_entry = add_binder(doc, "SupportGateZero", None, "Enabled")
        one_entry = add_binder(doc, "SupportGateOne", source, "Enabled", ["Face1"])
        multi = doc.addObject("PartDesign::SubShapeBinder", "SupportGateMulti")
        multi_setup = {
            "support_assignment": safe_call("Support=[source, second]", lambda: setattr(multi, "Support", [source, second])),
            "bind_copy_on_change_assignment": safe_call("BindCopyOnChange=Enabled", lambda: setattr(multi, "BindCopyOnChange", "Enabled")),
        }
        binders["SupportGateZero"] = zero_entry["object"]
        binders["SupportGateOne"] = one_entry["object"]
        binders["SupportGateMulti"] = multi
        setups["SupportGateZero"] = zero_entry["setup"]
        setups["SupportGateOne"] = one_entry["setup"]
        setups["SupportGateMulti"] = multi_setup

        save_path = work_root / "c12m10-copyonchange-probe.FCStd"
        payload["observations"]["save_before_recompute"] = safe_call("doc.saveAs", lambda: doc.saveAs(str(save_path)))
        if not cleanup_workdir:
            payload["saved_fcstd"] = str(save_path)
        payload["observations"]["binder_setup"] = setups
        payload["observations"]["documents_before_recompute"] = document_names(FreeCAD)
        payload["observations"]["initial_recompute"] = recompute_payload(doc, "initial doc.recompute")
        payload["observations"]["documents_after_initial_recompute"] = document_names(FreeCAD)
        payload["observations"]["document_snapshot_after_initial_recompute"] = document_snapshot(FreeCAD)
        payload["observations"]["mode_matrix_after_initial_recompute"] = {
            name: object_state(obj, f"{name}_after_initial_recompute") for name, obj in binders.items()
        }

        mutation: dict[str, Any] = {"before": object_state(binders["ModeMutationTrigger"], "mutation_before_assignment")}
        if hasattr(binders["ModeMutationTrigger"], PROBE_PROP):
            mutation["assignment"] = safe_call(f"set {PROBE_PROP}=17.0", lambda: setattr(binders["ModeMutationTrigger"], PROBE_PROP, 17.0))
            mutation["after_assignment"] = object_state(binders["ModeMutationTrigger"], "mutation_after_assignment")
            mutation["recompute"] = recompute_payload(doc, "mutation doc.recompute")
            mutation["after_recompute"] = object_state(binders["ModeMutationTrigger"], "mutation_after_recompute")
            mutation["documents_after_recompute"] = document_names(FreeCAD)
            mutation["document_snapshot_after_recompute"] = document_snapshot(FreeCAD)
        else:
            mutation["assignment"] = {
                "status": "unavailable",
                "reason": f"Binder did not expose dynamic {PROBE_PROP}; mutation-triggered Mutated cannot be observed from Python.",
            }
        payload["observations"]["mutation_trigger"] = mutation

        tmp_snapshot = mutation.get("document_snapshot_after_recompute") or payload["observations"]["document_snapshot_after_initial_recompute"]
        copied_target = find_copied_link_target(binders["ModeExplicitMutated"]) or find_copied_link_target(binders["ModeMutationTrigger"])
        payload["observations"]["copied_identity_attempts"] = {
            "private_copied_objs": {
                name: object_state(obj, f"{name}_copied_objs_probe")["hidden_state"]["_CopiedObjs"]
                for name, obj in binders.items()
            },
            "copied_link_targets": {
                name: object_state(obj, f"{name}_copied_link_probe")["hidden_state"]["_CopiedLink"]
                for name, obj in binders.items()
            },
            "tmp_document_objects": tmp_snapshot.get("_tmp_binder", {}),
        }
        payload["observations"]["dependency_order_attempts"] = dependency_order_observability(doc, source, tmp_snapshot)
        payload["observations"]["support_rewrite_attempts"] = {
            name: {
                "support_property": support_snapshot(obj),
                "copied_link": copied_link_snapshot(obj),
            }
            for name, obj in binders.items()
        }
        payload["observations"]["recompute_lifecycle_attempts"] = {
            "doc_recompute_results": {
                "initial": payload["observations"]["initial_recompute"],
                "mutation": mutation.get("recompute"),
            },
            "copied_object_method_attempts": copied_object_method_attempts(copied_target),
        }
        payload["observations"]["elementmap_namedshape_attempts"] = {
            "source_shape": shape_element_map_probe(getattr(source, "Shape", None)),
            "copied_shape": shape_element_map_probe(getattr(copied_target, "Shape", None) if copied_target else None),
            "binder_shape": shape_element_map_probe(getattr(binders["ModeExplicitMutated"], "Shape", None)),
        }
        payload["observations"]["partial_load_boundary"] = {
            "object": object_state(partial, "partial_load_true"),
            "support_property": support_property_observability(partial),
            "can_load_partial_attempt": safe_call("partial.canLoadPartial()", lambda: partial.canLoadPartial()) if hasattr(partial, "canLoadPartial") else {"status": "unavailable"},
        }

        cache_entry = add_binder(doc, "CacheBoundary", source, "Disabled", ["Face1"])
        cache_binder = cache_entry["object"]
        cache_initial = recompute_payload(doc, "cache initial recompute")
        cache_after_initial = cache_properties(cache_binder)
        cache_second = recompute_payload(doc, "cache hit recompute")
        cache_after_second = cache_properties(cache_binder)
        cache_reassign = assign_support(cache_binder, second, ["Face1"])
        cache_after_reassign_recompute = recompute_payload(doc, "cache support rewrite recompute")
        cache_after_reassign = cache_properties(cache_binder)
        payload["observations"]["cache_star_boundary"] = {
            "setup": cache_entry["setup"],
            "initial_recompute": cache_initial,
            "cache_after_initial": cache_after_initial,
            "second_recompute": cache_second,
            "cache_after_second": cache_after_second,
            "support_reassign": cache_reassign,
            "after_reassign_recompute": cache_after_reassign_recompute,
            "cache_after_reassign": cache_after_reassign,
            "decision_hint": "Cache_* entries are dynamic PropertyMatrix transform caches, not copied graph semantic state.",
        }
        payload["observed_fields_not_accepted_as_copied_graph_evidence"] = [
            "Python-visible BindCopyOnChange property state",
            "Python-visible PartialLoad property state",
            "object labels and names",
            "bbox and shape topology counts",
            "temporary document names and object order",
            "_CopiedLink target without source-to-copy rewrite map",
            "Cache_* dynamic matrix property presence",
        ]
        payload["unobservable_core_fields"] = [
            "_CopiedObjs private vector contents and stable stored order",
            "Document::copyObject internal dependency list with DepNoXLinked | DepSort",
            "exportObjects/importObjects source-to-imported mapping",
            "first and second SubShapeBinder copied->recomputeFeature(true) lifecycle calls",
            "per-stage ElementMap and NamedShape lifecycle before/after copied recompute",
        ]
        return payload
    except Exception as exc:
        payload.update({
            "route": "native_oracle_blocked",
            "decision": "native_oracle_blocked_retained",
            "blocker_layer": "probe_exception",
            "reason": str(exc),
            "traceback": traceback.format_exc(),
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


def blocked_raw_payload(args: argparse.Namespace, layer: str, reason: str, command: list[str], returncode: int, stdout: str = "", stderr: str = "") -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "artifact_kind": "raw_native_probe",
        "route": "native_oracle_blocked",
        "decision": "native_oracle_blocked_retained",
        "blocker_layer": layer,
        "reason": reason,
        "runtime_baseline": {
            "freecadcmd": args.freecadcmd,
            "freecad_version": "unavailable",
            "freecad_revision": "unavailable",
            "occt_version": "unavailable",
            "platform": platform.platform(),
            "script_path": str(Path(__file__).resolve()),
            "cwd": str(Path.cwd()),
            "timestamp_utc": now_iso(),
            "command": {
                "argv": command,
                "returncode": returncode,
                "stdout_tail": stdout[-4000:],
                "stderr_tail": stderr[-4000:],
            },
        },
        "probe_ids": [f"C12M10-PROBE-{idx:03d}" for idx in range(1, 12)],
        "unobservable_core_fields": [
            "all native copied graph fields unavailable because FreeCADCmd did not complete",
        ],
    }


def status_value(node: Any, *path: str) -> Any:
    cur = node
    for item in path:
        if not isinstance(cur, dict):
            return None
        cur = cur.get(item)
    return cur


def make_probe_results(raw: dict[str, Any], raw_path: Path) -> dict[str, Any]:
    artifact = display_path(raw_path)
    if raw.get("route") == "native_oracle_blocked":
        return {
            f"C12M10-PROBE-{idx:03d}": {
                "observed_status": "native_probe_blocked",
                "decision": "native_oracle_blocker",
                "artifact_or_note": f"{artifact}; FreeCADCmd/probe blocked before native evidence collection.",
            }
            for idx in range(1, 12)
        }

    observations = raw.get("observations", {})
    copied_private = status_value(observations, "copied_identity_attempts", "private_copied_objs") or {}
    copied_objs_observable = any(
        item.get("hasattr") and not isinstance(item.get("value"), dict)
        for item in copied_private.values()
        if isinstance(item, dict)
    )
    dependency_method = status_value(observations, "dependency_order_attempts", "method_attempts", "getDependencyList", "status")
    recompute_method_attempts = status_value(observations, "recompute_lifecycle_attempts", "copied_object_method_attempts") or {}
    recompute_internal_observed = recompute_method_attempts.get("recomputeFeature", {}).get("status") == "ok"
    element_map_attempts = status_value(observations, "elementmap_namedshape_attempts", "copied_shape", "method_attempts") or {}
    element_map_observed = element_map_attempts.get("getElementMapSize", {}).get("status") == "ok"
    cache_props = status_value(observations, "cache_star_boundary", "cache_after_initial") or {}

    return {
        "C12M10-PROBE-001": {
            "observed_status": "runtime_baseline_observed",
            "decision": "accepted_baseline",
            "artifact_or_note": f"{artifact}; runtime_baseline records FreeCADCmd path, version, OCCT/LibPack when exposed, platform, cwd, script and timestamp.",
        },
        "C12M10-PROBE-002": {
            "observed_status": "mode_matrix_python_visible_state_observed",
            "decision": "property_session_state_only",
            "artifact_or_note": f"{artifact}; Disabled, Enabled, mutation-triggered, explicit Mutated and PartialLoad cases are recorded, but this alone is not copied graph evidence.",
        },
        "C12M10-PROBE-003": {
            "observed_status": "zero_one_multi_support_gate_observed",
            "decision": "source_gate_confirmed_but_not_graph_ready",
            "artifact_or_note": f"{artifact}; zero/one/multi support binders record CopyOnChange dynamic property visibility and cleanup behavior.",
        },
        "C12M10-PROBE-004": {
            "observed_status": "tmp_binder_document_and_object_order_observed",
            "decision": "insufficient_session_document_evidence",
            "artifact_or_note": f"{artifact}; _tmp_binder document/object order is visible but remains native session state, not request-local copied graph payload.",
        },
        "C12M10-PROBE-005": {
            "observed_status": "copied_link_and_tmp_doc_objects_observed_private_copiedobjs_missing",
            "decision": "blocker",
            "artifact_or_note": f"{artifact}; _CopiedObjs private vector observable={copied_objs_observable}; stable stored copied identity/order is not exported.",
        },
        "C12M10-PROBE-006": {
            "observed_status": "tmp_doc_order_or_public_dependency_attempt_only",
            "decision": "blocker",
            "artifact_or_note": f"{artifact}; Python getDependencyList status={dependency_method}; Document::copyObject DepSort list and source-to-imported mapping remain unexported.",
        },
        "C12M10-PROBE-007": {
            "observed_status": "copied_link_target_and_support_values_observed",
            "decision": "insufficient_without_rewrite_map",
            "artifact_or_note": f"{artifact}; _CopiedLink target/subvalues are visible, but no deterministic source-to-copy support rewrite map is exported.",
        },
        "C12M10-PROBE-008": {
            "observed_status": "doc_recompute_and_copied_method_attempts_observed",
            "decision": "blocker",
            "artifact_or_note": f"{artifact}; copied recomputeFeature method observed={recompute_internal_observed}; first/second internal lifecycle calls are not exported.",
        },
        "C12M10-PROBE-009": {
            "observed_status": "shape_elementmap_method_attempts_observed",
            "decision": "blocker",
            "artifact_or_note": f"{artifact}; copied ElementMap/NamedShape lifecycle observed={element_map_observed}; per-stage lifecycle is not exported.",
        },
        "C12M10-PROBE-010": {
            "observed_status": "partialload_property_state_observed",
            "decision": "property_boundary_only",
            "artifact_or_note": f"{artifact}; PartialLoad and Support property observability recorded, but copied graph lifecycle remains blocked.",
        },
        "C12M10-PROBE-011": {
            "observed_status": "cache_star_dynamic_matrix_observed" if cache_props else "cache_star_attempt_recorded",
            "decision": "optimization_not_backend_state",
            "artifact_or_note": f"{artifact}; Cache_* is observed as dynamic PropertyMatrix cache when present and stays outside persistent backend state.",
        },
    }


def make_gate_payload(raw: dict[str, Any], raw_path: Path) -> dict[str, Any]:
    probe_results = make_probe_results(raw, raw_path)
    blocker_ids = [
        key
        for key, result in probe_results.items()
        if result["decision"] in {"blocker", "native_oracle_blocker"}
    ]
    missing_core = raw.get("unobservable_core_fields", [])
    decision = "native_copied_graph_evidence_ready" if not blocker_ids else "native_oracle_blocked_retained"
    return {
        "schema_version": SCHEMA_VERSION,
        "artifact_kind": "evidence_gate",
        "decision": decision,
        "raw_probe_artifact": display_path(raw_path),
        "runtime_baseline": raw.get("runtime_baseline", {}),
        "probe_results": probe_results,
        "blocker_probe_ids": blocker_ids,
        "missing_core_fields": missing_core,
        "observed_fields_not_accepted_as_copied_graph_evidence": raw.get("observed_fields_not_accepted_as_copied_graph_evidence", []),
        "delete_condition": (
            "Replace this retained blocker only after native evidence exposes stable _CopiedObjs identities, "
            "Document::copyObject dependency order and mapping, support rewrite map, recomputeFeature(true) "
            "lifecycle, and ElementMap/NamedShape lifecycle as request-local graph evidence."
        ),
        "next_steps": (
            "S3 must inherit native_oracle_blocked_retained unless a stronger native/C++ probe is added; "
            "do not approve DTO or implementation from property/session evidence alone."
        ),
    }


def patch_command_result(path: Path, returncode: int, command: list[str], stdout: str, stderr: str, args: argparse.Namespace) -> dict[str, Any]:
    if path.exists():
        payload = json.loads(path.read_text(encoding="utf-8"))
    else:
        payload = blocked_raw_payload(args, "freecadcmd", "FreeCADCmd did not produce native evidence JSON", command, returncode, stdout, stderr)
    payload.setdefault("runtime_baseline", {})
    payload["runtime_baseline"].setdefault("command", {})
    payload["runtime_baseline"]["freecadcmd"] = args.freecadcmd
    payload["runtime_baseline"]["command"]["argv"] = command
    payload["runtime_baseline"]["command"]["returncode"] = returncode
    if returncode != 0 and stdout:
        payload["runtime_baseline"]["command"]["stdout_tail"] = stdout[-4000:]
    if returncode != 0 and stderr:
        payload["runtime_baseline"]["command"]["stderr_tail"] = stderr[-4000:]
    atomic_write_json(path, payload)
    return payload


def run_inside_freecad(args: argparse.Namespace, command: list[str]) -> int:
    import FreeCAD  # type: ignore

    raw_path = Path(args.raw_out)
    payload = collect_probe_payload(FreeCAD, args, command)
    atomic_write_json(raw_path, payload)
    if args.pretty:
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if payload.get("route") != "native_oracle_blocked" else 1


def run_via_freecadcmd(raw_argv: list[str], args: argparse.Namespace) -> int:
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(script_args(raw_argv), ensure_ascii=False)
    command = [args.freecadcmd, str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    raw_path = Path(args.raw_out)
    gate_path = Path(args.gate_out)
    try:
        result = subprocess.run(command, cwd=Path.cwd(), env=env, capture_output=True, text=True)
    except FileNotFoundError as exc:
        raw = blocked_raw_payload(args, "freecadcmd_missing", str(exc), command, 127)
        atomic_write_json(raw_path, raw)
        atomic_write_json(gate_path, make_gate_payload(raw, raw_path))
        return 127
    raw = patch_command_result(raw_path, result.returncode, command, result.stdout, result.stderr, args)
    gate = make_gate_payload(raw, raw_path)
    atomic_write_json(gate_path, gate)
    if args.pretty:
        print(gate_path.read_text(encoding="utf-8"))
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
