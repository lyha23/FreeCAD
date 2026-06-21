# 【已实现】C5-M10-S0 live 基线与 scope 冻结

状态：`done_C5M10-S0_live_guard`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
cd4a092d9a

git log -1 --oneline
cd4a092d9a 文档: 收口C5-M9投影来源能力边界

git -c core.quotepath=false status --short -uall
 M docs/CADCore5.0-PartDesign-高价值剩余语义/README.md
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_blocker_queue.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_fixture_oracle_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_non_goal_registry.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_scope_review_matrix.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_source_candidates.tsv
 M docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_validation_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/6-21-21-49-C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线总入口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/6-21-21-49-C5-M10-PartWorkbenchSweepAdvancedPipeShellContract方案.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/6-21-21-50-C5-M10-S0-live基线与scope冻结.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/6-21-21-51-C5-M10-S1-sourceDtoOracle矩阵.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/6-21-21-52-C5-M10-S2-auxiliarySupportBinormal实现.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/6-21-21-53-C5-M10-S3-locationToleranceCombo实现.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分/6-21-21-54-C5-M10-S4-capability与文档收口.md
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/矩阵/c5m10_sweep_advanced_pipeshell_blocker_queue.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/矩阵/c5m10_sweep_advanced_pipeshell_fixture_oracle_matrix.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/矩阵/c5m10_sweep_advanced_pipeshell_non_goal_registry.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/矩阵/c5m10_sweep_advanced_pipeshell_scope.tsv
?? docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/矩阵/c5m10_sweep_advanced_pipeshell_validation_matrix.tsv
```

S0 起点已有 root C5 矩阵/README 修改和未跟踪 C5-M10 包文件；它们属于本轮上下文。本步骤只在这些 C5-M10 文档/矩阵内冻结 live guard，不清理或回退其它改动。

## S0 核查结论

- `cad-core/fixtures/c4m1/part-sweep-multi-profile-linearize.json` 当前固定 `Part::Sweep` 的 `Spine=Path`、两个 `Sections`、`Frenet=true`、`Transition=1` 和 `Linearize=true`；这是 C5-M6/C4M1 已发布的 expected-backed multi-profile / linearize guard，不回退。
- `cad-core/fixtures/c4m1/part-sweep-advanced-deferred.json` 当前只给 `AdvancedSweep` 传入 `AuxiliarySpine` 和 `Tolerance`，用于保护 advanced 字段的 locatable `unsupported_property` diagnostics，不产生 supported shape。
- `cad-core/src/part/part_sweep.cpp` 当前执行器只按 `Sweep::execute()` 消费 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`；`rejectDeferredSweepAdvancedProperties()` 对 `AuxiliarySpine`、`SupportMode`、`BiNormal`、`LocationMode`、`Tolerance` 保持 deferred diagnostic baseline。
- S0 起点时 `cad-core/src/adapters/c_api/c_api.cpp` capability 仍是 `supported_multi_profile_linearize_expected_backed`，covered slice 包含 multi-profile、linearize post-processing、PipeShell maker history 和 advanced deferred diagnostics；S0 不发布 AuxiliarySpine、SupportMode、Binormal、Location 或 Tolerance 支持。
- S0 起点时 `docs/CADCore3.0/capabilities-gap对照表.md` 把 `part_workbench.sweep` 的 supported slice 写为 C5-M6/C4M1 基础能力，并把 advanced PipeShell wrapper、located profile、support、trihedron / binormal、location mode、tolerance contract 继续路由到后续 broad owner；该 broad owner 已在 S4 收口为字段级 `part_sweep_wrapper_expected_collector` known_gap。

## 目标

冻结 C5-M10 Sweep advanced PipeShell 主线的起点：确认 `part_workbench.sweep` 已发布的 C5-M6/C4M1 multi-profile + `Linearize=true` expected-backed support 不回退，并记录当前 `part-sweep-advanced-deferred` 对 advanced 字段的 diagnostic baseline。S0 不改 cad-core 代码，只校正本包矩阵、root C5 矩阵和 scope 边界。

## 必读

- 本包总入口与方案。
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `cad-core/fixtures/c4m1/part-sweep-multi-profile-linearize.json`
- `cad-core/fixtures/c4m1/part-sweep-advanced-deferred.json`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 产物

- 本包局部矩阵中 `C5M10-BLK-000`、`C5M10-SCOPE-000`、`C5M10-ORC-001` 状态从 pending 更新为 S0 完成态。
- Root `C5-BLK-1001`、`C5-SCOPE-1001`、`C5-ORC-1001` 保持 C5-M10 pending 但补充 S0 live guard 结论。
- 记录 S0 起点 capability 里 `part_workbench.sweep` supported slice 和 advanced broad gap，不提前改支持声明。
- 如发现 root README 或矩阵与当前代码不一致，只修 C5-M10 相关边界，不重开 C5-M6/C5-M9 closed rows。

## 非目标

- 不实现 AuxiliarySpine、Binormal、SupportMode、Location 或 Tolerance。
- 不采集新 oracle。
- 不改 upstream FreeCAD source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```
