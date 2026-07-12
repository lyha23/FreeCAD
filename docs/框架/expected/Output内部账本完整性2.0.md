> 状态：早期设计背景。当前一致性权威只有 public/ledger；producer trace 即使被 collector 一并写出，也仅在 public/ledger 无法对齐时按需参考，不是通过条件。`--emit-ledger` 已是兼容 no-op；现行契约以 [工具规定](../../工具规定/7-9-16-55-FreeCADCmdExpectedLedger工具规定.md) 为准。

可以实现成一个**专门的 fixture oracle validator**。它的入口还是：

```text
fixtures/<phase>/expected/*.freecad.json
```

但它不要只看这个 JSON 的表面内容，而是要求每个 expected 旁边有一份 **FreeCADCmd 生成的账本证明文件**，然后脚本专门验这两者是否匹配、是否完整、是否能解释最终 expected 为什么长这样。

我建议目标定义成一句话：

> `*.freecad.json` 是对外协议 expected；
> `*.freecad.ledger.json` 是 FreeCADCmd 给这个 expected 开出的“验尸报告/权威账本”；
> validator 的职责是证明：expected 可以从 ledger 投影出来，而且所有输入引用都有明确结论。

---

## 1. 文件结构建议

不要把完整账本塞进前端协议 expected 里，否则 expected 会越来越臃肿。更好的方式是 sidecar：

```text
fixtures/
  recomputed/
    input/
      body_pad_pocket.freecad.json
    expected/
      body_pad_pocket.freecad.json
      body_pad_pocket.freecad.ledger.json
```

其中：

```text
body_pad_pocket.freecad.json
```

仍然是你现在用于对齐 `/cad/recompute` 的 expected。

```text
body_pad_pocket.freecad.ledger.json
```

是 FreeCADCmd 生成 expected 时额外导出的权威账本。

validator 从这个入口开始扫：

```text
fixtures/<phase>/expected/*.freecad.json
```

然后自动找同名 sidecar：

```text
foo.freecad.json
foo.freecad.ledger.json
```

没有 sidecar 就 hard fail。

---

## 2. ledger 里至少要有什么

第一版不需要追求特别复杂，但必须能回答四个问题：

```text
1. 这份 expected 是哪个输入生成的？
2. 输入里有哪些引用需要恢复？
3. recompute 之后，这些引用分别发生了什么？
4. 为什么最终 topoNamingState.objects 只发布这些对象，其他对象为什么没发布？
```

建议 ledger v1 长这样：

