# P8 Assembly Joint Placement / OndselSolver 收敛主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly solver 专项主线，来源于 `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md` 与 `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md` 中 Assembly 求解器、Joint placement、placement writeback 和完整 solver 发布边界。

## 主线目标

- 复核当前 `cad-core` 中 Assembly solver DTO、Ondsel adapter、representative fallback、placement writeback 和 diagnostics 的 live 状态。
- 把正式 P8 文档里的 Assembly solver 发布口径，与当前 C++、C ABI capabilities、fixtures 和 focused tests 的 build-mode aware 行为对齐。
- 对缺 FreeCAD native oracle 的 solver placement 子集保持 `notCollected`，不从 cad-core 当前输出倒推 golden。
- 对复杂 JointType 和完整 Assembly 事务生命周期做公开边界：能从 FreeCAD 源码和现有 cad-core 证据定义 request-local 语义的，进入 S6 C++ / focused tests；不能定义的保持 `unsupported` 或 `nonGoal`。

## 当前基线

- `cad-core/src/assembly/joint_solver.cpp` 已存在 request-local `AssemblySolveRequest`、representative adapter、可选 Ondsel adapter、grounded validation 和 placement update 输出。
- `cad-core/src/assembly/assembly_utils.cpp` 已把 solver result 转为 `solver_adapter` 元数据和 `documentObjectUpdates.action=assembly_set_placement`。
- `cad-core/CMakeLists.txt` 已按 `src/3rdParty/OndselSolver` 是否存在切换 `CAD_CORE_HAS_ONDSEL_SOLVER`，并链接 `OndselSolver`。
- `cad-core/tests/test_p8_features.py` 已覆盖 P8 Assembly input metadata、P8 representative writeback、C3M6 grounded real solver、multi-component writeback、invalid grounded result rejection和 unsupported joint diagnostics。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `assembly` capabilities 已按 build mode 发布 `representative_solver_adapter`、`ondsel_solver_adapter`、`placement_writeback` 和 `unsupported_joint_matrix` 子集；P8 正式文档已由 S6 回写。

## 证明链条

