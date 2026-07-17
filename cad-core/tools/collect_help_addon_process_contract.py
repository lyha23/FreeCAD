#!/usr/bin/env python3
"""Collect hermetic Help and AddonManager FreeCADCmd process authority."""

from __future__ import annotations

import argparse
import importlib
import json
import os
import sys
import tempfile
import types
import urllib.request
import webbrowser
import zipfile
from pathlib import Path
from typing import Any
from unittest.mock import patch

from freecad_expected_parity.process_contract import (
    SCHEMA,
    ProcessSpec,
    artifact,
    atomic_write,
    clean_environment,
    process_succeeded,
    run_process,
)


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
MANIFEST = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "process_contracts"
    / "help_addonmanager"
    / "manifest.v1.json"
)
DEFAULT_REPORT = (
    ROOT
    / "tools"
    / "freecad_expected_parity"
    / "reports"
    / "process_contract"
    / "help-addonmanager.v1.json"
)
DEFAULT_FREECADCMD = "/Users/li/.cargo/bin/FreeCADCmd"
ENV_ARGS = "FREECAD_HELP_ADDON_PROCESS_ARGS_JSON"
ENV_MARKER = "__freecad_help_addon_process_args_env__"
SOURCE_EVIDENCE = {
    "help": [
        "src/Mod/Help/Help.py::show/get_location/get_contents/convert/show_browser",
    ],
    "addonmanager": [
        "src/Mod/AddonManager/addonmanager_installer.py::AddonInstaller",
        "src/Mod/AddonManager/addonmanager_uninstaller.py::AddonUninstaller",
        "src/Mod/AddonManager/NetworkManager.py::NetworkManager",
        "src/Mod/AddonManager/addonmanager_metadata.py::MetadataReader/Version",
        "src/Mod/AddonManager/addonmanager_licenses.py::SPDXLicenseManager",
    ],
}
SOURCE_PATHS = [
    REPO / "src" / "Mod" / "Help" / "Help.py",
    REPO / "src" / "Mod" / "AddonManager" / "addonmanager_installer.py",
    REPO / "src" / "Mod" / "AddonManager" / "addonmanager_uninstaller.py",
    REPO / "src" / "Mod" / "AddonManager" / "NetworkManager.py",
    REPO / "src" / "Mod" / "AddonManager" / "addonmanager_metadata.py",
    REPO / "src" / "Mod" / "AddonManager" / "addonmanager_licenses.py",
]


def load_manifest() -> dict[str, Any]:
    payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if payload.get("schema") != "freecad-help-addonmanager-process-manifest/v1":
        raise ValueError("Help/AddonManager process manifest schema is invalid")
    cases = payload.get("cases")
    if not isinstance(cases, list) or not cases:
        raise ValueError("Help/AddonManager process manifest cases are missing")
    ids = [case.get("id") for case in cases if isinstance(case, dict)]
    if len(ids) != len(cases) or len(set(ids)) != len(ids):
        raise ValueError("Help/AddonManager process manifest case ids are invalid")
    return payload


def version_string(module: Any) -> str:
    version = module.Version()
    if isinstance(version, (list, tuple)):
        return " ".join(str(item) for item in version if item)
    return str(version)


def caught_error(callable_: Any) -> dict[str, str] | None:
    try:
        callable_()
    except Exception as exc:  # The public exception type/message is the authority.
        return {"type": type(exc).__name__, "message": str(exc)}
    return None


class FakeManifest:
    installed: set[str] = set()
    events: list[str] = []

    def contains(self, name: str) -> bool:
        return name in self.installed

    def record_new_installation(self, name: str, _addon: Any, **_kwargs: Any) -> None:
        self.installed.add(name)
        self.events.append(f"install:{name}")

    def record_update(self, name: str, _addon: Any, **_kwargs: Any) -> None:
        self.installed.add(name)
        self.events.append(f"update:{name}")

    def remove(self, name: str) -> None:
        self.installed.discard(name)
        self.events.append(f"remove:{name}")


