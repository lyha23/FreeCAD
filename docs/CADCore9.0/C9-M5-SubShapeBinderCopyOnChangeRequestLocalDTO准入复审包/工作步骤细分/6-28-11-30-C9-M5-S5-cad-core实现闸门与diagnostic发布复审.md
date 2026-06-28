# C9-M5-S5 cad-core 实现闸门与 diagnostic 发布复审

## 目标

把 S3 native evidence 和 S4 产品裁决映射到 cad-core implementation gate。S5 可以更新文档、矩阵和测试计划；只有在必要时才修改 capability assertions。S5 本身不做主要 C++ 实现。

## current cad-core 检查

- `cad-core/src/part_design/feature_shape_binder.cpp`：当前 CopyOnChange Enabled / Mutated / PartialLoad diagnostic boundary。
- `cad-core/src/app/copy_on_change.cpp`：App Link CopyOnChange request-local updates，可作为 DTO 对照。
- `cad-core/src/runtime/capability_contract.cpp`：`known_gaps`、`remaining_gaps`、diagnostic、delete / reopen condition。
- `cad-core/tests/test_c8_shapebinder.py`：SubShapeBinder capability 和 known gap focused assertion。
- `cad-core/tests/test_diagnostics.py`、`cad-core/tests/test_adapters.py`：diagnostic vocabulary 和 capability smoke。

## 闸门规则

| S4 裁决 | S5 动作 |
| --- | --- |
| `dto_approved_candidate` | 将对应 backend category 升级为 `backend_gap_requires_implementation`，为 S6 列出 C++ / fixture / test / capability 变更。 |
| `dto_rejected_known_gap_retained` | 保持 `known_gap_diagnostic`，S6 no-code release gate。 |
| `needs_more_native_evidence` | 保持 `oracle_blocked`，补充 reopen condition 和下一轮 probe 条件。 |

## 必须回写的矩阵行

- `C9M5-SCOPE-201`
- `C9M5-SCOPE-202`
- `C9M5-BLOCKER-501`
- `C9M5-CAT-102`
- `C9M5-CAT-104`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'copy_on_change_full_temporary|copy_on_change_boundary|known_gaps|remaining_gaps|BindCopyOnChange' cad-core/src/part_design/feature_shape_binder.cpp cad-core/src/app/copy_on_change.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_c8_shapebinder.py cad-core/tests/test_adapters.py
cd cad-core && ./cad-core capabilities > /tmp/c9m5-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
```

验收标准：

- 若 S4 未批准 DTO，focused tests 必须仍证明 `copy_on_change_full_temporary_document_cache_not_supported` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- 若 S4 批准 DTO，S5 必须列出 S6 的 exact code landing、fixture、expected、focused tests 和 capability update。
- S5 不允许靠删除 diagnostic 或 capabilities 字段伪装支持。

## 非目标

- 不运行全量 FreeCAD build。
- 不执行下游 Rust 同步。
