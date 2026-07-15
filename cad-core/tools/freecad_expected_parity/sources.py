"""Actual-payload adapters hidden behind the parity engine seam."""

from __future__ import annotations

import ctypes
import hashlib
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from .catalog import FixtureCase
try:
    from tools.element_map_producer_trace import TraceValidationError, validate_trace
except ModuleNotFoundError:
    from element_map_producer_trace import TraceValidationError, validate_trace


class CadRsBuffer(ctypes.Structure):
    """Byte buffer returned by the Rust C ABI's ``CadRsResult``."""

    _fields_ = [("ptr", ctypes.c_void_p), ("len", ctypes.c_size_t)]


class CadRsResult(ctypes.Structure):
    """The narrow recompute-result ABI from ``cad-core-ffi/include/cad_rs.h``."""

    _fields_ = [("status", ctypes.c_int32), ("json", CadRsBuffer), ("error", CadRsBuffer)]


class CadRsRecomputeArtifactsResult(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int32),
        ("response", CadRsBuffer),
        ("producer_trace", CadRsBuffer),
        ("error", CadRsBuffer),
    ]


class CadKernelV2Buffer(ctypes.Structure):
    """Byte buffer owned by ``freecad_kernel_ffi_v2``."""

    _fields_ = [("ptr", ctypes.c_void_p), ("len", ctypes.c_size_t)]


class CadKernelV2Result(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int32),
        ("json", CadKernelV2Buffer),
        ("error", CadKernelV2Buffer),
    ]


class CadKernelV2CreateResult(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int32),
        ("handle", ctypes.c_void_p),
        ("error", CadKernelV2Buffer),
    ]


@dataclass
class ActualPayload:
    payload: dict[str, Any] | None
    raw_bytes: bytes | None
    error: str | None = None
    stdout: str | None = None
    stderr: str | None = None
    producer_trace: dict[str, Any] | None = None
    producer_trace_raw: bytes | None = None
    producer_trace_error: str | None = None
    producer_trace_path: str | None = None


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
        parsed = parse_payload(raw, label=str(item.current_path))
        if parsed.error or parsed.payload is None:
            return parsed
        trace_path = item.current_trace_path()
        parsed.producer_trace_path = str(trace_path)
        if not trace_path.exists():
            parsed.producer_trace_error = f"missing producer trace: {trace_path}"
            return parsed
        try:
            trace_raw = trace_path.read_bytes()
            trace_payload = json.loads(trace_raw)
            request_payload = json.loads(item.input_path.read_text(encoding="utf-8"))
            validate_trace(
                trace_payload,
                input_document=request_payload,
                response_document=parsed.payload,
            )
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, TraceValidationError) as exc:
            parsed.producer_trace_error = f"invalid producer trace {trace_path}: {exc}"
            return parsed
        parsed.producer_trace = trace_payload
        parsed.producer_trace_raw = trace_raw
        return parsed


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
            producer_trace = output.with_name(
                output.name[: -len(".json")] + ".producer-trace.json"
            )
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
            if parsed.error or parsed.payload is None:
                return parsed
            if not producer_trace.exists():
                parsed.producer_trace_error = (
                    f"cad-core runner produced no producer trace for {item.label()}"
                )
                return parsed
            try:
                trace_raw = producer_trace.read_bytes()
                trace_payload = json.loads(trace_raw)
                request_payload = json.loads(item.input_path.read_text(encoding="utf-8"))
                validate_trace(
                    trace_payload,
                    input_document=request_payload,
                    response_document=parsed.payload,
                )
            except (OSError, UnicodeDecodeError, json.JSONDecodeError, TraceValidationError) as exc:
                parsed.producer_trace_error = (
                    f"invalid cad-core producer trace for {item.label()}: {exc}"
                )
                return parsed
            parsed.producer_trace = trace_payload
            parsed.producer_trace_raw = trace_raw
            parsed.producer_trace_path = "live:in-memory"
            return parsed


