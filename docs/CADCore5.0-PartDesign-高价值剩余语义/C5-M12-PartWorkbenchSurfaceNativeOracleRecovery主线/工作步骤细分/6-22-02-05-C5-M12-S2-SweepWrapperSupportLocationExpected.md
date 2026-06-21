# C5-M12-S2 Sweep wrapper support/location expected recovery

状态：`pending_C5M12-S2_sweep_wrapper_recovery`

## 目标

基于 S1 probe，恢复或进一步收窄 C5-M11 后剩余的 Sweep wrapper blockers：valid `SpineSupport` / `SupportMode` representative、`SectionOptions[].Location` / `WithContact` / `WithCorrection`、`advanced_combination`。

## 必读

- S1 probe 结论。
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c5m10/part-sweep-*.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-*.freecad.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 产物

- collectable Sweep wrapper 场景替换为 FreeCADCmd wrapper expected。
- 不可采集项改成更窄 blocker，记录 FreeCADCmd 错误、未采字段、下一批范围。
- 更新 `C5M12-BLK-201`、`C5M12-SCOPE-201`、`C5M12-ORC-201`。

## 非目标

- 不把 advanced wrapper 字段声明为 native `Part::Sweep` direct properties。
- 不做 GUI / persistent wrapper lifecycle。
- 不用 cad-core 输出生成 expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
```
