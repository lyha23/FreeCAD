# 【已实现】P8 DistanceTypeBasicGeometry S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、纳入范围、排除范围和状态字典，并用 live repo 证明当前 `Distance` JointType 仍是 scalar-only 基线。

## 声明口径

| 类别 | 允许声明 | 禁止声明 |
| --- | --- | --- |
| 本包目标 | 基础 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane` DistanceType 的 request-local Ondsel 支持 | 完整 Distance geometry matrix 已支持 |
| cad-core 边界 | 从每次请求的 DocumentObject graph 构建 solver DTO，返回 solver evidence / placement updates | 持久保存 MBD session、shape、NamedShape、ElementMap 或完整 BREP |
| `swapJCS` | request-local DTO ordering，保持 graph 不变 | 直接修改前端 DocumentObject graph 或跨请求缓存 JCS |
| oracle | 每个基础 DistanceType 至少有 focused fixture 和 FreeCADCmd expected | 只靠当前 fixture 输出、bbox、shape 数量或手写 expected 宣称支持 |

## 纳入范围

| scope | DistanceType | FreeCAD 映射 |
| --- | --- | --- |
| point basic | `PointPoint` | 零距离 `ASMTSphericalJoint`；非零 `ASMTSphSphJoint.distanceIJ` |
| edge basic | `LineLine` | `ASMTRevCylJoint.distanceIJ` |
| point-edge basic | `PointLine` | `ASMTCylSphJoint.distanceIJ` |
| face basic | `PlanePlane` | `ASMTPlanarJoint.offset` |
| point-face basic | `PointPlane` | `ASMTPointInPlaneJoint.offset` |
| edge-face basic | `LinePlane` | `ASMTLineInPlaneJoint.offset` |

## 排除范围

| 排除项 | 原因 | 重新打开条件 |
| --- | --- | --- |
| `LineCircle` / `CircleCircle` | 依赖 `getEdgeRadius()` | 第二批半径类 DistanceType package |
| `PlaneCylinder` / `PlaneSphere` / `CylinderCylinder` / `CylinderSphere` / `PointCylinder` / `PointSphere` | 依赖 `getFaceRadius()` | 第二批半径类 DistanceType package |
| `PointCurve` / `CurvePlane` / default `Other` | FreeCAD 仍有 TODO 或 fallback 语义，需要单独判断 | 单独 curve/default package |
| GUI/session/full transaction | 超出 stateless cad-core request boundary | 单独 Assembly protocol package |

## 状态字典

| 状态 | 含义 | 使用条件 |
| --- | --- | --- |
| `supportedBaseline` | 已有能力，只作为输入基线 | 不由本包修改 |
| `backendGap` | FreeCAD 有明确语义且 cad-core 当前不等价 | 必须有源码依据和 current mismatch |
| `notCollected` | oracle 尚未采集 | 必须在 S6 变成 fixture / expected 任务或继续留作下一包 |
| `releaseGate` | 实现后发布前必须同步的测试 / capability / docs | 必须有验收命令 |
| `nonGoal` | 本包明确不做 | 必须写用户 / 协议行为和 reopen 条件 |
| `supported` | 已实现并通过验收 | 只能在代码和 expected 全部通过后使用 |

## 必须回写的矩阵行

- `DTC-SCOPE-001` 当前 scalar Distance baseline。
- `DTC-SCOPE-002..005` 基础 DistanceType backend gap。
- `DTC-SCOPE-006..007` oracle / capability release gate。
- `DTC-SCOPE-008..009` radius-bearing / curve / GUI 边界。

## live 基线复核状态

2026-06-18 live 复核通过，S0 只关闭声明口径与当前基线确认，不表示 S1-S6、oracle、cad-core 实现或发布闸门完成。

baseline：

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`5b2609b585`
- `git log -1 --oneline`：`5b2609b585 docs: 标记P8 DistanceType入口已校验`
- `git -c core.quotepath=false status --short -uall`：开始复核时无输出，工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：开始复核时队列首项为 `6-18-18-30-P8-DistanceTypeBasicGeometry-S0-声明口径与live基线复核.md`，S1-S6 仍为 pending；重命名后队列首项应切换到 S1。

源码证据：

- cad-core 当前仍是 scalar-only Distance baseline：`cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` 的 live `rg` 命中显示 `joint.jointType == "Distance"` 在第 615 行进入固定分支，第 616 行创建 `MbD::ASMTSphSphJoint::With()`，第 617 行只写 `distanceJoint->distanceIJ = joint.distance.value_or(0.0)`。
- cad-core DTO 还没有 DistanceType / primitive 证据：`cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 第 45-77 行只有 `jointType`、`Reference1/2`、`distance`、`distance2`、`angle`、`pitch`、`slidingPartIndex`、`jcsSwappedForSolver`、`pitchRadius`、`rackPinionMarkerRewrite`，没有 `distanceType`、引用 element kind、solver joint class、`distanceIJ` 或 `offset` 字段。
- cad-core solver JSON 只公开 scalar evidence：`cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` 第 88-147 行只在第 97-99 行输出 `"distance"`，没有 `distance_type`、`solver_joint_class`、`distance_ij`、`offset` 或 Distance 专属 `jcs_swapped_for_solver`。
- cad-core request builder 只读取 Distance 标量：`cad-core/src/assembly/joint_solver.cpp::buildAssemblySolveRequest()` 第 955-965 行构造 `JointConstraint` 并在 `jointType == "Distance"` 时读取 `Distance` 到 `constraint.distance`，没有 Reference element 分类或 DistanceType dispatch。
- FreeCAD 语义不是 scalar-only：`src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` 第 83-128 行枚举包含 `PointPoint`、`LineLine`、`PlanePlane`、`PointPlane`、`LinePlane`、`PointLine` 以及半径 / 曲线扩展类型。
- FreeCAD 分类入口：`src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 第 154-371 行读取 `Reference1/2` element type，基础点 / 线 / 面分支命中 `PointPoint` 第 167-168 行、`LineLine` 第 178-179 行、`PlanePlane` 第 208-209 行、`PointPlane` 第 291-298 行、`LinePlane` 第 313-321 行、`PointLine` 第 355-362 行；同函数第 173、203、293、315、357 行有 `swapJCS(joint)` 排序证据。
- FreeCAD Distance 分发入口：`src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` 第 1169-1191 行在 `JointType::Distance` 时调用 `makeMbdJointDistance(joint)`。
- FreeCAD Ondsel joint 映射入口：`src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 第 1249-1405 行将基础 DistanceType 映射到 `ASMTSphericalJoint` / `ASMTSphSphJoint.distanceIJ`、`ASMTRevCylJoint.distanceIJ`、`ASMTPlanarJoint.offset`、`ASMTPointInPlaneJoint.offset`、`ASMTLineInPlaneJoint.offset`、`ASMTCylSphJoint.distanceIJ`；半径类分支第 1277-1287、1297-1307、1315-1353、1363-1372 行依赖 `getEdgeRadius()` / `getFaceRadius()`，继续排除在本包之外。

