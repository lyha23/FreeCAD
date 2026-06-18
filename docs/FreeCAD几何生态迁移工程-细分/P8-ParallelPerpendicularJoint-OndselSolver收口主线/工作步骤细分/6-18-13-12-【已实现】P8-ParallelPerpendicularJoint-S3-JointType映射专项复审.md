# 【已实现】P8 ParallelPerpendicularJoint S3 JointType 映射专项复审

## 目标

关闭 `PPJ-BLOCK-001`：确认 cad-core real Ondsel adapter 按 FreeCAD 一对一映射 Parallel / Perpendicular，并更新 supported predicate。

## 复审结果

- `cad-core/src/assembly/joint_solver.cpp` 已 include `OndselSolver/ASMTPerpendicularJoint.h`。
- `makeOndselJointOfType()` 已按 FreeCAD direct mapping 增加 `joint.jointType == "Parallel"` 和 `joint.jointType == "Perpendicular"` 分支，分别返回 `MbD::ASMTParallelAxesJoint::With()` 与 `MbD::ASMTPerpendicularJoint::With()`。
- 新增相邻 FreeCAD 依据注释，指向 `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()`，并写明 direct case `JointType::Parallel` / `JointType::Perpendicular` 返回对应 ASMT。
- `isSupportedOndselJointType()` 已纳入 `Parallel` / `Perpendicular`，仍未纳入 `RackPinion`、`Screw`、`Gears`、`Belt`。
- 本步只关闭 direct adapter mapping blocker；native expected、focused fixture、C ABI capability 发布和 upstream unsupported matrix 回写仍留给 S4/S5/S6。

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| Parallel mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Parallel` 返回 `CREATE<ASMTParallelAxesJoint>::With()` |
| Perpendicular mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Perpendicular` 返回 `CREATE<ASMTPerpendicularJoint>::With()` |
| marker path | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 两者都不走 RackPinion 特殊 marker |

## scope 表

| scope | 复审结果要求 |
| --- | --- |
| `PPJ-SCOPE-002` | `makeOndselJointOfType()` 返回 `MbD::ASMTParallelAxesJoint::With()`，supported predicate 包含 `Parallel` |
| `PPJ-SCOPE-003` | `makeOndselJointOfType()` 返回 `MbD::ASMTPerpendicularJoint::With()`，supported predicate 包含 `Perpendicular` |
| `PPJ-SCOPE-006` | RackPinion / Screw / Gears / Belt 不新增 supported |

当前结论：`PPJ-SCOPE-002/003` 的 direct adapter mapping 已完成，但尚不能发布为完整 supported；`PPJ-SCOPE-004/005` 仍分别约束 native expected 与 capability/docs 发布。

## 必须回写的矩阵行

- `PPJ-CAND-003`
- `PPJ-CAND-004`
- `PPJ-CAND-006`
- `PPJ-BLOCK-001`

状态回写：`PPJ-CAND-003/004/006`、`PPJ-SCOPE-002/003` 和 `PPJ-BLOCK-001` 已更新为 S3 direct mapping 已落地 / 后续 S4-S5 gate 未关闭的措辞。

## 验收标准

- `cad-core/src/assembly/joint_solver.cpp` 新增或确认 `ASMTPerpendicularJoint` include。
- `makeOndselJointOfType()` 有 FreeCAD 依据注释，写明 Parallel / Perpendicular 源文件、函数和关键映射。
- `isSupportedOndselJointType()` 包含 Parallel / Perpendicular，仍不包含 RackPinion / Screw / Gears / Belt。
- 检查命令：

```bash
rg -n "ASMTParallelAxesJoint|ASMTPerpendicularJoint|Parallel|Perpendicular|RackPinion|Screw|Gears|Belt" cad-core/src/assembly/joint_solver.cpp
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
```

## 非目标

- 不实现 RackPinion / Screw / Gears / Belt。
- 不改 assembly DTO 字段，除非 focused fixture 证明 Reference / Placement 读取缺口。
- 不在 expected 或 adapter 中补业务语义。
