from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import ROOT, BIN, CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import ROOT, BIN, CadCoreFixtureTestCase


class CadCoreAdapterTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    CORE_RESULT_CHANNELS = {
        "results",
        "elementReferenceUpdates",
        "documentObjectUpdates",
        "diagnostics",
        "binaryPayloads",
    }

    def run_cli_core_recompute_payload(self, payload: bytes | dict) -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            input_path = tmp_path / "request.json"
            output_path = tmp_path / "result.json"
            input_path.write_bytes(payload)
            env = os.environ.copy()
            env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            subprocess.run(
                [str(BIN), "recompute", str(input_path), "--output", str(output_path)],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return json.loads(output_path.read_text(encoding="utf-8"))

    def assert_core_result_contract(self, result: dict) -> None:
        payload = {key: value for key, value in result.items() if key != "adapter"}
        self.assertEqual(set(payload), self.CORE_RESULT_CHANNELS)
        self.assertIsInstance(payload["results"], list)
        self.assertIsInstance(payload["elementReferenceUpdates"], list)
        self.assertIsInstance(payload["documentObjectUpdates"], list)
        self.assertIsInstance(payload["diagnostics"], list)
        self.assertIsInstance(payload["binaryPayloads"], list)

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

    def assert_mesh_edge_segments_reference_subshapes(self, result_item: dict) -> None:
        mesh = result_item["mesh"]
        edge_segments = mesh["edgeSegments"]
        subshape_by_id = {item["id"]: item for item in result_item["subshapes"]}
        edge_subshape_ids = {
            item["id"] for item in result_item["subshapes"] if item["kind"] == "Edge"
        }

        self.assertTrue(edge_segments)
        self.assertEqual({segment["id"] for segment in edge_segments}, edge_subshape_ids)
        for segment in edge_segments:
            self.assertIn(segment["id"], subshape_by_id)
            self.assertEqual(subshape_by_id[segment["id"]]["kind"], "Edge")
            self.assertEqual(segment["indexed"], subshape_by_id[segment["id"]]["indexed"])
            self.assertGreaterEqual(len(segment["points"]), 2)

    def assert_mesh_vertex_points_reference_subshapes(self, result_item: dict) -> None:
        mesh = result_item["mesh"]
        vertex_points = mesh["vertexPoints"]
        subshape_by_id = {item["id"]: item for item in result_item["subshapes"]}
        vertex_subshape_ids = {
            item["id"] for item in result_item["subshapes"] if item["kind"] == "Vertex"
        }

        self.assertTrue(vertex_points)
        self.assertEqual({point["id"] for point in vertex_points}, vertex_subshape_ids)
        for point in vertex_points:
            self.assertIn(point["id"], subshape_by_id)
            self.assertEqual(subshape_by_id[point["id"]]["kind"], "Vertex")
            self.assertEqual(point["indexed"], subshape_by_id[point["id"]]["indexed"])
            self.assertEqual(len(point["point"]), 3)

    def test_c_api_returns_sketch_internal_profile_mesh(self) -> None:
        result = self.run_recompute_ffi("sketch-internal-face", "p5")
        sketch = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sketch["object"], "Sketch")
        self.assertIsNotNone(sketch["mesh"])
        self.assertGreater(len(sketch["mesh"]["vertices"]), 0)
        self.assertGreater(len(sketch["mesh"]["indices"]), 0)
        self.assertIn("Sketch:InternalFace1", sketch["mesh"]["faceIds"])
        internal_edge_ids = {segment["id"] for segment in sketch["mesh"]["edgeSegments"]}
        self.assertIn("Sketch:InternalEdge1", internal_edge_ids)
        internal_vertex_ids = {point["id"] for point in sketch["mesh"]["vertexPoints"]}
        self.assertIn("Sketch:InternalVertex1", internal_vertex_ids)
        self.assertTrue(any(item["id"] == "Sketch:InternalFace1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:InternalEdge1" for item in sketch["subshapes"]))
        self.assertTrue(any(item["id"] == "Sketch:InternalVertex1" for item in sketch["subshapes"]))

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
            if item["indexed"].startswith("InternalFace"):
                self.assertEqual(item["stableSubname"], "")
            elif item["stableSubname"]:
                self.assertRegex(item["stableSubname"], r"^(Edge|Vertex)\d+$")
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

    def test_c_api_mesh_edge_segments_reference_result_subshapes(self) -> None:
        result = self.run_recompute_ffi("rect-pad", "mvp")
        body = result["results"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(body["object"], "Body")
        self.assert_mesh_edge_segments_reference_subshapes(body)
        self.assert_mesh_vertex_points_reference_subshapes(body)

    def test_c4s11_cli_c_api_worker_wasm_share_core_result_contract(self) -> None:
        payload = (ROOT / "fixtures" / "mvp" / "rect-pad.json").read_bytes()

        cli_result = self.run_cli_core_recompute_payload(payload)
        c_api_result = self.run_recompute_ffi_payload(payload)
        worker_result = self.run_worker_recompute_ffi_payload(payload)
        wasm_result = self.run_wasm_recompute_ffi_payload(payload)

        for result in (cli_result, c_api_result, worker_result, wasm_result):
            self.assert_core_result_contract(result)
            self.assertEqual(result["binaryPayloads"], [])

        self.assertEqual(c_api_result, cli_result)
        for adapter, result in (("worker", worker_result), ("wasm", wasm_result)):
            self.assertEqual(result["adapter"], adapter)
            normalized = dict(result)
            normalized.pop("adapter")
            self.assertEqual(normalized, cli_result)

    def test_c4s11_adapter_resource_limit_diagnostic_preserves_result_schema(self) -> None:
        payload = json.loads((ROOT / "fixtures" / "c3m7" / "rect-pad-worker-mesh-limit.json").read_text())
        payload["mesh_limits"]["max_vertices"] = "four"

        for adapter, runner in [
            ("c_api", self.run_recompute_ffi_payload),
            ("worker", self.run_worker_recompute_ffi_payload),
            ("wasm", self.run_wasm_recompute_ffi_payload),
        ]:
            with self.subTest(adapter=adapter):
                result = runner(payload)
                self.assert_core_result_contract(result)
                if adapter == "c_api":
                    self.assertNotIn("adapter", result)
                else:
                    self.assertEqual(result["adapter"], adapter)
                self.assertEqual(result["results"][0]["object"], "Pad")
                diagnostic = result["diagnostics"][-1]
                self.assertEqual(diagnostic["code"], "adapter_resource_limit")
                self.assertEqual(diagnostic["stage"], "adapter")
                self.assertEqual(diagnostic["property"], "mesh_limits")
                self.assertEqual(diagnostic["target"], "mesh_limits")

    def test_c_api_recompute_returns_reference_shadow_update(self) -> None:
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["PropertyType"], "App::PropertyLinkSub")
        self.assertEqual(updates[0]["value"], "Sketch")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["ShadowSub"], [{"newName": "g305:split1", "oldName": "InternalFace1"}])

        shadow = updates[0]["ReferenceShadow"][0]
        self.assertEqual(shadow["target"], "Sketch")
        self.assertEqual(shadow["property"], "InternalShape")
        self.assertEqual(shadow["indexed"], "Face1")
        self.assertEqual(shadow["subname"], "InternalFace1")
        self.assertEqual(shadow["fingerprint"]["shapeType"], "Face")
        self.assertAlmostEqual(shadow["fingerprint"]["area"], 25.0)
        self.assertEqual(shadow["fingerprint"]["centroid"], [2.5, 2.5, 0.0])

    def test_c_api_recompute_returns_recovered_reference_shadow_update(self) -> None:
        ffi_result = self.run_recompute_ffi("pad-internal-face-reference-shadow-recover-sublist", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["StableSubList"], ["g305:split1"])

        shadow = updates[0]["ReferenceShadow"][0]
        self.assertEqual(shadow["indexed"], "Face1")
        self.assertEqual(shadow["subname"], "InternalFace1")
        self.assertEqual(shadow["stableSubname"], "g305:split1")
        self.assertAlmostEqual(shadow["fingerprint"]["area"], 25.0)

    def test_c_api_recompute_preserves_full_sublist_on_reference_shadow_update(self) -> None:
        payload = json.loads((ROOT / "fixtures" / "p5" / "pad-internal-face-reference-shadow.json").read_text())
        profile = payload["Objects"][1]["Properties"]["Profile"]
        profile["FullSubList"] = ["ExternalDoc#Sketch.InternalFace1"]

        ffi_result = self.run_recompute_ffi_payload(payload)
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Pad")
        self.assertEqual(updates[0]["property"], "Profile")
        self.assertEqual(updates[0]["SubList"], ["InternalFace1"])
        self.assertEqual(updates[0]["FullSubList"], ["ExternalDoc#Sketch.InternalFace1"])

    def test_c_api_recompute_returns_reference_shadow_update_for_link_sub_list(self) -> None:
        ffi_result = self.run_recompute_ffi("sketch-external-face-reference-shadow", "p5")
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["object"], "Sketch")
        self.assertEqual(updates[0]["property"], "ExternalGeometry")
        self.assertEqual(updates[0]["PropertyType"], "App::PropertyLinkSubList")
        self.assertEqual(len(updates[0]["SubSet"]), 1)

        item = updates[0]["SubSet"][0]
        self.assertEqual(item["value"], "Box")
        self.assertEqual(item["SubList"], ["Face5"])
        shadow = item["ReferenceShadow"][0]
        self.assertEqual(shadow["target"], "Box")
        self.assertEqual(shadow["property"], "Shape")
        self.assertEqual(shadow["indexed"], "Face5")
        self.assertEqual(shadow["subname"], "Face5")
        self.assertEqual(shadow["fingerprint"]["shapeType"], "Face")
        self.assertEqual(shadow["fingerprint"]["edgeCount"], 4)
        self.assertEqual(shadow["fingerprint"]["vertexCount"], 4)

    def test_c_api_recompute_preserves_full_sublist_on_link_sub_list_update(self) -> None:
        payload = json.loads((ROOT / "fixtures" / "p5" / "sketch-external-face-reference-shadow.json").read_text())
        external = payload["Objects"][1]["Properties"]["ExternalGeometry"]["SubSet"][0]
        external["FullSubList"] = ["ExternalDoc#Box.Face5"]

        ffi_result = self.run_recompute_ffi_payload(payload)
        updates = ffi_result["elementReferenceUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        item = updates[0]["SubSet"][0]
        self.assertEqual(item["value"], "Box")
        self.assertEqual(item["SubList"], ["Face5"])
        self.assertEqual(item["FullSubList"], ["ExternalDoc#Box.Face5"])

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
        self.assertIn("FullSubList", capabilities["document"]["link_property_fields"])
        self.assertIn("ShadowSub", capabilities["document"]["link_property_fields"])
        self.assertIn("ReferenceShadow", capabilities["document"]["link_property_fields"])
        self.assertIn("ExternalFlags", capabilities["document"]["link_property_fields"])
        self.assertIn("Document", capabilities["document"]["link_property_fields"])
        self.assertIn("SubSet", capabilities["document"]["link_property_fields"])
        self.assertEqual(
            capabilities["document"]["document_reference_fields"],
            [
                "file",
                "name",
                "label",
                "stamp",
                "status",
                "currentName",
                "currentLabel",
                "currentStamp",
                "currentStatus",
                "allowPartial",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_native_slot_fields"],
            ["ExternalGeo", "Geometry", "Values", "Items", "Ref", "RefIndex", "ExternalFlags", "Flags"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_flags"],
            ["Defining", "Frozen", "Detached", "Missing", "Sync"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["state_updates"],
            [
                "sync_clears_sync_flag",
                "missing_clears_when_reference_resolves",
                "frozen_reuses_reference_shadow_brep",
                "missing_unresolved_reuses_reference_shadow_brep",
                "frozen_reuses_native_external_geo",
                "missing_unresolved_reuses_native_external_geo",
                "detached_keeps_native_external_geo_and_clears_ref",
                "detached_removes_external_geometry_link",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["request_local_boundaries"],
            [
                "frozen_current_source_without_reference_shadow_brep_skips_projection",
                "native_external_geo_pool_is_request_local_not_backend_session",
            ],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["diagnostics"],
            ["missing_external_geometry_snapshot"],
        )
        self.assertEqual(
            capabilities["document"]["external_geometry_lifecycle"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["document"]["document_update_channels"],
            ["elementReferenceUpdates", "documentObjectUpdates"],
        )
        self.assertEqual(
            capabilities["link_transaction"]["document_object_updates"],
            [
                "show_element_create",
                "show_element_claim",
                "show_element_sync",
                "show_element_delete",
                "show_element_toggle_off",
                "child_cache_create",
                "child_cache_nested_plain_group",
                "child_cache_orphan_reclaim",
                "child_cache_stale_delete",
                "element_count_owner_lists_sync",
                "element_list_owner_sync",
                "element_list_child_sync",
                "copy_on_change_owned_child_sync",
                "copy_on_change_writeback_contract",
                "copy_on_change_group_sync",
                "copy_on_change_deep_copy_lifecycle",
                "copy_on_change_owned_child_mutation",
                "copy_on_change_touched_tracking",
            ],
        )
        self.assertEqual(
            capabilities["link_transaction"]["copy_on_change_writeback_contract"]["status"],
            "covered",
        )
        self.assertEqual(
            capabilities["link_transaction"]["copy_on_change_deep_copy_lifecycle"]["status"],
            "covered_full",
        )
        self.assertEqual(
            capabilities["link_transaction"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["link_transaction"]["writeback_properties"],
            [
                "ElementList",
                "ElementCount",
                "PlacementList",
                "ScaleList",
                "VisibilityList",
                "LinkedObject",
                "LinkCopyOnChange",
                "LinkCopyOnChangeSource",
                "LinkCopyOnChangeGroup",
                "LinkCopyOnChangeTouched",
                "_LinkOwner",
                "LinkTransform",
                "_CopyOnChangeControl",
                "_CopyOnChangeOwner",
                "_CopyOnChangeSourceObject",
                "_CopyOnChangeSourceId",
            ],
        )
        self.assertIn(
            "plain_group_child_cache_updates_are_document_object_updates",
            capabilities["link_transaction"]["request_local_boundaries"],
        )
        self.assertIn("copy_on_change_keeps_request_graph_immutable", capabilities["link_transaction"]["request_local_boundaries"])
        self.assertIn(
            "copy_on_change_frontend_persists_copied_graph_between_requests",
            capabilities["link_transaction"]["request_local_boundaries"],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["retag_aliases"],
            [
                "full_sublist_external_tag",
                "mapped_postfix_alias",
                "source_prefixed_stable_key",
                "label_qualified_subname",
                "multi_level_link_subname",
                "property_xlink_list_subset_compound",
            ],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["reference_update_fields"],
            [
                "FullSubList",
                "StableSubList",
                "ShadowSub",
                "ReferenceShadow",
                "ExternalFlags",
                "labelReferenceRename",
                "documentReference",
            ],
        )
        self.assertEqual(
            capabilities["link_reference_lifecycle"]["reference_recovery"],
            [
                "source_object_rename_by_reference_shadow_target_id",
                "label_rename_by_link_target_label",
                "label_rename_nested_by_link_group_path",
                "label_rename_cross_document_nested_by_full_sublist",
                "document_name_label_restore",
                "missing_external_document_diagnostic",
                "external_document_pending_reload_diagnostic",
                "external_document_unloaded_diagnostic",
            ],
        )
        self.assertEqual(capabilities["link_reference_lifecycle"]["remaining_gaps"], [])
        self.assertNotIn("missing_external_document_lifecycle", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("source_object_rename_recovery", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("label_rename_recovery", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn("label_rename_nested_lifecycle", capabilities["link_reference_lifecycle"]["remaining_gaps"])
        self.assertNotIn(
            "label_rename_cross_document_nested_lifecycle",
            capabilities["link_reference_lifecycle"]["remaining_gaps"],
        )
        pressure = capabilities["topo_reference_pressure"]
        self.assertEqual(pressure["status"], "done_c4m4_topo_reference_pressure")
        self.assertEqual(len(pressure["fixtures"]), 12)
        self.assertEqual(
            pressure["classifications"]["updated"],
            [
                "C4M4-TR-PRESS-001",
                "C4M4-TR-PRESS-002",
                "C4M4-TR-PRESS-004",
                "C4M4-TR-PRESS-005",
                "C4M4-TR-PRESS-009",
                "C4M4-TR-PRESS-012",
            ],
        )
        self.assertEqual(pressure["classifications"]["unchanged"], ["C4M4-TR-PRESS-006"])
        self.assertEqual(
            pressure["classifications"]["needs_reselect"],
            ["C4M4-TR-PRESS-007", "C4M4-TR-PRESS-010", "C4M4-TR-PRESS-011"],
        )
        self.assertEqual(
            pressure["classifications"]["diagnostic_only"],
            ["C4M4-TR-PRESS-003", "C4M4-TR-PRESS-008"],
        )
        self.assertIn("elementReferenceUpdates", pressure["update_fields"])
        self.assertIn("documentObjectUpdates", pressure["update_fields"])
        self.assertIn("split_stable_subname", pressure["diagnostics"])
        self.assertIn("ReferenceShadow.brep_single_subshape_only", pressure["request_local_boundaries"])
        self.assertIn("adapter_publishes_contract_metadata_only", pressure["request_local_boundaries"])
        self.assertEqual(pressure["remaining_gaps"], [])
        self.assertEqual(capabilities["sketcher"]["solver"]["status"], "done_c4m3_constraint_facing_audit")
        self.assertEqual(
            capabilities["sketcher"]["solver"]["diagnostics"],
            [
                "sketch_solver_conflict",
                "sketch_solver_malformed_constraint",
                "sketch_solver_partially_redundant",
                "sketch_solver_redundant",
                "unsupported_sketch_constraint_relation",
            ],
        )
        self.assertIn(
            "horizontal_vertical_same_target_conflict",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "duplicate_orientation_constraint_redundant",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "malformed_constraint_diagnostics",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "partial_redundancy_diagnostics",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "unconstrained_geometry_underconstrained_state",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "whole_line_orientation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "endpoint_coordinate_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "circle_radius_diameter_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "line_length_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "arc_length_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "point_on_object_line_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "parallel_line_pair_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "perpendicular_line_pair_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "perpendicular_line_circle_arc_midpoint_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "equal_line_circle_arc_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "tangent_line_circle_arc_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_line_axis_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_arc_endpoint_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_center_point_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "symmetric_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "solver_dof_driven_underconstrained_state",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn("full_solver_dof", capabilities["sketcher"]["solver"]["covered"])
        self.assertIn("dependent_parameter_group_analysis", capabilities["sketcher"]["solver"]["covered"])
        self.assertIn(
            "unsupported_relation_adapter_visible_diagnostic",
            capabilities["sketcher"]["solver"]["covered"],
        )
        self.assertIn(
            "whole_line_orientation_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "endpoint_coordinate_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "circle_radius_diameter_update_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_length_update_preserves_start_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "arc_length_update_scales_radius_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "point_on_object_line_projection_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_pair_parallel_update_preserves_second_start_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_pair_perpendicular_update_preserves_second_start_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "line_circle_arc_perpendicular_midpoint_projection_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "equal_relation_updates_second_geometry_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "tangent_line_circle_arc_updates_round_center_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_line_axis_updates_second_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_arc_endpoint_updates_second_arc_angle_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "symmetric_center_point_updates_second_point_without_full_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "unsupported_relation_fails_without_fake_profile",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertNotIn(
            "request_local_dof_estimate_without_full_solver_rank",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertNotIn(
            "partial_redundancy_warning_without_full_dependent_parameter_group_analysis",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertEqual(capabilities["sketcher"]["solver"]["remaining_gaps"], [])
        self.assertNotIn(
            "symmetric_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_arc_endpoint_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_center_point_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "tangent_equal_symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "tangent_symmetric_and_coupled_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "curve_and_coupled_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "perpendicular_and_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "line_pair_and_curve_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn("full_solver_dof", capabilities["sketcher"]["solver"]["remaining_gaps"])
        self.assertNotIn(
            "solver_dof_driven_underconstrained_state",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "arc_length_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "line_length_arc_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "length_radius_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "dimension_relation_solver_geometry_update",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn("solver_geometry_update", capabilities["sketcher"]["solver"]["remaining_gaps"])
        self.assertNotIn(
            "underconstrained_without_full_dof_count",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "diagnostics_only_without_backend_solver_session",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertIn(
            "malformed_blocks_profile_output",
            capabilities["sketcher"]["solver"]["request_local_boundaries"],
        )
        self.assertNotIn("underconstrained_state", capabilities["sketcher"]["solver"]["remaining_gaps"])
        self.assertNotIn(
            "malformed_constraint_diagnostics",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        self.assertNotIn(
            "partial_redundancy_diagnostics",
            capabilities["sketcher"]["solver"]["remaining_gaps"],
        )
        pressure = capabilities["sketcher"]["external_internal_pressure"]
        self.assertEqual(pressure["status"], "done_c4m3_external_internal_pressure")
        self.assertEqual(
            pressure["expected_backed"],
            [
                "external_geometry_frozen_native_pool",
                "external_geometry_detached_native_pool",
                "external_geometry_missing_recovered",
                "internal_shape_open_profile_empty",
                "internal_shape_bounded_cross_cutters",
                "internal_shape_self_intersection_bowtie",
                "internal_shape_split_dangling_mixed",
            ],
        )
        self.assertEqual(
            pressure["reference_shadow"],
            [
                "single_subshape_snapshot_only",
                "external_geometry_brep_snapshot_request_local",
                "internal_shape_edge_stable_shadow_sub_update",
            ],
        )
        self.assertEqual(
            pressure["deferred_diagnostics"],
            [
                "missing_external_geometry_snapshot",
                "unsupported_reference_shadow_brep",
                "subname_split_requires_reselect",
                "subname_deleted",
                "subname_resolve_ambiguous",
                "deleted_stable_subname",
            ],
        )
        self.assertEqual(
            pressure["request_local_boundaries"],
            [
                "no_full_brep_document_state",
                "no_backend_external_geometry_session",
                "no_sketch_executor_split_ownership_guess",
                "no_wire_joiner_fallback_candidate_fields",
            ],
        )
        self.assertEqual(pressure["remaining_gaps"], [])
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["document_object_updates"],
            [
                "body_basefeature_featurebase_create",
                "body_basefeature_group_sync",
                "body_feature_basefeature_sync",
                "body_tip_deleted_feature_reroute",
                "body_feature_basefeature_delete_reroute",
                "body_origin_datum_relink",
            ],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["writeback_properties"],
            ["Body.Group", "Body.Tip", "Body.Origin", "FeatureBase.BaseFeature", "Feature.BaseFeature"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["origin_lifecycle"],
            ["explicit_body_origin_link", "origin_global_placement_from_body", "origin_feature_role_relink"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["addsub_replay"],
            ["group_order_replay", "additive_fuse", "subtractive_cut", "stop_at_tip"],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["request_local_boundaries"],
            [
                "body_basefeature_writeback_keeps_request_graph_immutable",
                "body_delete_reroute_uses_request_local_stale_tip_evidence",
                "body_origin_parent_placement_without_backend_session",
            ],
        )
        self.assertEqual(
            capabilities["part_design"]["body_chain"]["remaining_gaps"],
            [],
        )
        up_to_shape_multi_face = capabilities["part_design"]["pad_pocket"]["up_to_shape_multi_face"]
        self.assertEqual(up_to_shape_multi_face["status"], "supported")
        self.assertEqual(up_to_shape_multi_face["objects"], ["part_design.pad", "part_design.pocket"])
        self.assertEqual(
            up_to_shape_multi_face["fixtures"],
            ["p3a/pocket-up-to-shape-multi-face", "p3a/pad-up-to-shape-multi-face"],
        )
        self.assertEqual(
            up_to_shape_multi_face["failure_fixtures"]["offset"],
            "p3a/pocket-up-to-shape-multiple-faces-offset",
        )
        self.assertEqual(
            up_to_shape_multi_face["failure_fixtures"]["invalid_subshape"],
            "p3a/pocket-up-to-shape-edge-subshape",
        )
        self.assertEqual(
            up_to_shape_multi_face["diagnostics"],
            ["unsupported_property", "unsupported_subshape_kind", "invalid_subshape", "missing_link_target"],
        )
        self.assertEqual(capabilities["part_design"]["pad_pocket"]["remaining_gaps"], [])
        revolution_groove = capabilities["part_design"]["revolution_groove"]
        self.assertEqual(revolution_groove["status"], "supported_c51s1_advanced_with_exact_groove_upto_blocker")
        self.assertEqual(revolution_groove["type_ids"], ["PartDesign::Revolution", "PartDesign::Groove"])
        self.assertIn("Type=Angle", revolution_groove["supported"])
        self.assertIn("Type=TwoAngles", revolution_groove["supported"])
        self.assertIn("Revolution Type=UpToFirst", revolution_groove["supported"])
        self.assertIn("Revolution Type=UpToLast", revolution_groove["supported"])
        self.assertIn("Revolution Type=UpToFace", revolution_groove["supported"])
        self.assertIn("Groove Type=ThroughAll", revolution_groove["supported"])
        self.assertIn("Profile.SubList=InternalFaceN", revolution_groove["supported"])
        self.assertIn("Sketch AxisN ReferenceAxis", revolution_groove["supported"])
        self.assertIn("PartDesign::Line ReferenceAxis", revolution_groove["supported"])
        self.assertIn("App::Line ReferenceAxis", revolution_groove["supported"])
        self.assertIn("FuseOrder=FeatureFirst", revolution_groove["supported"])
        self.assertIn("Body additive fuse replay", revolution_groove["supported"])
        self.assertIn("Body additive FeatureFirst fuse replay", revolution_groove["supported"])
        self.assertIn("Body subtractive cut replay", revolution_groove["supported"])
        self.assertIn("c4m2/partdesign-revolution-axis-angle-body", revolution_groove["fixtures"])
        self.assertIn("c4m2/partdesign-groove-axis-angle-body", revolution_groove["fixtures"])
        self.assertIn("c5m1/partdesign-revolution-two-angles-body", revolution_groove["fixtures"])
        self.assertIn("c5m1/partdesign-groove-two-angles-body", revolution_groove["fixtures"])
        self.assertIn("c5m1/partdesign-groove-through-all-body", revolution_groove["fixtures"])
        self.assertIn("c5m1/partdesign-revolution-part-edge-axis", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-internalface-profile", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-featurefirst-body", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-datumline-axis", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-appline-axis", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-sketch-axisn", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-uptoface-body", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-uptofirst-body", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-revolution-uptolast-body", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-groove-uptofirst-body", revolution_groove["fixtures"])
        self.assertIn("c51m1/partdesign-groove-uptoface-body", revolution_groove["fixtures"])
        self.assertEqual(
            revolution_groove["diagnostics"],
            [
                "invalid_angle",
                "invalid_axis",
                "invalid_property_value",
                "execution_failed",
                "missing_property",
                "missing_link_target",
                "invalid_subshape",
                "unsupported_subshape_kind",
                "unsupported_profile_region",
                "unsupported_property",
            ],
        )
        self.assertEqual(revolution_groove["deferred"], [])
        self.assertEqual(
            revolution_groove["exact_blockers"]["id"],
            "partdesign_groove_upto_brepfeat_cut_native_failure",
        )
        self.assertEqual(
            revolution_groove["exact_blockers"]["freecad_message"],
            "Revolution: Up to face: Could not revolve the sketch!",
        )
        self.assertEqual(
            revolution_groove["remaining_gaps"],
            ["partdesign_groove_upto_brepfeat_cut_native_failure"],
        )
        boolean = capabilities["part_design"]["boolean"]
        self.assertEqual(boolean["status"], "supported_c51s2_boolean_compound_section_with_exact_body_policy")
        self.assertEqual(boolean["type_ids"], ["PartDesign::Boolean"])
        self.assertIn("Type=Fuse", boolean["supported"])
        self.assertIn("Type=Cut", boolean["supported"])
        self.assertIn("Type=Common", boolean["supported"])
        self.assertIn("Type=Compound productized from Part TopoShape maker", boolean["supported"])
        self.assertIn("Type=Section productized as non-solid edge/wire output", boolean["supported"])
        self.assertIn("AllowCompound=true multi-solid result", boolean["supported"])
        self.assertIn("AllowCompound=false Compound multi-solid diagnostic", boolean["supported"])
        self.assertIn("multi-tool Group order", boolean["supported"])
        self.assertIn("Body replacement Tip replay", boolean["supported"])
        self.assertIn("Section standalone edge/wire result", boolean["supported"])
        self.assertIn("Section Body Tip non-solid diagnostic", boolean["supported"])
        self.assertIn("c4m2/partdesign-boolean-cut-body-tool", boolean["fixtures"])
        self.assertIn("c4m2/partdesign-boolean-fuse-body-tool", boolean["fixtures"])
        self.assertIn("c4m2/partdesign-boolean-common-body-tool", boolean["fixtures"])
        self.assertIn("c5m2/partdesign-boolean-allow-compound-multisolid", boolean["fixtures"])
        self.assertIn("c5m2/partdesign-boolean-multi-tool-ownership", boolean["fixtures"])
        self.assertIn("c5m2/partdesign-boolean-multisolid-rejected", boolean["fixtures"])
        self.assertIn("c5m2/partdesign-boolean-tool-missing-shape-diagnostic", boolean["fixtures"])
        self.assertIn("c51m2/partdesign-boolean-compound-body-tip", boolean["fixtures"])
        self.assertIn("c51m2/partdesign-boolean-compound-disallowed", boolean["fixtures"])
        self.assertIn("c51m2/partdesign-boolean-section-standalone", boolean["fixtures"])
        self.assertIn("c51m2/partdesign-boolean-section-body-tip-diagnostic", boolean["fixtures"])
        self.assertIn("c51m2/partdesign-boolean-section-no-intersection", boolean["fixtures"])
        self.assertEqual(
            boolean["diagnostics"],
            [
                "missing_property",
                "missing_link_target",
                "missing_target",
                "invalid_link_value",
                "multiple_solids_disallowed",
                "no_intersection",
                "partdesign_body_tip_non_solid",
                "unsupported_property",
                "execution_failed",
            ],
        )
        self.assertEqual(boolean["deferred"], [])
        self.assertEqual(
            boolean["product_contract"]["section_body_policy"],
            "standalone section edge/wire output is supported; Body Tip replacement is rejected with exact diagnostic",
        )
        self.assertEqual(boolean["remaining_gaps"], [])
        loft = capabilities["part_design"]["loft"]
        self.assertEqual(loft["status"], "supported_c51s3_profile_section_closed_multiwire_sewing")
        self.assertEqual(loft["type_ids"], ["PartDesign::AdditiveLoft", "PartDesign::SubtractiveLoft"])
        self.assertIn("Profile full sketch", loft["supported"])
        self.assertIn("Sections App::PropertyXLinkSubList full sketch", loft["supported"])
        self.assertIn("Closed=true with 3+ profile/sections", loft["supported"])
        self.assertIn("multi-wire profile/section ordering", loft["supported"])
        self.assertIn("MapperSewing modified history", loft["supported"])
        self.assertIn("Body additive fuse replay", loft["supported"])
        self.assertIn("Body subtractive cut replay", loft["supported"])
        self.assertIn("maker_history:partdesign_loft", loft["supported"])
        self.assertEqual(
            loft["fixtures"],
            [
                "c4m2/partdesign-loft-additive-body",
                "c4m2/partdesign-loft-subtractive-body",
                "c5m3/partdesign-loft-closed-multisection",
                "c5m3/partdesign-loft-multiwire-ordering",
                "c5m3/partdesign-loft-allow-compound-diagnostic",
                "c51m3/partdesign-loft-closed-multisection",
                "c51m3/partdesign-loft-multiwire-ordering",
                "c51m3/partdesign-loft-allow-compound-diagnostic",
            ],
        )
        self.assertIn("invalid_sections", loft["diagnostics"])
        self.assertIn("multiple_solids_disallowed", loft["diagnostics"])
        self.assertEqual(loft["remaining_gaps"], [])
        self.assertEqual(loft["deferred"], [])
        pipe = capabilities["part_design"]["pipe"]
        self.assertEqual(pipe["status"], "supported_c51s4_pipe_advanced_with_exact_source_blockers")
        self.assertEqual(pipe["type_ids"], ["PartDesign::AdditivePipe", "PartDesign::SubtractivePipe"])
        self.assertIn("Profile full sketch", pipe["supported"])
        self.assertIn("Spine selected Edge/Wire path", pipe["supported"])
        self.assertIn("Mode=Fixed", pipe["supported"])
        self.assertIn("Mode=Frenet", pipe["supported"])
        self.assertIn("Mode=Auxiliary", pipe["supported"])
        self.assertIn("Mode=Binormal", pipe["supported"])
        self.assertIn("Transformation=Multisection", pipe["supported"])
        self.assertIn("Body additive fuse replay", pipe["supported"])
        self.assertIn("Body subtractive cut replay", pipe["supported"])
        self.assertIn("maker_history:partdesign_pipe", pipe["supported"])
        self.assertIn("Transition=Transformed", pipe["supported"])
        self.assertIn("Transition=Right corner", pipe["supported"])
        self.assertIn("Transition=Round corner", pipe["supported"])
        self.assertIn("MapperSewing modified history", pipe["supported"])
        self.assertEqual(
            pipe["fixtures"],
            [
                "c4m2/partdesign-pipe-additive-body",
                "c4m2/partdesign-pipe-subtractive-body",
                "c4m2/partdesign-pipe-deferred-diagnostics",
                "c5m3/partdesign-pipe-sections-transformation",
                "c5m3/partdesign-pipe-transition-variants",
                "c5m3/partdesign-pipe-auxiliary-binormal-diagnostics",
                "c51m4/partdesign-pipe-fixed-round-body",
                "c51m4/partdesign-pipe-auxiliary-binormal-modes",
                "c51m4/partdesign-pipe-selected-spine-multisection",
                "c51m4/partdesign-pipe-source-backed-blockers",
            ],
        )
        self.assertIn("unsupported_property", pipe["diagnostics"])
        self.assertEqual(pipe["deferred"], [])
        self.assertIn("partdesign_pipe_transformation_laws_source_commented", pipe["exact_blockers"])
        self.assertIn("partdesign_pipe_spine_tangent_source_commented", pipe["exact_blockers"])
        self.assertNotIn("partdesign_pipe_multisection", pipe["remaining_gaps"])
        self.assertNotIn("partdesign_pipe_round_corner", pipe["remaining_gaps"])
        self.assertEqual(pipe["remaining_gaps"], [])
        datum_attachment = capabilities["part_design"]["datum_attachment"]
        self.assertEqual(
            datum_attachment["status"],
            "supported_c51x_selected_attach_engine_with_datum_point_single_input",
        )
        self.assertIn("PartDesign::CoordinateSystem", datum_attachment["type_ids"])
        self.assertIn("DatumLine Placement direction", datum_attachment["supported"])
        self.assertIn("Body Origin datum role relink", datum_attachment["supported"])
        self.assertIn("FlatFace selected MapMode", datum_attachment["supported"])
        self.assertIn("NormalToEdge selected MapMode", datum_attachment["supported"])
        self.assertIn("DatumPoint Vertex selected MapMode", datum_attachment["supported"])
        self.assertIn("DatumPoint OnEdge selected MapMode", datum_attachment["supported"])
        self.assertIn("DatumPoint CenterOfMass selected MapMode", datum_attachment["supported"])
        self.assertIn("AttachmentSupport StableSubList/ShadowSub request-local writeback", datum_attachment["supported"])
        self.assertIn("p7/datum-coordinate-system-reference-axis", datum_attachment["fixtures"])
        self.assertIn("c4m2/partdesign-datum-attachment-deferred-diagnostics", datum_attachment["fixtures"])
        self.assertIn("c5m4/partdesign-datum-attachment-mapmode-diagnostics", datum_attachment["fixtures"])
        self.assertIn("c51m5/partdesign-datum-selected-mapmodes", datum_attachment["fixtures"])
        self.assertIn("c51m5/partdesign-datum-offset-reverse-writeback", datum_attachment["fixtures"])
        self.assertIn("c51m5/partdesign-datum-point-single-input-modes", datum_attachment["fixtures"])
        self.assertIn("unsupported_property", datum_attachment["diagnostics"])
        self.assertIn("attachment_support_invalid_shape", datum_attachment["diagnostics"])
        self.assertEqual(
            datum_attachment["deferred"],
            [
                "GUI Attachment editor / ViewProvider / TaskPanel",
                "GUI interactive datum resize visual behavior",
            ],
        )
        self.assertEqual(
            datum_attachment["non_goals"],
            [
                "GUI Attachment editor / ViewProvider / TaskPanel",
                "GUI interactive datum resize visual behavior",
                "cross-request backend attachment session",
            ],
        )
        self.assertIn("datum_attach_engine_remaining_modes", datum_attachment["exact_blockers"])
        self.assertNotIn("Vertex", datum_attachment["exact_blockers"]["datum_attach_engine_remaining_modes"]["modes"])
        self.assertNotIn("OnEdge", datum_attachment["exact_blockers"]["datum_attach_engine_remaining_modes"]["modes"])
        self.assertNotIn("CenterOfMass", datum_attachment["exact_blockers"]["datum_attach_engine_remaining_modes"]["modes"])
        self.assertEqual(datum_attachment["remaining_gaps"], [])
        self.assertEqual(
            capabilities["part_design"]["hole"]["thread_tables"],
            [
                "ISOMetricProfile",
                "ISOMetricFineProfile",
                "UNC",
                "UNF",
                "UNEF",
                "NPT",
                "BSP",
                "BSW",
                "BSF",
                "ISOTyre",
            ],
        )
        self.assertIn("thread_tap_drill", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertIn("thread_clearance", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertIn("thread_whitworth_fallback", capabilities["part_design"]["hole"]["diameter_sources"])
        self.assertEqual(
            capabilities["part_design"]["hole"]["head_cut_types"],
            ["None", "Counterbore", "Countersink", "Counterdrill"],
        )
        self.assertIn("iso2009.json", capabilities["part_design"]["hole"]["head_cut_definition_files"])
        self.assertIn("din7984.json", capabilities["part_design"]["hole"]["head_cut_definition_files"])
        self.assertEqual(capabilities["part_design"]["hole"]["model_thread"]["status"], "done_first_slice")
        self.assertEqual(capabilities["part_design"]["hole"]["model_thread"]["geometry"], "pipe_shell")
        self.assertIn("ThreadClass", capabilities["part_design"]["hole"]["model_thread"]["properties"])
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["status"],
            "element_map_freeze_first_slice",
        )
        self.assertIn(
            "find_holes_make_shape_with_element_map",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "profile_source_tool_face_mapper_history",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "point_profile_head_cut_history",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "model_thread_compound_tool_shape",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertIn(
            "threaded_model_thread_head_cut_native_oracle",
            capabilities["part_design"]["hole"]["history"]["covered"],
        )
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["known_gap_fixtures"],
            [],
        )
        self.assertEqual(
            capabilities["part_design"]["hole"]["history"]["remaining"],
            [],
        )
        self.assertIn(
            "p7/hole-supported-model-thread-counterbore",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertIn(
            "p7/hole-supported-model-thread-metric",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertIn(
            "p7/hole-supported-point-counterbore",
            capabilities["part_design"]["hole"]["native_oracle_fixtures"],
        )
        self.assertEqual(capabilities["part_design"]["hole"]["native_oracle_known_gap_fixtures"], [])
        self.assertEqual(
            capabilities["part_design"]["hole"]["remaining_gaps"],
            [],
        )
        self.assertEqual(
            capabilities["adapters"]["core_entrypoints"],
            [
                "cad_core_recompute_json",
                "cad_core_export_json",
                "cad_core_capabilities_json",
                "cli_recompute",
                "worker_recompute",
                "wasm_recompute",
            ],
        )
        self.assertEqual(capabilities["adapters"]["contract_version"], "cad-core-result-v1")
        self.assertEqual(
            capabilities["adapters"]["schema_parity"]["entrypoints"],
            ["cli_recompute", "cad_core_recompute_json", "worker_recompute", "wasm_recompute"],
        )
        self.assertEqual(
            capabilities["adapters"]["schema_parity"]["contract"],
            "same_request_local_core_result",
        )
        self.assertEqual(
            capabilities["adapters"]["schema_parity"]["core_result_producers"],
            ["cad_core::runtime::recomputeResultJson", "cad_core::part::partGeometryCurveResultJson"],
        )
        self.assertEqual(
            capabilities["adapters"]["stateless_result_channels"],
            ["results", "elementReferenceUpdates", "documentObjectUpdates", "diagnostics", "binaryPayloads"],
        )
        self.assertEqual(
            capabilities["adapters"]["resource_diagnostics"],
            ["mesh_limit_exceeded", "adapter_resource_limit"],
        )
        self.assertEqual(
            capabilities["adapters"]["c_api_export"],
            ["buffer_only", "rejects_server_file_paths", "metadata_diagnostics", "stl_deflection"],
        )
        self.assertEqual(
            capabilities["adapters"]["cli_export"],
            ["file_protocol", "requires_object_format_file", "stl_deflection"],
        )
        self.assertEqual(capabilities["adapters"]["worker_adapter"]["entrypoint"], "cad_core_worker_recompute_json")
        self.assertEqual(capabilities["adapters"]["wasm_adapter"]["entrypoint"], "cad_core_wasm_recompute_json")
        self.assertEqual(capabilities["adapters"]["worker_adapter"]["result_contract"], "cad-core-result-v1")
        self.assertEqual(capabilities["adapters"]["wasm_adapter"]["result_contract"], "cad-core-result-v1")
        self.assertIn("adapter_resource_limit", capabilities["adapters"]["worker_adapter"]["resource_diagnostics"])
        self.assertIn("adapter_resource_limit", capabilities["adapters"]["wasm_adapter"]["resource_diagnostics"])
        self.assertIn("max_vertices", capabilities["adapters"]["mesh"]["streaming_limits"])
        self.assertIn("cad_core_mesh_binary_json", capabilities["adapters"]["mesh"]["binary_payloads"])
        self.assertIn("metadata_diagnostics", capabilities["adapters"]["mesh"]["binary_payloads"])
        self.assertEqual(
            capabilities["adapters"]["mesh"]["binary_payload_limits"],
            ["max_bytes", "adapter_resource_limit", "metadata_diagnostics"],
        )
        self.assertEqual(capabilities["adapters"]["remaining_gaps"], [])
        link_sub_fields = [
            "value",
            "SubList",
            "StableSubList",
            "FullSubList",
            "ShadowSub",
            "ReferenceShadow",
            "ExternalFlags",
            "Document",
        ]
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLink"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkGlobal"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkHidden"],
            ["value"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLink"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkList"],
            ["values"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkListHidden"],
            ["values"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkList"],
            ["values", "SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSub"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubHidden"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSub"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSubHidden"],
            link_sub_fields,
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubList"],
            ["SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyLinkSubListHidden"],
            ["SubSet"],
        )
        self.assertEqual(
            capabilities["document"]["link_property_shapes"]["App::PropertyXLinkSubList"],
            ["SubSet"],
        )

        for type_id in [
            "Sketcher::SketchObject",
            "PartDesign::Draft",
            "PartDesign::Thickness",
            "PartDesign::Hole",
            "Part::Box",
            "Part::Offset",
            "Part::Offset2D",
            "Part::Thickness",
            "Part::Loft",
            "Part::Sweep",
            "Part::FilledFace",
            "Part::GeomPlateSurface",
            "Part::BooleanFragments",
            "App::Link",
            "Assembly::AssemblyObject",
        ]:
            self.assertIn(type_id, capabilities["supported_type_ids"])

        for code in [
            "parse_error",
            "document_hash_mismatch",
            "external_document_pending_reload",
            "external_document_unloaded",
            "missing_external_document",
            "missing_external_geometry_snapshot",
            "missing_target",
            "missing_link_target",
            "missing_grounded_part",
            "missing_property",
            "missing_constraints",
            "missing_curve_source",
            "cycle_dependency",
            "execution_failed",
            "perform_failed",
            "surface_not_done",
            "approximation_failed",
            "multiple_solids_disallowed",
            "no_intersection",
            "partdesign_body_tip_non_solid",
            "unsupported_type",
            "unsupported_property",
            "unsupported_sketch_constraint_relation",
            "invalid_curve_source",
            "invalid_point_constraint",
            "invalid_parameter",
            "invalid_subshape",
            "unsupported_subshape_kind",
            "unsupported_stable_subname",
            "label_reference_ambiguous",
            "subname_resolve_ambiguous",
            "subname_resolve_failed",
            "subname_semantic_drift",
            "split_stable_subname",
            "deleted_stable_subname",
            "unsupported_assembly_solver",
            "mesh_limit_exceeded",
            "adapter_resource_limit",
            "unsupported_reference_shadow_brep",
            "sketch_solver_conflict",
            "sketch_solver_malformed_constraint",
            "sketch_solver_redundant",
        ]:
            self.assertIn(code, capabilities["diagnostic_codes"])

        self.assertTrue(self.ondsel_solver_available())
        ondsel_adapter = capabilities["assembly"]["ondsel_solver_adapter"]
        self.assertEqual(ondsel_adapter["status"], "covered_full")
        self.assertEqual(ondsel_adapter["mode"], "request_local_runPreDrag")
        self.assertEqual(ondsel_adapter["available"], True)
        self.assertEqual(ondsel_adapter["build_mode"], "CAD_CORE_HAS_ONDSEL_SOLVER=1")
        self.assertIn("grounded_fixed_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_ball_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_revolute_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_slider_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_distance_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_parallel_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_perpendicular_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_angle_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_gears_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_belt_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_rackpinion_joint", ondsel_adapter["covered"])
        self.assertIn("grounded_screw_joint", ondsel_adapter["covered"])
        self.assertIn("basic_distance_type", ondsel_adapter["covered"])
        self.assertIn("distance_type_extended_geometry", ondsel_adapter["covered"])
        self.assertIn("subshape_marker_placement", ondsel_adapter["covered"])
        basic_distance = ondsel_adapter["distance_type_basic_geometry"]
        self.assertEqual(basic_distance["status"], "covered_full")
        self.assertEqual(
            basic_distance["supported"],
            ["PointPoint", "LineLine", "PointLine", "PlanePlane", "PointPlane", "LinePlane"],
        )
        self.assertEqual(
            basic_distance["solver_joint_classes"]["PointPoint"],
            ["ASMTSphericalJoint", "ASMTSphSphJoint"],
        )
        self.assertEqual(basic_distance["solver_joint_classes"]["LineLine"], ["ASMTRevCylJoint"])
        self.assertEqual(basic_distance["solver_joint_classes"]["PointLine"], ["ASMTLineInPlaneJoint"])
        self.assertEqual(basic_distance["solver_joint_classes"]["PlanePlane"], ["ASMTPlanarJoint"])
        self.assertEqual(basic_distance["solver_joint_classes"]["PointPlane"], ["ASMTPointInPlaneJoint"])
        self.assertEqual(basic_distance["solver_joint_classes"]["LinePlane"], ["ASMTLineInPlaneJoint"])
        self.assertEqual(basic_distance["scalar_fields"], ["distance_ij", "offset"])
        self.assertEqual(basic_distance["remaining_radius_gaps"], [])
        self.assertEqual(basic_distance["extended_geometry_capability"], "distance_type_extended_geometry")
        self.assertIn("persistent_solver_state", basic_distance["non_goals"])
        extended_distance = ondsel_adapter["distance_type_extended_geometry"]
        self.assertEqual(extended_distance["status"], "covered_supported_subset")
        self.assertEqual(extended_distance["native_expected_count"], 13)
        self.assertEqual(
            extended_distance["supported"],
            [
                "LineCircle",
                "CircleCircle",
                "PlaneCylinder",
                "PlaneSphere",
                "CylinderCylinder",
                "CylinderSphere",
                "PointCylinder",
                "PointSphere",
                "PlaneTorus",
                "CylinderTorus",
                "TorusTorus",
                "TorusSphere",
                "SphereSphere",
            ],
        )
        self.assertEqual(extended_distance["solver_joint_classes"]["LineCircle"], ["ASMTRevCylJoint"])
        self.assertEqual(extended_distance["solver_joint_classes"]["PlaneCylinder"], ["ASMTLineInPlaneJoint"])
        self.assertEqual(extended_distance["solver_joint_classes"]["PointSphere"], ["ASMTSphSphJoint"])
        self.assertIn("reference1_primitive", extended_distance["evidence_fields"])
        self.assertIn("reference2_radius", extended_distance["evidence_fields"])
        self.assertIn("scalar_correction", extended_distance["scalar_correction_fields"])
        self.assertIn("scalar_correction_source", extended_distance["scalar_correction_fields"])
        self.assertIn("identity_offset_assembly_link_subset", extended_distance["request_local_boundaries"])
        self.assertIn(
            "distance_writeback_uses_user_distance_not_radius_corrected_scalar",
            extended_distance["request_local_boundaries"],
        )
        self.assertEqual(extended_distance["deferred_diagnostic_cases"], ["PointCurve"])
        self.assertIn("PlaneCone", extended_distance["default_or_todo_boundaries"])
        self.assertIn("LineCylinder", extended_distance["default_or_todo_boundaries"])
        self.assertIn("CurvePlane", extended_distance["default_or_todo_boundaries"])
        self.assertIn("Other", extended_distance["default_or_todo_boundaries"])
        self.assertIn("PointCurve", extended_distance["non_goals"])
        self.assertIn("default_or_todo_branch_support", extended_distance["non_goals"])
        self.assertNotIn("PointCurve", extended_distance["supported"])
        self.assertIn(
            "invalid_grounded_placement_rejected",
            ondsel_adapter["covered"],
        )
        self.assertEqual(
            capabilities["assembly"]["representative_solver_adapter"]["status"],
            "covered_representative",
        )
        self.assertEqual(
            capabilities["assembly"]["representative_solver_adapter"]["mode"],
            "fallback_metadata_only",
        )
        self.assertFalse(capabilities["assembly"]["representative_solver_adapter"]["available"])
        self.assertIn(
            "full_solver",
            capabilities["assembly"]["representative_solver_adapter"]["non_goals"],
        )
        validation = capabilities["assembly"]["solver_validation"]
        self.assertEqual(validation["status"], "covered_diagnostic_boundaries")
        self.assertIn("unsupported_assembly_solver", validation["diagnostic_codes"])
        self.assertIn("missing_grounded_part", validation["diagnostic_codes"])
        self.assertIn("ondsel_solver_failed", validation["diagnostic_codes"])
        self.assertIn("invalid_assembly_solver_result", validation["diagnostic_codes"])
        self.assertIn(
            "assembly-runtime-adapter-missing-grounded-part-diagnostic",
            validation["fixture_rows"],
        )
        self.assertIn(
            "frontend_graph_is_source_of_truth",
            validation["request_local_boundaries"],
        )
        subshape_marker = ondsel_adapter["subshape_marker_placement"]
        self.assertEqual(subshape_marker["status"], "covered_representative_subset")
        self.assertEqual(subshape_marker["mode"], "request_local_handleOneSide_markerPlacement")
        self.assertEqual(subshape_marker["build_mode"], "CAD_CORE_HAS_ONDSEL_SOLVER=1")
        self.assertEqual(subshape_marker["supported_reference_kinds"], ["object", "Vertex", "Edge", "Face", "mixed"])
        self.assertIn("object_level_baseline", subshape_marker["covered"])
        self.assertIn("vertex_jcs_marker", subshape_marker["covered"])
        self.assertIn("edge_jcs_marker", subshape_marker["covered"])
        self.assertIn("face_jcs_marker", subshape_marker["covered"])
        self.assertIn("assembly_link_identity_offset_subshape_marker", subshape_marker["covered"])
        self.assertIn("non_linear_edge_and_non_planar_face_identity_offset", subshape_marker["covered"])
        self.assertIn("mixed_swap_marker_sync", subshape_marker["covered"])
        self.assertIn("real_ondsel_marker_consumption", subshape_marker["covered"])
        self.assertIn("placement_updates_native_parity", subshape_marker["covered"])
        self.assertEqual(subshape_marker["active_expected_count"], 28)
        self.assertIn("identity_offset_assembly_link_subset", subshape_marker["request_local_boundaries"])
        self.assertIn("request_graph_no_persistent_solver_state", subshape_marker["request_local_boundaries"])
        self.assertNotIn("radius_bearing_distance_type", subshape_marker["non_goals"])
        self.assertIn("curve_default_distance_type", subshape_marker["non_goals"])
        self.assertIn("GUI/session", subshape_marker["non_goals"])
        self.assertIn("persistent_solver_state", subshape_marker["non_goals"])
        self.assertIn("non_assembly_link_subshape_primitive_frame_generalization", subshape_marker["non_goals"])
        self.assertIn("non_identity_bundled_offsetPlc", subshape_marker["non_goals"])
        self.assertEqual(subshape_marker["remaining_gaps"], [])
        self.assertEqual(ondsel_adapter["remaining_gaps"], [])
        self.assertEqual(
            capabilities["assembly"]["placement_writeback"]["solver_modes"],
            ["real_ondsel_solver"],
        )
        self.assertEqual(capabilities["assembly"]["placement_writeback"]["status"], "covered_full")
        self.assertEqual(
            capabilities["assembly"]["placement_writeback"]["build_mode"],
            "CAD_CORE_HAS_ONDSEL_SOLVER=1",
        )
        self.assertEqual(
            capabilities["assembly"]["placement_writeback"]["updates"],
            ["documentObjectUpdates.action=assembly_set_placement"],
        )
        self.assertEqual(
            capabilities["assembly"]["placement_writeback"]["mode"],
            "request_local_runPreDrag",
        )
        self.assertIn(
            "request_graph_apply_next_recompute_noop",
            capabilities["assembly"]["placement_writeback"]["covered"],
        )
        self.assertIn(
            "multi_component_writeback_order",
            capabilities["assembly"]["placement_writeback"]["covered"],
        )
        self.assertIn(
            "partial_writeback_subset",
            capabilities["assembly"]["placement_writeback"]["covered"],
        )
        self.assertEqual(capabilities["assembly"]["placement_writeback"]["remaining_gaps"], [])
        self.assertEqual(
            capabilities["assembly"]["supported_joint_matrix"],
            ["Fixed", "Revolute", "Cylindrical", "Slider", "Ball", "Distance", "Parallel", "Perpendicular", "Angle", "Gears", "Belt", "RackPinion", "Screw"],
        )
        self.assertEqual(
            capabilities["assembly"]["unsupported_joint_matrix"],
            [],
        )
        self.assertEqual(capabilities["assembly"]["remaining_gaps"], [])
        self.assertEqual(
            capabilities["wire_joiner"]["generated_open_export_bridge"]["status"],
            "covered_full",
        )
        self.assertIn(
            "result_slot_vertex_evidence_output",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_result_slot_vertex_evidence_wire_info_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_wire_info_source_edge_producer_output_sidecar",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "child_wire_result_slot_endpoint_materialization_edge_shape",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "endpoint_materialization_edge_seed_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "edge_info_producer_open_export_shape",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "edge_info_result_wire_producer_source_edge_export_shape",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "result_wire_producer_identity_source_shape_ready",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "result_wire_producer_identity_classifier_booleans",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_closed_source_result_slot_bridge",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_closed_source_result_slot_bridge_wire_info_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "result_wire_producer_blocker_transitional_result_slot_shape_still_used",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "result_wire_producer_state_transitional_result_slot_candidate",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "child_wire_result_slot_endpoint_materialization_evidence_vertices_field",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "child_wire_producer_ledger_wire_from_result_slot_evidence_field",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_producer_ledger_wire_from_result_slot_evidence",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "edgeInfo_resultWireProducerCandidate_internal",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        for deleted_field in [
            "wire_joiner_endpoint_materialization_ledger_vertex_seed",
            "child_wire_result_slot_endpoint_materialization_vertex_ledger",
            "producer_child_wire_result_slot_endpoint_materialization_counted",
            "wire_joiner_endpoint_materialization_ledger_current_member_debt_scoped",
            "child_wire_endpoint_materialization_evidence_field_renamed",
            "child_wire_producer_ledger_wire_endpoint_materialization_evidence_field_renamed",
            "result_slot_endpoint_materialization_ledger",
            "matched_endpoint_materialization_evidence",
            "endpoint_materialization_evidence_vertex_matches_other_output",
            "endpoint_materialization_evidence_vertex_identity",
            "open_wire_compound_endpoint_provenance_endpoint_materialization_matched_vertex_count",
            "open_wire_compound_current_member_split_ledger_endpoint_materialization_distinct_vertex_count",
            "open_wire_compound_current_member_split_ledger_endpoint_materialization_other_output_matched_vertex_count",
            "open_wire_compound_export_source_result_wire_producer_slot_value",
            "open_wire_compound_export_source_history_materialized_child_slot_value",
            "result_wire_producer_state_source_shape_ready",
            "history_materialization_entry_open_wire_compound_child_wire_candidate_bool",
            "mapper_evidence_result_wire_producer_identity_fields",
            "mapper_diagnostic_result_wire_producer_blocker_status",
            "mapper_diagnostic_missing_producer_identity_fallback",
            "result_wire_producer_entry_gate_from_materialization_entry_identity",
            "open_wire_compound_export_source_from_materialization_entry",
            "history_materialization_entry_typed_open_wire_compound_export_source",
            "named_shape_history_missing_result_wire_identity_count",
            "element_map_result_wire_identity_mismatch_count",
        ]:
            self.assertIn(
                deleted_field,
                capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
            )
        self.assertNotIn(
            "result_slot_vertex_evidence_output_count_zero",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "active_legacy_helper_open_export_override_not_exported",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "producer_identity_consumed_without_legacy_helper_flag",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertEqual(
            capabilities["wire_joiner"]["generated_open_export_bridge"]["remaining_gaps"],
            [],
        )
        self.assertNotIn(
            "child_wire_producer_ledger_edge_copy_gate_from_history_materialization_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["remaining_gaps"],
        )
        self.assertNotIn(
            "wire_joiner_history_materialization_ledger_result_slot_source_candidate_locator",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["remaining_gaps"],
        )
        self.assertIn(
            "history_materialization_binding_source_edgeinfo_candidate_list",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "history_materialization_binding_final_edgeinfo_index",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "open_wire_compound_export_source_ahistory_producer_child_wire",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "open_wire_compound_export_source_from_child_wire_final_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "mapper_history_aHistory_open_export_element_map_lifecycle",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "mapper_history_aHistory_split_deleted_terminal_history",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "wire_joiner_noOriginal_deleted_relation_from_mapper_history",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        for covered_field in [
            "history_materialization_binding_open_wire_compound_eligible_cache_removed",
            "history_materialization_entry_source_edgeinfo_identity_cache_removed",
            "history_materialization_entry_open_export_gate_cache_removed",
            "history_materialization_entry_full_ahistory_cache_removed",
            "history_materialization_entry_ahistory_source_lineage_cache_removed",
        ]:
            self.assertIn(
                covered_field,
                capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
            )
        self.assertNotIn(
            "plan_ledger_producer_open_export_edge_gated_by_classified_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        for removed_gap in [
            "source_shape_ready_derived_from_history_materialization_ledger_open_export_edge",
            "producer_ledger_ready_gate_direct_history_materialization_ledger",
        ]:
            self.assertNotIn(
                removed_gap,
                capabilities["wire_joiner"]["generated_open_export_bridge"]["remaining_gaps"],
            )
        self.assertIn(
            "source_shape_ready_derived_from_history_materialization_ledger_open_export_edge",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "edge_level_producer_ledger_ready_from_history_materialization_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertNotIn(
            "result_wire_producer_state_producer_ledger_ready",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "producer_readiness_promoted_after_openWireCompound_child_materialization",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "edge_level_producer_ledger_ready_gate_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "history_materialization_entry_typed_open_wire_compound_export_source",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "history_materialization_entry_open_wire_compound_export_source_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_wire_producer_identity_source_shape_ready_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_wire_producer_identity_classifier_booleans_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "legacy_helper_reason_not_exported",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "legacy_helper_reason_string_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "edge_info_result_wire_producer_candidate_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_legacy_helper_ahistory_sidecar_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_dead_helper_sidecars_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_producer_identity_candidate_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_wire_producer_ledger_entries_from_child_wire",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_result_wire_producer_entry_gate",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_result_wire_producer_entry_gate_from_classified_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_wire_producer_entry_gate_after_child_wire_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "edge_info_open_export_wire_helper_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "open_wire_compound_source_edge_producer_output",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_source_edge_producer_output_wire_info_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "child_wire_source_edge_producer_output_public_diagnostic",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertNotIn(
            "child_wire_source_edge_producer_output_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "edge_info_identity_source_edge_output_gate_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertNotIn(
            "child_wire_source_edge_producer_output_from_classified_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertNotIn(
            "child_wire_source_edge_producer_output_derived_from_producer_ledger_edge",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["remaining_gaps"],
        )
        self.assertNotIn(
            "child_wire_producer_ledger_edge_materialized",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_producer_ledger_edge_copy_gate_from_history_materialization_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "child_wire_producer_ledger_edge_copy_gate_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_producer_readiness_from_materialized_wire",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_producer_ledger_wire_materialized",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "wire_joiner_history_relation_from_child_wire_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "wire_joiner_history_event_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_consumes_wire_joiner_history_event_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_consumes_openWireCompound_child_wire_ownership_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "wire_joiner_open_export_mapper_history_concrete_events",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_mapper_evidence_result_wire_producer_identity_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_mapper_diagnostic_result_wire_producer_blocker_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_mapper_diagnostic_missing_producer_identity_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_result_wire_identity_counters_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "wire_joiner_open_export_element_map_unique_child_wire_alias",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "topo_open_export_relation_fallback_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_open_export_ownership_source_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_shape_identity_inventory",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_wire_producer_entry_consumes_child_wire_ownership",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_endpoint_identity_debt_per_endpoint",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_source_vmap_endpoint_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_vmap_replacement_event_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_endpoint_provenance_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "child_wire_root_member_producer_lifecycle_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "openWireCompound_child_wire_source_lineage",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_fragment_to_source_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_modified_generated_fragment_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_source_lineage_uses_sourceEdgeArray_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_input_edgeinfo_source_sidecar",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_source_identity_fallback_counter_split",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "splitEdges_history_shape_geometry_bridge_counter",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "closed_cycle_open_export_from_split_fragment_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "result_slot_vertex_evidence_endpoint_materialization_not_export_shape",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "producer_child_wire_prefers_vmap_vertex_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_child_wire_uses_root_member_ledger_without_result_slot_output",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "open_export_history_consumes_openWireCompound_child_wire_output",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "open_export_history_no_edgeinfo_reexport_fallback",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "missing_child_wire_history_diagnostic_without_edgeinfo_payload",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "getOpenWires_consumes_openWireCompound_child_ledger_only",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_split_ledger_vertex_debt_recorded_on_child_wire",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_split_ledger_candidate_output_identity_recorded",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_split_ledger_endpoint_identity_resolver",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_split_ledger_candidate_multiplicity_loss_diagnostic",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "current_member_closed_source_result_slot_bridge_diagnostic_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "transitional_result_slot_shape_still_used_blocker_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "transitional_result_slot_candidate_state_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertIn(
            "edge_info_result_slot_vertex_evidence_removed",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertNotIn(
            "no_original_final_output_prune_fallback_only",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["covered"],
        )
        self.assertNotIn(
            "resultSlotVertexEvidenceEdge",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "result_slot_endpoint_materialization_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_producer_ledger_edge_materialized",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_producer_ledger_edge_materialized",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_export_source",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_edge_info_iteration",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_edge_info_iteration2",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_owner_wire_info",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_owner_wire_info2",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_open_leaf_export",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_unowned_open_edge_export",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_root_current_member_child_producer",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_child_shape_identity_recorded",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_child_wire_edge_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_child_wire_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_source_vmap_endpoint_ledger",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_source_vmap_endpoint_ledger_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_endpoint_provenance_recorded",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_endpoint_provenance_source_vmap_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_endpoint_provenance_vmap_replacement_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_endpoint_provenance_endpoint_materialization_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_vmap_replacement_event_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_vmap_replacement_events",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "wire_joiner_history_event_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "wire_joiner_history_event_from_child_wire_ledger_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_producer_ledger_wire_from_result_slot_evidence",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_closed_source_result_slot_bridge",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_vertex_candidate",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_vertex_debt_recorded",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_member_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_vertex_ledger_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_candidate_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_distinct_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_candidate_distinct_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_endpoint_materialization_distinct_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_candidate_vertex_multiplicity_loss_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_other_output_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_candidate_other_output_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_endpoint_materialization_other_output_matched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_candidate_vertex_reuse_risk_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_candidate_missing_shared_output_identity_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_endpoint_identity_resolver",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_vertex_debt",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_output_unmatched_vertex_total",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_result_slot_only_vertex",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertNotIn(
            "open_wire_compound_current_member_split_ledger_result_slot_only_vertex_total",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "result_slot_only_identity",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_result_slot_only_vertex",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_result_slot_only_vertex_total",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_candidate_missing_shared_output_identity_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked_wire_info_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_unmatched_vertex_count",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_output_unmatched_vertex_total",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_current_member_split_ledger_vertex_multiplicity_blocked",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["deleted_fields"],
        )
        self.assertNotIn(
            "result_slot_vertex_evidence_output",
            capabilities["wire_joiner"]["generated_open_export_bridge"]["diagnostic_fields"],
        )
        self.assertEqual(
            capabilities["wire_joiner"]["purge_as_original_bridge"]["status"],
            "covered_full",
        )
        self.assertEqual(
            capabilities["wire_joiner"]["purge_as_original_bridge"]["mode"],
            "mapper_history_noOriginal_deleted_lifecycle",
        )
        self.assertIn(
            "openWireCompound_noOriginal_candidate_public_bridge_removed",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        for deleted_field in [
            "open_wire_compound_no_original_purge_candidate",
            "open_wire_compound_no_original_purge_candidate_wire_info_count",
            "source_identity_no_original_purge_candidate_edge_info_count",
            "open_wire_compound_no_original_purge_unmatched_wire_info_count",
        ]:
            self.assertIn(
                deleted_field,
                capabilities["wire_joiner"]["purge_as_original_bridge"]["deleted_fields"],
            )
        self.assertIn(
            "openWireCompound_child_wire_noOriginal_match",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "openWireCompound_child_wire_noOriginal_purge_verdict",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "openWireCompound_noOriginal_group_purge_verdict",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "openWireCompound_child_wire_noOriginal_shared_source_edge_ledger",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "splitFromInputEdge_from_splitter_history_ledger",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "noOriginal_uses_splitter_history_fragment_ledger",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "noOriginal_final_output_prune_removed",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "noOriginal_deleted_relation_from_mapper_history",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "noOriginal_split_deleted_terminal_history",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "edge_info_noOriginal_candidate_helper_removed",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["covered"],
        )
        self.assertIn(
            "purgeAsOriginalOpenEdge",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "purge_bridge_history_field",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "source_identity_purge_bridge_summary_fields",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["deleted_fields"],
        )
        self.assertIn(
            "open_wire_compound_no_original_shared_source_matched_edge_count",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["diagnostic_fields"],
        )
        self.assertIn(
            "open_wire_compound_purge_bridge_summary_fields",
            capabilities["wire_joiner"]["purge_as_original_bridge"]["deleted_fields"],
        )
        self.assertNotIn("object_metadata_gaps", capabilities)
        taper_history = capabilities["object_metadata"]["local_history"]["taper_history"]
        self.assertEqual(taper_history["status"], "covered_full")
        self.assertEqual(taper_history["remaining_gaps"], [])
        self.assertEqual(
            taper_history["objects"],
            ["part_design.pad", "part_design.pocket", "part.extrusion"],
        )
        self.assertIn(
            "object_result.topo_naming_history=maker_history:taper_thru_sections",
            taper_history["metadata"],
        )
        self.assertIn("p3b/pad-length-taper", taper_history["fixtures"])
        self.assertIn("p3b/pocket-length-taper", taper_history["fixtures"])
        self.assertIn("p5/part-extrusion-taper", taper_history["fixtures"])
        self.assertEqual(capabilities["object_metadata"]["remaining_gaps"], [])

        self.assertEqual(
            capabilities["topo_history"]["stable_subname_resolution"],
            [
                "indexed",
                "source_preserved",
                "one_to_one_history",
                "unique_same_kind_split_recovery",
                "reference_shadow_recovery",
            ],
        )
        self.assertEqual(
            capabilities["topo_history"]["mapper_history_core"],
            [
                "source_endpoint",
                "target_endpoint",
                "shape_kind",
                "relation",
                "maker_stage",
                "evidence",
                "recoverability",
                "diagnostic_status",
                "summary_only_diagnostics",
                "legacy_history_conversion",
                "element_map_preserved_aliases",
            ],
        )
        self.assertIn("refine_modified_deleted_generated", capabilities["topo_history"]["maker_history"])
        self.assertIn("sketch_internalshape_producer_evidence", capabilities["topo_history"]["maker_history"])
        self.assertIn("taper_thru_sections", capabilities["topo_history"]["maker_history"])
        self.assertIn("part_design_revolve", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_addsubshape_slot", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_multi_selection_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_draft_parameter_variants", capabilities["topo_history"]["maker_history"])
        self.assertIn("dressup_thickness_parameter_variants", capabilities["topo_history"]["maker_history"])
        self.assertIn("thickness_multi_solid_fuse_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("chain_dressup_pattern_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("shapefix_deleted_small_edge", capabilities["topo_history"]["maker_history"])
        self.assertIn("shapefix_root_modified_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("import_shape_element_map", capabilities["topo_history"]["maker_history"])
        self.assertIn("part_offset", capabilities["topo_history"]["maker_history"])
        self.assertIn("part_design_loft", capabilities["topo_history"]["maker_history"])
        self.assertIn("part_design_pipe", capabilities["topo_history"]["maker_history"])
        self.assertIn("loft_thru_sections", capabilities["topo_history"]["maker_history"])
        self.assertIn("pipeshell", capabilities["topo_history"]["maker_history"])
        self.assertIn("filling", capabilities["topo_history"]["maker_history"])
        self.assertIn("transformed_pattern_addsub_ownership", capabilities["topo_history"]["maker_history"])
        self.assertIn("transformed_pattern_full_history", capabilities["topo_history"]["maker_history"])
        self.assertIn("hole_find_holes_profile_source_history", capabilities["topo_history"]["maker_history"])
        producer_matrix = capabilities["topo_history"]["producer_matrix"]
        self.assertEqual(producer_matrix["shape_fix"]["status"], "covered_no_generated_producer")
        self.assertEqual(
            producer_matrix["shape_fix"]["covered"],
            ["deleted_small_edge", "root_modified", "generated_empty_review"],
        )
        self.assertEqual(producer_matrix["shape_fix"]["remaining"], [])
        self.assertEqual(producer_matrix["refine"]["status"], "covered")
        self.assertEqual(
            producer_matrix["refine"]["covered"],
            ["modified", "deleted", "generated"],
        )
        self.assertEqual(producer_matrix["section"]["status"], "covered")
        self.assertEqual(
            producer_matrix["section"]["covered"],
            [
                "approximation_property",
                "auto_fuzzy_value",
                "source_qualified_edge_history",
                "terminal_deleted_history",
            ],
        )
        self.assertEqual(producer_matrix["section"]["remaining"], [])
        self.assertEqual(producer_matrix["part_offset"]["status"], "done_second_slice")
        self.assertIn("face_source_offset", producer_matrix["part_offset"]["covered"])
        self.assertIn("fill_offset", producer_matrix["part_offset"]["covered"])
        self.assertIn("solid_source_make_element_solid", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_open_wire_no_fill", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_open_wire_fill", producer_matrix["part_offset"]["covered"])
        self.assertNotIn("fill_offset", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("solid_source_make_element_solid", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("offset2d_open_wire_no_fill", producer_matrix["part_offset"]["remaining"])
        self.assertNotIn("offset2d_open_wire_fill", producer_matrix["part_offset"]["remaining"])
        self.assertIn("offset2d_compound_child_recursive", producer_matrix["part_offset"]["covered"])
        self.assertIn("offset2d_compound_collective", producer_matrix["part_offset"]["covered"])
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_compound_collective",
            producer_matrix["part_offset"]["remaining"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_intersection_compound_collective",
            producer_matrix["part_offset"]["remaining"],
        )
        self.assertEqual(producer_matrix["import_shape"]["status"], "done_first_slice")
        self.assertIn("owner_qualified_alias", producer_matrix["import_shape"]["covered"])
        self.assertEqual(producer_matrix["sketch_internalshape"]["status"], "done_first_slice")
        self.assertIn(
            "mixed_bounded_faces_open_wires_oracle",
            producer_matrix["sketch_internalshape"]["covered"],
        )
        self.assertEqual(producer_matrix["sketch_internalshape"]["remaining"], [])
        self.assertEqual(producer_matrix["dressup"]["status"], "done_first_slice")
        self.assertIn("multi_selection_history", producer_matrix["dressup"]["covered"])
        self.assertIn("chamfer_parameter_variants", producer_matrix["dressup"]["covered"])
        self.assertIn("draft_datum_plane_line", producer_matrix["dressup"]["covered"])
        self.assertIn("draft_auto_neutral_plane_guess", producer_matrix["dressup"]["covered"])
        self.assertIn("thickness_parameter_variants", producer_matrix["dressup"]["covered"])
        self.assertIn("thickness_multi_solid_fuse_history", producer_matrix["dressup"]["covered"])
        self.assertIn("failure_diagnostics", producer_matrix["dressup"]["covered"])
        self.assertIn("chain_dressup_pattern_history", producer_matrix["dressup"]["covered"])
        self.assertNotIn("multi_selection_history", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("chamfer_parameter_variants", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("draft_datum_plane_line", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("draft_auto_neutral_plane_guess", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("thickness_parameter_variants", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("thickness_multi_solid_fuse_history", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("failure_diagnostics", producer_matrix["dressup"]["remaining"])
        self.assertNotIn("chain_dressup_pattern_history", producer_matrix["dressup"]["remaining"])
        self.assertEqual(producer_matrix["dressup"]["remaining"], [])
        self.assertEqual(producer_matrix["loft_thru_sections"]["status"], "done_expected_backed_first_batch")
        self.assertIn("generated_faces", producer_matrix["loft_thru_sections"]["covered"])
        self.assertIn("first_shape_last_shape_history", producer_matrix["loft_thru_sections"]["covered"])
        self.assertIn("source_profile_sections", producer_matrix["loft_thru_sections"]["covered"])
        self.assertIn(
            "solid_ruled_closed_max_degree_fixtures",
            producer_matrix["loft_thru_sections"]["covered"],
        )
        self.assertEqual(producer_matrix["loft_thru_sections"]["remaining"], [])
        self.assertEqual(producer_matrix["pipeshell"]["status"], "done_expected_backed_first_batch")
        self.assertIn("generated_modified_history", producer_matrix["pipeshell"]["covered"])
        self.assertIn("spine_profile_sources", producer_matrix["pipeshell"]["covered"])
        self.assertIn("spine_sublist_compound", producer_matrix["pipeshell"]["covered"])
        self.assertIn("solid_frenet_transition_fixtures", producer_matrix["pipeshell"]["covered"])
        self.assertEqual(producer_matrix["pipeshell"]["remaining"], [])
        self.assertEqual(
            producer_matrix["part_filling"]["status"],
            "done_expected_backed_first_batch_plus_c5m8_s4_compound_wrapper_boundary",
        )
        self.assertIn("maker_history:filling", producer_matrix["part_filling"]["covered"])
        self.assertIn("boundary_source_evidence", producer_matrix["part_filling"]["covered"])
        self.assertIn("closed_wire_default", producer_matrix["part_filling"]["covered"])
        self.assertIn("connected_boundary_edges_default", producer_matrix["part_filling"]["covered"])
        self.assertIn("invalid_diagnostics", producer_matrix["part_filling"]["covered"])
        self.assertIn("initial_surface_load_init_surface", producer_matrix["part_filling"]["covered"])
        self.assertIn("support_face_source_map", producer_matrix["part_filling"]["covered"])
        self.assertIn("order_source_map", producer_matrix["part_filling"]["covered"])
        self.assertIn("locatable_support_order_diagnostics", producer_matrix["part_filling"]["covered"])
        self.assertIn("constructor_params_metadata", producer_matrix["part_filling"]["covered"])
        self.assertIn("locatable_param_diagnostics", producer_matrix["part_filling"]["covered"])
        self.assertIn("non_boundary_constraint_source_evidence", producer_matrix["part_filling"]["covered"])
        self.assertIn("locatable_non_boundary_diagnostics", producer_matrix["part_filling"]["covered"])
        self.assertIn("compound_source_expansion", producer_matrix["part_filling"]["covered"])
        self.assertIn(
            "direct_makefilling_wrapper_lifecycle_diagnostic",
            producer_matrix["part_filling"]["covered"],
        )
        self.assertIn(
            "surface_support_order_native_helper_expected",
            producer_matrix["part_filling"]["remaining"],
        )
        self.assertIn(
            "non_default_params_native_helper_expected",
            producer_matrix["part_filling"]["remaining"],
        )
        self.assertIn(
            "non_boundary_edge_support_native_helper_expected",
            producer_matrix["part_filling"]["remaining"],
        )
        self.assertNotIn(
            "compound_boundary_optional_expected",
            producer_matrix["part_filling"]["remaining"],
        )
        self.assertEqual(producer_matrix["transformed"]["status"], "covered")
        self.assertIn("link_retag_composition", producer_matrix["transformed"]["covered"])
        self.assertIn("terminal_split_deleted", producer_matrix["transformed"]["covered"])
        self.assertEqual(producer_matrix["transformed"]["remaining"], [])
        self.assertIn("flagged_compound_tool_expansion", producer_matrix["body_boolean"]["covered"])
        self.assertIn("flagged_compound_tool_expansion", producer_matrix["part_boolean"]["covered"])
        self.assertEqual(producer_matrix["hole"]["status"], "done_first_slice")
        self.assertIn("profile_source_tool_face_mapper_history", producer_matrix["hole"]["covered"])
        self.assertIn("point_profile_head_cut_history", producer_matrix["hole"]["covered"])
        self.assertIn("model_thread_compound_tool_shape", producer_matrix["hole"]["covered"])
        self.assertIn("threaded_model_thread_head_cut_native_oracle", producer_matrix["hole"]["covered"])
        self.assertEqual(producer_matrix["hole"]["known_gap_fixtures"], [])
        self.assertEqual(producer_matrix["hole"]["remaining"], [])
        hole_capability = capabilities["part_design"]["hole"]
        self.assertEqual(hole_capability["history"]["status"], "element_map_freeze_first_slice")
        self.assertIn("profile_source_tool_face_mapper_history", hole_capability["history"]["covered"])
        self.assertEqual(
            hole_capability["remaining_gaps"],
            [],
        )
        self.assertEqual(capabilities["topo_history"]["terminal_history"], ["deleted", "split", "merge"])
        self.assertEqual(
            capabilities["topo_history"]["element_history_status"],
            [
                "generated_modified",
                "terminal_split_deleted",
                "subname_split_requires_reselect",
                "merge",
                "facemaker_pre_split",
                "facemaker_splitter",
                "facemaker_summary_only",
                "wire_joiner_history:element_map",
                "import_shape_element_map",
                "shapefix_root_history_modified",
                "element_map_policy_drop",
                "element_map_child_map:preserve_source_ranges",
                "element_map_child_map:recursive_source_ranges",
                "element_map_child_map:postfix_source_ranges",
                "element_map_child_map:hashed_child_map_keys",
                "element_map_policy_propagate:make_element_wires",
                "element_map_policy_propagate:make_element_shell",
                "hole_find_holes:profile_source",
                "hole_cut_history:element_map_freeze",
                "hole_model_thread:pipe_shell_tool_history",
                "part_sweep:pipeshell_history",
                "boolean_compound_tool:expand_children",
                "part_compound:make_element_compound",
                "part_offset_fill:sewing_history",
                "part_make_solid:make_element_solid",
                "part_offset2d:face_no_fill_makeoffset",
                "part_offset2d:face_fill_closed_makeoffset",
                "part_offset2d:wire_no_fill_makeoffset",
                "part_offset2d:wire_fill_open_makeoffset",
                "part_offset2d:compound_child_recursive",
                "part_offset2d:compound_collective_makeoffset",
                "part_thickness:make_thick_solid",
            ],
        )
        self.assertNotIn("sketch_internalshape_main_path", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("taper_full_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("import_shape_element_map", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_modified_generated_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("shapefix_generated_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn("transformed_pattern_full_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertEqual(capabilities["part_workbench"]["offset"]["status"], "done_second_slice")
        self.assertIn("Part::Compound", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("Part::Offset", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("Part::Offset2D", capabilities["part_workbench"]["offset"]["type_ids"])
        self.assertIn("fill_offset", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("solid_source_make_element_solid", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_face_no_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_face_fill_closed", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_open_wire_no_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_open_wire_fill", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_compound_child_recursive", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("offset2d_compound_collective", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("thickness_single_solid_face", capabilities["part_workbench"]["offset"]["covered"])
        self.assertIn("thickness_mode_join_oracle", capabilities["part_workbench"]["offset"]["covered"])
        self.assertNotIn("fill_offset", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "solid_source_make_element_solid",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn("offset2d", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_open_compound",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_open_wire_compound",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_open_wire_no_fill",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_open_wire_fill",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn("thickness", capabilities["part_workbench"]["offset"]["remaining_gaps"])
        self.assertNotIn(
            "thickness_mode_join_oracle",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        self.assertNotIn(
            "offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["part_workbench"]["offset"]["remaining_gaps"],
        )
        conic_curves = capabilities["part_workbench"]["conic_curves"]
        self.assertEqual(conic_curves["status"], "done_part_geometry_curve_edge_consumer")
        self.assertEqual(conic_curves["dto"], "PartConicCurveDTO")
        self.assertIn("partGeometryCurve", conic_curves["payload_keys"])
        self.assertIn("partGeometryCurveConsumers", conic_curves["payload_keys"])
        self.assertIn("Part.Hyperbola", conic_curves["part_geometry_types"])
        self.assertIn("Part.Parabola", conic_curves["part_geometry_types"])
        self.assertIn("GeomAbs_Hyperbola", conic_curves["curve_types"])
        self.assertIn("GeomAbs_Parabola", conic_curves["curve_types"])
        self.assertIn("hyperbola_finite_edge", conic_curves["covered"])
        self.assertIn("parabola_finite_edge", conic_curves["covered"])
        self.assertIn("part_extrusion_edge_to_face_consumer", conic_curves["covered"])
        self.assertIn("p8/part-hyperbola-edge", conic_curves["fixtures"])
        self.assertIn("p8/part-parabola-edge", conic_curves["fixtures"])
        self.assertIn("p8/part-conic-edge-invalid-params", conic_curves["fixtures"])
        self.assertIn("p8/part-conic-edge-extrusion", conic_curves["fixtures"])
        self.assertIn("p8/part-ruled-surface-conic-line", conic_curves["fixtures"])
        self.assertIn("invalid_part_conic_curve_kind", conic_curves["diagnostics"])
        self.assertIn("Part::Extrusion", conic_curves["consumer_type_ids"])
        self.assertIn("Part::RuledSurface", conic_curves["consumer_type_ids"])
        self.assertNotIn("Part::Hyperbola", conic_curves["consumer_type_ids"])
        self.assertNotIn("Part::Parabola", conic_curves["consumer_type_ids"])
        self.assertIn(
            "no_part_hyperbola_document_object_executor",
            conic_curves["request_local_boundaries"],
        )
        self.assertIn(
            "no_part_parabola_document_object_executor",
            conic_curves["request_local_boundaries"],
        )
        self.assertIn(
            "conic_edge_is_request_local_producer_not_document_object",
            conic_curves["request_local_boundaries"],
        )
        self.assertEqual(
            conic_curves["remaining_gaps"],
            [
                "gui_conic_edit",
                "full_sketcher_solver_conic_constraints",
                "distance_type_default_todo",
            ],
        )
        self.assertNotIn("full_part_surface_family", conic_curves["remaining_gaps"])
        self.assertNotIn("ruled_surface", conic_curves["remaining_gaps"])
        self.assertNotIn("projection_on_surface", conic_curves["remaining_gaps"])
        self.assertIn(
            "full_sketcher_solver_conic_constraints",
            conic_curves["remaining_gaps"],
        )
        project_on_surface = capabilities["part_workbench"]["project_on_surface"]
        self.assertEqual(project_on_surface["status"], "supported_expected_backed_published_slice")
        self.assertIn("Part::ProjectOnSurface", project_on_surface["type_ids"])
        self.assertEqual(
            project_on_surface["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.Mode.value",
                "Objects[].Properties.Height.value",
                "Objects[].Properties.Offset.value",
                "Objects[].Properties.Direction.value",
                "Objects[].Properties.SupportFace.value",
                "Objects[].Properties.SupportFace.SubList",
                "Objects[].Properties.Projection.SubSet",
                "recompute.objs",
            ],
        )
        for prop in ("Mode", "Height", "Offset", "Direction", "SupportFace", "Projection"):
            self.assertIn(prop, project_on_surface["properties"])
        for property_type in (
            "App::PropertyEnumeration",
            "App::PropertyLength",
            "App::PropertyDistance",
            "App::PropertyDirection",
            "App::PropertyLinkSub",
            "App::PropertyLinkSubList",
        ):
            self.assertIn(property_type, project_on_surface["property_types"])
        self.assertEqual(project_on_surface["mode_values"], ["All", "Faces", "Edges"])
        for covered in (
            "source_backed_document_object_executor",
            "support_face_property_link_sub",
            "mode_edges_faces_all_values",
            "projection_property_link_sub_list_ordered_edge_wire_or_face_items",
            "multiple_projection_ordered_link_sub_list",
            "multi_projection_result_append_order",
            "multi_projection_metadata_order",
            "mode_edges_project_wire",
            "mode_faces_project_face_rebuild",
            "face_rebuild_hole_wires",
            "mode_all_project_face_rebuild",
            "mode_all_height_prism",
            "mode_faces_height_keeps_face",
            "height_below_precision_keeps_face",
            "offset_zero",
            "offset_after_filter_compound_child_move",
            "offset_direction_normalized",
            "brepproj_projection_nearest_wire",
            "projected_face_parametric_wire_rebuild",
            "ordinary_indexed_named_shape",
            "expected_backed_fixture",
            "deferred_branch_diagnostics",
        ):
            self.assertIn(covered, project_on_surface["covered"])
        self.assertEqual(
            project_on_surface["fixtures"],
            [
                "c4m1/part-project-on-surface-edge-plane",
                "c4m1/part-project-on-surface-face-plane",
                "c4m1/part-project-on-surface-face-hole-plane",
                "c4m1/part-project-on-surface-face-edges-mode",
                "c4m1/part-project-on-surface-face-all-plane",
                "c4m1/part-project-on-surface-height-boundaries",
                "c4m1/part-project-on-surface-edge-offset",
                "c4m1/part-project-on-surface-face-offset",
                "c4m1/part-project-on-surface-height-offset-boundary",
                "c4m1/part-project-on-surface-multi-edge-order",
                "c4m1/part-project-on-surface-mixed-face-edge-order",
                "c4m1/part-project-on-surface-deferred-boundaries",
            ],
        )
        for diagnostic in (
            "missing_property",
            "missing_link_target",
            "invalid_subshape",
            "unsupported_subshape_kind",
            "unsupported_property",
            "invalid_direction",
            "execution_failed",
        ):
            self.assertIn(diagnostic, project_on_surface["diagnostics"])
        for boundary in (
            "source_shape_recomputed_from_document_graph",
            "single_support_face",
            "ordinary_link_sub_list_projection_order",
            "ordinary_indexed_named_shape_without_freecad_mapper_history",
        ):
            self.assertIn(boundary, project_on_surface["request_local_boundaries"])
        self.assertEqual(
            project_on_surface["remaining_gaps"],
            [
                "projected_edge_provenance_mapper_history",
                "gui_projection_task_panel",
                "unverified_advanced_branches",
            ],
        )
        self.assertIn("gui_projection_task_panel", project_on_surface["non_goals"])
        self.assertIn("projected_edge_provenance_mapper_history", project_on_surface["non_goals"])
        self.assertIn("unverified_advanced_branches", project_on_surface["non_goals"])
        self.assertNotIn("full_part_surface_family", project_on_surface["covered"])
        self.assertNotIn("full_part_surface_family", project_on_surface["remaining_gaps"])
        ruled_surface = capabilities["part_workbench"]["ruled_surface"]
        self.assertEqual(ruled_surface["status"], "supported_wire_wire_expected_backed")
        self.assertIn("Part::RuledSurface", ruled_surface["type_ids"])
        self.assertEqual(
            ruled_surface["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.Curve1.value",
                "Objects[].Properties.Curve1.SubList",
                "Objects[].Properties.Curve2.value",
                "Objects[].Properties.Curve2.SubList",
                "Objects[].Properties.Orientation.value",
                "recompute.objs",
            ],
        )
        self.assertIn("Curve1", ruled_surface["properties"])
        self.assertIn("Curve2", ruled_surface["properties"])
        self.assertIn("Orientation", ruled_surface["properties"])
        self.assertIn("App::PropertyLinkSub", ruled_surface["property_types"])
        self.assertIn("Automatic", ruled_surface["orientation_values"])
        self.assertIn("Forward", ruled_surface["orientation_values"])
        self.assertIn("Reversed", ruled_surface["orientation_values"])
        self.assertIn("source_backed_document_object_executor", ruled_surface["covered"])
        self.assertIn("curve1_curve2_property_link_sub", ruled_surface["covered"])
        self.assertIn("orientation_automatic_forward_reversed", ruled_surface["covered"])
        self.assertIn("edge_edge_brepfill_face", ruled_surface["covered"])
        self.assertIn("wire_wire_brepfill_shell", ruled_surface["covered"])
        self.assertIn("source_edge_provenance", ruled_surface["covered"])
        self.assertIn("wire_edge_provenance", ruled_surface["covered"])
        self.assertIn("conic_edge_consumer", ruled_surface["covered"])
        self.assertIn("expected_backed_fixtures", ruled_surface["covered"])
        for fixture in (
            "p8/part-ruled-surface-line-line",
            "p8/part-ruled-surface-conic-line",
            "p8/part-ruled-surface-orientation-reversed",
            "p8/part-ruled-surface-invalid-input",
            "c4m1/part-ruled-surface-wire-wire",
        ):
            self.assertIn(fixture, ruled_surface["fixtures"])
        self.assertEqual(
            ruled_surface["fixtures"],
            [
                "p8/part-ruled-surface-line-line",
                "p8/part-ruled-surface-conic-line",
                "p8/part-ruled-surface-orientation-reversed",
                "p8/part-ruled-surface-invalid-input",
                "c4m1/part-ruled-surface-wire-wire",
            ],
        )
        for diagnostic in (
            "missing_property",
            "missing_link_target",
            "invalid_subshape",
            "unsupported_subshape_kind",
            "no_edge",
        ):
            self.assertIn(diagnostic, ruled_surface["diagnostics"])
        self.assertEqual(
            ruled_surface["diagnostics"],
            [
                "missing_property",
                "missing_link_target",
                "invalid_subshape",
                "unsupported_subshape_kind",
                "no_edge",
            ],
        )
        self.assertIn("edge_edge_brepfill_face", ruled_surface["request_local_boundaries"])
        self.assertIn("wire_wire_brepfill_shell", ruled_surface["request_local_boundaries"])
        self.assertIn(
            "conic_line_expected_uses_make_ruled_surface_after_link_resolve",
            ruled_surface["request_local_boundaries"],
        )
        self.assertEqual(ruled_surface["remaining_gaps"], [])
        self.assertEqual(ruled_surface["non_goals"], [])
        self.assertNotIn("full_surface_family", ruled_surface["covered"])
        loft = capabilities["part_workbench"]["loft"]
        self.assertEqual(loft["status"], "supported_profile_linearize_expected_backed")
        self.assertIn("Part::Loft", loft["type_ids"])
        self.assertEqual(
            loft["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.Sections.values",
                "Objects[].Properties.Solid",
                "Objects[].Properties.Ruled",
                "Objects[].Properties.Closed",
                "Objects[].Properties.Linearize",
                "Objects[].Properties.MaxDegree",
                "recompute.objs",
            ],
        )
        self.assertIn("Sections", loft["properties"])
        self.assertIn("Solid", loft["properties"])
        self.assertIn("Ruled", loft["properties"])
        self.assertIn("Closed", loft["properties"])
        self.assertIn("Linearize", loft["properties"])
        self.assertIn("MaxDegree", loft["properties"])
        self.assertIn("App::PropertyLinkList", loft["property_types"])
        self.assertIn("source_backed_document_object_executor", loft["covered"])
        self.assertIn("sections_property_link_list", loft["covered"])
        self.assertIn("solid_ruled_closed_max_degree", loft["covered"])
        self.assertIn("loft_thru_sections_maker_history", loft["covered"])
        self.assertIn("prepare_profiles_face_vertex_expected_batch", loft["covered"])
        self.assertIn("linearize_planar_faces_post_processing", loft["covered"])
        for fixture in (
            "c3m4/part-loft-two-section-surface",
            "c3m4/part-loft-solid",
            "c3m4/part-loft-ruled",
            "c3m4/part-loft-closed",
            "c3m4/part-loft-invalid-sections",
            "c4m1/part-loft-linearize-profile-face",
            "c4m1/part-loft-linearize-profile-vertex",
        ):
            self.assertIn(fixture, loft["fixtures"])
        self.assertEqual(
            loft["fixtures"],
            [
                "c3m4/part-loft-two-section-surface",
                "c3m4/part-loft-solid",
                "c3m4/part-loft-ruled",
                "c3m4/part-loft-closed",
                "c3m4/part-loft-invalid-sections",
                "c4m1/part-loft-linearize-profile-face",
                "c4m1/part-loft-linearize-profile-vertex",
            ],
        )
        for diagnostic in (
            "missing_property",
            "missing_link_target",
            "invalid_property",
            "unsupported_property",
            "execution_failed",
        ):
            self.assertIn(diagnostic, loft["diagnostics"])
        self.assertEqual(
            loft["diagnostics"],
            [
                "missing_property",
                "missing_link_target",
                "invalid_property",
                "unsupported_property",
                "execution_failed",
            ],
        )
        self.assertIn("source_shape_recomputed_from_document_graph", loft["request_local_boundaries"])
        self.assertIn("source_backed_part_loft_document_object_only", loft["request_local_boundaries"])
        self.assertIn("linearize_faces_no_edges_post_processing", loft["request_local_boundaries"])
        self.assertIn("face_vertex_profile_expected_backed", loft["request_local_boundaries"])
        self.assertIn("complex_profile_family_deferred", loft["request_local_boundaries"])
        self.assertIn("complex_profile_family", loft["remaining_gaps"])
        self.assertEqual(
            loft["remaining_gaps"],
            [
                "complex_profile_family",
            ],
        )
        self.assertEqual(
            loft["non_goals"],
            [
                "complex_profile_family",
            ],
        )
        self.assertNotIn("sweep_filling_geomplate_pipeshell", loft["remaining_gaps"])
        self.assertNotIn("geomplate", loft["remaining_gaps"])
        self.assertNotIn("full_part_surface_family", loft["covered"])
        self.assertNotIn("full_part_surface_family", loft["remaining_gaps"])
        sweep = capabilities["part_workbench"]["sweep"]
        self.assertEqual(sweep["status"], "supported_multi_profile_linearize_expected_backed")
        self.assertIn("Part::Sweep", sweep["type_ids"])
        self.assertEqual(
            sweep["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.Sections.values",
                "Objects[].Properties.Spine.value",
                "Objects[].Properties.Spine.SubList",
                "Objects[].Properties.Solid",
                "Objects[].Properties.Frenet",
                "Objects[].Properties.Transition",
                "Objects[].Properties.Linearize",
                "recompute.objs",
            ],
        )
        for prop in (
            "Sections",
            "Spine",
            "Solid",
            "Frenet",
            "Transition",
            "Linearize",
            "AuxiliarySpine",
            "Tolerance",
        ):
            self.assertIn(prop, sweep["properties"])
        for prop in ("SupportMode", "BiNormal", "LocationMode"):
            self.assertIn(prop, sweep["properties"])
        for property_type in (
            "App::PropertyLinkList",
            "App::PropertyLinkSub",
            "App::PropertyBool",
            "App::PropertyEnumeration",
        ):
            self.assertIn(property_type, sweep["property_types"])
        for covered in (
            "source_backed_document_object_executor",
            "spine_property_link_sub_sublist_compound",
            "sections_property_link_list",
            "multi_profile_sections_expected_backed",
            "solid_frenet_transition_modes",
            "linearize_faces_no_edges_post_processing",
            "pipeshell_maker_history",
            "expected_backed_fixtures",
            "invalid_input_diagnostics",
            "advanced_pipeshell_wrapper_deferred_diagnostics",
        ):
            self.assertIn(covered, sweep["covered"])
        for fixture in (
            "c3m4/part-sweep-right-corner-surface",
            "c3m4/part-sweep-solid",
            "c3m4/part-sweep-frenet-off",
            "c3m4/part-sweep-transition-transformed",
            "c3m4/part-sweep-transition-round-corner",
            "c3m4/part-sweep-spine-subedges",
            "c3m4/part-sweep-open-profile-surface",
            "c3m4/part-sweep-invalid-inputs",
            "c4m1/part-sweep-multi-profile-linearize",
            "c4m1/part-sweep-advanced-deferred",
        ):
            self.assertIn(fixture, sweep["fixtures"])
        self.assertEqual(
            sweep["fixtures"],
            [
                "c3m4/part-sweep-right-corner-surface",
                "c3m4/part-sweep-solid",
                "c3m4/part-sweep-frenet-off",
                "c3m4/part-sweep-transition-transformed",
                "c3m4/part-sweep-transition-round-corner",
                "c3m4/part-sweep-spine-subedges",
                "c3m4/part-sweep-open-profile-surface",
                "c3m4/part-sweep-invalid-inputs",
                "c4m1/part-sweep-multi-profile-linearize",
                "c4m1/part-sweep-advanced-deferred",
            ],
        )
        for diagnostic in (
            "missing_property",
            "missing_link_target",
            "invalid_subshape",
            "unsupported_property",
            "execution_failed",
        ):
            self.assertIn(diagnostic, sweep["diagnostics"])
        self.assertEqual(
            sweep["diagnostics"],
            [
                "missing_property",
                "missing_link_target",
                "invalid_subshape",
                "unsupported_property",
                "execution_failed",
            ],
        )
        self.assertIn("source_shape_recomputed_from_document_graph", sweep["request_local_boundaries"])
        self.assertIn("source_backed_part_sweep_document_object_only", sweep["request_local_boundaries"])
        self.assertIn("multi_profile_sections_expected_backed", sweep["request_local_boundaries"])
        self.assertIn("linearize_faces_no_edges_post_processing", sweep["request_local_boundaries"])
        self.assertIn("advanced_pipeshell_wrapper_deferred_diagnostic", sweep["request_local_boundaries"])
        self.assertIn("hole_model_thread_internal_pipeshell_not_part_sweep", sweep["request_local_boundaries"])
        for non_goal in (
            "advanced_pipeshell_wrapper",
            "auxiliary_spine",
            "located_profile",
            "support_mode",
            "trihedron_binormal_modes",
            "hole_model_thread_internal_pipeshell",
        ):
            self.assertIn(non_goal, sweep["non_goals"])
        self.assertEqual(
            sweep["non_goals"],
            [
                "advanced_pipeshell_wrapper",
                "auxiliary_spine",
                "located_profile",
                "support_mode",
                "trihedron_binormal_modes",
                "hole_model_thread_internal_pipeshell",
            ],
        )
        self.assertEqual(
            sweep["remaining_gaps"],
            [
                "part_sweep_auxiliary_spine_contract",
                "part_sweep_support_mode_contract",
                "part_sweep_binormal_contract",
                "part_sweep_location_mode_contract",
                "part_sweep_tolerance_contract",
            ],
        )
        self.assertNotIn("linearize_post_processing", sweep["remaining_gaps"])
        self.assertNotIn("multi_profile_sections_expected", sweep["remaining_gaps"])
        self.assertNotIn("advanced_pipeshell_wrapper", sweep["remaining_gaps"])
        self.assertNotIn("hole_model_thread_internal_pipeshell", sweep["remaining_gaps"])
        self.assertNotIn("full_part_surface_family", sweep["covered"])
        self.assertNotIn("full_part_surface_family", sweep["remaining_gaps"])
        filling = capabilities["part_workbench"]["filling"]
        self.assertEqual(
            filling["status"],
            "supported_expected_backed_with_c5m8_s4_compound_and_wrapper_diagnostics",
        )
        self.assertIn("Part::FilledFace", filling["type_ids"])
        self.assertEqual(filling["helper"], "Part.makeFilledFace")
        self.assertEqual(
            filling["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.Boundary.SubSet",
                "Objects[].Properties.Surface.SubList",
                "Objects[].Properties.Supports.SubSet",
                "Objects[].Properties.Supports.SubSet[].Support",
                "Objects[].Properties.Orders.SubSet",
                "Objects[].Properties.Orders.SubSet[].Order",
                "Objects[].Properties.BRepOffsetAPIMakeFillingWrapper",
                "Objects[].Properties.BRepOffsetAPIMakeFillingUvPointOnSupport",
                "Objects[].Properties.Degree",
                "Objects[].Properties.PtsOnCurve",
                "Objects[].Properties.NumIter",
                "Objects[].Properties.Anisotropy",
                "Objects[].Properties.Tol2d",
                "Objects[].Properties.Tol3d",
                "Objects[].Properties.TolG1",
                "Objects[].Properties.TolG2",
                "Objects[].Properties.MaxDegree",
                "Objects[].Properties.MaxSegments",
                "recompute.objs",
            ],
        )
        self.assertFalse(filling["freecad_native_document_object"])
        self.assertEqual(filling["operation_model"], "source_backed_helper")
        for prop in ("Boundary", "Surface", "Supports", "Orders"):
            self.assertIn(prop, filling["properties"])
        self.assertIn("App::PropertyLinkSubList", filling["property_types"])
        for covered in (
            "part_filled_face_source_backed_helper",
            "boundary_property_link_sub_list",
            "closed_wire_default",
            "connected_boundary_edges_default",
            "brepoffsetapi_makefilling",
            "maker_history:filling",
            "default_params_metadata",
            "boundary_source_evidence",
            "initial_surface_load_init_surface",
            "support_face_source_map",
            "order_source_map",
            "support_order_source_evidence",
            "source_backed_surface_support_order_known_gap",
            "constructor_params_metadata",
            "non_default_params_source_backed_known_gap",
            "non_boundary_edge_isbound_false",
            "non_boundary_wire_constraints",
            "non_boundary_face_constraint",
            "non_boundary_vertex_point_constraint",
            "non_boundary_constraint_source_evidence",
            "compound_source_expansion",
            "compound_optional_expected_backed",
            "direct_makefilling_wrapper_lifecycle_diagnostic",
            "wrapper_uv_point_on_support_diagnostic",
            "expected_backed_fixtures",
            "invalid_diagnostics",
        ):
            self.assertIn(covered, filling["covered"])
        for fixture in (
            "c3m4/part-filling-closed-wire-default",
            "c3m4/part-filling-boundary-edges-default",
            "c3m4/part-filling-invalid-inputs",
            "c4m1/part-filling-advanced-deferred",
            "c5m8/part-filling-initial-surface-boundary",
            "c5m8/part-filling-support-order-edge-face",
            "c5m8/part-filling-invalid-support-order",
            "c5m8/part-filling-non-default-params",
            "c5m8/part-filling-param-diagnostics",
            "c5m8/part-filling-non-boundary-edge-support",
            "c5m8/part-filling-non-boundary-face-point",
            "c5m8/part-filling-non-boundary-wire",
            "c5m8/part-filling-non-boundary-diagnostics",
            "c5m8/part-filling-compound-optional-boundary",
            "c5m8/part-filling-wrapper-boundary",
            "c5m8/part-filling-wrapper-uv-point-boundary",
        ):
            self.assertIn(fixture, filling["fixtures"])
        self.assertEqual(
            filling["fixtures"],
            [
                "c3m4/part-filling-closed-wire-default",
                "c3m4/part-filling-boundary-edges-default",
                "c3m4/part-filling-invalid-inputs",
                "c4m1/part-filling-advanced-deferred",
                "c5m8/part-filling-initial-surface-boundary",
                "c5m8/part-filling-support-order-edge-face",
                "c5m8/part-filling-invalid-support-order",
                "c5m8/part-filling-non-default-params",
                "c5m8/part-filling-param-diagnostics",
                "c5m8/part-filling-non-boundary-edge-support",
                "c5m8/part-filling-non-boundary-face-point",
                "c5m8/part-filling-non-boundary-wire",
                "c5m8/part-filling-non-boundary-diagnostics",
                "c5m8/part-filling-compound-optional-boundary",
                "c5m8/part-filling-wrapper-boundary",
                "c5m8/part-filling-wrapper-uv-point-boundary",
            ],
        )
        for diagnostic in (
            "missing_property",
            "missing_link_target",
            "invalid_subshape",
            "invalid_surface_source",
            "invalid_support_target",
            "invalid_support_source",
            "invalid_order_target",
            "invalid_order_source",
            "invalid_parameter",
            "invalid_non_boundary_source",
            "unsupported_wrapper_lifecycle",
            "unsupported_property",
            "execution_failed",
        ):
            self.assertIn(diagnostic, filling["diagnostics"])
        self.assertEqual(
            filling["diagnostics"],
            [
                "missing_property",
                "missing_link_target",
                "invalid_subshape",
                "invalid_surface_source",
                "invalid_support_target",
                "invalid_support_source",
                "invalid_order_target",
                "invalid_order_source",
                "invalid_parameter",
                "invalid_non_boundary_source",
                "unsupported_wrapper_lifecycle",
                "unsupported_property",
                "execution_failed",
            ],
        )
        for boundary in (
            "source_backed_helper_not_freecad_document_object",
            "boundary_property_link_sub_list",
            "default_params_baseline",
            "constructor_params_source_backed",
            "initial_surface_face_source_backed",
            "support_face_map_source_backed",
            "order_map_source_backed_c0_g1_g2_parser",
            "surface_support_order_native_helper_oracle_known_gap",
            "support_order_g1_fixture_source_backed",
            "non_default_params_native_helper_oracle_known_gap",
            "non_boundary_edge_wire_face_vertex_source_backed",
            "non_boundary_edge_support_order_native_helper_oracle_known_gap",
            "compound_boundary_optional_expected_backed",
            "direct_makefilling_wrapper_diagnostic",
            "wrapper_uv_point_on_support_not_helper_dto",
            "native_surface_workbench_filling_feature_not_claimed",
        ):
            self.assertIn(boundary, filling["request_local_boundaries"])
        for gap in (
            "surface_support_order_native_helper_expected",
            "filling_support_order_g2_expected",
            "non_default_params_native_helper_expected",
            "non_boundary_edge_support_native_helper_expected",
        ):
            self.assertIn(gap, filling["remaining_gaps"])
        self.assertEqual(
            filling["remaining_gaps"],
            [
                "surface_support_order_native_helper_expected",
                "filling_support_order_g2_expected",
                "non_default_params_native_helper_expected",
                "non_boundary_edge_support_native_helper_expected",
            ],
        )
        for unsupported in ("non_boundary_constraints_deferred_diagnostic",):
            self.assertNotIn(unsupported, filling["request_local_boundaries"])
        self.assertIn("native_freecad_part_filledface_document_object", filling["non_goals"])
        self.assertIn("advanced_brepoffsetapi_makefilling_wrapper", filling["non_goals"])
        self.assertIn("surface_workbench_filling_feature", filling["non_goals"])
        self.assertEqual(
            filling["non_goals"],
            [
                "native_freecad_part_filledface_document_object",
                "advanced_brepoffsetapi_makefilling_wrapper",
                "surface_workbench_filling_feature",
            ],
        )
        self.assertNotIn("geomplate", filling["non_goals"])
        geomplate = capabilities["part_workbench"]["geomplate"]
        self.assertEqual(
            geomplate["status"],
            "supported_expected_backed_point_criteria_with_curve_wrapper_diagnostics",
        )
        self.assertIn("Part::GeomPlateSurface", geomplate["type_ids"])
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertEqual(
            geomplate["payload_keys"],
            [
                "Objects[].TypeId",
                "Objects[].Properties.CurveConstraints.SubSet",
                "Objects[].Properties.CurveConstraints.SubSet[].Surface",
                "Objects[].Properties.CurveConstraints.SubSet[].G0Criterion",
                "Objects[].Properties.CurveConstraints.SubSet[].G1Criterion",
                "Objects[].Properties.CurveConstraints.SubSet[].G2Criterion",
                "Objects[].Properties.PointConstraints",
                "Objects[].Properties.PointConstraints[].G0Criterion",
                "Objects[].Properties.PointConstraints[].G1Criterion",
                "Objects[].Properties.PointConstraints[].G2Criterion",
                "Objects[].Properties.InitialSurface",
                "Objects[].Properties.Surface",
                "Objects[].Properties.Curve2dOnSurface",
                "Objects[].Properties.Curve2dOnSurface[].Boundary",
                "Objects[].Properties.Curve2dOnSurface[].Surface",
                "Objects[].Properties.Curve2dOnSurface[].Curve2d",
                "Objects[].Properties.ProjectedCurve2d",
                "Objects[].Properties.ProjectedCurve2d[].Boundary",
                "Objects[].Properties.ProjectedCurve2d[].Surface",
                "Objects[].Properties.ProjectedCurve2d[].Curve2d",
                "Objects[].Properties.ProjectedCurve2d[].TolU",
                "Objects[].Properties.ProjectedCurve2d[].TolV",
                "Objects[].Properties.Point2dOnSurface",
                "Objects[].Properties.Point2dOnSurface[].Point",
                "Objects[].Properties.Point2dOnSurface[].Point2d",
                "Objects[].Properties.Point2dOnSurface[].Surface",
                "Objects[].Properties.Degree",
                "Objects[].Properties.NbPtsOnCur",
                "Objects[].Properties.NbIter",
                "Objects[].Properties.Tol2d",
                "Objects[].Properties.Tol3d",
                "Objects[].Properties.ApproxTol3d",
                "Objects[].Properties.ApproxMaxSegments",
                "Objects[].Properties.ApproxMaxDegree",
                "Objects[].Properties.ApproxContinuity",
                "recompute.objs",
            ],
        )
        self.assertFalse(geomplate["freecad_native_document_object"])
        self.assertEqual(geomplate["operation_model"], "source_backed_geometry_helper")
        for prop in (
            "CurveConstraints",
            "PointConstraints",
            "Degree",
            "NbPtsOnCur",
            "ApproxMaxSegments",
            "ApproxContinuity",
            "InitialSurface",
            "Surface",
            "Curve2dOnSurface",
            "ProjectedCurve2d",
            "Point2dOnSurface",
            "PlateSurfaceCurves",
        ):
            self.assertIn(prop, geomplate["properties"])
        for property_type in (
            "App::PropertyLinkSubList",
            "App::PropertyLinkSub",
            "JSON::PointConstraintList",
            "JSON::Curve2dConstraintList",
            "JSON::Point2dConstraintList",
            "JSON::PointCriteria",
            "JSON::NumericParams",
        ):
            self.assertIn(property_type, geomplate["property_types"])
        for covered in (
            "part_geomplate_surface_source_backed_helper",
            "buildplate_surface_helper",
            "default_3d_curve_point_expected_backed",
            "source_backed_3d_curve_g0_constraints",
            "point_3d_constraints",
            "initial_surface_reference_expected_backed",
            "g1_curve_on_surface_source_backed",
            "curve2d_on_surface_expected_backed",
            "point2d_on_surface_expected_backed",
            "mixed_g0_2d_surface_constraints_expected_backed",
            "point_constraint_criteria_expected_backed",
            "projected_curve2d_source_backed",
            "projected_curve2d_native_oracle_blocker",
            "default_build_params_metadata",
            "approximation_metadata",
            "explicit_approximation_params_expected_backed",
            "advanced_approximation_params_expected_backed",
            "geomplate_makeapprox_face",
            "source_evidence",
            "expected_backed_fixtures",
            "invalid_diagnostics",
            "g1_curve_on_surface_native_oracle_blocker",
        ):
            self.assertIn(covered, geomplate["covered"])
        for fixture in (
            "c3m4/part-geomplate-curve-point-default",
            "c3m4/part-geomplate-invalid-inputs",
            "c4m1/part-geomplate-advanced-constraints",
            "c4m1/part-geomplate-advanced-deferred",
            "c5m7/part-geomplate-initial-surface-g0",
            "c5m7/part-geomplate-g1-curve-on-surface",
            "c5m7/part-geomplate-curve2d-on-surface",
            "c5m7/part-geomplate-projected-curve2d",
            "c5m7/part-geomplate-point2d-on-surface",
            "c5m7/part-geomplate-mixed-surface-constraints",
            "c5m7/part-geomplate-point-custom-criteria",
            "c5m7/part-geomplate-curve-criteria-diagnostic",
            "c5m7/part-geomplate-wrapper-boundary",
        ):
            self.assertIn(fixture, geomplate["fixtures"])
        self.assertEqual(
            geomplate["fixtures"],
            [
                "c3m4/part-geomplate-curve-point-default",
                "c3m4/part-geomplate-invalid-inputs",
                "c4m1/part-geomplate-advanced-constraints",
                "c4m1/part-geomplate-advanced-deferred",
                "c5m7/part-geomplate-initial-surface-g0",
                "c5m7/part-geomplate-g1-curve-on-surface",
                "c5m7/part-geomplate-curve2d-on-surface",
                "c5m7/part-geomplate-projected-curve2d",
                "c5m7/part-geomplate-point2d-on-surface",
                "c5m7/part-geomplate-mixed-surface-constraints",
                "c5m7/part-geomplate-point-custom-criteria",
                "c5m7/part-geomplate-curve-criteria-diagnostic",
                "c5m7/part-geomplate-wrapper-boundary",
            ],
        )
        for diagnostic in (
            "missing_constraints",
            "missing_curve_source",
            "missing_surface_source",
            "invalid_curve_source",
            "invalid_curve2d_source",
            "invalid_surface_source",
            "invalid_point_constraint",
            "invalid_point2d_source",
            "invalid_parameter",
            "perform_failed",
            "surface_not_done",
            "approximation_failed",
            "unsupported_property",
            "unsupported_curve_criteria",
            "unsupported_wrapper_lifecycle",
        ):
            self.assertIn(diagnostic, geomplate["diagnostics"])
        self.assertEqual(
            geomplate["diagnostics"],
            [
                "missing_constraints",
                "missing_curve_source",
                "missing_surface_source",
                "invalid_curve_source",
                "invalid_curve2d_source",
                "invalid_surface_source",
                "invalid_point_constraint",
                "invalid_point2d_source",
                "invalid_parameter",
                "perform_failed",
                "surface_not_done",
                "approximation_failed",
                "unsupported_property",
                "unsupported_curve_criteria",
                "unsupported_wrapper_lifecycle",
            ],
        )
        for boundary in (
            "source_backed_helper_not_freecad_document_object",
            "request_local_geomplate_surface_not_persistent_geometry",
            "curve_constraints_are_3d_edge_sources",
            "point_constraints_are_3d_vectors",
            "g0_curve_constraints_first_batch",
            "initial_surface_load_init_surface",
            "g1_curve_on_surface_source_backed_not_native_expected_backed",
            "curve2d_on_surface_source_backed_expected_backed",
            "point2d_on_surface_source_backed_expected_backed",
            "projected_curve2d_source_backed_not_native_expected_backed",
            "point_constraint_criteria_expected_backed",
            "curve_constraint_criteria_setters_not_implemented_in_freecad",
            "platesurface_curves_requires_wrapper_lifecycle",
            "default_and_explicit_build_params",
            "explicit_approximation_params",
            "filling_capability_not_expanded",
            "advanced_approximation_params_expected_backed",
            "advanced_approximation_params_are_not_full_advanced_support",
            "2d_constraints_require_explicit_boundary_surface_and_uv_payload",
            "g1_native_oracle_blocked_by_python_wrapper",
            "projected_curve2d_native_oracle_blocked_by_python_wrapper",
        ):
            self.assertIn(boundary, geomplate["request_local_boundaries"])
        for gap in (
            "g1_curve_on_surface_native_oracle",
            "projected_2d_curve_native_oracle",
            "curve_constraint_criteria_setters_not_implemented",
            "platesurface_curves_wrapper_lifecycle",
        ):
            self.assertIn(gap, geomplate["remaining_gaps"])
        self.assertEqual(
            geomplate["remaining_gaps"],
            [
                "g1_curve_on_surface_native_oracle",
                "projected_2d_curve_native_oracle",
                "curve_constraint_criteria_setters_not_implemented",
                "platesurface_curves_wrapper_lifecycle",
            ],
        )
        for non_goal in (
            "gui_geomplate_feature",
            "native_freecad_part_geomplate_document_object",
            "fake_part_geomplate_document_object",
            "fake_persistent_platesurface_object",
            "filling_brepoffsetapi_makefilling_extension",
        ):
            self.assertIn(non_goal, geomplate["non_goals"])
        self.assertEqual(
            geomplate["non_goals"],
            [
                "gui_geomplate_feature",
                "native_freecad_part_geomplate_document_object",
                "fake_part_geomplate_document_object",
                "fake_persistent_platesurface_object",
                "filling_brepoffsetapi_makefilling_extension",
            ],
        )
        self.assertNotIn("Part::GeomPlate", geomplate["type_ids"])
        self.assertNotIn("part_platesurface_curves_wrapper", geomplate["covered"])
        for unsupported in (
            "curve_constraint_criteria",
            "part_platesurface_curves_wrapper",
            "full_advanced_geomplate_support",
            "full_part_surface_family",
        ):
            self.assertNotIn(unsupported, geomplate["covered"])
        self.assertNotIn("complete_mapper_history", capabilities["topo_history"]["remaining_gaps"])
        self.assertNotIn(
            "element_map_child_map_preserve_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_recursive_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_postfix_hash_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_hash_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_child_map_propagate_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "element_map_policy_propagate_shell_lifecycle",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn(
            "hole_threaded_model_thread_profile_head_oracle_matrix",
            capabilities["topo_history"]["remaining_gaps"],
        )
        self.assertNotIn("hole_threaded_model_thread_profile_head_oracle_matrix", capabilities["known_gaps"])
        self.assertNotIn("complete_mapper_history", capabilities["known_gaps"])
        self.assertNotIn("assembly_joint_solver", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_preserve_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_recursive_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn(
            "element_map_child_map_postfix_hash_propagate_lifecycle",
            capabilities["known_gaps"],
        )
        self.assertNotIn("element_map_child_map_hash_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_child_map_propagate_lifecycle", capabilities["known_gaps"])
        self.assertNotIn("element_map_policy_propagate_shell_lifecycle", capabilities["known_gaps"])
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_open_wire_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_fill_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn(
            "part_offset2d_makeoffsetfix_intersection_compound_collective",
            capabilities["known_gaps"],
        )
        self.assertNotIn("hole_threaded_model_thread_profile_head_oracle_matrix", capabilities["known_gaps"])
        self.assertNotIn("assembly_full_ondsel_solver", capabilities["known_gaps"])
        self.assertNotIn("assembly_solver_placement_updates", capabilities["known_gaps"])
        self.assertNotIn("show_element_missing_child_lifecycle", capabilities["known_gaps"])
        self.assertEqual(capabilities["known_gaps"], [])
        serialized_capabilities = json.dumps(capabilities, sort_keys=True)
        self.assertNotIn("full_part_surface_family", serialized_capabilities)
        self.assertNotIn("full_surface_family", serialized_capabilities)

    def test_c_api_recompute_reports_show_element_lifecycle_updates(self) -> None:
        ffi_result = self.run_recompute_ffi("app-link-show-element-synthetic", "p8")
        updates = ffi_result["documentObjectUpdates"]

        self.assertEqual(ffi_result["diagnostics"], [])
        self.assertEqual([item["action"] for item in updates], ["create", "create"])
        self.assertEqual(updates[0]["object"], "ArrayLink_i0")
        self.assertEqual(updates[0]["properties"]["LinkedObject"]["value"], "Box")

    def test_c3m7_worker_and_wasm_adapters_apply_streaming_mesh_limits(self) -> None:
        payload = (ROOT / "fixtures" / "c3m7" / "rect-pad-worker-mesh-limit.json").read_bytes()

        for adapter, runner in [
            ("worker", self.run_worker_recompute_ffi_payload),
            ("wasm", self.run_wasm_recompute_ffi_payload),
        ]:
            with self.subTest(adapter=adapter):
                result = runner(payload)
                diagnostic = result["diagnostics"][0]
                mesh = result["results"][0]["mesh"]

                self.assert_core_result_contract(result)
                self.assertEqual(result["adapter"], adapter)
                self.assertEqual(result["binaryPayloads"], [])
                self.assertEqual(diagnostic["code"], "mesh_limit_exceeded")
                self.assertEqual(diagnostic["stage"], "adapter")
                self.assertEqual(diagnostic["target"], "streaming_mesh_limits")
                self.assertTrue(mesh["limited"])
                self.assertEqual(mesh["streaming"]["protocol"], "cad-core-json-mesh-stream-v1")
                self.assertEqual(mesh["streaming"]["max_vertices"], 4)
                self.assertEqual(mesh["streaming"]["max_triangles"], 2)
                self.assertEqual(mesh["streaming"]["chunk_triangles"], 1)

    def test_c3m7_c_api_exports_binary_mesh_payload(self) -> None:
        document = json.loads((ROOT / "fixtures" / "mvp" / "rect-pad.json").read_text(encoding="utf-8"))

        status, metadata, data, error = self.call_mesh_binary_ffi({"document": document, "object": "Pad"})

        self.assertEqual(status, 0, error)
        self.assertIsNotNone(metadata)
        assert metadata is not None
        self.assertEqual(metadata["protocol"], "cad-core-binary-mesh-v1")
        self.assertEqual(metadata["content_type"], "application/vnd.cad-core.mesh+bin")
        self.assertEqual(metadata["layout"]["vertex_format"], "f64x3_le")
        self.assertEqual(metadata["layout"]["index_format"], "u32x3_le")
        self.assertEqual(metadata["diagnostics"], [])
        self.assertEqual(metadata["bytes"], len(data))
        self.assertGreater(metadata["vertex_count"], 0)
        self.assertGreater(metadata["triangle_count"], 0)
        self.assertEqual(metadata["index_offset"], metadata["vertex_count"] * 3 * 8)
        self.assertFalse(metadata["limited"])

    def test_c4s11_binary_mesh_payload_limit_reports_metadata_diagnostic(self) -> None:
        document = json.loads((ROOT / "fixtures" / "mvp" / "rect-pad.json").read_text(encoding="utf-8"))

        status, metadata, data, error = self.call_mesh_binary_ffi(
            {
                "document": document,
                "object": "Pad",
                "binary_payload_limits": {"max_bytes": 1},
            }
        )

        self.assertEqual(status, 0, error)
        self.assertEqual(data, b"")
        self.assertIsNotNone(metadata)
        assert metadata is not None
        self.assertEqual(metadata["protocol"], "cad-core-binary-mesh-v1")
        self.assertTrue(metadata["limited"])
        self.assertEqual(metadata["bytes"], 0)
        self.assertGreater(metadata["original_bytes"], metadata["byte_limit"])
        diagnostic = metadata["diagnostics"][-1]
        self.assertEqual(diagnostic["code"], "adapter_resource_limit")
        self.assertEqual(diagnostic["stage"], "adapter")
        self.assertEqual(diagnostic["property"], "binaryPayloads")
        self.assertEqual(diagnostic["target"], "binary_payload_limits.max_bytes")
        self.assertEqual(diagnostic["details"]["protocol"], "cad-core-binary-mesh-v1")

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