class FakeAddon:
    def __init__(self, name: str, url: str, branch: str = "main") -> None:
        self.name = name
        self.url = url
        self.branch = branch
        self.metadata = None
        self.installed_metadata = None
        self.icon_data = None
        self.statuses: list[str] = []

    def contains_workbench(self) -> bool:
        return False

    def set_status(self, status: Any) -> None:
        self.statuses.append(str(status))

    def get_zip_url(self) -> str:
        return self.url + ".zip"


def help_local_page(case_root: Path) -> tuple[dict[str, Any], bool]:
    import Help  # type: ignore

    docs = case_root / "local-help" / "Workbench"
    docs.mkdir(parents=True)
    page = docs / "Draft_Line.md"
    marker = "A7-LOCAL-HELP-CONTENT"
    page.write_text(f"# {marker}\n\n**offline**\n", encoding="utf-8")
    prefs = Help.PREFS
    prefs.SetBool("optionWiki", False)
    prefs.SetBool("optionMarkdown", False)
    prefs.SetBool("optionGithub", False)
    prefs.SetBool("optionCustom", True)
    prefs.SetString("Location", str(docs.parent))
    with patch.object(urllib.request, "urlopen", side_effect=AssertionError("network forbidden")):
        normalized = Help.underscore_page("Workbench/Draft Line")
        location, pagename = Help.get_location(normalized)
        contents = Help.get_contents(location)
        rendered = Help.convert(contents, "raw")
        missing = Help.get_contents(str(case_root / "missing-help.md"))
        Help.show("Workbench/Draft Line", conv="raw")
    actual = {
        "guiUp": bool(Help.FreeCAD.GuiUp),
        "normalizedPage": normalized,
        "locationIsLocalFixture": Path(location) == page,
        "pageName": pagename,
        "contentsMarker": marker in contents,
        "renderedHeading": f"<h1>{marker}</h1>" in rendered,
        "renderedHtmlEnvelope": rendered.startswith("<html>"),
        "missingReturnsErrorText": missing == Help.ERRORTXT,
        "uriIsFile": Help.get_uri(location).startswith("file://"),
        "moduleFile": str(Path(Help.__file__).resolve()),
    }
    passed = (
        not actual["guiUp"]
        and actual["normalizedPage"] == "Workbench/Draft_Line"
        and actual["locationIsLocalFixture"]
        and actual["contentsMarker"]
        and actual["renderedHeading"]
        and actual["renderedHtmlEnvelope"]
        and actual["missingReturnsErrorText"]
        and actual["uriIsFile"]
    )
    return actual, passed


def help_browser_fallback() -> tuple[dict[str, Any], bool]:
    import Help  # type: ignore

    calls: list[str] = []

    class FakeUrl:
        def __init__(self, value: str) -> None:
            self.value = value

    class FakeDesktopServices:
        @staticmethod
        def openUrl(url: FakeUrl) -> bool:
            calls.append(f"qt:{url.value}")
            return False

    fake_pyside = types.ModuleType("PySide")
    fake_pyside.QtCore = types.SimpleNamespace(QUrl=FakeUrl)  # type: ignore[attr-defined]
    fake_pyside.QtGui = types.SimpleNamespace(  # type: ignore[attr-defined]
        QDesktopServices=FakeDesktopServices
    )
    original_pyside = sys.modules.get("PySide")
    original_open = webbrowser.open_new
    try:
        sys.modules["PySide"] = fake_pyside
        webbrowser.open_new = lambda url: calls.append(f"browser:{url}") or True
        Help.show_browser("file:///A7/local-help.html")
    finally:
        webbrowser.open_new = original_open
        if original_pyside is None:
            sys.modules.pop("PySide", None)
        else:
            sys.modules["PySide"] = original_pyside
    actual = {
        "dispatch": calls,
        "realBrowserCalled": False,
        "moduleFile": str(Path(Help.__file__).resolve()),
    }
    return actual, calls == ["qt:file:///A7/local-help.html", "browser:file:///A7/local-help.html"]


def help_gui_boundary() -> tuple[dict[str, Any], bool]:
    import FreeCAD  # type: ignore
    import Help  # type: ignore

    gui_import = caught_error(lambda: importlib.import_module("FreeCADGui"))
    actual = {
        "guiUp": bool(FreeCAD.GuiUp),
        "freeCADGuiImport": gui_import or {"type": "none", "message": "importable"},
        "attemptedEntry": "Help.show_tab/show_dialog/openBrowserHTML",
        "executed": False,
        "reason": "FreeCADCmd reports GuiUp=false; invoking visible Qt/WebEngine UI is prohibited.",
        "closeCondition": "A hermetic GUI-capable FreeCAD harness can exercise WebEngine tab/dialog creation without visible user effects.",
        "moduleFile": str(Path(Help.__file__).resolve()),
    }
    return actual, actual["guiUp"] is False


