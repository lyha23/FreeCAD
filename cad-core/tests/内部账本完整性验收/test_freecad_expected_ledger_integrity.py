from __future__ import annotations

import copy
import contextlib
import importlib.util
import io
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = CAD_CORE_ROOT / "tools" / "validate_freecad_expected_ledger.py"
SPEC = importlib.util.spec_from_file_location("validate_freecad_expected_ledger", SCRIPT_PATH)
assert SPEC is not None
validator = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)

COLLECTOR_PATH = CAD_CORE_ROOT / "tools" / "collect_freecad_expected.py"
COLLECTOR_SPEC = importlib.util.spec_from_file_location("collect_freecad_expected", COLLECTOR_PATH)
assert COLLECTOR_SPEC is not None
collector = importlib.util.module_from_spec(COLLECTOR_SPEC)
assert COLLECTOR_SPEC.loader is not None
sys.modules[COLLECTOR_SPEC.name] = collector
COLLECTOR_SPEC.loader.exec_module(collector)

REAL_EXPECTED_PATH = (
    CAD_CORE_ROOT
    / "fixtures"
    / "c4m6"
    / "expected"
    / "topo-state-body-tip-stable-recovery.freecad.json"
)
REAL_LEDGER_PATH = REAL_EXPECTED_PATH.with_name(
    "topo-state-body-tip-stable-recovery.freecad.ledger.json"
)
REAL_FIXTURE_PATH = REAL_EXPECTED_PATH.parent.parent / "topo-state-body-tip-stable-recovery.json"
LINK_EXPECTED_PATH = (
    CAD_CORE_ROOT
    / "fixtures"
    / "c4m6"
    / "expected"
    / "topo-state-link-compound-child-maps.freecad.json"
)
LINK_LEDGER_PATH = LINK_EXPECTED_PATH.with_name(
    "topo-state-link-compound-child-maps.freecad.ledger.json"
)
LINK_FIXTURE_PATH = LINK_EXPECTED_PATH.parent.parent / "topo-state-link-compound-child-maps.json"
RECOVERABLE_EXPECTED_PATH = (
    CAD_CORE_ROOT
    / "fixtures"
    / "p5"
    / "expected"
    / "sketch-external-internal-edge-stable-recover.freecad.json"
)
RECOVERABLE_LEDGER_PATH = RECOVERABLE_EXPECTED_PATH.with_name(
    "sketch-external-internal-edge-stable-recover.freecad.ledger.json"
)
SHADOW_EXPECTED_PATH = (
    CAD_CORE_ROOT
    / "fixtures"
    / "c4m6"
    / "expected"
    / "topo-state-reference-shadow-brep.freecad.json"
)
SHADOW_LEDGER_PATH = SHADOW_EXPECTED_PATH.with_name(
    "topo-state-reference-shadow-brep.freecad.ledger.json"
)
SHADOW_FIXTURE_PATH = SHADOW_EXPECTED_PATH.parent.parent / "topo-state-reference-shadow-brep.json"


