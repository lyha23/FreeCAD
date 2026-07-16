"""Behavior tests for transactional FreeCAD fixture authority promotion."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "promote_freecad_fixture_authority",
    TOOLS / "promote_freecad_fixture_authority.py",
)
assert SPEC and SPEC.loader
promotion = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(promotion)


class FixtureAuthorityPromotionTests(unittest.TestCase):
    def write_json(self, path: Path, payload: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def stage_case(self, root: Path) -> dict[str, Path]:
        staging_root = root / "out" / "fixture-staging"
        fixtures = staging_root / "fixtures"
        fixture = fixtures / "p1" / "case.json"
        expected = fixtures / "p1" / "expected" / "case.freecad.json"
        ledger = fixtures / "p1" / "expected" / "case.freecad.ledger.json"
        run_a = staging_root / "candidates" / "p1" / "case" / "run-a"
        run_b = staging_root / "candidates" / "p1" / "case" / "run-b"
        run_a_expected = run_a / "p1" / "expected" / "case.freecad.json"
        run_b_expected = run_b / "p1" / "expected" / "case.freecad.json"
        run_a_ledger = run_a / "p1" / "expected" / "case.freecad.ledger.json"
        run_b_ledger = run_b / "p1" / "expected" / "case.freecad.ledger.json"
        collect_report = staging_root / "reports" / "p1" / "case" / "collect.json"
        repeat_report = staging_root / "reports" / "p1" / "case" / "repeat2.json"
        self.write_json(
            fixture,
            {"Objects": [{"Name": "Box", "TypeId": "Part::Box", "Properties": {}}]},
        )
        for path, payload in (
            (expected, {"object": "Box"}),
            (ledger, {"outcome": "accepted"}),
            (run_a_expected, {"object": "Box"}),
            (run_b_expected, {"object": "Box"}),
            (run_a_ledger, {"outcome": "accepted"}),
            (run_b_ledger, {"outcome": "accepted"}),
        ):
            self.write_json(path, payload)
        producer = {
            "path": "/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd",
            "sha256": "producer-sha",
        }
        collector = {"path": str(TOOLS / "collect_freecad_expected.py"), "sha256": "collector-sha"}
        runtime = {"freecadVersion": "1.2.0 revision test", "occtVersion": "7.8.1"}
        self.write_json(
            collect_report,
            {
                "status": "passed",
                "processed": 1,
                "failed": 0,
                "ledgerValidationStatus": "passed",
                "producerTraceStatus": "not_evaluated",
                "candidate": producer,
                "collector": collector,
                "runtime": runtime,
                "cases": [
                    {
                        "phase": "p1",
                        "case": "case",
                        "status": "passed",
                        "fixtureSha256": promotion.file_sha256(fixture),
                        "publicExpectedSha256": promotion.file_sha256(expected),
                        "ledgerSha256": promotion.file_sha256(ledger),
                        "ledgerOutcome": "accepted",
                    }
                ],
            },
        )
        self.write_json(
            repeat_report,
            {
                "mode": "repeated-staging-collection",
                "status": "passed",
                "publicExpectedStatus": "passed",
                "ledgerValidationStatus": "passed",
                "producerTraceStatus": "not_evaluated",
                "selectedCaseCount": 1,
                "executedCaseCount": 1,
                "runCount": 2,
                "successfulRunCount": 2,
                "candidateRoot": str((staging_root / "candidates" / "p1" / "case").resolve()),
                "candidate": producer,
                "collector": collector,
                "runtimeIdentities": [[runtime["freecadVersion"], runtime["occtVersion"]]],
                "manifest": {
                    "cases": 1,
                    "entries": [
                        {
                            "phase": "p1",
                            "case": "case",
                            "fixtureSha256": promotion.file_sha256(fixture),
                            "publicSha256": promotion.file_sha256(run_a_expected),
                            "ledgerSha256": promotion.file_sha256(run_a_ledger),
                        }
                    ],
                },
                "candidateRunDifferences": [],
            },
        )
        return {
            "fixture": fixture,
            "expected": expected,
            "ledger": ledger,
            "collectReport": collect_report,
            "repeatReport": repeat_report,
        }

    def test_promotes_five_authority_artifacts_and_role_together(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            staged = self.stage_case(root)
            fixtures_root = root / "fixtures"
            roles_path = root / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"
            reports_root = root / "tools" / "freecad_expected_parity" / "reports"
            self.write_json(
                roles_path,
                {
                    "schemaVersion": "cad-core.freecad-expected-fixture-roles.v1",
                    "legacyNativeExpectedDiscovery": False,
                    "requireCompleteInputCoverage": True,
                    "roles": [
                        {
                            "phase": "p1",
                            "case": "case",
                            "role": "unsupported",
                            "reason": "not promoted",
                        }
                    ],
                },
            )

            receipt = promotion.promote_fixture_authority(
                staging_fixture=staged["fixture"],
                fixtures_root=fixtures_root,
                roles_path=roles_path,
                reports_root=reports_root,
                collect_report_path=staged["collectReport"],
                repeat_report_path=staged["repeatReport"],
            )

            promoted_fixture = fixtures_root / "p1" / "case.json"
            promoted_expected = fixtures_root / "p1" / "expected" / "case.freecad.json"
            promoted_ledger = fixtures_root / "p1" / "expected" / "case.freecad.ledger.json"
            producer_report = reports_root / "promotions" / "p1-case.json"
            roles = json.loads(roles_path.read_text(encoding="utf-8"))
            promoted_exists = [
                path.is_file()
                for path in (
                    promoted_fixture,
                    promoted_expected,
                    promoted_ledger,
                    producer_report,
                )
            ]

        self.assertEqual("passed", receipt["status"])
        self.assertEqual(
            ["input", "publicExpected", "ledger", "producerReport", "roleManifest"],
            [item["kind"] for item in receipt["promotedArtifacts"]],
        )
        self.assertEqual([True, True, True, True], promoted_exists)
        self.assertEqual("native", roles["roles"][0]["role"])

    def test_rolls_back_every_written_artifact_when_commit_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            staged = self.stage_case(root)
            fixtures_root = root / "fixtures"
            roles_path = root / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"
            reports_root = root / "tools" / "freecad_expected_parity" / "reports"
            self.write_json(
                roles_path,
                {
                    "schemaVersion": "cad-core.freecad-expected-fixture-roles.v1",
                    "roles": [
                        {"phase": "p1", "case": "case", "role": "unsupported"}
                    ],
                },
            )
            real_replace = promotion.os.replace
            replace_calls = 0

            def fail_fourth_replace(source: Path, target: Path) -> None:
                nonlocal replace_calls
                replace_calls += 1
                if replace_calls == 4:
                    raise OSError("simulated producer report commit failure")
                real_replace(source, target)

            with mock.patch.object(
                promotion.os,
                "replace",
                side_effect=fail_fourth_replace,
            ):
                with self.assertRaisesRegex(OSError, "simulated producer report"):
                    promotion.promote_fixture_authority(
                        staging_fixture=staged["fixture"],
                        fixtures_root=fixtures_root,
                        roles_path=roles_path,
                        reports_root=reports_root,
                        collect_report_path=staged["collectReport"],
                        repeat_report_path=staged["repeatReport"],
                    )

            authority_paths = [
                fixtures_root / "p1" / "case.json",
                fixtures_root / "p1" / "expected" / "case.freecad.json",
                fixtures_root / "p1" / "expected" / "case.freecad.ledger.json",
                reports_root / "promotions" / "p1-case.json",
            ]
            roles = json.loads(roles_path.read_text(encoding="utf-8"))
            lock_exists = (reports_root / ".fixture-authority-promotion.lock").exists()
            authority_exists = [path.exists() for path in authority_paths]

        self.assertEqual([False, False, False, False], authority_exists)
        self.assertEqual("unsupported", roles["roles"][0]["role"])
        self.assertFalse(lock_exists)

    def test_rejects_ledger_changed_after_collection_without_writing_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            staged = self.stage_case(root)
            fixtures_root = root / "fixtures"
            roles_path = root / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"
            reports_root = root / "tools" / "freecad_expected_parity" / "reports"
            self.write_json(
                roles_path,
                {
                    "schemaVersion": "cad-core.freecad-expected-fixture-roles.v1",
                    "roles": [
                        {"phase": "p1", "case": "case", "role": "unsupported"}
                    ],
                },
            )
            self.write_json(
                staged["ledger"],
                {"outcome": "accepted", "tamperedAfterCollection": True},
            )

            with self.assertRaisesRegex(ValueError, "staging collect ledger hash"):
                promotion.promote_fixture_authority(
                    staging_fixture=staged["fixture"],
                    fixtures_root=fixtures_root,
                    roles_path=roles_path,
                    reports_root=reports_root,
                    collect_report_path=staged["collectReport"],
                    repeat_report_path=staged["repeatReport"],
                )

            authority_exists = [
                path.exists()
                for path in (
                    fixtures_root / "p1" / "case.json",
                    fixtures_root / "p1" / "expected" / "case.freecad.json",
                    fixtures_root / "p1" / "expected" / "case.freecad.ledger.json",
                    reports_root / "promotions" / "p1-case.json",
                )
            ]
            roles = json.loads(roles_path.read_text(encoding="utf-8"))

        self.assertEqual([False, False, False, False], authority_exists)
        self.assertEqual("unsupported", roles["roles"][0]["role"])

    def test_revokes_promoted_authority_when_checked_in_repeat_is_not_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            staged = self.stage_case(root)
            fixtures_root = root / "fixtures"
            roles_path = root / "tools" / "freecad_expected_parity" / "fixture_roles.v1.json"
            reports_root = root / "tools" / "freecad_expected_parity" / "reports"
            self.write_json(
                roles_path,
                {
                    "schemaVersion": "cad-core.freecad-expected-fixture-roles.v1",
                    "roles": [
                        {"phase": "p1", "case": "case", "role": "unsupported"}
                    ],
                },
            )
            promotion.promote_fixture_authority(
                staging_fixture=staged["fixture"],
                fixtures_root=fixtures_root,
                roles_path=roles_path,
                reports_root=reports_root,
                collect_report_path=staged["collectReport"],
                repeat_report_path=staged["repeatReport"],
            )
            promotion_report = reports_root / "promotions" / "p1-case.json"
            post_repeat_report = reports_root / "promotions" / "p1-case-post-repeat2.json"
            self.write_json(
                post_repeat_report,
                {
                    "status": "failed",
                    "publicExpectedStatus": "failed",
                    "candidateRunDifferences": [],
                    "firstFailure": {
                        "kind": "checked-in-regression",
                        "caseResult": {"phase": "p1", "case": "case"},
                    },
                },
            )

            receipt = promotion.revoke_fixture_authority(
                promotion_report_path=promotion_report,
                post_repeat_report_path=post_repeat_report,
                roles_path=roles_path,
                reports_root=reports_root,
            )

            roles = json.loads(roles_path.read_text(encoding="utf-8"))
            authority_exists = [
                path.exists()
                for path in (
                    fixtures_root / "p1" / "case.json",
                    fixtures_root / "p1" / "expected" / "case.freecad.json",
                    fixtures_root / "p1" / "expected" / "case.freecad.ledger.json",
                    promotion_report,
                )
            ]
            revocation_report = reports_root / "revocations" / "p1-case.json"
            revocation_exists = revocation_report.is_file()

        self.assertEqual("passed", receipt["status"])
        self.assertEqual([True, False, False, False], authority_exists)
        self.assertEqual("unsupported", roles["roles"][0]["role"])
        self.assertLessEqual(
            {"reason", "authority", "nextAction", "closeCondition"},
            roles["roles"][0].keys(),
        )
        self.assertTrue(revocation_exists)


if __name__ == "__main__":
    unittest.main()
