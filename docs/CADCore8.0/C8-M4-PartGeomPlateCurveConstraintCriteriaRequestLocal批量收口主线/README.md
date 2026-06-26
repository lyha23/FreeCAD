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
- S2 live 基线：`HEAD=37497319c6`（`37497319c6 docs: 完成 C8-M4 S1 源码覆盖复核`），`git status --short -uall` 无输出。
- S2 已将 S1 证据转成 route：CurveConstraint `G0Criterion` / `G1Criterion` / `G2Criterion` 三项同批进入 `request_local_backend_gap_candidate`，并确认当前是 DTO 字段缺失、`readCurveConstraints()` 以 `unsupported_curve_criteria` 阻断、Curve `addCurveConstraint()` path 未调用 `SetG*Criterion()` 的 request-local backend gap，不是 `already_supported` 或单纯 stale diagnostic。
- S2 将 FreeCAD native CurveConstraint setter 三项保持为 `native_oracle_blocked`，不阻止后续 cad-core request-local 实现；`PointConstraint` criteria 只作为 analog evidence；capability / diagnostics 为 publication pending；GUI、wrapper lifecycle、persistent geometry cache 为 `non_goal`。
- S2 关闭 `C8M4-BLOCKER-201`；完成后队列首项应前移到 S3。
- S3 live 基线：`HEAD=340ecaf864`（`340ecaf864 docs: 完成 C8-M4 S2 scope blocker 分类`），`git status --short -uall` 无输出。
- S3 已复核 native CurveConstraint criteria 边界：源码中 `setG0Criterion` / `setG1Criterion` / `setG2Criterion` 仍为 `PyExc_NotImplementedError("Not yet implemented")`，因此 `C8M4-ORACLE-101` 为 `native_oracle_blocked`；`PyInit` 与 `G0Criterion(u)` / `G1Criterion(u)` / `G2Criterion(u)` 证明 wrapped `GeomPlate_CurveConstraint` 有 criteria read state，但 getter 不证明 setter 支持。`FreeCADCmd 1.2.0 revision 20260519` 可构造 CurveConstraint，但 setter/getter runtime probe 均终止且未返回 JSON，记录为 `environment_probe_blocked`，不当作 semantic failure。
- S3 关闭 `C8M4-BLOCKER-301`；完成后队列首项应前移到 S4。
- S4 live 基线：`HEAD=c2b9038d0e`（`c2b9038d0e docs: 完成 C8-M4 S3 原生边界复核`），`git status --short -uall` 无输出。
- S4 已裁决打开 `open_S6_implementation_gate`：当前 Curve DTO 仍无 G0/G1/G2，`readCurveConstraints()` 仍由 `presentCriterionFields()` 在 source 创建前报 `unsupported_curve_criteria`，3D Curve `addCurveConstraint()` 仍未调用 `SetG0Criterion()` / `SetG1Criterion()` / `SetG2Criterion()`，但 `GeomPlateSourceEvidence` 已有 optional criteria 字段可复用；因此 S6 必须补 DTO、finite-number parser validation、3D curve apply path、request-local fixture、focused tests 和 capability publication。S4 关闭 `C8M4-BLOCKER-401`；完成后队列首项应前移到 S5。
- S0/S1/S2 只冻结声明、证据和 route 矩阵，不采 FreeCAD oracle，不新增 fixture / expected / tests / collector，不修改 C++ / Rust / FreeCAD `src/`。
- 输入 gap `part_workbench.geomplate.narrowed_gaps.curve_constraint_criteria_setters_not_implemented` 与 diagnostic `unsupported_curve_criteria` 在 S0 保留，不提前删除或声明 supported。

## 批量范围

- 纳入：CurveConstraint 的 `G0Criterion` / `G1Criterion` / `G2Criterion` 三个字段。
- 纳入：当前 `cad-core` DTO parser、OCCT apply path、fixture / focused tests、capability publication 口径；S4 已打开 S6 implementation gate，要求 Curve request-local criteria 同批补 DTO 字段、parser validation、3D Curve apply path 和 source evidence tests。
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
