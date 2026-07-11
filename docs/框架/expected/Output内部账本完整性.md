> 状态：早期设计背景。当前 public/ledger/producer-trace 职责以 [输入输出约定](../输入输出约定.md) 和 [工具规定](../../工具规定/7-9-16-55-FreeCADCmdExpectedLedger工具规定.md) 为准；本文旧路径、命令或仅扫描 public JSON 的建议不是现行入口。

可以实现成一个**专门的 expected artifact validator**，名字建议叫：

```text
cad-core/tools/validate_freecad_expected_ledger.py
```

它只验收：

```text
cad-core/fixtures/<phase>/expected/*.freecad.json
```

不依赖 cad-core runtime，不读取 `named_shapes`，只证明 checked-in `.freecad.json` 里的 **FreeCAD public topoNamingState 账本投影** 是完整、闭合、自洽的。

然后再用一个 unittest 包住它，放进：

```text
cad-core/tests/内部账本完整性验收/test_freecad_expected_ledger_integrity.py
```

这样 CI 里既可以直接跑脚本，也可以通过 unittest discover 跑。

---

## 这个验收脚本应该证明什么

它应该证明的是：

```text
fixtures/<phase>/expected/*.freecad.json
  里的 topoNamingState.objects
  是一个闭合账本：
    elementMap.entries 不断链
    childElementMaps 不断链
    mapperHistory 不断链
    elementReferenceUpdates 能回到 topoNamingState 解析
    diagnostics 能解释 split/deleted/ambiguous 这些终止恢复失败
```

这和现在 `test_freecad_expected_oracle_coverage.py` 不一样。那个文件更像“覆盖边界检查”。你要的是“expected artifact 完整性门禁”。

现有代码里已经有不少可复用基础：`fixture_expected.discover_expected_cases()` 已经能枚举 `fixtures/*/expected/*.freecad.json`。 `collect_freecad_expected.py` 生成的 `topo_state_object_payload()` 本身已经把一个 object state 组织成 `subshapes`、`elementMap`、`childElementMaps`、`mapperHistory` 四块。 所以 validator 要做的就是对这四块做闭包校验。

---

# 推荐实现结构

## 1. 新增脚本

路径：

```text
cad-core/tools/validate_freecad_expected_ledger.py
```

命令形式：

```bash
cd cad-core

python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict

python3 tools/validate_freecad_expected_ledger.py --all --strict
```

可选再加一个 FreeCADCmd 复现检查：

```bash
python3 tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict \
  --freecadcmd-check
```

其中：

```text
--strict
  做 phase-level 覆盖要求，比如必须覆盖 elementMap、childElementMaps、mapperHistory、
  generated/modified/split/deleted/merge/ambiguous、
  split_stable_subname/deleted_stable_subname/stable_identity_ambiguous。

--freecadcmd-check
  额外调用 tools/collect_freecad_expected.py --phase <phase> --check --skip-unsupported，
  证明 checked-in expected 还能由当前 FreeCADCmd collector 复现。
```

`collect_freecad_expected.py` 在非 FreeCAD Python 环境下会通过 FreeCADCmd 重新调用自己，所以这个选项可以作为“权威来源可复现”的第二道门。

---

## 2. 验收分两层

### A. 单文件完整性

每个 `.freecad.json` 单独检查：

