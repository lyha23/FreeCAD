# P8 CylindricalJoint OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly 后续收口实施包。它只处理 `Cylindrical` JointType 从后续 unsupported 队列到可构建、可验收、可发布的最小闭环。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md` 和 `docs/CADCore3.0/06-C3-M8后续收口清单.md`。

## 主线目标

- 把 `Cylindrical` JointType 的 FreeCAD `ASMTCylindricalJoint` 语义收口到 `cad-core` real Ondsel adapter。
- 保证 `cad-core` 构建依赖、fixture、native expected、capabilities、focused tests 和文档矩阵口径一致。
- 把 remaining unsupported matrix 保持在 Parallel / Perpendicular / RackPinion / Screw / Gears / Belt，不扩大到完整 Joint 事务或 GUI drag 生命周期。

## 当前基线

- `cad-core/src/assembly/joint_solver.cpp` 已把 `Cylindrical` 映射到 `MbD::ASMTCylindricalJoint::With()`，并纳入 real Ondsel supported predicate。
- `cad-core/src/adapters/c_api/c_api.cpp` 已发布 `supported_joint_matrix`，`Cylindrical` 只出现在 supported 列表；remaining unsupported matrix 保持 Parallel / Perpendicular / RackPinion / Screw / Gears / Belt。
- `cad-core/fixtures/c3m6/assembly-grounded-cylindrical-joint-real-solver.json` 和对应 FreeCADCmd expected 已入库，并通过 native collector `--check` 与 cad-core expected parity。
- `src/3rdParty/OndselSolver` 已初始化到仓库记录的 `30e9b64e8bf881d438d4b88834f9ba3674865418`，`cmake --build build --target cad-core cad_core_ffi` 已通过；不引入 unlinked fallback。
- 既有 P8 AssemblySolver 主线仍定义 real-only OndselSolver、request-local placement writeback 和 remaining unsupported JointType 边界；本包只做 Cylindrical 最小收口，不重写 P8 主线。

## 证明链条

```text
声明口径
  -> FreeCAD JointType / makeMbdJointOfType 源码候选
  -> scope review / blocker queue
  -> Ondsel 子模块与构建闸门
  -> Cylindrical JointType 映射专项复审
  -> native oracle / capability 发布专项复审
  -> code landing and gate closure
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| solver 顺序 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()` | `fixGroundedParts()` 后 `jointParts(joints)`、`mbdAssembly->runPreDrag()`、`setNewPlacements()` |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::JointType` | `Cylindrical` 位于 `Revolute` 和 `Slider` 之间，需与 `JointObject.py` 同序 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::JointTypes` | `JointTypes` 包含 `Cylindrical`，并声明 Reverse、LengthLimit、AngleLimit、PreSolve 边界 |
| MBD joint 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `case JointType::Cylindrical` 返回 `CREATE<ASMTCylindricalJoint>::With()` |
| placement 写回 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::setNewPlacements()` | solver 后写回对象 `Placement`，cad-core 只能输出 request-local `documentObjectUpdates` 建议 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO / adapter | `cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp` | 解析 JointConstraint、映射 `Cylindrical -> ASMTCylindricalJoint`、调用 real Ondsel `runPreDrag()` |
| runtime publication | `cad-core/src/assembly/assembly_object.cpp`、`cad-core/src/assembly/assembly_utils.cpp` | 输出 solver summary 和 `documentObjectUpdates.action=assembly_set_placement` |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 supported / unsupported JointType matrix 和 real-only solver 能力 |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 focused runtime、C ABI contract 和 expected parity |
| build gate | `cad-core/CMakeLists.txt`、`src/3rdParty/OndselSolver` | 保证 `CAD_CORE_HAS_ONDSEL_SOLVER=1` hard-linked 构建存在 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-12-21-P8-CylindricalJoint工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-18-12-22-【已实现】P8-CylindricalJoint-S0-声明口径与live基线复核.md` | 已实现：冻结 claims、非目标和构建闸门 |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-12-23-【已实现】P8-CylindricalJoint-S1-FreeCAD源码候选矩阵.md` | 已实现：建立 source authority 和 candidate TSV |
| S2 范围准入 | `工作步骤细分/6-18-12-24-【已实现】P8-CylindricalJoint-S2-范围准入与blocker矩阵.md` | 已实现：把候选路由到 supported / unsupported / nonGoal |
| S3 构建闸门 | `工作步骤细分/6-18-12-25-【已实现】P8-CylindricalJoint-S3-Ondsel子模块与构建闸门专项复审.md` | 已实现：收口 Ondsel 子模块和 hard-linked build |
| S4 映射复审 | `工作步骤细分/6-18-12-26-【已实现】P8-CylindricalJoint-S4-JointType映射专项复审.md` | 已实现：验证 `ASMTCylindricalJoint` adapter 和 supported matrix |
| S5 oracle / capability | `工作步骤细分/6-18-12-27-【已实现】P8-CylindricalJoint-S5-NativeOracle与capability发布专项复审.md` | 已实现：对齐 native expected、tests、capabilities 和 docs |
| S6 发布闸门 | `工作步骤细分/6-18-12-28-【已实现】P8-CylindricalJoint-S6-Oracle实现与发布闸门.md` | 已实现：关闭发布闸门并列出验收证据 |
| source candidates | `矩阵/p8_cylindrical_joint_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_cylindrical_joint_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_cylindrical_joint_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_cylindrical_joint_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_cylindrical_joint_backend_gap_classification.tsv` | supported / unsupported / nonGoal 分类 |

当前 S0-S6 均已实现；矩阵是发布闸门结论。验收已覆盖 hard-linked build、P8 focused tests、C ABI capability test、Cylindrical expected parity、native collector `--check`、TSV 字段一致性和 `git diff --check`。
