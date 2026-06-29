# 【已实现】C12-M1 S3 CopyOnChange 剩余 gap 复审

## 目标

复审 live capability 中唯一 active `remaining_gaps`：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。S3 的核心问题不是“如何实现 full temporary-document cache”，而是当前是否出现了新的 native copied-object evidence、产品 DTO approval 和 request-local current mismatch。

## 输入

- `docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/README.md`
- `docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/README.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/include/cad_core/part_design/feature_shape_binder.h`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`

## 范围

1. 复核 C9-M5 / C10-M4 retained decision 是否仍成立。
2. 区分 App::Link request-local `documentObjectUpdates` 与 SubShapeBinder `_tmp_binder` / `_CopiedObjs` / `copyObject` lifecycle。
3. 确认 unsupported `BindCopyOnChange=Enabled/Mutated` 与 `PartialLoad=True` 仍输出 `copy_on_change_full_temporary_document_cache_not_supported`。
4. 只有三项同时成立时才把 `C12M1-SCOPE-101` 改成 implementation candidate：stable native copied-object expected、产品批准 request-local DTO、current cad-core mismatch。

## 必须回写的矩阵行

- `C12M1-SCOPE-101`
- `C12M1-SCOPE-102`
- `C12M1-BLOCKER-301`
- `C12M1-CAT-001`
- `C12M1-VAL-301..304`

## S3 结论

- S3 起点确认 `pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=6d9f868bf6`（`6d9f868bf6 docs: 完成 C12-M1 S2 范围准入矩阵`），起始工作区干净。
- C9-M5 已关闭为 `no_code_retained_known_gap_release_gate`：native probe 只能观察 Disabled / Enabled / Mutated / PartialLoad、`_tmp_binder` 和 `_CopiedLink`；`_CopiedObjs`、`copyObject()` dependency order 与 `recomputeFeature(true)` ElementMap lifecycle 仍不能导出为稳定 request-local DTO，S4 产品裁决为 `dto_rejected_known_gap_retained`。
- C10-M4 已关闭为 no-code retained diagnostic / release gate：App::Link `documentObjectUpdates` 只覆盖前端 DocumentObject graph 中已持久化 copied graph 与 Link CopyOnChange properties 的写回，不是 SubShapeBinder `_tmp_binder` / `_CopiedObjs` / `copyObject` lifecycle；unsupported `BindCopyOnChange=Enabled/Mutated` 与 `PartialLoad=True` 仍由 `copy_on_change_full_temporary_document_cache_not_supported` 表达。
- 当前 `cad-core/src/runtime/capability_contract.cpp` 仍发布 `copy_on_change_full_temporary_document_cache` 为 `known_gap_diagnostic` / `oracle_blocked`，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`；`cad-core/src/part_design/feature_shape_binder.cpp` 仍只对 `BindCopyOnChange=Enabled/Mutated` 或 `PartialLoad=True` 追加 retained warning diagnostic，不产生 copied-object `documentObjectUpdates`。
- 三项代码闸门证据没有同时成立：没有 stable native copied-object expected；没有产品批准的 SubShapeBinder request-local DTO，既有产品裁决反而是 rejected/retained；也没有 current cad-core mismatch，因为当前代码和测试仍锁定 retained diagnostic。
- 因此 `C12M1-SCOPE-101` 关闭为 S3 retained known gap / oracle blocked，不改成 implementation candidate；`C12M1-SCOPE-102` 保持 App::Link DTO vocabulary reference-only non-goal；`C12M1-BLOCKER-301` 以 retained close condition 关闭；`C12M1-CAT-001` 不生成代码授权，交 S6 作为 no-code backlog gate 输入。
- S3 未实现 C++、未改 capability wording、未采 oracle、未运行 FreeCADCmd、未新增 fixtures，也未扩大 `ReferenceShadow.brep` single-subshape exception。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "copy_on_change_full_temporary_document_cache|copy_on_change_full_temporary_document_cache_not_supported|BindCopyOnChange|PartialLoad|documentObjectUpdates" cad-core/src/runtime/capability_contract.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/src/app/copy_on_change.cpp cad-core/src/app/link.cpp cad-core/tests/test_c8_shapebinder.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py
rg -n "dto_rejected_known_gap_retained|no_code_retained|oracle_blocked|known_gap_diagnostic|_tmp_binder|_CopiedObjs|copyObject" docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次 docs/CADCore12.0/README.md
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
```

通过条件：

- 如果没有新 native expected / product DTO approval / current mismatch，CopyOnChange 继续为 retained known gap。
- 如果发现完整三项证据，S3 必须把 evidence path 和 next implementation landing 写入矩阵，交 S6 授权。
- S3 不实现 C++，不改 capability wording。
- 验证后本文件已重命名为 `6-29-16-31-【已实现】C12-M1-S3-CopyOnChange剩余gap复审.md`，并更新工作步骤索引。

## 非目标

- 不实现 backend session、temporary document cache 或 persistent copied object。
- 不把 App::Link DTO 直接等同为 SubShapeBinder CopyOnChange supported。
- 不扩大 `ReferenceShadow.brep` single-subshape exception。
