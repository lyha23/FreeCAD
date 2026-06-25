# 【已实现】C7-M1 S3 ModelThread 与标准孔表发布收口

## 目标

执行 S2 裁决的 no-code publication closure。S2 没有 `backend_gap_requires_implementation`，因此 S3 不改几何实现、不改 tests、不新增 fixtures/expected，只把 supported / legacy / publication route 写成可发布状态。

## S3 执行基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=24b36ee45f`（`24b36ee45f 文档：完成 C7-M1 S2 准入裁决`），`git status --short -uall` 无输出。
- 队列状态：S3 执行前 C7-M1 队列为 S3-S5 pending；本文件标记为 `【已实现】` 后，队列应推进到 S4。
- 本步只更新 C7-M1 文档包和 TSV 矩阵；未改 C++、fixtures、expected 或 tests。

## route 落实

| route | S3 发布结论 |
| --- | --- |
| `already_closed_expected_backed` | supported standard/dynamic head cut、ModelThread metric、ModelThread counterbore、point/circle/arc profile source rows 已有 native expected 或 focused assertions，是 active closure。 |
| `historical_or_native_blocked` | legacy `hole-threaded-standard-*`、`hole-threaded-dynamic-*`、`hole-model-thread-metric`、thread clearance、thread depth pending expected stubs 只保留为 diagnostic historical / non-active legacy，不在 C7-M1 采集 oracle。 |
| `publication_closure_only` | capability 当前已发布 `remaining_gaps=[]`、ModelThread `pipe_shell`、Hole history covered 和 native oracle fixtures；S3 只收口文档/矩阵状态，capability drift 留给 S4 同步。 |
| `non_goal` | GUI conic edit、full Sketcher solver conic constraints、DistanceType default/todo、GUI Hole dialog、full Hole parity、full topo naming 不进入本包。 |

## no-code 边界

- 未修改 `cad-core/src/part_design/feature_hole.cpp`、`cad-core/src/runtime/capability_contract.cpp`、fixtures、expected 或 tests。
- 未采集 FreeCAD oracle，未新增 expected-backed case，未更新 test assertions。
- 未运行 `cmake --build build` 或 focused unittest；S3 的验证只覆盖队列、矩阵、路由词、空白和 diff check。
- 若未来重新出现 active backend gap，必须另开步骤重新裁决 code 落点、fixture rows、focused tests、capability/docs 更新项。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
rg -n 'backend_gap_requires_implementation|oracle_pending_collect|already_closed_expected_backed|publication_closure_only|historical_or_native_blocked|non_goal|no-code publication' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
git diff --check
```

## 通过条件

- S2 route 结论已落实为 publication closure。
- 本包内不再把 Hole ModelThread / 标准孔表 legacy pending rows 作为 active backend gap。
- S3 文件名和标题标记为 `【已实现】` 后，队列推进到 S4。
