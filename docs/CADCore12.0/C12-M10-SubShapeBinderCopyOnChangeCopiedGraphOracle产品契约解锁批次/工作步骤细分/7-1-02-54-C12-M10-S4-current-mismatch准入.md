# C12-M10 S4 current mismatch 准入

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

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
