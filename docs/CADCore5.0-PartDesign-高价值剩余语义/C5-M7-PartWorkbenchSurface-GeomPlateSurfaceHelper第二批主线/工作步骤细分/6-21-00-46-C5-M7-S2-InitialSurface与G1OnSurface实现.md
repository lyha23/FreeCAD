# C5-M7 S2 InitialSurface 与 G1 OnSurface 实现

## 目标

在同一 `PartGeomPlateSurfaceDTO` 下支持 initial surface reference 与 G1 curve-on-surface，产出 expected-backed 代表场景。

## FreeCAD 依据

- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp::BuildPlateSurfacePy::PyInit()`：`Surface` 参数触发 `LoadInitSurface(handle)`。
- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp::loadInitSurface()`：显式 initial surface 入口。
- `src/Mod/Part/App/Tools.cpp::Part::Tools::makeSurface()`：`Adaptor3d_CurveOnSurface` 创建 `GeomPlate_CurveConstraint(..., 1 /*GeomAbs_G1*/, ...)`。

## 工作

1. 扩展 DTO / parser，表达 initial surface target/subname 和 curve-on-surface source。
2. 在 `part_geomplate.cpp` 中通过 source-backed surface/curve 构造 OCCT constraint，不从 cad-core 输出倒推。
3. 新增 fixtures：
   - `cad-core/fixtures/c5m7/part-geomplate-initial-surface-g0.json`
   - `cad-core/fixtures/c5m7/part-geomplate-g1-curve-on-surface.json`
4. 采集 FreeCAD expected，补 focused tests 和 adapter capability。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 非目标

- 不处理 2D curve / point；留给 S3。
- 不把 failed oracle case 改成 cad-core 特判。
