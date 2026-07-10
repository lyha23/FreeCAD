"""Exact, fail-closed protocol-divergence registry handling."""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REGISTRY_SCHEMA = "cad-core.freecad-expected-protocol-divergences.v1"
SELECTOR_FIELDS = ("phase", "case", "category", "kind", "path")
CONTRACT_TEST_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+$")
CONTRACT_FIELDS = {"type", "keysMode", "requiredKeys", "properties", "items", "const"}
SELECTOR_PATTERN_CHARS = set("*?[](){}|+^$\\")


@dataclass
class Registry:
    entries: list[dict[str, Any]]
    errors: list[str]
    path: Path
    sha256: str | None


def _sha256(raw: bytes) -> str:
    return "sha256:" + hashlib.sha256(raw).hexdigest()


def _value_type(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, (int, float)):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    return type(value).__name__


def _contract_errors(contract: Any, context: str = "actualContract") -> list[str]:
    if not isinstance(contract, dict):
        return [f"{context} must be an object"]
    errors: list[str] = []
    unknown = sorted(set(contract) - CONTRACT_FIELDS)
    if unknown:
        errors.append(f"{context} has unknown field(s): {unknown}")
    kind = contract.get("type")
    if kind not in {"object", "array", "string", "number", "boolean", "null"}:
        errors.append(f"{context}.type is invalid")
        return errors
    if "const" in contract and _value_type(contract["const"]) != kind:
        errors.append(f"{context}.const does not match {context}.type")

    keys_mode = contract.get("keysMode")
    required = contract.get("requiredKeys")
    properties = contract.get("properties")
    object_keywords = ("keysMode", "requiredKeys", "properties")
    if any(keyword in contract for keyword in object_keywords) and kind != "object":
        errors.append(f"{context} object-only fields require type object")
    if kind == "object":
        if keys_mode is not None and keys_mode not in {"exact", "required"}:
            errors.append(f"{context}.keysMode is invalid")
        if keys_mode is not None and required is None:
            errors.append(f"{context}.keysMode requires requiredKeys")
        if required is not None and (
            not isinstance(required, list) or any(not isinstance(item, str) or not item for item in required)
        ):
            errors.append(f"{context}.requiredKeys must be a string list")
        if isinstance(required, list) and len(required) != len(set(required)):
            errors.append(f"{context}.requiredKeys must not contain duplicates")
    if properties is not None and kind == "object":
        if not isinstance(properties, dict):
            errors.append(f"{context}.properties must be an object")
        else:
            if isinstance(required, list):
                unrequired = sorted(set(properties) - set(required))
                if unrequired:
                    errors.append(f"{context}.properties are not required keys: {unrequired}")
            for key, child in properties.items():
                errors.extend(_contract_errors(child, f"{context}.properties.{key}"))
    if "items" in contract and kind != "array":
        errors.append(f"{context}.items requires type array")
    elif "items" in contract:
        errors.extend(_contract_errors(contract["items"], f"{context}.items"))
    return errors


def _entry_errors(entry: Any, index: int) -> list[str]:
    context = f"registry entry {index}"
    if not isinstance(entry, dict):
        return [f"{context} must be an object"]
    errors: list[str] = []
    if not isinstance(entry.get("id"), str) or not entry["id"]:
        errors.append(f"{context} missing id")
    selector = entry.get("selector")
    if not isinstance(selector, dict):
        errors.append(f"{context} selector must be an object")
    else:
        if set(selector) != set(SELECTOR_FIELDS):
            errors.append(f"{context} selector must contain exactly {'/'.join(SELECTOR_FIELDS)}")
        for field in SELECTOR_FIELDS:
            if not isinstance(selector.get(field), str) or not selector[field]:
                errors.append(f"{context} selector.{field} must be non-empty")
            elif any(character in selector[field] for character in SELECTOR_PATTERN_CHARS):
                errors.append(f"{context} selector.{field} must be an exact literal, not a pattern")
    errors.extend(_contract_errors(entry.get("actualContract"), f"{context}.actualContract"))
    for field in ("nativeExpected", "cadCoreProtocol", "frontendImpact", "authority", "removeWhen"):
        if not isinstance(entry.get(field), str) or not entry[field]:
            errors.append(f"{context} missing {field}")
    tests = entry.get("contractTests")
    if not isinstance(tests, list) or not tests:
        errors.append(f"{context} contractTests must be a non-empty list")
    elif any(not isinstance(test, str) or not CONTRACT_TEST_RE.fullmatch(test) for test in tests):
        errors.append(f"{context} contractTests contain invalid dotted id")
    return errors


