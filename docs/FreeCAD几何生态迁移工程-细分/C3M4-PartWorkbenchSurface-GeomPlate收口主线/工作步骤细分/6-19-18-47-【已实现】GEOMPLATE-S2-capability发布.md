# 【已实现】GEOMPLATE-S2 capability 发布

更新 CADCore3.0 docs、oracle 队列、C API capability metadata 和 adapter tests。发布口径必须写清 GeomPlate 是 geometry backend helper，不是 GUI 或 fake DocumentObject。

## 发布内容

- `cad_core_capabilities_json()` 新增 `part_workbench.geomplate`，发布 `Part::GeomPlateSurface` / `Part.GeomPlate.BuildPlateSurface` / `PartGeomPlateSurfaceDTO` source-backed geometry helper first batch。
- capability 覆盖 S1 的 3D G0 curve constraints、3D point constraints、build params、approximation metadata、source evidence、`GeomPlate_MakeApprox` face、expected-backed fixtures 和 invalid diagnostics。
- adapter tests 固定 `part_workbench.geomplate` 的 type id、helper、properties、fixtures、diagnostics、request-local boundaries、remaining gaps 和 non-goals。
- CADCore3.0 docs、接口样例和 P8 Part surface 文档已把 c3m4 expected 基线更新为 19，并把 GeomPlate 与 Filling 分开发布。

## 边界

- GeomPlate 是 cad-core geometry backend helper，不是 GUI feature，也不是 native/fake FreeCAD `Part::GeomPlate` DocumentObject。
- 不扩大 `part_workbench.filling`；Filling 仍只表示 `Part.makeFilledFace()` / `BRepOffsetAPI_MakeFilling` helper。
- initial surface、G1 curve-on-surface、projected 2D curve、custom criteria、`Part.PlateSurface(Curves=...)` 和 full Part surface family 仍是 gap / non-goal。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest -k geomplate
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest -k geomplate
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py '/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-GeomPlate收口主线/工作步骤细分' --format markdown
git diff --check
```
