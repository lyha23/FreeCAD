# CADCore7.0

CADCore7.0 承接 C6-M9 之后的下一轮 CAD Core 收口工作。C6-M9 已把 `PartDesign::Groove Type=UpToFirst/UpToFace` 裁决为 FreeCAD native `BRepFeat_MakeRevol` 稳定失败证据，`part_design.revolution_groove.remaining_gaps=[]`，不再作为实现缺口推进。

本轮不打开 C6-M10 Conic 小包：当前 conic 方向剩余项主要是 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo，这些不是当前无状态 CAD Core 后端实现批次。C7.0 的第一包转向 PartDesign Hole，但不重开泛 Hole 支持；live capability 已发布 `part_design.hole.remaining_gaps=[]`，`model_thread.status=done_first_slice`。C7-M1 的任务是把 Hole ModelThread、标准孔表驱动头部尺寸、点/圆/弧 profile source 和 history/capability 发布边界做成同一批次的闭环复核：批量采集 oracle、裁决旧 pending expected、必要时补 `cad-core` 实现、补 fixtures/focused tests、同步 capability/docs，并留下 release gate 记录。

## 入口

- C7-M1 总入口：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口主线总入口.md`
- C7-M1 方案：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口方案.md`
- C7-M1 工作步骤：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分/`
- C7-M1 矩阵：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/`

## 当前状态

- 方案创建基线：`HEAD=8fdf9b71c5`（`8fdf9b71c5 docs: 完成 C6-M9 S5 发布闸门`）。
- S0 live 基线已冻结：`HEAD=1624050685`（`1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`），`git status --short -uall` 无输出；C6-M1 到 C6-M9 的 `工作步骤细分` 队列均为空。
- C7-M1 当前队列已推进到 S1；S0 只确认 capability/test/legacy pending expected 现状，不裁决 S1/S2 rows。
- 当前 capability/test 基线：`part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`，`history.status=element_map_freeze_first_slice`，`history.remaining=[]`，`remaining_gaps=[]`；adapter tests 断言这些字段和 supported native oracle fixtures，legacy `hole-threaded-standard-counterbore/countersink` expected 仍写 `oracle pending`。
- C7-M1 不声明 full FreeCAD Hole parity，不声明 GUI Hole dialog，不声明 full topo naming / full MapperHistory。
- C7-M1 必须在 S2 之后才允许修改 C++、fixtures 或 expected；S2 必须先把代表场景裁决为 `already_closed`、`oracle_pending`、`backend_gap_requires_implementation`、`historical_or_native_blocked` 或 `non_goal`。

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
