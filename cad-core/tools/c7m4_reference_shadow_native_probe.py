#!/usr/bin/env python3
from __future__ import annotations

import argparse
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
import zipfile
from pathlib import Path
from typing import Any
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FREECADCMD = "/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd"
ENV_ARG_MARKER = "__cad_core_c7m4_reference_shadow_probe_args_env__"
ENV_ARG_NAME = "CAD_CORE_C7M4_REFERENCE_SHADOW_PROBE_ARGS_JSON"
SCHEMA_VERSION = "cad-core.c7m4-reference-shadow-native-probe.v1"


class ProbeBlocked(RuntimeError):
    def __init__(self, layer: str, message: str) -> None:
        super().__init__(message)
        self.layer = layer


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
        description="Probe FreeCAD native PropertyLinkSub ShadowSub restore for C7-M4.",
    )
    parser.add_argument(
        "fixture",
        nargs="?",
        default=str(ROOT / "fixtures/c3m5/dressup-reference-shadow-base-recovery.json"),
    )
    parser.add_argument(
        "--out",
        default=str(ROOT / "fixtures/c3m5/dressup-reference-shadow-base-recovery.native-probe.evidence.json"),
    )
    parser.add_argument("--freecadcmd", default=os.environ.get("FREECADCMD", DEFAULT_FREECADCMD))
    parser.add_argument("--workdir", help="Keep intermediate FCStd files in this directory.")
    parser.add_argument("--keep-workdir", action="store_true")
    return parser.parse_args(script_args(argv))


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(path)


def object_name(value: Any) -> str | None:
    name = getattr(value, "Name", None) or getattr(value, "Label", None)
    return str(name) if name else None


def jsonable(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, (list, tuple)):
        return [jsonable(item) for item in value]
    if isinstance(value, dict):
        return {str(key): jsonable(item) for key, item in value.items()}
    name = object_name(value)
    if name is not None:
        return {
            "Name": name,
            "TypeId": str(getattr(value, "TypeId", "")),
        }
    payload: dict[str, Any] = {}
    for attr in ("newName", "oldName"):
        if hasattr(value, attr):
            payload[attr] = str(getattr(value, attr))
    if payload:
        return payload
    return repr(value)


def read_fixture_contract(fixture: dict[str, Any]) -> dict[str, Any]:
    for spec in fixture.get("Objects", []):
        if spec.get("Name") != "Chamfer":
            continue
        base = spec.get("Properties", {}).get("Base", {})
        shadow_sub = base.get("ShadowSub", [])
        reference_shadow = base.get("ReferenceShadow", [])
        return {
            "target_object": base.get("value"),
            "stale_sublist": base.get("SubList", []),
            "stable_sublist": base.get("StableSubList", []),
            "shadow_sub": shadow_sub,
            "reference_shadow": reference_shadow,
            "old_subname": shadow_sub[0].get("oldName") if shadow_sub else "OldFilletEdge1",
            "new_subname": shadow_sub[0].get("newName") if shadow_sub else "Edge1",
        }
    raise ProbeBlocked("fixture", "fixture has no Chamfer.Base ReferenceShadow contract")


def attrs(element: ElementTree.Element | None) -> dict[str, str]:
    return dict(element.attrib) if element is not None else {}


def find_chamfer_base_property(root: ElementTree.Element) -> tuple[ElementTree.Element, ElementTree.Element, ElementTree.Element]:
    best: tuple[ElementTree.Element, ElementTree.Element, ElementTree.Element] | None = None
    for prop in root.iter("Property"):
        if prop.get("name") != "Base":
            continue
        link = prop.find("LinkSub")
        if link is not None:
            sub = link.find("Sub")
            if link.get("value") == "Fillet" and sub is not None:
                return prop, link, sub
            if best is None and sub is not None:
                best = (prop, link, sub)
        xlink = prop.find("XLink")
        if xlink is not None:
            if xlink.get("name") == "Fillet":
                return prop, xlink, xlink
            if best is None:
                best = (prop, xlink, xlink)
    if best is None:
        raise ProbeBlocked("xml_patch", "could not locate Chamfer.Base LinkSub/XLink property in Document.xml")
    return best


