# C5-M9-S2 projected edge / wire MapperHistory 实现

状态：`pending`

## 目标

在 `cad-core` 中补齐 `ProjectOnSurface` edge / wire 投影结果的 provenance 与 mapper/history 传播。输出 edge 不只说明 `source_projection`，还要能定位 source object/subname、Projection item index、wire fragment ownership 与 ElementMap/reference recovery 证据。

## 必读

- S1 完成后的 source / oracle / provenance 矩阵
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::getProjectionShapes()`、`createProjectedWire()`、`projectWire()`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/topo/`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`

## 产物

- 新增或扩展 `c5m9/part-project-on-surface-edge-provenance`、`part-project-on-surface-wire-split-provenance`、`part-project-on-surface-invalid-provenance-diagnostics`。
- 在 `part_project_on_surface.cpp` / topo 层补 source item map 与 projected edge/wire history，不把命名传播塞进 adapter。
- 对 expected 可采 case 写入 FreeCAD expected；不可采字段只写 source-backed known_gap 与 delete_condition。
- 增加 focused tests，覆盖 evidence 字段、invalid provenance diagnostics 和 adapter capability metadata。
- 更新 local/root matrices，关闭 `C5M9-BLK-201`。

## 非目标

- 不处理 face rebuild / Mode=All compound，那是 S3。
- 不改 GUI。
- 不用 output-order / bbox / fixture-name 判断来源。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
