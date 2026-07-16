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
SPEC = importlib.util.spec_from_file_location(
    "audit_freecad_fixture_authority",
    TOOLS / "audit_freecad_fixture_authority.py",
)
assert SPEC and SPEC.loader
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


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
            {"Objects": [{"Name": "Extrusion", "TypeId": "Part::Extrusion", "Properties": {}}]},
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


if __name__ == "__main__":
    unittest.main()
