"""Validation helpers for FreeCAD/CAD Core ElementMap producer traces."""

from .compare import compare_traces
from .model import ComparisonResult, ValidatedTrace
from .projection import project_trace
from .validate import (
    TraceValidationError,
    canonical_json_sha256,
    canonical_sha256,
    validate_trace,
)

__all__ = [
    "ComparisonResult",
    "TraceValidationError",
    "ValidatedTrace",
    "canonical_sha256",
    "canonical_json_sha256",
    "compare_traces",
    "project_trace",
    "validate_trace",
]
