# P8 DistanceTypeExtendedGeometry OndselSolver 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly DistanceType 后续收口实施包。它承接 `P8-DistanceTypeBasicGeometry-OndselSolver收口主线` 已发布的 Point / Line / Plane basic subset，把剩余 DistanceType 按 FreeCAD 同一条调用链一次性纳入方案、矩阵、oracle、实现和发布边界，而不是只挑单个 radius fixture。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` 和 `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 按 FreeCAD `AssemblyUtils.cpp::getDistanceType()`、`getEdgeRadius()`、`getFaceRadius()` 和 `AssemblyObject.cpp::makeMbdJointDistance()` 的同一调用链，完整复核剩余 DistanceType。
- 本轮最小完整语义批次覆盖显式 `makeMbdJointDistance()` switch 可表达的 extended cases：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`PlaneTorus`、`CylinderCylinder`、`CylinderSphere`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`、`PointCylinder`、`PointSphere`、`PointCurve`。
- 同源但语义仍依赖 FreeCAD TODO / default branch 的 cases 必须纳入矩阵审计，不得沉默遗漏：`PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`、`CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other`。
- 为 supported 子集批量采集 FreeCADCmd expected，补 cad-core DTO / radius evidence / solver class / scalar field，实现 fixtures、focused tests、C ABI capability 和文档矩阵闭环。
- 不把 GUI/session、跨请求 solver state、完整 marker offsetPlc 泛化或 FreeCAD TODO/default branch 误发布为 supported。

## 当前基线

- `P8-DistanceTypeBasicGeometry-OndselSolver收口主线` 已发布 `PointPoint`、`LineLine`、`PointLine`、`PlanePlane`、`PointPlane`、`LinePlane`；`DTC-SCOPE-008` 仍标记 radius-bearing `notCollected`，`DTC-SCOPE-009` 仍保留 curve/default boundary。
- `P8-Assembly-Reference-JCS-MarkerPlacement收口主线` 的 representative subshape marker placement subset 已被本包消费到 request-local `Assembly::AssemblyLink` identity-offset 场景；非 identity bundled `offsetPlc`、GUI/session 和 persistent solver state 仍不在 support claim 内。
- S6 已发布 `distance_type_extended_geometry` capability。当前 supported extended cases 为 13 个：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`、`PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`。
- 13 个 supported expected 已删除 `known_gap` / `backendGap` 并通过 `CadCoreExpectedFixtureTest`；这些 expected 的 `solver_adapter` 精确对齐 FreeCAD native oracle，`bbox_delta=0.2` 只用于 cad-core display primitive bbox 与 FreeCAD exact bbox 的局部容差。
- `PointCurve` 保留 diagnostic / nonGoal：native expected 已采集，但 FreeCAD 源码仍是 TODO-like plane-of-curve 语义，未进入 supported capability。
- cone、line-surface、curve-face、`Other` default/TODO 代表 case 继续为 diagnostic / nonGoal，不发布为 supported。

## 最小完整语义批次

本主线不按单 fixture 推进。S6 已按以下批次关闭：

1. Edge circle radius cases：`LineCircle`、`CircleCircle`，通过 `getEdgeRadius()` 对 `distanceIJ` 做 FreeCAD 等价修正，已进入 supported。
2. Face radius / direct surface cases：`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`，通过 `getFaceRadius()` 对 `offset` 或 `distanceIJ` 做 FreeCAD 等价修正，已进入 supported。
3. Explicit torus / sphere switch cases：`PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`，按 FreeCAD 当前 switch 的 ASMT class 和 scalar 字段进入 supported；torus radius 仍按 FreeCAD helper 行为贡献 0。
4. Explicit point-curve fallback：`PointCurve` 已采 native expected，但保留 diagnostic / nonGoal，不发布。
5. Default / TODO cases：cone、line-surface、curve-face 和 `Other` 已形成 checked-in diagnostic / nonGoal 结论，不从 capability 中消失，也不被标为 supported。

如 native oracle 无法稳定采集或 FreeCAD TODO / default 行为与产品语义冲突，允许把 default / curve cases 拆成后续专包；但必须在 S2/S5 文档说明拆分原因、下一批次范围和防止长期单 fixture 推进的措施。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DistanceType enum | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.h::DistanceType` | 枚举 Point / Line / Circle / Plane / Cylinder / Cone / Torus / Sphere / Curve / Other 全部组合 |
| DistanceType 分类 | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 读取 Reference1/2 element kind 与 OCCT primitive，必要时 `swapJCS(joint)`，把 line / circle / face / curve 侧转到 FreeCAD solver 顺序 |
| Edge radius | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getEdgeRadius()` | `GeomAbs_Circle` 返回 `sf.Circle().Radius()`，其它 edge 返回 0 |
| Face radius | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getFaceRadius()` | `GeomAbs_Cylinder` / `GeomAbs_Sphere` 返回对应 Radius，其它 face 返回 0 |
| Distance ASMT 映射 | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | 对显式 switch case 创建 ASMT joint，并把 `getJointDistance()`、edge radius、face radius 写入 `distanceIJ` 或 `offset` |
| default / TODO | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointCurve` 显式走 `ASMTPointInPlaneJoint.offset`；其它未列 case 进入 default `ASMTPlanarJoint.offset` |

## cad-core 落点

