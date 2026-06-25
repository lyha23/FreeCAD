# CADCore7.0

CADCore7.0 承接 C6-M9 之后的下一轮 CAD Core 收口工作。C6-M9 已把 `PartDesign::Groove Type=UpToFirst/UpToFace` 裁决为 FreeCAD native `BRepFeat_MakeRevol` 稳定失败证据，`part_design.revolution_groove.remaining_gaps=[]`，不再作为实现缺口推进。

本轮不打开 C6-M10 Conic 小包：当前 conic 方向剩余项主要是 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo，这些不是当前无状态 CAD Core 后端实现批次。C7.0 的第一包转向 PartDesign Hole，但不重开泛 Hole 支持；live capability 已发布 `part_design.hole.remaining_gaps=[]`，`model_thread.status=done_first_slice`。C7-M1 的任务是把 Hole ModelThread、标准孔表驱动头部尺寸、点/圆/弧 profile source 和 history/capability 发布边界做成同一批次的闭环复核：裁决旧 pending expected，确认 active rows 已经 expected-backed，保持 legacy rows 为 historical/non-active，并同步 capability/docs 与 release gate 记录。

## 入口

- C7-M1 总入口：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口主线总入口.md`
- C7-M1 方案：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口方案.md`
- C7-M1 工作步骤：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分/`
- C7-M1 矩阵：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/`

## 当前状态

- 方案创建基线：`HEAD=8fdf9b71c5`（`8fdf9b71c5 docs: 完成 C6-M9 S5 发布闸门`）。
- S0 live 基线已冻结：`HEAD=1624050685`（`1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`），`git status --short -uall` 无输出；C6-M1 到 C6-M9 的 `工作步骤细分` 队列均为空。
- S1-S3 已完成：FreeCAD Hole 源码链、cad-core capability/tests/fixtures 和 oracle rows 已被矩阵化；S2 确认没有 `backend_gap_requires_implementation`，S3 完成 no-code publication closure。
- S4 已完成：没有修改 C++、fixtures、expected 或 tests；focused unittest 验证 `part_design.hole` capability、ModelThread pipe-shell、standard/dynamic head cut、native oracle fixtures、history covered/remaining 和 adapter assertions 全部一致。
- S5 已完成：live 起点 `HEAD=dd2b919e46`（`dd2b919e46 文档：完成 C7-M1 S4 发布同步`），`git status --short -uall` 无输出；release gate `cmake --build build` 通过，S4/S5 focused unittest 5 tests OK。本包没有代码、fixtures、expected、test、topo/history 或 adapter schema 改动，未触发重型阶段回归。
- 当前 capability/test 发布口径：`part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`，`history.status=element_map_freeze_first_slice`，`history.remaining=[]`，`native_oracle_known_gap_fixtures=[]`，`remaining_gaps=[]`；adapter tests 断言这些字段和 supported native oracle fixtures。
- expected-backed rows 的 expected 文件记录 `FreeCADCmd oracle from ...`、`freecad_version=1.2.0 revision 20260519`、topology/volume，不是从当前 `cad-core` 输出倒推；legacy `hole-threaded-standard-*`、`hole-threaded-dynamic-*`、`hole-model-thread-metric`、thread clearance/depth pending stubs 只保留为 historical/non-active diagnostic。
- C7-M1 不声明 full FreeCAD Hole parity，不声明 GUI Hole dialog，不声明 full topo naming / full MapperHistory；当前队列为空。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0
```
