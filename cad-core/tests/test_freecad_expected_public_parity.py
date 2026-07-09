from __future__ import annotations

import importlib.util
import json
import stat
import sys
import tempfile
import unittest
from pathlib import Path

try:
    from .fixture_runner import BIN, ROOT
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import BIN, ROOT


def load_tool(name: str):
    path = ROOT / "tools" / f"{name}.py"
    if str(path.parent) not in sys.path:
        sys.path.insert(0, str(path.parent))
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


compare_freecad_expected = load_tool("compare_freecad_expected")
regenerate_cad_core_res = load_tool("regenerate_cad_core_res")


S4_FAMILY_REPRESENTATIVE_PHASES = {
    "c3m1": "toponaming_elementmap",
    "c10m1": "sketch_internal_shape",
    "c12m12": "part_primitive_pipe",
    "c3m5": "partdesign_body_dressup",
    "c3m6": "assembly_placement_link",
}


class FreecadExpectedPublicParityTest(unittest.TestCase):
    def write_json(self, path: Path, payload: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    def test_expected_discovery_only_uses_freecad_json_under_expected_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(root / "fixtures" / "demo" / "case-a.json", {})
            self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.freecad.json", {})
            self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.freecad.ledger.json", {})
            self.write_json(root / "fixtures" / "demo" / "expected" / "case-a.expeted.json", {})
            self.write_json(root / "fixtures" / "demo" / "cad-core-res" / "extra.cad-core.json", {})
            self.write_json(root / "fixtures" / "other" / "expected" / "case-b.freecad.json", {})

            cases = compare_freecad_expected.discover_expected_cases(root, phase="demo")

            self.assertEqual([(item.phase, item.case) for item in cases], [("demo", "case-a")])
            self.assertEqual(cases[0].input_path, root / "fixtures" / "demo" / "case-a.json")
            self.assertEqual(
                cases[0].current_path,
                root / "fixtures" / "demo" / "cad-core-res" / "case-a.cad-core.json",
            )

    def test_raw_mapped_name_hash_is_canonicalized_but_semantic_keys_stay_strict(self) -> None:
        expected = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "indexed": "Face1",
                            "mappedName": {"raw": "Face1;:H65a,F", "canonical": "Face1;:H*,F"},
                            "rawFreecadMappedName": "Pad.#d:4;:G;XTR;:H542:7,F",
                            "stableSubname": "Face1;:H*,F",
                        }
                    ],
                }
            ],
        }
        actual = {
            "diagnostics": [],
            "results": [
                {
                    "object": "Box",
                    "subshapes": [
                        {
                            "indexed": "Face1",
                            "mappedName": {"raw": "Face1;:H89b,F", "canonical": "Face1;:H*,F"},
                            "rawFreecadMappedName": "Pad.#d:4;:G;XTR;:H9:1,F",
                            "stableSubname": "Face1;:H*,F",
                        }
                    ],
                }
            ],
        }
        self.assertEqual(compare_freecad_expected.compare_payloads(expected, actual), [])

        actual["results"][0]["subshapes"][0]["stableSubname"] = "Face2;:H*,F"
        diffs = compare_freecad_expected.compare_payloads(expected, actual)

        self.assertEqual(len(diffs), 1)
        self.assertEqual(diffs[0]["category"], "results.subshapes")
        self.assertIn("stableSubname", diffs[0]["path"])

    def test_regenerate_uses_expected_discovery_and_never_writes_expected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture = root / "fixtures" / "demo" / "case-a.json"
            expected = root / "fixtures" / "demo" / "expected" / "case-a.freecad.json"
            extra = root / "fixtures" / "demo" / "cad-core-res" / "extra.cad-core.json"
            binary = root / "build" / "cad-core"
            self.write_json(fixture, {"request": "demo"})
            self.write_json(expected, {"expected": "native"})
            self.write_json(extra, {"extra": True})
            binary.parent.mkdir(parents=True)
            binary.write_text(
                "#!/usr/bin/env python3\n"
                "import json, pathlib, sys\n"
                "out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
                "out.parent.mkdir(parents=True, exist_ok=True)\n"
                "out.write_text(json.dumps({'results': []}, indent=2) + '\\n')\n",
                encoding="utf-8",
            )
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)

            cases = regenerate_cad_core_res.discover_expected_cases(root, phase="demo")
            report = regenerate_cad_core_res.regenerate_cases(cases, root=root, bin_path=binary)

            self.assertEqual(report["status"], "ok")
            current = root / "fixtures" / "demo" / "cad-core-res" / "case-a.cad-core.json"
            self.assertTrue(current.exists())
            self.assertEqual(json.loads(expected.read_text(encoding="utf-8")), {"expected": "native"})
            self.assertEqual(json.loads(extra.read_text(encoding="utf-8")), {"extra": True})

    def test_c4m6_strict_report_is_green_or_registered_intentional_divergence(self) -> None:
        output = ROOT / "out" / "freecad-expected-parity" / "c4m6.unittest.json"
        report = compare_freecad_expected.run_strict_compare(ROOT, phase="c4m6", output=output)

        self.assertTrue(output.exists())
        self.assertEqual(report["schemaVersion"], "cad-core.freecad-expected-parity.v1")
        self.assertEqual(report["summary"]["cases"], 9)

        forbidden_decisions = {
            "hash_mismatch_policy",
            "mapper_history_publication_gap",
            "protocol_decision_required",
            "runtime_publication_gap",
            "stable_subname_diagnostic_policy",
        }
        self.assertTrue(
            forbidden_decisions.isdisjoint(report["summary"]["decisions"]),
            report["summary"]["decisions"],
        )
        for category in (
            "diagnostics",
            "geometry.numeric",
            "json",
            "topoNamingState.objects",
            "topoNamingState.subshapes",
            "topoNamingState.elementMap",
            "topoNamingState.childElementMaps",
            "topoNamingState.mapperHistory",
        ):
            self.assertEqual(report["summary"]["categories"][category], 0, category)

        case_statuses = {item["case"]: item["status"] for item in report["cases"]}
        if report["status"] == "green":
            self.assertEqual(report["summary"]["passed"], 9)
            self.assertEqual(report["summary"]["red"], 0)
            self.assertEqual(report["summary"]["decisions"], {})
        else:
            self.assertEqual(report["status"], "red")
            intentional_diff_count = sum(item["diffCount"] for item in report["cases"])
            self.assertEqual(
                report["summary"]["decisions"],
                {"intentional_protocol_divergence": intentional_diff_count},
            )
            self.assertLessEqual(
                {case for case, status in case_statuses.items() if status == "red"},
                {
                    "topo-state-body-tip-stable-recovery",
                    "topo-state-document-hash-mismatch",
                    "topo-state-first-recompute-empty",
                    "topo-state-link-compound-child-maps",
                    "topo-state-mapper-history-events",
                    "topo-state-object-hash-mismatch",
                    "topo-state-reference-shadow-brep",
                },
            )
        for category in compare_freecad_expected.REPORT_CATEGORIES:
            self.assertIn(category, report["summary"]["categories"])
        for case_report in report["cases"]:
            self.assertEqual(case_report["diffCount"], len(case_report["diffs"]))
            self.assertEqual(sum(case_report["categories"].values()), case_report["diffCount"])
            self.assertEqual(sum(case_report["decisions"].values()), case_report["diffCount"])
            if case_report["status"] == "red":
                self.assertGreater(case_report["diffCount"], 0)
                for diff in case_report["diffs"]:
                    self.assertEqual(diff["decision"], "intentional_protocol_divergence")
                    self.assertIn(diff["category"], {"results", "results.subshapes"})
                    for field in compare_freecad_expected.CLASSIFICATION_FIELDS:
                        self.assertIsInstance(diff.get(field), str)
                        self.assertNotEqual(diff[field], "")

    def test_s4_family_representative_reports_are_classified_without_anonymous_gaps(self) -> None:
        for phase, expected_decision_prefix in S4_FAMILY_REPRESENTATIVE_PHASES.items():
            with self.subTest(phase=phase):
                output = ROOT / "out" / "freecad-expected-parity" / f"{phase}.s4-family.unittest.json"
                report = compare_freecad_expected.run_strict_compare(ROOT, phase=phase, output=output)

                self.assertTrue(output.exists())
                self.assertEqual(report["schemaVersion"], "cad-core.freecad-expected-parity.v1")
                self.assertGreater(report["summary"]["cases"], 0)
                decisions = report["summary"]["decisions"]
                self.assertNotIn("unclassified_phase_gap", decisions)
                self.assertFalse(
                    any("unclassified" in decision or "anonymous" in decision for decision in decisions),
                    decisions,
                )
                if report["status"] != "green":
                    self.assertTrue(
                        any(decision.startswith(expected_decision_prefix) for decision in decisions),
                        decisions,
                    )

                for case_report in report["cases"]:
                    self.assertEqual(case_report["diffCount"], len(case_report["diffs"]))
                    self.assertEqual(sum(case_report["categories"].values()), case_report["diffCount"])
                    self.assertEqual(sum(case_report["decisions"].values()), case_report["diffCount"])
                    for diff in case_report["diffs"]:
                        for field in compare_freecad_expected.CLASSIFICATION_FIELDS:
                            self.assertIsInstance(diff.get(field), str)
                            self.assertNotEqual(diff[field], "")
                        self.assertNotIn("unclassified", diff["decision"])
                        self.assertNotIn("anonymous", diff["decision"])

    @unittest.skipUnless(BIN.exists(), "cad-core binary is missing; run cmake --build build first")
    def test_c4m6_write_current_entrypoint_generates_same_named_output(self) -> None:
        report = compare_freecad_expected.run_write_current(
            ROOT,
            phase="c4m6",
            case="topo-state-schema-incompatible",
        )

        self.assertEqual(report["status"], "ok")
        self.assertEqual(report["summary"]["written"], 1)
        self.assertTrue(
            (
                ROOT
                / "fixtures"
                / "c4m6"
                / "cad-core-res"
                / "topo-state-schema-incompatible.cad-core.json"
            ).exists()
        )


if __name__ == "__main__":
    unittest.main()
