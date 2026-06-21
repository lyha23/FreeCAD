# C5-M10 Part Workbench Sweep Advanced PipeShell Contract 主线

本包承接 C5-M6 已发布的 `part_workbench.sweep` 基础能力：multi-profile、`Linearize=true`、基础 `Solid/Frenet/Transition` 已作为 expected-backed slice 留在 live guard。本包已关闭原 broad advanced PipeShell bucket，把同一 `BRepOffsetAPI_MakePipeShell` 调用链、同一 request-local DTO / API 边界、同一类 wrapper expected 能覆盖的代表场景一次纳入：AuxiliarySpine、spine support、Binormal、profile location、tolerance 和组合压力。

边界必须写清楚：上游 `src/Mod/Part/App/PartFeatures.cpp::Part::Sweep::execute()` 只暴露 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`，高级 PipeShell 字段来自 `src/Mod/Part/App/BRepOffsetAPI_MakePipeShell*.pyi/PyImp.cpp` wrapper，以及同一 OCCT builder 在 `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()` 中的使用方式。C5-M10 可以把这些字段作为 `part_workbench.sweep` 的 request-local advanced contract 发布或诊断，但不能宣称它们是 upstream native `Part::Sweep` 的直接属性。

## 目标

- 冻结 C5-M6 / C4M1 `part-sweep-*` live guard，确保已发布 multi-profile / linearize support 不回退。
- 复核 FreeCAD wrapper 与 PartDesign Pipe 对 `BRepOffsetAPI_MakePipeShell` 的调用链，冻结 `PartSweepAdvancedPipeShellDTO` 字段、字段来源和 unsupported / known_gap 删除条件。
- 用一轮批次覆盖 AuxiliarySpine、spine support、Binormal、profile location、tolerance 和组合场景，而不是一次只做单个 fixture。
- 在 `cad-core/src/part/part_sweep.cpp`、`cad-core/include/cad_core/part/topo_shape_expansion.h`、`cad-core/src/part/topo_shape_expansion.cpp`、fixtures、tests、capability metadata 和 docs 中形成闭环。
- 收口原 advanced PipeShell broad bucket：已实现字段发布 source-backed 或 diagnostic-backed；仍无法 oracle 的字段必须有 source-backed known_gap、delete_condition 和下一 owner。

## 入口文件

- 方案：`6-21-21-49-【已实现】C5-M10-PartWorkbenchSweepAdvancedPipeShellContract方案.md`
- scope 矩阵：`矩阵/c5m10_sweep_advanced_pipeshell_scope.tsv`
- source / DTO / oracle 字段矩阵：`矩阵/c5m10_sweep_advanced_pipeshell_source_dto_oracle_contract.tsv`
- fixture / oracle 矩阵：`矩阵/c5m10_sweep_advanced_pipeshell_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m10_sweep_advanced_pipeshell_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m10_sweep_advanced_pipeshell_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m10_sweep_advanced_pipeshell_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live guard | C5-M6 / C4M1 `part-sweep-multi-profile-linearize` 与 `part-sweep-advanced-deferred` | S0 冻结基础 support、当前 deferred diagnostics、capability 状态和 root matrix |
| source / DTO / oracle | `BRepOffsetAPI_MakePipeShell` wrapper、`Part::Sweep::execute()`、`PartDesign::Pipe::setupAlgorithm()` | S1 写清 request-local DTO 字段、上游来源、oracle 可采字段、expected schema 和不能过度声明的 native `Part::Sweep` 边界 |
| mode / support family | AuxiliarySpine、spine support、Binormal、invalid mode/support diagnostics | S2 批量补 cad-core 实现、fixtures、expected 或 source-backed known_gap、focused tests |
| location / tolerance / combo family | profile `Location/WithContact/WithCorrection`、`SetTolerance`、advanced combination | S3 批量补 DTO parser、builder options、diagnostics、fixtures 和组合回归 |
| capability closeout | docs、capability metadata、root matrices、remaining gaps | S4 已关闭 broad advanced bucket，只保留字段级 `part_sweep_wrapper_expected_collector` source-backed known_gap |

## S0 状态

S0 已冻结 live baseline `cd4a092d9a`：`part-sweep-multi-profile-linearize` 继续作为 C5-M6/C4M1 expected-backed multi-profile / `Linearize=true` guard；`part-sweep-advanced-deferred` 只保护 `AuxiliarySpine`、`Tolerance` 等 advanced 字段的 locatable `unsupported_property` diagnostics。S0 当时不发布新 advanced support，后续字段级 support / known_gap / diagnostics 由 S2-S4 关闭。

## S4 收口状态

S4 已同步 `cad-core` capability metadata、adapter assertions、`docs/CADCore3.0/capabilities-gap对照表.md`、root C5 矩阵和本包局部矩阵。`part_workbench.sweep` 当前发布基础 `Part::Sweep` expected-backed 字段，以及 C5-M10 request-local advanced DTO 的 source/diagnostic-backed 字段级合同；剩余 gap 只保留 `part_sweep_wrapper_expected_collector`，删除条件是 FreeCADCmd wrapper collector 能稳定采集 `Part.BRepOffsetAPI_MakePipeShell` 的 `shape_summary` 与 `object_fields.advanced` metadata。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```

## 非目标

- 不改 FreeCAD upstream source。
- 不把 advanced wrapper 字段说成 upstream native `Part::Sweep` 属性。
- 不把 PartDesign Pipe / Hole 的产品支持归入 Part Workbench Sweep；只把它们作为同一 OCCT builder 语义依据。
- 不实现 GUI、TaskPanel、ViewProvider、command UI 或 cross-request persistent builder。
- 不用 bbox、输出顺序、fixture 名或 adapter 后处理替代 DTO / builder / mapper 层实现。
