#!/usr/bin/env python3
"""Collect deterministic headless smoke receipts for retained Python/data entries."""

from __future__ import annotations

import argparse
import hashlib
import importlib
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "freecad-non-cad-smoke/v1"
ENV_ARG_NAME = "FREECAD_NON_CAD_SMOKE_ARGS_JSON"
ENV_ARG_MARKER = "__freecad_non_cad_smoke_args_env__"
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
DEFAULT_OUT_ROOT = (
    ROOT / "tools" / "freecad_expected_parity" / "reports" / "non_cad_smoke"
)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def artifact(path: Path) -> dict[str, Any]:
    resolved = path.resolve()
    return {
        "path": str(resolved),
        "sha256": file_sha256(resolved),
    }


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def freecad_version(FreeCAD: Any) -> str:
    version = FreeCAD.Version()
    if isinstance(version, (list, tuple)):
        if len(version) >= 4:
            revision = str(version[3]).split()[0]
            return f"{version[0]}.{version[1]}.{version[2]} revision {revision}"
        return " ".join(str(item) for item in version if item)
    return str(version)


def help_check() -> tuple[dict[str, Any], list[dict[str, Any]]]:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/Help/Help.py
    # ::underscore_page() is an import-safe, network-free headless code path.
    Help = importlib.import_module("Help")
    actual = Help.underscore_page("Workbench/Draft Line")
    expected = "Workbench/Draft_Line"
    check = {
        "id": "help-import-and-pure-path",
        "status": "passed" if actual == expected else "failed",
        "actual": actual,
        "expected": expected,
    }
    source = Path(str(Help.__file__))
    return check, [artifact(source)]


def addon_manager_check() -> tuple[dict[str, Any], list[dict[str, Any]]]:
    # FreeCAD: /Users/li/Chili3DProject/FreeCAD/src/Mod/AddonManager
    # /addonmanager_metadata.py::Version and /addonmanager_licenses.py
    # ::SPDXLicenseManager are headless Python/data paths with no network access.
    metadata = importlib.import_module("addonmanager_metadata")
    licenses = importlib.import_module("addonmanager_licenses")
    version = metadata.Version("v1.2.3-beta")
    license_manager = licenses.SPDXLicenseManager()
    actual = {
        "version": repr(version),
        "mitName": license_manager.name("MIT"),
        "mitOsiApproved": license_manager.is_osi_approved("MIT"),
    }
    expected = {
        "version": "1.2.3 -beta",
        "mitName": "MIT License",
        "mitOsiApproved": True,
    }
    check = {
        "id": "addon-manager-metadata-and-spdx-data",
        "status": "passed" if actual == expected else "failed",
        "actual": actual,
        "expected": expected,
    }
    spdx = Path(str(licenses.__file__)).resolve().parent / "Resources" / "licenses" / "spdx.json"
    return check, [
        artifact(Path(str(metadata.__file__))),
        artifact(Path(str(licenses.__file__))),
        artifact(spdx),
    ]


def producer_receipt(freecadcmd: Path, FreeCAD: Any) -> dict[str, Any]:
    resolved = freecadcmd.resolve()
    return {
        "requestedPath": str(freecadcmd),
        "path": str(resolved),
        "sha256": file_sha256(resolved),
        "freecadVersion": freecad_version(FreeCAD),
    }


def collect_entry(
    entry: str,
    check_fn: Callable[[], tuple[dict[str, Any], list[dict[str, Any]]]],
    *,
    freecadcmd: Path,
    FreeCAD: Any,
) -> dict[str, Any]:
    try:
        check, module_evidence = check_fn()
        status = "passed" if check.get("status") == "passed" else "failed"
        errors: list[str] = []
    except Exception as exc:  # keep a failed receipt as evidence of the real boundary
        check = {
            "id": f"{entry.lower()}-headless-smoke",
            "status": "failed",
            "error": f"{type(exc).__name__}: {exc}",
        }
        module_evidence = []
        status = "failed"
        errors = [check["error"]]
    return {
        "schema": SCHEMA,
        "entry": entry,
        "status": status,
        "producer": producer_receipt(freecadcmd, FreeCAD),
        "tool": artifact(Path(__file__)),
        "checks": [check],
        "moduleEvidence": module_evidence,
        "errors": errors,
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
        description="Collect controlled FreeCADCmd smoke receipts for Help and AddonManager."
    )
    parser.add_argument("--freecadcmd", default=DEFAULT_FREECADCMD)
    parser.add_argument("--out-root", default=str(DEFAULT_OUT_ROOT))
    return parser.parse_args(script_args(argv))


def run_inside_freecad(args: argparse.Namespace) -> int:
    import FreeCAD  # type: ignore

    output_root = Path(args.out_root).resolve()
    freecadcmd = Path(args.freecadcmd)
    receipts = {
        "Help": collect_entry("Help", help_check, freecadcmd=freecadcmd, FreeCAD=FreeCAD),
        "AddonManager": collect_entry(
            "AddonManager",
            addon_manager_check,
            freecadcmd=freecadcmd,
            FreeCAD=FreeCAD,
        ),
    }
    for entry, receipt in receipts.items():
        atomic_write_json(output_root / f"{entry}.json", receipt)
    failed = [entry for entry, receipt in receipts.items() if receipt["status"] != "passed"]
    print(
        "non-CAD smoke: "
        + ("status=passed entries=Help,AddonManager" if not failed else f"status=failed entries={','.join(failed)}")
    )
    return 1 if failed else 0


def run_via_freecadcmd(args: argparse.Namespace) -> int:
    freecadcmd = Path(args.freecadcmd)
    resolved = freecadcmd.resolve()
    if not resolved.is_file():
        raise FileNotFoundError(f"FreeCADCmd not found: {freecadcmd}")
    env = os.environ.copy()
    env[ENV_ARG_NAME] = json.dumps(
        ["--freecadcmd", str(freecadcmd), "--out-root", str(Path(args.out_root))]
    )
    return subprocess.run(
        [str(freecadcmd), str(Path(__file__).resolve()), "--pass", ENV_ARG_MARKER],
        cwd=ROOT,
        env=env,
    ).returncode


def main(argv: list[str] | None = None) -> int:
    args = parse_args(list(sys.argv[1:] if argv is None else argv))
    if invoked_by_freecad_cli_import():
        return run_inside_freecad(args)
    return run_via_freecadcmd(args)


if __name__ == "__main__" or invoked_by_freecad_cli_import():
    raise SystemExit(main())