def addon_metadata_spdx() -> tuple[dict[str, Any], bool]:
    import addonmanager_licenses as licenses  # type: ignore
    import addonmanager_metadata as metadata  # type: ignore

    xml = b"""<?xml version='1.0'?>
<package format='1' xmlns='https://wiki.freecad.org/Package_Metadata'>
  <name>A7 Local Addon</name><description>offline fixture</description>
  <version>2.4.1beta</version><date>2026-07-17</date>
  <maintainer>A7</maintainer><license>MIT</license>
  <url type='repository' branch='stable'>https://invalid.example/A7.git</url>
</package>"""
    parsed = metadata.MetadataReader.from_bytes(xml)
    invalid = caught_error(lambda: metadata.MetadataReader.from_bytes(b"<invalid/>"))
    manager = licenses.get_license_manager()
    actual = {
        "name": parsed.name,
        "version": repr(parsed.version),
        "branch": metadata.get_branch_from_metadata(parsed),
        "repo": metadata.get_repo_url_from_metadata(parsed),
        "mitName": manager.name("MIT"),
        "mitOsiApproved": manager.is_osi_approved("MIT"),
        "normalizedGpl": manager.normalize("GPL3+"),
        "invalidMetadata": invalid,
        "moduleFiles": [
            str(Path(metadata.__file__).resolve()),
            str(Path(licenses.__file__).resolve()),
        ],
    }
    passed = (
        actual["name"] == "A7 Local Addon"
        and actual["version"] == "2.4.1 beta"
        and actual["branch"] == "stable"
        and actual["mitName"] == "MIT License"
        and actual["mitOsiApproved"] is True
        and actual["normalizedGpl"] == "GPL-3.0-or-later"
        and invalid is not None
    )
    return actual, passed


def install_update_remove(case_root: Path) -> tuple[dict[str, Any], bool]:
    import addonmanager_installer as installer_module  # type: ignore
    import addonmanager_uninstaller as uninstaller_module  # type: ignore

    source = case_root / "fake-repo"
    install_root = case_root / "user-data" / "Mod"
    macro_root = case_root / "user-data" / "Macro"
    source.mkdir(parents=True)
    install_root.mkdir(parents=True)
    macro_root.mkdir(parents=True)
    payload = source / "payload.txt"
    payload.write_text("version-1", encoding="utf-8")
    FakeManifest.installed.clear()
    FakeManifest.events.clear()
    addon = FakeAddon("A7LocalAddon", str(source))
    with (
        patch.object(installer_module, "InstallationManifest", FakeManifest),
        patch.object(uninstaller_module, "InstallationManifest", FakeManifest),
    ):
        installer = installer_module.AddonInstaller(addon, allow_list=[])
        installer.installation_path = str(install_root)
        installer.macro_installation_path = str(macro_root)
        first = installer.run(installer_module.InstallationMethod.COPY)
        first_value = (install_root / addon.name / "payload.txt").read_text(encoding="utf-8")
        payload.write_text("version-2", encoding="utf-8")
        second = installer.run(installer_module.InstallationMethod.COPY)
        second_value = (install_root / addon.name / "payload.txt").read_text(encoding="utf-8")
        uninstaller = uninstaller_module.AddonUninstaller(addon)
        uninstaller.installation_path = str(install_root)
        uninstaller.macro_installation_path = str(macro_root)
        removed = uninstaller.run()
    actual = {
        "firstInstall": first,
        "firstValue": first_value,
        "updateInstall": second,
        "updatedValue": second_value,
        "removed": removed,
        "destinationExistsAfterRemove": (install_root / addon.name).exists(),
        "manifestEvents": list(FakeManifest.events),
        "statusesRecorded": len(addon.statuses),
        "moduleFiles": [
            str(Path(installer_module.__file__).resolve()),
            str(Path(uninstaller_module.__file__).resolve()),
        ],
    }
    passed = (
        first
        and first_value == "version-1"
        and second
        and second_value == "version-2"
        and removed
        and not actual["destinationExistsAfterRemove"]
        and actual["manifestEvents"]
        == ["install:A7LocalAddon", "update:A7LocalAddon", "remove:A7LocalAddon"]
    )
    return actual, passed


