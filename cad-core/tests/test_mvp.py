from __future__ import annotations

import ctypes
import json
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
    def run_recompute(self, fixture: str, group: str = "mvp") -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / f"{fixture}.result.json"
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(ROOT / "fixtures" / group / f"{fixture}.json"),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
            )
            return json.loads(output.read_text(encoding="utf-8"))

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
            "sketch-circle-profile": [],
            "sketch-coincident-profile": [],
            "sketch-construction-ignored": [],
            "sketch-ellipse-profile": [],
            "sketch-external-circle-edge": [],
            "sketch-external-circle-edge-as-line": [],
            "sketch-external-edge": [],
            "sketch-external-ellipse-edge": [],
            "sketch-external-face-unsupported": ["unsupported_subshape_kind"],
            "sketch-external-tilted-ellipse-edge": [],
            "sketch-external-tilted-circle-edge": [],
            "sketch-external-vertex": [],
            "sketch-missing-external": ["missing_link_target"],
            "sketch-unsupported-bspline": ["unsupported_geometry"],
            "sketch-unsupported-constraint": ["unsupported_property"],
        }
        for fixture, codes in expected.items():
            with self.subTest(fixture=fixture):
                self.assertEqual(self.diagnostic_codes(fixture, "p5"), codes)

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


if __name__ == "__main__":
    unittest.main()
