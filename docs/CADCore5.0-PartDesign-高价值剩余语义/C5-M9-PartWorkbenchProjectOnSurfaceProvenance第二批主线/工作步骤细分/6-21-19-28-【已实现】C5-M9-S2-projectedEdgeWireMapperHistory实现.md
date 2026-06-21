# 【已实现】C5-M9-S2 projected edge / wire MapperHistory 实现

状态：`done_C5M9-S2_edge_wire_mapper_history`

## live baseline

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`c45697eba3`
- `git log -1 --oneline`：`c45697eba3 docs: 冻结C5-M9-S1来源矩阵`
- `git -c core.quotepath=false status --short -uall`：无输出，S2 起点工作区干净。

## 目标

在 `cad-core` 中补齐 `ProjectOnSurface` edge / wire 投影结果的 provenance 与 mapper/history 传播。输出 edge 不只说明 `source_projection`，还要能定位 source object/subname、Projection item index、wire fragment ownership 与 ElementMap/reference recovery 证据。

## 必读

- S1 完成后的 source / oracle / provenance 矩阵
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::getProjectionShapes()`、`createProjectedWire()`、`projectWire()`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`

## 产物

- 已新增 `cad-core/fixtures/c5m9/part-project-on-surface-edge-provenance.json`、`part-project-on-surface-wire-split-provenance.json`、`part-project-on-surface-invalid-provenance-diagnostics.json`。
- 已新增三份 `cad-core/fixtures/c5m9/expected/*.freecad.json` source-backed known_gap，记录 native collector 暂不能导出 ProjectOnSurface per-edge / per-wire MapperHistory、ElementMap 与 cad-core diagnostic schema；删除条件是 FreeCADCmd collector 或专用 probe 能导出 native mapper/history。
- `cad-core/src/part/part_project_on_surface.cpp` 现在保留 `projection_item_ledger`，并把 edge/wire `projectWire` 输出写入 `projected_edge_wire_history` 与 `NamedShape.mapper_history`，字段包括 source object/subname、stable subname、0-based Projection item index、projected wire index、edge fragment index、mapper_history_id、target endpoint、`element_map_target` 和 `reference_recovery_hook`。
- `cad-core/src/part/topo_shape_mapper.cpp` / `include/cad_core/part/topo_shape_mapper.h` 增加 ProjectOnSurface mapper event helper；命名传播仍在 part/topo 层，不在 adapter 后处理。
- `cad-core/tools/collect_freecad_expected.py` 只补 collector 可见的 `projection_item_ledger`，不从 cad-core 输出倒推 native mapper/history。
- `cad-core/tests/test_p8_features.py` 覆盖 edge provenance、wire fragment ownership 和 invalid diagnostics property/target/subname；`cad-core/tests/test_adapters.py` 覆盖 S2 capability metadata。
- 已更新本包矩阵与 root C5 矩阵，关闭 `C5M9-BLK-201`；`C5M9-BLK-301` / `C5M9-BLK-401` 和 root `C5-BLK-901` 仍保持 pending。

## 非目标

- 不处理 face rebuild / Mode=All compound，那是 S3。
- 不改 GUI。
- 不用 output-order / bbox / fixture-name 判断来源。

## 已保留边界

- S2 不直接把 source stable subname 写入 `element_map`，避免把第一批 C4M1 indexed-only expected 一起改写；本轮先以 `MapperHistoryEvent.evidence.element_map_target` 与 `reference_recovery_hook=mapper_history_event_target_subname` 暴露 reference recovery hook。
- face rebuild、hole wire、Mode=All compound/solid child map 和完整 ElementMap/reference recovery 由 S3/S4 继续处理。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
```
