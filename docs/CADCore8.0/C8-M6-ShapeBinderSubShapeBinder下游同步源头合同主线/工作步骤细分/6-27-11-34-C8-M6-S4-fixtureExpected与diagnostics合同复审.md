# C8-M6 S4 fixture expected 与 diagnostics 合同复审

## 目标

复核 C8-M1 fixture expected 和 diagnostics vocabulary，确保下游同步合同采用 C8-M5 之后的最终 expected 口径。

## 输入

- `cad-core/fixtures/c8m1/*.json`
- `cad-core/fixtures/c8m1/expected/*.freecad.json`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_diagnostics.py`
- `cad-core/tests/test_expected_fixtures.py`
- C8-M5 S3/S4/S6 结论。

## 必须回写的矩阵行

- `c8m6_downstream_sync_fixture_contract_matrix.tsv` 全部行。
- `C8M6-SYNC-102`
- `C8M6-SYNC-103`
- `C8M6-SYNC-105`
- `C8M6-BLOCKER-401`

## 复核重点

- `shape-binder-subshape-binder-element-map-namedshape-body-replay`：以 C8-M5 刷新后的 expected 为准，不要求无输入 `Body.BaseFeature` 时出现 `BodyBaseFeature`。
- `subshape-binder-setlinks-normalization-diagnostics`：以 `cycle_rejected_by_property_link` 为 setter-level cycle 诊断合同；generic graph cycle 仍可保持 `cycle_dependency`。
- CopyOnChange fixture 只能证明 request-local property-state 和 diagnostic 边界；不能证明 full temporary-document copied-object cache。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

## 非目标

- 不刷新全部 C8-M1 expected。
- 不新增 native FreeCAD oracle collector。
- 不把 fixture 输出顺序差异写成失败，除非数量、几何内容、稳定 subname 或引用语义不一致。