| 层 | 当前代码落点 | 本包职责 |
| --- | --- | --- |
| assembly DTO | `cad-core/include/cad_core/assembly/joint_solver.h` | 扩展 DistanceType、reference primitive、radius evidence、resolved solver class、`distanceIJ` / `offset` 证据 |
| primitive/radius resolver | `cad-core/src/assembly/joint_solver.cpp`，必要时补 `cad-core/src/geometry` helper | 复用当前 OCCT primitive detection，新增 circle / cylinder / sphere radius extraction，明确 cone / torus / curve default 边界 |
| request builder | `cad-core/src/assembly/joint_solver.cpp::classifyDistanceType()` | 对所有 remaining enum cases 做 request-local classification 和 `jcsSwappedForSolver`，不持久修改 graph |
| ASMT mapping | `cad-core/src/assembly/joint_solver.cpp::resolveDistanceJointMapping()`、`makeOndselDistanceJoint()` | 对 supported extended cases 映射 FreeCAD ASMT class 和 scalar field；unsupported/default 走 diagnostic 或 explicit nonGoal |
| JSON 证据 | `cad-core/src/assembly/assembly_utils.cpp::solverJointJson()` | 输出 `distance_type`、primitive、radius evidence、solver class、scalar field、default route |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 批量采集 extended DistanceType native expected，不用 cad-core 输出倒推 FreeCAD golden |
| tests / fixtures | `cad-core/fixtures/c3m6`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py` | 覆盖 edge circle、face radius、torus/sphere、point curve、default boundary representative cases |
| capability | `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py` | 发布 `distance_type_extended_geometry`，把 supported subset、oracle count、remaining default/curve/nonGoal 边界写清楚 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-19-06-41-【已实现】P8-DistanceTypeExtendedGeometry工作步骤总入口.md` | S0-S6 执行索引，已校验索引 / 矩阵文件名 / 轻量验收 |
| S0 声明口径 | `工作步骤细分/6-19-06-42-【已实现】P8-DistanceTypeExtendedGeometry-S0-声明口径与live基线复核.md` | 已完成：冻结 claims、non-goals、现有 basic / marker baseline 和拆分纪律 |
| S1 源码候选 | `工作步骤细分/6-19-06-43-【已实现】P8-DistanceTypeExtendedGeometry-S1-FreeCAD源码候选矩阵.md` | 已完成：完整 DistanceType source authority 和 cad-core landing |
| S2 范围准入 | `工作步骤细分/6-19-06-44-【已实现】P8-DistanceTypeExtendedGeometry-S2-范围准入与blocker矩阵.md` | 已完成：冻结 supportedCandidate / oracleFirst / defaultBoundary / nonGoal 边界和 blocker ownership |
| S3 radius / primitive | `工作步骤细分/6-19-06-45-【已实现】P8-DistanceTypeExtendedGeometry-S3-RadiusPrimitive证据专项复审.md` | 已完成：DTO / primitive / radius / scalar correction / boundary evidence，不进入 ASMT 发布 |
| S4 ASMT mapping | `工作步骤细分/6-19-06-46-【已实现】P8-DistanceTypeExtendedGeometry-S4-OndselDistanceJoint扩展映射专项复审.md` | 已完成：显式 extended cases 的 ASMT class 与 scalar field 已映射，S5 oracle 仍是 parity 闸门 |
| S5 oracle / fixtures | `工作步骤细分/6-19-06-47-【已实现】P8-DistanceTypeExtendedGeometry-S5-NativeOracle与代表fixture专项复审.md` | 已完成：批量采集 expected，锁定 supported-candidate / diagnostic / nonGoal |
| S6 实现发布 | `工作步骤细分/6-19-06-48-【已实现】P8-DistanceTypeExtendedGeometry-S6-实现与发布闸门.md` | 已完成：13 个 extended cases 发布为 supported，PointCurve/default 边界保持 diagnostic / nonGoal，queue 为空 |
| source candidates | `矩阵/p8_distance_type_extended_geometry_source_candidates.tsv` | FreeCAD / cad-core 候选证据 |
| scope review | `矩阵/p8_distance_type_extended_geometry_scope_review_matrix.tsv` | scope 状态和验收路由 |
| blocker queue | `矩阵/p8_distance_type_extended_geometry_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| backend gap classification | `矩阵/p8_distance_type_extended_geometry_backend_gap_classification.tsv` | backendGap / notCollected / defaultBoundary / nonGoal 聚合 |
| non goal registry | `矩阵/p8_distance_type_extended_geometry_non_goal_registry.tsv` | 不进入本轮实现或不得发布的边界 |

## 当前执行结论

`goal-step-runner` 队列已清空。后续只在产品明确接受 `PointCurve` 或 default/TODO branch 的具体行为时，另开专包重审并补 expected / capability；不得把这些 diagnostic expected 直接升级为 supported。

## 非目标

- 不重新打开 basic Point / Line / Plane DistanceType 已收口结论。
- 不把 FreeCAD TODO / default branch 直接发布为 full support。
- 不实现 GUI drag / postDrag、Reverse UI、跨请求 solver session。
- 不把 BREP、NamedShape、ElementMap、mesh 或 MBD solver state 作为跨请求后端状态保存。
- 不从 cad-core 当前输出倒推 FreeCAD expected。
