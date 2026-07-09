from __future__ import annotations

import sys
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]
TOOLS_ROOT = CAD_CORE_ROOT / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import collect_freecad_expected as collector  # noqa: E402


class FakeFreeCAD:
    @staticmethod
    def Version() -> list[str]:
        return ["1", "2", "0", "test"]


class FakeShape:
    def __init__(self, *, is_null: bool = False) -> None:
        self._is_null = is_null

    def isNull(self) -> bool:
        return self._is_null


class FakeObject:
    def __init__(
        self,
        name: str,
        type_id: str,
        *,
        shape: FakeShape | None = None,
        internal_shape: FakeShape | None = None,
        tip: "FakeObject | None" = None,
        linked_object: "FakeObject | None" = None,
    ) -> None:
        self.Name = name
        self.TypeId = type_id
        if shape is not None:
            self.Shape = shape
        if internal_shape is not None:
            self.InternalShape = internal_shape
        if tip is not None:
            self.Tip = tip
        if linked_object is not None:
            self.LinkedObject = linked_object


class FreecadExpectedCollectorLedgerScopeTest(unittest.TestCase):
    def test_topo_ledger_targets_expand_from_created_graph_not_only_recompute_targets(self) -> None:
        fixture = {
            "Objects": [
                {"Name": "Sketch", "TypeId": "Sketcher::SketchObject"},
                {"Name": "Pad", "TypeId": "PartDesign::Pad"},
                {"Name": "Datum", "TypeId": "PartDesign::Plane"},
                {
                    "Name": "Body",
                    "TypeId": "PartDesign::Body",
                    "Properties": {
                        "Group": {
                            "PropertyType": "App::PropertyLinkList",
                            "values": ["Sketch", "Pad", "Datum"],
                        },
                        "Tip": {"PropertyType": "App::PropertyLink", "value": "Pad"},
                    },
                },
                {"Name": "ChildBoxA", "TypeId": "Part::Box"},
                {"Name": "ChildBoxB", "TypeId": "Part::Box"},
                {
                    "Name": "Compound",
                    "TypeId": "Part::Compound",
                    "Properties": {
                        "Objects": {
                            "PropertyType": "App::PropertyLinkList",
                            "values": ["ChildBoxA", "ChildBoxB"],
                        },
                    },
                },
                {
                    "Name": "CompoundLink",
                    "TypeId": "App::Link",
                    "Properties": {
                        "LinkedObject": {
                            "PropertyType": "App::PropertyXLinkSub",
                            "value": "Compound",
                        },
                    },
                },
            ],
            "recompute": {"objs": ["Body"]},
        }

        sketch = FakeObject(
            "Sketch",
            "Sketcher::SketchObject",
            shape=FakeShape(),
            internal_shape=FakeShape(),
        )
        pad = FakeObject("Pad", "PartDesign::Pad", shape=FakeShape())
        body = FakeObject("Body", "PartDesign::Body", shape=FakeShape(), tip=pad)
        compound = FakeObject("Compound", "Part::Compound", shape=FakeShape())
        created = {
            "Sketch": sketch,
            "Pad": pad,
            "Datum": FakeObject("Datum", "PartDesign::Plane"),
            "Body": body,
            "ChildBoxA": FakeObject("ChildBoxA", "Part::Box", shape=FakeShape()),
            "ChildBoxB": FakeObject("ChildBoxB", "Part::Box", shape=FakeShape()),
            "Compound": compound,
            "CompoundLink": FakeObject(
                "CompoundLink",
                "App::Link",
                linked_object=compound,
            ),
        }

        names = collector.topo_ledger_target_names(fixture, created, ["Body"])

        self.assertEqual(names[0], "Body")
        self.assertTrue(
            {
                "Sketch",
                "Pad",
                "Body",
                "ChildBoxA",
                "ChildBoxB",
                "Compound",
                "CompoundLink",
            }
            <= set(names),
            names,
        )
        self.assertNotIn("Datum", names)

    def test_topo_naming_response_uses_ledger_payloads_without_expanding_results(self) -> None:
        fixture = {
            "Objects": [
                {"Name": "Body", "TypeId": "PartDesign::Body"},
                {"Name": "Pad", "TypeId": "PartDesign::Pad"},
                {"Name": "Sketch", "TypeId": "Sketcher::SketchObject"},
            ],
            "recompute": {"objs": ["Body"]},
            "topoNamingState": {
                "schemaVersion": collector.TOPO_STATE_SCHEMA_VERSION,
                "producer": {"cadCoreVersion": collector.TOPO_STATE_PRODUCER_CAD_CORE_VERSION},
            },
        }
        result_payloads = {"Body": {"subshapes": []}}
        ledger_payloads = {
            "Body": {"subshapes": []},
            "Pad": {"subshapes": []},
            "Sketch": {"subshapes": []},
        }

        response = collector.topo_naming_state_response(
            fixture,
            FakeFreeCAD,
            result_payloads,
            ledger_payloads=ledger_payloads,
        )

        self.assertEqual([item["object"] for item in response["results"]], ["Body"])
        self.assertEqual(
            set(response["topoNamingState"]["objects"]),
            {"Body", "Pad", "Sketch"},
        )


if __name__ == "__main__":
    unittest.main()
