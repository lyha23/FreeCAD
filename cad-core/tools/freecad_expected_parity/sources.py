"""Actual-payload adapters hidden behind the parity engine seam."""

from __future__ import annotations

import ctypes
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from .catalog import FixtureCase


class CadRsBuffer(ctypes.Structure):
    """Byte buffer returned by the Rust C ABI's ``CadRsResult``."""

    _fields_ = [("ptr", ctypes.c_void_p), ("len", ctypes.c_size_t)]


class CadRsResult(ctypes.Structure):
    """The narrow recompute-result ABI from ``cad-core-ffi/include/cad_rs.h``."""

    _fields_ = [("status", ctypes.c_int32), ("json", CadRsBuffer), ("error", CadRsBuffer)]


@dataclass
class ActualPayload:
    payload: dict[str, Any] | None
    raw_bytes: bytes | None
    error: str | None = None
    stdout: str | None = None
    stderr: str | None = None


def parse_payload(raw: bytes, *, label: str) -> ActualPayload:
    try:
        value = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        return ActualPayload(None, raw, f"invalid JSON for {label}: {exc}")
    if not isinstance(value, dict):
        return ActualPayload(None, raw, f"JSON payload must be an object for {label}")
    return ActualPayload(value, raw)


def canonical_json_bytes(payload: dict[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


class SnapshotActualSource:
    kind = "snapshot"

    def load(self, item: FixtureCase) -> ActualPayload:
        if not item.current_path.exists():
            return ActualPayload(None, None, f"missing current artifact: {item.current_path}")
        try:
            raw = item.current_path.read_bytes()
        except OSError as exc:
            return ActualPayload(None, None, f"cannot read current artifact {item.current_path}: {exc}")
        return parse_payload(raw, label=str(item.current_path))


class InMemoryActualSource:
    kind = "in_memory"

    def __init__(self, actuals: Mapping[object, object] | None) -> None:
        self.actuals = actuals or {}

    def _value_for(self, item: FixtureCase) -> object | None:
        for key in ((item.phase, item.case), item.label(), item.case, item.current_path):
            if key in self.actuals:
                return self.actuals[key]
        return None

    def load(self, item: FixtureCase) -> ActualPayload:
        value = self._value_for(item)
        if value is None:
            return ActualPayload(None, None, f"missing in-memory actual for {item.label()}")
        if isinstance(value, dict):
            return ActualPayload(value, canonical_json_bytes(value))
        if isinstance(value, bytes):
            return parse_payload(value, label=f"in-memory {item.label()}")
        if isinstance(value, str):
            return parse_payload(value.encode("utf-8"), label=f"in-memory {item.label()}")
        return ActualPayload(None, None, f"unsupported in-memory actual for {item.label()}")


class LiveCadCoreSource:
    kind = "live"

    def __init__(self, root: Path, binary: Path | None, timeout_seconds: float | None = None) -> None:
        self.root = root
        self.binary = binary or root / "build" / "cad-core"
        self.timeout_seconds = timeout_seconds

    def load(self, item: FixtureCase) -> ActualPayload:
        if not item.input_path.exists():
            return ActualPayload(None, None, f"missing fixture input: {item.input_path}")
        if not self.binary.exists():
            return ActualPayload(None, None, f"missing cad-core binary: {self.binary}")
        with tempfile.TemporaryDirectory(prefix="freecad-expected-live-") as directory:
            output = Path(directory) / f"{item.case}.cad-core.json"
            command = [str(self.binary), "recompute", str(item.input_path), "--output", str(output)]
            env = os.environ.copy()
            env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            try:
                completed = subprocess.run(
                    command,
                    cwd=self.root,
                    env=env,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=self.timeout_seconds,
                    check=False,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                return ActualPayload(None, None, f"cad-core runner failed for {item.label()}: {exc}")
            if completed.returncode != 0:
                return ActualPayload(
                    None,
                    None,
                    f"cad-core runner returned {completed.returncode} for {item.label()}",
                    completed.stdout,
                    completed.stderr,
                )
            if not output.exists():
                return ActualPayload(
                    None,
                    None,
                    f"cad-core runner produced no output for {item.label()}",
                    completed.stdout,
                    completed.stderr,
                )
            try:
                raw = output.read_bytes()
            except OSError as exc:
                return ActualPayload(None, None, f"cannot read live output for {item.label()}: {exc}")
            parsed = parse_payload(raw, label=f"live output {item.label()}")
            parsed.stdout = completed.stdout
            parsed.stderr = completed.stderr
            return parsed


class RustFfiActualSource:
    """Obtain one live actual response from ``cad_rs_recompute_json`` per fixture.

    This adapter deliberately has no comparison policy: the FFI's successful
    JSON response is passed straight to the existing expected-parity engine.
    A non-zero ABI status is an execution failure, even when its error JSON is
    well formed; accepting it as a fixture response would hide a Rust runtime
    failure behind the registry.
    """

    kind = "rust-ffi"

    def __init__(self, library: Path | None) -> None:
        self.library_path = library
        self._library: Any | None = None
        self._load_error: str | None = None

    def _load_library(self) -> Any | None:
        if self._library is not None or self._load_error is not None:
            return self._library
        if self.library_path is None:
            self._load_error = "missing Rust FFI library (--ffi-lib is required for --actual-source rust-ffi)"
            return None
        if not self.library_path.exists():
            self._load_error = f"missing Rust FFI library: {self.library_path}"
            return None
        try:
            library = ctypes.CDLL(str(self.library_path))
            recompute = library.cad_rs_recompute_json
            recompute.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
            recompute.restype = CadRsResult
            free_result = library.cad_rs_free_result
            free_result.argtypes = [ctypes.POINTER(CadRsResult)]
            free_result.restype = None
        except (AttributeError, OSError) as exc:
            self._load_error = f"cannot load Rust FFI library {self.library_path}: {exc}"
            return None
        self._library = library
        return library

    @staticmethod
    def _buffer_bytes(buffer: CadRsBuffer) -> bytes:
        return ctypes.string_at(buffer.ptr, buffer.len) if buffer.ptr and buffer.len else b""

    @staticmethod
    def _error_text(raw: bytes) -> str:
        return raw.decode("utf-8", errors="replace") if raw else ""

    def load(self, item: FixtureCase) -> ActualPayload:
        if not item.input_path.exists():
            return ActualPayload(None, None, f"missing fixture input: {item.input_path}")
        library = self._load_library()
        if library is None:
            return ActualPayload(None, None, self._load_error)
        try:
            request = item.input_path.read_bytes()
        except OSError as exc:
            return ActualPayload(None, None, f"cannot read fixture input {item.input_path}: {exc}")
        try:
            result = library.cad_rs_recompute_json(request, len(request))
        except (OSError, ValueError, TypeError) as exc:
            return ActualPayload(None, None, f"Rust FFI recompute failed for {item.label()}: {exc}")
        try:
            payload_raw = self._buffer_bytes(result.json)
            error_raw = self._buffer_bytes(result.error)
            if result.status != 0:
                detail = self._error_text(error_raw)
                suffix = f": {detail}" if detail else ""
                return ActualPayload(
                    None,
                    payload_raw or None,
                    f"Rust FFI cad_rs_recompute_json returned status {result.status} for {item.label()}{suffix}",
                    stderr=detail or None,
                )
            parsed = parse_payload(payload_raw, label=f"Rust FFI output {item.label()}")
            if error_raw:
                parsed.stderr = self._error_text(error_raw)
            return parsed
        finally:
            library.cad_rs_free_result(ctypes.byref(result))


def make_actual_source(
    kind: str,
    *,
    root: Path,
    binary: Path | None,
    ffi_library: Path | None,
    in_memory_actuals: Mapping[object, object] | None,
    timeout_seconds: float | None,
) -> SnapshotActualSource | InMemoryActualSource | LiveCadCoreSource | RustFfiActualSource | None:
    if kind == "snapshot":
        return SnapshotActualSource()
    if kind == "in_memory":
        return InMemoryActualSource(in_memory_actuals)
    if kind == "live":
        return LiveCadCoreSource(root, binary, timeout_seconds)
    if kind == "rust-ffi":
        return RustFfiActualSource(ffi_library)
    return None


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    """Replace a current artifact only after a complete JSON payload is available."""

    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "wb", prefix=f".{path.name}.", suffix=".tmp", dir=path.parent, delete=False
    ) as handle:
        temporary = Path(handle.name)
        handle.write(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True).encode("utf-8"))
        handle.write(b"\n")
    try:
        temporary.replace(path)
    finally:
        if temporary.exists():
            temporary.unlink()
