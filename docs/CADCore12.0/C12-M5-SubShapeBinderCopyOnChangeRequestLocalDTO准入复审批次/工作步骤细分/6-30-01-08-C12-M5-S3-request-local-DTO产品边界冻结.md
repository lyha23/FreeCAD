# C12-M5 S3 request-local DTO 产品边界冻结

## 目标

在 S1/S2 evidence 基础上，决定 SubShapeBinder CopyOnChange 是否有产品可接受的 request-local DTO。没有 approved DTO 时，必须保留 known gap diagnostic。

## 必读文件

- `../矩阵/c12m5_copy_on_change_dto_contract_fields.tsv`
- `../矩阵/c12m5_copy_on_change_non_goal_registry.tsv`
- `docs/接口规定/01-cad-recompute全量输入输出接口.md`
- `docs/CADCore方案/细化方案/03-接口与验收样例.md`
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/tests/test_p8_features.py`

## 操作

1. 列出允许 DTO 字段：request graph 可持久化字段、copy intent、source id/name、support subname、mutation delta、diagnostic、`documentObjectUpdates`。
2. 列出禁止字段：temporary document、native object pointer、TopoDS / BREP full object、post-request NamedShape / ElementMap cache、`_CopiedObjs` private vector。
3. 判断前端是否能用该 DTO 更新 DocumentObject graph，再作为下一次请求唯一真实数据。
4. 发布 `dto_approved_for_mismatch_gate` 或 `dto_rejected_known_gap_retained`。

## 非目标

- 不要求后端持久化 copied-object cache。
- 不把 `ReferenceShadow.brep` 扩展成 full object BREP transport。
- 不靠 adapter 层修复 DTO 缺失。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'documentObjectUpdates|ReferenceShadow|LinkCopyOnChange|CopyOnChangeOwned|temporary document|full object BREP|copy_on_change_full_temporary_document_cache' docs/接口规定 docs/CADCore方案 cad-core/src/app cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审批次/矩阵/*.tsv
git diff --check
```

