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

    def test_accepts_single_case_staging_repeat_without_checked_in_authority(self) -> None:
        args = collector.parse_args(
            [
                "/tmp/staging/fixtures/p1/case.json",
                "--fixtures-root",
                "/tmp/staging/fixtures",
                "--validate-ledger",
                "--repeat",
                "2",
                "--candidate-root",
                "/tmp/staging/candidates/p1/case",
                "--report",
                "/tmp/staging/reports/p1/case/repeat2.json",
            ]
        )

        child = collector.staging_repeat_child_argv(
            args,
            candidate_root=Path("/tmp/staging/candidates/p1/case/run-a"),
            report=Path("/tmp/staging/candidates/p1/case/run-a.report.json"),
        )

        self.assertFalse(args.check)
        self.assertIn("--out", child)
        self.assertNotIn("--check", child)
        self.assertNotIn("--candidate-root", child)
        output = Path(child[child.index("--out") + 1])
        self.assertEqual(
            Path("/tmp/staging/candidates/p1/case/run-a/p1/expected/case.freecad.json"),
            output,
        )

    def test_staging_repeat_requires_strict_ledger_validation(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                collector.parse_args(
                    [
                        "/tmp/staging/fixtures/p1/case.json",
                        "--fixtures-root",
                        "/tmp/staging/fixtures",
                        "--repeat",
                        "2",
                        "--candidate-root",
                        "/tmp/staging/candidates/p1/case",
                    ]
                )

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
    def test_staging_repeat_runs_two_independent_collections_without_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures = root / "staging" / "fixtures"
            fixture = fixtures / "p1" / "case.json"
            fixture.parent.mkdir(parents=True)
            fixture.write_text(
                json.dumps(
                    {
                        "Objects": [
                            {"Name": "Box", "TypeId": "Part::Box", "Properties": {}}
                        ]
                    }
                ),
                encoding="utf-8",
            )
            candidate_root = root / "staging" / "candidates" / "p1" / "case"
            report = root / "staging" / "reports" / "p1" / "case" / "repeat2.json"
            freecadcmd = root / "FreeCADCmd"
            freecadcmd.write_bytes(b"candidate")
            args = collector.parse_args(
                [
                    str(fixture),
                    "--fixtures-root",
                    str(fixtures),
                    "--validate-ledger",
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
                output = Path(child_args.out)
                collector.atomic_write_json(output, {"object": "Box"})
                collector.atomic_write_json(
                    collector.ledger_path_for_expected(output),
                    {"outcome": "accepted"},
                )
                collector.atomic_write_json(
                    Path(child_args.report),
                    {
                        "discovered": 1,
                        "processed": 1,
                        "skipped": 0,
                        "failed": 0,
                        "status": "passed",
                        "publicExpectedStatus": "not_evaluated",
                        "ledgerValidationStatus": "passed",
                        "ledgerDriftStatus": "not_evaluated",
                        "producerTraceStatus": "not_evaluated",
                        "candidate": {
                            "path": str(freecadcmd.resolve()),
                            "sha256": collector.file_sha256(freecadcmd),
                        },
                        "collector": collector.collector_receipt(),
                        "runtime": {
                            "freecadVersion": "1.2.0 revision test",
                            "occtVersion": "7.8.1",
                        },
                    },
                )
                return 0

            with mock.patch.object(collector, "run_via_freecadcmd", side_effect=fake_run) as run:
                with contextlib.redirect_stderr(io.StringIO()):
                    result = collector.run_repeated_staging_collections(args)

            payload = json.loads(report.read_text(encoding="utf-8"))
            run_a = candidate_root / "run-a" / "p1" / "expected" / "case.freecad.json"
            run_b = candidate_root / "run-b" / "p1" / "expected" / "case.freecad.json"
            run_a_input = candidate_root / "run-a" / "p1" / "case.json"
            run_b_input = candidate_root / "run-b" / "p1" / "case.json"
            run_a_exists = run_a.is_file()
            run_b_exists = run_b.is_file()
            isolated_inputs_exist = run_a_input.is_file() and run_b_input.is_file()

        self.assertEqual(0, result)
        self.assertEqual(2, run.call_count)
        self.assertTrue(run_a_exists)
        self.assertTrue(run_b_exists)
        self.assertTrue(isolated_inputs_exist)
        self.assertEqual("repeated-staging-collection", payload["mode"])
        self.assertEqual("passed", payload["status"])
        self.assertEqual("passed", payload["publicExpectedStatus"])
        self.assertEqual("passed", payload["ledgerValidationStatus"])
        self.assertEqual("unchanged", payload["ledgerDriftStatus"])
        self.assertEqual("not_evaluated", payload["producerTraceStatus"])
        self.assertEqual(1, payload["manifest"]["cases"])
        self.assertEqual(2, len(payload["runs"]))
        self.assertEqual([], payload["candidateRunDifferences"])

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

    def test_document_object_group_uses_native_membership_api(self) -> None:
        class Group:
            def __init__(self) -> None:
                self.added: list[object] = []

            def addObject(self, child: object) -> None:
                self.added.append(child)

        box = object()
        group = Group()

        handled = collector.set_document_object_group_property(
            {"Box": box},
            group,
            "Group",
            {
                "PropertyType": "App::PropertyLinkList",
                "values": ["Box"],
            },
        )

        self.assertTrue(handled)
        self.assertEqual([box], group.added)
        self.assertIn("App::DocumentObjectGroup", collector.SUPPORTED_NATIVE_TYPES)

    def test_retained_typeid_batches_are_enabled_for_native_probing(self) -> None:
        self.assertLessEqual(
            {
                "App::Origin",
                "App::Part",
                "App::Plane",
                "App::Point",
                "Part::Extrusion",
                "Part::GeometryCurve",
                "Part::Offset",
                "Part::Offset2D",
                "Part::Thickness",
                "PartDesign::Draft",
                "PartDesign::ShapeBinder",
                "PartDesign::SubShapeBinder",
                "PartDesign::Thickness",
            },
            collector.SUPPORTED_NATIVE_TYPES,
        )

    def test_subshape_binder_partial_load_is_restored_after_support(self) -> None:
        properties = {
            "BindCopyOnChange": {
                "PropertyType": "App::PropertyEnumeration",
                "value": "Mutated",
            },
            "PartialLoad": {"PropertyType": "App::PropertyBool", "value": True},
            "Support": {
                "PropertyType": "App::PropertyXLinkSubList",
                "SubSet": [{"value": "SupportBox", "SubList": []}],
            },
            "Refine": {"PropertyType": "App::PropertyBool", "value": False},
        }

        ordered = list(
            collector.ordered_native_property_items(
                "PartDesign::SubShapeBinder",
                properties,
            )
        )

        self.assertEqual(
            ["Support", "Refine", "BindCopyOnChange", "PartialLoad"],
            [name for name, _ in ordered],
        )

    def test_material_shape_property_uses_materials_public_value(self) -> None:
        class Material:
            UUID = "random-runtime-uuid"

        class Box:
            Name = "Box"
            ShapeMaterial = None

        materials = SimpleNamespace(Material=Material)
        box = Box()
        value = {
            "PropertyType": "Materials::PropertyMaterial",
            "Name": "Fixture steel",
            "Author": "FreeCAD fixture",
            "License": "CC0-1.0",
            "Description": "Deterministic material public API coverage",
        }

        with mock.patch.dict(sys.modules, {"Materials": materials}):
            collector.set_property(object(), {}, box, "ShapeMaterial", value)

        self.assertEqual("Fixture steel", box.ShapeMaterial.Name)
        self.assertEqual("FreeCAD fixture", box.ShapeMaterial.Author)
        self.assertEqual("CC0-1.0", box.ShapeMaterial.License)
        self.assertEqual(
            {
                "name": "Fixture steel",
                "author": "FreeCAD fixture",
                "license": "CC0-1.0",
                "description": "Deterministic material public API coverage",
                "url": "",
                "reference": "",
            },
            collector.material_expected_payload(box.ShapeMaterial),
        )

    def test_spreadsheet_cells_use_sheet_set_and_alias_public_api(self) -> None:
        class Sheet:
            Name = "Sheet"

            def __init__(self) -> None:
                self.set_calls: list[tuple[str, str]] = []
                self.alias_calls: list[tuple[str, str]] = []

            def set(self, address: str, content: str) -> None:
                self.set_calls.append((address, content))

            def setAlias(self, address: str, alias: str) -> None:
                self.alias_calls.append((address, alias))

        sheet = Sheet()
        value = {
            "PropertyType": "Spreadsheet::PropertySheet",
            "cells": [
                {"address": "A1", "content": "3", "alias": "left"},
                {"address": "B1", "content": "=left + 4"},
            ],
        }

        collector.set_property(object(), {}, sheet, "Cells", value)

        self.assertEqual([("A1", "3"), ("B1", "=left + 4")], sheet.set_calls)
        self.assertEqual([("A1", "left")], sheet.alias_calls)

    def test_spreadsheet_layout_operations_use_public_sheet_api(self) -> None:
        class Sheet:
            Name = "Sheet"

            def __init__(self) -> None:
                self.calls: list[tuple] = []

            def set(self, address: str, content: str) -> None:
                pass

            def setAlias(self, address: str, alias: str) -> None:
                pass

            def setStyle(self, cell_range, value, options):
                self.calls.append(("setStyle", cell_range, value, options))

            def getStyle(self, address):
                return {"bold"}

            def setColumnWidth(self, column, width):
                self.calls.append(("setColumnWidth", column, width))

            def getColumnWidth(self, column):
                return 120

            def getUsedCells(self):
                return ["A1"]

        collector.ACTIVE_SPREADSHEET_OPERATION_RECEIPTS = {}
        sheet = Sheet()
        value = {
            "PropertyType": "Spreadsheet::PropertySheet",
            "cells": [{"address": "A1", "content": "3"}],
            "operations": [
                {"op": "setStyle", "range": "A1", "value": "bold"},
                {"op": "setColumnWidth", "column": "A", "width": 120},
            ],
        }

        collector.set_spreadsheet_cells(sheet, value)

        self.assertEqual(
            [
                ("setStyle", "A1", "bold", "replace"),
                ("setColumnWidth", "A", 120),
            ],
            sheet.calls,
        )
        self.assertEqual(
            ["target_result", "target_result"],
            [
                item["evidenceLevel"]
                for item in collector.ACTIVE_SPREADSHEET_OPERATION_RECEIPTS["Sheet"]
            ],
        )

    def test_recompute_mutations_run_between_two_native_recomputes(self) -> None:
        class Material:
            UUID = "runtime-uuid"

        class Box:
            Name = "Box"
            TypeId = "Part::Box"
            ShapeMaterial = None

        class Document:
            def __init__(self) -> None:
                self.calls: list[list[str]] = []

            def recompute(self, objects: list[object]) -> int:
                self.calls.append([str(getattr(item, "Name")) for item in objects])
                return 0

        fixture = {
            "Objects": [
                {
                    "Name": "Box",
                    "TypeId": "Part::Box",
                    "Properties": {},
                }
            ],
            "recompute": {
                "objs": ["Box"],
                "mutations": [
                    {
                        "object": "Box",
                        "properties": {
                            "ShapeMaterial": {
                                "PropertyType": "Materials::PropertyMaterial",
                                "Name": "Updated steel",
                            }
                        },
                    }
                ],
            },
        }
        box = Box()
        doc = Document()

        with mock.patch.dict(
            sys.modules,
            {"Materials": SimpleNamespace(Material=Material)},
        ):
            collector.run_recompute_sequence(
                object(),
                doc,
                {"Box": box},
                fixture,
            )

        self.assertEqual([["Box"], ["Box"]], doc.calls)
        self.assertEqual("Updated steel", box.ShapeMaterial.Name)

    def test_mesh_transform_noop_is_not_enabled_as_native_geometry(self) -> None:
        # FreeCAD FeatureMeshTransform.cpp::Transform::execute() has its mesh-copy and
        # Position transform body commented out and returns StdReturn without a Mesh result.
        self.assertNotIn("Mesh::Transform", collector.SUPPORTED_NATIVE_TYPES)

    def test_mesh_feature_batches_use_one_public_mesh_projection(self) -> None:
        self.assertLessEqual(
            {
                "Mesh::Sphere",
                "Mesh::Cube",
                "Mesh::Cone",
                "Mesh::Cylinder",
                "Mesh::Ellipsoid",
                "Mesh::Torus",
                "Mesh::SetOperations",
                "Mesh::Feature",
                "Mesh::FixDefects",
                "Mesh::FillHoles",
                "Mesh::FixDeformations",
                "Mesh::FixDegenerations",
                "Mesh::FixDuplicatedPoints",
                "Mesh::FixIndices",
                "Mesh::FixNonManifolds",
                "Mesh::RemoveComponents",
                "Mesh::HarmonizeNormals",
                "Mesh::FlipNormals",
                "Mesh::FixDuplicatedFaces",
            },
            collector.SUPPORTED_NATIVE_TYPES,
        )

        class Vector:
            def __init__(self, x: float, y: float, z: float) -> None:
                self.x = x
                self.y = y
                self.z = z

        class Facet:
            def __init__(self, normal: Vector) -> None:
                self.Normal = normal

        class Mesh:
            CountPoints = 4
            CountEdges = 6
            CountFacets = 4
            Volume = 1.0 / 6.0
            BoundBox = SimpleNamespace(
                XMin=0.0,
                YMin=0.0,
                ZMin=0.0,
                XMax=1.0,
                YMax=1.0,
                ZMax=1.0,
            )
            Facets = [Facet(Vector(0.0, 0.0, -1.0)), Facet(Vector(1.0, 0.0, 0.0))]

            def isSolid(self) -> bool:
                return True

            def countComponents(self) -> int:
                return 1

            def hasNonManifolds(self) -> bool:
                return False

            def hasNonUniformOrientedFacets(self) -> bool:
                return True

            def hasSelfIntersections(self) -> bool:
                return False

            def countNonUniformOrientedFacets(self) -> int:
                return 1

        sphere = SimpleNamespace(
            Name="Sphere",
            TypeId="Mesh::Sphere",
            Mesh=Mesh(),
            State=[],
            PropertyList=["Mesh", "Radius", "Sampling"],
        )

        payload = collector.mesh_feature_summary(sphere)

        self.assertTrue(payload["closed"])
        self.assertEqual(4, payload["mesh_summary"]["triangle_count"])
        self.assertEqual(1, payload["mesh_defects"]["component_count"])
        self.assertEqual(
            [0.0, 0.0, -1.0],
            payload["mesh_orientation"]["first_facet_normal"],
        )
        self.assertNotIn("native_diagnostic", payload)

    def test_mesh_kernel_property_uses_indexed_public_addfacets_contract(self) -> None:
        class MeshValue:
            def __init__(self) -> None:
                self.calls = []

            def addFacets(self, indexed, check) -> None:
                self.calls.append((indexed, check))

        mesh_value = MeshValue()
        feature = SimpleNamespace(Name="Source", Mesh=None)
        mesh_module = SimpleNamespace(Mesh=lambda: mesh_value)
        freecad = SimpleNamespace(Vector=lambda x, y, z: (x, y, z))
        envelope = {
            "PropertyType": "Mesh::PropertyMeshKernel",
            "vertices": [[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            "facets": [[0, 1, 2]],
            "check": False,
        }

        with mock.patch.dict(sys.modules, {"Mesh": mesh_module}):
            collector.set_property(freecad, {}, feature, "Mesh", envelope)

        self.assertIs(mesh_value, feature.Mesh)
        self.assertEqual(
            [(([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)], [(0, 1, 2)]), False)],
            mesh_value.calls,
        )

        invalid = dict(envelope, facets=[[0, 1, 3]])
        with self.assertRaisesRegex(collector.UnsupportedFixture, "out of range"):
            collector.mesh_kernel_property_value(freecad, invalid)

    def test_mesh_defect_projection_keeps_item_local_native_diagnostics(self) -> None:
        class EmptyMesh:
            CountPoints = 0
            CountEdges = 0
            CountFacets = 0
            Volume = 0.0
            BoundBox = SimpleNamespace(
                XMin=float("inf"),
                YMin=float("inf"),
                ZMin=float("inf"),
                XMax=-float("inf"),
                YMax=-float("inf"),
                ZMax=-float("inf"),
            )
            Facets = []

            def isSolid(self) -> bool:
                return False

            def countComponents(self) -> int:
                return 0

            def hasNonManifolds(self) -> bool:
                return False

            def hasNonUniformOrientedFacets(self) -> bool:
                return False

            def hasSelfIntersections(self) -> bool:
                return False

            def countNonUniformOrientedFacets(self) -> int:
                return 0

        result = SimpleNamespace(
            Name="Harmonized",
            TypeId="Mesh::HarmonizeNormals",
            Mesh=EmptyMesh(),
            State=["Invalid"],
            PropertyList=["Mesh", "Source"],
            Source=None,
        )

        payload = collector.mesh_feature_summary(result)

        self.assertEqual([0.0, 0.0, 0.0], payload["bbox"]["min"])
        self.assertEqual(
            "mesh_source_missing",
            payload["native_diagnostic"]["code"],
        )

    def test_stable_subshape_index_order_is_not_a_public_semantic_difference(self) -> None:
        def result_subshape(indexed: str, token: str) -> dict[str, object]:
            return {
                "id": f"Offset2D:{indexed}",
                "kind": "Edge",
                "indexed": indexed,
                "subname": indexed,
                "stableSubname": token,
                "identityStatus": "stable",
                "fullSubname": f"Offset2D.{indexed}",
                "rawFreecadMappedName": token,
                "canonicalFreecadMappedName": token,
                "resolvedIndexed": indexed,
                "ShadowSub": [],
                "ReferenceShadow": [],
            }

        def topo_state(first: str, second: str) -> dict[str, object]:
            entries = {}
            subshapes = {}
            for indexed, token in (("Edge1", first), ("Edge2", second)):
                subshapes[indexed] = {
                    "subname": indexed,
                    "identityStatus": "stable",
                    "resolvedIndexed": indexed,
                    "rawFreecadMappedName": token,
                    "canonicalFreecadMappedName": token,
                }
                entries[token] = {
                    "target": {"object": "Offset2D", "subname": indexed},
                    "source": {"object": "Offset2D", "subname": indexed},
                    "mappedName": {"raw": token, "canonical": token},
                    "shapeKind": "edge",
                    "recoverability": "resolved",
                    "evidence": {
                        "source": "freecad_expected_collector",
                        "mapperHistoryIds": [],
                        "childElementMapKey": None,
                    },
                }
            return {
                "topoNamingState": {
                    "objects": {
                        "Offset2D": {
                            "subshapes": subshapes,
                            "elementMap": {
                                "encoding": "cad-core.element-map.v1",
                                "status": "history_partial",
                                "entries": entries,
                            },
                            "childElementMaps": [],
                            "mapperHistory": [],
                        }
                    }
                }
            }

        existing_object = {
            "subshapes": [
                result_subshape("Edge1", "stable-a"),
                result_subshape("Edge2", "stable-b"),
            ]
        }
        reordered_object = {
            "subshapes": [
                result_subshape("Edge1", "stable-b"),
                result_subshape("Edge2", "stable-a"),
            ]
        }

        self.assertNotIn(
            "subshapes",
            collector.compare_object_expected(existing_object, reordered_object),
        )
        self.assertEqual(
            [],
            collector.compare_topo_naming_state_expected(
                topo_state("stable-a", "stable-b"),
                topo_state("stable-b", "stable-a"),
            ),
        )

        missing_token_object = {
            "subshapes": [
                result_subshape("Edge1", "stable-a"),
                result_subshape("Edge2", "stable-c"),
            ]
        }
        self.assertIn(
            "subshapes",
            collector.compare_object_expected(existing_object, missing_token_object),
        )

    def test_sketch_constraints_use_the_native_add_constraint_api(self) -> None:
        class Sketch:
            def __init__(self) -> None:
                self.added: list[tuple[object, ...]] = []

            def addConstraint(self, constraint: tuple[object, ...]) -> None:
                self.added.append(constraint)

        sketch = Sketch()
        sketcher = SimpleNamespace(Constraint=lambda *args: args)

        with mock.patch.dict(sys.modules, {"Sketcher": sketcher}):
            collector.set_sketch_property(
                object(),
                {},
                sketch,
                "Constraints",
                [{"Type": "Horizontal", "First": 0}],
            )

        self.assertEqual([("Horizontal", 0)], sketch.added)

    def test_sketch_constraint_enum_and_datum_use_native_constructor_shape(self) -> None:
        class Sketch:
            def __init__(self) -> None:
                self.added: list[tuple[object, ...]] = []

            def addConstraint(self, constraint: tuple[object, ...]) -> None:
                self.added.append(constraint)

        sketch = Sketch()
        sketcher = SimpleNamespace(Constraint=lambda *args: args)

        with mock.patch.dict(sys.modules, {"Sketcher": sketcher}):
            collector.set_sketch_property(
                object(),
                {},
                sketch,
                "Constraints",
                [{"Type": 9, "First": 1, "Second": 2, "Datum": 1.25}],
            )

        self.assertEqual([("Angle", 1, 2, 1.25)], sketch.added)

    def test_sketch_constraint_point_positions_use_freecad_pointpos_values(self) -> None:
        class Sketch:
            def __init__(self) -> None:
                self.added: list[tuple[object, ...]] = []

            def addConstraint(self, constraint: tuple[object, ...]) -> None:
                self.added.append(constraint)

        sketch = Sketch()
        sketcher = SimpleNamespace(Constraint=lambda *args: args)

        with mock.patch.dict(sys.modules, {"Sketcher": sketcher}):
            collector.set_sketch_property(
                object(),
                {},
                sketch,
                "Constraints",
                [
                    {
                        "Type": "Coincident",
                        "First": 0,
                        "FirstPos": "end",
                        "Second": 1,
                        "SecondPos": "start",
                    }
                ],
            )

        self.assertEqual([("Coincident", 0, 2, 1, 1)], sketch.added)

    def test_sketch_symmetric_constraint_preserves_the_third_geometry(self) -> None:
        class Sketch:
            def __init__(self) -> None:
                self.added: list[tuple[object, ...]] = []

            def addConstraint(self, constraint: tuple[object, ...]) -> None:
                self.added.append(constraint)

        sketch = Sketch()
        sketcher = SimpleNamespace(Constraint=lambda *args: args)

        with mock.patch.dict(sys.modules, {"Sketcher": sketcher}):
            collector.set_sketch_property(
                object(),
                {},
                sketch,
                "Constraints",
                [
                    {
                        "Type": "Symmetric",
                        "First": 1,
                        "FirstPos": "start",
                        "Second": 1,
                        "SecondPos": "end",
                        "Third": 0,
                    }
                ],
            )

        self.assertEqual([("Symmetric", 1, 1, 1, 2, 0)], sketch.added)

    def test_sketch_angle_preserves_explicit_first_position_via_public_property(self) -> None:
        class Constraint:
            def __init__(self, *args: object) -> None:
                self.args = args
                self.FirstPos: int | None = None

        class Sketch:
            def __init__(self) -> None:
                self.added: list[Constraint] = []

            def addConstraint(self, constraint: Constraint) -> None:
                self.added.append(constraint)

        sketch = Sketch()
        sketcher = SimpleNamespace(Constraint=Constraint)

        with mock.patch.dict(sys.modules, {"Sketcher": sketcher}):
            collector.set_sketch_property(
                object(),
                {},
                sketch,
                "Constraints",
                [
                    {
                        "Type": "Angle",
                        "First": 6,
                        "FirstPos": "start",
                        "Second": 5,
                        "Value": 1.25,
                    }
                ],
            )

        self.assertEqual(("Angle", 6, 5, 1.25), sketch.added[0].args)
        self.assertEqual(1, sketch.added[0].FirstPos)

    def test_raw_vector_uses_the_native_property_type(self) -> None:
        class FreeCAD:
            @staticmethod
            def Vector(x: float, y: float, z: float) -> tuple[float, float, float]:
                return (x, y, z)

        class Feature:
            Name = "Pad"
            Direction: object = None

            def getTypeIdOfProperty(self, name: str) -> str:
                self.assert_property_name = name
                return "App::PropertyVector"

        feature = Feature()

        collector.set_property(FreeCAD, {}, feature, "Direction", [0, 1, 1])

        self.assertEqual("Direction", feature.assert_property_name)
        self.assertEqual((0.0, 1.0, 1.0), feature.Direction)

    def test_integer_list_structured_property_is_written_generically(self) -> None:
        class Feature:
            ExternalTypes: object = None

        feature = Feature()

        collector.set_property(
            object(),
            {},
            feature,
            "ExternalTypes",
            {"PropertyType": "App::PropertyIntegerList", "value": [2, 3]},
        )

        self.assertEqual([2, 3], feature.ExternalTypes)

    def test_xlink_list_subset_resolves_object_links(self) -> None:
        link_a = object()
        link_b = object()

        resolved = collector.created_link_list(
            {"LinkA": link_a, "LinkB": link_b},
            {
                "PropertyType": "App::PropertyXLinkList",
                "SubSet": [{"value": "LinkA"}, {"value": "LinkB"}],
            },
            "LinkGroup ElementList",
        )

        self.assertEqual([link_a, link_b], resolved)
        self.assertEqual(
            {"LinkA", "LinkB"},
            collector.link_group_element_names(
                {
                    "Objects": [
                        {
                            "TypeId": "App::LinkGroup",
                            "Properties": {
                                "ElementList": {
                                    "PropertyType": "App::PropertyXLinkList",
                                    "SubSet": [
                                        {"value": "LinkA"},
                                        {"value": "LinkB"},
                                    ],
                                }
                            },
                        }
                    ]
                }
            ),
        )

    def test_show_element_link_uses_element_count_instead_of_writing_element_list(self) -> None:
        class Feature:
            def __init__(self, type_id: str, name: str, object_id: int) -> None:
                object.__setattr__(self, "TypeId", type_id)
                object.__setattr__(self, "Name", name)
                object.__setattr__(self, "ID", object_id)

            def __setattr__(self, name: str, value: object) -> None:
                if name == "ElementList":
                    raise AttributeError("ElementList is read-only")
                object.__setattr__(self, name, value)

        class Document:
            def __init__(self) -> None:
                self.objects: list[Feature] = []

            def addObject(self, type_id: str, name: str) -> Feature:
                feature = Feature(type_id, name, len(self.objects) + 1)
                self.objects.append(feature)
                return feature

        fixture = {
            "Objects": [
                {"Name": "Box", "ID": 1, "TypeId": "Part::Box", "Properties": {}},
                {
                    "Name": "ArrayLink",
                    "ID": 2,
                    "TypeId": "App::Link",
                    "Properties": {
                        "ShowElement": {
                            "PropertyType": "App::PropertyBool",
                            "value": True,
                        },
                        "ElementCount": {
                            "PropertyType": "App::PropertyInteger",
                            "value": 2,
                        },
                        "ElementList": {
                            "PropertyType": "App::PropertyLinkList",
                            "values": ["Box"],
                        },
                    },
                },
            ]
        }

        created = collector.create_objects(object(), Document(), fixture)

        self.assertEqual(2, created["ArrayLink"].ElementCount)
        fixture["Objects"][1]["Properties"]["ShowElement"]["value"] = False

        hidden_created = collector.create_objects(object(), Document(), fixture)

        self.assertEqual(2, hidden_created["ArrayLink"].ElementCount)


if __name__ == "__main__":
    unittest.main()
