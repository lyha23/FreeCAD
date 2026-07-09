"""Fixture-role catalogue used by collection, comparison and generation paths."""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROLES_SCHEMA = "cad-core.freecad-expected-fixture-roles.v1"
VALID_ROLES = {"native", "protocol_only", "unsupported"}


@dataclass(frozen=True)
class FixtureCase:
    phase: str
    case: str
    role: str
    input_path: Path
    expected_path: Path
    ledger_path: Path
    current_path: Path

    def label(self) -> str:
        return f"{self.phase}/{self.case}"


@dataclass
class CatalogResult:
    cases: list[FixtureCase]
    errors: list[str]
    skipped: list[dict[str, str]]
    roles_path: Path
    roles_sha256: str | None


def sha256_bytes(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def relative(path: Path, root: Path) -> str:
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def expected_path_for(fixtures_root: Path, phase: str, case: str) -> Path:
    return fixtures_root / phase / "expected" / f"{case}.freecad.json"


def ledger_path_for(expected_path: Path) -> Path:
    suffix = ".freecad.json"
    if not expected_path.name.endswith(suffix):
        raise ValueError(f"not a native expected path: {expected_path}")
    return expected_path.with_name(expected_path.name[: -len(suffix)] + ".freecad.ledger.json")


def current_path_for(fixtures_root: Path, phase: str, case: str) -> Path:
    return fixtures_root / phase / "cad-core-res" / f"{case}.cad-core.json"


def _load_manifest(path: Path) -> tuple[dict[str, Any] | None, list[str], str | None]:
    if not path.exists():
        return None, [f"missing fixture roles manifest: {path}"], None
    try:
        raw = path.read_bytes()
        payload = json.loads(raw)
    except (OSError, json.JSONDecodeError) as exc:
        return None, [f"invalid fixture roles manifest {path}: {exc}"], None
    if not isinstance(payload, dict):
        return None, [f"fixture roles manifest must be an object: {path}"], None
    if payload.get("schemaVersion") != ROLES_SCHEMA:
        return None, [f"invalid fixture roles schemaVersion: {path}"], sha256_bytes(raw)
    return payload, [], sha256_bytes(raw)


def _legacy_native_roles(fixtures_root: Path) -> list[dict[str, str]]:
    roles: list[dict[str, str]] = []
    for expected_path in sorted(fixtures_root.glob("*/expected/*.freecad.json")):
        roles.append(
            {
                "phase": expected_path.parent.parent.name,
                "case": expected_path.name[: -len(".freecad.json")],
                "role": "native",
            }
        )
    return roles


def _entry_error(entry: Any, index: int) -> str | None:
    if not isinstance(entry, dict):
        return f"fixture role {index} must be an object"
    phase = entry.get("phase")
    case = entry.get("case")
    role = entry.get("role")
    if not isinstance(phase, str) or not phase:
        return f"fixture role {index} missing phase"
    if not isinstance(case, str) or not case:
        return f"fixture role {index} missing case"
    if role not in VALID_ROLES:
        return f"fixture role {phase}/{case} has invalid role: {role}"
    if role == "unsupported":
        for key in ("reason", "authority", "nextAction", "closeCondition"):
            if not isinstance(entry.get(key), str) or not entry[key]:
                return f"unsupported fixture role {phase}/{case} missing {key}"
    return None


def _input_paths(fixtures_root: Path, phase: str | None) -> list[Path]:
    pattern = f"{phase}/*.json" if phase else "*/*.json"
    return sorted(path for path in fixtures_root.glob(pattern) if path.is_file())


def load_catalog(
    root: Path,
    *,
    phase: str | None = None,
    case: str | None = None,
    roles_path: Path | None = None,
) -> CatalogResult:
    """Load and audit selected roles before returning native comparison cases.

    The checked-in bootstrap manifest deliberately enables the explicit
    ``legacyNativeExpectedDiscovery`` bridge while all historic phases are
    migrated.  Custom/complete manifests do not get this fallback and are
    audited against every selected input fixture.
    """

    root = Path(root)
    fixtures_root = root / "fixtures"
    manifest_path = roles_path or Path(__file__).with_name("fixture_roles.v1.json")
    manifest, errors, roles_sha256 = _load_manifest(manifest_path)
    if manifest is None:
        return CatalogResult([], errors, [], manifest_path, roles_sha256)

    raw_entries = manifest.get("roles", manifest.get("cases", []))
    if not isinstance(raw_entries, list):
        return CatalogResult([], [*errors, "fixture roles must be a list"], [], manifest_path, roles_sha256)
    by_key: dict[tuple[str, str], dict[str, Any]] = {}
    for index, entry in enumerate(raw_entries):
        error = _entry_error(entry, index)
        if error:
            errors.append(error)
            continue
        assert isinstance(entry, dict)
        key = (str(entry["phase"]), str(entry["case"]))
        if key in by_key:
            errors.append(f"duplicate fixture role: {key[0]}/{key[1]}")
            continue
        by_key[key] = entry

    # Explicit roles always win over the bootstrap inventory.  This lets the
    # next provenance slice turn a legacy native case into protocol_only
    # without first deleting the compatibility bridge.
    if manifest.get("legacyNativeExpectedDiscovery") is True:
        for entry in _legacy_native_roles(fixtures_root):
            key = (entry["phase"], entry["case"])
            by_key.setdefault(key, entry)

    selected_entries = [
        entry
        for (entry_phase, entry_case), entry in sorted(by_key.items())
        if (phase is None or entry_phase == phase) and (case is None or entry_case == case)
    ]

    if manifest.get("requireCompleteInputCoverage") is True:
        for input_path in _input_paths(fixtures_root, phase):
            key = (input_path.parent.name, input_path.stem)
            if case is not None and key[1] != case:
                continue
            if key not in by_key:
                errors.append(f"fixture input has no role: {relative(input_path, root)}")

    cases: list[FixtureCase] = []
    skipped: list[dict[str, str]] = []
    for entry in selected_entries:
        entry_phase = str(entry["phase"])
        entry_case = str(entry["case"])
        role = str(entry["role"])
        input_path = fixtures_root / entry_phase / f"{entry_case}.json"
        expected_path = expected_path_for(fixtures_root, entry_phase, entry_case)
        ledger_path = ledger_path_for(expected_path)
        current_path = current_path_for(fixtures_root, entry_phase, entry_case)
        if not input_path.exists():
            errors.append(f"fixture role input is missing: {relative(input_path, root)}")
            continue
        if role == "native":
            if not expected_path.exists():
                errors.append(f"native fixture expected is missing: {relative(expected_path, root)}")
                continue
            if not ledger_path.exists():
                errors.append(f"native fixture ledger is missing: {relative(ledger_path, root)}")
                continue
            cases.append(
                FixtureCase(
                    phase=entry_phase,
                    case=entry_case,
                    role=role,
                    input_path=input_path,
                    expected_path=expected_path,
                    ledger_path=ledger_path,
                    current_path=current_path,
                )
            )
            continue
        if role == "protocol_only":
            protocol_path = fixtures_root / entry_phase / "expected" / f"{entry_case}.expeted.json"
            if not protocol_path.exists():
                errors.append(f"protocol-only fixture expected is missing: {relative(protocol_path, root)}")
            if expected_path.exists() or ledger_path.exists():
                errors.append(f"protocol-only fixture retains native artifacts: {entry_phase}/{entry_case}")
            skipped.append({"phase": entry_phase, "case": entry_case, "role": role, "reason": "protocol_only"})
            continue
        skipped.append(
            {
                "phase": entry_phase,
                "case": entry_case,
                "role": role,
                "reason": str(entry.get("reason", "unsupported")),
            }
        )

    return CatalogResult(cases, errors, skipped, manifest_path, roles_sha256)
