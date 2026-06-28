# C10-M2 PartDesign DressUp / Hole TopoHistory 批次总入口

本文是 `docs/CADCore10.0` 下的 C10-M2 实施主线。它承接 C10-M1 `SketchOpenWireInternalFaceStableSelector` 队列关闭后的 live 状态，专门复核 PartDesign DressUp 与 Hole 的 request-local `NamedShape` / `ElementMap` / MapperHistory 生产者账本。

## 主线目标

- 从 current capability 出发，确认 C10-M2 不重开 CopyOnChange，也不把 DressUp stale `ReferenceShadow` / Base recovery 的 oracle-blocked 行误写成 supported。
- 复核 FreeCAD `DressUp::getAddSubShape()`、`Fillet::execute()`、`Chamfer::execute()`、`Draft::execute()`、`Thickness::execute()`、`Hole::execute()` / `Hole::findHoles()` / `Hole::makeThread()` 调用链。
- 盘点 current cad-core 在 DressUp / Hole 的 first slice 覆盖：AddSubShape slot、multi-selection history、Draft / Thickness 参数变体、Hole profile-source / ModelThread / head-cut / Body subtractive history。
- 对 P7 已有 expected-backed 行做第二轮准入：只有 source-backed native oracle 或 focused test 证明 current mismatch 时，才由 S6 转成 C++ 实现任务。
- 明确禁止 raw `FaceN`、bbox、面积、输出顺序、source index、fixture 名称分支或 adapter 端修剪来推断 stable face / edge ownership。

## 当前基线

- 起点仓库：`/home/user/Chili3DProject/FreeCAD`。
- 起点 HEAD：`2420e3b842`（`docs: 完成 C10-M1 S6 发布闸门`）。
- 起点工作区：生成本包前 `git status --short` 为空。
- C10-M1 队列已关闭，`docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分` 无待执行步骤。
- C10-M2 S0 live 基线已冻结：起点 `HEAD=2420e3b842`（`docs: 完成 C10-M1 S6 发布闸门`），起点 dirty boundary 仅包含本包 docs / matrices 和 `docs/CADCore10.0/README.md` 的 in-scope 变更。
- current capability 中 `part_design.hole.history.status=element_map_freeze_first_slice`，`topo_history.producer_matrix.dressup.status=done_first_slice`，`topo_history.producer_matrix.hole.status=done_first_slice`，三者均没有 active `remaining`。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 仍是 retained known gap / `oracle_blocked`，不进入 C10-M2。
- `C10M2-BLOCKER-000` 已关闭为 `closed_s0`；`C10M2-SCOPE-001` 保持 `baseline_frozen_s0` docs-only release baseline，S6 只复核 drift，不打开 C++ gate。
- C10-M2 S1 source authority 已复核：`C10M2-SRC-101..204` 均有 live FreeCAD 或 current cad-core/tests path、symbol 和 concise evidence；`C10M2-BLOCKER-101` 已关闭为 `closed_s1`。S1 未采 native oracle，未改 C++，未升级 `supported` / `backendGap`。
- C10-M2 S2 scope 准入已完成：`C10M2-SCOPE-001/101/102/201/202/301/401` 均有合法 `current_status`、owner `next_step` 和 `close_condition`；`C10M2-BLOCKER-201` 已关闭为 `closed_s2`。S2 未采 native oracle，未改 C++，未新增 implementation row 或 `backend_gap_requires_implementation`。
- C10-M2 S3 DressUp producer-history 复审已完成：`C10M2-SCOPE-101=expected_backed_no_gap`，`C10M2-SCOPE-102=no_gap`，`C10M2-BLOCKER-301=closed_s3`，`C10M2-CAT-101=no_gap`。S3 未发现 expected-backed current mismatch，未改 C++、tests、fixtures、expected 或 capability；S6 只发布 no-code no-gap。
- C10-M2 S4 Hole producer-history 复审已完成：`C10M2-SCOPE-201=expected_backed_no_gap`，`C10M2-SCOPE-202=expected_backed_no_gap`，`C10M2-BLOCKER-401=closed_s4`，`C10M2-CAT-102=no_gap`。S4 未发现 expected-backed current mismatch，未改 C++、tests、fixtures、expected 或 capability；S6 只发布 no-code no-gap。
- C10-M2 S5 cross-feature old-reference / diagnostic boundary 复审已完成：`C10M2-SCOPE-301=diagnostic_retained`，`C10M2-BLOCKER-501=closed_s5`，`C10M2-CAT-103=diagnostic_retained`。S5 确认 Body boolean、transformed copy 和 Link retag 的 split / deleted / merge 只发布 structured diagnostics / retained history，stale `ReferenceShadow` / Base recovery 仍缺 native observable `ShadowSub` / `ReferenceShadow` evidence；未发现 expected-backed current mismatch，未开 S6 C++ row。

