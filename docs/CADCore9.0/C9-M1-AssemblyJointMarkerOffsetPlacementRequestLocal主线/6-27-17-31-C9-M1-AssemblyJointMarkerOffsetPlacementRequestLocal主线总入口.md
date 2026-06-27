# C9-M1 Assembly Joint marker / offset placement request-local 主线总入口

本文是 `docs/CADCore9.0` 下的 C9-M1 实施主线。它承接 C8-M7 完成后的 live 状态，聚焦 Assembly Joint marker placement、`offsetPlc`、request-local solver placement writeback 与 capability publication。

## 主线目标

- 冻结 C9-M1 声明口径：只处理 Assembly request-local marker / placement 扩面，不重开完整 Link 账本、持久写回或 cross-request solver session。
- 复核 FreeCAD `AssemblyObject::handleOneSideOfJoint()`、`runPreDrag()`、`setNewPlacements()` 与 `JointObject.py` schema。
- 复核 current `cad-core` 的 `joint_solver.cpp`、`assembly_utils.cpp`、capability 和 C3M6 / P8 focused tests。
- 把 `non_identity_bundled_offsetPlc`、`non_assembly_link_subshape_primitive_frame_generalization`、zero Angle fallback 和 placement writeback 分别路由为 `already_covered`、`oracle_candidate`、`backend_gap_candidate`、`known_gap_retained` 或 `diagnostic_non_goal`。
- S6 只消费有 FreeCAD 依据和 current mismatch 的 implementation gate；不靠 fixture 名、几何排序或 adapter 字符串隐藏能力差异。

## 当前基线

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ebd6fd1243`（`ebd6fd1243 docs: 新增 C9-M1 Assembly Joint marker 方案包`），开始工作区干净。
- C8-M1 到 C8-M7 工作步骤队列已复核为空：各队列脚本仅输出表头。
- live capability 已关闭 ImportShape residual；`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 仍是 `oracle_blocked` known gap，不进入 C9-M1。
- current `assembly.ondsel_solver_adapter.subshape_marker_placement` 已覆盖 object / Vertex / Edge / Face / mixed、identity-offset AssemblyLink subset、real Ondsel marker consumption 与 placement update native parity；但 capability 仍把 `non_identity_bundled_offsetPlc` 和 `non_assembly_link_subshape_primitive_frame_generalization` 放入 non-goals。

## 证明链条

```text
live 声明口径
  -> FreeCAD Assembly source authority
  -> current cad-core coverage / expected inventory
  -> scope review / blocker queue / nonGoal registry
  -> marker placement + offsetPlc 专项复审
  -> runPreDrag placement writeback 专项复审
  -> capability / diagnostics 发布准入
  -> S6 code landing or no-code release gate
```

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、基线、落点和验收分层。 |
| 方案 | `6-27-17-31-C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal方案.md` | 主线范围和实施策略。 |
| 工作步骤总入口 | `工作步骤细分/6-27-17-32-【已实现】C9-M1工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-27-17-33-【已实现】C9-M1-S0-live基线与声明口径冻结.md` | 冻结 live baseline 和允许声明。 |
| S1 | `工作步骤细分/6-27-17-34-【已实现】C9-M1-S1-FreeCAD源码与current覆盖候选.md` | 源码候选和 current coverage 复核。 |
| S2 | `工作步骤细分/6-27-17-35-【已实现】C9-M1-S2-范围准入与blocker矩阵.md` | scope / blocker / non-goal 路由。 |
| S3 | `工作步骤细分/6-27-17-36-【已实现】C9-M1-S3-markerPlacement与offsetPlc复审.md` | marker placement 与 `offsetPlc` oracle / gap 裁决。 |
| S4 | `工作步骤细分/6-27-17-37-【已实现】C9-M1-S4-runPreDragPlacementWriteback复审.md` | solver placement writeback 生命周期复审。 |
| S5 | `工作步骤细分/6-27-17-38-【已实现】C9-M1-S5-capability与diagnostics发布准入.md` | capability / diagnostics / non-goal 发布准入。 |
| S6 | `工作步骤细分/6-27-17-39-C9-M1-S6-Oracle实现与发布闸门.md` | 实现或 no-code release gate。 |
| source candidates | `矩阵/c9m1_assembly_marker_offset_source_candidates.tsv` | FreeCAD / cad-core source authority 种子。 |
| scope review | `矩阵/c9m1_assembly_marker_offset_scope_review_matrix.tsv` | scope 状态与 owner step。 |
| blocker queue | `矩阵/c9m1_assembly_marker_offset_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m1_assembly_marker_offset_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m1_assembly_marker_offset_backend_gap_classification.tsv` | backendGap / oracle / releaseGate 分类。 |
| validation matrix | `矩阵/c9m1_assembly_marker_offset_validation_matrix.tsv` | 分层验收命令。 |

当前 S0 已完成 live 基线与声明口径冻结；S1 已完成 FreeCAD source 与 current coverage 候选复核，`C9M1-BLOCKER-101` 已关闭。S2 已完成 scope / blocker / non-goal 路由，`C9M1-BLOCKER-201` 已关闭。S3 已完成 marker placement 与 `offsetPlc` 复审，`C9M1-BLOCKER-301` 已关闭。S4 已完成 request-local placement writeback、zero Angle known-gap 与 unsupported diagnostic 复审，`C9M1-BLOCKER-401` 已关闭。S5 已完成 capability / diagnostics 发布准入，`C9M1-BLOCKER-501` 已关闭；S6 只消费 S5 裁决的 no-code release gate，不需要 build、native oracle refresh、fixture 或 runtime C++ patch，除非 focused capability smoke 或 adapter tests 发现发布口径与 S3/S4 route 冲突。