```text
topoNamingState.schemaVersion 正确
producer.freecadVersion / producer.occtVersion 存在，且不是 cad-core-runtime
objects 是 dict

每个 object：
  subshapes key == subshape.subname
  subshape.subname 是 FaceN / EdgeN / VertexN 这种 indexed topo name
  rawFreecadMappedName -> canonicalFreecadMappedName 可重算

每个 elementMap entry：
  entry key == mappedName.canonical
  mappedName.canonical == canonical_freecad_mapped_name(mappedName.raw)
  target.object 存在于 topoNamingState.objects
  target.subname 存在于 target object 的 subshapes
  source.object/source.subname 非空
  evidence.mapperHistoryIds 是 list
  evidence.childElementMapKey 字段存在

每个 childElementMap：
  ownerObject == 当前 object
  childObject 非空，最好也存在于 objects
  elementMap.entries 非空
  每个 child entry 的 evidence.childElementMapKey == child map key
  每个 child entry 的 target.object == ownerObject
  每个 child entry 的 source.object == childObject

每个 mapperHistory event：
  id 唯一
  relation 合法
  source/target 结构合法
  被 elementMap.evidence.mapperHistoryIds 引用的 id 必须存在
  elementMap 只能引用 generated/modified/merge 这种 resolved event
  split/deleted/ambiguous 这种 terminal event 必须由 diagnostics 或 diagnostic_status 解释

每个 elementReferenceUpdates StableSubList：
  target object 必须在 topoNamingState.objects 里
  每个 stable token 必须能在 target object 的 elementMap、childElementMaps 或 mapperHistory 里解析
```

这一步证明：**expected JSON 不是一堆松散字段，而是一个闭合账本。**

现有 collector 里已经有类似 reference update 闭包检查：`assert_reference_updates_resolve_in_state()` 会检查 `StableSubList` 的 target 是否在 `topoNamingState.objects` 中，并要求 stable token 能在 object state 中解析。 这部分可以直接复用或迁移到 validator。

### B. phase-level 覆盖完整性

对于 `c4m6` 这种专门 topoNamingState phase，再做整体覆盖要求：

```text
至少一个 expected 有 elementMap.entries
至少一个 expected 有 childElementMaps
至少一个 expected 有 mapperHistory

mapperHistory relation 覆盖：
  generated
  modified
  split
  deleted
  merge
  ambiguous

diagnostics 覆盖：
  split_stable_subname
  deleted_stable_subname
  stable_identity_ambiguous
```

这一步证明：**这一 phase 的 expected corpus 覆盖了关键账本形态。**

---

# 脚本骨架

可以先这样实现，短期直接复用 `collect_freecad_expected.py` 里的 canonicalization/contract helper，后续再把这些 helper 抽成公共模块。

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
TOOLS_ROOT = ROOT / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from collect_freecad_expected import (  # noqa: E402
    TOPO_STATE_SCHEMA_VERSION,
    canonical_freecad_mapped_name,
    c4m6_protocol_contract_errors,
    display_path_stable_token,
    indexed_topo_subname,
    topo_naming_state_contract_errors,
    topo_state_object_has_stable_token,
)


REQUIRED_MAPPER_RELATIONS = {
    "generated",
    "modified",
    "split",
    "deleted",
    "merge",
    "ambiguous",
}

REQUIRED_RECOVERY_DIAGNOSTICS = {
    "split_stable_subname",
    "deleted_stable_subname",
    "stable_identity_ambiguous",
}

ENTRY_RESOLVED_MAPPER_RELATIONS = {
    "generated",
    "modified",
    "merge",
}


def case_name(path: Path) -> str:
    return f"{path.parent.parent.name}/{path.name.removesuffix('.freecad.json')}"


def expected_paths(phase: str | None) -> list[Path]:
    if phase:
        return sorted((ROOT / "fixtures" / phase / "expected").glob("*.freecad.json"))
    return sorted((ROOT / "fixtures").glob("*/expected/*.freecad.json"))


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def diagnostic_codes(payload: dict[str, Any]) -> set[str]:
    codes: set[str] = set()
    for diagnostic in payload.get("diagnostics") or []:
        if isinstance(diagnostic, dict) and isinstance(diagnostic.get("code"), str):
            codes.add(diagnostic["code"])
    return codes


def topo_objects(payload: dict[str, Any]) -> dict[str, Any]:
    state = payload.get("topoNamingState")
    if not isinstance(state, dict):
        return {}
    objects = state.get("objects")
    return objects if isinstance(objects, dict) else {}


def element_entries(object_state: dict[str, Any]) -> dict[str, Any]:
    element_map = object_state.get("elementMap")
    if not isinstance(element_map, dict):
        return {}
    entries = element_map.get("entries")
    return entries if isinstance(entries, dict) else {}


