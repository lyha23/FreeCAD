# C5-M8-S5 capability 与文档收口

## 目标

把 C5-M8 的 expected-backed、source-backed known_gap、diagnostic-backed 和 non-goal 边界同步到 capabilities、docs、root matrices 和本包队列；只有队列为空才宣告收口。

## 必读

- C5-M8 总入口、方案、局部矩阵和所有已实现 predecessor step。
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/README.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`

## 产物

- 更新 capability metadata 与 `docs/CADCore3.0/capabilities-gap对照表.md` 的 Filling support / remaining gap 文案。
- 更新 C5-M8 局部矩阵和全局 `cadcore5_*` 矩阵，关闭 C5-BLK-801。
- 本包所有 step 文件按完成状态改名为 `【已实现】...`。
- 用队列脚本证明 `工作步骤细分` 为空。

## 非目标

- 不重新打开 Surface Workbench、GUI、native DocumentObject 或完整 Part surface family。
- 不把 wrapper diagnostic 写成 supported。
- 不跑全量 FreeCAD 构建，除非本 step 的阶段收口需要 cad-core build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 docs/CADCore3.0 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
