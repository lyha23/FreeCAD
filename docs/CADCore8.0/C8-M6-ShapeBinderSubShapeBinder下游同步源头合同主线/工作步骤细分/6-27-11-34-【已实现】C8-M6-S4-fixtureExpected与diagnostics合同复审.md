# 【已实现】C8-M6 S4 fixture expected 与 diagnostics 合同复审

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

## S4 完成记录

- 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=0f8109dbe3`，`git log -1 --oneline=0f8109dbe3 docs: 完成 C8-M6 S3 合同复审`，开始工作区干净。
- 队列基线：S4 执行前 `step_goal_queue.py` 显示当前 pending 为 S4、S5、S6。
- fixture contract：12 个 `cad-core/fixtures/c8m1/*.json` input 与 12 个 `cad-core/fixtures/c8m1/expected/*.freecad.json` 已逐行回写到 `c8m6_downstream_sync_fixture_contract_matrix.tsv`，作为下游黑盒合同。
- `shape-binder-subshape-binder-element-map-namedshape-body-replay` 采用 C8-M5 refreshed expected：input 只有 `Body.Group` / `Body.Tip`，没有 `Body.BaseFeature`；expected objects 为 `Body`、`Fusion`、`ShapeBinder`、`SubShapeBinder`，不包含 `BodyBaseFeature`，`documentObjectUpdates` 保持空。
- `subshape-binder-setlinks-normalization-diagnostics` 采用 setter-level `cycle_rejected_by_property_link`；`tests/test_diagnostics.py` 继续保留 generic graph cycle 的 `cycle_dependency`，不做 adapter 字符串改写。
- CopyOnChange fixture 只证明 `BindCopyOnChange=Disabled/Enabled/Mutated`、`PartialLoad=True` 的 request-local property-state 和 known-gap diagnostic 边界；expected 中的 full temporary-document copied-object cache 仍是 `known_gap` / `native_oracle_blocked`。
- `C8M6-SYNC-102`、`C8M6-SYNC-103`、`C8M6-SYNC-105` 与 `C8M6-BLOCKER-401` 已回写为 `published_s4` / closed；未刷新 expected，未修改 `cad-core/src`、fixtures、tests 或 expected，未发现 `unexpected_mismatch`。
- 验证：`python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics` 通过，18 tests；`python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 通过，35 skipped known-gap fixtures。
- 文档验收：C8-M6 TSV 字段数检查通过；C8-M6 包和 `docs/CADCore8.0/README.md` 尾随空白扫描无匹配，`rg` exit 1 按规则视为通过；`git diff --check` 通过；S4 重命名后队列下一项为 S5。

## 非目标

- 不刷新全部 C8-M1 expected。
- 不新增 native FreeCAD oracle collector。
- 不把 fixture 输出顺序差异写成失败，除非数量、几何内容、稳定 subname 或引用语义不一致。