class RustFfiActualSource:
    """Obtain one response and diagnostic trace from one Rust artifacts FFI call.

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
            recompute = library.cad_rs_recompute_artifacts_json
            recompute.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
            recompute.restype = CadRsRecomputeArtifactsResult
            free_result = library.cad_rs_free_recompute_artifacts_result
            free_result.argtypes = [ctypes.POINTER(CadRsRecomputeArtifactsResult)]
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
            result = library.cad_rs_recompute_artifacts_json(request, len(request))
        except (OSError, ValueError, TypeError) as exc:
            return ActualPayload(None, None, f"Rust FFI recompute failed for {item.label()}: {exc}")
        try:
            payload_raw = self._buffer_bytes(result.response)
            trace_raw = self._buffer_bytes(result.producer_trace)
            error_raw = self._buffer_bytes(result.error)
            if result.status != 0:
                detail = self._error_text(error_raw)
                suffix = f": {detail}" if detail else ""
                return ActualPayload(
                    None,
                    payload_raw or None,
                    f"Rust FFI cad_rs_recompute_artifacts_json returned status {result.status} for {item.label()}{suffix}",
                    stderr=detail or None,
                )
            parsed = parse_payload(payload_raw, label=f"Rust FFI output {item.label()}")
            if parsed.error or parsed.payload is None:
                return parsed
            try:
                trace_payload = json.loads(trace_raw)
                validate_trace(
                    trace_payload,
                    input_bytes=request,
                    response_bytes=payload_raw,
                )
            except (UnicodeDecodeError, json.JSONDecodeError, TraceValidationError) as exc:
                return ActualPayload(
                    None,
                    payload_raw,
                    f"invalid Rust FFI producer trace for {item.label()}: {exc}",
                )
            parsed.producer_trace = trace_payload
            parsed.producer_trace_raw = trace_raw
            parsed.producer_trace_path = "rust-ffi:in-memory"
            if error_raw:
                parsed.stderr = self._error_text(error_raw)
            return parsed
        finally:
            library.cad_rs_free_recompute_artifacts_result(ctypes.byref(result))


class FreeCadKernelV2ActualSource:
    """Call the native v2 handle ABI once per selected fixture.

    The adapter owns no comparison policy and never reads a current/snapshot
    response.  It records enough call-level evidence for a report to prove the
    response came from the selected dylib and that all library-owned buffers
    and the handle crossed their paired release operations.
    """

    kind = "freecad-kernel-v2"

    def __init__(self, library: Path | None, timeout_seconds: float | None = None) -> None:
        self.library_path = library.resolve() if library is not None else None
        self.timeout_seconds = timeout_seconds
        self._calls: list[dict[str, Any]] = []
        self._create_count = 0
        self._destroy_count = 0
        self._adapter_version = "freecad-expected-parity.freecad-kernel-v2.v1"
        self._source_receipt = self._source_authority()

    @staticmethod
    def _sha256(raw: bytes) -> str:
        return "sha256:" + hashlib.sha256(raw).hexdigest()

    def _resource_root(self) -> Path | None:
        if self.library_path is None:
            return None
        parent = self.library_path.parent
        return parent.parent if parent.name == "lib" else parent

    def _source_authority(self) -> dict[str, Any]:
        resource_root = self._resource_root()
        if resource_root is None or len(resource_root.parents) < 2:
            return {"status": "unavailable", "reason": "cannot derive source repository"}
        repository = resource_root.parents[1]
        if not (repository / ".git").exists():
            return {"status": "unavailable", "repository": str(repository)}

        def git(*arguments: str) -> subprocess.CompletedProcess[str]:
            return subprocess.run(
                ["git", *arguments],
                cwd=repository,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )

        head = git("rev-parse", "HEAD")
        status = git("status", "--short")
        if head.returncode != 0 or status.returncode != 0:
            return {
                "status": "unavailable",
                "repository": str(repository),
                "error": (head.stderr + status.stderr).strip(),
            }
        snapshot = repository / "freecad-kernel/provenance/source-snapshot.json"
        return {
            "status": "recorded",
            "repository": str(repository),
            "head": head.stdout.strip(),
            "dirtyPaths": status.stdout.splitlines(),
            "sourceSnapshot": {
                "path": str(snapshot),
                "sha256": self._sha256(snapshot.read_bytes()) if snapshot.is_file() else None,
            },
        }

    def load(self, item: FixtureCase) -> ActualPayload:
        if not item.input_path.is_file():
            return ActualPayload(None, None, f"missing fixture input: {item.input_path}")
        if self.library_path is None:
            return ActualPayload(None, None, "missing v2 FFI library (--ffi-lib is required)")
        if not self.library_path.is_file():
            return ActualPayload(None, None, f"missing v2 FFI library: {self.library_path}")
        resource_root = self._resource_root()
        if resource_root is None:
            return ActualPayload(None, None, "cannot derive v2 resource root")
        runner = resource_root / "bin" / "FreeCADKernelV2ActualRunner"
        if not runner.is_file():
            return ActualPayload(None, None, f"missing v2 actual runner: {runner}")
        try:
            request = item.input_path.read_bytes()
        except OSError as exc:
            return ActualPayload(None, None, f"cannot read fixture input {item.input_path}: {exc}")
        with tempfile.TemporaryDirectory(prefix="freecad-kernel-v2-actual-") as directory:
            response_path = Path(directory) / "response.json"
            evidence_path = Path(directory) / "evidence.json"
            command = [
                str(runner),
                str(self.library_path),
                str(resource_root),
                str(item.input_path),
                str(response_path),
                str(evidence_path),
            ]
            try:
                completed = subprocess.run(
                    command,
                    cwd=resource_root,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=self.timeout_seconds,
                    check=False,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                return ActualPayload(
                    None, None, f"v2 actual runner failed for {item.label()}: {exc}"
                )
            try:
                call = json.loads(evidence_path.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
                return ActualPayload(
                    None,
                    None,
                    f"v2 actual runner produced no valid evidence for {item.label()}: {exc}",
                    completed.stdout,
                    completed.stderr,
                )
            call["case"] = item.label()
            self._calls.append(call)
            self._create_count += int(call.get("createCount", 0))
            self._destroy_count += int(call.get("destroyCount", 0))
            if completed.returncode != 0:
                return ActualPayload(
                    None,
                    None,
                    f"v2 actual runner returned {completed.returncode} for {item.label()}: {completed.stderr.strip()}",
                    completed.stdout,
                    completed.stderr,
                )
            try:
                payload_raw = response_path.read_bytes()
            except OSError as exc:
                return ActualPayload(None, None, f"v2 actual runner produced no response: {exc}")
            expected_evidence = {
                "libraryPath": str(self.library_path),
                "librarySha256": self._sha256(self.library_path.read_bytes()),
                "requestSha256": self._sha256(request),
                "rawResponseSha256": self._sha256(payload_raw),
                "createCount": 1,
                "destroyCount": 1,
                "handleCreated": True,
                "handleDestroyed": True,
                "createResultFreed": True,
                "versionResultFreed": True,
                "recomputeResultFreed": True,
            }
            mismatches = {
                key: {"expected": expected, "actual": call.get(key)}
                for key, expected in expected_evidence.items()
                if call.get(key) != expected
            }
            if mismatches:
                return ActualPayload(
                    None,
                    payload_raw,
                    "v2 actual runner evidence mismatch: " + json.dumps(mismatches, sort_keys=True),
                    completed.stdout,
                    completed.stderr,
                )
            parsed = parse_payload(payload_raw, label=f"freecad-kernel-v2 output {item.label()}")
            parsed.stdout = completed.stdout
            parsed.stderr = completed.stderr
            return parsed

    def evidence(self) -> dict[str, Any]:
        library_sha = None
        if self.library_path is not None and self.library_path.is_file():
            library_sha = self._sha256(self.library_path.read_bytes())
        return {
            "adapterVersion": self._adapter_version,
            "library": str(self.library_path) if self.library_path is not None else None,
            "librarySha256": library_sha,
            "resourceRoot": (
                str(self._resource_root()) if self._resource_root() is not None else None
            ),
            "execution": "isolated-helper-process",
            "sourceAuthority": self._source_receipt,
            "createCount": self._create_count,
            "destroyCount": self._destroy_count,
            "calls": self._calls,
        }


def make_actual_source(
    kind: str,
    *,
    root: Path,
    binary: Path | None,
    ffi_library: Path | None,
    in_memory_actuals: Mapping[object, object] | None,
    timeout_seconds: float | None,
) -> (
    SnapshotActualSource
    | InMemoryActualSource
    | LiveCadCoreSource
    | RustFfiActualSource
    | FreeCadKernelV2ActualSource
    | None
):
    if kind == "snapshot":
        return SnapshotActualSource()
    if kind == "in_memory":
        return InMemoryActualSource(in_memory_actuals)
    if kind == "live":
        return LiveCadCoreSource(root, binary, timeout_seconds)
    if kind == "rust-ffi":
        return RustFfiActualSource(ffi_library)
    if kind == "freecad-kernel-v2":
        return FreeCadKernelV2ActualSource(ffi_library, timeout_seconds)
    return None


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    """Replace a current artifact only after a complete JSON payload is available."""

    atomic_write_json_group([(path, payload)])


def _json_artifact_bytes(payload: dict[str, Any]) -> bytes:
    return (
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True).encode("utf-8")
        + b"\n"
    )


def _stage_bytes(path: Path, value: bytes) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "wb", prefix=f".{path.name}.", suffix=".tmp", dir=path.parent, delete=False
    ) as handle:
        temporary = Path(handle.name)
        handle.write(value)
        handle.flush()
        os.fsync(handle.fileno())
    return temporary


def atomic_write_json_group(artifacts: list[tuple[Path, dict[str, Any]]]) -> None:
    """Publish a response/trace set as one rollback-safe materialization unit."""

    if len({path for path, _payload in artifacts}) != len(artifacts):
        raise ValueError("duplicate path in atomic JSON artifact group")

    staged: dict[Path, Path] = {}
    originals: dict[Path, bytes | None] = {}
    published: list[Path] = []
    try:
        for path, payload in artifacts:
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists() and not path.is_file():
                raise OSError(f"artifact target is not a regular file: {path}")
            originals[path] = path.read_bytes() if path.exists() else None
            staged[path] = _stage_bytes(path, _json_artifact_bytes(payload))

        for path, _payload in artifacts:
            staged[path].replace(path)
            published.append(path)
    except Exception as exc:
        rollback_errors: list[str] = []
        for path in reversed(published):
            try:
                original = originals[path]
                if original is None:
                    path.unlink(missing_ok=True)
                else:
                    _stage_bytes(path, original).replace(path)
            except OSError as rollback_exc:
                rollback_errors.append(f"{path}: {rollback_exc}")
        if rollback_errors:
            raise RuntimeError(
                f"atomic JSON group failed: {exc}; rollback failed: {'; '.join(rollback_errors)}"
            ) from exc
        raise
    finally:
        for temporary in staged.values():
            temporary.unlink(missing_ok=True)
