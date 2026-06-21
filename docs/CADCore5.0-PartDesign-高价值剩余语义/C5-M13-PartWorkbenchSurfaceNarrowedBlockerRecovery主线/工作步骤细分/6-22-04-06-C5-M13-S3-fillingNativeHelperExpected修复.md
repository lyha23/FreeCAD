# C5-M13-S3 Filling native helper expected 修复

状态：`pending_C5M13-S3_filling_helper_expected`

## 目标

处理 C5-M12 后 Filling helper 的 remaining blockers：surface/support/order、G2、non-default params、non-boundary edge support/order。S3 必须按同一 `Part.makeFilledFace(...)` helper / `BRepOffsetAPI_MakeFilling` API 批量推进，不能只挑单个 support/order fixture。

## 必读

- S1 probe 矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`

## 产物

- 可采集的 surface/support/order/G2/params/non-boundary support-order 代表场景写 FreeCADCmd expected。
- 不可采集场景写精确 blocker：错误文本、参数组合、是否 crash、是否 decode 问题、删除条件。
- 如果 cad-core DTO / collector 参数构造错误，补实现和 focused tests；不要靠 output 修正。
- 更新局部矩阵 `C5M13-BLK-301`、`C5M13-SCOPE-301`、`C5M13-ORC-301` 和 root `C5-ORC-1304`。

## 非目标

- 不声明 native `Part::FilledFace` DocumentObject。
- 不实现 Surface Workbench GUI/native feature。
- 不引入 cross-request mutable `Part.BRepOffsetAPI.MakeFilling` wrapper。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m13 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```
