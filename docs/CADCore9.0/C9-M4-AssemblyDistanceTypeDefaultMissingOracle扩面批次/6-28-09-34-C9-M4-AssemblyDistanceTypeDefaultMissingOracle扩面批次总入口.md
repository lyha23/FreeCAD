# C9-M4 Assembly DistanceType default missing oracle 扩面批次总入口

本文是 `docs/CADCore9.0` 下的 C9-M4 实施主线。它承接 C9-M3 队列清空后的 live 状态，专门处理 Assembly `DistanceType` 中起点仍保留在 `default_or_todo_boundaries` 的 13 个缺 input / expected 行：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。S6 后这些行已全部发布为 expected-backed supported。

## 主线目标

- 不重开 C9-M1 / C9-M2 / C9-M3 已关闭的 request-local solver、marker、writeback、`PointCurve` 或四条 accepted default expected。
- 以 FreeCAD `AssemblyUtils.cpp::getDistanceType()` 和 `AssemblyObject.cpp::makeMbdJointDistance()` 为 source authority，批量补齐缺 oracle default branch 的 fixture / native expected / current comparison / publication route。
- 把缺 oracle 行先推进到 `native_oracle_required` 的可验证状态；只有采到 native expected 且 current mismatch 的行，才进入 S6 supported 实现闸门。
- 不从 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 的 C9-M3 结论继承整族支持。

## 当前基线

- S0 live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，当前 HEAD 为 `435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`。
- C9-M3 `工作步骤细分` 队列为空；S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 `docs/CADCore9.0/README.md` 修改和本 C9-M4 seed 包未提交。
- live capability 中 `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=31`、`default_or_todo_boundaries=[]`、`deferred_diagnostic_cases=[]`。
- `distance_type_extended_geometry.supported` 已包含 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`。
- `default_or_todo_boundaries` 在 S0 仍保留 13 个缺 input / expected 行；S3/S4/S5 已为全部 13 行补齐 native expected，S6 已消费全部 expected-backed mismatch 并发布为 supported。
- S0 forbidden claims 已冻结：缺 oracle row 不得写 supported 或 backendGap；不得继承 C9-M3 accepted default rows；不得靠 fixture 名称、bbox、几何排序、adapter string rewrite 或输出修剪隐藏缺口；GUI/session、persistent solver state、cross-request placement cache、primitive frame generalization 仍是 non-goal。
- S1 已关闭 source authority/current coverage：FreeCAD `getDistanceType()` 覆盖 13 个 missing default rows 的 Face/Face、Vertex/Face、Edge/Face 分类和 `swapJCS` ordering；FreeCAD `makeMbdJointDistance()` default branch 是 `ASMTPlanarJoint + offset=getJointDistance(joint)`；S6 已把这些 rows 从缺 oracle default boundary 推进到 supported。
- S2 已关闭 scope / blocker / non-goal 初始路由：FaceCone 交 S3，Point / Line + Surface 交 S4，CurveSurface 交 S5；缺 input / expected rows 只能保持 `native_oracle_required` / `notCollected`，S6 只消费 native expected-backed current mismatch。capability/diagnostics 是 `release_gate` / `diagnostics_guard_review`，GUI/session/cache/guessing/string rewrite/output pruning 保持 non-goal 或 guard。
- S3 已关闭 FaceCone native oracle：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + placement writeback；S6 已发布为 `ASMTPlanarJoint + offset=getJointDistance`。
- S4 已关闭 PointLineSurface native oracle：`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + placement writeback；S6 已发布为 `ASMTPlanarJoint + offset=getJointDistance`，保持 Face-first `swapJCS` ordering。
- S5 已关闭 CurveSurface native oracle：`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 均新增 c3m6 input / expected，FreeCADCmd `1.2.0 revision 20260519` native expected 为 solved + Face-first `swapJCS` ordering + placement writeback；S6 已发布支持，`part_workbench.conic_curves.distance_type_publication.default_or_todo_boundaries=[]`。
- S6 已关闭 Oracle 实现与发布闸门：13 个 C9-M4 expected 不再带 diagnostic-only `known_gap` / `nonGoal`，runtime 与 native expected parity，capability `native_expected_count=31`、`default_or_todo_boundaries=[]`。

## 证明链条

```text
C9-M3 queue empty
  -> live capability default_or_todo inventory
  -> FreeCAD DistanceType source authority
  -> scope review / nonGoal / blocker queue
  -> face-cone native oracle
  -> point-line surface native oracle
  -> curve-surface native oracle
  -> S6 code landing and supported publication
  -> capability / diagnostics / queue release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DistanceType 分类 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 按 Vertex / Edge / Face 与 line / circle / plane / cylinder / cone / torus / sphere 分类，并在需要时 `swapJCS(joint)`。 |
