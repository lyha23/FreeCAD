# C5-M9 Part Workbench ProjectOnSurface Provenance 第二批方案

## 当前基线

`part_workbench.project_on_surface` 已发布 C4M1 live guard：`Mode=Edges/Faces/All`、face rebuild / hole wires、`Mode=All` Height solid、Offset placement、多 `Projection` ordered `App::PropertyLinkSubList`、普通 indexed `NamedShape` 由 11 个 expected-backed geometry fixtures 覆盖；`part-project-on-surface-deferred-boundaries` 是 diagnostic-backed deferred guard。

当前已发布的第二批能力不是新增投影几何，而是投影结果的 provenance：输出 edge / wire / face / compound 能说明来自哪个 `Projection` object/subname、LinkSubList item、Mode 分支、wire fragment 或 face wire，并能进入 MapperHistory / ElementMap / reference recovery 账本。S4 收口后，capability JSON 的 `remaining_gaps` 只保留 `gui_projection_task_panel` 与 `unverified_advanced_branches`；native mapper/history hidden-until-probe 是 request-local/source-backed 边界，不再作为 broad provenance gap。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` 读取 `SupportFace`、`Projection`、`Direction`，逐个 source shape 调 `createProjectedWire()`，最后 `filterShapes()` 与 `createCompound()` 写入 `Shape`。
- `getProjectionShapes()` 按 `Projection.getValues()` 与 subname 列表返回 ordered projection shapes；C5-M9 必须保留这一层的 object/subname/item index，不允许后续按几何结果猜 source。
- `createProjectedWire()` 对 face 走 `projectFace()`，对 edge / wire 走 `projectWire()`；`projectFace()` 先投影 face wires，再 `createFaceFromParametricWire()` / `createSolidIfHeight()`；`projectWire()` 通过 `BRepProj_Projection` 取 nearest wire，再重建 projected edges。
- `filterShapes()` 按 `Mode=All/Faces/Edges` 过滤输出；`createCompound()` 决定多结果组合边界。C5-M9 需要让过滤前后的 ownership 仍可追溯。
- `TopoShapeMapper*` / `PropertyTopoShape*` 是后续 mapper/history 与 ElementMap 证据入口；如果 FreeCAD native collector 暂不能暴露完整历史，必须留下 source-backed known_gap 和删除条件。

## S1 source / oracle / provenance 合同

`getProjectionShapes()` 在 FreeCAD 中只返回 `std::vector<TopoDS_Shape>`，但它是在 `Projection.getValues()` 与 `Projection.getSubValues()` 的同一 index 上解析对象和 subname；C5-M9 后续实现必须在 cad-core 中把这层信息保留成 request-local projection item ledger。最小字段为：`source_object`、`source_subname`、`stable_subname`、`projection_item_index`、`source_shape_kind`、`mode`、`maker_stage`。`projection_item_index` 采用 0-based LinkSubList 顺序，和 collector 当前 `projection_items` 数组位置一致。

调用链传递规则：

- `resolveProjectionShapes()` / `getProjectionShapes()`：生成 projection item ledger，不允许只保存 shape；item 必须带 source object/subname、stable subname、item index 和 source shape kind。
- `createProjectedShapes()` / `createProjectedWire()`：每个输出 shape 必须继承 projection item id，并记录 `project_wire`、`project_face_wire`、`face_rebuild` 或 `height_solid` 分支。
- `projectWire()` / `projectWireEdges()`：每个 projected edge 输出记录 `projected_wire_index`、`edge_fragment_index`、source edge/wire subname、projection item index；若一条 source wire 产生多个 edge fragment，S2 必须用 fragment ledger 或 source-backed known_gap 表达，不得按 bbox 或输出顺序猜 owner。
- `projectFace()` / `projectFaceWires()`：外 wire 与 inner wire 记录 `face_wire_index`、`face_wire_role=outer|inner`、projection item index；face rebuild 结果继续持有 wire list evidence。
- `filterShapes()` / `filterProjectedShapes()`：`Mode=All/Faces/Edges` 过滤后保留 pre-filter result id；`Mode=Edges` 把 face 拆成 wires 时要记录 `filter_stage=face_to_wire` 和 face wire index。
- `createCompound()` / `compoundOf()`：compound child 记录 `compound_child_index`、pre-offset child id、offset-applied 状态；这是 ElementMap child-map/reference recovery 的入口，不能只在 metadata 里保留 aggregate counts。

provenance evidence 字段冻结在 `矩阵/c5m9_project_on_surface_provenance_evidence_matrix.tsv`。S2/S3 输出的 `NamedShape.mapper_history` event 必须用 `mapper_history_id` 或等价稳定 event key 连接 source endpoint 与 target endpoint；`ElementMap/reference recovery hook` 必须落在 `cad-core/src/part/topo_shape.cpp`、`cad-core/src/part/topo_shape_mapper.cpp`、`cad-core/include/cad_core/part/topo_shape_mapper.h` 的现有 NamedShape / MapperHistory 出口，不得作为 adapter 后处理。

## Native oracle 与 known_gap

当前 `cad-core/tools/collect_freecad_expected.py::project_on_surface_payload()` 能从 fixture/native shape 采集：`source_support`、`support_face`、`source_projection`、`projection_subshape`、`projection_items`、`mode`、`height`、`offset`、`offset_vector`、projected solid/face/wire/inner-wire counts、bbox、volume 和 topology counts。它不能采集 native ProjectOnSurface 的 per-edge fragment owner、face wire owner、compound child -> source child map、MapperHistory id、ElementMap map/history 或 reference recovery 结果。

临时 expected 策略：native 可见字段继续写入 `.freecad.json`；native 不可见的 provenance 字段不得从 cad-core 输出倒推 expected，只能在本包矩阵中记录 source-backed known_gap、delete_condition 和 S2/S3 的 semantic test owner。S2/S3 已根据 FreeCAD source ledger 实现 `NamedShape.mapper_history`，并有 fixture/focused test 证明字段来自 projection item ledger，而非 bbox、输出顺序、fixture 名或几何相似性；删除 native-hidden 边界的条件仍是 FreeCADCmd collector 或专用 probe 能导出对应 native ElementMap/MapperHistory。

## cad-core 落点

- `cad-core/src/part/part_project_on_surface.cpp`：补 projected result provenance DTO、source item map、edge/wire/face/all evidence，删除 broad `projected_edge_provenance_mapper_history` gap 的无证据部分。
- `cad-core/src/part/topo_shape.cpp`、`cad-core/src/part/topo_shape_mapper.cpp` 与 `cad-core/include/cad_core/part/topo_shape_mapper.h`：承接 MapperHistory / ElementMap / reference recovery hook；不得把命名传播写成 executor 输出修剪。
- `cad-core/tools/collect_freecad_expected.py` 与 `cad-core/fixtures/c5m9`：批量采集或记录 native oracle known_gap，expected 不得从 cad-core 输出倒推。
- `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`、`tests/test_adapters.py`：覆盖 provenance evidence、diagnostic target/subname、capability metadata 和 first-batch guard。
- `cad-core/src/adapters/c_api/c_api.cpp`：只同步 capability/schema/diagnostics，不承接 ProjectOnSurface 业务逻辑。

## 代表 fixtures

| 分组 | 目标 fixture | 验收重点 |
| --- | --- | --- |
| live guard | `c4m1/part-project-on-surface-*` 12 个现有 fixtures | C4M1 11 个 expected-backed geometry fixtures、metadata 和 1 个 diagnostic-backed deferred fixture 不回退 |
| edge / wire provenance | `c5m9/part-project-on-surface-edge-provenance`、`c5m9/part-project-on-surface-wire-split-provenance`、`c5m9/part-project-on-surface-invalid-provenance-diagnostics` | source object/subname、Projection item index、wire fragment ownership、MapperHistory/ElementMap evidence、locatable diagnostics |
| face / all provenance | `c5m9/part-project-on-surface-face-rebuild-provenance`、`c5m9/part-project-on-surface-all-compound-provenance` | outer/inner wire source evidence、face rebuild ownership、Mode=All compound/solid provenance、reference recovery hook |

## 实施顺序

1. S0：冻结 live baseline、root matrix、现有 C4M1 fixtures 和 capability 状态。
2. S1：读 FreeCAD source 与 cad-core 当前实现，写清 projected provenance 字段、oracle 可采路径、known_gap 删除条件和 fixture matrix。
3. S2：实现 edge / wire projected provenance 与 mapper/history 传播；补 fixtures、expected 或 known_gap、focused tests。
4. S3：实现 face rebuild、hole wire、Mode=All compound/solid provenance；补 ElementMap / reference recovery evidence 和 tests。
5. S4：已同步 capability/docs/root matrices，关闭 `projected_edge_provenance_mapper_history` broad gap；native-hidden mapper/history 保留为 source-backed request-local boundary，队列清空后收口。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 收口标准

- first-batch `ProjectOnSurface` 11 个 expected-backed geometry fixtures 和 1 个 diagnostic-backed deferred guard 保持通过。
- projected edge / wire / face / compound provenance 有 native expected、source-backed known_gap 或 diagnostic-backed 证据。
- MapperHistory / ElementMap / reference recovery 边界明确；没有 bbox、输出顺序、fixture 名或 adapter 修剪特判。
- capability metadata 中 `projected_edge_provenance_mapper_history` 不再是 broad gap；剩余 `gui_projection_task_panel` / `unverified_advanced_branches` 有具体 non-goal / future-owner 边界，`native_project_on_surface_mapper_history_hidden_until_probe` 有 delete_condition。
- 本包 `工作步骤细分` 队列为空，全局 `C5-BLK-901` 已关闭。
