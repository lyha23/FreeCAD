# C4-M1 ProjectOnSurface 独立主线方案

## 目标

把 `Part::ProjectOnSurface` 从 C3.0 surface family remaining boundary 中拆成独立 oracle-first 主线。先确认 FreeCAD 源码调用链、DTO 边界、native expected 和 diagnostic policy，再决定是否进入 cad-core executor。

## 范围

- FreeCAD 源码依据：`src/Mod/Part/App/FeatureProjectOnSurface.cpp/.h` 中 `ProjectOnSurface::execute/tryExecute/getSupportFace/getProjectionShapes/filterShapes/createProjectedWire/projectWire/projectFace/createSolidIfHeight/getOffsetPlacement`。
- stale path 结论：C4 初稿提到的 `PartFeatures.cpp` / `TopoShapeExpansion.cpp` 没有 ProjectOnSurface 实现命中，只是宽 surface family 草案残留。
- cad-core 落点：`cad-core/src/part/part_project_on_surface.cpp`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p8_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## C4-S1 first slice

已发布 expected-backed first slice：

- `Mode=Edges`
- `Height=0`
- `Offset=0`
- 单 `SupportFace`，且 subshape 必须是 face
- 单 `Projection`，且 subshape 必须是 edge / wire
- native expected：`cad-core/fixtures/c4m1/expected/part-project-on-surface-edge-plane.freecad.json`

明确 deferred：

- GUI Projection task panel
- `Mode=Faces` / `Mode=All`
- height / offset solid
- face rebuild / parametric wire face
- multi projection 和 advanced branches

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | live 基线、源码路径和 C3 boundary 复核 |
| S1 | native oracle / fixture 草案和 diagnostic policy，已完成 first slice |
| S2 | 后续只接收 expected-backed advanced branches，不再以 broad surface gap 推进 |

## 非目标

- 不迁移 GUI Projection task panel。
- 不从 cad-core 当前输出倒推 expected。
- 不把 ProjectOnSurface 挂回 conic 或 RuledSurface capability。