| face-cone default family | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 来自 Face / Face 分类。 |
| point-line surface default family | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` 来自 Vertex / Face 或 line Edge / Face 分类。 |
| curve-surface default family | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 来自 non-line Edge / Face 分类。 |
| default solver | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | `default` 创建 `ASMTPlanarJoint` 并写 `offset = getJointDistance(joint)`。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| DistanceType DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`recordDistanceTypeEvidence()`、`resolveDistanceJointMapping()`、`unsupportedReasonForOndselJoint()` 和 real Ondsel joint construction。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 采集 DistanceType native expected、标注 default boundary metadata、维护 accepted / retained rows。 |
| fixtures | `cad-core/fixtures/c3m6` | Assembly DistanceType request-local input fixture 与 `expected/*.freecad.json`。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | `distance_type_extended_geometry.supported`、`default_or_todo_boundaries`、non-goal 与 diagnostics publication。 |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py` | focused expected parity、diagnostics guard、capability smoke。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、当前状态、批次边界和验收分层。 |
| 方案 | `6-28-09-34-C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次方案.md` | C9-M4 实施策略。 |
| 工作步骤总入口 | `工作步骤细分/6-28-09-34-【已实现】C9-M4工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-28-09-35-【已实现】C9-M4-S0-live基线与缺oracle声明口径冻结.md` | 已冻结 live baseline、claim 和 forbidden claim。 |
| S1 | `工作步骤细分/6-28-09-36-【已实现】C9-M4-S1-FreeCAD源码与current覆盖候选.md` | 已关闭 FreeCAD source authority、current cad-core landing 与 missing inventory。 |
| S2 | `工作步骤细分/6-28-09-37-【已实现】C9-M4-S2-范围准入与blocker矩阵.md` | 已关闭 scope / blocker / non-goal / backend gap 初始路由。 |
| S3 | `工作步骤细分/6-28-09-38-【已实现】C9-M4-S3-FaceCone族native-oracle复审.md` | 已关闭 `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` native oracle，四行已由 S6 发布 supported。 |
| S4 | `工作步骤细分/6-28-09-39-【已实现】C9-M4-S4-PointLineSurface族native-oracle复审.md` | 已关闭 `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` native oracle，五行已由 S6 发布 supported。 |
| S5 | `工作步骤细分/6-28-09-40-【已实现】C9-M4-S5-CurveSurface族native-oracle复审.md` | 已关闭 `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` native oracle，四行已由 S6 发布 supported。 |
| S6 | `工作步骤细分/6-28-09-41-【已实现】C9-M4-S6-Oracle实现与发布闸门.md` | 已关闭 expected-backed default planar implementation 和 release gate。 |
| source candidates | `矩阵/c9m4_distance_type_default_missing_oracle_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m4_distance_type_default_missing_oracle_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m4_distance_type_default_missing_oracle_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m4_distance_type_default_missing_oracle_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m4_distance_type_default_missing_oracle_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m4_distance_type_default_missing_oracle_validation_matrix.tsv` | 分层验收命令。 |

当前 S0-S6 已关闭；矩阵记录 source/current coverage、scope 准入、oracle 路由和 S6 supported 发布结论。
