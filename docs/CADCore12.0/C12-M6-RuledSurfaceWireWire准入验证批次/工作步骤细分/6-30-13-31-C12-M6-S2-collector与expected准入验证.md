# C12-M6 S2 collector 与 expected 准入验证

## 目标

验证 wire/wire fixture 和 checked-in expected 是否满足旧 S2 要求的 source-backed collector 条件，而不是 cad-core 自证或手写 expected。

## 必读来源

- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json`
- `cad-core/fixtures/c4m1/expected/part-ruled-surface-wire-wire.freecad.json`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tests/test_p8_features.py::test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance`
- `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`

## 操作

1. 确认 fixture 使用 `TypeId=Part::RuledSurface`，输入对象是可重算 wire producers，`Curve1` / `Curve2` 为 `App::PropertyLinkSub`。
2. 确认 expected reference 指向 FreeCADCmd oracle，并记录 FreeCAD / OCCT baseline、shape、topology_counts、bbox、volume 和 named_shapes expectation。
3. 检查 collector 是否支持 `Part::RuledSurface` native type；如需重跑，只允许在 S2 记录 runtime baseline，不在本步改 expected。
4. 若 expected 缺失、reference 不可信或 collector 无法复现，关闭为 `retained_validation_blocker`，不进入 implementation。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c4m1_part_ruled_surface_wire_wire_builds_shell_with_provenance
```

S2 可只运行 focused test；不要默认刷新 expected 或跑 full build。