矩阵复核：

- `p8_distance_type_basic_geometry_scope_review_matrix.tsv` 已包含 `DTC-SCOPE-001..009`，状态值分别落在本文件状态字典：`supportedBaseline`、`backendGap`、`notCollected`、`releaseGate`、`nonGoal`。
- `p8_distance_type_basic_geometry_blocker_queue.tsv` 已包含 `DTC-BLOCK-001..007`，覆盖 S3-S6 的 classification、mapping、oracle、capability 和 boundary protection，不在 S0 关闭。
- `p8_distance_type_basic_geometry_non_goal_registry.tsv` 已包含 `DTC-NG-001..004`，覆盖 radius-bearing、curve/default、GUI/session、persistent solver state。
- `p8_distance_type_basic_geometry_backend_gap_classification.tsv` 已包含 `DTC-BG-001..008`，覆盖 S3-S6 backend gap / release gate / nonGoal 分类。
- `p8_distance_type_basic_geometry_source_candidates.tsv` 已包含 `DTC-CAND-001..016`，其中 `DTC-CAND-011..013` 对应当前 cad-core DTO、Distance mapping 和 JSON evidence gap。

## 验收标准

```bash
git status --short
rg -n 'joint.jointType == "Distance"|ASMTSphSphJoint|distanceIJ = joint.distance' cad-core/src/assembly/joint_solver.cpp
rg -n 'DistanceType::PointPoint|DistanceType::LineLine|DistanceType::PlanePlane|DistanceType::PointPlane|DistanceType::LinePlane|DistanceType::PointLine' src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/App/AssemblyUtils.cpp
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线/矩阵/*.tsv
```

完成条件：

- 本文件已记录上述 live 命令输出和源码证据；文件名加 `【已实现】` 后，队列应跳过 S0 并把 S1 作为下一首项。
- scope / nonGoal / blocker / backend gap 矩阵必须存在，并且包含本文件列出的 rows。
- 不允许把本包写成完整 Assembly solver 或完整 Distance geometry 支持。

## 非目标

- 不编辑 cad-core 代码。
- 不采集 native expected。
- 不修改已收口的 Screw/RackPinion、Gears/Belt 或 Cylindrical 包。
