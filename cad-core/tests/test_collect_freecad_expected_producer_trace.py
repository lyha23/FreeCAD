"""Focused contract tests for native producer-trace collection helpers."""

from __future__ import annotations

import copy
import importlib.util
import unittest
from pathlib import Path


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


def trace() -> dict:
    state = "state:sha256:initial"
    ledger = "ledger:sha256:after"
    return {
        "schemaVersion": collector.PRODUCER_TRACE_SCHEMA,
        "transactions": [{"sequence": 1, "eventRange": [2, 5], "outcome": "success"}],
        "events": [
            {"sequence": 1, "scopeSequence": 0, "slice": "initial", "beforeSnapshot": state, "afterSnapshot": state},
            {"sequence": 2, "scopeSequence": 0, "slice": "document.recompute.begin", "beforeSnapshot": state, "afterSnapshot": state},
            {"sequence": 3, "scopeSequence": 1, "slice": "scope.begin", "beforeSnapshot": state, "afterSnapshot": state},
            {"sequence": 4, "scopeSequence": 1, "slice": "scope.end", "beforeSnapshot": state, "afterSnapshot": ledger},
            {"sequence": 5, "scopeSequence": 0, "slice": "document.recompute.end", "beforeSnapshot": ledger, "afterSnapshot": ledger},
        ],
        "ledgerSnapshots": {
            state: {"kind": "state", "payload": {}},
            ledger: {"kind": "ledger", "payload": {}},
        },
        "stringTableSnapshots": {},
        "mapperSnapshots": {},
    }


class ProducerTraceCollectorTests(unittest.TestCase):
    def test_trace_path_tracks_expected_stem(self) -> None:
        expected = Path("/tmp/case.freecad.json")
        self.assertEqual(
            collector.producer_trace_path_for_expected(expected),
            Path("/tmp/case.freecad.producer-trace.json"),
        )

    def test_valid_trace_is_accepted(self) -> None:
        self.assertEqual(collector.validate_producer_trace(trace())["schemaVersion"], collector.PRODUCER_TRACE_SCHEMA)

    def test_missing_checkpoint_is_rejected(self) -> None:
        malformed = copy.deepcopy(trace())
        del malformed["ledgerSnapshots"]["ledger:sha256:after"]
        with self.assertRaisesRegex(RuntimeError, "missing snapshot"):
            collector.validate_producer_trace(malformed)

    def test_unclosed_scope_is_rejected(self) -> None:
        malformed = trace()
        malformed["events"] = malformed["events"][:-2]
        with self.assertRaisesRegex(RuntimeError, "unclosed scopes"):
            collector.validate_producer_trace(malformed)


if __name__ == "__main__":
    unittest.main()
