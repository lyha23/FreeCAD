from __future__ import annotations

import ctypes
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = Path(os.environ.get("CAD_CORE_TEST_BUILD_DIR", ROOT / "build"))
if not BUILD_DIR.is_absolute():
    BUILD_DIR = ROOT / BUILD_DIR
BIN = BUILD_DIR / "cad-core"
FFI_LIB_CANDIDATES = [
    BUILD_DIR / "libcad_core_ffi.dylib",
    BUILD_DIR / "libcad_core_ffi.so",
]


def semantic_fixture_path(fixture: str, phase: str | None = None) -> Path:
    if phase:
        candidate = ROOT / "fixtures" / phase / f"{fixture}.json"
        if candidate.is_file():
            return candidate
        raise AssertionError(
            f"fixture {fixture!r} does not exist in semantic phase {phase!r}"
        )
    matches = sorted((ROOT / "fixtures").glob(f"*/{fixture}.json"))
    if len(matches) != 1:
        raise AssertionError(
            f"fixture {fixture!r} resolved to {len(matches)} semantic phases: "
            f"{[path.parent.name for path in matches]}"
        )
    return matches[0]


class CadCoreBuffer(ctypes.Structure):
    _fields_ = [("ptr", ctypes.c_void_p), ("len", ctypes.c_size_t)]


class CadCoreResult(ctypes.Structure):
    _fields_ = [("status", ctypes.c_int32), ("json", CadCoreBuffer), ("error", CadCoreBuffer)]


class CadCoreExportResult(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int32),
        ("data", CadCoreBuffer),
        ("json", CadCoreBuffer),
        ("error", CadCoreBuffer),
    ]


class CadCoreFixtureTestCase(unittest.TestCase):
    def fixture_path(self, fixture: str, phase: str | None = None) -> Path:
        """Resolve a case in the semantic phase catalog.

        ``phase`` remains an optional assertion for tests that care about the
        capability boundary. Tests that do not care about classification omit
        it; case names are required to be unique across semantic phases.
        """

        try:
            return semantic_fixture_path(fixture, phase)
        except AssertionError as exc:
            self.fail(str(exc))

    def run_recompute_file(self, input_path: Path, extra_args: list[str] | None = None) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / f"{input_path.stem}.result.json"
            command = [
                str(BIN),
                "recompute",
                str(input_path),
                "--output",
                str(output),
            ]
            if extra_args:
                command.extend(extra_args)
            env = os.environ.copy()
            env["CAD_CORE_TEST_LEGACY_OUTPUT"] = "1"
            subprocess.run(command, cwd=ROOT, check=True, env=env)
            return json.loads(output.read_text(encoding="utf-8"))

    def run_recompute(self, fixture: str, group: str | None = None) -> dict:
        return self.run_recompute_file(self.fixture_path(fixture, group))

    def ffi_library_path(self) -> Path:
        for path in FFI_LIB_CANDIDATES:
            if path.exists():
                return path
        self.fail("cad_core_ffi library is missing; run cmake --build build first")

    def ffi_library(self) -> ctypes.CDLL:
        library = ctypes.CDLL(str(self.ffi_library_path()))
        library.cad_core_recompute_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_recompute_json.restype = CadCoreResult
        library.cad_core_worker_recompute_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_worker_recompute_json.restype = CadCoreResult
        library.cad_core_wasm_recompute_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_wasm_recompute_json.restype = CadCoreResult
        library.cad_core_capabilities_json.argtypes = []
        library.cad_core_capabilities_json.restype = CadCoreResult
        library.cad_core_export_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_export_json.restype = CadCoreExportResult
        library.cad_core_mesh_binary_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_mesh_binary_json.restype = CadCoreExportResult
        library.cad_core_free_result.argtypes = [ctypes.POINTER(CadCoreResult)]
        library.cad_core_free_result.restype = None
        library.cad_core_free_export_result.argtypes = [ctypes.POINTER(CadCoreExportResult)]
        library.cad_core_free_export_result.restype = None
        return library

    def run_recompute_ffi(self, fixture: str, group: str | None = None) -> dict:
        payload = self.fixture_path(fixture, group).read_bytes()
        return self.run_recompute_ffi_payload(payload)

    def run_recompute_ffi_payload(self, payload: bytes | dict) -> dict:
        library = self.ffi_library()
        return self.run_recompute_ffi_payload_with(
            library,
            library.cad_core_recompute_json,
            payload,
            "cad_core_recompute_json",
        )

    def run_recompute_ffi_payload_with(self, library, function, payload: bytes | dict, function_name: str) -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode("utf-8")
        result = function(payload, len(payload))
        try:
            if result.status != 0:
                error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
                self.fail(f"{function_name} failed with status {result.status}: {error}")
            raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
            return json.loads(raw)
        finally:
            library.cad_core_free_result(ctypes.byref(result))

    def run_worker_recompute_ffi_payload(self, payload: bytes | dict) -> dict:
        library = self.ffi_library()
        return self.run_recompute_ffi_payload_with(
            library,
            library.cad_core_worker_recompute_json,
            payload,
            "cad_core_worker_recompute_json",
        )

    def run_wasm_recompute_ffi_payload(self, payload: bytes | dict) -> dict:
        library = self.ffi_library()
        return self.run_recompute_ffi_payload_with(
            library,
            library.cad_core_wasm_recompute_json,
            payload,
            "cad_core_wasm_recompute_json",
        )

    def run_capabilities_ffi(self) -> dict:
        library = self.ffi_library()
        result = library.cad_core_capabilities_json()
        try:
            if result.status != 0:
                error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
                self.fail(f"cad_core_capabilities_json failed with status {result.status}: {error}")
            raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
            return json.loads(raw)
        finally:
            library.cad_core_free_result(ctypes.byref(result))

    def ondsel_solver_available(self) -> bool:
        return bool(self.run_capabilities_ffi()["assembly"]["ondsel_solver_adapter"]["available"])

    def call_export_ffi(self, request: dict) -> tuple[int, dict | None, bytes, str]:
        library = self.ffi_library()
        payload = json.dumps(request).encode("utf-8")
        result = library.cad_core_export_json(payload, len(payload))
        try:
            metadata = None
            if result.json.ptr:
                raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
                metadata = json.loads(raw)
            data = ctypes.string_at(result.data.ptr, result.data.len) if result.data.ptr else b""
            error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
            return result.status, metadata, data, error
        finally:
            library.cad_core_free_export_result(ctypes.byref(result))

    def call_mesh_binary_ffi(self, request: dict) -> tuple[int, dict | None, bytes, str]:
        library = self.ffi_library()
        payload = json.dumps(request).encode("utf-8")
        result = library.cad_core_mesh_binary_json(payload, len(payload))
        try:
            metadata = None
            if result.json.ptr:
                raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
                metadata = json.loads(raw)
            data = ctypes.string_at(result.data.ptr, result.data.len) if result.data.ptr else b""
            error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
            return result.status, metadata, data, error
        finally:
            library.cad_core_free_export_result(ctypes.byref(result))

    def diagnostic_codes(self, fixture: str, group: str | None = None) -> list[str]:
        return [item["code"] for item in self.run_recompute(fixture, group)["diagnostics"]]
