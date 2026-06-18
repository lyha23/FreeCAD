# P8 GearsBeltJoint OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly 后续收口实施包。它只处理 `Gears` / `Belt` 两个基于 `ASMTGearJoint` 的 JointType，从 remaining unsupported matrix 推进到 real Ondsel request-local supported 子集。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-ParallelPerpendicularJoint-OndselSolver收口主线` 和 `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 把 FreeCAD `Gears -> ASMTGearJoint(radiusI=Distance, radiusJ=Distance2)` 和 `Belt -> ASMTGearJoint(radiusI=Distance, radiusJ=-Distance2)` 的最小 request-local 语义迁入 `cad-core` real Ondsel adapter。
- 为两个 JointType 增加 `Distance2` DTO、c3m6 request-local fixture、FreeCADCmd native expected、focused runtime test 和 C ABI capability 发布。
- 保持 `RackPinion`、`Screw`、复杂 `Distance` geometry、GUI drag / postDrag、跨请求 solver session 继续在本包之外。

## 当前基线

- 当前 supported subset 是 `Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle`；`unsupported_joint_matrix` 只剩 `RackPinion / Screw / Gears / Belt`。
- FreeCAD `AssemblyObject::makeMbdJointOfType()` 对 Gears / Belt 均创建 `ASMTGearJoint`，差异只在 `radiusJ` 符号。
- S3 已在 `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 增加 `distance2`，并为 Gears / Belt 读取 `Distance` / `Distance2`。
- S3 已在 `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` 将 Gears / Belt 映射到 `ASMTGearJoint`；S4 已补齐 Gears / Belt native expected、`radius_i` / `radius_j` solver output 与 focused parity。
- `cad-core/src/adapters/c_api/c_api.cpp` 当前仍把 `Gears` / `Belt` 发布在 `unsupported_joint_matrix`。

## 证明链条

```text
声明口径
  -> FreeCAD JointType / makeMbdJointOfType 源码候选
  -> scope review / nonGoal / blocker queue
  -> Distance2 DTO 与 ASMTGearJoint 映射专项复审
  -> native oracle 与半径符号专项复审
  -> capability / docs / unsupported matrix 专项复审
  -> code landing for implementable unsupported
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::JointType` | `Gears` / `Belt` 位于 `Screw` 之后 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::JointTypes` | `JointUsingDistance` 包含 Gears / Belt，`JointUsingDistance2` 只包含 Gears / Belt |
| Gears 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Gears` 返回 `CREATE<ASMTGearJoint>::With()`，设置 `radiusI=Distance`、`radiusJ=Distance2` |
| Belt 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Belt` 返回 `CREATE<ASMTGearJoint>::With()`，设置 `radiusI=Distance`、`radiusJ=-Distance2` |
| marker 绑定 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 只有 `RackPinion` 走 `getRackPinionMarkers()`，Gears / Belt 走通用 `handleOneSideOfJoint()` |
| Ondsel joint | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTGearJoint.h` | `ASMTGearJoint` 暴露 `radiusI`、`radiusJ`、`aConstant` |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO | `cad-core/include/cad_core/assembly/joint_solver.h` | 在 `JointConstraint` 增加 `distance2` |
| assembly request builder | `cad-core/src/assembly/joint_solver.cpp` | 为 Gears / Belt 读取 `Distance` / `Distance2` |
| real Ondsel adapter | `cad-core/src/assembly/joint_solver.cpp` | 映射 `Gears` / `Belt` 到 `MbD::ASMTGearJoint::With()` 并设置半径 |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 supported / unsupported JointType matrix 和 covered keys |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 focused runtime、C ABI contract 和 expected parity |
| upstream docs | `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` | 回写 P8ASM-SCOPE-007 和 unsupported matrix 口径 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-14-20-【已实现】P8-GearsBeltJoint工作步骤总入口.md` | S0-S6 执行索引，已完成索引校验 |
| S0 声明口径 | `工作步骤细分/6-18-14-21-【已实现】P8-GearsBeltJoint-S0-声明口径与live基线复核.md` | 冻结 claims、非目标和 current unsupported 基线，已完成 live baseline 复核 |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-14-22-【已实现】P8-GearsBeltJoint-S1-FreeCAD源码候选矩阵.md` | 建立 source authority 和 candidate TSV，已完成源码候选复核 |
| S2 范围准入 | `工作步骤细分/6-18-14-23-【已实现】P8-GearsBeltJoint-S2-范围准入与blocker矩阵.md` | 已完成候选到 unsupportedImplementable / notCollected / releaseGate / nonGoal 的路由 |
| S3 DTO / 映射复审 | `工作步骤细分/6-18-14-24-【已实现】P8-GearsBeltJoint-S3-Distance2与ASMTGearJoint映射专项复审.md` | 已收口 `Distance2` DTO 和 `ASMTGearJoint` adapter |
| S4 oracle 复审 | `工作步骤细分/6-18-14-25-【已实现】P8-GearsBeltJoint-S4-NativeOracle与半径符号专项复审.md` | 已收口 FreeCADCmd expected、`radiusJ` 符号和 request-local solver output |
| S5 发布复审 | `工作步骤细分/6-18-14-26-P8-GearsBeltJoint-S5-Capability与unsupported矩阵专项复审.md` | 同步 capabilities、tests、P8 docs / TSV |
| S6 发布闸门 | `工作步骤细分/6-18-14-27-P8-GearsBeltJoint-S6-Oracle实现与发布闸门.md` | 指定代码落点、验收命令和禁止路径 |
| source candidates | `矩阵/p8_gears_belt_joint_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_gears_belt_joint_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_gears_belt_joint_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_gears_belt_joint_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_gears_belt_joint_backend_gap_classification.tsv` | unsupported / notCollected / releaseGate / nonGoal 分类 |

当前工作步骤索引已完成校验；S0 已完成 live baseline 复核，S1 已完成 FreeCAD 源码候选矩阵复核，S2 已完成范围准入与 blocker 路由，S3 已完成 DTO / adapter code landing，S4 已完成 native oracle 与半径符号复核；S5-S6 仍为 `待执行`。矩阵已记录 S4 oracle 关闭，但不是 supported 发布闸门结论。
