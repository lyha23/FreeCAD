# C12-M1 S3 CopyOnChange 剩余 gap 复审

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

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "copy_on_change_full_temporary_document_cache|copy_on_change_full_temporary_document_cache_not_supported|BindCopyOnChange|PartialLoad|documentObjectUpdates" cad-core/src/runtime/capability_contract.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/src/app/copy_on_change.cpp cad-core/src/app/link.cpp cad-core/tests/test_c8_shapebinder.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py
rg -n "dto_rejected_known_gap_retained|no_code_retained|oracle_blocked|known_gap_diagnostic|_tmp_binder|_CopiedObjs|copyObject" docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
git diff --check
```

通过条件：

- 如果没有新 native expected / product DTO approval / current mismatch，CopyOnChange 继续为 retained known gap。
- 如果发现完整三项证据，S3 必须把 evidence path 和 next implementation landing 写入矩阵，交 S6 授权。
- S3 不实现 C++，不改 capability wording。
- 验证后将本文件重命名为 `6-29-16-31-【已实现】C12-M1-S3-CopyOnChange剩余gap复审.md`，并更新工作步骤索引。

## 非目标

- 不实现 backend session、temporary document cache 或 persistent copied object。
- 不把 App::Link DTO 直接等同为 SubShapeBinder CopyOnChange supported。
- 不扩大 `ReferenceShadow.brep` single-subshape exception。
