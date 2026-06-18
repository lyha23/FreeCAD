# P8 Assembly Reference / JCS MarkerPlacement S5 实现与 focused parity

## 目标

切换 cad-core marker placement 主路径，使 representative Vertex / Edge / Face / mixed fixtures 的 `placement_updates` 与 checked-in FreeCAD expected 对齐，并保持已发布 JointTypes 不回退。

## 必须完成

- `jointReference()` 使用统一 resolver 计算 marker placement。
- resolver 失败时输出 diagnostic，不 silent fallback 到错误 placement，也不把 connector-only shortcut 当 subshape supported。
- object/subshape global transform、part-local transform、offsetPlc 边界、mixed swap sync 都必须在同一轮实现或明确写为阻塞，不能只实现 Vertex 或单个 Distance fixture。
- DistanceType S5 的 7 个 basic fixtures 可以从 solver DTO parity 升级到 placement parity 覆盖。
- `addConstraintToOndselAssembly()` 必须消费 resolver 输出；focused parity 不能只看 JSON evidence。
- RackPinion / Screw special rewrite focused tests 继续通过。
- object-level baseline expected 继续通过。
- `MP-BLOCK-010` fallback audit 必须关闭：无 fixture 名称分支、bbox heuristic、输出端修剪、silent connector fallback。

## 验收

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k 'assembly|distance_type|marker'
python3 -m unittest cad-core.tests.fixture_expected.CadCoreExpectedFixtureTest -k c3m6
rg -n "fixture|bbox|fallback|connectorPlacement|markerPlacement" cad-core/src/assembly/joint_solver.cpp cad-core/tests/test_p8_features.py
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/fixtures/c3m6 cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

如 `CadCoreExpectedFixtureTest -k c3m6` 被 unrelated known historical expected 阻塞，必须列出阻塞项，并至少执行本包新增 expected 的 focused route。

## 非目标

- 不修改 expected 来适配 cad-core。
- 不靠输出端修剪 placement_updates。
- 不添加 fixture 名称分支。
