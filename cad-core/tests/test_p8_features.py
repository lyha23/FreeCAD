from __future__ import annotations

import json
import math
import tempfile
from pathlib import Path

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import ROOT
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import ROOT
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreP8FeatureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_update_property_type(self, update: dict, property_name: str, property_type: str) -> None:
        self.assertEqual(update["properties"][property_name]["PropertyType"], property_type)

    def assert_identity_placement(self, placement: dict) -> None:
        self.assertEqual(placement["PropertyType"], "App::PropertyPlacement")
        self.assertEqual(placement["Base"], [0, 0, 0])
        self.assertEqual(placement["Rotation"], [0, 0, 0, 1])

    def run_with_document_updates_applied(self, fixture: str, group: str, updates: list[dict]) -> dict:
        payload = json.loads((ROOT / "fixtures" / group / f"{fixture}.json").read_text(encoding="utf-8"))
        by_name = {document_object["Name"]: document_object for document_object in payload["Objects"]}
        next_id = max(document_object["ID"] for document_object in payload["Objects"]) + 1

        for update in updates:
            action = update["action"]
            name = update["object"]
            properties = update.get("properties", {})
            if action == "delete":
                document_object = by_name.pop(name, None)
                if document_object is not None:
                    payload["Objects"].remove(document_object)
                continue

            if action == "create" and name not in by_name:
                object_id = update.get("objectId", next_id)
                next_id = max(next_id, object_id + 1)
                document_object = {
                    "Name": name,
                    "ID": object_id,
                    "TypeId": update["typeId"],
                    "Properties": dict(properties),
                }
                payload["Objects"].append(document_object)
                by_name[name] = document_object
                continue

            document_object = by_name[name]
            document_object["Properties"].update(properties)

        with tempfile.TemporaryDirectory() as tmp:
            applied_path = Path(tmp) / f"{fixture}-applied.json"
            applied_path.write_text(json.dumps(payload), encoding="utf-8")
            return self.run_recompute_file(applied_path)

    def assert_document_updates_apply_to_stable_graph(
        self,
        fixture: str,
        group: str,
        updates: list[dict],
    ) -> dict:
        applied_result = self.run_with_document_updates_applied(fixture, group, updates)
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])
        return applied_result

    def run_distance_type_reference_case(
        self,
        case_name: str,
        reference1: dict,
        reference2: dict,
        distance: float = 1.5,
    ) -> dict:
        objects = [
            {
                "Name": reference1["object"],
                "ID": 1,
                "TypeId": reference1["type_id"],
                "Properties": reference1["properties"],
            },
            {
                "Name": reference2["object"],
                "ID": 2,
                "TypeId": reference2["type_id"],
                "Properties": reference2["properties"],
            },
            {
                "Name": "DistanceJoint",
                "ID": 3,
                "TypeId": "App::FeaturePython",
                "Properties": {
                    "JointType": {
                        "PropertyType": "App::PropertyEnumeration",
                        "value": "Distance",
                    },
                    "Distance": {
                        "PropertyType": "App::PropertyFloat",
                        "value": distance,
                    },
                    "Reference1": {
                        "PropertyType": "App::PropertyXLinkSub",
                        "value": reference1["object"],
                        "SubList": [reference1["subname"]],
                    },
                    "Reference2": {
                        "PropertyType": "App::PropertyXLinkSub",
                        "value": reference2["object"],
                        "SubList": [reference2["subname"]],
                    },
                },
            },
            {
                "Name": "Joints",
                "ID": 4,
                "TypeId": "Assembly::JointGroup",
                "Properties": {
                    "Group": {
                        "PropertyType": "App::PropertyLinkList",
                        "values": ["DistanceJoint"],
                    }
                },
            },
            {
                "Name": "Assembly",
                "ID": 5,
                "TypeId": "Assembly::AssemblyObject",
                "Properties": {
                    "Group": {
                        "PropertyType": "App::PropertyLinkList",
                        "values": [reference1["object"], reference2["object"], "Joints"],
                    }
                },
            },
        ]
        payload = {"Objects": objects, "recompute": {"objs": ["Assembly"]}}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / f"{case_name}.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            return self.run_recompute_file(path)

    def test_p8_part_box_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-box", "p8")
        box = result["objects"]["Box"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(box["status"], "ok")
        self.assertEqual(box["primitive"], "box")
        self.assert_object_matches_expected(result, "p8", "part-box")

    def test_c3m4_part_offset_face_builds_request_local_history(self) -> None:
        result = self.run_recompute("part-offset-face", "c3m4")
        offset = result["objects"]["Offset"]
        named_shape = result["named_shapes"]["Offset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset")
        self.assertEqual(offset["source"], "Plane")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertFalse(offset["fill"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])

    def test_c3m4_part_offset_face_fill_uses_free_bound_sewing_path(self) -> None:
        result = self.run_recompute("part-offset-face-fill", "c3m4")
        offset = result["objects"]["Offset"]
        named_shape = result["named_shapes"]["Offset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset")
        self.assertEqual(offset["source"], "Plane")
        self.assertTrue(offset["fill"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset")
        self.assertIn("part_offset_fill:sewing_history", named_shape["element_history_status"])
        self.assertIn("part_offset_fill:perimeter_faces", named_shape["element_history_status"])
        self.assertNotEqual(named_shape["element_map_status"], "indexed_only")

    def test_c3m4_part_offset_solid_source_recovers_solid_result(self) -> None:
        result = self.run_recompute("part-offset-box-solid-source", "c3m4")
        offset = result["objects"]["Offset"]
        named_shape = result["named_shapes"]["Offset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset")
        self.assertEqual(offset["source"], "Box")
        self.assertEqual(offset["shape"], "occt_solid")
        self.assertFalse(offset["fill"])
        self.assertNotIn(
            "part_offset_solid_source:make_element_solid_failed",
            named_shape["element_history_status"],
        )

    def test_c3m4_part_offset2d_face_no_fill_rebuilds_face_from_offset_wires(self) -> None:
        result = self.run_recompute("part-offset2d-face-no-fill", "c3m4")
        offset = result["objects"]["Offset2D"]
        named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Plane")
        self.assertEqual(offset["shape"], "occt_face")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertFalse(offset["fill"])
        self.assertFalse(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_face_no_fill")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_offset2d:face_no_fill_makeoffset", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["Plane.Edge1"], "Edge1")

    def test_c3m4_part_offset2d_face_fill_uses_closed_source_and_offset_wires(self) -> None:
        result = self.run_recompute("part-offset2d-face-fill", "c3m4")
        offset = result["objects"]["Offset2D"]
        named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Plane")
        self.assertEqual(offset["shape"], "occt_face")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertTrue(offset["fill"])
        self.assertFalse(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_face_fill_closed")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_offset2d:face_fill_closed_makeoffset", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("Offset2D.Offset2DWires.Edge1", named_shape["element_map"])

    def test_c3m4_part_offset2d_open_wire_no_fill_returns_offset_wire(self) -> None:
        result = self.run_recompute("part-offset2d-open-wire-no-fill", "c3m4")
        offset = result["objects"]["Offset2D"]
        named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Sketch")
        self.assertEqual(offset["shape"], "occt_wire")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertFalse(offset["fill"])
        self.assertFalse(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_wire_no_fill")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_offset2d:wire_no_fill_makeoffset", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("Sketch.Edge1", named_shape["element_map"])

    def test_c3m4_part_offset2d_open_wire_fill_connects_source_and_offset_wires(self) -> None:
        result = self.run_recompute("part-offset2d-open-wire-fill", "c3m4")
        offset = result["objects"]["Offset2D"]
        named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Sketch")
        self.assertEqual(offset["shape"], "occt_face")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertTrue(offset["fill"])
        self.assertFalse(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_wire_fill_open")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_offset2d:wire_fill_open_makeoffset", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("Offset2D.Offset2DWires.Edge1", named_shape["element_map"])

    def test_c3m4_part_offset2d_compound_fill_recurses_children(self) -> None:
        result = self.run_recompute("part-offset2d-compound-open-wire-fill", "c3m4")
        compound = result["objects"]["Compound"]
        offset = result["objects"]["Offset2D"]
        compound_named_shape = result["named_shapes"]["Compound"]
        offset_named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(compound["status"], "ok")
        self.assertEqual(compound["feature"], "part_compound")
        self.assertEqual(compound["shape"], "occt_compound")
        self.assertEqual(compound["links"], ["SketchA", "SketchB"])
        self.assertIn("part_compound:make_element_compound", compound_named_shape["element_history_status"])
        self.assertIn(
            "element_map_child_map:preserve_source_ranges",
            compound_named_shape["element_history_status"],
        )
        edge_child_maps = [
            item for item in compound_named_shape["child_element_maps"] if item["kind"] == "edge"
        ]
        vertex_child_maps = [
            item for item in compound_named_shape["child_element_maps"] if item["kind"] == "vertex"
        ]
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["indexed_name"],
                    item["offset"],
                    item["count"],
                    item["target_start"],
                    item["target_end"],
                    item["has_source_element_map"],
                    item["source_child_map_count"],
                )
                for item in edge_child_maps
            ],
            [
                ("SketchA", "Edge1", 0, 2, "Edge1", "Edge2", True, 0),
                ("SketchB", "Edge1", 2, 2, "Edge3", "Edge4", True, 0),
            ],
        )
        self.assertEqual(
            [
                (
                    item["source_owner"],
                    item["indexed_name"],
                    item["offset"],
                    item["count"],
                    item["target_start"],
                    item["target_end"],
                    item["has_source_element_map"],
                    item["source_child_map_count"],
                )
                for item in vertex_child_maps
            ],
            [
                ("SketchA", "Vertex1", 0, 3, "Vertex1", "Vertex3", True, 0),
                ("SketchB", "Vertex1", 3, 3, "Vertex4", "Vertex6", True, 0),
            ],
        )
        self.assertTrue(
            all(item["source_element_map_size"] > 0 for item in edge_child_maps + vertex_child_maps)
        )

        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Compound")
        self.assertEqual(offset["shape"], "occt_compound")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertTrue(offset["fill"])
        self.assertFalse(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_compound_recursive")
        self.assertEqual(offset_named_shape["element_map_status"], "history_partial")
        self.assertIn("part_offset2d:compound_child_recursive", offset_named_shape["element_history_status"])
        self.assertIn("part_offset2d:wire_fill_open_makeoffset", offset_named_shape["element_history_status"])
        self.assertIn("history_consumed:merge", offset_named_shape["element_history_status"])

    def test_c3m4_part_offset2d_compound_intersection_offsets_children_collectively(self) -> None:
        result = self.run_recompute("part-offset2d-compound-intersection-no-fill", "c3m4")
        compound = result["objects"]["Compound"]
        offset = result["objects"]["Offset2D"]
        named_shape = result["named_shapes"]["Offset2D"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(compound["status"], "ok")
        self.assertEqual(compound["feature"], "part_compound")
        self.assertEqual(compound["links"], ["PlaneA", "PlaneB"])
        self.assertEqual(offset["status"], "ok")
        self.assertEqual(offset["feature"], "part_offset2d")
        self.assertEqual(offset["source"], "Compound")
        self.assertEqual(offset["shape"], "occt_compound")
        self.assertEqual(offset["mode"], "Skin")
        self.assertEqual(offset["join"], "Arc")
        self.assertFalse(offset["fill"])
        self.assertTrue(offset["intersection"])
        self.assertEqual(offset["topo_naming_history"], "maker_history:offset2d_compound_collective")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn(
            "part_offset2d:compound_collective_makeoffset",
            named_shape["element_history_status"],
        )
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])

    def test_c3m4_part_thickness_single_solid_face_uses_make_thick_solid_history(self) -> None:
        result = self.run_recompute("part-thickness-box-face", "c3m4")
        thickness = result["objects"]["Thickness"]
        named_shape = result["named_shapes"]["Thickness"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(thickness["status"], "ok")
        self.assertEqual(thickness["feature"], "part_thickness")
        self.assertEqual(thickness["source"], "Box")
        self.assertEqual(thickness["shape"], "occt_solid")
        self.assertEqual(thickness["mode"], "Skin")
        self.assertEqual(thickness["join"], "Arc")
        self.assertEqual(thickness["effective_join"], "Arc")
        self.assertEqual(thickness["selected_faces"], ["Face6"])
        self.assertEqual(thickness["topo_naming_history"], "maker_history:thick_solid")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_thickness:make_thick_solid", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["Box.Face6"], "Face6")

    def test_c3m4_part_thickness_rectoverso_tangent_uses_intersection_join(self) -> None:
        result = self.run_recompute("part-thickness-box-face-rectoverso-tangent", "c3m4")
        thickness = result["objects"]["Thickness"]
        named_shape = result["named_shapes"]["Thickness"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(thickness["status"], "ok")
        self.assertEqual(thickness["feature"], "part_thickness")
        self.assertEqual(thickness["source"], "Box")
        self.assertEqual(thickness["shape"], "occt_solid")
        self.assertEqual(thickness["mode"], "RectoVerso")
        self.assertEqual(thickness["join"], "Tangent")
        self.assertEqual(thickness["effective_join"], "Intersection")
        self.assertTrue(thickness["intersection"])
        self.assertEqual(thickness["selected_faces"], ["Face6"])
        self.assertEqual(thickness["topo_naming_history"], "maker_history:thick_solid")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_thickness:make_thick_solid", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])

    def test_c3m4_part_section_records_source_qualified_history(self) -> None:
        result = self.run_recompute("part-section-stable-history", "c3m4")
        section = result["objects"]["Section"]
        named_shape = result["named_shapes"]["Section"]
        history = named_shape["history"]
        mapper_history = named_shape["mapper_history"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(section["status"], "ok")
        self.assertEqual(section["boolean"], "section")
        self.assertEqual(section["base"], "Box")
        self.assertEqual(section["tool"], "Plane")
        self.assertEqual(section["approximation"], False)
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        self.assertIn("terminal_history:split_deleted", named_shape["element_history_status"])
        self.assertEqual(named_shape["element_map"]["Plane.Edge1"], "Edge1")
        self.assertEqual(named_shape["element_map"]["Plane.Edge2"], "Edge2")
        self.assertTrue(
            any(
                item["kind"] == "modified"
                and "Plane.Edge1" in item.get("sources", [])
                for item in history
            )
        )
        self.assertTrue(
            any(
                item["kind"] == "deleted"
                and "Box.Edge1" in item.get("sources", [])
                for item in history
            )
        )
        self.assertTrue(
            any(
                item["relation"] == "modified"
                and item.get("maker_stage") == "maker_history"
                and item["source"] == {"object": "Plane", "subname": "Edge1"}
                and item["target"] == {"object": "Section", "subname": "Edge1"}
                for item in mapper_history
            )
        )
        self.assertTrue(
            any(
                item["relation"] == "deleted"
                and item.get("maker_stage") == "terminal_history"
                and item["source"] == {"object": "Box", "subname": "Edge1"}
                and item["target"] == {"object": "Section", "subname": ""}
                for item in mapper_history
            )
        )
        self.assert_object_matches_expected(result, "c3m4", "part-section-stable-history")

    def assert_part_loft_history(self, result: dict, sections: list[str], *, linearize: bool = False) -> None:
        loft = result["objects"]["Loft"]
        named_shape = result["named_shapes"]["Loft"]
        mapper_history = named_shape["mapper_history"]

        self.assertEqual(loft["status"], "ok")
        self.assertEqual(loft["feature"], "part_loft")
        self.assertEqual(loft["sections"], sections)
        self.assertEqual(loft["linearize"], linearize)
        self.assertEqual(loft["topo_naming_history"], "maker_history:loft_thru_sections")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_loft:thru_sections_history", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        for section in sections:
            self.assertTrue(
                any(
                    event["maker_stage"] == "maker_history"
                    and event["relation"] == "generated"
                    and event["source"]["object"] == section
                    for event in mapper_history
                ),
                section,
            )

    def test_c3m4_part_loft_two_section_surface_uses_thru_sections_history(self) -> None:
        result = self.run_recompute("part-loft-two-section-surface", "c3m4")
        loft = result["objects"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_shell")
        self.assertEqual(loft["solid"], False)
        self.assertEqual(loft["ruled"], False)
        self.assertEqual(loft["closed"], False)
        self.assertEqual(loft["max_degree"], 5)
        self.assert_part_loft_history(result, ["LowerProfile", "UpperProfile"])
        self.assert_object_matches_expected(result, "c3m4", "part-loft-two-section-surface")

    def test_c3m4_part_loft_solid_builds_solid_not_surface_only(self) -> None:
        result = self.run_recompute("part-loft-solid", "c3m4")
        loft = result["objects"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_solid")
        self.assertEqual(loft["solid"], True)
        self.assertGreater(loft["volume"], 0.0)
        self.assert_part_loft_history(result, ["LowerProfile", "UpperProfile"])
        self.assert_object_matches_expected(result, "c3m4", "part-loft-solid")

    def test_c3m4_part_loft_ruled_accepts_edge_sections(self) -> None:
        result = self.run_recompute("part-loft-ruled", "c3m4")
        loft = result["objects"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_shell")
        self.assertEqual(loft["solid"], False)
        self.assertEqual(loft["ruled"], True)
        self.assertEqual(loft["max_degree"], 2)
        self.assert_part_loft_history(result, ["LowerEdge", "UpperEdge"])
        self.assert_object_matches_expected(result, "c3m4", "part-loft-ruled")

    def test_c3m4_part_loft_closed_duplicates_first_profile(self) -> None:
        result = self.run_recompute("part-loft-closed", "c3m4")
        loft = result["objects"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_shell")
        self.assertEqual(loft["closed"], True)
        self.assert_part_loft_history(result, ["ProfileA", "ProfileB", "ProfileC"])
        self.assert_object_matches_expected(result, "c3m4", "part-loft-closed")

    def test_c3m4_part_loft_invalid_sections_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-loft-invalid-sections", "c3m4")
        codes = [item["code"] for item in result["diagnostics"]]

        self.assertEqual(
            codes,
            [
                "missing_link_target",
                "missing_property",
                "missing_property",
                "execution_failed",
                "execution_failed",
                "execution_failed",
            ],
        )
        for object_name in (
            "MissingSections",
            "EmptySections",
            "OneSection",
            "RepeatedSections",
            "NonProfileSection",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_loft")
        self.assertEqual(result["objects"]["MissingTarget"]["status"], "error")
        self.assert_object_matches_expected(result, "c3m4", "part-loft-invalid-sections")

    def test_c4m1_part_loft_linearize_face_profile_is_expected_backed(self) -> None:
        result = self.run_recompute("part-loft-linearize-profile-face", "c4m1")
        loft = result["objects"]["Loft"]
        named_shape = result["named_shapes"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_shell")
        self.assertEqual(loft["solid"], False)
        self.assertEqual(loft["ruled"], False)
        self.assertEqual(loft["max_degree"], 5)
        self.assert_part_loft_history(result, ["LowerFace", "UpperProfile"], linearize=True)
        self.assertTrue(
            any(status.startswith("part_loft:linearize") for status in named_shape["element_history_status"])
        )
        self.assert_object_matches_expected(result, "c4m1", "part-loft-linearize-profile-face")

    def test_c4m1_part_loft_vertex_profile_is_expected_backed(self) -> None:
        result = self.run_recompute("part-loft-linearize-profile-vertex", "c4m1")
        loft = result["objects"]["Loft"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(loft["shape"], "occt_shell")
        self.assertEqual(loft["solid"], False)
        self.assertEqual(loft["ruled"], True)
        self.assertEqual(loft["max_degree"], 2)
        self.assert_part_loft_history(result, ["BaseProfile", "TipVertex"], linearize=True)
        self.assert_object_matches_expected(result, "c4m1", "part-loft-linearize-profile-vertex")

    def assert_part_sweep_history(
        self,
        result: dict,
        spine: str,
        sections: list[str],
        *,
        transition: str,
        solid: bool = False,
        frenet: bool = True,
        linearize: bool = False,
    ) -> None:
        sweep = result["objects"]["Sweep"]
        named_shape = result["named_shapes"]["Sweep"]
        mapper_history = named_shape["mapper_history"]

        self.assertEqual(sweep["status"], "ok")
        self.assertEqual(sweep["feature"], "part_sweep")
        self.assertEqual(sweep["spine"], spine)
        self.assertEqual(sweep["sections"], sections)
        self.assertEqual(sweep["solid"], solid)
        self.assertEqual(sweep["frenet"], frenet)
        self.assertEqual(sweep["transition"], transition)
        self.assertEqual(sweep["linearize"], linearize)
        self.assertEqual(sweep["topo_naming_history"], "maker_history:pipeshell")
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_sweep:pipeshell_history", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        for source in [spine, *sections]:
            self.assertTrue(
                any(
                    event["maker_stage"] == "maker_history"
                    and event["relation"] in {"generated", "modified"}
                    and event["source"]["object"] == source
                    for event in mapper_history
                ),
                source,
            )

    def test_c3m4_part_sweep_right_corner_surface_uses_pipeshell_history(self) -> None:
        result = self.run_recompute("part-sweep-right-corner-surface", "c3m4")
        sweep = result["objects"]["Sweep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sweep["shape"], "occt_shell")
        self.assert_part_sweep_history(result, "PathWire", ["Profile"], transition="Right corner")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-right-corner-surface")

    def test_c3m4_part_sweep_solid_builds_solid_not_surface_only(self) -> None:
        result = self.run_recompute("part-sweep-solid", "c3m4")
        sweep = result["objects"]["Sweep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sweep["shape"], "occt_solid")
        self.assertGreater(sweep["volume"], 0.0)
        self.assert_part_sweep_history(result, "PathWire", ["Profile"], transition="Right corner", solid=True)
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-solid")

    def test_c3m4_part_sweep_frenet_false_routes_set_mode_false(self) -> None:
        result = self.run_recompute("part-sweep-frenet-off", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_sweep_history(
            result,
            "PathWire",
            ["Profile"],
            transition="Right corner",
            frenet=False,
        )
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-frenet-off")

    def test_c3m4_part_sweep_transition_transformed_is_expected_backed(self) -> None:
        result = self.run_recompute("part-sweep-transition-transformed", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_sweep_history(result, "PathWire", ["Profile"], transition="Transformed")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-transition-transformed")

    def test_c3m4_part_sweep_transition_round_corner_is_expected_backed(self) -> None:
        result = self.run_recompute("part-sweep-transition-round-corner", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_sweep_history(result, "PathWire", ["OpenProfile"], transition="Round corner")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-transition-round-corner")

    def test_c3m4_part_sweep_spine_subedges_compound_before_pipeshell(self) -> None:
        result = self.run_recompute("part-sweep-spine-subedges", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_sweep_history(result, "PathSource", ["Profile"], transition="Right corner")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-spine-subedges")

    def test_c3m4_part_sweep_open_profile_surface_accepts_edge_profile(self) -> None:
        result = self.run_recompute("part-sweep-open-profile-surface", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_sweep_history(result, "PathWire", ["OpenProfile"], transition="Right corner")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-open-profile-surface")

    def test_c3m4_part_sweep_invalid_inputs_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-sweep-invalid-inputs", "c3m4")
        codes = [item["code"] for item in result["diagnostics"]]

        self.assertEqual(
            codes,
            [
                "missing_link_target",
                "missing_link_target",
                "missing_property",
                "missing_property",
                "invalid_subshape",
                "execution_failed",
                "execution_failed",
            ],
        )
        for object_name in (
            "EmptySections",
            "MissingSpine",
            "InvalidSpineSubList",
            "DisconnectedSpine",
            "NonProfileSection",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_sweep")
        self.assertEqual(result["objects"]["InvalidSpineTarget"]["status"], "error")
        self.assertEqual(result["objects"]["InvalidSectionLink"]["status"], "error")
        self.assert_object_matches_expected(result, "c3m4", "part-sweep-invalid-inputs")

    def test_c4m1_part_sweep_multi_profile_linearize_is_expected_backed(self) -> None:
        result = self.run_recompute("part-sweep-multi-profile-linearize", "c4m1")
        sweep = result["objects"]["Sweep"]
        named_shape = result["named_shapes"]["Sweep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertIn(sweep["shape"], {"occt_shell", "occt_solid"})
        self.assert_part_sweep_history(
            result,
            "Path",
            ["LowerProfile", "UpperProfile"],
            transition="Right corner",
            linearize=True,
        )
        self.assertTrue(
            any(status.startswith("part_sweep:linearize") for status in named_shape["element_history_status"])
        )
        self.assert_object_matches_expected(result, "c4m1", "part-sweep-multi-profile-linearize")

    def test_c4m1_part_sweep_advanced_wrapper_has_locatable_diagnostics(self) -> None:
        result = self.run_recompute("part-sweep-advanced-deferred", "c4m1")
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["unsupported_property"])
        self.assertEqual(result["objects"]["AdvancedSweep"]["status"], "error")
        self.assertEqual(result["objects"]["AdvancedSweep"]["feature"], "part_sweep")
        for diagnostic in diagnostics:
            self.assertEqual(diagnostic["object"], "AdvancedSweep")
            self.assertEqual(diagnostic["property"], "Tolerance")
            self.assertIn("target", diagnostic)
            self.assertIn("subname", diagnostic)

    def test_c5m10_part_sweep_auxiliary_spine_contract_is_source_backed(self) -> None:
        result = self.run_recompute("part-sweep-auxiliary-spine-contract", "c5m10")
        sweep = result["objects"]["Sweep"]
        expected = self.expected_freecad("c5m10", "part-sweep-auxiliary-spine-contract")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sweep["advanced"]["mode"], "Auxiliary")
        self.assertEqual(
            sweep["advanced"]["auxiliary_spine"],
            {
                "target": "AuxiliarySpine",
                "subname": "Edge1",
                "curvilinear": False,
                "contact": "NoContact",
            },
        )
        self.assert_part_sweep_history(
            result,
            "PipeSpine",
            ["SketchPipeProfile"],
            transition="Transformed",
            solid=False,
        )
        self.assertEqual(expected["known_gap"]["kind"], "part_sweep_auxiliary_spine_wrapper_oracle_missing")

    def test_c5m10_part_sweep_binormal_contract_is_source_backed(self) -> None:
        result = self.run_recompute("part-sweep-binormal-contract", "c5m10")
        sweep = result["objects"]["Sweep"]
        expected = self.expected_freecad("c5m10", "part-sweep-binormal-contract")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sweep["advanced"]["mode"], "Binormal")
        self.assertEqual(sweep["advanced"]["binormal"], [0.0, 0.0, 1.0])
        self.assertEqual(sweep["advanced"]["binormal_property"], "Binormal")
        self.assert_part_sweep_history(
            result,
            "PipeSpine",
            ["SketchPipeProfile"],
            transition="Transformed",
            solid=False,
        )
        self.assertEqual(expected["known_gap"]["kind"], "part_sweep_binormal_wrapper_oracle_missing")

    def test_c5m10_part_sweep_support_mode_and_mode_payloads_have_locatable_diagnostics(self) -> None:
        result = self.run_recompute("part-sweep-support-mode-diagnostics", "c5m10")
        diagnostics = result["diagnostics"]
        expected = self.expected_freecad("c5m10", "part-sweep-support-mode-diagnostics")

        self.assertEqual(len(diagnostics), 8)
        by_object = {diagnostic["object"]: diagnostic for diagnostic in diagnostics}
        self.assertEqual(
            {
                name: (item["code"], item["property"], item.get("target"), item.get("subname"))
                for name, item in by_object.items()
            },
            {
                "MissingAuxiliaryTarget": (
                    "missing_link_target",
                    "AuxiliarySpine",
                    "MissingAuxiliary",
                    "Edge1",
                ),
                "MissingSupport": (
                    "missing_link_target",
                    "SpineSupport",
                    "MissingSupport",
                    "SpineSupport",
                ),
                "InvalidSupportMode": (
                    "invalid_parameter",
                    "SupportMode",
                    "InvalidSupportMode",
                    "SupportMode",
                ),
                "InvalidSupportSubshape": (
                    "invalid_subshape",
                    "SpineSupport",
                    "SupportPlane",
                    "Face99",
                ),
                "InvalidAuxiliarySubshape": (
                    "invalid_subshape",
                    "AuxiliarySpine",
                    "SupportPlane",
                    "Face99",
                ),
                "InvalidAuxiliaryCurvilinear": (
                    "invalid_parameter",
                    "AuxiliaryCurvilinear",
                    "InvalidAuxiliaryCurvilinear",
                    "AuxiliaryCurvilinear",
                ),
                "ZeroBinormal": (
                    "invalid_parameter",
                    "Binormal",
                    "ZeroBinormal",
                    "Binormal",
                ),
                "MalformedBiNormal": (
                    "invalid_parameter",
                    "BiNormal",
                    "MalformedBiNormal",
                    "BiNormal",
                ),
            },
        )
        for object_name in by_object:
            self.assertEqual(result["objects"][object_name]["status"], "error")
        self.assertEqual(expected["known_gap"]["kind"], "part_sweep_support_mode_wrapper_oracle_missing")

    def assert_part_filling_history(
        self,
        result: dict,
        boundary_mode: str,
        expect_default_params: bool = True,
    ) -> None:
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]
        mapper_history = named_shape["mapper_history"]

        self.assertEqual(filled["status"], "ok")
        self.assertEqual(filled["feature"], "part_filled_face")
        self.assertEqual(filled["helper"], "Part.makeFilledFace")
        self.assertTrue(filled["source_backed_helper"])
        self.assertFalse(filled["freecad_native_document_object"])
        self.assertEqual(filled["shape"], "occt_face")
        self.assertEqual(filled["boundary_mode"], boundary_mode)
        self.assertEqual(filled["boundary_edge_count"], 4)
        self.assertEqual(filled["topo_naming_history"], "maker_history:filling")
        self.assertEqual(filled["default_params"]["degree"], 3)
        self.assertEqual(filled["default_params"]["points_on_curve"], 15)
        self.assertEqual(filled["default_params"]["iterations"], 2)
        self.assertEqual(filled["default_params"]["max_degree"], 8)
        self.assertEqual(filled["default_params"]["max_segments"], 9)
        if expect_default_params:
            self.assertEqual(filled["params"], filled["default_params"])
        self.assertEqual(filled["params_source"], "Part.makeFilledFace constructor kwargs")

        evidence = filled["boundary_source_evidence"]
        self.assertEqual({item["object"] for item in evidence}, {"Profile"})
        self.assertEqual({item["stable_subname"] for item in evidence}, {"Edge1", "Edge2", "Edge3", "Edge4"})
        self.assertEqual({item["shape_kind"] for item in evidence}, {"edge"})
        self.assertEqual(named_shape["element_map_status"], "history_partial")
        self.assertIn("part_filling:filling_history", named_shape["element_history_status"])
        self.assertIn("history_consumed:generated_modified", named_shape["element_history_status"])
        for edge in ("Edge1", "Edge2", "Edge3", "Edge4"):
            self.assertEqual(named_shape["element_map"][f"Profile.{edge}"], edge)
            self.assertTrue(
                any(
                    event["maker_stage"] == "maker_history:filling_boundary"
                    and event["diagnostic_status"] == "filling_boundary_source"
                    and event["source"] == {"object": "Profile", "subname": edge}
                    and event["target"] == {"object": "FilledFace", "subname": "Face1"}
                    for event in mapper_history
                ),
                edge,
            )

    def test_c3m4_part_filling_closed_wire_default_is_helper_expected_backed(self) -> None:
        result = self.run_recompute("part-filling-closed-wire-default", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_filling_history(result, "closed_wire")
        self.assert_object_matches_expected(result, "c3m4", "part-filling-closed-wire-default")

    def test_c3m4_part_filling_boundary_edges_default_builds_edge_wire(self) -> None:
        result = self.run_recompute("part-filling-boundary-edges-default", "c3m4")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_filling_history(result, "edge_wire_closed")
        self.assert_object_matches_expected(result, "c3m4", "part-filling-boundary-edges-default")

    def test_c3m4_part_filling_invalid_inputs_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-filling-invalid-inputs", "c3m4")
        codes = [item["code"] for item in result["diagnostics"]]
        expected = self.expected_freecad("c3m4", "part-filling-invalid-inputs")

        self.assertEqual(
            codes,
            [
                "missing_link_target",
                "missing_property",
                "execution_failed",
                "execution_failed",
                "invalid_support_target",
            ],
        )
        self.assertCountEqual(codes, expected["diagnostic_codes"])
        for object_name in (
            "EmptyBoundary",
            "FaceOnlyBoundary",
            "DisconnectedBoundary",
            "UnsupportedKwargs",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_filled_face")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.makeFilledFace")
        self.assertEqual(result["objects"]["MissingTarget"]["status"], "error")
        self.assert_object_matches_expected(result, "c3m4", "part-filling-invalid-inputs")

    def test_c4m1_part_filling_advanced_kwargs_are_concrete_deferred(self) -> None:
        result = self.run_recompute("part-filling-advanced-deferred", "c4m1")
        diagnostics = result["diagnostics"]

        self.assertEqual(
            [item["code"] for item in diagnostics],
            [
                "invalid_support_target",
                "invalid_order_source",
            ],
        )
        surface = result["objects"]["SurfaceDeferred"]
        self.assertEqual(surface["status"], "ok")
        self.assertEqual(surface["feature"], "part_filled_face")
        self.assertEqual(surface["helper"], "Part.makeFilledFace")
        self.assertEqual(surface["initial_surface_source_evidence"]["object"], "SupportPlane")
        self.assertEqual(surface["initial_surface_source_evidence"]["stable_subname"], "Face1")
        self.assertEqual(surface["surface_support_order_status"], "source_backed_native_helper_oracle_known_gap")
        for object_name in (
            "SupportsDeferred",
            "OrdersDeferred",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_filled_face")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.makeFilledFace")
        non_default = result["objects"]["NonDefaultParamsDeferred"]
        self.assertEqual(non_default["status"], "ok")
        self.assertEqual(non_default["feature"], "part_filled_face")
        self.assertEqual(non_default["helper"], "Part.makeFilledFace")
        self.assertEqual(non_default["params"]["degree"], 4)
        self.assertEqual(non_default["params"]["points_on_curve"], 15)
        self.assertEqual(non_default["default_params"]["degree"], 3)
        self.assertEqual(non_default["params_source"], "Part.makeFilledFace constructor kwargs")
        for diagnostic in diagnostics:
            self.assertIn("property", diagnostic)
            self.assertIn("target", diagnostic)
            self.assertIn("subname", diagnostic)

    def test_c5m8_part_filling_initial_surface_is_source_backed_known_gap(self) -> None:
        result = self.run_recompute("part-filling-initial-surface-boundary", "c5m8")
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]
        expected = self.expected_freecad("c5m8", "part-filling-initial-surface-boundary")

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("known_gap", expected)
        self.assert_part_filling_history(result, "closed_wire")
        self.assertEqual(filled["initial_surface_source_evidence"]["object"], "SupportPlane")
        self.assertEqual(filled["initial_surface_source_evidence"]["stable_subname"], "Face1")
        self.assertEqual(filled["support_order_source_evidence"], [])
        self.assertEqual(filled["surface_support_order_status"], "source_backed_native_helper_oracle_known_gap")
        self.assertIn("part_filling:initial_surface", named_shape["element_history_status"])
        self.assertTrue(
            any(
                event["maker_stage"] == "maker_history:filling_initial_surface"
                and event["diagnostic_status"] == "filling_initial_surface_source"
                and event["source"] == {"object": "SupportPlane", "subname": "Face1"}
                for event in named_shape["mapper_history"]
            )
        )

    def test_c5m8_part_filling_support_order_sources_are_source_backed_known_gap(self) -> None:
        result = self.run_recompute("part-filling-support-order-edge-face", "c5m8")
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]
        expected = self.expected_freecad("c5m8", "part-filling-support-order-edge-face")

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("known_gap", expected)
        self.assert_part_filling_history(result, "edge_wire_closed")
        self.assertIsNone(filled["initial_surface_source_evidence"])
        self.assertEqual(filled["support_face_count"], 2)
        self.assertEqual(filled["order_count"], 2)
        evidence = filled["support_order_source_evidence"]
        self.assertEqual({item["target_stable_subname"] for item in evidence}, {"Edge1", "Edge2"})
        self.assertEqual({item["support_object"] for item in evidence}, {"SupportPlane"})
        self.assertEqual({item["support_stable_subname"] for item in evidence}, {"Face1"})
        self.assertEqual({item["order"] for item in evidence}, {"G1"})
        self.assertIn("part_filling:support_order_sources", named_shape["element_history_status"])
        for edge in ("Edge1", "Edge2"):
            self.assertTrue(
                any(
                    event["maker_stage"] == "maker_history:filling_support_order"
                    and event["diagnostic_status"] == "filling_support_order_source"
                    and event["source"] == {"object": "Profile", "subname": edge}
                    and event["evidence"]["support_object"] == "SupportPlane"
                    and event["evidence"]["support_stable_subname"] == "Face1"
                    and event["evidence"]["order"] == "G1"
                    for event in named_shape["mapper_history"]
                ),
                edge,
            )

    def test_c5m8_part_filling_invalid_support_order_have_locatable_diagnostics(self) -> None:
        result = self.run_recompute("part-filling-invalid-support-order", "c5m8")
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["invalid_support_target", "invalid_order_source"])
        self.assertEqual(diagnostics[0]["target"], "SupportPlane")
        self.assertEqual(diagnostics[0]["subname"], "Face1")
        self.assertEqual(diagnostics[1]["target"], "Profile")
        self.assertEqual(diagnostics[1]["subname"], "Edge1")
        self.assertEqual(result["objects"]["InvalidSupport"]["status"], "error")
        self.assertEqual(result["objects"]["InvalidOrder"]["status"], "error")
        self.assert_object_matches_expected(result, "c5m8", "part-filling-invalid-support-order")

    def test_c5m8_part_filling_non_default_params_are_constructor_batch(self) -> None:
        result = self.run_recompute("part-filling-non-default-params", "c5m8")
        filled = result["objects"]["FilledFace"]
        expected = self.expected_freecad("c5m8", "part-filling-non-default-params")

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_filling_history(result, "edge_wire_closed", expect_default_params=False)
        self.assertEqual(
            filled["params"],
            {
                "degree": 4,
                "points_on_curve": 20,
                "iterations": 3,
                "anisotropy": True,
                "tolerance_2d": 0.00002,
                "tolerance_3d": 0.0002,
                "tolerance_g1": 0.02,
                "tolerance_g2": 0.2,
                "max_degree": 9,
                "max_segments": 12,
            },
        )
        self.assertEqual(filled["default_params"]["degree"], 3)
        self.assertEqual(filled["default_params"]["points_on_curve"], 15)
        self.assertEqual(filled["default_params"]["anisotropy"], False)
        self.assertEqual(filled["default_params"]["max_segments"], 9)
        self.assertEqual(filled["params_source"], "Part.makeFilledFace constructor kwargs")
        self.assertEqual(
            expected["known_gap"]["kind"],
            "part_filling_non_default_params_native_helper_oracle_blocked",
        )

    def test_c5m8_part_filling_param_diagnostics_are_locatable(self) -> None:
        result = self.run_recompute("part-filling-param-diagnostics", "c5m8")
        diagnostics = result["diagnostics"]

        self.assertEqual([item["code"] for item in diagnostics], ["invalid_parameter"] * 3)
        self.assertEqual(
            [(item["object"], item["property"], item["target"], item["subname"]) for item in diagnostics],
            [
                ("InvalidDegree", "Degree", "InvalidDegree", "Degree"),
                ("InvalidTolerance", "Tol3d", "InvalidTolerance", "Tol3d"),
                ("InvalidAnisotropy", "Anisotropy", "InvalidAnisotropy", "Anisotropy"),
            ],
        )
        for object_name in ("InvalidDegree", "InvalidTolerance", "InvalidAnisotropy"):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_filled_face")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.makeFilledFace")
        self.assert_object_matches_expected(result, "c5m8", "part-filling-param-diagnostics")

    def test_c5m8_part_filling_non_boundary_edge_support_is_source_backed_known_gap(self) -> None:
        result = self.run_recompute("part-filling-non-boundary-edge-support", "c5m8")
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]
        expected = self.expected_freecad("c5m8", "part-filling-non-boundary-edge-support")

        self.assertEqual(result["diagnostics"], [])
        self.assertIn("known_gap", expected)
        self.assert_part_filling_history(result, "closed_wire")
        self.assertEqual(filled["non_boundary_constraint_count"], 1)
        self.assertEqual(filled["non_boundary_constraints_status"], "source_backed_native_helper_oracle_known_gap")
        self.assertEqual(filled["support_face_count"], 1)
        self.assertEqual(filled["order_count"], 1)
        self.assertEqual(
            filled["non_boundary_constraint_source_evidence"],
            [
                {
                    "object": "ConstraintEdge",
                    "subname": "Edge1",
                    "stable_subname": "Edge1",
                    "shape_kind": "edge",
                    "builder_call": "Add(edge, support, order, IsBound=false)",
                    "is_boundary": False,
                }
            ],
        )
        self.assertEqual(len(filled["support_order_source_evidence"]), 1)
        support = filled["support_order_source_evidence"][0]
        self.assertEqual(support["target_object"], "ConstraintEdge")
        self.assertEqual(support["target_stable_subname"], "Edge1")
        self.assertEqual(support["support_object"], "SupportPlane")
        self.assertEqual(support["support_stable_subname"], "Face1")
        self.assertEqual(support["order"], "G1")
        self.assertFalse(support["is_boundary"])
        self.assertEqual(support["builder_call"], "Add(edge, support, order, IsBound=false)")
        self.assertIn("part_filling:non_boundary_constraints", named_shape["element_history_status"])
        self.assertTrue(
            any(
                event["maker_stage"] == "maker_history:filling_non_boundary_constraint"
                and event["diagnostic_status"] == "filling_non_boundary_constraint_source"
                and event["source"] == {"object": "ConstraintEdge", "subname": "Edge1"}
                and event["evidence"]["is_bound"] is False
                for event in named_shape["mapper_history"]
            )
        )

    def test_c5m8_part_filling_non_boundary_face_point_is_expected_backed(self) -> None:
        result = self.run_recompute("part-filling-non-boundary-face-point", "c5m8")
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_filling_history(result, "closed_wire")
        self.assertEqual(filled["non_boundary_constraint_count"], 2)
        self.assertEqual(
            {
                (item["object"], item["stable_subname"], item["shape_kind"], item["builder_call"])
                for item in filled["non_boundary_constraint_source_evidence"]
            },
            {
                ("SupportFace", "Face1", "face", "Add(face, order)"),
                ("CenterPoint", "Vertex1", "vertex", "Add(point)"),
            },
        )
        self.assertIn("part_filling:non_boundary_constraints", named_shape["element_history_status"])
        self.assert_object_matches_expected(result, "c5m8", "part-filling-non-boundary-face-point")

    def test_c5m8_part_filling_non_boundary_wire_is_expected_backed(self) -> None:
        result = self.run_recompute("part-filling-non-boundary-wire", "c5m8")
        filled = result["objects"]["FilledFace"]
        named_shape = result["named_shapes"]["FilledFace"]

        self.assertEqual(result["diagnostics"], [])
        self.assert_part_filling_history(result, "closed_wire")
        self.assertEqual(filled["non_boundary_constraint_count"], 3)
        self.assertEqual(
            {item["stable_subname"] for item in filled["non_boundary_constraint_source_evidence"]},
            {"Edge1", "Edge2", "Edge3"},
        )
        self.assertEqual(
            {item["object"] for item in filled["non_boundary_constraint_source_evidence"]},
            {"ConstraintWire"},
        )
        self.assertEqual(
            {item["builder_call"] for item in filled["non_boundary_constraint_source_evidence"]},
            {"Add(edge, support, order, IsBound=false)"},
        )
        self.assertIn("part_filling:non_boundary_constraints", named_shape["element_history_status"])
        self.assert_object_matches_expected(result, "c5m8", "part-filling-non-boundary-wire")

    def test_c5m8_part_filling_non_boundary_diagnostics_are_locatable(self) -> None:
        result = self.run_recompute("part-filling-non-boundary-diagnostics", "c5m8")
        diagnostics = result["diagnostics"]

        self.assertEqual(
            [item["code"] for item in diagnostics],
            ["missing_link_target", "invalid_non_boundary_source", "invalid_subshape"],
        )
        self.assertEqual(
            [
                (
                    item["object"],
                    item["property"],
                    item.get("target"),
                    item.get("subname"),
                )
                for item in diagnostics
            ],
            [
                ("MissingNonBoundaryTarget", "Boundary", "MissingConstraint", "Edge1"),
                ("InvalidNonBoundarySolid", "Boundary", "SolidConstraint", None),
                ("InvalidNonBoundarySubshape", "Boundary", "ConstraintEdge", "Edge99"),
            ],
        )
        for object_name in ("InvalidNonBoundarySolid", "InvalidNonBoundarySubshape"):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_filled_face")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.makeFilledFace")
        self.assertEqual(result["objects"]["MissingNonBoundaryTarget"]["status"], "error")
        self.assert_object_matches_expected(result, "c5m8", "part-filling-non-boundary-diagnostics")

    def test_c5m8_part_filling_compound_optional_boundary_is_expected_backed(self) -> None:
        result = self.run_recompute("part-filling-compound-optional-boundary", "c5m8")
        filled = result["objects"]["FilledFace"]
        compound = result["objects"]["Compound"]
        named_shape = result["named_shapes"]["FilledFace"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(compound["status"], "ok")
        self.assertEqual(compound["feature"], "part_compound")
        self.assertEqual(compound["shape"], "occt_compound")
        self.assertEqual(compound["links"], ["Profile"])
        self.assertEqual(filled["status"], "ok")
        self.assertEqual(filled["feature"], "part_filled_face")
        self.assertEqual(filled["helper"], "Part.makeFilledFace")
        self.assertEqual(filled["boundary_mode"], "closed_wire")
        self.assertEqual(filled["boundary_edge_count"], 4)
        self.assertEqual(filled["compound_source_count"], 1)
        self.assertEqual(filled["compound_expanded_source_count"], 1)
        self.assertEqual(filled["compound_source_expansion_status"], "source_backed")
        self.assertEqual({item["object"] for item in filled["boundary_source_evidence"]}, {"Compound", "Profile"})
        self.assertEqual(
            {item["stable_subname"] for item in filled["boundary_source_evidence"]},
            {"Edge1", "Edge2", "Edge3", "Edge4"},
        )
        self.assertIn("part_filling:compound_source_expansion", named_shape["element_history_status"])
        self.assert_object_matches_expected(result, "c5m8", "part-filling-compound-optional-boundary")

    def test_c5m8_part_filling_wrapper_boundary_is_lifecycle_diagnostic(self) -> None:
        result = self.run_recompute("part-filling-wrapper-boundary", "c5m8")
        diagnostic = result["diagnostics"][0]
        wrapper = result["objects"]["WrapperBoundary"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_wrapper_lifecycle"])
        self.assertEqual(diagnostic["property"], "BRepOffsetAPIMakeFillingWrapper")
        self.assertEqual(diagnostic["target"], "Profile")
        self.assertEqual(diagnostic["subname"], "Edge1")
        self.assertEqual(wrapper["status"], "error")
        self.assertEqual(wrapper["feature"], "part_brepoffsetapi_makefilling_wrapper")
        self.assertEqual(wrapper["helper"], "Part.BRepOffsetAPI.MakeFilling")
        self.assertFalse(wrapper["source_backed_helper"])
        self.assertEqual(wrapper["wrapper_lifecycle"], "python_mutable_builder_unsupported")
        self.assertIn("request-local Filling DTO", wrapper["delete_condition"])
        self.assert_object_matches_expected(result, "c5m8", "part-filling-wrapper-boundary")

    def test_c5m8_part_filling_wrapper_uv_point_is_lifecycle_diagnostic(self) -> None:
        result = self.run_recompute("part-filling-wrapper-uv-point-boundary", "c5m8")
        diagnostic = result["diagnostics"][0]
        wrapper = result["objects"]["WrapperUvPointOnSupport"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_wrapper_lifecycle"])
        self.assertEqual(diagnostic["property"], "BRepOffsetAPIMakeFillingUvPointOnSupport")
        self.assertEqual(diagnostic["target"], "SupportPlane")
        self.assertEqual(diagnostic["subname"], "Face1")
        self.assertEqual(wrapper["status"], "error")
        self.assertEqual(wrapper["feature"], "part_brepoffsetapi_makefilling_wrapper")
        self.assertEqual(wrapper["helper"], "Part.BRepOffsetAPI.MakeFilling")
        self.assertFalse(wrapper["source_backed_helper"])
        self.assertEqual(wrapper["wrapper_lifecycle"], "python_mutable_builder_unsupported")
        self.assertIn("request-local Filling DTO", wrapper["delete_condition"])
        self.assert_object_matches_expected(result, "c5m8", "part-filling-wrapper-uv-point-boundary")

    def test_c3m4_part_geomplate_curve_point_default_is_helper_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-curve-point-default", "c3m4")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertTrue(geomplate["source_backed_helper"])
        self.assertFalse(geomplate["freecad_native_document_object"])
        self.assertTrue(geomplate["is_done"])
        self.assertEqual(geomplate["surface_kind"], "GeomPlate_Surface")
        self.assertEqual(geomplate["shape"], "occt_face")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        self.assertEqual(geomplate["build_params"]["degree"], 3)
        self.assertEqual(geomplate["build_params"]["nb_pts_on_cur"], 10)
        self.assertEqual(geomplate["approximation"]["status"], "ok")
        self.assertEqual(geomplate["approximation"]["surface_kind"], "Geom_BSplineSurface")
        self.assertEqual(sum(item["kind"] == "curve3d" for item in source_evidence), 4)
        self.assertEqual(sum(item["kind"] == "point3d" for item in source_evidence), 1)
        self.assertEqual(
            {item["object"] for item in source_evidence if item["kind"] == "curve3d"},
            {"BoundaryA", "BoundaryB", "BoundaryC", "BoundaryD"},
        )
        self.assertEqual(sum(name.startswith("Face") for name in result["subshapes"]["GeomPlate"]), 1)
        self.assert_object_matches_expected(result, "c3m4", "part-geomplate-curve-point-default")

    def test_c3m4_part_geomplate_invalid_inputs_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-geomplate-invalid-inputs", "c3m4")
        codes = [item["code"] for item in result["diagnostics"]]
        expected = self.expected_freecad("c3m4", "part-geomplate-invalid-inputs")

        self.assertEqual(
            codes,
            [
                "missing_constraints",
                "invalid_curve_source",
                "invalid_point_constraint",
                "invalid_parameter",
                "unsupported_wrapper_lifecycle",
            ],
        )
        self.assertCountEqual(codes, expected["diagnostic_codes"])
        for object_name in (
            "EmptyConstraints",
            "InvalidCurveSource",
            "InvalidPoint",
            "InvalidParameter",
            "UnsupportedPlateSurfaceCurves",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_geomplate_surface")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.GeomPlate.BuildPlateSurface")
            self.assertTrue(result["objects"][object_name]["source_backed_helper"])
            self.assertFalse(result["objects"][object_name]["freecad_native_document_object"])
        self.assert_object_matches_expected(result, "c3m4", "part-geomplate-invalid-inputs")

    def test_c4m1_part_geomplate_advanced_constraints_are_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-advanced-constraints", "c4m1")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertTrue(geomplate["source_backed_helper"])
        self.assertFalse(geomplate["freecad_native_document_object"])
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        self.assertEqual(geomplate["build_params"]["degree"], 3)
        self.assertEqual(geomplate["build_params"]["nb_pts_on_cur"], 10)
        self.assertEqual(geomplate["approximation"]["status"], "ok")
        self.assertEqual(geomplate["approximation"]["surface_kind"], "Geom_BSplineSurface")
        self.assertEqual(geomplate["approximation"]["tol_3d"], 0.02)
        self.assertEqual(geomplate["approximation"]["continuity"], "C1")
        self.assertEqual(geomplate["approximation"]["max_segments"], 12)
        self.assertEqual(geomplate["approximation"]["max_degree"], 4)
        self.assertEqual(geomplate["approximation"]["max_distance"], 0.0001)
        self.assertEqual(geomplate["approximation"]["crit_order"], 0)
        curve_sources = [item for item in source_evidence if item["kind"] == "curve3d"]
        point_sources = [item for item in source_evidence if item["kind"] == "point3d"]
        self.assertEqual(len(curve_sources), 4)
        self.assertEqual(len(point_sources), 1)
        self.assertEqual(
            {item["object"] for item in curve_sources},
            {"BoundaryA", "BoundaryB", "BoundaryC", "BoundaryD"},
        )
        self.assertEqual({item["nb_pts"] for item in curve_sources}, {10})
        self.assertEqual({item["order"] for item in curve_sources}, {0})
        self.assertEqual(point_sources[0]["order"], 0)
        self.assertNotIn("curve2d", {item["kind"] for item in source_evidence})
        self.assertNotIn("projected_curve2d", {item["kind"] for item in source_evidence})
        self.assertNotIn("point2d", {item["kind"] for item in source_evidence})
        self.assert_object_matches_expected(result, "c4m1", "part-geomplate-advanced-constraints")

    def test_c4m1_part_geomplate_advanced_wrappers_are_concrete_deferred(self) -> None:
        result = self.run_recompute("part-geomplate-advanced-deferred", "c4m1")
        diagnostics = result["diagnostics"]

        self.assertEqual(
            [item["code"] for item in diagnostics],
            [
                "invalid_curve2d_source",
                "invalid_point2d_source",
                "unsupported_wrapper_lifecycle",
            ],
        )
        for object_name in (
            "Curve2dDeferred",
            "Point2dDeferred",
            "PlateSurfaceCurvesDeferred",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_geomplate_surface")
            self.assertEqual(result["objects"][object_name]["helper"], "Part.GeomPlate.BuildPlateSurface")
        initial_surface = result["objects"]["InitialSurfaceDeferred"]
        self.assertEqual(initial_surface["status"], "ok")
        self.assertEqual(initial_surface["feature"], "part_geomplate_surface")
        self.assertEqual(initial_surface["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertIn("initial_surface", {item["kind"] for item in initial_surface["source_evidence"]})
        for diagnostic in diagnostics:
            self.assertIn("property", diagnostic)
            self.assertIn("target", diagnostic)
            self.assertIn("subname", diagnostic)

    def test_c5m7_part_geomplate_initial_surface_g0_is_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-initial-surface-g0", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        self.assertEqual(sum(item["kind"] == "initial_surface" for item in source_evidence), 1)
        initial_surface = next(item for item in source_evidence if item["kind"] == "initial_surface")
        self.assertEqual(initial_surface["object"], "SupportPlane")
        self.assertEqual(initial_surface["subname"], "Face1")
        self.assertEqual(initial_surface["stable_subname"], "Face1")
        self.assertNotIn("curve_on_surface", {item["kind"] for item in source_evidence})
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-initial-surface-g0")

    def test_c5m7_part_geomplate_g1_curve_on_surface_is_source_backed_with_native_oracle_blocker(self) -> None:
        result = self.run_recompute("part-geomplate-g1-curve-on-surface", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]
        expected = self.expected_freecad("c5m7", "part-geomplate-g1-curve-on-surface")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        curve_on_surface = [item for item in source_evidence if item["kind"] == "curve_on_surface"]
        self.assertEqual(len(curve_on_surface), 1)
        self.assertEqual(curve_on_surface[0]["object"], "SupportPlane")
        self.assertEqual(curve_on_surface[0]["subname"], "Edge1")
        self.assertEqual(curve_on_surface[0]["order"], 1)
        self.assertEqual(curve_on_surface[0]["surface_object"], "SupportPlane")
        self.assertEqual(curve_on_surface[0]["surface_subname"], "Face1")
        self.assertEqual(sum(item["kind"] == "curve3d" for item in source_evidence), 3)
        self.assertEqual(sum(item["kind"] == "point3d" for item in source_evidence), 1)
        self.assertEqual(
            expected["known_gap"]["kind"],
            "geomplate_g1_curve_on_surface_native_oracle_blocked",
        )

    def test_c5m7_part_geomplate_curve2d_on_surface_is_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-curve2d-on-surface", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assertEqual(geomplate["dto"], "PartGeomPlateSurfaceDTO")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        curve2d = [item for item in source_evidence if item["kind"] == "curve2d_on_surface"]
        self.assertEqual(len(curve2d), 1)
        self.assertEqual(curve2d[0]["object"], "BoundaryA")
        self.assertEqual(curve2d[0]["subname"], "Edge1")
        self.assertEqual(curve2d[0]["surface_object"], "SupportPlane")
        self.assertEqual(curve2d[0]["surface_subname"], "Face1")
        self.assertEqual(curve2d[0]["curve2d_start"], [0.0, 0.0])
        self.assertEqual(curve2d[0]["curve2d_end"], [4.0, 0.0])
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-curve2d-on-surface")

    def test_c5m7_part_geomplate_projected_curve2d_is_source_backed_with_native_oracle_blocker(self) -> None:
        result = self.run_recompute("part-geomplate-projected-curve2d", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]
        expected = self.expected_freecad("c5m7", "part-geomplate-projected-curve2d")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        projected = [item for item in source_evidence if item["kind"] == "projected_curve2d"]
        self.assertEqual(len(projected), 1)
        self.assertEqual(projected[0]["object"], "BoundaryA")
        self.assertEqual(projected[0]["surface_object"], "SupportPlane")
        self.assertEqual(projected[0]["surface_subname"], "Face1")
        self.assertEqual(projected[0]["curve2d_start"], [0.0, 0.0])
        self.assertEqual(projected[0]["curve2d_end"], [4.0, 0.0])
        self.assertEqual(projected[0]["tol_u"], 0.01)
        self.assertEqual(projected[0]["tol_v"], 0.01)
        self.assertEqual(
            expected["known_gap"]["kind"],
            "geomplate_projected_curve2d_native_oracle_blocked",
        )

    def test_c5m7_part_geomplate_point2d_on_surface_is_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-point2d-on-surface", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        point2d = [item for item in source_evidence if item["kind"] == "point2d_on_surface"]
        self.assertEqual(len(point2d), 1)
        self.assertEqual(point2d[0]["point"], [2.0, 2.0, 1.0])
        self.assertEqual(point2d[0]["point2d"], [2.0, 2.0])
        self.assertEqual(point2d[0]["surface_object"], "SupportPlane")
        self.assertEqual(point2d[0]["surface_subname"], "Face1")
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-point2d-on-surface")

    def test_c5m7_part_geomplate_mixed_surface_constraints_are_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-mixed-surface-constraints", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        kinds = [item["kind"] for item in geomplate["source_evidence"]]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        self.assertEqual(kinds.count("curve2d_on_surface"), 1)
        self.assertEqual(kinds.count("point2d_on_surface"), 1)
        self.assertEqual(kinds.count("curve3d"), 3)
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-mixed-surface-constraints")

    def test_c5m7_part_geomplate_point_custom_criteria_are_expected_backed(self) -> None:
        result = self.run_recompute("part-geomplate-point-custom-criteria", "c5m7")
        geomplate = result["objects"]["GeomPlate"]
        source_evidence = geomplate["source_evidence"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(geomplate["status"], "ok")
        self.assertEqual(geomplate["curve_constraint_count"], 4)
        self.assertEqual(geomplate["point_constraint_count"], 1)
        point = next(item for item in source_evidence if item["kind"] == "point3d")
        self.assertEqual(point["order"], 0)
        self.assertEqual(point["g0_criterion"], 0.05)
        self.assertEqual(point["g1_criterion"], 0.02)
        self.assertEqual(point["g2_criterion"], 0.3)
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-point-custom-criteria")

    def test_c5m7_part_geomplate_curve_criteria_are_locatable_diagnostics(self) -> None:
        result = self.run_recompute("part-geomplate-curve-criteria-diagnostic", "c5m7")
        diagnostic = result["diagnostics"][0]
        geomplate = result["objects"]["CurveCriteriaDiagnostic"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_curve_criteria"])
        self.assertEqual(diagnostic["property"], "CurveConstraints.G0Criterion")
        self.assertEqual(diagnostic["target"], "BoundaryA")
        self.assertEqual(diagnostic["subname"], "Edge1")
        self.assertEqual(geomplate["status"], "error")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-curve-criteria-diagnostic")

    def test_c5m7_part_geomplate_wrapper_boundary_is_lifecycle_diagnostic(self) -> None:
        result = self.run_recompute("part-geomplate-wrapper-boundary", "c5m7")
        diagnostic = result["diagnostics"][0]
        geomplate = result["objects"]["PlateSurfaceCurvesBoundary"]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_wrapper_lifecycle"])
        self.assertEqual(diagnostic["property"], "PlateSurfaceCurves")
        self.assertEqual(diagnostic["target"], "SupportPlane")
        self.assertEqual(diagnostic["subname"], "Face1")
        self.assertEqual(geomplate["status"], "error")
        self.assertEqual(geomplate["feature"], "part_geomplate_surface")
        self.assertEqual(geomplate["helper"], "Part.GeomPlate.BuildPlateSurface")
        self.assert_object_matches_expected(result, "c5m7", "part-geomplate-wrapper-boundary")

    def project_on_surface_provenance_events(self, result: dict, object_name: str) -> list[dict]:
        return [
            event
            for event in result["named_shapes"][object_name]["mapper_history"]
            if event.get("diagnostic_status") == "project_on_surface_edge_wire_provenance"
        ]

    def project_on_surface_face_all_events(self, result: dict, object_name: str) -> list[dict]:
        return [
            event
            for event in result["named_shapes"][object_name]["mapper_history"]
            if event.get("diagnostic_status") == "project_on_surface_face_all_compound_provenance"
        ]

    def test_c5m9_part_project_on_surface_edge_provenance_has_mapper_history(self) -> None:
        result = self.run_recompute("part-project-on-surface-edge-provenance", "c5m9")
        projected = result["objects"]["ProjectedEdgeProvenance"]
        events = self.project_on_surface_provenance_events(result, "ProjectedEdgeProvenance")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["projection_item_ledger"], [
            {
                "source_object": "ProjectionLine",
                "source_subname": "Edge1",
                "stable_subname": "Edge1",
                "projection_item_index": 0,
                "source_shape_kind": "edge",
            }
        ])
        self.assertEqual(len(projected["projected_edge_wire_history"]), 1)
        self.assertEqual(len(events), 1)
        event = events[0]
        self.assertEqual(event["source"], {"object": "ProjectionLine", "subname": "Edge1"})
        self.assertEqual(event["target"], {"object": "ProjectedEdgeProvenance", "subname": "Edge1"})
        self.assertEqual(event["relation"], "generated")
        self.assertEqual(event["maker_stage"], "project_wire")
        self.assertEqual(event["recoverability"], "resolved")
        evidence = event["evidence"]
        self.assertEqual(evidence["projection_item_index"], 0)
        self.assertEqual(evidence["source_shape_kind"], "edge")
        self.assertEqual(evidence["edge_fragment_index"], 0)
        self.assertEqual(evidence["element_map_target"], "Edge1")
        self.assertEqual(evidence["reference_recovery_hook"], "mapper_history_event_target_subname")
        self.assertEqual(evidence["wire_fragment_ownership"]["source_object"], "ProjectionLine")

    def test_c5m9_part_project_on_surface_wire_split_records_fragment_ownership(self) -> None:
        result = self.run_recompute("part-project-on-surface-wire-split-provenance", "c5m9")
        projected = result["objects"]["ProjectedWireSplitProvenance"]
        events = self.project_on_surface_provenance_events(result, "ProjectedWireSplitProvenance")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["projection_item_ledger"][0]["source_object"], "ProjectionWireFace")
        self.assertEqual(projected["projection_item_ledger"][0]["source_subname"], "Wire1")
        self.assertEqual(projected["projection_item_ledger"][0]["projection_item_index"], 0)
        self.assertEqual(projected["projection_item_ledger"][0]["source_shape_kind"], "wire")
        self.assertEqual(len(events), 4)
        self.assertEqual({event["relation"] for event in events}, {"split"})
        self.assertEqual(
            sorted(event["evidence"]["edge_fragment_index"] for event in events),
            [0, 1, 2, 3],
        )
        self.assertEqual(
            sorted(event["target"]["subname"] for event in events),
            ["Edge1", "Edge2", "Edge3", "Edge4"],
        )
        for event in events:
            evidence = event["evidence"]
            self.assertEqual(event["source"], {"object": "ProjectionWireFace", "subname": "Wire1"})
            self.assertEqual(event["shape_kind"], "edge")
            self.assertEqual(evidence["source_shape_kind"], "wire")
            self.assertEqual(evidence["wire_fragment_ownership"]["projection_item_index"], 0)
            self.assertEqual(
                evidence["wire_fragment_ownership"]["edge_fragment_index"],
                evidence["edge_fragment_index"],
            )
            self.assertEqual(evidence["reference_recovery_hook"], "mapper_history_event_target_subname")

    def test_c5m9_part_project_on_surface_invalid_provenance_diagnostics_are_locatable(self) -> None:
        result = self.run_recompute("part-project-on-surface-invalid-provenance-diagnostics", "c5m9")
        diagnostics = {
            item["object"]: item
            for item in result["diagnostics"]
        }

        self.assertEqual(
            [item["code"] for item in result["diagnostics"]],
            ["missing_link_target", "unsupported_subshape_kind", "invalid_subshape"],
        )
        self.assertEqual(diagnostics["ProjectionMissingTarget"]["property"], "Projection")
        self.assertEqual(diagnostics["ProjectionMissingTarget"]["target"], "MissingProjectionLine")
        self.assertEqual(diagnostics["ProjectionMissingTarget"]["subname"], "Edge1")
        self.assertEqual(diagnostics["ProjectionUnsupportedVertex"]["property"], "Projection")
        self.assertEqual(diagnostics["ProjectionUnsupportedVertex"]["target"], "ProjectionVertex")
        self.assertEqual(diagnostics["ProjectionUnsupportedVertex"]["subname"], "Vertex1")
        self.assertEqual(diagnostics["ProjectionCountMismatch"]["property"], "Projection")
        self.assertEqual(diagnostics["ProjectionCountMismatch"]["target"], "ProjectionLine")
        self.assertEqual(diagnostics["ProjectionCountMismatch"]["subname"], "Edge1")

    def test_c5m9_part_project_on_surface_face_rebuild_records_wire_ownership(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-rebuild-provenance", "c5m9")
        projected = result["objects"]["ProjectedFaceRebuildProvenance"]
        named_shape = result["named_shapes"]["ProjectedFaceRebuildProvenance"]
        events = self.project_on_surface_face_all_events(result, "ProjectedFaceRebuildProvenance")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["mode"], "Faces")
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_inner_wire_count"], 1)
        self.assertEqual(len(projected["projected_face_all_history"]), 1)
        self.assertEqual(len(events), 1)
        self.assertIn(
            "part_project_on_surface:face_all_compound_mapper_history",
            named_shape["element_history_status"],
        )
        event = events[0]
        self.assertEqual(event["source"], {"object": "ProjectionFaceWithHole", "subname": "Face1"})
        self.assertEqual(event["target"], {"object": "ProjectedFaceRebuildProvenance", "subname": "Face1"})
        self.assertEqual(event["relation"], "generated")
        self.assertEqual(event["maker_stage"], "face_rebuild")
        self.assertEqual(event["recoverability"], "resolved")
        evidence = event["evidence"]
        self.assertEqual(evidence["face_rebuild_id"], "projection_item_0:face_rebuild:0")
        self.assertEqual(evidence["compound_child_index"], 0)
        self.assertFalse(evidence["offset_applied"])
        self.assertEqual(evidence["pre_offset_child_id"], "projection_item_0:face_rebuild:0")
        self.assertEqual(evidence["reference_recovery_hook"], "mapper_history_event_target_subname")
        self.assertEqual(
            [source["face_wire_role"] for source in evidence["face_wire_sources"]],
            ["outer", "inner"],
        )
        self.assertEqual(
            evidence["face_rebuild_ownership"]["outer_wire_source"]["face_wire_index"],
            0,
        )
        self.assertEqual(len(evidence["face_rebuild_ownership"]["inner_wire_sources"]), 1)
        self.assertEqual(evidence["face_rebuild_ownership"]["source_object"], "ProjectionFaceWithHole")

    def test_c5m9_part_project_on_surface_all_compound_records_solid_and_child_provenance(self) -> None:
        result = self.run_recompute("part-project-on-surface-all-compound-provenance", "c5m9")
        projected = result["objects"]["ProjectedAllCompoundProvenance"]
        named_shape = result["named_shapes"]["ProjectedAllCompoundProvenance"]
        face_all_events = self.project_on_surface_face_all_events(result, "ProjectedAllCompoundProvenance")
        edge_events = self.project_on_surface_provenance_events(result, "ProjectedAllCompoundProvenance")

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["mode"], "All")
        self.assertEqual(projected["height"], 1.25)
        self.assertEqual(projected["offset"], 0.25)
        self.assertEqual(projected["offset_application"], "compound_child_moved_after_filter")
        self.assertEqual(projected["offset_vector"], [0.0, 0.0, 0.25])
        self.assertEqual(projected["projection_item_ledger"][0]["source_object"], "ProjectionFace")
        self.assertEqual(projected["projection_item_ledger"][1]["source_object"], "ProjectionLine")
        self.assertEqual(projected["projected_solid_count"], 1)
        self.assertGreaterEqual(projected["projected_face_count"], 6)
        self.assertEqual(len(face_all_events), 1)
        self.assertEqual(len(edge_events), 1)
        self.assertIn(
            "part_project_on_surface:compound_child_reference_recovery",
            named_shape["element_history_status"],
        )

        solid_event = face_all_events[0]
        solid_evidence = solid_event["evidence"]
        self.assertEqual(solid_event["source"], {"object": "ProjectionFace", "subname": "Face1"})
        self.assertEqual(solid_event["maker_stage"], "height_solid")
        self.assertEqual(solid_event["relation"], "generated")
        self.assertEqual(solid_evidence["height_solid_id"], "projection_item_0:height_solid:0")
        self.assertEqual(solid_evidence["source_face_target"], "projection_item_0:face_rebuild:0")
        self.assertEqual(solid_evidence["height_solid_ownership"]["extrude_direction"], [0.0, 0.0, -1.25])
        self.assertEqual(solid_evidence["compound_child_index"], 0)
        self.assertTrue(solid_evidence["offset_applied"])
        self.assertEqual(solid_evidence["pre_offset_child_id"], "projection_item_0:height_solid:0")
        self.assertIn("compound_child_0", solid_evidence["child_element_map_key"])
        self.assertEqual(solid_evidence["reference_recovery_hook"], "mapper_history_event_target_subname")

        edge_evidence = edge_events[0]["evidence"]
        self.assertEqual(edge_events[0]["source"], {"object": "ProjectionLine", "subname": "Edge1"})
        self.assertEqual(edge_evidence["compound_child_index"], 1)
        self.assertTrue(edge_evidence["offset_applied"])
        self.assertEqual(edge_evidence["pre_offset_child_id"], "projection_item_1:project_wire:0")
        self.assertIn("compound_child_1", edge_evidence["child_element_map_key"])

    def test_c4m1_part_project_on_surface_edge_plane_is_expected_backed(self) -> None:
        result = self.run_recompute("part-project-on-surface-edge-plane", "c4m1")
        projected = result["objects"]["ProjectedEdges"]
        named_shape = result["named_shapes"]["ProjectedEdges"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["shape"], "occt_compound")
        self.assertEqual(projected["source_support"], "SupportPlane")
        self.assertEqual(projected["support_face"], "Face1")
        self.assertEqual(projected["source_projection"], "ProjectionLine")
        self.assertEqual(projected["projection_subshape"], "Edge1")
        self.assertEqual(projected["mode"], "Edges")
        self.assertEqual(projected["height"], 0.0)
        self.assertEqual(projected["offset"], 0.0)
        self.assertEqual(projected["topo_naming_history"], "indexed_projected_edges_no_mapper_history")
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 0)
        self.assertEqual(projected["projected_wire_count"], 0)
        self.assertEqual(projected["projected_inner_wire_count"], 0)
        self.assertEqual(sum(name.startswith("Edge") for name in result["subshapes"]["ProjectedEdges"]), 1)
        self.assertEqual(sum(name.startswith("Face") for name in result["subshapes"]["ProjectedEdges"]), 0)
        self.assertEqual(named_shape["element_map_status"], "indexed_only")
        self.assertNotIn("ProjectionLine.Edge1", named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-edge-plane")

    def test_c4m1_part_project_on_surface_face_plane_rebuilds_face(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-plane", "c4m1")
        projected = result["objects"]["ProjectedFace"]
        named_shape = result["named_shapes"]["ProjectedFace"]
        subshapes = result["subshapes"]["ProjectedFace"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["shape"], "occt_compound")
        self.assertEqual(projected["source_support"], "SupportPlane")
        self.assertEqual(projected["support_face"], "Face1")
        self.assertEqual(projected["source_projection"], "ProjectionFace")
        self.assertEqual(projected["projection_subshape"], "Face1")
        self.assertEqual(projected["mode"], "Faces")
        self.assertEqual(projected["height"], 0.0)
        self.assertEqual(projected["offset"], 0.0)
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_wire_count"], 1)
        self.assertEqual(projected["projected_inner_wire_count"], 0)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 4)
        self.assertEqual(named_shape["element_map_status"], "indexed_only")
        self.assertNotIn("ProjectionFace.Face1", named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-face-plane")

    def test_c4m1_part_project_on_surface_face_with_hole_preserves_inner_wire(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-hole-plane", "c4m1")
        projected = result["objects"]["ProjectedFaceWithHole"]
        subshapes = result["subshapes"]["ProjectedFaceWithHole"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["source_projection"], "ProjectionFaceWithHole")
        self.assertEqual(projected["projection_subshape"], "Face1")
        self.assertEqual(projected["mode"], "Faces")
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_wire_count"], 2)
        self.assertEqual(projected["projected_inner_wire_count"], 1)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 8)
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-face-hole-plane")

    def test_c4m1_part_project_on_surface_face_input_edges_mode_filters_to_wire(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-edges-mode", "c4m1")
        projected = result["objects"]["ProjectedFaceEdges"]
        subshapes = result["subshapes"]["ProjectedFaceEdges"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["source_projection"], "ProjectionFace")
        self.assertEqual(projected["projection_subshape"], "Face1")
        self.assertEqual(projected["mode"], "Edges")
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 0)
        self.assertEqual(projected["projected_wire_count"], 1)
        self.assertEqual(projected["projected_inner_wire_count"], 0)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 0)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 4)
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-face-edges-mode")

    def test_c4m1_part_project_on_surface_all_mode_accepts_zero_height_face(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-all-plane", "c4m1")
        projected = result["objects"]["ProjectedAll"]
        subshapes = result["subshapes"]["ProjectedAll"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["source_projection"], "ProjectionFace")
        self.assertEqual(projected["projection_subshape"], "Face1")
        self.assertEqual(projected["mode"], "All")
        self.assertEqual(projected["height"], 0.0)
        self.assertEqual(projected["offset"], 0.0)
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_wire_count"], 1)
        self.assertEqual(projected["projected_inner_wire_count"], 0)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 4)
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-face-all-plane")

    def test_c4m1_part_project_on_surface_all_height_extrudes_face_to_solid(self) -> None:
        result = self.run_recompute("part-project-on-surface-height-boundaries", "c4m1")
        projected_all = result["objects"]["ProjectedAllHeight"]
        projected_faces = result["objects"]["ProjectedFacesHeight"]
        all_subshapes = result["subshapes"]["ProjectedAllHeight"]
        faces_subshapes = result["subshapes"]["ProjectedFacesHeight"]
        all_named_shape = result["named_shapes"]["ProjectedAllHeight"]
        faces_named_shape = result["named_shapes"]["ProjectedFacesHeight"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected_all["status"], "ok")
        self.assertEqual(projected_all["feature"], "part_project_on_surface")
        self.assertEqual(projected_all["shape"], "occt_compound")
        self.assertEqual(projected_all["mode"], "All")
        self.assertEqual(projected_all["height"], 1.5)
        self.assertEqual(projected_all["offset"], 0.0)
        self.assertEqual(projected_all["projected_solid_count"], 1)
        self.assertEqual(projected_all["projected_face_count"], 6)
        self.assertEqual(projected_all["projected_wire_count"], 6)
        self.assertAlmostEqual(projected_all["volume"], 9.0)
        self.assertEqual(sum(name.startswith("Face") for name in all_subshapes), 6)
        self.assertEqual(sum(name.startswith("Edge") for name in all_subshapes), 12)
        self.assertEqual(sum(name.startswith("Vertex") for name in all_subshapes), 8)
        self.assertEqual(all_named_shape["element_map_status"], "indexed_only")
        self.assertNotIn("ProjectionFace.Face1", all_named_shape["element_map"])

        self.assertEqual(projected_faces["status"], "ok")
        self.assertEqual(projected_faces["feature"], "part_project_on_surface")
        self.assertEqual(projected_faces["mode"], "Faces")
        self.assertEqual(projected_faces["height"], 1.5)
        self.assertEqual(projected_faces["projected_solid_count"], 0)
        self.assertEqual(projected_faces["projected_face_count"], 1)
        self.assertAlmostEqual(projected_faces["volume"], 0.0)
        self.assertEqual(sum(name.startswith("Face") for name in faces_subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in faces_subshapes), 4)
        self.assertEqual(sum(name.startswith("Vertex") for name in faces_subshapes), 4)
        self.assertEqual(faces_named_shape["element_map_status"], "indexed_only")
        self.assertNotIn("ProjectionFace.Face1", faces_named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-height-boundaries")

    def test_c4m1_part_project_on_surface_edge_offset_moves_after_projection(self) -> None:
        result = self.run_recompute("part-project-on-surface-edge-offset", "c4m1")
        projected = result["objects"]["ProjectedEdgesOffset"]
        subshapes = result["subshapes"]["ProjectedEdgesOffset"]
        named_shape = result["named_shapes"]["ProjectedEdgesOffset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["mode"], "Edges")
        self.assertEqual(projected["height"], 0.0)
        self.assertEqual(projected["offset"], 0.75)
        self.assertEqual(projected["offset_application"], "compound_child_moved_after_filter")
        self.assertEqual(projected["offset_vector"], [0.0, 0.0, 0.75])
        self.assert_bbox_close_delta(projected["bbox"], [1.0, 1.0, 0.75], [5.0, 2.0, 0.75], 0.11)
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 0)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Vertex") for name in subshapes), 2)
        self.assertEqual(named_shape["element_map_status"], "indexed_only")
        self.assertIn("Edge1", subshapes)
        self.assertNotIn("ProjectionLine.Edge1", named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-edge-offset")

    def test_c4m1_part_project_on_surface_face_offset_preserves_indexed_subshapes(self) -> None:
        result = self.run_recompute("part-project-on-surface-face-offset", "c4m1")
        projected = result["objects"]["ProjectedFaceOffset"]
        subshapes = result["subshapes"]["ProjectedFaceOffset"]
        named_shape = result["named_shapes"]["ProjectedFaceOffset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["mode"], "Faces")
        self.assertEqual(projected["offset"], 0.5)
        self.assertEqual(projected["offset_application"], "compound_child_moved_after_filter")
        self.assertEqual(projected["offset_vector"], [0.0, 0.0, 0.5])
        self.assert_bbox_close_delta(projected["bbox"], [1.0, 1.0, 0.5], [4.0, 3.0, 0.5], 0.11)
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_wire_count"], 1)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 4)
        self.assertEqual(sum(name.startswith("Vertex") for name in subshapes), 4)
        self.assertEqual(named_shape["element_map_status"], "indexed_only")
        self.assertIn("Face1", subshapes)
        self.assertNotIn("ProjectionFace.Face1", named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-face-offset")

    def test_c4m1_part_project_on_surface_height_offset_moves_solid_after_prism(self) -> None:
        result = self.run_recompute("part-project-on-surface-height-offset-boundary", "c4m1")
        projected = result["objects"]["ProjectedAllHeightOffset"]
        subshapes = result["subshapes"]["ProjectedAllHeightOffset"]
        named_shape = result["named_shapes"]["ProjectedAllHeightOffset"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["mode"], "All")
        self.assertEqual(projected["height"], 1.5)
        self.assertEqual(projected["offset"], 0.25)
        self.assertEqual(projected["offset_application"], "compound_child_moved_after_filter")
        self.assertEqual(projected["offset_vector"], [0.0, 0.0, 0.25])
        self.assert_bbox_close_delta(projected["bbox"], [1.0, 1.0, -1.25], [4.0, 3.0, 0.25], 0.11)
        self.assertEqual(projected["projected_solid_count"], 1)
        self.assertEqual(projected["projected_face_count"], 6)
        self.assertEqual(projected["projected_wire_count"], 6)
        self.assertAlmostEqual(projected["volume"], 9.0)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 6)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 12)
        self.assertEqual(sum(name.startswith("Vertex") for name in subshapes), 8)
        self.assertEqual(named_shape["element_map_status"], "indexed_only")
        self.assertNotIn("ProjectionFace.Face1", named_shape["element_map"])
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-height-offset-boundary")

    def test_c4m1_part_project_on_surface_multi_edge_preserves_link_order(self) -> None:
        result = self.run_recompute("part-project-on-surface-multi-edge-order", "c4m1")
        projected = result["objects"]["ProjectedMultiEdges"]
        subshapes = result["subshapes"]["ProjectedMultiEdges"]
        edge_segments = result["mesh"]["ProjectedMultiEdges"]["edgeSegments"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["source_projection"], "ProjectionRightLine")
        self.assertEqual(projected["projection_subshape"], "Edge1")
        self.assertEqual(
            projected["projection_items"],
            [
                {"object": "ProjectionRightLine", "subshape": "Edge1"},
                {"object": "ProjectionLeftLine", "subshape": "Edge1"},
            ],
        )
        self.assertEqual(projected["mode"], "Edges")
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 0)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 2)
        self.assertEqual(len(edge_segments), 2)
        first_center_x = sum(point[0] for point in edge_segments[0]["points"]) / 2.0
        second_center_x = sum(point[0] for point in edge_segments[1]["points"]) / 2.0
        self.assertGreater(first_center_x, 4.0)
        self.assertLess(second_center_x, 3.0)
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-multi-edge-order")

    def test_c4m1_part_project_on_surface_mixed_face_edge_preserves_projection_metadata(self) -> None:
        result = self.run_recompute("part-project-on-surface-mixed-face-edge-order", "c4m1")
        projected = result["objects"]["ProjectedMixed"]
        subshapes = result["subshapes"]["ProjectedMixed"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(projected["status"], "ok")
        self.assertEqual(projected["feature"], "part_project_on_surface")
        self.assertEqual(projected["source_projection"], "ProjectionFace")
        self.assertEqual(projected["projection_subshape"], "Face1")
        self.assertEqual(
            projected["projection_items"],
            [
                {"object": "ProjectionFace", "subshape": "Face1"},
                {"object": "ProjectionLine", "subshape": "Edge1"},
            ],
        )
        self.assertEqual(projected["mode"], "All")
        self.assertEqual(projected["projected_solid_count"], 0)
        self.assertEqual(projected["projected_face_count"], 1)
        self.assertEqual(projected["projected_wire_count"], 1)
        self.assertEqual(sum(name.startswith("Face") for name in subshapes), 1)
        self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 5)
        self.assert_object_matches_expected(result, "c4m1", "part-project-on-surface-mixed-face-edge-order")

    def test_c4m1_part_project_on_surface_deferred_boundaries_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-project-on-surface-deferred-boundaries", "c4m1")
        codes = [item["code"] for item in result["diagnostics"]]

        self.assertEqual(
            codes,
            [
                "missing_link_target",
                "execution_failed",
                "invalid_subshape",
                "invalid_subshape",
                "unsupported_subshape_kind",
                "missing_property",
            ],
        )
        for object_name in (
            "ModeFacesDeferred",
            "ProjectionCountMismatch",
            "ProjectionEmptySubname",
            "ProjectionMissingTarget",
            "ProjectionUnsupportedVertex",
            "MissingSupport",
        ):
            self.assertEqual(result["objects"][object_name]["status"], "error")
        for object_name in (
            "ModeFacesDeferred",
            "ProjectionCountMismatch",
            "ProjectionEmptySubname",
            "ProjectionUnsupportedVertex",
            "MissingSupport",
        ):
            self.assertEqual(result["objects"][object_name]["feature"], "part_project_on_surface")

    def test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance(self) -> None:
        result = self.run_recompute("part-ruled-surface-wire-wire", "c4m1")
        ruled = result["objects"]["RuledSurface"]
        named_shape = result["named_shapes"]["RuledSurface"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ruled["status"], "ok")
        self.assertEqual(ruled["feature"], "part_ruled_surface")
        self.assertEqual(ruled["shape"], "occt_shell")
        self.assertEqual(ruled["source_curve1"], "LowerWire")
        self.assertEqual(ruled["source_curve2"], "UpperWire")
        self.assertEqual(ruled["orientation"], "Automatic")
        self.assertGreaterEqual(sum(name.startswith("Face") for name in result["subshapes"]["RuledSurface"]), 1)
        self.assert_ruled_surface_source_edge(result, "RuledSurface", "LowerWire.Edge1")
        self.assert_ruled_surface_source_edge(result, "RuledSurface", "UpperWire.Edge1")
        self.assertIn("part_ruled_surface:wire_wire_brepfill_shell", named_shape["element_history_status"])
        self.assert_object_matches_expected(result, "c4m1", "part-ruled-surface-wire-wire")

    def test_p8_app_link_proxies_linked_shape_with_link_placement(self) -> None:
        result = self.run_recompute("app-link-box", "p8")
        link = result["objects"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["link_transform"], False)
        self.assert_object_matches_expected(result, "p8", "app-link-box")

        transformed_result = self.run_recompute("app-link-box-transform", "p8")
        transformed = transformed_result["objects"]["BoxLink"]
        self.assertEqual(transformed["link_transform"], True)
        self.assert_object_matches_expected(transformed_result, "p8", "app-link-box-transform")

        scaled_result = self.run_recompute("app-link-box-scale", "p8")
        scaled = scaled_result["objects"]["BoxLink"]
        self.assert_object_matches_expected(scaled_result, "p8", "app-link-box-scale")

    def test_p8_app_link_scale_vector_overrides_scalar_scale(self) -> None:
        result = self.run_recompute("app-link-box-scale-vector", "p8")
        link = result["objects"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["bbox"]["min"], [5.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [9.0, 3.0, 2.0])
        self.assertAlmostEqual(link["volume"], 24.0)

    def test_p8_app_link_accepts_placement_alias(self) -> None:
        result = self.run_recompute("app-link-box-placement-alias", "p8")
        link = result["objects"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["bbox"]["min"], [5.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [7.0, 3.0, 4.0])

    def test_p8_app_link_subshape_uses_linked_object_sublist(self) -> None:
        result = self.run_recompute("app-link-box-face", "p8")
        link = result["objects"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["link_transform"], False)
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-box-face")

    def test_p8_app_link_subshape_compounds_multiple_sublist_items(self) -> None:
        result = self.run_recompute("app-link-box-multi-face", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_compound")
        self.assert_object_matches_expected(result, "p8", "app-link-box-multi-face")

    def test_p8_app_link_resolves_label_qualified_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-label-qualified-sublist", "p8")
        link = result["objects"]["BoxLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-label-qualified-sublist")

    def test_c3m2_app_link_rewrites_stale_label_qualified_subshape_alias(self) -> None:
        result = self.run_recompute("label-rename-recovery", "c3m2")
        link = result["objects"]["BoxLink"]
        update = result["elementReferenceUpdates"][0]
        rename = update["labelReferenceRename"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(update["object"], "BoxLink")
        self.assertEqual(update["property"], "LinkedObject")
        self.assertEqual(update["PropertyType"], "App::PropertyXLinkSub")
        self.assertEqual(update["value"], "Box")
        self.assertEqual(update["SubList"], ["$PrettyBox.Face1"])
        self.assertEqual(rename, {
            "index": 0,
            "oldLabel": "OldPrettyBox",
            "newLabel": "PrettyBox",
            "oldSubname": "$OldPrettyBox.Face1",
            "newSubname": "$PrettyBox.Face1",
            "method": "PropertyLinkBase.updateLabelReference",
        })
        self.assert_object_matches_expected(result, "c3m2", "label-rename-recovery")

    def test_c3m2_app_link_reports_duplicate_label_rename_ambiguity(self) -> None:
        result = self.run_recompute("label-rename-duplicate-target-label", "c3m2")
        diagnostic = next(
            item
            for item in result["diagnostics"]
            if item["code"] == "label_reference_ambiguous"
        )

        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(diagnostic["object"], "BoxLink")
        self.assertEqual(diagnostic["property"], "LinkedObject")
        self.assertEqual(diagnostic["target"], "Box")
        self.assertEqual(diagnostic["subname"], "$OldPrettyBox.Face1")
        self.assertIn("current target Label is not unique", diagnostic["message"])

    def test_c3m2_app_link_rewrites_nested_label_qualified_subshape_alias(self) -> None:
        result = self.run_recompute("nested-label-rename-recovery", "c3m2")
        link = result["objects"]["NestedPlainGroupFaceLink"]
        update = result["elementReferenceUpdates"][0]
        rename = update["labelReferenceRename"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "GroupLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(link["bbox"]["min"], [13.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [13.0, 1.0, 1.0])
        self.assertEqual(update["object"], "NestedPlainGroupFaceLink")
        self.assertEqual(update["property"], "LinkedObject")
        self.assertEqual(update["PropertyType"], "App::PropertyXLinkSub")
        self.assertEqual(update["value"], "GroupLink")
        self.assertEqual(update["SubList"], ["$PrettySub.$PrettyB.Face1"])
        self.assertEqual(rename, {
            "index": 0,
            "oldLabel": "OldPrettySub",
            "newLabel": "PrettySub",
            "oldSubname": "$OldPrettySub.$PrettyB.Face1",
            "newSubname": "$PrettySub.$PrettyB.Face1",
            "method": "PropertyLinkBase.updateLabelReference",
        })

    def test_c3m2_app_link_rewrites_cross_document_nested_label_alias(self) -> None:
        result = self.run_recompute("cross-document-nested-label-rename-recovery", "c3m2")
        link = result["objects"]["NestedExternalGroupFaceLink"]
        named_shape = result["named_shapes"]["NestedExternalGroupFaceLink"]
        update = result["elementReferenceUpdates"][0]
        rename = update["labelReferenceRename"][0]
        document_reference = update["documentReference"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(len(result["elementReferenceUpdates"]), 1)
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "GroupLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(link["bbox"]["min"], [13.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [13.0, 1.0, 1.0])
        self.assertEqual(update["object"], "NestedExternalGroupFaceLink")
        self.assertEqual(update["property"], "LinkedObject")
        self.assertEqual(update["PropertyType"], "App::PropertyXLinkSub")
        self.assertEqual(update["value"], "GroupLink")
        self.assertEqual(update["SubList"], ["$PrettySub.$PrettyB.Face1"])
        self.assertEqual(update["FullSubList"], ["ExternalDocRestored#$PrettySub.$PrettyB.Face1"])
        self.assertEqual(rename, {
            "index": 0,
            "oldLabel": "OldPrettySub",
            "newLabel": "PrettySub",
            "oldSubname": "$OldPrettySub.$PrettyB.Face1",
            "newSubname": "$PrettySub.$PrettyB.Face1",
            "method": "PropertyLinkBase.updateLabelReference",
        })
        self.assertEqual(document_reference["method"], "PropertyXLinkContainer.DocMap")
        self.assertEqual(document_reference["file"], "external.FCStd")
        self.assertEqual(document_reference["oldName"], "ExternalDoc")
        self.assertEqual(document_reference["newName"], "ExternalDocRestored")
        self.assertEqual(document_reference["oldLabel"], "External Assembly")
        self.assertEqual(document_reference["newLabel"], "External Assembly Restored")
        self.assertEqual(
            named_shape["element_map"]["ExternalDocRestored#$PrettySub.$PrettyB.Face1"],
            "Face1",
        )
        self.assertEqual(
            named_shape["element_map"]["Face1;:X;ExternalDocRestored#$PrettySub.$PrettyB.Face1"],
            "Face1",
        )

    def test_c3m2_xlink_rewrites_mapped_postfix_after_document_rename(self) -> None:
        result = self.run_recompute("xlink-mapped-postfix-rename-recovery", "c3m2")
        link = result["objects"]["ExternalFaceLink"]
        named_shape = result["named_shapes"]["ExternalFaceLink"]
        update = result["elementReferenceUpdates"][0]
        document_reference = update["documentReference"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(update["object"], "ExternalFaceLink")
        self.assertEqual(update["property"], "LinkedObject")
        self.assertEqual(update["PropertyType"], "App::PropertyXLinkSub")
        self.assertEqual(update["value"], "Box")
        self.assertEqual(update["SubList"], ["Face1"])
        self.assertEqual(update["StableSubList"], ["Face1"])
        self.assertEqual(update["FullSubList"], ["ExternalDocRestored#Box.Face1"])
        self.assertEqual(document_reference["method"], "PropertyXLinkContainer.DocMap")
        self.assertEqual(document_reference["file"], "external.FCStd")
        self.assertEqual(document_reference["oldName"], "ExternalDoc")
        self.assertEqual(document_reference["newName"], "ExternalDocRestored")
        self.assertEqual(document_reference["oldLabel"], "External Assembly")
        self.assertEqual(document_reference["newLabel"], "External Assembly Restored")
        self.assertEqual(named_shape["element_map"]["ExternalDocRestored#Box.Face1"], "Face1")
        self.assertEqual(
            named_shape["element_map"]["Face1;:X;ExternalDocRestored#Box.Face1"],
            "Face1",
        )
        self.assertIn(
            "Face1;:X;ExternalDocRestored#Box.Face1",
            named_shape["elements"]["Face1"]["sources"],
        )

    def test_c3m2_xlink_document_hash_mismatch_reports_doc_reference_update(self) -> None:
        result = self.run_recompute("xlink-document-hash-mismatch", "c3m2")
        link = result["objects"]["ExternalFaceLink"]
        diagnostic = result["diagnostics"][0]
        update = result["elementReferenceUpdates"][0]
        document_reference = update["documentReference"]

        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(diagnostic["code"], "document_hash_mismatch")
        self.assertEqual(diagnostic["severity"], "warning")
        self.assertEqual(diagnostic["object"], "ExternalFaceLink")
        self.assertEqual(diagnostic["property"], "LinkedObject")
        self.assertEqual(diagnostic["target"], "Box")
        self.assertEqual(update["object"], "ExternalFaceLink")
        self.assertEqual(update["property"], "LinkedObject")
        self.assertEqual(update["PropertyType"], "App::PropertyXLinkSub")
        self.assertEqual(update["value"], "Box")
        self.assertEqual(update["SubList"], ["Face1"])
        self.assertEqual(document_reference["method"], "PropertyXLinkContainer.DocMap")
        self.assertEqual(document_reference["file"], "external.FCStd")
        self.assertEqual(document_reference["oldName"], "ExternalDoc")
        self.assertEqual(document_reference["newName"], "ExternalDocRestored")
        self.assertEqual(document_reference["oldLabel"], "External Assembly")
        self.assertEqual(document_reference["newLabel"], "External Assembly Restored")
        self.assertEqual(document_reference["oldStamp"], "2026-01-01T00:00:00Z")
        self.assertEqual(document_reference["currentStamp"], "2026-02-01T00:00:00Z")

    def test_c3m2_xlink_missing_external_document_reports_graph_diagnostic(self) -> None:
        result = self.run_recompute("xlink-missing-external-document", "c3m2")
        link = result["objects"]["ExternalFaceLink"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual(link["status"], "error")
        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual([item["code"] for item in result["diagnostics"]], ["missing_external_document"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "ExternalFaceLink")
        self.assertEqual(diagnostic["property"], "LinkedObject")
        self.assertEqual(diagnostic["stage"], "graph")
        self.assertEqual(diagnostic["target"], "ExternalBox")
        self.assertEqual(diagnostic["subname"], "Face1")
        self.assertIn("missing.FCStd", diagnostic["message"])
        self.assertNotIn("missing_link_target", [item["code"] for item in result["diagnostics"]])

    def test_c3m2_xlink_pending_external_document_reports_reload_diagnostic(self) -> None:
        result = self.run_recompute("xlink-pending-external-document", "c3m2")
        link = result["objects"]["ExternalFaceLink"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual(link["status"], "error")
        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual([item["code"] for item in result["diagnostics"]], ["external_document_pending_reload"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "ExternalFaceLink")
        self.assertEqual(diagnostic["property"], "LinkedObject")
        self.assertEqual(diagnostic["stage"], "graph")
        self.assertEqual(diagnostic["target"], "ExternalBox")
        self.assertEqual(diagnostic["subname"], "Face1")
        self.assertIn("external.FCStd", diagnostic["message"])
        self.assertIn("pending reload", diagnostic["message"])
        self.assertIn("partial load allowed", diagnostic["message"])

    def test_c3m2_xlink_unloaded_external_document_reports_detached_diagnostic(self) -> None:
        result = self.run_recompute("xlink-unloaded-external-document", "c3m2")
        link = result["objects"]["ExternalFaceLink"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual(link["status"], "error")
        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual([item["code"] for item in result["diagnostics"]], ["external_document_unloaded"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "ExternalFaceLink")
        self.assertEqual(diagnostic["property"], "LinkedObject")
        self.assertEqual(diagnostic["stage"], "graph")
        self.assertEqual(diagnostic["target"], "ExternalBox")
        self.assertEqual(diagnostic["subname"], "Face1")
        self.assertIn("external.FCStd", diagnostic["message"])
        self.assertIn("unloaded or deleted", diagnostic["message"])

    def test_c3m2_xlink_pending_external_document_restored_by_request_graph(self) -> None:
        result = self.run_recompute("xlink-pending-external-document-restored", "c3m2")
        link = result["objects"]["ExternalFaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["elementReferenceUpdates"], [])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ExternalBox")
        self.assertEqual(link["shape"], "occt_face")

    def test_c3m2_xlink_source_object_rename_rewrites_update_target(self) -> None:
        result = self.run_recompute("source-object-rename-recovery", "c3m2")
        update = result["elementReferenceUpdates"][0]
        shadow = update["ReferenceShadow"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(result["objects"]["ProbePad"]["status"], "ok")
        self.assertEqual(update["object"], "ProbePad")
        self.assertEqual(update["property"], "UpToFace")
        self.assertEqual(update["PropertyType"], "App::PropertyLinkSub")
        self.assertEqual(update["value"], "RenamedBody")
        self.assertEqual(update["SubList"], ["Face5"])
        self.assertEqual(update["StableSubList"], ["Pad.Face6"])
        self.assertEqual(update["sourceObjectRename"], {
            "oldName": "Body",
            "newName": "RenamedBody",
            "method": "ReferenceShadow.targetId",
        })
        self.assertEqual(shadow["target"], "RenamedBody")
        self.assertEqual(shadow["reference_recovery"], "source_object_rename")

    def test_p8_app_link_preserves_full_sublist_external_mapped_alias(self) -> None:
        result = self.run_recompute("app-link-full-sublist-external-tag", "p8")
        link = result["objects"]["FaceLink"]
        named_shape = result["named_shapes"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(named_shape["element_map"]["ExternalDoc#Box.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["Face1;:X;ExternalDoc#Box.Face1"], "Face1")
        self.assertIn("Face1;:X;ExternalDoc#Box.Face1", named_shape["elements"]["Face1"]["sources"])
        self.assert_object_matches_expected(result, "p8", "app-link-full-sublist-external-tag")

    def test_p8_app_link_consumes_imported_element_maps_through_link_chain(self) -> None:
        result = self.run_recompute("app-link-imported-element-map-chain", "p8")

        self.assertEqual(result["diagnostics"], [])
        for object_name, linked_object, shape_label, element_name, stable_alias, chain_alias in [
            (
                "BrepEdgeLink",
                "BrepLink",
                "occt_edge",
                "Edge1",
                "ImportedCylinder.Edge1",
                "BrepLink.ImportedCylinder.Edge1",
            ),
            (
                "StepFaceLink",
                "StepLink",
                "occt_face",
                "Face1",
                "ImportedStep.Face1",
                "StepLink.ImportedStep.Face1",
            ),
            (
                "IgesFaceLink",
                "IgesLink",
                "occt_face",
                "Face1",
                "ImportedIges.Face1",
                "IgesLink.ImportedIges.Face1",
            ),
        ]:
            with self.subTest(object_name=object_name):
                link = result["objects"][object_name]
                named_shape = result["named_shapes"][object_name]

                self.assertEqual(link["status"], "ok")
                self.assertEqual(link["link"], "app_link")
                self.assertEqual(link["linked_object"], linked_object)
                self.assertEqual(link["shape"], shape_label)
                self.assertEqual(named_shape["element_map"][stable_alias], element_name)
                self.assertEqual(named_shape["element_map"][chain_alias], element_name)
                self.assertIn(stable_alias, named_shape["elements"][element_name]["sources"])
                self.assertIn(chain_alias, named_shape["elements"][element_name]["sources"])
                self.assertIn("history_consumed:merge", named_shape["element_history_status"])

        group = result["objects"]["ImportedLinkGroup"]
        group_named_shape = result["named_shapes"]["ImportedLinkGroup"]
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["BrepEdgeLink", "StepFaceLink", "IgesFaceLink"])
        self.assertEqual(group["visible_elements"], ["BrepEdgeLink", "StepFaceLink", "IgesFaceLink"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual(group_named_shape["element_map"]["0.Edge1"], "Edge1")
        self.assertEqual(group_named_shape["element_map"]["1.Face1"], "Face1")
        self.assertEqual(group_named_shape["element_map"]["2.Face1"], "Face2")
        self.assertIn("BrepLink.ImportedCylinder.Edge1", group_named_shape["elements"]["Edge1"]["sources"])
        self.assertIn("ImportedStep.Face1", group_named_shape["elements"]["Face1"]["sources"])
        self.assertIn("ImportedIges.Face1", group_named_shape["elements"]["Face2"]["sources"])
        self.assertIn(
            "element_map_child_map:preserve_source_ranges",
            group_named_shape["element_history_status"],
        )

    def test_p8_app_link_element_proxies_linked_shape(self) -> None:
        result = self.run_recompute("app-link-element-box", "p8")
        element = result["objects"]["BoxElement"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["link"], "app_link_element")
        self.assertEqual(element["linked_object"], "Box")
        self.assertEqual(element["link_transform"], False)
        self.assert_object_matches_expected(result, "p8", "app-link-element-box")

    def test_p8_app_link_group_compounds_element_shapes(self) -> None:
        result = self.run_recompute("app-link-group-elements", "p8")
        group = result["objects"]["LinkGroup"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["visible_elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assert_object_matches_expected(result, "p8", "app-link-group-elements")

    def test_p8_app_link_expands_linked_plain_group_children(self) -> None:
        result = self.run_recompute("app-link-linked-plain-group", "p8")
        plain_group = result["objects"]["PlainGroup"]
        link = result["objects"]["GroupLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(plain_group["status"], "ok")
        self.assertEqual(plain_group["container"], "document_object_group")
        self.assertEqual(plain_group["group"], ["BoxA", "BoxB"])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link_group")
        self.assertEqual(link["linked_object"], "PlainGroup")
        self.assertEqual(link["linked_plain_group"], True)
        self.assertEqual(link["elements"], ["BoxA", "BoxB"])
        self.assertEqual(link["visible_elements"], ["BoxA", "BoxB"])
        self.assertEqual(link["shape"], "occt_compound")
        self.assertEqual(link["bbox"]["min"], [10.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [15.0, 1.0, 1.0])

    def test_p8_app_link_resolves_linked_plain_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-linked-plain-group-label-subshape", "p8")
        link = result["objects"]["PlainGroupFaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "GroupLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(link["bbox"]["min"], [13.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [13.0, 1.0, 1.0])
        self.assert_object_matches_expected(result, "p8", "app-link-linked-plain-group-label-subshape")

    def test_p8_app_link_resolves_nested_plain_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-linked-plain-group-nested-label-subshape", "p8")
        group = result["objects"]["GroupLink"]
        link = result["objects"]["NestedPlainGroupFaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["elements"], ["BoxA", "SubGroup", "BoxB"])
        self.assertEqual(group["visible_elements"], ["BoxA", "BoxB"])
        self.assertEqual(group["bbox"]["min"], [10.0, 0.0, 0.0])
        self.assertEqual(group["bbox"]["max"], [15.0, 1.0, 1.0])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "GroupLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(link["bbox"]["min"], [13.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [13.0, 1.0, 1.0])
        self.assert_object_matches_expected(result, "p8", "app-link-linked-plain-group-nested-label-subshape")

    def test_p8_app_link_group_resolves_element_list_nested_plain_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-group-element-list-nested-plain-group-label-subshape", "p8")
        group = result["objects"]["LinkGroup"]
        link = result["objects"]["NestedElementListFaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["BoxA", "SubGroup", "BoxB"])
        self.assertEqual(group["visible_elements"], ["BoxA", "BoxB"])
        self.assertEqual(group["bbox"]["min"], [10.0, 0.0, 0.0])
        self.assertEqual(group["bbox"]["max"], [15.0, 1.0, 1.0])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["link_transform"], True)
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(link["bbox"]["min"], [13.0, 0.0, 0.0])
        self.assertEqual(link["bbox"]["max"], [13.0, 1.0, 1.0])
        self.assert_object_matches_expected(
            result,
            "p8",
            "app-link-group-element-list-nested-plain-group-label-subshape",
        )

    def test_p8_app_link_group_accepts_xlink_list_subset_elements(self) -> None:
        result = self.run_recompute("app-link-group-xlink-list-subset-elements", "p8")
        group = result["objects"]["LinkGroup"]
        named_shape = result["named_shapes"]["LinkGroup"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["visible_elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual(named_shape["element_map"]["0.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["1.Face1"], "Face7")

    def test_p8_app_link_resolves_group_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-group-subshape-alias", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-group-subshape-alias")

    def test_p8_app_link_resolves_group_index_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-sublist-index", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-sublist-index")

    def test_p8_app_link_resolves_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-sublist-label", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-sublist-label")

    def test_p8_app_link_resolves_element_list_child_target_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-nested-label-sublist", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-nested-label-sublist")

    def test_p8_app_link_resolves_hidden_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-hidden-sublist-label", "p8")
        group = result["objects"]["LinkGroup"]
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["visible_elements"], ["LinkA"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-hidden-sublist-label")

    def test_p8_app_link_resolves_object_qualified_nested_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-nested-object-qualified-sublist", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "BoxLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-nested-object-qualified-sublist")

    def test_p8_app_link_resolves_multilevel_label_qualified_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-multilevel-label-qualified-sublist", "p8")
        link = result["objects"]["FaceLink"]
        named_shape = result["named_shapes"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "ChainLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(named_shape["element_map"]["$PrettyChain.$PrettyBoxLink.$PrettyBox.Face1"], "Face1")
        self.assertEqual(named_shape["element_map"]["ChainLink.BoxLink.Box.Face1"], "Face1")
        self.assertIn("$PrettyChain.$PrettyBoxLink.$PrettyBox.Face1", named_shape["elements"]["Face1"]["sources"])
        self.assert_object_matches_expected(result, "p8", "app-link-multilevel-label-qualified-sublist")

    def test_p8_app_link_group_respects_visibility_list(self) -> None:
        result = self.run_recompute("app-link-group-visibility", "p8")
        group = result["objects"]["LinkGroup"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["LinkA", "LinkB"])
        self.assertEqual(group["visible_elements"], ["LinkA"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assert_object_matches_expected(result, "p8", "app-link-group-visibility")

    def test_p8_app_link_element_count_compounds_collapsed_elements(self) -> None:
        result = self.run_recompute("app-link-element-count-collapsed", "p8")
        group = result["objects"]["ArrayLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["linked_object"], "Box")
        self.assertEqual(group["element_count"], 3)
        self.assertEqual(group["collapsed_elements"], True)
        self.assertEqual(group["visible_indices"], [0, 1])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-collapsed")

    def test_p8_app_link_element_count_reports_owner_list_sync(self) -> None:
        result = self.run_recompute("app-link-element-count-owner-list-sync", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["element_count"], 3)
        self.assertEqual(group["collapsed_elements"], True)
        self.assertEqual(group["visible_indices"], [0, 1])
        self.assertEqual([item["action"] for item in updates], ["update"])
        self.assertEqual(updates[0]["reason"], "element_count_owner_lists_sync")
        self.assertEqual(updates[0]["object"], "ArrayLink")
        properties = updates[0]["properties"]
        self.assertEqual(properties["PlacementList"]["PropertyType"], "App::PropertyPlacementList")
        self.assert_update_property_type(updates[0], "ScaleList", "App::PropertyVectorList")
        self.assert_update_property_type(updates[0], "VisibilityList", "App::PropertyBoolList")
        self.assertEqual(properties["PlacementList"]["value"][0]["Base"], [0, 0, 0])
        self.assertEqual(properties["PlacementList"]["value"][1]["Base"], [1, 0, 0])
        self.assertEqual(properties["PlacementList"]["value"][2]["Base"], [2, 0, 0])
        self.assertEqual(properties["ScaleList"]["value"], [[1, 1, 1], [2, 1, 1], [3, 1, 1]])
        self.assertEqual(properties["VisibilityList"]["value"], [True, True, False])
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-element-count-owner-list-sync",
            "p8",
            updates,
        )

    def test_p8_app_link_show_element_toggle_off_preserves_child_lists(self) -> None:
        result = self.run_recompute("app-link-show-element-toggle-off-sync", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["element_count"], 2)
        self.assertEqual(group["collapsed_elements"], True)
        self.assertEqual(group["visible_indices"], [0, 1])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual([item["action"] for item in updates], ["update", "delete", "delete"])
        self.assertEqual(updates[0]["reason"], "show_element_toggle_off_owner_sync")
        self.assertEqual(updates[0]["object"], "ArrayLink")
        properties = updates[0]["properties"]
        self.assert_update_property_type(updates[0], "ElementList", "App::PropertyLinkList")
        self.assert_update_property_type(updates[0], "PlacementList", "App::PropertyPlacementList")
        self.assert_update_property_type(updates[0], "ScaleList", "App::PropertyVectorList")
        self.assertEqual(properties["ElementList"]["values"], [])
        self.assertEqual(properties["PlacementList"]["value"][0]["Base"], [0, 0, 0])
        self.assertEqual(properties["PlacementList"]["value"][1]["Base"], [5, 0, 0])
        self.assertEqual(properties["ScaleList"]["value"], [[1, 1, 1], [2, 2, 2]])
        self.assertEqual([item["reason"] for item in updates[1:]], ["show_element_toggle_off_child", "show_element_toggle_off_child"])
        self.assertEqual([item["object"] for item in updates[1:]], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-toggle-off-sync",
            "p8",
            updates,
        )

    def test_p8_app_link_show_element_syncs_explicit_element_list_owner(self) -> None:
        result = self.run_recompute("app-link-show-element-element-list-owner-sync", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual([item["action"] for item in updates], ["update"])
        self.assertEqual(updates[0]["reason"], "show_element_element_list_owner_sync")
        self.assertEqual(updates[0]["object"], "ArrayLink")
        properties = updates[0]["properties"]
        self.assert_update_property_type(updates[0], "ElementCount", "App::PropertyInteger")
        self.assert_update_property_type(updates[0], "VisibilityList", "App::PropertyBoolList")
        self.assertEqual(properties["ElementCount"]["value"], 2)
        self.assertEqual(properties["VisibilityList"]["value"], [False, True])
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-element-list-owner-sync",
            "p8",
            updates,
        )

    def test_p8_app_link_show_element_syncs_explicit_element_list_children(self) -> None:
        result = self.run_recompute("app-link-show-element-element-list-child-sync", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual([item["action"] for item in updates], ["claim", "update"])
        self.assertEqual([item["reason"] for item in updates], [
            "show_element_element_list_child_sync",
            "show_element_element_list_child_sync",
        ])
        self.assertEqual([item["object"] for item in updates], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assert_update_property_type(updates[0], "_LinkOwner", "App::PropertyInteger")
        self.assert_update_property_type(updates[0], "LinkedObject", "App::PropertyXLink")
        self.assert_update_property_type(updates[0], "LinkTransform", "App::PropertyBool")
        self.assert_update_property_type(updates[1], "_LinkOwner", "App::PropertyInteger")
        self.assert_update_property_type(updates[1], "LinkedObject", "App::PropertyXLink")
        self.assert_update_property_type(updates[1], "LinkTransform", "App::PropertyBool")
        self.assertEqual(updates[0]["properties"]["_LinkOwner"]["value"], 2)
        self.assertEqual(updates[1]["properties"]["LinkTransform"]["value"], False)
        self.assertEqual(updates[1]["properties"]["LinkedObject"]["value"], "Box")
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-element-list-child-sync",
            "p8",
            updates,
        )

    def test_p8_app_link_explicit_element_list_preserves_owned_copy_child_target(self) -> None:
        result = self.run_recompute("app-link-show-element-element-list-copy-on-change-owned-child", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["elements"], ["ArrayLink_i0"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0"])
        self.assertEqual(group["bbox"]["max"], [1.0, 1.0, 1.0])
        self.assertEqual([item["action"] for item in updates], ["update"])
        self.assertEqual(updates[0]["reason"], "show_element_element_list_child_sync")
        self.assertEqual(updates[0]["object"], "ArrayLink_i0")
        properties = updates[0]["properties"]
        self.assert_update_property_type(updates[0], "_LinkOwner", "App::PropertyInteger")
        self.assert_update_property_type(updates[0], "LinkTransform", "App::PropertyBool")
        self.assertEqual(properties["_LinkOwner"]["value"], 2)
        self.assertEqual(properties["LinkTransform"]["value"], False)
        self.assertNotIn("LinkedObject", properties)
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-element-list-copy-on-change-owned-child",
            "p8",
            updates,
        )

    def test_c3m6_app_link_copy_on_change_builds_deep_copy_lifecycle_updates(self) -> None:
        result = self.run_recompute("app-link-copy-on-change-deep-copy", "c3m6")
        link = result["objects"]["BoxLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual([item["action"] for item in updates], ["create", "create", "update"])
        self.assertEqual([item["reason"] for item in updates], [
            "copy_on_change_group_sync",
            "copy_on_change_deep_copy_lifecycle",
            "copy_on_change_deep_copy_lifecycle",
        ])
        self.assertEqual(updates[0]["properties"]["Group"]["values"], ["BoxLink_CopyOnChangeObject"])
        self.assertEqual(updates[1]["object"], "BoxLink_CopyOnChangeObject")
        self.assertEqual(updates[1]["sourceObject"], "Box")
        self.assertEqual(updates[1]["properties"]["Length"], 2)
        self.assertEqual(updates[1]["properties"]["Width"], 3)
        self.assertEqual(updates[1]["properties"]["Height"], 4)
        self.assertEqual(updates[1]["properties"]["Visibility"]["value"], False)
        self.assertEqual(updates[1]["historyPreserve"]["propertyTree"], "deep_copy")
        properties = updates[2]["properties"]
        self.assertEqual(properties["LinkedObject"]["value"], "BoxLink_CopyOnChangeObject")
        self.assertEqual(properties["LinkCopyOnChange"]["value"], 2)
        self.assertEqual(properties["LinkCopyOnChangeSource"]["value"], "Box")
        self.assertEqual(properties["LinkCopyOnChangeGroup"]["value"], "BoxLink_CopyOnChangeGroup")
        self.assertEqual(properties["LinkCopyOnChangeTouched"]["value"], False)

        applied_result = self.run_with_document_updates_applied(
            "app-link-copy-on-change-deep-copy",
            "c3m6",
            updates,
        )
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])
        self.assertEqual(applied_result["objects"]["BoxLink_CopyOnChangeObject"]["status"], "ok")
        self.assertEqual(applied_result["objects"]["BoxLink"]["linked_object"], "BoxLink_CopyOnChangeObject")

    def test_c3m6_app_link_copy_on_change_copies_subtree_relinks_and_preserves_history(self) -> None:
        result = self.run_recompute("app-link-copy-on-change-subtree-relink-history", "c3m6")
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(
            [(update["action"], update["reason"], update["object"]) for update in updates],
            [
                ("create", "copy_on_change_group_sync", "GroupLink_CopyOnChangeGroup"),
                ("create", "copy_on_change_deep_copy_lifecycle", "GroupLink_CopyOnChange_ChildBox"),
                ("create", "copy_on_change_deep_copy_lifecycle", "GroupLink_CopyOnChange_SourceLink"),
                ("create", "copy_on_change_deep_copy_lifecycle", "GroupLink_CopyOnChangeObject"),
                ("update", "copy_on_change_deep_copy_lifecycle", "GroupLink"),
            ],
        )
        group_copy = updates[3]
        self.assertEqual(
            group_copy["properties"]["Group"]["values"],
            ["GroupLink_CopyOnChange_SourceLink", "ExternalBox"],
        )
        self.assertNotIn("ExternalBox", group_copy["dependencyRewrite"])

        source_link_copy = updates[2]
        linked_object = source_link_copy["properties"]["LinkedObject"]
        self.assertEqual(linked_object["value"], "GroupLink_CopyOnChange_ChildBox")
        self.assertEqual(linked_object["StableSubList"], ["GroupLink_CopyOnChange_ChildBox.Face1"])
        self.assertEqual(linked_object["FullSubList"], ["GroupLink_CopyOnChange_ChildBox.Face1"])
        self.assertEqual(linked_object["ReferenceShadow"][0]["target"], "GroupLink_CopyOnChange_ChildBox")
        self.assertEqual(linked_object["ReferenceShadow"][0]["targetId"], updates[1]["objectId"])
        self.assertEqual(source_link_copy["historyPreserve"]["referenceShadow"], "preserved_and_retargeted")

        applied_result = self.run_with_document_updates_applied(
            "app-link-copy-on-change-subtree-relink-history",
            "c3m6",
            updates,
        )
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])
        self.assertEqual(applied_result["objects"]["GroupLink"]["linked_object"], "GroupLink_CopyOnChangeObject")
        self.assertEqual(applied_result["objects"]["GroupLink_CopyOnChangeObject"]["group"], ["GroupLink_CopyOnChange_SourceLink", "ExternalBox"])

    def test_c3m6_app_link_copy_on_change_touched_tracking_resyncs_existing_copy(self) -> None:
        result = self.run_recompute("app-link-copy-on-change-touched-tracking", "c3m6")
        link = result["objects"]["BoxLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "BoxLink_CopyOnChangeObject")
        self.assertEqual(
            [(update["action"], update["reason"], update["object"]) for update in updates],
            [
                ("update", "copy_on_change_group_sync", "BoxLink_CopyOnChangeGroup"),
                ("update", "copy_on_change_deep_copy_lifecycle", "BoxLink_CopyOnChangeObject"),
                ("update", "copy_on_change_deep_copy_lifecycle", "BoxLink"),
            ],
        )
        self.assertEqual(updates[1]["sourceObject"], "Box")
        self.assertEqual(updates[1]["properties"]["Length"], 2)
        self.assertEqual(updates[2]["properties"]["LinkCopyOnChange"]["value"], 3)
        self.assertEqual(updates[2]["properties"]["LinkCopyOnChangeTouched"]["value"], False)
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-copy-on-change-touched-tracking",
            "c3m6",
            updates,
        )

    def test_p8_app_link_element_count_resolves_indexed_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-sublist-index", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-sublist-index")

    def test_p8_app_link_element_count_resolves_indexed_linked_target_prefix(self) -> None:
        result = self.run_recompute("app-link-element-count-sublist-target-prefix", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-sublist-target-prefix")

    def test_p8_app_link_element_count_resolves_target_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-sublist-target-label", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-sublist-target-label")

    def test_p8_app_link_element_count_resolves_hidden_indexed_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-hidden-sublist-index", "p8")
        group = result["objects"]["ArrayLink"]
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["visible_indices"], [0])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-hidden-sublist-index")

    def test_p8_app_link_preserves_terminal_stable_history(self) -> None:
        for fixture, code, stable_subname in [
            ("app-link-stable-history-split", "split_stable_subname", "Pad.Face5"),
            ("app-link-stable-history-deleted", "deleted_stable_subname", "Pocket.Face5"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                diagnostic = result["diagnostics"][0]
                history_kinds = {
                    item["kind"]
                    for item in result["named_shapes"]["BodyLink"]["history"]
                }

                self.assertEqual(diagnostic["code"], code)
                self.assertEqual(diagnostic["object"], "ProbePad")
                self.assertEqual(diagnostic["property"], "UpToFace")
                self.assertEqual(diagnostic["target"], "BodyLink")
                self.assertEqual(diagnostic["subname"], stable_subname)
                self.assertIn(code.removesuffix("_stable_subname"), history_kinds)

    def test_p8_app_link_preserves_merge_history_after_retag(self) -> None:
        result = self.run_recompute("app-link-body-merge-history-retag", "p8")
        link = result["objects"]["BodyLink"]
        named_shape = result["named_shapes"]["BodyLink"]
        target_element = named_shape["element_map"]["Body.SketchPocket.Edge1"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Body")
        self.assertTrue(
            any(
                item["kind"] == "merge"
                and item["element"] == target_element
                and item["sources"] == ["Pocket.Edge3", "SketchPocket.Edge1"]
                for item in named_shape["history"]
            )
        )
        self.assert_object_matches_expected(result, "p8", "app-link-body-merge-history-retag")

    def test_p8_app_link_show_element_groups_materialized_children(self) -> None:
        result = self.run_recompute("app-link-show-element-materialized", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

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
        self.assertEqual([item["action"] for item in updates], ["update", "update"])
        self.assertEqual([item["reason"] for item in updates], ["show_element_child_sync", "show_element_child_sync"])
        self.assertEqual([item["object"] for item in updates], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-materialized",
            "p8",
            updates,
        )
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-materialized")

    def test_p8_app_link_show_element_inherits_child_link_target(self) -> None:
        result = self.run_recompute("app-link-show-element-inherited-child", "p8")
        element = result["objects"]["ArrayLink_i0"]
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["link"], "app_link_element")
        self.assertEqual(element["linked_object"], "Box")
        self.assertEqual(element["inherited_linked_object"], True)
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual([item["action"] for item in updates], ["update"])
        self.assertEqual(updates[0]["reason"], "show_element_child_sync")
        self.assertEqual(updates[0]["object"], "ArrayLink_i0")
        self.assertEqual(updates[0]["properties"]["LinkedObject"]["value"], "Box")
        self.assertEqual(updates[0]["properties"]["LinkTransform"]["value"], False)
        self.assertEqual(updates[0]["properties"]["Scale"]["value"], 1.0)
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-inherited-child",
            "p8",
            updates,
        )
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-inherited-child")

    def test_p8_app_link_show_element_inherits_child_transform_lists(self) -> None:
        result = self.run_recompute("app-link-show-element-inherited-placement-list", "p8")
        element = result["objects"]["ArrayLink_i1"]
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["inherited_linked_object"], True)
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["visible_elements"], ["ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual([item["action"] for item in updates], ["update", "update"])
        self.assertEqual([item["reason"] for item in updates], ["show_element_child_sync", "show_element_child_sync"])
        self.assertEqual([item["object"] for item in updates], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(updates[1]["properties"]["Placement"]["Base"], [5, 0, 0])
        self.assertEqual(updates[1]["properties"]["Scale"]["value"], 2.0)
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-inherited-placement-list",
            "p8",
            updates,
        )
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-inherited-placement-list")

    def test_p8_app_link_show_element_synthesizes_missing_children(self) -> None:
        result = self.run_recompute("app-link-show-element-synthetic", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertNotIn("ArrayLink_i0", result["objects"])
        self.assertNotIn("ArrayLink_i1", result["objects"])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["link"], "app_link_group")
        self.assertEqual(group["element_count"], 2)
        self.assertEqual(group["materialized_elements"], False)
        self.assertEqual(group["synthetic_elements"], True)
        self.assertEqual(group["request_local_elements"], True)
        self.assertEqual(group["elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_compound")
        self.assertEqual([item["action"] for item in updates], ["create", "create"])
        self.assertEqual([item["object"] for item in updates], ["ArrayLink_i0", "ArrayLink_i1"])
        self.assertEqual(updates[0]["owner"], "ArrayLink")
        self.assertEqual(updates[0]["ownerId"], 2)
        self.assert_update_property_type(updates[0], "_LinkOwner", "App::PropertyInteger")
        self.assert_update_property_type(updates[0], "LinkedObject", "App::PropertyXLink")
        self.assert_update_property_type(updates[0], "LinkTransform", "App::PropertyBool")
        self.assert_update_property_type(updates[0], "Placement", "App::PropertyPlacement")
        self.assert_update_property_type(updates[0], "Scale", "App::PropertyFloat")
        self.assertEqual(updates[0]["properties"]["_LinkOwner"]["value"], 2)
        self.assertEqual(updates[0]["properties"]["LinkedObject"]["value"], "Box")
        self.assertEqual(updates[1]["properties"]["Placement"]["Base"], [5, 0, 0])
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-synthetic",
            "p8",
            updates,
        )
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-synthetic")

    def test_p8_app_link_show_element_reports_stale_child_delete(self) -> None:
        result = self.run_recompute("app-link-show-element-stale-child-lifecycle", "p8")
        group = result["objects"]["ArrayLink"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["elements"], ["ArrayLink_i0"])
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual(len(updates), 2)
        self.assertEqual(updates[0]["action"], "delete")
        self.assertEqual(updates[0]["reason"], "show_element_excess_child")
        self.assertEqual(updates[0]["object"], "ArrayLink_i1")
        self.assertEqual(updates[0]["owner"], "ArrayLink")
        self.assertEqual(updates[0]["ownerId"], 2)
        self.assertEqual(updates[0]["index"], 1)
        self.assertEqual(updates[1]["action"], "update")
        self.assertEqual(updates[1]["reason"], "show_element_child_sync")
        self.assertEqual(updates[1]["object"], "ArrayLink_i0")
        self.assert_document_updates_apply_to_stable_graph(
            "app-link-show-element-stale-child-lifecycle",
            "p8",
            updates,
        )

    def test_p8_app_link_resolves_show_element_index_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-show-element-index-sublist", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-index-sublist")

    def test_p8_app_link_resolves_show_element_index_linked_target_prefix(self) -> None:
        result = self.run_recompute("app-link-show-element-index-target-prefix", "p8")
        link = result["objects"]["FaceLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-index-target-prefix")

    def test_p8_assembly_object_groups_basic_component_link(self) -> None:
        result = self.run_recompute("assembly-link-basic", "p8")
        component = result["objects"]["ComponentLink"]
        assembly = result["objects"]["Assembly"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(component["status"], "ok")
        self.assertEqual(component["link"], "assembly_link")
        self.assertEqual(component["linked_object"], "Box")
        self.assertEqual(component["rigid"], True)

        self.assertEqual(assembly["status"], "ok")
        self.assertEqual(assembly["assembly"], "object")
        self.assertEqual(assembly["group"], ["ComponentLink"])
        self.assertEqual(assembly["solve"], "skipped_no_joints")
        self.assertEqual(assembly["solver_adapter"]["status"], "skipped")
        self.assertEqual(assembly["solver_adapter"]["reason"], "no_joints")
        self.assert_object_matches_expected(result, "p8", "assembly-link-basic")

    def test_p8_assembly_grounded_only_solver_adapter_succeeds_noop(self) -> None:
        result = self.run_recompute("assembly-grounded-only-solver-success", "p8")
        assembly = result["objects"]["Assembly"]
        joint_group = result["objects"]["Joints"]
        grounded = result["objects"]["GroundedJoint"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(grounded["status"], "ok")
        self.assertEqual(grounded["assembly"], "grounded_joint")
        self.assertEqual(grounded["object_to_ground"], "ComponentLink")
        self.assertEqual(grounded["solve"], "grounded_input")
        self.assertEqual(joint_group["status"], "ok")
        self.assertEqual(joint_group["solve"], "solver_inputs")
        self.assertEqual(joint_group["joints"], ["GroundedJoint"])
        self.assertEqual(assembly["status"], "ok")
        self.assertEqual(assembly["assembly"], "object")
        self.assertEqual(assembly["joint_groups"], ["Joints"])
        self.assertEqual(assembly["joints"], ["GroundedJoint"])
        self.assertEqual(assembly["solve"], "solved_noop")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "grounded_only_noop")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJoint"])

    def test_p8_assembly_joint_group_reports_solver_inputs_and_placement_writeback(self) -> None:
        result = self.run_recompute("assembly-joint-group-diagnostics", "p8")
        assembly = result["objects"]["Assembly"]
        joint_group = result["objects"]["Joints"]
        grounded = result["objects"]["GroundedJoint"]
        fixed = result["objects"]["FixedJoint"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(grounded["status"], "ok")
        self.assertEqual(grounded["assembly"], "grounded_joint")
        self.assertEqual(grounded["object_to_ground"], "ComponentA")
        self.assertEqual(grounded["solve"], "grounded_input")

        self.assertEqual(fixed["status"], "ok")
        self.assertEqual(fixed["assembly"], "joint")
        self.assertEqual(fixed["joint_type"], "Fixed")
        self.assertEqual(fixed["reference1"]["object"], "ComponentA")
        self.assertEqual(fixed["reference2"]["object"], "ComponentB")
        self.assertEqual(fixed["solve"], "joint_input")

        self.assertEqual(joint_group["status"], "ok")
        self.assertEqual(joint_group["assembly"], "joint_group")
        self.assertEqual(joint_group["joints"], ["GroundedJoint", "FixedJoint"])
        self.assertEqual(joint_group["solve"], "solver_inputs")

        self.assertEqual(assembly["status"], "ok")
        self.assertEqual(assembly["assembly"], "object")
        self.assertEqual(assembly["joint_groups"], ["Joints"])
        self.assertEqual(assembly["joints"], ["GroundedJoint", "FixedJoint"])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJoint"])
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["action"], "assembly_set_placement")
        self.assertEqual(updates[0]["reason"], "assembly_solver_placement_writeback")
        self.assertEqual(updates[0]["joint"], "OndselSolver")
        self.assertEqual(updates[0]["joint_type"], "solver_result")
        self.assertEqual(updates[0]["object"], "ComponentB")
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [0.0, 0.0, 0.0])
        self.assert_object_matches_expected(result, "p8", "assembly-joint-group-diagnostics")

    def assert_c3m6_grounded_joint_uses_real_ondsel_solver(
        self,
        fixture: str,
        joint_name: str,
        joint_type: str,
        expected_update_base: list[float] | None,
        solver_scalars: dict[str, object] | None = None,
        joint_reference_objects: tuple[str, str] = ("ComponentA", "ComponentB"),
        expected_solver_joints: list[str] | None = None,
    ) -> None:
        result = self.run_recompute(fixture, "c3m6")
        assembly = result["objects"]["Assembly"]
        joint = result["objects"][joint_name]
        updates = result["documentObjectUpdates"]
        expected_solver_joints = expected_solver_joints or [joint_name]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(joint["status"], "ok")
        self.assertEqual(joint["assembly"], "joint")
        self.assertEqual(joint["joint_type"], joint_type)
        self.assertEqual(joint["reference1"]["object"], joint_reference_objects[0])
        self.assertEqual(joint["reference2"]["object"], joint_reference_objects[1])
        self.assertEqual(joint["solve"], "joint_input")

        self.assertEqual(assembly["status"], "ok")
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJoint"])
        self.assertEqual(assembly["solver_adapter"]["joints"], expected_solver_joints)
        solver_joints = {
            solver_joint["object"]: solver_joint
            for solver_joint in assembly["solver_adapter"]["solver_joints"]
        }
        solver_joint = solver_joints[joint_name]
        self.assertEqual(solver_joint["joint_type"], joint_type)
        if solver_scalars:
            for field, expected in solver_scalars.items():
                self.assertEqual(solver_joint[field], expected)
        self.assertEqual(assembly["solver_adapter"]["unsupported_joints"], [])
        if expected_update_base is None:
            self.assertEqual(updates, [])
            self.assert_object_matches_expected(result, "c3m6", fixture)
            return
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["action"], "assembly_set_placement")
        self.assertEqual(updates[0]["joint"], "OndselSolver")
        self.assertEqual(updates[0]["joint_type"], "solver_result")
        self.assertEqual(updates[0]["object"], "ComponentB")
        for actual, expected in zip(updates[0]["properties"]["Placement"]["Base"], expected_update_base):
            self.assertAlmostEqual(actual, expected, delta=1e-9)
        self.assert_object_matches_expected(result, "c3m6", fixture)

    def test_c3m6_assembly_grounded_ball_joint_uses_real_ondsel_solver(self) -> None:
        self.assert_c3m6_grounded_joint_uses_real_ondsel_solver(
            "assembly-grounded-ball-joint-real-solver",
            "BallJoint",
            "Ball",
            [0.0, 0.0, 0.0],
        )

    def test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver(self) -> None:
        cases = [
            (
                "assembly-grounded-revolute-joint-real-solver",
                "RevoluteJoint",
                "Revolute",
                [0.0, 0.0, 0.0],
                None,
            ),
            (
                "assembly-grounded-cylindrical-joint-real-solver",
                "CylindricalJoint",
                "Cylindrical",
                [0.0, 0.0, 0.0],
                None,
            ),
            (
                "assembly-grounded-slider-joint-real-solver",
                "SliderJoint",
                "Slider",
                [0.0, 0.0, 0.0],
                {"distance": 0.0},
            ),
            (
                "assembly-grounded-distance-joint-real-solver",
                "DistanceJoint",
                "Distance",
                [4.0, 0.0, 2.0],
                {"distance": 2.0},
            ),
            (
                "assembly-grounded-parallel-joint-real-solver",
                "ParallelJoint",
                "Parallel",
                None,
                None,
            ),
            (
                "assembly-grounded-perpendicular-joint-real-solver",
                "PerpendicularJoint",
                "Perpendicular",
                None,
                None,
            ),
            (
                "assembly-grounded-angle-joint-real-solver",
                "AngleJoint",
                "Angle",
                None,
                {"angle": 30.0},
            ),
            (
                "assembly-grounded-gears-joint-real-solver",
                "GearsJoint",
                "Gears",
                None,
                {"distance": 2.0, "distance2": 1.0, "radius_i": 2.0, "radius_j": 1.0},
            ),
            (
                "assembly-grounded-belt-joint-real-solver",
                "BeltJoint",
                "Belt",
                None,
                {"distance": 2.0, "distance2": 1.0, "radius_i": 2.0, "radius_j": -1.0},
            ),
            (
                "assembly-grounded-rackpinion-joint-real-solver",
                "RackPinionJoint",
                "RackPinion",
                [0.0, 0.0, 0.0],
                {"distance": 2.5, "pitch_radius": 2.5, "sliding_part_index": 2, "jcs_swapped_for_solver": True},
                ("ComponentB", "ComponentA"),
                ["SliderJoint", "RackPinionJoint"],
            ),
            (
                "assembly-grounded-screw-joint-real-solver",
                "ScrewJoint",
                "Screw",
                [0.0, 0.0, 0.0],
                {"distance": 1.25, "pitch": 1.25, "sliding_part_index": 2, "jcs_swapped_for_solver": True},
                ("ComponentB", "ComponentA"),
                ["SliderJoint", "ScrewJoint"],
            ),
        ]
        for fixture, joint_name, joint_type, expected_update_base, solver_scalars, *extra in cases:
            joint_reference_objects = extra[0] if len(extra) >= 1 else ("ComponentA", "ComponentB")
            expected_solver_joints = extra[1] if len(extra) >= 2 else None
            with self.subTest(fixture=fixture):
                self.assert_c3m6_grounded_joint_uses_real_ondsel_solver(
                    fixture,
                    joint_name,
                    joint_type,
                    expected_update_base,
                    solver_scalars,
                    joint_reference_objects,
                    expected_solver_joints,
                )

    def test_c3m6_assembly_distance_type_fixtures_match_native_expected(self) -> None:
        cases = [
            (
                "assembly-distance-point-point-nonzero-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "PointPoint",
                    "solver_joint_class": "ASMTSphSphJoint",
                    "distance_ij": 1.5,
                    "jcs_swapped_for_solver": False,
                },
            ),
            (
                "assembly-distance-point-point-zero-real-solver",
                {
                    "distance": 0.0,
                    "distance_type": "PointPoint",
                    "solver_joint_class": "ASMTSphericalJoint",
                    "jcs_swapped_for_solver": False,
                },
            ),
            (
                "assembly-distance-line-line-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "LineLine",
                    "solver_joint_class": "ASMTRevCylJoint",
                    "distance_ij": 1.5,
                    "jcs_swapped_for_solver": False,
                },
            ),
            (
                "assembly-distance-point-line-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "PointLine",
                    "solver_joint_class": "ASMTLineInPlaneJoint",
                    "offset": 1.5,
                    "jcs_swapped_for_solver": True,
                },
            ),
            (
                "assembly-distance-plane-plane-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "PlanePlane",
                    "solver_joint_class": "ASMTPlanarJoint",
                    "offset": 1.5,
                    "jcs_swapped_for_solver": False,
                },
            ),
            (
                "assembly-distance-point-plane-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "PointPlane",
                    "solver_joint_class": "ASMTPointInPlaneJoint",
                    "offset": 1.5,
                    "jcs_swapped_for_solver": True,
                },
            ),
            (
                "assembly-distance-line-plane-real-solver",
                {
                    "distance": 1.5,
                    "distance_type": "LinePlane",
                    "solver_joint_class": "ASMTLineInPlaneJoint",
                    "offset": 1.5,
                    "jcs_swapped_for_solver": True,
                },
            ),
        ]

        for fixture, solver_scalars in cases:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "c3m6")
                expected = self.expected_freecad("c3m6", fixture)
                assembly = result["objects"]["Assembly"]
                actual_joint = assembly["solver_adapter"]["solver_joints"][0]
                expected_joint = expected["solver_adapter"]["solver_joints"][0]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(assembly["solver_adapter"]["status"], "solved")
                self.assertEqual(expected["solver_adapter"]["status"], "solved")
                for key, value in solver_scalars.items():
                    self.assertEqual(actual_joint[key], value)
                    self.assertEqual(expected_joint[key], value)
                for key in ("reference1", "reference2"):
                    self.assertEqual(actual_joint[key]["object"], expected_joint[key]["object"])
                    self.assertEqual(actual_joint[key]["subnames"], expected_joint[key]["subnames"])
                for key in (
                    "reference1_element_kind",
                    "reference1_primitive",
                    "reference2_element_kind",
                    "reference2_primitive",
                ):
                    self.assertEqual(actual_joint[key], expected_joint[key])

    def test_c3m6_assembly_distance_type_s6_supported_expected_batch_matches_native(self) -> None:
        supported_cases = {
            "assembly-distance-line-circle-real-solver": ("LineCircle", "ASMTRevCylJoint", "distance_ij", 3.75),
            "assembly-distance-circle-circle-real-solver": ("CircleCircle", "ASMTRevCylJoint", "distance_ij", 5.5),
            "assembly-distance-plane-cylinder-real-solver": ("PlaneCylinder", "ASMTLineInPlaneJoint", "offset", 5.0),
            "assembly-distance-plane-sphere-real-solver": ("PlaneSphere", "ASMTPointInPlaneJoint", "offset", 6.0),
            "assembly-distance-cylinder-cylinder-real-solver": (
                "CylinderCylinder",
                "ASMTRevCylJoint",
                "distance_ij",
                6.25,
            ),
            "assembly-distance-cylinder-sphere-real-solver": (
                "CylinderSphere",
                "ASMTCylSphJoint",
                "distance_ij",
                9.5,
            ),
            "assembly-distance-point-cylinder-real-solver": (
                "PointCylinder",
                "ASMTCylSphJoint",
                "distance_ij",
                5.0,
            ),
            "assembly-distance-point-sphere-real-solver": (
                "PointSphere",
                "ASMTSphSphJoint",
                "distance_ij",
                6.0,
            ),
            "assembly-distance-plane-torus-real-solver": ("PlaneTorus", "ASMTPlanarJoint", "offset", 1.5),
            "assembly-distance-cylinder-torus-real-solver": (
                "CylinderTorus",
                "ASMTRevCylJoint",
                "distance_ij",
                5.0,
            ),
            "assembly-distance-torus-torus-real-solver": ("TorusTorus", "ASMTPlanarJoint", "offset", 1.5),
            "assembly-distance-torus-sphere-real-solver": (
                "TorusSphere",
                "ASMTCylSphJoint",
                "distance_ij",
                6.0,
            ),
            "assembly-distance-sphere-sphere-real-solver": (
                "SphereSphere",
                "ASMTSphSphJoint",
                "distance_ij",
                8.0,
            ),
        }
        diagnostic_cases = {
            "assembly-distance-point-curve-real-solver": (
                "PointCurve",
                "mapped_s4_extended",
                "extended_mapping_pending_s5_oracle",
                "point_curve_diagnostic_boundary",
            ),
        }
        default_cases = {
            "assembly-distance-plane-cone-default-boundary": "PlaneCone",
            "assembly-distance-line-cylinder-default-boundary": "LineCylinder",
            "assembly-distance-curve-plane-default-boundary": "CurvePlane",
            "assembly-distance-other-default-boundary": "Other",
        }

        for fixture, (distance_type, solver_class, scalar_field, scalar_value) in supported_cases.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c3m6", fixture)
                expected_joint = expected["solver_adapter"]["solver_joints"][0]
                result = self.run_recompute(fixture, "c3m6")
                adapter = result["objects"]["Assembly"]["solver_adapter"]
                actual_joint = adapter["solver_joints"][0]

                self.assertNotIn("known_gap", expected)
                self.assertNotIn("backendGap", expected)
                self.assertEqual(expected_joint["distance_type"], distance_type)
                self.assertEqual(expected_joint["solver_joint_class"], solver_class)
                self.assertAlmostEqual(expected_joint[scalar_field], scalar_value)
                self.assertEqual(expected_joint["distance_type_mapping_status"], "mapped_s4_extended")
                self.assertEqual(expected_joint["distance_type_boundary"], "extended_mapping_pending_s5_oracle")

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(adapter["status"], "solved")
                self.assertEqual(adapter["unsupported_joints"], [])
                self.assertEqual(adapter["placement_updates"], expected["solver_adapter"]["placement_updates"])
                self.assertEqual(actual_joint["distance_type"], distance_type)
                self.assertEqual(actual_joint["solver_joint_class"], solver_class)
                self.assertAlmostEqual(actual_joint[scalar_field], scalar_value)
                self.assertEqual(actual_joint["distance_type_mapping_status"], "mapped_s4_extended")
                self.assertEqual(
                    actual_joint["distance_type_boundary"],
                    "extended_mapping_pending_s5_oracle",
                )

        for fixture, (distance_type, mapping_status, boundary, unsupported_reason) in diagnostic_cases.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c3m6", fixture)
                expected_joint = expected["solver_adapter"]["solver_joints"][0]
                result = self.run_recompute(fixture, "c3m6")
                adapter = result["objects"]["Assembly"]["solver_adapter"]
                actual_joint = adapter["solver_joints"][0]

                self.assertIn("known_gap", expected)
                self.assertIn("delete_condition", expected["nonGoal"])
                self.assertNotIn("backendGap", expected)
                self.assertEqual(expected["nonGoal"]["ids"], ["DTE-NG-003"])
                self.assertEqual(expected_joint["distance_type"], distance_type)
                self.assertEqual(expected_joint["distance_type_mapping_status"], mapping_status)
                self.assertEqual(expected_joint["distance_type_boundary"], boundary)
                self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_assembly_solver"])
                self.assertEqual(adapter["status"], "unsupported")
                self.assertEqual(adapter["unsupported_joints"][0]["reason"], unsupported_reason)
                self.assertEqual(actual_joint["distance_type"], distance_type)
                self.assertEqual(actual_joint["distance_type_mapping_status"], mapping_status)
                self.assertEqual(actual_joint["distance_type_boundary"], boundary)

        for fixture, distance_type in default_cases.items():
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c3m6", fixture)
                expected_joint = expected["solver_adapter"]["solver_joints"][0]
                result = self.run_recompute(fixture, "c3m6")
                adapter = result["objects"]["Assembly"]["solver_adapter"]
                actual_joint = adapter["solver_joints"][0]

                self.assertIn("known_gap", expected)
                self.assertIn("delete_condition", expected["nonGoal"])
                self.assertNotIn("backendGap", expected)
                self.assertEqual(expected["nonGoal"]["ids"], ["DTE-NG-003"])
                self.assertEqual(expected_joint["distance_type"], distance_type)
                self.assertEqual(expected_joint["distance_type_mapping_status"], "default_boundary_not_mapped")
                self.assertEqual(expected_joint["distance_type_boundary"], "default_or_todo_boundary")
                self.assertNotIn("solver_joint_class", expected_joint)
                self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_assembly_solver"])
                self.assertEqual(adapter["status"], "unsupported")
                self.assertEqual(actual_joint["distance_type"], distance_type)
                self.assertEqual(actual_joint["distance_type_mapping_status"], "default_boundary_not_mapped")
                self.assertEqual(
                    adapter["unsupported_joints"][0]["reason"],
                    "default_boundary_not_mapped",
                )

    def test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto(self) -> None:
        point_a = {
            "object": "PointA",
            "type_id": "Part::Vertex",
            "properties": {"X": 0, "Y": 0, "Z": 0},
            "subname": "Vertex1",
        }
        point_b = {
            "object": "PointB",
            "type_id": "Part::Vertex",
            "properties": {"X": 1, "Y": 0, "Z": 0},
            "subname": "Vertex1",
        }
        line_a = {
            "object": "LineA",
            "type_id": "Part::Line",
            "properties": {"X1": 0, "Y1": 0, "Z1": 0, "X2": 0, "Y2": 0, "Z2": 1},
            "subname": "Edge1",
        }
        line_b = {
            "object": "LineB",
            "type_id": "Part::Line",
            "properties": {"X1": 1, "Y1": 0, "Z1": 0, "X2": 1, "Y2": 0, "Z2": 1},
            "subname": "Edge1",
        }
        plane_a = {
            "object": "PlaneA",
            "type_id": "Part::Plane",
            "properties": {"Length": 2, "Width": 2},
            "subname": "Face1",
        }
        plane_b = {
            "object": "PlaneB",
            "type_id": "Part::Plane",
            "properties": {"Length": 3, "Width": 2},
            "subname": "Face1",
        }
        circle_edge = {
            "object": "CircleEdge",
            "type_id": "Part::Cylinder",
            "properties": {"Radius": 2.25, "Height": 4},
            "subname": "Edge2",
            "element_kind": "Edge",
            "primitive": "circle",
            "radius": 2.25,
            "radius_source": "getEdgeRadius",
        }
        circle_edge_b = {
            "object": "CircleEdgeB",
            "type_id": "Part::Cylinder",
            "properties": {"Radius": 1.75, "Height": 3},
            "subname": "Edge2",
            "element_kind": "Edge",
            "primitive": "circle",
            "radius": 1.75,
            "radius_source": "getEdgeRadius",
        }
        ellipse_edge = {
            "object": "EllipseEdge",
            "type_id": "Part::Ellipse",
            "properties": {"MajorRadius": 4, "MinorRadius": 2, "Angle1": 0, "Angle2": 360},
            "subname": "Edge1",
            "element_kind": "Edge",
            "primitive": "curve",
            "radius": 0.0,
            "radius_source": "getEdgeRadius",
        }
        cylinder_face = {
            "object": "CylinderFace",
            "type_id": "Part::Cylinder",
            "properties": {"Radius": 3.5, "Height": 4},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "cylinder",
            "radius": 3.5,
            "radius_source": "getFaceRadius",
        }
        cylinder_face_b = {
            "object": "CylinderFaceB",
            "type_id": "Part::Cylinder",
            "properties": {"Radius": 1.25, "Height": 5},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "cylinder",
            "radius": 1.25,
            "radius_source": "getFaceRadius",
        }
        sphere_face = {
            "object": "SphereFace",
            "type_id": "Part::Sphere",
            "properties": {"Radius": 4.5, "Angle1": -90, "Angle2": 90, "Angle3": 360},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "sphere",
            "radius": 4.5,
            "radius_source": "getFaceRadius",
        }
        sphere_face_b = {
            "object": "SphereFaceB",
            "type_id": "Part::Sphere",
            "properties": {"Radius": 2.0, "Angle1": -90, "Angle2": 90, "Angle3": 360},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "sphere",
            "radius": 2.0,
            "radius_source": "getFaceRadius",
        }
        cone_face = {
            "object": "ConeFace",
            "type_id": "Part::Cone",
            "properties": {"Radius1": 2, "Radius2": 4, "Height": 6, "Angle": 360},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "cone",
            "radius": 0.0,
            "radius_source": "getFaceRadius",
        }
        torus_face_b = {
            "object": "TorusFaceB",
            "type_id": "Part::Torus",
            "properties": {"Radius1": 3, "Radius2": 0.75, "Angle1": -180, "Angle2": 180, "Angle3": 360},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "torus",
            "radius": 0.0,
            "radius_source": "getFaceRadius",
        }
        torus_face = {
            "object": "TorusFace",
            "type_id": "Part::Torus",
            "properties": {"Radius1": 5, "Radius2": 1, "Angle1": -180, "Angle2": 180, "Angle3": 360},
            "subname": "Face1",
            "element_kind": "Face",
            "primitive": "torus",
            "radius": 0.0,
            "radius_source": "getFaceRadius",
        }

        def expected_element_kind(reference: dict) -> str:
            return reference.get("element_kind", reference["subname"].rstrip("0123456789"))

        def expected_primitive(reference: dict) -> str:
            if "primitive" in reference:
                return reference["primitive"]
            return {
                "Part::Vertex": "point",
                "Part::Line": "line",
                "Part::Plane": "plane",
            }[reference["type_id"]]

        def assert_solver_reference(solver_joint: dict, side: str, expected_reference: dict) -> None:
            self.assertEqual(solver_joint[side]["object"], expected_reference["object"])
            self.assertEqual(solver_joint[side]["subnames"], [expected_reference["subname"]])
            self.assertEqual(solver_joint[f"{side}_element_kind"], expected_element_kind(expected_reference))
            self.assertEqual(solver_joint[f"{side}_primitive"], expected_primitive(expected_reference))
            if "radius" in expected_reference:
                self.assertAlmostEqual(solver_joint[f"{side}_radius"], expected_reference["radius"])
                self.assertEqual(solver_joint[f"{side}_radius_source"], expected_reference["radius_source"])
            elif f"{side}_radius" in solver_joint:
                self.assertAlmostEqual(solver_joint[f"{side}_radius"], 0.0)
            else:
                self.assertNotIn(f"{side}_radius", solver_joint)

        cases = [
            (
                "point_point_zero",
                point_a,
                point_b,
                0.0,
                "PointPoint",
                False,
                point_a,
                point_b,
                "ASMTSphericalJoint",
                None,
                None,
            ),
            (
                "point_point",
                point_a,
                point_b,
                1.5,
                "PointPoint",
                False,
                point_a,
                point_b,
                "ASMTSphSphJoint",
                "distance_ij",
                1.5,
            ),
            (
                "line_line",
                line_a,
                line_b,
                1.5,
                "LineLine",
                False,
                line_a,
                line_b,
                "ASMTRevCylJoint",
                "distance_ij",
                1.5,
            ),
            (
                "point_line",
                point_a,
                line_b,
                1.5,
                "PointLine",
                True,
                line_b,
                point_a,
                "ASMTLineInPlaneJoint",
                "offset",
                1.5,
            ),
            (
                "plane_plane",
                plane_a,
                plane_b,
                1.5,
                "PlanePlane",
                False,
                plane_a,
                plane_b,
                "ASMTPlanarJoint",
                "offset",
                1.5,
            ),
            (
                "point_plane",
                point_a,
                plane_b,
                1.5,
                "PointPlane",
                True,
                plane_b,
                point_a,
                "ASMTPointInPlaneJoint",
                "offset",
                1.5,
            ),
            (
                "line_plane",
                line_a,
                plane_b,
                1.5,
                "LinePlane",
                True,
                plane_b,
                line_a,
                "ASMTLineInPlaneJoint",
                "offset",
                1.5,
            ),
        ]

        for (
            case_name,
            reference1,
            reference2,
            distance,
            distance_type,
            swapped,
            solver_ref1,
            solver_ref2,
            solver_joint_class,
            scalar_field,
            scalar_value,
        ) in cases:
            with self.subTest(case=case_name):
                result = self.run_distance_type_reference_case(case_name, reference1, reference2, distance)
                assembly = result["objects"]["Assembly"]
                joint = result["objects"]["DistanceJoint"]
                solver_joint = assembly["solver_adapter"]["solver_joints"][0]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
                self.assertEqual(joint["reference1"]["object"], reference1["object"])
                self.assertEqual(joint["reference2"]["object"], reference2["object"])
                self.assertEqual(solver_joint["distance_type"], distance_type)
                self.assertEqual(solver_joint["distance"], distance)
                self.assertEqual(solver_joint["solver_joint_class"], solver_joint_class)
                self.assertEqual(solver_joint["distance_type_mapping_status"], "mapped_basic")
                self.assertEqual(solver_joint["distance_type_boundary"], "basic_supported")
                self.assertEqual(solver_joint["scalar_correction"], 0.0)
                self.assertEqual(solver_joint["scalar_correction_source"], "none")
                self.assertEqual(solver_joint["radius_source_side"], "none")
                if scalar_field is None:
                    self.assertNotIn("distance_ij", solver_joint)
                    self.assertNotIn("offset", solver_joint)
                else:
                    self.assertAlmostEqual(solver_joint[scalar_field], scalar_value)
                    other_scalar = "offset" if scalar_field == "distance_ij" else "distance_ij"
                    self.assertNotIn(other_scalar, solver_joint)
                self.assertEqual(solver_joint["jcs_swapped_for_solver"], swapped)
                assert_solver_reference(solver_joint, "reference1", solver_ref1)
                assert_solver_reference(solver_joint, "reference2", solver_ref2)
                for side, solver_ref in (("reference1", solver_ref1), ("reference2", solver_ref2)):
                    reference = solver_joint[side]
                    self.assertEqual(reference["object"], solver_ref["object"])
                    self.assertEqual(reference["markerResolutionStatus"], "resolved_subshape_handle_one_side")
                    self.assertEqual(reference["markerResolutionFrame"], "part_local_subshape_handle_one_side")
                    self.assertFalse(reference["markerResolutionRequiresHandleOneSide"])
                    self.assertFalse(reference["markerResolutionUsedObjectLevelBaseline"])
                    self.assertTrue(reference["markerResolutionConnectorDefaulted"])
                    self.assert_identity_placement(reference["connectorPlacement"])
                    self.assert_identity_placement(reference["markerPlacement"])
                    self.assertIn("handleOneSideOfJoint", reference["markerResolutionDiagnostic"])

        extended_cases = [
            {
                "case": "line_circle",
                "reference1": line_a,
                "reference2": circle_edge,
                "distance_type": "LineCircle",
                "swapped": False,
                "solver_ref1": line_a,
                "solver_ref2": circle_edge,
                "scalar_correction": 2.25,
                "scalar_correction_source": "getEdgeRadius(reference2)",
                "radius_source_side": "reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTRevCylJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 3.75,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "circle_circle",
                "reference1": circle_edge,
                "reference2": circle_edge_b,
                "distance_type": "CircleCircle",
                "swapped": False,
                "solver_ref1": circle_edge,
                "solver_ref2": circle_edge_b,
                "scalar_correction": 4.0,
                "scalar_correction_source": "getEdgeRadius(reference1)+getEdgeRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTRevCylJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 5.5,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "plane_cylinder",
                "reference1": plane_a,
                "reference2": cylinder_face,
                "distance_type": "PlaneCylinder",
                "swapped": False,
                "solver_ref1": plane_a,
                "solver_ref2": cylinder_face,
                "scalar_correction": 3.5,
                "scalar_correction_source": "getFaceRadius(reference2)",
                "radius_source_side": "reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTLineInPlaneJoint",
                "scalar_field": "offset",
                "scalar_value": 5.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "plane_sphere",
                "reference1": plane_a,
                "reference2": sphere_face,
                "distance_type": "PlaneSphere",
                "swapped": False,
                "solver_ref1": plane_a,
                "solver_ref2": sphere_face,
                "scalar_correction": 4.5,
                "scalar_correction_source": "getFaceRadius(reference2)",
                "radius_source_side": "reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTPointInPlaneJoint",
                "scalar_field": "offset",
                "scalar_value": 6.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "plane_torus",
                "reference1": plane_a,
                "reference2": torus_face,
                "distance_type": "PlaneTorus",
                "swapped": False,
                "solver_ref1": plane_a,
                "solver_ref2": torus_face,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTPlanarJoint",
                "scalar_field": "offset",
                "scalar_value": 1.5,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "cylinder_cylinder",
                "reference1": cylinder_face,
                "reference2": cylinder_face_b,
                "distance_type": "CylinderCylinder",
                "swapped": False,
                "solver_ref1": cylinder_face,
                "solver_ref2": cylinder_face_b,
                "scalar_correction": 4.75,
                "scalar_correction_source": "getFaceRadius(reference1)+getFaceRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTRevCylJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 6.25,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "cylinder_sphere",
                "reference1": cylinder_face,
                "reference2": sphere_face,
                "distance_type": "CylinderSphere",
                "swapped": False,
                "solver_ref1": cylinder_face,
                "solver_ref2": sphere_face,
                "scalar_correction": 8.0,
                "scalar_correction_source": "getFaceRadius(reference1)+getFaceRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTCylSphJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 9.5,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "cylinder_torus",
                "reference1": cylinder_face,
                "reference2": torus_face,
                "distance_type": "CylinderTorus",
                "swapped": False,
                "solver_ref1": cylinder_face,
                "solver_ref2": torus_face,
                "scalar_correction": 3.5,
                "scalar_correction_source": "getFaceRadius(reference1)+getFaceRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTRevCylJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 5.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "torus_torus",
                "reference1": torus_face,
                "reference2": torus_face_b,
                "distance_type": "TorusTorus",
                "swapped": False,
                "solver_ref1": torus_face,
                "solver_ref2": torus_face_b,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTPlanarJoint",
                "scalar_field": "offset",
                "scalar_value": 1.5,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "point_sphere",
                "reference1": point_a,
                "reference2": sphere_face,
                "distance_type": "PointSphere",
                "swapped": True,
                "solver_ref1": sphere_face,
                "solver_ref2": point_a,
                "scalar_correction": 4.5,
                "scalar_correction_source": "getFaceRadius(reference1)",
                "radius_source_side": "reference1",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTSphSphJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 6.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "point_cylinder",
                "reference1": point_a,
                "reference2": cylinder_face,
                "distance_type": "PointCylinder",
                "swapped": True,
                "solver_ref1": cylinder_face,
                "solver_ref2": point_a,
                "scalar_correction": 3.5,
                "scalar_correction_source": "getFaceRadius(reference1)",
                "radius_source_side": "reference1",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTCylSphJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 5.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "torus_sphere",
                "reference1": sphere_face,
                "reference2": torus_face,
                "distance_type": "TorusSphere",
                "swapped": True,
                "solver_ref1": torus_face,
                "solver_ref2": sphere_face,
                "scalar_correction": 4.5,
                "scalar_correction_source": "getFaceRadius(reference1)+getFaceRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTCylSphJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 6.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "sphere_sphere",
                "reference1": sphere_face,
                "reference2": sphere_face_b,
                "distance_type": "SphereSphere",
                "swapped": False,
                "solver_ref1": sphere_face,
                "solver_ref2": sphere_face_b,
                "scalar_correction": 6.5,
                "scalar_correction_source": "getFaceRadius(reference1)+getFaceRadius(reference2)",
                "radius_source_side": "reference1+reference2",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTSphSphJoint",
                "scalar_field": "distance_ij",
                "scalar_value": 8.0,
                "unsupported_reason": "missing_marker_placement",
            },
            {
                "case": "point_curve",
                "reference1": point_a,
                "reference2": ellipse_edge,
                "distance_type": "PointCurve",
                "swapped": True,
                "solver_ref1": ellipse_edge,
                "solver_ref2": point_a,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "mapped_s4_extended",
                "boundary": "extended_mapping_pending_s5_oracle",
                "solver_joint_class": "ASMTPointInPlaneJoint",
                "scalar_field": "offset",
                "scalar_value": 1.5,
                "unsupported_reason": "point_curve_diagnostic_boundary",
            },
            {
                "case": "plane_cone_default_boundary",
                "reference1": plane_a,
                "reference2": cone_face,
                "distance_type": "PlaneCone",
                "swapped": False,
                "solver_ref1": plane_a,
                "solver_ref2": cone_face,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "default_boundary_not_mapped",
                "boundary": "default_or_todo_boundary",
                "solver_joint_class": None,
                "scalar_field": None,
                "scalar_value": None,
                "unsupported_reason": "default_boundary_not_mapped",
            },
            {
                "case": "line_cylinder_default_boundary",
                "reference1": line_a,
                "reference2": cylinder_face,
                "distance_type": "LineCylinder",
                "swapped": True,
                "solver_ref1": cylinder_face,
                "solver_ref2": line_a,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "default_boundary_not_mapped",
                "boundary": "default_or_todo_boundary",
                "solver_joint_class": None,
                "scalar_field": None,
                "scalar_value": None,
                "unsupported_reason": "default_boundary_not_mapped",
            },
            {
                "case": "curve_plane_default_boundary",
                "reference1": ellipse_edge,
                "reference2": plane_a,
                "distance_type": "CurvePlane",
                "swapped": True,
                "solver_ref1": plane_a,
                "solver_ref2": ellipse_edge,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "default_boundary_not_mapped",
                "boundary": "default_or_todo_boundary",
                "solver_joint_class": None,
                "scalar_field": None,
                "scalar_value": None,
                "unsupported_reason": "default_boundary_not_mapped",
            },
            {
                "case": "line_curve_other_default_boundary",
                "reference1": ellipse_edge,
                "reference2": line_a,
                "distance_type": "Other",
                "swapped": True,
                "solver_ref1": line_a,
                "solver_ref2": ellipse_edge,
                "scalar_correction": 0.0,
                "scalar_correction_source": "none",
                "radius_source_side": "none",
                "mapping_status": "default_boundary_not_mapped",
                "boundary": "default_or_todo_boundary",
                "solver_joint_class": None,
                "scalar_field": None,
                "scalar_value": None,
                "unsupported_reason": "default_boundary_not_mapped",
            },
        ]
        for case in extended_cases:
            with self.subTest(case=case["case"]):
                result = self.run_distance_type_reference_case(
                    case["case"],
                    case["reference1"],
                    case["reference2"],
                    1.5,
                )
                assembly = result["objects"]["Assembly"]
                joint = result["objects"]["DistanceJoint"]
                solver_joint = assembly["solver_adapter"]["solver_joints"][0]

                if case["unsupported_reason"] is None:
                    self.assertEqual(result["diagnostics"], [])
                    self.assertEqual(assembly["solver_adapter"]["status"], "solved")
                    self.assertEqual(assembly["solver_adapter"]["unsupported_joints"], [])
                else:
                    self.assertEqual(
                        [item["code"] for item in result["diagnostics"]],
                        ["unsupported_assembly_solver"],
                    )
                    self.assertEqual(assembly["solver_adapter"]["status"], "unsupported")
                    self.assertEqual(
                        assembly["solver_adapter"]["unsupported_joints"],
                        [
                            {
                                "object": "DistanceJoint",
                                "joint_type": "Distance",
                                "reason": case["unsupported_reason"],
                            }
                        ],
                    )
                self.assertEqual(joint["reference1"]["object"], case["reference1"]["object"])
                self.assertEqual(joint["reference2"]["object"], case["reference2"]["object"])
                self.assertEqual(solver_joint["distance_type"], case["distance_type"])
                self.assertEqual(solver_joint["distance"], 1.5)
                if case["solver_joint_class"] is None:
                    self.assertNotIn("solver_joint_class", solver_joint)
                    self.assertNotIn("distance_ij", solver_joint)
                    self.assertNotIn("offset", solver_joint)
                else:
                    self.assertEqual(solver_joint["solver_joint_class"], case["solver_joint_class"])
                    self.assertAlmostEqual(solver_joint[case["scalar_field"]], case["scalar_value"])
                    other_scalar = "offset" if case["scalar_field"] == "distance_ij" else "distance_ij"
                    self.assertNotIn(other_scalar, solver_joint)
                self.assertEqual(solver_joint["jcs_swapped_for_solver"], case["swapped"])
                self.assertEqual(solver_joint["distance_type_mapping_status"], case["mapping_status"])
                self.assertEqual(solver_joint["distance_type_boundary"], case["boundary"])
                self.assertAlmostEqual(solver_joint["scalar_correction"], case["scalar_correction"])
                self.assertEqual(solver_joint["scalar_correction_source"], case["scalar_correction_source"])
                self.assertEqual(solver_joint["radius_source_side"], case["radius_source_side"])
                assert_solver_reference(solver_joint, "reference1", case["solver_ref1"])
                assert_solver_reference(solver_joint, "reference2", case["solver_ref2"])

    def test_c3m6_assembly_marker_resolver_exposes_object_level_baseline_evidence(self) -> None:
        result = self.run_recompute("assembly-grounded-distance-joint-real-solver", "c3m6")
        assembly = result["objects"]["Assembly"]
        solver_joint = assembly["solver_adapter"]["solver_joints"][0]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(solver_joint["joint_type"], "Distance")
        for side in ("reference1", "reference2"):
            reference = solver_joint[side]
            self.assertEqual(reference["markerResolutionStatus"], "resolved_object_level_baseline")
            self.assertEqual(reference["markerResolutionFrame"], "part_local_object_level")
            self.assertFalse(reference["markerResolutionRequiresHandleOneSide"])
            self.assertTrue(reference["markerResolutionUsedObjectLevelBaseline"])
            self.assertTrue(reference["markerResolutionConnectorDefaulted"])
            self.assert_identity_placement(reference["connectorPlacement"])
            self.assert_identity_placement(reference["markerPlacement"])
            self.assertIn("Object-level reference", reference["markerResolutionDiagnostic"])

    def test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch(self) -> None:
        subshape_cases = [
            ("assembly-marker-ball-vertex-real-solver", "Ball", False, False),
            ("assembly-marker-revolute-edge-real-solver", "Revolute", False, False),
            ("assembly-marker-slider-edge-real-solver", "Slider", False, False),
            ("assembly-marker-cylindrical-edge-real-solver", "Cylindrical", False, False),
            ("assembly-marker-fixed-face-real-solver", "Fixed", False, False),
            ("assembly-marker-parallel-face-real-solver", "Parallel", False, False),
            ("assembly-marker-perpendicular-face-real-solver", "Perpendicular", False, False),
            ("assembly-marker-angle-face-real-solver", "Angle", False, False),
            ("assembly-distance-point-point-nonzero-real-solver", "Distance", False, False),
            ("assembly-distance-point-point-zero-real-solver", "Distance", False, False),
            ("assembly-distance-line-line-real-solver", "Distance", False, False),
            ("assembly-distance-point-line-real-solver", "Distance", True, False),
            ("assembly-distance-plane-plane-real-solver", "Distance", False, False),
            ("assembly-distance-point-plane-real-solver", "Distance", True, False),
            ("assembly-distance-line-plane-real-solver", "Distance", True, False),
        ]
        for fixture, joint_type, swapped, remains_gap in subshape_cases:
            with self.subTest(fixture=fixture):
                expected = self.expected_freecad("c3m6", fixture)
                if remains_gap:
                    self.assertIn("known_gap", expected)
                    self.assertIn("MP-BLOCK-006", expected["backendGap"]["ids"])
                else:
                    self.assertNotIn("known_gap", expected)
                    self.assertNotIn("backendGap", expected)
                self.assertEqual(expected["solver_adapter"]["status"], "solved")
                self.assertIn("placement_updates", expected["solver_adapter"])
                self.assertGreaterEqual(len(expected["solver_adapter"]["solver_joints"]), 1)

                oracle = expected["native_marker_oracle"]
                self.assertTrue(oracle["requires_cad_core_marker_parity"])
                oracle_joint = oracle["solver_joints"][0]
                self.assertEqual(oracle_joint["joint_type"], joint_type)
                self.assertEqual(oracle_joint["jcs_swapped_for_solver"], swapped)
                for side in ("native_reference1", "native_reference2", "solver_reference1", "solver_reference2"):
                    reference = oracle_joint[side]
                    self.assertEqual(reference["status"], "resolved_native_handle_one_side")
                    self.assertTrue(reference["subshape_reference"])
                    self.assertIn("AssemblyObject.cpp", reference["source"])
                    self.assertEqual(reference["marker_placement"]["PropertyType"], "App::PropertyPlacement")
                if joint_type == "Distance":
                    self.assertIn("linear_distance_from_jcs_placements", oracle_joint["current_value"])
                if joint_type == "Angle":
                    self.assertIn("xy_angle_radians_from_jcs_placements", oracle_joint["current_value"])

        special = self.expected_freecad("c3m6", "assembly-rackpinion-marker-rewrite-real-solver")
        self.assertNotIn("known_gap", special)
        self.assertEqual(special["solver_adapter"]["status"], "solved")
        self.assertEqual(
            [joint["joint_type"] for joint in special["solver_adapter"]["solver_joints"]],
            ["Slider", "RackPinion"],
        )
        self.assertFalse(special["native_marker_oracle"]["requires_cad_core_marker_parity"])

    def test_c3m6_assembly_placement_writeback_applies_to_next_request_graph(self) -> None:
        result = self.run_recompute("assembly-grounded-distance-joint-real-solver", "c3m6")
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["action"], "assembly_set_placement")
        self.assertEqual(updates[0]["object"], "ComponentB")
        self.assertEqual(set(updates[0]["properties"].keys()), {"Placement"})
        self.assert_update_property_type(updates[0], "Placement", "App::PropertyPlacement")
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [4.0, 0.0, 2.0])

        applied_result = self.run_with_document_updates_applied(
            "assembly-grounded-distance-joint-real-solver",
            "c3m6",
            updates,
        )
        assembly = applied_result["objects"]["Assembly"]
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["placement_updates"], [])

    def test_c3m6_assembly_multi_component_writeback_order_and_target_fields(self) -> None:
        result = self.run_recompute("assembly-multi-component-placement-writeback", "c3m6")
        assembly = result["objects"]["Assembly"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["joints"], ["DistanceJointB", "DistanceJointC"])
        self.assertEqual([update["object"] for update in updates], ["ComponentB", "ComponentC"])
        self.assertEqual([update["objectId"] for update in updates], [5, 6])
        self.assertEqual([update["typeId"] for update in updates], ["Assembly::AssemblyLink", "Assembly::AssemblyLink"])
        self.assertEqual([update["action"] for update in updates], ["assembly_set_placement", "assembly_set_placement"])
        self.assertEqual([update["joint"] for update in updates], ["OndselSolver", "OndselSolver"])
        self.assertEqual([update["joint_type"] for update in updates], ["solver_result", "solver_result"])
        self.assertEqual([set(update["properties"].keys()) for update in updates], [{"Placement"}, {"Placement"}])
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [4.0, 0.0, 2.0])
        self.assertEqual(updates[1]["properties"]["Placement"]["Base"], [8.0, 0.0, 4.0])

        applied_result = self.run_with_document_updates_applied(
            "assembly-multi-component-placement-writeback",
            "c3m6",
            updates,
        )
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])

    def test_c3m6_assembly_invalid_grounded_distance_rejects_solver_writeback(self) -> None:
        result = self.run_recompute("assembly-invalid-grounded-distance-real-solver", "c3m6")
        assembly = result["objects"]["Assembly"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(
            assembly["solver_adapter"]["grounded_joints"],
            ["GroundedJointA", "GroundedJointB"],
        )
        self.assertEqual(assembly["solver_adapter"]["joints"], ["DistanceJoint"])
        self.assertEqual(assembly["solver_adapter"]["solver_joints"][0]["joint_type"], "Distance")
        self.assertEqual(assembly["solver_adapter"]["solver_joints"][0]["distance"], 2.0)
        updates = result["documentObjectUpdates"]
        self.assertEqual([update["object"] for update in updates], ["ComponentA", "ComponentB"])
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [0.0, 0.0, 0.0])
        self.assertEqual(updates[1]["properties"]["Placement"]["Base"], [4.0, 0.0, 0.0])
        self.assertAlmostEqual(
            updates[0]["properties"]["Placement"]["Rotation"][1],
            0.2588190451025208,
            delta=1e-12,
        )
        self.assert_object_matches_expected(result, "c3m6", "assembly-invalid-grounded-distance-real-solver")

    def test_p8_assembly_joint_reads_hidden_xlinksub_solver_references(self) -> None:
        result = self.run_recompute("assembly-joint-hidden-reference-diagnostics", "p8")
        fixed = result["objects"]["FixedJoint"]
        assembly = result["objects"]["Assembly"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fixed["status"], "ok")
        self.assertEqual(fixed["assembly"], "joint")
        self.assertEqual(fixed["joint_type"], "Fixed")
        self.assertEqual(fixed["reference1"]["object"], "ComponentA")
        self.assertEqual(fixed["reference1"]["subnames"], ["Face1"])
        self.assertEqual(fixed["reference2"]["object"], "ComponentB")
        self.assertEqual(fixed["reference2"]["subnames"], ["Face1"])
        self.assertEqual(fixed["solve"], "joint_input")
        self.assertEqual(assembly["joints"], ["GroundedJoint", "FixedJoint"])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJoint"])
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assertEqual(assembly["solver_adapter"]["solver_joints"][0]["reference1"]["subnames"], ["Face1"])
        self.assertEqual(assembly["solver_adapter"]["solver_joints"][0]["reference2"]["subnames"], ["Face1"])
        self.assertEqual(len(updates), 1)
        self.assertEqual(updates[0]["action"], "assembly_set_placement")
        self.assertEqual(updates[0]["object"], "ComponentB")
        self.assert_object_matches_expected(result, "p8", "assembly-joint-hidden-reference-diagnostics")

    def test_c3m6_assembly_grounded_joint_types_reports_real_solver_writeback(self) -> None:
        result = self.run_recompute("assembly-grounded-joint-types", "c3m6")
        assembly = result["objects"]["Assembly"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assertEqual(assembly["solver_adapter"]["solver_joints"][0]["joint_type"], "Fixed")
        self.assertEqual([item["action"] for item in updates], ["assembly_set_placement"])
        self.assertEqual(updates[0]["reason"], "assembly_solver_placement_writeback")
        self.assertEqual(updates[0]["object"], "ComponentB")
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [0.0, 0.0, 0.0])

    def test_c3m6_assembly_ungrounded_joint_errors_without_fallback(self) -> None:
        result = self.run_recompute("assembly-ungrounded-joint-errors", "c3m6")
        assembly = result["objects"]["Assembly"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], [])
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assert_object_matches_expected(result, "c3m6", "assembly-ungrounded-joint-errors")

    def test_c4m5_assembly_missing_grounded_part_has_locatable_diagnostic(self) -> None:
        result = self.run_recompute(
            "assembly-runtime-adapter-missing-grounded-part-diagnostic",
            "c4m5",
        )
        assembly = result["objects"]["Assembly"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["missing_grounded_part"])
        self.assertEqual(diagnostic["severity"], "error")
        self.assertEqual(diagnostic["object"], "Assembly")
        self.assertEqual(diagnostic["property"], "Group")
        self.assertEqual(diagnostic["stage"], "runtime")
        self.assertEqual(diagnostic["target"], "GroundedJointMissing")
        self.assertEqual(assembly["solve"], "error")
        self.assertEqual(assembly["solver_adapter"]["status"], "error")
        self.assertEqual(assembly["solver_adapter"]["reason"], "missing_grounded_part")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJointMissing"])
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assertEqual(result["documentObjectUpdates"], [])

    def test_c4m5_assembly_pointcurve_distance_stays_unsupported_diagnostic(self) -> None:
        result = self.run_recompute(
            "assembly-runtime-adapter-pointcurve-unsupported-diagnostic",
            "c4m5",
        )
        assembly = result["objects"]["Assembly"]
        diagnostic = result["diagnostics"][0]
        solver_joint = assembly["solver_adapter"]["solver_joints"][0]

        self.assertEqual([item["code"] for item in result["diagnostics"]], ["unsupported_assembly_solver"])
        self.assertEqual(diagnostic["target"], "DistanceJoint")
        self.assertEqual(assembly["solve"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["status"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["reason"], "unsupported_joint_type")
        self.assertEqual(
            assembly["solver_adapter"]["unsupported_joints"],
            [
                {
                    "object": "DistanceJoint",
                    "joint_type": "Distance",
                    "reason": "point_curve_diagnostic_boundary",
                }
            ],
        )
        self.assertEqual(solver_joint["distance_type"], "PointCurve")
        self.assertEqual(solver_joint["distance_type_mapping_status"], "mapped_s4_extended")
        self.assertEqual(result["documentObjectUpdates"], [])

    def test_c4m5_assembly_partial_writeback_updates_only_changed_components(self) -> None:
        result = self.run_recompute("assembly-runtime-adapter-partial-writeback", "c4m5")
        assembly = result["objects"]["Assembly"]
        updates = result["documentObjectUpdates"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["joints"], ["DistanceJointB", "DistanceJointC"])
        self.assertEqual([update["object"] for update in updates], ["ComponentC"])
        self.assertEqual(updates[0]["action"], "assembly_set_placement")
        self.assertEqual(updates[0]["reason"], "assembly_solver_placement_writeback")
        self.assertEqual(updates[0]["properties"]["Placement"]["Base"], [8.0, 0.0, 4.0])
        self.assertEqual(
            [update["object"] for update in assembly["solver_adapter"]["placement_updates"]],
            ["ComponentC"],
        )

        applied_result = self.run_with_document_updates_applied(
            "assembly-runtime-adapter-partial-writeback",
            "c4m5",
            updates,
        )
        self.assertEqual(applied_result["diagnostics"], [])
        self.assertEqual(applied_result["documentObjectUpdates"], [])

    def test_c3m6_assembly_unsupported_joint_stays_diagnostic(self) -> None:
        result = self.run_recompute("assembly-unsupported-joint-diagnostic", "c3m6")
        assembly = result["objects"]["Assembly"]
        diagnostic = result["diagnostics"][0]
        solver_joint = assembly["solver_adapter"]["solver_joints"][0]

        self.assertEqual(diagnostic["severity"], "warning")
        self.assertEqual(diagnostic["code"], "unsupported_assembly_solver")
        self.assertEqual(diagnostic["target"], "RackPinionJoint")
        self.assertEqual(assembly["solve"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["status"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["reason"], "unsupported_joint_type")
        self.assertEqual(assembly["solver_adapter"]["unsupported_joints"][0]["joint_type"], "RackPinion")
        self.assertEqual(solver_joint["sliding_part_index"], 0)
        self.assertFalse(solver_joint["jcs_swapped_for_solver"])
        self.assertEqual(solver_joint["pitch_radius"], 0.0)
        self.assertNotIn("rack_pinion_marker_rewrite", solver_joint)
        self.assertEqual(result["documentObjectUpdates"], [])

    def test_c3m6_assembly_screw_rackpinion_sliding_precondition_swaps_solver_dto(self) -> None:
        result = self.run_recompute("assembly-screw-rackpinion-sliding-swap-diagnostic", "c3m6")
        assembly = result["objects"]["Assembly"]
        screw = result["objects"]["ScrewJoint"]
        rack_pinion = result["objects"]["RackPinionJoint"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["unsupported_joints"], [])
        self.assertEqual(screw["reference1"]["object"], "ComponentB")
        self.assertEqual(screw["reference2"]["object"], "ComponentA")
        self.assertEqual(rack_pinion["reference1"]["object"], "ComponentB")
        self.assertEqual(rack_pinion["reference2"]["object"], "ComponentA")

        solver_joints = {
            solver_joint["object"]: solver_joint
            for solver_joint in assembly["solver_adapter"]["solver_joints"]
        }
        self.assertNotIn("sliding_part_index", solver_joints["SliderJoint"])
        for joint_name in ("ScrewJoint", "RackPinionJoint"):
            with self.subTest(joint_name=joint_name):
                solver_joint = solver_joints[joint_name]
                self.assertEqual(solver_joint["sliding_part_index"], 2)
                self.assertTrue(solver_joint["jcs_swapped_for_solver"])
                self.assertEqual(solver_joint["reference1"]["object"], "ComponentA")
                self.assertEqual(solver_joint["reference2"]["object"], "ComponentB")
        self.assertEqual(solver_joints["RackPinionJoint"]["distance"], 2.5)
        self.assertEqual(solver_joints["RackPinionJoint"]["pitch_radius"], 2.5)
        rewrite = solver_joints["RackPinionJoint"]["rack_pinion_marker_rewrite"]
        self.assertTrue(rewrite["applied"])
        self.assertEqual(rewrite["rack_object"], "ComponentA")
        self.assertEqual(rewrite["pinion_object"], "ComponentB")
        self.assertAlmostEqual(rewrite["yaw_adjustment"], math.pi / 2.0, delta=1e-12)
        self.assertEqual([update["object"] for update in result["documentObjectUpdates"]], ["ComponentB"])

    def test_c3m6_assembly_rackpinion_marker_rewrite_exposes_pitch_radius(self) -> None:
        result = self.run_recompute("assembly-rackpinion-marker-rewrite-real-solver", "c3m6")
        assembly = result["objects"]["Assembly"]
        rack_pinion = result["objects"]["RackPinionJoint"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(assembly["solve"], "solved")
        self.assertEqual(assembly["solver_adapter"]["status"], "solved")
        self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
        self.assertEqual(assembly["solver_adapter"]["unsupported_joints"], [])
        self.assertEqual(rack_pinion["reference1"]["object"], "ComponentB")
        self.assertEqual(rack_pinion["reference2"]["object"], "ComponentA")

        solver_joints = {
            solver_joint["object"]: solver_joint
            for solver_joint in assembly["solver_adapter"]["solver_joints"]
        }
        solver_joint = solver_joints["RackPinionJoint"]
        self.assertEqual(solver_joint["joint_type"], "RackPinion")
        self.assertEqual(solver_joint["distance"], 2.5)
        self.assertEqual(solver_joint["pitch_radius"], 2.5)
        self.assertEqual(solver_joint["sliding_part_index"], 2)
        self.assertTrue(solver_joint["jcs_swapped_for_solver"])
        self.assertEqual(solver_joint["reference1"]["object"], "ComponentA")
        self.assertEqual(solver_joint["reference2"]["object"], "ComponentB")

        rewrite = solver_joint["rack_pinion_marker_rewrite"]
        self.assertTrue(rewrite["applied"])
        self.assertEqual(rewrite["rack_object"], "ComponentA")
        self.assertEqual(rewrite["pinion_object"], "ComponentB")
        self.assertAlmostEqual(rewrite["yaw_adjustment"], math.pi, delta=1e-12)
        self.assertEqual(rewrite["rack_marker_placement"]["Base"], [0, 0, 0])
        rotation = rewrite["rack_marker_placement"]["Rotation"]
        self.assertAlmostEqual(rotation[0], 0.7071067811865476, delta=1e-12)
        self.assertAlmostEqual(rotation[1], 0.0, delta=1e-12)
        self.assertAlmostEqual(rotation[2], -0.7071067811865475, delta=1e-12)
        self.assertAlmostEqual(rotation[3], 0.0, delta=1e-12)

    def test_c3m6_assembly_s5_screw_rackpinion_grounded_fixtures_are_published_supported(self) -> None:
        cases = [
            (
                "assembly-grounded-screw-joint-real-solver",
                "ScrewJoint",
                "Screw",
                "pitch",
                1.25,
                None,
            ),
            (
                "assembly-grounded-rackpinion-joint-real-solver",
                "RackPinionJoint",
                "RackPinion",
                "pitch_radius",
                2.5,
                "rack_pinion_marker_rewrite",
            ),
        ]
        for fixture, joint_name, joint_type, scalar_field, scalar_value, rewrite_field in cases:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "c3m6")
                assembly = result["objects"]["Assembly"]
                solver_joints = {
                    solver_joint["object"]: solver_joint
                    for solver_joint in assembly["solver_adapter"]["solver_joints"]
                }
                solver_joint = solver_joints[joint_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(assembly["solve"], "solved")
                self.assertEqual(assembly["solver_adapter"]["mode"], "real_ondsel_solver")
                self.assertEqual(assembly["solver_adapter"]["unsupported_joints"], [])
                self.assertEqual(solver_joint["joint_type"], joint_type)
                self.assertEqual(solver_joint["distance"], scalar_value)
                self.assertEqual(solver_joint[scalar_field], scalar_value)
                self.assertEqual(solver_joint["sliding_part_index"], 2)
                self.assertTrue(solver_joint["jcs_swapped_for_solver"])
                self.assertEqual(solver_joint["reference1"]["object"], "ComponentA")
                self.assertEqual(solver_joint["reference2"]["object"], "ComponentB")
                if rewrite_field:
                    self.assertIn(rewrite_field, solver_joint)
                    self.assertTrue(solver_joint[rewrite_field]["applied"])
                self.assert_object_matches_expected(result, "c3m6", fixture)

    def test_p8_part_cylinder_builds_prism_extension_solid(self) -> None:
        result = self.run_recompute("part-cylinder", "p8")
        cylinder = result["objects"]["Cylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cylinder["status"], "ok")
        self.assertEqual(cylinder["primitive"], "cylinder")
        self.assert_object_matches_expected(result, "p8", "part-cylinder")

    def test_p8_part_cylinder_uses_prism_first_angle(self) -> None:
        result = self.run_recompute("part-cylinder-angled-prism", "p8")
        cylinder = result["objects"]["Cylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cylinder["first_angle"], 10.0)
        self.assert_object_matches_expected(result, "p8", "part-cylinder-angled-prism")

    def test_p8_part_sphere_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-sphere", "p8")
        sphere = result["objects"]["Sphere"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(sphere["status"], "ok")
        self.assertEqual(sphere["primitive"], "sphere")
        self.assertEqual(sphere["radius"], 3.0)
        self.assert_object_matches_expected(result, "p8", "part-sphere")

    def test_p8_part_cone_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-cone", "p8")
        cone = result["objects"]["Cone"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(cone["status"], "ok")
        self.assertEqual(cone["primitive"], "cone")
        self.assertEqual(cone["radius1"], 2.0)
        self.assertEqual(cone["radius2"], 4.0)
        self.assertEqual(cone["height"], 6.0)
        self.assert_object_matches_expected(result, "p8", "part-cone")

    def test_p8_part_torus_builds_freecad_revolved_solid(self) -> None:
        result = self.run_recompute("part-torus", "p8")
        torus = result["objects"]["Torus"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(torus["status"], "ok")
        self.assertEqual(torus["primitive"], "torus")
        self.assertEqual(torus["radius1"], 5.0)
        self.assertEqual(torus["radius2"], 1.0)
        self.assert_object_matches_expected(result, "p8", "part-torus")

    def test_p8_part_vertex_line_and_plane_build_topological_shapes(self) -> None:
        result = self.run_recompute("part-vertex", "p8")
        vertex = result["objects"]["Vertex"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(vertex["status"], "ok")
        self.assertEqual(vertex["primitive"], "vertex")
        self.assertEqual(vertex["shape"], "occt_vertex")
        self.assert_object_matches_expected(result, "p8", "part-vertex")

        result = self.run_recompute("part-line", "p8")
        line = result["objects"]["Line"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(line["status"], "ok")
        self.assertEqual(line["primitive"], "line")
        self.assertEqual(line["shape"], "occt_edge")
        self.assertEqual(line["start"], [0.0, 0.0, 0.0])
        self.assertEqual(line["end"], [1.0, 2.0, 3.0])
        self.assert_object_matches_expected(result, "p8", "part-line")

        result = self.run_recompute("part-plane", "p8")
        plane = result["objects"]["Plane"]
        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(plane["status"], "ok")
        self.assertEqual(plane["primitive"], "plane")
        self.assertEqual(plane["shape"], "occt_face")
        self.assertEqual(plane["length"], 4.0)
        self.assertEqual(plane["width"], 3.0)
        self.assert_object_matches_expected(result, "p8", "part-plane")

    def test_p8_part_import_brep_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-brep", "p8")
        imported = result["objects"]["ImportedCylinder"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_brep")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/cylinder1.brep")
        self.assert_object_matches_expected(result, "p8", "part-import-brep")

    def test_p8_part_import_step_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-step", "p8")
        imported = result["objects"]["ImportedStep"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_step")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/as1-ac-214_small.stp")
        self.assert_object_matches_expected(result, "p8", "part-import-step")

    def test_p8_part_import_iges_reads_file_shape(self) -> None:
        result = self.run_recompute("part-import-iges", "p8")
        imported = result["objects"]["ImportedIges"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_iges")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/rlf_12545.igs")
        self.assert_object_matches_expected(result, "p8", "part-import-iges")

    def test_p8_mesh_import_stl_reads_mesh_file_shape(self) -> None:
        result = self.run_recompute("mesh-import-stl", "p8")
        imported = result["objects"]["ImportedStl"]
        mesh = result["mesh"]["ImportedStl"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(imported["status"], "ok")
        self.assertEqual(imported["primitive"], "import_stl")
        self.assertEqual(imported["shape"], "occt_compound")
        self.assertEqual(imported["file_name"], "fixtures/p8/assets/unit-square.stl")
        self.assert_object_matches_expected(result, "p8", "mesh-import-stl")

    def test_p8_part_prism_builds_regular_polygon_solid(self) -> None:
        result = self.run_recompute("part-prism", "p8")
        prism = result["objects"]["Prism"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(prism["status"], "ok")
        self.assertEqual(prism["primitive"], "prism")
        self.assertEqual(prism["polygon"], 6)
        self.assertEqual(prism["circumradius"], 2.0)
        self.assertEqual(prism["height"], 5.0)
        self.assert_object_matches_expected(result, "p8", "part-prism")

    def test_p8_part_regular_polygon_builds_wire(self) -> None:
        result = self.run_recompute("part-regular-polygon", "p8")
        polygon = result["objects"]["RegularPolygon"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(polygon["status"], "ok")
        self.assertEqual(polygon["primitive"], "regular_polygon")
        self.assertEqual(polygon["shape"], "occt_wire")
        self.assertEqual(polygon["polygon"], 6)
        self.assertEqual(polygon["circumradius"], 2.0)
        self.assert_object_matches_expected(result, "p8", "part-regular-polygon")

    def test_p8_part_ellipsoid_builds_scaled_sphere_solid(self) -> None:
        result = self.run_recompute("part-ellipsoid", "p8")
        ellipsoid = result["objects"]["Ellipsoid"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ellipsoid["status"], "ok")
        self.assertEqual(ellipsoid["primitive"], "ellipsoid")
        self.assertEqual(ellipsoid["radius1"], 2.0)
        self.assertEqual(ellipsoid["radius2"], 4.0)
        self.assertEqual(ellipsoid["radius3"], 3.0)
        self.assert_object_matches_expected(result, "p8", "part-ellipsoid")

    def test_p8_part_wedge_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-wedge", "p8")
        wedge = result["objects"]["Wedge"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(wedge["status"], "ok")
        self.assertEqual(wedge["primitive"], "wedge")
        self.assert_object_matches_expected(result, "p8", "part-wedge")

    def test_p8_part_binary_booleans_build_occt_solids(self) -> None:
        cases = {
            "part-fuse": ("Fuse", "fuse"),
            "part-cut": ("Cut", "cut"),
            "part-common": ("Common", "common"),
        }

        for fixture, (object_name, operation) in cases.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                obj = result["objects"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(obj["status"], "ok")
                self.assertEqual(obj["boolean"], operation)
                self.assertEqual(obj["base"], "BaseBox")
                self.assertEqual(obj["tool"], "ToolBox")
                self.assert_object_matches_expected(result, "p8", fixture)

    def test_p8_part_multi_booleans_build_occt_solids(self) -> None:
        cases = {
            "part-multi-fuse": (
                "MultiFuse",
                "multi_fuse",
                None,
            ),
            "part-multi-common": (
                "MultiCommon",
                "multi_common",
                "CommonOfAllShapes",
            ),
            "part-multi-common-first-rest": (
                "MultiCommon",
                "multi_common",
                "CommonOfFirstAndRest",
            ),
        }

        for fixture, (object_name, operation, behavior) in cases.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                obj = result["objects"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(obj["status"], "ok")
                self.assertEqual(obj["boolean"], operation)
                self.assertEqual(obj["shapes"], ["BoxA", "BoxB", "BoxC"])
                if behavior is not None:
                    self.assertEqual(obj["behavior"], behavior)
                self.assert_object_matches_expected(result, "p8", fixture)

    def test_p8_part_xor_builds_compound_from_odd_coverage_pieces(self) -> None:
        result = self.run_recompute("part-xor", "p8")
        xor = result["objects"]["XOR"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(xor["status"], "ok")
        self.assertEqual(xor["boolean"], "xor")
        self.assertEqual(xor["shape"], "occt_compound")
        self.assertEqual(xor["objects"], ["BoxA", "BoxB"])
        self.assert_object_matches_expected(result, "p8", "part-xor")

    def test_p8_part_boolean_fragments_builds_general_fuse_pieces(self) -> None:
        for fixture, mode in [
            ("part-boolean-fragments", "Standard"),
            ("part-boolean-fragments-split", "Split"),
            ("part-boolean-fragments-compsolid", "CompSolid"),
        ]:
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                fragments = result["objects"]["BooleanFragments"]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(fragments["status"], "ok")
                self.assertEqual(fragments["boolean"], "fragments")
                self.assertEqual(fragments["shape"], "occt_compound")
                self.assertEqual(fragments["mode"], mode)
                self.assertEqual(fragments["objects"], ["BoxA", "BoxB"])
                self.assert_object_matches_expected(result, "p8", fixture)

    def test_p8_part_boolean_fragments_split_rebuilds_wire_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-wire-split", "p8")
        fragments = result["objects"]["BooleanFragments"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["PolyA", "PolyB"])
        self.assert_object_matches_expected(result, "p8", "part-boolean-fragments-wire-split")

    def test_p8_part_boolean_fragments_split_rebuilds_compsolid_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-compsolid-split", "p8")
        fragments = result["objects"]["Split"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["Comp", "BoxC"])
        self.assert_object_matches_expected(result, "p8", "part-boolean-fragments-compsolid-split")

    def test_p8_part_boolean_fragments_split_rebuilds_shell_aggregate_pieces(self) -> None:
        result = self.run_recompute("part-boolean-fragments-shell-split", "p8")
        fragments = result["objects"]["BooleanFragments"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(fragments["status"], "ok")
        self.assertEqual(fragments["boolean"], "fragments")
        self.assertEqual(fragments["mode"], "Split")
        self.assertEqual(fragments["shape"], "occt_compound")
        self.assertEqual(fragments["objects"], ["ShellBase", "SplitterFace"])
        self.assert_object_matches_expected(result, "p8", "part-boolean-fragments-shell-split")

    def test_p8_part_section_builds_intersection_edges(self) -> None:
        result = self.run_recompute("part-section", "p8")
        section = result["objects"]["Section"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(section["status"], "ok")
        self.assertEqual(section["boolean"], "section")
        self.assertEqual(section["shape"], "occt_compound")
        self.assertEqual(section["base"], "Box")
        self.assertEqual(section["tool"], "Plane")
        self.assertEqual(section["approximation"], False)
        self.assert_object_matches_expected(result, "p8", "part-section")

    def test_p8_part_ellipse_builds_edge(self) -> None:
        result = self.run_recompute("part-ellipse", "p8")
        ellipse = result["objects"]["Ellipse"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ellipse["status"], "ok")
        self.assertEqual(ellipse["primitive"], "ellipse")
        self.assertEqual(ellipse["shape"], "occt_edge")
        self.assertEqual(ellipse["major_radius"], 4.0)
        self.assertEqual(ellipse["minor_radius"], 2.0)
        self.assert_object_matches_expected(result, "p8", "part-ellipse")

    def test_p8_part_conic_geometry_curves_build_native_edges(self) -> None:
        cases = {
            "part-hyperbola-edge": ("HyperbolaEdge", "hyperbola", "GeomAbs_Hyperbola", "Part.Hyperbola"),
            "part-parabola-edge": ("ParabolaEdge", "parabola", "GeomAbs_Parabola", "Part.Parabola"),
        }
        for fixture, (object_name, curve_kind, curve_type, part_geometry_type) in cases.items():
            with self.subTest(fixture=fixture):
                result = self.run_recompute(fixture, "p8")
                curve = result["objects"][object_name]
                subshapes = result["subshapes"][object_name]

                self.assertEqual(result["diagnostics"], [])
                self.assertEqual(curve["status"], "ok")
                self.assertEqual(curve["feature"], "part_geometry_curve")
                self.assertEqual(curve["dto"], "PartConicCurveDTO")
                self.assertEqual(curve["shape"], "occt_edge")
                self.assertEqual(curve["curve_kind"], curve_kind)
                self.assertEqual(curve["curve_type"], curve_type)
                self.assertEqual(curve["part_geometry_type"], part_geometry_type)
                self.assertGreater(curve["length"], 0.0)
                self.assertIn("Edge1", subshapes)
                self.assertEqual(sum(name.startswith("Edge") for name in subshapes), 1)
                self.assert_object_matches_expected(result, "p8", fixture)

    def test_p8_part_conic_geometry_curves_feed_part_extrusion(self) -> None:
        result = self.run_recompute("part-conic-edge-extrusion", "p8")
        cases = {
            "HyperbolaExtrusion": ("HyperbolaEdge", "hyperbola", "GeomAbs_Hyperbola", "Part.Hyperbola"),
            "ParabolaExtrusion": ("ParabolaEdge", "parabola", "GeomAbs_Parabola", "Part.Parabola"),
        }

        self.assertEqual(result["diagnostics"], [])
        for extrusion_name, (base_name, curve_kind, curve_type, part_geometry_type) in cases.items():
            with self.subTest(extrusion=extrusion_name):
                base = result["objects"][base_name]
                extrusion = result["objects"][extrusion_name]
                subshapes = result["subshapes"][extrusion_name]

                self.assertEqual(base["feature"], "part_geometry_curve")
                self.assertEqual(base["curve_kind"], curve_kind)
                self.assertEqual(base["curve_type"], curve_type)
                self.assertEqual(base["part_geometry_type"], part_geometry_type)
                self.assertEqual(extrusion["status"], "ok")
                self.assertEqual(extrusion["feature"], "part_extrusion")
                self.assertEqual(extrusion["source_base"], base_name)
                self.assertIn(extrusion["shape"], {"occt_face", "occt_shell", "occt_compound"})
                self.assertEqual(extrusion["source_feature"], "part_geometry_curve")
                self.assertEqual(extrusion["source_dto"], "PartConicCurveDTO")
                self.assertEqual(extrusion["source_curve_kind"], curve_kind)
                self.assertEqual(extrusion["source_curve_type"], curve_type)
                self.assertEqual(extrusion["source_part_geometry_type"], part_geometry_type)
                self.assertGreaterEqual(sum(name.startswith("Face") for name in subshapes), 1)

        self.assert_object_matches_expected(result, "p8", "part-conic-edge-extrusion")

    def assert_ruled_surface_source_edge(
        self,
        result: dict,
        ruled_surface: str,
        source_edge: str,
    ) -> None:
        named_shape = result["named_shapes"][ruled_surface]
        element_map = named_shape["element_map"]
        elements = named_shape["elements"]

        self.assertIn(source_edge, element_map)
        target = element_map[source_edge]
        self.assertIn(target, elements)
        self.assertEqual(elements[target]["kind"], "edge")
        self.assertIn(source_edge, elements[target]["sources"])

    def test_p8_part_ruled_surface_edge_edge_builds_face_with_provenance(self) -> None:
        result = self.run_recompute("part-ruled-surface-line-line", "p8")
        ruled = result["objects"]["RuledSurface"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ruled["status"], "ok")
        self.assertEqual(ruled["feature"], "part_ruled_surface")
        self.assertEqual(ruled["shape"], "occt_face")
        self.assertEqual(ruled["source_curve1"], "Line1")
        self.assertEqual(ruled["source_curve2"], "Line2")
        self.assertEqual(ruled["orientation"], "Automatic")
        self.assertEqual(sum(name.startswith("Face") for name in result["subshapes"]["RuledSurface"]), 1)
        self.assert_ruled_surface_source_edge(result, "RuledSurface", "Line1.Edge1")
        self.assert_ruled_surface_source_edge(result, "RuledSurface", "Line2.Edge1")
        self.assertIn(
            "part_ruled_surface:shared_vertex_edge_relation",
            result["named_shapes"]["RuledSurface"]["element_history_status"],
        )
        self.assertTrue(
            any(
                item["maker_stage"] == "ruled_surface_shared_vertex_relation"
                and item["relation"] == "modified"
                for item in result["named_shapes"]["RuledSurface"]["mapper_history"]
            )
        )
        self.assert_object_matches_expected(result, "p8", "part-ruled-surface-line-line")

    def test_p8_part_ruled_surface_conic_edge_feeds_surface_executor(self) -> None:
        result = self.run_recompute("part-ruled-surface-conic-line", "p8")
        conic = result["objects"]["HyperbolaEdge"]
        ruled = result["objects"]["ConicRuledSurface"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(conic["feature"], "part_geometry_curve")
        self.assertEqual(conic["curve_kind"], "hyperbola")
        self.assertEqual(ruled["status"], "ok")
        self.assertEqual(ruled["feature"], "part_ruled_surface")
        self.assertEqual(ruled["shape"], "occt_face")
        self.assertEqual(ruled["source_curve1"], "HyperbolaEdge")
        self.assertEqual(ruled["source_curve2"], "BridgeLine")
        self.assertEqual(ruled["orientation"], "Automatic")
        self.assertEqual(ruled["source_curve1_feature"], "part_geometry_curve")
        self.assertEqual(ruled["source_curve1_dto"], "PartConicCurveDTO")
        self.assertEqual(ruled["source_curve1_curve_kind"], "hyperbola")
        self.assert_ruled_surface_source_edge(result, "ConicRuledSurface", "HyperbolaEdge.Edge1")
        self.assert_ruled_surface_source_edge(result, "ConicRuledSurface", "BridgeLine.Edge1")
        self.assert_object_matches_expected(result, "p8", "part-ruled-surface-conic-line")

    def test_p8_part_ruled_surface_orientation_reversed_is_not_ignored(self) -> None:
        result = self.run_recompute("part-ruled-surface-orientation-reversed", "p8")
        ruled = result["objects"]["ReversedRuledSurface"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(ruled["status"], "ok")
        self.assertEqual(ruled["feature"], "part_ruled_surface")
        self.assertEqual(ruled["shape"], "occt_face")
        self.assertEqual(ruled["orientation"], "Reversed")
        self.assert_ruled_surface_source_edge(result, "ReversedRuledSurface", "SkewLine1.Edge1")
        self.assert_ruled_surface_source_edge(result, "ReversedRuledSurface", "SkewLine2.Edge1")
        self.assert_object_matches_expected(result, "p8", "part-ruled-surface-orientation-reversed")

    def test_p8_part_ruled_surface_invalid_inputs_have_stable_diagnostics(self) -> None:
        result = self.run_recompute("part-ruled-surface-invalid-input", "p8")
        codes = [item["code"] for item in result["diagnostics"]]

        self.assertEqual(
            codes,
            [
                "missing_property",
                "missing_link_target",
                "invalid_subshape",
                "unsupported_subshape_kind",
                "no_edge",
            ],
        )
        for object_name in ("MissingCurve", "EmptyLink", "MultiSubname", "NonEdge", "NoEdge"):
            self.assertEqual(result["objects"][object_name]["status"], "error")
            self.assertEqual(result["objects"][object_name]["feature"], "part_ruled_surface")
        self.assert_object_matches_expected(result, "p8", "part-ruled-surface-invalid-input")

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
        self.assert_object_matches_expected(result, "p8", "part-helix")
        self.assertGreater(helix["length"], 12.0)

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
        self.assert_object_matches_expected(result, "p8", "part-spiral")
        self.assertGreater(spiral["length"], 24.0)
