# P8 ScrewRackPinionJoint OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly 后续收口实施包。它同时处理 `Screw` 和 `RackPinion` 两个 remaining special JointType，把当前 `unsupported_joint_matrix` 中最后两个 JointType 推进到 request-local real Ondsel 支持子集。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线` 和 `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 迁移 FreeCAD `Screw -> ASMTScrewJoint(pitch=Distance)` 的最小 request-local 语义，并实现其依赖的 `slidingPartIndex()` / `swapJCS()` 等价判断。
- 迁移 FreeCAD `RackPinion -> ASMTRackPinionJoint(pitchRadius=Distance)` 的最小 request-local 语义，并实现 `getRackPinionMarkers()` 中 rack / pinion 侧识别与 rack marker 旋转重写。
- 为 Screw / RackPinion 增加 c3m6 request-local fixture、FreeCADCmd native expected、focused runtime test 和 C ABI capability 发布。
- 保持 complex `DistanceType` geometry、GUI drag / postDrag、Reverse UI、跨请求 solver session 和完整 Assembly transaction lifecycle 继续在本包之外。

## 当前基线

- 当前 supported subset 是 `Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle / Gears / Belt`；`unsupported_joint_matrix` 只剩 `RackPinion / Screw`。
- FreeCAD `AssemblyObject::makeMbdJointOfType()` 对 Screw 创建 `ASMTScrewJoint` 并设置 `pitch=Distance`，但要求 `slidingPartIndex(joint) != 0`，必要时执行 `swapJCS(joint)`。
- FreeCAD `AssemblyObject::makeMbdJointOfType()` 对 RackPinion 创建 `ASMTRackPinionJoint` 并设置 `pitchRadius=Distance`；`makeMbdJoint()` 对 RackPinion 走 `getRackPinionMarkers()`，不是普通 `handleOneSideOfJoint()`。
- `cad-core/src/assembly/joint_solver.cpp::isSupportedOndselJointType()` 当前不包含 RackPinion / Screw；`cad-core/src/adapters/c_api/c_api.cpp` 当前仍把它们发布在 `unsupported_joint_matrix`。
- `cad-core` 已有 Slider、Distance、Gears/Belt 的 scalar DTO、real Ondsel adapter、fixture / expected 和 capability 发布路径，可复用为本包的输入输出骨架。

## 证明链条

```text
声明口径
  -> FreeCAD Screw / RackPinion 源码候选
  -> scope review / nonGoal / blocker queue
  -> slidingPartIndex 与 swapJCS 共享前置
  -> Screw pitch / RackPinion marker rewrite 专项复审
  -> native oracle / capability / unsupported matrix 发布复审
  -> code landing for implementable unsupported
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::JointType` | `RackPinion` / `Screw` 位于 `Angle` 之后、`Gears` / `Belt` 之前 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::JointUsingDistance` | `RackPinion` / `Screw` 使用 `Distance`，但不使用 `Distance2` |
| Screw 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Screw` 要求 `slidingPartIndex()`，必要时 `swapJCS()`，创建 `ASMTScrewJoint` 并设置 `pitch` |
| RackPinion 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `RackPinion` 创建 `ASMTRackPinionJoint` 并设置 `pitchRadius` |
| RackPinion marker | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::getRackPinionMarkers()` | 识别 rack / pinion 侧，rack marker 的 Z 轴对齐 pinion，X 轴对齐 sliding axis |
| sliding 依赖 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::slidingPartIndex()` | 扫描同一 Assembly 的 Slider joint，并比较 pitch / roll 判断 sliding side |
| Ondsel joints | `/home/user/Chili3DProject/FreeCAD/src/3rdParty/OndselSolver/OndselSolver/ASMTScrewJoint.h`、`ASMTRackPinionJoint.h` | 分别暴露 `pitch`、`pitchRadius` 和 `aConstant` |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO | `cad-core/include/cad_core/assembly/joint_solver.h` | 扩展 request-local JointConstraint / helper，承载 sliding side、Distance pitch / pitchRadius 和 marker rewrite 所需证据 |
| request builder | `cad-core/src/assembly/joint_solver.cpp` | 从同一 Assembly request 中扫描 Slider joint，计算 `slidingPartIndex`，并为 Screw / RackPinion 填充 solver input |
| real Ondsel adapter | `cad-core/src/assembly/joint_solver.cpp` | 映射 `Screw` 到 `MbD::ASMTScrewJoint::With()`，映射 `RackPinion` 到 `MbD::ASMTRackPinionJoint::With()` |
| marker / placement | `cad-core/src/assembly/joint_solver.cpp` | 为 RackPinion 实现 request-local rack marker 旋转重写；不能在 adapter 输出端补猜测 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 输出 Screw / RackPinion solver_joints 字段、native solver return 和 placement_updates oracle |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 supported / unsupported JointType matrix 和 covered keys |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 focused runtime、C ABI contract 和 expected parity |
| upstream docs | `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` | 回写 P8ASM-SCOPE-007 和 unsupported matrix 口径 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-16-35-【已实现】P8-ScrewRackPinionJoint工作步骤总入口.md` | S0-S6 执行索引，已完成索引校验 |
| S0 声明口径 | `工作步骤细分/6-18-16-36-【已实现】P8-ScrewRackPinionJoint-S0-声明口径与live基线复核.md` | 冻结 claims、非目标和 current unsupported 基线 |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-16-37-【已实现】P8-ScrewRackPinionJoint-S1-FreeCAD源码候选矩阵.md` | 已建立 source authority 和 candidate TSV |
| S2 范围准入 | `工作步骤细分/6-18-16-38-【已实现】P8-ScrewRackPinionJoint-S2-范围准入与blocker矩阵.md` | 已将候选路由到 implementable unsupported、notCollected、releaseGate、nonGoal |
| S3 sliding 前置复审 | `工作步骤细分/6-18-16-39-P8-ScrewRackPinionJoint-S3-SlidingAxis与swapJCS专项复审.md` | 收口 `slidingPartIndex()` / `swapJCS()` 共享前置 |
| S4 RackPinion marker 复审 | `工作步骤细分/6-18-16-40-P8-ScrewRackPinionJoint-S4-RackPinionMarker重写专项复审.md` | 收口 RackPinion rack / pinion marker rewrite |
| S5 oracle / 发布复审 | `工作步骤细分/6-18-16-41-P8-ScrewRackPinionJoint-S5-NativeOracle与Capability专项复审.md` | 收口 fixtures、FreeCADCmd expected、focused tests 和 capability |
| S6 发布闸门 | `工作步骤细分/6-18-16-42-P8-ScrewRackPinionJoint-S6-Oracle实现与发布闸门.md` | 指定代码落点、验收命令和禁止路径 |
| source candidates | `矩阵/p8_screw_rackpinion_joint_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_screw_rackpinion_joint_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_screw_rackpinion_joint_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_screw_rackpinion_joint_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_screw_rackpinion_joint_backend_gap_classification.tsv` | unsupported / notCollected / releaseGate / nonGoal 分类 |

当前工作步骤索引已完成校验；S0 已完成 live 基线复核；S1 已完成 source candidates 复核；S2 已完成范围准入与 blocker 队列路由；S3-S6 仍为 `待执行`。矩阵是后续路由输入，不是发布闸门结论。
