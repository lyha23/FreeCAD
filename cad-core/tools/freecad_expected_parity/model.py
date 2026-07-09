"""Public request and report types for the expected-release parity gate.

The module deliberately exposes requests and aggregate reports only.  Fixture
catalogue entries, sources and registry records stay implementation details so
the command-line adapters and tests cross the same small seam.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Mapping


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class EvaluationRequest:
    """Select expected fixtures and the source used to obtain actual payloads."""

    root: Path | str = CAD_CORE_ROOT
    phase: str | None = None
    case: str | None = None
    source_kind: str = "snapshot"
    binary: Path | str | None = None
    roles_path: Path | str | None = None
    registry_path: Path | str | None = None
    in_memory_actuals: Mapping[object, object] | None = None
    validate_ledger: bool = True
    timeout_seconds: float | None = None

    def root_path(self) -> Path:
        return Path(self.root)


@dataclass(frozen=True)
class MaterializeRequest:
    """Select live fixture runs whose validated results may replace current files."""

    root: Path | str = CAD_CORE_ROOT
    phase: str | None = None
    case: str | None = None
    binary: Path | str | None = None
    roles_path: Path | str | None = None
    timeout_seconds: float | None = None
    validate_ledger: bool = True

    def root_path(self) -> Path:
        return Path(self.root)


@dataclass
class ArtifactEvidence:
    input_sha256: str | None = None
    expected_sha256: str | None = None
    ledger_sha256: str | None = None
    actual_raw_sha256: str | None = None
    current_raw_sha256: str | None = None
    actual_normalized_sha256: str | None = None
    current_normalized_sha256: str | None = None
    current_fresh: bool | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "inputSha256": self.input_sha256,
            "expectedSha256": self.expected_sha256,
            "ledgerSha256": self.ledger_sha256,
            "actualRawSha256": self.actual_raw_sha256,
            "currentRawSha256": self.current_raw_sha256,
            "actualNormalizedSha256": self.actual_normalized_sha256,
            "currentNormalizedSha256": self.current_normalized_sha256,
            "currentFresh": self.current_fresh,
        }


@dataclass
class CaseReport:
    phase: str
    case: str
    role: str
    status: str
    expected: str
    current: str
    input: str
    artifact_evidence: ArtifactEvidence = field(default_factory=ArtifactEvidence)
    diffs: list[dict[str, Any]] = field(default_factory=list)
    preflight_errors: list[str] = field(default_factory=list)
    source_error: str | None = None

    def to_dict(self) -> dict[str, Any]:
        categories: dict[str, int] = {}
        decisions: dict[str, int] = {}
        for diff in self.diffs:
            category = str(diff.get("category", "json"))
            categories[category] = categories.get(category, 0) + 1
            decision = diff.get("decision")
            if isinstance(decision, str) and decision:
                decisions[decision] = decisions.get(decision, 0) + 1
        payload: dict[str, Any] = {
            "phase": self.phase,
            "case": self.case,
            "role": self.role,
            "status": self.status,
            "input": self.input,
            "expected": self.expected,
            "current": self.current,
            "artifactEvidence": self.artifact_evidence.to_dict(),
            "diffCount": len(self.diffs),
            "categories": dict(sorted(categories.items())),
            "decisions": dict(sorted(decisions.items())),
            "diffs": self.diffs,
        }
        if self.preflight_errors:
            payload["preflightErrors"] = self.preflight_errors
        if self.source_error:
            payload["sourceError"] = self.source_error
        return payload


@dataclass
class ParityReport:
    schema_version: str
    selection: dict[str, Any]
    run_evidence: dict[str, Any]
    exact_status: str
    semantic_status: str
    release_status: str
    release_gate_passed: bool
    summary: dict[str, Any]
    cases: list[CaseReport]
    registry_audit: dict[str, Any]
    preflight: dict[str, Any]

    @property
    def status(self) -> str:
        """Compatibility aggregate used by the old report-only CLI."""
        if self.release_status == "invalid":
            return "invalid"
        if self.exact_status == "not_evaluated":
            return "invalid"
        return "green" if self.exact_status == "green" else "red"

    def to_dict(self) -> dict[str, Any]:
        return {
            "schemaVersion": self.schema_version,
            "selection": self.selection,
            "runEvidence": self.run_evidence,
            "exactStatus": self.exact_status,
            "semanticStatus": self.semantic_status,
            "releaseStatus": self.release_status,
            "releaseGatePassed": self.release_gate_passed,
            "status": self.status,
            "summary": self.summary,
            "cases": [item.to_dict() for item in self.cases],
            "registryAudit": self.registry_audit,
            "preflight": self.preflight,
        }


@dataclass
class GenerationReport:
    schema_version: str
    status: str
    selection: dict[str, Any]
    summary: dict[str, Any]
    cases: list[dict[str, Any]]
    preflight: dict[str, Any]

    def to_dict(self) -> dict[str, Any]:
        return {
            "schemaVersion": self.schema_version,
            "status": self.status,
            "selection": self.selection,
            "summary": self.summary,
            "cases": self.cases,
            "preflight": self.preflight,
        }
