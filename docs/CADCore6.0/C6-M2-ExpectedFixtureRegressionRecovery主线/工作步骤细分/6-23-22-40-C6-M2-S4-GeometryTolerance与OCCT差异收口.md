# C6-M2 S4 GeometryTolerance 与 OCCT 差异收口

## 目标

处理 S2/S3 后剩余的 bbox、geometry、topology 或 OCCT 兼容性 mismatch。S4 的关键是区分 expected stale、环境 / OCCT 差异和真实实现回归，不允许用全局放宽断言绕过问题。

## 必读输入

- S2/S3 已实现文档
- `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`
- `矩阵/c6m2_expected_fixture_regression_backend_gap_classification.tsv`
- 相关 feature / geometry / topo 源码
- 相关 fixture expected JSON

## 实施内容

1. 只消费 owner 为 `bbox_geometry_or_occt_drift` 或 S3 遗留的 geometry 行。
2. 对每个 bbox mismatch 记录 expected/current 数值差异、fixture、shape owner 和可能 OCCT 版本原因。
3. 若是环境差异，登记 `known_environment_gap`，写明正式 expected 采集基线和本机 smoke 结论。
4. 若是实现回归，修 `features` / `geometry` / `topo` 正式语义，不做 fixture 特判。
5. 若是 expected stale，最小批次刷新对应 expected。
6. 完成后将本文件改名为 `6-23-22-40-【已实现】C6-M2-S4-GeometryTolerance与OCCT差异收口.md`。

## 非目标

- 不修改全局 bbox tolerance，除非证明该 tolerance 是测试合同错误。
- 不根据 fixture 名称写 geometry 特判。
- 不把 OCCT 兼容性 smoke test 当成正式 FreeCAD parity oracle。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```
