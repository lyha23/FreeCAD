from __future__ import annotations

import ctypes
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

try:
    from .fixture_runner import ROOT
except ImportError:  # pragma: no cover - supports unittest discovery.
    from fixture_runner import ROOT


TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from compare_freecad_expected import parse_args
from freecad_expected_parity import EvaluationRequest, evaluate
from freecad_expected_parity.catalog import FixtureCase, ROLES_SCHEMA
from freecad_expected_parity.registry import REGISTRY_SCHEMA
from freecad_expected_parity.sources import CadRsBuffer, CadRsResult, RustFfiActualSource


class _FakeFunction:
    def __init__(self, callback: object) -> None:
        self.callback = callback
        self.argtypes: object | None = None
        self.restype: object | None = None
        self.calls: list[tuple[object, ...]] = []

    def __call__(self, *args: object) -> object:
        self.calls.append(args)
        return self.callback(*args)  # type: ignore[operator]


class _FakeLibrary:
    def __init__(self, result: CadRsResult, buffers: list[ctypes.Array[ctypes.c_char]]) -> None:
        self._buffers = buffers
        self.cad_rs_recompute_json = _FakeFunction(lambda *_: result)
        self.cad_rs_free_result = _FakeFunction(lambda *_: None)


def _result(status: int, payload: bytes = b"", error: bytes = b"") -> tuple[CadRsResult, list[ctypes.Array[ctypes.c_char]]]:
    buffers = [ctypes.create_string_buffer(payload), ctypes.create_string_buffer(error)]
    result = CadRsResult(
        status=status,
        json=CadRsBuffer(ctypes.cast(buffers[0], ctypes.c_void_p), len(payload)),
        error=CadRsBuffer(ctypes.cast(buffers[1], ctypes.c_void_p), len(error)),
    )
    return result, buffers


class RustFfiExpectedSourceTest(unittest.TestCase):
    def fixture_case(self, root: Path) -> FixtureCase:
        input_path = root / "fixtures" / "demo" / "case-a.json"
        input_path.parent.mkdir(parents=True)
        input_path.write_bytes(b'{"request":"demo"}')
        return FixtureCase(
            phase="demo",
            case="case-a",
            role="native",
            input_path=input_path,
            expected_path=root / "fixtures" / "demo" / "expected" / "case-a.freecad.json",
            ledger_path=root / "fixtures" / "demo" / "expected" / "case-a.freecad.ledger.json",
            current_path=root / "fixtures" / "demo" / "cad-core-res" / "case-a.cad-core.json",
        )

    def test_status_zero_calls_rust_ffi_once_and_returns_its_inner_payload(self) -> None:
        payload = b'{"diagnostics":[],"results":[]}'
        result, buffers = _result(0, payload)
        request = b'{"request":"demo"}'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ffi_path = root / "libcad_core_ffi.dylib"
            ffi_path.touch()
            item = self.fixture_case(root)
            library = _FakeLibrary(result, buffers)
            with patch("freecad_expected_parity.sources.ctypes.CDLL", return_value=library) as cdll:
                actual = RustFfiActualSource(ffi_path).load(item)

        self.assertEqual({"diagnostics": [], "results": []}, actual.payload)
        self.assertIsNone(actual.error)
        self.assertEqual(request, library.cad_rs_recompute_json.calls[0][0])
        self.assertEqual(len(request), library.cad_rs_recompute_json.calls[0][1])
        self.assertEqual(1, len(library.cad_rs_recompute_json.calls))
        self.assertEqual(1, len(library.cad_rs_free_result.calls))
        cdll.assert_called_once_with(str(ffi_path))

    def test_nonzero_ffi_status_is_an_actual_source_error_not_a_payload(self) -> None:
        result, buffers = _result(7, b'{"diagnostics":[]}', b'{"code":"CAD_RS_FAILURE"}')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ffi_path = root / "libcad_core_ffi.dylib"
            ffi_path.touch()
            item = self.fixture_case(root)
            library = _FakeLibrary(result, buffers)
            with patch("freecad_expected_parity.sources.ctypes.CDLL", return_value=library):
                actual = RustFfiActualSource(ffi_path).load(item)

        self.assertIsNone(actual.payload)
        self.assertIn("returned status 7", actual.error or "")
        self.assertIn("CAD_RS_FAILURE", actual.error or "")
        self.assertEqual(1, len(library.cad_rs_free_result.calls))

    def test_rust_ffi_runs_through_existing_engine_without_live_current_freshness_policy(self) -> None:
        expected = {"diagnostics": [], "results": []}
        payload = json.dumps(expected).encode("utf-8")
        result, buffers = _result(0, payload)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            item = self.fixture_case(root)
            item.expected_path.parent.mkdir(parents=True)
            item.expected_path.write_text(json.dumps(expected), encoding="utf-8")
            item.ledger_path.write_text("{}", encoding="utf-8")
            item.current_path.parent.mkdir(parents=True)
            item.current_path.write_text('{"not":"the Rust payload"}', encoding="utf-8")
            roles = root / "fixture_roles.v1.json"
            roles.write_text(
                json.dumps(
                    {
                        "schemaVersion": ROLES_SCHEMA,
                        "legacyNativeExpectedDiscovery": False,
                        "requireCompleteInputCoverage": True,
                        "roles": [{"phase": "demo", "case": "case-a", "role": "native"}],
                    }
                ),
                encoding="utf-8",
            )
            registry = root / "protocol_divergences.v1.json"
            registry.write_text(json.dumps({"schemaVersion": REGISTRY_SCHEMA, "entries": []}), encoding="utf-8")
            ffi_path = root / "libcad_core_ffi.dylib"
            ffi_path.touch()
            library = _FakeLibrary(result, buffers)
            with patch("freecad_expected_parity.sources.ctypes.CDLL", return_value=library):
                report = evaluate(EvaluationRequest(root=root, phase="demo", source_kind="rust-ffi", ffi_library=ffi_path, roles_path=roles, registry_path=registry, validate_ledger=False)).to_dict()

        self.assertEqual("green", report["releaseStatus"])
        self.assertTrue(report["releaseGatePassed"])
        self.assertEqual("rust-ffi", report["selection"]["sourceKind"])
        self.assertEqual(1, len(library.cad_rs_recompute_json.calls))

    def test_cli_accepts_explicit_rust_ffi_source_and_library(self) -> None:
        args = parse_args(
            [
                "--phase",
                "c4m6",
                "--release-gate",
                "--actual-source",
                "rust-ffi",
                "--ffi-lib",
                "/tmp/libcad_core_ffi.dylib",
            ]
        )

        self.assertEqual("rust-ffi", args.actual_source)
        self.assertEqual(Path("/tmp/libcad_core_ffi.dylib"), args.ffi_lib)


if __name__ == "__main__":  # pragma: no cover
    unittest.main()
