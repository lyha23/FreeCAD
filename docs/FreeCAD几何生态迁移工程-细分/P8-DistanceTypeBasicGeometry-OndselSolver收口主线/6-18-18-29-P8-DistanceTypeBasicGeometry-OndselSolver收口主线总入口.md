# P8 DistanceTypeBasicGeometry OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly Distance 后续收口实施包。它只处理 `Distance` JointType 中不依赖半径提取的基础 `DistanceType` 几何映射，把点 / 线 / 平面引用从当前 scalar-only `ASMTSphSphJoint` 路径推进到 FreeCAD `makeMbdJointDistance()` 的 request-local Ondsel 语义。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线` 和 `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 迁移 FreeCAD `AssemblyUtils.cpp::getDistanceType()` 中 `Vertex` / line `Edge` / plane `Face` 组合的基础分类：`PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`。
- 迁移 FreeCAD `AssemblyObject.cpp::makeMbdJointDistance()` 中这些基础分类到 Ondsel joint 的映射：`ASMTSphSphJoint`、`ASMTSphericalJoint`、`ASMTRevCylJoint`、`ASMTCylSphJoint`、`ASMTPlanarJoint`、`ASMTPointInPlaneJoint`、`ASMTLineInPlaneJoint`。
- 为基础 DistanceType 增加 c3m6 request-local fixtures、FreeCADCmd native expected、focused runtime tests 和 C ABI capability/docs 发布。
- 保持 `LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` 等半径类 DistanceType 继续在本包之外；它们需要 `getEdgeRadius()` / `getFaceRadius()` 作为第二批。

## 当前基线

- P8 JointType 主线和后续子线已把 scalar JointTypes 推进到 request-local real Ondsel supported subset；Screw / RackPinion 包仍把 complex Distance geometry 标记为 `notCollected`。
- `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` 当前对 `jointType == "Distance"` 只创建 `ASMTSphSphJoint` 并写入 `distanceIJ = joint.distance`。
- `cad-core/include/cad_core/assembly/joint_solver.h::JointConstraint` 当前没有 `distanceType`、引用元素类型、基础几何 primitive 或 Distance solver joint class 证据字段。
- `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` 当前只公开 scalar `distance`，没有 `distance_type`、`distance_ij`、`offset` 或 resolved Ondsel class 证据。
- `cad-core/tools/collect_freecad_expected.py` 当前尚未为基础 DistanceType 输出 native expected 的 resolved joint class / distance field / offset field。

## 证明链条

```text
声明口径
  -> FreeCAD DistanceType 源码候选
  -> scope review / nonGoal / blocker queue
  -> Reference element 分类与 JCS 顺序专项复审
  -> Ondsel Distance joint 映射专项复审
  -> native oracle / capability 发布专项复审
  -> code landing for backendGap / notCollected / releaseGate
  -> 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DistanceType 枚举 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` | 列出 `PointPoint`、`LineLine`、`PlanePlane`、`PointPlane`、`LinePlane`、`PointLine` 以及半径 / 曲线扩展类型 |
| DistanceType 分类 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 读取 `Reference1` / `Reference2` 的元素类型，必要时 `swapJCS(joint)`，保证 line / face 侧在 FreeCAD 期望顺序 |
| Distance Joint 映射 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointPoint` 零距离转 `ASMTSphericalJoint`，非零转 `ASMTSphSphJoint.distanceIJ`；line / plane 基础组合转对应 Ondsel joint 与 `distanceIJ` / `offset` |
| JointType 分发 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `JointType::Distance` 调用 `makeMbdJointDistance(joint)`，不是固定 `ASMTSphSphJoint` |
| 半径类延后 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getEdgeRadius()`、`getFaceRadius()` | `LineCircle`、`PlaneCylinder`、`CylinderSphere` 等需要 edge / face 半径，必须留给第二批 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO | `cad-core/include/cad_core/assembly/joint_solver.h` | 为 `JointConstraint` 增加 request-local `distanceType`、引用 element kind / geometry primitive、resolved solver joint class、`distanceIJ` / `offset` 证据 |
| request builder | `cad-core/src/assembly/joint_solver.cpp::buildAssemblySolveRequest()` | 从 DocumentObject graph 的 Reference subnames / placement / fixture hint 中构建基础 DistanceType 分类证据，执行 request-local JCS ordering，不持久修改 graph |
| real Ondsel adapter | `cad-core/src/assembly/joint_solver.cpp::makeOndselJointOfType()` | 把 `Distance` JointType 分派到 FreeCAD `makeMbdJointDistance()` 等价的 Ondsel joint，而不是固定 `ASMTSphSphJoint` |
| JSON 证据 | `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` | 输出 `distance_type`、`solver_joint_class`、`distance_ij`、`offset` 和 `jcs_swapped_for_solver`，供 tests / expected 对比 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 采集 FreeCAD native expected 的 DistanceType、resolved Ondsel joint class 和 scalar field |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布基础 DistanceType support matrix，同时保留 radius-bearing / curve / GUI-session 边界 |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 6 类基础 DistanceType 的 runtime / expected / capability |
| upstream docs | `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` | 回写 complex Distance 从 broad `notCollected` 拆分为 basic supported 与 radius-bearing notCollected |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-18-29-【已实现】P8-DistanceTypeBasicGeometry工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-18-18-30-P8-DistanceTypeBasicGeometry-S0-声明口径与live基线复核.md` | 冻结 claims、禁止声明、状态字典和 current scalar-only 基线 |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-18-31-P8-DistanceTypeBasicGeometry-S1-FreeCAD源码候选矩阵.md` | 建立 DistanceType source candidates 和候选证据 |
| S2 范围准入 | `工作步骤细分/6-18-18-32-P8-DistanceTypeBasicGeometry-S2-范围准入与blocker矩阵.md` | 路由 scope、backendGap、notCollected、releaseGate 和 nonGoal |
| S3 引用分类复审 | `工作步骤细分/6-18-18-33-P8-DistanceTypeBasicGeometry-S3-ReferenceElement分类与JCS顺序专项复审.md` | 收口 element kind / primitive / swapJCS 的 request-local DTO 设计 |
| S4 Ondsel 映射复审 | `工作步骤细分/6-18-18-34-P8-DistanceTypeBasicGeometry-S4-OndselDistanceJoint映射专项复审.md` | 收口基础 DistanceType 到 Ondsel joint class 和 scalar field 的映射 |
| S5 oracle / capability 复审 | `工作步骤细分/6-18-18-35-P8-DistanceTypeBasicGeometry-S5-NativeOracle与Capability专项复审.md` | 收口 fixtures、FreeCADCmd expected、focused tests 和 capability publication |
| S6 发布闸门 | `工作步骤细分/6-18-18-36-P8-DistanceTypeBasicGeometry-S6-Oracle实现与发布闸门.md` | 消费 blocker 并给出代码落点 |
| source candidates | `矩阵/p8_distance_type_basic_geometry_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_distance_type_basic_geometry_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_distance_type_basic_geometry_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_distance_type_basic_geometry_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_distance_type_basic_geometry_backend_gap_classification.tsv` | backendGap / notCollected / releaseGate / nonGoal 分类 |

当前 S0-S6 都是待执行状态；矩阵是 seed，不是发布闸门结论。
