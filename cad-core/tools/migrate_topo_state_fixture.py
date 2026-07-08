#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator


SCHEMA_VERSION = "cad-core.topo-state.v1"
ELEMENT_MAP_VERSION = "cad-core.element-map.v1"
DEFAULT_CAD_CORE_VERSION = "fixture-contract-v1"
DEFAULT_FREECAD_VERSION = "1.2.0 revision 20260519"
DEFAULT_OCCT_VERSION = "fixture-occt-unspecified"

INTERNAL_FACE_RE = re.compile(r"^InternalFace([1-9]\d*)$")
SKETCH_FACE_TOKEN_RE = re.compile(r"^g([1-9]\d*);SKT;FAC$")


class MigrationError(RuntimeError):
    pass


@dataclass
class MigrationStats:
    rewritten_links: int = 0
    state_updates: int = 0
    element_map_entries: int = 0

    @property
    def changed(self) -> bool:
        return self.rewritten_links > 0 or self.state_updates > 0 or self.element_map_entries > 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Migrate legacy fixture InternalFaceN references into topoNamingState-backed "
            "StableSubList entries."
        )
    )
    parser.add_argument("inputs", nargs="+", type=Path, help="Fixture JSON files or directories.")
    parser.add_argument("--out", type=Path, help="Write a single converted fixture to this path.")
    parser.add_argument("--in-place", action="store_true", help="Overwrite each input fixture atomically.")
    parser.add_argument("--dry-run", action="store_true", help="Report what would change without writing files.")
    parser.add_argument(
        "--skip-unsupported",
        action="store_true",
        help="Continue with exit code 0 when a fixture is not supported by this mechanical migration.",
    )
    parser.add_argument(
        "--include-expected",
        action="store_true",
        help="Also scan files under expected/ directories. Default is to migrate fixture inputs only.",
    )
    parser.add_argument(
        "--ensure-state",
        action="store_true",
        help="Add a topoNamingState skeleton to supported DocumentObject fixtures even when no link is migrated.",
    )
    parser.add_argument(
        "--history-mode",
        choices=("minimal", "simple-sketch"),
        default="minimal",
        help=(
            "minimal writes only the stable ElementMap entry. simple-sketch also writes a synthetic "
            "FaceMakerBuildFace mapperHistory event per sketch edge for simple closed-sketch fixtures."
        ),
    )
    parser.add_argument(
        "--cad-core-version",
        default=DEFAULT_CAD_CORE_VERSION,
        help="producer.cadCoreVersion for newly created topoNamingState.",
    )
    parser.add_argument(
        "--freecad-version",
        default=DEFAULT_FREECAD_VERSION,
        help="producer.freecadVersion for newly created topoNamingState.",
    )
    parser.add_argument(
        "--occt-version",
        default=DEFAULT_OCCT_VERSION,
        help="producer.occtVersion for newly created topoNamingState.",
    )
    args = parser.parse_args(argv)
    if args.out and len(expand_input_paths(args.inputs, include_expected=args.include_expected)) != 1:
        parser.error("--out requires exactly one input fixture")
    if args.out and args.in_place:
        parser.error("--out and --in-place are mutually exclusive")
    if args.dry_run and args.out:
        parser.error("--dry-run and --out are mutually exclusive")
    return args


def is_expected_path(path: Path) -> bool:
    return "expected" in path.parts


def expand_input_paths(paths: list[Path], *, include_expected: bool) -> list[Path]:
    fixtures: list[Path] = []
    for path in paths:
        if path.is_dir():
            fixtures.extend(
                sorted(
                    child
                    for child in path.rglob("*.json")
                    if child.is_file() and (include_expected or not is_expected_path(child))
                )
            )
        else:
            if not include_expected and is_expected_path(path):
                continue
            fixtures.append(path)
    return fixtures


def load_json(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise MigrationError(f"{path}: fixture root must be a JSON object")
    return payload


def write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)


