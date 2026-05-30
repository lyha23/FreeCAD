from __future__ import annotations

import ctypes
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build" / "cad-core"
FFI_LIB_CANDIDATES = [
    ROOT / "build" / "libcad_core_ffi.dylib",
    ROOT / "build" / "libcad_core_ffi.so",
]


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

    def run_recompute(self, fixture: str, group: str = "mvp") -> dict:
        return self.run_recompute_file(ROOT / "fixtures" / group / f"{fixture}.json")

    def ffi_library_path(self) -> Path:
        for path in FFI_LIB_CANDIDATES:
            if path.exists():
                return path
        self.fail("cad_core_ffi library is missing; run cmake --build build first")

    def ffi_library(self) -> ctypes.CDLL:
        library = ctypes.CDLL(str(self.ffi_library_path()))
        library.cad_core_recompute_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_recompute_json.restype = CadCoreResult
        library.cad_core_capabilities_json.argtypes = []
        library.cad_core_capabilities_json.restype = CadCoreResult
        library.cad_core_export_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_export_json.restype = CadCoreExportResult
        library.cad_core_free_result.argtypes = [ctypes.POINTER(CadCoreResult)]
        library.cad_core_free_result.restype = None
        library.cad_core_free_export_result.argtypes = [ctypes.POINTER(CadCoreExportResult)]
        library.cad_core_free_export_result.restype = None
        return library

    def run_recompute_ffi(self, fixture: str, group: str = "mvp") -> dict:
        library = self.ffi_library()

        payload = (ROOT / "fixtures" / group / f"{fixture}.json").read_bytes()
        result = library.cad_core_recompute_json(payload, len(payload))
        try:
            if result.status != 0:
                error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
                self.fail(f"cad_core_recompute_json failed with status {result.status}: {error}")
            raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
            return json.loads(raw)
        finally:
            library.cad_core_free_result(ctypes.byref(result))

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

    def diagnostic_codes(self, fixture: str, group: str = "mvp") -> list[str]:
        return [item["code"] for item in self.run_recompute(fixture, group)["diagnostics"]]
