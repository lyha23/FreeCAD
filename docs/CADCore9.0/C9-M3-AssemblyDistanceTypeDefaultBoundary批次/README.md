# C9-M3 Assembly DistanceType default boundary 批次

## 定位

C9-M3 承接 C9-M2 关闭后的 Assembly request-local solver 状态，专门处理 `DistanceType` 中仍保留为 diagnostic / default-or-TODO 的边界。它不重开 C9-M1/C9-M2 已经关闭的 marker placement、bundled `offsetPlc`、placement writeback 或 zero Angle fallback。

## 当前状态

- live baseline：C9-M2 关闭 handoff 为 `b981e84f68`（`b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`）；S0 执行起点为 `pwd=/home/user/Chili3DProject/FreeCAD`、`HEAD=04bdd2e561`（`04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`），起始 status 无输出；S1 执行起点为 `HEAD=8f209aab54`（`8f209aab54 docs: 关闭 C9-M3 S0 基线冻结`），起始 status 无输出；S2 执行起点为 `48355eae5d`（`48355eae5d docs: 关闭 C9-M3 S1 源码候选矩阵`），起始 status 无输出；S3 执行起点为 `696046ca6c`（`696046ca6c docs: 关闭 C9-M3 S2 范围准入路由`），起始 status 无输出；S4 执行起点为 `213583d369`（`213583d369 docs: 关闭 C9-M3 S3 PointCurve 复审`），起始 status 无输出。
- C9-M2 queue 已由 S0 复核清空，C9-M3 是新批次。
- `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`，但 `distance_type_extended_geometry` 仍发布 `PointCurve` 为 deferred diagnostic，`default_or_todo_boundaries` 仍包含 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 及同源 cone / sphere / torus / curve 组合。
- `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已有 checked-in FreeCAD expected，但当前 tests 仍把它们锁在 diagnostic/default boundary。
- S0 已关闭，只回写 C9-M3 README / 总入口 / 矩阵并重命名 S0 step；未改 cad-core source、fixtures、expected 或 tests。S1 已关闭 FreeCAD source authority、cad-core current landing、checked-in expected inventory 和 diagnostics guard 复核；未采 oracle，未改 cad-core source、fixtures、expected 或 tests。S2 已关闭 scope / blocker / backend gap / non-goal 路由；未采 oracle，未改 cad-core source、fixtures、expected 或 tests。S3 已关闭 PointCurve expected/current 复审并路由为 S6 `backend_gap_candidate`；未改 cad-core source、fixtures、expected 或 tests。S4 已关闭 default branch existing expected/current 复审：`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 路由为 S6 `backend_gap_candidate`，缺 input/expected 的 default 扩面族保留 `notCollected` / `native_oracle_required`；未改 cad-core source、fixtures、expected 或 tests。S5-S6 仍待执行。

## S1 关闭证据

