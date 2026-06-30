# C12-M8 S3 request-local DTO 产品边界裁决

## 目标

裁决 S2 native copied graph evidence 中哪些字段可以进入 CAD Core request-local 产品契约，哪些字段必须保留为 forbidden backend/session state。

## 必读文件

- `../矩阵/c12m8_copy_on_change_dto_contract_fields.tsv`
- `../矩阵/c12m8_copy_on_change_non_goal_registry.tsv`
- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `docs/接口规定/01-cad-recompute全量输入输出接口.md`
- `cad-core/include/cad_core/app/copy_on_change.h`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/adapters`

## 操作

1. 若 S2 未输出 `native_copied_graph_evidence_ready`，S3 直接关闭为 `dto_not_reviewed_due_to_native_blocker`。
2. 若 S2 成立，只允许 request graph / `documentObjectUpdates` 表达 copied object 创建、属性写回、链接重写和前端持久化 graph mutation。
3. 禁止字段包括 temporary document handle、native object pointer、TopoDS、BREP、full object shape snapshot、persistent `NamedShape`、persistent `ElementMap`、backend cache key、post-request `_tmp_binder` 或 `_CopiedObjs` session state。
4. 将每个 DTO 字段标为 `approved`、`rejected`、`needs_product_decision` 或 `deferred_to_implementation_package`。
5. 更新 backend_gap_classification：只有 DTO 批准后才允许进入 current mismatch gate。

## 关闭条件

- `C12M8-DTO-001..012` 均有裁决。
- `C12M8-BLOCKER-301` 关闭：DTO boundary 已审完。
- S3 输出只能是 `dto_approved_for_request_local_graph`、`dto_rejected_known_gap_retained` 或 `dto_not_reviewed_due_to_native_blocker`。

## 非目标

- 不为了通过 DTO 审核而新增 backend session。
- 不把 full BREP / TopoDS / NamedShape / ElementMap cache 放进请求或响应。
- 不改 adapter 协议。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