def zip_and_network_mock(case_root: Path) -> tuple[dict[str, Any], bool]:
    import addonmanager_freecad_interface as fci  # type: ignore
    import addonmanager_installer as installer_module  # type: ignore
    import NetworkManager as network_module  # type: ignore
    from PySideWrapper import QtCore, QtNetwork  # type: ignore

    install_root = case_root / "user-data" / "Mod"
    macro_root = case_root / "user-data" / "Macro"
    install_root.mkdir(parents=True)
    macro_root.mkdir(parents=True)
    archive = case_root / "mock-addon.zip"
    with zipfile.ZipFile(archive, "w") as handle:
        handle.writestr("README.txt", "A7-MOCK-ZIP")
    archive_bytes = archive.read_bytes()
    requested_urls: list[str] = []

    def fake_blocking_get(url: str) -> bytes:
        requested_urls.append(url)
        return archive_bytes

    FakeManifest.installed.clear()
    FakeManifest.events.clear()
    addon = FakeAddon("A7ZipAddon", "https://mock.invalid/A7ZipAddon")
    with (
        patch.object(installer_module, "InstallationManifest", FakeManifest),
        patch.object(installer_module.utils, "blocking_get", fake_blocking_get),
    ):
        installer = installer_module.AddonInstaller(addon, allow_list=[])
        installer.installation_path = str(install_root)
        installer.macro_installation_path = str(macro_root)
        installed = installer.run(installer_module.InstallationMethod.ZIP)
    extracted = (install_root / addon.name / "README.txt").read_text(encoding="utf-8")

    fci.Preferences().set("proxy_type", "none")
    manager = network_module.NetworkManager()
    completed: list[dict[str, Any]] = []

    class FakeReply(QtCore.QObject):
        finished = QtCore.Signal()
        sslErrors = QtCore.Signal(object)
        readyRead = QtCore.Signal()
        downloadProgress = QtCore.Signal(int, int)

        def __init__(self, request: Any, operation: Any) -> None:
            super().__init__()
            self._request = request
            self._operation = operation

        def attribute(self, attribute: Any) -> Any:
            if attribute == QtNetwork.QNetworkRequest.HttpStatusCodeAttribute:
                return 200
            if attribute == QtNetwork.QNetworkRequest.RedirectionTargetAttribute:
                return None
            return None

        def error(self) -> Any:
            return QtNetwork.QNetworkReply.NetworkError.NoError

        def operation(self) -> Any:
            return self._operation

        def readAll(self) -> Any:
            return QtCore.QByteArray(b"A7-MOCK-NETWORK")

        def request(self) -> Any:
            return self._request

    class FakeQnam:
        def __init__(self) -> None:
            self.urls: list[str] = []
            self.replies: list[FakeReply] = []

        def get(self, request: Any) -> FakeReply:
            self.urls.append(request.url().toString())
            reply = FakeReply(request, QtNetwork.QNetworkAccessManager.GetOperation)
            self.replies.append(reply)
            return reply

        def head(self, request: Any) -> FakeReply:
            self.urls.append(request.url().toString())
            reply = FakeReply(request, QtNetwork.QNetworkAccessManager.HeadOperation)
            self.replies.append(reply)
            return reply

    fake_qnam = FakeQnam()
    manager.QNAM = fake_qnam
    manager.completed.connect(
        lambda index, code, data: completed.append(
            {"index": index, "code": code, "data": bytes(data).decode("utf-8")}
        )
    )
    network_index = manager.submit_unmonitored_get(
        "https://mock.invalid/catalog.json", disable_cache=True
    )
    fake_qnam.replies[-1].finished.emit()
    actual = {
        "zipInstalled": installed,
        "zipContent": extracted,
        "blockingGetUrls": requested_urls,
        "networkIndex": network_index,
        "qnamUrls": fake_qnam.urls,
        "completed": completed,
        "realNetworkRequests": 0,
        "moduleFiles": [
            str(Path(installer_module.__file__).resolve()),
            str(Path(network_module.__file__).resolve()),
        ],
    }
    passed = (
        installed
        and extracted == "A7-MOCK-ZIP"
        and requested_urls == ["https://mock.invalid/A7ZipAddon.zip"]
        and fake_qnam.urls == ["https://mock.invalid/catalog.json"]
        and completed
        == [{"index": network_index, "code": 200, "data": "A7-MOCK-NETWORK"}]
    )
    return actual, passed


