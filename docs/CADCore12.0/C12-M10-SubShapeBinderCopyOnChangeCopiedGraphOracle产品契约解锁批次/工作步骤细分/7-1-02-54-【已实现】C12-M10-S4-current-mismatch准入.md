# C12-M10 S4 current mismatch 准入（已实现）

## 目标

在 S2 native evidence ready 且 S3 DTO / product contract approved 的前提下，判断 current `cad-core` retained diagnostic 是否形成真实 mismatch。

## 必读文件

- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `../矩阵/c12m10_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m10_copy_on_change_dto_contract_fields.tsv`
- `../矩阵/c12m10_copy_on_change_product_contract_gate.tsv`

## 操作

1. 若 S2/S3 未通过，关闭为 blocked / not comparable，保留 diagnostic。
2. 若 S2/S3 通过，构造同一 request-local graph comparison，并运行必要 focused tests 或 diagnostics review。
3. 判断 current comparison status：`current-covered`、`mismatch-confirmed`、`blocked` 或 `not-comparable`。
4. 只有 `mismatch-confirmed` 才进入 S5 implementation authorization。
5. 将本 S4 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M10-BLOCKER-401` 关闭。
- backend classification 写入 S4 current comparison status。

## 非目标

- 不为了制造 mismatch 而改 expected 或放宽断言。
- 不运行 full build。
- 不改 production code。

## S4 结论

- S4 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=73a0c52afc`（`73a0c52afc 文档：关闭 C12-M10 S3 DTO 契约裁决`），起点 worktree clean。
- S2 裁决继承为 `native_oracle_blocked_retained`；S3 裁决继承为 `dto_not_reviewed_due_to_native_blocker`。
- S2 gate artifact 仍缺 `_CopiedObjs` stored identity/order、`Document::copyObject()` dependency order/mapping、内部 `recomputeFeature(true)` lifecycle 与 ElementMap / NamedShape 分阶段 lifecycle；S3 没有批准 copied graph execution DTO 或产品契约。
- current landing 复核结果一致：`cad-core/src/part_design/feature_shape_binder.cpp` 在 `BindCopyOnChange=Enabled|Mutated` 或 `PartialLoad=True` 时发布 `copy_on_change_full_temporary_document_cache_not_supported`；`cad-core/tests/test_c8_shapebinder.py` 和 `cad-core/src/runtime/capability_contract.cpp` 继续把该项作为 known gap / oracle_blocked retained diagnostic。
- S4 裁决为 `not_comparable` / `no_current_mismatch_retained_diagnostic`，不制造 `mismatch-confirmed` row，不授权 implementation package。
- 已关闭 `C12M10-BLOCKER-401=closed_s4_not_comparable_retained_diagnostic`，并记录 `C12M10-VAL-401=passed_s4_not_comparable_retained_diagnostic`。S5 必须继承 no mismatch input，只能继续做 retained diagnostic、oracle blocker 或 product-contract follow-up 分流。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
