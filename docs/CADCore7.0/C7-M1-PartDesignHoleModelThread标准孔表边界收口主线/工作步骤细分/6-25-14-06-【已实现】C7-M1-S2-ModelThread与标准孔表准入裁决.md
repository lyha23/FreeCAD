# 【已实现】C7-M1 S2 ModelThread 与标准孔表准入裁决

## 目标

基于 S0/S1 evidence，对每个 Hole representative 给出 route。S2 是代码闸门；没有 `backend_gap_requires_implementation` 结论，不得改 C++、fixtures、expected 或 tests。

## S2 执行基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=41c62e7070`（`41c62e7070 docs: 完成 C7-M1 S1 源码与 oracle 矩阵`），`git status --short -uall` 无输出。
- 队列状态：S2 执行前 C7-M1 队列为 S2-S5 pending；本文件标记为 `【已实现】` 后，队列应推进到 S3。
- 本步只更新 C7-M1 文档包和 TSV 矩阵；未改 C++、fixtures、expected 或 tests。

## route 裁决汇总

| route | rows | S2 裁决 |
| --- | --- | --- |
| `already_closed_expected_backed` | supported standard/dynamic head cut、ModelThread metric、ModelThread counterbore、point/circle/arc profile source、depth/shape 输入组 | 这些 rows 已有 native expected 或 focused tests，能够支撑当前 backend 语义闭环。 |
| `publication_closure_only` | live capability baseline、adapter publication、focused tests/docs family | capability 已发布 `remaining_gaps=[]`、ModelThread `pipe_shell` 和 native oracle fixtures；后续只同步发布口径。 |
| `historical_or_native_blocked` | legacy `hole-threaded-standard-*`、`hole-threaded-dynamic-*`、`hole-model-thread-metric`、thread clearance、thread depth pending expected stubs | 这些旧 rows 仍写 `hole_thread_geometry_oracle_pending`，但已有 supported counterpart；C7-M1 不补 oracle，不把它们算作 active backend failure。 |
| `non_goal` | GUI conic edit、full Sketcher solver conic constraints、DistanceType default/todo、GUI Hole dialog、full Hole parity、full topo naming、arbitrary external thread standards | 不属于本轮 PartDesign Hole ModelThread / 标准孔表 DTO 与 expected family。 |

## legacy pending expected 裁决

- `hole-threaded-standard-counterbore` / `hole-threaded-standard-countersink`：保留 diagnostic historical；active closure 映射到 `hole-supported-threaded-standard-counterbore` / `hole-supported-threaded-standard-countersink`。
- `hole-threaded-dynamic-iso2009` / `hole-threaded-dynamic-din7984`：保留 diagnostic historical；active closure 映射到 `hole-supported-threaded-dynamic-iso2009` / `hole-supported-threaded-dynamic-din7984`。
- `hole-model-thread-metric`：保留 diagnostic historical；active closure 映射到 `hole-supported-model-thread-metric`。
- `hole-thread-class-clearance`、`hole-thread-custom-clearance`、`hole-thread-depth-dimension-clamped`、`hole-thread-depth-din76`：保留 diagnostic historical；active closure 映射到对应 `hole-supported-*` rows。
- 上述 legacy rows 不在 C7-M1 采集 FreeCAD oracle，也不得直接等同后端失败。

## ModelThread + head cut 结论

`hole-supported-model-thread-counterbore` 已有 native expected，focused test 断言 `topology_counts={edges:106,faces:50,vertices:60}`、`volume=434.05359569539525`、`model_thread_compound_tool_shape`、`threaded_model_thread_head_cut_native_oracle`、`hole_model_thread:pipe_shell_tool_history`，且没有 `topology_gap` 或 `geometry_fallback`。

S2 裁决：ModelThread + head cut 没有 geometry/topology/history active gap。命名顺序差异不得算硬失败；只有 face/edge/vertex 数量、几何内容、稳定 subname 或引用语义不稳定才会重新打开 backend gap。

## S3 / S4 准入

- S3 不允许改 `cad-core/src/part_design/feature_hole.cpp`、其它 C++、fixtures、expected 或 tests。
- S3/S4 只允许 no-code publication closure：同步 README、总入口、矩阵、release-gate 记录和 stale roadmap wording。
- 若未来出现新的 `backend_gap_requires_implementation`，必须另开步骤重新写清 code 落点、fixture rows、focused tests、capability/docs 更新项；本 S2 没有授权实现。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
rg -n 'backend_gap_requires_implementation|oracle_pending_collect|already_closed_expected_backed|publication_closure_only|historical_or_native_blocked|non_goal' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
git diff --check
```

## 通过条件

- 每个 representative 都有明确 route 和下一步。
- S3 是否允许改代码被写清楚：不允许。
- S2 文件名和标题标记为 `【已实现】` 后，队列推进到 S3。
