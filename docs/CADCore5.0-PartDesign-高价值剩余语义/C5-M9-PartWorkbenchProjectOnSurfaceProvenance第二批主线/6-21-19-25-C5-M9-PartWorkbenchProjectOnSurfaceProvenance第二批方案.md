# C5-M9 Part Workbench ProjectOnSurface Provenance 第二批方案

## 当前基线

`part_workbench.project_on_surface` 已发布 C4M1 live guard：`Mode=Edges/Faces/All`、face rebuild / hole wires、`Mode=All` Height solid、Offset placement、多 `Projection` ordered `App::PropertyLinkSubList`、普通 indexed `NamedShape` 由 11 个 expected-backed geometry fixtures 覆盖；`part-project-on-surface-deferred-boundaries` 是 diagnostic-backed deferred guard。

当前缺口不是投影几何，而是投影结果的 provenance：输出 edge / wire / face / compound 能否说明来自哪个 `Projection` object/subname、LinkSubList item、Mode 分支、wire fragment 或 face wire，并能进入 MapperHistory / ElementMap / reference recovery 账本。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` 读取 `SupportFace`、`Projection`、`Direction`，逐个 source shape 调 `createProjectedWire()`，最后 `filterShapes()` 与 `createCompound()` 写入 `Shape`。
- `getProjectionShapes()` 按 `Projection.getValues()` 与 subname 列表返回 ordered projection shapes；C5-M9 必须保留这一层的 object/subname/item index，不允许后续按几何结果猜 source。
- `createProjectedWire()` 对 face 走 `projectFace()`，对 edge / wire 走 `projectWire()`；`projectFace()` 先投影 face wires，再 `createFaceFromParametricWire()` / `createSolidIfHeight()`；`projectWire()` 通过 `BRepProj_Projection` 取 nearest wire，再重建 projected edges。
- `filterShapes()` 按 `Mode=All/Faces/Edges` 过滤输出；`createCompound()` 决定多结果组合边界。C5-M9 需要让过滤前后的 ownership 仍可追溯。
- `TopoShapeMapper*` / `PropertyTopoShape*` 是后续 mapper/history 与 ElementMap 证据入口；如果 FreeCAD native collector 暂不能暴露完整历史，必须留下 source-backed known_gap 和删除条件。

## cad-core 落点

- `cad-core/src/part/part_project_on_surface.cpp`：补 projected result provenance DTO、source item map、edge/wire/face/all evidence，删除 broad `projected_edge_provenance_mapper_history` gap 的无证据部分。
- `cad-core/src/part/topo_shape.cpp` 与 `cad-core/src/topo`：承接 MapperHistory / ElementMap / reference recovery hook；不得把命名传播写成 executor 输出修剪。
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
5. S4：同步 capability/docs/root matrices，关闭 `projected_edge_provenance_mapper_history` broad gap 或改成精确 remaining gap，队列清空后收口。

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
- capability metadata 中 `projected_edge_provenance_mapper_history` 不再是 broad gap；剩余项有具体 owner / delete_condition。
- 本包 `工作步骤细分` 队列为空，全局 `C5-BLK-901` 才能关闭。
