# 【已实现】C6-M6-S6 阶段回归与 release-gate

## 目标

对 C6-M6 GeomPlate product contract 或 diagnostic boundary 做阶段回归和 heavy 收口。S6 通过后，才能把本主线写成已发布，并更新 `docs/CADCore6.0/README.md` 的最终状态。

## 结果

- `cmake --build build` 通过；仅出现 OCCT header 的既有 `sprintf` deprecation warning。
- 阶段回归通过：`python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`，`Ran 251 tests in 136.165s`，`OK (skipped=31)`。
- heavy 收口通过：`python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`，`Ran 287 tests in 126.397s`，`OK (skipped=31)`。
- `step_goal_queue.py` 对 C6-M6 工作步骤目录返回空表，S0 到 S6 均已关闭。
- `C6M6-BLK-901` 已关闭：C5-M10 Sweep 是 stale test expectation，当前 cad-core 输出 `ok` 并保留 FreeCADCmd wrapper blocker 为 historical evidence；C6-M5 Filling 是 OCCT 7.9.3 `BRepFill_Filling` builder-order 兼容性问题，已只在 builder 输入层归一化 G2，不改变公开 support-order evidence。

## 发布闸门

- `cmake --build build` 通过。
- GeomPlate focused tests 通过。
- expected / adapter capability focused suites 通过。
- 阶段回归通过：P8 / expected / adapter 相关 suites。
- heavy 收口通过：按 S5 影响范围补跑 topology / P8 / expected / adapter suites。
- `step_goal_queue.py` 对 C6-M6 工作步骤目录不再返回待执行实现步骤。
- capability 的 `remaining_gaps`、`narrowed_gaps` 和 `non_goals` 与 S3/S4/S5 证据一致。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/工作步骤细分 --format markdown
git diff --check -- cad-core docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线 docs/CADCore6.0/README.md
```

验收已通过，本文已重命名为 `6-24-20-00-【已实现】C6-M6-S6-阶段回归与release-gate.md`。C6-M6 可按 product contract non-parity 口径发布。

## 非目标

- 不跑全量上游 FreeCAD build。
- 不借 S6 扩大到 C6-M7 候选。
- 不声明 FreeCAD parity、GUI feature 或 native `Part::GeomPlate` DocumentObject。
