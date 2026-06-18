# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S5 实现与 focused parity（部分收口，S6 blocked）

## 目标

切换 cad-core marker placement 主路径，使 representative Vertex / Edge / Face / mixed fixtures 的 `placement_updates` 与 checked-in FreeCAD expected 对齐，并保持已发布 JointTypes 不回退。

## 必须完成

- `jointReference()` 使用统一 resolver 计算 marker placement。
- resolver 失败时输出 diagnostic，不 silent fallback 到错误 placement，也不把 connector-only shortcut 当 subshape supported。
- object/subshape global transform、part-local transform、offsetPlc 边界、mixed swap sync 都必须在同一轮实现或明确写为阻塞，不能只实现 Vertex 或单个 Distance fixture。
- DistanceType S5 的 7 个 basic fixtures 可以从 solver DTO parity 升级到 placement parity 覆盖。
- `addConstraintToOndselAssembly()` 必须消费 resolver 输出；focused parity 不能只看 JSON evidence。
- 删除 S4 expected 顶层 `known_gap` 的条件固定为：对应 fixture 的 FreeCAD `native_marker_oracle` 已被 cad-core resolver 消费，`solver_adapter.placement_updates` 与 checked-in expected 对齐，且 `CadCoreExpectedFixtureTest` 对该 fixture 不再跳过。
- RackPinion / Screw special rewrite focused tests 继续通过。
- object-level baseline expected 继续通过。
- `MP-BLOCK-010` fallback audit 必须关闭：无 fixture 名称分支、bbox heuristic、输出端修剪、silent connector fallback。

## S5 实现结论

- `cad-core/src/assembly/joint_solver.cpp::resolveJointMarkerPlacement()` 已从 S3 的 subshape withholding 切到 S5 resolver：Reference 缺失 / missing object 仍给稳定 diagnostic；Vertex、linear Edge、planar Face subshape 进入 `resolved_subshape_handle_one_side`，并输出 request-local part-local `markerPlacement`。
- real Ondsel adapter 不再对缺失 marker 做 identity fallback：supported joint 两侧 marker 缺失时进入 `unsupported_assembly_solver`，`addConstraintToOndselAssembly()` 只消费 resolver 输出。
- `CadCoreExpectedFixtureTest` 已解锁 14 个 S4 subshape expected：Ball Vertex、Revolute / Slider / Cylindrical Edge、Fixed / Parallel / Perpendicular / Angle Face、Distance PointPoint zero / nonzero、Distance LineLine、Distance PointPlane、Distance LinePlane、Distance PlanePlane。
- 仍保留 1 个 checked-in expected 的 `known_gap`：`assembly-distance-point-line-real-solver`。这个 case 当前 real Ondsel `placement_updates` 仍与 native expected 不一致，集中在 Distance PointLine 的 FreeCAD JCS / marker initialization parity。
- RackPinion / Screw special regression 未回退；`assembly-screw-rackpinion-sliding-swap-diagnostic` 现在因 marker resolver 可用进入 real solver path，focused test 已改为验证 request-local swap、Screw scalar、RackPinion rewrite 和 writeback。
- S5 未发布 C ABI capability；S6 不能进入发布闸门，除非上述 1 个 remaining expected 继续关闭或 upstream scope 文档明确收窄发布范围。

## remaining known_gap

| fixture | 当前 cad-core 表现 | native expected | blocker |
| --- | --- | --- | --- |
| `assembly-distance-point-line-real-solver` | `ComponentB` 写到 `[1.5, 0, 0]` | 近似 `[4.003128620988115, 0, -0.007784756160183933]` 且有 Y 轴旋转 | `MP-BLOCK-002/003/005/006` |

## 验收

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k 'assembly|distance_type|marker'
python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest -k c3m6
rg -n "fixture|bbox|fallback|connectorPlacement|markerPlacement" cad-core/src/assembly/joint_solver.cpp cad-core/tests/test_p8_features.py
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/fixtures/c3m6 cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

如 `CadCoreExpectedFixtureTest -k c3m6` 被 unrelated known historical expected 阻塞，必须列出阻塞项，并至少执行本包新增 expected 的 focused route。

本轮验证结论：

- `cmake --build cad-core/build` 通过。
- `python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k 'assembly|distance_type|marker'` 在当前 `unittest` 中按字面匹配，结果 0 tests / exit 5；等价多 `-k` 命令 `python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k assembly -k distance_type -k marker` 通过，`Ran 19 tests`。
- `python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest -k c3m6` 同样按测试方法名匹配，结果 0 tests / exit 5；实际执行 `python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest` 通过，`OK (skipped=10)`，其中本包 remaining c3m6 skip 为上述 1 个 expected。

## 非目标

- 不修改 expected 来适配 cad-core。
- 不靠输出端修剪 placement_updates。
- 不添加 fixture 名称分支。
