from __future__ import annotations

import json
import tempfile
from pathlib import Path

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import ROOT, CadCoreFixtureTestCase


class CadCoreAdapterTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_ffi_mesh_matches_expected_summary(self, result: dict, group: str, fixture: str) -> None:
        expected = self.expected_freecad(group, fixture)
        summary = expected["mesh_summary"]
        mesh = result["results"][0]["mesh"]
        vertices = mesh["vertices"]
        actual_bbox = {
            "min": [min(vertex[index] for vertex in vertices) for index in range(3)],
            "max": [max(vertex[index] for vertex in vertices) for index in range(3)],
        }

        self.assert_bbox_close_delta(
            actual_bbox,
            summary["bbox"]["min"],
            summary["bbox"]["max"],
            summary.get("bbox_delta", expected.get("bbox_delta", 1e-6)),
        )
        self.assertEqual(len(vertices), summary["vertex_count"])
        self.assertEqual(len(mesh["indices"]) // 3, summary["triangle_count"])

    def test_c_api_returns_sketch_internal_profile_mesh(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["object"], "Sketch")
        self.assertIsNotNone(sketch["mesh"])
        self.assertGreater(len(sketch["mesh"]["vertices"]), 0)
        self.assertGreater(len(sketch["mesh"]["indices"]), 0)
        self.assertIn("Sketch:InternalFace1", sketch["mesh"]["faceIds"])
        self.assertTrue(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))

    def test_c_api_keeps_open_sketch_internal_profile_mesh_null(self) -> None:
        result = self.run_recompute_ffi("sketch-open-wire-internal-empty", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["object"], "Sketch")
        self.assertIsNone(sketch["mesh"])
        self.assertFalse(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:Edge1" for item in sketch["subshapes"]))

    def test_c_api_applies_sketch_plane_frame_to_internal_profile_mesh(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-internal-face", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIsNotNone(sketch["mesh"])
        self.assert_ffi_mesh_matches_expected_summary(result, "p5", "sketch-plane-frame-internal-face")

    def test_c_api_composes_sketch_plane_frame_with_local_placement(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-placement", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIsNotNone(sketch["mesh"])
        self.assert_ffi_mesh_matches_expected_summary(result, "p5", "sketch-plane-frame-placement")

    def test_c_api_rejects_invalid_sketch_plane_frame(self) -> None:
        result = self.run_recompute_ffi("sketch-plane-frame-invalid", "p5")
        sketch = result["results"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["invalid_property_type"])
        self.assertIsNone(sketch["mesh"])

    def test_c_api_returns_split_internal_face_mesh_ids(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face-split-line", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("Sketch:InternalFace1", sketch["mesh"]["faceIds"])
        self.assertIn("Sketch:InternalFace2", sketch["mesh"]["faceIds"])
        self.assertTrue(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:InternalFace2" for item in sketch["subshapes"]))
        internal_subshapes = [
            item for item in sketch["subshapes"]
            if item["indexed"].startswith("Internal")
        ]
        self.assertGreater(len(internal_subshapes), 0)
        for item in internal_subshapes:
            self.assertEqual(item["stableSubname"], "")
        self.assertEqual(
            next(item for item in sketch["subshapes"] if item["indexed"] == "Edge1")["stableSubname"],
            "Edge1",
        )

    def test_c_api_matches_cli_for_p3b_recompute(self) -> None:
        ffi_result = self.run_recompute_ffi("pocket-custom-vector", "p3b")

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(ffi_result["elementReferenceUpdates"], [])
        self.assertNotIn("objects", ffi_result)
        self.assertNotIn("mesh", ffi_result)
        self.assertEqual([item["object"] for item in ffi_result["results"]], ["Body"])
        self.assertGreater(len(ffi_result["results"][0]["mesh"]["indices"]), 0)
        self.assertTrue(
            any(item["indexed"] == "Face1" for item in ffi_result["results"][0]["subshapes"])
        )

    def test_c_api_capabilities_exposes_web_contract_facts(self) -> None:
        capabilities = self.run_capabilities_ffi()

        self.assertEqual(capabilities["status"], "ok")
        self.assertEqual(capabilities["schema_version"], "cad-web-v1")
        self.assertEqual(capabilities["cad_core"]["api"], "cad_core_ffi")
        self.assertIn("OCCT", capabilities["cad_core"]["kernel"])
        self.assertEqual(capabilities["document"]["source"], "DocumentObject graph")
        self.assertEqual(capabilities["export_formats"], ["brep", "step", "stl"])
        self.assertIn("value", capabilities["document"]["link_property_fields"])
        self.assertIn("values", capabilities["document"]["link_property_fields"])
        self.assertIn("SubList", capabilities["document"]["link_property_fields"])
        self.assertIn("StableSubList", capabilities["document"]["link_property_fields"])
        self.assertIn("SubSet", capabilities["document"]["link_property_fields"])
        self.assertNotIn("FullSubList", capabilities["document"]["link_property_fields"])
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkList"],
            ["values"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSub"],
            ["value", "SubList", "StableSubList"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubList"],
            ["SubSet"],
        )

        for type_id in [
            "Sketcher::SketchObject",
            "PartDesign::Hole",
            "Part::Box",
            "Part::BooleanFragments",
            "App::Link",
            "Assembly::AssemblyObject",
        ]:
            self.assertIn(type_id, capabilities["supported_type_ids"])

        for code in [
            "parse_error",
            "missing_target",
            "missing_link_target",
            "cycle_dependency",
            "unsupported_type",
            "invalid_subshape",
            "unsupported_stable_subname",
            "split_stable_subname",
            "deleted_stable_subname",
        ]:
            self.assertIn(code, capabilities["diagnostic_codes"])

        self.assertIn("complete_mapper_history", capabilities["known_gaps"])

    def test_c_api_exports_recomputed_shape_buffers(self) -> None:
        document = json.loads((ROOT / "fixtures" / "p8" / "part-box.json").read_text(encoding="utf-8"))
        cases = {
            "brep": ("Part::ImportBrep", "ExportedBrep", "box.brep"),
            "step": ("Part::ImportStep", "ExportedStep", "box.step"),
            "stl": ("Mesh::Import", "ExportedStl", "box.stl"),
        }

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            for export_format, (type_id, object_name, file_name) in cases.items():
                with self.subTest(export_format=export_format):
                    status, metadata, data, error = self.call_export_ffi(
                        {"document": document, "object": "Box", "format": export_format}
                    )
                    self.assertEqual(status, 0, error)
                    self.assertIsNotNone(metadata)
                    assert metadata is not None
                    self.assertEqual(metadata["object"], "Box")
                    self.assertEqual(metadata["format"], export_format)
                    self.assertEqual(metadata["filename"], f"Box.{export_format}")
                    self.assertEqual(metadata["diagnostics"], [])
                    self.assertEqual(metadata["bytes"], len(data))
                    self.assertGreater(len(data), 0)

                    export_path = tmp_path / file_name
                    export_path.write_bytes(data)
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
                    expected = self.expected_freecad("p8", "part-box")

                    self.assertEqual(imported_result["diagnostics"], [])
                    self.assertEqual(imported["status"], "ok")
                    if export_format == "stl":
                        self.assertEqual(imported["primitive"], "import_stl")
                        self.assert_expected_object(imported_result, object_name, {"bbox": expected["bbox"]})
                        self.assertGreater(imported_result["mesh"][object_name]["summary"]["triangle_count"], 0)
                    else:
                        self.assert_expected_object(
                            imported_result,
                            object_name,
                            {"bbox": expected["bbox"], "volume": expected["volume"]},
                        )
    def test_c_api_export_reports_business_diagnostics_without_server_paths(self) -> None:
        document = json.loads((ROOT / "fixtures" / "p8" / "part-box.json").read_text(encoding="utf-8"))

        status, metadata, data, error = self.call_export_ffi(
            {"document": document, "object": "Missing", "format": "step"}
        )
        self.assertEqual(status, 0, error)
        self.assertEqual(data, b"")
        self.assertIsNotNone(metadata)
        assert metadata is not None
        self.assertEqual(metadata["bytes"], 0)
        self.assertEqual([item["code"] for item in metadata["diagnostics"]], ["missing_object"])

        status, metadata, data, error = self.call_export_ffi(
            {
                "document": document,
                "object": "Box",
                "format": "step",
                "export_file": "/tmp/box.step",
            }
        )
        self.assertEqual(status, 1)
        self.assertIsNone(metadata)
        self.assertEqual(data, b"")
        self.assertIn("server file path", error)

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
                    expected = self.expected_freecad("p8", "part-box")

                    self.assertEqual(imported_result["diagnostics"], [])
                    self.assertEqual(imported["status"], "ok")
                    if export_format == "stl":
                        self.assertEqual(imported["primitive"], "import_stl")
                        self.assert_expected_object(imported_result, object_name, {"bbox": expected["bbox"]})
                        self.assertGreater(imported_result["mesh"][object_name]["summary"]["triangle_count"], 0)
                    else:
                        self.assert_expected_object(
                            imported_result,
                            object_name,
                            {"bbox": expected["bbox"], "volume": expected["volume"]},
                        )
