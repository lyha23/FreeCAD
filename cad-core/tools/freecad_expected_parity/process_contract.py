"""Shared hermetic runner and schema helpers for native process contracts.

Unlike document fixtures, these receipts describe executable entrypoints.  Every run
gets an isolated FreeCAD user/config/temp boundary and a deliberately small environment.
Callers remain responsible for interpreting the executable's public result.
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Mapping, Sequence


SCHEMA = "freecad-native-process-contract/v1"
BASE_ENVIRONMENT_ALLOWLIST = ("PATH", "LANG", "LC_ALL", "SYSTEMROOT")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def artifact(path: Path) -> dict[str, Any]:
    resolved = path.resolve()
    return {"path": str(resolved), "sha256": sha256(resolved)}


def atomic_write(path: Path, payload: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def clean_environment(extra: Mapping[str, str] | None = None) -> dict[str, str]:
    environment = {
        key: os.environ[key]
        for key in BASE_ENVIRONMENT_ALLOWLIST
        if key in os.environ
    }
    environment.setdefault("PATH", "/usr/bin:/bin")
    if extra:
        environment.update({str(key): str(value) for key, value in extra.items()})
    return environment


def normalize_text(text: str, replacements: Mapping[str, str]) -> str:
    normalized = text.replace("\r\n", "\n")
    for source, target in sorted(
        replacements.items(), key=lambda item: len(item[0]), reverse=True
    ):
        normalized = normalized.replace(source, target)
    return normalized


def process_succeeded(receipt: Mapping[str, Any], expected_exit_code: int) -> bool:
    """Return whether one process receipt ended normally with the requested code."""

    return (
        receipt.get("timedOut") is False
        and receipt.get("signal") is None
        and receipt.get("exitCode") == expected_exit_code
    )


@dataclass(frozen=True)
class ProcessSpec:
    executable: Path
    argv: Sequence[str]
    cwd: Path
    environment: Mapping[str, str]
    timeout_seconds: int = 30
    replacements: Mapping[str, str] = field(default_factory=dict)


def run_process(spec: ProcessSpec) -> dict[str, Any]:
    """Run one fixed executable and return the complete normalized process receipt."""

    executable = spec.executable.resolve()
    argv = [str(executable), *[str(item) for item in spec.argv]]
    timed_out = False
    try:
        completed = subprocess.run(
            argv,
            cwd=spec.cwd,
            env=dict(spec.environment),
            capture_output=True,
            text=True,
            timeout=spec.timeout_seconds,
        )
        returncode = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        returncode = None
        stdout = exc.stdout.decode() if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode() if isinstance(exc.stderr, bytes) else (exc.stderr or "")

    normalize = lambda value: normalize_text(value, spec.replacements)
    return {
        "executable": artifact(executable),
        "argv": [normalize(item) for item in argv],
        "environment": {
            key: normalize(value) for key, value in sorted(spec.environment.items())
        },
        "cwd": normalize(str(spec.cwd.resolve())),
        "timeoutSeconds": spec.timeout_seconds,
        "exitCode": returncode if returncode is not None and returncode >= 0 else None,
        "signal": -returncode if returncode is not None and returncode < 0 else None,
        "timedOut": timed_out,
        "stdout": normalize(stdout),
        "stderr": normalize(stderr),
    }
