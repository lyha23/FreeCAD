#!/usr/bin/env python3
from __future__ import annotations

import argparse
import glob
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
LEDGER_SCHEMA = "freecad-toponaming-ledger/v1"

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


def canonical_json(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def sha256_json(value: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_json(value)).hexdigest()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ledger_path_for_expected(expected_path: Path) -> Path:
    name = expected_path.name
    if not name.endswith(".freecad.json"):
        raise ValueError(f"not a .freecad.json expected file: {expected_path}")
    stem = name[: -len(".freecad.json")]
    return expected_path.with_name(f"{stem}.freecad.ledger.json")


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

    sources = list_items(event.get("sources"))
    targets = list_items(event.get("targets"))

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
        for ref_id in list_items(event.get("inputReferenceIds")):
            if isinstance(ref_id, str) and ref_id:
                covered_refs.add(ref_id)

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
    if isinstance(topo_hash, str) and topo_hash:
        require(
            errors,
            isinstance(topo_state, dict) and sha256_json(topo_state) == topo_hash,
            f"topoNamingStateHash mismatch: {expected_path}",
        )
    elif strict:
        errors.append(f"missing fixture.topoNamingStateHash: {ledger_path}")


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

    require(errors, isinstance(topo_state, dict), f"expected file missing topoNamingState: {expected_path}")

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
        covers = list_items(entry.get("covers"))
        require(errors, bool(covers), f"projection for {published_name} must declare covered objects")
        for covered_object in covers:
            require(
                errors,
                isinstance(covered_object, str) and covered_object in objects,
                f"projection for {published_name} covers missing object: {covered_object}",
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
        object_record = objects[object_id]
        if isinstance(object_record, dict) and object_record.get("published") is True:
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

    round_trip_results = list_items(round_trip.get("results"))
    rt_ref_ids = {
        item.get("inputReferenceId")
        for item in round_trip_results
        if isinstance(item, dict) and isinstance(item.get("inputReferenceId"), str)
    }
    require(errors, required_refs <= rt_ref_ids, "roundTrip.results does not cover all required inputReferences")


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

    producer = dict_items(ledger.get("producer"))
    require(errors, producer.get("name") == "FreeCADCmd", f"ledger.producer.name must be FreeCADCmd: {ledger_path}")
    if strict:
        for key in ("freecadVersion", "occtVersion", "scriptVersion"):
            require(errors, isinstance(producer.get(key), str) and bool(producer.get(key)), f"producer.{key} missing")

    validate_hashes(errors, expected_path=expected_path, expected=expected, ledger_path=ledger_path, ledger=ledger, strict=strict)

    input_refs = list_items(ledger.get("inputReferences"))
    require(errors, isinstance(ledger.get("inputReferences"), list), f"ledger.inputReferences must be a list: {ledger_path}")
    all_refs = input_reference_ids(input_refs, required_only=False)
    required_refs = input_reference_ids(input_refs, required_only=True)

    if strict:
        require(errors, all(isinstance(ref, dict) and ref.get("id") for ref in input_refs), "every inputReference must have an id")

    objects = dict_items(ledger.get("objects"))
    events = list_items(ledger.get("events"))
    require(errors, isinstance(ledger.get("objects"), dict), f"ledger.objects must be an object map: {ledger_path}")
    require(errors, isinstance(ledger.get("events"), list), f"ledger.events must be a list: {ledger_path}")

    event_ids: set[str] = set()
    covered_refs: set[str] = set()
    for event in events:
        if not isinstance(event, dict):
            errors.append(f"event must be an object: {ledger_path}")
            continue
        event_id = event.get("id")
        if isinstance(event_id, str) and event_id:
            require(errors, event_id not in event_ids, f"duplicate event id: {event_id}")
            event_ids.add(event_id)
        covered_refs |= validate_event_shape(errors, event, objects)

    missing_refs = sorted(all_refs - covered_refs)
    require(errors, not missing_refs, f"inputReferences not covered by terminal events: {missing_refs}")

    coverage = dict_items(ledger.get("coverage"))
    uncovered = coverage.get("uncoveredInputReferenceIds") or []
    require(errors, not uncovered, f"coverage.uncoveredInputReferenceIds must be empty: {uncovered}")
    covered_declared = coverage.get("coveredInputReferenceIds")
    if isinstance(covered_declared, list) and covered_declared:
        require(
            errors,
            all_refs <= set(str(item) for item in covered_declared),
            "coverage.coveredInputReferenceIds does not cover all inputReferences",
        )
    elif strict and all_refs:
        errors.append("coverage.coveredInputReferenceIds missing")

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
