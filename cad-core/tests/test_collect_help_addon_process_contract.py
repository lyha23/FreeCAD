"""Focused fail-closed checks for the A7 Help/AddonManager authority contract."""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[1]
TOOLS = CAD_CORE_ROOT / "tools"
PARITY = TOOLS / "freecad_expected_parity"
API_SURFACE = PARITY / "retained_public_api_surface.v1.json"
REPORT = PARITY / "reports" / "process_contract" / "help-addonmanager.v1.json"
MANIFEST = PARITY / "process_contracts" / "help_addonmanager" / "manifest.v1.json"
sys.path.insert(0, str(TOOLS))

from freecad_expected_parity import retained_coverage


NATIVE_CASES = {
    "help-local-page-headless",
    "help-browser-fallback-mocked",
    "addon-metadata-spdx-local",
    "addon-copy-install-update-remove",
    "addon-zip-and-network-manager-mocked",
    "addon-invalid-boundaries",
}


class HelpAddonManagerProcessContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.receipt = json.loads(REPORT.read_text(encoding="utf-8"))

    def validate(self, receipt: dict, required: set[str]) -> list[str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "help-addonmanager.v1.json").write_text(
                json.dumps(receipt, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            contract_path = root / "contract.json"
            contract_path.write_text("{}\n", encoding="utf-8")
            _receipt, _path, errors = retained_coverage._native_process_contract_receipt(
                contract_path,
                "process-contract/help-addonmanager",
                process_contract_root=root,
                required_case_ids=required,
            )
        return errors

    def test_repeat2_receipt_and_manifest_cover_every_executable_branch(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual("freecad-native-process-contract/v1", self.receipt["schema"])
        self.assertEqual("passed", self.receipt["status"])
        self.assertEqual("passed", self.receipt["repeatStatus"])
        self.assertEqual(2, self.receipt["repeat"])
        self.assertEqual(
            {case["id"] for case in manifest["cases"]},
            {case["id"] for case in self.receipt["cases"]},
        )
        self.assertEqual([], self.validate(self.receipt, NATIVE_CASES))

    def test_network_and_user_state_are_hermetic(self) -> None:
        self.assertEqual("127.0.0.1:9", self.receipt["environmentPolicy"]["networkDeny"]["proxy"])
        for case in self.receipt["cases"]:
            for run in case["runs"]:
                environment = run["process"]["environment"]
                self.assertEqual("http://127.0.0.1:9", environment["HTTP_PROXY"])
                self.assertIn("<CASE_ROOT>", environment["HOME"])
                self.assertEqual(0, run.get("result", {}).get("actual", {}).get("realNetworkRequests", 0))

    def test_gui_boundary_is_recorded_but_cannot_cover_an_executable_branch(self) -> None:
        case = next(
            case
            for case in self.receipt["cases"]
            if case["id"] == "help-gui-webengine-boundary-probe"
        )
        self.assertEqual("source_backed_exception", case["coverageOutcome"])
        self.assertTrue(all(run["result"]["actual"]["guiUp"] is False for run in case["runs"]))
        errors = self.validate(self.receipt, {case["id"]})
        self.assertTrue(any("not covered" in error for error in errors))

    def test_contract_fails_closed_on_missing_process_receipt_fields(self) -> None:
        receipt = copy.deepcopy(self.receipt)
        receipt["cases"][0]["runs"][0]["process"].pop("environment")
        self.assertTrue(self.validate(receipt, {receipt["cases"][0]["id"]}))

    def test_api_items_use_item_local_native_process_cases(self) -> None:
        surface = json.loads(API_SURFACE.read_text(encoding="utf-8"))
        retained = {
            item["id"]: item
            for item in surface["apis"]
            if item["id"].startswith(("help.", "addonmanager."))
        }
        self.assertEqual(4, len(retained))
        for item in retained.values():
            self.assertEqual("native_process_test", item["disposition"])
            self.assertTrue(item["fixtureEvidence"])
            for evidence in item["fixtureEvidence"]:
                self.assertEqual("process-contract/help-addonmanager", evidence["fixture"])
                self.assertEqual("native_process_test", evidence["level"])
                self.assertTrue(set(evidence["processCases"]) <= NATIVE_CASES)


if __name__ == "__main__":
    unittest.main()
