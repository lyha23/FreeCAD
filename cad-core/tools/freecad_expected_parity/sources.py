"""Actual-payload adapters hidden behind the parity engine seam."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from .catalog import FixtureCase


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


def make_actual_source(
    kind: str,
    *,
    root: Path,
    binary: Path | None,
    in_memory_actuals: Mapping[object, object] | None,
    timeout_seconds: float | None,
) -> SnapshotActualSource | InMemoryActualSource | LiveCadCoreSource | None:
    if kind == "snapshot":
        return SnapshotActualSource()
    if kind == "in_memory":
        return InMemoryActualSource(in_memory_actuals)
    if kind == "live":
        return LiveCadCoreSource(root, binary, timeout_seconds)
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
