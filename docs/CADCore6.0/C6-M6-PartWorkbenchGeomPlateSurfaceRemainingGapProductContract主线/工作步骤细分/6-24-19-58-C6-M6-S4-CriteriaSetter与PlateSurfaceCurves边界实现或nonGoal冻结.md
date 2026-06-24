# C6-M6-S4 CriteriaSetter 与 PlateSurfaceCurves 边界实现或 nonGoal 冻结

## 目标

批量处理 FreeCAD wrapper 侧证据最强的两个边界：`curve_constraint_criteria_setters_not_implemented` 与 `platesurface_curves_wrapper_lifecycle`。S4 必须先确认 FreeCAD wrapper 当前行为；若仍是 `NotImplementedError` / SIGSEGV / persistent wrapper 生命周期需求，则冻结为 diagnostic / non-goal，不把它们写成 supported。

## 必读

- S0/S1/S2 矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/fixtures/c5m7/part-geomplate-curve-criteria-diagnostic.json`
- `cad-core/fixtures/c5m7/part-geomplate-wrapper-boundary.json`

## 产物

- 如 criteria setter 仍未实现：保留 `unsupported_curve_criteria` 诊断 fixture 和 capability evidence。
- 如 `Part.PlateSurface.Curves` 仍需要 persistent wrapper 或仍会崩溃：写入 non-goal registry 和 diagnostic boundary。
- 只有当 S2/S4 证明有 request-local safe contract 时，才允许新增 C6-M6 fixtures 和 tests。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_non_goal_registry.tsv`、input contract、blocker queue 和 validation matrix。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features -k geomplate
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
```

验收通过后，将本文重命名为 `6-24-19-58-【已实现】C6-M6-S4-CriteriaSetter与PlateSurfaceCurves边界实现或nonGoal冻结.md`。

## 非目标

- 不修改上游 FreeCAD source。
- 不创建 fake persistent `PlateSurface` object。
- 不把 wrapper lifecycle 放入前后端长期状态。
