# 【已实现】C6-M7-S5 阶段回归与 release-gate

## 目标

对 C6-M7 Loft subelement assignment 收口结果做阶段回归和发布闸门。S5 通过后，才能把 C6-M7 写成已发布，并决定是否进入 Surface Family freeze 候选。

## 执行基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=755e91bdc4`。
- `git log -1 --oneline=755e91bdc4 发布 C6-M7 S4 Loft 能力合同`。
- S5 开始时工作区干净。
- S5 开始时 C6-M7 队列只剩 `6-25-00-59-C6-M7-S5-阶段回归与release-gate.md`。

## 结果

- `cmake --build build` 通过。
- 阶段回归通过：`python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`，`Ran 254 tests in 85.216s`，`OK (skipped=31)`。
- `part_workbench.loft.remaining_gaps=[]`，C6-M7 selected subelement product contract evidence 已发布；原 `part_loft_subelement_assignment_native_hidden` 仅作为 `narrowed_gaps` / historical native-hidden evidence 保留。
- 本 release gate 不声明 FreeCAD parity，不新增 FreeCAD native selected subelement expected，不做 Surface Family freeze。
- S5 关闭后，下一步可另开 Surface Family freeze / publication audit 包。

## 发布闸门

- `cmake --build build` 已通过。
- Loft focused tests、expected / adapter capability focused suites 已包含在阶段回归中。
- 阶段回归已通过：P8 / expected / adapter 相关 suites。
- `step_goal_queue.py` 对 C6-M7 工作步骤目录不再返回待执行实现步骤。
- `part_workbench.loft.remaining_gaps`、`narrowed_gaps` 和 `non_goals` 与 S3/S4 证据一致。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```

验收通过后，本文已重命名为 `6-25-00-59-【已实现】C6-M7-S5-阶段回归与release-gate.md`。C6-M7 可按 CAD Core request-local product contract non-parity 口径发布；后续若需要 Surface Family freeze / publication audit，另开包处理。

## 非目标

- 不跑全量上游 FreeCAD build。
- 不借 S5 扩大到 Surface Family freeze。
- 不声明 FreeCAD parity。
- 不新增 FreeCAD native selected subelement expected。
