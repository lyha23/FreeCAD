from __future__ import annotations

import json
import stat
import sys
import tempfile
import unittest
from pathlib import Path

try:
    from .fixture_runner import ROOT
except ImportError:  # pragma: no cover - supports unittest discovery.
    from fixture_runner import ROOT


TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import EvaluationRequest, MaterializeRequest, evaluate, materialize_current
from freecad_expected_parity.catalog import ROLES_SCHEMA, load_catalog
from freecad_expected_parity.registry import REGISTRY_SCHEMA


class FreecadExpectedPublicParityTest(unittest.TestCase):
    def write_json(self, path: Path, payload: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def bootstrap(self, root: Path, *, expected: dict | None = None, current: dict | None = None) -> tuple[Path, Path]:
        expected = {"diagnostics": [], "results": []} if expected is None else expected
        current = expected if current is None else current
        self.write_json(root / "fixtures" / "demo" / "case-a.json", {"request": "demo"})
        self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.freecad.json", expected)
        self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.freecad.ledger.json", {})
        self.write_json(root / "fixtures" / "demo" / "cad-core-res" / "case-a.cad-core.json", current)
        roles = root / "fixture_roles.v1.json"
        self.write_json(
            roles,
            {
                "schemaVersion": ROLES_SCHEMA,
                "legacyNativeExpectedDiscovery": False,
                "requireCompleteInputCoverage": True,
                "roles": [{"phase": "demo", "case": "case-a", "role": "native"}],
            },
        )
        registry = root / "protocol_divergences.v1.json"
        self.write_json(registry, {"schemaVersion": REGISTRY_SCHEMA, "entries": []})
        return roles, registry

    def request(self, root: Path, roles: Path, registry: Path, **kwargs: object) -> EvaluationRequest:
        return EvaluationRequest(
            root=root,
            phase="demo",
            roles_path=roles,
            registry_path=registry,
            validate_ledger=False,
            **kwargs,
        )

    def test_evaluate_is_v2_and_normalizes_only_raw_mapped_hashes(self) -> None:
        expected = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "indexed": "Face1",
                            "mappedName": {"raw": "Face1;:H65a,F", "canonical": "Face1;:H*,F"},
                            "stableSubname": "Face1;:H*,F",
                        }
                    ],
                }
            ],
        }
        actual = json.loads(json.dumps(expected))
        actual["results"][0]["subshapes"][0]["mappedName"]["raw"] = "Face1;:H89b,F"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("cad-core.freecad-expected-parity.v2", report["schemaVersion"])
        self.assertEqual("green", report["exactStatus"])
        self.assertEqual("green", report["semanticStatus"])
        self.assertEqual("not_evaluated", report["releaseStatus"])
        self.assertEqual(1, report["summary"]["cases"])
        self.assertIn("comparisonProfileSha256", report["runEvidence"])
        self.assertIn("fixtureRolesSha256", report["runEvidence"])

    def test_raw_mapped_name_may_differ_when_canonical_identity_matches(self) -> None:
        expected = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "indexed": "Face1",
                            "mappedName": {"raw": "freecad-local-token", "canonical": "Face1;:H*,F"},
                        }
                    ],
                }
            ],
        }
        actual = json.loads(json.dumps(expected))
        actual["results"][0]["subshapes"][0]["mappedName"]["raw"] = "cad-core-local-token"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["exactStatus"])
        self.assertEqual("green", report["semanticStatus"])
        self.assertEqual(1, report["summary"]["diffs"])
        diff = report["cases"][0]["diffs"][0]
        self.assertEqual("representation_difference", diff["comparisonClass"])
        self.assertEqual("allowed_representation_difference", diff["decision"])
        self.assertTrue(diff["accepted"])

    def test_canonical_mapped_name_difference_remains_semantic_red(self) -> None:
        expected = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "indexed": "Face1",
                            "mappedName": {"raw": "freecad-token", "canonical": "Face1;:H*,F"},
                        }
                    ],
                }
            ],
        }
        actual = json.loads(json.dumps(expected))
        actual["results"][0]["subshapes"][0]["mappedName"]["canonical"] = "Face2;:H*,F"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["semanticStatus"])
        diff = report["cases"][0]["diffs"][0]
        self.assertTrue(diff["path"].startswith("results.Box.subshapes.Face1;:H*,F"))
        self.assertEqual("public_semantic", diff["comparisonClass"])
        self.assertFalse(diff["accepted"])

    def test_producer_local_subshape_tokens_may_differ_when_canonical_identity_matches(self) -> None:
        expected = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "id": "Box:Face1",
                            "fullSubname": "Box.Face1",
                            "indexed": "Face1",
                            "subname": "Face1",
                            "resolvedIndexed": "Face1",
                            "stableSubname": "freecad-stable-token",
                            "rawFreecadMappedName": "freecad-raw-token",
                            "canonicalFreecadMappedName": "Box.Face;:H*,F",
                            "identityStatus": "stable",
                            "kind": "Face",
                        }
                    ],
                }
            ],
        }
        actual = json.loads(json.dumps(expected))
        subshape = actual["results"][0]["subshapes"][0]
        subshape.update(
            {
                "id": "Box:Face7",
                "fullSubname": "Box.Face7",
                "indexed": "Face7",
                "subname": "Face7",
                "resolvedIndexed": "Face7",
                "stableSubname": "cad-core-stable-token",
                "rawFreecadMappedName": "cad-core-raw-token",
            }
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["exactStatus"])
        self.assertEqual("green", report["semanticStatus"])
        self.assertEqual(0, report["summary"]["semanticDiffs"])
        self.assertGreater(report["summary"]["representationDifferences"], 0)

    def test_zero_case_and_missing_role_are_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root)
            zero = evaluate(self.request(root, roles, registry, case="absent")).to_dict()
            self.assertEqual("invalid", zero["releaseStatus"])
            self.assertIn("zero native fixture cases selected", zero["preflight"]["errors"])

            self.write_json(root / "fixtures" / "demo" / "unassigned.json", {})
            missing_role = evaluate(self.request(root, roles, registry)).to_dict()

        self.assertEqual("invalid", missing_role["releaseStatus"])
        self.assertTrue(any("has no role" in error for error in missing_role["preflight"]["errors"]))

    def test_phase_without_native_expected_is_not_applicable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_json(root / "fixtures" / "probe-only" / "shape-fix.json", {"TypeId": "CadCore::Probe"})
            roles = root / "fixture_roles.v1.json"
            self.write_json(
                roles,
                {
                    "schemaVersion": ROLES_SCHEMA,
                    "legacyNativeExpectedDiscovery": False,
                    "requireCompleteInputCoverage": True,
                    "roles": [
                        {
                            "phase": "probe-only",
                            "case": "shape-fix",
                            "role": "unsupported",
                            "reason": "internal semantic probe",
                            "authority": "cad-core probe test",
                            "nextAction": "run focused probe",
                            "closeCondition": "probe test green",
                        }
                    ],
                },
            )
            registry = root / "protocol_divergences.v1.json"
            self.write_json(registry, {"schemaVersion": REGISTRY_SCHEMA, "entries": []})
            report = evaluate(
                EvaluationRequest(
                    root=root,
                    phase="probe-only",
                    roles_path=roles,
                    registry_path=registry,
                    source_kind="snapshot",
                    validate_ledger=False,
                )
            ).to_dict()

        self.assertEqual("not_applicable", report["releaseStatus"])
        self.assertEqual("not_applicable", report["status"])
        self.assertFalse(report["releaseGatePassed"])
        self.assertEqual([], report["cases"])
        self.assertTrue(report["preflight"]["valid"])

    def test_missing_ledger_is_invalid_even_when_ledger_validation_is_disabled(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root)
            (root / "fixtures" / "demo" / "expected" / "case-a.freecad.ledger.json").unlink()
            report = evaluate(self.request(root, roles, registry)).to_dict()

        self.assertEqual("invalid", report["releaseStatus"])
        self.assertTrue(any("ledger is missing" in error for error in report["preflight"]["errors"]))

    def test_invalid_actual_json_is_an_explicit_invalid_state(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": "{not json"})
            ).to_dict()

        self.assertEqual("invalid", report["releaseStatus"])
        self.assertEqual("not_evaluated", report["exactStatus"])
        self.assertIn("invalid JSON", report["cases"][0]["sourceError"])

    def test_registry_is_exact_and_stale_entries_are_invalid(self) -> None:
        expected = {"diagnostics": [], "results": []}
        actual = {"diagnostics": [], "results": [{"object": "Box"}]}
        entry = {
            "id": "DEMO-RESULT-001",
            "selector": {
                "phase": "demo",
                "case": "case-a",
                "category": "results",
                "kind": "extra",
                "path": "results.Box",
            },
            "actualContract": {"type": "object", "keysMode": "exact", "requiredKeys": ["object"]},
            "nativeExpected": "Native response publishes no result.",
            "cadCoreProtocol": "Demo transport result.",
            "frontendImpact": "Test-only evidence.",
            "authority": "tests.test_freecad_expected_public_parity",
            "contractTests": [
                "tests.test_freecad_expected_public_parity.FreecadExpectedPublicParityTest.test_registry_is_exact_and_stale_entries_are_invalid"
            ],
            "removeWhen": "The test transport is removed.",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            self.write_json(registry, {"schemaVersion": REGISTRY_SCHEMA, "entries": [entry]})
            accepted = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()
            self.assertEqual("red", accepted["exactStatus"])
            self.assertEqual("green", accepted["semanticStatus"])
            self.assertTrue(accepted["registryAudit"]["valid"])
            self.assertEqual([entry["contractTests"][0]], accepted["registryAudit"]["contractTests"])

            entry["selector"] = dict(entry["selector"], kind="missing")
            self.write_json(registry, {"schemaVersion": REGISTRY_SCHEMA, "entries": [entry]})
            stale = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("invalid", stale["releaseStatus"])
        self.assertIn("DEMO-RESULT-001", stale["registryAudit"]["staleEntries"])

    def test_registry_contract_drift_is_semantic_red_without_invalidating_the_registry(self) -> None:
        expected = {"diagnostics": [], "results": []}
        actual = {"diagnostics": [], "results": [{"object": "Box", "unexpected": True}]}
        entry = {
            "id": "DEMO-RESULT-001",
            "selector": {
                "phase": "demo",
                "case": "case-a",
                "category": "results",
                "kind": "extra",
                "path": "results.Box",
            },
            "actualContract": {"type": "object", "keysMode": "exact", "requiredKeys": ["object"]},
            "nativeExpected": "Native response publishes no result.",
            "cadCoreProtocol": "Demo transport result.",
            "frontendImpact": "Test-only evidence.",
            "authority": "tests.test_freecad_expected_public_parity",
            "contractTests": [
                "tests.test_freecad_expected_public_parity.FreecadExpectedPublicParityTest.test_registry_contract_drift_is_semantic_red_without_invalidating_the_registry"
            ],
            "removeWhen": "The test transport is removed.",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            self.write_json(registry, {"schemaVersion": REGISTRY_SCHEMA, "entries": [entry]})
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["exactStatus"])
        self.assertEqual("red", report["semanticStatus"])
        self.assertTrue(report["registryAudit"]["valid"])
        self.assertEqual("DEMO-RESULT-001", report["registryAudit"]["contractFailures"][0]["id"])

    def test_registry_contract_schema_rejects_incompatible_keywords_and_const_types(self) -> None:
        expected = {"diagnostics": [], "results": []}
        actual = {"diagnostics": [], "results": [{"object": "Box"}]}
        entry = {
            "id": "DEMO-RESULT-001",
            "selector": {
                "phase": "demo",
                "case": "case-a",
                "category": "results",
                "kind": "extra",
                "path": "results.Box",
            },
            "actualContract": {"type": "array", "keysMode": "exact", "requiredKeys": ["object"]},
            "nativeExpected": "Native response publishes no result.",
            "cadCoreProtocol": "Demo transport result.",
            "frontendImpact": "Test-only evidence.",
            "authority": "tests.test_freecad_expected_public_parity",
            "contractTests": [
                "tests.test_freecad_expected_public_parity.FreecadExpectedPublicParityTest.test_registry_contract_schema_rejects_incompatible_keywords_and_const_types"
            ],
            "removeWhen": "The test transport is removed.",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            invalid_const = dict(entry, id="DEMO-RESULT-002", actualContract={"type": "string", "const": 1})
            wildcard = dict(
                entry,
                id="DEMO-RESULT-003",
                selector=dict(entry["selector"], path="results.*.mesh"),
                actualContract={"type": "object", "keysMode": "exact", "requiredKeys": ["object"]},
            )
            self.write_json(
                registry,
                {"schemaVersion": REGISTRY_SCHEMA, "entries": [entry, invalid_const, wildcard]},
            )
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("invalid", report["releaseStatus"])
        errors = report["registryAudit"]["validationErrors"]
        self.assertTrue(any("object-only fields require type object" in error for error in errors))
        self.assertTrue(any("const does not match" in error for error in errors))
        self.assertTrue(any("exact literal, not a pattern" in error for error in errors))

    def test_reference_shadow_transport_contract_rejects_nested_array_drift(self) -> None:
        current_path = (
            ROOT
            / "fixtures"
            / "c4m6"
            / "cad-core-res"
            / "topo-state-reference-shadow-brep.cad-core.json"
        )
        actual = json.loads(current_path.read_text(encoding="utf-8"))
        probe = next(item for item in actual["results"] if item["object"] == "ProbeSketch")
        edge = next(item for item in probe["subshapes"] if item["indexed"] == "Edge1")
        edge["ReferenceShadow"] = [{"unexpected": "snapshot geometry"}]

        report = evaluate(
            EvaluationRequest(
                root=ROOT,
                phase="c4m6",
                case="topo-state-reference-shadow-brep",
                source_kind="in_memory",
                in_memory_actuals={("c4m6", "topo-state-reference-shadow-brep"): actual},
            )
        ).to_dict()

        self.assertTrue(report["registryAudit"]["valid"])
        self.assertEqual("red", report["semanticStatus"])
        self.assertEqual("C13M5-C4M6-TRANSPORT-005", report["registryAudit"]["contractFailures"][0]["id"])

    def test_unregistered_mesh_field_is_a_non_blocking_product_extension(self) -> None:
        expected = {"diagnostics": [], "results": [{"object": "Box"}]}
        actual = {"diagnostics": [], "results": [{"object": "Box", "mesh": {"vertices": []}}]}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["exactStatus"])
        self.assertEqual("green", report["semanticStatus"])
        self.assertEqual("results.Box.mesh", report["cases"][0]["diffs"][0]["path"])
        diff = report["cases"][0]["diffs"][0]
        self.assertEqual("product_extension", diff["comparisonClass"])
        self.assertEqual("allowed_product_extension", diff["decision"])
        self.assertTrue(diff["accepted"])

    def test_actual_only_result_field_is_a_non_blocking_product_extension(self) -> None:
        expected = {"diagnostics": [], "results": [{"object": "Box", "volume": 1.0}]}
        actual = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "volume": 1.0,
                    "frontendTransport": {"pickable": True},
                }
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("green", report["semanticStatus"])
        diff = report["cases"][0]["diffs"][0]
        self.assertEqual("results.Box.frontendTransport", diff["path"])
        self.assertEqual("product_extension", diff["comparisonClass"])
        self.assertEqual("allowed_product_extension", diff["decision"])

    def test_actual_only_diagnostic_remains_semantic_red(self) -> None:
        expected = {"diagnostics": [], "results": []}
        actual = {
            "diagnostics": [
                {
                    "code": "unexpected_failure",
                    "severity": "error",
                    "object": "Box",
                }
            ],
            "results": [],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, expected=expected, current=actual)
            report = evaluate(
                self.request(root, roles, registry, source_kind="in_memory", in_memory_actuals={"demo/case-a": actual})
            ).to_dict()

        self.assertEqual("red", report["semanticStatus"])
        diff = report["cases"][0]["diffs"][0]
        self.assertEqual("diagnostics.unexpected_failure", diff["path"])
        self.assertEqual("public_semantic", diff["comparisonClass"])
        self.assertFalse(diff["accepted"])

    def test_fixture_role_artifact_audit_rejects_duplicate_orphan_and_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root)
            roles_payload = json.loads(roles.read_text(encoding="utf-8"))
            roles_payload["roles"].append(dict(roles_payload["roles"][0]))
            self.write_json(roles, roles_payload)
            self.write_json(root / "fixtures" / "demo" / "expected" / "orphan.freecad.json", {})
            self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.expeted.json", {})
            report = evaluate(self.request(root, roles, registry)).to_dict()

        self.assertEqual("invalid", report["releaseStatus"])
        errors = report["preflight"]["errors"]
        self.assertTrue(any("duplicate fixture role" in error for error in errors))
        self.assertTrue(any("native expected has no fixture role" in error for error in errors))
        self.assertTrue(any("non-protocol fixture retains protocol expected" in error for error in errors))

    def test_fixture_roles_cannot_enable_legacy_discovery_or_coverage_opt_out(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root)
            roles_payload = json.loads(roles.read_text(encoding="utf-8"))
            roles_payload["legacyNativeExpectedDiscovery"] = True
            self.write_json(roles, roles_payload)
            legacy = evaluate(self.request(root, roles, registry)).to_dict()

            roles_payload["legacyNativeExpectedDiscovery"] = False
            roles_payload["requireCompleteInputCoverage"] = False
            self.write_json(roles, roles_payload)
            opt_out = evaluate(self.request(root, roles, registry)).to_dict()

        self.assertEqual("invalid", legacy["releaseStatus"])
        self.assertIn(
            "fixture roles manifest must set legacyNativeExpectedDiscovery to false",
            legacy["preflight"]["errors"],
        )
        self.assertEqual("invalid", opt_out["releaseStatus"])
        self.assertIn(
            "fixture roles manifest must set requireCompleteInputCoverage to true",
            opt_out["preflight"]["errors"],
        )

    def test_checked_in_roles_preserve_representative_phase_snapshot_discovery(self) -> None:
        for phase in ("c3m1", "c10m1", "c12m12", "c3m5", "c3m6"):
            with self.subTest(phase=phase):
                catalog = load_catalog(ROOT, phase=phase)
                self.assertEqual([], catalog.errors)
                self.assertGreater(len(catalog.cases), 0)

    def test_representative_family_snapshot_reports_keep_metadata_without_acceptance(self) -> None:
        expected_known_gap_ids = {
            "c3m1": "C13M5-S4-KG-TOPO-001",
            "c10m1": "C13M5-S4-KG-SKETCH-001",
            "c12m12": "C13M5-S4-KG-PART-001",
            "c3m5": "C13M5-S4-KG-PD-001",
            "c3m6": "C13M5-S4-KG-ASM-001",
        }
        metadata_fields = (
            "owner",
            "owner_step",
            "decision",
            "source",
            "freecad_authority",
            "next_action",
            "close_condition",
            "knownGapId",
        )
        for phase, known_gap_id in expected_known_gap_ids.items():
            with self.subTest(phase=phase):
                report = evaluate(
                    EvaluationRequest(root=ROOT, phase=phase, source_kind="snapshot")
                ).to_dict()
                self.assertTrue(report["preflight"]["valid"], report["preflight"]["errors"])
                self.assertGreater(report["summary"]["cases"], 0)
                self.assertEqual("red", report["exactStatus"])
                self.assertEqual("red", report["semanticStatus"])
                self.assertEqual("not_evaluated", report["releaseStatus"])
                diffs = [diff for item in report["cases"] for diff in item["diffs"]]
                semantic_diffs = [diff for diff in diffs if diff["comparisonClass"] == "public_semantic"]
                observations = [diff for diff in diffs if diff["comparisonClass"] != "public_semantic"]
                self.assertGreater(len(semantic_diffs), 0)
                for diff in semantic_diffs:
                    self.assertFalse(diff["accepted"])
                    self.assertEqual("S4", diff["owner_step"])
                    self.assertEqual(known_gap_id, diff["knownGapId"])
                    self.assertNotEqual("unaccepted_diff", diff["decision"])
                    for field in metadata_fields:
                        self.assertTrue(diff[field], f"{phase} {diff['path']} missing {field}")
                for observation in observations:
                    self.assertTrue(observation["accepted"])
                    self.assertIn(
                        observation["decision"],
                        {"allowed_product_extension", "allowed_representation_difference"},
                    )

    def test_live_freshness_and_materialization_are_atomic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            roles, registry = self.bootstrap(root, current={"before": True})
            binary = root / "build" / "cad-core"
            binary.parent.mkdir(parents=True)
            binary.write_text(
                "#!/usr/bin/env python3\n"
                "import json, pathlib, sys\n"
                "out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
                "out.write_text(json.dumps({'after': True}))\n",
                encoding="utf-8",
            )
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
            live = evaluate(
                self.request(root, roles, registry, source_kind="live", binary=binary)
            ).to_dict()
            self.assertEqual("invalid", live["releaseStatus"])
            self.assertFalse(live["cases"][0]["artifactEvidence"]["currentFresh"])

            generated = materialize_current(
                MaterializeRequest(root=root, phase="demo", binary=binary, roles_path=roles, validate_ledger=False)
            ).to_dict()
            current_path = root / "fixtures" / "demo" / "cad-core-res" / "case-a.cad-core.json"
            self.assertEqual("ok", generated["status"])
            self.assertEqual({"after": True}, json.loads(current_path.read_text(encoding="utf-8")))

            binary.write_text(
                "#!/usr/bin/env python3\n"
                "import pathlib, sys\n"
                "out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
                "out.write_text('{not json')\n",
                encoding="utf-8",
            )
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
            rejected = materialize_current(
                MaterializeRequest(root=root, phase="demo", binary=binary, roles_path=roles, validate_ledger=False)
            ).to_dict()

            self.assertEqual("invalid", rejected["status"])
            self.assertEqual({"after": True}, json.loads(current_path.read_text(encoding="utf-8")))


if __name__ == "__main__":
    unittest.main()
