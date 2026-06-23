# C6-M2 S3 SchemaDrift 收口

## 目标

优先处理 S2 判定为 schema / contract drift 的 mismatch，包括 `diagnostic_codes`、`external_geometry_count`、link 类型和 assembly `solver_adapter` 字段。S3 必须按合同修 schema 或 expected，不能通过放宽测试隐藏字段漂移。

## 必读输入

- S2 已实现文档
- `矩阵/c6m2_expected_fixture_regression_fixture_oracle_matrix.tsv`
- `矩阵/c6m2_expected_fixture_regression_backend_gap_classification.tsv`
- `cad-core/tests/test_expected_fixtures.py`
- 相关 parser / adapter / feature 源码

## 实施内容

1. 只消费 owner 为 `diagnostic_codes_contract_drift`、`external_geometry_count_contract_drift`、`link_schema_or_type_drift`、`assembly_solver_adapter_schema_drift` 的行。
2. 对 `fix_implementation` 行修对应 C++ / parser / adapter / tests。
3. 对 `refresh_expected` 行做最小 expected 更新，并记录命令证据。
4. 对无法关闭的 schema 行写 known gap、owner 和删除条件。
5. 更新矩阵状态和本步骤文档，完成后改名为 `6-23-22-39-【已实现】C6-M2-S3-SchemaDrift收口.md`。

## 非目标

- 不处理 bbox / OCCT 差异，留给 S4。
- 不刷新所有 expected。
- 不删除 schema 字段或断言来获得通过。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```

若 focused 仍失败，失败必须只剩 S4 或后续 known gap owner。
