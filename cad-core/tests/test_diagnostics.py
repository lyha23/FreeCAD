from __future__ import annotations

try:
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreDiagnosticsTest(CadCoreFixtureTestCase):
    def test_p2_fixture_diagnostics(self) -> None:
        expected = {
            "rect-pad-pocket": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p2"), codes)

    def test_reference_lifecycle_matrix(self) -> None:
        diagnostic_cases = {
            ("p2", "rect-pad-pocket"): [],
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
            repeats = 2 if (group, fixture) in {
                ("c3m2", "sketch-external-frozen-brep-reuse"),
                ("c3m2", "sketch-external-missing-brep-reuse"),
            } else 1
            for attempt in range(repeats):
                with self.subTest(fixture=fixture, attempt=attempt):
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

    def test_p3a_fixture_diagnostics(self) -> None:
        expected = {
            "pocket-through-all": [],
            "pocket-through-all-without-base": ["execution_failed"],
            "pocket-up-to-face": [],
            "pocket-up-to-shape-face": [],
            "pocket-up-to-shape-multiple-faces-offset": ["unsupported_property"],
            "pocket-up-to-shape-edge-subshape": ["unsupported_subshape_kind"],
            "pad-up-to-face": [],
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
            "pad-start-offset": [],
            "pad-start-offset-reversed": [],
            "pad-symmetric-start-offset": [],
            "pad-symmetric-taper": [],
            "pocket-symmetric-length": [],
            "pad-custom-vector": [],
            "pocket-custom-vector": [],
            "pad-reference-axis": [],
            "pad-reference-axis-edge": [],
            "pad-reference-axis-linear-bspline-edge": [],
            "pad-reference-axis-nonlinear-bspline-rejected": ["unsupported_subshape_kind"],
            "pad-sketch-placement": [],
            "pad-custom-direction-placement": [],
            "pad-custom-direction-sketch-rotation": [],
            "pocket-body-placement": [],
            "body-basefeature-placement": [],
            "pad-length-taper": [],
            "pad-length-taper-inner-wire": [],
            "pocket-length-taper": [],
            "pad-two-sides-taper": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p3b"), codes)

    def test_p4_fixture_diagnostics(self) -> None:
        expected = {
            "body-link-list": [],
            "feature-link-sub-list": [],
            "part-placement-body": [],
            "sketch-placement-pocket": [],
            "typed-property-pad": [],
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
            "sketch-external-whole-box": [],
            "sketch-external-internal-edge": [],
            "sketch-external-internal-vertex": [],
            "sketch-external-tilted-ellipse-edge": [],
            "sketch-external-tilted-circle-edge": [],
            "sketch-external-vertex": [],
            "sketch-hyperbola-arc-profile": [],
            "sketch-horizontal-vertical-profile": [],
            "sketch-internal-face": [],
            "sketch-line-relation-constraints-profile": [],
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
            "sketch-external-edge-stable-indexed-opaque-sublist": [],
            "up-to-face-stable-body-split": ["execution_failed"],
            "up-to-face-stable-indexed-opaque-sublist": [],
            "up-to-face-stable-indexed-reference": [],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p6"), codes)

    def test_p7_fixture_diagnostics(self) -> None:
        expected = {
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
            "app-link-box-scale": [],
            "app-link-box-transform": [],
            "app-link-element-box": [],
            "app-link-element-count-collapsed": [],
            "app-link-element-count-hidden-sublist-index": [],
            "app-link-element-count-sublist-index": [],
            "app-link-element-count-sublist-target-label": [],
            "app-link-element-list-sublist-index": [],
            "app-link-group-elements": [],
            "app-link-group-subshape-alias": [],
            "app-link-group-visibility": [],
            "app-link-show-element-inherited-child": [],
            "app-link-show-element-inherited-placement-list": [],
            "app-link-show-element-materialized": [],
            "app-link-show-element-synthetic": [],
            "app-link-stable-history-split": ["split_stable_subname"],
            "part-boolean-fragments": [],
            "part-boolean-fragments-compsolid": [],
            "part-boolean-fragments-compsolid-split": [],
            "part-boolean-fragments-shell-split": [],
            "part-boolean-fragments-split": [],
            "part-boolean-fragments-wire-split": [],
            "mesh-import-stl": [],
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
            "part-import-iges": [],
            "part-import-step": [],
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
