# C8-M5 C8M1 Expected Fixture Regression Recovery 阶段回归恢复主线总入口

## 目标

恢复 C8 阶段回归闸门。C8-M5 只处理 C8-M4 S6 暴露的两个 C8-M1 expected fixture drift，先冻结 live 失败，再按 FreeCAD source authority 与当前 `cad-core` 行为判断是 expected 过期还是实现回退，最后让阶段回归命令重新成为后续 C8 主线的发布闸门。

## 范围

必须进入本轮：

- `shape-binder-subshape-binder-element-map-namedshape-body-replay` 的 `BodyBaseFeature` 对象漂移。
- `subshape-binder-setlinks-normalization-diagnostics` 的 `cycle_rejected_by_property_link` / `cycle_dependency` 诊断漂移。
- `cad-core/fixtures/c8m1/expected/`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_c8_shapebinder.py`、`cad-core/tests/test_diagnostics.py` 中与上述两行直接相关的 expected / focused assertion。

明确不进入本轮：

- 不实现 `copy_on_change_full_temporary_document_cache`，该行继续保持 C8-M2 的 `known_gap` / `oracle_blocked` 结论。
- 不批量刷新全部 C8-M1 expected。
- 不新增 ShapeBinder / SubShapeBinder 几何能力。
- 不修改 FreeCAD 上游源码。
- 不重开 C8-M3 / C8-M4 已关闭 capability。

## 方案与矩阵

- 方案：`6-27-09-19-C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复方案.md`
- Source candidates：`矩阵/c8m5_expected_fixture_regression_source_candidates.tsv`
- Scope review：`矩阵/c8m5_expected_fixture_regression_scope_review_matrix.tsv`
- Blocker queue：`矩阵/c8m5_expected_fixture_regression_blocker_queue.tsv`
- Fixture oracle：`矩阵/c8m5_expected_fixture_regression_fixture_oracle_matrix.tsv`
- Non-goal registry：`矩阵/c8m5_expected_fixture_regression_non_goal_registry.tsv`
- Backend gap classification：`矩阵/c8m5_expected_fixture_regression_backend_gap_classification.tsv`
- Validation matrix：`矩阵/c8m5_expected_fixture_regression_validation_matrix.tsv`

## 工作步骤

执行入口在 `工作步骤细分/`：

1. S0：live 回归基线冻结。
2. S1：失败清单与 owner 分类矩阵。
3. S2：Expected authority 与当前输出复核。
4. S3：`BodyBaseFeature` 对象漂移专项复审。
5. S4：cycle 诊断代码漂移专项复审。
6. S5：approved refresh or code fix 准入。
7. S6：阶段回归发布闸门。

## 验收

文档包创建后先跑短跑验收：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线 docs/CADCore8.0/README.md
git diff --check
```

S6 发布闸门必须回到：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```
