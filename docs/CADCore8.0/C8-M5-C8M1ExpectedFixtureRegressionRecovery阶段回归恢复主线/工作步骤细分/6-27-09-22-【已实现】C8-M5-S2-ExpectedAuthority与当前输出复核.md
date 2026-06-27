# C8-M5-S2 ExpectedAuthority 与当前输出复核

## 目标

复核两个 C8-M1 expected 是否仍代表 FreeCAD authority，还是已经被 C8-M1 后续实现、测试或 diagnostic vocabulary 正式替代。

## 输入

- S1 更新后的 source / scope / oracle 矩阵。
- `cad-core/tests/fixture_expected.py`
- `cad-core/tests/test_expected_fixtures.py`
- 两个 C8-M1 input fixture 与 expected fixture。

## 执行

1. 对两个目标 fixture 单独跑 current recompute / expected compare，保存最小 diff 摘要到 README 或 oracle 矩阵。
2. 若 diff 只来自 expected stale，标为 `approved_refresh_candidate`，但不在 S2 刷文件。
3. 若 diff 指向 runtime 回退，标为 `code_fix_required_candidate`，并写明落点。
4. 若需要 native FreeCAD 复采，先确认 `FreeCADCmd` / collector 可用；若 sandbox Qt/processor 报错，只记录为环境限制，不把它当成 FreeCAD 语义结论。
5. 更新 blocker / fixture oracle / backend gap classification 矩阵。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

该命令在 S2 可失败，但失败列表必须只剩本轮两个目标 drift 或文档中记录的 unrelated known issue。

文档检查：

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/矩阵/*.tsv
git diff --check
```

## 退出标准

- `C8M5-BLOCKER-201` 关闭。
- 两个 drift 都有 `approved_refresh_candidate` 或 `code_fix_required_candidate` 的初步裁决。
- native oracle 需求和环境限制被写清楚。

## S2 完成记录

- 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=528f294e98`，`git log -1 --oneline=528f294e98 docs: 完成 C8-M5 S1 owner 分类矩阵`，开始工作区干净。
- 对两个目标 fixture 以 `CAD_CORE_TEST_LEGACY_OUTPUT=1 ./build/cad-core recompute` 单独导出 current 输出到 `/tmp/c8m5-s2/`，并用 expected comparator 做局部比较。
- `shape-binder-subshape-binder-element-map-namedshape-body-replay`：current diagnostics 为空，current object map 缺 `BodyBaseFeature`，`documentObjectUpdates=[]`；fresh native collector 与仓库 expected 均含 `BodyBaseFeature`，但 input fixture 本身没有 `BodyBaseFeature` 对象，也没有 `Body.BaseFeature` 属性。S2 初步裁决为 `approved_refresh_candidate`，S3 继续裁决是刷新该 expected 字段还是拆出独立 BaseFeature fixture。
- `subshape-binder-setlinks-normalization-diagnostics`：current diagnostic code 为 `cycle_dependency`，expected 与 fresh native collector 均为 `cycle_rejected_by_property_link`；`SubShapeBinder::setLinks()` 对 self link 在 setter 阶段拒绝。S2 初步裁决为 `code_fix_required_candidate`，S4 落点是 runtime/reference lifecycle 或 graph 诊断语义层，不在 adapter 改字符串。
- `python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 已运行，结果为 1 error、1 failure、35 skipped；失败列表仅包含上述两个目标 drift。
- native collector 已通过 `/home/user/.local/bin/freecadcmd` 成功运行，版本为 `FreeCAD 1.2.0 revision 20260519`；未遇到 sandbox Qt/processor 环境限制。
- 已更新 README、fixture oracle、backend gap classification、scope review、source candidates、blocker queue 和 validation matrix；`C8M5-BLOCKER-201` 已关闭，下一队列首项为 S3。
