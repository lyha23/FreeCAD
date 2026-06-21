from __future__ import annotations

import json

try:
    from .fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreExpectedFixtureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_c5m10_sweep_wrapper_expected_metadata_matches_s2_boundaries(self) -> None:
        expected_backed = {
            "part-sweep-auxiliary-spine-contract": {"auxiliary_spine", "mode"},
            "part-sweep-binormal-contract": {"binormal", "binormal_property", "mode"},
            "part-sweep-tolerance-contract": {"tolerance"},
        }
        for fixture, advanced_keys in expected_backed.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c5m10", fixture)
                self.assertNotIn("known_gap", expected)
                self.assertEqual(expected["object_fields"]["feature"], "part_sweep")
                self.assertEqual(expected["object_fields"]["topo_naming_history"], "maker_history:pipeshell")
                self.assertIn("shape_summary", expected)
                self.assertIn("topology_counts", expected["shape_summary"])
                self.assertTrue(advanced_keys <= set(expected["object_fields"]["advanced"]))

                wrapper_oracle = expected["wrapper_oracle"]
                self.assertEqual(wrapper_oracle["helper"], "Part.BRepOffsetAPI_MakePipeShell")
                self.assertEqual(wrapper_oracle["runtime_helper"], "Part.BRepOffsetAPI.MakePipeShell")
                self.assertEqual(wrapper_oracle["dto"], "PartSweepAdvancedPipeShellDTO")
                self.assertFalse(wrapper_oracle["freecad_native_document_object"])
                self.assertTrue(wrapper_oracle["builder_status"]["build_ok"])

        narrowed_blockers = {
            "part-sweep-support-mode-diagnostics": (
                "part_sweep_support_mode_fixture_diagnostic_only",
                "object_fields.advanced.spine_support",
            ),
            "part-sweep-located-profile-contract": (
                "part_sweep_located_profile_freecadcmd_wrapper_build_blocker",
                "object_fields.advanced.sections[].location",
            ),
            "part-sweep-advanced-combined-contract": (
                "part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker",
                "object_fields.advanced.auxiliary_spine + sections.location + tolerance combined payload",
            ),
        }
        for fixture, (kind, uncollected_field) in narrowed_blockers.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c5m10", fixture)
                known_gap = expected["known_gap"]
                self.assertEqual(known_gap["kind"], kind)
                self.assertNotEqual(kind, "part_sweep_wrapper_expected_collector")
                self.assertEqual(
                    known_gap["freecadcmd_evidence"]["helper"],
                    "Part.BRepOffsetAPI_MakePipeShell",
                )
                self.assertIn(uncollected_field, known_gap["uncollected_fields"])

    def test_expected_fixtures_match_recompute_results(self) -> None:
        for group, fixture, expected_path in discover_expected_cases():
            with self.subTest(group=group, fixture=fixture):
                expected = json.loads(expected_path.read_text(encoding="utf-8"))
                if "known_gap" in expected:
                    self.skipTest(f"{group}/{fixture}: {expected['known_gap']}")
                result = self.run_recompute(fixture, group)
                self.assertEqual(
                    [diagnostic["code"] for diagnostic in result["diagnostics"]],
                    expected.get("diagnostic_codes", []),
                )
                self.assert_result_matches_expected(result, group, fixture)
