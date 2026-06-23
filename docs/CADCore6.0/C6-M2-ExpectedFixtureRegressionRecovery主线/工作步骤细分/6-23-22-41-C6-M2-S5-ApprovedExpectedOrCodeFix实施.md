# C6-M2 S5 ApprovedExpectedOrCodeFix 实施

## 目标

执行 S2-S4 已批准但尚未完成的 expected refresh 或代码修复，形成最小完整收口批次。S5 不能新增未经矩阵批准的 fixture 行。

## 必读输入

- S2/S3/S4 已实现文档
- `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`
- `矩阵/c6m2_expected_fixture_regression_blocker_queue.tsv`
- 所有待更新 expected JSON 或待修源码

## 实施内容

1. 汇总所有 `approvedRefresh` 和 `implementationFix` 行，按 owner 分批执行。
2. 对 expected refresh，更新最小必要 JSON，并在矩阵记录生成命令或 diff 证据。
3. 对 implementation fix，修改对应 `cad-core` 层级，不在 adapter 或测试里绕过业务语义。
4. 跑 focused expected fixture test，必要时补相关 feature / adapter focused test。
5. 更新 blocker queue 的 close condition 和状态。
6. 完成后将本文件改名为 `6-23-22-41-【已实现】C6-M2-S5-ApprovedExpectedOrCodeFix实施.md`。

## 非目标

- 不做 blanket refresh。
- 不引入新产品能力。
- 不改上游 FreeCAD。
- 不把 remaining known gap 标成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```
