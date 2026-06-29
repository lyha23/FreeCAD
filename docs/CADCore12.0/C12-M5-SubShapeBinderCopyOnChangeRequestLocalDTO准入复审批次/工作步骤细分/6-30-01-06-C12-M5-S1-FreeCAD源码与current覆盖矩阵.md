# C12-M5 S1 FreeCAD 源码与 current 覆盖矩阵

## 目标

复核 FreeCAD SubShapeBinder CopyOnChange 调用链和当前 `cad-core` 覆盖，明确哪些是 source authority、哪些只是 App::Link DTO 参考，关闭 source authority blocker。

## 必读文件

- `../矩阵/c12m5_copy_on_change_source_candidates.tsv`
- `../矩阵/c12m5_copy_on_change_dto_contract_fields.tsv`
- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/Mod/PartDesign/App/ShapeBinder.h`
- `src/App/Link.cpp`
- `src/App/Document.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/runtime/capability_contract.cpp`

## 操作

1. 记录 FreeCAD `setupCopyOnChange()`、`checkCopyOnChange()`、`update()`、`copyObject()` 的真实调用顺序和关键字段。
2. 标出 current `cad-core` 中 retained diagnostic、BindMode Detached update、App::Link `documentObjectUpdates` 的边界。
3. 更新 source_candidates 和 dto_contract_fields 中 S1 行。
4. 若 source path 或 current landing 不存在，保留 blocker，不进入 S2 probe。

## 非目标

- 不从 App::Link coverage 推导 SubShapeBinder supported。
- 不实现 CopyOnChange。
- 不新增 fixture。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'setupCopyOnChange|checkCopyOnChange|_CopiedObjs|_tmp_binder|copyObject|recomputeFeature|copy_on_change_full_temporary_document_cache_not_supported' src/Mod/PartDesign/App/ShapeBinder.cpp src/App/Link.cpp src/App/Document.cpp cad-core/src/part_design/feature_shape_binder.cpp cad-core/src/runtime/capability_contract.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```

