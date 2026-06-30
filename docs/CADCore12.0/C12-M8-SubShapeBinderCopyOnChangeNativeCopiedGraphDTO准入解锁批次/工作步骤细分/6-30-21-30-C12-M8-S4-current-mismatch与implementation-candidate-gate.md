# C12-M8 S4 current mismatch 与 implementation candidate gate

## 目标

只有在 S2 native copied graph evidence 与 S3 DTO approval 同时成立后，才比较 current `cad-core` retained diagnostic 是否形成真实 implementation mismatch。

## 必读文件

- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `../矩阵/c12m8_copy_on_change_validation_matrix.tsv`
- `cad-core/tests/test_c8_shapebinder.py`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`

## 操作

1. 若 S2 或 S3 未通过，S4 关闭为 `no_current_mismatch_retained_diagnostic`。
2. 若两者通过，构造 current comparison：同一 request-local graph / DTO 输入下，current `cad-core` 是否仍只返回 `copy_on_change_full_temporary_document_cache_not_supported`。
3. 确认 mismatch 是否属于 C++ implementation，还是 capability/docs wording 修复。
4. 若 mismatch 成立，记录最小 implementation surface：`feature_shape_binder`、`app/copy_on_change`、document parser、fixtures expected、focused tests、capability adapter assertion。
5. 若 mismatch 不成立，保留 diagnostic 或 publication repair。

## 关闭条件

- `C12M8-BLOCKER-401` 关闭：current comparison gate 已裁决。
- `C12M8-CAT-001` 更新为 `implementation_candidate`、`publication_repair_only` 或 `retained_diagnostic`。
- 若 implementation candidate 成立，写清后续包 scope，C12-M8 本身仍不落 C++。

## 非目标

- 不在 S4 直接修代码。
- 不新增 fixture 特判。
- 不把 App::Link transport 当成 SubShapeBinder current success。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
cd cad-core
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
git diff --check
```

若只做 no-code gate，focused tests 可记录为 not run，并说明阻断原因。