def semantic_hash(value: Any) -> str:
    canonical = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return "sha256:" + hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def object_specs(fixture: dict[str, Any]) -> dict[str, dict[str, Any]]:
    objects = fixture.get("Objects")
    if not isinstance(objects, list):
        raise MigrationError("fixture.Objects must be an array")
    specs: dict[str, dict[str, Any]] = {}
    for spec in objects:
        if not isinstance(spec, dict):
            continue
        name = spec.get("Name")
        if isinstance(name, str) and name:
            specs[name] = spec
    return specs


def iter_link_items(fixture: dict[str, Any]) -> Iterator[tuple[str, str, dict[str, Any]]]:
    objects = fixture.get("Objects")
    if not isinstance(objects, list):
        return
    for spec in objects:
        if not isinstance(spec, dict):
            continue
        object_name = str(spec.get("Name") or "<unnamed>")
        properties = spec.get("Properties")
        if not isinstance(properties, dict):
            continue
        for property_name, property_value in properties.items():
            if not isinstance(property_value, dict):
                continue
            property_type = property_value.get("PropertyType")
            if property_type == "App::PropertyLinkSubList":
                sub_set = property_value.get("SubSet")
                if isinstance(sub_set, list):
                    for item in sub_set:
                        if isinstance(item, dict):
                            yield object_name, str(property_name), item
            elif property_type == "App::PropertyLinkSub":
                yield object_name, str(property_name), property_value


def token_for_internal_face(subname: str) -> str | None:
    match = INTERNAL_FACE_RE.match(subname)
    if match is None:
        return None
    return f"g{match.group(1)};SKT;FAC"


def internal_face_for_token(token: str) -> str | None:
    match = SKETCH_FACE_TOKEN_RE.match(token)
    if match is None:
        return None
    return f"InternalFace{match.group(1)}"


def adoptable_existing_sketch_face_token(token: Any) -> str | None:
    if token == "g1;SKT;FAC":
        return "InternalFace1"
    return None


def validate_sketch_target(specs: dict[str, dict[str, Any]], target_name: str) -> dict[str, Any]:
    target = specs.get(target_name)
    if target is None:
        raise MigrationError(f"Link target {target_name!r} is not present in fixture.Objects")
    if target.get("TypeId") != "Sketcher::SketchObject":
        raise MigrationError(
            f"Link target {target_name!r} is {target.get('TypeId')!r}; only Sketcher::SketchObject "
            "InternalFaceN references can be migrated to gN;SKT;FAC"
        )
    return target


def blank_or_absent_stable_list(value: Any, expected_len: int) -> bool:
    if value is None:
        return True
    if value == []:
        return True
    if isinstance(value, list) and len(value) == expected_len:
        return all(item == "" or item is None for item in value)
    return False


def normalize_link_item(
    item: dict[str, Any],
    specs: dict[str, dict[str, Any]],
    required_entries: dict[str, dict[str, str]],
    object_name: str,
    property_name: str,
) -> bool:
    target_name = item.get("value")
    if not isinstance(target_name, str) or not target_name:
        return False

    stable_source = item.get("StableSubListSource")
    stable_sub_list = item.get("StableSubList")
    if stable_source == "topoNamingState" and isinstance(stable_sub_list, list):
        changed = False
        for token in stable_sub_list:
            if not isinstance(token, str) or not token:
                continue
            internal_face = internal_face_for_token(token)
            if internal_face is None:
                continue
            validate_sketch_target(specs, target_name)
            required_entries.setdefault(target_name, {})[internal_face] = token
        sub_list = item.get("SubList")
        if isinstance(sub_list, list) and sub_list:
            internal_tokens = [token_for_internal_face(str(subname)) for subname in sub_list]
            if all(token is not None for token in internal_tokens):
                item["SubList"] = []
                changed = True
        return changed

    sub_list = item.get("SubList")
    if stable_source in (None, "") and isinstance(stable_sub_list, list) and sub_list == []:
        target_subnames = [adoptable_existing_sketch_face_token(token) for token in stable_sub_list]
        if target_subnames and all(target_subname is not None for target_subname in target_subnames):
            validate_sketch_target(specs, target_name)
            item["StableSubListSource"] = "topoNamingState"
            for target_subname, token in zip(target_subnames, stable_sub_list):
                required_entries.setdefault(target_name, {})[str(target_subname)] = str(token)
            return True

    if not isinstance(sub_list, list) or not sub_list:
        return False

    tokens: list[str] = []
    for subname in sub_list:
        if not isinstance(subname, str):
            return False
        token = token_for_internal_face(subname)
        if token is None:
            return False
        tokens.append(token)

    if not blank_or_absent_stable_list(stable_sub_list, len(tokens)):
        raise MigrationError(
            f"{object_name}.{property_name} target {target_name!r} has both legacy InternalFaceN "
            "SubList and non-empty StableSubList; refusing to guess the intended stable mapping"
        )

    validate_sketch_target(specs, target_name)
    item["SubList"] = []
    item["StableSubList"] = tokens
    item["StableSubListSource"] = "topoNamingState"
    for subname, token in zip(sub_list, tokens):
        required_entries.setdefault(target_name, {})[str(subname)] = token
    return True