def subshapes(object_state: dict[str, Any]) -> dict[str, Any]:
    value = object_state.get("subshapes")
    return value if isinstance(value, dict) else {}


def child_maps(object_state: dict[str, Any]) -> list[Any]:
    value = object_state.get("childElementMaps")
    return value if isinstance(value, list) else []


def mapper_history(object_state: dict[str, Any]) -> list[Any]:
    value = object_state.get("mapperHistory")
    return value if isinstance(value, list) else []


def validate_producer(path: Path, payload: dict[str, Any]) -> list[str]:
    name = case_name(path)
    errors: list[str] = []

    state = payload.get("topoNamingState")
    if not isinstance(state, dict):
        # Diagnostic-only incompatibility expected files are allowed to have no state.
        codes = diagnostic_codes(payload)
        if codes & {"topo_state_schema_incompatible", "topo_state_producer_incompatible"}:
            return []
        return [f"{name}:missing_topoNamingState"]

    if state.get("schemaVersion") != TOPO_STATE_SCHEMA_VERSION:
        errors.append(f"{name}:topoNamingState.schemaVersion")

    producer = state.get("producer")
    if not isinstance(producer, dict):
        errors.append(f"{name}:topoNamingState.producer")
        return errors

    freecad_version = producer.get("freecadVersion")
    occt_version = producer.get("occtVersion")

    if not isinstance(freecad_version, str) or not freecad_version:
        errors.append(f"{name}:producer.freecadVersion")
    elif freecad_version == "cad-core-runtime":
        errors.append(f"{name}:producer.freecadVersion.cad_core_runtime")

    if not isinstance(occt_version, str) or not occt_version:
        errors.append(f"{name}:producer.occtVersion")

    return errors


def validate_subshape_closure(path: Path, object_name: str, object_state: dict[str, Any]) -> list[str]:
    name = case_name(path)
    errors: list[str] = []

    for subshape_key, subshape in subshapes(object_state).items():
        context = f"{name}:topoNamingState.{object_name}.subshapes.{subshape_key}"
        if not isinstance(subshape, dict):
            errors.append(context)
            continue

        if subshape.get("subname") != subshape_key:
            errors.append(f"{context}.subname")

        if not indexed_topo_subname(subshape_key):
            errors.append(f"{context}.indexed")

        raw = subshape.get("rawFreecadMappedName")
        canonical = subshape.get("canonicalFreecadMappedName")
        if isinstance(raw, str) and raw:
            if display_path_stable_token(raw):
                errors.append(f"{context}.rawFreecadMappedName.display_path")
            expected_canonical = canonical_freecad_mapped_name(raw)
            if canonical != expected_canonical:
                errors.append(f"{context}.canonicalFreecadMappedName")

    return errors


