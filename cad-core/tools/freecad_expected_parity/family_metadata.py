"""Read-only S4 family metadata for unaccepted parity diffs.

This module preserves the rollout ownership surface from the previous
comparator without participating in protocol-divergence acceptance.  Registry
matching remains the only route by which an exact diff can become an approved
protocol divergence.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class FamilyMetadata:
    code: str
    label: str
    owner: str
    freecad_authority: str
    next_action: str
    close_condition: str
    known_gap_id: str | None


FAMILIES: dict[str, FamilyMetadata] = {
    "toponaming_elementmap": FamilyMetadata(
        code="toponaming_elementmap",
        label="TopoNamingState / ElementMap / App::Link",
        owner=(
            "cad-core/src/topo; cad-core/src/runtime/topo_naming_state.cpp; "
            "cad-core/src/app/link.cpp"
        ),
        freecad_authority=(
            "src/App/ElementMap.cpp; src/App/PropertyLinks.cpp; "
            "src/Mod/Part/App/TopoShapeExpansion.cpp"
        ),
        next_action=(
            "Carry ElementMap, childElementMaps and App::Link owner projection through "
            "the public expected release view before marking this family green."
        ),
        close_condition=(
            "Representative ElementMap/App::Link phases have strict reports with no "
            "anonymous diffs; remaining gaps have documented known-gap ids."
        ),
        known_gap_id="C13M5-S4-KG-TOPO-001",
    ),
    "sketch_internal_shape": FamilyMetadata(
        code="sketch_internal_shape",
        label="Sketch / InternalShape / split fragment",
        owner="cad-core/src/features/sketch_object.cpp; cad-core/src/geometry; cad-core/src/topo",
        freecad_authority=(
            "src/Mod/Sketcher/App/SketchObject.cpp; "
            "src/Mod/Sketcher/App/SketchObjectGeometry.cpp; "
            "src/Mod/Part/App/FaceMaker*.cpp; src/Mod/Part/App/WireJoiner.cpp"
        ),
        next_action=(
            "Compare Sketch Shape/InternalShape, open wire and split-fragment evidence "
            "against the native expected payload before changing geometry code."
        ),
        close_condition=(
            "Sketch/internal-shape phases either strict green or carry explicit "
            "split/internal-face known gaps with focused test coverage."
        ),
        known_gap_id="C13M5-S4-KG-SKETCH-001",
    ),
    "part_primitive_pipe": FamilyMetadata(
        code="part_primitive_pipe",
        label="Part primitives / boolean / sweep / loft / pipe",
        owner="cad-core/src/features; cad-core/src/geometry; cad-core/src/topo",
        freecad_authority="src/Mod/Part/App; src/Mod/PartDesign/App/FeaturePipe.cpp",
        next_action=(
            "Route primitive, boolean, loft, sweep and pipe differences to the Part "
            "feature/geometry/topo implementation path instead of adapter trimming."
        ),
        close_condition=(
            "Selected Part/Pipe phases have strict classified reports and each red "
            "bucket is tied to a FreeCAD source-backed implementation task."
        ),
        known_gap_id="C13M5-S4-KG-PART-001",
    ),
    "partdesign_body_dressup": FamilyMetadata(
        code="partdesign_body_dressup",
        label="PartDesign Body / dress-up / pattern / hole",
        owner="cad-core/src/features; cad-core/src/topo; cad-core/src/geometry",
        freecad_authority=(
            "src/Mod/PartDesign/App/Body.cpp; "
            "src/Mod/PartDesign/App/FeatureDressUp.cpp; "
            "src/Mod/PartDesign/App/FeatureTransformed.cpp; "
            "src/Mod/PartDesign/App/FeatureHole.cpp"
        ),
        next_action=(
            "Keep Body/Tip replay, dress-up ownership, pattern history and Hole "
            "differences grouped for focused PartDesign implementation batches."
        ),
        close_condition=(
            "Body/dress-up/pattern/hole reports have no anonymous diffs and known "
            "gaps name the missing PartDesign semantic batch."
        ),
        known_gap_id="C13M5-S4-KG-PD-001",
    ),
    "assembly_placement_link": FamilyMetadata(
        code="assembly_placement_link",
        label="Assembly / placement / App::Link",
        owner="cad-core/src/assembly; cad-core/src/app/link.cpp; cad-core/src/runtime/recompute.cpp",
        freecad_authority="src/Mod/Assembly/App; src/App/Link.cpp; src/App/PropertyLinks.cpp",
        next_action=(
            "Classify App::Link, Assembly marker, solver DTO and placement writeback "
            "differences before expanding the release gate to large assembly phases."
        ),
        close_condition=(
            "Assembly/App::Link representative phases have strict reports with "
            "solver/placement/link gaps recorded and deletion conditions documented."
        ),
        known_gap_id="C13M5-S4-KG-ASM-001",
    ),
    "phase_family_registry": FamilyMetadata(
        code="phase_family_registry",
        label="Phase family registry",
        owner="cad-core/tools/freecad_expected_parity/family_metadata.py",
        freecad_authority="docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵",
        next_action=(
            "Assign this phase or case to one of the S4 semantic families before "
            "using it as a release gate candidate."
        ),
        close_condition=(
            "The phase is registered under a semantic family and no report uses the "
            "generic registry bucket."
        ),
        known_gap_id=None,
    ),
}

_PHASE_CATALOG_PATH = Path(__file__).with_name("fixture_capability_phases.v1.json")
_PHASE_CATALOG = json.loads(_PHASE_CATALOG_PATH.read_text(encoding="utf-8"))["phases"]


def _phases_for_module(module: str) -> set[str]:
    return {
        str(entry["phase"])
        for entry in _PHASE_CATALOG
        if entry.get("module") == module
    }


PART_PHASES = _phases_for_module("Part")
SKETCH_PHASES = _phases_for_module("Sketcher")
PARTDESIGN_PHASES = _phases_for_module("PartDesign")
TOPOLOGY_PHASES = _phases_for_module("Topology")
ASSEMBLY_PHASES = _phases_for_module("Assembly")


def family_for_case(phase: str, case: str) -> FamilyMetadata:
    """Return reporting ownership only; callers must not use it for acceptance."""

    case_lower = case.lower()
    if phase in ASSEMBLY_PHASES or case_lower.startswith("assembly-"):
        return FAMILIES["assembly_placement_link"]
    if case_lower.startswith("app-link") or "app-link" in case_lower:
        return FAMILIES["assembly_placement_link"]
    if phase in TOPOLOGY_PHASES or case_lower.startswith("element-map"):
        return FAMILIES["toponaming_elementmap"]
    if phase in SKETCH_PHASES or any(
        token in case_lower
        for token in ("sketch", "internal", "split", "wirejoiner", "face-maker")
    ):
        return FAMILIES["sketch_internal_shape"]
    if phase in PART_PHASES or case_lower.startswith(
        ("part-", "mesh-import", "partdesign-pipe")
    ):
        return FAMILIES["part_primitive_pipe"]
    if phase in PARTDESIGN_PHASES or any(
        token in case_lower
        for token in (
            "body",
            "dressup",
            "fillet",
            "chamfer",
            "draft",
            "thickness",
            "pattern",
            "mirrored",
            "scaled",
            "multi-transform",
            "hole",
            "pad",
            "pocket",
        )
    ):
        return FAMILIES["partdesign_body_dressup"]
    return FAMILIES["phase_family_registry"]


def _owner_for_category(family: FamilyMetadata, category: str) -> str:
    if category.startswith("topoNamingState"):
        return f"cad-core/src/runtime/topo_naming_state.cpp; cad-core/src/topo; {family.owner}"
    if category == "results.subshapes":
        return f"cad-core/src/runtime/recompute.cpp; cad-core/src/topo; {family.owner}"
    if category == "diagnostics":
        return f"cad-core/src/runtime/recompute.cpp; {family.owner}"
    return family.owner


def _decision_for_diff(family: FamilyMetadata, diff: dict[str, Any]) -> str:
    category = str(diff.get("category", "json"))
    kind = str(diff.get("kind", ""))
    path = str(diff.get("path", ""))
    terminal_field = path.rpartition(".")[2]
    if (
        (category == "results" and kind == "extra" and terminal_field == "mesh")
        or (category == "results.subshapes" and kind == "extra" and terminal_field == "subshapes")
    ):
        return f"{family.code}_transport_metadata_gap"
    if category.startswith("topoNamingState"):
        return f"{family.code}_topo_state_publication_gap"
    if category == "diagnostics":
        return f"{family.code}_diagnostic_policy_gap"
    if category == "results.subshapes":
        return f"{family.code}_subshape_identity_gap"
    if category == "geometry.numeric":
        return f"{family.code}_geometry_summary_gap"
    if category == "results":
        return f"{family.code}_result_publication_gap"
    return f"{family.code}_json_contract_gap"


def _next_action_for_decision(family: FamilyMetadata, decision: str) -> str:
    if decision.endswith("_transport_metadata_gap"):
        return (
            f"{family.label}: keep cad-core frontend transport metadata separate from "
            "native public expected fields until the release view masks or documents it."
        )
    if decision.endswith("_topo_state_publication_gap"):
        return (
            f"{family.label}: publish or intentionally scope topoNamingState fields "
            "for this family using the same public-state boundary as topology-state."
        )
    if decision.endswith("_subshape_identity_gap"):
        return (
            f"{family.label}: compare subshape identity, stableSubname and mapped-name "
            "evidence before changing feature geometry."
        )
    if decision.endswith("_diagnostic_policy_gap"):
        return (
            f"{family.label}: align diagnostic code/severity/policy with the native "
            "expected collector before treating the phase as green."
        )
    if decision.endswith("_geometry_summary_gap"):
        return (
            f"{family.label}: decide whether bbox/volume/topology summary fields are "
            "runtime publication gaps or real geometry parity gaps."
        )
    return family.next_action


def _close_condition_for_decision(family: FamilyMetadata, decision: str) -> str:
    if decision.endswith("_transport_metadata_gap"):
        return (
            "S5 release gate either excludes frontend-only transport fields with evidence "
            "or documents them as intentional family divergence."
        )
    if decision.endswith("_topo_state_publication_gap"):
        return (
            "The family report has no missing public topoNamingState fields, or each "
            "missing field has a source-backed known-gap id and deletion condition."
        )
    if decision.endswith("_subshape_identity_gap"):
        return (
            "Subshape identity diffs are strict green, accepted as naming-order-only, "
            "or assigned to a focused topo/geometry implementation batch."
        )
    if decision.endswith("_diagnostic_policy_gap"):
        return (
            "Diagnostic diffs are strict green or the known-gap entry names the exact "
            "policy mismatch and removal trigger."
        )
    if decision.endswith("_geometry_summary_gap"):
        return (
            "Geometry summary diffs are strict green or separated into publication "
            "versus implementation gaps with focused tests."
        )
    return family.close_condition


def metadata_for_unaccepted_diff(phase: str, case: str, diff: dict[str, Any]) -> dict[str, str | None]:
    """Describe an unaccepted family diff without changing its acceptance state."""

    family = family_for_case(phase, case)
    category = str(diff.get("category", "json"))
    decision = _decision_for_diff(family, diff)
    return {
        "owner": _owner_for_category(family, category),
        "owner_step": "S4",
        "decision": decision,
        "source": family.freecad_authority,
        "freecad_authority": family.freecad_authority,
        "next_action": _next_action_for_decision(family, decision),
        "close_condition": _close_condition_for_decision(family, decision),
        "knownGapId": family.known_gap_id,
    }
