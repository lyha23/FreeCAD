from __future__ import annotations

import json

try:
    from .fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreExpectedFixtureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_c5m12_loft_complex_profile_expected_metadata_matches_s3_boundaries(self) -> None:
        expected_backed = {
            "part-loft-complex-wire-face": ["LowerWire", "MiddleFace", "UpperWire"],
            "part-loft-complex-vertex-sketch-object": ["BaseSketch", "TipVertex"],
        }
        for fixture, sections in expected_backed.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c5m12", fixture)
                self.assertNotIn("known_gap", expected)
                self.assertEqual(expected["object_fields"]["feature"], "part_loft")
                self.assertEqual(expected["object_fields"]["topo_naming_history"], "maker_history:loft_thru_sections")
                self.assertEqual(expected["object_fields"]["sections"], sections)
                self.assertIn("bbox", expected)
                self.assertIn("topology_counts", expected)

        diagnostic = self.expected_freecad("c5m12", "part-loft-subelement-assignment-diagnostic")
        known_gap = diagnostic["known_gap"]
        self.assertEqual(known_gap["kind"], "part_loft_subelement_assignment_native_hidden")
        self.assertEqual(known_gap["freecadcmd_evidence"]["property"], "Sections")
        self.assertIn("object_fields.sections[].subname", known_gap["uncollected_fields"])

    def test_c5m12_filling_geomplate_expected_metadata_matches_s4_boundaries(self) -> None:
        expected = self.expected_freecad("c5m12", "part-filling-non-boundary-edge-no-support-order")
        self.assertNotIn("known_gap", expected)
        self.assertEqual(expected["object_fields"]["feature"], "part_filled_face")
        self.assertEqual(expected["object_fields"]["topo_naming_history"], "maker_history:filling")
        self.assertEqual(expected["object_fields"]["non_boundary_constraint_count"], 1)
        self.assertEqual(
            expected["object_fields"]["non_boundary_constraints_status"],
            "freecad_expected_backed",
        )
        self.assertIn("bbox", expected)
        self.assertIn("topology_counts", expected)

        blockers = {
            ("c5m8", "part-filling-initial-surface-boundary"): (
                "part_filling_initial_surface_native_helper_oracle_blocked",
                "filling_surface_only",
                "shape_summary for Part.makeFilledFace(surface=face)",
            ),
            ("c5m8", "part-filling-support-order-edge-face"): (
                "part_filling_support_order_native_helper_oracle_blocked",
                "filling_support_order_g1",
                "shape_summary for Part.makeFilledFace(supports=..., orders=...)",
            ),
            ("c5m8", "part-filling-non-default-params"): (
                "part_filling_non_default_params_native_helper_oracle_blocked",
                "filling_params_all",
                "shape_summary for explicit BRepOffsetAPI_MakeFilling constructor params",
            ),
            ("c5m8", "part-filling-non-boundary-edge-support"): (
                "part_filling_non_boundary_edge_support_native_helper_oracle_blocked",
                "filling_nonboundary_support_order_g1",
                "shape_summary for non-boundary edge with support/order",
            ),
            ("c5m7", "part-geomplate-g1-curve-on-surface"): (
                "geomplate_g1_curve_on_surface_native_oracle_blocked",
                "geomplate_g1_curve_on_surface",
                "shape_summary for Adaptor3d_CurveOnSurface G1 helper result",
            ),
            ("c5m7", "part-geomplate-projected-curve2d"): (
                "geomplate_projected_curve2d_native_oracle_blocked",
                "geomplate_projected_curve2d",
                "shape_summary for ProjectedCurve2d helper result",
            ),
        }
        for (group, fixture), (kind, probe_case, uncollected_field) in blockers.items():
            with self.subTest(group=group, fixture=fixture):
                known_gap = self.expected_freecad(group, fixture)["known_gap"]
                self.assertEqual(known_gap["kind"], kind)
                self.assertEqual(known_gap["freecadcmd_evidence"]["probe_case"], probe_case)
                self.assertIn(uncollected_field, known_gap["uncollected_fields"])

        params_gap = self.expected_freecad("c5m8", "part-filling-non-default-params")["known_gap"]
        self.assertEqual(
            set(params_gap["c5m13_expected_backed_subsets"]),
            {"Degree", "NumIter", "Tol2d+Tol3d", "MaxDegree"},
        )
        self.assertEqual(
            {item["field"] for item in params_gap["blocked_param_subsets"]},
            {
                "PtsOnCurve",
                "Anisotropy",
                "TolG1+TolG2",
                "MaxSegments",
                "all explicit constructor params",
            },
        )
        self.assertEqual({item["shell_exit"] for item in params_gap["blocked_param_subsets"]}, {139})

    def test_c5m13_filling_param_expected_metadata_matches_s3_boundaries(self) -> None:
        expected_params = {
            "part-filling-param-degree-only": {"degree": 4},
            "part-filling-param-num-iter-only": {"iterations": 4},
            "part-filling-param-tol2d-tol3d-only": {
                "tolerance_2d": 0.00001,
                "tolerance_3d": 0.0001,
            },
            "part-filling-param-max-degree-only": {"max_degree": 9},
        }
        for fixture, param_subset in expected_params.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c5m13", fixture)
                self.assertNotIn("known_gap", expected)
                self.assertEqual(expected["object_fields"]["feature"], "part_filled_face")
                self.assertEqual(expected["object_fields"]["helper"], "Part.makeFilledFace")
                self.assertEqual(expected["object_fields"]["topo_naming_history"], "maker_history:filling")
                self.assertEqual(
                    expected["object_fields"]["params_source"],
                    "Part.makeFilledFace constructor kwargs",
                )
                self.assertIn("shape_summary", expected)
                self.assertIn("topology_counts", expected["shape_summary"])
                self.assertIn("params", expected["object_fields"])
                for key, value in param_subset.items():
                    self.assertEqual(expected["object_fields"]["params"][key], value)

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

        expected = self.expected_freecad("c5m12", "part-sweep-spine-support-surface-normal")
        self.assertNotIn("known_gap", expected)
        self.assertEqual(expected["object_fields"]["feature"], "part_sweep")
        self.assertEqual(expected["object_fields"]["topo_naming_history"], "maker_history:pipeshell")
        self.assertIn("shape_summary", expected)
        self.assertEqual(
            expected["object_fields"]["advanced"],
            {
                "mode": "SurfaceNormal",
                "spine_support": {
                    "set_mode_ok": True,
                    "subname": "Face1",
                    "target": "SupportPlane",
                },
                "support_mode": "SurfaceNormal",
            },
        )
        wrapper_oracle = expected["wrapper_oracle"]
        self.assertEqual(wrapper_oracle["dto"], "PartSweepAdvancedPipeShellDTO")
        self.assertFalse(wrapper_oracle["freecad_native_document_object"])
        self.assertTrue(wrapper_oracle["builder_status"]["set_spine_support"])
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
                evidence = known_gap["freecadcmd_evidence"]
                if fixture == "part-sweep-located-profile-contract":
                    self.assertEqual(evidence["failed_stage"], "build")
                    self.assertTrue(evidence["is_ready_before_build"])
                    self.assertEqual(evidence["status_before_build"], 0)
                    self.assertIn("6-22-04-46-c5m13-s2-sweep-location-combined-probe.py", evidence["probe_script"])
                    self.assertIn("located_profile_owned_vertex", evidence["failing_location_representatives"])
                    self.assertIn("located_free_vertex", evidence["failing_location_representatives"])
                    self.assertIn("located_add_before_transition", evidence["failing_call_order_variants"])
                    self.assertIn("plain_control", evidence["successful_controls"])
                if fixture == "part-sweep-advanced-combined-contract":
                    self.assertEqual(evidence["failed_stage"], "build")
                    self.assertTrue(evidence["is_ready_before_build"])
                    self.assertEqual(evidence["status_before_build"], 0)
                    self.assertIn("6-22-04-46-c5m13-s2-sweep-location-combined-probe.py", evidence["probe_script"])
                    self.assertEqual(
                        evidence["depends_on"],
                        "BRepOffsetAPI_MakePipeShell add(Profile, Location, WithContact, WithCorrection) overload",
                    )
                    self.assertIn("combined_tolerance_aux_add", evidence["failing_combined_call_orders"])
                    self.assertIn("combined_no_location_control", evidence["successful_controls"])

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
