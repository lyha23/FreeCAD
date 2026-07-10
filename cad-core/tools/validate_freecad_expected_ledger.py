#!/usr/bin/env python3
from __future__ import annotations

import argparse
import glob
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
LEDGER_SCHEMA = "freecad-toponaming-ledger/v1"
TOPO_STATE_SCHEMA_VERSION = "cad-core.topo-state.v1"
TOPO_STATE_PRODUCER_CAD_CORE_VERSION = "fixture-contract-v1"
FREECAD_MAPPED_NAME_HASH_RE = re.compile(r":H(?!\*)-?[0-9A-Fa-f]+(?::[0-9A-Fa-f]+)?")
FREECAD_MAPPED_NAME_COLON_HASH_RE = re.compile(r":H:(?!\*)-?[0-9A-Fa-f]+")
FREECAD_MAPPED_NAME_DELETE_RE = re.compile(r";D(?!\*)[0-9A-Fa-f]+")

VALID_EVENT_KINDS = {
    "resolved",
    "modified",
    "generated",
    "split",
    "merged",
    "deleted",
    "ambiguous",
    "owner_changed",
    "failed_with_diagnostics",
}

TERMINAL_EVENT_KINDS = set(VALID_EVENT_KINDS)

PROJECTION_DROP_REASONS = {
    "covered_by_published_owner",
    "covered_by_body_tip",
    "covered_by_link_target",
    "covered_by_compound_child_map",
    "not_referenced",
    "deleted",
    "diagnostic_only",
    "covered_by_feature",
}

PUBLIC_TOPO_OBJECT_FIELDS = {
    "objectHash",
    "elementMapVersion",
    "subshapes",
    "elementMap",
    "childElementMaps",
    "mapperHistory",
}


