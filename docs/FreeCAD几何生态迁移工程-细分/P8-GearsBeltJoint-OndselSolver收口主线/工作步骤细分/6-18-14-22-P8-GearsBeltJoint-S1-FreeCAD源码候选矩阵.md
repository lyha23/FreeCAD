# P8 GearsBeltJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明 Gears / Belt 是可以单独实现的 `ASMTGearJoint` 子集。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `Gears` / `Belt` 在 `enum class JointType` 中 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointUsingDistance` 包含 Gears / Belt；`JointUsingDistance2` 只包含 Gears / Belt |
| MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `ASMTGearJoint`，`radiusI` / `radiusJ` |
| marker 路径 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 只有 RackPinion 特判；Gears / Belt 走通用 `handleOneSideOfJoint` |
| Ondsel joint | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTGearJoint.h` | `radiusI = 0.0, radiusJ = 0.0` |

## 扫描轴

- Gears / Belt 是否只需要 JointType string、Reference1 / Reference2、Placement1 / Placement2、Distance、Distance2。
- Belt 是否只和 Gears 在 `radiusJ` 符号上不同。
- `JointNoNegativeDistance` 是否属于 GUI 输入下限，不是 cad-core solver 支持前置。
- Existing c3m6 grounded two-component pattern 是否可复用。

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
