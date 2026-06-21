# 【已实现】C5-M11-S0 live gap 与 scope 冻结

状态：`done_C5M11-S0_live_gap_guard`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
0250a4e701

git log -1 --oneline
0250a4e701 fix: 修复 Body 显示边引用映射到 Tip 特征

git -c core.quotepath=false status --short -uall
 M docs/CADCore5.0-PartDesign-高价值剩余语义/README.md
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_blocker_queue.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_fixture_oracle_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_non_goal_registry.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_scope_review_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_source_candidates.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_validation_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/
```

S0 起点已有 root C5 矩阵/README 修改和未跟踪 C5-M11 包文件；这些文件属于本轮上下文。本步骤只冻结 C5-M10 wrapper expected live gap 与 root C5 对应 S0 结论，不清理、不回退其它改动。

## S0 核查结论

- C5-M10 已把 `part_workbench.sweep` advanced PipeShell 字段拆成字段级 source / diagnostic-backed 合同；`cad-core/src/adapters/c_api/c_api.cpp` 和 `cad-core/tests/test_adapters.py` 当前仍只把基础 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize` 列为 expected-backed。
- 当前唯一剩余 wrapper gap 是 `part_sweep_wrapper_expected_collector`，字段覆盖 `AuxiliarySpine`、`AuxiliaryCurvilinear`、`SpineSupport`、`SupportMode`、`Binormal` / `BiNormal`、`SectionOptions[].Location` / `WithContact` / `WithCorrection`、`Tolerance.tol3d` / `boundTol` / `tolAngular` 和 `advanced_combination`。
- 六个待替换 expected 文件固定为 `part-sweep-auxiliary-spine-contract.freecad.json`、`part-sweep-binormal-contract.freecad.json`、`part-sweep-support-mode-diagnostics.freecad.json`、`part-sweep-located-profile-contract.freecad.json`、`part-sweep-tolerance-contract.freecad.json`、`part-sweep-advanced-combined-contract.freecad.json`。
- 六个 expected 当前都是 `known_gap` payload，删除条件一致：只有 FreeCADCmd `Part.BRepOffsetAPI_MakePipeShell` request-local wrapper collector 能稳定记录 `shape_summary` 与 `object_fields.advanced` metadata 后，才能替换为 expected-backed JSON；不得从 cad-core 输出、bbox、拓扑数量或 fixture 名称反推 expected。
- native `Part::Sweep` overclaim guard 不变：上游 direct properties 仍只声明 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`；advanced 字段只能表述为 request-local wrapper contract，不能写成 upstream native `Part::Sweep` 直接属性。

## 目标

冻结 C5-M11 起点：确认 C5-M10 advanced PipeShell 字段已经 source / diagnostic-backed，剩余 gap 只剩 `part_sweep_wrapper_expected_collector`，并记录必须批量替换的六个 expected 文件。S0 不改 cad-core 行为。

## 必读

- 本包总入口与方案。
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c5m10/expected/*.freecad.json`

## 产物

- 本包局部矩阵中 `C5M11-BLK-000`、`C5M11-SCOPE-000`、`C5M11-ORC-001` 更新为 S0 完成态。
- 记录当前 wrapper gap 字段、fixture 列表、delete condition 和 native `Part::Sweep` overclaim guard。
- Root `C5-BLK-1101`、`C5-SCOPE-1101`、`C5-ORC-1101` 保持 pending，但补充 S0 live gap 结论。

## 非目标

- 不实现 collector。
- 不采集 expected。
- 不改变 capability 支持声明。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```
