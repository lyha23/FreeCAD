# C9-M3 Assembly DistanceType default boundary 批次总入口

本文是 `docs/CADCore9.0` 下的 C9-M3 实施主线。它承接 C9-M2 队列清空后的 live 状态，聚焦 Assembly `DistanceType` 中仍被 capability 发布为 diagnostic / default-or-TODO 的 request-local solver 边界：`PointCurve` 平面化、`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 既有 native expected，以及同一 FreeCAD default branch 下的 cone / sphere / torus / curve 组合扩面。

## 主线目标

- 不重开 C9-M1 / C9-M2 已关闭的 marker、bundled `offsetPlc`、placement writeback 或 zero Angle fallback。
- 以 FreeCAD `AssemblyUtils.cpp::getDistanceType()` 和 `AssemblyObject.cpp::makeMbdJointDistance()` 为 source authority，批量复核 `PointCurve` 与 default branch 的 native oracle、current cad-core 行为、capability 发布和 focused tests。
- 把当前 `PointCurve` diagnostic gate 与 `default_or_todo_boundary` 分成可验证的 scope：已有 expected 先激活，缺 native expected 的组合先采集，不从 current output 倒推支持状态。
- 只有 native expected 与 current cad-core mismatch 形成 `backend_gap_candidate` 时，S6 才进入 C++ 实现 gate；否则 S6 做 focused tests / capability 发布收口。

## 当前基线

- C9-M2 工作步骤队列已清空，handoff 提交为 `b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`；S0 执行 live 基线为 `04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`，起始 status 无输出；S1 执行 live 基线为 `8f209aab54 docs: 关闭 C9-M3 S0 基线冻结`，起始 status 无输出；S2 执行 live 基线为 `48355eae5d docs: 关闭 C9-M3 S1 源码候选矩阵`，起始 status 无输出；S3 执行 live 基线为 `696046ca6c docs: 关闭 C9-M3 S2 范围准入路由`，起始 status 无输出；S4 执行 live 基线为 `213583d369 docs: 关闭 C9-M3 S3 PointCurve 复审`，起始 status 无输出；S5 执行 live 基线为 `7b252ed6df docs: 关闭 C9-M3 S4 DefaultPlanarBranch 复审`，起始 status 无输出。
- live capability 中 `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`assembly.ondsel_solver_adapter.status=covered_full`。
- Assembly DistanceType 发布已更新：`distance_type_extended_geometry.native_expected_count=18`，`supported` 包含 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`，`deferred_diagnostic_cases=[]`。
- checked-in accepted expected：`assembly-distance-point-curve-real-solver`、`assembly-distance-plane-cone-default-boundary`、`assembly-distance-line-cylinder-default-boundary`、`assembly-distance-curve-plane-default-boundary`、`assembly-distance-other-default-boundary` 已删除 stale `DTE-NG-003` / diagnostic metadata，并与 current parity。
- 缺 input/expected 的 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 继续保留在 `default_or_todo_boundaries`，route 为 `notCollected` / `native_oracle_required`。
- S0-S5 已关闭 source / scope / oracle / publication gate；S6 已关闭 implementation and release gate，`C9M3-BLOCKER-601` 关闭，队列清空后本包收口。

## S4 关闭证据

- checked-in default expected：`assembly-distance-plane-cone-default-boundary.freecad.json`、`assembly-distance-line-cylinder-default-boundary.freecad.json`、`assembly-distance-curve-plane-default-boundary.freecad.json`、`assembly-distance-other-default-boundary.freecad.json` 均为 FreeCADCmd `1.2.0 revision 20260519` solved oracle，`native_solver.return_code=0`，`solver_adapter.status=solved`，`unsupported_joints=[]`，并写回 `ComponentB` placement。
- FreeCAD default solver authority：`AssemblyObject.cpp::makeMbdJointDistance()` 的 `default` 分支创建 `ASMTPlanarJoint` 并写 `offset=getJointDistance(joint)`；四个 fixture 的 `Distance` 均为 `1.5`。checked-in expected metadata 仍保留 `distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`、`known_gap=DTE-BLOCK-006/DTE-NG-003` 和 `nonGoal.ids=["DTE-NG-003"]`，由 S5/S6 决定是否删除或保留。
- current comparison 已运行四个单 fixture recompute 到 `/tmp/c9m3-s4-plane-cone.json`、`/tmp/c9m3-s4-line-cylinder.json`、`/tmp/c9m3-s4-curve-plane.json`、`/tmp/c9m3-s4-other.json`；四者均输出 `unsupported_assembly_solver`，message 为 `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`。现有 `test_p8_features.py` 与 `joint_solver.cpp` 仍保留 no solver class / `default_boundary_not_mapped` guard。
- 缺 input/expected 的 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 只记录 `notCollected` / `native_oracle_required`，后续需先采 native oracle。

## S5 关闭证据

- S5 执行起点为 `7b252ed6df docs: 关闭 C9-M3 S4 DefaultPlanarBranch 复审`，起始 status 无输出；S5 只执行 publication readiness，不进入 C++、expected、adapter 或 test 实现。
- S6 必须消费的 route 已明确：`PointCurve` 和 `PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 是 expected-backed `backend_gap_candidate`；缺 oracle 的 default families 继续 `notCollected` / `native_oracle_required`，并必须留在 `default_or_todo_boundaries` 或等价可见字段。
- diagnostics guard 已明确：unsupported JointType、`missing_grounded_part`、`missing_marker_placement`、`invalid_assembly_solver_result` 和未采 default boundary 不能因 accepted rows 被支持而静默消失。
- primitive frame、persistent solver、GUI/session 和 adapter string hiding 仍是 non-goal / guard；不得用 `known_gaps=[]` 或 adapter 文案把 runtime diagnostic 藏起来。

## 证明链条

```text
C9-M2 queue empty
  -> FreeCAD DistanceType source authority
  -> checked-in expected inventory and missing oracle matrix
  -> PointCurve plane-of-curve review
  -> default planar branch batch oracle
  -> capability / diagnostics publication gate
  -> S6 supported publication and queue-empty release gate
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
| S3 | `工作步骤细分/6-28-00-10-【已实现】C9-M3-S3-PointCurve平面化oracle复审.md` | 已关闭：PointCurve native expected/current mismatch -> S6 `backend_gap_candidate`，current diagnostic gate 暂保留。 |
| S4 | `工作步骤细分/6-28-00-11-【已实现】C9-M3-S4-DefaultPlanarBranch批量oracle复审.md` | 已关闭：existing default expected/current mismatch -> S6 `backend_gap_candidate`；缺 input/expected 扩面族保留 `notCollected` / `native_oracle_required`。 |
| S5 | `工作步骤细分/6-28-00-12-【已实现】C9-M3-S5-capability与diagnostics发布准入.md` | 已关闭：publication route 和 diagnostics guard 已准入，S6 消费 expected-backed backend gap，缺 oracle rows 保持可见。 |
| S6 | `工作步骤细分/6-28-00-13-【已实现】C9-M3-S6-Oracle实现与发布闸门.md` | 已关闭：expected-backed backend gap 已实现，accepted expected / capability / tests / docs 已同步，缺 oracle rows 保持可见。 |
| source candidates | `矩阵/c9m3_distance_type_default_boundary_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m3_distance_type_default_boundary_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m3_distance_type_default_boundary_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m3_distance_type_default_boundary_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m3_distance_type_default_boundary_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m3_distance_type_default_boundary_validation_matrix.tsv` | 分层验收命令。 |

当前 S0-S6 已关闭；矩阵记录最终支持性发布结论和 remaining notCollected/default rows。
