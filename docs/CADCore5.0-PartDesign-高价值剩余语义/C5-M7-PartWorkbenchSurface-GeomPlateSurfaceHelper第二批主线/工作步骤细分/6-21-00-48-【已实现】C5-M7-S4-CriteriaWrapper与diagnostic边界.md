# 【已实现】C5-M7 S4 Criteria / Wrapper 与 diagnostic 边界

状态：`【已实现】`

## 收口结论

- Point criteria 已按 FreeCAD `PointConstraintPyImp.cpp::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()` 支持：`PartGeomPlateSurfaceDTO` 在 point constraint source 中携带 `G0Criterion` / `G1Criterion` / `G2Criterion`，executor 调用 `GeomPlate_PointConstraint::SetG*Criterion()`，并在 source evidence 中输出 criteria 值。
- `cad-core/fixtures/c5m7/part-geomplate-point-custom-criteria.json` 已由 FreeCADCmd collector 采集 expected：`expected/part-geomplate-point-custom-criteria.freecad.json`。代表 case 使用有效 point order=0；FreeCAD/OCCT 会拒绝 order=2 point constraint。
- Curve criteria 不支持声明：当前 FreeCAD `CurveConstraintPyImp.cpp::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()` 仍直接抛 `PyExc_NotImplementedError("Not yet implemented")`。cad-core 对 curve constraint payload 中的 criteria 字段输出 `unsupported_curve_criteria` locatable diagnostic，fixture 为 `part-geomplate-curve-criteria-diagnostic`。
- `Part.PlateSurface.Curves` 不支持声明：`PlateSurfacePyImp.cpp::PlateSurfacePy::PyInit()` 虽解析 `Curves`，但分支仍是 `TODO`；`Geometry.cpp::GeomPlateSurface::Save()` / `Restore()` 仍是 `NotImplementedError`。cad-core 输出 `unsupported_wrapper_lifecycle` locatable diagnostic，fixture 为 `part-geomplate-wrapper-boundary`，不伪造 persistent PlateSurface object。
- `part_workbench.geomplate` capability 已从 broad custom/wrapper gap 改成 point criteria expected-backed、curve criteria setter diagnostic、PlateSurface.Curves wrapper lifecycle diagnostic；`c3m4/part-geomplate-invalid-inputs` 与 `c4m1/part-geomplate-advanced-deferred` 也同步使用 `unsupported_wrapper_lifecycle`。

## 目标

把 custom criteria 与 `Part.PlateSurface.Curves` wrapper 从 broad gap 改成 supported 或 concrete deferred boundary。

## FreeCAD 依据

- `src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()`。
- `src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp` criteria accessors。
- `src/Mod/Part/App/PlateSurfacePyImp.cpp` 与 `src/Mod/Part/App/Geometry.cpp` 的 PlateSurface lifecycle。

## 工作

1. 判断 criteria 是否能作为同一 DTO 字段支持，并采集 point criteria expected 或输出 concrete diagnostic。
2. 判断 `Part.PlateSurface.Curves` 是否属于同一 request-local DTO；若需要 persistent wrapper lifecycle，则保留 `part-geomplate-wrapper-boundary` diagnostic fixture。
3. adapter capability 中删除 broad `advanced constraints` 表述，保留 precise supported / remaining / non-goal。
4. 更新本包 non-goal registry。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 非目标

- 不伪造 persistent PlateSurface object。
- 不把 wrapper support 写成 supported，除非同一 DTO 和 expected 已证明。
