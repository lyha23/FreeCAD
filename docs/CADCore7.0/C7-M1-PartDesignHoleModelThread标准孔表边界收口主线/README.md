# C7-M1 PartDesign Hole ModelThread 标准孔表边界收口主线

本目录是 CADCore7.0 的第一条主线。它不重开泛 Hole 能力，而是围绕同一 FreeCAD 调用链、同一 PartDesign Hole DTO/API 边界、同一 `cad-core/fixtures/p7/hole-*` expected 家族做批量收口。

当前 live capability 已显示 `part_design.hole.remaining_gaps=[]`，`model_thread.status=done_first_slice`，并列出 `p7/hole-supported-threaded-dynamic-iso2009`、`p7/hole-supported-threaded-dynamic-din7984`、`p7/hole-supported-model-thread-metric`、`p7/hole-point-profile`、`p7/hole-supported-point-counterbore`、`p7/hole-supported-model-thread-counterbore` 等 native oracle fixtures。C7-M1 的价值不是从零实现 Hole，而是确认这些发布项和旧 pending standard rows、ModelThread pipe-shell history、profile source 映射、capability/test/docs 是否已经形成可审计闭环；若 S2 证明还有 active backend gap，S3 再做源码级实现。

## 入口

- 主线总入口：`6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口主线总入口.md`
- 方案：`6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 方案创建基线：`HEAD=8fdf9b71c5`（`8fdf9b71c5 docs: 完成 C6-M9 S5 发布闸门`）。
- S0 已完成：live baseline 为 `HEAD=1624050685`（`1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`），`git status --short -uall` 无输出；C6-M1 到 C6-M9 队列为空，C7-M1 队列推进到 S1。
- S0 冻结结论：`part_design.hole.remaining_gaps=[]`、`model_thread.status=done_first_slice`、`model_thread.geometry=pipe_shell`、`history.remaining=[]`；`test_adapters.py` 断言 capability fields 和 native oracle fixtures，`test_p7_features.py` 覆盖 supported standard head cut、ModelThread metric/counterbore 和 head-cut oracle matrix。
- legacy `cad-core/fixtures/p7/expected/hole-threaded-standard-counterbore.freecad.json` 与 `hole-threaded-standard-countersink.freecad.json` 仍是 `hole_thread_geometry_oracle_pending`；S0 只冻结该事实，S1/S2 再裁决收集、迁移或 legacy/non-active 路由。
- S1 已完成：live baseline 为 `HEAD=669974037a`（`669974037a 文档：冻结 C7-M1 S0 Hole live 基线`），`git status --short -uall` 无输出；S1 复核 `FeatureHole.cpp` 调用链和 `feature_hole.cpp` / capability / focused tests，已把 supported native oracle、legacy pending expected、ModelThread + head cut、point/circle/arc profile source rows 写入矩阵。
- S2 已完成：live baseline 为 `HEAD=41c62e7070`（`41c62e7070 docs: 完成 C7-M1 S1 源码与 oracle 矩阵`），`git status --short -uall` 无输出；supported native oracle rows 均裁决为 `already_closed_expected_backed`，capability/docs 漂移裁决为 `publication_closure_only`，legacy pending expected rows 裁决为 `historical_or_native_blocked` 的 diagnostic historical / non-active legacy。
- S2 结论：没有 `backend_gap_requires_implementation`，ModelThread + head cut 不存在 geometry/topology/history active gap；命名顺序差异不得算硬失败。S3/S4 只允许 no-code publication closure，不改 C++、fixtures、expected 或 tests。
- 本主线明确排除 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo、GUI Hole dialog 和 full topo naming。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```
