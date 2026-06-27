# C9-M3 Assembly DistanceType default boundary 批次总入口

本文是 `docs/CADCore9.0` 下的 C9-M3 实施主线。它承接 C9-M2 队列清空后的 live 状态，聚焦 Assembly `DistanceType` 中仍被 capability 发布为 diagnostic / default-or-TODO 的 request-local solver 边界：`PointCurve` 平面化、`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 既有 native expected，以及同一 FreeCAD default branch 下的 cone / sphere / torus / curve 组合扩面。

## 主线目标

- 不重开 C9-M1 / C9-M2 已关闭的 marker、bundled `offsetPlc`、placement writeback 或 zero Angle fallback。
- 以 FreeCAD `AssemblyUtils.cpp::getDistanceType()` 和 `AssemblyObject.cpp::makeMbdJointDistance()` 为 source authority，批量复核 `PointCurve` 与 default branch 的 native oracle、current cad-core 行为、capability 发布和 focused tests。
- 把当前 `PointCurve` diagnostic gate 与 `default_or_todo_boundary` 分成可验证的 scope：已有 expected 先激活，缺 native expected 的组合先采集，不从 current output 倒推支持状态。
- 只有 native expected 与 current cad-core mismatch 形成 `backend_gap_candidate` 时，S6 才进入 C++ 实现 gate；否则 S6 做 focused tests / capability 发布收口。

## 当前基线

- C9-M2 工作步骤队列已清空，handoff 提交为 `b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`；S0 执行 live 基线为 `04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`，起始 status 无输出；S1 执行 live 基线为 `8f209aab54 docs: 关闭 C9-M3 S0 基线冻结`，起始 status 无输出；S2 执行 live 基线为 `48355eae5d docs: 关闭 C9-M3 S1 源码候选矩阵`，起始 status 无输出。
- live capability 中 `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`assembly.ondsel_solver_adapter.status=covered_full`。
- Assembly DistanceType 仍发布 `distance_type_extended_geometry.deferred_diagnostic_cases=["PointCurve"]`，`default_or_todo_boundaries` 包含 `PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`、`CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other`。
- checked-in expected 已存在：`assembly-distance-point-curve-real-solver`、`assembly-distance-plane-cone-default-boundary`、`assembly-distance-line-cylinder-default-boundary`、`assembly-distance-curve-plane-default-boundary`、`assembly-distance-other-default-boundary`；这些当前仍带 `DTE-NG-003` / diagnostic 路由。
- 当前 `cad-core/src/assembly/joint_solver.cpp` 已能给 `PointCurve` 计算 `ASMTPointInPlaneJoint` / `offset` DTO，但 `unsupportedReasonForOndselJoint()` 仍显式返回 `point_curve_diagnostic_boundary`；default branch 缺 solver class，当前走 `default_boundary_not_mapped`。
- S0 已关闭：`C9M3-BLOCKER-000` / `C9M3-SCOPE-001` 只冻结 live baseline 与 forbidden claims；`PointCurve`、default branch、primitive frame、GUI/session、persistent solver 均不在 S0 写成 supported 或 backendGap。S1 已关闭：`C9M3-BLOCKER-101` 固化 FreeCAD `getDistanceType()` / `makeMbdJointDistance()` source authority、cad-core current landing、collector metadata、expected inventory 和 diagnostics guard；仍不把候选写成 supported 或 backendGap。S2 已关闭：`C9M3-BLOCKER-201` 将 `C9M3-SCOPE-101..404`、`C9M3-BG-101..601`、`C9M3-NG-001..005` 路由到 S3-S6 或 non-goal guard；没有 native expected 或 current mismatch 的行仍不写成 backendGap。`C9M3-BG-501` / `C9M3-BG-601` 仍是 release gate。

## 证明链条