def patch_fcstd_document_xml(
    baseline_fcstd: Path,
    patched_fcstd: Path,
    old_subname: str,
    new_subname: str,
) -> dict[str, Any]:
    with zipfile.ZipFile(baseline_fcstd, "r") as source:
        entries = {info.filename: (info, source.read(info.filename)) for info in source.infolist()}
    if "Document.xml" not in entries:
        raise ProbeBlocked("xml_patch", "baseline FCStd has no Document.xml")

    root = ElementTree.fromstring(entries["Document.xml"][1])
    prop, link, sub = find_chamfer_base_property(root)
    tag = link.tag
    before = {
        "property_attrs": attrs(prop),
        "tag": tag,
        "link_attrs": attrs(link),
        "sub_attrs": attrs(sub),
    }
    if tag == "LinkSub":
        sub.set("value", old_subname)
        sub.set("shadow", new_subname)
        sub.attrib.pop("shadowed", None)
        link.set("count", "1")
    elif tag == "XLink":
        link.set("sub", old_subname)
        link.set("shadow", new_subname)
        link.attrib.pop("shadowed", None)
    else:
        raise ProbeBlocked("xml_patch", f"unsupported Base property tag {tag}")
    after = {
        "property_attrs": attrs(prop),
        "tag": tag,
        "link_attrs": attrs(link),
        "sub_attrs": attrs(sub),
    }

    document_xml = ElementTree.tostring(root, encoding="utf-8", xml_declaration=True)
    with zipfile.ZipFile(patched_fcstd, "w") as target:
        for filename, (info, data) in entries.items():
            target.writestr(info, document_xml if filename == "Document.xml" else data)
    return {
        "property_tag": tag,
        "before": before,
        "after": after,
    }


def call_candidate_methods(chamfer: Any) -> dict[str, Any]:
    candidates: list[dict[str, Any]] = []
    for label, getter in (
        ("attribute_Base", lambda: getattr(chamfer, "Base", None)),
        ("getPropertyByName_Base", lambda: chamfer.getPropertyByName("Base")),
    ):
        try:
            candidate = getter()
        except Exception as exc:
            candidates.append({"source": label, "status": "error", "error": str(exc)})
            continue
        entry: dict[str, Any] = {
            "source": label,
            "type": type(candidate).__name__,
            "repr": repr(candidate),
            "value": jsonable(candidate),
        }
        for method_name, method_args in (
            ("getSubValues", ()),
            ("getSubValues_false", (False,)),
            ("getSubValues_true", (True,)),
            ("getShadowSubs", ()),
        ):
            actual_name = "getSubValues" if method_name.startswith("getSubValues") else method_name
            if not hasattr(candidate, actual_name):
                entry[method_name] = {"status": "unavailable"}
                continue
            try:
                entry[method_name] = {
                    "status": "ok",
                    "value": jsonable(getattr(candidate, actual_name)(*method_args)),
                }
            except Exception as exc:
                entry[method_name] = {"status": "error", "error": str(exc)}
        candidates.append(entry)
    return {
        "candidates": candidates,
        "shadow_subs": first_success(candidates, "getShadowSubs"),
        "subvalues_default": first_success(candidates, "getSubValues"),
        "subvalues_false": first_success(candidates, "getSubValues_false"),
        "subvalues_true": first_success(candidates, "getSubValues_true"),
    }


def decode_property_dump(data: bytes) -> dict[str, Any]:
    try:
        with zipfile.ZipFile(io.BytesIO(data), "r") as archive:
            names = archive.namelist()
            payload: dict[str, Any] = {"entries": names}
            if "Persistence.xml" in names:
                xml_bytes = archive.read("Persistence.xml")
                payload["persistence_xml"] = xml_bytes.decode("utf-8", errors="replace")
                try:
                    root = ElementTree.fromstring(xml_bytes)
                    link = root.find("LinkSub") or root.find("XLink")
                    if link is not None:
                        sub = link.find("Sub")
                        payload["property_tag"] = link.tag
                        payload["link_attrs"] = attrs(link)
                        payload["sub_attrs"] = attrs(sub)
                except ElementTree.ParseError as exc:
                    payload["xml_parse_error"] = str(exc)
            return payload
    except Exception as exc:
        return {"decode_status": "error", "error": str(exc)}


def property_container_evidence(chamfer: Any) -> dict[str, Any]:
    evidence: dict[str, Any] = {}
    try:
        evidence["type_id"] = chamfer.getTypeIdOfProperty("Base")
    except Exception as exc:
        evidence["type_id"] = {"status": "error", "error": str(exc)}
    try:
        dumped = bytes(chamfer.dumpPropertyContent("Base", Compression=0))
        evidence["dumpPropertyContent"] = {
            "status": "ok",
            "compression": 0,
            "byte_count": len(dumped),
            "hex_prefix": dumped[:96].hex(),
            "decoded": decode_property_dump(dumped),
        }
    except Exception as exc:
        evidence["dumpPropertyContent"] = {"status": "error", "error": str(exc)}
    return evidence


