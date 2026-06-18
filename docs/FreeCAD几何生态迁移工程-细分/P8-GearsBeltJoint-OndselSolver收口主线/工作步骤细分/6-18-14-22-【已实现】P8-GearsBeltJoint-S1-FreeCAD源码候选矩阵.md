# P8 GearsBeltJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明 Gears / Belt 是可以单独实现的 `ASMTGearJoint` 子集。

当前状态：已完成。S1 只关闭 source authority 与候选矩阵复核，不关闭 `Distance2` DTO、native oracle、capability 发布或 supported 声明。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `RackPinion`、`Screw`、`Gears`、`Belt` 在 `enum class JointType` 中按该顺序相邻 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointUsingDistance` 包含 RackPinion / Screw / Gears / Belt；`JointUsingDistance2` 只包含 Gears / Belt；`JointNoNegativeDistance` 是 GUI 输入下限证据 |
| MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | Gears / Belt 都创建 `ASMTGearJoint`；Gears 设置 `radiusI=Distance`、`radiusJ=Distance2`，Belt 设置 `radiusJ=-Distance2` |
| marker 路径 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 只有 RackPinion 特判；Gears / Belt 走通用 `handleOneSideOfJoint` |
| Ondsel joint | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTGearJoint.h` | 暴露 `radiusI`、`radiusJ`、`aConstant` |
| RackPinion / Screw 边界 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType(); makeMbdJoint()` | RackPinion 使用 `ASMTRackPinionJoint` 并在 `makeMbdJoint()` 走特殊 marker；Screw 使用 `ASMTScrewJoint`、`slidingPartIndex()` 与 `swapJCS()`，不属于本包 |

## 扫描轴

- Gears / Belt 是否只需要 JointType string、Reference1 / Reference2、Placement1 / Placement2、Distance、Distance2。
- Belt 是否只和 Gears 在 `radiusJ` 符号上不同。
- `JointNoNegativeDistance` 是否属于 GUI 输入下限，不是 cad-core solver 支持前置。
- Existing c3m6 grounded two-component pattern 是否可复用。

## 复核结论

- `AssemblyUtils.h` 与 `JointObject.py` 的类型顺序一致，Gears / Belt 跟在 RackPinion / Screw 之后。
- Gears / Belt 的 FreeCAD MBD factory 是独立的 `ASMTGearJoint` 子集，差异只在 Belt 对 `Distance2` 取负后写入 `radiusJ`。
- `makeMbdJoint()` 只对 RackPinion 改写 marker；Gears / Belt 使用现有 Reference / Placement 两侧绑定路径。
- `JointNoNegativeDistance` 只支撑 GUI/input-boundary 判断，不是 cad-core solver blocker。
- cad-core 当前仍缺 `JointConstraint.distance2`、Gears / Belt 的 `makeOndselJointOfType()` 映射和 supported predicate；C API 仍把 RackPinion / Screw / Gears / Belt 一起发布为 unsupported。
- 因此 S1 只证明 Gears / Belt 可作为本包后续实现候选；RackPinion / Screw 保持本包之外，不能在 S1 改为 supported。

## 必须回写的矩阵行

- `GBJ-CAND-001` 到 `GBJ-CAND-012` 必须完成 evidence 复核。
- `GBJ-SCOPE-001` 到 `GBJ-SCOPE-008` 必须由 S2 路由。

## 验收标准

- `p8_gears_belt_joint_source_candidates.tsv` 每行都有 source file、symbol、evidence、cad-core landing。
- 至少包含 `AssemblyObject::makeMbdJointOfType`、`JointObject.py::JointUsingDistance2`、`ASMTGearJoint.h`、`cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`、c3m6 fixture / expected route。
- 检查命令：

```bash
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/p8_gears_belt_joint_source_candidates.tsv
rg -n "ASMTGearJoint|JointType::Gears|JointType::Belt|Distance2|radiusI|radiusJ|Gears|Belt" src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/JointObject.py src/3rdParty/OndselSolver/OndselSolver/ASMTGearJoint.h cad-core/include/cad_core/assembly/joint_solver.h cad-core/src/assembly/joint_solver.cpp
```

## 非目标

- S1 不推广 RackPinion / Screw。
- S1 不把 FreeCAD GUI distance lower-bound rules 当 cad-core solver support blocker。
- S1 不从 fixture bbox 倒推 solver 语义。
