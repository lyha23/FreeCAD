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
from freecad_expected_parity.catalog import ROLES_SCHEMA
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
