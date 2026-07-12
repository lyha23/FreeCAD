"""Focused tests for the native FreeCAD fixture regression gate."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tests.producer_trace_fixture import bind_trace, producer_trace, retag


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected_regression_gate", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


class NativeFixtureManifestTests(unittest.TestCase):
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

    def test_writes_complete_candidate_triple_below_mirrored_phase_path(self) -> None:
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
            trace = bind_trace(producer_trace(), request, response)
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            collector.atomic_write_json(collector.producer_trace_path_for_expected(authority), trace)
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
                    producer_trace=trace,
                )

            self.assertEqual(
                collector.compare_candidate_runs(manifest, [run_a, run_b], fixtures_root=fixtures),
                [],
            )

            collector.atomic_write_json(
                collector.producer_trace_path_for_expected(
                    collector.candidate_expected_path(
                        run_b, fixtures_root=fixtures, expected_path=authority
                    )
                ),
                bind_trace(retag(producer_trace(), 0x5A), request, response),
            )
            variations: list[dict] = []
            differences = collector.compare_candidate_runs(
                manifest,
                [run_a, run_b],
                fixtures_root=fixtures,
                variations=variations,
            )
            self.assertEqual([], differences)
            self.assertEqual(1, len(variations))
            self.assertEqual("equivalent_after_projection", variations[0]["status"])
            self.assertIn(
                "runtime_tag_bijection", variations[0]["normalizationSummary"]
            )

            run_b_expected = collector.candidate_expected_path(
                run_b,
                fixtures_root=fixtures,
                expected_path=authority,
            )
            collector.atomic_write_json(run_b_expected, {"object": "Other"})
            with contextlib.redirect_stderr(io.StringIO()):
                errors = collector.compare_candidate_runs(
                    manifest,
                    [run_a, run_b],
                    fixtures_root=fixtures,
                )
            self.assertTrue(any(error["artifact"] == "public" for error in errors))


class RepeatedRegressionGateTests(unittest.TestCase):
    def test_runs_collector_twice_and_publishes_passing_report(self) -> None:
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
            (phase / "case.json").write_text(json.dumps(request), encoding="utf-8")
            collector.atomic_write_json(authority, response)
            collector.atomic_write_json(collector.ledger_path_for_expected(authority), {})
            collector.atomic_write_json(collector.producer_trace_path_for_expected(authority), trace)
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
                    producer_trace=trace,
                )
                collector.atomic_write_json(
                    Path(child_args.report),
                    {
                        "discovered": 1,
                        "processed": 1,
                        "skipped": 0,
                        "failed": 0,
                        "status": "passed",
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
            self.assertEqual(payload["manifest"]["cases"], 1)
            self.assertEqual(payload["candidate"]["sha256"], collector.file_sha256(freecadcmd))
            self.assertEqual(payload["candidate"]["sourceRoot"], str(root.resolve()))
            self.assertEqual(payload["candidate"]["build"]["buildType"], "RelWithDebInfo")
            self.assertEqual(payload["candidate"]["build"]["generator"], "Ninja")
            self.assertIn("commit", payload["candidate"])
            self.assertIn("dirty", payload["candidate"])
            self.assertEqual(payload["candidateRunDifferences"], [])

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
            self.assertEqual(payload["stage"], "preflight")
            self.assertIn("broken.freecad.ledger.json", payload["detail"])


class CollectionReportTests(unittest.TestCase):
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
            self.assertIsNone(payload["firstFailure"])
            self.assertEqual(len(payload["cases"]), 1)
            artifacts = payload["cases"][0]["artifacts"]
            self.assertEqual(artifacts["publicAuthority"]["status"], "equal")
            self.assertEqual(artifacts["ledgerAuthority"]["status"], "equal")
            self.assertEqual(artifacts["producerTraceDiagnostic"]["status"], "equal")
            self.assertEqual(artifacts["candidateWrite"]["status"], "written")


if __name__ == "__main__":
    unittest.main()
