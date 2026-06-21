# 【已实现】C5-M10-S4 capability 与文档收口

状态：`done_C5M10-S4_capability_docs_closed`

## 目标

在 S2/S3 实现后收口原 advanced PipeShell broad bucket。S4 的任务是把 support、source-backed known_gap、diagnostic、non-goal 和 remaining gap 全部写成字段级边界，并同步 root C5 矩阵、package-local 矩阵、capability docs 和 adapter capability assertions。

## 完成结果

- `cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.sweep` capability 已更新为 `supported_multi_profile_linearize_c5m10_advanced_source_diagnostic_backed_closeout`，新增 `field_boundaries` 与 `source_backed_known_gaps.part_sweep_wrapper_expected_collector`，并把 remaining gap 收敛为唯一 collector 缺口。
- `cad-core/tests/test_adapters.py` 已断言字段级 expected-backed / source-backed known_gap / diagnostic-backed / non-goal 边界、wrapper collector delete condition 和 remaining gap 精确列表。
- `docs/CADCore3.0/capabilities-gap对照表.md`、C5 README、root C5 矩阵和本包局部矩阵已同步最终状态；`C5-BLK-1001`、`C5-SCOPE-1001`、`C5-ORC-1005`、`C5M10-BLK-401`、`C5M10-SCOPE-401`、`C5M10-ORC-401` 均已关闭。
- 本 step 文件已改名为 `6-21-21-54-【已实现】C5-M10-S4-capability与文档收口.md`；本包方案文件已改名为 `6-21-21-49-【已实现】C5-M10-PartWorkbenchSweepAdvancedPipeShellContract方案.md`。

## 必读

- S0-S3 完成后的全部变更。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- `part_workbench.sweep` capability metadata 不再保留 broad advanced bucket；改为字段级 supported / source-backed known_gap / diagnostic / non-goal。
- `docs/CADCore3.0/capabilities-gap对照表.md` 同步 C5-M10 边界：基础 Sweep support 仍来自 C5-M6，advanced request-local contract 来自 C5-M10。
- 本包局部矩阵关闭 S4 之前的 pending rows，并把 `C5M10-BLK-401` 标为完成。
- Root `C5-SRC-010`、`C5-SCOPE-1001`、`C5-BLK-1001`、`C5-ORC-1001..1005`、`C5-VAL-1001` 等行同步最终状态。
- 工作步骤文件完成后改名或标题加 `【已实现】`，确保 queue script 返回空队列。

## 非目标

- 不新增 S2/S3 之外的新 advanced case。
- 不把暂不能采集 native expected 的 source-backed known_gap 写成 expected-backed。
- 不改 GUI、PartDesign Pipe 产品边界或 upstream FreeCAD。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
