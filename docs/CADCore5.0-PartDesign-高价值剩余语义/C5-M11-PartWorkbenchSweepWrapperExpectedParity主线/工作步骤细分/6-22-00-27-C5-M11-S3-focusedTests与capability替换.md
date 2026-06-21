# C5-M11-S3 focused tests 与 capability 替换

状态：`pending_C5M11-S3_tests_capability`

## 目标

把 S2 的 wrapper expected 接入 focused tests 和 capability metadata：`part_workbench.sweep` advanced wrapper 字段从 `source_backed_known_gap` 晋级 expected-backed，`remaining_gaps` 删除 `part_sweep_wrapper_expected_collector`。

## 必读

- S2 生成的 `cad-core/fixtures/c5m10/expected/*.freecad.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 产物

- `tests.test_p8_features` 不再断言 C5-M10 wrapper expected 的 `known_gap.kind`，改为断言 `object_fields.advanced` 与 shape summary。
- `tests.test_expected_fixtures` 覆盖 C5-M10 advanced wrapper expected compare；`--check` 路径能捕获 metadata 回退。
- `c_api.cpp` 的 `part_workbench.sweep` capability 把 wrapper fields 写入 expected-backed 或 expected-backed-with-diagnostics；`source_backed_known_gaps.part_sweep_wrapper_expected_collector` 删除或缩到明确未采子项。
- `tests.test_adapters` 断言 `remaining_gaps` 不再包含 `part_sweep_wrapper_expected_collector`，同时 non-goals 不被误删。
- 更新 `C5M11-BLK-301`、`C5M11-SCOPE-301`、`C5M11-ORC-301`。

## 非目标

- 不新增 C5-M10 之外的 advanced wrapper fixture。
- 不删除 GUI、PartDesign Pipe/Hole、persistent wrapper lifecycle 等 non-goal。
- 不改 native `Part::Sweep` base field expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```
