# 【已实现】P8 ScrewRackPinionJoint S4 RackPinion Marker 重写专项复审

## 目标

关闭 `SRJ-BLOCK-003` 和 `SRJ-BLOCK-004`：确认 RackPinion 既能映射 `ASMTRackPinionJoint(pitchRadius=Distance)`，又能按 FreeCAD `getRackPinionMarkers()` 重写 rack marker placement。

## S4 复核结论

- `cad-core/src/assembly/joint_solver.cpp` 已新增 RackPinion request-local marker rewrite：复用 S3 的 `slidingPartIndex` / DTO `swapJCS` 结果，让 rack 位于 I 侧，重写 `reference1.markerPlacement` 后再进入 `addConstraintToOndselAssembly()`。
- `makeOndselJointOfType()` 已映射 `RackPinion -> MbD::ASMTRackPinionJoint::With()`，并设置 `pitchRadius=Distance`；S4 仍不实现 Screw。
- `solver_joints` JSON 已暴露 `distance`、`pitch_radius` 与 `rack_pinion_marker_rewrite` 摘要，focused test 可验证 rack / pinion side、yaw adjustment 和重写后的 rack marker placement。
- 缺少 Slider 前置的 RackPinion 继续输出 `unsupported_assembly_solver`，不进入 fake solve；RackPinion marker rewrite 不写回前端 DocumentObject graph。
- S4 未修改 C ABI capabilities、supported / unsupported publication matrix、native expected、上游 `P8ASM-SCOPE-007` 或 complex Distance scope。

## 代码落点

| 落点 | S4 行为 |
| --- | --- |
| `cad-core/include/cad_core/assembly/joint_solver.h` | 新增 `pitchRadius` 与 `RackPinionMarkerRewrite` request-local 证据字段 |
| `cad-core/src/assembly/joint_solver.cpp` | 实现 `ASMTRackPinionJoint(pitchRadius=Distance)` conversion、RackPinion marker rewrite 和 Slider precondition gate |
| `cad-core/src/assembly/assembly_utils.cpp` | `solver_joints` 输出 `pitch_radius` 与 marker rewrite 摘要 |
| `cad-core/fixtures/c3m6/assembly-rackpinion-marker-rewrite-real-solver.json` | RackPinion-only focused runtime fixture，覆盖 side=2 DTO swap 与 marker rewrite |
| `cad-core/tests/test_p8_features.py` | 锁定缺 Slider diagnostic、S3 DTO swap 回归、S4 RackPinion focused evidence |

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| RackPinion MBD mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | 创建 `ASMTRackPinionJoint`，设置 `pitchRadius=getJointDistance(joint)` |
| RackPinion marker path | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 只有 RackPinion 进入 `getRackPinionMarkers()` |
| rack side | `src/Mod/Assembly/App/AssemblyObject.cpp::getRackPinionMarkers()` | `slidingPartIndex()` 决定 rack side，必要时 `swapJCS()` |
| rack marker rotation | `src/Mod/Assembly/App/AssemblyObject.cpp::getRackPinionMarkers()` | pinion Z 轴为 rack Z，rack X 轴沿 Slider Z 轴，并做 yaw adjustment |
| Ondsel field | `src/3rdParty/OndselSolver/OndselSolver/ASMTRackPinionJoint.h` | `pitchRadius` |

## scope 表

| scope | 复审结果要求 |
| --- | --- |
| `SRJ-SCOPE-004` | RackPinion conversion 设置 `pitchRadius`，marker rewrite 在 addConstraint 前完成 |
| `SRJ-SCOPE-002` | 使用 S3 的 sliding side / JCS ordering，不重复猜测 |
| `SRJ-SCOPE-007` | 不把 RackPinion marker rewrite 扩展成 complex Distance geometry |

## 必须回写的矩阵行

- `SRJ-CAND-004`
- `SRJ-CAND-007`
- `SRJ-CAND-008`
- `SRJ-CAND-011`
- `SRJ-BLOCK-003`
- `SRJ-BLOCK-004`

## 验收标准

- `cad-core/src/assembly/joint_solver.cpp` 包含 `ASMTRackPinionJoint` conversion 和 FreeCAD 依据注释。
- RackPinion solver_joints 输出 `distance` / `pitch_radius`，并能暴露 marker rewrite 诊断或摘要字段用于 focused test。
- Rack marker rewrite 发生在 Ondsel marker 创建前；不得通过 solver result 后处理或 adapter 输出修正替代。
- 检查命令：

```bash
rg -n "ASMTRackPinionJoint|pitchRadius|pitch_radius|getRackPinionMarkers|rack|pinion|yaw|marker" cad-core/src/assembly/joint_solver.cpp cad-core/src/assembly/assembly_utils.cpp cad-core/tests/test_p8_features.py
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_unsupported_joint_stays_diagnostic
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_screw_rackpinion_sliding_precondition_swaps_solver_dto
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_rackpinion_marker_rewrite_exposes_pitch_radius
```

## 非目标

- S4 不实现 Screw native expected 或 capability publication。
- S4 不把 rack / pinion placement 写回前端长期状态。
- S4 不用 fixture 名称、component 名称或几何 bbox 推断 rack side。
