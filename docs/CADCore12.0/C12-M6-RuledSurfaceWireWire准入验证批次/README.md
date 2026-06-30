# C12-M6 RuledSurface Wire/Wire 准入验证批次

C12-M6 用于验证 `Part::RuledSurface` wire/wire 分支是否已经满足正式准入条件。

触发原因是旧 C3M4 PartSurface 主线把 `PARTSURF-BLOCK-005` 保留为 `deferred-S2`，理由是当时缺少 source-backed collector、cad-core input schema 和 shell/topo provenance 三项闭环。但当前 live code / capability 已出现 `supported_wire_wire_expected_backed`、`c4m1/part-ruled-surface-wire-wire` fixture、checked-in expected 和 focused tests。C12-M6 的任务是核实这组 current evidence 是否足够正式关闭旧 deferred blocker，或者是否需要文档发布修复 / 代码修复。

本包不是默认 C++ implementation 包。只有 S5 证明 checked-in expected 与 current output mismatch，且 mismatch 不能通过 docs/capability wording 修复时，才允许另开实现包。

## S0 live 冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=71dba06fb6`。
- `git log -1 --oneline=71dba06fb6 docs: 完成 C12-M5 S5 发布闸门`。
- 起点 dirty boundary 只包含 `docs/CADCore12.0/README.md` 修改和未跟踪的 C12-M6 包；未发现 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source 改动。
- C12-M1 / C12-M2 / C12-M3 / C12-M4 / C12-M5 `工作步骤细分` 队列均只输出表头。
- C12-M6 S0 开始前队列为索引、S0、S1、S2、S3、S4、S5；索引只承担导航职责，随 S0 一并标记为 `【已实现】` 后，下一步为 S1 source/current evidence 复核。
- `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`，`remaining_gaps=[]`，covered 包含 `wire_wire_brepfill_shell` / `wire_edge_provenance` / `expected_backed_fixtures`，request-local boundaries 包含 `wire_wire_brepfill_shell`。
- 旧 `PARTSURF-BLOCK-005`、`PARTSURF-SCOPE-007`、`PARTSURF-FIX-005` 的 deferred 条件冻结为：source-backed collector、cad-core input schema、shell/topo provenance。

## 入口

- 总入口：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次总入口.md`
- 方案：`6-30-13-27-C12-M6-RuledSurfaceWireWire准入验证批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前裁决目标

| 结果 | 含义 |
| --- | --- |
| `wire_wire_admitted_current_supported` | collector、schema、provenance 和 focused tests 均成立；关闭旧 deferred blocker，无新增 C++。 |
| `publication_repair_required` | current code/test 成立，但旧 C3M4/CADCore 文档或 capability wording 漂移；只做文档/metadata 修复。 |
| `implementation_candidate_required` | checked-in expected 与 current output 出现真实 mismatch，且来源可定位到 Part executor / TopoShapeExpansion / topo provenance；另开 implementation 包。 |
| `retained_validation_blocker` | 三项准入证据仍缺一项；保留 blocker，不实现、不发布 supported。 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
```