def load_registry(path: Path) -> Registry:
    if not path.exists():
        return Registry([], [f"missing protocol divergence registry: {path}"], path, None)
    try:
        raw = path.read_bytes()
        payload = json.loads(raw)
    except (OSError, json.JSONDecodeError) as exc:
        return Registry([], [f"invalid protocol divergence registry {path}: {exc}"], path, None)
    sha256 = _sha256(raw)
    if not isinstance(payload, dict):
        return Registry([], [f"protocol divergence registry must be an object: {path}"], path, sha256)
    errors: list[str] = []
    if payload.get("schemaVersion") != REGISTRY_SCHEMA:
        errors.append(f"invalid protocol divergence registry schemaVersion: {path}")
    entries = payload.get("entries", payload.get("divergences"))
    if not isinstance(entries, list):
        errors.append("protocol divergence registry entries must be a list")
        return Registry([], errors, path, sha256)
    result: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    seen_selectors: set[tuple[str, ...]] = set()
    for index, entry in enumerate(entries):
        entry_errors = _entry_errors(entry, index)
        errors.extend(entry_errors)
        if entry_errors or not isinstance(entry, dict):
            continue
        entry_id = str(entry["id"])
        selector = entry["selector"]
        selector_key = tuple(str(selector[field]) for field in SELECTOR_FIELDS)
        if entry_id in seen_ids:
            errors.append(f"duplicate protocol divergence id: {entry_id}")
        if selector_key in seen_selectors:
            errors.append(f"duplicate protocol divergence selector: {'/'.join(selector_key)}")
        seen_ids.add(entry_id)
        seen_selectors.add(selector_key)
        result.append(entry)
    return Registry(result, errors, path, sha256)


def validate_actual_contract(value: Any, contract: dict[str, Any], path: str = "actual") -> list[str]:
    errors: list[str] = []
    required_type = contract.get("type")
    if _value_type(value) != required_type:
        return [f"{path}.type expected {required_type}, got {_value_type(value)}"]
    if "const" in contract and value != contract["const"]:
        return [f"{path}.value expected {contract['const']!r}, got {value!r}"]
    if isinstance(value, dict):
        required_keys = contract.get("requiredKeys", [])
        if isinstance(required_keys, list):
            required = set(required_keys)
            if contract.get("keysMode") == "exact" and set(value) != required:
                errors.append(f"{path}.keys expected exact {sorted(required)}, got {sorted(value)}")
            elif not required <= set(value):
                errors.append(f"{path}.keys missing {sorted(required - set(value))}")
        properties = contract.get("properties")
        if isinstance(properties, dict):
            for key, child_contract in properties.items():
                if key not in value:
                    errors.append(f"{path}.{key} missing")
                elif isinstance(child_contract, dict):
                    errors.extend(validate_actual_contract(value[key], child_contract, f"{path}.{key}"))
    if isinstance(value, list) and isinstance(contract.get("items"), dict):
        for index, child in enumerate(value):
            errors.extend(validate_actual_contract(child, contract["items"], f"{path}[{index}]"))
    return errors


def _in_scope(selector: dict[str, Any], phase: str | None, case: str | None) -> bool:
    return (phase is None or selector.get("phase") == phase) and (case is None or selector.get("case") == case)


def apply_registry(
    registry: Registry,
    diffs: list[dict[str, Any]],
    *,
    phase: str | None,
    case: str | None,
) -> dict[str, Any]:
    """Annotate diffs and return an audit; invalid configuration never accepts a diff."""

    selected = [entry for entry in registry.entries if _in_scope(entry["selector"], phase, case)]
    consumed: dict[str, int] = {str(entry["id"]): 0 for entry in selected}
    ambiguous: list[str] = []
    contract_failures: list[dict[str, Any]] = []
    for diff in diffs:
        selector = tuple(str(diff.get(field, "")) for field in SELECTOR_FIELDS)
        matches = [
            entry
            for entry in selected
            if selector == tuple(str(entry["selector"].get(field, "")) for field in SELECTOR_FIELDS)
        ]
        if len(matches) == 0:
            diff["decision"] = "unaccepted_diff"
            diff["accepted"] = False
            continue
        if len(matches) != 1:
            label = "/".join(selector)
            ambiguous.append(label)
            diff["decision"] = "ambiguous_registry_match"
            diff["accepted"] = False
            continue
        entry = matches[0]
        entry_id = str(entry["id"])
        consumed[entry_id] += 1
        contract_errors = validate_actual_contract(diff.get("_actualValue"), entry["actualContract"])
        diff["registryId"] = entry_id
        if contract_errors:
            contract_failures.append({"id": entry_id, "path": diff.get("path"), "errors": contract_errors})
            diff["decision"] = "registry_contract_failed"
            diff["accepted"] = False
        else:
            diff["decision"] = "approved_protocol_divergence"
            diff["accepted"] = True
            diff["contractTests"] = entry["contractTests"]
    stale = sorted(entry_id for entry_id, count in consumed.items() if count != 1)
    errors = [*registry.errors]
    if ambiguous:
        errors.extend(f"ambiguous registry match: {label}" for label in sorted(set(ambiguous)))
    if stale:
        errors.extend(f"stale or multiply-consumed registry entry: {entry_id}" for entry_id in stale)
    selected_contract_tests = sorted(
        {test for entry in selected for test in entry.get("contractTests", []) if isinstance(test, str)}
    )
    consumed_contract_tests = sorted(
        {
            test
            for entry in selected
            if consumed.get(str(entry["id"]), 0) > 0
            for test in entry.get("contractTests", [])
            if isinstance(test, str)
        }
    )
    return {
        "path": str(registry.path),
        "sha256": registry.sha256,
        "valid": not errors,
        "validationErrors": errors,
        "selectedEntries": [str(entry["id"]) for entry in selected],
        "contractTests": selected_contract_tests,
        "consumedContractTests": consumed_contract_tests,
        "consumedEntries": dict(sorted(consumed.items())),
        "staleEntries": stale,
        "contractFailures": contract_failures,
    }
