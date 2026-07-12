"""Typed, immutable views over a validated producer trace."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Mapping


@dataclass(frozen=True)
class SemanticScope:
    sequence: int
    parent: int
    transaction: int
    object_name: str
    producer: str
    producer_kind: str
    stage: str
    sibling_ordinal: int
    path: tuple[str, ...]

    @property
    def identity(self) -> tuple[Any, ...]:
        return (
            self.transaction,
            self.path[:-1],
            self.object_name,
            self.producer_kind,
            self.stage,
            self.sibling_ordinal,
        )


@dataclass(frozen=True)
class ProjectedEvent:
    raw: Mapping[str, Any]
    scope_path: tuple[str, ...]
    identity: tuple[Any, ...]

    @property
    def sequence(self) -> int:
        return int(self.raw["sequence"])


@dataclass(frozen=True)
class ValidatedTrace:
    payload: Mapping[str, Any]
    transactions: tuple[Mapping[str, Any], ...]
    events: tuple[ProjectedEvent, ...]
    scopes: Mapping[int, SemanticScope]
    scope_order: tuple[int, ...]
    events_by_scope: Mapping[tuple[int, int], tuple[ProjectedEvent, ...]]
    snapshots: Mapping[str, Mapping[str, Any]]
    object_keys: Mapping[str, tuple[str, str, str]]
    object_tags: Mapping[str, int]
    executed_objects_by_transaction: Mapping[int, frozenset[str]]
    projected_snapshot_cache: dict[str, Any] = field(
        default_factory=dict, compare=False, repr=False
    )
    projected_summary_cache: dict[str, Any] = field(
        default_factory=dict, compare=False, repr=False
    )


@dataclass(frozen=True)
class NormalizationRecord:
    reason_code: str
    semantic_scope_path: tuple[str, ...] = ()
    expected_json_pointer: str | None = None
    actual_json_pointer: str | None = None
    expected_raw: Any = None
    actual_raw: Any = None
    semantic_key: Any = None


@dataclass(frozen=True)
class ComparisonResult:
    status: str
    classification: str
    transaction_ordinal: int | None = None
    semantic_scope_path: tuple[str, ...] = ()
    expected_event: Mapping[str, Any] | None = None
    actual_event: Mapping[str, Any] | None = None
    expected_event_identity: tuple[Any, ...] | None = None
    actual_event_identity: tuple[Any, ...] | None = None
    json_pointer: str | None = None
    expected_value: Any = None
    actual_value: Any = None
    downstream_drift_count: int = 0
    before_alignment: str = "not_compared"
    after_alignment: str = "not_compared"
    owner: str = "unknown"
    detail: str = ""
    equivalence: str = "none"
    normalizations: tuple[NormalizationRecord, ...] = ()
    raw_difference_count: int = 0
    semantic_difference_count: int = 0