- FreeCAD source authority：`AssemblyUtils.cpp::getDistanceType()` 已复核 Vertex / Edge / Face 与 line / circle / plane / cylinder / cone / torus / sphere 的分类顺序；`PointCurve` 来自 Vertex + 非 line Edge，`PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 等来自 Face / Edge default-or-TODO 边界。
- FreeCAD solver mapping：`AssemblyObject.cpp::makeMbdJointDistance()` 中 `PointCurve` 创建 `ASMTPointInPlaneJoint` 并写 `offset`；default branch 创建 `ASMTPlanarJoint` 并写 `offset`。
- cad-core current 落点：`classifyDistanceType()`、`resolveDistanceJointMapping()`、`unsupportedReasonForOndselJoint()`、collector metadata、capability publication、focused tests 和 checked-in expected inventory 已全部写入 `source_candidates` 与 `scope_review_matrix`。
- S1 关闭后队列只剩 S2-S6；`C9M3-BLOCKER-101` 已关闭，S2 继续处理 scope / blocker / non-goal 路由。

## S2 关闭证据

- S2 执行起点为 `48355eae5d`（`48355eae5d docs: 关闭 C9-M3 S1 源码候选矩阵`），起始 status 无输出。
- `C9M3-SCOPE-101..404` 已按 S1 source candidates 路由：`PointCurve` 交 S3 `existing_expected_review`；已有 default expected 交 S4 `existing_expected_review`；缺 expected 的 default branch 交 S4 `native_oracle_required` / 后续显式 `notCollected`；capability / expected metadata / final validation 保持 `release_gate`；diagnostics guard 保持 `diagnostics_guard_review`；primitive frame、persistent solver、GUI/session 与 fixture guessing 保持 `diagnostic_non_goal` / `forbidden_claim`。
- `C9M3-BLOCKER-201` 已关闭；`C9M3-BG-101..601` 与 `C9M3-NG-001..005` 已回链到 scope。S2 未采 oracle、未改 expected、未运行 build，也未删除 `default_or_todo_branch_support`。

## S3 关闭证据

- S3 执行起点为 `696046ca6c`（`696046ca6c docs: 关闭 C9-M3 S2 范围准入路由`），起始 status 无输出。
- `assembly-distance-point-curve-real-solver.freecad.json` 的 native expected 为 solved：`PointCurve` 映射 `ASMTPointInPlaneJoint`，`offset=1.5`，`unsupported_joints=[]`，并写回 `ComponentB` 的 `assembly_set_placement`。
- current `cad-core` 单 fixture recompute 输出仍为 `unsupported_assembly_solver` / `point_curve_diagnostic_boundary`，`documentObjectUpdates=[]`；`test_p8_features.py` 与 `joint_solver.cpp::unsupportedReasonForOndselJoint()` 同步保留该 guard。
- `C9M3-SCOPE-101`、`C9M3-BLOCKER-301`、`C9M3-BG-101` 已回写为 expected-backed mismatch -> S6 `backend_gap_candidate`。S3 不直接发布 `PointCurve` supported，S6 负责移除 guard 或显式保留 retained diagnostic。

## S4 关闭证据

- S4 执行起点为 `213583d369`（`213583d369 docs: 关闭 C9-M3 S3 PointCurve 复审`），起始 status 无输出；S4 执行前队列首项为本步骤，关闭后已重命名为 `6-28-00-11-【已实现】C9-M3-S4-DefaultPlanarBranch批量oracle复审.md`。
- checked-in expected：`assembly-distance-plane-cone-default-boundary.freecad.json`、`assembly-distance-line-cylinder-default-boundary.freecad.json`、`assembly-distance-curve-plane-default-boundary.freecad.json`、`assembly-distance-other-default-boundary.freecad.json` 均来自 FreeCADCmd `1.2.0 revision 20260519`，`native_solver.return_code=0`，`solver_adapter.status=solved`，`unsupported_joints=[]`，并写回 `ComponentB` placement。
- FreeCAD source authority：`AssemblyObject.cpp::makeMbdJointDistance()` 的 `default` 分支创建 `ASMTPlanarJoint` 并写 `offset=getJointDistance(joint)`；四个 fixture 的 `Distance` 均为 `1.5`。checked-in expected metadata 仍保留 `distance_type_mapping_status=default_boundary_not_mapped`、`distance_type_boundary=default_or_todo_boundary`、`known_gap=DTE-BLOCK-006/DTE-NG-003` 和 `nonGoal.ids=["DTE-NG-003"]`。
- current `cad-core` 已运行四个单 fixture recompute 到 `/tmp/c9m3-s4-plane-cone.json`、`/tmp/c9m3-s4-line-cylinder.json`、`/tmp/c9m3-s4-curve-plane.json`、`/tmp/c9m3-s4-other.json`；四者均为 `unsupported_assembly_solver`，message 为 `Ondsel solver adapter keeps default/TODO DistanceType boundary unsupported`，`documentObjectUpdates=[]`。`test_p8_features.py` 与 `joint_solver.cpp` 仍断言 default rows 无 `solver_joint_class` 且 reason 为 `default_boundary_not_mapped`。
- `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 在 `cad-core/fixtures/c3m6` 未发现 input 或 expected；S4 保留 `notCollected` / `native_oracle_required`，不写 backendGap。
- `C9M3-SCOPE-201..203`、`C9M3-BLOCKER-401`、`C9M3-BG-201..203` 已回写。S4 不直接发布 default branch supported，S6 只消费 expected-backed `PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` mismatch，缺 oracle 族先补 native oracle。

## 批次边界

| 方向 | 当前状态 | C9-M3 目标 |
| --- | --- | --- |
| `PointCurve` | S3 证明 native expected solved：`ASMTPointInPlaneJoint`、`offset=1.5`、`ComponentB` placement writeback；current runtime 仍报 `point_curve_diagnostic_boundary` 且无 updates | S6 消费 `backend_gap_candidate`，决定移除 guard 并发布 supported，或显式保留 retained diagnostic。 |
| 已有 default expected | S4 证明 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` native expected solved 且有 `ComponentB` placement writeback；current runtime 仍报 `default_boundary_not_mapped` 且无 updates | S6 消费 expected-backed `backend_gap_candidate`，决定实现 `ASMTPlanarJoint` + `offset` 并发布，或显式保留 retained diagnostic。 |
| default branch 扩面 | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 缺 input/expected | 保留 `notCollected` / `native_oracle_required`；不得只靠一个 fixture 推断全族，也不得写 backendGap。 |
| capability publication | `PointCurve` 与 `default_or_todo_branch_support` 仍在 non-goals | S5/S6 根据 oracle 和实现更新 supported / non-goal / default boundary。 |
| primitive frame generalization | diagnostic non-goal | 不进入 C9-M3。 |

## 入口

- 总入口：`6-28-00-06-C9-M3-AssemblyDistanceTypeDefaultBoundary批次总入口.md`
- 方案：`6-28-00-06-C9-M3-AssemblyDistanceTypeDefaultBoundary批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次 docs/CADCore9.0/README.md
git diff --check
```