```text
C9-M2 queue empty
  -> FreeCAD DistanceType source authority
  -> checked-in expected inventory and missing oracle matrix
  -> PointCurve plane-of-curve review
  -> default planar branch batch oracle
  -> capability / diagnostics publication gate
  -> code landing for expected-backed backendGap or supported publication
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DistanceType 分类 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 根据 Vertex / Edge / Face 和 line / circle / plane / cylinder / cone / torus / sphere 分类，并按 FreeCAD 顺序调用 `swapJCS(joint)`。 |
| PointCurve | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | Vertex + 非 line edge 进入 `DistanceType::PointCurve`，注释说明 other curves 做 plane-of-the-curve。 |
| PointCurve solver | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `PointCurve` 创建 `ASMTPointInPlaneJoint` 并写入 `offset = getJointDistance(joint)`。 |
| default branch | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `default` 创建 `ASMTPlanarJoint` 并写入 `offset = getJointDistance(joint)`。 |
| default 分类覆盖 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `PlaneCone`、`CylinderCone`、`Cone*`、`PointCone`、`LineCylinder`、`CurvePlane`、`Curve*`、`Other` 等先被分类，再由 default branch 处理未显式映射项。 |

S1 复核补充：Edge/Face 中 line edge 进入 `LinePlane`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`，非 line edge 进入 `CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`；Vertex/Edge 中非 line edge 进入 `PointCurve`；未命中组合返回 `Other`。

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| DistanceType DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`recordDistanceTypeEvidence()`、`resolveDistanceJointMapping()`、`unsupportedReasonForOndselJoint()`、real Ondsel joint construction。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 采集 / 标注 DistanceType native expected、known gap、delete condition 和 supported publication metadata。 |
| fixtures | `cad-core/fixtures/c3m6` | DistanceType request-local input fixture 与 `expected/*.freecad.json`。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | Assembly `distance_type_extended_geometry` 的 supported、non-goals、default/todo boundaries 与 diagnostics publication。 |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py` | focused expected parity、unsupported diagnostics guard、capability smoke。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、当前状态、批次边界和验收分层。 |
| 方案 | `6-28-00-06-C9-M3-AssemblyDistanceTypeDefaultBoundary批次方案.md` | C9-M3 实施策略。 |
| 工作步骤总入口 | `工作步骤细分/6-28-00-06-【已实现】C9-M3工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-28-00-07-【已实现】C9-M3-S0-live基线与DistanceType声明口径冻结.md` | 冻结 live baseline、claim 和 forbidden claim。 |
| S1 | `工作步骤细分/6-28-00-08-【已实现】C9-M3-S1-FreeCAD源码与current覆盖候选.md` | FreeCAD source authority、current cad-core coverage 与 checked-in expected inventory。 |
| S2 | `工作步骤细分/6-28-00-09-【已实现】C9-M3-S2-范围准入与blocker矩阵.md` | scope / blocker / non-goal / backend gap 初始路由。 |
| S3 | `工作步骤细分/6-28-00-10-C9-M3-S3-PointCurve平面化oracle复审.md` | PointCurve native expected、current diagnostic gate、focused tests。 |
| S4 | `工作步骤细分/6-28-00-11-C9-M3-S4-DefaultPlanarBranch批量oracle复审.md` | default branch 既有 expected 激活与缺口批量采集。 |
| S5 | `工作步骤细分/6-28-00-12-C9-M3-S5-capability与diagnostics发布准入.md` | capability publication、diagnostics guard 和 non-goal 保留。 |
| S6 | `工作步骤细分/6-28-00-13-C9-M3-S6-Oracle实现与发布闸门.md` | 根据 oracle 结果实现或 release gate。 |
| source candidates | `矩阵/c9m3_distance_type_default_boundary_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m3_distance_type_default_boundary_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m3_distance_type_default_boundary_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m3_distance_type_default_boundary_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m3_distance_type_default_boundary_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m3_distance_type_default_boundary_validation_matrix.tsv` | 分层验收命令。 |

当前 S0-S2 已关闭，S3-S6 仍为待执行状态；矩阵仍不是支持性发布结论。
