# C12-M4 S3 expected 与 capability 迁移设计

## 目标

设计后续 implementation package：把 C5-M9 expected wording 从 `known_gap/native_hidden replacement` 迁移为 `product_contract/native_oracle_unavailable`，并定位 tests / capability wording 的最小修改范围。

## 必读文件

- `矩阵/c12m4_project_on_surface_request_local_ledger_expected_migration_matrix.tsv`
- C5-M9 expected JSON 文件。
- `cad-core/tests/test_p8_features.py` 中 C5-M9 ProjectOnSurface focused tests。
- 当前 capability / docs 中 ProjectOnSurface provenance wording 的实际位置。

## 操作

1. 明确每个 expected 文件的目标 wording、非目标和 focused validation。
2. 搜索 capability source / docs，不能凭记忆猜路径。
3. 生成后续 implementation package 建议：改哪些文件、跑哪些 focused tests、如何证明没有改 C++ 语义。
4. 若 capability 路径没找到，保留 blocker，不扩大到全仓库实现。

## 非目标

- 不在 S3 直接改 expected JSON。
- 不改 tests 名称或 assertions。
- 不改 capabilities 输出。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M4-ProjectOnSurfaceRequestLocalLedger产品契约批次 docs/CADCore12.0/README.md
git diff --check
```
