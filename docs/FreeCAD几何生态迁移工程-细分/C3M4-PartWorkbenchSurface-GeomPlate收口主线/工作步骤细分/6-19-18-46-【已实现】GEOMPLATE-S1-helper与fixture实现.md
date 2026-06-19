# 【已实现】GEOMPLATE-S1 helper 与 fixture 实现

新增 GeomPlate helper / DTO / collector / fixtures / tests。第一批覆盖 curve constraints、point constraints、perform success、perform failure diagnostics 和 surface metadata。

## 完成内容

- 新增 cad-core `Part::GeomPlateSurface` helper request type，注册 executor，并通过 `PartGeomPlateSurfaceDTO` metadata 表达 build params、curve/point constraint counts、source evidence、surface kind、G0/G1/G2 errors 和 approximation status。
- 新增低层 GeomPlate helper，使用 `GeomPlate_BuildPlateSurface`、`GeomPlate_CurveConstraint`、`GeomPlate_PointConstraint` 和 `GeomPlate_MakeApprox`；不复用 Filling / `BRepOffsetAPI_MakeFilling`。
- 新增 `collect_freecad_expected.py` 的 GeomPlate helper route：用 `Part.GeomPlate.BuildPlateSurface()`、`CurveConstraint`、`PointConstraint`、`perform()`、`surface().makeApprox().toShape()` 采集 expected。
- 新增 fixtures：
  - `cad-core/fixtures/c3m4/part-geomplate-curve-point-default.json`
  - `cad-core/fixtures/c3m4/part-geomplate-invalid-inputs.json`
  - 对应 checked-in `expected/*.freecad.json`
- 补充 `tests/test_p8_features.py` focused assertions，覆盖 success metadata/source evidence 和 diagnostics group。

## 边界

- `Part::GeomPlateSurface` 是 cad-core source-backed helper，不是 FreeCAD native `Part::GeomPlate` DocumentObject。
- G1 curve-on-surface、initial surface、projected 2D curve、`Part.PlateSurface(Curves=...)` 仍未发布支持。
- S1 不发布 capability；C API capability metadata、CADCore3.0 文档和 adapter tests 留给 S2。

## 验证

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest -k geomplate
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
```

补充队列验收：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py '/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-GeomPlate收口主线/工作步骤细分' --format markdown
```
