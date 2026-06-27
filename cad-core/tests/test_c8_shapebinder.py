from __future__ import annotations

import json
import subprocess

try:
    from .fixture_expected import ExpectedFixtureAssertions
    from .fixture_runner import BIN, ROOT, CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `python -m unittest tests.test_c8_shapebinder`.
    from fixture_expected import ExpectedFixtureAssertions
    from fixture_runner import BIN, ROOT, CadCoreFixtureTestCase


class CadCoreC8ShapeBinderTest(ExpectedFixtureAssertions, CadCoreFixtureTestCase):
    def run_capabilities_cli(self) -> dict:
        completed = subprocess.run(
            [str(BIN), "capabilities"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        return json.loads(completed.stdout)

    def assert_no_error_diagnostics(self, result: dict) -> None:
        self.assertEqual(
            [item for item in result["diagnostics"] if item.get("severity") == "error"],
            [],
        )

    def assert_object_summary_matches(self, result: dict, group: str, fixture: str, object_name: str) -> None:
        expected = self.expected_freecad(group, fixture)["objects"][object_name]["shape_summary"]
        actual = result["objects"][object_name]
        self.assert_bbox_close_delta(actual["bbox"], expected["bbox"]["min"], expected["bbox"]["max"], 1e-6)
        self.assertAlmostEqual(actual["volume"], expected["volume"], delta=1e-6)
        if "area" in actual:
            self.assertAlmostEqual(actual["area"], expected["area"], delta=1e-6)
        if "length" in actual:
            self.assertAlmostEqual(actual["length"], expected["length"], delta=1e-6)
        counts = expected["topology_counts"]
        subshapes = result["subshapes"][object_name]
        self.assertEqual(sum(key.startswith("Face") for key in subshapes), counts["faces"])
        self.assertEqual(sum(key.startswith("Edge") for key in subshapes), counts["edges"])
        self.assertEqual(sum(key.startswith("Vertex") for key in subshapes), counts["vertices"])

    def test_shape_binder_whole_subshape_trace_and_datum_expected_backed(self) -> None:
        whole = self.run_recompute("shape-binder-whole-box-cross-body", "c8m1")
        self.assert_no_error_diagnostics(whole)
        self.assert_object_summary_matches(whole, "c8m1", "shape-binder-whole-box-cross-body", "ShapeBinder")

        subshapes = self.run_recompute("shape-binder-face-edge-vertex-multi-subshape", "c8m1")
        self.assert_no_error_diagnostics(subshapes)
        for object_name in ("ShapeBinderFace", "ShapeBinderEdge", "ShapeBinderVertex", "ShapeBinderMulti"):
            self.assert_object_summary_matches(
                subshapes,
                "c8m1",
                "shape-binder-face-edge-vertex-multi-subshape",
                object_name,
            )

        trace = self.run_recompute("shape-binder-trace-support-placement", "c8m1")
        self.assert_no_error_diagnostics(trace)
        self.assert_object_summary_matches(
            trace,
            "c8m1",
            "shape-binder-trace-support-placement",
            "ShapeBinderTraceFalse",
        )
        self.assert_object_summary_matches(
            trace,
            "c8m1",
            "shape-binder-trace-support-placement",
            "ShapeBinderTraceTrue",
        )

        datum = self.run_recompute("shape-binder-datum-fallback-line-plane-point", "c8m1")
        self.assert_no_error_diagnostics(datum)
        for object_name in ("ShapeBinderLine", "ShapeBinderPlane", "ShapeBinderPoint"):
            self.assert_object_summary_matches(
                datum,
                "c8m1",
                "shape-binder-datum-fallback-line-plane-point",
                object_name,
            )

    def test_subshape_binder_support_makeface_offset_fuse_relative_expected_backed(self) -> None:
        basic = self.run_recompute("subshape-binder-basic-support-whole-face-edge-list", "c8m1")
        self.assert_no_error_diagnostics(basic)
        for object_name in ("SubShapeBinderWhole", "SubShapeBinderFace", "SubShapeBinderEdgeList"):
            self.assert_object_summary_matches(
                basic,
                "c8m1",
                "subshape-binder-basic-support-whole-face-edge-list",
                object_name,
            )

        ops = self.run_recompute("subshape-binder-makeface-offset-fuse-refine", "c8m1")
        self.assert_no_error_diagnostics(ops)
        for object_name in (
            "SubShapeBinderMakeFace",
            "SubShapeBinderOffset",
            "SubShapeBinderFuse",
            "SubShapeBinderRefine",
        ):
            self.assert_object_summary_matches(
                ops,
                "c8m1",
                "subshape-binder-makeface-offset-fuse-refine",
                object_name,
            )

        relative = self.run_recompute("subshape-binder-relative-context-nested-route", "c8m1")
        self.assert_no_error_diagnostics(relative)
        for object_name in ("SubShapeBinderNested", "SubShapeBinderRelative"):
            self.assert_object_summary_matches(
                relative,
                "c8m1",
                "subshape-binder-relative-context-nested-route",
                object_name,
            )

    def test_subshape_binder_profile_consumer_and_body_replay_expected_backed(self) -> None:
        result = self.run_recompute("subshape-binder-profile-consumer-before-after-pad", "c8m1")
        self.assert_no_error_diagnostics(result)
        for object_name in ("BinderProfile", "Revolution", "Body"):
            self.assert_object_summary_matches(
                result,
                "c8m1",
                "subshape-binder-profile-consumer-before-after-pad",
                object_name,
            )
        self.assertEqual(result["objects"]["BinderProfile"]["feature"], "sub_shape_binder")
        self.assertEqual(result["objects"]["Revolution"]["status"], "ok")
        self.assertEqual(result["objects"]["Body"]["status"], "ok")

    def test_binder_element_map_namedshape_and_body_replay_stay_request_local(self) -> None:
        result = self.run_recompute("shape-binder-subshape-binder-element-map-namedshape-body-replay", "c8m1")
        self.assert_no_error_diagnostics(result)
        self.assertNotIn("BodyBaseFeature", result["objects"])
        self.assertEqual(result["documentObjectUpdates"], [])
        self.assertEqual(result["objects"]["Body"]["group"], ["SubShapeBinder"])
        self.assertEqual(result["objects"]["Body"]["tip"], "SubShapeBinder")
        for object_name in ("Fusion", "ShapeBinder", "SubShapeBinder", "Body"):
            self.assertEqual(result["objects"][object_name]["status"], "ok")
        for object_name in ("ShapeBinder", "SubShapeBinder", "Body"):
            element_map = result["named_shapes"][object_name]["element_map"]
            self.assertTrue(any(source.startswith("Box.") for source in element_map))
            self.assertTrue(any(source.startswith("Box001.") for source in element_map))

    def test_bindmode_and_copy_on_change_lifecycle_boundaries_are_explicit(self) -> None:
        bindmode = self.run_recompute("subshape-binder-bindmode-synchronized-frozen-detached", "c8m1")
        self.assert_no_error_diagnostics(bindmode)
        self.assertEqual(bindmode["objects"]["BindModeSynchronized"]["bind_mode"], "Synchronized")
        self.assertEqual(
            bindmode["objects"]["BindModeFrozen"]["bind_mode_boundary"],
            "request_local_frozen_without_persistent_previous_shape",
        )
        self.assertEqual(bindmode["objects"]["BindModeDetached"]["bind_mode_writeback"], "clear_support")
        self.assertEqual(
            bindmode["documentObjectUpdates"],
            [
                {
                    "object": "BindModeDetached",
                    "property": "Support",
                    "action": "clear",
                    "reason": "PartDesign::SubShapeBinder BindMode=Detached request-local writeback",
                }
            ],
        )

        copy = self.run_recompute("subshape-binder-copy-on-change-disabled-enabled-mutated-partialload", "c8m1")
        self.assertEqual(
            [item["code"] for item in copy["diagnostics"]],
            [
                "copy_on_change_full_temporary_document_cache_not_supported",
                "copy_on_change_full_temporary_document_cache_not_supported",
                "copy_on_change_full_temporary_document_cache_not_supported",
            ],
        )
        for object_name in (
            "CopyOnChangeDisabled",
            "CopyOnChangeEnabled",
            "CopyOnChangeMutated",
            "PartialLoadEnabled",
        ):
            self.assert_object_summary_matches(
                copy,
                "c8m1",
                "subshape-binder-copy-on-change-disabled-enabled-mutated-partialload",
                object_name,
            )

    def test_setlinks_normalization_keeps_cycle_diagnostic_without_unsupported_type(self) -> None:
        result = self.run_recompute("subshape-binder-setlinks-normalization-diagnostics", "c8m1")
        codes = [item["code"] for item in result["diagnostics"]]
        self.assertIn("cycle_dependency", codes)
        self.assertNotIn("unsupported_type", codes)
        self.assert_object_summary_matches(
            result,
            "c8m1",
            "subshape-binder-setlinks-normalization-diagnostics",
            "SubShapeBinderEmptySubList",
        )

    def test_capability_contract_publishes_c8m1_binder_scope(self) -> None:
        capabilities = self.run_capabilities_cli()

        shape_binder = capabilities["part_design"]["shape_binder"]
        self.assertEqual(shape_binder["status"], "supported_c8m1_expected_backed_request_local")
        self.assertIn("whole_shape_support", shape_binder["covered"])
        self.assertIn("TraceSupport source-to-target transform", shape_binder["covered"])
        self.assertIn("ElementMap/NamedShape retag through linked source", shape_binder["covered"])
        self.assertEqual(shape_binder["remaining_gaps"], [])

        sub_shape_binder = capabilities["part_design"]["sub_shape_binder"]
        self.assertEqual(
            sub_shape_binder["status"],
            "supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap",
        )
        self.assertIn("MakeFace from support edges", sub_shape_binder["covered"])
        self.assertIn("BindMode Synchronized/Frozen/Detached request-local subset", sub_shape_binder["covered"])
        self.assertEqual(sub_shape_binder["remaining_gaps"], ["copy_on_change_full_temporary_document_cache"])
        copy_on_change = sub_shape_binder["known_gaps"]["copy_on_change_full_temporary_document_cache"]
        self.assertEqual(copy_on_change["status"], "known_gap_diagnostic")
        self.assertEqual(copy_on_change["route"], "oracle_blocked")
        self.assertEqual(copy_on_change["diagnostic"], "copy_on_change_full_temporary_document_cache_not_supported")
        self.assertIn("delete_condition", copy_on_change)
        self.assertIn("reopen_condition", copy_on_change)

        topo_history = capabilities["topo_history"]
        self.assertIn("shapebinder", topo_history["maker_history"])
        self.assertIn("subshapebinder", topo_history["maker_history"])
        producer = topo_history["producer_matrix"]["shapebinder"]
        self.assertEqual(producer["status"], "supported_c8m1_expected_backed_request_local")
        self.assertIn("maker_history:shapebinder", producer["covered"])
        self.assertIn("maker_history:subshapebinder", producer["covered"])
        self.assertEqual(producer["remaining"], [])
