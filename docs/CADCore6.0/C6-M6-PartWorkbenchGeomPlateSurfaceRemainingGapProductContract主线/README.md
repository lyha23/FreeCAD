# C6-M6 Part Workbench GeomPlateSurface Remaining Gap Product Contract 主线

本目录承接 C6-M5 之后的下一批 CAD Core 6.0 工作：围绕 `part_workbench.geomplate` 中仍处于 `remaining_gaps` 的 G1 curve-on-surface、无 InitialSurface 的 ProjectedCurve2d、curve criteria setter 和 `Part.PlateSurface.Curves` wrapper lifecycle，建立一个可执行的产品合同收口包。

## 入口

- 主线总入口：`6-24-19-52-C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线总入口.md`
- 方案：`6-24-19-52-C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- C6-M6 已完成方案、队列创建、S0 live 基线冻结、S1 source / wrapper / oracle 候选矩阵、S2 remainingGap 准入路由、S3 G1 / ProjectedCurve2d 合同实现或收窄、S4 criteria / wrapper boundary 冻结、S5 fixtures / tests / capability / docs 发布同步和 S6 阶段回归 release gate。
- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`；`HEAD=05dbaf0c53 docs: 完成 C6-M6 S1 GeomPlate 源码候选矩阵`；开始时工作区干净，C6-M6 queue 从 S2 开始。
- 当前 `part_workbench.geomplate` 仍发布为 source-backed geometry helper，不是 GUI feature，也不是原生 FreeCAD `DocumentObject`。
- S5 发布后 `remaining_gaps=[]`；上述 4 项不再是 active gap，分别保留为 `narrowed_gaps`、`non_goals` 和 historical evidence，不写成 FreeCAD parity 或 full GeomPlate support。
- S6 release gate 已通过：阶段回归 `Ran 251 tests in 136.165s`，`OK (skipped=31)`；heavy 收口 `Ran 287 tests in 126.397s`，`OK (skipped=31)`；C6-M6 queue 返回空表。
- S2 准入路由只使用 5 个状态：`implementationReady`、`nativeOracleBlocked`、`diagnosticOnly`、`nonGoal`、`releaseGate`。
- S3 批次已完成并由 S5 发布：`G1 curve-on-surface` 发布为 request-local source-backed product contract，并保留 native-hidden expected 删除条件；`ProjectedCurve2d without InitialSurface` 保留 `V1==V2` nativeOracleBlocked，同时新增 C6-M6 no-InitialSurface contract guard，不把 cad-core output 写成 FreeCAD expected。
- S4 批次已冻结并由 S5 发布：`curve criteria setter` 保留 `unsupported_curve_criteria` locatable diagnostic 和 delete condition，`PlateSurface.Curves wrapper lifecycle` 保留 `unsupported_wrapper_lifecycle` / `nonGoal` 和 reopen condition；未新增 C6-M6 wrapper fixture，因为 FreeCAD 仍无 request-local safe contract，仍不得引入 persistent wrapper state、fake native `DocumentObject` 或 cross-request geometry cache。

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
