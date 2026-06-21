# C5-M9-S3 face / all compound provenance 实现

状态：`pending`

## 目标

补齐 `ProjectOnSurface` face rebuild、hole wire、`Mode=All` compound 或 height solid 的 provenance。重点是 face / wire / compound child 的 ownership 与 ElementMap/reference recovery 证据，而不是重新实现投影几何。

## 必读

- S1/S2 完成后的 source / oracle / provenance 矩阵和 edge/wire history 实现
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::projectFace()`、`createFaceFromParametricWire()`、`filterShapes()`、`createCompound()`、`createSolidIfHeight()`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/topo/`
- `cad-core/tests/test_p8_features.py`

## 产物

- 新增或扩展 `c5m9/part-project-on-surface-face-rebuild-provenance` 与 `part-project-on-surface-all-compound-provenance`。
- 补 outer / inner wire source evidence、face rebuild ownership、Mode=All compound/solid child provenance 和 reference recovery hook。
- 保持 existing c4m1 face / all / height / offset fixtures expected-backed。
- 增加 focused tests 和 capability metadata 断言。
- 更新 local/root matrices，关闭 `C5M9-BLK-301`。

## 非目标

- 不实现 GUI、ViewProvider、TaskPanel。
- 不声明完整 `ProjectOnSurface` 或完整 Part surface family。
- 不把 face/all 输出按 child order 猜 source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
