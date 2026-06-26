from __future__ import annotations

import json

try:
    from .fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions, discover_expected_cases
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreExpectedFixtureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_c6m2_s5_pocket_without_base_matches_body_oracle(self) -> None:
        result = self.run_recompute("pocket-without-base", "p2")
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["status"], "ok")
        self.assertEqual(body["replayed_subtractive_features"], ["Pocket"])
        self.assert_object_matches_expected(result, "p2", "pocket-without-base")

    def test_c6m2_s4_geometry_bbox_rows_match_object_oracle(self) -> None:
        for group, fixture in (
            ("c3m1", "element-map-child-map-recursive-compound"),
            ("c4m4", "topo-reference-pressure-import-unchanged"),
            ("c5m1", "partdesign-revolution-profile-linked-face"),
            ("p8", "app-link-imported-element-map-chain"),
        ):
            with self.subTest(group=group, fixture=fixture):
                result = self.run_recompute(fixture, group)
                expected = self.expected_freecad(group, fixture)

                self.assert_expected_object(
                    result,
                    expected["object"],
                    expected,
                )

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

    def test_c6m7_loft_subelement_product_contract_metadata_matches_s3_boundaries(self) -> None:
        product = self.expected_freecad("c6m7", "part-loft-subelement-product")
        fields = product["object_fields"]

        self.assertEqual(product["oracle_evidence"]["owner_class"], "cad_core_product_contract")
        self.assertFalse(product["oracle_evidence"]["freecad_native_parity"])
        self.assertNotIn("known_gap", product)
        self.assertEqual(fields["feature"], "part_loft")
        self.assertEqual(fields["sections"], ["LowerProfile", "UpperEdge"])
        self.assertEqual(fields["contract"], "cad_core_product_contract")
        self.assertEqual(fields["contract_provenance"], "cad_core_product_contract_non_parity")
        self.assertFalse(fields["freecad_native_expected"])
        self.assertEqual(fields["sections_contract"]["freecad_native_property"], "App::PropertyLinkList")
        self.assertFalse(fields["sections_contract"]["native_parity"])
        self.assertEqual(fields["selected_sections"][0]["subname"], "Edge1")
        self.assertEqual(fields["selected_sections"][0]["stable_subname"], "Edge1")

        invalid = self.expected_freecad("c6m7", "part-loft-subelement-product-invalid")
        self.assertEqual(invalid["diagnostic_codes"], ["invalid_subshape"])
        self.assertEqual(invalid["oracle_evidence"]["owner_class"], "cad_core_product_contract_diagnostics")
        self.assertFalse(invalid["oracle_evidence"]["freecad_native_parity"])

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
        }
        for (group, fixture), (kind, probe_case, uncollected_field) in blockers.items():
            with self.subTest(group=group, fixture=fixture):
                known_gap = self.expected_freecad(group, fixture)["known_gap"]
                self.assertEqual(known_gap["kind"], kind)
                self.assertEqual(known_gap["freecadcmd_evidence"]["probe_case"], probe_case)
                self.assertIn(uncollected_field, known_gap["uncollected_fields"])
                if fixture == "part-geomplate-g1-curve-on-surface":
                    self.assertIn(
                        "6-22-05-28-c5m13-s4-geomplate-native-oracle-probe.py",
                        known_gap["freecadcmd_evidence"]["c5m13_s4_probe_script"],
                    )
                    self.assertTrue(known_gap["freecadcmd_evidence"]["c5m13_s4_variants"])

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

    def test_c5m13_geomplate_projected_curve2d_expected_metadata_matches_s4_boundaries(self) -> None:
        for group, fixture in (
            ("c5m7", "part-geomplate-projected-curve2d"),
            ("c5m13", "part-geomplate-projected-curve2d-initial-surface"),
        ):
            with self.subTest(group=group, fixture=fixture):
                expected = self.expected_freecad(group, fixture)
                self.assertNotIn("known_gap", expected)
                self.assertEqual(expected["object"], "GeomPlate")
                self.assertEqual(expected["object_fields"]["feature"], "part_geomplate_surface")
                self.assertEqual(expected["object_fields"]["helper"], "Part.GeomPlate.BuildPlateSurface")
                self.assertEqual(expected["object_fields"]["dto"], "PartGeomPlateSurfaceDTO")
                self.assertFalse(expected["object_fields"]["freecad_native_document_object"])
                self.assertEqual(expected["object_fields"]["curve_constraint_count"], 4)
                self.assertEqual(expected["object_fields"]["point_constraint_count"], 1)
                self.assertIn("bbox", expected)
                self.assertEqual(expected["topology_counts"]["faces"], 1)
                self.assertEqual(
                    expected["oracle_evidence"]["helper"],
                    "Part.GeomPlate.CurveConstraint.setProjectedCurve",
                )
                self.assertEqual(
                    expected["oracle_evidence"]["collectability"],
                    "expected_backed_with_initial_surface",
                )
                self.assertIn(
                    "range_0_4_tol_0p01_initial_surface",
                    expected["oracle_evidence"]["successful_probe_case"],
                )
                self.assertEqual(
                    expected["oracle_evidence"]["blocked_without_initial_surface"],
                    "RuntimeError: Geom_RectangularTrimmedSurface::V1==V2",
                )

        criteria = self.expected_freecad("c5m7", "part-geomplate-curve-criteria-diagnostic")
        self.assertIn("finite-number CurveConstraint criteria", criteria["c8m4_s6_evidence"]["runtime_result"])
        self.assertEqual(criteria["native_error_code"], "invalid_parameter")
        self.assertEqual(
            criteria["c8m4_s6_publication"]["status"],
            "request_local_curve_criteria_supported_invalid_type_diagnostic_retained",
        )
        self.assertFalse(criteria["c8m4_s6_publication"]["active_remaining_gap"])
        self.assertIn(
            "CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion",
            criteria["c8m4_s6_evidence"]["source_authority"],
        )
        wrapper = self.expected_freecad("c5m7", "part-geomplate-wrapper-boundary")
        self.assertIn("139/SIGSEGV", wrapper["c5m13_s4_evidence"]["runtime_result"])
        self.assertEqual(wrapper["native_error_code"], "unsupported_wrapper_lifecycle")
        self.assertEqual(
            wrapper["c6m6_s5_publication"]["status"],
            "published_c6m6_non_goal_boundary",
        )
        self.assertFalse(wrapper["c6m6_s5_publication"]["active_remaining_gap"])
        self.assertIn(
            "PlateSurfacePyImp.cpp::PlateSurfacePy::PyInit()",
            wrapper["c5m13_s4_evidence"]["source_authority"],
        )

    def test_c6m6_geomplate_s3_contract_expected_metadata_matches_blockers(self) -> None:
        g1 = self.expected_freecad("c6m6", "part-geomplate-g1-curve-on-surface-contract")
        g1_gap = g1["known_gap"]
        self.assertEqual(g1_gap["kind"], "geomplate_g1_curve_on_surface_native_oracle_blocked")
        self.assertEqual(g1_gap["cad_core_contract"]["status"], "request_local_source_backed")
        self.assertEqual(g1_gap["cad_core_contract"]["source_evidence_kind"], "curve_on_surface")
        self.assertFalse(g1_gap["cad_core_contract"]["expected_native_shape"])
        self.assertEqual(
            g1_gap["s5_publication"]["status"],
            "published_c6m6_product_contract_non_parity",
        )
        self.assertFalse(g1_gap["s5_publication"]["active_remaining_gap"])
        self.assertIn(
            "/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::Part::Tools::makeSurface()",
            g1_gap["source_authority"],
        )
        self.assertIn("Adaptor3d_CurveOnSurface G1", g1_gap["delete_condition"])
        self.assertIn("shape_summary for Adaptor3d_CurveOnSurface", g1_gap["uncollected_fields"][0])

        projected = self.expected_freecad("c6m6", "part-geomplate-projected-curve2d-no-initial-surface")
        projected_gap = projected["known_gap"]
        self.assertEqual(
            projected_gap["kind"],
            "geomplate_projected_curve2d_no_initial_surface_native_oracle_blocked",
        )
        self.assertEqual(
            projected_gap["cad_core_contract"]["status"],
            "request_local_source_backed_not_freecad_expected",
        )
        self.assertTrue(projected_gap["cad_core_contract"]["requires_initial_surface_for_native_expected"])
        self.assertFalse(projected_gap["cad_core_contract"]["expected_native_shape"])
        self.assertEqual(
            projected_gap["s5_publication"]["status"],
            "published_c6m6_product_contract_non_parity",
        )
        self.assertFalse(projected_gap["s5_publication"]["active_remaining_gap"])
        self.assertEqual(
            projected_gap["freecadcmd_evidence"]["error"],
            "RuntimeError: Geom_RectangularTrimmedSurface::V1==V2",
        )
        self.assertIn(
            "cad-core/fixtures/c5m13/part-geomplate-projected-curve2d-initial-surface.json",
            projected_gap["expected_backed_subsets"],
        )
        self.assertIn("no-InitialSurface ProjectedCurve2d", projected_gap["delete_condition"])

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
