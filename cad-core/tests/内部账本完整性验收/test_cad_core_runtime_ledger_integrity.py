from __future__ import annotations

import sys
import unittest
from pathlib import Path
from typing import Any


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]
TESTS_ROOT = CAD_CORE_ROOT / "tests"
if str(TESTS_ROOT) not in sys.path:
    sys.path.insert(0, str(TESTS_ROOT))

from fixture_runner import CadCoreFixtureTestCase  # noqa: E402


REQUIRED_MAPPER_RELATIONS = {
    "generated",
    "modified",
    "split",
    "deleted",
    "merge",
    "ambiguous",
}

REQUIRED_RECOVERY_DIAGNOSTICS = {
    "split_stable_subname",
    "deleted_stable_subname",
    "stable_identity_ambiguous",
}

REQUIRED_PROVENANCE_FIELDS = {
    "entry_key",
    "status",
    "current_element",
    "source_element",
    "element_type",
    "raw_mapped_name",
    "canonical_mapped_name",
}

REQUIRED_CHILD_MAP_FIELDS = {
    "source_owner",
    "kind",
    "offset",
    "count",
    "target_start",
    "target_end",
    "has_source_element_map",
    "source_element_map_size",
}

REQUIRED_MAPPER_EVENT_FIELDS = {
    "relation",
    "source",
    "target",
    "shape_kind",
    "maker_stage",
    "recoverability",
    "diagnostic_status",
}


class CadCoreRuntimeLedgerIntegrityTest(CadCoreFixtureTestCase):
    """Validate cad-core's legacy NamedShape ledger, not the FreeCAD oracle files."""

    def test_runtime_named_shapes_publish_element_map_and_mapped_name_provenance(self) -> None:
        result = self.run_recompute("topo-state-body-tip-stable-recovery", "c4m6")
        named_shape = result["named_shapes"]["Body"]

        element_map = named_shape.get("element_map")
        provenance = named_shape.get("mapped_name_provenance")
        self.assertIsInstance(element_map, dict)
        self.assertIsInstance(provenance, dict)
        self.assertGreater(len(element_map), 0)
        self.assertGreater(len(provenance), 0)

        source_backed = {
            key: item
            for key, item in provenance.items()
            if isinstance(item, dict) and item.get("status") == "source_backed"
        }
        self.assertGreater(len(source_backed), 0)

        for key, item in source_backed.items():
            with self.subTest(entry=key):
                self.assertEqual(set(item) & REQUIRED_PROVENANCE_FIELDS, REQUIRED_PROVENANCE_FIELDS)
                self.assertEqual(item["entry_key"], key)
                self.assertIn(key, element_map)
                self.assertIn(item["current_element"], named_shape["elements"])
                self.assertIn(item["element_type"], {"Face", "Edge", "Vertex"})
                self.assertNotEqual(item["source_element"], "")
                self.assertNotEqual(item["raw_mapped_name"], "")
                self.assertNotEqual(item["canonical_mapped_name"], "")

        canonicalized = [
            item
            for item in source_backed.values()
            if item["raw_mapped_name"] != item["canonical_mapped_name"]
            and ":H*" in item["canonical_mapped_name"]
        ]
        self.assertGreater(len(canonicalized), 0)

    def test_runtime_named_shapes_publish_child_element_maps(self) -> None:
        result = self.run_recompute("topo-state-link-compound-child-maps", "c4m6")
        compound = result["named_shapes"]["Compound"]

        child_maps = compound.get("child_element_maps")
        self.assertIsInstance(child_maps, list)
        self.assertGreater(len(child_maps), 0)

        source_owners: set[str] = set()
        kinds: set[str] = set()
        encoded_key_count = 0
        for item in child_maps:
            with self.subTest(child_map=item):
                self.assertIsInstance(item, dict)
                self.assertEqual(set(item) & REQUIRED_CHILD_MAP_FIELDS, REQUIRED_CHILD_MAP_FIELDS)
                self.assertIsInstance(item["source_owner"], str)
                self.assertGreater(item["source_owner"], "")
                self.assertIn(item["kind"], {"face", "edge", "vertex"})
                self.assertIsInstance(item["offset"], int)
                self.assertGreaterEqual(item["offset"], 0)
                self.assertIsInstance(item["count"], int)
                self.assertGreater(item["count"], 0)
                self.assertIsInstance(item["has_source_element_map"], bool)
                self.assertIsInstance(item["source_element_map_size"], int)
                self.assertGreaterEqual(item["source_element_map_size"], 0)
                self.assertIsInstance(item["target_start"], str)
                self.assertIsInstance(item["target_end"], str)
                self.assertGreater(item["target_start"], "")
                self.assertGreater(item["target_end"], "")

            source_owners.add(item["source_owner"])
            kinds.add(item["kind"])
            if item.get("encoded_child_map_key"):
                encoded_key_count += 1

        self.assertTrue({"ChildBoxA", "ChildBoxB"} <= source_owners)
        self.assertTrue({"face", "edge", "vertex"} <= kinds)
        self.assertGreater(encoded_key_count, 0)

    def test_runtime_named_shapes_publish_mapper_history_relations(self) -> None:
        result = self.run_recompute("topo-state-mapper-history-events", "c4m6")
        named_shape = result["named_shapes"]["HistoryProbe"]
        mapper_history = named_shape.get("mapper_history")

        self.assertIsInstance(mapper_history, list)
        self.assertGreater(len(mapper_history), 0)

        relations: set[str] = set()
        diagnostic_statuses: set[str] = set()
        events_by_relation: dict[str, list[dict[str, Any]]] = {}
        for event in mapper_history:
            with self.subTest(event=event):
                self.assertIsInstance(event, dict)
                self.assertEqual(set(event) & REQUIRED_MAPPER_EVENT_FIELDS, REQUIRED_MAPPER_EVENT_FIELDS)
                self.assertIsInstance(event["source"], dict)
                self.assertIsInstance(event["target"], dict)
                self.assertIn("object", event["source"])
                self.assertIn("subname", event["source"])
                self.assertIn("object", event["target"])
                self.assertIn("subname", event["target"])

            relation = event["relation"]
            relations.add(relation)
            events_by_relation.setdefault(relation, []).append(event)
            if event["diagnostic_status"]:
                diagnostic_statuses.add(event["diagnostic_status"])

        self.assertTrue(REQUIRED_MAPPER_RELATIONS <= relations, sorted(relations))
        self.assertTrue(REQUIRED_RECOVERY_DIAGNOSTICS <= diagnostic_statuses, sorted(diagnostic_statuses))
        self.assertEqual(
            {event["target"]["subname"] for event in events_by_relation["split"]},
            {"Edge2", "Edge3"},
        )
        self.assertTrue(
            all(event["recoverability"] == "needs_reselect" for event in events_by_relation["split"])
        )
        self.assertTrue(
            all(event["recoverability"] == "deleted" for event in events_by_relation["deleted"])
        )
        self.assertTrue(
            all(event["recoverability"] == "ambiguous" for event in events_by_relation["ambiguous"])
        )

    def test_runtime_reference_recovery_failures_emit_response_diagnostics(self) -> None:
        result = self.run_recompute("topo-state-mapper-history-events", "c4m6")
        diagnostic_codes = {
            diagnostic.get("code")
            for diagnostic in result.get("diagnostics", [])
            if isinstance(diagnostic, dict)
        }

        self.assertTrue(REQUIRED_RECOVERY_DIAGNOSTICS <= diagnostic_codes, sorted(diagnostic_codes))
        self.assertIn("unsupported_native_mapper_history", diagnostic_codes)


if __name__ == "__main__":
    unittest.main()
