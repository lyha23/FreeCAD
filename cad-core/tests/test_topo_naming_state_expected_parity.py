"""Full topoNamingState parity against native FreeCAD expected fixtures.

This is intentionally stricter than test_topo_naming_state_response.py. The
focused response tests are useful smoke tests, but they are not evidence that
all FreeCAD expected topoNamingState payloads match. Every discovered
`fixtures/<group>/expected/*.freecad.json` file with a topoNamingState payload
is treated as the authority here.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any

try:
    from .fixture_expected import discover_expected_cases
    from .fixture_runner import BIN, ROOT
except ImportError:  # pragma: no cover - supports `unittest discover tests`.
    from fixture_expected import discover_expected_cases
    from fixture_runner import BIN, ROOT


FREECAD_MAPPED_NAME_HASH_RE = re.compile(r":H(?!\*)-?[0-9A-Fa-f]+(?::[0-9A-Fa-f]+)?")
FREECAD_MAPPED_NAME_COLON_HASH_RE = re.compile(r":H:(?!\*)-?[0-9A-Fa-f]+")
FREECAD_MAPPED_NAME_DELETE_RE = re.compile(r";D(?!\*)[0-9A-Fa-f]+")
TOPO_INDEX_NAME_RE = re.compile(
    r"^(InternalFace|InternalEdge|InternalVertex|Face|Edge|Vertex|Wire|Shell|Solid|Compound)\d+$"
)
RESPONSE_SUBSHAPE_IDENTITY_FIELDS = (
    "kind",
    "indexed",
    "subname",
    "stableSubname",
    "identityStatus",
    "fullSubname",
    "rawFreecadMappedName",
    "canonicalFreecadMappedName",
    "resolvedIndexed",
    "sourceStableSubname",
    "fragmentStableSubname",
    "sourceGeometryKind",
)
RESPONSE_SUBSHAPE_IDENTITY_COMPARE_ORDER = (
    "stableSubname",
    "subname",
    "rawFreecadMappedName",
    "canonicalFreecadMappedName",
    "resolvedIndexed",
    "identityStatus",
    "fullSubname",
    "kind",
    "indexed",
    "sourceStableSubname",
    "fragmentStableSubname",
    "sourceGeometryKind",
)
MISSING_FIELD = "<missing>"
MAX_RESPONSE_IDENTITY_DIFFERENCES = 20


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def canonical_freecad_mapped_name(mapped_name: str) -> str:
    def replace_hash(match: re.Match[str]) -> str:
        return ":H*:*" if match.group(0).count(":") > 1 else ":H*"

    normalized = FREECAD_MAPPED_NAME_HASH_RE.sub(replace_hash, mapped_name)
    normalized = FREECAD_MAPPED_NAME_COLON_HASH_RE.sub(":H*", normalized)
    return FREECAD_MAPPED_NAME_DELETE_RE.sub(";D*", normalized)


def canonicalize_freecad_mapped_names_and_keys(value: Any) -> Any:
    if isinstance(value, str):
        canonical = canonical_freecad_mapped_name(value)
        return TOPO_INDEX_NAME_RE.sub(lambda match: f"{match.group(1)}*", canonical)
    if isinstance(value, list):
        return [canonicalize_freecad_mapped_names_and_keys(item) for item in value]
    if isinstance(value, dict):
        return {
            canonical_freecad_mapped_name(str(key)): canonicalize_freecad_mapped_names_and_keys(item)
            for key, item in value.items()
        }
    return value


def comparable_topo_naming_state(state: dict[str, Any]) -> dict[str, Any]:
    comparable = canonicalize_freecad_mapped_names_and_keys(state)
    if not isinstance(comparable, dict):
        return {}
    producer = comparable.get("producer")
    if isinstance(producer, dict):
        producer = dict(producer)
        producer["freecadVersion"] = "*"
        producer["occtVersion"] = "*"
        comparable = dict(comparable)
        comparable["producer"] = producer
    return comparable


def topo_state_expected_cases() -> list[tuple[str, str, Path]]:
    group_filter = os.environ.get("CAD_CORE_TOPO_STATE_PARITY_GROUP")
    fixture_filter = os.environ.get("CAD_CORE_TOPO_STATE_PARITY_FIXTURE")
    cases: list[tuple[str, str, Path]] = []
    for group, fixture, expected_path in discover_expected_cases():
        if group_filter and group != group_filter:
            continue
        if fixture_filter and fixture != fixture_filter:
            continue
        expected = load_json(expected_path)
        if "known_gap" in expected:
            continue
        if "topoNamingState" not in expected:
            continue
        cases.append((group, fixture, expected_path))
    return cases


def first_difference(actual: Any, expected: Any, path: str = "topoNamingState") -> str | None:
    if type(actual) is not type(expected):
        return (
            f"{path}: type differs: actual={type(actual).__name__} "
            f"expected={type(expected).__name__}"
        )
    if isinstance(expected, dict):
        actual_keys = set(actual)
        expected_keys = set(expected)
        if actual_keys != expected_keys:
            missing = sorted(expected_keys - actual_keys)
            extra = sorted(actual_keys - expected_keys)
            return f"{path}: key set differs: missing={missing} extra={extra}"
        for key in sorted(expected):
            diff = first_difference(actual[key], expected[key], f"{path}.{key}")
            if diff is not None:
                return diff
        return None
    if isinstance(expected, list):
        if len(actual) != len(expected):
            return f"{path}: length differs: actual={len(actual)} expected={len(expected)}"
        for index, expected_item in enumerate(expected):
            diff = first_difference(actual[index], expected_item, f"{path}[{index}]")
            if diff is not None:
                return diff
        return None
    if actual != expected:
        return f"{path}: value differs: actual={actual!r} expected={expected!r}"
    return None


def topo_naming_state_difference(
    actual_response: dict[str, Any],
    expected_response: dict[str, Any],
) -> str | None:
    if "topoNamingState" not in actual_response:
        return "topoNamingState: missing from actual response"
    return first_difference(
        comparable_topo_naming_state(actual_response["topoNamingState"]),
        comparable_topo_naming_state(expected_response["topoNamingState"]),
    )


def response_subshape_identity_index(response: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for object_result in response.get("results", []):
        if not isinstance(object_result, dict):
            continue
        object_name = object_result.get("object")
        if not isinstance(object_name, str) or not object_name:
            continue
        for subshape in object_result.get("subshapes", []):
            if not isinstance(subshape, dict):
                continue
            indexed = subshape.get("indexed")
            if not isinstance(indexed, str) or not indexed:
                continue
            identity = {
                field: subshape[field]
                for field in RESPONSE_SUBSHAPE_IDENTITY_FIELDS
                if field in subshape
            }
            result[f"{object_name}:{indexed}"] = identity
    return canonicalize_freecad_mapped_names_and_keys(result)


def response_subshape_identity_difference(
    actual_response: dict[str, Any],
    expected_response: dict[str, Any],
) -> str | None:
    actual = response_subshape_identity_index(actual_response)
    expected = response_subshape_identity_index(expected_response)
    differences: list[str] = []
    if set(actual) != set(expected):
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        differences.append(f"results.subshapes.identity: key set differs: missing={missing} extra={extra}")
    for field in RESPONSE_SUBSHAPE_IDENTITY_COMPARE_ORDER:
        missing_subshapes = [
            subshape_key
            for subshape_key in sorted(expected)
            if subshape_key in actual
            and field in expected[subshape_key]
            and field not in actual[subshape_key]
        ]
        if missing_subshapes:
            examples = missing_subshapes[:5]
            suffix = "" if len(missing_subshapes) <= len(examples) else f" (+{len(missing_subshapes) - len(examples)} more)"
            differences.append(
                f"results.subshapes.identity.{field}: missing from actual on "
                f"{len(missing_subshapes)} subshapes: {examples}{suffix}"
            )
    for subshape_key in sorted(expected):
        if subshape_key not in actual:
            continue
        actual_identity = actual[subshape_key]
        expected_identity = expected[subshape_key]
        for field in RESPONSE_SUBSHAPE_IDENTITY_COMPARE_ORDER:
            actual_value = actual_identity.get(field, MISSING_FIELD)
            expected_value = expected_identity.get(field, MISSING_FIELD)
            if actual_value != expected_value:
                if actual_value == MISSING_FIELD and expected_value != MISSING_FIELD:
                    continue
                differences.append(
                    f"results.subshapes.identity.{subshape_key}.{field}: value differs: "
                    f"actual={actual_value!r} expected={expected_value!r}"
                )
                if len(differences) >= MAX_RESPONSE_IDENTITY_DIFFERENCES:
                    return "; ".join(differences)
        extra_fields = sorted(set(actual_identity) - set(RESPONSE_SUBSHAPE_IDENTITY_COMPARE_ORDER))
        if extra_fields:
            differences.append(
                f"results.subshapes.identity.{subshape_key}: unexpected fields={extra_fields}"
            )
            if len(differences) >= MAX_RESPONSE_IDENTITY_DIFFERENCES:
                return "; ".join(differences)
    return "; ".join(differences) if differences else None


def native_expected_parity_difference(
    actual_response: dict[str, Any],
    expected_response: dict[str, Any],
) -> str | None:
    differences = []
    topo_difference = topo_naming_state_difference(actual_response, expected_response)
    if topo_difference is not None:
        differences.append(topo_difference)
    response_difference = response_subshape_identity_difference(actual_response, expected_response)
    if response_difference is not None:
        differences.append(response_difference)
    return "; ".join(differences) if differences else None


def parity_report(passed: list[str], failed: list[tuple[str, str]]) -> str:
    lines = [
        "topoNamingState native expected parity report",
        "authority=fixtures/<group>/expected/*.freecad.json",
        f"passed={len(passed)} failed={len(failed)}",
        "",
        "PASSED:",
    ]
    lines.extend(f"  - {case}" for case in passed)
    lines.extend(["", "FAILED:"])
    lines.extend(f"  - {case}: {reason}" for case, reason in failed)
    return "\n".join(lines)


class TopoNamingStateExpectedParityTest(unittest.TestCase):
    def run_official_recompute(self, group: str, fixture: str) -> dict[str, Any]:
        input_path = ROOT / "fixtures" / group / f"{fixture}.json"
        with tempfile.TemporaryDirectory() as tmp:
            output_path = Path(tmp) / f"{fixture}.result.json"
            env = os.environ.copy()
            env.pop("CAD_CORE_TEST_LEGACY_OUTPUT", None)
            subprocess.run(
                [
                    str(BIN),
                    "recompute",
                    str(input_path),
                    "--output",
                    str(output_path),
                ],
                cwd=ROOT,
                check=True,
                env=env,
            )
            return load_json(output_path)

    def test_all_native_expected_topo_naming_states_match_freecad(self) -> None:
        cases = topo_state_expected_cases()
        self.assertGreater(
            len(cases),
            0,
            "No topoNamingState native expected fixtures were discovered",
        )

        passed: list[str] = []
        failed: list[tuple[str, str]] = []
        for group, fixture, expected_path in cases:
            case_name = f"{group}/{fixture}"
            try:
                expected = load_json(expected_path)
                actual = self.run_official_recompute(group, fixture)
                diff = native_expected_parity_difference(actual, expected)
            except Exception as exc:  # pragma: no cover - reported through unittest failure.
                failed.append((case_name, f"{exc.__class__.__name__}: {exc}"))
                continue

            if diff is None:
                passed.append(case_name)
            else:
                failed.append((case_name, diff))

        report = parity_report(passed, failed)
        if failed:
            self.fail(report)
        print(report)


if __name__ == "__main__":
    unittest.main()
