"""Focused tests for the native FreeCAD fixture regression gate."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from tests.producer_trace_fixture import bind_trace, producer_trace


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected_regression_gate", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


class NativeFixtureManifestTests(unittest.TestCase):
    def test_authority_root_uses_role_catalog_instead_of_stale_native_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            for case in ("registered", "stale"):
                (phase / f"{case}.json").write_text("{}\n", encoding="utf-8")
                (expected / f"{case}.freecad.json").write_text("{}\n", encoding="utf-8")
                (expected / f"{case}.freecad.ledger.json").write_text("{}\n", encoding="utf-8")
            catalog = SimpleNamespace(
                cases=[
                    SimpleNamespace(
                        phase="p1",
                        case="registered",
                        input_path=phase / "registered.json",
                        expected_path=expected / "registered.freecad.json",
                        ledger_path=expected / "registered.freecad.ledger.json",
                    )
                ],
                errors=[],
            )

            with (
                mock.patch.object(collector, "is_authority_fixtures_root", return_value=True),
                mock.patch.object(collector, "load_native_role_catalog", return_value=catalog),
            ):
                manifest = collector.native_expected_manifest(fixtures)

            self.assertEqual([("p1", "registered")], [(item.phase, item.case) for item in manifest])

    def test_discovers_only_complete_checked_in_native_cases(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            (phase / "native.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.ledger.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.producer-trace.json").write_text("{}\n", encoding="utf-8")
            (phase / "protocol.json").write_text("{}\n", encoding="utf-8")
            (expected / "protocol.expeted.json").write_text("{}\n", encoding="utf-8")

            manifest = collector.native_expected_manifest(fixtures)

            self.assertEqual([(entry.phase, entry.case) for entry in manifest], [("p1", "native")])
            self.assertEqual(manifest[0].fixture_path, phase / "native.json")
            self.assertEqual(manifest[0].expected_path, expected / "native.freecad.json")

    def test_discovers_public_ledger_authority_without_producer_trace(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            (phase / "native.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.ledger.json").write_text("{}\n", encoding="utf-8")

            manifest = collector.native_expected_manifest(fixtures)

            self.assertEqual([(entry.phase, entry.case) for entry in manifest], [("p1", "native")])
            self.assertIsNone(manifest[0].producer_trace_path)

    def test_missing_native_companion_is_a_hard_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            (phase / "broken.json").write_text("{}\n", encoding="utf-8")
            (expected / "broken.freecad.json").write_text("{}\n", encoding="utf-8")
            (expected / "broken.freecad.producer-trace.json").write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "broken.freecad.ledger.json"):
                collector.native_expected_manifest(fixtures)

    def test_manifest_report_excludes_optional_trace_from_authority_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            (phase / "native.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.json").write_text("{}\n", encoding="utf-8")
            (expected / "native.freecad.ledger.json").write_text("{}\n", encoding="utf-8")

            without_trace = collector.native_manifest_report(
                collector.native_expected_manifest(fixtures)
            )
            (expected / "native.freecad.producer-trace.json").write_text(
                '{"diagnostic": true}\n', encoding="utf-8"
            )
            with_trace = collector.native_manifest_report(
                collector.native_expected_manifest(fixtures)
            )

            self.assertIsNone(without_trace["entries"][0]["producerTraceSha256"])
            self.assertIsNotNone(with_trace["entries"][0]["producerTraceSha256"])
            self.assertEqual(without_trace["sha256"], with_trace["sha256"])


class RegressionGateCliTests(unittest.TestCase):
    def test_accepts_all_native_repeated_read_only_gate(self) -> None:
        args = collector.parse_args(
            [
                "--all-native",
                "--check",
                "--repeat",
                "2",
                "--candidate-root",
                "/tmp/candidates",
                "--report",
                "/tmp/report.json",
            ]
        )

        self.assertTrue(args.all_native)
        self.assertTrue(args.check)
        self.assertEqual(args.repeat, 2)
        self.assertEqual(args.candidate_root, "/tmp/candidates")
        self.assertEqual(args.report, "/tmp/report.json")

    def test_all_native_requires_check(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                collector.parse_args(["--all-native"])

    def test_repeated_check_requires_candidate_root(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                collector.parse_args(["--all-native", "--check", "--repeat", "2"])

    def test_accepts_single_case_repeated_read_only_gate(self) -> None:
        args = collector.parse_args(
            [
                "/tmp/fixtures/p1/case.json",
                "--fixtures-root",
                "/tmp/fixtures",
                "--check",
                "--repeat",
                "2",
                "--candidate-root",
                "/tmp/candidates",
                "--report",
                "/tmp/report.json",
            ]
        )

        child = collector.regression_child_argv(
            args,
            candidate_root=Path("/tmp/candidates/run-a"),
            report=Path("/tmp/candidates/run-a.report.json"),
        )

        self.assertEqual("/tmp/fixtures/p1/case.json", args.fixture)
        self.assertIn("/tmp/fixtures/p1/case.json", child)
        self.assertNotIn("--all-native", child)

    def test_explicit_phase_without_native_authority_fails_closed(self) -> None:
        args = collector.parse_args(
            [
                "--phase",
                "p1",
                "--check",
                "--fixtures-root",
                "/tmp/fixtures",
            ]
        )
        catalog = SimpleNamespace(cases=[], skipped=[], errors=[])

        with mock.patch.object(collector, "fixture_role_catalog", return_value=catalog):
            with self.assertRaisesRegex(ValueError, "no native authority selected"):
                collector.fixture_paths(args)

    def test_repeat_gate_rejects_child_report_without_split_verdicts(self) -> None:
        run = {
            "returncode": 0,
            "report": {
                "status": "passed",
                "discovered": 1,
                "processed": 1,
                "skipped": 0,
                "failed": 0,
            },
        }

        self.assertFalse(
            collector.repeated_run_passed(
                run,
                expected_cases=1,
                producer_trace_requested=False,
            )
        )

    def test_missing_optional_provenance_does_not_block_repeat(self) -> None:
        run = {
            "returncode": 0,
            "report": {
                "status": "passed",
                "publicExpectedStatus": "passed",
                "ledgerValidationStatus": "passed",
                "ledgerDriftStatus": "unchanged",
                "producerTraceStatus": "not_evaluated",
                "discovered": 1,
                "processed": 1,
                "skipped": 0,
                "failed": 0,
            },
        }

        self.assertTrue(
            collector.repeated_run_passed(
                run,
                expected_cases=1,
                producer_trace_requested=False,
            )
        )

    def test_complete_collection_provenance_has_no_warnings(self) -> None:
        report = {
            "candidate": {
                "path": "/tmp/FreeCADCmd",
                "exists": True,
                "sha256": "sha256:binary",
                "sourceRoot": "/tmp/source",
                "commit": "abc123",
                "dirty": False,
                "dirtyEntries": 0,
                "dirtyStatusSha256": "sha256:clean",
                "build": {
                    "directory": "/tmp/build",
                    "cmakeCache": "/tmp/build/CMakeCache.txt",
                    "cmakeCacheSha256": "sha256:cache",
                    "homeDirectory": "/tmp/source",
                    "buildType": "RelWithDebInfo",
                    "generator": "Ninja",
                },
            },
            "collector": {"path": "/tmp/collector.py", "sha256": "sha256:collector"},
            "collectorInvocation": {
                "argv": ["/tmp/FreeCADCmd", "/tmp/collector.py", "--pass", "marker"],
                "cwd": "/tmp",
                "environment": {collector.ENV_ARG_NAME: "[]"},
            },
            "runtime": {"freecadVersion": "1.0", "occtVersion": "7.9.3"},
            "candidateRoot": "/tmp/candidates",
            "cases": [
                {
                    "fixtureSha256": "sha256:fixture",
                    "publicExpectedSha256": "sha256:public",
                    "ledgerSha256": "sha256:ledger",
                    "artifacts": {
                        "candidateWrite": {
                            "status": "written",
                            "publicSha256": "sha256:candidate-public",
                            "ledgerSha256": "sha256:candidate-ledger",
                        }
                    },
                }
            ],
        }

        self.assertEqual([], collector.provenance_warnings(report))


class NativeProducerTraceComparisonTests(unittest.TestCase):
    def test_compares_new_trace_against_checked_in_native_trace(self) -> None:
        request = {
            "Objects": [
                {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}},
                {
                    "Name": "UntracedHelper",
                    "TypeId": "App::FeaturePython",
                    "Properties": {},
                },
            ]
        }
        response = {"object": "Pad"}
        expected = bind_trace(producer_trace(), request, response)
        equal = bind_trace(producer_trace(), request, response)
        different = bind_trace(producer_trace(field_value="different"), request, response)

        equal_result = collector.compare_native_producer_traces(
            expected,
            equal,
            fixture=request,
            expected_response=response,
            actual_response=response,
        )
        different_result = collector.compare_native_producer_traces(
            expected,
            different,
            fixture=request,
            expected_response=response,
            actual_response=response,
        )

        self.assertEqual(equal_result.status, "equal")
        self.assertEqual(different_result.status, "different")


class CandidateArtifactTests(unittest.TestCase):
    def test_rejects_candidate_root_inside_checked_in_fixtures(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures = Path(temp_dir) / "fixtures"
            fixtures.mkdir()

            with self.assertRaisesRegex(ValueError, "candidate root must be outside"):
                collector.validate_regression_output_path(
                    fixtures / "candidate",
                    fixtures_root=fixtures,
                    label="candidate root",
                )

    def test_writes_candidate_with_optional_trace_below_mirrored_phase_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            authority = fixtures / "p1" / "expected" / "case.freecad.json"
            candidate_root = root / "run-a"
            authority.parent.mkdir(parents=True)

            candidate = collector.write_candidate_artifacts(
                candidate_root,
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected={"value": "public"},
                ledger={"value": "ledger"},
                producer_trace={"value": "trace"},
            )

            self.assertEqual(candidate, candidate_root / "p1" / "expected" / "case.freecad.json")
            self.assertEqual(
                json.loads(candidate.read_text(encoding="utf-8")),
                {"value": "public"},
            )
            self.assertTrue(collector.ledger_path_for_expected(candidate).is_file())
            self.assertTrue(collector.producer_trace_path_for_expected(candidate).is_file())

    def test_writes_public_ledger_candidate_without_producer_trace(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            authority = fixtures / "p1" / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)

            candidate = collector.write_candidate_artifacts(
                root / "run-a",
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected={"value": "public"},
                ledger={"value": "ledger"},
                producer_trace=None,
            )

            self.assertTrue(candidate.is_file())
            self.assertTrue(collector.ledger_path_for_expected(candidate).is_file())
            self.assertFalse(collector.producer_trace_path_for_expected(candidate).exists())

    def test_compares_candidate_runs_with_the_authority_comparators(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            request = {
                "Objects": [
                    {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}
                ]
            }
            response = {"object": "Pad"}
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            manifest = collector.native_expected_manifest(fixtures)
            run_a = root / "run-a"
            run_b = root / "run-b"
            for run in (run_a, run_b):
                collector.write_candidate_artifacts(
                    run,
                    fixtures_root=fixtures,
                    expected_path=authority,
                    public_expected=response,
                    ledger={},
                    producer_trace=None,
                )

            self.assertEqual(
                collector.compare_candidate_runs(manifest, [run_a, run_b], fixtures_root=fixtures),
                [],
            )

            run_b_expected = collector.candidate_expected_path(
                run_b,
                fixtures_root=fixtures,
                expected_path=authority,
            )
            collector.atomic_write_json(run_b_expected, {"object": "Other"})
            diagnostics: list[dict] = []
            with contextlib.redirect_stderr(io.StringIO()):
                errors = collector.compare_candidate_runs(
                    manifest,
                    [run_a, run_b],
                    fixtures_root=fixtures,
                    producer_diagnostics=diagnostics,
                )
            self.assertTrue(any(error["artifact"] == "public" for error in errors))
            self.assertEqual("unavailable", diagnostics[0]["status"])

    def test_aligned_public_ledger_runs_do_not_require_or_compare_traces(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            (phase / "case.json").write_text("{}\n", encoding="utf-8")
            collector.atomic_write_json(authority, {"value": "public"})
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {"value": "ledger"})
            manifest = collector.native_expected_manifest(fixtures)
            for run in (root / "run-a", root / "run-b"):
                collector.write_candidate_artifacts(
                    run,
                    fixtures_root=fixtures,
                    expected_path=authority,
                    public_expected={"value": "public"},
                    ledger={"value": "ledger"},
                    producer_trace=None,
                )

            with mock.patch.object(collector, "compare_native_producer_traces") as compare_trace:
                differences = collector.compare_candidate_runs(
                    manifest,
                    [root / "run-a", root / "run-b"],
                    fixtures_root=fixtures,
                )

            self.assertEqual([], differences)
            compare_trace.assert_not_called()

    def test_ledger_drift_is_reported_without_failing_public_repeatability(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            (phase / "case.json").write_text("{}\n", encoding="utf-8")
            collector.atomic_write_json(authority, {"value": "public"})
            collector.atomic_write_json(
                collector.ledger_path_for_expected(authority),
                {"metadata": "authority"},
            )
            manifest = collector.native_expected_manifest(fixtures)
            collector.write_candidate_artifacts(
                root / "run-a",
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected={"value": "public"},
                ledger={"metadata": "run-a"},
                producer_trace=None,
            )
            collector.write_candidate_artifacts(
                root / "run-b",
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected={"value": "public"},
                ledger={"metadata": "run-b"},
                producer_trace=None,
            )
            ledger_drifts: list[dict] = []

            with contextlib.redirect_stderr(io.StringIO()):
                differences = collector.compare_candidate_runs(
                    manifest,
                    [root / "run-a", root / "run-b"],
                    fixtures_root=fixtures,
                    ledger_drifts=ledger_drifts,
                )

            self.assertEqual([], differences)
            self.assertEqual(["ledger"], [item["artifact"] for item in ledger_drifts])
            self.assertEqual("drifted", ledger_drifts[0]["status"])

    def test_public_mismatch_keeps_trace_difference_in_diagnostic_channel(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            request = {"Objects": [{"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}]}
            baseline_response = {"object": "Pad"}
            actual_response = {"object": "Other"}
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, baseline_response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            manifest = collector.native_expected_manifest(fixtures)
            collector.write_candidate_artifacts(
                root / "run-a",
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected=baseline_response,
                ledger={},
                producer_trace=bind_trace(producer_trace(), request, baseline_response),
            )
            collector.write_candidate_artifacts(
                root / "run-b",
                fixtures_root=fixtures,
                expected_path=authority,
                public_expected=actual_response,
                ledger={},
                producer_trace=bind_trace(producer_trace(field_value="different"), request, actual_response),
            )
            diagnostics: list[dict] = []

            differences = collector.compare_candidate_runs(
                manifest,
                [root / "run-a", root / "run-b"],
                fixtures_root=fixtures,
                producer_diagnostics=diagnostics,
            )

            self.assertEqual(["public"], [item["artifact"] for item in differences])
            self.assertEqual(1, len(diagnostics))
            self.assertEqual("producer-trace", diagnostics[0]["artifact"])
            self.assertEqual("different", diagnostics[0]["status"])


class RepeatedRegressionGateTests(unittest.TestCase):
    def test_runs_collector_twice_without_traces_and_publishes_passing_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            request = {
                "Objects": [
                    {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}
                ]
            }
            response = {"object": "Pad"}
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            candidate_root = root / "candidates"
            report = root / "report.json"
            build = root / "build"
            freecadcmd = build / "bin" / "FreeCADCmd"
            freecadcmd.parent.mkdir(parents=True)
            freecadcmd.write_bytes(b"candidate")
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={root}\n"
                "CMAKE_BUILD_TYPE:STRING=RelWithDebInfo\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n",
                encoding="utf-8",
            )
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--repeat",
                    "2",
                    "--candidate-root",
                    str(candidate_root),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(freecadcmd),
                ]
            )

            def fake_run(_argv: list[str], child_args: object) -> int:
                run_root = Path(child_args.candidate_root)
                collector.write_candidate_artifacts(
                    run_root,
                    fixtures_root=fixtures,
                    expected_path=authority,
                    public_expected=response,
                    ledger={},
                    producer_trace=None,
                )
                collector.atomic_write_json(
                    Path(child_args.report),
                    {
                        "discovered": 1,
                        "processed": 1,
                        "skipped": 0,
                        "failed": 0,
                        "status": "passed",
                        "publicExpectedStatus": "passed",
                        "ledgerValidationStatus": "passed",
                        "ledgerDriftStatus": "unchanged",
                        "producerTraceStatus": "not_evaluated",
                    },
                )
                return 0

            with mock.patch.object(collector, "run_via_freecadcmd", side_effect=fake_run) as run:
                with contextlib.redirect_stderr(io.StringIO()):
                    result = collector.run_repeated_checks(args)

            self.assertEqual(result, 0)
            self.assertEqual(run.call_count, 2)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(payload["status"], "passed")
            self.assertEqual(payload["publicExpectedStatus"], "passed")
            self.assertEqual(payload["ledgerValidationStatus"], "passed")
            self.assertEqual(payload["ledgerDriftStatus"], "unchanged")
            self.assertEqual(payload["producerTraceStatus"], "not_evaluated")
            self.assertNotIn("publicLedgerConsistency", payload)
            self.assertNotIn("producerSemanticVerdict", payload)
            self.assertEqual(payload["manifest"]["cases"], 1)
            self.assertEqual(payload["candidate"]["sha256"], collector.file_sha256(freecadcmd))
            self.assertEqual(payload["candidate"]["sourceRoot"], str(root.resolve()))
            self.assertEqual(payload["candidate"]["build"]["buildType"], "RelWithDebInfo")
            self.assertEqual(payload["candidate"]["build"]["generator"], "Ninja")
            self.assertIn("commit", payload["candidate"])
            self.assertIn("dirty", payload["candidate"])
            self.assertEqual(
                collector.file_sha256(Path(collector.__file__)),
                payload["collector"]["sha256"],
            )
            self.assertEqual(str(Path(collector.__file__).resolve()), payload["collector"]["path"])
            for run_result in payload["runs"]:
                invocation = run_result["collectorInvocation"]
                self.assertEqual(freecadcmd.resolve(), Path(invocation["argv"][0]).resolve())
                self.assertEqual(str(Path(collector.__file__).resolve()), invocation["argv"][1])
                self.assertEqual(str(Path.cwd().resolve()), invocation["cwd"])
                child_argv = json.loads(invocation["environment"][collector.ENV_ARG_NAME])
                candidate_index = child_argv.index("--candidate-root") + 1
                self.assertEqual(run_result["root"], str(Path(child_argv[candidate_index]).resolve()))
            self.assertEqual(payload["candidateRunDifferences"], [])
            self.assertEqual(payload["producerTraceDiagnostics"], [])
            self.assertIsNone(payload["firstFailure"])
            self.assertIsNone(payload["firstProducerTraceDiagnostic"])

    def test_single_case_repeat_compares_only_the_selected_native_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            selected_fixture = fixtures / "p1" / "selected.json"
            selected_authority = fixtures / "p1" / "expected" / "selected.freecad.json"
            other_fixture = fixtures / "p2" / "other.json"
            other_authority = fixtures / "p2" / "expected" / "other.freecad.json"
            for fixture, authority, object_name in (
                (selected_fixture, selected_authority, "Selected"),
                (other_fixture, other_authority, "Other"),
            ):
                authority.parent.mkdir(parents=True)
                fixture.write_text(
                    json.dumps(
                        {
                            "Objects": [
                                {
                                    "Name": object_name,
                                    "TypeId": "Part::Box",
                                    "Properties": {},
                                }
                            ]
                        }
                    ),
                    encoding="utf-8",
                )
                collector.atomic_write_json(authority, {"object": object_name})
                collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})

            candidate_root = root / "candidates"
            report = root / "report.json"
            freecadcmd = root / "FreeCADCmd"
            freecadcmd.write_bytes(b"candidate")
            args = collector.parse_args(
                [
                    str(selected_fixture),
                    "--fixtures-root",
                    str(fixtures),
                    "--check",
                    "--repeat",
                    "2",
                    "--candidate-root",
                    str(candidate_root),
                    "--report",
                    str(report),
                    "--freecadcmd",
                    str(freecadcmd),
                ]
            )

            def fake_run(_argv: list[str], child_args: object) -> int:
                collector.write_candidate_artifacts(
                    Path(child_args.candidate_root),
                    fixtures_root=fixtures,
                    expected_path=selected_authority,
                    public_expected={"object": "Selected"},
                    ledger={},
                    producer_trace=None,
                )
                collector.atomic_write_json(
                    Path(child_args.report),
                    {
                        "discovered": 1,
                        "processed": 1,
                        "skipped": 0,
                        "failed": 0,
                        "status": "passed",
                        "publicExpectedStatus": "passed",
                        "ledgerValidationStatus": "passed",
                        "ledgerDriftStatus": "unchanged",
                        "producerTraceStatus": "not_evaluated",
                    },
                )
                return 0

            with mock.patch.object(collector, "run_via_freecadcmd", side_effect=fake_run):
                with contextlib.redirect_stderr(io.StringIO()):
                    result = collector.run_repeated_checks(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertEqual("passed", payload["publicExpectedStatus"])
            self.assertEqual("passed", payload["ledgerValidationStatus"])
            self.assertEqual(1, payload["manifest"]["cases"])
            self.assertEqual("p1", payload["manifest"]["entries"][0]["phase"])
            self.assertEqual("selected", payload["manifest"]["entries"][0]["case"])

    def test_phase_repeat_processes_only_native_authorities_without_role_skips(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            native_fixture = phase / "native.json"
            authority = phase / "expected" / "native.freecad.json"
            authority.parent.mkdir(parents=True)
            native_fixture.write_text('{"Objects": []}\n', encoding="utf-8")
            (phase / "protocol.json").write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(authority, {"object": "Native"})
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            catalog = SimpleNamespace(
                cases=[SimpleNamespace(input_path=native_fixture)],
                skipped=[
                    {
                        "phase": "p1",
                        "case": "protocol",
                        "role": "protocol_only",
                        "reason": "manual protocol contract",
                    }
                ],
                errors=[],
            )
            report = root / "report.json"
            args = collector.parse_args(
                [
                    "--phase",
                    "p1",
                    "--check",
                    "--repeat",
                    "2",
                    "--candidate-root",
                    str(root / "candidates"),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            def fake_run(_argv: list[str], child_args: object) -> int:
                return collector.run_inside_freecad(child_args)

            with (
                mock.patch.object(collector, "fixture_role_catalog", return_value=catalog),
                mock.patch.object(collector, "collect_one", return_value={"object": "Native"}),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                mock.patch.object(collector, "validate_generated_expected_ledger", return_value=[]),
                mock.patch.object(collector, "run_via_freecadcmd", side_effect=fake_run),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                result = collector.run_repeated_checks(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertEqual(1, payload["manifest"]["cases"])
            self.assertTrue(
                all(run["report"]["skipped"] == 0 for run in payload["runs"])
            )

    def test_preflight_failure_still_writes_machine_readable_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            expected = phase / "expected"
            expected.mkdir(parents=True)
            (phase / "broken.json").write_text("{}\n", encoding="utf-8")
            (expected / "broken.freecad.json").write_text("{}\n", encoding="utf-8")
            (expected / "broken.freecad.producer-trace.json").write_text("{}\n", encoding="utf-8")
            freecadcmd = root / "FreeCADCmd"
            freecadcmd.write_bytes(b"candidate")
            candidate_root = root / "candidates"
            report = root / "report.json"

            with contextlib.redirect_stderr(io.StringIO()):
                result = collector.main(
                    [
                        "--all-native",
                        "--check",
                        "--repeat",
                        "2",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                        "--fixtures-root",
                        str(fixtures),
                        "--freecadcmd",
                        str(freecadcmd),
                    ]
                )

            self.assertEqual(result, 1)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(payload["status"], "failed")
            self.assertEqual(payload["publicExpectedStatus"], "not_evaluated")
            self.assertEqual(payload["ledgerValidationStatus"], "not_evaluated")
            self.assertEqual(payload["ledgerDriftStatus"], "not_evaluated")
            self.assertEqual(payload["producerTraceStatus"], "not_evaluated")
            self.assertEqual(
                collector.file_sha256(Path(collector.__file__)),
                payload["collector"]["sha256"],
            )
            self.assertEqual(payload["stage"], "preflight")
            self.assertIn("broken.freecad.ledger.json", payload["detail"])


class CollectionReportTests(unittest.TestCase):
    def test_single_case_missing_expected_fails_before_collection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            fixture_path = fixtures / "p1" / "case.json"
            fixture_path.parent.mkdir(parents=True)
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            report = root / "report.json"
            args = collector.parse_args(
                [
                    str(fixture_path),
                    "--fixtures-root",
                    str(fixtures),
                    "--check",
                    "--validate-ledger",
                    "--report",
                    str(report),
                ]
            )

            with mock.patch.object(collector, "collect_one") as collect_one:
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual("failed", payload["publicExpectedStatus"])
            self.assertEqual("failed", payload["ledgerValidationStatus"])
            self.assertEqual("not_evaluated", payload["ledgerDriftStatus"])
            self.assertEqual("missing", payload["cases"][0]["artifacts"]["publicAuthority"]["status"])
            collect_one.assert_not_called()

    def test_single_case_missing_ledger_fails_before_collection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            fixture_path = fixtures / "p1" / "case.json"
            expected = fixtures / "p1" / "expected" / "case.freecad.json"
            expected.parent.mkdir(parents=True)
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(expected, {"object": "Case"})
            report = root / "report.json"
            args = collector.parse_args(
                [
                    str(fixture_path),
                    "--fixtures-root",
                    str(fixtures),
                    "--check",
                    "--validate-ledger",
                    "--report",
                    str(report),
                ]
            )

            with mock.patch.object(collector, "collect_one") as collect_one:
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual("missing", payload["cases"][0]["artifacts"]["ledgerAuthority"]["status"])
            self.assertEqual("not_evaluated", payload["ledgerDriftStatus"])
            collect_one.assert_not_called()

    def test_collection_exception_is_machine_readable_and_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            fixture_path = fixtures / "p1" / "case.json"
            expected = fixtures / "p1" / "expected" / "case.freecad.json"
            expected.parent.mkdir(parents=True)
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(expected, {"object": "Case"})
            collector.atomic_write_json(collector.ledger_path_for_expected(expected), {})
            report = root / "report.json"
            args = collector.parse_args(
                [
                    str(fixture_path),
                    "--fixtures-root",
                    str(fixtures),
                    "--check",
                    "--validate-ledger",
                    "--report",
                    str(report),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", side_effect=RuntimeError("collection exploded")),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual("collection", payload["firstFailure"]["errors"][0]["stage"])
            self.assertIn("collection exploded", payload["firstFailure"]["errors"][0]["detail"])

    def test_zero_case_phase_writes_preflight_failure_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            fixtures.mkdir()
            report = root / "report.json"
            catalog = SimpleNamespace(cases=[], skipped=[], errors=[])

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": SimpleNamespace()}),
                mock.patch.object(collector, "fixture_role_catalog", return_value=catalog),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                result = collector.main(
                    [
                        "--phase",
                        "empty",
                        "--check",
                        "--fixtures-root",
                        str(fixtures),
                        "--report",
                        str(report),
                    ]
                )

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual("preflight", payload["stage"])
            self.assertIn("no native authority selected", payload["detail"])

    def test_write_mode_does_not_claim_public_semantic_equality(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            fixture_path = fixtures / "p1" / "case.json"
            fixture_path.parent.mkdir(parents=True)
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            out_path = fixtures / "p1" / "expected" / "case.freecad.json"
            report = root / "report.json"
            args = collector.parse_args(
                [
                    str(fixture_path),
                    "--fixtures-root",
                    str(fixtures),
                    "--out",
                    str(out_path),
                    "--report",
                    str(report),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value={"object": "Pad"}),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertEqual("not_evaluated", payload["publicExpectedStatus"])
            self.assertEqual("not_evaluated", payload["ledgerValidationStatus"])
            self.assertEqual("not_evaluated", payload["ledgerDriftStatus"])
            self.assertEqual("not_evaluated", payload["producerTraceStatus"])
            self.assertEqual("accepted", payload["cases"][0]["ledgerOutcome"])

    def test_reports_each_authority_artifact_and_first_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            request = {
                "Objects": [
                    {"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}
                ]
            }
            response = {"object": "Pad"}
            trace = bind_trace(producer_trace(), request, response)
            fixture_path = phase / "case.json"
            fixture_path.write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            collector.atomic_write_json(collector.producer_trace_path_for_expected(authority), trace)
            report = root / "report.json"
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--producer-trace",
                    "--candidate-root",
                    str(root / "candidate"),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value=response),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                mock.patch.object(collector, "LAST_FREECAD_PRODUCER_TRACE", trace),
            ):
                result = collector.run_inside_freecad(args)

            self.assertEqual(result, 0)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(payload["status"], "passed")
            self.assertEqual(payload["publicExpectedStatus"], "passed")
            self.assertEqual(payload["ledgerValidationStatus"], "not_evaluated")
            self.assertEqual(payload["ledgerDriftStatus"], "unchanged")
            self.assertEqual(payload["producerTraceStatus"], "not_evaluated")
            self.assertNotIn("publicLedgerConsistency", payload)
            self.assertNotIn("producerSemanticVerdict", payload)
            self.assertEqual(
                collector.file_sha256(Path(collector.__file__)),
                payload["collector"]["sha256"],
            )
            self.assertIsNone(payload["firstFailure"])
            self.assertIsNone(payload["firstProducerTraceDiagnostic"])
            self.assertEqual(len(payload["cases"]), 1)
            case = payload["cases"][0]
            self.assertEqual(collector.file_sha256(fixture_path), case["fixtureSha256"])
            self.assertEqual(collector.file_sha256(authority), case["publicExpectedSha256"])
            self.assertEqual(
                collector.file_sha256(collector.ledger_path_for_expected(authority)),
                case["ledgerSha256"],
            )
            invocation = payload["collectorInvocation"]
            self.assertEqual(str(Path(collector.__file__).resolve()), invocation["argv"][1])
            invocation_args = json.loads(
                invocation["environment"][collector.ENV_ARG_NAME]
            )
            self.assertIn("--all-native", invocation_args)
            self.assertIn("--check", invocation_args)
            self.assertIn("freecadVersion", payload["runtime"])
            self.assertIn("occtVersion", payload["runtime"])
            artifacts = case["artifacts"]
            self.assertEqual(artifacts["publicAuthority"]["status"], "equal")
            self.assertEqual(artifacts["ledgerAuthority"]["status"], "equal")
            self.assertEqual(artifacts["producerTraceDiagnostic"]["status"], "not_evaluated")
            self.assertEqual(
                artifacts["producerTraceDiagnostic"]["reason"],
                "public-ledger-aligned",
            )
            self.assertEqual(artifacts["candidateWrite"]["status"], "written")
            self.assertIsNotNone(artifacts["candidateWrite"]["publicSha256"])
            self.assertIsNotNone(artifacts["candidateWrite"]["ledgerSha256"])

    def test_missing_generated_trace_does_not_block_aligned_public_ledger(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            fixture_path = phase / "case.json"
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(authority, {"object": "Pad"})
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            report = root / "report.json"
            candidate_root = root / "candidate"
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--producer-trace",
                    "--candidate-root",
                    str(candidate_root),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value={"object": "Pad"}),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                mock.patch.object(collector, "LAST_FREECAD_PRODUCER_TRACE", None),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            candidate = collector.candidate_expected_path(
                candidate_root,
                fixtures_root=fixtures,
                expected_path=authority,
            )
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertIsNone(payload["firstFailure"])
            self.assertEqual(
                "unavailable",
                payload["cases"][0]["artifacts"]["producerTraceDiagnostic"]["status"],
            )
            self.assertTrue(candidate.is_file())
            self.assertTrue(collector.ledger_path_for_expected(candidate).is_file())
            self.assertFalse(collector.producer_trace_path_for_expected(candidate).exists())

    def test_ledger_drift_is_separate_and_does_not_fail_public_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            fixture_path = phase / "case.json"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            fixture_path.write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(authority, {"object": "Pad"})
            collector.atomic_write_json(
                collector.ledger_path_for_expected(authority),
                {"metadata": "authority"},
            )
            report = root / "report.json"
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--candidate-root",
                    str(root / "candidate"),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value={"object": "Pad"}),
                mock.patch.object(
                    collector,
                    "collect_expected_ledger",
                    return_value={"metadata": "candidate"},
                ),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["status"])
            self.assertEqual("passed", payload["publicExpectedStatus"])
            self.assertEqual("drifted", payload["ledgerDriftStatus"])
            self.assertEqual("different", payload["cases"][0]["artifacts"]["ledgerAuthority"]["status"])
            self.assertIsNone(payload["firstFailure"])

    def test_invalid_generated_trace_does_not_block_aligned_public_ledger(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            (phase / "case.json").write_text('{"Objects": []}\n', encoding="utf-8")
            collector.atomic_write_json(authority, {"object": "Pad"})
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            report = root / "report.json"
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--producer-trace",
                    "--candidate-root",
                    str(root / "candidate"),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value={"object": "Pad"}),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                mock.patch.object(collector, "LAST_FREECAD_PRODUCER_TRACE", {"trace": True}),
                mock.patch.object(
                    collector,
                    "bind_producer_trace_artifacts",
                    side_effect=ValueError("invalid trace"),
                ),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["publicExpectedStatus"])
            self.assertEqual("not_evaluated", payload["ledgerValidationStatus"])
            self.assertEqual("unchanged", payload["ledgerDriftStatus"])
            self.assertEqual("invalid", payload["producerTraceStatus"])
            self.assertEqual(
                "invalid",
                payload["cases"][0]["artifacts"]["producerTraceDiagnostic"]["status"],
            )
            self.assertIsNone(payload["firstFailure"])

    def test_public_failure_uses_trace_only_as_separate_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "fixtures"
            phase = fixtures / "p1"
            authority = phase / "expected" / "case.freecad.json"
            authority.parent.mkdir(parents=True)
            request = {"Objects": [{"Name": "Pad", "TypeId": "PartDesign::Pad", "Properties": {}}]}
            expected_response = {"object": "Pad"}
            actual_response = {"object": "Other"}
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, expected_response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            collector.atomic_write_json(
                collector.producer_trace_path_for_expected(authority),
                bind_trace(producer_trace(), request, expected_response),
            )
            report = root / "report.json"
            args = collector.parse_args(
                [
                    "--all-native",
                    "--check",
                    "--producer-trace",
                    "--candidate-root",
                    str(root / "candidate"),
                    "--report",
                    str(report),
                    "--fixtures-root",
                    str(fixtures),
                    "--freecadcmd",
                    str(root / "FreeCADCmd"),
                ]
            )

            with (
                mock.patch.object(collector, "collect_one", return_value=actual_response),
                mock.patch.object(collector, "collect_expected_ledger", return_value={}),
                mock.patch.object(
                    collector,
                    "LAST_FREECAD_PRODUCER_TRACE",
                    producer_trace(field_value="different"),
                ),
            ):
                result = collector.run_inside_freecad(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual(1, payload["failed"])
            self.assertEqual("failed", payload["publicExpectedStatus"])
            self.assertEqual("not_evaluated", payload["ledgerValidationStatus"])
            self.assertEqual("unchanged", payload["ledgerDriftStatus"])
            self.assertEqual("failed", payload["producerTraceStatus"])
            self.assertEqual(
                ["public-authority"],
                [error["stage"] for error in payload["firstFailure"]["errors"]],
            )
            self.assertEqual(
                "different",
                payload["firstProducerTraceDiagnostic"]["status"],
            )


if __name__ == "__main__":
    unittest.main()
