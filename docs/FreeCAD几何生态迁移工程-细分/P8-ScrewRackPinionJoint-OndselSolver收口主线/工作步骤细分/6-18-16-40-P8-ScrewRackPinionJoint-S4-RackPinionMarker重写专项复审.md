# P8 ScrewRackPinionJoint S4 RackPinion Marker 重写专项复审

## 目标

关闭 `SRJ-BLOCK-003` 和 `SRJ-BLOCK-004`：确认 RackPinion 既能映射 `ASMTRackPinionJoint(pitchRadius=Distance)`，又能按 FreeCAD `getRackPinionMarkers()` 重写 rack marker placement。

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
```

## 非目标

- S4 不实现 Screw native expected 或 capability publication。
- S4 不把 rack / pinion placement 写回前端长期状态。
- S4 不用 fixture 名称、component 名称或几何 bbox 推断 rack side。
