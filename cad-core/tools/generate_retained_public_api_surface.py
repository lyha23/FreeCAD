#!/usr/bin/env python3
"""Generate the retained source-side API denominator from reviewed capability/source rows."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PARITY_ROOT = Path(__file__).with_name("freecad_expected_parity")
DEFAULT_CAPABILITIES = PARITY_ROOT / "retained_public_capabilities.v1.json"
DEFAULT_OUTPUT = PARITY_ROOT / "retained_public_api_surface.v1.json"
SCHEMA = "freecad-retained-public-api-surface/v1"


def slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def exposure(module: str, public_surface: str) -> str:
    if module == "FreeCADMainCmd":
        return "ProcessEntry"
    if module in {"FreeCADMainPy", "Help", "AddonManager"}:
        return "PythonModule"
    if module == "FreeCADBase":
        return "PythonWrapper"
    if module == "OndselSolver":
        return "RetainedTargetDependency"
    if "::" in public_surface:
        return "DocumentObjectTypeId"
    return "CxxPublicApi"


def native_expressibility(disposition: str) -> str:
    return {
        "native_fixture": "native_fixture",
        "native_process_test": "native_process_test",
        "non_cad_smoke": "native_process_test",
        "protocol_only": "protocol_only",
        "unsupported": "unsupported",
        "uncovered": "native_fixture",
    }[disposition]


def api_from_capability(module: str, capability: dict[str, Any]) -> dict[str, Any]:
    runtime_branch = str(capability["runtimeBranch"])
    item = {
        "id": f"{slug(module)}.{capability['id']}",
        "module": module,
        "symbol": str(capability["sourceEvidence"][0]),
        "publicSurface": str(capability["publicSurface"]),
        "exposure": exposure(module, str(capability["publicSurface"])),
        "sourceEvidence": list(capability["sourceEvidence"]),
        "runtimeBranches": [runtime_branch],
        "nativeExpressibility": native_expressibility(str(capability["disposition"])),
        "fixtureEvidence": [
            {**evidence, "branch": runtime_branch}
            for evidence in capability.get("fixtureEvidence", [])
        ],
        "disposition": str(capability["disposition"]),
        "capabilityIds": [str(capability["id"])],
    }
    if capability.get("rationale"):
        item["rationale"] = str(capability["rationale"])
    return item


def mesh_api_items(capability: dict[str, Any]) -> list[dict[str, Any]]:
    source_root = "/Users/li/Chili3DProject/FreeCAD/src/Mod/Mesh/App"
    capability_id = str(capability["id"])
    items: list[dict[str, Any]] = []
    primitive_properties = {
        "Sphere": "Radius/Sampling",
        "Ellipsoid": "Radius1/Radius2/Sampling",
        "Cylinder": "Radius/Length/EdgeLength/Closed/Sampling",
        "Cone": "Radius1/Radius2/Length/EdgeLength/Closed/Sampling",
        "Torus": "Radius1/Radius2/Sampling",
        "Cube": "Length/Width/Height",
    }
    for primitive, properties in primitive_properties.items():
        normal_branch = f"execute {primitive} through MeshObject::create{primitive}"
        recompute_branch = f"mustExecute when {properties} is touched"
        covered = primitive in {"Sphere", "Cube"}
        primitive_evidence = []
        if primitive == "Sphere":
            primitive_evidence = [
                {
                    "fixture": "mesh-primitives/mesh-sphere",
                    "level": "target_result",
                    "branch": normal_branch,
                },
                {
                    "fixture": "mesh-primitives/mesh-sphere-recompute-radius-sampling",
                    "level": "target_result",
                    "branch": recompute_branch,
                },
            ]
        elif primitive == "Cube":
            primitive_evidence = [
                {
                    "fixture": "mesh-primitives/mesh-cube",
                    "level": "target_result",
                    "branch": normal_branch,
                },
                {
                    "fixture": "mesh-primitives/mesh-cube-recompute-dimensions",
                    "level": "target_result",
                    "branch": recompute_branch,
                },
            ]
        items.append(
            {
                "id": f"mesh.{slug(primitive)}",
                "module": "Mesh",
                "symbol": f"Mesh::{primitive}::execute/mustExecute",
                "publicSurface": f"Mesh::{primitive}",
                "exposure": "DocumentObjectTypeId",
                "sourceEvidence": [
                    f"{source_root}/AppMesh.cpp::CreateMeshAppModule",
                    f"{source_root}/FeatureMeshSolid.cpp::{primitive}::mustExecute",
                    f"{source_root}/FeatureMeshSolid.cpp::{primitive}::execute",
                ],
                "runtimeBranches": [normal_branch, recompute_branch],
                "nativeExpressibility": "native_fixture",
                "fixtureEvidence": primitive_evidence,
                "disposition": "native_fixture" if covered else "uncovered",
                "capabilityIds": [capability_id],
                **(
                    {}
                    if covered
                    else {
                        "rationale": (
                            "A0 classifies this registered source-side primitive; A1's first "
                            "representative batch is intentionally sphere plus cube."
                        )
                    }
                ),
            }
        )

    items.append(
        {
            "id": "mesh.set_operations",
            "module": "Mesh",
            "symbol": "Mesh::SetOperations::mustExecute/execute",
            "publicSurface": "Mesh::SetOperations",
            "exposure": "DocumentObjectTypeId",
            "sourceEvidence": [
                f"{source_root}/AppMesh.cpp::CreateMeshAppModule",
                f"{source_root}/FeatureMeshSetOperations.cpp::SetOperations::mustExecute",
                f"{source_root}/FeatureMeshSetOperations.cpp::SetOperations::execute",
                f"{source_root}/Core/SetOperations.cpp::MeshCore::SetOperations::Do",
            ],
            "runtimeBranches": [
                "union",
                "intersection",
                "difference",
                "OperationType or Source1/Source2 touched recompute",
                "missing source native diagnostic",
                "invalid operation native diagnostic",
                "disjoint operands",
            ],
            "nativeExpressibility": "native_fixture",
            "fixtureEvidence": [
                {
                    "fixture": "mesh-setops/mesh-setops-union",
                    "level": "target_result",
                    "branch": "union",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-intersection",
                    "level": "target_result",
                    "branch": "intersection",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-difference",
                    "level": "target_result",
                    "branch": "difference",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-switch-operation",
                    "level": "target_result",
                    "branch": "OperationType or Source1/Source2 touched recompute",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-switch-source",
                    "level": "target_result",
                    "branch": "OperationType or Source1/Source2 touched recompute",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-missing-source",
                    "level": "native_diagnostic",
                    "branch": "missing source native diagnostic",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-invalid-operation",
                    "level": "native_diagnostic",
                    "branch": "invalid operation native diagnostic",
                },
                {
                    "fixture": "mesh-setops/mesh-setops-disjoint-intersection",
                    "level": "target_result",
                    "branch": "disjoint operands",
                },
            ],
            "disposition": "native_fixture",
            "capabilityIds": [capability_id],
        }
    )

    defect_types = {
        "FixDefects": "base execute returns StdReturn and Source touch drives mustExecute",
        "HarmonizeNormals": "copy Source.Mesh then harmonizeNormals",
        "FlipNormals": "copy Source.Mesh then flipNormals",
        "FixNonManifolds": "copy Source.Mesh then removeNonManifolds",
        "FixDuplicatedFaces": "copy Source.Mesh then removeDuplicatedFacets",
        "FixDuplicatedPoints": "copy Source.Mesh then removeDuplicatedPoints",
        "FixDegenerations": "copy Source.Mesh then validateDegenerations",
        "FixDeformations": "copy Source.Mesh then validateDeformations",
        "FixIndices": "copy Source.Mesh then validateIndices",
        "FillHoles": "copy Source.Mesh then fillupHoles",
        "RemoveComponents": "copy Source.Mesh then removeComponents",
    }
    a1_types = {"HarmonizeNormals", "FlipNormals", "FixDuplicatedFaces"}
    for defect_type, branch in defect_types.items():
        disposition = "native_fixture" if defect_type in a1_types else "uncovered"
        runtime_branches = [branch]
        evidence: list[dict[str, Any]] = []
        if defect_type == "HarmonizeNormals":
            runtime_branches.extend(
                ["Source touched recompute", "missing or non-Mesh Source boundary"]
            )
            evidence = [
                {
                    "fixture": "mesh-defects/mesh-defects-harmonize-normals",
                    "level": "target_result",
                    "branch": branch,
                },
                {
                    "fixture": "mesh-defects/mesh-defects-source-recompute-update",
                    "level": "target_result",
                    "branch": "Source touched recompute",
                },
                {
                    "fixture": "mesh-defects/mesh-defects-missing-source",
                    "level": "native_diagnostic",
                    "branch": "missing or non-Mesh Source boundary",
                },
                {
                    "fixture": "mesh-defects/mesh-defects-source-without-mesh-property",
                    "level": "native_diagnostic",
                    "branch": "missing or non-Mesh Source boundary",
                },
            ]
        elif defect_type == "FlipNormals":
            evidence = [
                {
                    "fixture": "mesh-defects/mesh-defects-flip-normals",
                    "level": "target_result",
                    "branch": branch,
                }
            ]
        elif defect_type == "FixDuplicatedFaces":
            runtime_branches.append("no duplicated faces leaves the mesh unchanged")
            evidence = [
                {
                    "fixture": "mesh-defects/mesh-defects-fix-duplicated-faces",
                    "level": "target_result",
                    "branch": branch,
                },
                {
                    "fixture": "mesh-defects/mesh-defects-no-duplicated-faces",
                    "level": "target_result",
                    "branch": "no duplicated faces leaves the mesh unchanged",
                },
            ]
        item = {
            "id": f"mesh.{slug(defect_type)}",
            "module": "Mesh",
            "symbol": f"Mesh::{defect_type}::execute",
            "publicSurface": f"Mesh::{defect_type}",
            "exposure": "DocumentObjectTypeId",
            "sourceEvidence": [
                f"{source_root}/AppMesh.cpp::CreateMeshAppModule",
                f"{source_root}/FeatureMeshDefects.cpp::{defect_type}::execute",
            ],
            "runtimeBranches": runtime_branches,
            "nativeExpressibility": "native_fixture",
            "fixtureEvidence": evidence,
            "disposition": disposition,
            "capabilityIds": [capability_id],
        }
        if disposition == "uncovered":
            item["rationale"] = (
                "The public execute branch is classified in A0; it is outside A1's first "
                "representative defect minimum and remains visible for later expansion."
            )
        items.append(item)
    return items


def build_surface(capabilities: dict[str, Any]) -> dict[str, Any]:
    apis: list[dict[str, Any]] = []
    for module in capabilities["modules"]:
        module_name = str(module["name"])
        for capability in module["capabilities"]:
            if (
                module_name == "Mesh"
                and capability["id"] == "mesh_primitives_setops_and_defects"
            ):
                apis.extend(mesh_api_items(capability))
            else:
                apis.append(api_from_capability(module_name, capability))
    return {
        "schema": SCHEMA,
        "scope": (
            "Source-side retained public API and major runtime-branch denominator. "
            "Fixture TypeIds may satisfy evidence but never create this denominator."
        ),
        "retainedClosureSource": {
            "targetClosure": "/Users/li/Chili3DProject/FreeCAD2/tools/probe_release_contract.json",
            "pruningPlan": (
                "/Users/li/Chili3DProject/FreeCAD2/docs/\u51cf\u6cd5\u5b9e\u73b0/"
                "FreeCAD2-\u65e0QtApp\u51cf\u679d\u5b9e\u65bd\u65b9\u6848.md"
            ),
            "capabilityTrace": str(DEFAULT_CAPABILITIES),
        },
        "requiredModules": list(capabilities["requiredModules"]),
        "apis": sorted(apis, key=lambda item: (item["module"], item["id"])),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capabilities", default=str(DEFAULT_CAPABILITIES))
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args(argv)
    capabilities = json.loads(Path(args.capabilities).read_text(encoding="utf-8"))
    payload = build_surface(capabilities)
    Path(args.output).write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"retained public API surface: apis={len(payload['apis'])} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
