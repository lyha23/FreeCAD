# P8 ParallelPerpendicularJoint OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly 后续收口实施包。它只处理 `Parallel` / `Perpendicular` JointType 从 remaining unsupported matrix 到 real Ondsel request-local supported 子集的最小闭环。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线`、`docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 把 FreeCAD `Parallel -> ASMTParallelAxesJoint` 和 `Perpendicular -> ASMTPerpendicularJoint` 的直接 JointType 映射迁入 `cad-core` real Ondsel adapter。
- 为两个 JointType 增加 c3m6 request-local fixture、FreeCADCmd native expected、focused runtime test 和 C ABI capability 发布。
- 保持 RackPinion / Screw / Gears / Belt、复杂 Distance geometry、GUI drag / postDrag、跨请求 solver session 继续在本包之外。

## 当前状态

- `Cylindrical` 已通过独立收口包发布为 supported；本包又把 Parallel / Perpendicular 发布为 request-local supported，当前 remaining unsupported matrix 只保留 RackPinion / Screw / Gears / Belt。
- FreeCAD `AssemblyObject::makeMbdJointOfType()` 对 Parallel / Perpendicular 是一对一 ASMT 映射，不依赖 `Distance` / `Distance2`、特殊 marker 或 sliding part 判断。
- `cad-core/src/assembly/joint_solver.cpp` 已 include `ASMTParallelAxesJoint` 和 `ASMTPerpendicularJoint`，direct `Parallel` / `Perpendicular` 分支返回 real Ondsel ASMT joint，`isSupportedOndselJointType()` 已包含两者。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `supported_joint_matrix` 已包含 Parallel / Perpendicular，`unsupported_joint_matrix` 只保留 RackPinion / Screw / Gears / Belt。

## 证明链条

```text
声明口径
  -> FreeCAD JointType / makeMbdJointOfType 源码候选
  -> scope review / nonGoal / blocker queue
  -> Parallel / Perpendicular 映射专项复审
  -> native oracle 与 request-local writeback 专项复审
  -> capability / docs / unsupported matrix 专项复审
  -> code landing 与 oracle 发布收口
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| JointType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::JointType` | `Parallel`、`Perpendicular` 位于 `Distance` 和 `Angle` 之间 |
| Python 属性 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/JointObject.py::JointTypes` | `JointTypes` 包含 `Parallel` / `Perpendicular`；`Parallel` 在 `JointUsingReverse`，`Perpendicular` 在 `JointParallelForbidden` |
| MBD 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Parallel` 返回 `CREATE<ASMTParallelAxesJoint>::With()`；`Perpendicular` 返回 `CREATE<ASMTPerpendicularJoint>::With()` |
| marker 绑定 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()` | 非 RackPinion 走通用 `handleOneSideOfJoint()` marker 绑定 |
| solver 写回 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::solve()` | `mbdAssembly->runPreDrag()` 后 `setNewPlacements()`；cad-core 只输出 request-local update 建议 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO / adapter | `cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp` | 解析 JointConstraint，映射 Parallel / Perpendicular 到 real Ondsel ASMT joint |
| runtime publication | `cad-core/src/assembly/assembly_object.cpp`、`cad-core/src/assembly/assembly_utils.cpp` | 输出 solver summary 和 `documentObjectUpdates.action=assembly_set_placement` |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 supported / unsupported JointType matrix 和 covered keys |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 focused runtime、C ABI contract 和 expected parity |
| upstream docs | `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` | 回写 P8ASM-SCOPE-007 和 unsupported matrix 口径 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-13-08-【已实现】P8-ParallelPerpendicularJoint工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-18-13-09-【已实现】P8-ParallelPerpendicularJoint-S0-声明口径与live基线复核.md` | 冻结 claims、非目标和当前 unsupported 基线 |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-13-10-【已实现】P8-ParallelPerpendicularJoint-S1-FreeCAD源码候选矩阵.md` | 建立 source authority 和 candidate TSV |
| S2 范围准入 | `工作步骤细分/6-18-13-11-【已实现】P8-ParallelPerpendicularJoint-S2-范围准入与blocker矩阵.md` | 将候选路由到 unsupported / notCollected / nonGoal |
| S3 映射复审 | `工作步骤细分/6-18-13-12-【已实现】P8-ParallelPerpendicularJoint-S3-JointType映射专项复审.md` | 收口 ASMTParallelAxesJoint / ASMTPerpendicularJoint adapter |
| S4 oracle 复审 | `工作步骤细分/6-18-13-13-【已实现】P8-ParallelPerpendicularJoint-S4-NativeOracle与placement写回专项复审.md` | 收口 FreeCADCmd expected 与 request-local writeback |
| S5 发布复审 | `工作步骤细分/6-18-13-14-【已实现】P8-ParallelPerpendicularJoint-S5-Capability与unsupported矩阵专项复审.md` | 同步 capabilities、tests、P8 docs / TSV |
| S6 发布闸门 | `工作步骤细分/6-18-13-15-【已实现】P8-ParallelPerpendicularJoint-S6-Oracle实现与发布闸门.md` | 关闭 PPJ-BLOCK-001..005 并记录发布验证 |
| source candidates | `矩阵/p8_parallel_perpendicular_joint_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_parallel_perpendicular_joint_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_parallel_perpendicular_joint_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_parallel_perpendicular_joint_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_parallel_perpendicular_joint_backend_gap_classification.tsv` | unsupported / notCollected / nonGoal 分类 |

当前 S0-S6 均为 `【已实现】`；矩阵已从 seed 转为发布闸门结论。`PPJ-SCOPE-002/003/004/005` 为 supported，`PPJ-SCOPE-006` 保持 unsupported，`PPJ-SCOPE-007` 保持 nonGoal。