def validate_entry_closure(
    path: Path,
    object_name: str,
    object_state: dict[str, Any],
    objects: dict[str, Any],
) -> list[str]:
    name = case_name(path)
    errors: list[str] = []
    events_by_id = {
        str(event["id"]): event
        for event in mapper_history(object_state)
        if isinstance(event, dict) and isinstance(event.get("id"), str)
    }

    for token, entry in element_entries(object_state).items():
        context = f"{name}:topoNamingState.{object_name}.elementMap.{token}"

        if not isinstance(entry, dict):
            errors.append(context)
            continue

        mapped_name = entry.get("mappedName")
        if not isinstance(mapped_name, dict):
            errors.append(f"{context}.mappedName")
            continue

        raw = mapped_name.get("raw")
        canonical = mapped_name.get("canonical")
        if not isinstance(canonical, str) or not canonical:
            errors.append(f"{context}.mappedName.canonical")
        elif token != canonical:
            errors.append(f"{context}.key_canonical")

        if isinstance(raw, str) and raw:
            expected_canonical = canonical_freecad_mapped_name(raw)
            if canonical != expected_canonical:
                errors.append(f"{context}.mappedName.canonical_from_raw")

        target = entry.get("target")
        if not isinstance(target, dict):
            errors.append(f"{context}.target")
            continue

        target_object = target.get("object")
        target_subname = target.get("subname")

        if not isinstance(target_object, str) or target_object not in objects:
            errors.append(f"{context}.target.object")
            continue

        if not isinstance(target_subname, str) or not indexed_topo_subname(target_subname):
            errors.append(f"{context}.target.subname.indexed")
        elif target_subname not in subshapes(objects[target_object]):
            errors.append(f"{context}.target.subname.missing")

        source = entry.get("source")
        if not isinstance(source, dict):
            errors.append(f"{context}.source")
        else:
            if not isinstance(source.get("object"), str) or not source.get("object"):
                errors.append(f"{context}.source.object")
            if not isinstance(source.get("subname"), str) or not source.get("subname"):
                errors.append(f"{context}.source.subname")

        evidence = entry.get("evidence")
        if not isinstance(evidence, dict):
            errors.append(f"{context}.evidence")
            continue

        mapper_ids = evidence.get("mapperHistoryIds")
        if mapper_ids is None:
            errors.append(f"{context}.evidence.mapperHistoryIds.missing")
        elif not isinstance(mapper_ids, list):
            errors.append(f"{context}.evidence.mapperHistoryIds")
        else:
            for mapper_id in mapper_ids:
                event = events_by_id.get(str(mapper_id))
                if event is None:
                    errors.append(f"{context}.evidence.mapperHistoryIds.missing_event")
                    continue
                if event.get("relation") not in ENTRY_RESOLVED_MAPPER_RELATIONS:
                    errors.append(f"{context}.evidence.mapperHistoryIds.terminal_event")

        if "childElementMapKey" not in evidence:
            errors.append(f"{context}.evidence.childElementMapKey.missing")

    return errors


def validate_child_map_closure(
    path: Path,
    object_name: str,
    object_state: dict[str, Any],
    objects: dict[str, Any],
) -> list[str]:
    name = case_name(path)
    errors: list[str] = []

    for index, child_map in enumerate(child_maps(object_state)):
        context = f"{name}:topoNamingState.{object_name}.childElementMaps.{index}"

        if not isinstance(child_map, dict):
            errors.append(context)
            continue

        key = child_map.get("key")
        owner = child_map.get("ownerObject")
        child = child_map.get("childObject")

        if not isinstance(key, str) or not key:
            errors.append(f"{context}.key")
            key = ""

        if owner != object_name:
            errors.append(f"{context}.ownerObject")

        if not isinstance(child, str) or not child:
            errors.append(f"{context}.childObject")
        elif child not in objects:
            errors.append(f"{context}.childObject.missing_from_topoNamingState.objects")

        entries = child_map.get("elementMap", {}).get("entries")
        if not isinstance(entries, dict) or not entries:
            errors.append(f"{context}.elementMap.entries")
            continue

        for token, entry in entries.items():
            entry_context = f"{context}.elementMap.entries.{token}"

            if not isinstance(entry, dict):
                errors.append(entry_context)
                continue

            target = entry.get("target")
            source = entry.get("source")
            evidence = entry.get("evidence")

            if not isinstance(target, dict) or target.get("object") != object_name:
                errors.append(f"{entry_context}.target.object")

            target_subname = target.get("subname") if isinstance(target, dict) else None
            if not isinstance(target_subname, str) or not indexed_topo_subname(target_subname):
                errors.append(f"{entry_context}.target.subname.indexed")
            elif target_subname not in subshapes(object_state):
                errors.append(f"{entry_context}.target.subname.missing")

            if isinstance(child, str) and child:
                if not isinstance(source, dict) or source.get("object") != child:
                    errors.append(f"{entry_context}.source.object")

            child_key = evidence.get("childElementMapKey") if isinstance(evidence, dict) else None
            if child_key != key:
                errors.append(f"{entry_context}.evidence.childElementMapKey")

    return errors


