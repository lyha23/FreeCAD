# 【已实现】C8-M4-S1 FreeCAD 源码与 current 覆盖批量复核

## 目标

复核 CurveConstraint criteria 的 FreeCAD source authority、当前 `cad-core` DTO / parser / OCCT apply path、existing tests 和 capability publication。S1 只做证据复核和矩阵回写，不改 C++。

## live 基线

本步骤已记录：

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`d3d02af5e3`
- `git log -1 --oneline`：`d3d02af5e3 docs: 完成 C8-M4 S0 live 基线冻结`
- `git status --short -uall`：无输出，开始工作区干净。
- S1 执行前 C8-M4 队列首项为 `6-27-02-35-C8-M4-S1-FreeCAD源码与current覆盖批量复核.md`。

## S1 复核结论

- FreeCAD `CurveConstraintPy::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()` 当前仍全部显式 `PyExc_NotImplementedError("Not yet implemented")`。这只关闭 source/current coverage 复核，不关闭 native setter parity。
- FreeCAD `CurveConstraintPy::G0Criterion(u)` / `G1Criterion(u)` / `G2Criterion(u)` 直接读取 wrapped `GeomPlate_CurveConstraint` 的 criteria state，证明 OCCT 约束对象存在 criteria 读状态；但 getter 不能证明 Python setter 支持。
- `PointConstraintPy::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()` 已实现并调用 `GeomPlate_PointConstraint::SetG*Criterion()`，只能作为 criteria setter analog，不能替代 CurveConstraint native setter parity。
- `cad-core` 当前 `GeomPlateCurveConstraintSource` 没有 `g0Criterion` / `g1Criterion` / `g2Criterion` 字段。当前源码中存在这些 optional 字段的是 `GeomPlatePointConstraintSource` 和 `GeomPlateSourceEvidence`，不是 CurveConstraint DTO。
- `readCurveConstraints()` 的真实阻断是：发现 `G0Criterion` / `G1Criterion` / `G2Criterion` 任一字段即发布 `unsupported_curve_criteria` 并在创建 `GeomPlateCurveConstraintSource` 前返回。由于 Curve DTO 没有 criteria 字段，parser 也没有把这些字段读入 curve source；这不是单纯 stale diagnostic。
- `addCurveConstraint()`、`addCurveOnSurfaceConstraint()` 和 `addCurve2dConstraint()` 当前没有对 CurveConstraint 调用 `SetG0Criterion()` / `SetG1Criterion()` / `SetG2Criterion()`。这些 `SetG*Criterion()` 调用只存在于 `addPointConstraint()`。
- capability 当前同时列出 `Objects[].Properties.CurveConstraints.SubSet[].G0Criterion/G1Criterion/G2Criterion` payload key，但把 Curve criteria 放在 `unsupported_curve_criteria` diagnostic 和 `curve_constraint_criteria_setters_not_implemented` narrowed gap 下；`remaining_gaps` 为空，`covered` 中没有 `curve_constraint_criteria`。
- native setter boundary 与 `cad-core` request-local boundary 必须分离：FreeCAD native setter 当前仍 blocked；`cad-core` request-local criteria 支持理论上可作为独立产品契约进入 S4/S6，但当前实现并未已经具备 Curve DTO / parser / apply path。
- `C8M4-BLOCKER-101` 已关闭为 source/current coverage 已复核；S2 应基于“Curve request-local backend gap confirmed”而不是“DTO 已有字段/Apply path 已完成”的旧假设继续分类。

## 已回写的矩阵

- `c8m4_geomplate_criteria_source_candidates.tsv`
- `c8m4_geomplate_criteria_scope_review_matrix.tsv`
- `c8m4_geomplate_criteria_backend_gap_classification.tsv`
- `c8m4_geomplate_criteria_blocker_queue.tsv`

## 必读源码

FreeCAD：

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::getG0Criterion/getG1Criterion/getG2Criterion`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setCurve2dOnSurf/setProjectedCurve`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion`

cad-core：

- `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/part_geomplate.h::GeomPlateCurveConstraintSource`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/part_geomplate.cpp::readCurveConstraints()`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/part_geomplate.cpp::addCurveConstraint()`
- `/home/user/Chili3DProject/FreeCAD/cad-core/src/runtime/capability_contract.cpp`
- `/home/user/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`
- `/home/user/Chili3DProject/FreeCAD/cad-core/tests/test_adapters.py`

## 复核问题

1. FreeCAD `CurveConstraintPy` 的 G0 / G1 / G2 setter 当前是否仍显式 `NotImplemented`。
2. FreeCAD getter 是否证明底层 OCCT constraint 存在 criteria 状态。
3. `PointConstraintPy` setter 是否可作为 criteria setter analog，而不是 Curve setter parity。
4. `cad-core` DTO 是否已经持有 G0 / G1 / G2。
5. `readCurveConstraints()` 阻断 criteria 的原因是否只是 native setter gap，还是还有 DTO / validation / OCCT 风险。
6. `addCurveConstraint()` 是否已经按 source criteria 调用 OCCT `SetG*Criterion`。
7. tests 和 capability 中当前如何表述 `unsupported_curve_criteria`。

## 必须回写的矩阵

- `c8m4_geomplate_criteria_source_candidates.tsv`
- `c8m4_geomplate_criteria_scope_review_matrix.tsv`
- `c8m4_geomplate_criteria_backend_gap_classification.tsv`
- `c8m4_geomplate_criteria_blocker_queue.tsv`

## 验收标准

- `C8M4-BLOCKER-101` 关闭为 source/current coverage 已复核。
- 每个 source row 有 file、symbol、evidence、cad-core landing 和 S1 结论。
- S1 结论必须明确：FreeCAD native setter boundary 与 `cad-core` request-local boundary 分离；并明确当前 Curve DTO / parser / apply path 仍是 request-local backend gap。
- 不新增 fixture / expected / tests，不改 C++。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'setG0Criterion|setG1Criterion|setG2Criterion|getG0Criterion|getG1Criterion|getG2Criterion|PyExc_NotImplementedError|NotImplemented' src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp
rg -n 'GeomPlateCurveConstraintSource|readCurveConstraints|addCurveConstraint|unsupported_curve_criteria|curve_constraint_criteria' cad-core/include/cad_core/part/part_geomplate.h cad-core/src/part/part_geomplate.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不采 native expected。
- 不修改 `cad-core` parser。
- 不调整 capability 口径。
