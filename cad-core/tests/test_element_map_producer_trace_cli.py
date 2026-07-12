"""CLI publication and preflight-failure tests for producer-trace sidecars."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.element_map_producer_trace import validate_trace


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
        self.run_case(ROOT / "fixtures" / "p5" / "sketch-open-wire-internal-empty.json", "open.cad-core.json")

    def test_default_sidecar_path_for_plain_output(self) -> None:
        self.run_case(ROOT / "fixtures" / "p5" / "sketch-open-wire-internal-empty.json", "open.json")

    def test_preflight_rejection_has_closed_abort_transaction(self) -> None:
        output, trace_path = self.run_case(
            ROOT / "fixtures" / "c4m6" / "topo-state-schema-incompatible.json",
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
            ROOT / "fixtures" / "p5" / "sketch-open-wire-internal-empty.json",
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
                str(ROOT / "fixtures" / "p5" / "sketch-open-wire-internal-empty.json"),
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


if __name__ == "__main__":
    unittest.main()
