from __future__ import annotations

import json

from .fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreExpectedFixtureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
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
