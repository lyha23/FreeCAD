# 【已实现】C9-M3 S5 capability 与 diagnostics 发布准入

## 目标

在 S3/S4 裁决后，审查 capability 和 diagnostics 发布是否诚实：accepted DistanceType 必须出现在 supported / request-local boundary 中；未采集或未接受的 DistanceType 必须继续以 default/todo、diagnostic 或 non-goal 形式可见。

## S5 live baseline

- 执行目录：`/home/user/Chili3DProject/FreeCAD`。
- 起始 HEAD：`7b252ed6df`（`7b252ed6df docs: 关闭 C9-M3 S4 DefaultPlanarBranch 复审`）。
- 起始 `git -c core.quotepath=false status --short -uall` 无输出。
- S5 执行前队列首项为本文件；S5 关闭后只应剩余 S6。

## 检查面

| 面 | 文件 | 检查点 |
| --- | --- | --- |
| capability | `cad-core/src/runtime/capability_contract.cpp` | `distance_type_extended_geometry.supported`、`default_or_todo_boundaries`、`deferred_diagnostic_cases`、`non_goals`。 |
| adapter tests | `cad-core/tests/test_adapters.py` | supported 列表、PointCurve / default branch non-goal 删除或保留、remaining gaps。 |
| runtime diagnostics | `cad-core/tests/test_p8_features.py` | unsupported JointType、missing grounded、missing marker、not collected default boundary 仍有 diagnostics。 |
| expected metadata | `cad-core/fixtures/c3m6/expected` | accepted expected 不再携带 stale `DTE-NG-003`，retained diagnostic expected 保留 delete condition。 |

## S5 发布准入裁决

- `PointCurve` 已由 S3 证明是 expected-backed mismatch：native expected solved 为 `ASMTPointInPlaneJoint` + `offset=1.5` + `ComponentB` placement writeback，current runtime 仍是 `point_curve_diagnostic_boundary` / `unsupported_assembly_solver`。S5 不改 C++、capability 或 expected；S6 必须消费该 `backend_gap_candidate`，要么实现并发布 supported，要么显式保留 retained diagnostic。
- `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已由 S4 证明是 expected-backed mismatch：native expected solved，FreeCAD default branch authority 是 `ASMTPlanarJoint` + `offset=getJointDistance(joint)`，current runtime 仍是 `default_boundary_not_mapped`。S6 只能消费这四条 accepted expected-backed rows，不得把 default branch 整族直接发布 supported。
- 缺 oracle 的 default families：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 继续是 `notCollected` / `native_oracle_required`。S6 若支持 accepted rows，也必须让这些 rows 留在 `default_or_todo_boundaries` 或等价可见字段中。
- primitive frame、persistent solver state、GUI/session 和 adapter string hiding 继续是 non-goal / guard；不得用 `known_gaps=[]` 或 adapter 文案改写隐藏 runtime diagnostic。

## diagnostics guard

S6 支持 accepted rows 之后，以下 guard 仍必须可见并有 focused diagnostics 约束：

- unsupported JointType：仍以 `unsupported_assembly_solver` / `unsupported_joint_type` 路由，不因 DistanceType 发布而被吞掉。
- missing grounded：`missing_grounded_part` 仍是 solver validation diagnostic。
- missing marker：`missing_marker_placement` 仍是 unsupported reason。
- invalid solver output：`invalid_assembly_solver_result` 仍在 validation diagnostics 中。
- 未采 default boundary：`notCollected` / `native_oracle_required` 行不得因为 `PointCurve` 或四条 default expected 被实现而静默消失。

## S6 publication route

| route | rows | S6 obligation |
| --- | --- | --- |
| `backend_gap_candidate` | `PointCurve` | 实现或明确 retained diagnostic；若实现，移除 `point_curve_diagnostic_boundary` guard、同步 focused tests、capability supported / non-goal 和 expected metadata。 |
| `backend_gap_candidate` | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` | 实现 `ASMTPlanarJoint` + `offset` 或明确 retained diagnostic；若实现，只发布这四条 accepted rows。 |
| `notCollected` / `native_oracle_required` | 缺 input/expected 的 default 扩面族 | 先采 native oracle，再判断 supported 或 backend gap；S6 不得继承支持。 |
| non-goal / guard | primitive frame、persistent solver、GUI/session、adapter string hiding | 保持 visible non-goal，除非另有产品 scope。 |

## 必须回写的矩阵行

- `C9M3-SCOPE-301..304`
- `C9M3-BLOCKER-501`
- `C9M3-BG-301..501`

这些行已回写为 S5 publication route / diagnostics guard / S6 readiness；S5 不改变 runtime 发布状态。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'distance_type_extended_geometry|PointCurve|default_or_todo_boundaries|deferred_diagnostic_cases|default_or_todo_branch_support|unsupported_assembly_solver|missing_grounded_part|missing_marker_placement|invalid_assembly_solver_result|backend_gap_candidate|notCollected|native_oracle_required|diagnostics_guard' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py cad-core/fixtures/c3m6 docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- S5 明确哪些 capability 字段由 S6 改，哪些保持 diagnostic/non-goal。
- `default_or_todo_branch_support` 不得在未实现 / 未采 expected 前从 non-goals 消失。
- diagnostics guard 不因 `PointCurve` 或 default branch 支持而静默消失。
- 若 capability 只需发布同步，必须说明没有 C++ backend gap 的证据。

## 非目标

- 不在 adapter 层伪造支持状态。
- 不把 `known_gaps=[]` 当成隐藏 diagnostic 的理由。
- 不把 missing oracle case 写成 supported。
