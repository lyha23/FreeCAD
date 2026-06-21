# C5-M9-S4 capability 与文档收口

状态：`pending`

## 目标

把 C5-M9 的实现结果同步到 capabilities、gap 对照、root matrices 和本包文档。关闭或拆细 `projected_edge_provenance_mapper_history` broad gap，并证明本包队列为空后才能收口。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore3.0/oracle-fixture队列.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`

## 产物

- 更新 capabilities：supported / source-backed known_gap / diagnostic / non-goal 精确到 edge/wire/face/all provenance。
- 更新 `docs/CADCore3.0/capabilities-gap对照表.md` 和 C5 root matrices，关闭 `C5-BLK-901` 或留下更小、具体、有 owner 的 remaining gap。
- 更新本包入口、方案、local matrix 和 step 文件名状态。
- 运行 queue script，确认 C5-M9 package queue 为空。

## 非目标

- 不把 unsupported GUI / full family 写成 supported。
- 不补超出 S2/S3 证据的实现。
- 不用 broad “ProjectOnSurface 完整支持” 替代精确 capability wording。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