class FreecadExpectedLedgerIntegrityTest(unittest.TestCase):
    def synthetic_capture_for_references(
        self,
        replay_payload: dict,
        input_references: list[dict],
    ) -> dict:
        topo_state = copy.deepcopy(replay_payload["topoNamingState"])
        objects = topo_state.get("objects", {})
        bindings = []
        for ref in input_references:
            target_name = ref.get("target") or ref.get("owner")
            stable_token = ref.get("stableSubname") or ref.get("element")
            entry = collector.ledger_topo_state_entry_for_token(
                objects,
                target_name,
                stable_token,
            )
            target = entry.get("target") if isinstance(entry, dict) else None
            if not isinstance(target, dict):
                continue
            bindings.append({
                "owner": ref.get("owner"),
                "propertyPath": ref.get("propertyPath"),
                "target": ref.get("target"),
                "stableSubname": ref.get("stableSubname") or ref.get("element"),
                "resolvedObject": target.get("object"),
                "resolvedSubname": target.get("subname"),
                "nativeResolvedSubname": target.get("subname"),
                "nativeResolutionEvidence": "FreeCAD.getSubObject",
                "assignmentStatus": "succeeded",
            })
        return {
            "rawTopoNamingState": topo_state,
            "publishedTopoNamingState": copy.deepcopy(topo_state),
            "resolvedReferenceBindings": bindings,
        }

    def validate_mutated_case(
        self,
        expected_source: Path,
        ledger_source: Path,
        fixture_source: Path,
        mutate,
    ) -> list[str]:
        expected = validator.load_json(expected_source)
        ledger = validator.load_json(ledger_source)
        mutate(expected, ledger)
        topo_hash = validator.sha256_json(expected["topoNamingState"])
        ledger["fixture"]["expectedPayloadHash"] = validator.sha256_json(expected)
        ledger["fixture"]["topoNamingStateHash"] = topo_hash
        ledger["roundTrip"]["inputTopoNamingStateHash"] = topo_hash

        with tempfile.TemporaryDirectory() as temp_dir:
            phase_dir = Path(temp_dir) / expected_source.parent.parent.name
            expected_dir = phase_dir / "expected"
            expected_dir.mkdir(parents=True)
            fixture_path = phase_dir / fixture_source.name
            fixture_path.write_text(fixture_source.read_text(encoding="utf-8"), encoding="utf-8")
            expected_path = expected_dir / expected_source.name
            ledger_path = expected_dir / ledger_source.name
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")
            return validator.validate_expected_file(expected_path, strict=True)

    def validate_mutated_real_case(self, mutate) -> list[str]:
        return self.validate_mutated_case(
            REAL_EXPECTED_PATH,
            REAL_LEDGER_PATH,
            REAL_FIXTURE_PATH,
            mutate,
        )

    def test_all_checked_in_expected_ledgers_validate_strict(self) -> None:
        expected_paths = sorted((CAD_CORE_ROOT / "fixtures").glob("*/expected/*.freecad.json"))

        self.assertGreater(len(expected_paths), 0)
        for expected_path in expected_paths:
            with self.subTest(expected=expected_path):
                self.assertEqual([], validator.validate_expected_file(expected_path, strict=True))

    def test_public_topo_object_must_match_ledger_evidence(self) -> None:
        def replace_body_projection(expected, _ledger) -> None:
            expected["topoNamingState"]["objects"]["Body"] = {
                "forged": "not-backed-by-ledger-evidence"
            }

        errors = self.validate_mutated_real_case(replace_body_projection)

        self.assertIn("public topoNamingState object does not match ledger evidence: Body", errors)

    def test_public_topo_object_hash_must_match_fixture_object(self) -> None:
        def corrupt_object_hash(expected, _ledger) -> None:
            expected["topoNamingState"]["objects"]["Body"]["objectHash"] = "sha256:forged"

        errors = self.validate_mutated_real_case(corrupt_object_hash)

        self.assertIn("public topoNamingState objectHash mismatch: Body", errors)

    def test_public_subshape_identity_must_match_ledger_element_map(self) -> None:
        def corrupt_raw_mapped_name(expected, _ledger) -> None:
            expected["topoNamingState"]["objects"]["Body"]["subshapes"]["Edge1"][
                "rawFreecadMappedName"
            ] = "forged-raw-mapped-name"

        errors = self.validate_mutated_real_case(corrupt_raw_mapped_name)

        self.assertIn("public subshape identity does not match ledger evidence: Body.Edge1", errors)

    def test_published_projection_must_use_same_ledger_object(self) -> None:
        def point_body_at_pad(_expected, ledger) -> None:
            projection = ledger["projection"]["publishedObjects"]["Body"]
            projection["ledgerObject"] = "Pad"
            projection["covers"] = ["Pad"]

        errors = self.validate_mutated_real_case(point_body_at_pad)

        self.assertIn("projection for Body must use ledger object Body", errors)

    def test_event_input_reference_ids_must_be_declared(self) -> None:
        def add_undeclared_reference(_expected, ledger) -> None:
            ledger["events"][0]["inputReferenceIds"].append("ref:undeclared")

        errors = self.validate_mutated_real_case(add_undeclared_reference)

        self.assertIn("events reference undeclared inputReferences: ['ref:undeclared']", errors)

    def test_event_input_reference_ids_reject_non_strings(self) -> None:
        def add_non_string_reference(_expected, ledger) -> None:
            ledger["events"][0]["inputReferenceIds"].append(42)

        errors = self.validate_mutated_real_case(add_non_string_reference)

        self.assertIn(
            "event:1: inputReferenceIds must contain unique non-empty strings",
            errors,
        )

    def test_event_must_match_its_specific_input_reference_element(self) -> None:
        def swap_event_reference_ids(_expected, ledger) -> None:
            first_ref = ledger["events"][0]["inputReferenceIds"][0]
            second_ref = ledger["events"][1]["inputReferenceIds"][0]
            ledger["events"][0]["inputReferenceIds"] = [second_ref]
            ledger["events"][1]["inputReferenceIds"] = [first_ref]

        errors = self.validate_mutated_case(
            SHADOW_EXPECTED_PATH,
            SHADOW_LEDGER_PATH,
            SHADOW_FIXTURE_PATH,
            swap_event_reference_ids,
        )

        self.assertIn(
            "event:1: event endpoints do not match inputReference ref:2 element evidence",
            errors,
        )

    def test_fixture_metadata_must_match_expected_path_and_input(self) -> None:
        def corrupt_fixture_binding(_expected, ledger) -> None:
            ledger["fixture"]["phase"] = "wrong-phase"
            ledger["fixture"]["case"] = "wrong-case"
            ledger["fixture"]["inputHash"] = "sha256:not-the-fixture"

        errors = self.validate_mutated_real_case(corrupt_fixture_binding)

        self.assertIn("fixture.phase mismatch: expected c4m6, got wrong-phase", errors)
        self.assertIn(
            "fixture.case mismatch: expected topo-state-body-tip-stable-recovery, got wrong-case",
            errors,
        )
        self.assertIn(f"fixture.inputHash mismatch: {REAL_FIXTURE_PATH.name}", errors)

    def test_accepted_ledger_cannot_bind_stale_input_topo_state(self) -> None:
        expected = validator.load_json(RECOVERABLE_EXPECTED_PATH)
        ledger = validator.load_json(RECOVERABLE_LEDGER_PATH)
        fixture = validator.load_json(
            RECOVERABLE_EXPECTED_PATH.parent.parent
            / "sketch-external-internal-edge-stable-recover.json"
        )
        fixture["topoNamingState"]["documentHash"] = "sha256:stale-document"
        ledger["fixture"]["inputHash"] = validator.sha256_json(fixture)

        with tempfile.TemporaryDirectory() as temp_dir:
            phase_dir = Path(temp_dir) / "p5"
            expected_dir = phase_dir / "expected"
            expected_dir.mkdir(parents=True)
            expected_path = expected_dir / RECOVERABLE_EXPECTED_PATH.name
            fixture_path = phase_dir / "sketch-external-internal-edge-stable-recover.json"
            expected_path.write_text(validator.json.dumps(expected), encoding="utf-8")
            fixture_path.write_text(validator.json.dumps(fixture), encoding="utf-8")
            expected_path.with_name(RECOVERABLE_LEDGER_PATH.name).write_text(
                validator.json.dumps(ledger),
                encoding="utf-8",
            )
            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertIn(
            "accepted ledger binds invalid input topoNamingState: topo_state_document_hash_mismatch",
            errors,
        )

    def test_input_reference_ids_must_be_unique(self) -> None:
        def duplicate_reference(_expected, ledger) -> None:
            ledger["inputReferences"].append(dict(ledger["inputReferences"][0]))

        errors = self.validate_mutated_real_case(duplicate_reference)

        self.assertIn("duplicate inputReference id: ref:1", errors)

    def test_input_references_must_match_bound_fixture(self) -> None:
        def erase_reference_graph(_expected, ledger) -> None:
            ledger["inputReferences"] = []
            for event in ledger["events"]:
                event["inputReferenceIds"] = []
            ledger["coverage"] = {
                "coveredInputReferenceIds": [],
                "uncoveredInputReferenceIds": [],
            }
            ledger["roundTrip"]["results"] = []

        errors = self.validate_mutated_real_case(erase_reference_graph)

        self.assertIn("ledger.inputReferences do not match bound fixture references", errors)

    def test_coverage_must_match_terminal_event_references_exactly(self) -> None:
        def add_undeclared_coverage(_expected, ledger) -> None:
            ledger["coverage"]["coveredInputReferenceIds"].append("ref:undeclared")

        errors = self.validate_mutated_real_case(add_undeclared_coverage)

        self.assertIn("coverage.coveredInputReferenceIds must match terminal event coverage", errors)

    def test_coverage_reference_ids_reject_duplicates(self) -> None:
        def duplicate_coverage_reference(_expected, ledger) -> None:
            ledger["coverage"]["coveredInputReferenceIds"].append("ref:1")

        errors = self.validate_mutated_real_case(duplicate_coverage_reference)

        self.assertIn(
            "coverage.coveredInputReferenceIds must contain unique non-empty strings",
            errors,
        )

    def test_projection_source_event_ids_must_exist(self) -> None:
        def add_missing_source_event(_expected, ledger) -> None:
            ledger["projection"]["publishedObjects"]["Body"]["sourceEventIds"] = [
                "event:missing"
            ]

        errors = self.validate_mutated_real_case(add_missing_source_event)

        self.assertIn("projection for Body references missing source events: ['event:missing']", errors)

    def test_projection_source_event_ids_reject_non_strings(self) -> None:
        def add_non_string_source_event(_expected, ledger) -> None:
            ledger["projection"]["publishedObjects"]["Body"]["sourceEventIds"].append(42)

        errors = self.validate_mutated_real_case(add_non_string_source_event)

        self.assertIn(
            "projection for Body sourceEventIds must contain unique non-empty strings",
            errors,
        )

    def test_projection_published_objects_must_match_public_objects_exactly(self) -> None:
        def add_extra_published_object(_expected, ledger) -> None:
            ledger["projection"]["publishedObjects"]["Ghost"] = {
                "ledgerObject": "Body",
                "covers": ["Body"],
                "sourceEventIds": [],
            }

        errors = self.validate_mutated_real_case(add_extra_published_object)

        self.assertIn(
            "projection.publishedObjects must exactly match public topoNamingState objects",
            errors,
        )

    def test_relevant_unpublished_object_requires_dropped_projection_evidence(self) -> None:
        def remove_drop_evidence(_expected, ledger) -> None:
            ledger["projection"]["droppedObjects"].pop("CompoundLink")
            ledger["objects"]["CompoundLink"]["published"] = True

        errors = self.validate_mutated_case(
            LINK_EXPECTED_PATH,
            LINK_LEDGER_PATH,
            LINK_FIXTURE_PATH,
            remove_drop_evidence,
        )

        self.assertIn(
            "relevant object is not published and has no droppedObjects explanation: CompoundLink",
            errors,
        )

    def test_dropped_object_must_be_covered_by_published_object(self) -> None:
        def point_drop_at_missing_owner(_expected, ledger) -> None:
            ledger["projection"]["droppedObjects"]["CompoundLink"]["coveredBy"] = "Ghost"

        errors = self.validate_mutated_case(
            LINK_EXPECTED_PATH,
            LINK_LEDGER_PATH,
            LINK_FIXTURE_PATH,
            point_drop_at_missing_owner,
        )

        self.assertIn("dropped object CompoundLink is covered by unpublished object: Ghost", errors)

    def test_round_trip_results_must_match_required_references_exactly(self) -> None:
        def add_extra_round_trip_result(_expected, ledger) -> None:
            ledger["roundTrip"]["results"].append(
                {
                    "inputReferenceId": "ref:undeclared",
                    "status": "resolved",
                }
            )

        errors = self.validate_mutated_real_case(add_extra_round_trip_result)

        self.assertIn("roundTrip.results must exactly cover required inputReferences", errors)

    def test_passed_round_trip_requires_resolved_reference_results(self) -> None:
        def mark_reference_failed(_expected, ledger) -> None:
            ledger["roundTrip"]["results"][0]["status"] = "failed"

        errors = self.validate_mutated_real_case(mark_reference_failed)

        self.assertIn("roundTrip passed with unresolved inputReferences: ['ref:1']", errors)

    def test_round_trip_results_reject_malformed_entries(self) -> None:
        def add_malformed_result(_expected, ledger) -> None:
            ledger["roundTrip"]["results"].append(42)

        errors = self.validate_mutated_real_case(add_malformed_result)

        self.assertIn(
            "roundTrip.results entries must contain non-empty inputReferenceId and status strings",
            errors,
        )

    def test_ledger_comparison_ignores_bound_hashes(self) -> None:
        generated = copy.deepcopy(validator.load_json(REAL_LEDGER_PATH))
        generated["fixture"]["expectedPayloadHash"] = "sha256:different-expected"
        generated["fixture"]["topoNamingStateHash"] = "sha256:different-topo-state"
        generated["roundTrip"]["inputTopoNamingStateHash"] = "sha256:different-round-trip"

        self.assertTrue(collector.compare_ledger_json(REAL_LEDGER_PATH, generated))

    def test_ledger_comparison_rejects_runtime_version_drift(self) -> None:
        generated = copy.deepcopy(validator.load_json(REAL_LEDGER_PATH))
        generated["producer"]["freecadVersion"] = "different-freecad"

        with contextlib.redirect_stderr(io.StringIO()):
            self.assertFalse(collector.compare_ledger_json(REAL_LEDGER_PATH, generated))

    def test_ledger_comparison_treats_element_inventory_as_unordered(self) -> None:
        generated = copy.deepcopy(validator.load_json(REAL_LEDGER_PATH))
        generated["objects"]["Body"]["afterElements"]["Edge"].reverse()

        self.assertTrue(collector.compare_ledger_json(REAL_LEDGER_PATH, generated))

    def test_ledger_comparison_treats_events_as_id_addressed(self) -> None:
        generated = copy.deepcopy(validator.load_json(REAL_LEDGER_PATH))
        generated["events"].reverse()

        self.assertTrue(collector.compare_ledger_json(REAL_LEDGER_PATH, generated))

    def test_ledger_comparison_rejects_semantic_drift(self) -> None:
        generated = copy.deepcopy(validator.load_json(REAL_LEDGER_PATH))
        generated["coverage"]["coveredInputReferenceIds"] = []

        with contextlib.redirect_stderr(io.StringIO()):
            self.assertFalse(collector.compare_ledger_json(REAL_LEDGER_PATH, generated))

    def test_check_ledger_cli_option_is_available(self) -> None:
        args = collector.parse_args(["--phase", "c4m6", "--check", "--check-ledger"])

        self.assertTrue(args.check)
        self.assertTrue(args.check_ledger)

    def test_generated_ledger_validation_does_not_reuse_checked_in_sidecar(self) -> None:
        expected = validator.load_json(REAL_EXPECTED_PATH)
        generated_ledger = validator.load_json(REAL_LEDGER_PATH)
        generated_ledger["roundTrip"]["status"] = "failed"

        errors = collector.validate_generated_expected_ledger(
            REAL_FIXTURE_PATH,
            validator.load_json(REAL_FIXTURE_PATH),
            expected,
            generated_ledger,
        )

        self.assertIn("roundTrip.status must be passed", errors)

    def test_accepted_ledger_requires_matching_published_capture(self) -> None:
        expected = {
            "topoNamingState": {
                "schemaVersion": "cad-core.topo-state.v1",
                "objects": {},
            }
        }
        mismatched_capture = {
            "rawTopoNamingState": expected["topoNamingState"],
            "publishedTopoNamingState": {
                "schemaVersion": "cad-core.topo-state.v1",
                "objects": {"Forged": {}},
            },
            "resolvedReferenceBindings": [],
        }

        with self.assertRaisesRegex(
            collector.UnsupportedFixture,
            "FreeCAD ledger capture does not match public topoNamingState",
        ):
            collector.build_freecad_expected_ledger(
                Path("fixtures/c4m6/case.json"),
                {"Objects": []},
                expected,
                freecad_version_value="1.0",
                occt_version_value="7.8",
                freecad_capture=mismatched_capture,
            )

    def test_rejected_outcome_cannot_bypass_published_topo_closure(self) -> None:
        def forge_rejection(expected, ledger) -> None:
            diagnostic = {"code": "forged_rejection", "severity": "error"}
            expected["diagnostics"] = [diagnostic]
            ledger["outcome"] = "rejected"
            ledger["diagnostics"] = [diagnostic]
            ledger["rejection"] = {
                "diagnosticCodes": ["forged_rejection"],
                "reason": "forged",
            }

        errors = self.validate_mutated_real_case(forge_rejection)

        self.assertIn("expected with topoNamingState must use accepted ledger outcome", errors)

    def test_capture_excludes_binding_until_native_assignment_succeeds(self) -> None:
        original_bindings = collector.ACTIVE_RESOLVED_REFERENCE_BINDINGS
        collector.ACTIVE_RESOLVED_REFERENCE_BINDINGS = []
        value = {
            "value": "Sketch",
            "StableSubList": ["g1;SKT;FAC"],
            "StableSubListSource": "topoNamingState",
        }
        try:
            collector.record_resolved_reference_bindings(
                value,
                ["InternalFace1"],
                owner="Pad",
                property_path="Profile.SubSet.0",
                native_resolution_evidence=["FreeCAD.getSubObject"],
            )
            pending_capture = collector.freecad_ledger_capture({}, {}, collector.ACTIVE_RESOLVED_REFERENCE_BINDINGS)

            collector.mark_resolved_reference_bindings_succeeded(
                "Pad",
                "Profile",
            )
            succeeded_capture = collector.freecad_ledger_capture({}, {}, collector.ACTIVE_RESOLVED_REFERENCE_BINDINGS)
        finally:
            collector.ACTIVE_RESOLVED_REFERENCE_BINDINGS = original_bindings

        self.assertEqual([], pending_capture["resolvedReferenceBindings"])
        self.assertEqual(
            "succeeded",
            succeeded_capture["resolvedReferenceBindings"][0]["assignmentStatus"],
        )

    def test_round_trip_rejects_succeeded_binding_to_absent_raw_subshape(self) -> None:
        replay_payload = validator.load_json(REAL_EXPECTED_PATH)
        input_references = validator.load_json(REAL_LEDGER_PATH)["inputReferences"]
        replay_capture = self.synthetic_capture_for_references(
            replay_payload,
            input_references,
        )
        binding = replay_capture["resolvedReferenceBindings"][0]
        binding["resolvedSubname"] = "Face999"
        binding["nativeResolvedSubname"] = "Face999"

        results, diagnostics = collector.round_trip_reference_results(
            replay_payload,
            replay_capture,
            input_references,
        )

        self.assertEqual([{"inputReferenceId": "ref:1", "status": "failed"}], results)
        self.assertEqual("round_trip_reference_not_resolved", diagnostics[0]["code"])

    def test_unmapped_subshape_evidence_must_come_from_raw_capture(self) -> None:
        published_subshape = {
            "subname": "Edge1",
            "identityStatus": "stable",
            "resolvedIndexed": "Edge1",
            "rawFreecadMappedName": "forged-raw-name",
            "canonicalFreecadMappedName": "forged-canonical-name",
        }
        empty_object = {
            "objectHash": "sha256:body",
            "elementMapVersion": "cad-core.element-map.v1",
            "subshapes": {},
            "elementMap": {"encoding": "cad-core.element-map.v1", "entries": {}},
            "childElementMaps": [],
            "mapperHistory": [],
        }
        raw_topo_state = {"objects": {"Body": copy.deepcopy(empty_object)}}
        published_object = copy.deepcopy(empty_object)
        published_object["subshapes"] = {"Edge1": published_subshape}
        published_topo_state = {"objects": {"Body": published_object}}

        objects = collector.ledger_objects_from_states(
            {"Objects": [{"Name": "Body", "TypeId": "PartDesign::Body"}]},
            raw_topo_state,
            published_topo_state,
            [],
        )

        self.assertNotIn("Edge1", objects["Body"].get("subshapeEvidence", {}))

    def test_round_trip_results_require_native_replay_bindings(self) -> None:
        replay_payload = validator.load_json(REAL_EXPECTED_PATH)
        input_references = validator.load_json(REAL_LEDGER_PATH)["inputReferences"]
        replay_capture = self.synthetic_capture_for_references(
            replay_payload,
            input_references,
        )

        results, diagnostics = collector.round_trip_reference_results(
            replay_payload,
            replay_capture,
            input_references,
        )

        self.assertEqual(
            [
                {
                    "inputReferenceId": "ref:1",
                    "status": "resolved",
                }
            ],
            results,
        )
        self.assertEqual([], diagnostics)

    def test_round_trip_marks_missing_stable_token_failed(self) -> None:
        replay_payload = validator.load_json(REAL_EXPECTED_PATH)
        input_references = copy.deepcopy(
            validator.load_json(REAL_LEDGER_PATH)["inputReferences"]
        )
        input_references[0]["stableSubname"] = "missing-stable-token"
        input_references[0]["element"] = "missing-stable-token"
        replay_capture = self.synthetic_capture_for_references(
            replay_payload,
            input_references,
        )

        results, diagnostics = collector.round_trip_reference_results(
            replay_payload,
            replay_capture,
            input_references,
        )

        self.assertEqual(
            [{"inputReferenceId": "ref:1", "status": "failed"}],
            results,
        )
        self.assertEqual("round_trip_reference_not_resolved", diagnostics[0]["code"])

    def test_round_trip_accepts_mapper_history_recoverable_reference(self) -> None:
        replay_payload = validator.load_json(RECOVERABLE_EXPECTED_PATH)
        input_references = validator.load_json(RECOVERABLE_LEDGER_PATH)["inputReferences"]
        replay_capture = self.synthetic_capture_for_references(
            replay_payload,
            input_references,
        )

        results, diagnostics = collector.round_trip_reference_results(
            replay_payload,
            replay_capture,
            input_references,
        )

        self.assertTrue(all(item["status"] == "resolved" for item in results))
        self.assertEqual([], diagnostics)

    def test_round_trip_rejects_echoed_token_without_native_binding(self) -> None:
        replay_payload = validator.load_json(REAL_EXPECTED_PATH)
        input_references = validator.load_json(REAL_LEDGER_PATH)["inputReferences"]
        replay_capture = {
            "rawTopoNamingState": replay_payload["topoNamingState"],
            "publishedTopoNamingState": replay_payload["topoNamingState"],
            "resolvedReferenceBindings": [],
        }

        results, diagnostics = collector.round_trip_reference_results(
            replay_payload,
            replay_capture,
            input_references,
        )

        self.assertEqual([{"inputReferenceId": "ref:1", "status": "failed"}], results)
        self.assertEqual("round_trip_reference_not_resolved", diagnostics[0]["code"])

    def test_sidecar_ledger_validates_expected_projection(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {
                    "Body": {
                        "objectHash": "sha256:body",
                        "elementMapVersion": "cad-core.element-map.v1",
                        "subshapes": {
                            "Face5": {"subname": "Face5", "identityStatus": "current_only"},
                            "Face6": {"subname": "Face6", "identityStatus": "current_only"},
                        },
                        "elementMap": {},
                        "childElementMaps": [],
                        "mapperHistory": [],
                    }
                }
            }
        }
        topo_hash = validator.sha256_json(expected["topoNamingState"])
        expected_hash = validator.sha256_json(expected)
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": expected_hash,
                "topoNamingStateHash": topo_hash,
            },
            "inputReferences": [
                {
                    "id": "ref:1",
                    "owner": "Body",
                    "path": ["Body", "Pad"],
                    "target": "Pad",
                    "element": "Face3",
                    "stableSubname": "Face3",
                    "source": "StableSubList",
                    "required": True,
                }
            ],
            "objects": {
                "Body": {
                    "published": True,
                    "beforeElements": {
                        "Face": ["Face3"]
                    },
                    "afterElements": {
                        "Face": ["Face5", "Face6"]
                    },
                },
                "Pad": {
                    "published": False,
                    "beforeElements": {
                        "Face": ["Face3"]
                    },
                    "afterElements": {
                        "Face": ["Face5", "Face6"]
                    },
                },
            },
            "events": [
                {
                    "id": "event:1",
                    "kind": "split",
                    "sources": [
                        {
                            "object": "Pad",
                            "element": "Face3",
                        }
                    ],
                    "targets": [
                        {
                            "object": "Pad",
                            "element": "Face5",
                        },
                        {
                            "object": "Pad",
                            "element": "Face6",
                        },
                    ],
                    "inputReferenceIds": ["ref:1"],
                }
            ],
            "projection": {
                "publishedObjects": {
                    "Body": {
                        "ledgerObject": "Body",
                        "covers": ["Body", "Pad"],
                    }
                },
                "droppedObjects": {
                    "Pad": {
                        "reason": "covered_by_body_tip",
                        "coveredBy": "Body",
                        "sourceEventIds": ["event:1"],
                    }
                },
            },
            "coverage": {
                "coveredInputReferenceIds": ["ref:1"],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": topo_hash,
                "results": [
                    {
                        "inputReferenceId": "ref:1",
                        "status": "resolved",
                    }
                ],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            self.assertEqual([], validator.validate_expected_file(expected_path, strict=True))

    def test_missing_sidecar_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            expected_path.write_text("{}", encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertEqual([f"missing ledger sidecar: {expected_path.with_name('case.freecad.ledger.json')}"], errors)

    def test_expected_hash_mismatch_is_error(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {}
            }
        }
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": "sha256:not-the-current-payload",
                "topoNamingStateHash": validator.sha256_json(expected["topoNamingState"]),
            },
            "inputReferences": [],
            "objects": {},
            "events": [],
            "projection": {
                "publishedObjects": {},
                "droppedObjects": {},
            },
            "coverage": {
                "coveredInputReferenceIds": [],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": validator.sha256_json(expected["topoNamingState"]),
                "results": [],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertIn(f"expectedPayloadHash mismatch: {expected_path}", errors)

    def test_every_input_reference_must_have_event_conclusion(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {
                    "Body": {}
                }
            }
        }
        topo_hash = validator.sha256_json(expected["topoNamingState"])
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": validator.sha256_json(expected),
                "topoNamingStateHash": topo_hash,
            },
            "inputReferences": [
                {
                    "id": "ref:optional",
                    "owner": "Body",
                    "path": ["Body"],
                    "element": "Face3",
                    "required": False,
                }
            ],
            "objects": {
                "Body": {
                    "published": True,
                }
            },
            "events": [],
            "projection": {
                "publishedObjects": {
                    "Body": {
                        "ledgerObject": "Body",
                        "covers": ["Body"],
                    }
                },
                "droppedObjects": {},
            },
            "coverage": {
                "coveredInputReferenceIds": [],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": topo_hash,
                "results": [],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertIn("inputReferences not covered by terminal events: ['ref:optional']", errors)

    def test_collector_builds_accepted_sidecar_ledger(self) -> None:
        fixture = {
            "Objects": [
                {
                    "Name": "Sketch",
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {},
                },
                {
                    "Name": "Pad",
                    "TypeId": "PartDesign::Pad",
                    "Properties": {
                        "Profile": {
                            "PropertyType": "App::PropertyLinkSubList",
                            "SubSet": [
                                {
                                    "value": "Sketch",
                                    "StableSubList": ["g1;SKT;FAC"],
                                    "StableSubListSource": "topoNamingState",
                                }
                            ],
                        }
                    },
                },
            ],
            "recompute": {
                "objs": ["Sketch"]
            },
            "topoNamingState": {
                "schemaVersion": "cad-core.topo-state.v1",
                "producer": {
                    "cadCoreVersion": "fixture-contract-v1",
                },
                "objects": {
                    "Sketch": {
                        "objectHash": "sha256:sketch",
                        "elementMapVersion": "cad-core.element-map.v1",
                        "subshapes": {
                            "InternalFace1": {
                                "subname": "InternalFace1",
                                "identityStatus": "current_only",
                            }
                        },
                        "elementMap": {
                            "entries": {
                                "g1;SKT;FAC": {
                                    "source": {
                                        "object": "Sketch",
                                        "subname": "InternalFace1",
                                    },
                                    "target": {
                                        "object": "Sketch",
                                        "subname": "InternalFace1",
                                    },
                                    "recoverability": "resolved",
                                    "evidence": {
                                        "source": "element_map"
                                    },
                                }
                            }
                        },
                        "childElementMaps": [],
                        "mapperHistory": [],
                    }
                },
            },
        }
        expected = {
            "diagnostics": [],
            "elementReferenceUpdates": [],
            "results": [],
            "topoNamingState": {
                "schemaVersion": "cad-core.topo-state.v1",
                "objects": fixture["topoNamingState"]["objects"],
            },
        }
        topo_hash = collector.semantic_hash(expected["topoNamingState"])
        freecad_capture = {
            "rawTopoNamingState": copy.deepcopy(expected["topoNamingState"]),
            "publishedTopoNamingState": copy.deepcopy(expected["topoNamingState"]),
            "resolvedReferenceBindings": [
                {
                    "owner": "Pad",
                    "propertyPath": "Profile.SubSet.0",
                    "target": "Sketch",
                    "stableSubname": "g1;SKT;FAC",
                    "resolvedObject": "Sketch",
                    "resolvedSubname": "InternalFace1",
                    "nativeResolvedSubname": "InternalFace1",
                    "nativeResolutionEvidence": "FreeCAD.getSubObject",
                    "assignmentStatus": "succeeded",
                }
            ],
        }
        ledger = collector.build_freecad_expected_ledger(
            Path("fixtures/c4m6/case.json"),
            fixture,
            expected,
            freecad_version_value="1.0",
            occt_version_value="7.8",
            freecad_capture=freecad_capture,
            round_trip={
                "status": "passed",
                "inputTopoNamingStateHash": topo_hash,
                "results": [
                    {
                        "inputReferenceId": "ref:1",
                        "status": "resolved",
                    }
                ],
            },
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertEqual([], errors)
        self.assertEqual("accepted", ledger["outcome"])
        self.assertEqual(["ref:1"], ledger["coverage"]["coveredInputReferenceIds"])

    def test_rejected_sidecar_ledger_validates_without_topo_state(self) -> None:
        expected = {
            "diagnostics": [
                {
                    "code": "topo_state_schema_incompatible",
                    "severity": "error",
                }
            ],
            "elementReferenceUpdates": [],
            "results": [],
        }
        ledger = collector.build_freecad_expected_ledger(
            Path("fixtures/c4m6/schema-case.json"),
            {
                "Objects": [],
                "topoNamingState": {
                    "schemaVersion": "cad-core.topo-state.v0"
                },
            },
            expected,
            freecad_version_value="1.0",
            occt_version_value="7.8",
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "schema-case.freecad.json"
            ledger_path = Path(temp_dir) / "schema-case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertEqual([], errors)
        self.assertEqual("rejected", ledger["outcome"])


if __name__ == "__main__":
    unittest.main()
