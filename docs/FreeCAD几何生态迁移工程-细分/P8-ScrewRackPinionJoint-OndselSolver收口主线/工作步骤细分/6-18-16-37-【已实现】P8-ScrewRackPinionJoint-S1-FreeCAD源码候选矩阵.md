# 【已实现】P8 ScrewRackPinionJoint S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明 Screw / RackPinion 是可以一起实现的 remaining special JointType 子集，并明确 shared sliding 前置和 RackPinion marker rewrite 风险。

## S1 复核结论

- `p8_screw_rackpinion_joint_source_candidates.tsv` 已从 seed 更新为已复核候选矩阵，`SRJ-CAND-001` 到 `SRJ-CAND-014` 均包含 source file、symbol、evidence、cad-core landing、scope hint 和 next step。
- FreeCAD 依据确认：`RackPinion` / `Screw` 是 `JointType` 和 `JointUsingDistance` 成员；两者只使用 scalar `Distance`，不使用 `Distance2`；complex `DistanceType` 不进入本包支持声明。
- Screw 依据确认：`AssemblyObject::makeMbdJointOfType()` 先调用 `slidingPartIndex()`，失败返回空 joint，必要时 `swapJCS()`，再创建 `ASMTScrewJoint` 并写入 `pitch=getJointDistance(joint)`。
- RackPinion 依据确认：`makeMbdJointOfType()` 创建 `ASMTRackPinionJoint` 并写入 `pitchRadius=getJointDistance(joint)`；`makeMbdJoint()` 只对 RackPinion 调用 `getRackPinionMarkers()`；marker rewrite 必须在 Ondsel marker 创建前完成。
- cad-core 当前基线确认：`JointConstraint` 尚无 sliding side / swapped JCS / marker rewrite 证据字段；request builder 还不读取 Screw / RackPinion 的 `Distance`；`makeOndselJointOfType()` 和 supported predicate 仍不包含 Screw / RackPinion；C ABI 仍把两者发布在 `unsupported_joint_matrix`。
- c3m6 路线确认：当前 focused matrix 已覆盖 Gears / Belt scalar route；`assembly-unsupported-joint-diagnostic.json` 和 `test_c3m6_assembly_unsupported_joint_stays_diagnostic` 仍锁定 RackPinion unsupported 现状，后续 S5 才能新增 Screw / RackPinion real-solver fixtures 和 expected。
- S1 只建立 source authority 和 candidate hints；不关闭 P8ASM-SCOPE-007，不声明 supported，不采集 native oracle，不修改 C++。

## FreeCAD 依据

| 候选 | 源码 | 关键短句 / 字段 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h` | `enum class JointType` 中 `RackPinion` / `Screw` 位于 `Angle` 后、`Gears` / `Belt` 前，且注释要求和 `JointObject.py` 一致 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py` | `JointUsingDistance` 包含 RackPinion / Screw；`JointUsingDistance2` 只包含 Gears / Belt；UI 文案区分 `Thread pitch` 与 `Pitch radius` |
| Screw MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `slidingPartIndex()` -> optional `swapJCS()` -> `ASMTScrewJoint` -> `pitch=getJointDistance(joint)` |
| RackPinion MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `ASMTRackPinionJoint`，`pitchRadius=getJointDistance(joint)` |
| shared sliding | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::slidingPartIndex()` | 扫描同一 Assembly 的 Slider joint，比较 JCS pitch / roll，返回 1 / 2 / 0 |
| DTO swap | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp` 与 `AssemblyUtils.h::swapJCS()` | Screw / RackPinion 在 sliding side 不是第一侧时交换 JCS；cad-core 只能做 request-local DTO 顺序调整 |
| RackPinion marker | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::getRackPinionMarkers()` | rack marker 作为 I、pinion marker 作为 J；rack marker Z 轴跟 pinion 对齐，X 轴沿 sliding axis |
| Ondsel joints | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTScrewJoint.h`、`ASMTRackPinionJoint.h` | 分别暴露 `pitch` / `pitchRadius` 和 `aConstant` |

## 扫描轴

- `Distance` scalar 是 Screw `pitch` / RackPinion `pitchRadius` 的候选输入；`Distance2` 仍只属于 Gears / Belt。
- `slidingPartIndex()` 是 shared precondition；Screw / RackPinion 不能绕过 Slider 证据直接发布 supported。
- `swapJCS()` 在 cad-core 中应落到 request DTO 引用顺序，不应改写前端长期 DocumentObject graph。
- RackPinion marker rewrite 是独立路径，必须在 Ondsel marker 创建前完成，不能靠 solver output 后处理。
- c3m6 route 要分清当前 unsupported diagnostic fixture、未来 real-solver fixtures、expected collector 和 capability publication。

## 必须回写的矩阵行

- `SRJ-CAND-001` 到 `SRJ-CAND-014` 已完成 evidence 复核。
- `SRJ-SCOPE-001` 到 `SRJ-SCOPE-008` 必须由 S2 路由。

## 验收标准

- `p8_screw_rackpinion_joint_source_candidates.tsv` 每行都有 source file、symbol、evidence、cad-core landing、scope hint 和 next step。
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
- S1 不修改 C++、fixtures、native oracle 或 P8ASM-SCOPE-007。
