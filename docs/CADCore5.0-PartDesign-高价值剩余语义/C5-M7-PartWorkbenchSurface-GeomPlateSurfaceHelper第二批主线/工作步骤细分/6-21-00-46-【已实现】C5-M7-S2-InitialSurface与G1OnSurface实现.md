# 【已实现】C5-M7 S2 InitialSurface 与 G1 OnSurface 实现

状态：`【已实现】`

## 收口结论

- `PartGeomPlateSurfaceDTO` 已支持 `InitialSurface` / `Surface` source reference：通过 `App::PropertyLinkSub` 解析真实 target/subname，executor 在 `GeomPlate_BuildPlateSurface` 创建后调用 `LoadInitSurface`，并输出 `initial_surface` source evidence。
- `cad-core/fixtures/c5m7/part-geomplate-initial-surface-g0.json` 已采集 FreeCAD expected：`expected/part-geomplate-initial-surface-g0.freecad.json`。
- G1 curve-on-surface 已按 `Tools.cpp::Part::Tools::makeSurface()` 的 `Adaptor3d_CurveOnSurface -> GeomPlate_CurveConstraint(..., 1 /*GeomAbs_G1*/, ...)` 语义落到 cad-core：curve constraint item 可携带 per-curve `Surface` link，executor 从 edge + face 的 pcurve 构造 curve-on-surface，并输出 `curve_on_surface` source evidence。
- `cad-core/fixtures/c5m7/part-geomplate-g1-curve-on-surface.json` 已加入；其 expected 文件保留 `known_gap`，因为 FreeCADCmd Python 侧 `Part.GeomPlate.CurveConstraint.setCurve2dOnSurf(...)` probe 会终止原生进程，无法稳定采集 `Adaptor3d_CurveOnSurface` native oracle。该 blocker 不使用 cad-core bbox/topology 反推。
- `part_workbench.geomplate` capability 已同步为 InitialSurface expected-backed、G1 source-backed with native oracle blocker；S3 继续负责 `Curve2dOnSurface` / `ProjectedCurve2d` / `Point2dOnSurface`。

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
