# C9-M4 Assembly DistanceType default missing oracle 扩面批次

## 定位

C9-M4 承接 C9-M3 关闭后的 Assembly DistanceType 状态，处理 `default_or_todo_boundaries` 中仍缺 input / expected 的 13 个 default branch rows。它的第一目标是补 native oracle 和路由证据，不把未采集 rows 直接写成 supported。

## 当前状态

- S0 live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=435f3f26b9`（`435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`）。
- S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 `docs/CADCore9.0/README.md` 修改和未提交的 C9-M4 seed 包；这些是本轮上下文，不是可回退的无关改动。
- C9-M3 `工作步骤细分` 队列已复核为空；C9-M4 S0 已冻结起点，S1 已关闭 source authority/current coverage 候选，S2 已关闭 scope / blocker / non-goal 初始路由，S3 已关闭 FaceCone native oracle，S4 已关闭 PointLineSurface native oracle，S5 已关闭 CurveSurface native oracle，S6 已关闭实现与发布闸门。
- current capability 口径：`assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=31`、`distance_type_extended_geometry.default_or_todo_boundaries=[]`、`part_workbench.conic_curves.distance_type_publication.default_or_todo_boundaries=[]`。
- `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已 supported；C9-M4 不重开这些 C9-M3 accepted rows。
- S3 已为 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 新增 c3m6 input / expected；S4 已为 `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 新增 c3m6 input / expected；S5 已为 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 新增 c3m6 input / expected。S6 已消费全部 13 个 expected-backed mismatch，runtime 与 native expected 均为 solved + placement writeback。
- S1 source authority 复核已关闭：FreeCAD `getDistanceType()` 负责 13 rows 的 Face/Face、Vertex/Face、Edge/Face 分类与 `swapJCS` ordering；FreeCAD `makeMbdJointDistance()` default branch 创建 `ASMTPlanarJoint` 并写 `offset=getJointDistance(joint)`；current cad-core 已把 C9-M3 四行与 C9-M4 十三行合并为 expected-backed default planar supported set。
- S2 scope 路由已关闭：FaceCone 交 S3，Point / Line + Surface 交 S4，CurveSurface 交 S5；缺 input / expected 的 row 只能保持 `native_oracle_required` / `notCollected`，S6 只消费 native expected-backed mismatch。capability/diagnostics 是 `release_gate` / `diagnostics_guard_review`；GUI/session、persistent solver state、cross-request placement cache、primitive-frame generalization、inherited C9-M3 support、fixture-name/bbox/geometry-order guessing、adapter string rewrite、output pruning 仍是 `diagnostic_non_goal` 或 guard。
- forbidden claims：不得继承 C9-M3 supported，不得靠 fixture 名称、bbox、几何排序、adapter 文案或输出修剪支持 default branch；persistent solver state、cross-request placement cache、non-AssemblyLink primitive frame generalization 仍是 non-goal。

## 批次边界

| 方向 | 当前状态 | C9-M4 目标 |
| --- | --- | --- |
| Face / Face cone family | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` expected-backed supported | S6 已发布为 `ASMTPlanarJoint + offset=getJointDistance`。 |
| Point / Line + Surface family | `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` expected-backed supported | S6 已发布为 `ASMTPlanarJoint + offset=getJointDistance`，保持 FreeCAD `swapJCS` solver ordering。 |
| Curve + Surface family | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` expected-backed supported | S6 已发布为 `ASMTPlanarJoint + offset=getJointDistance`；conic publication mirror default list 已清空。 |
| S6 implementation gate | 全部 13 个 native expected-backed rows 已进入实现 | `default_or_todo_boundaries=[]`，diagnostics guard 仍覆盖 unsupported JointType、missing marker、missing grounded 和 invalid solver output。 |

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