def validate_mapper_history_closure(
    path: Path,
    object_name: str,
    object_state: dict[str, Any],
    payload_codes: set[str],
) -> tuple[list[str], set[str], set[str]]:
    name = case_name(path)
    errors: list[str] = []
    relations: set[str] = set()
    diagnostic_statuses: set[str] = set()
    event_ids: set[str] = set()

    for index, event in enumerate(mapper_history(object_state)):
        context = f"{name}:topoNamingState.{object_name}.mapperHistory.{index}"

        if not isinstance(event, dict):
            errors.append(context)
            continue

        event_id = event.get("id")
        if not isinstance(event_id, str) or not event_id:
            errors.append(f"{context}.id")
        elif event_id in event_ids:
            errors.append(f"{context}.id.duplicate")
        else:
            event_ids.add(event_id)

        relation = event.get("relation")
        if not isinstance(relation, str) or not relation:
            errors.append(f"{context}.relation")
        else:
            relations.add(relation)

        source = event.get("source")
        if not isinstance(source, dict):
            errors.append(f"{context}.source")
        else:
            if not isinstance(source.get("object"), str) or not source.get("object"):
                errors.append(f"{context}.source.object")
            if not isinstance(source.get("subname"), str):
                errors.append(f"{context}.source.subname")

        target = event.get("target")
        if not isinstance(target, dict):
            errors.append(f"{context}.target")
        else:
            target_subname = target.get("subname")
            if isinstance(target_subname, str) and target_subname and not indexed_topo_subname(target_subname):
                errors.append(f"{context}.target.subname.indexed")

        status = event.get("diagnostic_status")
        if isinstance(status, str) and status:
            diagnostic_statuses.add(status)
            if status not in payload_codes:
                errors.append(f"{context}.diagnostic_status.not_in_response_diagnostics")

    return errors, relations, diagnostic_statuses


def iter_reference_update_items(update: dict[str, Any]) -> Iterable[dict[str, Any]]:
    property_type = update.get("PropertyType")

    if property_type in {"App::PropertyLinkSub", "App::PropertyLinkSubGlobal"}:
        yield update
        return

    if property_type in {"App::PropertyLinkSubList", "App::PropertyLinkSubListGlobal"}:
        sub_set = update.get("SubSet")
        if isinstance(sub_set, list):
            for item in sub_set:
                if isinstance(item, dict):
                    yield item


def validate_reference_updates(path: Path, payload: dict[str, Any], objects: dict[str, Any]) -> list[str]:
    name = case_name(path)
    errors: list[str] = []

    updates = payload.get("elementReferenceUpdates")
    if not isinstance(updates, list):
        return errors

    for update_index, update in enumerate(updates):
        if not isinstance(update, dict):
            errors.append(f"{name}:elementReferenceUpdates.{update_index}")
            continue

        for item_index, item in enumerate(iter_reference_update_items(update)):
            context = f"{name}:elementReferenceUpdates.{update_index}.{item_index}"

            stable_sub_list = item.get("StableSubList")
            if not isinstance(stable_sub_list, list) or not stable_sub_list:
                continue

            target_name = item.get("value")
            if not isinstance(target_name, str) or target_name not in objects:
                errors.append(f"{context}.value.target_missing")
                continue

            object_state = objects[target_name]

            reference_shadows = item.get("ReferenceShadow")
            if isinstance(reference_shadows, list):
                for shadow_index, shadow in enumerate(reference_shadows):
                    if not isinstance(shadow, dict):
                        continue
                    shadow_target = shadow.get("target")
                    if isinstance(shadow_target, str) and shadow_target and shadow_target != target_name:
                        errors.append(f"{context}.ReferenceShadow.{shadow_index}.target")

            for token_index, token in enumerate(stable_sub_list):
                if not isinstance(token, str) or not token:
                    errors.append(f"{context}.StableSubList.{token_index}")
                    continue
                if display_path_stable_token(token):
                    errors.append(f"{context}.StableSubList.{token_index}.display_path")
                    continue
                if not topo_state_object_has_stable_token(object_state, token):
                    errors.append(f"{context}.StableSubList.{token_index}.unresolved")

    return errors


