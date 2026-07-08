from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = ROOT / "tools" / "migrate_topo_state_fixture.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("migrate_topo_state_fixture", TOOL_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {TOOL_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class TopoStateFixtureMigrationTest(unittest.TestCase):
    def test_pocket_without_base_legacy_internal_face_becomes_state_backed_token(self) -> None:
        tool = load_tool()
        fixture = {
            "Objects": [
                {
                    "Name": "SketchPocket",
                    "ID": 1,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {
                        "Geometry": [
                            {"kind": "LineSegment", "id": 1, "start": [2, 1], "end": [8, 1]},
                            {"kind": "LineSegment", "id": 2, "start": [8, 1], "end": [8, 4]},
                            {"kind": "LineSegment", "id": 3, "start": [8, 4], "end": [2, 4]},
                            {"kind": "LineSegment", "id": 4, "start": [2, 4], "end": [2, 1]},
                        ],
                        "Constraints": [],
                    },
                },
                {
                    "Name": "Pocket",
                    "ID": 2,
                    "TypeId": "PartDesign::Pocket",
                    "Properties": {
                        "Profile": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "SketchPocket",
                                    "SubList": ["InternalFace1"],
                                    "StableSubList": [""],
                                }
                            ],
                        },
                        "Type": "Length",
                        "Length": 10,
                        "Reversed": True,
                        "SideType": "One side",
                    },
                },
                {
                    "Name": "Body",
                    "ID": 3,
                    "TypeId": "PartDesign::Body",
                    "Properties": {
                        "Group": {
                            "PropertyType": "App::PropertyLinkList",
                            "values": ["SketchPocket", "Pocket"],
                        },
                        "Tip": {
                            "PropertyType": "App::PropertyLinkSub",
                            "value": "Pocket",
                            "SubList": [],
                        },
                    },
                },
            ],
            "recompute": {
                "objs": ["Body"],
            },
        }
        args = argparse.Namespace(
            history_mode="minimal",
            cad_core_version="fixture-contract-v1",
            freecad_version="1.2.0 revision 20260519",
            occt_version="fixture-occt-unspecified",
            ensure_state=False,
            skip_unsupported=False,
        )

        migrated, stats = tool.migrate_fixture(fixture, args)

        pocket = next(item for item in migrated["Objects"] if item["Name"] == "Pocket")
        profile_ref = pocket["Properties"]["Profile"]["SubSet"][0]
        self.assertEqual(profile_ref["SubList"], [])
        self.assertEqual(profile_ref["StableSubList"], ["g1;SKT;FAC"])
        self.assertEqual(profile_ref["StableSubListSource"], "topoNamingState")

        state = migrated["topoNamingState"]
        sketch_state = state["objects"]["SketchPocket"]
        entry = sketch_state["elementMap"]["entries"]["g1;SKT;FAC"]
        self.assertEqual(entry["target"], {"object": "SketchPocket", "subname": "InternalFace1"})
        self.assertEqual(entry["shapeKind"], "face")
        self.assertEqual(entry["mappedName"], {"raw": "g1;SKT;FAC", "canonical": "g1;SKT;FAC"})
        self.assertEqual(entry["evidence"]["mapperHistoryIds"], [])
        self.assertEqual(sketch_state["mapperHistory"], [])
        self.assertEqual(sketch_state["elementMap"]["status"], "indexed_only")
        self.assertEqual(stats.rewritten_links, 1)
        self.assertEqual(stats.element_map_entries, 1)

        remigrated, remigrated_stats = tool.migrate_fixture(migrated, args)
        self.assertEqual(remigrated, migrated)
        self.assertFalse(remigrated_stats.changed)

    def test_rect_pad_pocket_existing_state_is_preserved(self) -> None:
        tool = load_tool()
        fixture_path = ROOT / "fixtures" / "p2" / "rect-pad-pocket.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        args = argparse.Namespace(
            history_mode="minimal",
            cad_core_version="fixture-contract-v1",
            freecad_version="1.2.0 revision 20260519",
            occt_version="fixture-occt-unspecified",
            ensure_state=False,
            skip_unsupported=False,
        )

        migrated, stats = tool.migrate_fixture(fixture, args)

        self.assertEqual(migrated, fixture)
        self.assertFalse(stats.changed)

    def test_existing_g1_stable_face_token_gets_state_source_and_element_map(self) -> None:
        tool = load_tool()
        fixture = {
            "Objects": [
                {
                    "Name": "Sketch",
                    "ID": 1,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {
                        "Geometry": [
                            {"kind": "LineSegment", "id": 1, "start": [0, 0], "end": [1, 0]},
                            {"kind": "LineSegment", "id": 2, "start": [1, 0], "end": [1, 1]},
                            {"kind": "LineSegment", "id": 3, "start": [1, 1], "end": [0, 1]},
                            {"kind": "LineSegment", "id": 4, "start": [0, 1], "end": [0, 0]},
                        ],
                        "Constraints": [],
                    },
                },
                {
                    "Name": "Pad",
                    "ID": 2,
                    "TypeId": "PartDesign::Pad",
                    "Properties": {
                        "Profile": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "Sketch",
                                    "SubList": [],
                                    "StableSubList": ["g1;SKT;FAC"],
                                }
                            ],
                        }
                    },
                },
            ],
            "recompute": {
                "objs": ["Pad"],
            },
        }
        args = argparse.Namespace(
            history_mode="minimal",
            cad_core_version="fixture-contract-v1",
            freecad_version="1.2.0 revision 20260519",
            occt_version="fixture-occt-unspecified",
            ensure_state=False,
            skip_unsupported=False,
        )

        migrated, stats = tool.migrate_fixture(fixture, args)

        profile_ref = migrated["Objects"][1]["Properties"]["Profile"]["SubSet"][0]
        self.assertEqual(profile_ref["SubList"], [])
        self.assertEqual(profile_ref["StableSubList"], ["g1;SKT;FAC"])
        self.assertEqual(profile_ref["StableSubListSource"], "topoNamingState")
        entry = migrated["topoNamingState"]["objects"]["Sketch"]["elementMap"]["entries"]["g1;SKT;FAC"]
        self.assertEqual(entry["target"], {"object": "Sketch", "subname": "InternalFace1"})
        self.assertEqual(stats.rewritten_links, 1)
        self.assertEqual(stats.element_map_entries, 1)

    def test_ensure_state_adds_empty_topo_state_to_plain_fixture(self) -> None:
        tool = load_tool()
        fixture = {
            "Objects": [
                {
                    "Name": "Plane",
                    "ID": 1,
                    "TypeId": "Part::Plane",
                    "Properties": {},
                }
            ],
            "recompute": {
                "objs": ["Plane"],
            },
        }
        args = argparse.Namespace(
            history_mode="minimal",
            cad_core_version="fixture-contract-v1",
            freecad_version="1.2.0 revision 20260519",
            occt_version="fixture-occt-unspecified",
            ensure_state=True,
            skip_unsupported=False,
        )

        migrated, stats = tool.migrate_fixture(fixture, args)

        state = migrated["topoNamingState"]
        self.assertEqual(state["schemaVersion"], "cad-core.topo-state.v1")
        self.assertEqual(state["objects"], {})
        self.assertTrue(state["documentHash"].startswith("sha256:"))
        self.assertEqual(stats.rewritten_links, 0)
        self.assertGreater(stats.state_updates, 0)

    def test_ensure_state_skip_unsupported_preserves_invalid_link(self) -> None:
        tool = load_tool()
        fixture = {
            "Objects": [
                {
                    "Name": "Body",
                    "ID": 1,
                    "TypeId": "PartDesign::Body",
                    "Properties": {},
                },
                {
                    "Name": "Pad",
                    "ID": 2,
                    "TypeId": "PartDesign::Pad",
                    "Properties": {
                        "Profile": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "Body",
                                    "SubList": ["InternalFace1"],
                                    "StableSubList": [""],
                                }
                            ],
                        }
                    },
                },
            ],
            "recompute": {
                "objs": ["Pad"],
            },
        }
        args = argparse.Namespace(
            history_mode="minimal",
            cad_core_version="fixture-contract-v1",
            freecad_version="1.2.0 revision 20260519",
            occt_version="fixture-occt-unspecified",
            ensure_state=True,
            skip_unsupported=True,
        )

        migrated, stats = tool.migrate_fixture(fixture, args)

        profile_ref = migrated["Objects"][1]["Properties"]["Profile"]["SubSet"][0]
        self.assertEqual(profile_ref["SubList"], ["InternalFace1"])
        self.assertEqual(profile_ref["StableSubList"], [""])
        self.assertEqual(migrated["topoNamingState"]["objects"], {})
        self.assertEqual(stats.rewritten_links, 0)
        self.assertGreater(stats.state_updates, 0)


if __name__ == "__main__":
    unittest.main()
