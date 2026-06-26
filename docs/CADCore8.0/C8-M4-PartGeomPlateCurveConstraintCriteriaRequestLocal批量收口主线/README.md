# C8-M4 Part GeomPlate CurveConstraint Criteria Request-Local 批量收口主线

C8-M4 承接 C8-M3 完成后的 live gap：`part_workbench.geomplate.narrowed_gaps.curve_constraint_criteria_setters_not_implemented`。本包不继续扩展 conic 曲线，而是把 `Part.GeomPlate` 中 `CurveConstraint` 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 请求内 criteria 支持做成一轮批量裁决。

本包的关键边界是：FreeCAD 原生 `CurveConstraintPy` setter 可能仍是 `NotImplemented`，这不能被写成 native setter parity 已支持；但 `cad-core` 已有 DTO 字段与 OCCT `GeomPlate_CurveConstraint::SetG*Criterion` 落点，需要判断是否可以删除当前 `unsupported_curve_criteria` 诊断，转为 request-local product contract。

## 入口

- 总入口：`6-27-02-33-C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线总入口.md`
- 方案：`6-27-02-33-C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前基线

- 起点 HEAD：`48900289ec`（`48900289ec chore: 完成 C8-M3 S6 capability 发布闸门`）。
- C8-M3 队列为空，`part_workbench.conic_curves.remaining_gaps=[]`。
- C8-M4 队列新建后应从 S0 开始；索引文件本身标记为 `【已实现】`，避免被 `goal-step-runner` 当成待执行步骤。

## 批量范围

- 纳入：CurveConstraint 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 三个字段。
- 纳入：当前 `cad-core` DTO parser、OCCT apply path、fixture / focused tests、capability publication 口径。
- 纳入：FreeCAD `CurveConstraintPyImp.cpp` setter native 边界复核。
- 不纳入：`PlateSurface.Curves` wrapper 生命周期、GUI TaskPanel、跨请求 BREP / TopoDS_Shape / NamedShape cache、完整 GeomPlate 曲面族扩展。

## 通用验收

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```
