# 【已实现】C5-M7 S3 Projected 2D 与 Point2D 实现

状态：`【已实现】`

## 收口结论

- `PartGeomPlateSurfaceDTO` 已支持 `Curve2dOnSurface`、`ProjectedCurve2d` 和 `Point2dOnSurface` 请求表达；每条 2D curve 显式携带 `Boundary` 3D edge link、`Surface` face link 和 `Curve2d` payload，每条 2D point 显式携带 `Point`、`Point2d` 和 `Surface` link。
- `cad-core/fixtures/c5m7/part-geomplate-curve2d-on-surface.json`、`part-geomplate-point2d-on-surface.json`、`part-geomplate-mixed-surface-constraints.json` 已采集 FreeCAD expected；mixed fixture 覆盖 G0 curve + curve2d + point2d 组合。
- `cad-core/fixtures/c5m7/part-geomplate-projected-curve2d.json` 已实现 cad-core source-backed executor 和 source evidence，但 FreeCADCmd Python wrapper 调用 `setProjectedCurve(...)` 会终止原生进程，因此 expected 文件保留 `geomplate_projected_curve2d_native_oracle_blocked` known_gap；删除条件是稳定 native oracle 可调用 `Part.GeomPlate.CurveConstraint.setProjectedCurve(...)`。
- S2 的 G1 curve-on-surface native oracle blocker 不在本步解决；需要 G1 curve-on-surface 的 mixed expected 仍不能伪装为 expected-backed。
- 旧 `c4m1/part-geomplate-advanced-deferred` 中缺少显式 Boundary/Surface/Curve2d payload 的 2D 项现在给出 `invalid_curve2d_source` / `invalid_point2d_source` locatable diagnostics，`PlateSurfaceCurves` 继续留给 S4。

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
