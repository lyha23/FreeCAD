# 【已实现】C5-M9-S3 face / all compound provenance 实现

状态：`done_C5M9-S3_face_all_compound_provenance`

## 目标

补齐 `ProjectOnSurface` face rebuild、hole wire、`Mode=All` compound 或 height solid 的 provenance。重点是 face / wire / compound child 的 ownership 与 ElementMap/reference recovery 证据，而不是重新实现投影几何。

## 必读

- S1/S2 完成后的 source / oracle / provenance 矩阵和 edge/wire history 实现
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::projectFace()`、`createFaceFromParametricWire()`、`filterShapes()`、`createCompound()`、`createSolidIfHeight()`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tests/test_p8_features.py`

## 产物

- 已新增 `cad-core/fixtures/c5m9/part-project-on-surface-face-rebuild-provenance.json` 与 `part-project-on-surface-all-compound-provenance.json`。
- 已新增两份 source-backed known_gap expected，记录 native collector 暂不能导出 ProjectOnSurface face/all child maps、MapperHistory 或 reference recovery；删除条件是 FreeCADCmd collector 或专用 probe 能导出对应 native history。
- `cad-core/src/part/part_project_on_surface.cpp` 已在 S2 projection item ledger 基础上补 outer / inner wire source evidence、face rebuild ownership、Mode=All height solid provenance、compound child index、pre-offset child id、offset-applied evidence、`element_map_target` 与 `reference_recovery_hook`。
- `NamedShape.mapper_history` 现在区分 `project_on_surface_edge_wire_provenance` 与 `project_on_surface_face_all_compound_provenance`，face/all 事件来源于 projection item ledger 与 FreeCAD `projectFace()` / `createFaceFromParametricWire()` / `createSolidIfHeight()` / `createCompound()` 调用链，不使用 bbox、fixture name、几何相似性或 adapter 后处理。
- 已保持 C4M1 face / all / height / offset fixtures expected-backed，并新增 focused tests 与 capability metadata 断言。
- 已更新 local/root matrices，关闭 `C5M9-BLK-301`；`C5M9-BLK-401` 与 root `C5-BLK-901` 仍保持 pending，留给 S4 capability/docs 收口。

## 验收结果

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

结果：build 通过；`Ran 211 tests in 77.350s`，`OK (skipped=25)`。

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
