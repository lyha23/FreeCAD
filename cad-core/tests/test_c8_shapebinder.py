from __future__ import annotations

import json
import subprocess

try:
    from .fixture_runner import BIN, ROOT, CadCoreFixtureTestCase
except ImportError:  # pragma: no cover - supports `python -m unittest tests.test_c8_shapebinder`.
    from fixture_runner import BIN, ROOT, CadCoreFixtureTestCase


class CadCoreC8ShapeBinderTest(CadCoreFixtureTestCase):
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

    def test_binder_element_map_namedshape_and_body_replay_stay_request_local(self) -> None:
        result = self.run_recompute("shape-binder-subshape-binder-element-map-namedshape-body-replay", "partdesign-binder")
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
        bindmode = self.run_recompute("subshape-binder-bindmode-synchronized-frozen-detached", "partdesign-binder")
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

        copy = self.run_recompute("subshape-binder-copy-on-change-disabled-enabled-mutated-partialload", "partdesign-binder")
        self.assertEqual(copy["diagnostics"], [])
        self.assertEqual(
            copy["objects"]["CopyOnChangeEnabled"]["copy_on_change_boundary"],
            "request_local_support_recompute_no_persistent_temp_doc",
        )
        self.assertEqual(
            copy["objects"]["CopyOnChangeEnabled"]["copy_on_change_lifecycle"],
            "enabled_waiting_for_frontend_mutation",
        )
        self.assertEqual(
            copy["objects"]["CopyOnChangeMutated"]["copy_on_change_lifecycle"],
            "mutated_from_request_graph",
        )
        self.assertEqual(
            copy["objects"]["PartialLoadEnabled"]["partial_load_boundary"],
            "request_local_input_no_lazy_backend_session",
        )

    def test_setlinks_normalization_keeps_cycle_diagnostic_without_unsupported_type(self) -> None:
        result = self.run_recompute("subshape-binder-setlinks-normalization-diagnostics", "partdesign-binder")
        codes = [item["code"] for item in result["diagnostics"]]
        self.assertEqual(codes, ["cycle_rejected_by_property_link"])
        self.assertNotIn("unsupported_type", codes)
        diagnostic = result["diagnostics"][0]
        self.assertEqual(diagnostic["object"], "SubShapeBinderCycle")
        self.assertEqual(diagnostic["property"], "Support")
        self.assertEqual(diagnostic["target"], "SubShapeBinderCycle")

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
            "supported_c8m1_expected_backed_request_local",
        )
        self.assertIn("MakeFace from support edges", sub_shape_binder["covered"])
        self.assertIn("BindMode Synchronized/Frozen/Detached request-local subset", sub_shape_binder["covered"])
        self.assertIn(
            "BindCopyOnChange Enabled/Mutated request-local support recompute metadata",
            sub_shape_binder["covered"],
        )
        self.assertIn("PartialLoad request-local input boundary", sub_shape_binder["covered"])
        self.assertEqual(sub_shape_binder["remaining_gaps"], [])
        self.assertEqual(sub_shape_binder["known_gaps"], {})

        topo_history = capabilities["topo_history"]
        self.assertIn("shapebinder", topo_history["maker_history"])
        self.assertIn("subshapebinder", topo_history["maker_history"])
        producer = topo_history["producer_matrix"]["shapebinder"]
        self.assertEqual(producer["status"], "supported_c8m1_expected_backed_request_local")
        self.assertIn("maker_history:shapebinder", producer["covered"])
        self.assertIn("maker_history:subshapebinder", producer["covered"])
        self.assertEqual(producer["remaining"], [])
