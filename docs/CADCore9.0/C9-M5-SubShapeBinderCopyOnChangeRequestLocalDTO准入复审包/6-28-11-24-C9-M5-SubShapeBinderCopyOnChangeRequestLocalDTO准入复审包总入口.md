# C9-M5 SubShapeBinder CopyOnChange RequestLocal DTO 准入复审包总入口

本文是 `docs/CADCore9.0` 下的 C9-M5 实施主线。它承接 C9-M4 queue-empty 后的 live 状态，专门复审 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 是否能从 C8 retained known gap 推进为产品批准的 request-local CopyOnChange DTO。

## 主线目标

- 不重开 C9-M1 到 C9-M4 的 Assembly 批次；这些队列已经关闭。
- 不重开 C8-M1 已支持的 ShapeBinder / SubShapeBinder request-local executor、ElementMap、NamedShape、Body replay。
- 以 FreeCAD `SubShapeBinder::setupCopyOnChange()`、`SubShapeBinder::update()`、`LinkBaseExtension::syncCopyOnChange()` 和 `Document::copyObject()` 为 source authority，重新核对 CopyOnChange full path。
- 只有当 native evidence 证明稳定 request-local copied-object DTO 且产品决策批准时，S6 才打开 C++ 实现闸门；否则保留 `known_gap_diagnostic` / `oracle_blocked`。

## 当前基线

- 方案生成基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ceef6a128b feat: 关闭 C9-M4 S6 默认距离类型发布闸门`。
- 生成前 `git status --short -uall` 无输出。
- S0 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ceef6a128b`；开始时 status 仅包含 `docs/CADCore9.0/README.md` 修改和本 C9-M5 包未跟踪文件。
- C9-M4 `工作步骤细分` 队列为空；Assembly capability 当前无 active remaining gap。
- live capability 唯一非空 `remaining_gaps` 为 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，当前发布为 `known_gap_diagnostic` / `oracle_blocked`。
- `cad-core/src/part_design/feature_shape_binder.cpp` 已在 CopyOnChange Enabled / Mutated / PartialLoad 时返回 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。
- S0 冻结声明：full temporary-document copied-object cache 不得写成 supported 或 `backend_gap_requires_implementation`；S6 code gate 只在 S3 native evidence 与 S4 产品边界同时成立时打开。
- S1 source/current 审计已关闭：FreeCAD full path 仍依赖 `_tmp_binder` / `_CopiedObjs` / `copyObject()` / `recomputeFeature(true)` / `_CopiedLink`；current `cad-core` 保持 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic、`known_gap_diagnostic` / `oracle_blocked` capability 和 focused test 断言。

## 证明链条

```text
C9-M4 queue empty
  -> live capability remaining gap inventory
  -> FreeCAD CopyOnChange source authority
  -> scope review / nonGoal / blocker queue
  -> native lifecycle probe hardening
  -> request-local DTO product boundary review
  -> cad-core implementation gate decision
  -> S6 no-code or code publication gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| SubShapeBinder CopyOnChange setup | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` | 单 support 且 `BindCopyOnChange != Disabled` 时调用 `LinkBaseExtension::setupCopyOnChange()`，并在 source property change 时清空 `_CopiedObjs` cache。 |
| Mutated copied-object lifecycle | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` | `BindCopyOnChange == Mutated` 时创建 `"_tmp_binder"` temporary document、调用 `copyObject()`、缓存 `_CopiedObjs`、执行 `recomputeFeature(true)` 并写 `_CopiedLink`。 |
| App Link CopyOnChange 对照 | `/home/user/Chili3DProject/FreeCAD/src/App/Link.cpp::LinkBaseExtension::syncCopyOnChange()` | 使用 hidden `CopyOnChangeGroup`、`copyObject()`、`_SourceUUID` / `_ObjectUUID` 匹配和 CopyOnChange property paste 同步。 |
| Owned copy creation | `/home/user/Chili3DProject/FreeCAD/src/App/Link.cpp::LinkBaseExtension::makeCopyOnChange()` | `getOnChangeCopyObjects()` 后调用 `Document::copyObject()`，并把 link 切到 owned copied object。 |
| Document copy / recompute | `/home/user/Chili3DProject/FreeCAD/src/App/Document.cpp::Document::copyObject()`、`Document::recomputeFeature()` | full copied-object lifecycle 的 dependency order、UUID、ElementMap 和 recompute side effect 来源。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| SubShapeBinder executor | `cad-core/src/part_design/feature_shape_binder.cpp` | 执行 request-local support shape、BindMode writeback、CopyOnChange diagnostic boundary。 |
| CopyOnChange document updates | `cad-core/src/app/copy_on_change.cpp` | App Link CopyOnChange request-local lifecycle updates，可作为 DTO 设计对照但不能隐式替代 SubShapeBinder full cache。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `sub_shape_binder.known_gaps`、diagnostic、delete/reopen condition 和 `remaining_gaps`。 |
| focused tests | `cad-core/tests/test_c8_shapebinder.py`、`cad-core/tests/test_diagnostics.py`、`cad-core/tests/test_adapters.py` | 约束当前 known gap、diagnostic、capability 和未来发布口径。 |
| native probes / fixtures | `cad-core/tools`、`cad-core/fixtures/c9m5` | C9-M5 若采集新 oracle，应落在新 c9m5 fixture/probe，避免覆盖 C8-M2 evidence。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、当前状态、收口边界和验收命令。 |
| 方案 | `6-28-11-24-C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包方案.md` | C9-M5 实施策略与 S0-S6 拆分。 |
| 工作步骤总入口 | `工作步骤细分/6-28-11-24-【已实现】C9-M5工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-28-11-25-【已实现】C9-M5-S0-live基线与known-gap声明口径冻结.md` | 已冻结 live baseline、claim、forbidden claim 和状态词典。 |
| S1 | `工作步骤细分/6-28-11-26-【已实现】C9-M5-S1-FreeCAD源码与current覆盖候选矩阵.md` | 已复核 FreeCAD source authority、current cad-core landing 和候选矩阵。 |
| S2 | `工作步骤细分/6-28-11-27-C9-M5-S2-范围准入与blocker矩阵.md` | 对 CopyOnChange property-state、full cache、DTO、non-goal 做准入路由。 |
| S3 | `工作步骤细分/6-28-11-28-C9-M5-S3-native-CopyOnChange生命周期probe复审.md` | 复跑并增强 native CopyOnChange lifecycle probe。 |
| S4 | `工作步骤细分/6-28-11-29-C9-M5-S4-request-local-DTO产品边界复审.md` | 审核是否存在可发布的 request-local DTO。 |
| S5 | `工作步骤细分/6-28-11-30-C9-M5-S5-cad-core实现闸门与diagnostic发布复审.md` | 对当前 cad-core diagnostic boundary 和实现闸门做 release audit。 |
| S6 | `工作步骤细分/6-28-11-31-C9-M5-S6-Oracle实现与发布闸门.md` | 根据 S3-S5 evidence 决定 code gate 或 no-code retained known gap。 |
| source candidates | `矩阵/c9m5_copyonchange_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m5_copyonchange_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m5_copyonchange_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m5_copyonchange_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m5_copyonchange_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m5_copyonchange_validation_matrix.tsv` | 分层验收命令。 |

当前工作步骤总入口索引、S0 和 S1 已标为 `【已实现】`；S2-S6 仍是待执行状态。S1 已关闭 source/current 审计，但不改变 CopyOnChange known gap 发布口径。
