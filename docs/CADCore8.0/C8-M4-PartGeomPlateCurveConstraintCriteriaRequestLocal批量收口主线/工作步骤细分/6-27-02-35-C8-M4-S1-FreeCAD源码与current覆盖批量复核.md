# C8-M4-S1 FreeCAD 源码与 current 覆盖批量复核

## 目标

复核 CurveConstraint criteria 的 FreeCAD source authority、当前 `cad-core` DTO / parser / OCCT apply path、existing tests 和 capability publication。S1 只做证据复核和矩阵回写，不改 C++。

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
- S1 结论必须明确：FreeCAD native setter boundary 与 `cad-core` request-local boundary 是否分离。
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
