#!/usr/bin/env python3
"""Stage the A4 Assembly writeback and solver-failure native probes."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = ROOT / "out" / "fixture-staging" / "fixtures"


def semantic_hash(value: Any) -> str:
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return "sha256:" + hashlib.sha256(payload.encode("utf-8")).hexdigest()


def placement(
    base: tuple[float, float, float],
    *,
    axis: tuple[float, float, float] = (1.0, 0.0, 0.0),
    degrees: float = 0.0,
) -> dict[str, Any]:
    half = math.radians(degrees) / 2.0
    sine = math.sin(half)
    return {
        "PropertyType": "App::PropertyPlacement",
        "Base": list(base),
        "Rotation": [axis[0] * sine, axis[1] * sine, axis[2] * sine, math.cos(half)],
    }


def box(name: str, object_id: int) -> dict[str, Any]:
    return {
        "Name": name,
        "ID": object_id,
        "TypeId": "Part::Box",
        "Properties": {"Length": 2, "Width": 2, "Height": 2},
    }


def component(
    name: str,
    object_id: int,
    linked_object: str,
    *,
    at: tuple[float, float, float],
    degrees: float = 0.0,
    axis: tuple[float, float, float] = (1.0, 0.0, 0.0),
) -> dict[str, Any]:
    return {
        "Name": name,
        "ID": object_id,
        "TypeId": "Assembly::AssemblyLink",
        "Properties": {
            "LinkedObject": {"PropertyType": "App::PropertyXLink", "value": linked_object},
            "Rigid": True,
            "Placement": placement(at, axis=axis, degrees=degrees),
        },
    }


def grounded(name: str, object_id: int, target: str) -> dict[str, Any]:
    return {
        "Name": name,
        "ID": object_id,
        "TypeId": "App::FeaturePython",
        "Properties": {
            "ObjectToGround": {"PropertyType": "App::PropertyLinkGlobal", "value": target}
        },
    }


def joint(
    name: str,
    object_id: int,
    joint_type: str,
    reference1: str,
    reference2: str,
    *,
    subname1: str | None = None,
    subname2: str | None = None,
    properties: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "JointType": {"PropertyType": "App::PropertyEnumeration", "value": joint_type},
        "Reference1": {
            "PropertyType": "App::PropertyXLinkSub",
            "value": reference1,
            "SubList": [subname1] if subname1 else [],
        },
        "Reference2": {
            "PropertyType": "App::PropertyXLinkSub",
            "value": reference2,
            "SubList": [subname2] if subname2 else [],
        },
    }
    payload.update(properties or {})
    return {"Name": name, "ID": object_id, "TypeId": "App::FeaturePython", "Properties": payload}


def assembly_fixture(objects: list[dict[str, Any]], component_names: list[str], joint_names: list[str]) -> dict[str, Any]:
    next_id = max(int(item["ID"]) for item in objects) + 1
    objects.extend([
        {
            "Name": "Joints",
            "ID": next_id,
            "TypeId": "Assembly::JointGroup",
            "Properties": {
                "Group": {"PropertyType": "App::PropertyLinkList", "values": joint_names}
            },
        },
        {
            "Name": "Assembly",
            "ID": next_id + 1,
            "TypeId": "Assembly::AssemblyObject",
            "Properties": {
                "Group": {
                    "PropertyType": "App::PropertyLinkList",
                    "values": component_names + ["Joints"],
                }
            },
        },
    ])
    fixture = {"Objects": objects, "recompute": {"objs": ["Assembly"]}}
    fixture["topoNamingState"] = {
        "schemaVersion": "cad-core.topo-state.v1",
        "producer": {
            "cadCoreVersion": "fixture-contract-v1",
            "freecadVersion": "1.2.0 revision 20260519",
            "occtVersion": "fixture-occt-unspecified",
        },
        "objects": {},
        "documentHash": semantic_hash({"Objects": objects, "recompute": fixture["recompute"]}),
    }
    return fixture


def orientation_writeback() -> dict[str, Any]:
    objects = [
        box("Box", 1),
        component("Ground", 2, "Box", at=(0, 0, 0)),
        component("AnglePart", 3, "Box", at=(4, 0, 0), degrees=10),
        component("ParallelPart", 4, "Box", at=(0, 4, 0), degrees=30),
        component("PerpendicularPart", 5, "Box", at=(0, 0, 4), degrees=45),
        grounded("GroundedJoint", 6, "Ground"),
        joint(
            "AngleJoint",
            7,
            "Angle",
            "Ground",
            "AnglePart",
            subname1="Face1",
            subname2="Face1",
            properties={"Angle": {"PropertyType": "App::PropertyFloat", "value": 30}},
        ),
        joint(
            "ParallelJoint",
            8,
            "Parallel",
            "Ground",
            "ParallelPart",
            subname1="Face1",
            subname2="Face1",
        ),
        joint(
            "PerpendicularJoint",
            9,
            "Perpendicular",
            "Ground",
            "PerpendicularPart",
            subname1="Face1",
            subname2="Face1",
        ),
    ]
    return assembly_fixture(
        objects,
        ["Ground", "AnglePart", "ParallelPart", "PerpendicularPart"],
        ["GroundedJoint", "AngleJoint", "ParallelJoint", "PerpendicularJoint"],
    )


def zero_radius_solver_failure() -> dict[str, Any]:
    objects = [
        box("Box", 1),
        component("Ground", 2, "Box", at=(0, 0, 0)),
        component("ShaftA", 3, "Box", at=(4, 0, 0)),
        component("ShaftB", 4, "Box", at=(-4, 0, 0)),
        grounded("GroundedJoint", 5, "Ground"),
        joint("RevoluteA", 6, "Revolute", "Ground", "ShaftA"),
        joint("RevoluteB", 7, "Revolute", "Ground", "ShaftB"),
        joint(
            "GearsJoint",
            8,
            "Gears",
            "ShaftA",
            "ShaftB",
            properties={
                "Distance": {"PropertyType": "App::PropertyFloat", "value": 0},
                "Distance2": {"PropertyType": "App::PropertyFloat", "value": 1},
            },
        ),
    ]
    return assembly_fixture(
        objects,
        ["Ground", "ShaftA", "ShaftB"],
        ["GroundedJoint", "RevoluteA", "RevoluteB", "GearsJoint"],
    )


def gear_and_belt_writeback() -> dict[str, Any]:
    """Drive one shaft in each coupled pair so the peer must be written back."""
    objects = [
        box("Box", 1),
        component("Ground", 2, "Box", at=(0, 0, 0)),
        component("GearA", 3, "Box", at=(4, 0, 0), degrees=10),
        component("GearB", 4, "Box", at=(-4, 0, 0)),
        component("BeltA", 5, "Box", at=(0, 4, 0), degrees=10),
        component("BeltB", 6, "Box", at=(0, -4, 0)),
        grounded("GroundedJoint", 7, "Ground"),
        joint("GearRevoluteB", 9, "Revolute", "Ground", "GearB"),
        joint(
            "GearDriverAngle",
            10,
            "Angle",
            "Ground",
            "GearA",
            subname1="Face1",
            subname2="Face1",
            properties={"Angle": {"PropertyType": "App::PropertyFloat", "value": 30}},
        ),
        joint(
            "GearsJoint",
            11,
            "Gears",
            "GearA",
            "GearB",
            properties={
                "Distance": {"PropertyType": "App::PropertyFloat", "value": 2},
                "Distance2": {"PropertyType": "App::PropertyFloat", "value": 1},
            },
        ),
        joint("BeltRevoluteB", 13, "Revolute", "Ground", "BeltB"),
        joint(
            "BeltDriverAngle",
            14,
            "Angle",
            "Ground",
            "BeltA",
            subname1="Face1",
            subname2="Face1",
            properties={"Angle": {"PropertyType": "App::PropertyFloat", "value": 30}},
        ),
        joint(
            "BeltJoint",
            15,
            "Belt",
            "BeltA",
            "BeltB",
            properties={
                "Distance": {"PropertyType": "App::PropertyFloat", "value": 2},
                "Distance2": {"PropertyType": "App::PropertyFloat", "value": 1},
            },
        ),
    ]
    return assembly_fixture(
        objects,
        ["Ground", "GearA", "GearB", "BeltA", "BeltB"],
        [
            "GroundedJoint",
            "GearRevoluteB",
            "GearDriverAngle",
            "GearsJoint",
            "BeltRevoluteB",
            "BeltDriverAngle",
            "BeltJoint",
        ],
    )


def cases() -> dict[str, dict[str, Any]]:
    return {
        "assembly-gears-belt-placement-writeback": gear_and_belt_writeback(),
        "assembly-orientation-joints-placement-writeback": orientation_writeback(),
        "assembly-gears-zero-radius-native-solver-failure": zero_radius_solver_failure(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=str(DEFAULT_ROOT))
    args = parser.parse_args()
    phase_root = Path(args.root) / "assembly-solve"
    phase_root.mkdir(parents=True, exist_ok=True)
    for case, fixture in sorted(cases().items()):
        path = phase_root / f"{case}.json"
        path.write_text(
            json.dumps(fixture, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