def canonical_json(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def sha256_json(value: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_json(value)).hexdigest()


def canonical_freecad_mapped_name(mapped_name: str) -> str:
    def replace_hash(match: re.Match[str]) -> str:
        return ":H*:*" if match.group(0).count(":") > 1 else ":H*"

    normalized = FREECAD_MAPPED_NAME_HASH_RE.sub(replace_hash, mapped_name)
    normalized = FREECAD_MAPPED_NAME_COLON_HASH_RE.sub(":H*", normalized)
    return FREECAD_MAPPED_NAME_DELETE_RE.sub(";D*", normalized)


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ledger_path_for_expected(expected_path: Path) -> Path:
    name = expected_path.name
    if not name.endswith(".freecad.json"):
        raise ValueError(f"not a .freecad.json expected file: {expected_path}")
    stem = name[: -len(".freecad.json")]
    return expected_path.with_name(f"{stem}.freecad.ledger.json")


def fixture_case_for_expected(expected_path: Path) -> str:
    suffix = ".freecad.json"
    if not expected_path.name.endswith(suffix):
        raise ValueError(f"not a .freecad.json expected file: {expected_path}")
    return expected_path.name[: -len(suffix)]


def fixture_path_for_expected(expected_path: Path) -> Path | None:
    if expected_path.parent.name != "expected":
        return None
    return expected_path.parent.parent / f"{fixture_case_for_expected(expected_path)}.json"


def require(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def element_inventory(record: dict[str, Any], key: str) -> set[str]:
    raw = record.get(key) or {}
    result: set[str] = set()

    if isinstance(raw, dict):
        for values in raw.values():
            if isinstance(values, list):
                result.update(str(value) for value in values)
    elif isinstance(raw, list):
        result.update(str(value) for value in raw)

    return result


def dict_items(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def list_items(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def validate_unique_non_empty_string_list(
    errors: list[str],
    value: Any,
    label: str,
    *,
    required: bool = True,
) -> list[str]:
    if value is None and not required:
        return []
    if not isinstance(value, list):
        errors.append(f"{label} must be a list")
        return []
    valid = all(isinstance(item, str) and bool(item) for item in value)
    unique = valid and len(value) == len(set(value))
    require(
        errors,
        valid and unique,
        f"{label} must contain unique non-empty strings",
    )
    return [item for item in value if isinstance(item, str) and item]


def input_reference_ids(input_refs: list[Any], *, required_only: bool) -> set[str]:
    ids: set[str] = set()
    for ref in input_refs:
        if not isinstance(ref, dict):
            continue
        if not required_only or ref.get("required", True):
            ref_id = ref.get("id")
            if isinstance(ref_id, str) and ref_id:
                ids.add(ref_id)
    return ids


def bound_fixture_input_references(fixture_payload: dict[str, Any]) -> list[dict[str, Any]]:
    refs: list[dict[str, Any]] = []
    for spec in list_items(fixture_payload.get("Objects")):
        if not isinstance(spec, dict):
            continue
        owner = spec.get("Name")
        properties = spec.get("Properties")
        if not isinstance(owner, str) or not owner or not isinstance(properties, dict):
            continue

        def visit(value: Any, property_path: list[str]) -> None:
            if isinstance(value, dict):
                target_name = value.get("value")
                stable_sub_list = value.get("StableSubList")
                if isinstance(target_name, str) and isinstance(stable_sub_list, list):
                    sub_list = value.get("SubList") if isinstance(value.get("SubList"), list) else []
                    shadows = value.get("ReferenceShadow") if isinstance(value.get("ReferenceShadow"), list) else []
                    for index, stable_token in enumerate(stable_sub_list):
                        if not isinstance(stable_token, str) or not stable_token:
                            continue
                        refs.append({
                            "id": f"ref:{len(refs) + 1}",
                            "owner": owner,
                            "path": [owner, target_name],
                            "propertyPath": ".".join(property_path),
                            "target": target_name,
                            "element": stable_token,
                            "source": value.get("StableSubListSource") or "StableSubList",
                            "required": True,
                            "stableSubname": stable_token,
                            "displaySubname": sub_list[index] if index < len(sub_list) else "",
                            "hasReferenceShadow": index < len(shadows),
                        })
                for key, item in value.items():
                    visit(item, property_path + [str(key)])
            elif isinstance(value, list):
                for index, item in enumerate(value):
                    visit(item, property_path + [str(index)])

        visit(properties, [])
    return refs


def input_topo_state_rejection_code(fixture_payload: dict[str, Any]) -> str | None:
    topo_state = fixture_payload.get("topoNamingState")
    if topo_state is None:
        return None
    if not isinstance(topo_state, dict):
        return "topo_state_schema_incompatible"
    if not topo_state:
        return None
    if topo_state.get("schemaVersion") != TOPO_STATE_SCHEMA_VERSION:
        return "topo_state_schema_incompatible"

    producer = topo_state.get("producer")
    if (
        not isinstance(producer, dict)
        or producer.get("cadCoreVersion") != TOPO_STATE_PRODUCER_CAD_CORE_VERSION
    ):
        return "topo_state_producer_incompatible"

    expected_document_hash = sha256_json({
        "Objects": fixture_payload.get("Objects", []),
        "recompute": fixture_payload.get("recompute", {}),
    })
    if topo_state.get("documentHash") != expected_document_hash:
        return "topo_state_document_hash_mismatch"

    objects = topo_state.get("objects")
    if not isinstance(objects, dict):
        return "topo_state_schema_incompatible"
    specs = {
        item.get("Name"): item
        for item in list_items(fixture_payload.get("Objects"))
        if isinstance(item, dict) and isinstance(item.get("Name"), str) and item.get("Name")
    }
    for object_name, object_state in objects.items():
        if object_name not in specs or not isinstance(object_state, dict):
            return "topo_state_object_owner_incompatible"
        if object_state.get("objectHash") != sha256_json(specs[object_name]):
            return "topo_state_object_hash_mismatch"
    return None


def ref_related_objects(input_refs: list[Any]) -> set[str]:
    objects: set[str] = set()
    for ref in input_refs:
        if not isinstance(ref, dict):
            continue
        owner = ref.get("owner")
        if isinstance(owner, str) and owner:
            objects.add(owner)
        for path_item in list_items(ref.get("path")):
            if isinstance(path_item, str) and path_item:
                objects.add(path_item)
    return objects


def event_related_objects(events: list[Any]) -> set[str]:
    objects: set[str] = set()
    for event in events:
        if not isinstance(event, dict):
            continue
        for side in ("sources", "targets"):
            for endpoint in list_items(event.get(side)):
                if isinstance(endpoint, dict):
                    object_name = endpoint.get("object")
                    if isinstance(object_name, str) and object_name:
                        objects.add(object_name)
    return objects


def evidence_endpoint(value: Any) -> tuple[str, str] | None:
    if not isinstance(value, dict):
        return None
    object_name = value.get("object")
    element = value.get("subname") or value.get("element")
    if not isinstance(object_name, str) or not object_name:
        return None
    if not isinstance(element, str) or not element:
        return None
    return object_name, element


def same_stable_token(left: Any, right: Any) -> bool:
    return (
        isinstance(left, str)
        and bool(left)
        and isinstance(right, str)
        and bool(right)
        and canonical_freecad_mapped_name(left) == canonical_freecad_mapped_name(right)
    )


def input_reference_evidence_endpoints(
    input_ref: dict[str, Any],
    objects: dict[str, Any],
) -> set[tuple[str, str]]:
    target_name = input_ref.get("target") or input_ref.get("owner")
    stable_token = input_ref.get("stableSubname") or input_ref.get("element")
    if not isinstance(target_name, str) or not isinstance(stable_token, str):
        return set()

    endpoints: set[tuple[str, str]] = {(target_name, stable_token)}
    object_record = dict_items(objects.get(target_name))
    element_maps = [dict_items(object_record.get("elementMap"))]
    for child_map in list_items(object_record.get("childElementMaps")):
        if isinstance(child_map, dict):
            element_maps.append(dict_items(child_map.get("elementMap")))

    for element_map in element_maps:
        for entry_key, entry in dict_items(element_map.get("entries")).items():
            if not isinstance(entry, dict):
                continue
            mapped_name = dict_items(entry.get("mappedName"))
            candidate_tokens = (
                entry_key,
                mapped_name.get("raw"),
                mapped_name.get("canonical"),
            )
            if not any(same_stable_token(token, stable_token) for token in candidate_tokens):
                continue
            for side in ("source", "target"):
                endpoint = evidence_endpoint(entry.get(side))
                if endpoint is not None:
                    endpoints.add(endpoint)

    for history_event in list_items(object_record.get("mapperHistory")):
        if not isinstance(history_event, dict):
            continue
        mapped_name = dict_items(history_event.get("mappedName"))
        candidate_tokens: list[Any] = [
            mapped_name.get("raw"),
            mapped_name.get("canonical"),
        ]
        history_endpoints: list[tuple[str, str]] = []
        for side in ("source", "target"):
            endpoint = evidence_endpoint(history_event.get(side))
            if endpoint is not None:
                history_endpoints.append(endpoint)
                candidate_tokens.append(endpoint[1])
        if any(same_stable_token(token, stable_token) for token in candidate_tokens):
            endpoints.update(history_endpoints)
    return endpoints


def validate_event_input_reference_bindings(
    errors: list[str],
    *,
    event: dict[str, Any],
    input_refs_by_id: dict[str, dict[str, Any]],
    objects: dict[str, Any],
) -> None:
    event_id = str(event.get("id") or "<missing-event-id>")
    event_endpoints = {
        endpoint
        for side in ("sources", "targets")
        for raw_endpoint in list_items(event.get(side))
        if (endpoint := evidence_endpoint(raw_endpoint)) is not None
    }
    for ref_id in list_items(event.get("inputReferenceIds")):
        input_ref = input_refs_by_id.get(ref_id) if isinstance(ref_id, str) else None
        if input_ref is None:
            continue
        allowed_objects = {
            item
            for item in [
                input_ref.get("owner"),
                input_ref.get("target"),
                *list_items(input_ref.get("path")),
            ]
            if isinstance(item, str) and item
        }
        evidence_endpoints = input_reference_evidence_endpoints(input_ref, objects)
        require(
            errors,
            bool(event_endpoints & evidence_endpoints)
            and any(endpoint[0] in allowed_objects for endpoint in event_endpoints),
            f"{event_id}: event endpoints do not match inputReference {ref_id} element evidence",
        )


def public_object_matches_ledger_evidence(public_object: Any, ledger_object: Any) -> bool:
    if not isinstance(public_object, dict) or not isinstance(ledger_object, dict):
        return False
    if not PUBLIC_TOPO_OBJECT_FIELDS <= set(public_object):
        return False

    for key, empty_value in (
        ("elementMap", {}),
        ("childElementMaps", []),
        ("mapperHistory", []),
    ):
        if public_object.get(key, empty_value) != ledger_object.get(key, empty_value):
            return False

    public_subshapes = public_object.get("subshapes")
    if not isinstance(public_subshapes, dict):
        return False
    after_elements = element_inventory(ledger_object, "afterElements")
    return set(str(name) for name in public_subshapes) <= after_elements


def public_subshape_matches_ledger_evidence(
    object_name: str,
    subshape_name: str,
    subshape: Any,
    ledger_object: Any,
) -> bool:
    if not isinstance(subshape, dict) or not isinstance(ledger_object, dict):
        return False
    if subshape.get("subname") != subshape_name:
        return False

    identity_status = subshape.get("identityStatus")
    if identity_status == "current_only":
        return set(subshape) == {"subname", "identityStatus"}
    if identity_status != "stable":
        return False
    if set(subshape) != {
        "subname",
        "identityStatus",
        "resolvedIndexed",
        "rawFreecadMappedName",
        "canonicalFreecadMappedName",
    }:
        return False
    if subshape.get("resolvedIndexed") != subshape_name:
        return False

    element_maps = [dict_items(ledger_object.get("elementMap"))]
    for child_map in list_items(ledger_object.get("childElementMaps")):
        if isinstance(child_map, dict):
            element_maps.append(dict_items(child_map.get("elementMap")))
    for element_map in element_maps:
        for entry in dict_items(element_map.get("entries")).values():
            if not isinstance(entry, dict):
                continue
            target = dict_items(entry.get("target"))
            mapped_name = dict_items(entry.get("mappedName"))
            if (
                target.get("object") == object_name
                and target.get("subname") == subshape_name
                and mapped_name.get("raw") == subshape.get("rawFreecadMappedName")
                and mapped_name.get("canonical") == subshape.get("canonicalFreecadMappedName")
            ):
                return True
    return dict_items(ledger_object.get("subshapeEvidence")).get(subshape_name) == subshape


def validate_endpoint(
    errors: list[str],
    *,
    event_id: str,
    endpoint: Any,
    objects: dict[str, Any],
    side: str,
) -> None:
    if not isinstance(endpoint, dict):
        errors.append(f"{event_id}: {side} endpoint must be an object")
        return

    object_id = endpoint.get("object")
    element = endpoint.get("element")

    require(
        errors,
        isinstance(object_id, str) and object_id in objects,
        f"{event_id}: {side} object not found in ledger.objects: {object_id}",
    )

    if not isinstance(object_id, str) or object_id not in objects or not element:
        return

    object_record = objects[object_id]
    if not isinstance(object_record, dict):
        errors.append(f"{event_id}: ledger.objects.{object_id} must be an object")
        return

    inventory_key = "beforeElements" if side == "source" else "afterElements"
    inventory = element_inventory(object_record, inventory_key)
    require(
        errors,
        bool(inventory),
        f"{event_id}: {side} element {object_id}.{element} has no {inventory_key} inventory",
    )
    require(
        errors,
        str(element) in inventory,
        f"{event_id}: {side} element {object_id}.{element} not found in {inventory_key}",
    )


def validate_event_shape(
    errors: list[str],
    event: dict[str, Any],
    objects: dict[str, Any],
) -> set[str]:
    event_id = str(event.get("id") or "<missing-event-id>")
    kind = event.get("kind")
    covered_refs: set[str] = set()

    require(errors, bool(event.get("id")), "event missing id")
    require(errors, kind in VALID_EVENT_KINDS, f"{event_id}: invalid event kind: {kind}")

    require(errors, isinstance(event.get("sources"), list), f"{event_id}: sources must be a list")
    require(errors, isinstance(event.get("targets"), list), f"{event_id}: targets must be a list")
    sources = list_items(event.get("sources"))
    targets = list_items(event.get("targets"))
    event_input_ref_ids = validate_unique_non_empty_string_list(
        errors,
        event.get("inputReferenceIds"),
        f"{event_id}: inputReferenceIds",
    )

    if kind in {"resolved", "modified", "owner_changed"}:
        require(errors, len(sources) >= 1, f"{event_id}: {kind} event must have at least one source")
        require(errors, len(targets) >= 1, f"{event_id}: {kind} event must have at least one target")

    if kind == "split":
        require(errors, len(sources) >= 1, f"{event_id}: split event must have at least one source")
        require(errors, len(targets) >= 2, f"{event_id}: split event must have at least two targets")

    if kind == "merged":
        require(errors, len(sources) >= 2, f"{event_id}: merged event must have at least two sources")
        require(errors, len(targets) >= 1, f"{event_id}: merged event must have at least one target")

    if kind == "deleted":
        require(errors, len(sources) >= 1, f"{event_id}: deleted event must have at least one source")
        require(errors, len(targets) == 0, f"{event_id}: deleted event must not have targets")

    if kind == "generated":
        require(errors, len(targets) >= 1, f"{event_id}: generated event must have at least one target")

    if kind in {"ambiguous", "failed_with_diagnostics"}:
        has_diagnostic = bool(event.get("diagnosticIds") or event.get("diagnostics") or event.get("candidates"))
        require(errors, has_diagnostic, f"{event_id}: {kind} event must have diagnostics or candidates")

    for source in sources:
        validate_endpoint(errors, event_id=event_id, endpoint=source, objects=objects, side="source")

    for target in targets:
        validate_endpoint(errors, event_id=event_id, endpoint=target, objects=objects, side="target")

    if kind in TERMINAL_EVENT_KINDS:
        covered_refs.update(event_input_ref_ids)

    return covered_refs


def validate_hashes(
    errors: list[str],
    *,
    expected_path: Path,
    expected: dict[str, Any],
    ledger_path: Path,
    ledger: dict[str, Any],
    strict: bool,
) -> None:
    fixture = dict_items(ledger.get("fixture"))
    topo_state = expected.get("topoNamingState")

    expected_hash = fixture.get("expectedPayloadHash")
    if isinstance(expected_hash, str) and expected_hash:
        require(errors, sha256_json(expected) == expected_hash, f"expectedPayloadHash mismatch: {expected_path}")
    elif strict:
        errors.append(f"missing fixture.expectedPayloadHash: {ledger_path}")

    topo_hash = fixture.get("topoNamingStateHash")
    if isinstance(topo_state, dict):
        if isinstance(topo_hash, str) and topo_hash:
            require(errors, sha256_json(topo_state) == topo_hash, f"topoNamingStateHash mismatch: {expected_path}")
        elif strict:
            errors.append(f"missing fixture.topoNamingStateHash: {ledger_path}")
    elif topo_hash:
        errors.append(f"topoNamingStateHash present but expected has no topoNamingState: {expected_path}")


def validate_fixture_binding(
    errors: list[str],
    *,
    expected_path: Path,
    ledger: dict[str, Any],
) -> dict[str, Any] | None:
    fixture_path = fixture_path_for_expected(expected_path)
    if fixture_path is None:
        return None

    fixture = dict_items(ledger.get("fixture"))
    expected_phase = expected_path.parent.parent.name
    expected_case = fixture_case_for_expected(expected_path)
    require(
        errors,
        fixture.get("phase") == expected_phase,
        f"fixture.phase mismatch: expected {expected_phase}, got {fixture.get('phase')}",
    )
    require(
        errors,
        fixture.get("case") == expected_case,
        f"fixture.case mismatch: expected {expected_case}, got {fixture.get('case')}",
    )
    if not fixture_path.exists():
        errors.append(f"missing input fixture: {fixture_path}")
        return None

    fixture_payload = load_json(fixture_path)
    if not isinstance(fixture_payload, dict):
        errors.append(f"input fixture must be an object: {fixture_path}")
        return None
    require(
        errors,
        fixture.get("inputHash") == sha256_json(fixture_payload),
        f"fixture.inputHash mismatch: {fixture_path.name}",
    )
    return fixture_payload


def validate_public_object_fixture_binding(
    errors: list[str],
    *,
    expected: dict[str, Any],
    fixture_payload: dict[str, Any] | None,
) -> None:
    if fixture_payload is None:
        return
    specs = {
        item.get("Name"): item
        for item in list_items(fixture_payload.get("Objects"))
        if isinstance(item, dict) and isinstance(item.get("Name"), str) and item.get("Name")
    }
    topo_state = expected.get("topoNamingState")
    topo_objects = topo_state.get("objects") if isinstance(topo_state, dict) else {}
    if not isinstance(topo_objects, dict):
        return
    for object_name, object_state in topo_objects.items():
        if not isinstance(object_state, dict):
            continue
        source_spec = specs.get(object_name) or {"Name": object_name}
        require(
            errors,
            object_state.get("objectHash") == sha256_json(source_spec),
            f"public topoNamingState objectHash mismatch: {object_name}",
        )
        element_map = dict_items(object_state.get("elementMap"))
        require(
            errors,
            object_state.get("elementMapVersion") == element_map.get("encoding"),
            f"public topoNamingState elementMapVersion mismatch: {object_name}",
        )


def validate_rejected_ledger(
    errors: list[str],
    *,
    expected_path: Path,
    expected: dict[str, Any],
    ledger: dict[str, Any],
) -> None:
    diagnostics = list_items(expected.get("diagnostics"))
    rejection = dict_items(ledger.get("rejection"))
    diagnostic_codes = [
        item.get("code")
        for item in diagnostics
        if isinstance(item, dict) and isinstance(item.get("code"), str)
    ]

    require(errors, bool(diagnostic_codes), f"rejected expected must include diagnostics: {expected_path}")
    require(errors, bool(rejection), "rejected ledger must include rejection evidence")
    if rejection:
        declared_codes = set(str(item) for item in list_items(rejection.get("diagnosticCodes")))
        require(errors, bool(declared_codes), "rejection.diagnosticCodes must not be empty")
        require(
            errors,
            set(diagnostic_codes) <= declared_codes,
            "rejection.diagnosticCodes must cover expected diagnostics",
        )


def validate_projection(
    errors: list[str],
    *,
    expected_path: Path,
    expected: dict[str, Any],
    ledger: dict[str, Any],
    relevant_objects: set[str],
) -> None:
    topo_state = expected.get("topoNamingState")
    topo_objects = dict_items(topo_state.get("objects")) if isinstance(topo_state, dict) else {}
    objects = dict_items(ledger.get("objects"))
    projection = dict_items(ledger.get("projection"))
    published = dict_items(projection.get("publishedObjects"))
    dropped = dict_items(projection.get("droppedObjects"))
    event_ids = {
        event.get("id")
        for event in list_items(ledger.get("events"))
        if isinstance(event, dict) and isinstance(event.get("id"), str) and event.get("id")
    }

    require(errors, isinstance(topo_state, dict), f"expected file missing topoNamingState: {expected_path}")
    require(
        errors,
        set(published) == set(topo_objects),
        "projection.publishedObjects must exactly match public topoNamingState objects",
    )

    for published_name in topo_objects:
        require(
            errors,
            published_name in published,
            f"published topoNamingState object has no projection entry: {published_name}",
        )
        entry = dict_items(published.get(published_name))
        ledger_object = entry.get("ledgerObject")
        require(
            errors,
            isinstance(ledger_object, str) and ledger_object in objects,
            f"projection for {published_name} points to missing ledger object: {ledger_object}",
        )
        require(
            errors,
            ledger_object == published_name,
            f"projection for {published_name} must use ledger object {published_name}",
        )
        covers = validate_unique_non_empty_string_list(
            errors,
            entry.get("covers"),
            f"projection for {published_name} covers",
        )
        require(errors, bool(covers), f"projection for {published_name} must declare covered objects")
        require(
            errors,
            published_name in covers,
            f"projection for {published_name} must cover itself",
        )
        for covered_object in covers:
            require(
                errors,
                isinstance(covered_object, str) and covered_object in objects,
                f"projection for {published_name} covers missing object: {covered_object}",
            )
        source_event_ids = validate_unique_non_empty_string_list(
            errors,
            entry.get("sourceEventIds"),
            f"projection for {published_name} sourceEventIds",
            required=False,
        )
        missing_source_events = sorted(
            event_id for event_id in source_event_ids if event_id not in event_ids
        )
        require(
            errors,
            not missing_source_events,
            f"projection for {published_name} references missing source events: {missing_source_events}",
        )
        public_object = topo_objects.get(published_name)
        ledger_object_record = objects.get(ledger_object) if isinstance(ledger_object, str) else None
        require(
            errors,
            public_object_matches_ledger_evidence(public_object, ledger_object_record),
            f"public topoNamingState object does not match ledger evidence: {published_name}",
        )
        if isinstance(public_object, dict):
            for subshape_name, subshape in dict_items(public_object.get("subshapes")).items():
                require(
                    errors,
                    public_subshape_matches_ledger_evidence(
                        published_name,
                        str(subshape_name),
                        subshape,
                        ledger_object_record,
                    ),
                    f"public subshape identity does not match ledger evidence: {published_name}.{subshape_name}",
                )

    published_ledger_objects = {
        entry.get("ledgerObject")
        for entry in published.values()
        if isinstance(entry, dict) and isinstance(entry.get("ledgerObject"), str)
    }

    for object_id in sorted(relevant_objects):
        if object_id in published_ledger_objects:
            continue
        if object_id not in objects:
            errors.append(f"relevant object not found in ledger.objects: {object_id}")
            continue

        drop = dropped.get(object_id)
        require(
            errors,
            isinstance(drop, dict),
            f"relevant object is not published and has no droppedObjects explanation: {object_id}",
        )
        if isinstance(drop, dict):
            reason = drop.get("reason")
            require(
                errors,
                isinstance(reason, str) and bool(reason),
                f"dropped object must have reason: {object_id}",
            )
            if isinstance(reason, str) and reason:
                require(
                    errors,
                    reason in PROJECTION_DROP_REASONS,
                    f"dropped object has unknown reason {reason}: {object_id}",
                )
            require(
                errors,
                bool(drop.get("coveredBy") or drop.get("sourceEventIds")),
                f"dropped object must have cover/source evidence: {object_id}",
            )
            covered_by = drop.get("coveredBy")
            if isinstance(covered_by, str) and covered_by:
                require(
                    errors,
                    covered_by in published,
                    f"dropped object {object_id} is covered by unpublished object: {covered_by}",
                )
            source_event_ids = validate_unique_non_empty_string_list(
                errors,
                drop.get("sourceEventIds"),
                f"dropped object {object_id} sourceEventIds",
                required=False,
            )
            missing_source_events = sorted(
                event_id for event_id in source_event_ids if event_id not in event_ids
            )
            require(
                errors,
                not missing_source_events,
                f"dropped object {object_id} references missing source events: {missing_source_events}",
            )


def validate_round_trip(
    errors: list[str],
    *,
    expected_path: Path,
    expected: dict[str, Any],
    ledger: dict[str, Any],
    required_refs: set[str],
    strict: bool,
) -> None:
    topo_state = expected.get("topoNamingState")
    round_trip = dict_items(ledger.get("roundTrip"))
    require(errors, round_trip.get("status") == "passed", "roundTrip.status must be passed")

    rt_hash = round_trip.get("inputTopoNamingStateHash")
    if isinstance(rt_hash, str) and rt_hash:
        require(
            errors,
            isinstance(topo_state, dict) and rt_hash == sha256_json(topo_state),
            f"roundTrip.inputTopoNamingStateHash mismatch: {expected_path}",
        )
    elif strict:
        errors.append("missing roundTrip.inputTopoNamingStateHash")

    require(errors, isinstance(round_trip.get("results"), list), "roundTrip.results must be a list")
    round_trip_results = list_items(round_trip.get("results"))
    require(
        errors,
        all(
            isinstance(item, dict)
            and isinstance(item.get("inputReferenceId"), str)
            and bool(item.get("inputReferenceId"))
            and isinstance(item.get("status"), str)
            and bool(item.get("status"))
            for item in round_trip_results
        ),
        "roundTrip.results entries must contain non-empty inputReferenceId and status strings",
    )
    rt_ref_id_list = [
        item.get("inputReferenceId")
        for item in round_trip_results
        if isinstance(item, dict)
        and isinstance(item.get("inputReferenceId"), str)
        and item.get("inputReferenceId")
    ]
    rt_ref_ids = set(rt_ref_id_list)
    require(
        errors,
        rt_ref_ids == required_refs and len(rt_ref_id_list) == len(required_refs),
        "roundTrip.results must exactly cover required inputReferences",
    )
    unresolved_refs = sorted(
        item.get("inputReferenceId")
        for item in round_trip_results
        if isinstance(item, dict)
        and isinstance(item.get("inputReferenceId"), str)
        and item.get("status") != "resolved"
    )
    require(
        errors,
        not unresolved_refs,
        f"roundTrip passed with unresolved inputReferences: {unresolved_refs}",
    )


def validate_expected_file(expected_path: Path, strict: bool = True) -> list[str]:
    errors: list[str] = []
    ledger_path = ledger_path_for_expected(expected_path)

    if not ledger_path.exists():
        return [f"missing ledger sidecar: {ledger_path}"]

    expected = load_json(expected_path)
    ledger = load_json(ledger_path)

    if not isinstance(expected, dict):
        return [f"expected payload must be an object: {expected_path}"]
    if not isinstance(ledger, dict):
        return [f"ledger payload must be an object: {ledger_path}"]

    require(errors, ledger.get("schema") == LEDGER_SCHEMA, f"invalid or missing ledger.schema: {ledger_path}")
    outcome = ledger.get("outcome", "accepted")
    require(errors, outcome in {"accepted", "rejected"}, f"invalid ledger.outcome: {outcome}")

    producer = dict_items(ledger.get("producer"))
    require(errors, producer.get("name") == "FreeCADCmd", f"ledger.producer.name must be FreeCADCmd: {ledger_path}")
    if strict:
        for key in ("freecadVersion", "occtVersion", "scriptVersion"):
            require(errors, isinstance(producer.get(key), str) and bool(producer.get(key)), f"producer.{key} missing")

    validate_hashes(errors, expected_path=expected_path, expected=expected, ledger_path=ledger_path, ledger=ledger, strict=strict)
    fixture_payload = validate_fixture_binding(errors, expected_path=expected_path, ledger=ledger)
    validate_public_object_fixture_binding(errors, expected=expected, fixture_payload=fixture_payload)

    has_public_topo_state = isinstance(expected.get("topoNamingState"), dict)
    require(
        errors,
        not has_public_topo_state or outcome == "accepted",
        "expected with topoNamingState must use accepted ledger outcome",
    )
    require(
        errors,
        outcome != "accepted" or has_public_topo_state,
        "accepted ledger outcome requires expected topoNamingState",
    )

    input_rejection_code = (
        input_topo_state_rejection_code(fixture_payload)
        if fixture_payload is not None
        else None
    )
    if input_rejection_code is not None:
        expected_diagnostic_codes = {
            item.get("code")
            for item in list_items(expected.get("diagnostics"))
            if isinstance(item, dict) and isinstance(item.get("code"), str)
        }
        require(
            errors,
            outcome == "rejected",
            f"accepted ledger binds invalid input topoNamingState: {input_rejection_code}",
        )
        require(
            errors,
            input_rejection_code in expected_diagnostic_codes,
            f"expected diagnostics do not match input topoNamingState rejection: {input_rejection_code}",
        )

    input_refs = list_items(ledger.get("inputReferences"))
    require(errors, isinstance(ledger.get("inputReferences"), list), f"ledger.inputReferences must be a list: {ledger_path}")
    if fixture_payload is not None:
        require(
            errors,
            input_refs == bound_fixture_input_references(fixture_payload),
            "ledger.inputReferences do not match bound fixture references",
        )
    seen_ref_ids: set[str] = set()
    for ref in input_refs:
        ref_id = ref.get("id") if isinstance(ref, dict) else None
        if not isinstance(ref_id, str) or not ref_id:
            continue
        require(errors, ref_id not in seen_ref_ids, f"duplicate inputReference id: {ref_id}")
        seen_ref_ids.add(ref_id)
    all_refs = input_reference_ids(input_refs, required_only=False)
    required_refs = input_reference_ids(input_refs, required_only=True)

    if strict:
        require(errors, all(isinstance(ref, dict) and ref.get("id") for ref in input_refs), "every inputReference must have an id")

    if outcome == "rejected":
        validate_rejected_ledger(errors, expected_path=expected_path, expected=expected, ledger=ledger)
        return errors

    objects = dict_items(ledger.get("objects"))
    events = list_items(ledger.get("events"))
    require(errors, isinstance(ledger.get("objects"), dict), f"ledger.objects must be an object map: {ledger_path}")
    require(errors, isinstance(ledger.get("events"), list), f"ledger.events must be a list: {ledger_path}")

    event_ids: set[str] = set()
    covered_refs: set[str] = set()
    input_refs_by_id = {
        ref.get("id"): ref
        for ref in input_refs
        if isinstance(ref, dict) and isinstance(ref.get("id"), str) and ref.get("id")
    }
    for event in events:
        if not isinstance(event, dict):
            errors.append(f"event must be an object: {ledger_path}")
            continue
        event_id = event.get("id")
        if isinstance(event_id, str) and event_id:
            require(errors, event_id not in event_ids, f"duplicate event id: {event_id}")
            event_ids.add(event_id)
        covered_refs |= validate_event_shape(errors, event, objects)
        validate_event_input_reference_bindings(
            errors,
            event=event,
            input_refs_by_id=input_refs_by_id,
            objects=objects,
        )

    missing_refs = sorted(all_refs - covered_refs)
    require(errors, not missing_refs, f"inputReferences not covered by terminal events: {missing_refs}")
    undeclared_refs = sorted(covered_refs - all_refs)
    require(
        errors,
        not undeclared_refs,
        f"events reference undeclared inputReferences: {undeclared_refs}",
    )

    coverage = dict_items(ledger.get("coverage"))
    require(errors, isinstance(ledger.get("coverage"), dict), "ledger.coverage must be an object")
    covered_declared = coverage.get("coveredInputReferenceIds")
    uncovered_declared = coverage.get("uncoveredInputReferenceIds")
    declared_covered_ref_list = validate_unique_non_empty_string_list(
        errors,
        covered_declared,
        "coverage.coveredInputReferenceIds",
    )
    declared_uncovered_ref_list = validate_unique_non_empty_string_list(
        errors,
        uncovered_declared,
        "coverage.uncoveredInputReferenceIds",
    )
    declared_covered_refs = set(declared_covered_ref_list)
    declared_uncovered_refs = set(declared_uncovered_ref_list)
    require(
        errors,
        declared_covered_refs == covered_refs,
        "coverage.coveredInputReferenceIds must match terminal event coverage",
    )
    require(
        errors,
        declared_uncovered_refs == all_refs - covered_refs,
        "coverage.uncoveredInputReferenceIds must match uncovered inputReferences",
    )
    require(
        errors,
        not declared_uncovered_refs,
        f"coverage.uncoveredInputReferenceIds must be empty: {sorted(declared_uncovered_refs)}",
    )

    relevant_objects = ref_related_objects(input_refs) | event_related_objects(events)
    validate_projection(errors, expected_path=expected_path, expected=expected, ledger=ledger, relevant_objects=relevant_objects)
    validate_round_trip(errors, expected_path=expected_path, expected=expected, ledger=ledger, required_refs=required_refs, strict=strict)

    return errors


def expected_paths_from_patterns(patterns: list[str]) -> list[Path]:
    paths: list[Path] = []
    for pattern in patterns:
        for raw_path in glob.glob(pattern, recursive=True):
            path = Path(raw_path)
            if path.name.endswith(".freecad.ledger.json"):
                continue
            if path.name.endswith(".freecad.json"):
                paths.append(path)
    return sorted(set(paths))


def default_phase_pattern(phase: str) -> str:
    return str(ROOT / "fixtures" / phase / "expected" / "*.freecad.json")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate *.freecad.json expected files against FreeCADCmd sidecar ledgers."
    )
    parser.add_argument("patterns", nargs="*", help='Expected file glob patterns, e.g. "fixtures/*/expected/*.freecad.json".')
    parser.add_argument("--phase", help="Validate cad-core/fixtures/<phase>/expected/*.freecad.json.")
    parser.add_argument("--all", action="store_true", help="Validate every cad-core/fixtures/*/expected/*.freecad.json.")
    parser.add_argument("--strict", action="store_true", help="Require all v1 authority fields and hashes.")
    args = parser.parse_args(argv)

    patterns = list(args.patterns)
    if args.phase:
        patterns.append(default_phase_pattern(args.phase))
    if args.all:
        patterns.append(str(ROOT / "fixtures" / "*" / "expected" / "*.freecad.json"))
    if not patterns:
        parser.error("provide patterns, --phase, or --all")

    paths = expected_paths_from_patterns(patterns)
    if not paths:
        print("No expected *.freecad.json files found.", file=sys.stderr)
        return 2

    failures: dict[Path, list[str]] = {}
    for path in paths:
        errors = validate_expected_file(path, strict=args.strict)
        if errors:
            failures[path] = errors
            print(f"FAIL {path}")
            for error in errors:
                print(f"  - {error}")
        else:
            print(f"OK   {path}")

    if failures:
        print(f"\nvalidated={len(paths)} failed={len(failures)} errors={sum(len(v) for v in failures.values())}", file=sys.stderr)
        return 1

    print(f"\nValidated {len(paths)} expected fixture(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
