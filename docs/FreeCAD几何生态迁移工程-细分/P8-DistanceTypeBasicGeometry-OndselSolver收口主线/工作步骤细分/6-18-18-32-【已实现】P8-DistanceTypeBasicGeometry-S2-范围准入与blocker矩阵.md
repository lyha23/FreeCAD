# 【已实现】P8 DistanceTypeBasicGeometry S2 范围准入与 blocker 矩阵

## 目标

把 S1 source candidates 路由到 scope review、blocker queue、nonGoal registry 和 backend gap classification，让 S3-S6 可以直接消费实现项。

## 矩阵路由复核状态

2026-06-18 live 复核通过，S2 只关闭范围准入与矩阵路由，不关闭 S3-S6 blocker，不采集 expected，不修改 cad-core 代码，也不把任何 basic DistanceType 标成 `supported`。

baseline：

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`6b63f5f6f7`
- `git log -1 --oneline`：`6b63f5f6f7 docs: 完成P8 DistanceType S1源码候选矩阵`
- `git -c core.quotepath=false status --short -uall`：开始复核时无输出，工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：开始复核时队列首项为 `6-18-18-32-P8-DistanceTypeBasicGeometry-S2-范围准入与blocker矩阵.md`，S3-S6 仍为 pending。

源码依据：

- FreeCAD `src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` 第 83-128 行列出基础 point / line / plane、radius-bearing、curve/default 类型。
- FreeCAD `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 第 160-371 行读取 `Reference1/2` element kind，并在 line / face / point-line 等分支执行 `swapJCS(joint)` 排序。
- FreeCAD `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` 第 1190-1191 行把 `JointType::Distance` 分派到 `makeMbdJointDistance(joint)`；`makeMbdJointDistance()` 第 1249-1405 行给基础 DistanceType 映射不同 Ondsel joint class 和 `distanceIJ` / `offset`。
- cad-core `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 第 45-77 行仍没有 `distanceType`、element primitive、solver class、`distanceIJ` 或 `offset` 证据字段。
- cad-core `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` 第 615-618 行仍把所有 `Distance` 固定为 `ASMTSphSphJoint.distanceIJ = joint.distance`；`buildAssemblySolveRequest()` 第 955-965 行只读取 scalar `Distance`。
- cad-core `cad-core/src/adapters/c_api/c_api.cpp` 第 150-174 行和第 681-683 行发布 JointType 级 `Distance` 能力，尚无 basic DistanceType split，因此只能作为 `DTC-SCOPE-007` release gate。

矩阵结论：

- `DTC-SCOPE-001` 保持 `supportedBaseline`，只代表当前 scalar Distance JointType baseline，不代表 DistanceType parity。
- `DTC-SCOPE-002..005` 均为 `backendGap`，分别路由到 `DTC-BLOCK-001..004`，由 S3/S4 关闭。
- `DTC-SCOPE-006` 保持 `notCollected`，路由到 `DTC-BLOCK-005`，由 S5 采集 native expected 和 focused fixtures。
- `DTC-SCOPE-007` 保持 `releaseGate`，路由到 `DTC-BLOCK-006`，由 S5/S6 同步 capability、tests、docs / matrices。
- `DTC-SCOPE-008` 保持 `notCollected` 且明确是 second batch，路由到 `DTC-BLOCK-007`，不得在本包发布 radius-bearing support。
- `DTC-SCOPE-009` 保持 `nonGoal`，由 `DTC-NG-002..004` 和 `DTC-BLOCK-007` 保护 curve/default、GUI/session 和 persistent state 边界。
- `DTC-BLOCK-001..007` 均有 S3-S6 消费步骤和关闭条件；S2 不关闭这些 blocker。
- `DTC-NG-001..004` 均记录了用户 / 协议行为和 reopen 条件。
- `DTC-BG-001..008` 均能追溯到 `DTC-SCOPE-002..009`。
- 5 个 TSV 字段数复核通过；状态值只使用 S0 状态字典中的 `supportedBaseline`、`backendGap`、`notCollected`、`releaseGate`、`nonGoal`，没有新增或误用 `supported`。

## 分类规则

| 分类 | 准入条件 | 示例 |
| --- | --- | --- |
| `supportedBaseline` | 当前 cad-core 已支持，且不由本包修改 | scalar Distance JointType 存在，但不代表 DistanceType 已支持 |
| `backendGap` | FreeCAD 有明确语义，cad-core 当前不等价，且本包能闭环 | `PointPoint` 零 / 非零、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` |
| `notCollected` | 需要 native oracle 但尚未采集 | c3m6 expected、radius-bearing DistanceType |
| `releaseGate` | 实现后必须同步才能发布 | C ABI capability、docs / matrix publication |
| `nonGoal` | 本包不做且有 reopen 条件 | curve/default、GUI/session、persistent solver |

## blocker 路由

| blocker | 消费步骤 | 关闭条件 |
| --- | --- | --- |
| `DTC-BLOCK-001` | S3 | `JointConstraint` / JSON 能暴露基础 DistanceType、element classification 和 JCS ordering |
| `DTC-BLOCK-002` | S4 | `PointPoint` 零 / 非零映射到正确 Ondsel joint |
| `DTC-BLOCK-003` | S4 | `LineLine` / `PointLine` 映射到正确 Ondsel joint |
| `DTC-BLOCK-004` | S4 | `PlanePlane` / `PointPlane` / `LinePlane` 映射到正确 Ondsel joint |
| `DTC-BLOCK-005` | S5 | fixtures 和 FreeCADCmd expected 存在并通过 check |
| `DTC-BLOCK-006` | S5 / S6 | capability、tests、docs / matrices 同步 |
| `DTC-BLOCK-007` | S6 | radius-bearing / curve / GUI/session 未被误发布 |

## 必须回写的矩阵行

- `p8_distance_type_basic_geometry_scope_review_matrix.tsv`：所有 `DTC-SCOPE-*` 状态必须有效。
- `p8_distance_type_basic_geometry_blocker_queue.tsv`：所有 backendGap / releaseGate 都必须有 blocker。
- `p8_distance_type_basic_geometry_non_goal_registry.tsv`：所有 nonGoal 必须有用户 / 协议行为和 reopen 条件。
- `p8_distance_type_basic_geometry_backend_gap_classification.tsv`：所有 P1 项必须有下一轮代码落点。

## 验收标准

```bash
rg -n 'backendGap|notCollected|releaseGate|nonGoal|supportedBaseline' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_scope_review_matrix.tsv
rg -n 'DTC-BLOCK-00[1-7]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_blocker_queue.tsv
rg -n 'DTC-NG-00[1-4]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_non_goal_registry.tsv
rg -n 'DTC-BG-00[1-8]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/p8_distance_type_basic_geometry_backend_gap_classification.tsv
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

完成条件：

- 每个 scope 都路由到有效状态。
- 每个 blocker / nonGoal / backendGap row 都能追溯到 scope 或 exclusion reason。
- 不允许没有 FreeCAD authority 的 `backendGap`。
- 不允许把 `notCollected` 当成长期文档状态；S6 必须给出采集或延后路径。

## 非目标

- 不把半径类 DistanceType 并入第一批。
- 不做代码变更。
- 不做发布声明。
