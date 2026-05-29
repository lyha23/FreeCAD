from __future__ import annotations

import ctypes
import json
import math
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "cad-core"
FFI_LIB_CANDIDATES = [
    ROOT / "build" / "libcad_core_ffi.dylib",
    ROOT / "build" / "libcad_core_ffi.so",
]


class CadCoreBuffer(ctypes.Structure):
    _fields_ = [("ptr", ctypes.c_void_p), ("len", ctypes.c_size_t)]


class CadCoreResult(ctypes.Structure):
    _fields_ = [("status", ctypes.c_int32), ("json", CadCoreBuffer), ("error", CadCoreBuffer)]


class CadCoreOcctMvpTest(unittest.TestCase):
    def run_recompute_file(self, input_path: Path, extra_args: list[str] | None = None) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / f"{input_path.stem}.result.json"
            command = [
                str(BIN),
                "recompute",
                str(input_path),
                "--output",
                str(output),
            ]
            if extra_args:
                command.extend(extra_args)
            subprocess.run(command, cwd=ROOT, check=True)
            return json.loads(output.read_text(encoding="utf-8"))

    def run_recompute(self, fixture: str, group: str = "mvp") -> dict:
        return self.run_recompute_file(ROOT / "fixtures" / group / f"{fixture}.json")

    def ffi_library_path(self) -> Path:
        for path in FFI_LIB_CANDIDATES:
            if path.exists():
                return path
        self.fail("cad_core_ffi library is missing; run cmake --build build first")

    def run_recompute_ffi(self, fixture: str, group: str = "mvp") -> dict:
        library = ctypes.CDLL(str(self.ffi_library_path()))
        library.cad_core_recompute_json.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        library.cad_core_recompute_json.restype = CadCoreResult
        library.cad_core_free_result.argtypes = [ctypes.POINTER(CadCoreResult)]
        library.cad_core_free_result.restype = None

        payload = (ROOT / "fixtures" / group / f"{fixture}.json").read_bytes()
        result = library.cad_core_recompute_json(payload, len(payload))
        try:
            if result.status != 0:
                error = ctypes.string_at(result.error.ptr, result.error.len).decode("utf-8") if result.error.ptr else ""
                self.fail(f"cad_core_recompute_json failed with status {result.status}: {error}")
            raw = ctypes.string_at(result.json.ptr, result.json.len).decode("utf-8")
            return json.loads(raw)
        finally:
            library.cad_core_free_result(ctypes.byref(result))

    def diagnostic_codes(self, fixture: str, group: str = "mvp") -> list[str]:
        return [item["code"] for item in self.run_recompute(fixture, group)["diagnostics"]]

    def assert_topology_counts(self, subshape_map: dict, expected: dict) -> None:
        self.assertEqual(
            sum(key.startswith("Face") for key in subshape_map),
            expected["topology_counts"]["faces"],
        )
        self.assertEqual(
            sum(key.startswith("Edge") for key in subshape_map),
            expected["topology_counts"]["edges"],
        )
        self.assertEqual(
            sum(key.startswith("Vertex") for key in subshape_map),
            expected["topology_counts"]["vertices"],
        )

    def assert_bbox_close(self, actual: dict, expected_min: list[float], expected_max: list[float]) -> None:
        for actual_value, expected_value in zip(actual["min"], expected_min):
            self.assertAlmostEqual(actual_value, expected_value, delta=1e-6)
        for actual_value, expected_value in zip(actual["max"], expected_max):
            self.assertAlmostEqual(actual_value, expected_value, delta=1e-6)

    def assert_bbox_close_delta(self, actual: dict, expected_min: list[float], expected_max: list[float], delta: float) -> None:
        for actual_value, expected_value in zip(actual["min"], expected_min):
            self.assertAlmostEqual(actual_value, expected_value, delta=delta)
        for actual_value, expected_value in zip(actual["max"], expected_max):
            self.assertAlmostEqual(actual_value, expected_value, delta=delta)

    def expected_freecad(self, group: str, fixture: str) -> dict:
        return json.loads((ROOT / "fixtures" / group / "expected" / f"{fixture}.freecad.json").read_text())

    def assert_object_matches_expected(self, result: dict, group: str, fixture: str) -> None:
        expected = self.expected_freecad(group, fixture)
        obj = result["objects"][expected["object"]]

        self.assert_bbox_close(obj["bbox"], expected["bbox"]["min"], expected["bbox"]["max"])
        self.assertAlmostEqual(obj["volume"], expected["volume"])
        self.assert_topology_counts(result["subshapes"][expected["object"]], expected)

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
            "pocket-without-base": ["execution_failed"],
            "pocket-open-sketch": ["open_profile"],
            "unsupported-pocket-type": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p2"), codes)

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
            "pocket-up-to-shape-multi-face-unsupported": ["unsupported_subshape_kind"],
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
            "sketch-arc-ellipse-profile": [],
            "sketch-arc-profile": [],
            "sketch-bspline-profile": [],
            "sketch-circle-profile": [],
            "sketch-coincident-profile": [],
            "sketch-construction-ignored": [],
            "sketch-ellipse-profile": [],
            "sketch-external-circle-edge": [],
            "sketch-external-circle-edge-as-line": [],
            "sketch-external-edge": [],
            "sketch-external-ellipse-edge": [],
            "sketch-external-face-unsupported": ["unsupported_subshape_kind"],
            "sketch-external-internal-edge": [],
            "sketch-external-internal-vertex": [],
            "sketch-external-tilted-ellipse-edge": [],
            "sketch-external-tilted-circle-edge": [],
            "sketch-external-vertex": [],
            "sketch-internal-face": [],
            "sketch-missing-external": ["missing_link_target"],
            "sketch-open-wire-internal-empty": [],
            "sketch-rect-circle-island": [],
            "sketch-rect-circle-hole": [],
            "sketch-unsupported-constraint": ["unsupported_property"],
            "sketch-unsupported-hyperbola": ["unsupported_geometry"],
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
            "sketch-external-edge-stable-body-split": ["split_stable_subname"],
            "sketch-external-edge-stable-body-split-after-add": ["split_stable_subname"],
            "sketch-external-edge-stable-indexed-opaque-sublist": [],
            "sketch-external-edge-stable-multi-prism": [],
            "sketch-external-edge-stable-taper-preserved": [],
            "up-to-face-stable-body-deleted": ["deleted_stable_subname"],
            "up-to-face-stable-body-history": [],
            "up-to-face-stable-body-preserved": [],
            "up-to-face-stable-body-split": ["split_stable_subname"],
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
            "hole-thread-clearance": [],
            "hole-threaded-cosmetic": [],
            "hole-threaded-bsf-cosmetic": [],
            "hole-threaded-bsp-fallback-cosmetic": [],
            "hole-threaded-bsw-cosmetic": [],
            "hole-threaded-fine-cosmetic": [],
            "hole-threaded-isotyre-cosmetic": [],
            "hole-threaded-known-gap": ["unsupported_property"],
            "hole-threaded-npt-cosmetic": [],
            "hole-threaded-unef-cosmetic": [],
            "hole-threaded-unf-cosmetic": [],
            "hole-threaded-unc-cosmetic": [],
            "hole-through-all": [],
            "hole-unc-clearance": [],
            "hole-without-base": ["execution_failed"],
            "linear-pattern-custom-spacings": [],
            "linear-pattern-pad-datum-line": [],
            "linear-pattern-pad-sketch-axis": [],
            "linear-pattern-pad-two-directions": [],
            "linear-pattern-spacing-pattern": [],
            "linear-pattern-whole-shape": [],
            "mirrored-pad-datum-plane": [],
            "mirrored-fillet-support-transform": [],
            "mirrored-refine-true": [],
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
            "app-link-group-elements": [],
            "app-link-group-subshape-alias": [],
            "app-link-group-visibility": [],
            "app-link-missing": ["missing_link_target"],
            "app-link-show-element-materialized": [],
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
            "part-cut": [],
            "part-cylinder": [],
            "part-cylinder-angled-prism": [],
            "part-ellipse": [],
            "part-ellipsoid": [],
            "part-fuse": [],
            "part-helix": [],
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

    def test_rect_pad_outputs_occt_mesh_and_subshape_map(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "rect-pad.result.json"
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(ROOT / "fixtures" / "mvp" / "rect-pad.json"),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
            )
            result = json.loads(output.read_text(encoding="utf-8"))
            pad = result["objects"]["Pad"]
            mesh = result["mesh"]["Pad"]
            subshape_map = result["subshapes"]["Pad"]

            self.assertEqual(result["diagnostics"], [])
            self.assertIn("OCCT", pad["kernel"])
            self.assertEqual(pad["bbox"]["min"], [0.0, 0.0, 0.0])
            self.assertEqual(pad["bbox"]["max"], [10.0, 5.0, 10.0])
            self.assertAlmostEqual(pad["volume"], 500.0)
            self.assertTrue(subshape_map)

            expected = json.loads((ROOT / "fixtures" / "mvp" / "expected" / "rect-pad.freecad.json").read_text())
            self.assertGreater(mesh["summary"]["vertex_count"], 0)
            self.assertGreater(mesh["summary"]["triangle_count"], 0)
            self.assertEqual(mesh["summary"]["triangle_count"], expected["mesh_summary"]["triangle_count"])
            self.assert_topology_counts(subshape_map, expected)

    def test_rect_pad_pocket_outputs_cut_body(self) -> None:
        result = self.run_recompute("rect-pad-pocket", "p2")
        body = result["objects"]["Body"]
        mesh = result["mesh"]["Body"]
        subshape_map = result["subshapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(body["volume"], 320.0)
        self.assertGreater(mesh["summary"]["triangle_count"], 0)
        self.assertNotIn("shapes", result)

        expected = json.loads((ROOT / "fixtures" / "p2" / "expected" / "rect-pad-pocket.freecad.json").read_text())
        self.assertEqual(mesh["summary"]["bbox"], expected["bbox"])
        self.assertAlmostEqual(mesh["summary"]["volume"], expected["volume"])
        self.assert_topology_counts(subshape_map, expected)

    def test_body_basefeature_pad_uses_base_solid(self) -> None:
        result = self.run_recompute("body-basefeature-pad", "p2")
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 5.0])
        self.assertAlmostEqual(body["volume"], 250.0)

    def test_p3a_pocket_through_all_outputs_cut_body(self) -> None:
        result = self.run_recompute("pocket-through-all", "p3a")
        body = result["objects"]["Body"]
        expected = json.loads((ROOT / "fixtures" / "p3a" / "expected" / "pocket-through-all.freecad.json").read_text())

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"], expected["bbox"])
        self.assertAlmostEqual(body["volume"], expected["volume"])
        self.assertEqual(result["objects"]["Pocket"]["method"], "ThroughAll")
        self.assert_topology_counts(result["subshapes"]["Body"], expected)

    def test_p3a_pocket_through_all_without_base_does_not_fake_body(self) -> None:
        result = self.run_recompute("pocket-through-all-without-base", "p3a")

        self.assertEqual(self.diagnostic_codes("pocket-through-all-without-base", "p3a"), ["execution_failed"])
        self.assertEqual(result["objects"]["Pocket"]["status"], "error")
        self.assertEqual(result["objects"]["Body"]["status"], "skipped")
        self.assertNotIn("Body", result["mesh"])

    def test_p3a_pocket_up_to_face_outputs_cut_body(self) -> None:
        result = self.run_recompute("pocket-up-to-face", "p3a")
        repeat = self.run_recompute("pocket-up-to-face", "p3a")
        body = result["objects"]["Body"]
        expected = json.loads((ROOT / "fixtures" / "p3a" / "expected" / "pocket-up-to-face.freecad.json").read_text())

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(repeat["diagnostics"], [])
        self.assertEqual(body["bbox"], expected["bbox"])
        self.assertEqual(repeat["objects"]["Body"]["bbox"], expected["bbox"])
        self.assertAlmostEqual(body["volume"], expected["volume"])
        self.assertAlmostEqual(repeat["objects"]["Body"]["volume"], expected["volume"])
        self.assertEqual(result["objects"]["Pocket"]["method"], "UpToFace")
        self.assert_topology_counts(result["subshapes"]["Body"], expected)
        self.assertEqual(sorted(result["subshapes"]["Body"]), sorted(repeat["subshapes"]["Body"]))

    def test_p3a_pocket_up_to_shape_outputs_cut_body(self) -> None:
        for fixture, expected_name in {
            "pocket-up-to-shape-solid": "pocket-up-to-shape-solid.freecad.json",
            "pocket-up-to-shape-face": "pocket-up-to-shape-face.freecad.json",
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3a")
                body = result["objects"]["Body"]
                expected = json.loads((ROOT / "fixtures" / "p3a" / "expected" / expected_name).read_text())

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(body["bbox"], expected["bbox"])
                self.assertAlmostEqual(body["volume"], expected["volume"])
                self.assertEqual(result["objects"]["Pocket"]["method"], "UpToShape")
                self.assert_topology_counts(result["subshapes"]["Body"], expected)

    def test_p3a_pad_up_to_face_outputs_solid(self) -> None:
        result = self.run_recompute("pad-up-to-face", "p3a")
        pad = result["objects"]["Pad"]
        expected = json.loads((ROOT / "fixtures" / "p3a" / "expected" / "pad-up-to-face.freecad.json").read_text())

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["bbox"], expected["bbox"])
        self.assertAlmostEqual(pad["volume"], expected["volume"])
        self.assertEqual(pad["method"], "UpToFace")
        self.assert_topology_counts(result["subshapes"]["Pad"], expected)

    def test_p3b_two_sides_length_outputs_expected_extents(self) -> None:
        result = self.run_recompute("pad-two-sides-length", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pad-two-sides-length")

    def test_p3b_two_sides_up_to_targets(self) -> None:
        for fixture in [
            "pad-two-sides-up-to-face1",
            "pad-two-sides-up-to-shape1",
            "pad-two-sides-up-to-face2",
            "pad-two-sides-up-to-shape2",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(pad["method"], "Two sides")
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_up_to_first_last_selects_nearest_or_furthest_body_face(self) -> None:
        first = self.run_recompute("pad-up-to-first", "p3b")
        last = self.run_recompute("pad-up-to-last", "p3b")

        self.assertEqual(first["diagnostics"], [])
        self.assertEqual(last["diagnostics"], [])
        self.assertEqual(first["objects"]["Pad"]["method"], "UpToFirst")
        self.assertEqual(last["objects"]["Pad"]["method"], "UpToLast")
        self.assert_bbox_close(first["objects"]["Pad"]["bbox"], [0.0, 0.0, -2.0], [10.0, 5.0, 0.0])
        self.assert_bbox_close(last["objects"]["Pad"]["bbox"], [0.0, 0.0, -2.0], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(first["objects"]["Pad"]["volume"], 100.0, delta=1e-6)
        self.assertAlmostEqual(last["objects"]["Pad"]["volume"], 600.0, delta=1e-6)

    def test_p3b_pocket_two_sides_length_cuts_body(self) -> None:
        result = self.run_recompute("pocket-two-sides-length", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pocket-two-sides-length")

    def test_p3b_symmetric_length_outputs_expected_extents(self) -> None:
        for fixture in ["pad-symmetric-length", "pocket-symmetric-length"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_custom_vector_uses_along_sketch_normal_length(self) -> None:
        result = self.run_recompute("pad-custom-vector", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pad-custom-vector")

    def test_p3b_pocket_custom_vector_cuts_body(self) -> None:
        result = self.run_recompute("pocket-custom-vector", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["method"], "Length")
        self.assert_object_matches_expected(result, "p3b", "pocket-custom-vector")

    def test_c_api_matches_cli_for_p3b_recompute(self) -> None:
        cli_result = self.run_recompute("pocket-custom-vector", "p3b")
        ffi_result = self.run_recompute_ffi("pocket-custom-vector", "p3b")

        self.assertEqual(ffi_result["diagnostics"], cli_result["diagnostics"])
        self.assertEqual(ffi_result["objects"], cli_result["objects"])
        self.assertEqual(ffi_result["mesh"], cli_result["mesh"])
        self.assertEqual(ffi_result["subshapes"], cli_result["subshapes"])
        self.assertEqual(ffi_result["named_shapes"], cli_result["named_shapes"])

    def test_p3b_reference_axis_uses_sketch_normal_axis(self) -> None:
        for fixture in ["pad-reference-axis", "pad-reference-axis-edge"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_sketch_placement_transforms_profile(self) -> None:
        result = self.run_recompute("pad-sketch-placement", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pad-sketch-placement")

    def test_p3b_custom_direction_with_placement(self) -> None:
        for fixture in ["pad-custom-direction-placement", "pad-custom-direction-sketch-rotation"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")

                self.assertEqual(result["diagnostics"], [])
                self.assert_object_matches_expected(result, "p3b", fixture)

    def test_p3b_body_and_featurebase_placement(self) -> None:
        result = self.run_recompute("pocket-body-placement", "p3b")

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "pocket-body-placement")

        result = self.run_recompute("body-basefeature-placement", "p3b")
        body = result["objects"]["Body"]
        feature_base = result["objects"]["FeatureBase"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_object_matches_expected(result, "p3b", "body-basefeature-placement")
        self.assertEqual(feature_base["bbox"], body["bbox"])
        self.assertAlmostEqual(feature_base["volume"], body["volume"])

    def test_p3b_taper_outputs_geometry_and_marks_topo_gap(self) -> None:
        result = self.run_recompute("pad-length-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["topo_naming"], "known_gap:taper_history")
        self.assert_object_matches_expected(result, "p3b", "pad-length-taper")

        result = self.run_recompute("pad-two-sides-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["topo_naming"], "known_gap:taper_history")
        self.assertEqual(pad["method"], "Two sides")
        self.assert_object_matches_expected(result, "p3b", "pad-two-sides-taper")

        result = self.run_recompute("pad-symmetric-taper", "p3b")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["topo_naming"], "known_gap:taper_history")
        self.assertEqual(pad["method"], "Symmetric")
        self.assert_object_matches_expected(result, "p3b", "pad-symmetric-taper")

    def test_p3b_taper_supports_inner_wire_profile(self) -> None:
        result = self.run_recompute("pad-length-taper-inner-wire", "p3b")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["profile_ready"], True)
        self.assertEqual(sketch["edge_count"], 8)
        self.assertEqual(sketch["internal_face_count"], 1)
        self.assertEqual(pad["topo_naming"], "known_gap:taper_history")
        self.assert_bbox_close_delta(pad["bbox"], [-0.2793662559, -0.2793662559, 0.0], [12.2793662559, 8.2793662559, 8.0], 1e-6)
        self.assertAlmostEqual(pad["volume"], 735.2890094646057, delta=1e-6)

    def test_p3b_pocket_taper_cuts_body(self) -> None:
        result = self.run_recompute("pocket-length-taper", "p3b")
        pocket = result["objects"]["Pocket"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["topo_naming"], "known_gap:taper_history")
        self.assert_object_matches_expected(result, "p3b", "pocket-length-taper")

    def test_p4_normalized_links_drive_graph_and_executors(self) -> None:
        result = self.run_recompute("body-link-list", "p4")
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["status"], "ok")
        self.assertEqual(body["group"], ["Sketch", "Pad"])
        self.assertEqual(body["tip"], "Pad")
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 10.0])

        result = self.run_recompute("feature-link-sub-list", "p4")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["method"], "UpToShape")
        self.assertEqual(pad["source_profile"], "Sketch")
        self.assertEqual(pad["bbox"]["max"], [10.0, 5.0, 5.0])

    def test_p4_part_local_placement_transforms_body_output(self) -> None:
        result = self.run_recompute("part-placement-body", "p4")
        body = result["objects"]["Body"]
        part = result["objects"]["Part"]
        mesh_summary = result["mesh"]["Body"]["summary"]
        part_mesh_summary = result["mesh"]["Part"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [20.0, 3.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [30.0, 8.0, 10.0])
        self.assertEqual(part["display_object"], "Body")
        self.assertEqual(part["bbox"], body["bbox"])
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertEqual(part_mesh_summary["bbox"], part["bbox"])
        self.assertAlmostEqual(body["volume"], 500.0)
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])
        self.assertAlmostEqual(part["volume"], body["volume"])

    def test_p4_sketch_placement_pocket_uses_same_body_coordinates(self) -> None:
        result = self.run_recompute("sketch-placement-pocket", "p4")
        body = result["objects"]["Body"]
        mesh_summary = result["mesh"]["Body"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 10.0])
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertAlmostEqual(body["volume"], 440.0)
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])

    def test_p4_typed_scalar_properties_feed_feature_extrude(self) -> None:
        result = self.run_recompute("typed-property-pad", "p4")
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["method"], "Length")
        self.assertEqual(pad["bbox"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(pad["bbox"]["max"], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(pad["volume"], 500.0)

    def test_p4_datum_plane_support_places_sketch_profile(self) -> None:
        result = self.run_recompute("datum-plane-support", "p4")
        body = result["objects"]["Body"]
        pad = result["objects"]["Pad"]
        mesh_summary = result["mesh"]["Body"]["summary"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["DatumPlane"]["status"], "ok")
        self.assertEqual(body["bbox"]["min"], [0.0, 0.0, 2.0])
        self.assertEqual(body["bbox"]["max"], [10.0, 5.0, 7.0])
        self.assertEqual(pad["bbox"], body["bbox"])
        self.assertEqual(mesh_summary["bbox"], body["bbox"])
        self.assertAlmostEqual(body["volume"], 250.0)
        self.assertAlmostEqual(mesh_summary["volume"], body["volume"])

    def test_p4_datum_line_reference_axis_drives_pad_direction(self) -> None:
        result = self.run_recompute("datum-line-reference-axis", "p4")
        datum_line = result["objects"]["DatumLine"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(datum_line["status"], "ok")
        self.assertEqual(datum_line["datum"], "line")
        self.assertAlmostEqual(datum_line["direction"][0], 0.7071067811865476)
        self.assertAlmostEqual(datum_line["direction"][1], 0.0)
        self.assertAlmostEqual(datum_line["direction"][2], 0.7071067811865475)
        self.assertEqual(pad["method"], "Length")
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [16.0, 5.0, 6.0])
        self.assertAlmostEqual(pad["volume"], 300.0, delta=1e-6)

    def test_p4_datum_point_uses_parent_part_placement(self) -> None:
        result = self.run_recompute("datum-point-part-placement", "p4")
        datum_point = result["objects"]["DatumPoint"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(datum_point["status"], "ok")
        self.assertEqual(datum_point["datum"], "point")
        self.assertEqual(datum_point["point"], [13.0, 4.0, 5.0])

    def test_p5_coincident_constraints_merge_profile_endpoints(self) -> None:
        result = self.run_recompute("sketch-coincident-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 2)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
        self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_circle_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-circle-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 1)
        self.assert_bbox_close(pad["bbox"], [-2.0, -2.0, 0.0], [2.0, 2.0, 3.0])
        self.assertAlmostEqual(pad["volume"], 37.69911184307752)

    def test_p5_mixed_closed_wires_make_profile_with_hole(self) -> None:
        result = self.run_recompute("sketch-rect-circle-hole", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["edge_count"], 5)
        self.assertEqual(sketch["raw_edge_count"], 5)
        self.assertEqual(sketch["internal_face_count"], 1)
        self.assertEqual(sketch["internal_edge_count"], 5)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 3.0])
        self.assertAlmostEqual(pad["volume"], (50.0 - math.pi) * 3.0, delta=1e-6)

    def test_p5_nested_closed_wires_keep_island_face(self) -> None:
        result = self.run_recompute("sketch-rect-circle-island", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["profile"], "occt_compound")
        self.assertEqual(sketch["edge_count"], 6)
        self.assertEqual(sketch["raw_edge_count"], 6)
        self.assertEqual(sketch["internal_face_count"], 2)
        self.assertEqual(sketch["internal_edge_count"], 6)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 3.0])
        self.assertAlmostEqual(pad["volume"], (50.0 - 2.0 * math.pi) * 3.0, delta=1e-6)

    def test_p5_arc_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-arc-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 2)
        self.assert_bbox_close(pad["bbox"], [-2.0, 0.0, 0.0], [2.0, 2.0, 3.0])
        self.assertAlmostEqual(pad["volume"], 18.84955592153876)

    def test_p5_arc_ellipse_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-arc-ellipse-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 2)
        self.assert_bbox_close(pad["bbox"], [-3.0, 0.0, 0.0], [3.0, 1.0, 2.0])
        self.assertAlmostEqual(pad["volume"], 9.42477796076938)

    def test_p5_ellipse_profile_outputs_pad(self) -> None:
        result = self.run_recompute("sketch-ellipse-profile", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 1)
        self.assert_bbox_close(pad["bbox"], [-3.0, -1.0, 0.0], [3.0, 1.0, 2.0])
        self.assertAlmostEqual(pad["volume"], 18.84955592153876)

    def test_p5_bspline_profile_builds_internal_shape(self) -> None:
        result = self.run_recompute("sketch-bspline-profile", "p5")
        sketch = result["objects"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["profile"], "occt_face")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["raw_edge_count"], 2)
        self.assertEqual(sketch["internal_shape"], "occt_internal_shape")
        self.assertEqual(sketch["internal_face_count"], 1)
        self.assertGreaterEqual(sketch["internal_edge_count"], 2)

    def test_p5_construction_geometry_is_ignored_for_profile(self) -> None:
        result = self.run_recompute("sketch-construction-ignored", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["coincident_constraints_applied"], 1)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
        self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_external_edge_projects_as_construction_geometry(self) -> None:
        result = self.run_recompute("sketch-external-edge", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
        self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_external_vertex_projects_as_construction_geometry(self) -> None:
        result = self.run_recompute("sketch-external-vertex", "p5")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_point_count"], 1)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
        self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_external_curve_edges_project_as_construction_geometry(self) -> None:
        for fixture in ["sketch-external-circle-edge", "sketch-external-ellipse-edge"]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 4)
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], 1)
                self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
                self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_non_parallel_external_circle_edge_projection_variants(self) -> None:
        for fixture, expected_curve_count in {
            "sketch-external-circle-edge-as-line": 0,
            "sketch-external-tilted-circle-edge": 1,
            "sketch-external-tilted-ellipse-edge": 1,
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p5")
                sketch = result["objects"]["Sketch"]
                pad = result["objects"]["Pad"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["status"], "ok")
                self.assertEqual(sketch["edge_count"], 4)
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], expected_curve_count)
                self.assertEqual(sketch["external_point_count"], 0)
                self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 4.0])
                self.assertAlmostEqual(pad["volume"], 200.0)

    def test_p5_closed_sketch_exports_internal_subshapes(self) -> None:
        result = self.run_recompute("sketch-internal-face", "p5")
        sketch = result["objects"]["Sketch"]
        subshape_map = result["subshapes"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertTrue(sketch["profile_ready"])
        self.assertEqual(sketch["internal_shape"], "occt_internal_shape")
        self.assertEqual(sketch["internal_face_count"], 1)
        self.assertEqual(sketch["internal_edge_count"], 4)
        self.assertEqual(sketch["internal_vertex_count"], 4)
        self.assertIn("InternalFace1", subshape_map)
        self.assertIn("InternalEdge1", subshape_map)
        self.assertIn("InternalVertex1", subshape_map)
        self.assertEqual(sketch["internal_element_map"]["InternalEdge1"], "Edge1")
        self.assertEqual(sketch["internal_element_map"]["Edge1"], "InternalEdge1")
        self.assertEqual(sketch["internal_element_map"]["InternalVertex1"], "Vertex1")
        self.assertEqual(sketch["internal_element_map"]["Vertex1"], "InternalVertex1")

    def test_p5_external_geometry_resolves_internal_edge(self) -> None:
        result = self.run_recompute("sketch-external-internal-edge", "p5")
        sketch = result["objects"]["Sketch"]
        base_sketch = result["objects"]["BaseSketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(base_sketch["internal_edge_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [6.0, 3.0, 2.0])
        self.assertAlmostEqual(pad["volume"], 36.0)

        result = self.run_recompute("sketch-external-internal-vertex", "p5")
        sketch = result["objects"]["Sketch"]
        base_sketch = result["objects"]["BaseSketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(base_sketch["internal_vertex_count"], 4)
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 1)
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [6.0, 3.0, 2.0])
        self.assertAlmostEqual(pad["volume"], 36.0)

    def test_p5_open_sketch_keeps_raw_shape_without_profile_face(self) -> None:
        result = self.run_recompute("sketch-open-wire-internal-empty", "p5")
        sketch = result["objects"]["Sketch"]
        subshape_map = result["subshapes"]["Sketch"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["status"], "ok")
        self.assertEqual(sketch["shape"], "occt_sketch_shape")
        self.assertEqual(sketch["profile"], "none")
        self.assertFalse(sketch["profile_ready"])
        self.assertEqual(sketch["raw_edge_count"], 3)
        self.assertEqual(sketch["internal_shape"], "none")
        self.assertEqual(sketch["internal_element_map"], {})
        self.assertEqual(sum(key.startswith("Edge") for key in subshape_map), 3)

    def test_p6_named_shape_exports_indexed_element_ledger(self) -> None:
        result = self.run_recompute("named-shape-indexed-pad", "p6")
        named_shape = result["named_shapes"]["Pad"]
        elements = named_shape["elements"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["owner"], "Pad")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Face1"], "Face1")
        self.assertEqual(elements[named_shape["element_map"]["Sketch.Edge1"]]["kind"], "edge")
        self.assertEqual(elements[named_shape["element_map"]["Sketch.Vertex1"]]["kind"], "vertex")
        self.assertIn("Face1", elements)
        self.assertIn("Edge1", elements)
        self.assertIn("Vertex1", elements)
        self.assertEqual(elements["Face1"]["kind"], "face")
        self.assertEqual(elements["Face1"]["status"], "generated")
        self.assertEqual(set(elements), set(result["subshapes"]["Pad"]))
        generated_sources = {
            source
            for item in named_shape["history"]
            if item["kind"] == "generated"
            for source in item["sources"]
        }
        self.assertIn("Sketch.Edge1", generated_sources)
        self.assertIn("Sketch.Vertex1", generated_sources)

    def test_p6_two_side_and_symmetric_fast_prisms_keep_profile_history(self) -> None:
        for fixture, owner, source in [
            ("pad-two-sides-length", "Pad", "Sketch"),
            ("pad-symmetric-length", "Pad", "Sketch"),
            ("pocket-two-sides-length", "Pocket", "SketchPocket"),
            ("pocket-symmetric-length", "Pocket", "SketchPocket"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"][owner]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Vertex1"]]["kind"], "vertex")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Face1"]]["kind"], "face")

    def test_p6_multi_prism_xor_propagates_profile_history(self) -> None:
        for fixture in [
            "pad-two-sides-up-to-face1",
            "pad-two-sides-up-to-shape1",
            "pad-two-sides-up-to-face2",
            "pad-two-sides-up-to-shape2",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"]["Pad"]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertTrue(any(key.startswith("Pad.XorUnion1.") for key in named_shape["element_map"]))
                self.assertEqual(elements[named_shape["element_map"]["Sketch.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"]["Sketch.Vertex1"]]["kind"], "vertex")

    def test_p6_taper_preserved_sources_are_partial_history(self) -> None:
        for fixture, owner, source, extra_edges in [
            ("pad-length-taper", "Pad", "Sketch", []),
            ("pad-two-sides-taper", "Pad", "Sketch", []),
            ("pad-symmetric-taper", "Pad", "Sketch", []),
            ("pad-length-taper-inner-wire", "Pad", "Sketch", ["Edge5"]),
            ("pocket-length-taper", "Pocket", "SketchPocket", []),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                named_shape = result["named_shapes"][owner]
                elements = named_shape["elements"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(result["objects"][owner]["topo_naming"], "known_gap:taper_history")
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Edge1"]]["kind"], "edge")
                self.assertEqual(elements[named_shape["element_map"][f"{source}.Vertex1"]]["kind"], "vertex")
                for edge in extra_edges:
                    self.assertEqual(elements[named_shape["element_map"][f"{source}.{edge}"]]["kind"], "edge")

    def test_p6_taper_records_thru_sections_generated_history(self) -> None:
        for fixture, owner, source_edges, section_sources in [
            ("pad-length-taper", "Pad", ["Sketch.Edge1"], ["Pad.TaperSection2.Edge1"]),
            ("pocket-length-taper", "Pocket", ["SketchPocket.Edge1"], ["Pocket.TaperSection2.Edge1"]),
            (
                "pad-two-sides-taper",
                "Pad",
                ["Sketch.Edge1"],
                ["Pad.Prism1.TaperSection2.Edge1", "Pad.Prism2.TaperSection2.Edge1"],
            ),
            (
                "pad-symmetric-taper",
                "Pad",
                ["Sketch.Edge1"],
                ["Pad.Prism1.TaperSection2.Edge1", "Pad.Prism2.TaperSection2.Edge1"],
            ),
            (
                "pad-length-taper-inner-wire",
                "Pad",
                ["Sketch.Edge1", "Sketch.Edge5"],
                ["Pad.Outer.TaperSection2.Edge1", "Pad.Inner1.TaperSection2.Edge1"],
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p3b")
                history = result["named_shapes"][owner]["history"]

                self.assertEqual(result["diagnostics"], [])
                for source_edge in source_edges:
                    self.assertTrue(
                        any(item["kind"] == "generated" and item["sources"] == [source_edge] for item in history)
                    )
                for section_source in section_sources:
                    self.assertTrue(
                        any(
                            item["kind"] == "generated"
                            and item["sources"] == [section_source]
                            for item in history
                        )
                    )

    def test_p6_body_boolean_named_shape_records_maker_history(self) -> None:
        for fixture, required_sources in {
            "body-additive-fuse-history": ("BaseFeature.", "Pad."),
            "body-boolean-history": ("Pad.", "Pocket."),
        }.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                named_shape = result["named_shapes"]["Body"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(named_shape["owner"], "Body")
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertEqual(named_shape["element_map"]["Face1"], "Face1")
                non_indexed_sources = {
                    source
                    for item in named_shape["history"]
                    if item["kind"] != "indexed"
                    for source in item["sources"]
                }
                for required_source in required_sources:
                    self.assertTrue(any(source.startswith(required_source) for source in non_indexed_sources))
                self.assertTrue(any(key.startswith(required_sources[0]) for key in named_shape["element_map"]))
                self.assertTrue(any(key.startswith(required_sources[1]) for key in named_shape["element_map"]))

    def test_p6_body_boolean_propagates_nested_element_map_aliases(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-body-profile-source", "p6")
        named_shape = result["named_shapes"]["Body"]
        sketch = result["objects"]["ProbeSketch"]
        elements = named_shape["elements"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(elements[named_shape["element_map"]["SketchPad.Edge1"]]["kind"], "edge")
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)

    def test_p6_body_boolean_records_merge_history_for_shared_current_elements(self) -> None:
        for fixture, stable_sources in [
            ("body-additive-fuse-history", ("Pad.Edge3", "TopSketch.Edge1")),
            ("body-boolean-history", ("Pocket.Edge3", "SketchPocket.Edge1")),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                named_shape = result["named_shapes"]["Body"]
                target = named_shape["element_map"][stable_sources[1]]

                self.assertEqual(result["diagnostics"], [])
                self.assertTrue(
                    any(
                        item["kind"] == "merge"
                        and item["element"] == target
                        and all(source in item["sources"] for source in stable_sources)
                        for item in named_shape["history"]
                    )
                )

    def test_p6_body_split_history_does_not_guess_element_map(self) -> None:
        result = self.run_recompute("body-split-history", "p6")
        named_shape = result["named_shapes"]["Body"]
        split_entries = [item for item in named_shape["history"] if item["kind"] == "split"]
        split_sources = {source for item in split_entries for source in item["sources"]}
        deleted_elements = {item["element"] for item in named_shape["history"] if item["kind"] == "deleted"}

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Pad.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["Pad.Edge1"], "Edge1")
        self.assertEqual(named_shape["element_map"]["Pad.Vertex1"], "Vertex1")
        self.assertIn("Pad.Face5", split_sources)
        self.assertNotIn("Pad.Face5", named_shape["element_map"])
        self.assertTrue(any(element["status"] == "split" for element in named_shape["elements"].values()))
        self.assertIn("Pocket.Face5", deleted_elements)
        self.assertNotIn("Pocket.Face5", named_shape["element_map"])

    def test_p6_stable_subname_history_diagnostics(self) -> None:
        for fixture, code, object_name, property_name, stable_subname in [
            ("up-to-face-stable-body-split", "split_stable_subname", "ProbePad", "UpToFace", "Pad.Face5"),
            ("up-to-face-stable-body-deleted", "deleted_stable_subname", "ProbePad", "UpToFace", "Pocket.Face5"),
            ("sketch-external-edge-stable-body-split", "split_stable_subname", "ProbeSketch", "ExternalGeometry", "Pocket.Edge1"),
            ("sketch-external-edge-stable-body-deleted", "deleted_stable_subname", "ProbeSketch", "ExternalGeometry", "Pocket.Edge11"),
            (
                "sketch-external-edge-stable-body-split-after-add",
                "split_stable_subname",
                "ProbeSketch",
                "ExternalGeometry",
                "Pocket.Edge1",
            ),
            (
                "sketch-external-edge-stable-body-deleted-after-add",
                "deleted_stable_subname",
                "ProbeSketch",
                "ExternalGeometry",
                "Pocket.Edge11",
            ),
        ]:
            with self.subTest(fixture=fixture):
                diagnostic = self.run_recompute(fixture, "p6")["diagnostics"][0]

                self.assertEqual(diagnostic["code"], code)
                self.assertEqual(diagnostic["object"], object_name)
                self.assertEqual(diagnostic["property"], property_name)
                self.assertEqual(diagnostic["target"], "Body")
                self.assertEqual(diagnostic["subname"], stable_subname)

    def test_p6_up_to_face_uses_element_map_before_stale_sublist(self) -> None:
        for fixture in [
            "up-to-face-stable-indexed-reference",
            "up-to-face-stable-indexed-opaque-sublist",
            "up-to-face-stable-body-history",
            "up-to-face-stable-body-preserved",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                feature = result["objects"].get("ProbePad", result["objects"].get("Pocket"))

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(feature["status"], "ok")
                self.assertEqual(feature["method"], "UpToFace")

    def test_p6_external_geometry_link_sub_list_uses_element_map(self) -> None:
        result = self.run_recompute("sketch-external-edge-stable-indexed-opaque-sublist", "p6")
        sketch = result["objects"]["Sketch"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["external_geometry_count"], 1)
        self.assertEqual(sketch["external_curve_count"], 0)
        self.assertEqual(sketch["external_point_count"], 0)
        self.assertEqual(pad["status"], "ok")

        for fixture in [
            "sketch-external-edge-stable-body-preserved",
            "sketch-external-edge-stable-body-profile-source",
            "sketch-external-edge-stable-multi-prism",
            "sketch-external-edge-stable-taper-preserved",
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p6")
                sketch = result["objects"]["ProbeSketch"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(sketch["external_geometry_count"], 1)
                self.assertEqual(sketch["external_curve_count"], 0)
                self.assertEqual(sketch["external_point_count"], 0)

    def test_p7_refine_false_is_feature_refine_noop(self) -> None:
        result = self.run_recompute("pad-refine-false", "p7")
        pad = result["objects"]["Pad"]
        expected = self.expected_freecad("mvp", "rect-pad")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertNotIn("topo_naming", pad)
        self.assert_bbox_close(pad["bbox"], expected["bbox"]["min"], expected["bbox"]["max"])
        self.assertAlmostEqual(pad["volume"], expected["volume"])
        self.assert_topology_counts(result["subshapes"]["Pad"], expected)

    def test_p7_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("pad-refine-true", "p7")
        pad = result["objects"]["Pad"]
        named_shape = result["named_shapes"]["Pad"]
        expected = self.expected_freecad("mvp", "rect-pad")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pad["status"], "ok")
        self.assertEqual(pad["refine"], "applied")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assert_bbox_close(pad["bbox"], expected["bbox"]["min"], expected["bbox"]["max"])
        self.assertAlmostEqual(pad["volume"], expected["volume"])
        self.assert_topology_counts(result["subshapes"]["Pad"], expected)

    def test_p7_pocket_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("pocket-refine-true", "p7")
        pocket = result["objects"]["Pocket"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pocket["status"], "ok")
        self.assertNotIn("refine", pocket)
        self.assertEqual(body["refined_features"], ["Pocket"])
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(body["volume"], 320.0)

    def test_p7_coordinate_system_exposes_axes_for_reference_axis(self) -> None:
        result = self.run_recompute("datum-coordinate-system-reference-axis", "p7")
        coordinate_system = result["objects"]["CoordinateSystem"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(coordinate_system["datum"], "coordinate_system")
        self.assertAlmostEqual(coordinate_system["z_axis"][0], 0.7071067811865476)
        self.assertAlmostEqual(coordinate_system["z_axis"][2], 0.7071067811865475)
        self.assertEqual(pad["method"], "Length")
        self.assert_bbox_close(pad["bbox"], [0.0, 0.0, 0.0], [16.0, 5.0, 6.0])
        self.assertAlmostEqual(pad["volume"], 300.0, delta=1e-6)

    def test_p7_coordinate_system_can_place_sketch_support(self) -> None:
        result = self.run_recompute("datum-coordinate-system-sketch-support", "p7")
        coordinate_system = result["objects"]["CoordinateSystem"]
        body = result["objects"]["Body"]
        pad = result["objects"]["Pad"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(coordinate_system["origin"], [0.0, 0.0, 2.0])
        self.assertEqual(pad["bbox"], body["bbox"])
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 2.0], [10.0, 5.0, 7.0])
        self.assertAlmostEqual(body["volume"], 250.0, delta=1e-6)

    def test_p7_origin_ignores_local_placement_but_keeps_parent_group_placement(self) -> None:
        result = self.run_recompute("origin-identity-placement", "p7")
        origin = result["objects"]["Origin"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(origin["datum"], "origin")
        self.assertEqual(origin["origin"], [10.0, 0.0, 0.0])
        self.assertEqual(origin["x_axis"], [1.0, 0.0, 0.0])

    def test_p7_hole_blind_depth_cuts_body(self) -> None:
        result = self.run_recompute("hole-blind-depth", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(hole["add_sub"], "sub")
        self.assert_bbox_close_delta(hole["bbox"], [4.0, 1.5, 6.0], [6.0, 3.5, 10.0], 1e-2)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 1e-2)
        self.assertAlmostEqual(body["volume"], 487.4336293856408, delta=1e-6)

    def test_p7_hole_refine_true_uses_body_final_result_refine(self) -> None:
        result = self.run_recompute("hole-refine-true", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["status"], "ok")
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(body["tip"], "Hole")
        self.assertEqual(body["refined_features"], ["Hole"])
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 1e-2)
        self.assertAlmostEqual(body["volume"], 487.4336293856408, delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")

    def test_p7_hole_through_all_cuts_body(self) -> None:
        result = self.run_recompute("hole-through-all", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["method"], "ThroughAll")
        self.assertGreater(hole["depth"], 10.0)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 1e-2)
        self.assertAlmostEqual(body["volume"], 468.58407346410206, delta=1e-6)

    def test_p7_hole_counterbore_cuts_head_cylinder_and_shaft(self) -> None:
        result = self.run_recompute("hole-counterbore", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        expected_cut_volume = math.pi * ((2.0 * 2.0 * 2.0) + (1.0 * 1.0 * 2.0))

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Counterbore")
        self.assertEqual(hole["hole_cut_diameter"], 4.0)
        self.assertEqual(hole["hole_cut_depth"], 2.0)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 5e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_countersink_cuts_conical_head_and_shaft(self) -> None:
        result = self.run_recompute("hole-countersink", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        countersink_volume = math.pi * (2.0 * 2.0 + 2.0 * 1.0 + 1.0 * 1.0) / 3.0
        expected_cut_volume = countersink_volume + math.pi * 3.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Countersink")
        self.assertEqual(hole["hole_cut_countersink_angle"], 90.0)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 5e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_counterdrill_cuts_head_cone_between_two_cylinders(self) -> None:
        result = self.run_recompute("hole-counterdrill", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        head_cylinder = math.pi * 2.0 * 2.0 * 1.0
        transition = math.pi * (2.0 * 2.0 + 2.0 * 1.0 + 1.0 * 1.0) / 3.0
        shaft = math.pi * 2.0
        expected_cut_volume = head_cylinder + transition + shaft

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "Counterdrill")
        self.assertEqual(hole["hole_cut_depth"], 1.0)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 5e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_angled_drill_point_extends_blind_hole_tip(self) -> None:
        result = self.run_recompute("hole-angled-drill-point", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        tip_depth = math.tan(math.radians((180.0 - 118.0) / 2.0))
        expected_cut_volume = (math.pi * 4.0) + (math.pi * tip_depth / 3.0)

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["hole_cut_type"], "None")
        self.assertEqual(hole["drill_point"], "Angled")
        self.assertEqual(hole["drill_for_depth"], False)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 5e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_uses_sketch_points_as_hole_centers(self) -> None:
        result = self.run_recompute("hole-point-profile", "p7")
        sketch = result["objects"]["SketchHole"]
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["raw_point_count"], 1)
        self.assertEqual(sketch["profile_ready"], False)
        self.assertEqual(hole["method"], "Dimension")
        self.assertEqual(hole["source_profile"], "SketchHole")
        self.assert_bbox_close_delta(hole["bbox"], [4.0, 1.5, 6.0], [6.0, 3.5, 10.0], 1e-2)
        self.assertAlmostEqual(hole["volume"], 4.0 * math.pi, delta=1e-6)
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0], 1e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - (4.0 * math.pi), delta=1e-6)

    def test_p7_hole_tapered_profile_uses_tapered_angle_bottom_radius(self) -> None:
        result = self.run_recompute("hole-tapered", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        top_radius = 1.0
        depth = 4.0
        bottom_radius = top_radius - depth / math.tan(math.radians(80.0))
        expected_cut_volume = math.pi * depth * (
            top_radius * top_radius + top_radius * bottom_radius + bottom_radius * bottom_radius
        ) / 3.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["tapered"], True)
        self.assertEqual(hole["tapered_angle"], 80.0)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assert_bbox_close_delta(hole["bbox"], [4.0, 1.5, 6.0], [6.0, 3.5, 10.0], 5e-2)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_without_model_thread_uses_tap_drill_plain_tool(self) -> None:
        result = self.run_recompute("hole-threaded-cosmetic", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        expected_cut_volume = math.pi * 0.375 * 0.375 * 4.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricProfile")
        self.assertEqual(hole["thread_size"], "M1x0.25")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 1.0)
        self.assertEqual(hole["thread_pitch"], 0.25)
        self.assertEqual(hole["diameter"], 0.75)
        self.assertEqual(hole["drill_point"], "Flat")
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_fine_profile_uses_fine_tap_drill_table(self) -> None:
        result = self.run_recompute("hole-threaded-fine-cosmetic", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        expected_cut_volume = math.pi * 1.75 * 1.75 * 4.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricFineProfile")
        self.assertEqual(hole["thread_size"], "M4x0.5")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 4.0)
        self.assertEqual(hole["thread_pitch"], 0.5)
        self.assertEqual(hole["diameter"], 3.5)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_unc_profile_uses_unc_tap_drill_table(self) -> None:
        result = self.run_recompute("hole-threaded-unc-cosmetic", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        expected_cut_volume = math.pi * 1.175 * 1.175 * 4.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "UNC")
        self.assertEqual(hole["thread_size"], "#4")
        self.assertEqual(hole["diameter_source"], "thread_tap_drill")
        self.assertEqual(hole["thread_diameter"], 2.845)
        self.assertEqual(hole["thread_pitch"], 0.635)
        self.assertEqual(hole["diameter"], 2.35)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_unf_and_unef_profiles_use_tap_drill_tables(self) -> None:
        for fixture, thread_type, thread_size, thread_diameter, thread_pitch, diameter in [
            ("hole-threaded-unf-cosmetic", "UNF", "#4", 2.845, 0.529, 2.40),
            ("hole-threaded-unef-cosmetic", "UNEF", "#12", 5.486, 0.794, 4.80),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]
                body = result["objects"]["Body"]
                radius = diameter / 2.0
                expected_cut_volume = math.pi * radius * radius * 4.0

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["diameter_source"], "thread_tap_drill")
                self.assertEqual(hole["thread_diameter"], thread_diameter)
                self.assertEqual(hole["thread_pitch"], thread_pitch)
                self.assertEqual(hole["diameter"], diameter)
                self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
                self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_pipe_and_british_profiles_use_freecad_tables(self) -> None:
        for fixture, thread_type, thread_size, thread_diameter, thread_pitch, diameter, source, base_volume in [
            (
                "hole-threaded-npt-cosmetic",
                "NPT",
                "1/16",
                7.938,
                0.941,
                7.938 - (2.0 * (0.8 * 0.941)) * 0.75,
                "thread_npt_fallback",
                20.0 * 20.0 * 10.0,
            ),
            (
                "hole-threaded-bsp-fallback-cosmetic",
                "BSP",
                "1 1/8",
                37.897,
                2.309,
                37.897 - (2.0 * (0.640327 * 2.309)) * 0.75,
                "thread_whitworth_fallback",
                50.0 * 50.0 * 10.0,
            ),
            (
                "hole-threaded-bsw-cosmetic",
                "BSW",
                "1/8",
                3.175,
                0.635,
                2.55,
                "thread_tap_drill",
                20.0 * 20.0 * 10.0,
            ),
            (
                "hole-threaded-bsf-cosmetic",
                "BSF",
                "3/16",
                4.763,
                0.794,
                4.00,
                "thread_tap_drill",
                20.0 * 20.0 * 10.0,
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]
                body = result["objects"]["Body"]
                radius = diameter / 2.0
                expected_cut_volume = math.pi * radius * radius * 4.0

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], True)
                self.assertEqual(hole["model_thread"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["diameter_source"], source)
                self.assertEqual(hole["thread_diameter"], thread_diameter)
                self.assertEqual(hole["thread_pitch"], thread_pitch)
                self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
                self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
                self.assertAlmostEqual(body["volume"], base_volume - expected_cut_volume, delta=1e-6)

    def test_p7_hole_threaded_isotyre_profile_uses_pitch_fallback(self) -> None:
        result = self.run_recompute("hole-threaded-isotyre-cosmetic", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        diameter = 5.334 - 0.705
        expected_cut_volume = math.pi * (diameter / 2.0) * (diameter / 2.0) * 4.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], True)
        self.assertEqual(hole["model_thread"], False)
        self.assertEqual(hole["thread_type"], "ISOTyre")
        self.assertEqual(hole["thread_size"], "5v1")
        self.assertEqual(hole["diameter_source"], "thread_pitch_fallback")
        self.assertEqual(hole["thread_diameter"], 5.334)
        self.assertEqual(hole["thread_pitch"], 0.705)
        self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assertAlmostEqual(body["volume"], 20.0 * 20.0 * 10.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_thread_clearance_uses_iso_metric_fit_table(self) -> None:
        result = self.run_recompute("hole-thread-clearance", "p7")
        hole = result["objects"]["Hole"]
        body = result["objects"]["Body"]
        expected_cut_volume = math.pi * 2.4 * 2.4 * 4.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(hole["threaded"], False)
        self.assertEqual(hole["thread_type"], "ISOMetricProfile")
        self.assertEqual(hole["thread_size"], "M4x0.7")
        self.assertEqual(hole["thread_fit"], "Coarse")
        self.assertEqual(hole["diameter_source"], "thread_clearance")
        self.assertEqual(hole["thread_diameter"], 4.0)
        self.assertEqual(hole["thread_pitch"], 0.7)
        self.assertEqual(hole["diameter"], 4.8)
        self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
        self.assertAlmostEqual(body["volume"], 500.0 - expected_cut_volume, delta=1e-6)

    def test_p7_hole_thread_clearance_uses_uts_table_and_generic_fallback(self) -> None:
        for fixture, thread_type, thread_size, diameter, source, base_volume in [
            ("hole-unc-clearance", "UNC", "#4", 3.3, "thread_uts_clearance", 10.0 * 5.0 * 10.0),
            (
                "hole-isotyre-clearance-fallback",
                "ISOTyre",
                "5v1",
                5.334 * 1.10,
                "thread_clearance_fallback",
                20.0 * 20.0 * 10.0,
            ),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                hole = result["objects"]["Hole"]
                body = result["objects"]["Body"]
                expected_cut_volume = math.pi * (diameter / 2.0) * (diameter / 2.0) * 4.0

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(hole["threaded"], False)
                self.assertEqual(hole["thread_type"], thread_type)
                self.assertEqual(hole["thread_size"], thread_size)
                self.assertEqual(hole["thread_fit"], "Normal")
                self.assertEqual(hole["diameter_source"], source)
                self.assertAlmostEqual(hole["diameter"], diameter, delta=1e-9)
                self.assertAlmostEqual(hole["volume"], expected_cut_volume, delta=1e-6)
                self.assertAlmostEqual(body["volume"], base_volume - expected_cut_volume, delta=1e-6)

    def test_p7_hole_without_base_and_threaded_gaps_are_explicit(self) -> None:
        result = self.run_recompute("hole-without-base", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "execution_failed")
        self.assertEqual(diagnostic["object"], "Hole")
        self.assertEqual(diagnostic["property"], "Profile")

        result = self.run_recompute("hole-threaded-known-gap", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "unsupported_property")
        self.assertEqual(diagnostic["object"], "Hole")
        self.assertEqual(diagnostic["property"], "ModelThread")
        self.assertIn("Hole::makeThread", diagnostic["message"])

    def test_p7_fillet_replaces_body_tip_shape(self) -> None:
        result = self.run_recompute("fillet-pad-edge", "p7")
        fillet = result["objects"]["Fillet"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["dress_up"], "fillet")
        self.assertEqual(fillet["body_mode"], "replace")
        self.assertEqual(body["tip"], "Fillet")
        self.assertEqual(body["bbox"], fillet["bbox"])
        self.assertAlmostEqual(body["volume"], 499.4634954084936, delta=1e-6)
        self.assertAlmostEqual(fillet["volume"], body["volume"], delta=1e-6)
        self.assertLess(body["volume"], 500.0)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Pad.") for key in named_shape["element_map"]))

    def test_p7_chamfer_replaces_body_tip_shape(self) -> None:
        result = self.run_recompute("chamfer-pad-edge", "p7")
        chamfer = result["objects"]["Chamfer"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(chamfer["status"], "ok")
        self.assertEqual(chamfer["dress_up"], "chamfer")
        self.assertEqual(chamfer["body_mode"], "replace")
        self.assertEqual(body["tip"], "Chamfer")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [10.0, 5.0, 10.0])
        self.assertAlmostEqual(body["volume"], 498.75, delta=1e-6)
        self.assertAlmostEqual(chamfer["volume"], body["volume"], delta=1e-6)
        self.assertLess(body["volume"], 500.0)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Pad.") for key in named_shape["element_map"]))

    def test_p7_dressup_refine_true_uses_refinemodel_path(self) -> None:
        for fixture, object_name, expected_volume in [
            ("fillet-refine-true", "Fillet", 499.4634954084936),
            ("chamfer-refine-true", "Chamfer", 498.75),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p7")
                dress_up = result["objects"][object_name]
                body = result["objects"]["Body"]
                named_shape = result["named_shapes"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(dress_up["status"], "ok")
                self.assertEqual(dress_up["refine"], "applied")
                self.assertEqual(body["tip"], object_name)
                self.assertAlmostEqual(dress_up["volume"], expected_volume, delta=1e-6)
                self.assertAlmostEqual(body["volume"], expected_volume, delta=1e-6)
                self.assertEqual(named_shape["element_map_status"], "history_partial")

    def test_p7_dressup_base_diagnostics_are_structured(self) -> None:
        result = self.run_recompute("fillet-missing-edge", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_subshape")
        self.assertEqual(diagnostic["object"], "Fillet")
        self.assertEqual(diagnostic["property"], "Base")
        self.assertEqual(diagnostic["target"], "Pad")
        self.assertEqual(diagnostic["subname"], "Edge99")

        result = self.run_recompute("chamfer-invalid-size", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "Chamfer")
        self.assertEqual(diagnostic["property"], "Size")

    def test_p7_mirrored_features_mode_fuses_transformed_additive_original(self) -> None:
        result = self.run_recompute("mirrored-pad-datum-plane", "p7")
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Pad"])
        self.assertEqual(mirrored["body_mode"], "replace")
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [4.0, 2.0, 2.0])
        self.assertAlmostEqual(body["volume"], 16.0, delta=1e-6)
        self.assertAlmostEqual(mirrored["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Pad.") for key in named_shape["element_map"]))
        self.assertTrue(any(key.startswith("SketchPad.") for key in named_shape["element_map"]))

    def test_p7_transformed_refine_true_uses_refinemodel_path(self) -> None:
        result = self.run_recompute("mirrored-refine-true", "p7")
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["refine"], "applied")
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [4.0, 2.0, 2.0])
        self.assertAlmostEqual(body["volume"], 16.0, delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")

    def test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache(self) -> None:
        result = self.run_recompute("mirrored-fillet-support-transform", "p7")
        fillet = result["objects"]["Fillet"]
        mirrored = result["objects"]["Mirrored"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fillet["status"], "ok")
        self.assertEqual(fillet["support_transform"], True)
        self.assertEqual(fillet["add_sub_cache"], "support_transform")
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Features")
        self.assertEqual(mirrored["originals"], ["Fillet"])
        self.assertEqual(body["tip"], "Mirrored")
        self.assert_bbox_close_delta(body["bbox"], [0.0, 0.0, 0.0], [4.0, 2.0, 2.0], 2e-2)
        self.assertAlmostEqual(body["volume"], fillet["volume"] * 2.0, delta=1e-6)
        self.assertAlmostEqual(mirrored["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Fillet.") for key in named_shape["element_map"]))

    def test_p7_mirrored_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("mirrored-whole-shape", "p7")
        mirrored = result["objects"]["Mirrored"]
        named_shape = result["named_shapes"]["Mirrored"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(mirrored["status"], "ok")
        self.assertEqual(mirrored["transformed"], "mirrored")
        self.assertEqual(mirrored["transform_mode"], "Whole shape")
        self.assertEqual(mirrored["originals"], ["Pad"])
        self.assert_bbox_close(mirrored["bbox"], [0.0, 0.0, 0.0], [4.0, 2.0, 2.0])
        self.assertAlmostEqual(mirrored["volume"], 16.0, delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Pad.") for key in named_shape["element_map"]))

    def test_p7_linear_pattern_features_mode_fuses_additive_originals_by_extent(self) -> None:
        result = self.run_recompute("linear-pattern-pad-datum-line", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [6.0, 2.0, 2.0])
        self.assertAlmostEqual(body["volume"], 24.0, delta=1e-6)
        self.assertAlmostEqual(pattern["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Pad.") for key in named_shape["element_map"]))

    def test_p7_linear_pattern_uses_sketch_construction_axis(self) -> None:
        result = self.run_recompute("linear-pattern-pad-sketch-axis", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [2.0, 6.0, 2.0])
        self.assertAlmostEqual(body["volume"], 24.0, delta=1e-6)
        self.assertAlmostEqual(pattern["volume"], body["volume"], delta=1e-6)

    def test_p7_linear_pattern_combines_two_directions(self) -> None:
        result = self.run_recompute("linear-pattern-pad-two-directions", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [6.0, 5.0, 2.0])
        self.assertAlmostEqual(body["volume"], 48.0, delta=1e-6)
        self.assertAlmostEqual(pattern["volume"], body["volume"], delta=1e-6)

    def test_p7_linear_pattern_custom_spacing_list_controls_steps(self) -> None:
        result = self.run_recompute("linear-pattern-custom-spacings", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [9.0, 2.0, 2.0])
        self.assertAlmostEqual(body["volume"], 24.0, delta=1e-6)

    def test_p7_linear_pattern_spacing_pattern_controls_steps(self) -> None:
        result = self.run_recompute("linear-pattern-spacing-pattern", "p7")
        pattern = result["objects"]["LinearPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(body["tip"], "LinearPattern")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [12.0, 2.0, 2.0])
        self.assertAlmostEqual(body["volume"], 32.0, delta=1e-6)

    def test_p7_linear_pattern_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("linear-pattern-whole-shape", "p7")
        pattern = result["objects"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "linear_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assert_bbox_close(pattern["bbox"], [0.0, 0.0, 0.0], [6.0, 2.0, 2.0])
        self.assertAlmostEqual(pattern["volume"], 24.0, delta=1e-6)

    def test_p7_polar_pattern_features_mode_rotates_additive_originals_by_extent(self) -> None:
        result = self.run_recompute("polar-pattern-pad-datum-line", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assertEqual(pattern["body_mode"], "replace")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_bbox_close(body["bbox"], [-3.0, -3.0, 0.0], [3.0, 3.0, 2.0])
        self.assertAlmostEqual(body["volume"], 8.0, delta=1e-6)
        self.assertAlmostEqual(pattern["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("PolarPattern.Transform") for key in named_shape["element_map"]))

    def test_p7_polar_pattern_uses_sketch_normal_axis(self) -> None:
        result = self.run_recompute("polar-pattern-pad-sketch-axis", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Features")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_bbox_close(body["bbox"], [-3.0, -3.0, 0.0], [3.0, 3.0, 2.0])
        self.assertAlmostEqual(body["volume"], 8.0, delta=1e-6)
        self.assertAlmostEqual(pattern["volume"], body["volume"], delta=1e-6)

    def test_p7_polar_pattern_spacing_pattern_controls_angles(self) -> None:
        result = self.run_recompute("polar-pattern-spacing-pattern", "p7")
        pattern = result["objects"]["PolarPattern"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(body["tip"], "PolarPattern")
        self.assert_bbox_close(body["bbox"], [-1.0, -3.0, 0.0], [3.0, 3.0, 2.0])
        self.assertAlmostEqual(body["volume"], 6.0, delta=1e-6)

    def test_p7_polar_pattern_whole_shape_fuses_transformed_support(self) -> None:
        result = self.run_recompute("polar-pattern-whole-shape", "p7")
        pattern = result["objects"]["PolarPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(pattern["status"], "ok")
        self.assertEqual(pattern["transformed"], "polar_pattern")
        self.assertEqual(pattern["transform_mode"], "Whole shape")
        self.assertEqual(pattern["originals"], ["Pad"])
        self.assert_bbox_close(pattern["bbox"], [-3.0, -3.0, 0.0], [3.0, 3.0, 2.0])
        self.assertAlmostEqual(pattern["volume"], 8.0, delta=1e-6)

    def test_p7_scaled_features_mode_scales_around_first_original_center_of_mass(self) -> None:
        result = self.run_recompute("scaled-pad-factor-two", "p7")
        scaled = result["objects"]["Scaled"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(scaled["status"], "ok")
        self.assertEqual(scaled["transformed"], "scaled")
        self.assertEqual(scaled["transform_mode"], "Features")
        self.assertEqual(scaled["originals"], ["Pad"])
        self.assertEqual(scaled["body_mode"], "replace")
        self.assertEqual(body["tip"], "Scaled")
        self.assert_bbox_close(body["bbox"], [-1.0, -1.0, -1.0], [3.0, 3.0, 3.0])
        self.assertAlmostEqual(body["volume"], 64.0, delta=1e-6)
        self.assertAlmostEqual(scaled["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("Scaled.Transform") for key in named_shape["element_map"]))

    def test_p7_scaled_diagnostics_are_structured(self) -> None:
        result = self.run_recompute("scaled-invalid-factor", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "Scaled")
        self.assertEqual(diagnostic["property"], "Factor")

    def test_p7_scaled_whole_shape_scales_support_around_origin(self) -> None:
        result = self.run_recompute("scaled-whole-shape", "p7")
        scaled = result["objects"]["Scaled"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(scaled["status"], "ok")
        self.assertEqual(scaled["transformed"], "scaled")
        self.assertEqual(scaled["transform_mode"], "Whole shape")
        self.assertEqual(scaled["originals"], ["Pad"])
        self.assert_bbox_close(scaled["bbox"], [0.0, 0.0, 0.0], [4.0, 4.0, 4.0])
        self.assertAlmostEqual(scaled["volume"], 64.0, delta=1e-6)

    def test_p7_multi_transform_combines_linear_pattern_and_mirror(self) -> None:
        result = self.run_recompute("multi-transform-linear-mirror", "p7")
        multi = result["objects"]["MultiTransform"]
        body = result["objects"]["Body"]
        named_shape = result["named_shapes"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["LinearPattern"]["transformation_template"], True)
        self.assertEqual(result["objects"]["Mirrored"]["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Features")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assertEqual(multi["body_mode"], "replace")
        self.assertEqual(body["tip"], "MultiTransform")
        self.assert_bbox_close(body["bbox"], [0.0, 0.0, 0.0], [6.0, 6.0, 2.0])
        self.assertAlmostEqual(body["volume"], 48.0, delta=1e-6)
        self.assertAlmostEqual(multi["volume"], body["volume"], delta=1e-6)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(key.startswith("MultiTransform.Transform") for key in named_shape["element_map"]))

    def test_p7_multi_transform_scaled_child_uses_diagonal_composition(self) -> None:
        result = self.run_recompute("multi-transform-scaled-diagonal", "p7")
        multi = result["objects"]["MultiTransform"]
        body = result["objects"]["Body"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["LinearPattern"]["transformation_template"], True)
        self.assertEqual(result["objects"]["Scaled"]["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(body["tip"], "MultiTransform")
        self.assert_bbox_close_delta(body["bbox"], [0.0, -0.5, -0.5], [7.5, 1.5, 1.5], 1e-5)
        self.assertAlmostEqual(body["volume"], 12.375, delta=1e-6)

    def test_p7_multi_transform_scaled_divisor_gap_is_explicit(self) -> None:
        result = self.run_recompute("multi-transform-scaled-divisor-known-gap", "p7")
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["code"], "invalid_length")
        self.assertEqual(diagnostic["object"], "MultiTransform")
        self.assertEqual(diagnostic["property"], "Transformations")
        self.assertIn("divisor", diagnostic["message"])

    def test_p7_multi_transform_whole_shape_uses_support_and_child_transforms(self) -> None:
        result = self.run_recompute("multi-transform-whole-shape", "p7")
        multi = result["objects"]["MultiTransform"]
        child = result["objects"]["LinearPattern"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(child["transformation_template"], True)
        self.assertEqual(multi["status"], "ok")
        self.assertEqual(multi["transformed"], "multi_transform")
        self.assertEqual(multi["transform_mode"], "Whole shape")
        self.assertEqual(multi["originals"], ["Pad"])
        self.assert_bbox_close(multi["bbox"], [0.0, 0.0, 0.0], [3.0, 1.0, 1.0])
        self.assertAlmostEqual(multi["volume"], 3.0, delta=1e-6)

    def test_p8_part_box_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-box", "p8")
        box = result["objects"]["Box"]
        named_shape = result["named_shapes"]["Box"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(box["status"], "ok")
        self.assertEqual(box["primitive"], "box")
        self.assert_bbox_close(box["bbox"], [0.0, 0.0, 0.0], [2.0, 3.0, 4.0])
        self.assertAlmostEqual(box["volume"], 24.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["Box"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertEqual(named_shape["owner"], "Box")
        self.assertIn("Face1", named_shape["elements"])

    def test_p8_app_link_proxies_linked_shape_with_link_placement(self) -> None:
        result = self.run_recompute("app-link-box", "p8")
        link = result["objects"]["BoxLink"]
        named_shape = result["named_shapes"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["link_transform"], False)
        self.assert_bbox_close(link["bbox"], [5.0, 0.0, 0.0], [7.0, 3.0, 4.0])
        self.assertAlmostEqual(link["volume"], 24.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["BoxLink"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertEqual(named_shape["owner"], "BoxLink")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Box.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["Box.Edge1"], "Edge1")
        self.assertEqual(named_shape["element_map"]["Box.Vertex1"], "Vertex1")

        transformed = self.run_recompute("app-link-box-transform", "p8")["objects"]["BoxLink"]
        self.assertEqual(transformed["link_transform"], True)
        self.assert_bbox_close(transformed["bbox"], [15.0, 0.0, 0.0], [17.0, 3.0, 4.0])

        scaled = self.run_recompute("app-link-box-scale", "p8")["objects"]["BoxLink"]
        self.assert_bbox_close(scaled["bbox"], [5.0, 0.0, 0.0], [9.0, 6.0, 8.0])
        self.assertAlmostEqual(scaled["volume"], 192.0, delta=1e-6)

    def test_p8_app_link_subshape_uses_linked_object_sublist(self) -> None:
        result = self.run_recompute("app-link-box-face", "p8")
        link = result["objects"]["BoxLink"]
        named_shape = result["named_shapes"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["link_transform"], False)
        self.assertEqual(link["shape"], "occt_face")
        self.assertAlmostEqual(link["volume"], 0.0, delta=1e-9)
        self.assert_topology_counts(result["subshapes"]["BoxLink"], {"topology_counts": {"faces": 1, "edges": 4, "vertices": 4}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Box.Face1"], "Face1")

    def test_p8_app_link_subshape_compounds_multiple_sublist_items(self) -> None:
        result = self.run_recompute("app-link-box-multi-face", "p8")
        link = result["objects"]["FaceLink"]
        named_shape = result["named_shapes"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_compound")
        self.assert_bbox_close(link["bbox"], [0.0, 10.0, 0.0], [2.0, 13.0, 4.0])
        self.assert_topology_counts(result["subshapes"]["FaceLink"], {"topology_counts": {"faces": 2, "edges": 8, "vertices": 8}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Box.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["Box.Face2"], "Face2")

    def test_p8_app_link_element_proxies_linked_shape(self) -> None:
        result = self.run_recompute("app-link-element-box", "p8")
        element = result["objects"]["BoxElement"]
        named_shape = result["named_shapes"]["BoxElement"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["link"], "app_link_element")
        self.assertEqual(element["linked_object"], "Box")
        self.assertEqual(element["link_transform"], False)
        self.assert_bbox_close(element["bbox"], [2.0, 0.0, 0.0], [4.0, 3.0, 4.0])
        self.assertAlmostEqual(element["volume"], 24.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["BoxElement"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["Box.Face1"], "Face1")

    def test_p8_app_link_group_compounds_element_shapes(self) -> None:
        result = self.run_recompute("app-link-group-elements", "p8")
        group = result["objects"]["LinkGroup"]
        named_shape = result["named_shapes"]["LinkGroup"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["visible_elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assert_bbox_close(group["bbox"], [0.0, 10.0, 0.0], [7.0, 13.0, 4.0])
        self.assertAlmostEqual(group["volume"], 48.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["LinkGroup"], {"topology_counts": {"faces": 12, "edges": 24, "vertices": 16}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["LinkA.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["LinkB.Face1"], "Face7")

    def test_p8_app_link_resolves_group_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-group-subshape-alias", "p8")
        link = result["objects"]["FaceLink"]
        named_shape = result["named_shapes"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assertAlmostEqual(link["volume"], 0.0, delta=1e-9)
        self.assert_topology_counts(result["subshapes"]["FaceLink"], {"topology_counts": {"faces": 1, "edges": 4, "vertices": 4}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["LinkGroup.LinkB.Face1"], "Face1")

    def test_p8_app_link_group_respects_visibility_list(self) -> None:
        result = self.run_recompute("app-link-group-visibility", "p8")
        group = result["objects"]["LinkGroup"]
        named_shape = result["named_shapes"]["LinkGroup"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["visible_elements"], ["LinkA"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assert_bbox_close(group["bbox"], [0.0, 10.0, 0.0], [2.0, 13.0, 4.0])
        self.assertAlmostEqual(group["volume"], 24.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["LinkGroup"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["LinkA.Face1"], "Face1")
        self.assertNotIn("LinkB.Face1", named_shape["element_map"])

    def test_p8_app_link_element_count_compounds_collapsed_elements(self) -> None:
        result = self.run_recompute("app-link-element-count-collapsed", "p8")
        group = result["objects"]["ArrayLink"]
        named_shape = result["named_shapes"]["ArrayLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["linked_object"], "Box")
        self.assertEqual(group["element_count"], 3)
        self.assertEqual(group["collapsed_elements"], True)
        self.assertEqual(group["visible_indices"], [0, 1])
        self.assertEqual(group["shape"], "occt_compound")
        self.assert_bbox_close(group["bbox"], [0.0, 10.0, 0.0], [9.0, 13.0, 4.0])
        self.assertAlmostEqual(group["volume"], 72.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["ArrayLink"], {"topology_counts": {"faces": 12, "edges": 24, "vertices": 16}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["ArrayLink_i0.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["ArrayLink_i1.Face1"], "Face7")
        self.assertNotIn("ArrayLink_i2.Face1", named_shape["element_map"])

    def test_p8_app_link_show_element_groups_materialized_children(self) -> None:
        result = self.run_recompute("app-link-show-element-materialized", "p8")
        group = result["objects"]["ArrayLink"]
        named_shape = result["named_shapes"]["ArrayLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["ArrayLink_i0"]["status"], "ok")
        self.assertEqual(result["objects"]["ArrayLink_i1"]["status"], "ok")
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["element_count"], 2)
        self.assertEqual(group["materialized_elements"], True)
        self.assertEqual(group["elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assert_bbox_close(group["bbox"], [0.0, 10.0, 0.0], [7.0, 13.0, 4.0])
        self.assertAlmostEqual(group["volume"], 48.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["ArrayLink"], {"topology_counts": {"faces": 12, "edges": 24, "vertices": 16}})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertEqual(named_shape["element_map"]["ArrayLink_i0.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["ArrayLink_i1.Face1"], "Face7")

    def test_p8_assembly_object_groups_basic_component_link(self) -> None:
        result = self.run_recompute("assembly-link-basic", "p8")
        component = result["objects"]["ComponentLink"]
        assembly = result["objects"]["Assembly"]
        assembly_named_shape = result["named_shapes"]["Assembly"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(component["status"], "ok")
        self.assertEqual(component["link"], "assembly_link")
        self.assertEqual(component["linked_object"], "Box")
        self.assertEqual(component["rigid"], True)
        self.assert_bbox_close(component["bbox"], [0.0, 5.0, 0.0], [2.0, 8.0, 4.0])

        self.assertEqual(assembly["status"], "ok")
        self.assertEqual(assembly["assembly"], "object")
        self.assertEqual(assembly["group"], ["ComponentLink"])
        self.assertEqual(assembly["solve"], "not_migrated")
        self.assert_bbox_close(assembly["bbox"], [0.0, 5.0, 0.0], [2.0, 8.0, 4.0])
        self.assertAlmostEqual(assembly["volume"], 24.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["Assembly"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertEqual(assembly_named_shape["element_map_status"], "history_partial")
        self.assertEqual(assembly_named_shape["element_map"]["ComponentLink.Face1"], "Face1")
        self.assertEqual(assembly_named_shape["element_map"]["Box.Face1"], "Face1")

    def test_p8_part_cylinder_builds_prism_extension_solid(self) -> None:
        result = self.run_recompute("part-cylinder", "p8")
        cylinder = result["objects"]["Cylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cylinder["status"], "ok")
        self.assertEqual(cylinder["primitive"], "cylinder")
        self.assert_bbox_close_delta(cylinder["bbox"], [-2.0, -2.0, 0.0], [2.0, 2.0, 5.0], 2e-2)
        self.assertAlmostEqual(cylinder["volume"], math.pi * 2.0 * 2.0 * 5.0, delta=1e-6)
        self.assertIn("Cylinder", result["named_shapes"])

    def test_p8_part_cylinder_uses_prism_first_angle(self) -> None:
        result = self.run_recompute("part-cylinder-angled-prism", "p8")
        cylinder = result["objects"]["Cylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cylinder["first_angle"], 10.0)
        self.assertAlmostEqual(cylinder["volume"], math.pi * 5.0, delta=1e-6)
        self.assert_bbox_close_delta(cylinder["bbox"], [-1.0, -1.0, 0.0], [1.0 + 5.0 * math.tan(math.radians(10.0)), 1.0, 5.0], 1e-2)

    def test_p8_part_sphere_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-sphere", "p8")
        sphere = result["objects"]["Sphere"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sphere["status"], "ok")
        self.assertEqual(sphere["primitive"], "sphere")
        self.assertEqual(sphere["radius"], 3.0)
        self.assert_bbox_close_delta(sphere["bbox"], [-3.0, -3.0, -3.0], [3.0, 3.0, 3.0], 2e-1)
        self.assertAlmostEqual(sphere["volume"], 4.0 * math.pi * 3.0 * 3.0 * 3.0 / 3.0, delta=1e-6)
        self.assertIn("Sphere", result["named_shapes"])

    def test_p8_part_cone_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-cone", "p8")
        cone = result["objects"]["Cone"]
        expected_volume = math.pi * 6.0 * (2.0 * 2.0 + 2.0 * 4.0 + 4.0 * 4.0) / 3.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cone["status"], "ok")
        self.assertEqual(cone["primitive"], "cone")
        self.assertEqual(cone["radius1"], 2.0)
        self.assertEqual(cone["radius2"], 4.0)
        self.assertEqual(cone["height"], 6.0)
        self.assert_bbox_close_delta(cone["bbox"], [-4.0, -4.0, 0.0], [4.0, 4.0, 6.0], 2e-1)
        self.assertAlmostEqual(cone["volume"], expected_volume, delta=1e-6)
        self.assertIn("Cone", result["named_shapes"])

    def test_p8_part_torus_builds_freecad_revolved_solid(self) -> None:
        result = self.run_recompute("part-torus", "p8")
        torus = result["objects"]["Torus"]
        expected_volume = 2.0 * math.pi * math.pi * 5.0 * 1.0 * 1.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(torus["status"], "ok")
        self.assertEqual(torus["primitive"], "torus")
        self.assertEqual(torus["radius1"], 5.0)
        self.assertEqual(torus["radius2"], 1.0)
        self.assert_bbox_close_delta(torus["bbox"], [-6.0, -6.0, -1.0], [6.0, 6.0, 1.0], 2e-1)
        self.assertAlmostEqual(torus["volume"], expected_volume, delta=1e-6)
        self.assertIn("Torus", result["named_shapes"])

    def test_p8_part_vertex_line_and_plane_build_topological_shapes(self) -> None:
        result = self.run_recompute("part-vertex", "p8")
        vertex = result["objects"]["Vertex"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(vertex["status"], "ok")
        self.assertEqual(vertex["primitive"], "vertex")
        self.assertEqual(vertex["shape"], "occt_vertex")
        self.assert_bbox_close(vertex["bbox"], [1.0, 2.0, 3.0], [1.0, 2.0, 3.0])
        self.assertEqual(vertex["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Vertex"], {"topology_counts": {"faces": 0, "edges": 0, "vertices": 1}})

        result = self.run_recompute("part-line", "p8")
        line = result["objects"]["Line"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(line["status"], "ok")
        self.assertEqual(line["primitive"], "line")
        self.assertEqual(line["shape"], "occt_edge")
        self.assertEqual(line["start"], [0.0, 0.0, 0.0])
        self.assertEqual(line["end"], [1.0, 2.0, 3.0])
        self.assert_bbox_close_delta(line["bbox"], [0.0, 0.0, 0.0], [1.0, 2.0, 3.0], 2e-1)
        self.assertEqual(line["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Line"], {"topology_counts": {"faces": 0, "edges": 1, "vertices": 2}})

        result = self.run_recompute("part-plane", "p8")
        plane = result["objects"]["Plane"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(plane["status"], "ok")
        self.assertEqual(plane["primitive"], "plane")
        self.assertEqual(plane["shape"], "occt_face")
        self.assertEqual(plane["length"], 4.0)
        self.assertEqual(plane["width"], 3.0)
        self.assert_bbox_close(plane["bbox"], [0.0, 0.0, 0.0], [4.0, 3.0, 0.0])
        self.assertEqual(plane["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Plane"], {"topology_counts": {"faces": 1, "edges": 4, "vertices": 4}})

    def test_p8_part_import_brep_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-brep", "p8")
        imported = result["objects"]["ImportedCylinder"]
        named_shape = result["named_shapes"]["ImportedCylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_brep")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/cylinder1.brep")
        self.assert_bbox_close_delta(
            imported["bbox"],
            [-2.014582351803892, -2.0, -0.014582351803892262],
            [2.014582351803892, 2.0, 10.014582351803892],
            1e-6,
        )
        self.assertAlmostEqual(imported["volume"], 125.66370614359178, delta=1e-6)
        self.assert_topology_counts(
            result["subshapes"]["ImportedCylinder"],
            {"topology_counts": {"faces": 3, "edges": 3, "vertices": 2}},
        )
        self.assertEqual(named_shape["owner"], "ImportedCylinder")
        self.assertEqual(named_shape["element_map_status"], "indexed_only")

    def test_p8_part_import_step_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-step", "p8")
        imported = result["objects"]["ImportedStep"]
        named_shape = result["named_shapes"]["ImportedStep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_step")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/as1-ac-214_small.stp")
        self.assert_bbox_close_delta(
            imported["bbox"],
            [122.48640506238414, 25.0, -7.036455729509775],
            [175.02511031877214, 125.0, 80.0],
            1e-6,
        )
        self.assertAlmostEqual(imported["volume"], 104939.95611579117, delta=1e-6)
        self.assert_topology_counts(
            result["subshapes"]["ImportedStep"],
            {"topology_counts": {"faces": 62, "edges": 134, "vertices": 84}},
        )
        self.assertEqual(named_shape["owner"], "ImportedStep")
        self.assertEqual(named_shape["element_map_status"], "indexed_only")

    def test_p8_part_import_iges_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-iges", "p8")
        imported = result["objects"]["ImportedIges"]
        named_shape = result["named_shapes"]["ImportedIges"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_iges")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/rlf_12545.igs")
        self.assert_bbox_close_delta(
            imported["bbox"],
            [-6.238209180579999, -6.25164176738, 0.009999888938014427],
            [6.2617910739800005, 6.24835848718, 4.71000012118],
            1e-6,
        )
        self.assertAlmostEqual(imported["volume"], 768.697234526593, delta=1e-6)
        self.assert_topology_counts(
            result["subshapes"]["ImportedIges"],
            {"topology_counts": {"faces": 47, "edges": 240, "vertices": 240}},
        )
        self.assertEqual(named_shape["owner"], "ImportedIges")
        self.assertEqual(named_shape["element_map_status"], "indexed_only")

    def test_p8_mesh_import_stl_reads_mesh_file_shape(self) -> None:
        result = self.run_recompute("mesh-import-stl", "p8")
        imported = result["objects"]["ImportedStl"]
        mesh = result["mesh"]["ImportedStl"]
        named_shape = result["named_shapes"]["ImportedStl"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_stl")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/unit-square.stl")
        self.assert_bbox_close(imported["bbox"], [0.0, 0.0, 0.0], [1.0, 1.0, 0.0])
        self.assertEqual(imported["volume"], 0.0)
        self.assertEqual(mesh["summary"]["vertex_count"], 4)
        self.assertEqual(mesh["summary"]["triangle_count"], 2)
        self.assert_topology_counts(
            result["subshapes"]["ImportedStl"],
            {"topology_counts": {"faces": 2, "edges": 5, "vertices": 4}},
        )
        self.assertEqual(named_shape["owner"], "ImportedStl")
        self.assertEqual(named_shape["element_map_status"], "indexed_only")

    def test_p8_cli_exports_recomputed_shape_files(self) -> None:
        cases = {
            "brep": ("Part::ImportBrep", "ExportedBrep", "box.brep"),
            "step": ("Part::ImportStep", "ExportedStep", "box.step"),
            "stl": ("Mesh::Import", "ExportedStl", "box.stl"),
        }

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            for export_format, (type_id, object_name, file_name) in cases.items():
                with self.subTest(export_format=export_format):
                    export_path = tmp_path / file_name
                    result = self.run_recompute_file(
                        ROOT / "fixtures" / "p8" / "part-box.json",
                        [
                            "--export-object",
                            "Box",
                            "--export-format",
                            export_format,
                            "--export-file",
                            str(export_path),
                        ],
                    )

                    self.assertEqual(result["diagnostics"], [])
                    self.assertEqual(
                        result["exports"],
                        [{"object": "Box", "format": export_format, "file": str(export_path)}],
                    )
                    self.assertTrue(export_path.exists())
                    self.assertGreater(export_path.stat().st_size, 0)

                    import_request = {
                        "Objects": [
                            {
                                "Name": object_name,
                                "ID": 1,
                                "TypeId": type_id,
                                "Properties": {"FileName": str(export_path)},
                            }
                        ],
                        "recompute": {"objs": [object_name]},
                    }
                    import_path = tmp_path / f"import-{export_format}.json"
                    import_path.write_text(json.dumps(import_request), encoding="utf-8")
                    imported_result = self.run_recompute_file(import_path)
                    imported = imported_result["objects"][object_name]

                    self.assertEqual(imported_result["diagnostics"], [])
                    self.assertEqual(imported["status"], "ok")
                    self.assert_bbox_close_delta(imported["bbox"], [0.0, 0.0, 0.0], [2.0, 3.0, 4.0], 1e-6)
                    if export_format == "stl":
                        self.assertEqual(imported["primitive"], "import_stl")
                        self.assertGreater(imported_result["mesh"][object_name]["summary"]["triangle_count"], 0)
                    else:
                        self.assertAlmostEqual(imported["volume"], 24.0, delta=1e-6)

    def test_p8_part_prism_builds_regular_polygon_solid(self) -> None:
        result = self.run_recompute("part-prism", "p8")
        prism = result["objects"]["Prism"]
        expected_area = 6.0 * 2.0 * 2.0 * math.sin(2.0 * math.pi / 6.0) / 2.0

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(prism["status"], "ok")
        self.assertEqual(prism["primitive"], "prism")
        self.assertEqual(prism["polygon"], 6)
        self.assertEqual(prism["circumradius"], 2.0)
        self.assertEqual(prism["height"], 5.0)
        self.assert_bbox_close_delta(prism["bbox"], [-2.0, -math.sqrt(3.0), 0.0], [2.0, math.sqrt(3.0), 5.0], 1e-6)
        self.assertAlmostEqual(prism["volume"], expected_area * 5.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["Prism"], {"topology_counts": {"faces": 8, "edges": 18, "vertices": 12}})
        self.assertIn("Prism", result["named_shapes"])

    def test_p8_part_regular_polygon_builds_wire(self) -> None:
        result = self.run_recompute("part-regular-polygon", "p8")
        polygon = result["objects"]["RegularPolygon"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(polygon["status"], "ok")
        self.assertEqual(polygon["primitive"], "regular_polygon")
        self.assertEqual(polygon["shape"], "occt_wire")
        self.assertEqual(polygon["polygon"], 6)
        self.assertEqual(polygon["circumradius"], 2.0)
        self.assert_bbox_close_delta(polygon["bbox"], [-2.0, -math.sqrt(3.0), 0.0], [2.0, math.sqrt(3.0), 0.0], 2e-1)
        self.assertEqual(polygon["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["RegularPolygon"], {"topology_counts": {"faces": 0, "edges": 6, "vertices": 6}})
        self.assertIn("RegularPolygon", result["named_shapes"])

    def test_p8_part_ellipsoid_builds_scaled_sphere_solid(self) -> None:
        result = self.run_recompute("part-ellipsoid", "p8")
        ellipsoid = result["objects"]["Ellipsoid"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ellipsoid["status"], "ok")
        self.assertEqual(ellipsoid["primitive"], "ellipsoid")
        self.assertEqual(ellipsoid["radius1"], 2.0)
        self.assertEqual(ellipsoid["radius2"], 4.0)
        self.assertEqual(ellipsoid["radius3"], 3.0)
        self.assert_bbox_close_delta(ellipsoid["bbox"], [-4.0, -3.0, -2.0], [4.0, 3.0, 2.0], 2.5e-1)
        self.assertAlmostEqual(ellipsoid["volume"], 32.0 * math.pi, delta=1e-1)
        self.assert_topology_counts(result["subshapes"]["Ellipsoid"], {"topology_counts": {"faces": 1, "edges": 3, "vertices": 2}})
        self.assertIn("Ellipsoid", result["named_shapes"])

    def test_p8_part_wedge_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-wedge", "p8")
        wedge = result["objects"]["Wedge"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(wedge["status"], "ok")
        self.assertEqual(wedge["primitive"], "wedge")
        self.assert_bbox_close(wedge["bbox"], [0.0, 0.0, 0.0], [10.0, 10.0, 10.0])
        self.assertAlmostEqual(wedge["volume"], 653.3333333333333, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["Wedge"], {"topology_counts": {"faces": 6, "edges": 12, "vertices": 8}})
        self.assertIn("Wedge", result["named_shapes"])

    def test_p8_part_binary_booleans_build_occt_solids(self) -> None:
        cases = {
            "part-fuse": ("Fuse", "fuse", [0.0, 0.0, 0.0], [3.0, 2.0, 2.0], 12.0, {"faces": 14, "edges": 28, "vertices": 16}),
            "part-cut": ("Cut", "cut", [0.0, 0.0, 0.0], [1.0, 2.0, 2.0], 4.0, {"faces": 6, "edges": 12, "vertices": 8}),
            "part-common": ("Common", "common", [1.0, 0.0, 0.0], [2.0, 2.0, 2.0], 4.0, {"faces": 6, "edges": 12, "vertices": 8}),
        }

        for fixture, (object_name, operation, bbox_min, bbox_max, volume, counts) in cases.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                obj = result["objects"][object_name]
                named_shape = result["named_shapes"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(obj["status"], "ok")
                self.assertEqual(obj["boolean"], operation)
                self.assertEqual(obj["base"], "BaseBox")
                self.assertEqual(obj["tool"], "ToolBox")
                self.assert_bbox_close(obj["bbox"], bbox_min, bbox_max)
                self.assertAlmostEqual(obj["volume"], volume, delta=1e-6)
                self.assert_topology_counts(result["subshapes"][object_name], {"topology_counts": counts})
                self.assertEqual(named_shape["owner"], object_name)
                self.assertIn("Face1", named_shape["elements"])

    def test_p8_part_multi_booleans_build_occt_solids(self) -> None:
        cases = {
            "part-multi-fuse": (
                "MultiFuse",
                "multi_fuse",
                None,
                [0.0, 0.0, 0.0],
                [4.0, 2.0, 2.0],
                16.0,
                {"faces": 18, "edges": 36, "vertices": 20},
            ),
            "part-multi-common": (
                "MultiCommon",
                "multi_common",
                "CommonOfAllShapes",
                [2.0, 0.0, 0.0],
                [3.0, 2.0, 2.0],
                4.0,
                {"faces": 6, "edges": 12, "vertices": 8},
            ),
            "part-multi-common-first-rest": (
                "MultiCommon",
                "multi_common",
                "CommonOfFirstAndRest",
                [1.0, 0.0, 0.0],
                [4.0, 2.0, 2.0],
                12.0,
                {"faces": 16, "edges": 28, "vertices": 16},
            ),
        }

        for fixture, (object_name, operation, behavior, bbox_min, bbox_max, volume, counts) in cases.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                obj = result["objects"][object_name]
                named_shape = result["named_shapes"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(obj["status"], "ok")
                self.assertEqual(obj["boolean"], operation)
                self.assertEqual(obj["shapes"], ["BoxA", "BoxB", "BoxC"])
                if behavior is not None:
                    self.assertEqual(obj["behavior"], behavior)
                self.assert_bbox_close(obj["bbox"], bbox_min, bbox_max)
                self.assertAlmostEqual(obj["volume"], volume, delta=1e-6)
                self.assert_topology_counts(result["subshapes"][object_name], {"topology_counts": counts})
                self.assertEqual(named_shape["owner"], object_name)
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertIn("Face1", named_shape["elements"])

    def test_p8_part_xor_builds_compound_from_odd_coverage_pieces(self) -> None:
        result = self.run_recompute("part-xor", "p8")
        xor = result["objects"]["XOR"]
        named_shape = result["named_shapes"]["XOR"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(xor["status"], "ok")
        self.assertEqual(xor["boolean"], "xor")
        self.assertEqual(xor["shape"], "occt_compound")
        self.assertEqual(xor["objects"], ["BoxA", "BoxB"])
        self.assert_bbox_close(xor["bbox"], [0.0, 0.0, 0.0], [3.0, 2.0, 2.0])
        self.assertAlmostEqual(xor["volume"], 8.0, delta=1e-6)
        self.assert_topology_counts(result["subshapes"]["XOR"], {"topology_counts": {"faces": 12, "edges": 24, "vertices": 16}})
        self.assertEqual(named_shape["owner"], "XOR")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("Face1", named_shape["elements"])

    def test_p8_part_boolean_fragments_builds_general_fuse_pieces(self) -> None:
        for fixture, mode in [
            ("part-boolean-fragments", "Standard"),
            ("part-boolean-fragments-split", "Split"),
            ("part-boolean-fragments-compsolid", "CompSolid"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                fragments = result["objects"]["BooleanFragments"]
                named_shape = result["named_shapes"]["BooleanFragments"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(fragments["status"], "ok")
                self.assertEqual(fragments["boolean"], "fragments")
                self.assertEqual(fragments["shape"], "occt_compound")
                self.assertEqual(fragments["mode"], mode)
                self.assertEqual(fragments["objects"], ["BoxA", "BoxB"])
                self.assert_bbox_close(fragments["bbox"], [0.0, 0.0, 0.0], [3.0, 2.0, 2.0])
                self.assertAlmostEqual(fragments["volume"], 12.0, delta=1e-6)
                self.assert_topology_counts(
                    result["subshapes"]["BooleanFragments"],
                    {"topology_counts": {"faces": 16, "edges": 28, "vertices": 16}},
                )
                self.assertEqual(named_shape["owner"], "BooleanFragments")
                self.assertEqual(named_shape["element_map_status"], "history_partial")
                self.assertTrue(any(item["kind"] == "split" for item in named_shape["history"]))
                self.assertTrue(any(item["kind"] == "modified" for item in named_shape["history"]))

    def test_p8_part_boolean_fragments_split_rebuilds_wire_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-wire-split", "p8")
        fragments = result["objects"]["BooleanFragments"]
        named_shape = result["named_shapes"]["BooleanFragments"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["PolyA", "PolyB"])
        self.assert_bbox_close_delta(fragments["bbox"], [-2.1, -2.1, -0.1], [3.1, 2.1, 0.1], 1e-6)
        self.assertEqual(fragments["volume"], 0.0)
        self.assert_topology_counts(
            result["subshapes"]["BooleanFragments"],
            {"topology_counts": {"faces": 0, "edges": 12, "vertices": 10}},
        )
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(item["kind"] == "split" for item in named_shape["history"]))

    def test_p8_part_boolean_fragments_split_rebuilds_compsolid_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-compsolid-split", "p8")
        fragments = result["objects"]["Split"]
        named_shape = result["named_shapes"]["Split"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["Comp", "BoxC"])
        self.assert_bbox_close_delta(
            fragments["bbox"],
            [0.0, 0.0, 0.0],
            [3.0, 2.5000001000000007, 2.0],
            1e-6,
        )
        self.assertAlmostEqual(fragments["volume"], 20.0, delta=1e-6)
        self.assert_topology_counts(
            result["subshapes"]["Split"],
            {"topology_counts": {"faces": 36, "edges": 60, "vertices": 32}},
        )
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(item["kind"] == "split" for item in named_shape["history"]))

    def test_p8_part_boolean_fragments_split_rebuilds_shell_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-shell-split", "p8")
        fragments = result["objects"]["BooleanFragments"]
        named_shape = result["named_shapes"]["BooleanFragments"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["ShellBase", "SplitterFace"])
        self.assert_bbox_close_delta(
            fragments["bbox"],
            [0.0, 0.0, -0.5000001],
            [2.0, 1.0, 0.5000001],
            1e-6,
        )
        self.assertEqual(fragments["volume"], 0.0)
        self.assert_topology_counts(
            result["subshapes"]["BooleanFragments"],
            {"topology_counts": {"faces": 4, "edges": 13, "vertices": 10}},
        )
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertTrue(any(item["kind"] == "split" for item in named_shape["history"]))
        self.assertTrue(
            any(
                item["kind"] == "split" and "ShellBase.Face1" in item["sources"]
                for item in named_shape["history"]
            )
        )

    def test_p8_part_section_builds_intersection_edges(self) -> None:
        result = self.run_recompute("part-section", "p8")
        section = result["objects"]["Section"]
        named_shape = result["named_shapes"]["Section"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(section["status"], "ok")
        self.assertEqual(section["boolean"], "section")
        self.assertEqual(section["shape"], "occt_compound")
        self.assertEqual(section["base"], "Box")
        self.assertEqual(section["tool"], "Plane")
        self.assertEqual(section["approximation"], False)
        self.assert_bbox_close_delta(section["bbox"], [-0.1, -0.1, 0.9], [2.1, 2.1, 1.1], 1e-6)
        self.assertEqual(section["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Section"], {"topology_counts": {"faces": 0, "edges": 4, "vertices": 4}})
        self.assertEqual(named_shape["owner"], "Section")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("Edge1", named_shape["elements"])

    def test_p8_part_ellipse_builds_edge(self) -> None:
        result = self.run_recompute("part-ellipse", "p8")
        ellipse = result["objects"]["Ellipse"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ellipse["status"], "ok")
        self.assertEqual(ellipse["primitive"], "ellipse")
        self.assertEqual(ellipse["shape"], "occt_edge")
        self.assertEqual(ellipse["major_radius"], 4.0)
        self.assertEqual(ellipse["minor_radius"], 2.0)
        self.assert_bbox_close_delta(ellipse["bbox"], [-4.0, -2.0, 0.0], [4.0, 2.0, 0.0], 2e-1)
        self.assertEqual(ellipse["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Ellipse"], {"topology_counts": {"faces": 0, "edges": 1, "vertices": 1}})
        self.assertIn("Ellipse", result["named_shapes"])

    def test_p8_part_helix_builds_spiral_helix_wire(self) -> None:
        result = self.run_recompute("part-helix", "p8")
        helix = result["objects"]["Helix"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(helix["status"], "ok")
        self.assertEqual(helix["primitive"], "helix")
        self.assertEqual(helix["shape"], "occt_wire")
        self.assertEqual(helix["pitch"], 1.0)
        self.assertEqual(helix["height"], 2.0)
        self.assertEqual(helix["radius"], 1.0)
        self.assertEqual(helix["turns"], 2.0)
        self.assert_bbox_close_delta(helix["bbox"], [-1.0, -1.0, 0.0], [1.0, 1.0, 2.0], 2e-1)
        self.assertGreater(helix["length"], 12.0)
        self.assertEqual(helix["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Helix"], {"topology_counts": {"faces": 0, "edges": 1, "vertices": 2}})
        self.assertIn("Helix", result["named_shapes"])

    def test_p8_part_spiral_builds_spiral_helix_wire(self) -> None:
        result = self.run_recompute("part-spiral", "p8")
        spiral = result["objects"]["Spiral"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(spiral["status"], "ok")
        self.assertEqual(spiral["primitive"], "spiral")
        self.assertEqual(spiral["shape"], "occt_wire")
        self.assertEqual(spiral["growth"], 1.0)
        self.assertEqual(spiral["radius"], 1.0)
        self.assertEqual(spiral["radius_top"], 3.0)
        self.assertEqual(spiral["rotations"], 2.0)
        self.assert_bbox_close_delta(spiral["bbox"], [-2.6, -2.84, 0.0], [3.0, 2.35, 0.0], 2e-1)
        self.assertGreater(spiral["length"], 24.0)
        self.assertEqual(spiral["volume"], 0.0)
        self.assert_topology_counts(result["subshapes"]["Spiral"], {"topology_counts": {"faces": 0, "edges": 2, "vertices": 3}})
        self.assertIn("Spiral", result["named_shapes"])


if __name__ == "__main__":
    unittest.main()
