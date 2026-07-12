from __future__ import annotations

import unittest

from tests.producer_trace_fixture import producer_trace
from tools.element_map_producer_trace import project_trace


class ProducerTraceProjectionTests(unittest.TestCase):
    def test_scope_ids_do_not_change_semantic_paths(self) -> None:
        first = project_trace(producer_trace(scope_sequence=7))
        second = project_trace(producer_trace(scope_sequence=41))
        self.assertEqual(first.events[1].scope_path, second.events[1].scope_path)
        self.assertEqual(first.events[3].identity, second.events[3].identity)

    def test_event_identity_uses_occurrence_without_raw_sequence(self) -> None:
        projected = project_trace(producer_trace())
        select = next(event for event in projected.events if event.raw["slice"] == "maker.select")
        self.assertEqual(select.identity[-1], 1)
        self.assertNotIn(select.sequence, select.identity[:-1])


if __name__ == "__main__":
    unittest.main()
