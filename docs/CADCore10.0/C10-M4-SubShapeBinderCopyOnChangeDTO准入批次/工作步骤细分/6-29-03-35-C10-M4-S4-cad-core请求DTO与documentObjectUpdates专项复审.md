# C10-M4 S4 cad-core 请求 DTO 与 documentObjectUpdates 专项复审

## 目标

在 S3 证据基础上复核 current cad-core 是否已有足够 request-local DTO / `documentObjectUpdates` 能力承接 SubShapeBinder CopyOnChange。S4 主要做 current comparison 和 DTO boundary，默认不改 C++。

## 必做动作

1. 复核 `cad-core/src/app/copy_on_change.cpp` 与 `cad-core/include/cad_core/app/copy_on_change.h` 的 App::Link DTO 输出是否可复用，哪些字段不能直接套到 SubShapeBinder。
2. 复核 `cad-core/src/part_design/feature_shape_binder.cpp` 当前 `BindCopyOnChange` / `PartialLoad` diagnostic 与 `documentObjectUpdates` 行为。
3. 若 S3 有 native expected，比较 current output，确认是否存在 mismatch。
4. 若产品批准 request-local DTO 子集，写清 DTO input / output / writeback / diagnostics；否则保持 `product_decision_needed`。
5. 更新 scope / blocker / backend-gap matrix；只有 evidence + product decision + mismatch 同时成立，才打开 `backend_gap_candidate`。
6. 通过验收后重命名本文件为 `【已实现】`。

## DTO 边界必须回答

- 前端请求里携带的是 copied object snapshot、copy intent、还是 source mutation evidence。
- cad-core 返回的是 `documentObjectUpdates`、diagnostic、还是两者都有。
- recompute 后前端如何把 copied object 写回 DocumentObject graph。
- 哪些 FreeCAD temporary document / cache 行为继续保持 unsupported。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "documentObjectUpdates|CopyOnChange|BindCopyOnChange|PartialLoad|copy_on_change_full_temporary_document_cache_not_supported" cad-core/src/app/copy_on_change.cpp cad-core/include/cad_core/app/copy_on_change.h cad-core/src/app/link.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/include/cad_core/part_design/feature_shape_binder.h cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py cad-core/src/runtime/capability_contract.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
