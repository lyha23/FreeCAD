# C9-M4 Assembly DistanceType default missing oracle 扩面批次总入口

本文是 `docs/CADCore9.0` 下的 C9-M4 实施主线。它承接 C9-M3 队列清空后的 live 状态，专门处理 Assembly `DistanceType` 中仍保留在 `default_or_todo_boundaries` 的 13 个缺 input / expected 行：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。

## 主线目标

- 不重开 C9-M1 / C9-M2 / C9-M3 已关闭的 request-local solver、marker、writeback、`PointCurve` 或四条 accepted default expected。
- 以 FreeCAD `AssemblyUtils.cpp::getDistanceType()` 和 `AssemblyObject.cpp::makeMbdJointDistance()` 为 source authority，批量补齐缺 oracle default branch 的 fixture / native expected / current comparison / publication route。
- 把缺 oracle 行先推进到 `native_oracle_required` 的可验证状态；只有采到 native expected 且 current mismatch 的行，才进入 S6 `backend_gap_candidate` 或 supported 实现闸门。
- 不从 `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 的 C9-M3 结论继承整族支持。

## 当前基线

- S0 live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，当前 HEAD 为 `435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`。
- C9-M3 `工作步骤细分` 队列为空；S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 `docs/CADCore9.0/README.md` 修改和本 C9-M4 seed 包未提交。
- live capability 中 `assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=18`、`deferred_diagnostic_cases=[]`。
- `distance_type_extended_geometry.supported` 已包含 `PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`。
- `default_or_todo_boundaries` 仍保留 13 个缺 input / expected 行；S0 对 `cad-core/fixtures/c3m6` 的 inventory scan 对这些名称无命中，这些行是 C9-M4 的唯一默认目标。
- S0 forbidden claims 已冻结：缺 oracle row 不得写 supported 或 backendGap；不得继承 C9-M3 accepted default rows；不得靠 fixture 名称、bbox、几何排序、adapter string rewrite 或输出修剪隐藏缺口；GUI/session、persistent solver state、cross-request placement cache、primitive frame generalization 仍是 non-goal。

## 证明链条

```text
C9-M3 queue empty
  -> live capability default_or_todo inventory
  -> FreeCAD DistanceType source authority
  -> scope review / nonGoal / blocker queue
  -> face-cone native oracle
  -> point-line surface native oracle
  -> curve-surface native oracle
  -> S6 code landing or retained notCollected route
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
| S1 | `工作步骤细分/6-28-09-36-C9-M4-S1-FreeCAD源码与current覆盖候选.md` | FreeCAD source authority、current cad-core landing 与 missing inventory。 |
| S2 | `工作步骤细分/6-28-09-37-C9-M4-S2-范围准入与blocker矩阵.md` | scope / blocker / non-goal / backend gap 初始路由。 |
| S3 | `工作步骤细分/6-28-09-38-C9-M4-S3-FaceCone族native-oracle复审.md` | `CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` native oracle。 |
| S4 | `工作步骤细分/6-28-09-39-C9-M4-S4-PointLineSurface族native-oracle复审.md` | `PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus` native oracle。 |
| S5 | `工作步骤细分/6-28-09-40-C9-M4-S5-CurveSurface族native-oracle复审.md` | `CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` native oracle。 |
| S6 | `工作步骤细分/6-28-09-41-C9-M4-S6-Oracle实现与发布闸门.md` | 根据 oracle 结果实现或 release gate。 |
| source candidates | `矩阵/c9m4_distance_type_default_missing_oracle_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m4_distance_type_default_missing_oracle_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m4_distance_type_default_missing_oracle_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m4_distance_type_default_missing_oracle_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m4_distance_type_default_missing_oracle_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m4_distance_type_default_missing_oracle_validation_matrix.tsv` | 分层验收命令。 |

当前 S0 已关闭，S1-S6 仍为待执行状态；矩阵是 seed，不是支持性发布结论。