```json
{
  "schema": "freecad-toponaming-ledger/v1",
  "producer": {
    "name": "FreeCADCmd",
    "freecadVersion": "x.y.z",
    "occtVersion": "x.y.z",
    "scriptVersion": "..."
  },
  "fixture": {
    "phase": "recomputed",
    "case": "body_pad_pocket",
    "inputHash": "sha256:...",
    "expectedPayloadHash": "sha256:...",
    "topoNamingStateHash": "sha256:..."
  },
  "inputReferences": [
    {
      "id": "ref:1",
      "owner": "Body",
      "path": ["Body", "Pad"],
      "element": "Face3",
      "source": "StableSubList",
      "required": true
    }
  ],
  "objects": {
    "Body": {
      "type": "PartDesign::Body",
      "role": "owner",
      "published": true,
      "tip": "Pocket",
      "children": ["Sketch", "Pad", "Pocket"]
    },
    "Pad": {
      "type": "PartDesign::Pad",
      "role": "internal_shape",
      "published": false,
      "beforeElements": {
        "Face": ["Face1", "Face2", "Face3"]
      },
      "afterElements": {
        "Face": ["Face1", "Face5", "Face6"]
      },
      "elementMap": {
        "Face3": ["Face5", "Face6"]
      }
    },
    "Pocket": {
      "type": "PartDesign::Pocket",
      "role": "internal_shape",
      "published": false,
      "beforeElements": {
        "Face": ["Face1", "Face2"]
      },
      "afterElements": {
        "Face": ["Face1", "Face2", "Face8"]
      }
    }
  },
  "events": [
    {
      "id": "event:1",
      "kind": "split",
      "sources": [
        {
          "object": "Pad",
          "element": "Face3"
        }
      ],
      "targets": [
        {
          "object": "Pad",
          "element": "Face5"
        },
        {
          "object": "Pad",
          "element": "Face6"
        }
      ],
      "inputReferenceIds": ["ref:1"],
      "provenance": "FreeCAD.TopologicalNaming.ElementMap"
    }
  ],
  "projection": {
    "publishedObjects": {
      "Body": {
        "ledgerObject": "Body",
        "covers": ["Body", "Pad", "Pocket"],
        "sourceEventIds": ["event:1"],
        "reason": "Body Tip state covers child feature naming"
      }
    },
    "droppedObjects": {
      "Pad": {
        "reason": "covered_by_published_owner",
        "coveredBy": "Body",
        "sourceEventIds": ["event:1"]
      },
      "Pocket": {
        "reason": "covered_by_published_owner",
        "coveredBy": "Body",
        "sourceEventIds": []
      }
    }
  },
  "coverage": {
    "coveredInputReferenceIds": ["ref:1"],
    "uncoveredInputReferenceIds": []
  },
  "roundTrip": {
    "status": "passed",
    "inputTopoNamingStateHash": "sha256:...",
    "results": [
      {
        "inputReferenceId": "ref:1",
        "status": "resolved",
        "resolvedTo": [
          {
            "object": "Pad",
            "element": "Face5"
          },
          {
            "object": "Pad",
            "element": "Face6"
          }
        ]
      }
    ]
  },
  "diagnostics": []
}
```

重点不是字段名一定要照抄，而是这个 ledger 必须有几类信息：

```text
inputReferences
objects
events
projection
coverage
roundTrip
hashes
```

没有这些，就没法证明完整性。

---

## 3. validator 应该验什么

validator 不应该关心 cad-core，也不应该请求 `/cad/recompute`。它只做 FreeCAD expected 的验收。

第一版建议 hard fail 这些规则。

### A. expected 和 ledger 必须绑定

validator 计算：

```text
expectedPayloadHash
topoNamingStateHash
ledgerHash
```

然后检查 ledger 里声明的 hash 是否匹配。

这样可以防止这种情况：

```text
expected 改了
ledger 没改
测试还绿
```

或者：

```text
ledger 是旧输入生成的
expected 是新输入生成的
```

这类情况必须 hard fail。

---

### B. 每个输入引用必须有结论

凡是 `inputReferences` 里的引用，都必须被某个 event 覆盖。

允许的终态可以是：

```text
resolved
modified
generated
split
merged
deleted
ambiguous
owner_changed
failed_with_diagnostics
```

不能出现：

```text
输入引用了 Pad.Face3
ledger 没有任何 event 提到 ref:1
```

这就是账本不完整。

---

### C. 每个 event 的 source / target 必须能落到对象账本里

比如 event 说：

```json
{
  "kind": "split",
  "sources": [{ "object": "Pad", "element": "Face3" }],
  "targets": [
    { "object": "Pad", "element": "Face5" },
    { "object": "Pad", "element": "Face6" }
  ]
}
```

那么 validator 要检查：

```text
Pad 在 objects 里存在
Face3 在 Pad.beforeElements 里存在
Face5 / Face6 在 Pad.afterElements 里存在
split 事件必须至少有 2 个 target
deleted 事件不能有 target
ambiguous 事件必须有 diagnostics 或 candidates
generated 事件可以没有 source，但必须有 target
```

否则 ledger 是假的或不完整。

---

### D. published topoNamingState 必须能从 ledger 投影出来

假设 expected 里只有：

```json
{
  "topoNamingState": {
    "objects": {
      "Body": {}
    }
  }
}
```

validator 要求 ledger 里必须有：

```json
{
  "projection": {
    "publishedObjects": {
      "Body": {
        "ledgerObject": "Body",
        "covers": ["Body", "Pad", "Pocket"]
      }
    }
  }
}
```

