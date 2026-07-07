#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


ID_FIELDS = ("id", "Id", "geometryId")


def read_geometry_id(item: dict[str, Any]) -> int | None:
    for field in ID_FIELDS:
        if field not in item:
            continue
        value = item[field]
        if isinstance(value, dict) and "value" in value:
            value = value["value"]
        if isinstance(value, int) and not isinstance(value, bool) and value > 0:
            return value
        return None
    return None


def with_inserted_id(item: dict[str, Any], geometry_id: int) -> dict[str, Any]:
    if "kind" not in item:
        result = {"id": geometry_id}
        result.update(item)
        return result

    result: dict[str, Any] = {}
    for key, value in item.items():
        result[key] = value
        if key == "kind":
            result["id"] = geometry_id
    return result


def assign_geometry_ids(document: dict[str, Any]) -> int:
    added = 0
    objects = document.get("Objects")
    if not isinstance(objects, list):
        return added

    for obj in objects:
        if not isinstance(obj, dict) or obj.get("TypeId") != "Sketcher::SketchObject":
            continue
        properties = obj.get("Properties")
        if not isinstance(properties, dict):
            continue
        geometry = properties.get("Geometry")
        if not isinstance(geometry, list):
            continue

        used = {
            geometry_id
            for item in geometry
            if isinstance(item, dict)
            for geometry_id in [read_geometry_id(item)]
            if geometry_id is not None
        }
        next_id = max(used, default=0) + 1
        for index, item in enumerate(geometry):
            if not isinstance(item, dict) or any(field in item for field in ID_FIELDS):
                continue
            while next_id in used:
                next_id += 1
            geometry[index] = with_inserted_id(item, next_id)
            used.add(next_id)
            next_id += 1
            added += 1

    return added


def iter_fixture_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_file():
            files.append(path)
            continue
        for candidate in sorted(path.rglob("*.json")):
            if "expected" in candidate.parts:
                continue
            files.append(candidate)
    return files


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assign stable id fields to Sketcher::SketchObject.Properties.Geometry[] fixtures."
    )
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    changed_files = 0
    total_added = 0
    for path in iter_fixture_files(args.paths):
        data = json.loads(path.read_text(encoding="utf-8"))
        added = assign_geometry_ids(data)
        if added == 0:
            continue
        changed_files += 1
        total_added += added
        print(f"{path}: added {added} Geometry id(s)")
        if not args.dry_run:
            path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"changed_files={changed_files} added_geometry_ids={total_added}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
