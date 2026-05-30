from __future__ import annotations

from .fixture_expected import ExpectedFixtureAssertions
from .fixture_runner import CadCoreFixtureTestCase


class CadCoreP8FeatureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def test_p8_part_box_builds_occt_solid(self) -> None:
        result = self.run_recompute("part-box", "p8")
        box = result["objects"]["Box"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(box["status"], "ok")
        self.assertEqual(box["primitive"], "box")
        self.assert_object_matches_expected(result, "p8", "part-box")

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

    def test_p8_app_link_preserves_full_sublist_alias(self) -> None:
        result = self.run_recompute("app-link-full-sublist-retag", "p8")
        link = result["objects"]["BoxLink"]
        element_map = result["named_shapes"]["BoxLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["ExternalDoc.Box.Face1"], "Face1")
        self.assertEqual(element_map["Face1;:X;ExternalDoc.Box.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-full-sublist-retag")

    def test_p8_app_link_resolves_label_qualified_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-label-qualified-sublist", "p8")
        link = result["objects"]["BoxLink"]
        element_map = result["named_shapes"]["BoxLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "Box")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["$PrettyBox.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-label-qualified-sublist")

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
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["1.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-sublist-index")

    def test_p8_app_link_resolves_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-sublist-label", "p8")
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["$PrettyB.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-sublist-label")

    def test_p8_app_link_resolves_hidden_group_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-list-hidden-sublist-label", "p8")
        group = result["objects"]["LinkGroup"]
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["visible_elements"], ["LinkA"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "LinkGroup")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["$PrettyB.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-element-list-hidden-sublist-label")

    def test_p8_app_link_resolves_object_qualified_nested_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-nested-object-qualified-sublist", "p8")
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["link"], "app_link")
        self.assertEqual(link["linked_object"], "BoxLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["BoxLink.Box.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-nested-object-qualified-sublist")

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
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-collapsed")

    def test_p8_app_link_element_count_resolves_indexed_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-sublist-index", "p8")
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["1.Face1"], "Face1")
        self.assertEqual(element_map["Face1;:I1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-sublist-index")

    def test_p8_app_link_element_count_resolves_target_label_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-sublist-target-label", "p8")
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["$PrettyBox.Face1"], "Face1")
        self.assertEqual(element_map["0.Face1"], "Face1")
        self.assert_object_matches_expected(result, "p8", "app-link-element-count-sublist-target-label")

    def test_p8_app_link_element_count_resolves_hidden_indexed_subshape_alias(self) -> None:
        result = self.run_recompute("app-link-element-count-hidden-sublist-index", "p8")
        group = result["objects"]["ArrayLink"]
        link = result["objects"]["FaceLink"]
        element_map = result["named_shapes"]["FaceLink"]["element_map"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(group["visible_indices"], [0])
        self.assertEqual(group["shape"], "occt_solid")
        self.assertEqual(link["status"], "ok")
        self.assertEqual(link["linked_object"], "ArrayLink")
        self.assertEqual(link["shape"], "occt_face")
        self.assertEqual(element_map["1.Face1"], "Face1")
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

    def test_p8_app_link_show_element_groups_materialized_children(self) -> None:
        result = self.run_recompute("app-link-show-element-materialized", "p8")
        group = result["objects"]["ArrayLink"]

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
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-materialized")

    def test_p8_app_link_show_element_inherits_child_link_target(self) -> None:
        result = self.run_recompute("app-link-show-element-inherited-child", "p8")
        element = result["objects"]["ArrayLink_i0"]
        group = result["objects"]["ArrayLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["link"], "app_link_element")
        self.assertEqual(element["linked_object"], "Box")
        self.assertEqual(element["inherited_linked_object"], True)
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["visible_elements"], ["ArrayLink_i0"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-inherited-child")

    def test_p8_app_link_show_element_inherits_child_transform_lists(self) -> None:
        result = self.run_recompute("app-link-show-element-inherited-placement-list", "p8")
        element = result["objects"]["ArrayLink_i1"]
        group = result["objects"]["ArrayLink"]

        self.assertEqual(result["diagnostics"], [])
        self.assertEqual(element["status"], "ok")
        self.assertEqual(element["inherited_linked_object"], True)
        self.assertEqual(group["status"], "ok")
        self.assertEqual(group["visible_elements"], ["ArrayLink_i1"])
        self.assertEqual(group["shape"], "occt_solid")
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-inherited-placement-list")

    def test_p8_app_link_show_element_synthesizes_missing_children(self) -> None:
        result = self.run_recompute("app-link-show-element-synthetic", "p8")
        group = result["objects"]["ArrayLink"]

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
        self.assert_object_matches_expected(result, "p8", "app-link-show-element-synthetic")

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
        self.assertEqual(assembly["solve"], "not_migrated")
        self.assert_object_matches_expected(result, "p8", "assembly-link-basic")

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