def addon_invalid_boundaries(case_root: Path) -> tuple[dict[str, Any], bool]:
    import addonmanager_installer as installer_module  # type: ignore
    import addonmanager_uninstaller as uninstaller_module  # type: ignore

    invalid_install = caught_error(
        lambda: installer_module.AddonInstaller._validate_object(
            types.SimpleNamespace(name="A7Invalid", url="file:///tmp")
        )
    )
    install_root = case_root / "user-data" / "Mod"
    install_root.mkdir(parents=True)
    sentinel = install_root / "sentinel.txt"
    sentinel.write_text("keep", encoding="utf-8")
    dangerous = FakeAddon("./", str(case_root))
    with patch.object(uninstaller_module, "InstallationManifest", FakeManifest):
        uninstaller = uninstaller_module.AddonUninstaller(dangerous)
        uninstaller.installation_path = str(install_root)
        rejected_remove = uninstaller.run()
    actual = {
        "invalidInstaller": invalid_install,
        "dangerousRemoveSucceeded": rejected_remove,
        "sentinelPreserved": sentinel.read_text(encoding="utf-8") == "keep",
        "moduleFiles": [
            str(Path(installer_module.__file__).resolve()),
            str(Path(uninstaller_module.__file__).resolve()),
        ],
    }
    return actual, invalid_install is not None and not rejected_remove and actual["sentinelPreserved"]


def run_inner(case_id: str, result_path: Path) -> int:
    import FreeCAD  # type: ignore

    case_root = result_path.parent
    if case_id == "help-local-page-headless":
        actual, passed = help_local_page(case_root)
    elif case_id == "help-browser-fallback-mocked":
        actual, passed = help_browser_fallback()
    elif case_id == "help-gui-webengine-boundary-probe":
        actual, passed = help_gui_boundary()
    elif case_id == "addon-metadata-spdx-local":
        actual, passed = addon_metadata_spdx()
    elif case_id == "addon-copy-install-update-remove":
        actual, passed = install_update_remove(case_root)
    elif case_id == "addon-zip-and-network-manager-mocked":
        actual, passed = zip_and_network_mock(case_root)
    elif case_id == "addon-invalid-boundaries":
        actual, passed = addon_invalid_boundaries(case_root)
    else:
        raise ValueError(f"unsupported Help/AddonManager process case: {case_id}")
    payload = {
        "case": case_id,
        "status": "passed" if passed else "failed",
        "actual": actual,
        "producerVersion": version_string(FreeCAD),
    }
    atomic_write(result_path, payload)
    print(f"Help/AddonManager process contract: case={case_id} status={payload['status']}")
    return 0 if passed else 1


def script_args(argv: list[str]) -> list[str]:
    args = list(argv)
    if "--pass" in args:
        args = args[args.index("--pass") + 1 :]
    if args == [ENV_MARKER] and os.environ.get(ENV_ARGS):
        return json.loads(os.environ[ENV_ARGS])
    return args


