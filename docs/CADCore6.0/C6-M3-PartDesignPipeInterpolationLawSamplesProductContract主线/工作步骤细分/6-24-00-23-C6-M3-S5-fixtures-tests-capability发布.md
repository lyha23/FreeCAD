# C6-M3 S5 fixtures tests capability 发布

## 目标

用 fixtures、focused tests、expected fixture 和 adapter/capability assertions 发布 Interpolation LawSamples product contract。

## 必读输入

- S0-S4 已实现文档
- `cad-core/fixtures/c6m1`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`

## 实施内容

1. 新增或迁移成功 Interpolation fixture。
2. 新增 invalid LawSamples diagnostics fixture / tests。
3. 更新 capability：移除或收窄 `partdesign_pipe_interpolation_law_product_contract_required` remaining gap，只发布已实现的 product contract。
4. 更新 adapter tests，确认 capability wording 不声明 FreeCAD parity。
5. 跑 focused tests 和 expected fixture gate。
6. 更新矩阵和 S5 文档，完成后改名为 `6-24-00-23-【已实现】C6-M3-S5-fixtures-tests-capability发布.md`。

## 非目标

- 不发布 full PartDesign Pipe coverage。
- 不删除未实现的 diagnostics。
- 不新增 LawSamples GUI 合同。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```