也就是说，最终发布出去的每一个对象都必须能回答：

```text
我来自哪个 ledger object？
我覆盖了哪些内部对象？
我为什么足够代表这些内部对象？
```

否则 expected 只是一个结果，不是权威结果。

---

### E. 被裁掉的对象必须有解释

这是你最关心的点。

比如 expected 里不再发布：

```text
Pad
Pocket
Sketch.InternalShape
ChildBoxA
ChildBoxB
```

这可以是对的，但 ledger 里必须说明它们为什么没发布。

允许的理由可以是：

```text
covered_by_published_owner
covered_by_body_tip
covered_by_link_target
covered_by_compound_child_map
not_referenced
deleted
diagnostic_only
```

不允许这种：

```text
Pad 出现在 inputReferences / events 里
但是既没有 published，也没有 droppedObjects 解释
```

这种情况应该 hard fail，因为这就是“为了让 expected 变少，可能把真账本删没了”。

---

### F. round-trip 必须通过

ledger 里必须记录 FreeCADCmd 做过一次 round-trip：

```text
第一轮：
  input + references -> expected topoNamingState

第二轮：
  input + 第一轮 topoNamingState 原样回传 -> references restored
```

validator 至少要检查：

```text
roundTrip.status == "passed"
roundTrip.inputTopoNamingStateHash == expected.topoNamingState 的 hash
每个 required inputReference 都有 roundTrip result
```

如果没有 round-trip 证据，这份 expected 只能叫“看起来像结果”，还不能叫“可长期保存并恢复的结果”。

---

## 4. 脚本放哪里

建议加一个纯 Python validator：

```text
tools/fixtures/validate_freecad_expected_ledger.py
```

再加一个 pytest wrapper：

```text
tests/fixtures/test_freecad_expected_ledger.py
```

CI 里跑：

```bash
python tools/fixtures/validate_freecad_expected_ledger.py "fixtures/*/expected/*.freecad.json" --strict
```

或者：

```bash
pytest -q tests/fixtures/test_freecad_expected_ledger.py
```

---

## 5. validator 脚本雏形

下面这个是第一版可以落地的骨架。它先不依赖 FreeCAD，也不依赖 cad-core，只验 expected 和 ledger 的自洽性。

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import glob
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


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

