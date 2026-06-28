# C9-M4 Assembly DistanceType default missing oracle 扩面批次

## 定位

C9-M4 承接 C9-M3 关闭后的 Assembly DistanceType 状态，处理 `default_or_todo_boundaries` 中仍缺 input / expected 的 13 个 default branch rows。它的第一目标是补 native oracle 和路由证据，不把未采集 rows 直接写成 supported。

## 当前状态

- S0 live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=435f3f26b9`（`435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`）。
- S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 `docs/CADCore9.0/README.md` 修改和未提交的 C9-M4 seed 包；这些是本轮上下文，不是可回退的无关改动。
- C9-M3 `工作步骤细分` 队列已复核为空；C9-M4 S0 已冻结起点，S1 已关闭 source authority/current coverage 候选，S2 已关闭 scope / blocker / non-goal 初始路由，S3 已关闭 FaceCone native oracle，S4-S6 仍待执行。
- current capability 口径：`assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=18`、`deferred_diagnostic_cases=[]`。
- `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已 supported；C9-M4 不重开这些 C9-M3 accepted rows。
- S3 已为 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 新增 c3m6 input / expected，native FreeCAD expected 均为 solved + placement writeback，current cad-core 保持 `unsupported/default_boundary_not_mapped`，因此四行路由为 S6 `backend_gap_candidate`。其余 9 个 rows 仍缺 input / expected，保持 `native_oracle_required` / `notCollected`。
- S1 source authority 复核已关闭：FreeCAD `getDistanceType()` 负责 13 rows 的 Face/Face、Vertex/Face、Edge/Face 分类与 `swapJCS` ordering；FreeCAD `makeMbdJointDistance()` default branch 创建 `ASMTPlanarJoint` 并写 `offset=getJointDistance(joint)`；current cad-core 只把 C9-M3 accepted defaults 映射为 supported，13 个 C9-M4 rows 继续保持 `default_boundary_not_mapped/default_or_todo_boundary`。
- S2 scope 路由已关闭：FaceCone 交 S3，Point / Line + Surface 交 S4，CurveSurface 交 S5；缺 input / expected 的 row 只能保持 `native_oracle_required` / `notCollected`，只有 S3-S5 采到 native expected 且 current mismatch 后，S6 才能写 `backend_gap_candidate` 或实现。capability/diagnostics 是 `release_gate` / `diagnostics_guard_review`；GUI/session、persistent solver state、cross-request placement cache、primitive-frame generalization、inherited C9-M3 support、fixture-name/bbox/geometry-order guessing、adapter string rewrite、output pruning 仍是 `diagnostic_non_goal` 或 guard。
- forbidden claims：不得继承 C9-M3 supported，不得靠 fixture 名称、bbox、几何排序、adapter 文案或输出修剪支持 default branch；persistent solver state、cross-request placement cache、non-AssemblyLink primitive frame generalization 仍是 non-goal。

## 批次边界

| 方向 | 当前状态 | C9-M4 目标 |
| --- | --- | --- |
| Face / Face cone family | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` expected-backed current unsupported | S3 已关闭，四行交 S6 `backend_gap_candidate`。 |
| Point / Line + Surface family | `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 缺 input / expected | S4 采集 native expected 或记录 notCollected。 |
| Curve + Surface family | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 缺 input / expected | S5 采集 native expected 或记录 notCollected，并同步 conic publication mirror。 |
| S6 implementation gate | 仅 accepted native expected rows 可进入实现 | 实现 expected-backed mismatch；missing rows 保持 visible default boundary；release gate 不等同实现证据。 |

## 入口

- 总入口：`6-28-09-34-C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次总入口.md`
- 方案：`6-28-09-34-C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次 docs/CADCore9.0/README.md
git diff --check
```
