# C6-M7-S5 阶段回归与 release-gate

## 目标

对 C6-M7 Loft subelement assignment 收口结果做阶段回归和发布闸门。S5 通过后，才能把 C6-M7 写成已发布，并决定是否进入 Surface Family freeze 候选。

## 发布闸门

- `cmake --build build` 通过。
- Loft focused tests 通过。
- expected / adapter capability focused suites 通过。
- 阶段回归通过：P8 / expected / adapter 相关 suites。
- `step_goal_queue.py` 对 C6-M7 工作步骤目录不再返回待执行实现步骤。
- `part_workbench.loft.remaining_gaps`、`narrowed_gaps` 和 `non_goals` 与 S3/S4 证据一致。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```

验收通过后，将本文重命名为 `6-25-00-59-【已实现】C6-M7-S5-阶段回归与release-gate.md`。若 `part_workbench.loft.remaining_gaps=[]`，下一步可开 Surface Family freeze；若仍保留 native-hidden blocker，必须写清 blocker 分类，不能伪造发布完成。

## 非目标

- 不跑全量上游 FreeCAD build。
- 不借 S5 扩大到 Surface Family freeze。
- 不声明 FreeCAD parity。