def ensure_topo_state(fixture: dict[str, Any], args: argparse.Namespace) -> dict[str, Any]:
    state = fixture.get("topoNamingState")
    if not isinstance(state, dict):
        state = {}
        fixture["topoNamingState"] = state

    state["schemaVersion"] = SCHEMA_VERSION
    producer = state.get("producer")
    if not isinstance(producer, dict):
        producer = {}
        state["producer"] = producer
    producer.setdefault("cadCoreVersion", args.cad_core_version)
    producer.setdefault("freecadVersion", args.freecad_version)
    producer.setdefault("occtVersion", args.occt_version)

    objects = state.get("objects")
    if not isinstance(objects, dict):
        state["objects"] = {}
    return state


def sketch_source_edges(sketch_spec: dict[str, Any]) -> list[str]:
    properties = sketch_spec.get("Properties")
    geometry = properties.get("Geometry") if isinstance(properties, dict) else None
    if not isinstance(geometry, list):
        return []
    edges: list[str] = []
    for index, item in enumerate(geometry, start=1):
        edge_id = item.get("id") if isinstance(item, dict) else None
        if isinstance(edge_id, int) and edge_id > 0:
            edges.append(f"Edge{edge_id}")
        else:
            edges.append(f"Edge{index}")
    return edges


def mapper_history_id(object_name: str, source_edge: str, target_subname: str) -> str:
    source_suffix = source_edge.lower().replace("edge", "edge", 1)
    target_suffix = target_subname.lower().replace("internalface", "internal-face", 1)
    return f"{object_name}.mh-{source_suffix}-{target_suffix}"


def mapper_history_events(
    object_name: str,
    sketch_spec: dict[str, Any],
    target_subname: str,
) -> list[dict[str, Any]]:
    source_edges = sketch_source_edges(sketch_spec)
    events: list[dict[str, Any]] = []
    for source_edge in source_edges:
        events.append(
            {
                "id": mapper_history_id(object_name, source_edge, target_subname),
                "source": {
                    "object": object_name,
                    "subname": source_edge,
                },
                "target": {
                    "object": object_name,
                    "subname": target_subname,
                },
                "shape_kind": "face",
                "relation": "generated",
                "maker_stage": "FaceMakerBuildFace",
                "evidence": {
                    "source_edges": source_edges,
                },
                "recoverability": "resolved",
                "diagnostic_status": "",
            }
        )
    return events


def merge_mapper_history(existing: list[Any], additions: list[dict[str, Any]]) -> list[Any]:
    seen = {item.get("id") for item in existing if isinstance(item, dict)}
    merged = list(existing)
    for item in additions:
        if item["id"] not in seen:
            merged.append(item)
            seen.add(item["id"])
    return merged


