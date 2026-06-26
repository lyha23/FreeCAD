# C8-M4 Part GeomPlate CurveConstraint Criteria Request-Local 批量收口主线

C8-M4 承接 C8-M3 完成后的 live gap：`part_workbench.geomplate.narrowed_gaps.curve_constraint_criteria_setters_not_implemented`。本包不继续扩展 conic 曲线，而是把 `Part.GeomPlate` 中 `CurveConstraint` 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 请求内 criteria 支持做成一轮批量裁决。

本包的关键边界是：FreeCAD 原生 `CurveConstraintPy` setter 可能仍是 `NotImplemented`，这不能被写成 native setter parity 已支持；S1 已确认当前 `cad-core` Curve DTO、parser 和 Curve builder 也尚未接通 criteria，需要由后续步骤判断是否打开 request-local product contract 实现闸门。

## 入口

- 总入口：`6-27-02-33-C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线总入口.md`
- 方案：`6-27-02-33-C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前基线

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=7a9fa7bdd3`（`7a9fa7bdd3 docs: 创建 C8-M4 GeomPlate criteria 收口方案`），`git status --short -uall` 无输出。
- 方案创建旧起点：`48900289ec`（`48900289ec chore: 完成 C8-M3 S6 capability 发布闸门`），只保留为创建时历史证据；S0 执行证据以上述 live 基线为准。
- C8-M3 队列为空，`part_workbench.conic_curves.remaining_gaps=[]`。
- C8-M4 S0 执行前队列首项是 `6-27-02-34-C8-M4-S0-live基线与批量范围冻结.md`；S0 完成后队列首项前移到 S1。
- S1 live 基线：`HEAD=d3d02af5e3`（`d3d02af5e3 docs: 完成 C8-M4 S0 live 基线冻结`），`git status --short -uall` 无输出。
- S1 已复核 current coverage：FreeCAD CurveConstraint 三个 native setter 仍为 `NotImplementedError`；Curve getter 证明 OCCT criteria state 存在；PointConstraint setter 只作为 analog；当前 `GeomPlateCurveConstraintSource` 没有 G0/G1/G2 字段，`readCurveConstraints()` 仍以 `unsupported_curve_criteria` 阻断，Curve `addCurveConstraint()` 路径尚未调用 `SetG*Criterion()`。
- S1 关闭 `C8M4-BLOCKER-101`；完成后队列首项应前移到 S2。
- S0 只冻结声明和矩阵，不采 FreeCAD oracle，不新增 fixture / expected / tests / collector，不修改 C++ / Rust / FreeCAD `src/`。
- 输入 gap `part_workbench.geomplate.narrowed_gaps.curve_constraint_criteria_setters_not_implemented` 与 diagnostic `unsupported_curve_criteria` 在 S0 保留，不提前删除或声明 supported。

## 批量范围

- 纳入：CurveConstraint 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 三个字段。
- 纳入：当前 `cad-core` DTO parser、OCCT apply path、fixture / focused tests、capability publication 口径；S1 已确认 Curve request-local criteria 仍缺 DTO 字段、parser 读取和 Curve apply path。
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
