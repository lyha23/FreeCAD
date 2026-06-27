# C9-M3 Assembly DistanceType default boundary 批次

## 定位

C9-M3 承接 C9-M2 关闭后的 Assembly request-local solver 状态，专门处理 `DistanceType` 中仍保留为 diagnostic / default-or-TODO 的边界。它不重开 C9-M1/C9-M2 已经关闭的 marker placement、bundled `offsetPlc`、placement writeback 或 zero Angle fallback。

## 当前状态

- live baseline：C9-M2 关闭 handoff 为 `b981e84f68`（`b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`）；S0 执行起点为 `pwd=/home/user/Chili3DProject/FreeCAD`、`HEAD=04bdd2e561`（`04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`），起始 status 无输出；S1 执行起点为 `HEAD=8f209aab54`（`8f209aab54 docs: 关闭 C9-M3 S0 基线冻结`），起始 status 无输出；S2 执行起点为 `48355eae5d`（`48355eae5d docs: 关闭 C9-M3 S1 源码候选矩阵`），起始 status 无输出；S3 执行起点为 `696046ca6c`（`696046ca6c docs: 关闭 C9-M3 S2 范围准入路由`），起始 status 无输出；S4 执行起点为 `213583d369`（`213583d369 docs: 关闭 C9-M3 S3 PointCurve 复审`），起始 status 无输出；S5 执行起点为 `7b252ed6df`（`7b252ed6df docs: 关闭 C9-M3 S4 DefaultPlanarBranch 复审`），起始 status 无输出。
- C9-M2 queue 已由 S0 复核清空，C9-M3 是新批次。
- `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`，`distance_type_extended_geometry.native_expected_count=18`。`PointCurve` 与 `PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 已进入 supported；`deferred_diagnostic_cases=[]`。
- `default_or_todo_boundaries` 只保留缺 input/expected 的 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。这些 rows 仍是 `notCollected` / `native_oracle_required`，不得继承 S6 supported。
- S0-S5 已关闭并记录 source / scope / oracle / publication gate；S6 已消费 expected-backed backend gap：移除 `PointCurve` diagnostic guard，只对四条 accepted default expected 映射 `ASMTPlanarJoint + offset=distance`，同步 collector / expected metadata / capability / tests，并关闭 `C9M3-BLOCKER-601`。

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

## S5 关闭证据

- S5 执行起点为 `7b252ed6df`（`7b252ed6df docs: 关闭 C9-M3 S4 DefaultPlanarBranch 复审`），起始 status 无输出；S5 执行前队列首项为本步骤，关闭后只应剩余 S6。
- capability / adapter / runtime diagnostics 当前仍保持诚实发布：`PointCurve` 在 `deferred_diagnostic_cases` 和 `non_goals`，`default_or_todo_boundaries` 仍包含 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 及缺 oracle 的 default 扩面族，`default_or_todo_branch_support` 仍是 non-goal。
- S6 必须消费的 publication route 已写入矩阵：`PointCurve` 与 `PlaneCone` / `LineCylinder` / `CurvePlane` / `Other` 是 expected-backed `backend_gap_candidate`；缺 oracle 的 default families 继续是 `notCollected` / `native_oracle_required`，并必须留在 `default_or_todo_boundaries` 或等价可见字段。
- diagnostics guard 已记录：unsupported JointType、`missing_grounded_part`、`missing_marker_placement`、`invalid_assembly_solver_result` 和未采 default boundary 不能因 S6 支持 accepted rows 而静默消失。
- primitive frame、persistent solver、GUI/session 和 adapter string hiding 继续是 non-goal / guard。S5 不把 `known_gaps=[]` 当作隐藏 diagnostic 的理由，也不在 adapter 层伪造 supported。

## S6 关闭证据

- `PointCurve` current parity：runtime 不再走 `point_curve_diagnostic_boundary`，solver DTO 为 `ASMTPointInPlaneJoint` / `offset=1.5`，`assembly-distance-point-curve-real-solver` 的 `placement_updates` 与 checked-in native expected 近似一致。
- accepted default rows current parity：`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 只按 checked-in expected-backed rows 映射为 `ASMTPlanarJoint` / `offset=1.5`；`Other` 保留 public `offset=distance` DTO，并在 request-local Ondsel marker 接线中复现 FreeCAD placement。
- expected / collector：5 个 accepted expected 已删除 stale `known_gap` / `nonGoal.ids=["DTE-NG-003"]`；collector 未来重采这些 rows 不会重加旧 DTE-NG-003，未 accepted default families 仍走 default boundary metadata。
- capability / tests：`native_expected_count=18`，`supported` 增加 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`；`deferred_diagnostic_cases=[]`；缺 oracle rows 留在 `default_or_todo_boundaries`。focused tests 覆盖 accepted parity、unsupported JointType、`missing_grounded_part`、`missing_marker_placement`、capability 中的 `invalid_assembly_solver_result` guard，以及未采 `CurveCylinder` default boundary。
- 验证已通过：`cmake --build build`；`python3 -m unittest tests.test_p8_features tests.test_adapters`。

## 批次边界

| 方向 | 当前状态 | C9-M3 目标 |
| --- | --- | --- |
| `PointCurve` | expected-backed supported；runtime 输出 `ASMTPointInPlaneJoint` / `offset=1.5` 并写回 `ComponentB` placement | 已关闭。 |
| 已有 default expected | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` expected-backed supported；runtime 输出 `ASMTPlanarJoint` / `offset=1.5` 并写回 `ComponentB` placement | 已关闭。 |
| default branch 扩面 | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 缺 input/expected | 保留 `notCollected` / `native_oracle_required`；不得只靠一个 fixture 推断全族，也不得写 backendGap。 |
| capability publication | supported / default boundary / non-goal 字段已按 S6 route 更新；缺 oracle rows 仍可见 | 已关闭。 |
| diagnostics guard | unsupported JointType、`missing_grounded_part`、`missing_marker_placement`、`invalid_assembly_solver_result` 和未采 default boundary 继续可见 | 已关闭。 |
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