def subshape_entry_matches(entry: Any, target_subname: str, stable_token: str) -> bool:
    if not isinstance(entry, dict):
        return False
    return (
        entry.get("subname") == target_subname
        and entry.get("rawFreecadMappedName") == stable_token
        and entry.get("canonicalFreecadMappedName") == stable_token
        and entry.get("resolvedIndexed") == target_subname
        and entry.get("identityStatus") == "stable"
    )


def element_map_entry_matches(entry: Any, object_name: str, target_subname: str, stable_token: str) -> bool:
    if not isinstance(entry, dict):
        return False
    target = entry.get("target")
    mapped_name = entry.get("mappedName")
    evidence = entry.get("evidence")
    return (
        isinstance(target, dict)
        and target.get("object") == object_name
        and target.get("subname") == target_subname
        and entry.get("shapeKind") == "face"
        and isinstance(mapped_name, dict)
        and mapped_name.get("raw") == stable_token
        and mapped_name.get("canonical") == stable_token
        and entry.get("recoverability") == "resolved"
        and isinstance(evidence, dict)
    )


def assert_no_entry_target_conflict(entry: Any, object_name: str, target_subname: str, stable_token: str) -> None:
    if not isinstance(entry, dict):
        return
    target = entry.get("target")
    if not isinstance(target, dict):
        return
    existing_object = target.get("object")
    existing_subname = target.get("subname")
    if existing_object is None and existing_subname is None:
        return
    if existing_object != object_name or existing_subname != target_subname:
        raise MigrationError(
            f"Existing elementMap entry {stable_token!r} targets "
            f"{existing_object!r}.{existing_subname!r}, expected {object_name!r}.{target_subname!r}"
        )


def ensure_object_state(
    state_objects: dict[str, Any],
    object_name: str,
    sketch_spec: dict[str, Any],
    subname_to_token: dict[str, str],
    history_mode: str,
) -> tuple[bool, int]:
    object_changed = False
    entry_changes = 0
    object_state = state_objects.get(object_name)
    if not isinstance(object_state, dict):
        object_state = {}
        state_objects[object_name] = object_state
        object_changed = True

    object_hash = semantic_hash(sketch_spec)
    if object_state.get("objectHash") != object_hash:
        object_state["objectHash"] = object_hash
        object_changed = True
    if object_state.get("elementMapVersion") != ELEMENT_MAP_VERSION:
        object_state["elementMapVersion"] = ELEMENT_MAP_VERSION
        object_changed = True

    subshapes = object_state.get("subshapes")
    if not isinstance(subshapes, dict):
        subshapes = {}
        object_state["subshapes"] = subshapes
        object_changed = True

    element_map = object_state.get("elementMap")
    if not isinstance(element_map, dict):
        element_map = {}
        object_state["elementMap"] = element_map
        object_changed = True
    if element_map.get("encoding") != ELEMENT_MAP_VERSION:
        element_map["encoding"] = ELEMENT_MAP_VERSION
        object_changed = True
    entries = element_map.get("entries")
    if not isinstance(entries, dict):
        entries = {}
        element_map["entries"] = entries
        object_changed = True

    child_maps = object_state.get("childElementMaps")
    if not isinstance(child_maps, list):
        object_state["childElementMaps"] = []
        object_changed = True

    mapper_history = object_state.get("mapperHistory")
    if not isinstance(mapper_history, list):
        mapper_history = []
        object_state["mapperHistory"] = mapper_history
        object_changed = True

    for target_subname, stable_token in sorted(subname_to_token.items()):
        history_ids: list[str] = []
        if history_mode == "simple-sketch":
            additions = mapper_history_events(object_name, sketch_spec, target_subname)
            mapper_history = merge_mapper_history(mapper_history, additions)
            object_state["mapperHistory"] = mapper_history
            history_ids = [item["id"] for item in additions]
            if additions:
                object_changed = True

        subshape_payload = {
            "subname": target_subname,
            "rawFreecadMappedName": stable_token,
            "canonicalFreecadMappedName": stable_token,
            "resolvedIndexed": target_subname,
            "identityStatus": "stable",
        }
        if not subshape_entry_matches(subshapes.get(target_subname), target_subname, stable_token):
            subshapes[target_subname] = subshape_payload
            object_changed = True

        entry_payload = {
            "target": {
                "object": object_name,
                "subname": target_subname,
            },
            "shapeKind": "face",
            "source": {
                "object": object_name,
                "subname": target_subname,
            },
            "mappedName": {
                "raw": stable_token,
                "canonical": stable_token,
            },
            "recoverability": "resolved",
            "evidence": {
                "source": "element_map" if history_ids else "legacy_internal_face_fixture_migration",
                "mapperHistoryIds": history_ids,
                "childElementMapKey": None,
            },
        }
        existing_entry = entries.get(stable_token)
        assert_no_entry_target_conflict(existing_entry, object_name, target_subname, stable_token)
        if not element_map_entry_matches(existing_entry, object_name, target_subname, stable_token):
            entries[stable_token] = entry_payload
            object_changed = True
            entry_changes += 1

    status = "history_partial" if mapper_history else "indexed_only"
    if element_map.get("status") != status:
        element_map["status"] = status
        object_changed = True

    return object_changed, entry_changes


