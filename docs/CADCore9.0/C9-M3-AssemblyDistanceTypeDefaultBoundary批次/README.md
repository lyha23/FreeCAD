# C9-M3 Assembly DistanceType default boundary 批次

## 定位

C9-M3 承接 C9-M2 关闭后的 Assembly request-local solver 状态，专门处理 `DistanceType` 中仍保留为 diagnostic / default-or-TODO 的边界。它不重开 C9-M1/C9-M2 已经关闭的 marker placement、bundled `offsetPlc`、placement writeback 或 zero Angle fallback。

## 当前状态

- live baseline：C9-M2 关闭 handoff 为 `b981e84f68`（`b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`）；S0 执行起点为 `pwd=/home/user/Chili3DProject/FreeCAD`、`HEAD=04bdd2e561`（`04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`），起始 status 无输出。
- C9-M2 queue 已由 S0 复核清空，C9-M3 是新批次。
- `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`，但 `distance_type_extended_geometry` 仍发布 `PointCurve` 为 deferred diagnostic，`default_or_todo_boundaries` 仍包含 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 及同源 cone / sphere / torus / curve 组合。
- `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已有 checked-in FreeCAD expected，但当前 tests 仍把它们锁在 diagnostic/default boundary。
- S0 已关闭，只回写 C9-M3 README / 总入口 / 矩阵并重命名 S0 step；未改 cad-core source、fixtures、expected 或 tests。S1-S6 仍待执行。

## 批次边界

| 方向 | 当前状态 | C9-M3 目标 |
| --- | --- | --- |
| `PointCurve` | native expected 存在；cad-core DTO 已映射 `ASMTPointInPlaneJoint`，但 runtime guard 仍报 diagnostic | S3 判定是否可提升为 expected-backed supported；S6 消费 backend gap。 |
| 已有 default expected | `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已有 expected，但 current route 是 `default_boundary_not_mapped` | S4 激活 / 比较 expected，形成 supported、backendGap 或 retained diagnostic。 |
| default branch 扩面 | 多个 cone / sphere / torus / curve 组合在 capability 中仍为 default_or_todo boundaries | S4 批量采集或明确 `notCollected`；不得只靠一个 fixture 推断全族。 |
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
