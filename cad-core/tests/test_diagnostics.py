from __future__ import annotations

try:
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreDiagnosticsTest(CadCoreFixtureTestCase):
    def test_fixture_diagnostics(self) -> None:
        expected = {
            "empty": [],
            "unknown-type": ["unsupported_type"],
            "duplicate-name": ["duplicate_object_name"],
            "duplicate-id": ["duplicate_object_id"],
            "legacy-lowercase": ["parse_error"],
            "missing-profile": ["missing_property"],
            "missing-link": ["missing_link_target"],
            "missing-target": ["missing_object"],
            "cycle-dependency": ["cycle_dependency"],
            "unsupported-geometry": ["unsupported_geometry"],
            "invalid-length": ["invalid_length"],
            "unsupported-property": ["unsupported_property"],
            "open-sketch": ["open_profile"],
            "rect-pad": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture), codes)

    def test_p2_fixture_diagnostics(self) -> None:
        expected = {
            "body-basefeature-pad": [],
            "rect-pad-pocket": [],
            "missing-basefeature": ["missing_link_target"],
            "pocket-without-base": [],
            "pocket-open-sketch": ["open_profile"],
            "unsupported-pocket-type": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p2"), codes)

    def test_reference_lifecycle_matrix(self) -> None:
        diagnostic_cases = {
            ("mvp", "rect-pad"): [],
            ("mvp", "missing-link"): ["missing_link_target"],
            ("c3m2", "sketch-external-frozen-brep-reuse"): [],
            ("c3m2", "sketch-external-frozen-missing-snapshot"): ["missing_external_geometry_snapshot"],
            ("c3m2", "sketch-external-missing-brep-reuse"): [],
            ("c3m2", "sketch-external-missing-missing-snapshot"): ["missing_external_geometry_snapshot"],
            ("c3m2", "xlink-missing-external-document"): ["missing_external_document"],
            ("c3m2", "xlink-pending-external-document"): ["external_document_pending_reload"],
            ("c3m2", "xlink-unloaded-external-document"): ["external_document_unloaded"],
            ("c8m1", "subshape-binder-setlinks-normalization-diagnostics"): [
                "cycle_rejected_by_property_link"
            ],
        }
        for (group, fixture), codes in diagnostic_cases.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, group), codes)

        frozen_native = self.run_recompute("sketch-external-frozen-source-changed", "p5")
        self.assertEqual(frozen_native["diagnostics"], [])
        self.assertEqual(frozen_native["documentObjectUpdates"], [])
        self.assertEqual(
            frozen_native["objects"]["Sketch"]["external_geometry_state_counts"]["frozen"],
            1,
        )

        detached = self.run_recompute("sketch-external-detached-source-changed", "p5")
        self.assertEqual(detached["diagnostics"], [])
        self.assertEqual([item["reason"] for item in detached["documentObjectUpdates"]], ["external_geometry_detach"])

        missing_recovered = self.run_recompute("sketch-external-missing-fix", "c3m2")
        self.assertEqual(missing_recovered["diagnostics"], [])
        self.assertEqual(
            [item["reason"] for item in missing_recovered["documentObjectUpdates"]],
            ["external_geometry_flags_sync"],
        )

        label_rename = self.run_recompute("label-rename-recovery", "c3m2")
        self.assertEqual(label_rename["diagnostics"], [])
        self.assertIn("labelReferenceRename", label_rename["elementReferenceUpdates"][0])

        document_rename = self.run_recompute("xlink-document-hash-mismatch", "c3m2")
        self.assertEqual([item["code"] for item in document_rename["diagnostics"]], ["document_hash_mismatch"])
        self.assertIn("documentReference", document_rename["elementReferenceUpdates"][0])

    def test_p3a_fixture_diagnostics(self) -> None:
        expected = {
            "pocket-through-all": [],
            "pocket-through-all-without-base": ["execution_failed"],
            "pocket-up-to-face": [],
            "pocket-up-to-face-parallel": ["execution_failed"],
            "pocket-up-to-face-intersects-sketch": ["execution_failed"],
            "up-to-face-missing-target": ["missing_link_target"],
            "up-to-face-missing-subshape": ["invalid_subshape"],
            "up-to-face-edge-subshape": ["unsupported_subshape_kind"],
            "pocket-up-to-shape-solid": [],
            "pocket-up-to-shape-face": [],
            "pocket-up-to-shape-multi-face": [],
            "pad-up-to-shape-multi-face": [],
            "pocket-up-to-shape-multiple-faces-offset": ["unsupported_property"],
            "pocket-up-to-shape-edge-subshape": ["unsupported_subshape_kind"],
            "pocket-up-to-shape-empty": ["invalid_subshape"],
            "pad-up-to-face": [],
            "pad-through-all-unsupported": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p3a"), codes)

    def test_p3b_fixture_diagnostics(self) -> None:
        expected = {
            "pad-two-sides-length": [],
            "pad-two-sides-up-to-face1": [],
            "pad-two-sides-up-to-face2": [],
            "pad-two-sides-up-to-shape1": [],
            "pad-two-sides-up-to-shape2": [],
            "pad-up-to-first": [],
            "pad-up-to-last": [],
            "pocket-two-sides-length": [],
            "pad-symmetric-length": [],
            "pad-symmetric-taper": [],
            "pocket-symmetric-length": [],
            "pad-custom-vector": [],
            "pocket-custom-vector": [],
            "pad-reference-axis": [],
            "pad-reference-axis-edge": [],
            "pad-sketch-placement": [],
            "pad-custom-direction-placement": [],
            "pad-custom-direction-sketch-rotation": [],
            "pocket-body-placement": [],
            "body-basefeature-placement": [],
            "pad-invalid-direction": ["invalid_direction"],
            "pad-reference-axis-parallel": ["invalid_direction"],
            "pad-reference-axis-missing-target": ["missing_link_target"],
            "pad-symmetric-up-to-unsupported": ["unsupported_property"],
            "pad-length-taper": [],
            "pad-length-taper-inner-wire": [],
            "pocket-length-taper": [],
            "pad-two-sides-taper": [],
            "pocket-invalid-taper": ["invalid_taper"],
            "pad-two-sides-up-to-face2-missing-target": ["missing_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p3b"), codes)

    def test_p4_fixture_diagnostics(self) -> None:
        expected = {
            "body-link-list": [],
            "feature-link-sub-list": [],
            "missing-link-target": ["missing_link_target"],
            "cycle-link-sub": ["cycle_dependency"],
            "invalid-link-value": ["invalid_link_value"],
            "invalid-link-list-conflict": ["invalid_link_value"],
            "invalid-link-list-value": ["invalid_link_value"],
            "invalid-link-list-values-type": ["invalid_link_value"],
            "invalid-link-sub-list-conflict": ["invalid_link_value"],
            "invalid-link-sub-list-value": ["invalid_link_value"],
            "invalid-link-sub-list-subset-type": ["invalid_link_value"],
            "invalid-link-sub-list-nested-property-type": ["invalid_link_value"],
            "invalid-link-sub-stable-length": ["invalid_link_value"],
            "invalid-link-sub-list-stable-length": ["invalid_link_value"],
            "invalid-link-sub-full-sublist": ["invalid_link_value"],
            "part-placement-body": [],
            "sketch-placement-pocket": [],
            "typed-property-pad": [],
            "invalid-placement": ["invalid_placement"],
            "invalid-typed-property": ["invalid_property_type"],
            "datum-plane-support": [],
            "datum-line-reference-axis": [],
            "datum-point-part-placement": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p4"), codes)

    def test_p5_fixture_diagnostics(self) -> None:
        expected = {
            "sketch-angle-constraints-profile": [],
            "sketch-angle-pointwise-constraints-profile": [],
            "sketch-arc-ellipse-profile": [],
            "sketch-arc-profile": [],
            "sketch-block-constraints-profile": [],
            "sketch-bspline-profile": [],
            "sketch-circle-profile": [],
            "sketch-coincident-profile": [],
            "sketch-conic-arcs-construction-filter": [],
            "sketch-conic-arcs-external-geometry-native": [],
            "sketch-conic-arcs-external-geometry-projected": [],
            "sketch-construction-ignored": [],
            "sketch-coordinate-constraints-profile": [],
            "sketch-diameter-constraints-profile": [],
            "sketch-dimensional-constraints-profile": [],
            "sketch-ellipse-profile": [],
            "sketch-equal-constraints-profile": [],
            "sketch-external-circle-edge": [],
            "sketch-external-circle-edge-as-line": [],
            "sketch-external-edge": [],
            "sketch-external-ellipse-edge": [],
            "sketch-external-face-both": [],
            "sketch-external-face": [],
            "sketch-external-face-intersection": [],
            "sketch-external-face-normal": [],
            "sketch-external-face-unsupported": ["invalid_subshape"],
            "sketch-external-whole-box": [],
            "sketch-external-internal-edge": [],
            "sketch-external-internal-vertex": [],
            "sketch-external-tilted-ellipse-edge": [],
            "sketch-external-tilted-circle-edge": [],
            "sketch-external-vertex": [],
            "sketch-hyperbola-arc-profile": [],
            "sketch-horizontal-vertical-profile": [],
            "sketch-invalid-conic-arc-params": ["unsupported_geometry"],
            "sketch-internal-face": [],
            "sketch-line-relation-constraints-profile": [],
            "sketch-missing-external": ["missing_link_target"],
            "sketch-open-wire-internal-empty": [],
            "sketch-parabola-arc-profile": [],
            "sketch-perpendicular-curve-constraints-profile": [],
            "sketch-perpendicular-pointwise-constraints-profile": [],
            "sketch-point-on-object-constraints-profile": [],
            "sketch-point-pair-constraints-profile": [],
            "sketch-rect-circle-island": [],
            "sketch-rect-circle-hole": [],
            "sketch-symmetric-constraints-profile": [],
            "sketch-tangent-constraints-profile": [],
            "sketch-unsupported-constraint": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p5"), codes)

    def test_p6_fixture_diagnostics(self) -> None:
        expected = {
            "body-additive-fuse-history": [],
            "body-boolean-history": [],
            "body-split-history": [],
            "named-shape-indexed-pad": [],
            "sketch-external-edge-stable-body-deleted": ["deleted_stable_subname"],
            "sketch-external-edge-stable-body-deleted-after-add": ["deleted_stable_subname"],
            "sketch-external-edge-stable-body-preserved": [],
            "sketch-external-edge-stable-body-profile-source": [],
            "sketch-external-edge-stable-body-split": [],
            "sketch-external-edge-stable-body-split-after-add": [],
            "sketch-external-edge-stable-body-split-current-sublist": [],
            "sketch-external-edge-stable-indexed-opaque-sublist": [],
            "sketch-external-edge-stable-multi-prism": [],
            "sketch-external-edge-stable-taper-preserved": [],
            "up-to-face-stable-body-deleted": ["deleted_stable_subname"],
            "up-to-face-stable-body-history": [],
            "up-to-face-stable-body-preserved": [],
            "up-to-face-stable-body-split": ["execution_failed"],
            "up-to-face-stable-indexed-opaque-sublist": [],
            "up-to-face-stable-indexed-reference": [],
            "up-to-face-stable-subname-known-gap": ["unsupported_stable_subname"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p6"), codes)

    def test_p7_fixture_diagnostics(self) -> None:
        expected = {
            "datum-coordinate-system-invalid-axis": ["unsupported_subshape_kind"],
            "datum-coordinate-system-reference-axis": [],
            "datum-coordinate-system-sketch-support": [],
            "chamfer-invalid-size": ["invalid_length"],
            "chamfer-pad-edge": [],
            "chamfer-refine-true": [],
            "fillet-missing-edge": ["invalid_subshape"],
            "fillet-pad-edge": [],
            "fillet-refine-true": [],
            "hole-angled-drill-point": [],
            "hole-blind-depth": [],
            "hole-counterbore": [],
            "hole-counterdrill": [],
            "hole-countersink": [],
            "hole-isotyre-clearance-fallback": [],
            "hole-point-profile": [],
            "hole-refine-true": [],
            "hole-tapered": [],
            "hole-thread-class-clearance": [],
            "hole-thread-custom-clearance": [],
            "hole-thread-depth-dimension-clamped": [],
            "hole-thread-depth-din76": [],
            "hole-thread-clearance": [],
            "hole-model-thread-metric": [],
            "hole-threaded-cosmetic": [],
            "hole-threaded-dynamic-din7984": [],
            "hole-threaded-dynamic-iso2009": [],
            "hole-threaded-bsf-cosmetic": [],
            "hole-threaded-bsp-fallback-cosmetic": [],
            "hole-threaded-bsw-cosmetic": [],
            "hole-threaded-fine-cosmetic": [],
            "hole-threaded-isotyre-cosmetic": [],
            "hole-threaded-known-gap": ["execution_failed"],
            "hole-threaded-npt-cosmetic": [],
            "hole-threaded-standard-counterbore": [],
            "hole-threaded-standard-countersink": [],
            "hole-threaded-unef-cosmetic": [],
            "hole-threaded-unf-cosmetic": [],
            "hole-threaded-unc-cosmetic": [],
            "hole-through-all": [],
            "hole-unc-clearance": [],
            "hole-without-base": ["execution_failed"],
            "linear-pattern-custom-spacings": [],
            "linear-pattern-pad-datum-line": [],
            "linear-pattern-pad-pocket-multi-original": [],
            "linear-pattern-pad-sketch-axis": [],
            "linear-pattern-pad-two-directions": [],
            "linear-pattern-pocket-subtractive-original": [],
            "linear-pattern-spacing-pattern": [],
            "linear-pattern-whole-shape-body-prefix-support": [],
            "linear-pattern-whole-shape-refined-prefix-support": [],
            "linear-pattern-whole-shape": [],
            "mirrored-dressup-chain-support-transform": [],
            "mirrored-pad-datum-plane": [],
            "mirrored-fillet-support-transform": [],
            "mirrored-refine-true": [],
            "mirrored-stable-history-deleted": ["deleted_stable_subname"],
            "mirrored-stable-history-split": ["split_stable_subname"],
            "mirrored-whole-shape": [],
            "multi-transform-linear-mirror": [],
            "multi-transform-scaled-diagonal": [],
            "multi-transform-scaled-divisor-known-gap": ["invalid_length"],
            "multi-transform-whole-shape": [],
            "origin-identity-placement": [],
            "pad-refine-false": [],
            "pad-refine-true": [],
            "pocket-refine-true": [],
            "polar-pattern-pad-datum-line": [],
            "polar-pattern-pad-sketch-axis": [],
            "polar-pattern-spacing-pattern": [],
            "polar-pattern-whole-shape": [],
            "scaled-invalid-factor": ["invalid_length"],
            "scaled-pad-factor-two": [],
            "scaled-whole-shape": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p7"), codes)

    def test_p8_fixture_diagnostics(self) -> None:
        expected = {
            "app-link-box": [],
            "app-link-box-face": [],
            "app-link-box-multi-face": [],
            "app-link-box-missing-subshape": ["invalid_subshape"],
            "app-link-box-scale": [],
            "app-link-box-transform": [],
            "app-link-element-box": [],
            "app-link-element-count-collapsed": [],
            "app-link-element-count-hidden-sublist-index": [],
            "app-link-element-count-sublist-index": [],
            "app-link-element-count-sublist-target-label": [],
            "app-link-element-list-hidden-sublist-label": [],
            "app-link-element-list-sublist-index": [],
            "app-link-element-list-sublist-label": [],
            "app-link-group-elements": [],
            "app-link-group-subshape-alias": [],
            "app-link-group-visibility": [],
            "app-link-label-qualified-sublist": [],
            "app-link-missing": ["missing_link_target"],
            "app-link-nested-object-qualified-sublist": [],
            "app-link-show-element-inherited-child": [],
            "app-link-show-element-inherited-placement-list": [],
            "app-link-show-element-materialized": [],
            "app-link-show-element-synthetic": [],
            "app-link-stable-history-deleted": ["deleted_stable_subname"],
            "app-link-stable-history-split": ["split_stable_subname"],
            "assembly-grounded-only-solver-success": [],
            "assembly-joint-hidden-reference-diagnostics": [],
            "assembly-joint-group-diagnostics": [],
            "assembly-link-basic": [],
            "part-boolean-fragments": [],
            "part-boolean-fragments-compsolid": [],
            "part-boolean-fragments-compsolid-split": [],
            "part-boolean-fragments-shell-split": [],
            "part-boolean-fragments-split": [],
            "part-boolean-fragments-wire-split": [],
            "mesh-import-stl": [],
            "mesh-import-stl-missing": ["execution_failed"],
            "part-box": [],
            "part-common": [],
            "part-cone": [],
            "part-conic-edge-extrusion": [],
            "part-conic-edge-invalid-params": [
                "invalid_part_conic_curve_kind",
                "invalid_part_conic_radius",
                "invalid_part_conic_focal",
                "invalid_part_conic_trim",
                "invalid_part_conic_number",
            ],
            "part-cut": [],
            "part-cylinder": [],
            "part-cylinder-angled-prism": [],
            "part-ellipse": [],
            "part-ellipsoid": [],
            "part-fuse": [],
            "part-helix": [],
            "part-hyperbola-edge": [],
            "part-import-brep": [],
            "part-import-brep-missing": ["execution_failed"],
            "part-import-iges": [],
            "part-import-iges-missing": ["execution_failed"],
            "part-import-step": [],
            "part-import-step-missing": ["execution_failed"],
            "part-line": [],
            "part-multi-common": [],
            "part-multi-common-first-rest": [],
            "part-multi-fuse": [],
            "part-parabola-edge": [],
            "part-plane": [],
            "part-prism": [],
            "part-regular-polygon": [],
            "part-section": [],
            "part-sphere": [],
            "part-torus": [],
            "part-vertex": [],
            "part-wedge": [],
            "part-xor": [],
            "part-spiral": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p8"), codes)

    def test_diagnostics_include_stage_target_and_subname_metadata(self) -> None:
        missing_target = self.run_recompute("missing-link-target", "p4")["diagnostics"][0]
        self.assertEqual(missing_target["code"], "missing_link_target")
        self.assertEqual(missing_target["object"], "Pad")
        self.assertEqual(missing_target["property"], "Profile")
        self.assertEqual(missing_target["stage"], "graph")
        self.assertEqual(missing_target["target"], "MissingSketch")

        invalid_placement = self.run_recompute("invalid-placement", "p4")["diagnostics"][0]
        self.assertEqual(invalid_placement["code"], "invalid_placement")
        self.assertEqual(invalid_placement["object"], "Sketch")
        self.assertEqual(invalid_placement["property"], "Placement")
        self.assertEqual(invalid_placement["stage"], "parse")

        missing_subshape = self.run_recompute("up-to-face-missing-subshape", "p3a")["diagnostics"][0]
        self.assertEqual(missing_subshape["code"], "invalid_subshape")
        self.assertEqual(missing_subshape["object"], "Pocket")
        self.assertEqual(missing_subshape["property"], "UpToFace")
        self.assertEqual(missing_subshape["stage"], "runtime")
        self.assertEqual(missing_subshape["target"], "Pad")
        self.assertEqual(missing_subshape["subname"], "Face99")
