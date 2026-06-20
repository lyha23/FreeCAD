# C5-M7 S3 Projected 2D 与 Point2D 实现

## 目标

补齐同一 `BuildPlateSurface` helper 下的 2D constraint 表达：curve2d-on-surface、projected 2D curve 和 point2d-on-surface。

## FreeCAD 依据

- `src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setCurve2dOnSurf()`：调用 `SetCurve2dOnSurf(curve2)`。
- `src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setProjectedCurve()`：调用 `SetProjectedCurve(hCurve, tolU, tolV)`。
- `src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setPnt2dOnSurf()`：调用 `SetPnt2dOnSurf(gp_Pnt2d(x, y))`。

## 工作

1. 扩展 DTO，表达 2D curve payload、projected tolerance、2D point 和关联 surface。
2. 新增 expected-backed fixtures：
   - `cad-core/fixtures/c5m7/part-geomplate-curve2d-on-surface.json`
   - `cad-core/fixtures/c5m7/part-geomplate-projected-curve2d.json`
   - `cad-core/fixtures/c5m7/part-geomplate-point2d-on-surface.json`
   - `cad-core/fixtures/c5m7/part-geomplate-mixed-surface-constraints.json`
3. 补 malformed 2D payload diagnostics，要求 object/property/target/subname 可定位。
4. 更新 expected collector、tests、capability。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 非目标

- 不用结果 shape 猜测 2D curve 所在 surface。
- 不实现 Filling 或 ProjectOnSurface。
