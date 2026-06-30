# C12-M8 S4 current mismatch 与 implementation candidate gate【已实现】

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

## S4 结论

- 本轮 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=6279752006`，`git log -1 --oneline=6279752006 文档：关闭 C12-M8 S3 DTO 边界裁决`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S4 执行前队列确认：`6-30-21-30-C12-M8-S4-current-mismatch与implementation-candidate-gate.md` 是第一条 pending，后续为 S5-S6。
- S2 已关闭为 `native_evidence_retained_blocker`，S3 已关闭为 `dto_not_reviewed_due_to_native_blocker`；没有 approved CopyOnChange request-local DTO，因此同一 request-local graph comparison 不成立。
- current `cad-core` 行为与现有证据一致：`feature_shape_binder` 对 `BindCopyOnChange=Enabled` / `Mutated` 和 `PartialLoad=True` 保留 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic，并把边界记录为 request-local known gap。
- capability contract 继续发布 `part_design.sub_shape_binder` 的 `known_gap_diagnostic` / `oracle_blocked` / `copy_on_change_full_temporary_document_cache_not_supported`，与 S2/S3 retained blocker 一致。
- `cad-core/tests/test_c8_shapebinder.py` 覆盖当前 retained diagnostic 与 capability known gap；`cad-core/tests/test_adapters.py` 只做 capability publication smoke，不构成 SubShapeBinder CopyOnChange success evidence。
- App::Link `documentObjectUpdates` transport、property/session 状态、label、bbox、shape count、`_tmp_binder` document name 和 `_CopiedLink` target 均不作为 SubShapeBinder success/mismatch 证据。
- S4 输出：`no_current_mismatch_retained_diagnostic`。本步不构造 implementation candidate；最多保留 publication wording check 给 S6。

## 关闭条件

- `C12M8-BLOCKER-401` 关闭：current comparison gate 已裁决。
- `C12M8-CAT-001` 更新为 `retained_diagnostic/no_current_mismatch`。
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

本轮按 no-code gate 关闭，未运行 focused tests：`not_run_blocked_by_s2_s3_gate`。必要验收改为 C12-M8 队列、TSV 字段数和 `git diff --check`。