TERMINAL_EVENT_KINDS = {
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


def canonical_json(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def sha256_json(value: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_json(value)).hexdigest()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def ledger_path_for_expected(expected_path: Path) -> Path:
    name = expected_path.name
    if not name.endswith(".freecad.json"):
        raise ValueError(f"Not a .freecad.json expected file: {expected_path}")

    stem = name[: -len(".freecad.json")]
    return expected_path.with_name(f"{stem}.freecad.ledger.json")


def require(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def element_inventory(record: dict[str, Any], key: str) -> set[str]:
    """
    Accepts:
      beforeElements: { "Face": ["Face1"], "Edge": ["Edge1"] }
    Returns:
      { "Face1", "Edge1" }
    """
    raw = record.get(key) or {}
    result: set[str] = set()

    if isinstance(raw, dict):
        for values in raw.values():
            if isinstance(values, list):
                result.update(str(v) for v in values)
    elif isinstance(raw, list):
        result.update(str(v) for v in raw)

    return result


def ref_key(ref: dict[str, Any]) -> str:
    obj = ref.get("object")
    elem = ref.get("element")
    return f"{obj}.{elem}"


def validate_event_shape(
    errors: list[str],
    event: dict[str, Any],
    objects: dict[str, dict[str, Any]],
) -> None:
    event_id = event.get("id", "<missing-event-id>")
    kind = event.get("kind")

    require(
        errors,
        kind in VALID_EVENT_KINDS,
        f"{event_id}: invalid event kind: {kind}",
    )

    sources = event.get("sources") or []
    targets = event.get("targets") or []

    if kind in {"resolved", "modified", "owner_changed"}:
        require(
            errors,
            len(sources) >= 1,
            f"{event_id}: {kind} event must have at least one source",
        )
        require(
            errors,
            len(targets) >= 1,
            f"{event_id}: {kind} event must have at least one target",
        )

    if kind == "split":
        require(
            errors,
            len(sources) >= 1,
            f"{event_id}: split event must have at least one source",
        )
        require(
            errors,
            len(targets) >= 2,
            f"{event_id}: split event must have at least two targets",
        )

    if kind == "merged":
        require(
            errors,
            len(sources) >= 2,
            f"{event_id}: merged event must have at least two sources",
        )
        require(
            errors,
            len(targets) >= 1,
            f"{event_id}: merged event must have at least one target",
        )

    if kind == "deleted":
        require(
            errors,
            len(sources) >= 1,
            f"{event_id}: deleted event must have at least one source",
        )
        require(
            errors,
            len(targets) == 0,
            f"{event_id}: deleted event must not have targets",
        )

    if kind == "generated":
        require(
            errors,
            len(targets) >= 1,
            f"{event_id}: generated event must have at least one target",
        )

    if kind in {"ambiguous", "failed_with_diagnostics"}:
        has_diag = bool(event.get("diagnosticIds") or event.get("diagnostics"))
        require(
            errors,
            has_diag,
            f"{event_id}: {kind} event must have diagnostics",
        )

    for source in sources:
        obj_id = source.get("object")
        elem = source.get("element")

        require(
            errors,
            obj_id in objects,
            f"{event_id}: source object not found in ledger.objects: {obj_id}",
        )

        if obj_id in objects and elem:
            before = element_inventory(objects[obj_id], "beforeElements")
            if before:
                require(
                    errors,
                    elem in before,
                    f"{event_id}: source element {obj_id}.{elem} not found in beforeElements",
                )

    for target in targets:
        obj_id = target.get("object")
        elem = target.get("element")

        require(
            errors,
            obj_id in objects,
            f"{event_id}: target object not found in ledger.objects: {obj_id}",
        )

        if obj_id in objects and elem:
            after = element_inventory(objects[obj_id], "afterElements")
            if after:
                require(
                    errors,
                    elem in after,
                    f"{event_id}: target element {obj_id}.{elem} not found in afterElements",
                )


def validate_expected_file(expected_path: Path, strict: bool = True) -> list[str]:
    errors: list[str] = []

    expected = load_json(expected_path)
    ledger_path = ledger_path_for_expected(expected_path)

    require(
        errors,
        ledger_path.exists(),
        f"missing ledger sidecar: {ledger_path}",
    )

    if errors:
        return errors

    ledger = load_json(ledger_path)

    require(
        errors,
        ledger.get("schema") == "freecad-toponaming-ledger/v1",
        f"invalid or missing ledger.schema in {ledger_path}",
    )

    producer = ledger.get("producer") or {}
    require(
        errors,
        producer.get("name") == "FreeCADCmd",
        f"ledger.producer.name must be FreeCADCmd in {ledger_path}",
    )

    topo_state = expected.get("topoNamingState")
    require(
        errors,
        isinstance(topo_state, dict),
        f"expected file missing topoNamingState: {expected_path}",
    )

    fixture = ledger.get("fixture") or {}

    expected_hash = fixture.get("expectedPayloadHash")
    if expected_hash:
        require(
            errors,
            sha256_json(expected) == expected_hash,
            f"expectedPayloadHash mismatch: {expected_path}",
        )
    elif strict:
        errors.append(f"missing fixture.expectedPayloadHash: {ledger_path}")

    topo_hash = fixture.get("topoNamingStateHash")
    if topo_hash and isinstance(topo_state, dict):
        require(
            errors,
            sha256_json(topo_state) == topo_hash,
            f"topoNamingStateHash mismatch: {expected_path}",
        )
    elif strict:
        errors.append(f"missing fixture.topoNamingStateHash: {ledger_path}")

    input_refs = ledger.get("inputReferences") or []
    require(
        errors,
        isinstance(input_refs, list),
        f"ledger.inputReferences must be a list: {ledger_path}",
    )

    ref_ids = {
        ref.get("id")
        for ref in input_refs
        if isinstance(ref, dict) and ref.get("required", True)
    }

    require(
        errors,
        all(ref_ids),
        f"every required inputReference must have an id: {ledger_path}",
    )

    objects = ledger.get("objects") or {}
    require(
        errors,
        isinstance(objects, dict),
        f"ledger.objects must be an object map: {ledger_path}",
    )

    events = ledger.get("events") or []
    require(
        errors,
        isinstance(events, list),
        f"ledger.events must be a list: {ledger_path}",
    )

    event_ids: set[str] = set()
    covered_ref_ids: set[str] = set()

    for event in events:
        if not isinstance(event, dict):
            errors.append(f"event must be an object: {ledger_path}")
            continue

        event_id = event.get("id")
        require(
            errors,
            bool(event_id),
            f"event missing id: {ledger_path}",
        )

        if event_id:
            require(
                errors,
                event_id not in event_ids,
                f"duplicate event id: {event_id}",
            )
            event_ids.add(event_id)

        validate_event_shape(errors, event, objects)

        kind = event.get("kind")
        if kind in TERMINAL_EVENT_KINDS:
            for ref_id in event.get("inputReferenceIds") or []:
                covered_ref_ids.add(ref_id)

    missing_refs = sorted(ref_ids - covered_ref_ids)
    require(
        errors,
        not missing_refs,
        f"inputReferences not covered by terminal events: {missing_refs}",
    )

    coverage = ledger.get("coverage") or {}
    uncovered = coverage.get("uncoveredInputReferenceIds") or []
    require(
        errors,
        not uncovered,
        f"coverage.uncoveredInputReferenceIds must be empty: {uncovered}",
    )

    if coverage.get("coveredInputReferenceIds"):
        declared = set(coverage["coveredInputReferenceIds"])
        require(
            errors,
            ref_ids <= declared,
            f"coverage.coveredInputReferenceIds does not cover all required refs",
        )

    projection = ledger.get("projection") or {}
    published_projection = projection.get("publishedObjects") or {}
    dropped_projection = projection.get("droppedObjects") or {}

    topo_objects = {}
    if isinstance(topo_state, dict):
        topo_objects = topo_state.get("objects") or {}

    require(
        errors,
        isinstance(topo_objects, dict),
        f"topoNamingState.objects must be an object map: {expected_path}",
    )

    for published_name in topo_objects.keys():
        require(
            errors,
            published_name in published_projection,
            f"published topoNamingState object has no projection entry: {published_name}",
        )

        entry = published_projection.get(published_name) or {}
        ledger_object = entry.get("ledgerObject")

        require(
            errors,
            ledger_object in objects,
            f"projection for {published_name} points to missing ledger object: {ledger_object}",
        )

        covers = entry.get("covers") or []
        require(
            errors,
            bool(covers),
            f"projection for {published_name} must declare covered objects",
        )

        for covered_object in covers:
            require(
                errors,
                covered_object in objects,
                f"projection for {published_name} covers missing object: {covered_object}",
            )

    # Relevant internal objects: anything touched by refs or events.
    relevant_objects: set[str] = set()

    for ref in input_refs:
        if isinstance(ref, dict):
            owner = ref.get("owner")
            if owner:
                relevant_objects.add(owner)

            for part in ref.get("path") or []:
                relevant_objects.add(part)

    for event in events:
        if not isinstance(event, dict):
            continue

        for source in event.get("sources") or []:
            if source.get("object"):
                relevant_objects.add(source["object"])

        for target in event.get("targets") or []:
            if target.get("object"):
                relevant_objects.add(target["object"])

    published_ledger_objects = {
        entry.get("ledgerObject")
        for entry in published_projection.values()
        if isinstance(entry, dict)
    }

    for object_id in sorted(relevant_objects):
        if object_id in published_ledger_objects:
            continue

        if object_id not in objects:
            errors.append(f"relevant object not found in ledger.objects: {object_id}")
            continue

        if objects[object_id].get("published") is True:
            continue

        drop = dropped_projection.get(object_id)
        require(
            errors,
            isinstance(drop, dict),
            f"relevant object is not published and has no droppedObjects explanation: {object_id}",
        )

        if isinstance(drop, dict):
            has_reason = bool(drop.get("reason"))
            has_cover = bool(drop.get("coveredBy") or drop.get("sourceEventIds"))
            require(
                errors,
                has_reason and has_cover,
                f"dropped object must have reason and cover/source evidence: {object_id}",
            )

    round_trip = ledger.get("roundTrip") or {}
    require(
        errors,
        round_trip.get("status") == "passed",
        f"roundTrip.status must be passed: {ledger_path}",
    )

    rt_hash = round_trip.get("inputTopoNamingStateHash")
    if rt_hash and isinstance(topo_state, dict):
        require(
            errors,
            rt_hash == sha256_json(topo_state),
            f"roundTrip.inputTopoNamingStateHash mismatch: {expected_path}",
        )
    elif strict:
        errors.append(f"missing roundTrip.inputTopoNamingStateHash: {ledger_path}")

    round_trip_results = round_trip.get("results") or []
    rt_ref_ids = {
        item.get("inputReferenceId")
        for item in round_trip_results
        if isinstance(item, dict)
    }

    require(
        errors,
        ref_ids <= rt_ref_ids,
        f"roundTrip.results does not cover all required inputReferences",
    )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "patterns",
        nargs="+",
        help='Glob patterns, e.g. "fixtures/*/expected/*.freecad.json"',
    )
    parser.add_argument("--strict", action="store_true", default=False)
    args = parser.parse_args()

    paths: list[Path] = []

    for pattern in args.patterns:
        for raw in glob.glob(pattern, recursive=True):
            path = Path(raw)
            if path.name.endswith(".freecad.ledger.json"):
                continue
            if path.name.endswith(".freecad.json"):
                paths.append(path)

    paths = sorted(set(paths))

    if not paths:
        print("No expected *.freecad.json files found.", file=sys.stderr)
        return 2

    total_errors = 0

    for path in paths:
        errors = validate_expected_file(path, strict=args.strict)

        if errors:
            total_errors += len(errors)
            print(f"\nFAIL {path}")
            for error in errors:
                print(f"  - {error}")
        else:
            print(f"OK   {path}")

    if total_errors:
        print(f"\n{total_errors} validation error(s).", file=sys.stderr)
        return 1

    print(f"\nValidated {len(paths)} expected fixture(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

运行：

```bash
python tools/fixtures/validate_freecad_expected_ledger.py "fixtures/*/expected/*.freecad.json" --strict
```

这个脚本第一版就能抓住你担心的核心问题：

```text
expected 变小了，但 ledger 解释不了为什么变小
输入引用消失了
Pad/Pocket/Sketch.InternalShape 被裁掉但没有 projection 说明
topoNamingState 和 round-trip 不是同一份状态
expected 和 ledger hash 不匹配
```

---

## 6. FreeCADCmd 生成 ledger 的流程

validator 只是验账本。真正的账本应该由 FreeCADCmd 生成。

建议在生成 expected 的 FreeCADCmd 脚本里加这个流程：

```text
1. 读取 fixture input
2. 记录 before snapshot
3. 记录 inputReferences
4. 执行 recompute
5. 记录 after snapshot
6. 从 FreeCAD topo naming / ElementMap / child maps 里导出 events
7. 生成 published topoNamingState
8. 生成 projection：说明 published state 覆盖了哪些内部对象
9. 用 published topoNamingState 做一次 round-trip
10. 写出 expected/*.freecad.json
11. 写出 expected/*.freecad.ledger.json
12. 立刻调用 validator；validator 失败则不允许更新 expected
```

大致是：

```bash
FreeCADCmd -c tools/freecad/generate_recomputed_fixture.py \
  --input fixtures/recomputed/input/body_pad_pocket.freecad.json \
  --expected fixtures/recomputed/expected/body_pad_pocket.freecad.json \
  --emit-ledger

python tools/fixtures/validate_freecad_expected_ledger.py \
  fixtures/recomputed/expected/body_pad_pocket.freecad.json \
  --strict
```

---

## 7. 最关键的设计点：projection

你这个问题的核心其实是 projection。

也就是：

```text
完整账本 -> 前端协议 topoNamingState
```

不是所有 ledger 对象都要发布，但所有“不发布”的决定都必须有解释。

比如最终 expected 只有：

```json
{
  "topoNamingState": {
    "objects": {
      "Body": {}
    }
  }
}
```

这没问题，但 ledger 里必须有：

```json
{
  "projection": {
    "publishedObjects": {
      "Body": {
        "ledgerObject": "Body",
        "covers": ["Body", "Pad", "Pocket", "Sketch.InternalShape"],
        "reason": "Body Tip state is the protocol publication boundary"
      }
    },
    "droppedObjects": {
      "Pad": {
        "reason": "covered_by_body_tip",
        "coveredBy": "Body"
      },
      "Pocket": {
        "reason": "covered_by_body_tip",
        "coveredBy": "Body"
      },
      "Sketch.InternalShape": {
        "reason": "not_protocol_published_but_consumed_by_Pad_generation",
        "coveredBy": "Pad"
      }
    }
  }
}
```

这就把“少”变成了可审计的“少”。

没有 projection，`topoNamingState.objects` 变少只能叫：

```text
输出少了
```

有 projection，才能叫：

```text
经过 FreeCADCmd 证明，可以只发布这些
```

---

## 8. CI 里怎么卡住

建议加一个 pytest：

```python
# tests/fixtures/test_freecad_expected_ledger.py

from pathlib import Path

from tools.fixtures.validate_freecad_expected_ledger import validate_expected_file


def test_freecad_expected_ledgers_are_complete():
    root = Path("fixtures")

    expected_files = sorted(
        p
        for p in root.glob("*/expected/*.freecad.json")
        if not p.name.endswith(".freecad.ledger.json")
    )

    assert expected_files, "no FreeCAD expected fixture files found"

    failures = {}

    for path in expected_files:
        errors = validate_expected_file(path, strict=True)
        if errors:
            failures[str(path)] = errors

    assert not failures, "\n".join(
        [
            f"{path}:\n" + "\n".join(f"  - {e}" for e in errors)
            for path, errors in failures.items()
        ]
    )
```

这样以后任何人改：

```text
fixtures/<phase>/expected/*.freecad.json
```

但没同步更新 ledger，CI 会直接挂。

---

## 9. 第一版 hard fail 清单

我建议第一版先别做太多，先卡这 8 条：

```text
1. 每个 expected 必须有同名 .freecad.ledger.json
2. ledger.schema 必须匹配
3. ledger.producer.name 必须是 FreeCADCmd
4. expectedPayloadHash 必须匹配
5. topoNamingStateHash 必须匹配
6. 每个 inputReference 必须被 terminal event 覆盖
7. 每个 topoNamingState.objects 项必须有 projection.publishedObjects 解释
8. 每个 relevant 但未发布的对象必须有 projection.droppedObjects 解释
9. roundTrip.status 必须是 passed
```

第 9 条很重要，可以作为强规则。

---

## 10. 最终效果

以后你看一个 fixture，不再是只看：

```text
expected 里面 topoNamingState.objects 变少了
```

而是 CI 会证明：

```text
输入引用都覆盖了
内部对象都入账了
split/deleted/ambiguous/generated/modified 都有结论
发布出去的对象都有来源
没发布的对象都有理由
round-trip 真的通过
expected 和 ledger 没有 hash mismatch
```

这样 `fixtures/<phase>/expected/*.freecad.json` 才能变成真正的 FreeCAD 权威 expected。

一句话说：

**不要让 `topoNamingState` 自己证明自己。让 FreeCADCmd 生成 ledger，再让 validator 验 ledger 能否解释 `topoNamingState`。只有 ledger 通过，expected 才算权威。**