def first_success(candidates: list[dict[str, Any]], field: str) -> dict[str, Any]:
    for candidate in candidates:
        value = candidate.get(field)
        if isinstance(value, dict) and value.get("status") == "ok":
            return value
    return {
        "status": "unavailable",
        "reason": f"FreeCAD Python property value does not expose {field}",
    }


def shape_payload(shape_summary: Any, obj: Any) -> dict[str, Any]:
    shape = getattr(obj, "Shape", None)
    if shape is None:
        return {"status": "missing_shape"}
    try:
        if shape.isNull():
            return {"status": "null_shape"}
        return {"status": "ok", "summary": shape_summary(shape)}
    except Exception as exc:
        return {"status": "error", "error": str(exc)}


def capture_state(chamfer: Any, body: Any, shape_summary: Any) -> dict[str, Any]:
    return {
        "Base": jsonable(getattr(chamfer, "Base", None)),
        "base_property_api": call_candidate_methods(chamfer),
        "base_property_container": property_container_evidence(chamfer),
        "Chamfer_shape": shape_payload(shape_summary, chamfer),
        "Body_shape": shape_payload(shape_summary, body),
    }


def has_expected_shadow_pair(shadow_payload: dict[str, Any], old_subname: str, new_subname: str) -> bool:
    if shadow_payload.get("status") != "ok":
        return False
    values = shadow_payload.get("value")
    if not isinstance(values, list):
        return False
    for item in values:
        if isinstance(item, dict) and item.get("oldName") == old_subname and item.get("newName") == new_subname:
            return True
    return False


def route_from_evidence(payload: dict[str, Any], old_subname: str, new_subname: str) -> str:
    after = payload.get("restore", {}).get("after_recompute", {})
    shadow_payload = after.get("base_property_api", {}).get("shadow_subs", {})
    if shadow_payload.get("status") != "ok":
        return "native_oracle_blocked"
    if not has_expected_shadow_pair(shadow_payload, old_subname, new_subname):
        return "native_not_supported"
    if after.get("Chamfer_shape", {}).get("status") != "ok" or after.get("Body_shape", {}).get("status") != "ok":
        return "native_not_supported"
    return "native_oracle_collected"


def make_blocked_payload(args: argparse.Namespace, layer: str, message: str, command: list[str], returncode: int) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "route": "native_oracle_blocked",
        "blocker_layer": layer,
        "reason": message,
        "command": {
            "argv": command,
            "returncode": returncode,
        },
        "delete_condition": (
            "Rerun C7-M4 S2 after FreeCADCmd, FCStd/XML restore, reopen, and "
            "Base.getShadowSubs observation are available; only then replace this blocker "
            "with native_oracle_collected or native_not_supported evidence."
        ),
    }