def validate_expected_file(path: Path) -> tuple[list[str], dict[str, Any]]:
    payload = load_json(path)
    errors: list[str] = []

    # Reuse existing collector-side contract checks first.
    errors.extend(topo_naming_state_contract_errors(payload, case_name(path)))
    errors.extend(c4m6_protocol_contract_errors(path, payload, case_name(path)))

    # Add artifact-specific strong closure checks.
    errors.extend(validate_producer(path, payload))

    objects = topo_objects(payload)
    payload_codes = diagnostic_codes(payload)

    relations: set[str] = set()
    recovery_diagnostics: set[str] = set(payload_codes)

    for object_name, object_state in objects.items():
        if not isinstance(object_state, dict):
            errors.append(f"{case_name(path)}:topoNamingState.{object_name}")
            continue

        errors.extend(validate_subshape_closure(path, str(object_name), object_state))
        errors.extend(validate_entry_closure(path, str(object_name), object_state, objects))
        errors.extend(validate_child_map_closure(path, str(object_name), object_state, objects))

        mapper_errors, object_relations, object_statuses = validate_mapper_history_closure(
            path,
            str(object_name),
            object_state,
            payload_codes,
        )
        errors.extend(mapper_errors)
        relations |= object_relations
        recovery_diagnostics |= object_statuses

    errors.extend(validate_reference_updates(path, payload, objects))

    stats = {
        "has_topo_state": bool(objects),
        "element_map_entries": sum(
            len(element_entries(obj))
            for obj in objects.values()
            if isinstance(obj, dict)
        ),
        "child_element_maps": sum(
            len(child_maps(obj))
            for obj in objects.values()
            if isinstance(obj, dict)
        ),
        "mapper_history_events": sum(
            len(mapper_history(obj))
            for obj in objects.values()
            if isinstance(obj, dict)
        ),
        "relations": relations,
        "recovery_diagnostics": recovery_diagnostics,
    }

    return errors, stats


