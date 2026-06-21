# 【已实现】C5-M9-S1 source / oracle / provenance 矩阵

状态：`done_C5M9-S1_source_oracle_provenance`

## live baseline

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`df0b5c55b1`
- `git log -1 --oneline`：`df0b5c55b1 docs: 冻结C5-M9-S0 live基线`
- `git -c core.quotepath=false status --short -uall`：无输出，S1 起点工作区干净。

## 目标

读 FreeCAD source 与 cad-core 当前实现，冻结 projected result provenance 字段、native oracle 可采路径、source-backed known_gap 删除条件和 fixture matrix。S1 先把账本说清楚，再允许 S2/S3 写实现。

## 必读

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.h`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/include/cad_core/part/topo_shape_mapper.h`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- 已写清 `getProjectionShapes()` 的 object/subname/item index 如何传到 `createProjectedWire()`、`projectWire()`、`projectFace()`、`filterShapes()`、`createCompound()`。
- 已定义 C5-M9 provenance evidence：source object、source subname、Projection item index、Mode 分支、edge/wire fragment、face wire、compound child、mapper/history id、ElementMap/reference recovery hook。
- 已盘点 native collector 能采字段；不能采的字段记录 source-backed known_gap、delete_condition 和临时 expected 策略。
- 已更新本包 fixture/oracle matrix、scope matrix、blocker queue 和 root C5-M9 landing path；已关闭 `C5M9-BLK-101`，未关闭 S2/S3/S4 blocker。

## FreeCAD 调用链冻结

| 阶段 | FreeCAD 依据 | C5-M9 合同 |
| --- | --- | --- |
| `getProjectionShapes()` | `Projection.getValues()` 和 `Projection.getSubValues()` 按同一 index 读取；数量不一致时报错；每项用 `Feature::getTopoShape(... NeedSubElement | ResolveLink | Transform, subvalue)` 解析。 | cad-core 不能只保存 `TopoDS_Shape`；必须同时保存 `source_object`、`source_subname`、`stable_subname`、0-based `projection_item_index`、`source_shape_kind`。 |
| `createProjectedWire()` | Face 走 `projectFace()` -> `createFaceFromWire()` -> `createSolidIfHeight()`；Edge/Wire 走 `projectWire()`。 | 每个 projected result 都继承 projection item id，并记录 `maker_stage=project_wire|project_face_wire|face_rebuild|height_solid`。 |
| `projectWire()` | `BRepProj_Projection` 取 nearest projected wire，再遍历 projected wire 的 edges 输出。 | S2 为 projected edge/wire 建 `edge_fragment_index`、`projected_wire_index`、source endpoint -> target endpoint `MapperHistoryEvent`。 |
| `projectFace()` | `getWires()` 先 outer wire、再 inner wires；每个 wire 投影并 `fixWire()`。 | S3 保留 `face_wire_index`、`face_wire_role=outer|inner`，face rebuild 不能丢失 wire source evidence。 |
| `filterShapes()` | `All` 保留非空；`Faces` 只保留 face；`Edges` 保留 edge/wire，并把 face 展开为 wires。 | filter 后记录 `filter_output_index`、`filter_mode`、`pre_filter_result_id`；face->wire 记录原 face wire role。 |
| `createCompound()` | 按 filtered order 加 compound child；Offset 非 0 时对子 shape 调 `Moved(loc)`。 | S3 为 compound child 记录 `compound_child_index`、pre-offset child id、offset applied，并接入 child ElementMap/reference recovery。 |

## provenance evidence 字段

最小字段冻结在 `矩阵/c5m9_project_on_surface_provenance_evidence_matrix.tsv`：

- projection source：`source_object`、`source_subname`、`stable_subname`、`projection_item_index`、`source_shape_kind`。
- branch/result：`mode`、`maker_stage`、`projected_result_id`、`projected_wire_index`、`edge_fragment_index`。
- face/all：`face_wire_index`、`face_wire_role`、`face_rebuild_id`、`height_solid_id`、`compound_child_index`、`pre_offset_child_id`、`offset_applied`。
- topo naming：`mapper_history_id` 或等价 event key、source endpoint、target endpoint、relation、recoverability、`element_map_target`、`reference_recovery_hook`。

## native oracle 可采与 known_gap

当前 `collect_freecad_expected.py::project_on_surface_payload()` 可以采：

- fixture/native 属性：`source_support`、`support_face`、`source_projection`、`projection_subshape`、`projection_items`、`mode`、`height`、`offset`、`offset_vector`。
- native shape summary：bbox、volume、topology counts、projected solid/face/wire/inner-wire counts。
- cad-core diagnostic-backed live guard：missing/invalid `SupportFace` / `Projection` 的 property 和 target。

当前不能采：

- native per-edge/per-wire fragment owner；
- native face outer/inner wire owner 与 face rebuild source set；
- native compound child -> source child map；
- native `MapperHistory` id、Generated/Modified relation、ElementMap history 和 reference recovery result。

source-backed known_gap 删除条件：

- `C5M9-KG-201`：S2 edge/wire fixtures 输出 `NamedShape.mapper_history` source->target events，且 focused tests 证明字段来自 projection item ledger；若 FreeCADCmd collector/probe 能导出 native mapper history，应改为 native expected-backed。
- `C5M9-KG-301..305`：S3 face/all fixtures 输出 face wire、face rebuild、height solid、filter、compound child evidence，且 ElementMap/reference recovery hook 可测试；若 native child map/history 可采，应替换 source-backed known_gap。
- `C5M9-KG-401/402`：ProjectOnSurface 的 `NamedShape.mapper_history` 与 `resolveElementReference()` 覆盖 projected subnames 后删除。

临时 expected 策略：native 可见字段可以写入 `.freecad.json`；native 不可见的 provenance 字段不得从 cad-core 输出倒推 expected，只能在矩阵或 fixture notes 中声明 source-backed known_gap 和 delete_condition。

## 非目标

- 不写 executor 主路径。
- 不用 cad-core 输出倒推 expected。
- 不按 bbox、输出顺序、fixture 名或几何相似性设计 provenance。
- 不关闭 S2/S3/S4 blocker。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