## 状态词典

| 状态词 | C10-M2 口径 |
| --- | --- |
| `baseline_frozen_s0` | 只表示 S0 已冻结 live 起点、允许声明、forbidden claims 和验证命令。 |
| `closed_s0` | 只表示 S0 blocker 关闭；不表示 DressUp / Hole supported 范围扩大。 |
| `source_audit_pending` | 只表示 FreeCAD authority / current cad-core 落点待复核，不等于 supported 或 backend gap。 |
| `oracle_candidate` | 需要 FreeCAD native expected 或既有 checked-in expected 证明语义，不从 current 输出倒推。 |
| `current_mismatch_candidate` | 已有 authority / expected 但尚未确认 current cad-core 是否偏离。 |
| `backend_gap_candidate` | 只有 S3-S5 证明 current mismatch 后，S6 才能转为 C++ 实现。 |
| `diagnostic_retained` | 保留稳定诊断或 known gap，不声明 supported，不做几何猜测。 |
| `notCollected` | 只表示还没有可观察 oracle；不是实现任务。 |
| `release_gate` | S6 对已证明的 no-gap、retained diagnostic 或实现结果做发布收口。 |

## 证明链条

```text
C10-M1 queue empty
  -> current capability DressUp/Hole first slice
  -> FreeCAD source candidates
  -> scope review / blocker / non-goal / backend-gap matrices
  -> DressUp producer-history review
  -> Hole producer-history review
  -> cross-feature old-reference and diagnostic boundary review
  -> S6 code landing or no-code release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DressUp base / slot | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | 生成 AddSubShape slot，后续 Body / transformed family 不能只看最终 replacement solid。 |
| Fillet | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()` | 消费 Base edge / face selection、连续边和 OCCT fillet maker。 |
| Chamfer | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()` | 消费 Equal distance、Two distances、Distance and Angle、FlipDirection 等参数路径。 |
| Draft | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute()` | 解析 neutral plane / pull direction / selected faces。 |
| Thickness | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp::Thickness::execute()` | 解析 selected faces、mode、join、intersection、多 solid fuse history。 |
| Hole execute | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::execute()` | 组装 profile、head cut、thread、ModelThread 和 subtractive Body cut 输入。 |
| Hole producer history | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::findHoles()` | 调用 `makeShapeWithElementMap(protoHole, mapper, {baseshape})` 传播 profile/source tool face history。 |
| Hole ModelThread | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp::Hole::makeThread()` | 构建实体螺纹工具，后续必须保留 tool face / compound tool shape history。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| DressUp shared support | `cad-core/src/part_design/feature_dress_up_support.h`、`cad-core/src/part_design/feature_dress_up.cpp` | 解析 Base / selection / AddSubShape slot，发布 DressUp result。 |
| Fillet / Chamfer | `cad-core/src/part_design/feature_fillet.cpp`、`cad-core/src/part_design/feature_chamfer.cpp` | OCCT maker、selection history、refine 与 AddSubShape slot publication。 |
| Draft / Thickness | `cad-core/src/part_design/feature_draft.cpp`、`cad-core/src/part_design/feature_thickness.cpp` | face selection、neutral plane、multi-solid / fuse history。 |
| Hole | `cad-core/src/part_design/feature_hole.cpp` | profile source、head cut、thread table、ModelThread pipe-shell tool、subtractive history。 |
| Body replay | `cad-core/src/part_design/body.cpp` | Add/Sub final-result cut / fuse、Body tip 与 slot history consumption。 |
| Topo / ElementMap | `cad-core/include/cad_core/part/topo_shape.h`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/app/element_map.cpp` | request-local `NamedShape`、`ElementMap`、mapper history、split / deleted / merge diagnostics。 |
| Capability / tests | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py` | 发布能力口径并约束 P7 focused scenarios。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、入口、队列和文档验收命令。 |
| 方案 | `6-28-22-53-C10-M2-PartDesignDressUpHoleTopoHistory批次方案.md` | C10-M2 实施策略与 S0-S6 拆分。 |
| 工作步骤总入口 | `工作步骤细分/6-28-22-54-【已实现】C10-M2工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-28-22-55-【已实现】C10-M2-S0-live基线与声明口径冻结.md` | 已冻结 live baseline、allowed claims、forbidden claims、状态词典和验证命令。 |
| S1 | `工作步骤细分/6-28-22-56-【已实现】C10-M2-S1-FreeCAD源码与current覆盖候选矩阵.md` | 已复核 FreeCAD source authority 与 current cad-core coverage。 |
| S2 | `工作步骤细分/6-28-22-57-【已实现】C10-M2-S2-范围准入与blocker矩阵.md` | 已对 oracle、implementation、diagnostic 和 non-goal 做范围准入。 |
| S3 | `工作步骤细分/6-28-22-58-【已实现】C10-M2-S3-DressUp生产者History专项复审.md` | DressUp AddSubShape slot、selection history、Draft / Thickness history 复审。 |
| S4 | `工作步骤细分/6-28-22-59-【已实现】C10-M2-S4-Hole生产者History专项复审.md` | Hole `findHoles()`、profile-source、ModelThread、head-cut 和 subtractive cut 复审。 |
| S5 | `工作步骤细分/6-28-23-00-【已实现】C10-M2-S5-跨特征旧引用恢复与diagnostic边界专项复审.md` | DressUp / Hole 经 Body、transformed、Link retag 后的 split / deleted / old reference 边界复审。 |
| S6 | `工作步骤细分/6-28-23-01-C10-M2-S6-Oracle实现与发布闸门.md` | 消费 S3-S5 证明过的 mismatch，实施或发布 no-code gate。 |
| source candidates | `矩阵/c10m2_dressup_hole_topohistory_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c10m2_dressup_hole_topohistory_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c10m2_dressup_hole_topohistory_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c10m2_dressup_hole_topohistory_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c10m2_dressup_hole_topohistory_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c10m2_dressup_hole_topohistory_validation_matrix.tsv` | 分层验收命令。 |

当前工作步骤总入口、S0、S1、S2、S3、S4 和 S5 已标为 `【已实现】`；S6 仍待执行。矩阵仍是 route / release gate 口径，除 `C10M2-BLOCKER-000=closed_s0`、`C10M2-BLOCKER-101=closed_s1`、`C10M2-BLOCKER-201=closed_s2`、`C10M2-BLOCKER-301=closed_s3`、`C10M2-BLOCKER-401=closed_s4`、`C10M2-BLOCKER-501=closed_s5`、`C10M2-SCOPE-001=baseline_frozen_s0`、`C10M2-SCOPE-101=expected_backed_no_gap`、`C10M2-SCOPE-102=no_gap`、`C10M2-SCOPE-201=expected_backed_no_gap`、`C10M2-SCOPE-202=expected_backed_no_gap`、`C10M2-SCOPE-301=diagnostic_retained`、`C10M2-CAT-101=no_gap`、`C10M2-CAT-102=no_gap` 与 `C10M2-CAT-103=diagnostic_retained` 外，不是发布闸门结论。
