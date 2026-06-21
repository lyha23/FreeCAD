# C5-M9-S1 source / oracle / provenance 矩阵

状态：`pending`

## 目标

读 FreeCAD source 与 cad-core 当前实现，冻结 projected result provenance 字段、native oracle 可采路径、source-backed known_gap 删除条件和 fixture matrix。S1 先把账本说清楚，再允许 S2/S3 写实现。

## 必读

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.h`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/topo/`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- 写清 `getProjectionShapes()` 的 object/subname/item index 如何传到 `createProjectedWire()`、`projectWire()`、`projectFace()`、`filterShapes()`、`createCompound()`。
- 定义 C5-M9 provenance evidence：source object、source subname、Projection item index、Mode 分支、edge/wire fragment、face wire、compound child、mapper/history id、ElementMap/reference recovery hook。
- 盘点 native collector 能采哪些字段；不能采的字段记录 source-backed known_gap、delete_condition 和临时 expected 策略。
- 更新本包 fixture/oracle matrix、scope matrix 和 blocker queue；关闭 `C5M9-BLK-101`。

## 非目标

- 不写 executor 主路径。
- 不用 cad-core 输出倒推 expected。
- 不按 bbox、输出顺序、fixture 名或几何相似性设计 provenance。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
