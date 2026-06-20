# C5-M7 S4 Criteria / Wrapper 与 diagnostic 边界

## 目标

把 custom criteria 与 `Part.PlateSurface.Curves` wrapper 从 broad gap 改成 supported 或 concrete deferred boundary。

## FreeCAD 依据

- `src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()`。
- `src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp` criteria accessors。
- `src/Mod/Part/App/PlateSurfacePyImp.cpp` 与 `src/Mod/Part/App/Geometry.cpp` 的 PlateSurface lifecycle。

## 工作

1. 判断 criteria 是否能作为同一 DTO 字段支持，并采集 `part-geomplate-custom-criteria` expected 或输出 concrete diagnostic。
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
