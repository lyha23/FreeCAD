# C12-M5 S4 current mismatch 与实现候选闸门

## 目标

只有在 S2 native evidence 和 S3 产品 DTO 均成立时，才比较 current `cad-core` retained diagnostic 与目标 DTO 行为，决定是否发布 implementation candidate。

## 必读文件

- `../矩阵/c12m5_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m5_copy_on_change_blocker_queue.tsv`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/runtime/capability_contract.cpp`

## 操作

1. 检查 S2 是否有 `native_evidence_ready`，S3 是否有 `dto_approved_for_mismatch_gate`。
2. 若任一条件不成立，关闭为 `no_current_mismatch_retained_diagnostic`。
3. 若条件成立，定义最小 implementation candidate：fixtures、expected、core source、adapter/capability/test/docs 修改面。
4. 禁止在 S4 直接落 C++；S4 只发布或拒绝 implementation candidate。

## 非目标

- 不绕过 S2/S3 直接实现。
- 不删除 diagnostic。
- 不把 unsupported test 改成通过 supported wording。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder.CadCoreC8ShapeBinderTest.test_capability_contract_publishes_c8m1_binder_scope tests.test_c8_shapebinder.CadCoreC8ShapeBinderTest.test_copy_on_change_disabled_enabled_mutated_partialload_reports_retained_diagnostic
cd /Users/li/Chili3DProject/FreeCAD
git diff --check
```

