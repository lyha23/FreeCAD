# P8 ScrewRackPinionJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明 Screw / RackPinion 是可以一起实现的 remaining special JointType 子集，并明确 shared sliding 前置和 RackPinion marker rewrite 风险。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `RackPinion` / `Screw` 在 `enum class JointType` 中 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointUsingDistance` 包含 RackPinion / Screw；`Distance` UI 文案说明 pitch radius / thread pitch |
| Screw MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `ASMTScrewJoint`，`pitch=getJointDistance(joint)` |
| RackPinion MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `ASMTRackPinionJoint`，`pitchRadius=getJointDistance(joint)` |
| shared sliding | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::slidingPartIndex()` | 扫描 Slider joint，比较 JCS pitch / roll，返回 1 / 2 / 0 |
| JCS swap | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::swapJCS()` | Screw / RackPinion 在 sliding side 不是第一侧时交换 JCS |
| RackPinion marker | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::getRackPinionMarkers()` | rack marker Z 轴跟 pinion 对齐，X 轴沿 sliding axis |
| Ondsel joints | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTScrewJoint.h`、`ASMTRackPinionJoint.h` | `pitch` / `pitchRadius` |

## 扫描轴

- Screw / RackPinion 是否共用 `Distance` scalar，而不需要 `Distance2`。
- `slidingPartIndex()` 是否能从 request-local `JointConstraint` 和同一 Assembly 内 Slider joint 证据计算。
- `swapJCS()` 是否应作为 request DTO 交换，而不是改写持久 DocumentObject graph。
- RackPinion marker rewrite 是否必须在创建 Ondsel marker 前完成，不能靠 solver output 后处理。
- Existing c3m6 grounded two-component pattern 是否足以构造 Slider + Screw / Slider + RackPinion fixtures。

## 必须回写的矩阵行

- `SRJ-CAND-001` 到 `SRJ-CAND-013` 必须完成 evidence 复核。
- `SRJ-SCOPE-001` 到 `SRJ-SCOPE-008` 必须由 S2 路由。

## 验收标准

- `p8_screw_rackpinion_joint_source_candidates.tsv` 每行都有 source file、symbol、evidence、cad-core landing。
- 至少包含 `AssemblyObject::makeMbdJointOfType`、`slidingPartIndex`、`swapJCS`、`getRackPinionMarkers`、`ASMTScrewJoint.h`、`ASMTRackPinionJoint.h`、`cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`、c3m6 fixture / expected route。
- 检查命令：

```bash
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵/p8_screw_rackpinion_joint_source_candidates.tsv
rg -n "ASMTScrewJoint|ASMTRackPinionJoint|JointType::Screw|JointType::RackPinion|slidingPartIndex|swapJCS|getRackPinionMarkers|pitchRadius|pitch" src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/JointObject.py src/3rdParty/OndselSolver/OndselSolver/ASMTScrewJoint.h src/3rdParty/OndselSolver/OndselSolver/ASMTRackPinionJoint.h cad-core/src/assembly/joint_solver.cpp cad-core/src/adapters/c_api/c_api.cpp
```

## 非目标

- S1 不把 complex DistanceType 当成本包候选。
- S1 不把 GUI labels、Reverse UI 或 drag lifecycle 当 cad-core solver support 前置。
- S1 不从 fixture bbox 倒推 solver 语义。
