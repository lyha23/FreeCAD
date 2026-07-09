from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = CAD_CORE_ROOT / "tools" / "validate_freecad_expected_ledger.py"
SPEC = importlib.util.spec_from_file_location("validate_freecad_expected_ledger", SCRIPT_PATH)
assert SPEC is not None
validator = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


class FreecadExpectedLedgerIntegrityTest(unittest.TestCase):
    def test_sidecar_ledger_validates_expected_projection(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {
                    "Body": {}
                }
            }
        }
        topo_hash = validator.sha256_json(expected["topoNamingState"])
        expected_hash = validator.sha256_json(expected)
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": expected_hash,
                "topoNamingStateHash": topo_hash,
            },
            "inputReferences": [
                {
                    "id": "ref:1",
                    "owner": "Body",
                    "path": ["Body", "Pad"],
                    "element": "Face3",
                    "source": "StableSubList",
                    "required": True,
                }
            ],
            "objects": {
                "Body": {
                    "published": True,
                    "beforeElements": {
                        "Face": ["Face3"]
                    },
                    "afterElements": {
                        "Face": ["Face5", "Face6"]
                    },
                },
                "Pad": {
                    "published": False,
                    "beforeElements": {
                        "Face": ["Face3"]
                    },
                    "afterElements": {
                        "Face": ["Face5", "Face6"]
                    },
                },
            },
            "events": [
                {
                    "id": "event:1",
                    "kind": "split",
                    "sources": [
                        {
                            "object": "Pad",
                            "element": "Face3",
                        }
                    ],
                    "targets": [
                        {
                            "object": "Pad",
                            "element": "Face5",
                        },
                        {
                            "object": "Pad",
                            "element": "Face6",
                        },
                    ],
                    "inputReferenceIds": ["ref:1"],
                }
            ],
            "projection": {
                "publishedObjects": {
                    "Body": {
                        "ledgerObject": "Body",
                        "covers": ["Body", "Pad"],
                    }
                },
                "droppedObjects": {
                    "Pad": {
                        "reason": "covered_by_body_tip",
                        "coveredBy": "Body",
                        "sourceEventIds": ["event:1"],
                    }
                },
            },
            "coverage": {
                "coveredInputReferenceIds": ["ref:1"],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": topo_hash,
                "results": [
                    {
                        "inputReferenceId": "ref:1",
                        "status": "resolved",
                    }
                ],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            self.assertEqual([], validator.validate_expected_file(expected_path, strict=True))

    def test_missing_sidecar_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            expected_path.write_text("{}", encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertEqual([f"missing ledger sidecar: {expected_path.with_name('case.freecad.ledger.json')}"], errors)

    def test_expected_hash_mismatch_is_error(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {}
            }
        }
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": "sha256:not-the-current-payload",
                "topoNamingStateHash": validator.sha256_json(expected["topoNamingState"]),
            },
            "inputReferences": [],
            "objects": {},
            "events": [],
            "projection": {
                "publishedObjects": {},
                "droppedObjects": {},
            },
            "coverage": {
                "coveredInputReferenceIds": [],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": validator.sha256_json(expected["topoNamingState"]),
                "results": [],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertIn(f"expectedPayloadHash mismatch: {expected_path}", errors)

    def test_every_input_reference_must_have_event_conclusion(self) -> None:
        expected = {
            "topoNamingState": {
                "objects": {
                    "Body": {}
                }
            }
        }
        topo_hash = validator.sha256_json(expected["topoNamingState"])
        ledger = {
            "schema": "freecad-toponaming-ledger/v1",
            "producer": {
                "name": "FreeCADCmd",
                "freecadVersion": "1.0",
                "occtVersion": "7.8",
                "scriptVersion": "test",
            },
            "fixture": {
                "expectedPayloadHash": validator.sha256_json(expected),
                "topoNamingStateHash": topo_hash,
            },
            "inputReferences": [
                {
                    "id": "ref:optional",
                    "owner": "Body",
                    "path": ["Body"],
                    "element": "Face3",
                    "required": False,
                }
            ],
            "objects": {
                "Body": {
                    "published": True,
                }
            },
            "events": [],
            "projection": {
                "publishedObjects": {
                    "Body": {
                        "ledgerObject": "Body",
                        "covers": ["Body"],
                    }
                },
                "droppedObjects": {},
            },
            "coverage": {
                "coveredInputReferenceIds": [],
                "uncoveredInputReferenceIds": [],
            },
            "roundTrip": {
                "status": "passed",
                "inputTopoNamingStateHash": topo_hash,
                "results": [],
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            expected_path = Path(temp_dir) / "case.freecad.json"
            ledger_path = Path(temp_dir) / "case.freecad.ledger.json"
            expected_path.write_text(validator.json.dumps(expected, ensure_ascii=False), encoding="utf-8")
            ledger_path.write_text(validator.json.dumps(ledger, ensure_ascii=False), encoding="utf-8")

            errors = validator.validate_expected_file(expected_path, strict=True)

        self.assertIn("inputReferences not covered by terminal events: ['ref:optional']", errors)


if __name__ == "__main__":
    unittest.main()
