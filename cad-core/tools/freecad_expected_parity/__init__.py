"""FreeCAD native expected versus CAD Core release-gate evaluation."""

from .engine import evaluate, materialize_current
from .model import EvaluationRequest, GenerationReport, MaterializeRequest, ParityReport

__all__ = [
    "EvaluationRequest",
    "GenerationReport",
    "MaterializeRequest",
    "ParityReport",
    "evaluate",
    "materialize_current",
]
