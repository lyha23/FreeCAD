# C10-M3 ReferenceShadow / ShadowSub Native Recovery 准入批次总入口

本文是 `docs/CADCore10.0` 下的 C10-M3 实施主线。它承接 C10-M2 `PartDesignDressUpHoleTopoHistory` 队列关闭后的 live 状态，专门复核 stale `ReferenceShadow` / `ShadowSub` old-reference recovery 是否存在 native 可观测证据，以及是否足以打开 cad-core C++ 实现闸门。

## 主线目标

- 不继续扩展 C10-M2 已关闭的 DressUp / Hole producer history；C10-M2 已裁决为 no-code release gate。
- 围绕 FreeCAD `PropertyLinkBase::updateElementReferences()`、`GeoFeature::resolveElement()`、`PropertyPartShape::afterRestore()` 与 `PropertyLinkSub::getSubValues(false/true)` 建立 native 可观测性矩阵。
- 复核 current cad-core 的 `ReferenceShadow` parser、BREP / fingerprint recovery、`ElementMap` split / deleted diagnostic 和 `elementReferenceUpdates` 输出边界。
- 只有 native observable `ShadowSub` / `ReferenceShadow` recovery evidence 加 current cad-core mismatch 同时成立时，S6 才打开 C++ / focused tests / expected 实现行。
- 明确禁止 raw `FaceN`、bbox、面积、输出顺序、source index、fixture 名称、adapter pruning 或跨请求 geometry cache 来推断 stable reference。

## 当前基线

- 起点仓库：`/home/user/Chili3DProject/FreeCAD`。
- 包创建起点 HEAD：`3279f8f524`（`docs: 完成 C10-M2 S6 发布闸门`）。
- S0 live 基线：`HEAD=f528b8f7f6`（`docs: 新增 C10-M3 ReferenceShadow native recovery 方案`），S0 起始 `git -c core.quotepath=false status --short -uall` 为空。
- S0 只关闭 docs / matrix 基线：`C10M3-BLOCKER-000=closed_s0`，`C10M3-SCOPE-001=baseline_frozen_s0`；不打开 C++ gate。
- S1 已关闭 source authority audit：`C10M3-BLOCKER-101=closed_s1`，`C10M3-SRC-101..204` 已刷新为 live FreeCAD / current cad-core path、symbol、evidence、focused tests 与 capability landing；不声明 supported 或 backend gap。
- C10-M1 / C10-M2 队列均已关闭，`docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分` 无待执行步骤。
- C10-M2 已发布：DressUp producer history `expected_backed_no_gap` / `no_gap`，Hole producer history `expected_backed_no_gap`，cross-feature old-reference recovery `diagnostic_retained`。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 仍是 retained known gap / `oracle_blocked`，不进入 C10-M3。

## 状态词典

| 状态词 | C10-M3 口径 |
| --- | --- |
| `baseline_pending` | S0 之前的初始状态，只表示待冻结 live 起点。 |
| `baseline_frozen_s0` | 只表示 S0 已冻结 live 起点、允许声明、forbidden claims 和验证命令。 |
| `closed_s0` | 只表示 S0 blocker 关闭；不表示 stale `ReferenceShadow` / Base recovery supported。 |
| `source_audit_pending` | S1 待读 FreeCAD / cad-core source authority。 |
| `native_oracle_candidate` | 需要 FreeCADCmd / FCStd / XML restore native probe 证明可观察性。 |
| `current_comparison_pending` | 已有 native evidence 后，待比较 current cad-core。 |
| `backend_gap_candidate` | native evidence 与 current mismatch 同时存在后，S6 才能消费。 |
| `diagnostic_retained` | 保留 stable diagnostic，不声明 supported。 |
| `notCollected` | 尚无 native 可观察证据，不能实现。 |
| `release_gate` | S6 对 implementation / no-code / retained diagnostic 做发布收口。 |

## 证明链条

