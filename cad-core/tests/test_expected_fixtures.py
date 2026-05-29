from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreExpectedFixtureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_expected_fixtures_match_recompute_results(self) -> None:
        for group, fixture, _expected_path in discover_expected_cases():
            with self.subTest(group=group, fixture=fixture):
                result = self.run_recompute(fixture, group)
                self.assertEqual(result["diagnostics"], [])
                self.assert_result_matches_expected(result, group, fixture)
