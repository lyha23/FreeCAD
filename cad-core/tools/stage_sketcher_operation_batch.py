#!/usr/bin/env python3
"""Stage the A3 Sketcher editing-operation native-authority batch."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = ROOT / "out" / "fixture-staging" / "fixtures"


def semantic_hash(value: Any) -> str:
    payload = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def with_topo_state(fixture: dict[str, Any]) -> dict[str, Any]:
    fixture["topoNamingState"] = {
        "schemaVersion": "cad-core.topo-state.v1",
        "producer": {
            "cadCoreVersion": "fixture-contract-v1",
            "freecadVersion": "1.2.0 revision 20260519",
            "occtVersion": "fixture-occt-unspecified",
        },
        "objects": {},
        "documentHash": semantic_hash(
            {
                "Objects": fixture.get("Objects", []),
                "recompute": fixture.get("recompute", {}),
            }
        ),
    }
    return fixture


def sketch_fixture(
    geometry: list[dict[str, Any]],
    operations: list[dict[str, Any]],
    *,
    constraints: list[dict[str, Any]] | None = None,
    mutations: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    fixture = {
            "Objects": [
                {
                    "Name": "Sketch",
                    "ID": 1,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {
                        "Geometry": geometry,
                        "Constraints": constraints or [],
                        "Operations": operations,
                    },
                }
            ],
            "recompute": {"objs": ["Sketch"]},
        }
    if mutations:
        fixture["recompute"]["mutations"] = mutations
    return with_topo_state(fixture)


def cases() -> dict[str, dict[str, Any]]:
    rectangle = [
        {"kind": "LineSegment", "start": [0, 0], "end": [10, 0]},
        {"kind": "LineSegment", "start": [10, 0], "end": [10, 5]},
        {"kind": "LineSegment", "start": [10, 5], "end": [0, 5]},
        {"kind": "LineSegment", "start": [0, 5], "end": [0, 0]},
    ]
    return {
        "sketch-trim-line-between-cutters": sketch_fixture(
            [
                {"kind": "LineSegment", "start": [-5, 0], "end": [5, 0]},
                {"kind": "LineSegment", "start": [-2, -2], "end": [-2, 2]},
                {"kind": "LineSegment", "start": [2, -2], "end": [2, 2]},
            ],
            [
                {
                    "op": "trim",
                    "geometryId": 0,
                    "point": [0, 0],
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-fillet-invalid-geometry-pair": sketch_fixture(
            [
                {"kind": "LineSegment", "start": [-5, 0], "end": [0, 0]},
                {"kind": "LineSegment", "start": [0, 0], "end": [0, 5]},
            ],
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 99],
                    "referencePoints": [[-4, 0], [0, 4]],
                    "radius": 1.0,
                    "trim": True,
                    "createCorner": False,
                    "chamfer": False,
                    "expectedOutcome": "native_diagnostic",
                }
            ],
        ),
        "sketch-trim-arc-between-cutters": sketch_fixture(
            [
                {
                    "kind": "ArcOfCircle",
                    "center": [0, 0],
                    "radius": 5,
                    "startAngle": 0,
                    "endAngle": 3.141592653589793,
                },
                {"kind": "LineSegment", "start": [-2, -1], "end": [-2, 6]},
                {"kind": "LineSegment", "start": [2, -1], "end": [2, 6]},
            ],
            [
                {
                    "op": "trim",
                    "geometryId": 0,
                    "point": [0, 5],
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-trim-conic-between-cutters": sketch_fixture(
            [
                {
                    "kind": "ArcOfEllipse",
                    "center": [0, 0],
                    "majorRadius": 5,
                    "minorRadius": 3,
                    "startAngle": 0,
                    "endAngle": 3.141592653589793,
                },
                {"kind": "LineSegment", "start": [-2, -1], "end": [-2, 4]},
                {"kind": "LineSegment", "start": [2, -1], "end": [2, 4]},
            ],
            [
                {
                    "op": "trim",
                    "geometryId": 0,
                    "point": [0, 3],
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-trim-no-intersection-deletes-geometry": sketch_fixture(
            [
                {"kind": "LineSegment", "start": [0, 0], "end": [5, 0]},
                {"kind": "LineSegment", "start": [0, 10], "end": [5, 10]},
            ],
            [
                {
                    "op": "trim",
                    "geometryId": 0,
                    "point": [2, 0],
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-trim-invalid-geometry-diagnostic": sketch_fixture(
            [{"kind": "LineSegment", "start": [0, 0], "end": [5, 0]}],
            [
                {
                    "op": "trim",
                    "geometryId": 99,
                    "point": [2, 0],
                    "expectedOutcome": "native_diagnostic",
                }
            ],
        ),
        "sketch-extend-line-recompute-update": sketch_fixture(
            [{"kind": "LineSegment", "start": [0, 0], "end": [5, 0]}],
            [
                {
                    "op": "extend",
                    "geometryId": 0,
                    "increment": 1.0,
                    "endpoint": "end",
                    "expectedOutcome": "target_result",
                }
            ],
            mutations=[
                {
                    "object": "Sketch",
                    "properties": {
                        "Operations": [
                            {
                                "op": "extend",
                                "geometryId": 0,
                                "increment": 2.0,
                                "endpoint": "start",
                                "expectedOutcome": "target_result",
                            }
                        ]
                    },
                }
            ],
        ),
        "sketch-extend-arc": sketch_fixture(
            [
                {
                    "kind": "ArcOfCircle",
                    "center": [0, 0],
                    "radius": 2,
                    "startAngle": 0,
                    "endAngle": 1.5707963267948966,
                }
            ],
            [
                {
                    "op": "extend",
                    "geometryId": 0,
                    "increment": 1.5707963267948966,
                    "endpoint": "end",
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-extend-conic-native-diagnostic": sketch_fixture(
            [
                {
                    "kind": "ArcOfEllipse",
                    "center": [0, 0],
                    "majorRadius": 5,
                    "minorRadius": 3,
                    "startAngle": 0,
                    "endAngle": 1.5707963267948966,
                }
            ],
            [
                {
                    "op": "extend",
                    "geometryId": 0,
                    "increment": 0.5,
                    "endpoint": "end",
                    "expectedOutcome": "native_diagnostic",
                }
            ],
        ),
        "sketch-extend-line-to-degenerate-boundary": sketch_fixture(
            [
                {"kind": "LineSegment", "start": [0, 0], "end": [5, 0]},
                {"kind": "LineSegment", "start": [0, 10], "end": [5, 10]},
            ],
            [
                {
                    "op": "extend",
                    "geometryId": 0,
                    "increment": -5.0,
                    "endpoint": "end",
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-fillet-trim-closed-profile": sketch_fixture(
            rectangle,
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 1],
                    "referencePoints": [[8, 0], [10, 2]],
                    "radius": 1.0,
                    "trim": True,
                    "createCorner": False,
                    "chamfer": False,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-fillet-create-corner": sketch_fixture(
            rectangle,
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 1],
                    "referencePoints": [[8, 0], [10, 2]],
                    "radius": 1.0,
                    "trim": True,
                    "createCorner": True,
                    "chamfer": False,
                    "expectedOutcome": "target_result",
                }
            ],
            constraints=[
                {
                    "Type": "Coincident",
                    "First": 0,
                    "FirstPos": "end",
                    "Second": 1,
                    "SecondPos": "start",
                }
            ],
        ),
        "sketch-fillet-chamfer": sketch_fixture(
            rectangle,
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 1],
                    "referencePoints": [[8, 0], [10, 2]],
                    "radius": 1.0,
                    "trim": True,
                    "createCorner": False,
                    "chamfer": True,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-fillet-without-trim": sketch_fixture(
            [
                {"kind": "LineSegment", "start": [-5, 0], "end": [0, 0]},
                {"kind": "LineSegment", "start": [0, 0], "end": [0, 5]},
            ],
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 1],
                    "referencePoints": [[-4, 0], [0, 4]],
                    "radius": 1.0,
                    "trim": False,
                    "createCorner": False,
                    "chamfer": False,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-fillet-radius-recompute-update": sketch_fixture(
            rectangle,
            [
                {
                    "op": "fillet",
                    "geometryIds": [0, 1],
                    "referencePoints": [[8, 0], [10, 2]],
                    "radius": 1.0,
                    "trim": True,
                    "createCorner": False,
                    "chamfer": False,
                    "expectedOutcome": "target_result",
                },
                {
                    "op": "addConstraint",
                    "constraint": {"Type": "Radius", "First": 4, "Value": 1.0},
                    "expectedOutcome": "target_result",
                },
            ],
            mutations=[
                {
                    "object": "Sketch",
                    "properties": {
                        "Operations": [
                            {
                                "op": "setDatum",
                                "constraintId": 2,
                                "value": 2.0,
                                "expectedOutcome": "target_result",
                            }
                        ]
                    },
                }
            ],
        ),
        "sketch-clone-line-dimensional-constraint": sketch_fixture(
            [{"kind": "LineSegment", "start": [0, 0], "end": [5, 0]}],
            [
                {
                    "op": "addCopy",
                    "geometryIds": [0],
                    "displacement": [0, 3],
                    "clone": True,
                    "expectedOutcome": "target_result",
                }
            ],
            constraints=[{"Type": "Distance", "First": 0, "Value": 5.0}],
        ),
        "sketch-block-after-copy-recompute-update": sketch_fixture(
            [{"kind": "LineSegment", "start": [0, 0], "end": [5, 0]}],
            [
                {
                    "op": "addCopy",
                    "geometryIds": [0],
                    "displacement": [0, 3],
                    "clone": False,
                    "expectedOutcome": "target_result",
                }
            ],
            mutations=[
                {
                    "object": "Sketch",
                    "properties": {
                        "Operations": [
                            {
                                "op": "block",
                                "geometryId": 1,
                                "expectedOutcome": "target_result",
                            }
                        ]
                    },
                }
            ],
        ),
        "sketch-clone-conic-arc": sketch_fixture(
            [
                {
                    "kind": "ArcOfHyperbola",
                    "center": [0, 0],
                    "majorRadius": 4,
                    "minorRadius": 2,
                    "startAngle": -0.5,
                    "endAngle": 0.5,
                }
            ],
            [
                {
                    "op": "addCopy",
                    "geometryIds": [0],
                    "displacement": [0, 4],
                    "clone": True,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-conic-expose-internal-geometry": sketch_fixture(
            [
                {
                    "kind": "ArcOfParabola",
                    "center": [0, 0],
                    "focal": 1.0,
                    "startAngle": -1.0,
                    "endAngle": 1.0,
                }
            ],
            [
                {
                    "op": "exposeInternalGeometry",
                    "geometryId": 0,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-conic-convert-to-bspline": sketch_fixture(
            [
                {
                    "kind": "ArcOfEllipse",
                    "center": [0, 0],
                    "majorRadius": 5,
                    "minorRadius": 3,
                    "startAngle": 0,
                    "endAngle": 1.5707963267948966,
                }
            ],
            [
                {
                    "op": "convertToNURBS",
                    "geometryId": 0,
                    "expectedOutcome": "target_result",
                }
            ],
        ),
        "sketch-bspline-degree-knot-recompute-update": sketch_fixture(
            [
                {
                    "kind": "BSpline",
                    "degree": 2,
                    "poles": [[0, 0], [2, 3], [4, 3], [6, 0]],
                }
            ],
            [
                {
                    "op": "increaseBSplineDegree",
                    "geometryId": 0,
                    "increment": 1,
                    "expectedOutcome": "target_result",
                }
            ],
            mutations=[
                {
                    "object": "Sketch",
                    "properties": {
                        "Operations": [
                            {
                                "op": "insertBSplineKnot",
                                "geometryId": 0,
                                "parameter": 0.25,
                                "multiplicity": 1,
                                "expectedOutcome": "target_result",
                            }
                        ]
                    },
                }
            ],
        ),
        "sketch-bspline-knot-multiplicity": sketch_fixture(
            [
                {
                    "kind": "BSpline",
                    "degree": 2,
                    "poles": [[0, 0], [2, 3], [4, 3], [6, 0]],
                }
            ],
            [
                {
                    "op": "insertBSplineKnot",
                    "geometryId": 0,
                    "parameter": 0.25,
                    "multiplicity": 1,
                    "expectedOutcome": "target_result",
                },
                {
                    "op": "modifyBSplineKnotMultiplicity",
                    "geometryId": 0,
                    "knotIndex": 2,
                    "increment": 1,
                    "expectedOutcome": "target_result",
                },
            ],
        ),
        "sketch-bspline-invalid-knot-native-diagnostic": sketch_fixture(
            [
                {
                    "kind": "BSpline",
                    "degree": 2,
                    "poles": [[0, 0], [2, 3], [4, 3], [6, 0]],
                }
            ],
            [
                {
                    "op": "insertBSplineKnot",
                    "geometryId": 0,
                    "parameter": 2.0,
                    "multiplicity": 1,
                    "expectedOutcome": "native_diagnostic",
                }
            ],
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(DEFAULT_ROOT))
    args = parser.parse_args()
    phase_root = Path(args.root) / "sketcher-operations"
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