```text
声明口径
  -> FreeCAD Assembly / Joint 源码候选
  -> scope review / nonGoal / blocker queue
  -> Ondsel solver adapter 专项复审
  -> placement writeback 生命周期专项复审
  -> JointType 覆盖与 unsupported 矩阵专项复审
  -> oracle / C++ implementation / docs publication
  -> P8 Assembly 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Assembly solve 主流程 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`jointParts()`、`runPreDrag()`、`setNewPlacements()` |
| grounded validation | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::validateNewPlacements()` | grounded object moved 时拒绝 bad solve |
| placement writeback | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::setNewPlacements()` | solver 后写回对象 `Placement` |
| MBD assembly | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdAssembly()` | 创建 `OndselAssembly` |
| grounded part | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::fixGroundedPart()` | 用 `ASMTFixedJoint` 固定 grounded part |
| JointType 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()` | Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Parallel / Perpendicular / Angle / RackPinion / Screw / Gears / Belt |
| Joint Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::Joint` | `JointType`、`Reference1`、`Reference2`、`Placement1`、`Placement2`、Distance / Angle / hidden XLinkSub |
| JointGroup child 过滤 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/JointGroup.cpp::JointGroup::getJoints()` | 跳过 suppressed / grounded，收集有 joint connector proxy 的 child |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Assembly executor | `cad-core/src/assembly/assembly_object.cpp` | Assembly grouped display、joint groups、solver summary 发布 |
| Joint input | `cad-core/src/assembly/joint_group.cpp`; `cad-core/src/assembly/assembly_utils.cpp` | Joint / GroundedJoint 元数据、JointGroup children、solver input JSON |
| Solver DTO / adapter | `cad-core/include/cad_core/assembly/joint_solver.h`; `cad-core/src/assembly/joint_solver.cpp` | request-local solve request、representative fallback、real Ondsel adapter、validation |
| Link / placement source | `cad-core/src/assembly/assembly_link.cpp`; `cad-core/src/app/link.cpp`; `cad-core/src/app/property_links.cpp` | AssemblyLink display、XLink / hidden reference 解析、placement source |
| Capabilities | `cad-core/src/adapters/c_api/c_api.cpp` | Assembly solver capability publication |
| Tests / fixtures | `cad-core/tests/test_p8_features.py`; `cad-core/fixtures/p8`; `cad-core/fixtures/c3m6` | P8 / C3M6 focused solver and writeback coverage |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-17-21-18-【已实现】P8-AssemblySolver工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-17-21-19-【已实现】P8-AssemblySolver-S0-声明口径与live基线复核.md` | 已完成：冻结 live 口径、状态词典、禁止声明和 publication drift |
| S1 源码候选 | `工作步骤细分/6-17-21-20-【已实现】P8-AssemblySolver-S1-FreeCAD源码候选矩阵.md` | 已完成：生成 FreeCAD / cad-core source candidates，补齐 P8ASM-CAND-001..018 |
| S2 范围准入 | `工作步骤细分/6-17-21-21-【已实现】P8-AssemblySolver-S2-范围准入与blocker矩阵.md` | 已完成：分类 scope、blocker、releaseGate、notCollected、unsupported 和 nonGoal；未创建无证据 backendGap |
| S3 Ondsel adapter | `工作步骤细分/6-17-21-22-【已实现】P8-AssemblySolver-S3-OndselSolver适配专项复审.md` | 已完成：复核 real solver / representative fallback / build link 边界，S6 已按 build mode 收口 |
| S4 writeback | `工作步骤细分/6-17-21-23-【已实现】P8-AssemblySolver-S4-PlacementWriteback生命周期专项复审.md` | 已完成：复核 `documentObjectUpdates`、grounded validation、下一请求应用、多组件顺序；S6 已按 linked / unlinked route 收口 |
| S5 JointType | `工作步骤细分/6-17-21-24-【已实现】P8-AssemblySolver-S5-JointType覆盖与unsupported矩阵专项复审.md` | 已完成：裁决 Fixed / Revolute / Slider / Ball / Distance / Angle releaseGate 子集、diagnostic-only JointType 和 S6 扩展顺序 |
| S6 发布闸门 | `工作步骤细分/6-17-21-25-【已实现】P8-AssemblySolver-S6-Oracle实现与发布闸门.md` | 已完成：消费 blocker，落 C++ / expected / docs 发布 |
| source candidates | `矩阵/p8_assembly_solver_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_assembly_solver_scope_review_matrix.tsv` | 语义项状态矩阵 |
| blocker queue | `矩阵/p8_assembly_solver_blocker_queue.tsv` | 可执行 blocker 队列 |
| non-goal registry | `矩阵/p8_assembly_solver_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| backend gap classification | `矩阵/p8_assembly_solver_backend_gap_classification.tsv` | backendGap / releaseGate / unsupported 聚合 |

当前工作步骤总入口只是索引，已完成创建；S0 已完成 live 基线复核并确认 publication drift 已收敛：正式 P8 文档、C ABI capabilities、focused tests、fixtures 与当前 C++ 现在都按 build-mode aware 口径发布 request-local solver 子集。S1 已完成源码候选矩阵补证，只建立 source authority；S2 已完成范围准入与 blocker 矩阵裁决，把当前主线拆为 releaseGate、notCollected、unsupported 和 nonGoal，没有凭旧文档创建 backendGap。S3 已完成 Ondsel adapter 专项复审：real adapter 只在 `CAD_CORE_HAS_ONDSEL_SOLVER=1` 时可声明，unlinked build 只发布 representative fallback；S4 已完成 PlacementWriteback 生命周期专项复审：writeback 只作为 `documentObjectUpdates` 的 `Placement` 更新建议，下一请求由前端 graph 消费；S5 已完成 JointType 裁决：当前 releaseGate 子集只有 Fixed / Revolute / Slider / Ball / Distance / Angle，Cylindrical / Parallel / Perpendicular / RackPinion / Screw / Gears / Belt 保持 diagnostic-only，复杂 Distance geometry 仍 `notCollected`。S6 已完成收口：当前 code / tests / docs / matrices 一致，native FreeCAD solver placement oracle 仍未入库。