def invoked_by_freecad() -> bool:
    return "--pass" in sys.argv and any(
        not arg.startswith("-") and Path(arg).resolve() == Path(__file__).resolve()
        for arg in sys.argv[1:]
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freecadcmd", default=DEFAULT_FREECADCMD)
    parser.add_argument("--report", default=str(DEFAULT_REPORT))
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--case", action="append", dest="cases")
    parser.add_argument("--inner-case")
    parser.add_argument("--result")
    return parser.parse_args(script_args(argv))


def normalized_result(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def run_case(
    case: dict[str, Any], *, executable: Path, temporary_root: Path, label: str
) -> dict[str, Any]:
    case_root = temporary_root / label / case["id"]
    directories = {
        name: case_root / name
        for name in (
            "user-home",
            "user-data",
            "user-temp",
            "xdg-config",
            "xdg-cache",
            "xdg-data",
        )
    }
    case_root.mkdir(parents=True)
    for directory in directories.values():
        directory.mkdir(parents=True)
    result_path = case_root / "result.json"
    user_cfg = case_root / "user.cfg"
    system_cfg = case_root / "system.cfg"
    replacements = {
        str(case_root): "<CASE_ROOT>",
        str(temporary_root): "<TMP>",
        str(ROOT): "<CAD_CORE>",
        str(REPO): "<REPO>",
    }
    inner_args = ["--inner-case", case["id"], "--result", str(result_path)]
    environment = clean_environment(
        {
            "PATH": "/usr/bin:/bin",
            "LANG": "C",
            "LC_ALL": "C",
            "HOME": str(directories["user-home"]),
            "FREECAD_USER_HOME": str(directories["user-home"]),
            "FREECAD_USER_DATA": str(directories["user-data"]),
            "FREECAD_USER_TEMP": str(directories["user-temp"]),
            "TMPDIR": str(directories["user-temp"]),
            "XDG_CONFIG_HOME": str(directories["xdg-config"]),
            "XDG_CACHE_HOME": str(directories["xdg-cache"]),
            "XDG_DATA_HOME": str(directories["xdg-data"]),
            "HTTP_PROXY": "http://127.0.0.1:9",
            "HTTPS_PROXY": "http://127.0.0.1:9",
            "ALL_PROXY": "http://127.0.0.1:9",
            "NO_PROXY": "",
            ENV_ARGS: json.dumps(inner_args),
        }
    )
    argv = [
        "-u",
        str(user_cfg),
        "-s",
        str(system_cfg),
        str(Path(__file__).resolve()),
        "--pass",
        ENV_MARKER,
    ]
    process = run_process(
        ProcessSpec(
            executable=executable,
            argv=argv,
            cwd=case_root,
            environment=environment,
            timeout_seconds=30,
            replacements=replacements,
        )
    )
    result = normalized_result(result_path)
    exit_matches = process_succeeded(process, 0)
    result_matches = bool(result and result.get("status") == "passed")
    if case["id"] == "help-local-page-headless":
        result_matches = result_matches and "A7-LOCAL-HELP-CONTENT" in process["stdout"]
    status = "passed" if exit_matches and result_matches else "failed"
    coverage_outcome = (
        "source_backed_exception"
        if case.get("coveragePolicy") == "source_backed_exception" and status == "passed"
        else "native_process_test"
        if status == "passed"
        else "failed"
    )
    return {
        "label": label,
        "caseId": case["id"],
        "status": status,
        "coverageOutcome": coverage_outcome,
        "process": process,
        "result": result,
    }


def semantic_result(run: dict[str, Any]) -> str:
    process = run["process"]
    payload = {
        "status": run["status"],
        "coverageOutcome": run["coverageOutcome"],
        "exitCode": process["exitCode"],
        "signal": process["signal"],
        "timedOut": process["timedOut"],
        "stdout": process["stdout"],
        "stderr": process["stderr"],
        "result": run["result"],
    }
    return json.dumps(payload, ensure_ascii=False, sort_keys=True)


def run_outer(args: argparse.Namespace) -> int:
    if args.repeat < 2:
        raise ValueError("--repeat must be at least 2")
    manifest = load_manifest()
    all_cases = manifest["cases"]
    requested_ids = set(args.cases or [case["id"] for case in all_cases])
    known_ids = {case["id"] for case in all_cases}
    unknown_ids = requested_ids - known_ids
    if unknown_ids:
        raise ValueError(f"unknown process-contract cases: {sorted(unknown_ids)}")
    cases = [case for case in all_cases if case["id"] in requested_ids]
    requested = Path(args.freecadcmd)
    executable = requested.resolve()
    if not executable.is_file():
        raise FileNotFoundError(f"FreeCADCmd not found: {requested}")
    runs: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="freecad-help-addon-contract-") as temporary:
        temporary_root = Path(temporary)
        for repeat in range(args.repeat):
            label = f"run-{repeat + 1}"
            for case in cases:
                runs.append(
                    run_case(
                        case,
                        executable=executable,
                        temporary_root=temporary_root,
                        label=label,
                    )
                )
    case_rows = []
    errors: list[str] = []
    for case in cases:
        case_runs = [run for run in runs if run["caseId"] == case["id"]]
        stable = (
            len(case_runs) == args.repeat
            and all(run["status"] == "passed" for run in case_runs)
            and len({semantic_result(run) for run in case_runs}) == 1
        )
        if not stable:
            errors.append(f"case failed or drifted: {case['id']}")
        case_rows.append(
            {
                "id": case["id"],
                "group": case["group"],
                "evidenceFor": case["evidenceFor"],
                "sourceEvidence": SOURCE_EVIDENCE[case["group"]],
                "status": "passed" if stable else "failed",
                "coverageOutcome": case_runs[0]["coverageOutcome"] if case_runs else "failed",
                "runs": case_runs,
            }
        )
    report = {
        "schema": SCHEMA,
        "contractId": manifest["contractId"],
        "status": "passed" if not errors else "failed",
        "repeat": args.repeat,
        "repeatStatus": "passed" if not errors else "failed",
        "caseCount": len(cases),
        "manifestCaseCount": len(all_cases),
        "producer": {"requestedPath": str(requested), **artifact(executable)},
        "tool": artifact(Path(__file__)),
        "runner": artifact(ROOT / "tools" / "freecad_expected_parity" / "process_contract.py"),
        "resources": [artifact(MANIFEST), *[artifact(path) for path in SOURCE_PATHS]],
        "environmentPolicy": {
            "inheritedAllowlist": ["SYSTEMROOT"],
            "fixed": {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"},
            "caseLocal": [
                "HOME",
                "FREECAD_USER_HOME",
                "FREECAD_USER_DATA",
                "FREECAD_USER_TEMP",
                "TMPDIR",
                "XDG_CONFIG_HOME",
                "XDG_CACHE_HOME",
                "XDG_DATA_HOME",
                ENV_ARGS,
            ],
            "networkDeny": {
                "proxy": "127.0.0.1:9",
                "helpUrlopen": "patched to fail",
                "addonZip": "local bytes through mocked blocking_get",
                "networkManager": "real queue/request path with mocked QNAM",
            },
        },
        "normalization": {
            "lineEndings": "CRLF to LF",
            "replacements": {
                "temporaryRoot": "<TMP>",
                "cadCoreRoot": "<CAD_CORE>",
                "repositoryRoot": "<REPO>",
            },
        },
        "cadCoreRuntimeParity": "not_evaluated",
        "boundaryExceptions": [
            {
                "branch": "Help WebEngine tab/dialog creation under GuiUp=true",
                "classification": "source_backed_exception",
                "receiptCase": "help-gui-webengine-boundary-probe",
                "reason": "The release FreeCADCmd process reports GuiUp=false and the task prohibits visible GUI effects.",
                "closeCondition": "A hermetic GUI-capable FreeCAD harness can exercise WebEngine tab/dialog creation without visible user effects.",
            },
            {
                "branch": "Real remote Help and Addon downloads",
                "classification": "prohibited_external_effect",
                "reason": "The authority contract deliberately uses local pages, fake repositories, proxy-denied urllib, and mocked QNAM/blocking_get.",
                "closeCondition": "Not required for deterministic API authority; network transport is represented by the mocked request/response contract.",
            },
        ],
        "cases": case_rows,
        "errors": errors,
    }
    atomic_write(Path(args.report), report)
    print(
        "Help/AddonManager process contract: "
        f"status={report['status']} cases={len(cases)} repeat={args.repeat}"
    )
    return 0 if not errors else 1


def main(argv: list[str] | None = None) -> int:
    args = parse_args(list(sys.argv[1:] if argv is None else argv))
    if invoked_by_freecad():
        if not args.inner_case or not args.result:
            raise ValueError("FreeCAD inner process contract requires --inner-case and --result")
        return run_inner(args.inner_case, Path(args.result))
    return run_outer(args)


if invoked_by_freecad():
    inner_code = main()
    if inner_code:
        raise SystemExit(inner_code)
elif __name__ == "__main__":
    raise SystemExit(main())
