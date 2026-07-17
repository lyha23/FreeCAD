#!/usr/bin/env python3
"""Stage the A1 Mesh primitive, set-operation, and defect fixture batch."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = ROOT / "out" / "fixture-staging" / "fixtures"
LEGACY_MAP = ROOT / "tools" / "freecad_expected_parity" / "fixture_legacy_phase_map.v1.json"


def property_link(target: str) -> dict[str, Any]:
    return {"PropertyType": "App::PropertyLink", "value": target}


def placement(x: float, y: float, z: float) -> dict[str, Any]:
    return {
        "PropertyType": "App::PropertyPlacement",
        "Base": [x, y, z],
        "Rotation": [0.0, 0.0, 0.0, 1.0],
    }


def mesh_cube(name: str, *, at: tuple[float, float, float] = (0, 0, 0)) -> dict[str, Any]:
    return {
        "Name": name,
        "TypeId": "Mesh::Cube",
        "Properties": {
            "Length": 2.0,
            "Width": 2.0,
            "Height": 2.0,
            "Placement": placement(*at),
        },
    }


def mesh_import(name: str, asset: str) -> dict[str, Any]:
    return {
        "Name": name,
        "TypeId": "Mesh::Import",
        "Properties": {"FileName": f"fixtures/_assets/{asset}"},
    }


def mesh_feature(
    name: str,
    *,
    vertices: list[list[float]],
    facets: list[list[int]],
    check: bool = True,
) -> dict[str, Any]:
    return {
        "Name": name,
        "TypeId": "Mesh::Feature",
        "Properties": {
            "Mesh": {
                "PropertyType": "Mesh::PropertyMeshKernel",
                "vertices": vertices,
                "facets": facets,
                "check": check,
            }
        },
    }


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


def primitive_cases() -> dict[str, dict[str, Any]]:
    cases = {
        "mesh-sphere": {
            "Objects": [
                {
                    "Name": "Sphere",
                    "TypeId": "Mesh::Sphere",
                    "Properties": {"Radius": 2.0, "Sampling": 16},
                }
            ],
            "recompute": {"objs": ["Sphere"]},
        },
        "mesh-sphere-recompute-radius-sampling": {
            "Objects": [
                {
                    "Name": "Sphere",
                    "TypeId": "Mesh::Sphere",
                    "Properties": {"Radius": 1.0, "Sampling": 8},
                }
            ],
            "recompute": {
                "objs": ["Sphere"],
                "mutations": [
                    {
                        "object": "Sphere",
                        "properties": {"Radius": 2.0, "Sampling": 16},
                    }
                ],
            },
        },
        "mesh-sphere-zero-radius-min-sampling": {
            "Objects": [
                {
                    "Name": "Sphere",
                    "TypeId": "Mesh::Sphere",
                    "Properties": {"Radius": 0.0, "Sampling": 0},
                }
            ],
            "recompute": {"objs": ["Sphere"]},
        },
        "mesh-sphere-negative-radius": {
            "Objects": [
                {
                    "Name": "Sphere",
                    "TypeId": "Mesh::Sphere",
                    "Properties": {"Radius": -1.0, "Sampling": 8},
                }
            ],
            "recompute": {"objs": ["Sphere"]},
        },
        "mesh-cube": {
            "Objects": [
                {
                    "Name": "Cube",
                    "TypeId": "Mesh::Cube",
                    "Properties": {"Length": 1.0, "Width": 2.0, "Height": 3.0},
                }
            ],
            "recompute": {"objs": ["Cube"]},
        },
        "mesh-cube-recompute-dimensions": {
            "Objects": [
                {
                    "Name": "Cube",
                    "TypeId": "Mesh::Cube",
                    "Properties": {"Length": 1.0, "Width": 2.0, "Height": 3.0},
                }
            ],
            "recompute": {
                "objs": ["Cube"],
                "mutations": [
                    {
                        "object": "Cube",
                        "properties": {"Length": 2.0, "Width": 3.0, "Height": 4.0},
                    }
                ],
            },
        },
        "mesh-cube-zero-length": {
            "Objects": [
                {
                    "Name": "Cube",
                    "TypeId": "Mesh::Cube",
                    "Properties": {"Length": 0.0, "Width": 1.0, "Height": 1.0},
                }
            ],
            "recompute": {"objs": ["Cube"]},
        },
    }
    primitive_specs = {
        "cone": {
            "TypeId": "Mesh::Cone",
            "initial": {
                "Radius1": 1.0,
                "Radius2": 2.0,
                "Length": 4.0,
                "EdgeLength": 0.75,
                "Closed": True,
                "Sampling": 16,
            },
            "updated": {
                "Radius1": 2.0,
                "Radius2": 0.5,
                "Length": 3.0,
                "EdgeLength": 0.5,
                "Closed": False,
                "Sampling": 20,
            },
        },
        "cylinder": {
            "TypeId": "Mesh::Cylinder",
            "initial": {
                "Radius": 1.5,
                "Length": 4.0,
                "EdgeLength": 0.75,
                "Closed": True,
                "Sampling": 16,
            },
            "updated": {
                "Radius": 2.0,
                "Length": 2.5,
                "EdgeLength": 0.5,
                "Closed": False,
                "Sampling": 20,
            },
        },
        "ellipsoid": {
            "TypeId": "Mesh::Ellipsoid",
            "initial": {"Radius1": 1.5, "Radius2": 3.0, "Sampling": 16},
            "updated": {"Radius1": 2.0, "Radius2": 4.0, "Sampling": 20},
        },
        "torus": {
            "TypeId": "Mesh::Torus",
            "initial": {"Radius1": 4.0, "Radius2": 1.0, "Sampling": 16},
            "updated": {"Radius1": 5.0, "Radius2": 1.5, "Sampling": 20},
        },
    }
    for primitive, spec in primitive_specs.items():
        object_name = primitive.title()
        cases[f"mesh-{primitive}"] = {
            "Objects": [
                {
                    "Name": object_name,
                    "TypeId": spec["TypeId"],
                    "Properties": spec["initial"],
                }
            ],
            "recompute": {"objs": [object_name]},
        }
        cases[f"mesh-{primitive}-recompute-properties"] = {
            "Objects": [
                {
                    "Name": object_name,
                    "TypeId": spec["TypeId"],
                    "Properties": spec["initial"],
                }
            ],
            "recompute": {
                "objs": [object_name],
                "mutations": [
                    {"object": object_name, "properties": spec["updated"]}
                ],
            },
        }
    return cases


def setop_fixture(
    operation: str,
    *,
    second_at: tuple[float, float, float] = (1.0, 0.0, 0.0),
) -> dict[str, Any]:
    return {
        "Objects": [
            mesh_cube("Left"),
            mesh_cube("Right", at=second_at),
            {
                "Name": "Result",
                "TypeId": "Mesh::SetOperations",
                "Properties": {
                    "Source1": property_link("Left"),
                    "Source2": property_link("Right"),
                    "OperationType": operation,
                },
            },
        ],
        "recompute": {"objs": ["Result"]},
    }


def setop_cases() -> dict[str, dict[str, Any]]:
    cases = {
        f"mesh-setops-{operation}": setop_fixture(operation)
        for operation in ("union", "intersection", "difference")
    }
    switch_operation = setop_fixture("union")
    switch_operation["recompute"]["mutations"] = [
        {"object": "Result", "properties": {"OperationType": "intersection"}}
    ]
    cases["mesh-setops-switch-operation"] = switch_operation
    switch_source = setop_fixture("intersection")
    switch_source["Objects"].insert(2, mesh_cube("Disjoint", at=(5.0, 0.0, 0.0)))
    switch_source["recompute"]["mutations"] = [
        {"object": "Result", "properties": {"Source2": property_link("Disjoint")}}
    ]
    cases["mesh-setops-switch-source"] = switch_source
    cases["mesh-setops-missing-source"] = {
        "Objects": [
            mesh_cube("Left"),
            {
                "Name": "Result",
                "TypeId": "Mesh::SetOperations",
                "Properties": {
                    "Source1": property_link("Left"),
                    "OperationType": "union",
                },
            },
        ],
        "recompute": {"objs": ["Result"]},
    }
    cases["mesh-setops-invalid-operation"] = setop_fixture("xor")
    cases["mesh-setops-disjoint-intersection"] = setop_fixture(
        "intersection", second_at=(5.0, 0.0, 0.0)
    )
    return cases


def defect_feature(
    type_id: str,
    source: str | None,
    *,
    properties: dict[str, Any] | None = None,
) -> dict[str, Any]:
    values = dict(properties or {})
    if source is not None:
        values["Source"] = property_link(source)
    return {"Name": "Result", "TypeId": type_id, "Properties": values}


def defect_cases() -> dict[str, dict[str, Any]]:
    harmonize = {
        "Objects": [
            mesh_import("Source", "inconsistent-tetrahedron.stl"),
            defect_feature("Mesh::HarmonizeNormals", "Source"),
        ],
        "recompute": {"objs": ["Source", "Result"]},
    }
    flip = {
        "Objects": [
            mesh_import("Source", "unit-tetrahedron.stl"),
            defect_feature("Mesh::FlipNormals", "Source"),
        ],
        "recompute": {"objs": ["Source", "Result"]},
    }
    duplicated = {
        "Objects": [
            mesh_import("Source", "duplicated-square.stl"),
            defect_feature("Mesh::FixDuplicatedFaces", "Source"),
        ],
        "recompute": {"objs": ["Source", "Result"]},
    }
    source_switch = {
        "Objects": [
            mesh_import("Inconsistent", "inconsistent-tetrahedron.stl"),
            mesh_import("Clean", "unit-tetrahedron.stl"),
            defect_feature("Mesh::HarmonizeNormals", "Inconsistent"),
        ],
        "recompute": {
            "objs": ["Result"],
            "mutations": [
                {"object": "Result", "properties": {"Source": property_link("Clean")}}
            ],
        },
    }
    open_tetrahedron = mesh_feature(
        "Source",
        vertices=[
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
        ],
        facets=[[0, 2, 1], [0, 1, 3], [1, 2, 3]],
    )
    disconnected = mesh_feature(
        "Source",
        vertices=[
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [1.0, 1.0, 0.0],
            [0.0, 1.0, 0.0],
            [3.0, 0.0, 0.0],
            [4.0, 0.0, 0.0],
            [3.0, 1.0, 0.0],
        ],
        facets=[[0, 1, 2], [0, 2, 3], [4, 5, 6]],
    )
    cases = {
        "mesh-defects-harmonize-normals": harmonize,
        "mesh-defects-flip-normals": flip,
        "mesh-defects-fix-duplicated-faces": duplicated,
        "mesh-defects-source-recompute-update": source_switch,
        "mesh-defects-missing-source": {
            "Objects": [defect_feature("Mesh::HarmonizeNormals", None)],
            "recompute": {"objs": ["Result"]},
        },
        "mesh-defects-source-without-mesh-property": {
            "Objects": [
                {
                    "Name": "Box",
                    "TypeId": "Part::Box",
                    "Properties": {"Length": 1.0, "Width": 1.0, "Height": 1.0},
                },
                defect_feature("Mesh::HarmonizeNormals", "Box"),
            ],
            "recompute": {"objs": ["Result"]},
        },
        "mesh-defects-no-duplicated-faces": {
            "Objects": [
                mesh_import("Source", "unit-tetrahedron.stl"),
                defect_feature("Mesh::FixDuplicatedFaces", "Source"),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fix-defects-base-recompute-source": {
            "Objects": [
                mesh_feature(
                    "First",
                    vertices=[[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
                    facets=[[0, 1, 2]],
                ),
                mesh_feature(
                    "Second",
                    vertices=[[0.0, 0.0, 0.0], [2.0, 0.0, 0.0], [0.0, 2.0, 0.0]],
                    facets=[[0, 1, 2]],
                ),
                defect_feature("Mesh::FixDefects", "First"),
            ],
            "recompute": {
                "objs": ["Result"],
                "mutations": [
                    {"object": "Result", "properties": {"Source": property_link("Second")}}
                ],
            },
        },
        "mesh-defects-fix-duplicated-points": {
            "Objects": [
                mesh_feature(
                    "Source",
                    vertices=[
                        [0.0, 0.0, 0.0],
                        [1.0, 0.0, 0.0],
                        [1.0, 1.0, 0.0],
                        [0.0, 0.0, 0.0],
                        [1.0, 1.0, 0.0],
                        [0.0, 1.0, 0.0],
                    ],
                    facets=[[0, 1, 2], [3, 4, 5]],
                    check=False,
                ),
                defect_feature("Mesh::FixDuplicatedPoints", "Source"),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fix-degenerations": {
            "Objects": [
                mesh_feature(
                    "Source",
                    vertices=[
                        [0.0, 0.0, 0.0],
                        [1.0, 0.0, 0.0],
                        [0.0, 1.0, 0.0],
                        [1.0, 0.0, 0.0],
                    ],
                    facets=[[0, 1, 2], [0, 1, 3]],
                    check=False,
                ),
                defect_feature(
                    "Mesh::FixDegenerations", "Source", properties={"Epsilon": 0.001}
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fix-deformations": {
            "Objects": [
                mesh_feature(
                    "Source",
                    vertices=[
                        [0.0, 0.0, 0.0],
                        [5.0, -0.1, 0.0],
                        [10.0, 0.0, 0.0],
                        [0.0, 5.0, 0.0],
                    ],
                    facets=[[0, 1, 2], [0, 2, 3]],
                ),
                defect_feature(
                    "Mesh::FixDeformations",
                    "Source",
                    properties={"Epsilon": 0.0001, "MaxAngle": 5.0},
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fix-indices": {
            "Objects": [
                mesh_feature(
                    "Source",
                    vertices=[
                        [10.0, -10.0, 10.0],
                        [10.0, 10.0, 10.0],
                        [0.0, 0.0, 10.0],
                        [-10.0, -10.0, 10.0],
                        [-10.0, 10.0, 10.0],
                        [10.0, 0.0, 10.0],
                    ],
                    facets=[
                        [0, 1, 2],
                        [3, 4, 2],
                        [1, 4, 2],
                        [0, 3, 2],
                        [4, 1, 1],
                        [1, 5, 0],
                    ],
                    check=False,
                ),
                defect_feature("Mesh::FixIndices", "Source"),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fix-non-manifolds": {
            "Objects": [
                mesh_feature(
                    "Source",
                    vertices=[
                        [0.0, 0.0, 0.0],
                        [1.0, 0.0, 0.0],
                        [0.0, 1.0, 0.0],
                        [0.0, -1.0, 0.0],
                        [0.0, 0.0, 1.0],
                    ],
                    facets=[[0, 1, 2], [1, 0, 3], [0, 1, 4]],
                    check=False,
                ),
                defect_feature("Mesh::FixNonManifolds", "Source"),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fill-holes": {
            "Objects": [
                open_tetrahedron,
                defect_feature(
                    "Mesh::FillHoles",
                    "Source",
                    properties={"FillupHolesOfLength": 3, "MaxArea": 2.0},
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-fill-holes-closed-boundary": {
            "Objects": [
                mesh_import("Source", "unit-tetrahedron.stl"),
                defect_feature(
                    "Mesh::FillHoles",
                    "Source",
                    properties={"FillupHolesOfLength": 0, "MaxArea": 2.0},
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-remove-components": {
            "Objects": [
                disconnected,
                defect_feature(
                    "Mesh::RemoveComponents", "Source", properties={"RemoveCompOfSize": 1}
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
        "mesh-defects-remove-components-zero-threshold": {
            "Objects": [
                disconnected,
                defect_feature(
                    "Mesh::RemoveComponents", "Source", properties={"RemoveCompOfSize": 0}
                ),
            ],
            "recompute": {"objs": ["Source", "Result"]},
        },
    }
    return cases


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(with_topo_state(payload), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staging-root", default=str(DEFAULT_ROOT))
    parser.add_argument(
        "--update-legacy-map",
        action="store_true",
        help="register the promoted semantic cases in the legacy identity map",
    )
    parser.add_argument(
        "--expansion-only",
        action="store_true",
        help="stage only the API-closure cases added after the original A1 batch",
    )
    args = parser.parse_args(argv)
    root = Path(args.staging_root)
    batches = {
        "mesh-primitives": primitive_cases(),
        "mesh-setops": setop_cases(),
        "mesh-defects": defect_cases(),
    }
    if args.expansion_only:
        original = {
            "mesh-primitives": {
                "mesh-sphere",
                "mesh-sphere-recompute-radius-sampling",
                "mesh-sphere-zero-radius-min-sampling",
                "mesh-sphere-negative-radius",
                "mesh-cube",
                "mesh-cube-recompute-dimensions",
                "mesh-cube-zero-length",
            },
            "mesh-defects": {
                "mesh-defects-harmonize-normals",
                "mesh-defects-flip-normals",
                "mesh-defects-fix-duplicated-faces",
                "mesh-defects-source-recompute-update",
                "mesh-defects-missing-source",
                "mesh-defects-source-without-mesh-property",
                "mesh-defects-no-duplicated-faces",
            },
        }
        batches = {
            phase: {
                case: payload
                for case, payload in cases.items()
                if case not in original.get(phase, set())
            }
            for phase, cases in batches.items()
            if phase in original
        }
    for phase, cases in batches.items():
        for case, payload in cases.items():
            write_json(root / phase / f"{case}.json", payload)
    if args.update_legacy_map:
        legacy = json.loads(LEGACY_MAP.read_text(encoding="utf-8"))
        rows = legacy.get("cases")
        if not isinstance(rows, list):
            raise ValueError("fixture legacy phase map cases must be a list")
        by_target = {
            (str(row.get("phase")), str(row.get("case"))): row
            for row in rows
            if isinstance(row, dict)
        }
        for phase, cases in batches.items():
            for case in cases:
                by_target[(phase, case)] = {
                    "case": case,
                    "deduplicatedMirror": False,
                    "legacyCase": case,
                    "legacyPhase": phase,
                    "phase": phase,
                }
        legacy["cases"] = [
            by_target[key] for key in sorted(by_target)
        ]
        LEGACY_MAP.write_text(
            json.dumps(legacy, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    counts = {phase: len(cases) for phase, cases in batches.items()}
    print(f"staged Mesh A1 fixtures: counts={counts} root={root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