def run_inside_freecad(args: argparse.Namespace, command: list[str]) -> int:
    import FreeCAD  # type: ignore

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from collect_freecad_expected import create_objects, freecad_version, shape_summary  # type: ignore

    fixture_path = Path(args.fixture)
    out_path = Path(args.out)
    work_root = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="c7m4-reference-shadow-probe-"))
    work_root.mkdir(parents=True, exist_ok=True)
    cleanup_workdir = not args.workdir and not args.keep_workdir
    doc = None
    reopened = None
    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "route": "native_oracle_blocked",
        "fixture": str(fixture_path),
        "freecad_version": freecad_version(FreeCAD),
        "command": {
            "argv": command,
            "returncode": 0,
        },
        "negative_control": {
            "command": (
                "cd cad-core && FREECADCMD=/Users/li/.cargo/bin/freecadcmd "
                "python3 tools/collect_freecad_expected.py "
                "fixtures/c3m5/dressup-reference-shadow-base-recovery.json "
                "--out /tmp/c7m4-dressup-reference-shadow-stablesublist-fed.freecad.json"
            ),
            "role": "StableSubList-fed bypass guard only; not native oracle evidence",
        },
    }
    try:
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        contract = read_fixture_contract(fixture)
        old_subname = str(contract["old_subname"])
        new_subname = str(contract["new_subname"])
        payload["input_contract"] = contract
        if not cleanup_workdir:
            payload["workdir"] = str(work_root)

        doc = FreeCAD.newDocument("C7M4ReferenceShadowBaseline")
        created = create_objects(FreeCAD, doc, fixture)
        doc.recompute()
        payload["baseline"] = capture_state(created["Chamfer"], created["Body"], shape_summary)
        baseline_fcstd = work_root / "baseline.FCStd"
        patched_fcstd = work_root / "patched-reference-shadow.FCStd"
        doc.saveAs(str(baseline_fcstd))
        if not cleanup_workdir:
            payload["baseline"]["fcstd"] = str(baseline_fcstd)
        FreeCAD.closeDocument(doc.Name)
        doc = None

        payload["xml_patch"] = patch_fcstd_document_xml(
            baseline_fcstd,
            patched_fcstd,
            old_subname,
            new_subname,
        )
        if not cleanup_workdir:
            payload["patched_fcstd"] = str(patched_fcstd)

        reopened = FreeCAD.openDocument(str(patched_fcstd))
        chamfer = reopened.getObject("Chamfer")
        body = reopened.getObject("Body")
        if chamfer is None or body is None:
            raise ProbeBlocked("restore", "reopened document is missing Chamfer or Body")
        payload["restore"] = {
            "before_recompute": capture_state(chamfer, body, shape_summary),
        }
        try:
            reopened.recompute()
            payload["restore"]["recompute"] = {"status": "ok"}
        except Exception as exc:
            payload["restore"]["recompute"] = {
                "status": "error",
                "error": str(exc),
                "traceback": traceback.format_exc(),
            }
        payload["restore"]["after_recompute"] = capture_state(chamfer, body, shape_summary)
        payload["route"] = route_from_evidence(payload, old_subname, new_subname)
        if payload["route"] == "native_oracle_blocked":
            payload["blocker_layer"] = "python_property_api"
            payload["reason"] = (
                "FreeCADCmd reopened the patched FCStd, but the Python-visible Base property "
                "did not expose getShadowSubs(), so S2 cannot prove the native ShadowSub lifecycle."
            )
        elif payload["route"] == "native_not_supported":
            payload["reason"] = (
                "FreeCAD restored the patched document, but the expected ShadowSub pair or "
                "post-recompute Chamfer/Body shape evidence was absent."
            )
        else:
            payload["reason"] = (
                "FreeCAD restored oldName=OldFilletEdge1 with newName=Edge1 through the patched "
                "FCStd/XML LinkSub path and recompute produced Chamfer/Body shape summaries."
            )
        payload["delete_condition"] = (
            "Use this evidence in S3; do not replace it with StableSubList-fed collector output."
        )
        atomic_write_json(out_path, payload)
        return 0
    except ProbeBlocked as exc:
        payload.update({
            "route": "native_oracle_blocked",
            "blocker_layer": exc.layer,
            "reason": str(exc),
            "command": {
                "argv": command,
                "returncode": 1,
            },
            "delete_condition": (
                "Rerun C7-M4 S2 after this restore/inspection layer is available; do not "
                "open the C++ implementation gate from StableSubList-fed geometry alone."
            ),
        })
        atomic_write_json(out_path, payload)
        return 1
    except Exception as exc:
        payload.update({
            "route": "native_oracle_blocked",
            "blocker_layer": "probe_exception",
            "reason": str(exc),
            "traceback": traceback.format_exc(),
            "command": {
                "argv": command,
                "returncode": 1,
            },
            "delete_condition": (
                "Fix the native probe or FreeCAD runtime and rerun S2 before opening "
                "ReferenceShadow recovery implementation."
            ),
        })
        atomic_write_json(out_path, payload)
        return 1
    finally:
        if reopened is not None:
            try:
                FreeCAD.closeDocument(reopened.Name)
            except Exception:
                pass
        if doc is not None:
            try:
                FreeCAD.closeDocument(doc.Name)
            except Exception:
                pass
        if cleanup_workdir:
            shutil.rmtree(work_root, ignore_errors=True)


def patch_returncode(path: Path, returncode: int, command: list[str]) -> None:
    if path.exists():
        payload = json.loads(path.read_text(encoding="utf-8"))
    else:
        payload = make_blocked_payload(
            parse_args(sys.argv[1:]),
            "freecadcmd",
            "FreeCADCmd did not produce native probe evidence JSON",
            command,
            returncode,
        )
    payload.setdefault("command", {})
    payload["command"]["argv"] = command
    payload["command"]["returncode"] = returncode
    atomic_write_json(path, payload)


def run_via_freecadcmd(argv: list[str], args: argparse.Namespace) -> int:
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(script_args(argv), ensure_ascii=False)
    command = [args.freecadcmd, str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER]
    result = subprocess.run(command, cwd=Path.cwd(), env=env)
    patch_returncode(Path(args.out), result.returncode, command)
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
