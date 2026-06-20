# C5-M6 Part Workbench Surface Profile / PostProcess 第二批主线总入口

## 目标

本主线承接 ProjectOnSurface 收口后的下一类高价值 surface family 语义，但不把它混入 `PartDesign::Loft` / `PartDesign::Pipe`。本包只处理 source-backed `Part::Loft` 与 `Part::Sweep` 的同类调用链：profile family、`Linearize=true` 后处理、Sweep multi-profile 与相关 capability 发布边界。

当前 live 代码已出现 `part_workbench.loft.status=supported_profile_linearize_expected_backed` 与 `part_workbench.sweep.status=supported_multi_profile_linearize_expected_backed`，因此本包先做 live 复核和文档收口；若执行时发现 fixture / expected / focused tests 缺失，再按 oracle-first 补实现。

## 包结构

- 方案：`6-20-22-03-C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批方案.md`
- scope 矩阵：`矩阵/c5m6_surface_profile_postprocess_scope.tsv`
- fixture / oracle 矩阵：`矩阵/c5m6_surface_profile_postprocess_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m6_surface_profile_postprocess_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m6_surface_profile_postprocess_non_goal_registry.tsv`
- 验收矩阵：`矩阵/c5m6_surface_profile_postprocess_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## S0 live 事实冻结

- live 基线：`HEAD=7217840df4`，`git log -1 --oneline=7217840df4 feat: 发布ProjectOnSurface能力收口`。
- S0 起始 dirty 集合为本 C5-M6 包草案与 CADCore5 根 README/矩阵行；本步不改 C++、不新增 fixture / expected。
- `part_workbench.loft` 只冻结 face / vertex profile 与 `Linearize=true` expected-backed slice；`complex_profile_family` 保持 future owner。
- `part_workbench.sweep` 只冻结 multi-profile `Sections` 与 `Linearize=true` expected-backed slice；advanced PipeShell wrapper 仅有 deferred diagnostics，不计入 supported。

## 执行顺序

1. `C5-M6-S0`：live 基线、scope 和当前 supported 事实冻结。
2. `C5-M6-S1`：Loft face / vertex profile 与 `Linearize=true` 复核收口。
3. `C5-M6-S2`：Sweep multi-profile 与 `Linearize=true` 复核收口。
4. `C5-M6-S3`：剩余 complex profile / advanced PipeShell contract 分流。
5. `C5-M6-S4`：capability、CADCore3.0 文档和队列收口。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分 --format markdown
```

## 边界

- 本包不是 PartDesign `FeatureLoft` / `FeaturePipe` 的继续实现，不消费 C5-M3 的 PartDesign known-gap oracle。
- 不把 Filling / GeomPlate advanced constraints 混入本轮；它们的 DTO / API 边界不同。
- 不把 advanced `BRepOffsetAPI_MakePipeShell` wrapper、Hole internal PipeShell 或 GUI task panel 算作 `Part::Sweep` 支持。
