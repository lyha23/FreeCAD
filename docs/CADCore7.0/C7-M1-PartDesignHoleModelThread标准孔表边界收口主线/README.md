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
- S0-S5 尚未执行；`工作步骤细分/6-25-14-03-【已实现】C7-M1工作步骤总入口.md` 只是索引，具体 pending step 从 S0 开始。
- S2 前不得改 C++、fixtures、expected 或 tests。
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