def run_freecadcmd_check(phase: str, freecadcmd: str | None) -> int:
    command = [
        sys.executable,
        str(ROOT / "tools" / "collect_freecad_expected.py"),
        "--phase",
        phase,
        "--check",
        "--skip-unsupported",
    ]
    if freecadcmd:
        command.extend(["--freecadcmd", freecadcmd])
    return subprocess.run(command, cwd=ROOT).returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate ledger completeness of fixtures/<phase>/expected/*.freecad.json."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--phase")
    group.add_argument("--all", action="store_true")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--freecadcmd-check", action="store_true")
    parser.add_argument("--freecadcmd")
    args = parser.parse_args(argv)

    paths = expected_paths(None if args.all else args.phase)
    if not paths:
        print("no expected .freecad.json files found", file=sys.stderr)
        return 1

    all_errors: list[str] = []
    corpus_stats = {
        "topo_state_files": 0,
        "element_map_entries": 0,
        "child_element_maps": 0,
        "mapper_history_events": 0,
        "relations": set(),
        "recovery_diagnostics": set(),
    }

    for path in paths:
        errors, stats = validate_expected_file(path)
        all_errors.extend(errors)

        if stats["has_topo_state"]:
            corpus_stats["topo_state_files"] += 1
        corpus_stats["element_map_entries"] += stats["element_map_entries"]
        corpus_stats["child_element_maps"] += stats["child_element_maps"]
        corpus_stats["mapper_history_events"] += stats["mapper_history_events"]
        corpus_stats["relations"] |= stats["relations"]
        corpus_stats["recovery_diagnostics"] |= stats["recovery_diagnostics"]

    if args.strict:
        if corpus_stats["topo_state_files"] == 0:
            all_errors.append("corpus:topoNamingState.missing")
        if corpus_stats["element_map_entries"] <= 0:
            all_errors.append("corpus:elementMap.entries.empty")
        if corpus_stats["child_element_maps"] <= 0:
            all_errors.append("corpus:childElementMaps.empty")
        if corpus_stats["mapper_history_events"] <= 0:
            all_errors.append("corpus:mapperHistory.empty")

        missing_relations = REQUIRED_MAPPER_RELATIONS - corpus_stats["relations"]
        if missing_relations:
            all_errors.append(f"corpus:mapperHistory.relations.missing={sorted(missing_relations)}")

        missing_diagnostics = REQUIRED_RECOVERY_DIAGNOSTICS - corpus_stats["recovery_diagnostics"]
        if missing_diagnostics:
            all_errors.append(f"corpus:recoveryDiagnostics.missing={sorted(missing_diagnostics)}")

    if args.freecadcmd_check:
        phases = sorted({path.parent.parent.name for path in paths})
        for phase in phases:
            rc = run_freecadcmd_check(phase, args.freecadcmd)
            if rc != 0:
                all_errors.append(f"{phase}:FreeCADCmd.check.failed")

    if all_errors:
        for error in all_errors:
            print(error, file=sys.stderr)
        print(
            f"validated={len(paths)} failed={len(all_errors)}",
            file=sys.stderr,
        )
        return 1

    print(
        "validated="
        f"{len(paths)} "
        f"topo_state_files={corpus_stats['topo_state_files']} "
        f"elementMap.entries={corpus_stats['element_map_entries']} "
        f"childElementMaps={corpus_stats['child_element_maps']} "
        f"mapperHistory={corpus_stats['mapper_history_events']} "
        f"relations={sorted(corpus_stats['relations'])} "
        f"diagnostics={sorted(corpus_stats['recovery_diagnostics'])}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

---

# 再加一个 unittest 包装

路径：

```text
cad-core/tests/内部账本完整性验收/test_freecad_expected_ledger_integrity.py
```

内容可以非常薄：

```python
from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path


CAD_CORE_ROOT = Path(__file__).resolve().parents[2]


class FreecadExpectedLedgerIntegrityTest(unittest.TestCase):
    def test_c4m6_expected_freecad_json_ledger_is_complete(self) -> None:
        command = [
            sys.executable,
            str(CAD_CORE_ROOT / "tools" / "validate_freecad_expected_ledger.py"),
            "--phase",
            "c4m6",
            "--strict",
        ]
        subprocess.run(command, cwd=CAD_CORE_ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
```

CI 可以跑：

```bash
cd cad-core
python3 -m unittest discover -s tests/内部账本完整性验收 -p 'test_*.py'
```

或者直接跑：

```bash
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
```

---

# 如果你还想把“权威性”也放进这个环节

那就把 CI step 写成两个命令：

```bash
# 1. 证明 checked-in expected JSON 自身账本闭合
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict

# 2. 证明 checked-in expected JSON 能由 FreeCADCmd collector 复现
python3 tools/collect_freecad_expected.py --phase c4m6 --check --skip-unsupported
```

或者合并成一个命令：

```bash
python3 tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict \
  --freecadcmd-check
```

这样证明链就很清楚：

```text
FreeCADCmd collector 可复现 expected
+
expected JSON 自身 topoNamingState 账本闭合
=
fixtures/<phase>/expected/*.freecad.json 可以作为权威 oracle artifact
```

---

# 我建议命名上明确区分

现在你可以保留三个层次：

```text
test_freecad_expected_oracle_coverage.py
  验证 expected corpus 覆盖了哪些 public oracle 形态。

validate_freecad_expected_ledger.py
  验证 fixtures/<phase>/expected/*.freecad.json 的 topoNamingState 账本闭合完整。

collect_freecad_expected.py --check
  验证 expected corpus 可由 FreeCADCmd/native collector 复现。
```

这样就不会再混淆：

```text
coverage test      = 有没有覆盖到
ledger validator   = 账本是否闭合完整
FreeCADCmd --check = 是否能由权威生产者复现
```
