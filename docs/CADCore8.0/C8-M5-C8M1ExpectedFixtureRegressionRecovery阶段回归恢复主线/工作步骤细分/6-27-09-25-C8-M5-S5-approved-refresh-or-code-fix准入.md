# C8-M5-S5 approved refresh or code fix 准入

## 目标

把 S3/S4 的裁决落成最小实现：每个 drift 只能走 `approved_expected_refresh` 或 `code_fix_required`，并在同一轮完成 focused test 更新。

## 执行

1. 逐项读取 `backend_gap_classification.tsv`，确认 `C8M5-GAP-101` 和 `C8M5-GAP-201` 已有裁决。
2. 对 `approved_expected_refresh`：只更新对应 expected fixture 的必要字段，记录 source authority 与 current output diff，不做全集刷新。
3. 对 `code_fix_required`：只修改对应 runtime 主路径和 focused tests，不在 adapter / comparator / fixture 名分支补丁。
4. 若修改 diagnostic vocabulary，同步 `cad-core/src/runtime/capability_contract.cpp`、`tests/test_diagnostics.py` 或 adapter tests 中相关最小断言。
5. 更新 README、blocker queue、fixture oracle、validation matrix。

## 验收

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

仓库检查：

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/矩阵/*.tsv
git diff --check
```

## 退出标准

- `C8M5-BLOCKER-501` 关闭。
- 两个 drift 均已落代码或 expected 更新。
- focused expected fixture、C8 shapebinder tests、diagnostics tests 通过。
- `copy_on_change_full_temporary_document_cache` 未从 known gap 变成 supported。
