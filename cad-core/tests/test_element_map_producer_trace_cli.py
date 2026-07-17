"""CLI publication and preflight-failure tests for producer-trace sidecars."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.element_map_producer_trace import validate_trace
from tests.fixture_runner import semantic_fixture_path


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "build" / "cad-core"


class ElementMapProducerTraceCliTests(unittest.TestCase):
    def setUp(self) -> None:
        if not CLI.exists():
            self.skipTest("build/cad-core is required")
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def run_case(self, fixture: Path, output_name: str) -> tuple[Path, Path]:
        output = self.directory / output_name
        completed = subprocess.run(
            [str(CLI), "recompute", str(fixture), "--output", str(output)],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        if output_name.endswith(".cad-core.json"):
            trace = output.with_name(output_name[:-5] + ".producer-trace.json")
        else:
            trace = output.with_name(output.stem + ".cad-core.producer-trace.json")
        self.assertTrue(trace.is_file())
        self.assertGreater(trace.stat().st_size, 0)
        validate_trace(
            trace,
            input_document=json.loads(fixture.read_text(encoding="utf-8")),
            response_document=json.loads(output.read_text(encoding="utf-8")),
        )
        return output, trace

    def test_default_sidecar_path_for_cad_core_output(self) -> None:
        self.run_case(ROOT / "fixtures" / "sketcher-geometry" / "sketch-open-wire-internal-empty.json", "open.cad-core.json")

    def test_default_sidecar_path_for_plain_output(self) -> None:
        self.run_case(ROOT / "fixtures" / "sketcher-geometry" / "sketch-open-wire-internal-empty.json", "open.json")

    def test_preflight_rejection_has_closed_abort_transaction(self) -> None:
        output, trace_path = self.run_case(
            ROOT / "fixtures" / "topology-state" / "topo-state-schema-incompatible.json",
            "rejected.cad-core.json",
        )
        response = json.loads(output.read_text(encoding="utf-8"))
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        self.assertTrue(response["diagnostics"])
        self.assertEqual(trace["transactions"][0]["outcome"], "abort")
        failures = [event for event in trace["events"] if event["slice"] == "failure"]
        self.assertEqual(failures[-1]["reason"], "topo_state_preflight_rejected")
        self.assertFalse(failures[-1]["fields"]["partialWrite"])

    def test_existing_stale_sidecar_is_replaced(self) -> None:
        trace_path = self.directory / "open.cad-core.producer-trace.json"
        trace_path.write_text('{"stale":true}\n', encoding="utf-8")
        _, published = self.run_case(
            ROOT / "fixtures" / "sketcher-geometry" / "sketch-open-wire-internal-empty.json",
            "open.cad-core.json",
        )
        self.assertNotIn("stale", json.loads(published.read_text(encoding="utf-8")))

    def test_sidecar_write_failure_is_nonzero_and_leaves_no_partial_file(self) -> None:
        output = self.directory / "blocked.cad-core.json"
        trace = self.directory / "blocked.cad-core.producer-trace.json"
        trace.mkdir()
        (trace / "keep").write_text("force rename failure", encoding="utf-8")
        completed = subprocess.run(
            [
                str(CLI),
                "recompute",
                str(ROOT / "fixtures" / "sketcher-geometry" / "sketch-open-wire-internal-empty.json"),
                "--output",
                str(output),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertFalse(trace.is_file())
        self.assertFalse(Path(str(trace) + ".tmp").exists())

    def test_unsupported_executor_and_dependency_skip_close_the_transaction(self) -> None:
        fixture = self.directory / "dependency-skip.json"
        request = {
            "Objects": [
                {
                    "Name": "Broken",
                    "ID": 1,
                    "TypeId": "Unsupported::Producer",
                    "Properties": {},
                },
                {
                    "Name": "DependentBody",
                    "ID": 2,
                    "TypeId": "PartDesign::Body",
                    "Properties": {
                        "Tip": {
                            "PropertyType": "App::PropertyLink",
                            "value": "Broken",
                        }
                    },
                },
            ],
            "recompute": {"objs": ["DependentBody"]},
        }
        fixture.write_text(json.dumps(request), encoding="utf-8")
        output, trace_path = self.run_case(fixture, "dependency.cad-core.json")
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        self.assertEqual("abort", trace["transactions"][0]["outcome"])
        endings = [
            (event["object"], event["decision"], event["reason"])
            for event in trace["events"]
            if event["slice"] == "document.object.execute.end"
        ]
        self.assertIn(("Broken", "rejected", "unsupported_type"), endings)
        self.assertIn(("DependentBody", "skipped", "dependency_failed"), endings)
        self.assertTrue(json.loads(output.read_text(encoding="utf-8"))["diagnostics"])

    def test_body_and_sketch_early_returns_publish_stable_guard_reasons(self) -> None:
        fixture = self.directory / "producer-guards.json"
        request = {
            "Objects": [
                {
                    "Name": "BrokenBody",
                    "ID": 1,
                    "TypeId": "PartDesign::Body",
                    "Properties": {},
                },
                {
                    "Name": "BrokenSketch",
                    "ID": 2,
                    "TypeId": "Sketcher::SketchObject",
                    "Properties": {},
                },
            ],
            "recompute": {"objs": ["BrokenBody", "BrokenSketch"]},
        }
        fixture.write_text(json.dumps(request), encoding="utf-8")
        _output, trace_path = self.run_case(fixture, "guards.cad-core.json")
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        rejected = {
            (event["object"], event["reason"])
            for event in trace["events"]
            if event["slice"] in {"partdesign.body_tip", "sketch.producer"}
            and event["decision"] == "rejected"
        }
        self.assertIn(("BrokenBody", "body_group_missing"), rejected)
        self.assertIn(("BrokenSketch", "sketch_geometry_missing_or_not_array"), rejected)
        required_scopes = {
            event["scopeSequence"]
            for event in trace["events"]
            if event["slice"] == "scope.begin"
            and event["fields"].get("descriptor", {}).get("requiresFinalCheckpoint") is True
        }
        checkpointed = {
            event["scopeSequence"]
            for event in trace["events"]
            if event["slice"] == "maker.final_checkpoint"
        }
        self.assertTrue(required_scopes.issubset(checkpointed))

    def test_recompute_plan_matches_native_dependency_order(self) -> None:
        cases = (
            "body-addsub-replay-stops-at-tip",
            "body-delete-tip-reroute-basefeature",
            "body-tip-reroute-basefeature",
            "chained-dressup-pattern-history",
            "linear-pattern-multi-original-link-retag",
        )
        expected_inputs = {
            "body-tip-reroute-basefeature": {"Pad": ["TopSketch", "BasePad"]},
            "chained-dressup-pattern-history": {
                "DatumLine": [],
                "LinearPattern": ["DatumLine", "Chamfer"],
            },
            "linear-pattern-multi-original-link-retag": {
                "DatumLine": [],
                "LinearPattern": ["DatumLine", "Pad", "Pocket"],
            },
        }
        for case_name in cases:
            with self.subTest(case=case_name):
                fixture = semantic_fixture_path(case_name)
                _output, trace_path = self.run_case(fixture, f"{case_name}.cad-core.json")
                request = json.loads(fixture.read_text(encoding="utf-8"))
                trace = json.loads(trace_path.read_text(encoding="utf-8"))
                native_trace = json.loads(
                    (fixture.parent / "expected" / f"{case_name}.freecad.producer-trace.json").read_text(
                        encoding="utf-8"
                    )
                )
                fixture_objects = {item["Name"] for item in request["Objects"]}
                expected_order = [
                    name
                    for name in native_trace["transactions"][0]["effectiveTargets"]
                    if name in fixture_objects
                ]

                self.assertEqual(expected_order, trace["transactions"][0]["effectiveTargets"])
                plan_events = [
                    event
                    for event in trace["events"]
                    if event["slice"] == "document.recompute.plan"
                ]
                self.assertEqual(1, len(plan_events))
                self.assertEqual(expected_order, plan_events[0]["fields"]["order"])
                input_slots = {
                    event["object"]: [item["object"] for item in event["fields"]["inputSlots"]]
                    for event in trace["events"]
                    if event["slice"] == "document.object.execute.begin"
                }
                for object_name, expected_dependencies in expected_inputs.get(case_name, {}).items():
                    self.assertEqual(expected_dependencies, input_slots[object_name])

    def test_dressup_addsub_cache_is_built_only_when_transformed_consumes_it(self) -> None:
        for case_name in ("chamfer-two-distances-edge", "fillet-face-selection-history"):
            with self.subTest(case=case_name):
                fixture = semantic_fixture_path(case_name)
                _output, trace_path = self.run_case(
                    fixture, f"{case_name}.cad-core.json"
                )
                trace = json.loads(trace_path.read_text(encoding="utf-8"))
                cut_scopes = [
                    event
                    for event in trace["events"]
                    if event["slice"] == "scope.begin"
                    and event["fields"].get("stage") == "makeElementBoolean"
                    and event["fields"].get("descriptor", {}).get("operation") == "cut"
                ]
                self.assertEqual([], cut_scopes)
                selected = "Face1" if case_name.startswith("fillet") else "Edge1"
                self.assertTrue(
                    any(
                        event["slice"] == "reference.resolve"
                        and event["decision"] == "resolved"
                        and event["reason"] == "geofeature_element"
                        and event["fields"].get("old") == selected
                        for event in trace["events"]
                    )
                )

        output, trace_path = self.run_case(
            phase / "chained-dressup-pattern-history.json",
            "chained-dressup-pattern-history.cad-core.json",
        )
        response = json.loads(output.read_text(encoding="utf-8"))
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        self.assertEqual([], response["diagnostics"])
        self.assertIn("LinearPattern", {item["object"] for item in response["results"]})
        self.assertFalse(
            any(
                diagnostic.get("code") in {"missing_addsub_cache", "degraded_addsub_cache"}
                for diagnostic in response["diagnostics"]
            )
        )
        pattern_fuses = [
            event
            for event in trace["events"]
            if event["slice"] == "boolean.lifecycle"
            and event["decision"] == "begin"
            and event["fields"].get("operation") == "FUS"
            and event["fields"].get("inputCount") == "3"
        ]
        self.assertEqual(1, len(pattern_fuses))
    def test_sketch_face_maker_names_result_before_wire_joiner_handoff(self) -> None:
        fixture = ROOT / "fixtures/partdesign-body/body-addsub-replay-stops-at-tip.json"
        _output, trace_path = self.run_case(fixture, "face-maker-order.cad-core.json")
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        events = trace["events"]

        face_scopes = [
            event
            for event in events
            if event["slice"] == "scope.begin"
            and event["fields"].get("stage") == "FaceMaker::postBuild"
        ]
        self.assertTrue(face_scopes)
        for face_begin in face_scopes:
            scope_sequence = face_begin["scopeSequence"]
            face_end = next(
                event
                for event in events
                if event["slice"] == "scope.end"
                and event["scopeSequence"] == scope_sequence
            )
            maker_begins = [
                event
                for event in events
                if event["slice"] == "scope.begin"
                and event["parentScopeSequence"] == scope_sequence
                and event["producer"] == "Part::TopoShape"
            ]
            self.assertTrue(maker_begins)
            wire_begins = [
                event
                for event in events
                if event["slice"] == "scope.begin"
                and event["parentScopeSequence"] == face_begin["parentScopeSequence"]
                and event["fields"].get("stage") == "WireJoiner::getOpenWires"
                and event["sequence"] > face_begin["sequence"]
            ]
            if wire_begins:
                self.assertLess(face_end["sequence"], wire_begins[0]["sequence"])

        maker_scopes = [
            event
            for event in events
            if event["slice"] == "scope.begin"
            and event["fields"].get("stage") == "makeShapeWithElementMap"
        ]
        self.assertTrue(maker_scopes)
        self.assertTrue(all(event["producer"] == "Part::TopoShape" for event in maker_scopes))

    def test_partdesign_boolean_preserves_native_pad_subshape_identity_order(self) -> None:
        fixture = ROOT / "fixtures/partdesign-body/body-addsub-replay-stops-at-tip.json"
        _output, trace_path = self.run_case(fixture, "pad-boolean-subshape-order.cad-core.json")
        events = json.loads(trace_path.read_text(encoding="utf-8"))["events"]

        pad_map_scope = next(
            event
            for event in events
            if event["slice"] == "scope.begin"
            and event["fields"].get("stage") == "mapSubElement"
            and event["fields"].get("descriptor", {}).get("sourceOwner") == "Pad"
            and event["fields"].get("descriptor", {}).get("operation") == "CUT"
        )
        scope_sequence = pad_map_scope["scopeSequence"]
        indexed_source = None
        preserved_targets = {}
        for event in events:
            if event["scopeSequence"] != scope_sequence:
                continue
            if event["slice"] == "element_map.find_all":
                indexed_source = event["fields"].get("indexed")
            elif event["slice"] == "element_map.write" and indexed_source:
                preserved_targets.setdefault(indexed_source, event["fields"].get("target"))
                indexed_source = None

        self.assertEqual(
            {name: preserved_targets.get(name) for name in ("Edge9", "Edge10", "Edge11", "Edge12")},
            {"Edge9": "Edge11", "Edge10": "Edge16", "Edge11": "Edge9", "Edge12": "Edge10"},
        )

        boolean_begin = next(
            event
            for event in events
            if event["slice"] == "boolean.lifecycle"
            and event["decision"] == "begin"
            and event["fields"].get("operation") == "CUT"
        )
        boolean_end = next(
            event
            for event in events
            if event["slice"] == "scope.end"
            and event["scopeSequence"] == boolean_begin["scopeSequence"]
        )
        boolean_events = [
            event
            for event in events
            if boolean_begin["sequence"] < event["sequence"] < boolean_end["sequence"]
        ]
        self.assertEqual(1, sum(event["slice"] == "maker.final_checkpoint" for event in boolean_events))
        self.assertFalse(
            any(
                event["slice"] == "boolean.lifecycle" and event["decision"] == "success"
                for event in boolean_events
            )
        )

        final_property = [
            event
            for event in events
            if event["object"] == "Pocket" and event["slice"] == "property_shape.set_value"
        ][-1]
        final_refine_end = [
            event
            for event in events
            if event["slice"] == "scope.end"
            and event["fields"].get("stage") == "makeElementRefine"
            and event["sequence"] < final_property["sequence"]
        ][-1]
        self.assertEqual(
            1,
            sum(
                event["object"] == "Pocket"
                and event["slice"] == "toposhape.set_shape"
                and final_refine_end["sequence"] < event["sequence"] < final_property["sequence"]
                for event in events
            ),
        )
        self.assertTrue(
            any(
                event["object"] == "Pocket"
                and event["slice"] == "shape_slot.assign"
                and event["reason"] == "extrude_boolean_result"
                for event in events
            )
        )
        fuse_begin = next(
            event
            for event in events
            if event["slice"] == "boolean.lifecycle"
            and event["decision"] == "begin"
            and event["fields"].get("operation") == "FUS"
        )
        fuse_end = next(
            event
            for event in events
            if event["slice"] == "scope.end"
            and event["scopeSequence"] == fuse_begin["scopeSequence"]
        )
        fuse_scopes = [
            event
            for event in events
            if event["slice"] == "scope.begin"
            and fuse_begin["sequence"] < event["sequence"] < fuse_end["sequence"]
        ]
        self.assertEqual(
            1,
            sum(
                event["fields"].get("stage") == "mapSubElement"
                and event["fields"].get("descriptor", {}).get("sourceOwner")
                in {"Pocket", "PadAfterTip"}
                for event in fuse_scopes
            ),
        )
        self.assertTrue(
            any(
                event["slice"] == "toposhape.map_sub_element"
                and event["decision"] == "preserved"
                and event["reason"] == "compound_partner_child_map"
                for event in events
                if fuse_begin["sequence"] < event["sequence"] < fuse_end["sequence"]
            )
        )


if __name__ == "__main__":
    unittest.main()