def migrate_fixture(fixture: dict[str, Any], args: argparse.Namespace) -> tuple[dict[str, Any], MigrationStats]:
    migrated = copy.deepcopy(fixture)
    stats = MigrationStats()
    specs = object_specs(migrated)
    required_entries: dict[str, dict[str, str]] = {}

    for object_name, property_name, item in iter_link_items(migrated):
        try:
            if normalize_link_item(item, specs, required_entries, object_name, property_name):
                stats.rewritten_links += 1
        except MigrationError:
            if not (args.ensure_state and args.skip_unsupported):
                raise

    if required_entries or args.ensure_state:
        state_before = copy.deepcopy(migrated.get("topoNamingState"))
        state = ensure_topo_state(migrated, args)
        state_objects = state.get("objects")
        if not isinstance(state_objects, dict):
            raise MigrationError("topoNamingState.objects must be an object")
        for object_name, subname_to_token in sorted(required_entries.items()):
            changed, entry_changes = ensure_object_state(
                state_objects,
                object_name,
                specs[object_name],
                subname_to_token,
                args.history_mode,
            )
            if changed:
                stats.state_updates += 1
            stats.element_map_entries += entry_changes
        document_hash = semantic_hash(
            {
                "Objects": migrated.get("Objects", []),
                "recompute": migrated.get("recompute", {}),
            }
        )
        if state.get("documentHash") != document_hash:
            state["documentHash"] = document_hash
            stats.state_updates += 1
        if not required_entries and migrated.get("topoNamingState") != state_before:
            stats.state_updates += 1

    return migrated, stats


def output_path_for(input_path: Path, args: argparse.Namespace) -> Path | None:
    if args.in_place:
        return input_path
    if args.out:
        return args.out
    return None


def run_one(path: Path, args: argparse.Namespace) -> int:
    fixture = load_json(path)
    migrated, stats = migrate_fixture(fixture, args)
    destination = output_path_for(path, args)

    if args.dry_run:
        print(
            f"{path}: rewritten_links={stats.rewritten_links} "
            f"state_updates={stats.state_updates} element_map_entries={stats.element_map_entries}"
        )
        return 0

    if destination is None:
        print(json.dumps(migrated, ensure_ascii=False, indent=2))
        return 0

    if migrated == fixture:
        print(f"{path}: already normalized")
        return 0

    write_json_atomic(destination, migrated)
    print(
        f"{path}: wrote {destination} "
        f"(rewritten_links={stats.rewritten_links}, element_map_entries={stats.element_map_entries})"
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    status = 0
    for path in expand_input_paths(args.inputs, include_expected=args.include_expected):
        try:
            status = max(status, run_one(path, args))
        except MigrationError as exc:
            level = "skipped" if args.skip_unsupported else "error"
            print(f"{path}: {level}: {exc}", file=sys.stderr)
            if not args.skip_unsupported:
                status = 1
    return status


if __name__ == "__main__":
    raise SystemExit(main())