```text
C10-M2 queue empty
  -> stale ReferenceShadow / ShadowSub retained diagnostic
  -> FreeCAD source authority
  -> native observability oracle
  -> current cad-core recovery comparison
  -> protocol / diagnostic boundary
  -> S6 code landing or no-code retained diagnostic release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Link reference update | `/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::updateElementReferences()` | 遍历 link property 并调用 `_updateElementReference()` 更新 subname / shadow。 |
| ShadowSub update | `/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` | 调用 `GeoFeature::resolveElement()`，根据 old/new element name 写回 `ShadowSub`。 |
| old/new style sub values | `/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkSub::getSubValues(bool)`、`PropertyLinkSubList::getSubValues(bool)` | 通过 `_ShadowSubList` 在 old style / new style subname 间切换。 |
| element resolve | `/home/user/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::resolveElement()` | 消费 ElementMap version 与 oldName / newName 映射恢复引用。 |
| Shape restore | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp::PropertyPartShape::afterRestore()` | restore 后触发 ElementMap version / reverse update 相关路径。 |
| Mapper history | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp`、`TopoShapeMapper.cpp` | `makeShapeWithElementMap` / `MapperHistory` 为旧引用恢复提供 history 来源。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Link property parser | `cad-core/src/app/property_links.cpp` | 解析 `StableSubList`、`ShadowSub`、`ReferenceShadow`、full subname 和 external document tag。 |
| ReferenceShadow recovery | `cad-core/src/part/topo_shape_reference.cpp` | BREP / fingerprint 解码、single target recovery、split / deleted / unsupported diagnostics。 |
| ElementMap / NamedShape | `cad-core/include/cad_core/part/topo_shape.h`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/app/element_map.cpp` | request-local element map、mapper history、stable reference resolution 和 `element_history_status`。 |
| Recompute output | `cad-core/include/cad_core/runtime/element_reference_update.h`、`cad-core/src/runtime/element_reference_update.cpp` | 发布 `elementReferenceUpdates` 给前端回写 graph。 |
| Tests / capability | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/src/runtime/capability_contract.cpp` | 约束 ReferenceShadow 字段、split / deleted diagnostics、adapter 输出和 capability 口径。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、入口、队列和文档验收命令。 |
| 方案 | `6-29-01-07-C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次方案.md` | C10-M3 实施策略与 S0-S6 拆分。 |
| 工作步骤总入口 | `工作步骤细分/6-29-01-08-【已实现】C10-M3工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-29-01-09-【已实现】C10-M3-S0-live基线与声明口径冻结.md` | 冻结 live baseline、allowed claims、forbidden claims 和状态词典。 |
| S1 | `工作步骤细分/6-29-01-10-【已实现】C10-M3-S1-FreeCAD源码与current覆盖候选矩阵.md` | 复核 FreeCAD source authority 与 current cad-core coverage。 |
| S2 | `工作步骤细分/6-29-01-11-C10-M3-S2-范围准入与blocker矩阵.md` | 对 native oracle、backend gap、diagnostic 和 non-goal 做范围准入。 |
| S3 | `工作步骤细分/6-29-01-12-C10-M3-S3-native可观测性与oracle采集专项复审.md` | 复核 FCStd / XML restore 后 ShadowSub / ReferenceShadow 是否 native observable。 |
| S4 | `工作步骤细分/6-29-01-13-C10-M3-S4-cad-core恢复路径与current mismatch专项复审.md` | 比较 current cad-core parser / recovery / diagnostics 与 native evidence。 |
| S5 | `工作步骤细分/6-29-01-14-C10-M3-S5-前端协议诊断与non-goal边界专项复审.md` | 复核 `elementReferenceUpdates`、ReferenceShadow.brep 单 subshape 例外和禁止 shortcut。 |
| S6 | `工作步骤细分/6-29-01-15-C10-M3-S6-Oracle实现与发布闸门.md` | 消费 S3-S5 evidence，落 C++ / tests 或发布 no-code retained diagnostic。 |
| source candidates | `矩阵/c10m3_reference_shadow_recovery_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c10m3_reference_shadow_recovery_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c10m3_reference_shadow_recovery_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c10m3_reference_shadow_recovery_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c10m3_reference_shadow_recovery_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c10m3_reference_shadow_recovery_validation_matrix.tsv` | 分层验收命令。 |

当前工作步骤总入口索引、S0 与 S1 已标为 `【已实现】`；S2-S6 待执行。矩阵发布结论为：`C10M3-BLOCKER-000=closed_s0`、`C10M3-BLOCKER-101=closed_s1`、`C10M3-SCOPE-001=baseline_frozen_s0`；source candidates 只表示 live source audit 已闭环，其余 native oracle、current comparison、protocol diagnostic 和 release gate 行仍是 S2-S6 的待审 seed，不是 supported 结论。
