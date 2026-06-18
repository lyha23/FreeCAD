# P8 Assembly Reference / JCS MarkerPlacement S3 MarkerPlacementResolver 专项复审

## 目标

设计 cad-core 的统一 marker placement resolver，使 `jointReference()` 不再把 subshape reference 的 marker placement 简化为 `PlacementN`。S3 可以落 DTO / helper 代码和 focused JSON evidence，但不发布 capability。S3 的设计必须覆盖 `MP-BLOCK-001`、`MP-BLOCK-002`、`MP-BLOCK-003`、`MP-BLOCK-005` 的实现前置，不允许只做一个 fixture 的临时 helper。

## 实现纪律

- resolver 必须以 FreeCAD `handleOneSideOfJoint()` 为 source authority。
- resolver 输出 moving part local marker placement。
- resolver 必须分清 `connectorPlacement` 输入、object/subshape global placement、part-local marker placement 和 offsetPlc 边界。
- mixed reference 的 `swapJCS` 必须同步 reference、connector placement 和 marker placement，且只修改 request-local DTO。
- `buildAssemblySolveRequest()` 在 classification 前构造 references，因此后续 DistanceType / swap / special rewrite 不能让 markerPlacement 与 ReferenceN 脱节。
- 缺少 subshape placement、axis、normal 或 containing part 时输出稳定 diagnostic，不按 fixture 名称猜。
- RackPinion / Screw special rewrite 只能消费 resolver 输出，不被统一 resolver 覆盖。
- 禁止 silent fallback 到 connector-only markerPlacement；若为了 object-level baseline 保留兼容路径，必须通过 object-level 判定和 diagnostic 证明不会覆盖 subshape support。

## 必须拆开的 resolver 子问题

| 子问题 | 关闭 blocker | 要求 |
| --- | --- | --- |
| ordinary dispatch / validation | `MP-BLOCK-001` | moving part、object、ReferenceN 缺失时有稳定 diagnostic |
| object/subshape global transform | `MP-BLOCK-002` | 对齐 `getGlobalPlacement(nullptr, ref) * PlacementN` |
| part-local transform / offset | `MP-BLOCK-003` | 对齐 `getGlobalPlacement(part, ref).inverse()` 和 `offsetPlc` |
| mixed swap sync | `MP-BLOCK-005` | DistanceType swap 后 ReferenceN、PlacementN、connector、marker、solver DTO 同步 |
| fallback audit | `MP-BLOCK-010` | 不引入 fixture 名称分支、bbox heuristic、输出端修剪或 silent connector fallback |

## 建议落点

| 文件 | 内容 |
| --- | --- |
| `cad-core/include/cad_core/assembly/joint_solver.h` | resolver evidence / diagnostic 字段 |
| `cad-core/src/assembly/joint_solver.cpp` | `resolveJointMarkerPlacement()` 或等价 helper |
| `cad-core/src/assembly/assembly_utils.cpp` | JSON evidence 输出 |
| `cad-core/tests/test_p8_features.py` | 不依赖 native expected 的 DTO / marker evidence smoke |

## 验收

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k marker
rg -n "connectorPlacement|markerPlacement|fallback|bbox|fixture" cad-core/src/assembly/joint_solver.cpp cad-core/include/cad_core/assembly/joint_solver.h
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

## 非目标

- 不采集 expected。
- 不发布 C ABI capability。
- 不实现 radius-bearing DistanceType。
