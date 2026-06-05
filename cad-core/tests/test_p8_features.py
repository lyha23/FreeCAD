from __future__ import annotations

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import CadCoreFixtureTestCase


class CadCoreP8FeatureTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def assert_update_property_type(self, update: dict, property_name: str, property_type: str) -> None:
        self.assertEqual(update["properties"][property_name]["PropertyType"], property_type)

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

    def test_p8_assembly_joint_group_reports_solver_inputs_and_unsupported_solver(self) -> None:
        result = self.run_recompute("assembly-joint-group-diagnostics", "p8")
        assembly = result["objects"]["Assembly"]
        joint_group = result["objects"]["Joints"]
        grounded = result["objects"]["GroundedJoint"]
        fixed = result["objects"]["FixedJoint"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["severity"], "warning")
        self.assertEqual(diagnostic["code"], "unsupported_assembly_solver")
        self.assertEqual(diagnostic["object"], "Assembly")
        self.assertEqual(diagnostic["property"], "Group")
        self.assertEqual(diagnostic["target"], "FixedJoint")
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
        self.assertEqual(assembly["solve"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["status"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["reason"], "joint_type_not_migrated")
        self.assertEqual(assembly["solver_adapter"]["grounded_joints"], ["GroundedJoint"])
        self.assertEqual(assembly["solver_adapter"]["joints"], ["FixedJoint"])
        self.assert_object_matches_expected(result, "p8", "assembly-joint-group-diagnostics")

    def test_p8_assembly_joint_reads_hidden_xlinksub_solver_references(self) -> None:
        result = self.run_recompute("assembly-joint-hidden-reference-diagnostics", "p8")
        fixed = result["objects"]["FixedJoint"]
        assembly = result["objects"]["Assembly"]
        diagnostic = result["diagnostics"][0]

        self.assertEqual(diagnostic["severity"], "warning")
        self.assertEqual(diagnostic["code"], "unsupported_assembly_solver")
        self.assertEqual(diagnostic["target"], "FixedJoint")
        self.assertEqual(fixed["status"], "ok")
        self.assertEqual(fixed["assembly"], "joint")
        self.assertEqual(fixed["joint_type"], "Fixed")
        self.assertEqual(fixed["reference1"]["object"], "ComponentA")
        self.assertEqual(fixed["reference1"]["subnames"], ["Face1"])
        self.assertEqual(fixed["reference2"]["object"], "ComponentB")
        self.assertEqual(fixed["reference2"]["subnames"], ["Face1"])
        self.assertEqual(fixed["solve"], "joint_input")
        self.assertEqual(assembly["joints"], ["FixedJoint"])
        self.assertEqual(assembly["solve"], "unsupported")
        self.assertEqual(assembly["solver_adapter"]["status"], "unsupported")
        self.assert_object_matches_expected(result, "p8", "assembly-joint-hidden-reference-diagnostics")

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
