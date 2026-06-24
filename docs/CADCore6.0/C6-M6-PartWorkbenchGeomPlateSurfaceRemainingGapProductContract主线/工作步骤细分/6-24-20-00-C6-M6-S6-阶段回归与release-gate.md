# C6-M6-S6 阶段回归与 release-gate

## 目标

对 C6-M6 GeomPlate product contract 或 diagnostic boundary 做阶段回归和 heavy 收口。S6 通过后，才能把本主线写成已发布，并更新 `docs/CADCore6.0/README.md` 的最终状态。

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

验收通过后，将本文重命名为 `6-24-20-00-【已实现】C6-M6-S6-阶段回归与release-gate.md`。若重型收口因环境或 OCCT 差异失败，必须在矩阵中写清分类，不能直接标发布通过。

## 非目标

- 不跑全量上游 FreeCAD build。
- 不借 S6 扩大到 C6-M7 候选。
- 不声明 FreeCAD parity、GUI feature 或 native `Part::GeomPlate` DocumentObject。
