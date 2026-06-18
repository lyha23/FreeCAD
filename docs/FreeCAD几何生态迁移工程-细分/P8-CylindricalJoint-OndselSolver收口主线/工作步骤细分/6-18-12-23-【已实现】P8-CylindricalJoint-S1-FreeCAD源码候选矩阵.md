# P8 CylindricalJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，限定 Cylindrical 的真实实现范围。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `enum class JointType` 包含 `Cylindrical` |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointTypes = ["Fixed", "Revolute", "Cylindrical", ...]` |
| MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Cylindrical: return CREATE<ASMTCylindricalJoint>::With();` |
| solver 执行 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::solve()` | `mbdAssembly->runPreDrag(); setNewPlacements();` |
| limit 边界 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | `Slider || Cylindrical` length limits；`Revolute || Cylindrical` angle limits |

## 扫描轴

- `JointType` 枚举顺序和 string value 是否与 `JointObject.py` 一致。
- `makeMbdJointOfType()` 是否只需一对一映射 `ASMTCylindricalJoint`，不牵涉 distance geometry。
- `makeMbdJoint()` 中 Cylindrical limit 语义是否本包纳入；默认 `notCollected`，不进入本轮支持声明。
- cad-core 是否在 `joint_solver.cpp`、capabilities、tests、fixtures 中同步。

## 必须回写的矩阵行

- `CYL-CAND-001` 到 `CYL-CAND-010` 必须完成 evidence 复核。
- `CYL-SCOPE-001` 到 `CYL-SCOPE-005` 必须由 S2 路由。
- `CYL-SCOPE-006` 必须保留为 nonGoal。

## 验收标准

- `p8_cylindrical_joint_source_candidates.tsv` 每行都有 source file、symbol、evidence、cad-core landing。
- 至少包含 `AssemblyObject::makeMbdJointOfType`、`AssemblyUtils.h::JointType`、`JointObject.py::JointTypes`、`cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`、c3m6 fixture / expected。
- 检查命令：

```bash
awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/p8_cylindrical_joint_source_candidates.tsv
rg -n "ASMTCylindricalJoint|JointType::Cylindrical|Cylindrical" src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/App/AssemblyUtils.h src/Mod/Assembly/JointObject.py cad-core/src/assembly/joint_solver.cpp
```

## 非目标

- S1 不推广 unsupported JointType。
- S1 不用 fixture bbox 倒推 solver 语义。
- S1 不把 Cylindrical limit 语义并入最小映射支持。
