from __future__ import annotations

import hashlib
import json
import subprocess
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
from freecad_expected_parity.catalog import FixtureCase
from freecad_expected_parity.sources import FreeCadKernelV2ActualSource


def _sha256(raw: bytes) -> str:
    return "sha256:" + hashlib.sha256(raw).hexdigest()


class FreeCadKernelV2ExpectedSourceTest(unittest.TestCase):
    def test_helper_process_returns_live_payload_and_lifecycle_evidence(self) -> None:
        response = b'{"diagnostics":[],"results":[]}'
        request = b'{"Objects":[],"recompute":{"objs":[]}}'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "lib" / "libfreecad_kernel_ffi_v2.dylib"
            runner = root / "bin" / "FreeCADKernelV2ActualRunner"
            input_path = root / "fixture.json"
            library.parent.mkdir(parents=True)
            runner.parent.mkdir(parents=True)
            library.write_bytes(b"test-v2-library")
            runner.touch()
            input_path.write_bytes(request)
            library = library.resolve()
            runner = runner.resolve()
            item = FixtureCase(
                phase="p8",
                case="part-box",
                role="native",
                input_path=input_path,
                expected_path=root / "expected.json",
                ledger_path=root / "expected.ledger.json",
                current_path=root / "current.json",
            )

            def run_helper(command: list[str], **_: object) -> subprocess.CompletedProcess[str]:
                self.assertEqual(str(runner), command[0])
                self.assertEqual(str(library), command[1])
                response_path = Path(command[4])
                evidence_path = Path(command[5])
                response_path.write_bytes(response)
                evidence_path.write_text(
                    json.dumps(
                        {
                            "schemaVersion": "freecad-kernel-v2.actual-runner.v1",
                            "helperVersion": "FreeCADKernelV2ActualRunner.v1",
                            "libraryPath": str(library),
                            "librarySha256": _sha256(library.read_bytes()),
                            "configSha256": "sha256:config",
                            "requestSha256": _sha256(request),
                            "rawResponseSha256": _sha256(response),
                            "createCount": 1,
                            "destroyCount": 1,
                            "handleCreated": True,
                            "handleDestroyed": True,
                            "createResultFreed": True,
                            "versionResultFreed": True,
                            "recomputeResultFreed": True,
                            "createStatus": 0,
                            "versionStatus": 0,
                            "recomputeStatus": 0,
                            "abiVersion": 2,
                        }
                    ),
                    encoding="utf-8",
                )
                return subprocess.CompletedProcess(command, 0, "", "")

            source_authority = {
                "status": "recorded",
                "repository": "/source/FreeCAD2",
                "head": "0123456789abcdef",
                "dirtyPaths": [" M freecad-kernel/src/CadKernel.cpp"],
                "sourceSnapshot": {
                    "path": "/source/source-snapshot.json",
                    "sha256": "sha256:snapshot",
                },
            }
            with patch.object(
                FreeCadKernelV2ActualSource,
                "_source_authority",
                return_value=source_authority,
            ):
                source = FreeCadKernelV2ActualSource(library)
            with patch(
                "freecad_expected_parity.sources.subprocess.run", side_effect=run_helper
            ), patch("freecad_expected_parity.sources.ctypes.CDLL") as cdll:
                actual = source.load(item)

        self.assertEqual({"diagnostics": [], "results": []}, actual.payload)
        self.assertIsNone(actual.error)
        cdll.assert_not_called()
        evidence = source.evidence()
        self.assertEqual("isolated-helper-process", evidence["execution"])
        self.assertEqual(1, evidence["createCount"])
        self.assertEqual(1, evidence["destroyCount"])
        self.assertEqual(2, evidence["calls"][0]["abiVersion"])
        self.assertEqual("0123456789abcdef", evidence["sourceAuthority"]["head"])
        self.assertEqual(
            [" M freecad-kernel/src/CadKernel.cpp"],
            evidence["sourceAuthority"]["dirtyPaths"],
        )

    def test_cli_accepts_explicit_v2_source_and_library(self) -> None:
        args = parse_args(
            [
                "--actual-source",
                "freecad-kernel-v2",
                "--ffi-lib",
                "/tmp/libfreecad_kernel_ffi_v2.dylib",
            ]
        )
        self.assertEqual("freecad-kernel-v2", args.actual_source)
        self.assertEqual(Path("/tmp/libfreecad_kernel_ffi_v2.dylib"), args.ffi_lib)


if __name__ == "__main__":  # pragma: no cover
    unittest.main()
