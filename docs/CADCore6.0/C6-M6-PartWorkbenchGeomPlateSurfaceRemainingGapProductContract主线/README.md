# C6-M6 Part Workbench GeomPlateSurface Remaining Gap Product Contract 主线

本目录承接 C6-M5 之后的下一批 CAD Core 6.0 工作：围绕 `part_workbench.geomplate` 中仍处于 `remaining_gaps` 的 G1 curve-on-surface、无 InitialSurface 的 ProjectedCurve2d、curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle，建立一个可执行的产品合同收口包。

## 入口

- 主线总入口：`6-24-19-52-C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线总入口.md`
- 方案：`6-24-19-52-C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- C6-M6 已完成方案、队列创建和 S0 live 基线冻结；S1 到 S6 仍 pending。
- live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=fa5e3ebe33 docs: 完成 C6-M5 S6 发布闸门收口`；C6-M1 到 C6-M5 队列均已关闭。
- 当前 `part_workbench.geomplate` 仍发布为 source-backed geometry helper，不是 GUI feature，也不是原生 FreeCAD `DocumentObject`。
- 当前 active `remaining_gaps` 为 4 项：`g1_curve_on_surface_native_hidden_diagnostic_only`、`projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`、`curve_constraint_criteria_setters_not_implemented`、`platesurface_curves_wrapper_lifecycle`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线 docs/CADCore6.0/README.md
```
