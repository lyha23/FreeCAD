"""Focused contract tests for native producer-trace collection helpers."""

from __future__ import annotations

import copy
import importlib.util
import unittest
from pathlib import Path

from tests.producer_trace_fixture import producer_trace, resequence


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


def trace() -> dict:
    return producer_trace()


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
        malformed["ledgerSnapshots"].clear()
        with self.assertRaisesRegex(RuntimeError, "snapshot missing"):
            collector.validate_producer_trace(malformed)

    def test_unclosed_scope_is_rejected(self) -> None:
        malformed = trace()
        malformed["events"].pop(4)
        resequence(malformed)
        with self.assertRaisesRegex(RuntimeError, "unclosed scopes"):
            collector.validate_producer_trace(malformed)

    def test_binding_declares_and_checks_canonical_native_snapshot_hashes(self) -> None:
        request = {"request": True}
        response = {"response": True}
        bound = collector.bind_producer_trace_artifacts(
            producer_trace(),
            input_document=request,
            response_document=response,
        )
        self.assertEqual("FreeCAD", bound["producer"]["name"])
        self.assertEqual(
            "canonical-json-sha256-v1",
            bound["producer"]["snapshotPayloadHashAlgorithm"],
        )
        for group in ("stringTableSnapshots", "ledgerSnapshots", "mapperSnapshots"):
            for snapshot in bound[group].values():
                self.assertRegex(snapshot["canonicalPayloadSha256"], r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
