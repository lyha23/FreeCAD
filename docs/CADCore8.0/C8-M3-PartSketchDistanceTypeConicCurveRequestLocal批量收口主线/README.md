# C8-M3 Part / Sketch / DistanceType Conic Curve Request-Local 批量收口主线

本目录承接 C8-M2 no-code release gate 之后的下一轮 CADCore8.0 工作。C8-M2 已明确 `SubShapeBinder BindCopyOnChange` full temporary-document cache 继续保持 `oracle_blocked` / `known_gap_diagnostic`，不应继续在同一条线里硬开 C++ implementation gate。

C8-M3 转向当前 live capability 中仍有 active `remaining_gaps` 的 `part_workbench.conic_curves`，但不把它拆成单个字符串修补。它把同一条 conic 曲线语义链作为一个批量闭环处理：

- `PartConicCurveDTO` 的 Hyperbola / Parabola request-local edge producer 与 Part consumer。
- Sketcher 的 ArcOfHyperbola / ArcOfParabola 输入、profile、external-reference 和 solver-facing 状态边界。
- Assembly / Ondsel `DistanceType` 默认分类与 conic / curve reference 的关系。
- GUI conic edit 与 full Sketcher solver conic constraints 的 non-goal 发布边界。

## 入口

- 主线总入口：`6-27-01-00-C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线总入口.md`
- 方案：`6-27-01-00-C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-27-01-00-【已实现】C8-M3工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- C8-M2 队列为空，`SubShapeBinder` CopyOnChange full temporary-document cache 未打开实现闸门。
- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=c6a848b69c`（`c6a848b69c docs: 完成 C8-M2 S6 发布闸门`）。S0 开始状态只包含 `docs/CADCore8.0/README.md` 与本 C8-M3 文档包 / 矩阵 / 工作步骤变更，未见代码、fixture、expected 或 collector dirty 文件。
- live capability 中 `part_workbench.conic_curves.status=done_part_geometry_curve_edge_consumer`，已有 `PartConicCurveDTO`、Hyperbola / Parabola edge producer、Extrusion / RuledSurface consumer 和 p8 fixtures。
- live capability 中 `part_workbench.conic_curves.remaining_gaps=["gui_conic_edit","full_sketcher_solver_conic_constraints","distance_type_default_todo"]`，S0 只把这三项记录为输入，不删除、不声明支持。
- C8-M3 S0 已完成 live 基线与批量范围冻结，`C8M3-BLOCKER-000` 已关闭；S1-S6 仍待执行。矩阵是 seed，不是发布结论。

## 批量边界

- 本包优先把同一 FreeCAD conic geometry 调用链、同一 request-local DTO/API 边界、同一类 expected 能覆盖的 Part / Sketch / DistanceType 代表场景纳入同一轮。
- 只有当 S2 证明 FreeCAD 调用链分叉、oracle 无法采集、语义边界不清或实现风险跨模块扩散时，才把某个代表场景拆到下一批。
- 拆分必须记录下一批范围和删除条件，避免长期停留在单 fixture 推进。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```
