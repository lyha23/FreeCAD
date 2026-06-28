# C9-M5 SubShapeBinder CopyOnChange RequestLocal DTO 准入复审包方案

## 背景

C9-M4 关闭后，Assembly request-local 能力面已经没有 active remaining gap。当前 live capability 中仍非空的项只有 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。该项来自 C8-M1 / C8-M2：C8-M2 已裁定当时只能证明 property-state evidence，不能证明稳定 request-local copied-object DTO，因此保留为 `known_gap_diagnostic` / `oracle_blocked`。

C9-M5 不是直接实现这个 cache，也不是否定 C8-M2。它做一次新的准入复审：在当前 FreeCAD source、cad-core capability、已有 C8-M2 probe 和可能的新 C9-M5 native evidence 下，判断是否存在可落入无状态 CAD Core 的 DTO 子集。

## 原则

- 不保存跨请求 FreeCAD document、temporary document、BREP、TopoDS_Shape、NamedShape、ElementMap 或 copied-object cache。
- 不用 fixture 名、bbox、面积、长度、几何类型或输出排序猜 CopyOnChange 语义。
- 不把 `_CopiedObjs` private vector、`copyObject()` dependency order、`recomputeFeature(true)` ElementMap lifecycle 直接当成可传输 DTO，除非 S3-S5 证明有 request-local、可序列化、产品批准的替代表达。
- S5 已确认没有 `backend_gap_requires_implementation` row；S6 只消费 S3-S5 产生的 retained known gap / release gate 证据。

## 最小完整语义批次

| 批次 | 范围 | 预期处理 |
| --- | --- | --- |
| live capability gap | `part_design.sub_shape_binder.remaining_gaps` 唯一项 | S0 冻结，不直接实现 |
| FreeCAD full path | `setupCopyOnChange()`、`update()`、`LinkBaseExtension`、`Document::copyObject()` | S1 source authority |
| native lifecycle evidence | Disabled / Enabled / Mutated / PartialLoad / `_tmp_binder` / `_CopiedLink` / copied object visibility | S3 复跑并增强 probe |
| request-local DTO | graph update、documentObjectUpdates、diagnostics、copied-object mapping 的可序列化边界 | S4 产品边界复审 |
| cad-core implementation gate | `feature_shape_binder.cpp`、`copy_on_change.cpp`、capability、focused tests | S5/S6 决定是否落代码 |
| persistent cache | GUI、backend session、temporary document、full BREP / TopoDS_Shape cache | non-goal |

## S0 live 基线与 known-gap 声明冻结

S0 只冻结当前 live 状态：C9-M4 队列为空、Assembly capability 已清、SubShapeBinder CopyOnChange 是唯一 remaining gap 且当前为 `known_gap_diagnostic` / `oracle_blocked`。S0 必须写清 forbidden claims：不得把 full temporary-document cache 写成 supported，不得把 property-state evidence 写成 DTO，不得绕过无状态边界。

## S1 FreeCAD source 与 current 覆盖候选矩阵

S1 复核 FreeCAD source：`ShapeBinder.cpp::setupCopyOnChange()`、`ShapeBinder.cpp::update()`、`Link.cpp::syncCopyOnChange()`、`Link.cpp::makeCopyOnChange()`、`Document.cpp::copyObject()` / `recomputeFeature()`。同时复核 current cad-core：`feature_shape_binder.cpp`、`copy_on_change.cpp`、`capability_contract.cpp`、`test_c8_shapebinder.py`。S1 不采 oracle、不改 C++。

## S2 范围准入与 blocker 矩阵

S2 把 S1 的 source/current evidence 路由到 `native_oracle_required`、`product_decision_required`、`known_gap_retained`、`backend_gap_candidate`、`diagnostic_non_goal` 或 `release_gate`。S2 必须明确：只有 S3-S5 证明稳定 request-local DTO，S6 才能把 `backend_gap_candidate` 升级为 `backend_gap_requires_implementation`。

## S3 native CopyOnChange 生命周期 probe 复审

S3 复跑 C8-M2 native probe，并新增 C9-M5 专用 evidence，而不是覆盖 C8-M2 expected。重点观察：

- Disabled / Enabled / Mutated / PartialLoad 的 Python-visible property-state。
- `_tmp_binder`、`_CopiedLink`、copied object name、source subname 和 Support / copied support 对应关系。
- 能否在 request 结束前导出稳定、可序列化、不含 TopoDS_Shape / NamedShape / ElementMap cache 的 copied-object mapping。

若仍无法观察 `_CopiedObjs`、dependency order 或 recompute lifecycle 的稳定 DTO，S3 必须保持 `oracle_blocked`。

## S4 request-local DTO 产品边界复审

S4 根据 S3 evidence 评估是否允许一个新 DTO。候选 DTO 只能包含前端 graph 可持久化或 request-local response 可返回的结构化字段，例如 copy intent、source object id/name、support subnames、mutated property delta、documentObjectUpdates 和 diagnostic。禁止包含 hidden temporary document、raw shape、BREP、TopoDS_Shape、NamedShape、ElementMap 或跨请求 object cache。

S4 的输出不是代码实现，而是产品边界裁决：`dto_approved_candidate`、`dto_rejected_known_gap_retained` 或 `needs_more_native_evidence`。

## S5 cad-core 实现闸门与 diagnostic 发布复审

S5 已对照当前 C++、capability 和 focused tests 关闭为 no-code release gate：

- S4 未批准 DTO，S5 不把任何 row 升级为 `backend_gap_requires_implementation`。
- current `cad-core` 继续发布 `copy_on_change_full_temporary_document_cache_not_supported`。
- capability / tests / docs 继续发布 `known_gap_diagnostic`、`oracle_blocked` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。

## S6 Oracle 实现与发布闸门

S6 是 no-code release gate：保留 `copy_on_change_full_temporary_document_cache` known gap，更新 docs / matrices / capability smoke 证据，不改 C++。

S6 不允许半支持：full temporary-document cache 仍是必要条件，且 S4/S5 未批准替代 DTO，因此不能把它发布成 supported。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore9.0/README.md
git diff --check
```

实现短跑只在 S6 打开 C++ gate 时执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters
```

重型收口只在 capability schema、shared document update、ElementMap / NamedShape、reference update 或 copy-on-change shared API 发生变化时执行。
