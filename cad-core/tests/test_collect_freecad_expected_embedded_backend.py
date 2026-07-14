from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest import mock


COLLECTOR = Path(__file__).resolve().parents[1] / "tools" / "collect_freecad_expected.py"
SPEC = importlib.util.spec_from_file_location("collect_freecad_expected_embedded_backend", COLLECTOR)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = collector
SPEC.loader.exec_module(collector)


class EmbeddedBackendContractTests(unittest.TestCase):
    def embedded_paths(self, root: Path) -> tuple[Path, Path, Path, Path, Path]:
        fixtures = root / "fixtures"
        fixture = fixtures / "p1" / "case.json"
        fixture.parent.mkdir(parents=True)
        fixture.write_text("{}\n", encoding="utf-8")
        expected = fixture.parent / "expected" / "case.freecad.json"
        expected.parent.mkdir()
        expected.write_text("{}\n", encoding="utf-8")
        expected.with_name("case.freecad.ledger.json").write_text("{}\n", encoding="utf-8")
        candidate_root = root / "candidates"
        report = root / "embedded-report.json"
        candidate_binary = root / "F0AEmbeddedKernelHarness"
        candidate_binary.write_bytes(b"embedded-candidate")
        return fixtures, fixture, candidate_root, report, candidate_binary

    def runtime_receipt(
        self,
        candidate_binary: Path,
        freecad_module: Path,
    ) -> dict[str, object]:
        return {
            "runtimeId": "test-runtime",
            "processId": __import__("os").getpid(),
            "ownerThreadNativeId": threading.get_native_id(),
            "applicationAddress": 0x1234,
            "candidateBinary": str(candidate_binary),
            "freecadModule": str(freecad_module),
        }

    def inittab_runtime_receipt(
        self,
        candidate_binary: Path,
        binding_artifact: Path,
    ) -> dict[str, object]:
        return {
            "runtimeId": "test-runtime",
            "processId": __import__("os").getpid(),
            "ownerThreadNativeId": threading.get_native_id(),
            "applicationAddress": 0x1234,
            "candidateBinary": str(candidate_binary),
            "freecadBindingMode": "inittab",
            "freecadBindingArtifact": str(binding_artifact),
        }

    def test_requires_read_only_check_with_candidate_and_report_receipts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(
                Path(temp_dir)
            )

            invalid_argv = [
                str(fixture),
                "--fixtures-root",
                str(fixtures),
                "--validate-ledger",
            ]
            with self.assertRaisesRegex(ValueError, "--check"):
                collector.run_embedded(invalid_argv, runtime_receipt={})

    def test_rejects_fixture_outside_authority_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, _fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            outside = root / "outside.json"
            outside.write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "fixtures root"):
                collector.run_embedded(
                    [
                        str(outside),
                        "--fixtures-root",
                        str(fixtures),
                        "--check",
                        "--validate-ledger",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                    ],
                    runtime_receipt={},
                )

    def test_rejects_unregistered_fixture_without_authority_companions(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, _fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            unregistered = fixtures / "p1" / "unregistered.json"
            unregistered.write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "authority companions"):
                collector.run_embedded(
                    [
                        str(unregistered),
                        "--fixtures-root",
                        str(fixtures),
                        "--check",
                        "--validate-ledger",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                    ],
                    runtime_receipt={},
                )

    def test_runs_inside_existing_runtime_without_spawning_freecadcmd(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            def fake_run_inside(args: object) -> int:
                Path(args.report).write_text(
                    json.dumps(
                        {
                            "schema": "freecad-fixture-regression-report/v1",
                            "status": "passed",
                            "discovered": 1,
                            "processed": 0,
                            "skipped": 0,
                            "failed": 1,
                            "freecadcmd": "/must/not/be/reported",
                            "candidate": {"path": "/must/not/be/reported"},
                            "cases": [
                                {
                                    "status": "failed",
                                    "artifacts": {
                                        "publicAuthority": {"status": "equal"},
                                        "ledgerAuthority": {"status": "different"},
                                        "ledgerValidation": {"status": "valid"},
                                    },
                                }
                            ],
                        }
                    ),
                    encoding="utf-8",
                )
                return 1

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector.sys, "executable", str(candidate_binary)),
                mock.patch.object(collector, "run_inside_freecad", side_effect=fake_run_inside) as run_inside,
                mock.patch.object(collector, "run_via_freecadcmd") as run_via_freecadcmd,
            ):
                result = collector.run_embedded(
                    [
                        str(fixture),
                        "--fixtures-root",
                        str(fixtures),
                        "--check",
                        "--validate-ledger",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                    ],
                    runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                )

            self.assertEqual(0, result)
            run_inside.assert_called_once()
            run_via_freecadcmd.assert_not_called()
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual("embedded", payload["executionBackend"])
            self.assertEqual("passed", payload["status"])
            self.assertEqual("scoped_passed", payload["cadFinalResultStatus"])
            self.assertEqual("scoped_passed", payload["ledgerValidationStatus"])
            self.assertEqual("different", payload["ledgerAuthorityStatus"])
            self.assertFalse(payload["releaseGatePassed"])
            self.assertIsNone(payload["freecadcmd"])
            self.assertEqual(str(freecad_module.resolve()), payload["candidate"]["path"])
            self.assertEqual(
                str(candidate_binary.resolve()), payload["candidate"]["hostBinary"]["path"]
            )
            self.assertEqual(str(freecad_module.resolve()), payload["embeddedRuntime"]["freecadModule"])
            self.assertEqual("1.2.3 revision 456", payload["embeddedRuntime"]["freecadVersion"])

    def test_accepts_inittab_runtime_with_built_in_origin_and_real_binding_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            build = root / "build"
            candidate_binary = build / "bin/F0AEmbeddedKernelHarness"
            binding_artifact = build / "lib/libFreeCADApp.dylib"
            candidate_binary.parent.mkdir(parents=True)
            binding_artifact.parent.mkdir(parents=True)
            candidate_binary.write_bytes(b"embedded-candidate")
            binding_artifact.write_bytes(b"freecad-app")
            (build / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={root}\n",
                encoding="utf-8",
            )
            fake_freecad = types.SimpleNamespace(
                __spec__=types.SimpleNamespace(origin="built-in"),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector.sys, "executable", str(candidate_binary)),
            ):
                FreeCAD, actual_binary, artifact, binding_mode = collector._validate_embedded_runtime(
                    self.inittab_runtime_receipt(candidate_binary, binding_artifact)
                )

            self.assertIs(fake_freecad, FreeCAD)
            self.assertEqual(candidate_binary.resolve(), actual_binary)
            self.assertEqual(binding_artifact.resolve(), artifact)
            self.assertEqual("inittab", binding_mode)

    def test_missing_embedded_freecad_module_never_falls_back_to_subprocess(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            with (
                mock.patch.dict(sys.modules, {"FreeCAD": None}),
                mock.patch.object(collector, "run_via_freecadcmd") as run_via_freecadcmd,
            ):
                with self.assertRaisesRegex(RuntimeError, "already initialized"):
                    collector.run_embedded(
                        [
                            str(fixture),
                            "--fixtures-root",
                            str(fixtures),
                            "--check",
                            "--validate-ledger",
                            "--candidate-root",
                            str(candidate_root),
                            "--report",
                            str(report),
                        ],
                        runtime_receipt={
                            "runtimeId": "test-runtime",
                            "processId": __import__("os").getpid(),
                            "ownerThreadNativeId": threading.get_native_id(),
                            "applicationAddress": 0x1234,
                            "candidateBinary": str(candidate_binary),
                            "freecadModule": str(root / "FreeCAD.so"),
                        },
                    )
            run_via_freecadcmd.assert_not_called()

    def test_owner_thread_receipt_mismatch_is_rejected_before_collection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )
            receipt = self.runtime_receipt(candidate_binary, freecad_module)
            receipt["ownerThreadNativeId"] = threading.get_native_id() + 1

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector, "run_inside_freecad") as run_inside,
            ):
                with self.assertRaisesRegex(RuntimeError, "owner thread"):
                    collector.run_embedded(
                        [
                            str(fixture),
                            "--fixtures-root",
                            str(fixtures),
                            "--check",
                            "--validate-ledger",
                            "--candidate-root",
                            str(candidate_root),
                            "--report",
                            str(report),
                        ],
                        runtime_receipt=receipt,
                    )
            run_inside.assert_not_called()

    def test_forged_candidate_binary_is_rejected_before_collection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector, "run_inside_freecad") as run_inside,
            ):
                with self.assertRaisesRegex(RuntimeError, "current process executable"):
                    collector.run_embedded(
                        [
                            str(fixture),
                            "--fixtures-root",
                            str(fixtures),
                            "--check",
                            "--validate-ledger",
                            "--candidate-root",
                            str(candidate_root),
                            "--report",
                            str(report),
                        ],
                        runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                    )
            run_inside.assert_not_called()

    def test_missing_required_report_is_a_hard_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector.sys, "executable", str(candidate_binary)),
                mock.patch.object(collector, "run_inside_freecad", return_value=0),
            ):
                with self.assertRaisesRegex(RuntimeError, "required report"):
                    collector.run_embedded(
                        [
                            str(fixture),
                            "--fixtures-root",
                            str(fixtures),
                            "--check",
                            "--validate-ledger",
                            "--candidate-root",
                            str(candidate_root),
                            "--report",
                            str(report),
                        ],
                        runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                    )

    def test_unexplained_collector_failure_cannot_be_relabelled_passed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            def fake_run_inside(args: object) -> int:
                Path(args.report).write_text(
                    json.dumps(
                        {
                            "status": "failed",
                            "discovered": 1,
                            "processed": 0,
                            "skipped": 0,
                            "failed": 2,
                            "cases": [
                                {
                                    "status": "failed",
                                    "artifacts": {
                                        "publicAuthority": {"status": "equal"},
                                        "ledgerAuthority": {"status": "different"},
                                        "ledgerValidation": {"status": "valid"},
                                    },
                                }
                            ],
                        }
                    ),
                    encoding="utf-8",
                )
                return 1

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector.sys, "executable", str(candidate_binary)),
                mock.patch.object(collector, "run_inside_freecad", side_effect=fake_run_inside),
            ):
                result = collector.run_embedded(
                    [
                        str(fixture),
                        "--fixtures-root",
                        str(fixtures),
                        "--check",
                        "--validate-ledger",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                    ],
                    runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                )

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(1, result)
            self.assertEqual("failed", payload["status"])
            self.assertEqual(1, payload["unexplainedCollectorFailures"])
            self.assertFalse(payload["releaseGatePassed"])

    def test_complete_all_native_scope_can_publish_release_gate_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fixtures, _fixture, candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )

            def fake_run_inside(args: object) -> int:
                Path(args.report).write_text(
                    json.dumps(
                        {
                            "status": "passed",
                            "discovered": 1,
                            "processed": 1,
                            "skipped": 0,
                            "failed": 0,
                            "cases": [
                                {
                                    "status": "passed",
                                    "artifacts": {
                                        "publicAuthority": {"status": "equal"},
                                        "ledgerAuthority": {"status": "different"},
                                        "ledgerValidation": {"status": "valid"},
                                    },
                                }
                            ],
                        }
                    ),
                    encoding="utf-8",
                )
                return 1

            with (
                mock.patch.dict(sys.modules, {"FreeCAD": fake_freecad}),
                mock.patch.object(collector.sys, "executable", str(candidate_binary)),
                mock.patch.object(collector, "run_inside_freecad", side_effect=fake_run_inside),
            ):
                scoped_result = collector.run_embedded(
                    [
                        "--all-native",
                        "--fixtures-root",
                        str(fixtures),
                        "--check",
                        "--validate-ledger",
                        "--candidate-root",
                        str(candidate_root),
                        "--report",
                        str(report),
                    ],
                    runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                )
                scoped_payload = json.loads(report.read_text(encoding="utf-8"))
                with mock.patch.object(collector, "ROOT", root):
                    result = collector.run_embedded(
                        [
                            "--all-native",
                            "--fixtures-root",
                            str(fixtures),
                            "--check",
                            "--validate-ledger",
                            "--candidate-root",
                            str(candidate_root),
                            "--report",
                            str(report),
                        ],
                        runtime_receipt=self.runtime_receipt(candidate_binary, freecad_module),
                    )

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(0, scoped_result)
            self.assertEqual("scoped_passed", scoped_payload["cadFinalResultStatus"])
            self.assertFalse(scoped_payload["authorityRootMatched"])
            self.assertFalse(scoped_payload["releaseGatePassed"])
            self.assertEqual(0, result)
            self.assertEqual("passed", payload["cadFinalResultStatus"])
            self.assertEqual("passed", payload["ledgerValidationStatus"])
            self.assertTrue(payload["completeNativeManifest"])
            self.assertTrue(payload["releaseGatePassed"])

    def test_skip_or_document_leak_blocks_all_native_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _fixtures, _fixture, _candidate_root, report, candidate_binary = self.embedded_paths(root)
            freecad_module = root / "FreeCAD.so"
            freecad_module.write_bytes(b"freecad-module")
            fake_freecad = types.SimpleNamespace(
                __file__=str(freecad_module),
                Version=lambda: (1, 2, 3, "456 build"),
                listDocuments=lambda: {},
            )
            args = types.SimpleNamespace(
                report=str(report),
                all_native=True,
                phase=None,
                fixtures_root=str(_fixtures),
            )
            receipt = self.runtime_receipt(candidate_binary, freecad_module)

            for label, skipped, documents_after, ledger_status, aggregate_failed in (
                ("unexpected skip", 1, [], "valid", 1),
                ("document leak", 0, ["LeakedDocument"], "valid", 1),
                ("invalid ledger", 0, [], "invalid", 2),
            ):
                with self.subTest(label=label):
                    report.write_text(
                        json.dumps(
                            {
                                "status": "failed",
                                "discovered": 1,
                                "processed": 0,
                                "skipped": skipped,
                                "failed": aggregate_failed,
                                "cases": [
                                    {
                                        "status": "failed",
                                        "artifacts": {
                                            "publicAuthority": {"status": "equal"},
                                            "ledgerAuthority": {"status": "different"},
                                            "ledgerValidation": {"status": ledger_status},
                                        },
                                    }
                                ],
                            }
                        ),
                        encoding="utf-8",
                    )
                    payload = collector._annotate_embedded_report(
                        args,
                        candidate_binary=candidate_binary,
                        binding_artifact=freecad_module,
                        binding_mode="extension",
                        FreeCAD=fake_freecad,
                        runtime_receipt=receipt,
                        documents_before=[],
                        documents_after=documents_after,
                    )

                    self.assertIsNotNone(payload)
                    self.assertEqual("failed", payload["status"])
                    self.assertFalse(payload["releaseGatePassed"])
                    if label == "invalid ledger":
                        self.assertEqual(1, payload["failed"])
                        self.assertIsNotNone(payload["firstFailure"])


if __name__ == "__main__":
    unittest.main()
