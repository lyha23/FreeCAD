# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S3 MarkerPlacementResolver 专项复审

## 目标

设计 cad-core 的统一 marker placement resolver，使 `jointReference()` 不再把 subshape reference 的 marker placement 简化为 `PlacementN`。S3 可以落 DTO / helper 代码和 focused JSON evidence，但不发布 capability。S3 的设计必须覆盖 `MP-BLOCK-001`、`MP-BLOCK-002`、`MP-BLOCK-003`、`MP-BLOCK-005` 的实现前置，不允许只做一个 fixture 的临时 helper。

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
ed9673867b

git log -1 --oneline
ed9673867b docs: 完成 P8 MarkerPlacement S2 范围准入

git -c core.quotepath=false status --short -uall
<clean>
```

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

## S3 实现结论

- `AssemblyJointReference` 已新增 resolver status / frame / diagnostic / evidence 字段，`solver_joints.reference1/2` JSON 现在暴露 `connectorPlacement`、`markerPlacement`、`markerResolutionStatus`、`markerResolutionFrame`、`markerResolutionDiagnostic` 和相关布尔证据。
- `cad-core/src/assembly/joint_solver.cpp::resolveJointMarkerPlacement()` 已落地 object-level 安全路径：object-level reference 使用 `PlacementN` 或 FreeCAD 缺省 identity 作为 request-local part-local marker baseline。
- subshape refs 现在稳定输出 `requires_subshape_handle_one_side_evidence`，并 withholding `markerPlacement`，明确需要 FreeCAD `handleOneSideOfJoint()` 的 object-global -> part-local -> `offsetPlc` 证据；这不是 supported subshape parity。
- DistanceType mixed swap 继续通过 request-local `swapJointConstraintJcs()` 交换整个 `AssemblyJointReference`，focused test 已证明 reference / connector / marker / resolver evidence 与 solver DTO 同步换位。
- RackPinion / Screw special rewrite 未被普通 resolver 覆盖；RackPinion rewrite 仍只消费 resolver 产出的 `markerPlacement`。

## blocker 状态

| blocker | S3 裁决 | 剩余边界 |
| --- | --- | --- |
| `MP-BLOCK-001` | S3 已完成 resolver DTO diagnostic 前置：缺 Reference / missing object 会进入稳定 marker resolution status。 | S5 仍需用真实实现和 focused parity 完全关闭 ordinary dispatch。 |
| `MP-BLOCK-002` | S3 已部分关闭：subshape refs 不再把 connector `PlacementN` 冒充为 resolved marker。 | S4/S5 仍需 native expected 和真实 object/subshape global placement 证据。 |
| `MP-BLOCK-003` | S3 已部分关闭：part-local / `offsetPlc` 缺证据时会输出稳定 diagnostic。 | S5 仍需实现或继续显式诊断 containing-part / offset 边界。 |
| `MP-BLOCK-005` | S3 已完成 request-local swap evidence 前置，focused DTO test 证明交换后 reference / connector / marker evidence 同步。 | S4 仍需 current value / native oracle，S5 仍需 parity。 |
| `MP-BLOCK-010` | S3 rg 审计未在 resolver 文件引入 fixture 名称、bbox heuristic 或输出端修剪。 | S5/S6 切换真实 subshape placement 后仍需复审。 |

S3 不关闭 `MP-BLOCK-004/006/007/008/009`，不采 FreeCAD expected，不发布 C ABI capability，不把 S4-S6 或 native oracle parity 写成 supported。

## 验收

```bash
cmake --build cad-core/build
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k marker
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_distance_type_reference_classification_exposes_solver_dto cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_rackpinion_marker_rewrite_exposes_pitch_radius
rg -n "connectorPlacement|markerPlacement|fallback|bbox|fixture" cad-core/src/assembly/joint_solver.cpp cad-core/include/cad_core/assembly/joint_solver.h
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

本轮验证结论：

- `cmake --build cad-core/build` 通过。
- `python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k marker` 通过。
- 指定 DistanceType DTO 与 RackPinion rewrite 两个 focused tests 通过。
- S3 未执行 native FreeCAD expected 采集。

## 非目标

- 不采集 expected。
- 不发布 C ABI capability。
- 不实现 radius-bearing DistanceType。
