"""Focused tests for deterministic fixture authority inventory and triage."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(CAD_CORE_ROOT / "tests"))
SPEC = importlib.util.spec_from_file_location(
    "audit_freecad_fixture_authority",
    TOOLS / "audit_freecad_fixture_authority.py",
)
assert SPEC and SPEC.loader
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)
from freecad_expected_parity import retained_coverage
from fixture_runner import semantic_fixture_path


class FixtureAuthorityInventoryTests(unittest.TestCase):
    def write_json(self, path: Path, payload: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    def role(self, phase: str, case: str, role: str) -> dict[str, str]:
        result = {"phase": phase, "case": case, "role": role}
        if role != "native":
            result.update(
                {
                    "reason": f"{role} reason",
                    "authority": "source authority",
                    "nextAction": "next action",
                    "closeCondition": "close condition",
                }
            )
        return result

    def bootstrap(self, root: Path) -> Path:
        roles = [
            self.role("p1", "native", "native"),
            self.role("p1", "protocol", "protocol_only"),
            self.role("p1", "internal", "unsupported"),
            self.role("p1", "collector-gap", "unsupported"),
            self.role("p1", "candidate", "unsupported"),
        ]
        fixtures = root / "fixtures" / "p1"
        expected = fixtures / "expected"
        self.write_json(
            fixtures / "native.json",
            {"Objects": [{"Name": "Box", "TypeId": "Part::Box", "Properties": {}}]},
        )
        self.write_json(expected / "native.freecad.json", {})
        self.write_json(expected / "native.freecad.ledger.json", {})
        self.write_json(
            fixtures / "protocol.json",
            {"Objects": [{"Name": "Box", "TypeId": "Part::Box", "Properties": {}}]},
        )
        self.write_json(expected / "protocol.expeted.json", {})
        self.write_json(
            fixtures / "internal.json",
            {"Objects": [{"Name": "Probe", "TypeId": "CadCore::Probe", "Properties": {}}]},
        )
        self.write_json(
            fixtures / "collector-gap.json",
            {
                "Objects": [
                    {
                        "Name": "UnsupportedPartObject",
                        "TypeId": "Part::UnsupportedNativeType",
                        "Properties": {},
                    }
                ]
            },
        )
        self.write_json(
            fixtures / "candidate.json",
            {"Objects": [{"Name": "Box", "TypeId": "Part::Box", "Properties": {}}]},
        )
        roles_path = root / "fixture_roles.v1.json"
        self.write_json(
            roles_path,
            {
                "schemaVersion": "cad-core.freecad-expected-fixture-roles.v1",
                "legacyNativeExpectedDiscovery": False,
                "requireCompleteInputCoverage": True,
                "roles": roles,
            },
        )
        return roles_path

    def retained_closure(self, root: Path) -> tuple[Path, Path]:
        contract_path = root / "probe_release_contract.json"
        self.write_json(
            contract_path,
            {
                "schema": "freecad2.probe-release-contract.v1",
                "milestone": "M8",
                "modules": [
                    {"name": name, "targets": [name], "evidence": [f"src/Mod/{name}"]}
                    for name in (
                        "Material",
                        "Part",
                        "Sketcher",
                        "PartDesign",
                        "Mesh",
                        "Spreadsheet",
                        "Assembly",
                    )
                ],
                "targetDependencies": ["OndselSolver"],
                "releaseArtifacts": [
                    {"target": target, "paths": {"macos": f"lib/{target}"}}
                    for target in (
                        "FreeCADBase",
                        "FreeCADApp",
                        "FreeCADMainCmd",
                        "FreeCADMainPy",
                    )
                ],
            },
        )
        plan_path = root / "pruning-plan.md"
        plan_path.write_text(
            "Retain headless Python/data entries Help/AddonManager.\n",
            encoding="utf-8",
        )
        return contract_path, plan_path

    def test_builds_complete_inventory_and_four_way_classification(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            roles_path = self.bootstrap(root)

            report = audit.build_report(root, roles_path)

        self.assertEqual("passed", report["status"])
        self.assertEqual(5, report["counts"]["inputs"])
        self.assertEqual(
            {"native": 1, "protocol_only": 1, "unsupported": 3},
            report["counts"]["roles"],
        )
        classifications = {
            item["case"]: item["classification"]["category"]
            for item in report["unsupported"]
        }
        self.assertEqual("non_native_fixture", classifications["internal"])
        self.assertEqual("collector_general_gap", classifications["collector-gap"])
        self.assertEqual("not_investigated", classifications["candidate"])
        self.assertEqual(
            "freecad_native_not_expressible",
            report["protocolOnly"][0]["classification"]["category"],
        )
        self.assertEqual(["candidate"], [item["case"] for item in report["nextNativeCandidates"]])
        self.assertEqual(
            ["collector-gap"],
            [item["case"] for item in report["collectorImplementationQueue"]],
        )

    def test_supported_sketch_constraints_require_a_real_probe(self) -> None:
        classification = audit.unsupported_classification(
            {
                "Objects": [
                    {
                        "Name": "Sketch",
                        "TypeId": "Sketcher::SketchObject",
                        "Properties": {
                            "Geometry": [],
                            "Constraints": [{"Type": "Horizontal", "First": 0}],
                        },
                    }
                ]
            }
        )

        self.assertEqual("not_investigated", classification["category"])
        self.assertTrue(classification["candidateEligible"])

    def test_invalid_native_constraint_indexes_are_not_a_collector_gap(self) -> None:
        self.assertEqual(
            "freecad_native_not_expressible",
            audit.probe_failure_category("Constraint has invalid indexes"),
        )

    def test_native_self_links_are_not_a_collector_gap(self) -> None:
        self.assertEqual(
            "freecad_native_not_expressible",
            audit.probe_failure_category("failed to set property Support: self linking"),
        )

    def test_invalid_geometry_curve_batch_is_not_a_collector_gap(self) -> None:
        self.assertEqual(
            "freecad_native_not_expressible",
            audit.probe_failure_category(
                "Part::GeometryCurve edge expected collection supports one valid DTO per fixture"
            ),
        )

    def test_public_repeat_nondeterminism_is_a_native_authority_boundary(self) -> None:
        classification = audit.apply_probe_receipt(
            {
                "category": "not_investigated",
                "candidateEligible": True,
                "evidence": [],
            },
            {
                "status": "passed",
                "ledgerOutcome": "accepted",
                "repeat2Status": "failed",
                "repeat2FirstFailure": {
                    "kind": "candidate-determinism",
                    "artifact": "public",
                },
            },
        )

        self.assertEqual("freecad_native_not_expressible", classification["category"])
        self.assertFalse(classification["candidateEligible"])

    def test_revocation_receipt_overrides_a_passing_staging_repeat(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            probes = root / "tools" / "freecad_expected_parity" / "reports" / "probes"
            revocations = root / "tools" / "freecad_expected_parity" / "reports" / "revocations"
            self.write_json(
                probes / "p1-case-collect.json",
                {
                    "status": "passed",
                    "cases": [
                        {
                            "phase": "p1",
                            "case": "case",
                            "ledgerOutcome": "accepted",
                            "errors": [],
                        }
                    ],
                },
            )
            self.write_json(probes / "p1-case-repeat2.json", {"status": "passed"})
            self.write_json(
                revocations / "p1-case.json",
                {
                    "schema": "freecad-fixture-authority-revocation/v1",
                    "status": "passed",
                    "phase": "p1",
                    "case": "case",
                    "failure": {"kind": "candidate-determinism", "artifact": "public"},
                },
            )

            receipts = audit.probe_receipts(root)

        receipt = receipts[("p1", "case")]
        self.assertEqual("failed", receipt["repeat2Status"])
        self.assertEqual("candidate-determinism", receipt["repeat2FirstFailure"]["kind"])

    def test_duplicate_role_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            roles_path = self.bootstrap(root)
            payload = json.loads(roles_path.read_text(encoding="utf-8"))
            payload["roles"].append(dict(payload["roles"][0]))
            self.write_json(roles_path, payload)

            report = audit.build_report(root, roles_path)

        self.assertEqual("failed", report["status"])
        self.assertTrue(any("duplicate fixture role" in item for item in report["anomalies"]))

    def test_maps_fixture_owners_and_emits_retained_work_queues(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            roles_path = self.bootstrap(root)

            report = audit.build_report(root, roles_path)

        rows = {item["case"]: item for item in report["cases"]}
        self.assertEqual(["Part"], rows["native"]["ownerModules"])
        self.assertTrue(rows["native"]["inRetainedClosure"])
        self.assertEqual([], rows["internal"]["ownerModules"])
        self.assertFalse(rows["internal"]["inRetainedClosure"])
        self.assertEqual(
            ["collector-gap"],
            [item["case"] for item in report["retainedModuleCollectorImplementationQueue"]],
        )
        self.assertEqual(
            ["candidate"],
            [item["case"] for item in report["stagingCandidateQueue"]],
        )
        self.assertEqual([], report["promotionQueue"])
        self.assertEqual(
            ["internal"],
            [item["case"] for item in report["blockedOrReclassified"]],
        )

    def test_reclassifies_protocol_properties_and_invalid_reference_probes(self) -> None:
        chili = audit.unsupported_classification(
            {
                "Objects": [
                    {
                        "TypeId": "Sketcher::SketchObject",
                        "Properties": {
                            "PlaneFrame": {
                                "PropertyType": "Chili::SketchPlaneFrame",
                            }
                        },
                    }
                ]
            }
        )

        self.assertEqual("non_native_fixture", chili["category"])
        self.assertEqual(
            "non_native_fixture",
            audit.probe_failure_category("link target MissingProjectionLine was not created"),
        )
        self.assertEqual(
            "freecad_native_not_expressible",
            audit.probe_failure_category(
                "Sections[0] uses a subelement link; native App::PropertyLinkList collector only supports object links"
            ),
        )

    def test_builds_module_coverage_from_retained_closure_and_fixture_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            roles_path = self.bootstrap(root)
            contract_path, plan_path = self.retained_closure(root)
            authority = audit.build_report(root, roles_path)

            report = retained_coverage.build_module_coverage_report(
                authority,
                closure_contract_path=contract_path,
                pruning_plan_path=plan_path,
            )

        self.assertEqual("failed", report["coverageStatus"])
        self.assertEqual(4, report["globalFixtureCount"])
        modules = {item["name"]: item for item in report["modules"]}
        self.assertEqual(
            [
                "FreeCADBase",
                "FreeCADApp",
                "FreeCADMainCmd",
                "FreeCADMainPy",
                "Material",
                "Part",
                "Sketcher",
                "PartDesign",
                "Mesh",
                "Spreadsheet",
                "Assembly",
                "OndselSolver",
                "Help",
                "AddonManager",
            ],
            [item["name"] for item in report["modules"]],
        )
        self.assertEqual(4, modules["Part"]["fixtureCount"])
        self.assertEqual(1, modules["Part"]["nativeAuthorityCount"])
        self.assertEqual(1, modules["Part"]["protocolOnlyCount"])
        self.assertEqual(1, modules["Part"]["collectorGeneralGapCount"])
        self.assertEqual(1, modules["Part"]["notInvestigatedCount"])
        self.assertEqual(2, modules["Part"]["nativeEligibleWithoutAuthorityCount"])
        self.assertEqual("failed", modules["Part"]["coverageStatus"])
        self.assertEqual([], modules["Material"]["fixtures"])
        self.assertEqual("failed", modules["Material"]["coverageStatus"])
        self.assertEqual("non_cad_smoke", modules["Help"]["coverageKind"])
        self.assertEqual("failed", modules["Help"]["coverageStatus"])

    def test_module_owners_include_material_property_and_spreadsheet_object(self) -> None:
        material_fixture = {
            "Objects": [
                {
                    "Name": "Box",
                    "TypeId": "Part::Box",
                    "Properties": {
                        "ShapeMaterial": {
                            "PropertyType": "Materials::PropertyMaterial",
                            "Name": "Fixture material",
                        }
                    },
                }
            ]
        }
        spreadsheet_fixture = {
            "Objects": [
                {
                    "Name": "Sheet",
                    "TypeId": "Spreadsheet::Sheet",
                    "Properties": {},
                }
            ]
        }

        self.assertEqual(
            ["Material", "Part"],
            retained_coverage.fixture_owner_modules(material_fixture),
        )
        self.assertEqual(
            ["Spreadsheet"],
            retained_coverage.fixture_owner_modules(spreadsheet_fixture),
        )
        self.assertEqual("material", audit.business_family(material_fixture))
        self.assertEqual("spreadsheet", audit.business_family(spreadsheet_fixture))

    def test_public_capability_report_separates_execution_thin_and_uncovered(self) -> None:
        authority = {
            "cases": [
                {"phase": "p1", "case": "native", "role": "native"},
                {"phase": "p1", "case": "protocol", "role": "protocol_only"},
            ]
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "capabilities.json"
            self.write_json(
                contract_path,
                {
                    "schema": "freecad-retained-public-capabilities/v1",
                    "scope": "representative public capabilities and major runtime branches",
                    "requiredModules": ["Part"],
                    "modules": [
                        {
                            "name": "Part",
                            "capabilities": [
                                {
                                    "id": "primitive_execute",
                                    "publicSurface": "Part::Box",
                                    "runtimeBranch": "Execute a primitive",
                                    "disposition": "native_fixture",
                                    "sourceEvidence": ["src/Mod/Part/App/PrimitiveFeature.cpp::Box::execute"],
                                    "fixtureEvidence": [
                                        {"fixture": "p1/native", "level": "target_result"}
                                    ],
                                },
                                {
                                    "id": "property_only",
                                    "publicSurface": "Part::Feature properties",
                                    "runtimeBranch": "TypeId and property presence only",
                                    "disposition": "native_fixture",
                                    "sourceEvidence": ["src/Mod/Part/App/PartFeature.cpp::Feature"],
                                    "fixtureEvidence": [
                                        {"fixture": "p1/native", "level": "typeid_property_only"}
                                    ],
                                },
                                {
                                    "id": "export_side_effect",
                                    "publicSurface": "Part shape export",
                                    "runtimeBranch": "Write a STEP file",
                                    "disposition": "uncovered",
                                    "sourceEvidence": ["src/Mod/Part/App/TopoShapePyImp.cpp::exportStep"],
                                    "rationale": "No retained fixture exercises the public export side effect.",
                                    "fixtureEvidence": [],
                                },
                                {
                                    "id": "restore_only_state",
                                    "publicSurface": "App link restore",
                                    "runtimeBranch": "Repair a persisted label reference",
                                    "disposition": "protocol_only",
                                    "sourceEvidence": ["src/App/PropertyLinks.cpp::restoreLabelReference"],
                                    "rationale": "The state only exists during persisted restore.",
                                    "fixtureEvidence": [
                                        {"fixture": "p1/protocol", "level": "protocol_contract"}
                                    ],
                                },
                            ],
                        }
                    ],
                },
            )

            report = retained_coverage.build_public_capability_coverage_report(
                authority,
                capability_contract_path=contract_path,
                fixture_corpus_closure_status="passed",
            )

        capabilities = {
            item["id"]: item for item in report["modules"][0]["capabilities"]
        }
        self.assertEqual("passed", report["fixtureCorpusClosure"]["status"])
        self.assertEqual("partial", report["moduleApiCoverage"]["status"])
        self.assertEqual("not_evaluated", report["cadCoreRuntimeParity"]["status"])
        self.assertEqual("covered", capabilities["primitive_execute"]["coverageStatus"])
        self.assertEqual("thin", capabilities["property_only"]["coverageStatus"])
        self.assertEqual("uncovered", capabilities["export_side_effect"]["coverageStatus"])
        self.assertEqual(
            "non_native_exception",
            capabilities["restore_only_state"]["coverageStatus"],
        )

    def test_public_capability_contract_fails_when_required_module_is_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "capabilities.json"
            self.write_json(
                contract_path,
                {
                    "schema": "freecad-retained-public-capabilities/v1",
                    "scope": "fail-closed module inventory",
                    "requiredModules": ["Part", "Mesh"],
                    "modules": [
                        {
                            "name": "Part",
                            "capabilities": [
                                {
                                    "id": "primitive_execute",
                                    "publicSurface": "Part::Box",
                                    "runtimeBranch": "Box execute",
                                    "disposition": "uncovered",
                                    "sourceEvidence": [
                                        "src/Mod/Part/App/PrimitiveFeature.cpp::Box::execute"
                                    ],
                                    "fixtureEvidence": [],
                                    "rationale": "No fixture in the test inventory.",
                                }
                            ],
                        }
                    ],
                },
            )

            report = retained_coverage.build_public_capability_coverage_report(
                {"cases": []},
                capability_contract_path=contract_path,
                fixture_corpus_closure_status="passed",
            )

        self.assertEqual("failed", report["status"])
        self.assertIn(
            "capability contract modules must exactly match requiredModules order",
            report["errors"],
        )

    def test_public_capability_report_fails_when_native_authority_is_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "capabilities.json"
            self.write_json(
                contract_path,
                {
                    "schema": "freecad-retained-public-capabilities/v1",
                    "scope": "fail-closed native authority mapping",
                    "requiredModules": ["Part"],
                    "modules": [
                        {
                            "name": "Part",
                            "capabilities": [
                                {
                                    "id": "primitive_execute",
                                    "publicSurface": "Part::Box",
                                    "runtimeBranch": "Box execute",
                                    "disposition": "native_fixture",
                                    "sourceEvidence": [
                                        "src/Mod/Part/App/PrimitiveFeature.cpp::Box::execute"
                                    ],
                                    "fixtureEvidence": [
                                        {
                                            "fixture": "p1/missing",
                                            "level": "target_result",
                                        }
                                    ],
                                }
                            ],
                        }
                    ],
                },
            )

            report = retained_coverage.build_public_capability_coverage_report(
                {"cases": []},
                capability_contract_path=contract_path,
                fixture_corpus_closure_status="passed",
            )

        self.assertEqual("failed", report["status"])
        self.assertEqual("failed", report["moduleApiCoverage"]["status"])
        self.assertEqual(
            "missing_authority",
            report["modules"][0]["capabilities"][0]["coverageStatus"],
        )

    def test_public_api_surface_is_independent_and_capability_traceable(self) -> None:
        authority = {
            "cases": [{"phase": "mesh-primitives", "case": "sphere", "role": "native"}]
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            capabilities_path = root / "capabilities.json"
            surface_path = root / "api-surface.json"
            self.write_json(
                capabilities_path,
                {
                    "schema": "freecad-retained-public-capabilities/v1",
                    "requiredModules": ["Mesh"],
                    "modules": [
                        {
                            "name": "Mesh",
                            "capabilities": [{"id": "mesh_primitives"}],
                        }
                    ],
                },
            )
            self.write_json(
                surface_path,
                {
                    "schema": "freecad-retained-public-api-surface/v1",
                    "retainedClosureSource": {
                        "targetClosure": "probe_release_contract.json",
                        "pruningPlan": "pruning-plan.md",
                    },
                    "requiredModules": ["Mesh"],
                    "apis": [
                        {
                            "id": "mesh.sphere",
                            "module": "Mesh",
                            "symbol": "Mesh::Sphere::execute",
                            "publicSurface": "Mesh::Sphere",
                            "exposure": "DocumentObjectTypeId",
                            "sourceEvidence": [
                                "src/Mod/Mesh/App/FeatureMeshSolid.cpp::Sphere::execute"
                            ],
                            "runtimeBranches": ["execute sphere"],
                            "nativeExpressibility": "native_fixture",
                            "fixtureEvidence": [
                                {
                                    "fixture": "mesh-primitives/sphere",
                                    "level": "target_result",
                                    "branch": "execute sphere",
                                }
                            ],
                            "disposition": "native_fixture",
                            "capabilityIds": ["mesh_primitives"],
                        }
                    ],
                },
            )

            report = retained_coverage.build_public_api_coverage_report(
                authority,
                api_surface_path=surface_path,
                capability_contract_path=capabilities_path,
                fixture_corpus_closure_status="passed",
            )

        self.assertEqual("passed", report["apiSurfaceClosure"]["status"])
        self.assertEqual("passed", report["moduleApiCoverage"]["status"])
        self.assertEqual("passed", report["fixtureCorpusClosure"]["status"])
        self.assertEqual("not_evaluated", report["cadCoreRuntimeParity"]["status"])

    def test_public_api_surface_fails_closed_on_unmapped_capability(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            capabilities_path = root / "capabilities.json"
            surface_path = root / "api-surface.json"
            self.write_json(
                capabilities_path,
                {
                    "schema": "freecad-retained-public-capabilities/v1",
                    "requiredModules": ["Mesh"],
                    "modules": [
                        {
                            "name": "Mesh",
                            "capabilities": [
                                {"id": "mesh_primitives"},
                                {"id": "mesh_setops"},
                            ],
                        }
                    ],
                },
            )
            self.write_json(
                surface_path,
                {
                    "schema": "freecad-retained-public-api-surface/v1",
                    "retainedClosureSource": {
                        "targetClosure": "probe_release_contract.json",
                        "pruningPlan": "pruning-plan.md",
                    },
                    "requiredModules": ["Mesh"],
                    "apis": [
                        {
                            "id": "mesh.sphere",
                            "module": "Mesh",
                            "symbol": "Mesh::Sphere::execute",
                            "publicSurface": "Mesh::Sphere",
                            "exposure": "DocumentObjectTypeId",
                            "sourceEvidence": ["FeatureMeshSolid.cpp::Sphere::execute"],
                            "runtimeBranches": ["execute sphere"],
                            "nativeExpressibility": "native_fixture",
                            "fixtureEvidence": [],
                            "disposition": "uncovered",
                            "rationale": "No authority in the focused test.",
                            "capabilityIds": ["mesh_primitives"],
                        }
                    ],
                },
            )

            report = retained_coverage.build_public_api_coverage_report(
                {"cases": []},
                api_surface_path=surface_path,
                capability_contract_path=capabilities_path,
                fixture_corpus_closure_status="passed",
            )

        self.assertEqual("failed", report["apiSurfaceClosure"]["status"])
        self.assertTrue(
            any("Mesh/mesh_setops" in error for error in report["errors"])
        )

    def test_non_cad_smoke_requires_structured_passing_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            receipt_path = Path(temp_dir) / "Help.json"
            self.write_json(receipt_path, {"status": "passed"})
            malformed = retained_coverage._non_cad_module_row(
                "Help",
                receipt_path,
                evidence=["plan.md"],
            )
            self.write_json(
                receipt_path,
                {
                    "schema": "freecad-non-cad-smoke/v1",
                    "entry": "Help",
                    "status": "passed",
                    "producer": {"sha256": "abc123"},
                    "checks": [
                        {
                            "id": "help-import-and-pure-path",
                            "status": "passed",
                        }
                    ],
                },
            )
            valid = retained_coverage._non_cad_module_row(
                "Help",
                receipt_path,
                evidence=["plan.md"],
            )

        self.assertEqual("failed", malformed["coverageStatus"])
        self.assertEqual("non_cad_smoke", valid["coverageStatus"])

    def test_cli_rebuilds_inventory_and_coverage_baseline_together(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            roles_path = self.bootstrap(root)
            contract_path, plan_path = self.retained_closure(root)
            inventory_path = root / "reports" / "inventory.json"
            coverage_path = root / "reports" / "coverage.json"

            result = audit.main(
                [
                    "--root",
                    str(root),
                    "--roles",
                    str(roles_path),
                    "--report",
                    str(inventory_path),
                    "--coverage-report",
                    str(coverage_path),
                    "--closure-contract",
                    str(contract_path),
                    "--pruning-plan",
                    str(plan_path),
                ]
            )

            inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
            coverage = json.loads(coverage_path.read_text(encoding="utf-8"))

        self.assertEqual(0, result)
        self.assertEqual("passed", inventory["status"])
        self.assertEqual("passed", coverage["status"])
        self.assertEqual("failed", coverage["coverageStatus"])

    def test_module_coverage_rejects_failed_or_stale_producer_report(self) -> None:
        authority = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        smoke_receipts = {
            entry: TOOLS
            / "freecad_expected_parity"
            / "reports"
            / "non_cad_smoke"
            / f"{entry}.json"
            for entry in ("Help", "AddonManager")
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            producer_report = Path(temp_dir) / "all-native.json"
            variants = {
                "failed": {"status": "failed"},
                "stale": {
                    "status": "passed",
                    "mode": "repeated-native-check",
                    "manifest": {"cases": 557, "entries": []},
                },
            }
            for label, payload in variants.items():
                with self.subTest(label=label):
                    self.write_json(producer_report, payload)
                    coverage = retained_coverage.build_module_coverage_report(
                        authority,
                        closure_contract_path=audit.DEFAULT_CLOSURE_CONTRACT,
                        pruning_plan_path=audit.DEFAULT_PRUNING_PLAN,
                        producer_report_path=producer_report,
                        non_cad_smoke_receipts=smoke_receipts,
                    )
                    self.assertEqual("failed", coverage["coverageStatus"])
                    self.assertTrue(
                        any(
                            "producer report" in item
                            for item in coverage["producerValidation"]["errors"]
                        )
                    )

    def test_checked_in_report_matches_live_inventory(self) -> None:
        report_path = (
            TOOLS
            / "freecad_expected_parity"
            / "reports"
            / "fixture_authority_inventory.v1.json"
        )
        self.assertTrue(report_path.is_file())
        checked_in = json.loads(report_path.read_text(encoding="utf-8"))
        live = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        self.assertEqual(checked_in, live)

    def test_checked_in_module_coverage_matches_live_closure(self) -> None:
        report_path = (
            TOOLS
            / "freecad_expected_parity"
            / "reports"
            / "retained_module_fixture_coverage.v1.json"
        )
        producer_report = (
            TOOLS
            / "freecad_expected_parity"
            / "reports"
            / "producer-reproduction.v1.json"
        )
        self.assertTrue(report_path.is_file())
        checked_in = json.loads(report_path.read_text(encoding="utf-8"))
        authority = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        live = retained_coverage.build_module_coverage_report(
            authority,
            closure_contract_path=audit.DEFAULT_CLOSURE_CONTRACT,
            pruning_plan_path=audit.DEFAULT_PRUNING_PLAN,
            producer_report_path=producer_report,
            non_cad_smoke_receipts={
                entry: TOOLS
                / "freecad_expected_parity"
                / "reports"
                / "non_cad_smoke"
                / f"{entry}.json"
                for entry in ("Help", "AddonManager")
            },
        )
        self.assertEqual(checked_in, live)
        self.assertEqual("passed", live["coverageStatus"])
        self.assertEqual(0, live["retainedModuleCollectorImplementationQueueCount"])

    def test_checked_in_public_capability_report_matches_live_reverse_inventory(self) -> None:
        parity_root = TOOLS / "freecad_expected_parity"
        report_path = (
            parity_root / "reports" / "retained_public_capability_coverage.v1.json"
        )
        contract_path = parity_root / "retained_public_capabilities.v1.json"
        closure_path = (
            parity_root / "reports" / "retained_module_fixture_coverage.v1.json"
        )
        self.assertTrue(report_path.is_file())
        self.assertTrue(contract_path.is_file())

        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        for module in contract["modules"]:
            for capability in module["capabilities"]:
                for source in capability["sourceEvidence"]:
                    source_path = CAD_CORE_ROOT.parent / source.split("::", 1)[0]
                    self.assertTrue(source_path.exists(), source)

        checked_in = json.loads(report_path.read_text(encoding="utf-8"))
        closure = json.loads(closure_path.read_text(encoding="utf-8"))
        authority = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        live = retained_coverage.build_public_capability_coverage_report(
            authority,
            capability_contract_path=contract_path,
            fixture_corpus_closure_status=closure["coverageStatus"],
        )
        self.assertEqual(checked_in, live)
        self.assertEqual("passed", live["fixtureCorpusClosure"]["status"])
        self.assertEqual("covered", live["moduleApiCoverage"]["status"])
        self.assertEqual("not_evaluated", live["cadCoreRuntimeParity"]["status"])

        modules = {row["name"]: row for row in live["modules"]}
        for module_name in ("Material", "Mesh", "Spreadsheet"):
            covered = [
                row
                for row in modules[module_name]["capabilities"]
                if row["coverageStatus"] == "covered"
            ]
            self.assertGreaterEqual(len(covered), 3, module_name)

        mesh_capabilities = {
            row["id"]: row for row in modules["Mesh"]["capabilities"]
        }
        self.assertEqual(
            "non_native_exception",
            mesh_capabilities["feature_mesh_transform"]["coverageStatus"],
        )

    def test_checked_in_public_api_report_matches_live_source_denominator(self) -> None:
        parity_root = TOOLS / "freecad_expected_parity"
        report_path = parity_root / "reports" / "retained_public_api_coverage.v1.json"
        surface_path = parity_root / "retained_public_api_surface.v1.json"
        capabilities_path = parity_root / "retained_public_capabilities.v1.json"
        closure_path = (
            parity_root / "reports" / "retained_module_fixture_coverage.v1.json"
        )
        self.assertTrue(report_path.is_file())
        self.assertTrue(surface_path.is_file())

        checked_in = json.loads(report_path.read_text(encoding="utf-8"))
        closure = json.loads(closure_path.read_text(encoding="utf-8"))
        authority = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        live = retained_coverage.build_public_api_coverage_report(
            authority,
            api_surface_path=surface_path,
            capability_contract_path=capabilities_path,
            fixture_corpus_closure_status=closure["coverageStatus"],
        )
        self.assertEqual(checked_in, live)
        self.assertEqual("passed", live["apiSurfaceClosure"]["status"])
        self.assertEqual(74, live["apiSurfaceClosure"]["apiCount"])
        self.assertEqual(0, live["apiSurfaceClosure"]["unclassifiedApiCount"])
        self.assertEqual("partial", live["moduleApiCoverage"]["status"])
        self.assertEqual("passed", live["fixtureCorpusClosure"]["status"])
        self.assertEqual("not_evaluated", live["cadCoreRuntimeParity"]["status"])
        ondsel = next(row for row in live["modules"] if row["name"] == "OndselSolver")
        matrix_api = next(
            row
            for row in ondsel["apis"]
            if row["id"] == "ondselsolver.full_joint_type_and_degenerate_matrix"
        )
        self.assertEqual("covered", matrix_api["coverageStatus"])
        self.assertEqual("passed", matrix_api["supportMatrixReceipt"]["status"])

    def test_checked_in_fixture_phases_are_module_capability_classifications(self) -> None:
        report = audit.build_report(CAD_CORE_ROOT, audit.ROLES_PATH)
        classification = report["phaseClassification"]
        self.assertEqual("passed", classification["status"])
        self.assertEqual(44, classification["phaseCount"])
        self.assertEqual([], classification["errors"])

        phases = {row["phase"] for row in classification["phases"]}
        self.assertIn("partdesign-extrude", phases)
        self.assertIn("topology-resolve", phases)
        self.assertIn("sketcher-solve", phases)
        self.assertNotIn("p8", phases)
        self.assertNotIn("c4m6", phases)

        legacy_map_path = (
            TOOLS / "freecad_expected_parity" / "fixture_legacy_phase_map.v1.json"
        )
        legacy_map = json.loads(legacy_map_path.read_text(encoding="utf-8"))
        self.assertEqual("cad-core.fixture-legacy-phase-map.v1", legacy_map["schemaVersion"])
        self.assertEqual(840, len(legacy_map["cases"]))
        targets = {(row["phase"], row["case"]) for row in legacy_map["cases"]}
        self.assertEqual(840, len(targets))
        self.assertEqual(
            {(row["phase"], row["case"]) for row in report["cases"]},
            targets,
        )

    def test_explicit_semantic_phase_is_not_silently_ignored(self) -> None:
        with self.assertRaisesRegex(AssertionError, "does not exist in semantic phase"):
            semantic_fixture_path("part-boolean-fragments", "p8")

        self.assertEqual(
            "part-boolean",
            semantic_fixture_path("part-boolean-fragments").parent.name,
        )


if __name__ == "__main__":
    unittest.main()
