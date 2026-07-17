#!/usr/bin/env python3
"""Generate the source-derived A4 Assembly/OndselSolver support matrix."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = (
    ROOT / "tools" / "freecad_expected_parity" / "assembly_solver_support_matrix.v1.json"
)

JOINT_TYPES = [
    "Fixed",
    "Revolute",
    "Cylindrical",
    "Slider",
    "Ball",
    "Distance",
    "Parallel",
    "Perpendicular",
    "Angle",
    "RackPinion",
    "Screw",
    "Gears",
    "Belt",
]
MARKER_FAMILIES = [
    "whole_object",
    "point",
    "line",
    "curve",
    "circle",
    "plane",
    "cylinder",
    "sphere",
    "cone",
    "torus",
]
DISTANCE_TYPES = [
    "PointPoint",
    "LineLine",
    "LineCircle",
    "CircleCircle",
    "PlanePlane",
    "PlaneCylinder",
    "PlaneSphere",
    "PlaneCone",
    "PlaneTorus",
    "CylinderCylinder",
    "CylinderSphere",
    "CylinderCone",
    "CylinderTorus",
    "ConeCone",
    "ConeTorus",
    "ConeSphere",
    "TorusTorus",
    "TorusSphere",
    "SphereSphere",
    "PointPlane",
    "PointCylinder",
    "PointSphere",
    "PointCone",
    "PointTorus",
    "LinePlane",
    "LineCylinder",
    "LineSphere",
    "LineCone",
    "LineTorus",
    "CurvePlane",
    "CurveCylinder",
    "CurveSphere",
    "CurveCone",
    "CurveTorus",
    "PointLine",
    "PointCurve",
    "Other",
]

DEFAULT_PLANAR_DISTANCE_TYPES = {
    "PlaneCone",
    "CylinderCone",
    "ConeCone",
    "ConeTorus",
    "ConeSphere",
    "PointCone",
    "PointTorus",
    "LineCylinder",
    "LineSphere",
    "LineCone",
    "LineTorus",
    "CurvePlane",
    "CurveCylinder",
    "CurveSphere",
    "CurveCone",
    "CurveTorus",
    "Other",
}

NON_DISTANCE_CASES = {
    "Fixed": "assembly-marker-fixed-face-real-solver",
    "Revolute": "assembly-marker-revolute-edge-real-solver",
    "Cylindrical": "assembly-marker-cylindrical-edge-real-solver",
    "Slider": "assembly-marker-slider-edge-real-solver",
    "Ball": "assembly-marker-ball-vertex-real-solver",
    "RackPinion": "assembly-rackpinion-marker-rewrite-real-solver",
    "Screw": "assembly-grounded-screw-joint-real-solver",
    "Parallel": "assembly-orientation-joints-placement-writeback",
    "Perpendicular": "assembly-orientation-joints-placement-writeback",
    "Angle": "assembly-orientation-joints-placement-writeback",
    "Gears": "assembly-gears-belt-placement-writeback",
    "Belt": "assembly-gears-belt-placement-writeback",
}
NON_DISTANCE_WRITEBACK_OBJECTS = {
    "Parallel": ["ParallelPart"],
    "Perpendicular": ["PerpendicularPart"],
    "Angle": ["AnglePart"],
    "Gears": ["GearA", "GearB"],
    "Belt": ["BeltA", "BeltB"],
}
NON_DISTANCE_MARKERS = {
    "Fixed": ["plane"],
    "Revolute": ["line"],
    "Cylindrical": ["line"],
    "Slider": ["line"],
    "Ball": ["point"],
    "Parallel": ["plane"],
    "Perpendicular": ["plane"],
    "Angle": ["plane"],
    "RackPinion": ["whole_object"],
    "Screw": ["whole_object"],
    "Gears": ["whole_object"],
    "Belt": ["whole_object"],
}
DISTANCE_CASES = {
    "PointPoint": "assembly-distance-point-point-nonzero-real-solver",
    "LineLine": "assembly-distance-line-line-real-solver",
    "LineCircle": "assembly-distance-line-circle-real-solver",
    "CircleCircle": "assembly-distance-circle-circle-real-solver",
    "PlanePlane": "assembly-distance-plane-plane-real-solver",
    "PlaneCylinder": "assembly-distance-plane-cylinder-real-solver",
    "PlaneSphere": "assembly-distance-plane-sphere-real-solver",
    "PlaneCone": "assembly-distance-plane-cone-default-boundary",
    "PlaneTorus": "assembly-distance-plane-torus-real-solver",
    "CylinderCylinder": "assembly-distance-cylinder-cylinder-real-solver",
    "CylinderSphere": "assembly-distance-cylinder-sphere-real-solver",
    "CylinderCone": "assembly-distance-cylinder-cone-default-boundary",
    "CylinderTorus": "assembly-distance-cylinder-torus-real-solver",
    "ConeCone": "assembly-distance-cone-cone-default-boundary",
    "ConeTorus": "assembly-distance-cone-torus-default-boundary",
    "ConeSphere": "assembly-distance-cone-sphere-default-boundary",
    "TorusTorus": "assembly-distance-torus-torus-real-solver",
    "TorusSphere": "assembly-distance-torus-sphere-real-solver",
    "SphereSphere": "assembly-distance-sphere-sphere-real-solver",
    "PointPlane": "assembly-distance-point-plane-real-solver",
    "PointCylinder": "assembly-distance-point-cylinder-real-solver",
    "PointSphere": "assembly-distance-point-sphere-real-solver",
    "PointCone": "assembly-distance-point-cone-default-boundary",
    "PointTorus": "assembly-distance-point-torus-default-boundary",
    "LinePlane": "assembly-distance-line-plane-real-solver",
    "LineCylinder": "assembly-distance-line-cylinder-default-boundary",
    "LineSphere": "assembly-distance-line-sphere-default-boundary",
    "LineCone": "assembly-distance-line-cone-default-boundary",
    "LineTorus": "assembly-distance-line-torus-default-boundary",
    "CurvePlane": "assembly-distance-curve-plane-default-boundary",
    "CurveCylinder": "assembly-distance-curve-cylinder-default-boundary",
    "CurveSphere": "assembly-distance-curve-sphere-default-boundary",
    "CurveCone": "assembly-distance-curve-cone-default-boundary",
    "CurveTorus": "assembly-distance-curve-torus-default-boundary",
    "PointLine": "assembly-distance-point-line-real-solver",
    "PointCurve": "assembly-distance-point-curve-real-solver",
    "Other": "assembly-distance-other-default-boundary",
}


def distance_marker_families(distance_type: str) -> list[str]:
    if distance_type == "Other":
        return ["curve"]
    tokens = []
    for family, marker in (
        ("Point", "point"),
        ("Line", "line"),
        ("Curve", "curve"),
        ("Circle", "circle"),
        ("Plane", "plane"),
        ("Cylinder", "cylinder"),
        ("Sphere", "sphere"),
        ("Cone", "cone"),
        ("Torus", "torus"),
    ):
        if family in distance_type and marker not in tokens:
            tokens.append(marker)
    return tokens


def evidence_class(
    evidence_id: str,
    *,
    fixture: str | None,
    checks: dict[str, Any],
    level: str,
) -> dict[str, Any]:
    return {
        "id": evidence_id,
        "fixture": f"assembly-solve/{fixture}" if fixture else None,
        "level": level,
        "checks": checks,
    }


def matrix() -> dict[str, Any]:
    evidence: list[dict[str, Any]] = []
    rows: list[dict[str, Any]] = []
    for joint_type in JOINT_TYPES:
        if joint_type == "Distance":
            continue
        fixture = NON_DISTANCE_CASES.get(joint_type)
        evidence_id = f"joint.{joint_type}.supported_solve_writeback"
        evidence.append(
            evidence_class(
                evidence_id,
                fixture=fixture,
                checks={
                    "nativeSolverReturn": 0,
                    "solverStatus": "solved",
                    "jointTypes": [joint_type],
                    "minimumPlacementUpdates": 1,
                    **(
                        {"placementUpdateObjects": NON_DISTANCE_WRITEBACK_OBJECTS[joint_type]}
                        if joint_type in NON_DISTANCE_WRITEBACK_OBJECTS
                        else {}
                    ),
                },
                level="dependency_result",
            )
        )
        rows.append({
            "id": f"joint.{joint_type}.generic_marker_pair",
            "jointType": joint_type,
            "markerGeometryFamilies": NON_DISTANCE_MARKERS[joint_type],
            "supportStatus": "source_supported",
            "evidenceClassIds": [evidence_id, "solver.failure.shared"],
        })

    for distance_type in DISTANCE_TYPES:
        evidence_id = f"distance.{distance_type}.supported_solve_writeback"
        evidence.append(
            evidence_class(
                evidence_id,
                fixture=DISTANCE_CASES[distance_type],
                checks={
                    "nativeSolverReturn": 0,
                    "solverStatus": "solved",
                    "jointTypes": ["Distance"],
                    "distanceTypes": [distance_type],
                    "minimumPlacementUpdates": 1,
                },
                level="dependency_result",
            )
        )
        rows.append({
            "id": f"joint.Distance.{distance_type}",
            "jointType": "Distance",
            "markerGeometryFamilies": distance_marker_families(distance_type),
            "distanceType": distance_type,
            "supportStatus": (
                "source_default_planar_boundary"
                if distance_type in DEFAULT_PLANAR_DISTANCE_TYPES
                else "source_supported"
            ),
            "evidenceClassIds": [evidence_id, "solver.failure.shared"],
        })

    evidence.append(
        evidence_class(
            "marker.rackpinion_missing_slider_boundary",
            fixture="assembly-unsupported-joint-diagnostic",
            checks={
                "nativeSolverReturn": 0,
                "solverStatus": "solved",
                "jointTypes": ["RackPinion"],
                "slidingPartIndex": 0,
                "maximumPlacementUpdates": 0,
            },
            level="native_diagnostic",
        )
    )
    evidence.append(
        evidence_class(
            "solver.failure.shared",
            fixture="assembly-gears-zero-radius-native-solver-failure",
            checks={
                "nativeSolverReturn": -1,
                "solverStatus": "error",
                "solverReason": "native_solver_failed",
            },
            level="native_diagnostic",
        )
    )
    rows.append({
        "id": "joint.RackPinion.missing_slider_degenerate",
        "jointType": "RackPinion",
        "markerGeometryFamilies": ["whole_object"],
        "supportStatus": "source_degenerate_unsupported",
        "evidenceClassIds": [
            "marker.rackpinion_missing_slider_boundary",
            "solver.failure.shared",
        ],
    })

    return {
        "schema": "freecad-assembly-solver-support-matrix/v1",
        "scope": "A4 source-derived retained Assembly/OndselSolver support equivalence classes",
        "sourceAuthority": [
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::JointTypes",
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::GeometryType/DistanceType",
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve/makeMbdAssembly/makeMbdJointOfType/makeMbdJointDistance/handleOneSideOfJoint/setNewPlacements",
            "/Users/li/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMT*Joint.cpp::createMbD",
        ],
        "jointTypes": JOINT_TYPES,
        "markerGeometryFamilies": MARKER_FAMILIES,
        "distanceTypes": DISTANCE_TYPES,
        "evidenceClasses": sorted(evidence, key=lambda item: item["id"]),
        "rows": sorted(rows, key=lambda item: item["id"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(matrix(), ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
